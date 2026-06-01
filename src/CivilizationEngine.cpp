#include "header/CivilizationEngine.h"
#include "header/Entity.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <numeric>

CivilizationEngine* globalCivEngine = nullptr;

// ── Innovation catalog (the complete pool of discoverable technologies) ────────
struct InnovTemplate {
    std::string name, category, description;
    float       complexity;
    std::vector<std::string> prereqs;
};

static const std::vector<InnovTemplate> CATALOG = {
    // Agriculture
    {"Seed Selection",    "agriculture","Choosing the best seeds to replant",        20, {}},
    {"Seasonal Planting", "agriculture","Timing crops to the seasons",               35, {"Seed Selection"}},
    {"Animal Keeping",    "agriculture","Domesticating animals for labour and food", 40, {}},
    {"Food Drying",       "agriculture","Preserving food through sun and smoke",     22, {}},
    {"Irrigation",        "agriculture","Channelling water to fields",               60, {"Seasonal Planting"}},
    // Tool
    {"Fire Making",       "tool",       "Producing fire reliably and on demand",     18, {}},
    {"Edge Knapping",     "tool",       "Shaping stone into sharp cutting tools",    22, {}},
    {"Rope Braiding",     "tool",       "Twisting plant fibres into strong cord",    18, {}},
    {"Clay Shaping",      "tool",       "Forming and hardening clay vessels",        32, {}},
    {"Handle Crafting",   "tool",       "Binding grips onto tools and weapons",      28, {"Edge Knapping"}},
    {"Metal Working",     "tool",       "Smelting and shaping malleable metals",     78, {"Fire Making","Edge Knapping"}},
    // Medicine
    {"Wound Binding",     "medicine",   "Cleaning and wrapping injuries",            18, {}},
    {"Fever Herb",        "medicine",   "Plant remedies to reduce fever",            28, {}},
    {"Bone Setting",      "medicine",   "Realigning broken bones to heal cleanly",   42, {"Wound Binding"}},
    {"Quarantine",        "medicine",   "Isolating the sick to stop contagion",      28, {}},
    {"Birthing Method",   "medicine",   "Structured support for safe childbirth",    48, {"Wound Binding"}},
    // Social
    {"Trade Token",       "social",     "Objects used as agreed exchange value",     22, {}},
    {"Oral Record",       "social",     "Systematic memorised transmission of lore", 18, {}},
    {"Conflict Mediation","social",     "Structured resolution of disputes",         32, {}},
    {"Counting Method",   "social",     "Abstract numerical reasoning",              30, {}},
    {"Signal System",     "social",     "Long-distance coded communication",         45, {"Oral Record"}},
    {"Marriage Rite",     "social",     "Formalised partnership ceremony",           22, {}},
    // Military
    {"Shield Craft",      "military",   "Making defensive tools for combat",         24, {"Edge Knapping"}},
    {"Group Formation",   "military",   "Coordinated movement in group conflict",    32, {}},
    {"Fortification",     "military",   "Building protective barriers",              62, {"Clay Shaping"}},
    {"Weapon Sharpening", "military",   "Improving weapon lethality and durability", 22, {"Edge Knapping"}},
    // Spiritual
    {"Sacred Chant",      "spiritual",  "Ritualized communal vocal expression",      14, {}},
    {"Death Rite",        "spiritual",  "Formal burial and mourning ceremony",       18, {}},
    {"Dream Reading",     "spiritual",  "Interpreting dreams as guidance",           24, {}},
    {"Star Calendar",     "spiritual",  "Tracking time through celestial patterns",  48, {"Counting Method","Dream Reading"}},
    {"Ancestor Offering", "spiritual",  "Ritual gifts to honour the deceased",       22, {"Death Rite"}},
};

// ── Constructor ───────────────────────────────────────────────────────────────
CivilizationEngine::CivilizationEngine() {}

// ── Main tick ─────────────────────────────────────────────────────────────────
void CivilizationEngine::tick(std::vector<Entity>& entities, int day) {
    removeDeadFromTribes(entities);
    updateDominanceRanks(entities);
    updateTribes(entities, day);
    updateReligions(entities, day);
    updateInnovations(entities, day);
    updateTribeRelations(entities, day);
    updateEra(entities);
    applyEffectsToEntities(entities, day);
}

// ── Dominance ranks ───────────────────────────────────────────────────────────
void CivilizationEngine::updateDominanceRanks(std::vector<Entity>& entities) {
    for (Entity& ent : entities) {
        if (ent.entityHealth <= 0.0f) continue;
        float charisma = computeCharisma(&ent);

        float socialBonus = 0.0f;
        for (const auto& s : ent.list_entityPointedSocial)
            socialBonus += s.social * 0.015f;

        float fearBonus = 0.0f;
        for (const auto& a : ent.list_entityPointedAnger)
            fearBonus += a.anger * 0.008f;

        float leaderBonus = 0.0f;
        for (const auto& tribe : tribes)
            if (tribe.leaderId == ent.entityId)
                leaderBonus = 20.0f + tribe.population() * 0.6f;

        ent.dominanceRank = std::min(100.0f,
            charisma * 0.5f + socialBonus + fearBonus + leaderBonus);
    }
}

// ── Tribe management ──────────────────────────────────────────────────────────
static float dist2(float ax, float ay, float bx, float by) {
    float dx = ax - bx, dy = ay - by;
    return dx*dx + dy*dy;
}

