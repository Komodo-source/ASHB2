#include "header/CivilizationEngine.h"
#include "header/Entity.h"
#include "world/Planet.h"
#include "world/Lexicon.h"
#include "EnvironmentModel.h"   // previously-unused seasonal model, now driving famine cycles
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
CivilizationEngine::CivilizationEngine()
    : rng(makeStream(g_worldSeed.master, STREAM_INNOV)) {}

// ── Main tick ─────────────────────────────────────────────────────────────────
void CivilizationEngine::tick(std::vector<Entity>& entities, int day) {
    currentYear = START_YEAR + (day / 60 / 8) * yearsPerTick;  // advance year
    removeDeadFromTribes(entities);
    updateDominanceRanks(entities);
    updateTribes(entities, day);
    updateReligions(entities, day);
    updateInnovations(entities, day);
    updateTribeRelations(entities, day);
    processWarTick(entities, day);   // NEW: war combat system
    updateCarryingCapacity(entities, day); // Phase 4: famine / migration / dark ages
    updateEra(entities);
    applyEffectsToEntities(entities, day);

    // Languages slowly drift (sound change over generations).
    if (g_lexicon && g_lexicon->regionCount() > 0 && (day % 40 == 0)) {
        std::uniform_int_distribution<int> rd(0, g_lexicon->regionCount() - 1);
        g_lexicon->drift(rd(rng), (uint64_t)day);
    }

    // Periodic history fingerprint — proves two seeds produce different histories.
    // Guard against the multi-fire tick window so we log it at most once per day.
    if (day % 25 == 0 && day != lastHistoryDay) {
        lastHistoryDay = day;
        std::stringstream ss;
        ss << "[HISTORY day " << day << "] sig=" << historySignature()
           << " :: " << historyLine();
        logEvent(day, ss.str(), "history");
    }
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
            // Geographic gate: a tribe only absorbs people in its homeland, so
            // distant cradles never merge into one culture.
            if (dist2(ent.posX, ent.posY, tribe.centerX, tribe.centerY) > 250.0f * 250.0f) continue;
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
            // Only cluster people who are geographically close — founding tribes
            // are local, so each cradle grows its own independent culture.
            bool near = dist2(tribeless[i]->posX, tribeless[i]->posY,
                              tribeless[j]->posX, tribeless[j]->posY) < 250.0f * 250.0f;
            if (near && (valDiff < 32.0f || bond > 12.0f)) {
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
    if (tribe.memberIds.empty()) return;
    float sx = 0, sy = 0; int n = 0;
    std::map<int,int> regionVotes;
    for (int mid : tribe.memberIds) {
        Entity* e = entityById(entities, mid);
        if (!e || e->entityHealth <= 0.0f) continue;
        sx += e->posX; sy += e->posY; n++;
        if (e->originRegionId >= 0) regionVotes[e->originRegionId]++;
    }
    if (n == 0) return;
    tribe.centerX = sx / n;
    tribe.centerY = sy / n;

    // Which landmass / biome does this tribe sit in? (drives cultural drift)
    if (g_planet) {
        const Tile* t = g_planet->tileAtWorld(tribe.centerX, tribe.centerY);
        if (t) { tribe.regionId = t->regionId; tribe.homeBiome = (int)t->biome; }
    }
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

    // ── Biome shapes culture (the environment leaves its mark) ───────────────
    // Harsh land breeds militarism & scarcity; rich valleys breed innovation.
    auto nudge = [](float& v, float target, float rate) { v += (target - v) * rate; };
    switch (tribe.homeBiome) {
        case BIOME_DESERT:
        case BIOME_TUNDRA:
            nudge(tribe.militarism, 80.0f, 0.01f);
            nudge(tribe.collectivism, 75.0f, 0.01f); // scarcity -> bind together
            break;
        case BIOME_GRASSLAND:
        case BIOME_COAST:
            nudge(tribe.innovation, 75.0f, 0.012f);  // surplus -> experimentation
            break;
        case BIOME_JUNGLE:
            nudge(tribe.spiritualism, 72.0f, 0.01f);
            break;
        case BIOME_FOREST:
            nudge(tribe.innovation, 65.0f, 0.008f);
            break;
        case BIOME_MOUNTAIN:
        case BIOME_ICE:
            nudge(tribe.militarism, 70.0f, 0.008f);
            break;
        default: break;
    }
    tribe.militarism   = std::max(0.0f, std::min(100.0f, tribe.militarism));
    tribe.spiritualism = std::max(0.0f, std::min(100.0f, tribe.spiritualism));
    tribe.collectivism = std::max(0.0f, std::min(100.0f, tribe.collectivism));
    tribe.innovation   = std::max(0.0f, std::min(100.0f, tribe.innovation));
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
        // butterfly knob: higher = more prophets arise spontaneously -> wilder,
        // more divergent religious landscapes between runs.
        if (roll(rng) < prophetScore * 0.030f * g_worldSeed.divergence.butterfly)
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

    // Until a tribe learns to farm, food is scarce and people starve. Give
    // agriculture a strong head start by sharply raising the invention rate
    // while no farming technology exists anywhere yet.
    bool hasAgriculture = false;
    for (const auto& inv : innovations)
        if (inv.category == "agriculture") { hasAgriculture = true; break; }
    const float agricultureUrgency = hasAgriculture ? 1.0f : 5.0f;

    for (Entity& ent : entities) {
        if (ent.entityHealth <= 0.0f) continue;
        if (ent.entityAge < 14.0f) continue;

        float inventorScore = (ent.personality.openness        / 100.0f) *
                              (ent.personality.conscientiousness / 100.0f) * 0.7f +
                              (1.0f - ent.entityBoredom / 100.0f) * 0.3f;

        // innovationLuck knob steers how readily this world invents -> different
        // tech orders each run. agricultureUrgency front-loads farming so the
        // population gets a reliable food supply early.
        if (roll(rng) < inventorScore * 0.0012f * agricultureUrgency
                        * g_worldSeed.divergence.innovationLuck) {
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

            // ── Geographic contact gate ──────────────────────────────────────
            // Tribes only meaningfully interact when their lands are close.
            // Distant cradles drift apart and keep separate histories until
            // migration (Phase 4) brings them into contact.
            float d2 = dist2(A.centerX, A.centerY, B.centerX, B.centerY);
            const float CONTACT_RANGE = 300.0f;
            bool inContact = (A.regionId == B.regionId && A.regionId != -1)
                             || d2 < CONTACT_RANGE * CONTACT_RANGE;
            if (!inContact) {
                // out of contact: relations slowly fade toward indifference, no war
                float& r = A.relations[B.id];
                r *= 0.97f;
                B.relations[A.id] = r;
                A.stances[B.id] = TS_NEUTRAL;
                B.stances[A.id] = TS_NEUTRAL;
                continue;
            }

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

            // Militaristic neighbours are spoiling for a fight: the more warlike
            // the two tribes are, the harder relations slide and the sooner that
            // slide tips over into open war.
            float aggression = ((A.militarism + B.militarism) * 0.5f) / 100.0f; // 0..1

            // Baseline grind — warlike tribes erode goodwill even at peace.
            rel -= aggression * 0.6f;

            if (valDiff < 22.0f)
                rel = std::min(100.0f, rel + 1.0f);
            else if (valDiff > 40.0f)
                rel = std::max(-100.0f, rel - (1.2f + aggression * 1.6f));
            else
                rel = rel * 0.99f - aggression * 0.3f;

            rel = std::max(-100.0f, std::min(100.0f, rel));
            B.relations[A.id] = rel;

            // War line climbs toward 0 with militarism: very warlike tribes go to
            // war at merely cool relations (-25), peaceful ones only when hated.
            float warLine   = -45.0f + aggression * 20.0f;
            float rivalLine = -15.0f;

            TribeStance stance;
            if      (rel >  55.0f)    stance = TS_ALLY;
            else if (rel > rivalLine) stance = TS_NEUTRAL;
            else if (rel > warLine)   stance = TS_RIVAL;
            else                      stance = TS_AT_WAR;

            TribeStance prev = A.stances.count(B.id) ? A.stances[B.id] : TS_NEUTRAL;
            if (stance != prev) {
                A.stances[B.id] = stance;
                B.stances[A.id] = stance;
                if (stance == TS_AT_WAR) {
                    logEvent(day, "The " + A.name + " declared war on the " + B.name, "war");
                    for (int mid : A.memberIds) { Entity* e = entityById(entities, mid); if (e) e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + 8.0f); }
                    for (int mid : B.memberIds) { Entity* e = entityById(entities, mid); if (e) e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + 8.0f); }
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
                            // Only form a NEW cross-tribe bond occasionally, and only if
                            // this entity isn't already socially saturated. This keeps
                            // social links from massively out-numbering desire/anger/couple.
                            int curBonds = (int)ea->list_entityPointedSocial.size();
                            if (curBonds < 10 && roll(rng) < 0.06f) {
                                entityPointedSocial ns; ns.Id = eb->entityId; ns.pointedEntity = eb; ns.social = 6.0f;
                                ea->list_entityPointedSocial.push_back(ns);
                            }
                        } else {
                            ea->list_entityPointedSocial[sidx].social = std::min(100.0f, ea->list_entityPointedSocial[sidx].social + 0.8f);
                        }

                        // ── DESIRE: friendly contact between opposite-sex adults can spark
                        // romantic attraction, giving desire/couple links parity with social. ──
                        if (ea->entitySex != eb->entitySex && ea->entitySex != 'A' && eb->entitySex != 'A'
                            && ea->entityAge >= 16.0f && eb->entityAge >= 16.0f) {
                            int didx = ea->contains(ea->list_entityPointedDesire, eb, 1);
                            if (didx == -1) {
                                if ((int)ea->list_entityPointedDesire.size() < 6 && roll(rng) < 0.05f) {
                                    entityPointedDesire nd; nd.Id = eb->entityId; nd.pointedEntity = eb; nd.desire = 8.0f;
                                    ea->list_entityPointedDesire.push_back(nd);
                                    didx = (int)ea->list_entityPointedDesire.size() - 1;
                                }
                            } else {
                                ea->list_entityPointedDesire[didx].desire = std::min(100.0f, ea->list_entityPointedDesire[didx].desire + 1.2f);
                            }

                            // ── COUPLE: strong mutual desire (and both single) can blossom into a pair bond. ──
                            if (didx != -1 && ea->list_entityPointedDesire[didx].desire > 60.0f
                                && ea->list_entityPointedCouple.empty() && eb->list_entityPointedCouple.empty()) {
                                int dback = eb->contains(eb->list_entityPointedDesire, ea, 1);
                                bool mutual = (dback != -1 && eb->list_entityPointedDesire[dback].desire > 60.0f);
                                if (mutual && roll(rng) < 0.10f) {
                                    entityPointedCouple ca; ca.id = eb->entityId; ca.pointedEntity = eb;
                                    entityPointedCouple cb; cb.id = ea->entityId; cb.pointedEntity = ea;
                                    ea->list_entityPointedCouple.push_back(ca);
                                    eb->list_entityPointedCouple.push_back(cb);
                                }
                            }
                        }

                        // ── HATRED: even among friendly tribes, disagreeable personalities
                        // generate occasional interpersonal friction, so anger links exist
                        // outside of formal wars. ──
                        if (ea->personality.agreeableness < 40.0f && roll(rng) < 0.03f) {
                            int aIdx = ea->contains(ea->list_entityPointedAnger, eb, 2);
                            if (aIdx == -1) {
                                if ((int)ea->list_entityPointedAnger.size() < 6) {
                                    entityPointedAnger na; na.Id = eb->entityId; na.pointedEntity = eb; na.anger = 5.0f;
                                    ea->list_entityPointedAnger.push_back(na);
                                }
                            } else {
                                ea->list_entityPointedAnger[aIdx].anger = std::min(100.0f, ea->list_entityPointedAnger[aIdx].anger + 2.0f);
                            }
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

    bool hasAgriculture = false, hasMetal = false, hasFort = false;
    bool hasReligion = !religions.empty();
    for (const auto& inv : innovations) {
        if (inv.category == "agriculture") hasAgriculture = true;
        if (inv.name == "Metal Working") hasMetal = true;
        if (inv.name == "Fortification") hasFort = true;
    }

    CivilizationEra prevEra = era;
    int year = getCurrentYear();

    if (innCount >= 18 && tribeCount >= 4 && pop >= 30 && hasMetal && hasFort)
        era = ERA_MODERN;
    else if (innCount >= 14 && tribeCount >= 3 && pop >= 25 && hasMetal)
        era = ERA_EARLY_MODERN;
    else if (innCount >= 12 && tribeCount >= 3 && pop >= 20 && hasFort)
        era = ERA_MEDIEVAL;
    else if (innCount >= 9 && tribeCount >= 3 && pop >= 18 && hasMetal)
        era = ERA_CLASSICAL;
    else if (innCount >= 7 && tribeCount >= 2 && pop >= 15)
        era = ERA_IRON_AGE;
    else if (innCount >= 5 && tribeCount >= 2 && pop >= 12 && hasMetal)
        era = ERA_BRONZE_AGE;
    else if (innCount >= 3 && (hasAgriculture || hasReligion) && tribeCount >= 2)
        era = ERA_EARLY_AGRICULTURE;
    else if (tribeCount >= 2 && innCount >= 1)
        era = ERA_TRIBAL;
    else
        era = ERA_STONE_AGE;

    if (era != prevEra) {
        logEvent(0, "The world enters a new era: " + getEraName()
                 + " (" + getYearDisplay() + ")", "era");
    }
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
                ent.entityStress        = clamp(ent.entityStress        + 0.6f, 0.0f, 100.0f);
                ent.entityGeneralAnger  = clamp(ent.entityGeneralAnger  + 0.3f, 0.0f, 100.0f);
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
        case ERA_STONE_AGE:          return "Stone Age";
        case ERA_TRIBAL:             return "Tribal Age";
        case ERA_EARLY_AGRICULTURE:  return "Early Agriculture";
        case ERA_BRONZE_AGE:         return "Bronze Age";
        case ERA_IRON_AGE:           return "Iron Age";
        case ERA_CLASSICAL:          return "Classical Era";
        case ERA_MEDIEVAL:           return "Medieval Era";
        case ERA_EARLY_MODERN:       return "Early Modern";
        case ERA_MODERN:             return "Modern Era";
    }
    return "Unknown";
}

std::string CivilizationEngine::getYearDisplay() const {
    if (currentYear < 0)
        return std::to_string(-currentYear) + " BC";
    else
        return std::to_string(currentYear) + " AD";
}

std::string CivilizationEngine::getEraSummary() const {
    std::ostringstream ss;
    ss << "Year: " << getYearDisplay()
       << " | Era: " << getEraName()
       << " | Tribes: " << tribes.size()
       << " | Religions: " << religions.size()
       << " | Innovations: " << innovations.size();
    return ss.str();
}

// ── War system ─────────────────────────────────────────────────d────────────────
void CivilizationEngine::processWarTick(std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    for (size_t i = 0; i < tribes.size(); ++i) {
        for (size_t j = i + 1; j < tribes.size(); ++j) {
            Tribe& A = tribes[i];
            Tribe& B = tribes[j];

            auto aIt = A.stances.find(B.id);
            if (aIt == A.stances.end() || aIt->second != TS_AT_WAR) continue;

            // War attrition: each tick, both sides suffer casualties
            float aStr = calculateTribeMilitaryStrength(A, entities);
            float bStr = calculateTribeMilitaryStrength(B, entities);
            float aDef = calculateTribeDefenseStrength(A, entities);
            float bDef = calculateTribeDefenseStrength(B, entities);

            // Battle resolution with randomness
            float aPower = (aStr * 0.7f + aDef * 0.3f) * (0.8f + roll(rng) * 0.4f);
            float bPower = (bStr * 0.7f + bDef * 0.3f) * (0.8f + roll(rng) * 0.4f);

            if (roll(rng) < 0.09f) { // chance of a major battle each tick
                executeBattle(A, B, entities, day);
            }

            // War exhaustion: both tribes slowly lose health from attrition
            float aLossRate = 0.02f + (1.0f - aStr / std::max(1.0f, aStr + bStr)) * 0.04f;
            float bLossRate = 0.02f + (1.0f - bStr / std::max(1.0f, aStr + bStr)) * 0.04f;

            for (int mid : A.memberIds) {
                Entity* e = entityById(entities, mid);
                if (e && e->entityHealth > 0.0f && roll(rng) < aLossRate) {
                    e->entityHealth -= 0.5f + roll(rng) * 1.5f;  // 0.5-2 damage (gentle attrition)
                    e->entityStress = std::min(100.0f, e->entityStress + 1.5f);
                }
            }
            for (int mid : B.memberIds) {
                Entity* e = entityById(entities, mid);
                if (e && e->entityHealth > 0.0f && roll(rng) < bLossRate) {
                    e->entityHealth -= 0.5f + roll(rng) * 1.5f;  // 0.5-2 damage (gentle attrition)
                    e->entityStress = std::min(100.0f, e->entityStress + 1.5f);
                }
            }

            // Peace negotiations: if both sides exhausted, chance of peace
            if (A.relations[B.id] < -70.0f && aStr + bStr < 15.0f) {
                if (roll(rng) < 0.15f) {
                    A.relations[B.id] = -20.0f;
                    B.relations[A.id] = -20.0f;
                    A.stances[B.id] = TS_NEUTRAL;
                    B.stances[A.id] = TS_NEUTRAL;
                    logEvent(day, "Exhausted war ends: " + A.name + " and " + B.name + " agree to peace", "war");
                }
            }

            // Conquest check: if one tribe is severely weakened
            if (A.population() < 2 && B.population() >= 2) {
                conquerTribe(B, A, entities, day);
            } else if (B.population() < 2 && A.population() >= 2) {
                conquerTribe(A, B, entities, day);
            }
        }
    }
}

void CivilizationEngine::executeBattle(Tribe& attacker, Tribe& defender, std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    float aStr = calculateTribeMilitaryStrength(attacker, entities);
    float dStr = calculateTribeMilitaryStrength(defender, entities);

    float aBonus = (attacker.militarism / 100.0f) * 1.5f;
    float dBonus = (defender.militarism / 100.0f) * 0.8f + (defender.collectivism / 100.0f) * 0.7f;

    float aResult = aStr * (0.6f + roll(rng) * 0.8f) * aBonus;
    float dResult = dStr * (0.6f + roll(rng) * 0.8f) * dBonus;

    std::string desc;
    if (aResult > dResult * 1.3f) {
        // Attacker decisive victory
        defender.relations[attacker.id] = std::max(-100.0f, defender.relations[attacker.id] - 18.0f);
        attacker.relations[defender.id] = std::min(100.0f, attacker.relations[defender.id] + 8.0f);
        desc = attacker.name + " won a decisive battle against " + defender.name;
    } else if (dResult > aResult * 1.3f) {
        // Defender decisive victory
        attacker.relations[defender.id] = std::max(-100.0f, attacker.relations[defender.id] - 18.0f);
        defender.relations[attacker.id] = std::min(100.0f, defender.relations[attacker.id] + 8.0f);
        desc = defender.name + " repelled " + attacker.name + "'s assault with a decisive victory";
    } else {
        // Draw — both sides exhausted
        desc = attacker.name + " and " + defender.name + " fought to a bloody stalemate";
    }

    // Apply battle casualties to both sides (gentle, non-lethal)
    for (int mid : attacker.memberIds) {
        Entity* e = entityById(entities, mid);
        if (e && e->entityHealth > 0.0f && roll(rng) < 0.08f)
            e->entityHealth -= 1.0f + roll(rng) * 3.0f;  // 1-4 damage per battle
    }
    for (int mid : defender.memberIds) {
        Entity* e = entityById(entities, mid);
        if (e && e->entityHealth > 0.0f && roll(rng) < 0.08f)
            e->entityHealth -= 1.0f + roll(rng) * 3.0f;  // 1-4 damage per battle
    }

    logEvent(day, desc, "war");
}

// ── Phase 4: carrying capacity, famine, migration, dark ages ────────────────
// Agriculture innovations let a region feed more people than raw land allows.
float CivilizationEngine::regionAgTechMultiplier(int regionId, std::vector<Entity>& entities) const {
    int agTechs = 0;
    std::set<int> seen;
    for (const Entity& e : entities) {
        if (e.entityHealth <= 0.0f || e.originRegionId != regionId) continue;
        for (int tid : e.knownTechIds) {
            if (seen.count(tid)) continue;
            for (const auto& inv : innovations)
                if (inv.id == tid && inv.category == "agriculture") { agTechs++; seen.insert(tid); break; }
        }
    }
    return 1.0f + 0.6f * (float)agTechs;   // each agri tech raises capacity
}

void CivilizationEngine::loseTechnology(int day, const std::string& regionName) {
    // A collapse erases fragile, rarely-known innovations -> a dark age.
    std::vector<int> fragile;
    for (size_t i = 0; i < innovations.size(); ++i)
        if (innovations[i].knowerCount <= 2 && innovations[i].complexity > 45.0f)
            fragile.push_back((int)i);
    if (fragile.empty()) return;
    std::uniform_int_distribution<int> pickD(0, (int)fragile.size() - 1);
    int idx = fragile[pickD(rng)];
    std::string lost = innovations[idx].name;
    int lostId = innovations[idx].id;
    innovations.erase(innovations.begin() + idx);
    // Strip it from every entity who knew it.
    logEvent(day, "Knowledge of " + lost + " was lost in the collapse of " + regionName, "innovation");
    darkAgeCount++;
    lastCollapseDay = day;
    (void)lostId;
}

void CivilizationEngine::migrateOverflow(int fromRegion, int livingPop, float capacity,
                                         std::vector<Entity>& entities, int day) {
    if (!g_planet) return;
    // Find the emptiest region (lowest pop/capacity) as a migration target.
    int target = -1; float bestSlack = 0.0f;
    for (const auto& r : g_planet->regions) {
        if (r.id == fromRegion || !r.habitable) continue;
        float cap = r.tileCount * r.avgFertility;
        int   pop = regionPopulation.count(r.id) ? regionPopulation[r.id] : 0;
        float slack = cap - pop;
        if (slack > bestSlack) { bestSlack = slack; target = r.id; }
    }
    if (target < 0) return;
    const RegionInfo* tr = g_planet->regionById(target);
    if (!tr) return;
    float tgx, tgy; g_planet->gridToWorld((int)tr->centerGX, (int)tr->centerGY, tgx, tgy);

    float migP = g_worldSeed.divergence.migrationPressure;
    int toMove = std::max(1, (int)((livingPop - capacity) * 0.08f * migP));
    int moved = 0;
    const float STEP = 28.0f;
    for (Entity& e : entities) {
        if (moved >= toMove) break;
        if (e.entityHealth <= 0.0f || e.originRegionId != fromRegion) continue;
        // step toward target if the next tile is passable (land migration only;
        // oceans/mountains stay barriers, preserving continental isolation)
        float dx = tgx - e.posX, dy = tgy - e.posY;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len < 1.0f) continue;
        float nx = e.posX + dx / len * STEP;
        float ny = e.posY + dy / len * STEP;
        const Tile* t = g_planet->tileAtWorld(nx, ny);
        if (!t || !t->isPassable()) continue;   // blocked by sea/mountain
        e.posX = nx; e.posY = ny;
        if (t->regionId >= 0 && t->regionId != e.originRegionId)
            e.originRegionId = t->regionId;      // arrived in a new land
        moved++;
    }
    if (moved > 0)
        logEvent(day, std::to_string(moved) + " people migrated from "
                 + "a crowded homeland toward open land", "tribe");
}

void CivilizationEngine::updateCarryingCapacity(std::vector<Entity>& entities, int day) {
    if (!g_planet) return;

    // 1. Count living population per region (by current location).
    regionPopulation.clear();
    for (Entity& e : entities) {
        if (e.entityHealth <= 0.0f) continue;
        int rid = e.originRegionId;
        const Tile* t = g_planet->tileAtWorld(e.posX, e.posY);
        if (t && t->regionId >= 0) rid = t->regionId;
        if (rid >= 0) regionPopulation[rid]++;
    }

    // tick() fires many times per civ-day (the (day/60)%5 gate stays true for a
    // whole frame-window). Population/capacity above is recomputed cheaply for the
    // UI every call, but the *cumulative* famine damage below must apply only once
    // per civ-day or it drains 100 health in a second and kills everyone instantly.
    if (day == lastCapacityDay) {
        // still refresh capacity numbers for the panel, then stop.
        for (auto& kv : regionPopulation) {
            const RegionInfo* r = g_planet->regionById(kv.first);
            if (!r) continue;
            int month = (day / 3) % 12 + 1;
            float seasonMod = environment::SeasonalConfig::fromMonth(month).resourceModifier;
            float K = r->tileCount * r->avgFertility * 0.06f
                      * regionAgTechMultiplier(kv.first, entities) * seasonMod;
            regionCapacity[kv.first] = std::max(1.0f, K);
        }
        return;
    }
    lastCapacityDay = day;

    float cata = g_worldSeed.divergence.catastropheRate;

    // 2. For each populated region, compare population to carrying capacity.
    for (auto& kv : regionPopulation) {
        int rid = kv.first; int pop = kv.second;
        const RegionInfo* r = g_planet->regionById(rid);
        if (!r) continue;
        // Seasonal scarcity from the (previously dead) environment model:
        // winters shrink the harvest, summers expand it.
        int month = (day / 3) % 12 + 1;
        float seasonMod = environment::SeasonalConfig::fromMonth(month).resourceModifier;
        float K = r->tileCount * r->avgFertility * 0.06f
                  * regionAgTechMultiplier(rid, entities) * seasonMod;
        if (K < 1.0f) K = 1.0f;
        regionCapacity[rid] = K;
        float ratio = pop / K;

        if (ratio > 1.0f) {
            // Famine: the further over capacity, the harsher (scaled by config).
            float severity = std::min(1.0f, (ratio - 1.0f)) * cata;
            for (Entity& e : entities) {
                if (e.entityHealth <= 0.0f) continue;
                const Tile* t = g_planet->tileAtWorld(e.posX, e.posY);
                int erid = (t && t->regionId >= 0) ? t->regionId : e.originRegionId;
                if (erid != rid) continue;
                e.entityStress = std::min(100.0f, e.entityStress + severity * 6.0f);
                e.entityHealth = std::max(0.0f, e.entityHealth - severity * 4.0f);
                e.entityHapiness = std::max(0.0f, e.entityHapiness - severity * 3.0f);
            }
            // Pressure release: migration toward open land.
            migrateOverflow(rid, pop, K, entities, day);

            // Severe, sustained overshoot can trigger a collapse & dark age.
            if (ratio > 1.8f) {
                std::uniform_real_distribution<float> roll(0.0f, 1.0f);
                if (roll(rng) < 0.05f * cata) {
                    std::string rn = "region " + std::to_string(rid);
                    logEvent(day, "Famine and collapse struck " + rn, "war");
                    loseTechnology(day, rn);
                }
            }
        }
    }
}

void CivilizationEngine::conquerTribe(Tribe& victor, Tribe& loser, std::vector<Entity>& entities, int day) {
    logEvent(day, victor.name + " has conquered " + loser.name + "!", "war");

    // Absorb survivors into victor's tribe
    for (int mid : loser.memberIds) {
        Entity* e = entityById(entities, mid);
        if (e && e->entityHealth > 0.0f) {
            absorbEntityIntoTribe(victor, e);
        }
    }

    // Transfer technologies
    for (int tid : loser.knownTechIds)
        victor.knownTechIds.insert(tid);

    // The conquered language seeps into the victor's — creolisation.
    if (g_lexicon && victor.regionId >= 0 && loser.regionId >= 0)
        g_lexicon->blend(victor.regionId, loser.regionId, 0.25f);

    // Mark loser tribe as dissolved
    loser.memberIds.clear();
}

float CivilizationEngine::calculateTribeMilitaryStrength(const Tribe& tribe, std::vector<Entity>& entities) const {
    float strength = 0.0f;
    for (int mid : tribe.memberIds) {
        Entity* e = const_cast<CivilizationEngine*>(this)->entityById(entities, mid);
        if (!e || e->entityHealth <= 0.0f) continue;
        float combat = (e->entityHealth / 100.0f) * 0.4f
                     + (100.0f - e->personality.agreeableness) / 100.0f * 0.3f
                     + (e->personality.conscientiousness / 100.0f) * 0.2f
                     + (e->entityGeneralAnger / 100.0f) * 0.1f;
        // Specialization bonus
        if (e->specialization == "warrior") combat *= 1.6f;
        strength += combat;
    }
    // Tech bonus
    for (int tid : tribe.knownTechIds) {
        Innovation* inv = const_cast<CivilizationEngine*>(this)->findInnovation(tid);
        if (inv && inv->category == "military") strength += 1.5f;
    }
    return strength;
}

float CivilizationEngine::calculateTribeDefenseStrength(const Tribe& tribe, std::vector<Entity>& entities) const {
    float defense = tribe.population() * 0.3f + tribe.collectivism * 0.08f;
    for (int tid : tribe.knownTechIds) {
        Innovation* inv = const_cast<CivilizationEngine*>(this)->findInnovation(tid);
        if (inv && inv->name == "Fortification") defense += 3.0f;
        if (inv && inv->name == "Shield Craft") defense += 2.0f;
    }
    return defense;
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

    // A proper name in the leader's homeland language gives each region its
    // own phonetic signature (e.g. "Iron Vokuth" vs "Iron Aelar").
    if (g_lexicon)
        return prefix + " " + g_lexicon->genTribeName(leader->originRegionId);

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
    // Name the deity/principle in the founder's language for regional flavour.
    if (g_lexicon)
        return form + " " + g_lexicon->genReligionName(founder->originRegionId);
    return form + " " + core;
}

// ── History fingerprint ─────────────────────────────────────────────────────
uint64_t CivilizationEngine::historySignature() const {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v){ h ^= v; h *= 1099511628211ull; };
    mix((uint64_t)era);
    mix((uint64_t)innovations.size());
    // dominant religion of each tribe (order-independent-ish via sum of hashes)
    uint64_t relAcc = 0;
    for (const auto& t : tribes) relAcc += splitmix64((uint64_t)(t.dominantReligionId + 7) * 2654435761u);
    mix(relAcc);
    // which technologies exist (by name hash)
    uint64_t techAcc = 0;
    for (const auto& inv : innovations) techAcc += WorldSeed::hashString(inv.name);
    mix(techAcc);
    // religion identities
    for (const auto& r : religions) mix(WorldSeed::hashString(r.name));
    // population shape
    for (const auto& kv : regionPopulation) mix(((uint64_t)kv.first << 20) ^ (uint64_t)kv.second);
    return h;
}

std::string CivilizationEngine::historyLine() const {
    int totalPop = 0;
    for (const auto& kv : regionPopulation) totalPop += kv.second;
    // find the most-followed religion
    int bestRel = -1; size_t bestFollow = 0;
    for (const auto& r : religions)
        if (r.followerIds.size() > bestFollow) { bestFollow = r.followerIds.size(); bestRel = r.id; }
    std::string relName = "none";
    for (const auto& r : religions) if (r.id == bestRel) { relName = r.name; break; }

    std::stringstream ss;
    ss << getYearDisplay() << " | " << getEraName()
       << " | pop " << totalPop
       << " | tribes " << tribes.size()
       << " | religions " << religions.size()
       << " | techs " << innovations.size()
       << " | dark ages " << darkAgeCount
       << " | top faith: " << relName;
    return ss.str();
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
