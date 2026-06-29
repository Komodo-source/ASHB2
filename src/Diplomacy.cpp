#include "header/CivilizationEngine.h"
#include "header/Entity.h"

#include <algorithm>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// Group diplomacy (Phase 4.2)
//
// Formal treaties layered on top of the emergent relations/stance drift in
// updateTribeRelations(). Each civ tick we first apply the ongoing effects of
// active treaties (and expire/break stale ones), then let tribes deliberately
// propose new agreements based on relations, relative power and need.
// ─────────────────────────────────────────────────────────────────────────────

const char* treatyTypeName(TreatyType t) {
    switch (t) {
        case TREATY_PEACE:    return "Peace";
        case TREATY_ALLIANCE: return "Alliance";
        case TREATY_TRADE:    return "Trade Pact";
        case TREATY_TRIBUTE:  return "Tribute";
    }
    return "Treaty";
}

bool CivilizationEngine::hasActiveTreaty(int a, int b, TreatyType type) const {
    for (const Treaty& t : treaties)
        if (t.active && t.type == type && t.between(a, b)) return true;
    return false;
}

int CivilizationEngine::activeTreatyCount() const {
    int n = 0;
    for (const Treaty& t : treaties) if (t.active) n++;
    return n;
}

std::string CivilizationEngine::diplomacySummary() const {
    std::ostringstream os;
    int shown = 0;
    for (const Treaty& t : treaties) {
        if (!t.active) continue;
        const Tribe* A = const_cast<CivilizationEngine*>(this)->findTribe(t.tribeA);
        const Tribe* B = const_cast<CivilizationEngine*>(this)->findTribe(t.tribeB);
        if (!A || !B) continue;
        if (shown++) os << "\n";
        os << treatyTypeName(t.type) << ": " << A->name;
        os << (t.type == TREATY_TRIBUTE ? " <- " : " & ") << B->name;
        if (t.type == TREATY_TRIBUTE) os << " (" << (int)t.tributeAmount << " food/turn)";
        if (shown >= 12) break;
    }
    if (shown == 0) return "No standing treaties.";
    return os.str();
}