void CivilizationEngine::updateTribes(std::vector<Entity>& entities, int day) {
    // 1. Update existing tribes
    for (auto& tribe : tribes) {
        updateTribeCenter(tribe, entities);
        updateTribeValues(tribe, entities);
        updateTribeReligion(tribe, entities);
        updateTribeTech(tribe, entities);
        electLeader(tribe, entities);

        // Absorb tribeless entities with very compatible values — strict threshold so
        // dissimilar entities can form their own tribe instead
        for (Entity& ent : entities) {
            if (ent.entityHealth <= 0.0f) continue;
            if (ent.tribeId != -1) continue;
            if (tribe.population() >= 10) continue; // keep tribes small so others can form
            float valDiff = std::abs(ent.ValueSystem.collectivism   - tribe.collectivism)  * 0.4f +
                            std::abs(ent.ValueSystem.spiritualNeed  - tribe.spiritualism)  * 0.3f +
                            std::abs(ent.ValueSystem.achievementDrive - tribe.innovation)  * 0.3f;
            if (valDiff < 22.0f) { // only very similar entities absorbed; others form new tribes
                absorbEntityIntoTribe(tribe, &ent);
                logEvent(day, ent.name + " joined " + tribe.name, "tribe");
            }
        }
    }

    // 2. Try forming new tribes from tribeless clusters
    std::vector<Entity*> tribeless;
    for (Entity& ent : entities)
        if (ent.entityHealth > 0.0f && ent.tribeId == -1 && ent.entityAge > 16.0f)
            tribeless.push_back(&ent);

    // Cluster tribeless entities by value affinity + social bonds.
    // Cap cluster size at 5 so leftover entities can form separate tribes next tick.
    std::vector<bool> used(tribeless.size(), false);
    bool formedThisTick = false;
    for (size_t i = 0; i < tribeless.size(); ++i) {
        if (used[i]) continue;
        std::vector<Entity*> cluster;
        cluster.push_back(tribeless[i]);
        for (size_t j = i + 1; j < tribeless.size() && cluster.size() < 5; ++j) {
            if (used[j]) continue;
            float valDiff = std::abs(tribeless[i]->ValueSystem.collectivism  - tribeless[j]->ValueSystem.collectivism)  * 0.4f
                          + std::abs(tribeless[i]->ValueSystem.spiritualNeed - tribeless[j]->ValueSystem.spiritualNeed)  * 0.3f
                          + std::abs(tribeless[i]->ValueSystem.achievementDrive - tribeless[j]->ValueSystem.achievementDrive) * 0.3f;
            float bond = tribeless[i]->searchConnSocial(tribeless[j]);
            if (valDiff < 32.0f || bond > 12.0f) {
                cluster.push_back(tribeless[j]);
                used[j] = true;
            }
        }
        used[i] = true;
        if (cluster.size() >= 3) {
            if (formTribe(cluster, day)) {
                formedThisTick = true;
                break; // one new tribe per tick — others form on subsequent ticks
            }
        }
    }

    // 3. Split large tribes (>15 members) into two
    splitLargeTribes(entities, day);

    // 4. Dissolve tiny tribes
    dissolveSmallTribes(entities, day);
}

bool CivilizationEngine::formTribe(std::vector<Entity*>& cluster, int day) {
    // Find highest charisma entity in cluster
    Entity* bestLeader = nullptr;
    float   bestCharisma = 0.0f;
    for (Entity* ent : cluster) {
        float ch = computeCharisma(ent);
        if (ch > bestCharisma) { bestCharisma = ch; bestLeader = ent; }
    }
    if (!bestLeader || bestCharisma < 20.0f) return false;

    Tribe tribe;
    tribe.id          = nextTribeId++;
    tribe.foundedOnDay = day;
    tribe.leaderId    = bestLeader->entityId;
    tribe.name        = tribeName(bestLeader);

    // Initialise collective values from cluster average
    float mil = 0, spi = 0, col = 0, inn = 0;
    for (Entity* ent : cluster) {
        ent->tribeId = tribe.id;
        tribe.memberIds.push_back(ent->entityId);
        mil += (100.0f - ent->personality.agreeableness);
        spi += ent->ValueSystem.spiritualNeed;
        col += ent->ValueSystem.collectivism;
        inn += ent->personality.openness;
    }
    float n = (float)cluster.size();
    tribe.militarism   = mil / n;
    tribe.spiritualism = spi / n;
    tribe.collectivism = col / n;
    tribe.innovation   = inn / n;

    tribes.push_back(tribe);
    logEvent(day, "The " + tribe.name + " was founded by " + bestLeader->name +
             " (" + std::to_string((int)cluster.size()) + " members)", "tribe");
    return true;
}

void CivilizationEngine::electLeader(Tribe& tribe, std::vector<Entity>& entities) {
    if (tribe.memberIds.empty()) return;
    int   bestId   = -1;
    float bestRank = -1.0f;
    for (int mid : tribe.memberIds) {
        Entity* ent = entityById(entities, mid);
        if (!ent || ent->entityHealth <= 0.0f) continue;
        if (ent->dominanceRank > bestRank) { bestRank = ent->dominanceRank; bestId = mid; }
    }
    if (bestId >= 0 && bestId != tribe.leaderId) {
        Entity* old = entityById(entities, tribe.leaderId);
        Entity* neo = entityById(entities, bestId);
        if (old && neo) {
            std::string desc = neo->name + " seized leadership of " + tribe.name
                             + " from " + old->name;
            logEvent(0, desc, "tribe");
        }
        tribe.leaderId = bestId;
    }
}

void CivilizationEngine::updateTribeCenter(Tribe& tribe, std::vector<Entity>& entities) {
    // No spatial positions — tribe center concept is unused
    (void)tribe; (void)entities;
}