// ── Apply ongoing effects of every active treaty; expire or break stale ones ───
void CivilizationEngine::applyTreatyEffects(std::vector<Entity>& entities, int day) {
    auto clamp = [](float v, float lo, float hi){ return std::max(lo, std::min(hi, v)); };

    for (Treaty& t : treaties) {
        if (!t.active) continue;
        Tribe* A = findTribe(t.tribeA);
        Tribe* B = findTribe(t.tribeB);

        // A party vanished (conquered / died out) → the treaty is void.
        if (!A || !B || A->population() == 0 || B->population() == 0) {
            t.active = false;
            continue;
        }

        // Natural expiry.
        if (t.expiryDay >= 0 && day >= t.expiryDay) {
            t.active = false;
            logEvent(day, std::string(treatyTypeName(t.type)) + " between the " + A->name
                          + " and the " + B->name + " has lapsed", "diplomacy");
            continue;
        }

        float& relAB = A->relations[B->id];
        float& relBA = B->relations[A->id];
        bool atWar = (A->stances.count(B->id) && A->stances[B->id] == TS_AT_WAR);

        switch (t.type) {
            case TREATY_PEACE: {
                // Enforce the peace: end any war and keep relations off the war line.
                if (atWar) {
                    A->stances[B->id] = TS_NEUTRAL; B->stances[A->id] = TS_NEUTRAL;
                    A->ethnicWarWith.erase(B->id);  B->ethnicWarWith.erase(A->id);
                }
                relAB = clamp(std::max(relAB, -25.0f) + 0.2f, -100.0f, 100.0f);
                relBA = relAB;
                break;
            }
            case TREATY_TRADE: {
                // War voids commerce.
                if (atWar) {
                    t.active = false;
                    logEvent(day, "War shatters the trade pact between the " + A->name
                                  + " and the " + B->name, "diplomacy");
                    break;
                }
                // Mutual enrichment + slowly warming ties.
                A->granary += 0.8f; B->granary += 0.8f;
                relAB = clamp(relAB + 0.35f, -100.0f, 100.0f);
                relBA = relAB;
                break;
            }
            case TREATY_ALLIANCE: {
                if (atWar) {  // allies who came to blows: the alliance is dead
                    t.active = false;
                    logEvent(day, "The alliance of the " + A->name + " and the "
                                  + B->name + " collapses into war", "diplomacy");
                    break;
                }
                A->stances[B->id] = TS_ALLY; B->stances[A->id] = TS_ALLY;
                relAB = clamp(std::max(relAB, 45.0f) + 0.2f, -100.0f, 100.0f);
                relBA = relAB;
                break;
            }
            case TREATY_TRIBUTE: {
                // B (payer) hands food to A (receiver). If the granary is bare the
                // payer skips a turn; sustained inability breeds rebellion below.
                float pay = std::min(B->granary, t.tributeAmount);
                B->granary -= pay;
                A->granary += pay;
                // The payment buys protection (no war) but breeds quiet resentment.
                relAB = clamp(relAB + 0.1f, -100.0f, 100.0f);
                relBA = clamp(relBA - 0.15f, -100.0f, 100.0f);
                for (int mid : B->memberIds) {
                    Entity* e = entityById(entities, mid);
                    if (e) e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + 0.05f);
                }
                // Rebellion: once the vassal outgrows its overlord, it throws off the yoke.
                float strA = calculateTribeMilitaryStrength(*A, entities);
                float strB = calculateTribeMilitaryStrength(*B, entities);
                if (strB > strA * 1.15f) {
                    t.active = false;
                    relAB = clamp(relAB - 30.0f, -100.0f, 100.0f); relBA = relAB;
                    logEvent(day, "The " + B->name + " grow strong and cast off tribute to the "
                                  + A->name, "diplomacy");
                }
                break;
            }
        }
    }

    // Compact the list occasionally so it doesn't grow without bound.
    if (treaties.size() > 256) {
        treaties.erase(std::remove_if(treaties.begin(), treaties.end(),
                       [](const Treaty& t){ return !t.active; }), treaties.end());
    }
}