void CivilizationEngine::updateTribeValues(Tribe& tribe, std::vector<Entity>& entities) {
    if (tribe.memberIds.empty()) return;
    float mil = 0, spi = 0, col = 0, inn = 0; float n = 0;
    for (int mid : tribe.memberIds) {
        Entity* e = entityById(entities, mid);
        if (!e || e->entityHealth <= 0.0f) continue;
        mil += (100.0f - e->personality.agreeableness);
        spi += e->ValueSystem.spiritualNeed;
        col += e->ValueSystem.collectivism;
        inn += e->personality.openness;
        n++;
    }
    if (n == 0) return;
    // Slow drift toward member average
    auto drift = [&](float& v, float target) { v += (target / n - v) * 0.05f; };
    drift(tribe.militarism,   mil);
    drift(tribe.spiritualism, spi);
    drift(tribe.collectivism, col);
    drift(tribe.innovation,   inn);
}

void CivilizationEngine::updateTribeReligion(Tribe& tribe, std::vector<Entity>& entities) {
    if (religions.empty()) return;
    std::map<int, int> counts;
    for (int mid : tribe.memberIds) {
        Entity* e = entityById(entities, mid);
        if (e && e->religionId >= 0) counts[e->religionId]++;
    }
    int bestRel = -1, bestCount = 0;
    for (auto& p : counts)
        if (p.second > bestCount) { bestCount = p.second; bestRel = p.first; }
    tribe.dominantReligionId = bestRel;
}

void CivilizationEngine::updateTribeTech(Tribe& tribe, std::vector<Entity>& entities) {
    for (int mid : tribe.memberIds) {
        Entity* e = entityById(entities, mid);
        if (!e) continue;
        for (int tid : e->knownTechIds)
            tribe.knownTechIds.insert(tid);
    }
}

void CivilizationEngine::absorbEntityIntoTribe(Tribe& tribe, Entity* ent) {
    ent->tribeId = tribe.id;
    tribe.memberIds.push_back(ent->entityId);
    // Give entity access to tribe's known technologies
    for (int tid : tribe.knownTechIds)
        if (std::find(ent->knownTechIds.begin(), ent->knownTechIds.end(), tid) == ent->knownTechIds.end())
            ent->knownTechIds.push_back(tid);
}

void CivilizationEngine::removeDeadFromTribes(std::vector<Entity>& entities) {
    for (auto& tribe : tribes) {
        tribe.memberIds.erase(
            std::remove_if(tribe.memberIds.begin(), tribe.memberIds.end(),
                [&](int id) {
                    Entity* e = entityById(entities, id);
                    return !e || e->entityHealth <= 0.0f;
                }),
            tribe.memberIds.end());
    }
}

void CivilizationEngine::dissolveSmallTribes(std::vector<Entity>& entities, int day) {
    for (auto it = tribes.begin(); it != tribes.end(); ) {
        if (it->population() < 3) {
            logEvent(day, it->name + " has dissolved", "tribe");
            for (int mid : it->memberIds) {
                Entity* e = entityById(entities, mid);
                if (e) e->tribeId = -1;
            }
            it = tribes.erase(it);
        } else { ++it; }
    }
}

void CivilizationEngine::splitLargeTribes(std::vector<Entity>& entities, int day) {
    // Use index loop: formTribe() push_backs to tribes, invalidating iterators.
    for (size_t idx = 0; idx < tribes.size(); ++idx) {
        if (tribes[idx].population() <= 8) continue;

        std::vector<Entity*> members;
        for (int mid : tribes[idx].memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f) members.push_back(e);
        }
        if (members.size() < 5) continue;

        // Two farthest-apart members as seeds
        float maxDist = -1.0f; int seedA = 0, seedB = 1;
        for (size_t i = 0; i < members.size(); ++i) {
            for (size_t j = i + 1; j < members.size(); ++j) {
                float d = std::abs(members[i]->ValueSystem.collectivism  - members[j]->ValueSystem.collectivism ) * 0.4f
                        + std::abs(members[i]->ValueSystem.spiritualNeed - members[j]->ValueSystem.spiritualNeed) * 0.3f
                        + std::abs(members[i]->personality.openness      - members[j]->personality.openness     ) * 0.3f;
                if (d > maxDist) { maxDist = d; seedA = i; seedB = j; }
            }
        }
        if (maxDist < 10.0f) continue;

        std::vector<Entity*> groupA, groupB;
        for (Entity* e : members) {
            float dA = std::abs(e->ValueSystem.collectivism     - members[seedA]->ValueSystem.collectivism    ) * 0.4f
                      + std::abs(e->ValueSystem.spiritualNeed   - members[seedA]->ValueSystem.spiritualNeed   ) * 0.3f
                      + std::abs(e->ValueSystem.achievementDrive- members[seedA]->ValueSystem.achievementDrive) * 0.3f;
            float dB = std::abs(e->ValueSystem.collectivism     - members[seedB]->ValueSystem.collectivism    ) * 0.4f
                      + std::abs(e->ValueSystem.spiritualNeed   - members[seedB]->ValueSystem.spiritualNeed   ) * 0.3f
                      + std::abs(e->ValueSystem.achievementDrive- members[seedB]->ValueSystem.achievementDrive) * 0.3f;
            if (dA <= dB) groupA.push_back(e);
            else          groupB.push_back(e);
        }
        if (groupA.size() < 3 || groupB.size() < 3) continue;

        // Save name before formTribe \u2014 it push_backs to tribes, shifting indices
        std::string splitName = tribes[idx].name;

        for (Entity* e : groupB) {
            e->tribeId = -1;
            tribes[idx].memberIds.erase(
                std::remove(tribes[idx].memberIds.begin(), tribes[idx].memberIds.end(), e->entityId),
                tribes[idx].memberIds.end());
        }

        formTribe(groupB, day); // may push_back to tribes; idx still valid (no erase)
        logEvent(day, "The " + splitName + " has split -- a faction broke away", "tribe");
    }
}

// ── Religion management ────────────────────────────────────────────────────────
void CivilizationEngine::updateReligions(std::vector<Entity>& entities, int day) {
    // Check for prophets among unaffiliated entities
    for (Entity& ent : entities) {
        if (ent.entityHealth <= 0.0f) continue;
        if (ent.religionId != -1) continue;   // already follows one
        if (ent.entityAge < 18.0f) continue;

        // Prophet probability: spiritual need + openness + life experience
        float prophetScore = (ent.ValueSystem.spiritualNeed / 100.0f) *
                             (ent.personality.openness     / 100.0f);
        for (const auto& mem : ent.lifeMemories)
            if (mem.isFormative) prophetScore += 0.08f;
        prophetScore = std::min(1.0f, prophetScore);

        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        if (roll(rng) < prophetScore * 0.030f)
            foundReligion(&ent, day);
    }

    // Spread existing religions
    spreadReligions(entities, day);

    // Tribal conversion pressure: dominant religion pulls uncommitted tribe members in
    {
        std::uniform_real_distribution<float> rollT(0.0f, 1.0f);
        for (auto& tribe : tribes) {
            if (tribe.memberIds.size() < 3) continue;
            std::map<int, int> relCounts;
            for (int mid : tribe.memberIds) {
                Entity* e = entityById(entities, mid);
                if (e && e->entityHealth > 0.0f && e->religionId >= 0)
                    relCounts[e->religionId]++;
            }
            if (relCounts.empty()) continue;
            int domRel = -1, domCount = 0;
            for (auto& p : relCounts) if (p.second > domCount) { domCount = p.second; domRel = p.first; }
            float ratio = (float)domCount / (float)tribe.memberIds.size();
            if (ratio < 0.30f) continue;
            Religion* r = findReligion(domRel);
            if (!r) continue;
            for (int mid : tribe.memberIds) {
                Entity* e = entityById(entities, mid);
                if (!e || e->entityHealth <= 0.0f || e->religionId != -1) continue;
                float pressure = ratio * (e->ValueSystem.collectivism / 100.0f) * 0.12f;
                pressure *= (0.5f + e->ValueSystem.spiritualNeed / 150.0f);
                if (rollT(rng) < pressure) {
                    e->religionId = domRel;
                    r->followerIds.push_back(e->entityId);
                    logEvent(day, e->name + " joined " + r->name + " (tribal community)", "religion");
                }
            }
        }
    }

    // Update influence
    for (auto& rel : religions) {
        rel.influence = std::min(100.0f,
            (float)rel.followerIds.size() / std::max(1.0f, (float)entities.size()) * 100.0f);
    }
}

bool CivilizationEngine::foundReligion(Entity* prophet, int day) {
    Religion rel;
    rel.id              = nextReligionId++;
    rel.founderEntityId = prophet->entityId;
    rel.foundedOnDay    = day;
    rel.followerIds     = {prophet->entityId};
    prophet->religionId = rel.id;

    // Doctrine from founder personality
    if (prophet->personality.agreeableness > 60.0f)
        rel.moralCode = MC_PEACEFUL;
    else if (prophet->personality.neuroticism > 65.0f)
        rel.moralCode = MC_STRICT;
    else if ((100.0f - prophet->personality.agreeableness) > 65.0f)
        rel.moralCode = MC_WARRIOR;
    else
        rel.moralCode = MC_FLEXIBLE;

    if (prophet->personality.extraversion > 60.0f)
        rel.ritual = RT_WEEKLY_GATHERING;
    else if (prophet->personality.openness > 65.0f)
        rel.ritual = RT_MEDITATION;
    else if (prophet->personality.conscientiousness > 65.0f)
        rel.ritual = RT_DAILY_PRAYER;
    else
        rel.ritual = RT_CEREMONY;

    rel.isPolytheistic  = (prophet->personality.openness > 55.0f);
    rel.spiritualDemand = prophet->ValueSystem.spiritualNeed;

    // Generate sacred principle from prophet's life
    bool hasGrief = false;
    for (const auto& g : prophet->griefStates) if (g.intensity > 0.3f) { hasGrief = true; break; }

    if (hasGrief)
        rel.holyPrinciple = "Those who are lost shall be remembered forever.";
    else if (prophet->entityStress > 60.0f)
        rel.holyPrinciple = "Order is the shield against suffering.";
    else if (prophet->personality.openness > 70.0f)
        rel.holyPrinciple = "All things are connected in the great weave.";
    else
        rel.holyPrinciple = "Live rightly, and the world shall be good.";

    rel.name = religionName(prophet);
    religions.push_back(rel);

    logEvent(day, prophet->name + " founded \"" + rel.name + "\" — \""
             + rel.holyPrinciple + "\"", "religion");
    return true;
}