// ── Let tribes propose new treaties (bounded per tick) ─────────────────────────
void CivilizationEngine::proposeTreaties(std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    int signedThisTick = 0;
    const int MAX_PER_TICK = 2;

    auto inContact = [](const Tribe& A, const Tribe& B) {
        float dx = A.centerX - B.centerX, dy = A.centerY - B.centerY;
        bool sameRegion = (A.regionId == B.regionId && A.regionId != -1);
        return sameRegion || (dx*dx + dy*dy) < 300.0f * 300.0f;
    };

    for (size_t i = 0; i < tribes.size() && signedThisTick < MAX_PER_TICK; ++i) {
        for (size_t j = i + 1; j < tribes.size() && signedThisTick < MAX_PER_TICK; ++j) {
            Tribe& A = tribes[i];
            Tribe& B = tribes[j];
            if (A.population() == 0 || B.population() == 0) continue;
            if (!inContact(A, B)) continue;

            float rel   = A.relations.count(B.id) ? A.relations[B.id] : 0.0f;
            bool  atWar = (A.stances.count(B.id) && A.stances[B.id] == TS_AT_WAR);
            float strA  = calculateTribeMilitaryStrength(A, entities);
            float strB  = calculateTribeMilitaryStrength(B, entities);
            float ratio = strB > 0.1f ? strA / strB : 5.0f;   // A's power vs B's

            // ── PEACE: end a war the weaker side is losing, when tempers cool ──
            if (atWar && !hasActiveTreaty(A.id, B.id, TREATY_PEACE)) {
                bool lopsided = ratio > 1.6f || ratio < 0.625f;   // one side clearly losing
                float chance  = 0.04f + (lopsided ? 0.10f : 0.0f) + std::max(0.0f, (rel + 60.0f)) * 0.0015f;
                if (roll(rng) < chance) {
                    Treaty t; t.type = TREATY_PEACE; t.tribeA = A.id; t.tribeB = B.id;
                    t.startDay = day; t.expiryDay = day + 220;
                    treaties.push_back(t);
                    A.stances[B.id] = TS_NEUTRAL; B.stances[A.id] = TS_NEUTRAL;
                    A.ethnicWarWith.erase(B.id);  B.ethnicWarWith.erase(A.id);
                    A.relations[B.id] = std::max(rel, -20.0f); B.relations[A.id] = A.relations[B.id];
                    totalTreatiesSigned++; signedThisTick++;
                    logEvent(day, "PEACE: the " + A.name + " and the " + B.name
                                  + " lay down their arms", "diplomacy");
                    continue;
                }
            }

            if (atWar) continue;  // the remaining treaties require peace

            // ── ALLIANCE: warm relations formalise into mutual defence ─────────
            if (rel > 60.0f && !hasActiveTreaty(A.id, B.id, TREATY_ALLIANCE)) {
                if (roll(rng) < 0.12f) {
                    Treaty t; t.type = TREATY_ALLIANCE; t.tribeA = A.id; t.tribeB = B.id;
                    t.startDay = day; t.expiryDay = -1;
                    treaties.push_back(t);
                    A.stances[B.id] = TS_ALLY; B.stances[A.id] = TS_ALLY;
                    totalTreatiesSigned++; signedThisTick++;
                    logEvent(day, "ALLIANCE: the " + A.name + " and the " + B.name
                                  + " swear mutual defence", "diplomacy");
                    continue;
                }
            }

            // ── TRIBUTE: a dominant neighbour extorts food instead of war ──────
            // Imposed on a markedly weaker, not-yet-friendly tribe.
            if (ratio > 1.9f && rel < 25.0f
                && !hasActiveTreaty(A.id, B.id, TREATY_TRIBUTE)
                && A.militarism > 45.0f) {
                if (roll(rng) < 0.08f) {
                    Treaty t; t.type = TREATY_TRIBUTE; t.tribeA = A.id; t.tribeB = B.id;
                    t.startDay = day; t.expiryDay = day + 260;
                    t.tributeAmount = 1.5f;
                    treaties.push_back(t);
                    A.relations[B.id] = std::max(rel, -30.0f); B.relations[A.id] = A.relations[B.id];
                    totalTreatiesSigned++; signedThisTick++;
                    logEvent(day, "TRIBUTE: the " + B.name + " bow to the mightier "
                                  + A.name + ", paying food for peace", "diplomacy");
                    continue;
                }
            }
            // Symmetric tribute check (B stronger than A).
            if (ratio < 0.526f && rel < 25.0f
                && !hasActiveTreaty(A.id, B.id, TREATY_TRIBUTE)
                && B.militarism > 45.0f) {
                if (roll(rng) < 0.08f) {
                    Treaty t; t.type = TREATY_TRIBUTE; t.tribeA = B.id; t.tribeB = A.id;
                    t.startDay = day; t.expiryDay = day + 260;
                    t.tributeAmount = 1.5f;
                    treaties.push_back(t);
                    B.relations[A.id] = std::max(rel, -30.0f); A.relations[B.id] = B.relations[A.id];
                    totalTreatiesSigned++; signedThisTick++;
                    logEvent(day, "TRIBUTE: the " + A.name + " bow to the mightier "
                                  + B.name + ", paying food for peace", "diplomacy");
                    continue;
                }
            }

            // ── TRADE: cordial neighbours at peace open a standing exchange ────
            if (rel > 12.0f && !hasActiveTreaty(A.id, B.id, TREATY_TRADE)
                && !hasActiveTreaty(A.id, B.id, TREATY_TRIBUTE)) {
                if (roll(rng) < 0.10f) {
                    Treaty t; t.type = TREATY_TRADE; t.tribeA = A.id; t.tribeB = B.id;
                    t.startDay = day; t.expiryDay = day + 320;
                    treaties.push_back(t);
                    totalTreatiesSigned++; signedThisTick++;
                    logEvent(day, "TRADE PACT: the " + A.name + " and the " + B.name
                                  + " open the caravan roads", "diplomacy");
                    continue;
                }
            }
        }
    }
}

void CivilizationEngine::updateDiplomacy(std::vector<Entity>& entities, int day) {
    applyTreatyEffects(entities, day);
    proposeTreaties(entities, day);
}