void CivilizationEngine::spreadReligions(std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    for (auto& rel : religions) {
        // Clean up dead followers
        rel.followerIds.erase(
            std::remove_if(rel.followerIds.begin(), rel.followerIds.end(),
                [&](int fid) {
                    Entity* e = entityById(entities, fid);
                    return !e || e->entityHealth <= 0.0f;
                }),
            rel.followerIds.end());

        std::vector<int> snapshot = rel.followerIds; // avoid modifying list while iterating
        for (int fid : snapshot) {
            Entity* follower = entityById(entities, fid);
            if (!follower || follower->entityHealth <= 0.0f) continue;

            float preacherCharisma = computeCharisma(follower) / 100.0f;
            float zealotry = 0.4f + follower->ValueSystem.spiritualNeed / 150.0f;

            // Spread through social bonds
            for (const auto& bond : follower->list_entityPointedSocial) {
                if (!bond.pointedEntity) continue;
                Entity* target = bond.pointedEntity;
                if (target->religionId != -1) continue;
                if (target->entityAge < 10.0f || target->entityHealth <= 0.0f) continue;

                float convProb = (bond.social / 100.0f) * preacherCharisma * 0.08f;
                convProb *= (0.5f + target->ValueSystem.spiritualNeed / 100.0f);
                convProb *= zealotry;
                // Same tribe = much stronger conversion pressure
                if (follower->tribeId != -1 && follower->tribeId == target->tribeId)
                    convProb *= 2.5f;

                if (roll(rng) < convProb) {
                    target->religionId = rel.id;
                    rel.followerIds.push_back(target->entityId);
                    // Shared faith strengthens the bond
                    for (auto& b : follower->list_entityPointedSocial) {
                        if (b.pointedEntity == target) { b.social = std::min(100.0f, b.social + 6.0f); break; }
                    }
                    logEvent(day, target->name + " converted to " + rel.name
                             + " (through " + follower->name + ")", "religion");
                }
            }

            // Also spread to tribe members directly (weaker, no bond needed)
            if (follower->tribeId != -1) {
                Tribe* tribe = findTribe(follower->tribeId);
                if (tribe) {
                    for (int mid : tribe->memberIds) {
                        if (mid == fid) continue;
                        Entity* member = entityById(entities, mid);
                        if (!member || member->religionId != -1) continue;
                        if (member->entityHealth <= 0.0f || member->entityAge < 10.0f) continue;
                        float tribeProb = preacherCharisma * zealotry *
                                         (member->ValueSystem.spiritualNeed / 100.0f) * 0.05f;
                        if (roll(rng) < tribeProb) {
                            member->religionId = rel.id;
                            rel.followerIds.push_back(member->entityId);
                            logEvent(day, member->name + " joined " + rel.name
                                     + " (tribal influence)", "religion");
                        }
                    }
                }
            }
        }
    }
}

// ── Innovation management ──────────────────────────────────────────────────────
void CivilizationEngine::updateInnovations(std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    for (Entity& ent : entities) {
        if (ent.entityHealth <= 0.0f) continue;
        if (ent.entityAge < 14.0f) continue;

        float inventorScore = (ent.personality.openness        / 100.0f) *
                              (ent.personality.conscientiousness / 100.0f) * 0.7f +
                              (1.0f - ent.entityBoredom / 100.0f) * 0.3f;

        if (roll(rng) < inventorScore * 0.0012f) {
            Tribe* tribe = findTribe(ent.tribeId);
            discoverInnovation(&ent, tribe, day);
        }
    }

    spreadInnovations(entities, day);
}

bool CivilizationEngine::discoverInnovation(Entity* inventor, Tribe* tribe, int day) {
    // Find innovations not yet discovered globally, whose prerequisites are met
    std::vector<const InnovTemplate*> candidates;
    for (const auto& tmpl : CATALOG) {
        // Already discovered?
        bool alreadyFound = false;
        for (const auto& inv : innovations)
            if (inv.name == tmpl.name) { alreadyFound = true; break; }
        if (alreadyFound) continue;

        // Prerequisites met by this entity?
        bool prereqsMet = true;
        for (const std::string& prereq : tmpl.prereqs) {
            bool known = false;
            for (int tid : inventor->knownTechIds) {
                Innovation* inv = findInnovation(tid);
                if (inv && inv->name == prereq) { known = true; break; }
            }
            if (!known) { prereqsMet = false; break; }
        }
        if (prereqsMet) candidates.push_back(&tmpl);
    }

    if (candidates.empty()) return false;

    // Bias toward category matching the entity's context
    std::string preferredCat = pickCategory(inventor, tribe);
    std::vector<const InnovTemplate*> preferred;
    for (auto* c : candidates)
        if (c->category == preferredCat) preferred.push_back(c);

    const InnovTemplate* chosen = preferred.empty()
        ? candidates[std::uniform_int_distribution<int>(0, (int)candidates.size()-1)(rng)]
        : preferred [std::uniform_int_distribution<int>(0, (int)preferred.size()-1)(rng)];

    Innovation inv;
    inv.id                   = nextInnovId++;
    inv.name                 = chosen->name;
    inv.category             = chosen->category;
    inv.description          = chosen->description;
    inv.complexity           = chosen->complexity;
    inv.prereqNames          = chosen->prereqs;
    inv.discoveredByEntityId = inventor->entityId;
    inv.discoveredByTribeId  = tribe ? tribe->id : -1;
    inv.discoveredOnDay      = day;
    inv.knowerCount          = 1;
    innovations.push_back(inv);

    inventor->knownTechIds.push_back(inv.id);
    if (tribe) tribe->knownTechIds.insert(inv.id);

    logEvent(day, inventor->name + " discovered \"" + inv.name + "\" (" + inv.category + ")"
             + (tribe ? " for the " + tribe->name : ""), "innovation");
    return true;
}

void CivilizationEngine::spreadInnovations(std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    for (auto& inv : innovations) {
        for (Entity& ent : entities) {
            if (ent.entityHealth <= 0.0f) continue;
            // Already knows it?
            if (std::find(ent.knownTechIds.begin(), ent.knownTechIds.end(), inv.id)
                != ent.knownTechIds.end()) continue;
            // Check prereqs
            if (!entityKnowsPrereqs(&ent, inv)) continue;

            // Does anyone nearby know it?
            bool nearbyKnower = false;
            for (const auto& bond : ent.list_entityPointedSocial) {
                if (!bond.pointedEntity) continue;
                if (std::find(bond.pointedEntity->knownTechIds.begin(),
                              bond.pointedEntity->knownTechIds.end(), inv.id)
                    != bond.pointedEntity->knownTechIds.end()) {
                    nearbyKnower = true; break;
                }
            }
            // Also tribe members
            Tribe* tribe = findTribe(ent.tribeId);
            if (!nearbyKnower && tribe && tribe->knownTechIds.count(inv.id)) nearbyKnower = true;

            if (!nearbyKnower) continue;

            // Spread probability (higher for simpler innovations)
            float spreadProb = (1.0f - inv.complexity / 100.0f) * 0.008f;
            if (roll(rng) < spreadProb) {
                ent.knownTechIds.push_back(inv.id);
                if (tribe) tribe->knownTechIds.insert(inv.id);
                inv.knowerCount++;
            }
        }
    }
}

bool CivilizationEngine::entityKnowsPrereqs(const Entity* ent, const Innovation& inv) const {
    for (const std::string& prereq : inv.prereqNames) {
        bool found = false;
        for (int tid : ent->knownTechIds) {
            const_cast<CivilizationEngine*>(this); // const access through non-const members
            for (const auto& i : innovations)
                if (i.id == tid && i.name == prereq) { found = true; break; }
            if (found) break;
        }
        if (!found) return false;
    }
    return true;
}

std::string CivilizationEngine::pickCategory(const Entity* ent, const Tribe* tribe) const {
    // Pick innovation category most relevant to current context
    if (ent->entityHealth < 35.0f) return "medicine";
    if (tribe && tribe->militarism > 65.0f) return "military";
    if (ent->ValueSystem.spiritualNeed > 65.0f) return "spiritual";
    if (tribe && tribe->innovation > 60.0f) return "tool";
    if (ent->entityStress > 55.0f) return "social";
    // Default: agriculture (universal need)
    return "agriculture";
}

// ── Tribe relations ────────────────────────────────────────────────────────────
void CivilizationEngine::updateTribeRelations(std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    for (size_t i = 0; i < tribes.size(); ++i) {
        for (size_t j = i + 1; j < tribes.size(); ++j) {
            Tribe& A = tribes[i];
            Tribe& B = tribes[j];

            float valDiff = std::abs(A.militarism   - B.militarism)   * 0.3f
                          + std::abs(A.spiritualism  - B.spiritualism) * 0.2f
                          + std::abs(A.collectivism  - B.collectivism) * 0.25f
                          + std::abs(A.innovation    - B.innovation)   * 0.25f;

            // Shared religion = diplomatic warmth; rival religions with high spiritualism = friction
            if (A.dominantReligionId != -1 && A.dominantReligionId == B.dominantReligionId)
                valDiff *= 0.55f;
            else if (A.dominantReligionId != -1 && B.dominantReligionId != -1
                     && A.dominantReligionId != B.dominantReligionId
                     && A.spiritualism > 55.0f && B.spiritualism > 55.0f)
                valDiff *= 1.35f;

            float& rel = A.relations[B.id];
            B.relations[A.id] = rel;

            if (valDiff < 25.0f)
                rel = std::min(100.0f, rel + 1.2f);
            else if (valDiff > 50.0f)
                rel = std::max(-100.0f, rel - 0.9f);
            else
                rel = rel * 0.98f;

            TribeStance stance;
            if      (rel >  55.0f) stance = TS_ALLY;
            else if (rel > -20.0f) stance = TS_NEUTRAL;
            else if (rel > -55.0f) stance = TS_RIVAL;
            else                   stance = TS_AT_WAR;

            TribeStance prev = A.stances.count(B.id) ? A.stances[B.id] : TS_NEUTRAL;
            if (stance != prev) {
                A.stances[B.id] = stance;
                B.stances[A.id] = stance;
                if (stance == TS_AT_WAR) {
                    logEvent(day, "The " + A.name + " declared war on the " + B.name, "war");
                    for (int mid : A.memberIds) { Entity* e = entityById(entities, mid); if (e) e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + 18.0f); }
                    for (int mid : B.memberIds) { Entity* e = entityById(entities, mid); if (e) e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + 18.0f); }
                } else if (stance == TS_ALLY) {
                    logEvent(day, "The " + A.name + " and the " + B.name + " formed an alliance", "diplomacy");
                } else if (stance == TS_RIVAL) {
                    logEvent(day, "Tensions rise between the " + A.name + " and the " + B.name, "diplomacy");
                }
            }

            // ── Propagate inter-tribe emotions to individual members each tick ──
            if (stance == TS_ALLY || rel > 40.0f) {
                // Alliance love: individuals across tribes build social bonds
                for (int aidx : A.memberIds) {
                    if (roll(rng) > 0.12f) continue;
                    Entity* ea = entityById(entities, aidx);
                    if (!ea || ea->entityHealth <= 0.0f) continue;
                    for (int bidx : B.memberIds) {
                        if (roll(rng) > 0.12f) continue;
                        Entity* eb = entityById(entities, bidx);
                        if (!eb || eb->entityHealth <= 0.0f) continue;
                        int sidx = ea->contains(ea->list_entityPointedSocial, eb, 4);
                        if (sidx == -1) {
                            if (roll(rng) < 0.20f) {
                                entityPointedSocial ns; ns.Id = eb->entityId; ns.pointedEntity = eb; ns.social = 6.0f;
                                ea->list_entityPointedSocial.push_back(ns);
                            }
                        } else {
                            ea->list_entityPointedSocial[sidx].social = std::min(100.0f, ea->list_entityPointedSocial[sidx].social + 0.8f);
                        }
                    }
                }
            } else if (stance == TS_RIVAL || stance == TS_AT_WAR) {
                // Rivalry/War: members grow anger and discrimination toward enemy tribe
                float angerRate = (stance == TS_AT_WAR) ? 0.18f : 0.07f;
                float angerInc  = (stance == TS_AT_WAR) ? 3.0f  : 0.9f;
                for (int aidx : A.memberIds) {
                    if (roll(rng) > angerRate) continue;
                    Entity* ea = entityById(entities, aidx);
                    if (!ea || ea->entityHealth <= 0.0f) continue;
                    for (int bidx : B.memberIds) {
                        if (roll(rng) > angerRate) continue;
                        Entity* eb = entityById(entities, bidx);
                        if (!eb || eb->entityHealth <= 0.0f) continue;
                        int angIdx = ea->contains(ea->list_entityPointedAnger, eb, 2);
                        if (angIdx == -1) {
                            entityPointedAnger na; na.Id = eb->entityId; na.pointedEntity = eb; na.anger = angerInc;
                            ea->list_entityPointedAnger.push_back(na);
                        } else {
                            ea->list_entityPointedAnger[angIdx].anger = std::min(100.0f, ea->list_entityPointedAnger[angIdx].anger + angerInc * 0.4f);
                        }
                        // War also directly damages social bonds (hatred erodes connection)
                        if (stance == TS_AT_WAR) {
                            int sIdx = ea->contains(ea->list_entityPointedSocial, eb, 4);
                            if (sIdx != -1) ea->list_entityPointedSocial[sIdx].social = std::max(0.0f, ea->list_entityPointedSocial[sIdx].social - 0.5f);
                        }
                    }
                }
            }
        }
    }
}

// ── Era progression ────────────────────────────────────────────────────────────
void CivilizationEngine::updateEra(const std::vector<Entity>& entities) {
    int pop = (int)entities.size();
    int innCount = (int)innovations.size();
    int tribeCount = (int)tribes.size();

    bool hasAgriculture = false, hasReligion = !religions.empty();
    for (const auto& inv : innovations)
        if (inv.category == "agriculture") { hasAgriculture = true; break; }

    if (innCount >= 15 && tribeCount >= 3 && pop >= 25)
        era = ERA_PROTO_CIVILIZATION;
    else if (innCount >= 5 && (hasAgriculture || hasReligion) && tribeCount >= 2)
        era = ERA_EARLY_CULTURE;
    else if (tribeCount >= 2 && innCount >= 1)
        era = ERA_TRIBAL;
    else
        era = ERA_HUNTER_GATHERER;
}

// ── Civilizational effects on individual entities ─────────────────────────────
void CivilizationEngine::applyEffectsToEntities(std::vector<Entity>& entities, int day) {
    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };

    for (Entity& ent : entities) {
        if (ent.entityHealth <= 0.0f) continue;

        Tribe*    tribe    = findTribe(ent.tribeId);
        Religion* religion = findReligion(ent.religionId);

        // ── Tribe membership effects ─────────────────────────────────────────
        if (tribe) {
            // Belonging reduces loneliness
            ent.entityLoneliness = clamp(ent.entityLoneliness - 1.5f, 0.0f, 100.0f);
            // Collective safety reduces stress slightly
            ent.entityStress     = clamp(ent.entityStress     - 0.8f, 0.0f, 100.0f);

            // At war: fear and anger spike
            bool atWar = false;
            for (auto& p : tribe->stances)
                if (p.second == TS_AT_WAR) { atWar = true; break; }
            if (atWar) {
                ent.entityStress        = clamp(ent.entityStress        + 2.5f, 0.0f, 100.0f);
                ent.entityGeneralAnger  = clamp(ent.entityGeneralAnger  + 1.0f, 0.0f, 100.0f);
            }

            // Leader: responsibility stress + esteem boost
            if (tribe->leaderId == ent.entityId) {
                ent.entityStress = clamp(ent.entityStress + 1.5f, 0.0f, 100.0f);
                ent.Esteem       = clamp(ent.Esteem       + 2.5f, 0.0f, 100.0f);
            }

            // Assign specialization based on dominant personality
            if (ent.specialization.empty()) {
                float maxTrait = std::max({ ent.personality.extraversion,
                                            ent.personality.agreeableness,
                                            ent.personality.conscientiousness,
                                            ent.personality.openness,
                                            100.0f - ent.personality.agreeableness });
                if (maxTrait == ent.personality.openness             ) ent.specialization = "scholar";
                else if (maxTrait == ent.personality.conscientiousness) ent.specialization = "craftsman";
                else if (maxTrait == ent.personality.extraversion     ) ent.specialization = "trader";
                else if (maxTrait == ent.personality.agreeableness    ) ent.specialization = "healer";
                else                                                    ent.specialization = "warrior";
            }
        }

        // ── Religion effects ─────────────────────────────────────────────────
        if (religion) {
            ent.entityStress      = clamp(ent.entityStress      - 1.2f, 0.0f, 100.0f);
            ent.entityMentalHealth = clamp(ent.entityMentalHealth + 0.5f, 0.0f, 100.0f);
            // High-demand religion → stronger community bond
            if (religion->spiritualDemand > 65.0f)
                ent.entityLoneliness = clamp(ent.entityLoneliness - 2.0f, 0.0f, 100.0f);
        }

        // ── Innovation / specialization effects ──────────────────────────────
        if (!ent.knownTechIds.empty()) {
            ent.entityBoredom  = clamp(ent.entityBoredom  - 0.8f, 0.0f, 100.0f);
            ent.entityHapiness = clamp(ent.entityHapiness + 0.3f, 0.0f, 100.0f);
        }

        // Tech-specific entity benefits
        bool hasAgriculture = false, hasMedicine = false, hasQuarantine = false;
        for (int tid : ent.knownTechIds) {
            Innovation* inv = findInnovation(tid);
            if (!inv) continue;
            if (inv->category == "agriculture") hasAgriculture = true;
            if (inv->category == "medicine")    hasMedicine    = true;
            if (inv->name    == "Quarantine")   hasQuarantine  = true;
        }
        // Agriculture: steady passive health recovery from better nutrition
        if (hasAgriculture && ent.entityDiseaseType == -1)
            ent.entityHealth = clamp(ent.entityHealth + 0.15f, 0.0f, 100.0f);
        // Medicine: partially counteract disease damage each tick
        if (hasMedicine && ent.entityDiseaseType != -1) {
            ent.entityHealth  = clamp(ent.entityHealth  + 0.6f, 0.0f, 100.0f);
            ent.entityAntiBody = clamp(ent.entityAntiBody + 1.5f, 0.0f, 100.0f);
        }
        // Quarantine knowledge: reduces chance of catching new diseases (antibody buffer)
        if (hasQuarantine && ent.entityAntiBody < 40.0f)
            ent.entityAntiBody = clamp(ent.entityAntiBody + 0.5f, 0.0f, 100.0f);
    }
}

// ── Era / Summary ─────────────────────────────────────────────────────────────
std::string CivilizationEngine::getEraName() const {
    switch (era) {
        case ERA_HUNTER_GATHERER:   return "Hunter-Gatherer Age";
        case ERA_TRIBAL:            return "Tribal Age";
        case ERA_EARLY_CULTURE:     return "Early Culture";
        case ERA_PROTO_CIVILIZATION:return "Proto-Civilization";
    }
    return "Unknown";
}

std::string CivilizationEngine::getEraSummary() const {
    std::ostringstream ss;
    ss << "Era: " << getEraName()
       << " | Tribes: " << tribes.size()
       << " | Religions: " << religions.size()
       << " | Innovations: " << innovations.size();
    return ss.str();
}

// ── Naming systems ────────────────────────────────────────────────────────────
std::string CivilizationEngine::tribeName(const Entity* leader) {
    float ext = leader->personality.extraversion;
    float agr = leader->personality.agreeableness;
    float con = leader->personality.conscientiousness;
    float opn = leader->personality.openness;
    float neu = leader->personality.neuroticism;
    float maxTrait = std::max({ext, agr, con, opn, neu});

    std::string prefix;
    if      (maxTrait == ext) prefix = pick<std::string>({"Great",     "Rising",    "Loud",       "United"});
    else if (maxTrait == agr) prefix = pick<std::string>({"Gentle",    "Peaceful",  "Open",       "Kind"  });
    else if (maxTrait == con) prefix = pick<std::string>({"True",      "Steadfast", "Faithful",   "Ordered"});
    else if (maxTrait == opn) prefix = pick<std::string>({"Wandering", "Ancient",   "Curious",    "Seeing"});
    else                      prefix = pick<std::string>({"Iron",      "Storm",     "Dark",       "Hard"  });

    std::string suffix = pick<std::string>({"Clan","Kin","People","Circle","Lodge","Band","Tribe"});
    return prefix + " " + suffix;
}

std::string CivilizationEngine::religionName(const Entity* founder) {
    bool hasGrief = false;
    for (const auto& g : founder->griefStates) if (g.intensity > 0.3f) { hasGrief = true; break; }

    std::string core;
    if (hasGrief)
        core = pick<std::string>({"the Departed","the Return","the Passage","the Lost Ones"});
    else if (founder->entityStress > 60.0f)
        core = pick<std::string>({"the Balance","the Reckoning","the Order","the Judge"});
    else if (founder->personality.openness > 65.0f)
        core = pick<std::string>({"the Great Cycle","the Infinite","the Weave","the Pattern"});
    else
        core = pick<std::string>({"the Light","the Way","the Becoming","the Source"});

    std::string form = pick<std::string>({"Children of","Path of","Way of","Seekers of","Servants of"});
    return form + " " + core;
}

// ── Lookup helpers ────────────────────────────────────────────────────────────
Tribe* CivilizationEngine::findTribe(int id) {
    if (id < 0) return nullptr;
    for (auto& t : tribes) if (t.id == id) return &t;
    return nullptr;
}

Religion* CivilizationEngine::findReligion(int id) {
    if (id < 0) return nullptr;
    for (auto& r : religions) if (r.id == id) return &r;
    return nullptr;
}

Innovation* CivilizationEngine::findInnovation(int id) {
    if (id < 0) return nullptr;
    for (auto& i : innovations) if (i.id == id) return &i;
    return nullptr;
}

Innovation* CivilizationEngine::findInnovationByName(const std::string& name) {
    for (auto& i : innovations) if (i.name == name) return &i;
    return nullptr;
}

Entity* CivilizationEngine::entityById(std::vector<Entity>& entities, int id) {
    for (auto& e : entities) if (e.entityId == id) return &e;
    return nullptr;
}

float CivilizationEngine::computeCharisma(const Entity* ent) const {
    // Charisma = weighted blend of Big Five traits most predictive of social influence
    float ch = ent->personality.extraversion      * 0.35f
             + ent->personality.agreeableness     * 0.25f
             + ent->personality.conscientiousness * 0.20f
             - ent->personality.neuroticism       * 0.15f
             + ent->personality.openness          * 0.05f;
    return std::max(0.0f, std::min(100.0f, ch));
}

void CivilizationEngine::logEvent(int day, const std::string& desc, const std::string& cat) {
    CivEvent ev;
    ev.day         = day;
    ev.description = desc;
    ev.category    = cat;
    eventLog.push_front(ev);
    if (eventLog.size() > 120) eventLog.pop_back();
}
