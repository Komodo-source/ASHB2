#include "header/CivilizationEngine.h"
#include "header/Economics.h"
#include "header/Entity.h"
#include "world/Planet.h"
#include "world/PheromoneField.h"   // Step 4: battles poison the ground with danger scent
#include "world/Lexicon.h"
#include "world/ResourceSystem.h"   // per-region resource pools feed craftsmen
#include "EnvironmentModel.h"   // previously-unused seasonal model, now driving famine cycles
#include "header/TechTree.h"    // structured, prerequisite-gated technology tree
#include "header/Kinship.h"     // family/dynasty registry (Plan 4.1)
#include "header/Logging.h"     // persist civilization events to civilization_log.txt
#include <algorithm>
#include <cmath>
#include <sstream>
#include <numeric>
#include <vector>

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
     {"Vaccine/Medication",   "medicine",   "Efficient method for helping to get better faster ",    50, {}},
     {"Autophagy ",   "medicine",   "Method used in order to renew new cells",    75, {"Vaccine/Medication"}},
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

    // ── Classical → Industrial ceiling extension (Improvement Plan 1.2) ─────────
    // Raises the discoverable-tech ceiling from 31 to 53 and provides the
    // prerequisite-gated "chokepoint" techs (Writing, Iron Smelting, Gunpowder,
    // Scientific Method, Steam Power) that the era ladder now keys off, so a
    // civilisation can actually climb past the Medieval plateau.
    {"Writing",           "social",     "Recording language in durable symbols",     55, {"Oral Record","Counting Method"}},
    {"Currency",          "social",     "Standardised money for exchange",           50, {"Trade Token","Counting Method"}},
    {"Mathematics",       "social",     "Abstract calculation and geometry",         65, {"Counting Method","Writing"}},
    {"Law Code",          "social",     "Codified rules binding a whole people",      60, {"Writing","Conflict Mediation"}},
    {"Philosophy",        "social",     "Systematic reasoning about the world",       72, {"Writing"}},
    {"Banking",           "social",     "Credit, loans and capital",                 74, {"Currency","Writing"}},
    {"Printing",          "social",     "Mechanical reproduction of writing",         80, {"Writing","Masonry"}},
    {"Scientific Method", "social",     "Systematic empirical inquiry",              92, {"Philosophy","Mathematics"}},
    {"Masonry",           "tool",       "Dressed-stone construction",                58, {"Clay Shaping"}},
    {"Wheel & Axle",      "tool",       "Rotational transport and machinery",        60, {"Handle Crafting"}},
    {"Sailing",           "tool",       "Harnessing wind for water travel",          64, {"Rope Braiding"}},
    {"Iron Smelting",     "tool",       "High-heat forging of iron",                 82, {"Metal Working"}},
    {"Optics",            "tool",       "Lenses for sight and study",                84, {"Mathematics","Masonry"}},
    {"Mechanical Clock",  "tool",       "Precise measurement of time",               78, {"Mathematics","Metal Working"}},
    {"Steam Power",       "tool",       "Harnessing steam for mechanical work",      95, {"Scientific Method","Mechanical Clock"}},
    {"Herbal Medicine",   "medicine",   "A systematic pharmacopoeia of remedies",    66, {"Fever Herb","Writing"}},
    {"Sanitation",        "medicine",   "Clean water and waste management",          68, {"Quarantine","Masonry"}},
    {"Cavalry",           "military",   "Mounted warfare",                           58, {"Animal Keeping","Weapon Sharpening"}},
    {"Siege Craft",       "military",   "Engines and tactics to break fortifications",76,{"Fortification","Mathematics"}},
    {"Steel Forging",     "military",   "Superior weapons and armour",               88, {"Iron Smelting"}},
    {"Gunpowder",         "military",   "Explosive powder for war",                  90, {"Iron Smelting"}},
    {"Astronomy",         "spiritual",  "Mapping the heavens for calendar and voyage",70,{"Star Calendar","Mathematics"}},
};

// ── Constructor ───────────────────────────────────────────────────────────────
CivilizationEngine::CivilizationEngine()
    : rng(makeStream(g_worldSeed.master, STREAM_INNOV)) {}

// ── Main tick ─────────────────────────────────────────────────────────────────
void CivilizationEngine::tick(std::vector<Entity>& entities, int day) {
    currentYear = START_YEAR + (day / 60 / 8) * yearsPerTick;  // advance year
    int living = 0;
    for (const Entity& e : entities) if (e.entityHealth > 0.0f) living++;
    if (living > peakPopulation) peakPopulation = living;
    removeDeadFromTribes(entities);
    updateDominanceRanks(entities);
    updateTribes(entities, day);
    updateReligions(entities, day);
    updateInnovations(entities, day);
    updateTribeRelations(entities, day);
    updateDiplomacy(entities, day);  // formal treaties: peace, alliance, trade, tribute
    updateGovernment(entities, day); // legitimacy, coups, vassal upkeep & rebellion
    processWarTick(entities, day);   // NEW: war combat system
    updateCarryingCapacity(entities, day); // Phase 4: famine / migration / dark ages
    updateClimate(entities, day);          // Plan 12: droughts, floods, quakes, eruptions
    updateEra(entities);
    applyEffectsToEntities(entities, day);
    updateDivisionOfLabour(entities, day);   // surplus → specialists; famine → back to the fields
    updateTechTree(entities, day);           // research accrues; tribes climb the tech tree
    updateEconomyResources(entities, day);   // Plan 6: materials/metals/luxury/knowledge + forts
    updateCulture(entities, day);            // Plan 7: artists produce culture; dark ages erode it
    updateTechDiffusion(entities, day);      // Plan 1.4: techs spread tribe→tribe
    // Once-per-civ-day social passes (dynasties, classes, colonisation, sagas).
    if (day != lastDynastyDay) {
        lastDynastyDay = day;
        updateSocialClasses(entities, day);  // Plan 4.2: emergent wealth classes
        updateDynasties(entities, day);      // Plan 4.1: family prestige & great families
        updateColonization(entities, day);   // Plan 13: found colonies in empty land
        updateNarrativeChains(entities, day);// Plan 14: redemption / golden-age sagas
    }

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

        // Structured world-state heartbeat so the post-mortem analyst can chart
        // the civilisation over time (population, tribes, faiths, tech, and the
        // running war/diplomacy tallies) without parsing the prose line.
        int livingPop = 0;
        for (const auto& kv : regionPopulation) livingPop += kv.second;
        std::stringstream sd;
        sd << "kind=snapshot year=\"" << getYearDisplay() << "\""
           << " era=\"" << getEraName() << "\""
           << " population=" << livingPop
           << " peakPopulation=" << peakPopulation
           << " tribes=" << tribes.size()
           << " religions=" << religions.size()
           << " innovations=" << innovations.size()
           << " darkAges=" << darkAgeCount
           << " births=" << totalBirths
           << " deaths=" << totalDeaths
           << " warDeaths=" << totalWarDeaths
           << " warsDeclared=" << totalWarsDeclared
           << " ethnicWars=" << totalEthnicWars
           << " battles=" << totalBattles
           << " conquests=" << totalConquests
           << " treatiesSigned=" << totalTreatiesSigned;
        // Plan 6/7 metrics for the post-mortem: wealth inequality, mean cultural
        // vitality, total great works, and mean fortification level.
        {
            float culSum = 0.0f, fortSum = 0.0f; int works = 0;
            for (const auto& t : tribes) {
                culSum += t.cultureScore; fortSum += t.fortificationLevel;
                works  += t.culturalAchievements;
            }
            int nt = std::max(1, (int)tribes.size());
            sd << " wealthGini=" << wealthGini(entities)
               << " avgCulture=" << (int)(culSum / nt)
               << " culturalWorks=" << works
               << " avgFortification=" << (int)(fortSum / nt)
               << " disasters=" << totalDisasters
               << " civilWars=" << totalCivilWars
               << " colonies=" << totalColonies
               << " greatFamilies=" << totalGreatFamilies
               << " sagas=" << totalSagas
               << " techSpreads=" << totalTechSpreads
               << " classElite=" << eliteCount
               << " classMiddle=" << middleCount
               << " classOutcast=" << outcastCount;
            // Genetic/behavioural drift (Plan 23-lite): mean population personality,
            // so a post-mortem can chart how the gene pool shifts over the run.
            double oSum = 0, nSum = 0; int living = 0;
            for (const auto& e : entities) {
                if (e.entityHealth <= 0.0f) continue;
                oSum += e.personality.openness; nSum += e.personality.neuroticism; ++living;
            }
            if (living > 0)
                sd << " meanOpenness=" << (int)(oSum / living)
                   << " meanNeuroticism=" << (int)(nSum / living);
        }
        logEvent(day, ss.str(), "history", sd.str());
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
    // M7 healer: an entity whose tribeId points at a tribe that no longer
    // exists (dissolved while they weren't on its member roll) is really
    // tribeless — reset it so the clustering pass below can see them again.
    // Without this, a whole generation of dangling ids froze tribe formation.
    for (Entity& ent : entities) {
        if (ent.tribeId >= 0 && findTribe(ent.tribeId) == nullptr)
            ent.tribeId = -1;
    }

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
            if (tribe.population() >= 20) continue; // cap absorption so new tribes can still form
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
        // M9 tuning: 4 founders minimum — 3 let every stray trio incorporate,
        // fragmenting the map into dozens of micro-tribes.
        if (cluster.size() >= 4) {
            if (formTribe(cluster, day)) {
                formedThisTick = true;
                break; // one new tribe per tick — others form on subsequent ticks
            }
        }
    }

    // 2b. Cultural schism — keeps NEW tribes appearing across the whole run,
    // not just at the start. Over time a charismatic member whose values have
    // drifted far from the tribe gathers like-minded followers and breaks away
    // to found their own tribe. Deliberately rare and heavily gated (tribe must
    // be established, the rift must be real, a faction must exist, and even then
    // only a small per-tick chance) so the map doesn't fragment into a churn of
    // micro-tribes the way religions can proliferate.
    {
        std::uniform_real_distribution<float> rollS(0.0f, 1.0f);
        // Index loop: formTribe() may push_back to `tribes`. We mutate the parent
        // and read everything we need BEFORE calling formTribe, and never touch
        // tribes[ti] afterwards, so a reallocation can't bite us.
        size_t tribeCountBefore = tribes.size();
        for (size_t ti = 0; ti < tribeCountBefore; ++ti) {
            Tribe& tr = tribes[ti];
            if (tr.population() < 7) continue;           // need a body to split from
            if (day - tr.foundedOnDay < 240) continue;   // must be established first

            // The would-be breakaway: highest-charisma member (not the leader)
            // whose values diverge most from the tribe's culture.
            Entity* dissident = nullptr; float bestScore = 0.0f;
            for (int mid : tr.memberIds) {
                Entity* e = entityById(entities, mid);
                if (!e || e->entityHealth <= 0.0f || e->entityId == tr.leaderId) continue;
                float div = std::abs(e->ValueSystem.collectivism - tr.collectivism) * 0.4f
                          + std::abs(e->ValueSystem.spiritualNeed - tr.spiritualism) * 0.3f
                          + std::abs(e->personality.openness      - tr.innovation)   * 0.3f;
                float score = (div / 100.0f) * (computeCharisma(e) / 100.0f);
                if (score > bestScore) { bestScore = score; dissident = e; }
            }
            if (!dissident || bestScore < 0.20f) continue;

            // Rare per-tick chance, scaled by how deep the rift is. The 0.004
            // coefficient is well below the religion-founding roll so schisms
            // stay an occasional event, not a constant churn.
            float schismChance = bestScore * 0.004f * g_worldSeed.divergence.butterfly;
            if (rollS(rng) >= schismChance) continue;

            // The dissident pulls along value-aligned / socially-bonded members.
            std::vector<Entity*> faction; faction.push_back(dissident);
            for (int mid : tr.memberIds) {
                if ((int)faction.size() >= 5) break;
                Entity* e = entityById(entities, mid);
                if (!e || e == dissident || e->entityHealth <= 0.0f) continue;
                if (e->entityId == tr.leaderId) continue;        // the leader stays
                float valDiff = std::abs(e->ValueSystem.collectivism     - dissident->ValueSystem.collectivism)     * 0.4f
                              + std::abs(e->ValueSystem.spiritualNeed     - dissident->ValueSystem.spiritualNeed)     * 0.3f
                              + std::abs(e->ValueSystem.achievementDrive  - dissident->ValueSystem.achievementDrive)  * 0.3f;
                float bond = e->searchConnSocial(dissident);
                if (valDiff < 28.0f || bond > 14.0f) faction.push_back(e);
            }
            // Need a real faction, and the parent must survive the loss.
            if (faction.size() < 3) continue;
            if (tr.population() - (int)faction.size() < 3) continue;

            // Detach the faction from the parent (all parent mutation happens here,
            // before formTribe), then let them found their own tribe.
            std::string parentName = tr.name;
            int parentId = tr.id;
            for (Entity* e : faction) {
                e->tribeId = -1;
                tr.memberIds.erase(std::remove(tr.memberIds.begin(), tr.memberIds.end(), e->entityId),
                                   tr.memberIds.end());
            }
            if (formTribe(faction, day)) {   // may reallocate `tribes`; we touch tr no more
                logEvent(day, "A faction broke away from " + parentName +
                         " over irreconcilable values, led by " + dissident->name, "tribe");
            } else {
                // Founding fell through (no charismatic leader): re-find the parent
                // by id (vector may have changed) and reabsorb so nobody leaks out.
                for (auto& pt : tribes) if (pt.id == parentId) {
                    for (Entity* e : faction) { e->tribeId = parentId; pt.memberIds.push_back(e->entityId); }
                    break;
                }
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
             " (" + std::to_string((int)cluster.size()) + " members)", "tribe",
             "kind=tribe_founded tribe=\"" + tribe.name + "\" tribeId=" + std::to_string(tribe.id)
             + " leader=\"" + bestLeader->name + "\" leaderId=" + std::to_string(bestLeader->entityId)
             + " members=" + std::to_string((int)cluster.size())
             + " militarism=" + std::to_string((int)tribe.militarism)
             + " spiritualism=" + std::to_string((int)tribe.spiritualism)
             + " collectivism=" + std::to_string((int)tribe.collectivism)
             + " innovation=" + std::to_string((int)tribe.innovation));
    return true;
}

//collect taxes every week
void CivilizationEngine::collectTaxes(Tribe& tribe, std::vector<Entity>& ent) {
  for (int id_entity : tribe.memberIds) {
    Entity* tribe_member = entityById(ent, id_entity);
    tribe.economy.earnMoney(tribe_member->salary.token * tribe.taxeRate);
    tribe_member->salary.token -= tribe_member->salary.token * tribe.taxeRate;
  }
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
    if (ent->specialization.empty()) ent->specialization = "farmer";
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
    // M9 tuning: split at >24, not >8. Splitting at 8 was a fragmentation
    // engine — every growing tribe fissioned within a generation, the map hit
    // ~48 micro-tribes, and with O(pairs) war checks the war rate exploded.
    for (size_t idx = 0; idx < tribes.size(); ++idx) {
        // A tribe splits either from sheer size, or — Plan 8, civil war — when its
        // legitimacy has collapsed and a sizeable faction breaks away in revolt.
        bool civilWar = (tribes[idx].govSatisfaction < 25.0f && tribes[idx].population() > 10);
        if (tribes[idx].population() <= 24 && !civilWar) continue;

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
        if (civilWar) {
            ++totalCivilWars;
            // The mother tribe, having purged its malcontents, regains some calm.
            tribes[idx].govSatisfaction = std::min(100.0f, tribes[idx].govSatisfaction + 20.0f);
            logEvent(day, "CIVIL WAR: the " + splitName
                     + " tore itself apart — a faction seceded in revolt", "tribe",
                     "kind=civil_war tribe=\"" + splitName + "\""
                     + " seceders=" + std::to_string(groupB.size()));
        } else {
            logEvent(day, "The " + splitName + " has split -- a faction broke away", "tribe");
        }
    }
}

// ── Religion management ────────────────────────────────────────────────────────
void CivilizationEngine::updateReligions(std::vector<Entity>& entities, int day) {
    // M7: prophets arise in spiritual VACUUMS, not in saturated markets. The
    // old unconditional roll founded a new faith every few days (15+ by day
    // 125, still climbing); real religious landscapes stabilise at a handful.
    int   viableReligions = 0;
    int   affiliated = 0, living = 0;
    for (const auto& rel : religions) {
        int alive = 0;
        for (int fid : rel.followerIds)
            if (Entity* f = entityById(entities, fid))
                if (f->entityHealth > 0.0f) ++alive;
        if (alive >= 3) ++viableReligions;
    }
    for (const Entity& e : entities) {
        if (e.entityHealth <= 0.0f) continue;
        ++living;
        if (e.religionId != -1) ++affiliated;
    }
    float vacuum = living > 0 ? 1.0f - (float)affiliated / (float)living : 1.0f;
    // Gate at 6 viable: small faiths keep growing after founding stops, so a
    // cutoff of 8 overshot to ~10 viable religions by late run.
    bool  saturated = (viableReligions >= 6);

    // Check for prophets among unaffiliated entities
    for (Entity& ent : entities) {
        if (saturated) break;
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
        // `vacuum` scales founding by the unconverted share of the population.
        if (roll(rng) < prophetScore * 0.030f * vacuum * g_worldSeed.divergence.butterfly)
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

    // M7: faiths that fail to gather a congregation fade back into folklore.
    // A religion older than its 120-day grace period with fewer than 3 living
    // followers dissolves — its stragglers return to the unaffiliated pool
    // (where the vacuum term above lets new prophets try again).
    religions.erase(
        std::remove_if(religions.begin(), religions.end(),
            [&](Religion& rel) {
                if (day - rel.foundedOnDay < 120) return false;
                int alive = 0;
                for (int fid : rel.followerIds)
                    if (Entity* f = entityById(entities, fid))
                        if (f->entityHealth > 0.0f) ++alive;
                if (alive >= 3) return false;
                for (int fid : rel.followerIds)
                    if (Entity* f = entityById(entities, fid))
                        if (f->religionId == rel.id) f->religionId = -1;
                for (auto& t : tribes)
                    if (t.dominantReligionId == rel.id) t.dominantReligionId = -1;
                logEvent(day, "The faith of \"" + rel.name + "\" faded into memory, its last "
                         + std::to_string(alive) + " followers scattered", "religion",
                         "kind=religion_extinct religionId=" + std::to_string(rel.id));
                return true;
            }),
        religions.end());

    // Religious syncretism: let mutually-tolerant faiths merge (Plan 3.1.B) —
    // an alternative to the extinction churn, run before influence is recomputed.
    trySyncretism(entities, day);

    // Update influence and grow religious institutions (Plan 3.1.C).
    for (auto& rel : religions) {
        int alive = 0;
        for (int fid : rel.followerIds)
            if (Entity* f = entityById(entities, fid))
                if (f->entityHealth > 0.0f) ++alive;
        rel.influence = std::min(100.0f,
            (float)alive / std::max(1.0f, (float)entities.size()) * 100.0f);

        // Congregation size unlocks ever-grander institutions. They only ratchet
        // UP, so a faith keeps the cathedral it built even through a lean decade;
        // they vanish only when the faith itself dies (its Religion is erased).
        int lvl = 0;
        if      (alive >= 400) lvl = 6;   // Religious Center
        else if (alive >= 250) lvl = 5;   // Holy Order
        else if (alive >= 150) lvl = 4;   // Cathedral
        else if (alive >=  80) lvl = 3;   // Monastery
        else if (alive >=  35) lvl = 2;   // Temple
        else if (alive >=  12) lvl = 1;   // Shrine
        if (lvl > rel.institutionLevel) {
            rel.institutionLevel = lvl;
            logEvent(day, "The " + rel.name + " raised a " + rel.institutionName(), "religion",
                     "kind=religion_institution religionId=" + std::to_string(rel.id)
                     + " level=" + std::to_string(lvl)
                     + " institution=\"" + rel.institutionName() + "\""
                     + " followers=" + std::to_string(alive));
        }
    }
}

int CivilizationEngine::trySyncretism(std::vector<Entity>& entities, int day) {
    if (religions.size() < 2) return 0;
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    // One O(N) pass: living tribe of every entity, so the pair scan below never
    // calls the O(N) entityById in a hot loop.
    std::map<int,int> tribeOf;        // entityId -> tribeId (living only)
    for (const Entity& e : entities)
        if (e.entityHealth > 0.0f) tribeOf[e.entityId] = e.tribeId;

    auto liveFollowers = [&](const Religion& r, std::set<int>& tribes) {
        int a = 0;
        for (int fid : r.followerIds) {
            auto it = tribeOf.find(fid);
            if (it == tribeOf.end()) continue;   // dead / gone
            ++a;
            if (it->second >= 0) tribes.insert(it->second);
        }
        return a;
    };

    for (size_t i = 0; i < religions.size(); ++i) {
        for (size_t j = i + 1; j < religions.size(); ++j) {
            Religion& A = religions[i];
            Religion& B = religions[j];
            // Only mutually-tolerant faiths reconcile; the exclusive ones war.
            if (A.tolerance < 55.0f || B.tolerance < 55.0f) continue;

            std::set<int> aTribes, bTribes;
            int aAlive = liveFollowers(A, aTribes);
            int bAlive = liveFollowers(B, bTribes);
            if (aAlive < 3 || bAlive < 3) continue;

            // They must actually rub shoulders — share at least one tribe.
            bool shareTribe = false;
            for (int t : aTribes) if (bTribes.count(t)) { shareTribe = true; break; }
            if (!shareTribe) continue;

            // A deliberate, rare cultural event, not a constant churn.
            if (roll(rng) > 0.10f) continue;

            bool aBigger = (aAlive >= bAlive);
            size_t bigIdx = aBigger ? i : j, smallIdx = aBigger ? j : i;
            Religion& big   = religions[bigIdx];
            Religion& small = religions[smallIdx];
            float wb = (float)std::max(1, aBigger ? aAlive : bAlive);
            float ws = (float)std::max(1, aBigger ? bAlive : aAlive);
            auto blend = [&](float x, float y){ return (x * wb + y * ws) / (wb + ws); };
            big.militarism     = blend(big.militarism,     small.militarism);
            big.tolerance      = blend(big.tolerance,      small.tolerance);
            big.asceticism     = blend(big.asceticism,     small.asceticism);
            big.authority      = blend(big.authority,      small.authority);
            big.afterlifeFocus = blend(big.afterlifeFocus, small.afterlifeFocus);

            int bigId = big.id, smallId = small.id;
            std::string bigName = big.name, smallName = small.name;

            // Fold the smaller congregation into the larger faith.
            for (int fid : small.followerIds) {
                Entity* e = entityById(entities, fid);
                if (e && e->entityHealth > 0.0f && e->religionId == smallId) {
                    e->religionId = bigId;
                    big.followerIds.push_back(fid);
                }
            }
            for (auto& t : tribes)
                if (t.dominantReligionId == smallId) t.dominantReligionId = bigId;

            logEvent(day, "The faiths of \"" + smallName + "\" and \"" + bigName
                     + "\" reconciled and merged into a single tradition", "religion",
                     "kind=religion_syncretism absorbedId=" + std::to_string(smallId)
                     + " absorbedName=\"" + smallName + "\""
                     + " intoId=" + std::to_string(bigId) + " intoName=\"" + bigName + "\"");

            // Remove the absorbed faith. `big`/`small` references die here, but we
            // return immediately, so nothing dangles.
            religions.erase(religions.begin() + smallIdx);
            return 1;   // at most one merge per tick keeps this cheap & legible
        }
    }
    return 0;
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

    bool hasGrief = false;
    for (const auto& g : prophet->griefStates) if (g.intensity > 0.3f) { hasGrief = true; break; }

    // ── Doctrinal axes from the founder (Plan 3.1.A) ─────────────────────────
    auto clamp01 = [](float v){ return std::max(0.0f, std::min(100.0f, v)); };
    const auto& P = prophet->personality;
    rel.militarism     = clamp01((100.0f - P.agreeableness) * 0.6f + P.neuroticism * 0.4f);
    rel.tolerance      = clamp01(P.openness * 0.7f + P.agreeableness * 0.3f);
    rel.asceticism     = clamp01(rel.spiritualDemand * 0.6f + P.conscientiousness * 0.4f);
    rel.authority      = clamp01(P.conscientiousness * 0.6f + (100.0f - P.openness) * 0.4f);
    rel.afterlifeFocus = clamp01(rel.spiritualDemand * 0.5f + prophet->entityStress * 0.3f
                                 + (hasGrief ? 25.0f : 0.0f));

    // ── Unique creed generated from the dominant axis (Plan 3.1.D) ───────────
    // The axis that deviates furthest from the neutral 50 defines the faith's
    // voice — so a crusader faith preaches conquest and a syncretic one preaches
    // coexistence, instead of every religion sharing one bland motto.
    struct AxisCreed { float dev; std::string creed; };
    std::vector<AxisCreed> creeds = {
        { rel.militarism    - 50.0f, "The sword purifies; the faithful shall conquer." },
        { 50.0f - rel.militarism,    "Blessed are the peacemakers, for they inherit the earth." },
        { rel.tolerance     - 50.0f, "The divine wears many faces; embrace them all." },
        { 50.0f - rel.tolerance,     "There is one truth, and all other paths are shadow." },
        { rel.asceticism    - 50.0f, "Flesh is illusion; only the spirit endures." },
        { 50.0f - rel.asceticism,    "The world is a gift; take joy in its abundance." },
        { rel.authority     - 50.0f, "Order reflects the divine hierarchy; obey the anointed." },
        { 50.0f - rel.authority,     "No crown stands between a soul and the sacred." },
        { rel.afterlifeFocus- 50.0f, "This world is a trial; the next is the reward." },
        { 50.0f - rel.afterlifeFocus,"Heaven is the life we build with our own hands." },
    };
    auto strongest = std::max_element(creeds.begin(), creeds.end(),
        [](const AxisCreed& a, const AxisCreed& b){ return a.dev < b.dev; });
    // Fall back to a neutral creed only when no axis is meaningfully pronounced.
    rel.holyPrinciple = (strongest->dev > 12.0f)
        ? strongest->creed
        : (hasGrief ? "Those who are lost shall be remembered forever."
                    : "Live rightly, and the world shall be good.");

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

    // Complexity gates the pace of progress: a simple trick (~18) almost always
    // lands once attempted, but odds fall off with the square of complexity —
    // Metal Working (78) fizzles ~95% of attempts, Gunpowder (90) ~96%. Without
    // this the whole 53-tech catalog emptied within a few decades because a
    // prereq-met tech was discovered on the first eureka regardless of difficulty.
    float odds = 18.0f / std::max(18.0f, chosen->complexity);
    odds *= odds;
    std::uniform_real_distribution<float> attemptRoll(0.0f, 1.0f);
    if (attemptRoll(rng) > odds) return false;   // the insight slips away

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
    if (tribe){
      tribe->knownTechIds.insert(inv.id);
      tribe->knownTechName.insert(inv.name);
    }

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

            // M7: wars need CAUSES, not just cultural distance. Two concrete
            // grievances grind relations down no matter how similar the
            // cultures are — without these, similar tribes warmed to +100 and
            // the war line was never reached (zero wars in 600 days).
            // (a) Faith friction: rival dominant religions chafe continuously.
            if (A.dominantReligionId != -1 && B.dominantReligionId != -1 &&
                A.dominantReligionId != B.dominantReligionId)
                rel -= 0.5f;
            // (b) Hunger envy: a starving tribe eyes a fat neighbour's granary,
            // and desperation makes it warlike.
            if (A.granary < 5.0f && B.granary > 30.0f) {
                rel -= 0.6f;
                A.militarism = std::min(100.0f, A.militarism + 0.25f);
            }
            if (B.granary < 5.0f && A.granary > 30.0f) {
                rel -= 0.6f;
                B.militarism = std::min(100.0f, B.militarism + 0.25f);
            }
            // (c) Conquest opportunism: a warlike tribe towering over a much
            // smaller neighbour smells easy land, and covets it. Uses population
            // as a cheap strength proxy — the real strength math runs only at
            // the moment of declaration.
            int popA = A.population(), popB = B.population();
            if (A.militarism > 55.0f && popB > 0 && popA > popB * 2) rel -= 0.45f;
            if (B.militarism > 55.0f && popA > 0 && popB > popA * 2) rel -= 0.45f;

            if (valDiff < 22.0f)
                rel = std::min(100.0f, rel + 1.0f);
            else if (valDiff > 40.0f)
                rel = std::max(-100.0f, rel - (1.2f + aggression * 1.6f));
            else
                rel = rel * 0.99f - aggression * 0.3f;

            rel = std::max(-100.0f, std::min(100.0f, rel));
            B.relations[A.id] = rel;

            // War line climbs toward 0 with militarism. Plan 2.1's floor (-62..-48)
            // overcorrected the endemic ~860-war run to nearly zero wars: with
            // similar cultures warming at +1.0/tick, relations could never grind
            // that low. Raised so warlike pairs tip at ~-34 and even peaceful
            // ones at -52, while war-weary tribes (high exhaustion) still hold
            // back for a generation after a fight.
            float warLine   = -52.0f + aggression * 18.0f
                              - std::max(A.warExhaustion, B.warExhaustion) * 0.15f;
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
                    // ── Casus belli first, faith second (Plan 2.1.C) ─────────────
                    // The reference run was 98.8% "holy war" because ANY faith
                    // difference lit a war of religion. Now a holy war demands real
                    // zealotry on BOTH sides — two intolerant faiths, at least one
                    // of them a crusading creed. Every other war is fought over the
                    // concrete thing that actually pushed relations over the edge:
                    // hunger, a power imbalance, a rebellious vassal, or plain
                    // border friction. Holy war becomes the exception, not the rule.
                    Religion* ra = findReligion(A.dominantReligionId);
                    Religion* rb = findReligion(B.dominantReligionId);
                    bool faithsDiffer = (A.dominantReligionId != -1 && B.dominantReligionId != -1
                                         && A.dominantReligionId != B.dominantReligionId);
                    float tolA = ra ? ra->tolerance  : 50.0f, tolB = rb ? rb->tolerance  : 50.0f;
                    float milA = ra ? ra->militarism : 0.0f,  milB = rb ? rb->militarism : 0.0f;
                    bool holyWar   = faithsDiffer && tolA < 45.0f && tolB < 45.0f
                                     && std::max(milA, milB) > 55.0f;
                    bool deepHatred = (rel < -80.0f) && (aggression > 0.6f);

                    bool hungry = (A.granary < 8.0f && B.granary > 25.0f)
                                  || (B.granary < 8.0f && A.granary > 25.0f);
                    float sA = calculateTribeMilitaryStrength(A, entities);
                    float sB = calculateTribeMilitaryStrength(B, entities);
                    float hi = std::max(sA, sB), lo = std::max(1.0f, std::min(sA, sB));
                    bool imbalance   = (hi > lo * 1.8f);
                    bool militaristic = (A.militarism > 58.0f || B.militarism > 58.0f);
                    bool vassalSpat  = (A.overlordTribeId == B.id || B.overlordTribeId == A.id);

                    WarReason reason;
                    if      (holyWar)                    reason = WAR_ETHNIC;
                    else if (vassalSpat)                 reason = WAR_TRIBUTE;
                    else if (hungry)                     reason = WAR_RESOURCE;
                    else if (imbalance && militaristic)  reason = WAR_CONQUEST;
                    else if (deepHatred)                 reason = WAR_ETHNIC;   // ancient hatred
                    else                                 reason = WAR_BORDER;

                    bool ethnic = (reason == WAR_ETHNIC);
                    A.ethnicWarWith.insert(ethnic ? B.id : -999);
                    B.ethnicWarWith.insert(ethnic ? A.id : -999);
                    A.ethnicWarWith.erase(-999);
                    B.ethnicWarWith.erase(-999);

                    totalWarsDeclared++;
                    if (ethnic) totalEthnicWars++;
                    std::string why = holyWar ? "a holy war of faiths"
                                   : (reason == WAR_ETHNIC) ? "a war of ancient hatreds"
                                   : warReasonName(reason);
                    std::string head = ethnic
                        ? ("ETHNIC WAR: the " + A.name + " and the " + B.name + " plunge into " + why)
                        : ("The " + A.name + " and the " + B.name + " go to war — " + why);
                    logEvent(day, head, "war",
                             "kind=war_declared ethnic=" + std::string(ethnic ? "1" : "0")
                             + " reason=\"" + why + "\""
                             + " tribeA=\"" + A.name + "\" tribeAId=" + std::to_string(A.id)
                             + " tribeB=\"" + B.name + "\" tribeBId=" + std::to_string(B.id)
                             + " popA=" + std::to_string(A.population())
                             + " popB=" + std::to_string(B.population())
                             + " relations=" + std::to_string((int)rel));

                    float rage = ethnic ? 22.0f : 8.0f;
                    for (int mid : A.memberIds) { Entity* e = entityById(entities, mid); if (e) e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + rage); }
                    for (int mid : B.memberIds) { Entity* e = entityById(entities, mid); if (e) e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + rage); }

                    // War tears apart any couples that straddle the new front line.
                    breakCrossTribeCouples(A, B, entities, day);
                } else if (stance == TS_ALLY) {
                    logEvent(day, "The " + A.name + " and the " + B.name + " formed an alliance", "diplomacy",
                             "kind=alliance tribeA=\"" + A.name + "\" tribeAId=" + std::to_string(A.id)
                             + " tribeB=\"" + B.name + "\" tribeBId=" + std::to_string(B.id)
                             + " relations=" + std::to_string((int)rel));
                } else if (stance == TS_RIVAL) {
                    logEvent(day, "Tensions rise between the " + A.name + " and the " + B.name, "diplomacy",
                             "kind=rivalry tribeA=\"" + A.name + "\" tribeAId=" + std::to_string(A.id)
                             + " tribeB=\"" + B.name + "\" tribeBId=" + std::to_string(B.id)
                             + " relations=" + std::to_string((int)rel));
                }
                // Leaving war: clear the ethnic-war marker and send the soldiers
                // home — a post-war baby boom follows (returning-soldier effect).
                if (prev == TS_AT_WAR && stance != TS_AT_WAR) {
                    A.ethnicWarWith.erase(B.id);
                    B.ethnicWarWith.erase(A.id);
                    endWarFor(A, 0, day);
                    endWarFor(B, 0, day);
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

    // Key foundational technologies gate each era. Extended (Plan 1.2) so the
    // enlarged CATALOG can actually carry a civilisation to the Renaissance and
    // beyond — previously ERA_RENNAISSANCE was unreachable and the ladder capped
    // out around innCount 18.
    bool hasAgriculture = false, hasMetal = false, hasFort = false;
    bool hasIronSmelt = false, hasWriting = false, hasGunpowder = false;
    bool hasScientific = false, hasSteam = false;
    bool hasReligion = !religions.empty();
    for (const auto& inv : innovations) {
        if (inv.category == "agriculture") hasAgriculture = true;
        if (inv.name == "Metal Working")   hasMetal       = true;
        if (inv.name == "Fortification")   hasFort        = true;
        if (inv.name == "Iron Smelting")   hasIronSmelt   = true;
        if (inv.name == "Writing")         hasWriting     = true;
        if (inv.name == "Gunpowder")       hasGunpowder   = true;
        if (inv.name == "Scientific Method") hasScientific = true;
        if (inv.name == "Steam Power")     hasSteam       = true;
    }

    CivilizationEra prevEra = era;
    int year = getCurrentYear();

    if      (innCount >= 44 && hasSteam)                    era = ERA_MODERN;
    else if (innCount >= 38 && hasScientific)               era = ERA_EARLY_MODERN;
    else if (innCount >= 32 && hasGunpowder)                era = ERA_RENNAISSANCE;
    else if (innCount >= 26 && hasFort && hasWriting)       era = ERA_MEDIEVAL;
    else if (innCount >= 20 && hasIronSmelt)                era = ERA_CLASSICAL;
    else if (innCount >= 14 && hasIronSmelt)                era = ERA_IRON_AGE;
    else if (innCount >=  9 && hasMetal)                    era = ERA_BRONZE_AGE;
    else if (innCount >=  5 && (hasAgriculture || hasReligion) && tribeCount >= 2)
                                                            era = ERA_EARLY_AGRICULTURE;
    else if (tribeCount >= 2 && innCount >= 1)              era = ERA_TRIBAL;
    else                                                    era = ERA_STONE_AGE;

    // ── Institutional memory floor (Plan 1.1, Option D) ──────────────────────
    // A civilisation carries the memory of its golden age: the live era can slip
    // at most ONE step below the highest era ever achieved. This is what turns a
    // catastrophic Medieval→Stone-Age cascade into a recoverable single-step
    // regression, ending the endless oscillation loop.
    if ((int)maxEraAchieved > (int)ERA_STONE_AGE) {
        CivilizationEra floorEra =
            (CivilizationEra)std::max((int)ERA_STONE_AGE, (int)maxEraAchieved - 1);
        if ((int)era < (int)floorEra) era = floorEra;
    }
    if ((int)era > (int)maxEraAchieved) maxEraAchieved = era;

    if (era != prevEra) {
        logEvent(0, "The world enters a new era: " + getEraName()
                 + " (" + getYearDisplay() + ")", "era",
                 "kind=era_change era=\"" + getEraName() + "\" year=\"" + getYearDisplay() + "\""
                 + " population=" + std::to_string(pop)
                 + " tribes=" + std::to_string(tribeCount)
                 + " innovations=" + std::to_string(innCount));
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

            // Every tribe member holds an explicit role; field work is the
            // default — crafts are assigned on promotion in updateDivisionOfLabour.
            if (ent.specialization.empty()) {
                ent.specialization = "farmer";
                ent.roleSinceDay   = day;
            }
        }

        // ── Religion effects ─────────────────────────────────────────────────
        if (religion) {
            ent.entityStress      = clamp(ent.entityStress      - 1.2f, 0.0f, 100.0f);
            ent.entityMentalHealth = clamp(ent.entityMentalHealth + 0.5f, 0.0f, 100.0f);
            // High-demand religion → stronger community bond
            if (religion->spiritualDemand > 65.0f)
                ent.entityLoneliness = clamp(ent.entityLoneliness - 2.0f, 0.0f, 100.0f);
            // Religious institutions (Plan 3.1.C): the grander the faith's works,
            // the more solace and belonging they give their congregation.
            if (religion->institutionLevel > 0) {
                float instBonus = 0.3f * (float)religion->institutionLevel;
                ent.entityHapiness    = clamp(ent.entityHapiness    + instBonus,       0.0f, 100.0f);
                ent.entityMentalHealth = clamp(ent.entityMentalHealth + instBonus * 0.5f, 0.0f, 100.0f);
            }
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

// ── Division of labour ────────────────────────────────────────────────────────
// The engine of "economic base determines superstructure". Each tribe runs a
// communal granary: subsistence farmers deposit their surplus; the granary then
// feeds a capped number of non-farming specialists (artisans, priests, soldiers,
// traders, scholars). How many specialists a tribe can support is set by the
// food surplus — so a rich valley sprouts a priestly/artisan class while a
// hungry one stays all-hands-to-the-plough, and a famine dissolves the
// superstructure back into farmers overnight.
void CivilizationEngine::updateDivisionOfLabour(std::vector<Entity>& entities, int day) {
    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };

    for (Tribe& tribe : tribes) {
        // Gather living members.
        std::vector<Entity*> members;
        members.reserve(tribe.memberIds.size());
        for (int mid : tribe.memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f) members.push_back(e);
        }
        if (members.empty()) { tribe.granary *= 0.95f; tribe.specialistCount = 0; continue; }

        // 1. Farmers tithe their surplus into the granary (keep a comfort buffer)
        //    and draw the humblest wage in the tribe.
        const float comfort = 12.0f;
        float deposited = 0.0f;
        for (Entity* e : members) {
            if (e->isSpecialist) continue;
            e->salary.earnMoney(1.0f);
            if (e->foodStore > comfort) {
                float give = (e->foodStore - comfort) * 0.5f;
                e->foodStore -= give;
                deposited += give;
            }
        }
        // Agricultural & storage techs (Agriculture, Irrigation, Pottery…)
        // multiply the surplus a tribe banks each tick.
        deposited *= TechTreeSystem::foodMultiplier(tribe);
        tribe.granary += deposited;
        tribe.granary *= 0.985f;   // antiquity has no refrigeration — stores spoil

        // 2. How many mouths can the surplus free from the fields?
        //    A buffer of ~8 rations per specialist must exist; an innovation-led
        //    cultural ceiling caps the share (more inventive tribes specialise more).
        const float ration   = 1.2f;
        int affordable = (int)std::floor(tribe.granary / (ration * 8.0f));
        float ceilFrac = clamp(0.15f + tribe.innovation / 400.0f, 0.15f, 0.45f);
        int   ceiling  = (int)std::ceil(members.size() * ceilFrac);
        int   target   = std::max(0, std::min({ affordable, ceiling, (int)members.size() - 1 }));

        int current = 0;
        for (Entity* e : members) if (e->isSpecialist) current++;

        // 3. Promote toward target (highest dominance/talent first), or demote the
        //    surplus (lowest first — survival keeps the ablest provisioned).
        if (current < target) {
            std::sort(members.begin(), members.end(), [](Entity* a, Entity* b) {
                return a->dominanceRank > b->dominanceRank;
            });
            for (Entity* e : members) {
                if (current >= target) break;
                if (e->isSpecialist) continue;
                if (e->roleSinceDay >= 0 && day - e->roleSinceDay < 3) continue;
                e->isSpecialist = true;
                // Careers are chosen at promotion, from the dominant trait.
                if (e->specialization.empty() || e->specialization == "farmer") {
                    float maxTrait = std::max({ e->personality.extraversion,
                                                e->personality.agreeableness,
                                                e->personality.conscientiousness,
                                                e->personality.openness,
                                                100.0f - e->personality.agreeableness,
                                                e->ValueSystem.spiritualNeed });
                    if (maxTrait == e->ValueSystem.spiritualNeed           ) e->specialization = "priest";
                    else if (maxTrait == e->personality.openness           ) e->specialization = "scholar";
                    else if (maxTrait == e->personality.conscientiousness  ) e->specialization = "craftsman";
                    else if (maxTrait == e->personality.extraversion       ) e->specialization = "trader";
                    else if (maxTrait == e->personality.agreeableness      ) e->specialization = "healer";
                    else                                                     e->specialization = "warrior";
                }
                e->roleSinceDay = day;
                current++;
                logEvent(day, e->name + " is freed from the fields to serve as a "
                              + e->specialization + " of " + tribe.name, "labour",
                              "kind=specialist role=\"" + e->specialization + "\""
                              + " entity=\"" + e->name + "\" entityId=" + std::to_string(e->entityId)
                              + " tribe=\"" + tribe.name + "\" tribeId=" + std::to_string(tribe.id)
                              + " granary=" + std::to_string((int)tribe.granary)
                              + " specialists=" + std::to_string(target));
            }
        } else if (current > target) {
            std::sort(members.begin(), members.end(), [](Entity* a, Entity* b) {
                return a->dominanceRank < b->dominanceRank;
            });
            int toDemote = current - target;
            for (Entity* e : members) {
                if (toDemote <= 0) break;
                if (!e->isSpecialist) continue;
                if (e->roleSinceDay >= 0 && day - e->roleSinceDay < 3) continue;
                e->isSpecialist = false;
                e->specialization = "farmer";
                e->roleSinceDay = day;
                toDemote--;
            }
        }

        // 4. Provision specialists from the granary; an unfed one returns to the
        //    fields (the famine fail-safe — nobody starves on principle).
        tribe.specialistCount = 0;
        for (Entity* e : members) {
            if (!e->isSpecialist) continue;
            if (tribe.granary >= ration) {
                tribe.granary -= ration;
                e->foodStore = std::min(20.0f, e->foodStore + ration);
            } else {
                e->isSpecialist = false;
                e->specialization = "farmer";
                e->roleSinceDay = day;
                continue;
            }
            tribe.specialistCount++;

            // 5. Specialist output → tribe & self. Kept light so the survival
            //    balance is untouched; the point is the *structure*, not big buffs.
            const std::string& s = e->specialization;
            e->entityBoredom = clamp(e->entityBoredom - 1.0f, 0.0f, 100.0f);
            e->Esteem        = clamp(e->Esteem + 0.8f, 0.0f, 100.0f);
            if (s == "craftsman") {
                // Artisans turn timber & ore into tools — draws on the homeland's pools.
                g_resources.extract(tribe.regionId, RES_WOOD,  0.4f);
                g_resources.extract(tribe.regionId, RES_METAL, 0.2f);
                tribe.innovation = clamp(tribe.innovation + 0.05f, 0.0f, 100.0f);
                e->salary.earnMoney(5.0f);
            } else if (s == "scholar") {
                tribe.innovation = clamp(tribe.innovation + 0.08f, 0.0f, 100.0f);
                e->salary.earnMoney(8.0f);
            } else if (s == "trader") {
                e->salary.earnMoney(20.0f);
            } else if (s == "warrior") {
                tribe.militarism = clamp(tribe.militarism + 0.04f, 0.0f, 100.0f);
                e->salary.earnMoney(3.0f);
            } else if (s == "healer") {
                // Healers tend the sickest and most burdened of their kin.
                int treated = 0;
                for (Entity* p : members) {
                    if (treated >= 3) break;
                    if (p == e) continue;
                    if (p->entityDiseaseType != -1 || p->entityStress > 55.0f) {
                        p->entityStress = clamp(p->entityStress - 0.8f, 0.0f, 100.0f);
                        if (p->entityDiseaseType != -1)
                            p->entityAntiBody = clamp(p->entityAntiBody + 0.6f, 0.0f, 100.0f);
                        treated++;
                    }
                }
                e->salary.earnMoney(4.0f);
            } else if (s == "priest") {
                tribe.spiritualism = clamp(tribe.spiritualism + 0.04f, 0.0f, 100.0f);
                for (Entity* p : members)
                    if (p->religionId >= 0)
                        p->entityHapiness = clamp(p->entityHapiness + 0.05f, 0.0f, 100.0f);
                e->salary.earnMoney(4.0f);
            }
            // "farmer": tithes to the granary in step 1 above (lowest wage there).
        }

        // 6. Priests & healers bind the community: their presence eases everyone's
        //    loneliness and stress a touch (collective cohesion from the cult).
        int clergy = 0;
        for (Entity* e : members)
            if (e->isSpecialist && (e->specialization == "healer" || e->specialization == "scholar"))
                clergy++;
        if (clergy > 0) {
            float relief = std::min(2.0f, 0.4f * clergy);
            for (Entity* e : members) {
                e->entityLoneliness = clamp(e->entityLoneliness - relief, 0.0f, 100.0f);
                e->entityStress     = clamp(e->entityStress     - relief * 0.5f, 0.0f, 100.0f);
            }
        }
    }
}

// ── Structured technology tree ─────────────────────────────────────────────────
void CivilizationEngine::updateTechTree(std::vector<Entity>& entities, int day) {
    TechTreeSystem::tick(*this, entities, day);
}

// ── Resource economy & fortifications (Improvement Plan 6.1 / 2.3) ─────────────
// Specialists no longer just exist — they produce distinct stockpiles: craftsmen
// work materials (and, once metalworking exists, ore); traders amass luxuries;
// scholars accumulate knowledge. Those stockpiles then do concrete work: knowledge
// speeds research, luxuries lift spirits, and materials are spent raising
// fortifications toward the ceiling the current era allows.
void CivilizationEngine::updateEconomyResources(std::vector<Entity>& entities, int day) {
    (void)day;
    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };

    bool hasMetalTech = false, hasWriting = false, hasPrinting = false;
    for (const auto& inv : innovations) {
        if (inv.name == "Metal Working" || inv.name == "Iron Smelting") hasMetalTech = true;
        if (inv.name == "Writing")  hasWriting  = true;
        if (inv.name == "Printing") hasPrinting = true;
    }

    // Era sets how grand a tribe's defensive works may become.
    float fortCap;
    switch (era) {
        case ERA_STONE_AGE: case ERA_TRIBAL: case ERA_EARLY_AGRICULTURE: fortCap = 10.0f; break; // palisade
        case ERA_BRONZE_AGE: case ERA_IRON_AGE:                          fortCap = 25.0f; break; // walls
        case ERA_CLASSICAL: case ERA_MEDIEVAL:                           fortCap = 50.0f; break; // castle
        case ERA_RENNAISSANCE:                                           fortCap = 75.0f; break; // star fort
        default:                                                         fortCap = 100.0f; break;
    }

    for (Tribe& t : tribes) {
        int pop = 0, crafts = 0, scholars = 0, traders = 0;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (!e || e->entityHealth <= 0.0f) continue;
            ++pop;
            if (!e->isSpecialist) continue;
            const std::string& s = e->specialization;
            if      (s == "craftsman") ++crafts;
            else if (s == "scholar")   ++scholars;
            else if (s == "trader")    ++traders;
        }
        if (pop == 0) continue;

        // Abstract land holdings relax toward population — this is what a decisive
        // battle then carves a slice out of.
        t.territory += ((float)pop - t.territory) * 0.05f;

        // Production.
        t.matStock       += crafts * 0.8f;
        if (hasMetalTech) t.metalStock += crafts * 0.3f;
        t.luxuryStock    += traders * 0.5f;
        t.knowledgeStock += scholars * 0.6f;

        // Upkeep / spoilage.
        t.matStock    *= 0.99f;
        t.metalStock  *= 0.995f;
        t.luxuryStock *= 0.98f;

        // Education & knowledge transmission (Plan 11): an illiterate people forgets
        // — oral knowledge decays each tick. Writing preserves it; printing makes it
        // near-permanent and even compounds. This is what lets a literate society
        // hold its gains through a dark age instead of relearning from scratch.
        if (hasPrinting)      t.knowledgeStock *= 1.002f;   // knowledge begets knowledge
        else if (hasWriting)  { /* preserved — no decay */ }
        else                  t.knowledgeStock *= 0.98f;    // oral tradition fades

        // Effects. Knowledge speeds the tech-tree climb…
        t.researchPoints += t.knowledgeStock * 0.02f;
        // …luxuries lift the mood of the whole tribe…
        if (t.luxuryStock > 1.0f) {
            float lux = std::min(1.5f, t.luxuryStock * 0.02f);
            for (int mid : t.memberIds) {
                Entity* e = entityById(entities, mid);
                if (e && e->entityHealth > 0.0f)
                    e->entityHapiness = clamp(e->entityHapiness + lux, 0.0f, 100.0f);
            }
            t.luxuryStock *= 0.9f;   // consumed in the enjoying
        }
        // …and materials are spent raising fortifications toward the era ceiling.
        if (t.matStock > 5.0f && t.fortificationLevel < fortCap) {
            float build = std::min(0.5f, (fortCap - t.fortificationLevel) * 0.05f);
            t.fortificationLevel = clamp(t.fortificationLevel + build, 0.0f, fortCap);
            t.matStock -= build * 4.0f;
        }
        // Works crumble down to what the (possibly regressed) era can maintain.
        if (t.fortificationLevel > fortCap)
            t.fortificationLevel = std::max(fortCap, t.fortificationLevel - 0.2f);
    }
}

// Gini coefficient of individual wealth across the living population (Plan 6.2).
float CivilizationEngine::wealthGini(const std::vector<Entity>& entities) const {
    std::vector<float> w;
    for (const auto& e : entities)
        if (e.entityHealth > 0.0f) w.push_back(std::max(0.0f, e.salary.token));
    if (w.size() < 2) return 0.0f;
    std::sort(w.begin(), w.end());
    double sum = 0.0;
    for (float x : w) sum += x;
    if (sum <= 0.0) return 0.0f;
    double n = (double)w.size(), wsum = 0.0;
    for (size_t i = 0; i < w.size(); ++i) wsum += (double)(i + 1) * (double)w[i];
    double g = (2.0 * wsum) / (n * sum) - (n + 1.0) / n;
    return (float)std::max(0.0, std::min(1.0, g));
}

// ── Culture & arts (Improvement Plan 7) ───────────────────────────────────────
// Content scholar/artisan specialists turn leisure and inspiration (faith,
// prosperity) into a tribal culture score, which lifts happiness and eases
// boredom; occasionally a great work is produced. Dark ages erode culture — the
// library burns with the granary.
void CivilizationEngine::updateCulture(std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };

    for (Tribe& t : tribes) {
        int pop = 0, artists = 0;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (!e || e->entityHealth <= 0.0f) continue;
            ++pop;
            if (e->isSpecialist && (e->specialization == "scholar" || e->specialization == "craftsman")
                && e->entityStress < 50.0f)
                ++artists;
        }
        if (pop == 0) { t.cultureScore *= 0.98f; continue; }

        // Inspiration flows from artists, luxury (patronage) and a living faith.
        float inspiration = artists * 0.5f + t.luxuryStock * 0.02f
                            + (t.dominantReligionId >= 0 ? 0.3f : 0.0f);
        t.cultureScore = clamp(t.cultureScore + inspiration * 0.1f - 0.05f, 0.0f, 100.0f);

        // A great work: rare, needs a vibrant culture and at least one artist.
        if (artists > 0 && t.cultureScore > 40.0f
            && roll(rng) < 0.01f * (t.cultureScore / 100.0f)) {
            ++t.culturalAchievements;
            t.cultureScore = clamp(t.cultureScore + 5.0f, 0.0f, 100.0f);
            logEvent(day, "The " + t.name + " produced a great work of culture", "culture",
                     "kind=cultural_achievement tribe=\"" + t.name + "\""
                     + " tribeId=" + std::to_string(t.id)
                     + " cultureScore=" + std::to_string((int)t.cultureScore)
                     + " total=" + std::to_string(t.culturalAchievements));
        }

        // Culture lifts the spirit and stirs the mind of the whole people.
        if (t.cultureScore > 10.0f) {
            float c = t.cultureScore * 0.01f;
            for (int mid : t.memberIds) {
                Entity* e = entityById(entities, mid);
                if (!e || e->entityHealth <= 0.0f) continue;
                e->entityHapiness = clamp(e->entityHapiness + c * 0.3f, 0.0f, 100.0f);
                e->entityBoredom  = clamp(e->entityBoredom  - c * 0.3f, 0.0f, 100.0f);
            }
        }
    }

    // A collapse this very day scorches the cultural institutions too.
    if (lastCollapseDay == day)
        for (Tribe& t : tribes) t.cultureScore *= 0.9f;
}

// ── Climate & natural disasters (Improvement Plan 12) ─────────────────────────
// Beyond the seasonal famine cycle, the land itself occasionally turns violent:
// droughts and floods are common, earthquakes rarer, volcanoes and meteors rare
// and devastating. Each strikes one region — harming its people, and (for the
// tectonic kinds) toppling fortifications and spilling granaries. Scaled by the
// world's catastropheRate divergence knob so seeds differ in how cruel nature is.
void CivilizationEngine::updateClimate(std::vector<Entity>& entities, int day) {
    if (!g_planet) return;
    if (day == lastClimateDay) return;      // once per civ-day (tick fires repeatedly)
    lastClimateDay = day;

    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    float cata = g_worldSeed.divergence.catastropheRate;

    // regionPopulation was refreshed by updateCarryingCapacity earlier this tick.
    for (const auto& kv : regionPopulation) {
        int rid = kv.first, pop = kv.second;
        if (pop <= 0) continue;
        if (roll(rng) >= 0.015f * cata) continue;   // disasters are rare

        float r = roll(rng);
        std::string type; float severity;
        if      (r < 0.40f) { type = "drought";           severity = 0.3f + roll(rng) * 0.4f; }
        else if (r < 0.70f) { type = "flood";             severity = 0.3f + roll(rng) * 0.4f; }
        else if (r < 0.90f) { type = "earthquake";        severity = 0.4f + roll(rng) * 0.4f; }
        else if (r < 0.98f) { type = "volcanic eruption"; severity = 0.6f + roll(rng) * 0.4f; }
        else                { type = "meteor strike";     severity = 0.8f + roll(rng) * 0.2f; }
        bool tectonic = (type != "drought" && type != "flood");

        int killed = 0;
        for (Entity& e : entities) {
            if (e.entityHealth <= 0.0f) continue;
            const Tile* t = g_planet->tileAtWorld(e.posX, e.posY);
            int erid = (t && t->regionId >= 0) ? t->regionId : e.originRegionId;
            if (erid != rid) continue;
            float before = e.entityHealth;
            if (type == "drought") {
                e.entityHunger = std::min(100.0f, e.entityHunger + severity * 40.0f);
                e.foodStore    = std::max(0.0f,   e.foodStore    - severity * 10.0f);
                e.entityHealth = std::max(0.0f,   e.entityHealth - severity * 8.0f);
            } else if (type == "flood") {
                e.entityHealth = std::max(0.0f,   e.entityHealth - severity * 18.0f);
                e.entityStress = std::min(100.0f, e.entityStress + severity * 20.0f);
            } else if (type == "earthquake") {
                e.entityHealth = std::max(0.0f,   e.entityHealth - severity * 22.0f);
            } else {  // volcano / meteor
                e.entityHealth = std::max(0.0f,   e.entityHealth - severity * 40.0f);
                e.entityStress = std::min(100.0f, e.entityStress + severity * 30.0f);
            }
            e.entityHapiness = std::max(0.0f, e.entityHapiness - severity * 15.0f);
            if (before > 0.0f && e.entityHealth <= 0.0f) { e.pendingDeathCause = type; ++killed; }
        }

        // Tectonic disasters wreck the built environment too.
        if (tectonic) {
            for (Tribe& tb : tribes) {
                if (tb.regionId != rid) continue;
                tb.fortificationLevel *= (1.0f - severity * 0.4f);
                tb.matStock           *= (1.0f - severity * 0.3f);
                tb.granary            *= (1.0f - severity * 0.2f);
            }
        }

        ++totalDisasters;
        logEvent(day, "DISASTER: a " + type + " struck the land", "environment",
                 "kind=natural_disaster type=\"" + type + "\" region=" + std::to_string(rid)
                 + " severity=" + std::to_string((int)(severity * 100))
                 + " killed=" + std::to_string(killed)
                 + " population=" + std::to_string(pop));
    }
}

// ── Technology diffusion between tribes (Improvement Plan 1.4) ─────────────────
// Knowledge no longer stays siloed in the tribe that invented it: neighbours in
// contact, and especially allies and co-religionists, gradually learn each
// other's techs. This spreads innovation across the map (raising each tech's
// knowerCount, which also makes it harder to lose in a dark age — ties to 1.1).
void CivilizationEngine::updateTechDiffusion(std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    for (size_t i = 0; i < tribes.size(); ++i) {
        for (size_t j = i + 1; j < tribes.size(); ++j) {
            Tribe& A = tribes[i];
            Tribe& B = tribes[j];
            bool ally = (A.stances.count(B.id) && A.stances[B.id] == TS_ALLY);
            bool contact = ally || (A.regionId == B.regionId && A.regionId != -1);
            if (!contact) continue;
            if (A.stances.count(B.id) && A.stances[B.id] == TS_AT_WAR) continue; // enemies hoard secrets

            bool sameFaith = (A.dominantReligionId != -1 && A.dominantReligionId == B.dominantReligionId);
            float chance = (ally ? 0.15f : 0.05f) + (sameFaith ? 0.05f : 0.0f);
            if (roll(rng) >= chance) continue;

            Tribe& donor = (A.knownTechIds.size() >= B.knownTechIds.size()) ? A : B;
            Tribe& recv  = (A.knownTechIds.size() >= B.knownTechIds.size()) ? B : A;
            int learned = -1;
            for (int tid : donor.knownTechIds)
                if (!recv.knownTechIds.count(tid)) { learned = tid; break; }
            if (learned < 0) continue;

            recv.knownTechIds.insert(learned);
            if (Innovation* inv = findInnovation(learned)) inv->knowerCount++;
            for (int mid : recv.memberIds) {   // seed one carrier so it can spread onward
                Entity* e = entityById(entities, mid);
                if (e && e->entityHealth > 0.0f) {
                    if (std::find(e->knownTechIds.begin(), e->knownTechIds.end(), learned) == e->knownTechIds.end())
                        e->knownTechIds.push_back(learned);
                    break;
                }
            }
            ++totalTechSpreads;
            std::string tn = "a technique";
            if (Innovation* inv = findInnovation(learned)) tn = inv->name;
            logEvent(day, "The " + recv.name + " learned " + tn + " from the " + donor.name, "innovation",
                     "kind=tech_spread tech=\"" + tn + "\""
                     + " from=\"" + donor.name + "\" to=\"" + recv.name + "\""
                     + (ally ? " via=alliance" : (sameFaith ? " via=faith" : " via=contact")));
        }
    }
}

// ── Emergent social classes (Improvement Plan 4.2) ────────────────────────────
// Wealth (each person's salary.token) sorts the living into elite/upper/middle/
// lower/outcast strata by percentile. Class shapes life: the elite gain esteem
// and influence, the outcasts slide toward despair and unrest. No per-entity
// field is stored — the census is recomputed each civ-day for effects & report.
void CivilizationEngine::updateSocialClasses(std::vector<Entity>& entities, int day) {
    (void)day;
    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    std::vector<float> w;
    w.reserve(entities.size());
    for (const auto& e : entities)
        if (e.entityHealth > 0.0f) w.push_back(std::max(0.0f, e.salary.token));
    if (w.size() < 5) return;
    std::vector<float> s = w;
    std::sort(s.begin(), s.end());
    auto pct = [&](float p) { return s[std::min(s.size() - 1, (size_t)(p * s.size()))]; };
    float p10 = pct(0.10f), p30 = pct(0.30f), p70 = pct(0.70f), p90 = pct(0.90f);

    eliteCount = upperCount = middleCount = lowerCount = outcastCount = 0;
    for (Entity& e : entities) {
        if (e.entityHealth <= 0.0f) continue;
        float tok = std::max(0.0f, e.salary.token);
        if (tok >= p90 && e.isSpecialist) {
            ++eliteCount;
            e.Esteem = clamp(e.Esteem + 0.6f, 0.0f, 100.0f);
            e.entityHapiness = clamp(e.entityHapiness + 0.2f, 0.0f, 100.0f);
        } else if (tok >= p70) {
            ++upperCount;
            e.Esteem = clamp(e.Esteem + 0.3f, 0.0f, 100.0f);
        } else if (tok >= p30) {
            ++middleCount;
        } else if (tok >= p10) {
            ++lowerCount;
            e.entityStress = clamp(e.entityStress + 0.4f, 0.0f, 100.0f);
        } else {
            ++outcastCount;
            e.entityStress = clamp(e.entityStress + 0.8f, 0.0f, 100.0f);
            e.entityMentalHealth = clamp(e.entityMentalHealth - 0.5f, 0.0f, 100.0f);  // despair risk
        }
    }
}

// ── Family dynasties & prestige (Improvement Plan 4.1) ────────────────────────
// Leaders, the wealthy and the devout raise their family's standing; when a
// family's prestige crosses a high bar it is proclaimed a "great family" and its
// living members carry that pride (an esteem bonus). Prestige decays gently so
// dynasties must keep earning their place.
void CivilizationEngine::updateDynasties(std::vector<Entity>& entities, int day) {
    if (!globalKinship) return;
    // Gentle universal decay.
    for (auto& fam : globalKinship->families) fam.prestige *= 0.999f;

    for (Tribe& t : tribes) {
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (!e || e->entityHealth <= 0.0f || e->familyId < 0) continue;
            Family* fam = globalKinship->findFamily(e->familyId);
            if (!fam) continue;
            float gain = 0.0f;
            if (t.leaderId == e->entityId) gain += 0.5f;            // a ruling house
            gain += std::min(0.4f, e->salary.token * 0.0005f);      // wealth
            if (e->religionId >= 0) {
                Religion* r = findReligion(e->religionId);
                if (r && r->founderEntityId == e->entityId) gain += 0.6f;  // founded a faith
            }
            fam->prestige = std::min(100.0f, fam->prestige + gain);
        }
    }

    for (auto& fam : globalKinship->families) {
        if (!fam.prominent && fam.prestige > 80.0f) {
            fam.prominent = true;
            ++totalGreatFamilies;
            for (Entity& e : entities)
                if (e.entityHealth > 0.0f && e.familyId == fam.id)
                    e.Esteem = std::min(100.0f, e.Esteem + 8.0f);
            logEvent(day, "A great family rises: the line of " + fam.name
                     + " has become a power in the land", "dynasty",
                     "kind=great_family family=\"" + fam.name + "\""
                     + " familyId=" + std::to_string(fam.id)
                     + " prestige=" + std::to_string((int)fam.prestige));
        }
    }
}

// ── Colonization (Improvement Plan 13) ────────────────────────────────────────
// When a homeland is badly overcrowded and empty habitable land exists elsewhere,
// adventurous (high-openness) volunteers strike out to found a colony tribe in
// the new region — deliberate expansion rather than mere overflow drift.
void CivilizationEngine::updateColonization(std::vector<Entity>& entities, int day) {
    if (!g_planet) return;
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    if (roll(rng) > 0.06f) return;   // a rare, deliberate undertaking

    int src = -1; float worst = 1.4f;
    for (auto& kv : regionPopulation) {
        float cap = regionCapacity.count(kv.first) ? regionCapacity[kv.first] : 0.0f;
        if (cap > 0.0f) {
            float over = kv.second / cap;
            if (over > worst) { worst = over; src = kv.first; }
        }
    }
    if (src < 0) return;

    int tgt = -1;
    for (const auto& r : g_planet->regions) {
        if (!r.habitable || r.id == src) continue;
        int pop = regionPopulation.count(r.id) ? regionPopulation[r.id] : 0;
        if (pop < 5) { tgt = r.id; break; }
    }
    if (tgt < 0) return;
    const RegionInfo* tr = g_planet->regionById(tgt);
    if (!tr) return;
    float tgx, tgy; g_planet->gridToWorld((int)tr->centerGX, (int)tr->centerGY, tgx, tgy);

    std::vector<Entity*> colonists;
    for (Entity& e : entities) {
        if (colonists.size() >= 14) break;
        if (e.entityHealth <= 0.0f || e.personality.openness < 45.0f) continue;
        const Tile* t = g_planet->tileAtWorld(e.posX, e.posY);
        int erid = (t && t->regionId >= 0) ? t->regionId : e.originRegionId;
        if (erid != src) continue;
        colonists.push_back(&e);
    }
    if (colonists.size() < 6) return;

    for (Entity* e : colonists) {
        e->posX = tgx + (roll(rng) * 20.0f - 10.0f);
        e->posY = tgy + (roll(rng) * 20.0f - 10.0f);
        e->originRegionId = tgt;
        e->tribeId = -1;
    }
    if (formTribe(colonists, day)) {
        ++totalColonies;
        logEvent(day, "COLONISATION: " + std::to_string(colonists.size())
                 + " pioneers set out and founded a colony in open land", "migration",
                 "kind=colonization settlers=" + std::to_string(colonists.size())
                 + " fromRegion=" + std::to_string(src) + " toRegion=" + std::to_string(tgt));
    }
}

// ── Narrative chains / sagas (Improvement Plan 14) ────────────────────────────
// Lightweight "history book" arcs: a people that loses a war and later wins one
// has a redemption saga; a tribe of great culture and prosperity enters a golden
// age. Rare rolls keep these as memorable, occasional chronicle entries.
void CivilizationEngine::updateNarrativeChains(std::vector<Entity>& entities, int day) {
    (void)entities;
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    for (Tribe& t : tribes) {
        if (t.population() < 6) continue;
        // Redemption: recently victorious after a defeat still fresh in memory.
        if (t.recentWarResult > 0 && t.postWarBoomUntilDay >= day && roll(rng) < 0.02f) {
            ++totalSagas;
            logEvent(day, "SAGA: the " + t.name + " rose from defeat to triumph — a redemption remembered", "saga",
                     "kind=saga arc=\"redemption\" tribe=\"" + t.name + "\"");
        }
        // Golden age: a prosperous, cultured, peaceful people.
        else if (t.cultureScore > 75.0f && t.warExhaustion < 20.0f && roll(rng) < 0.015f) {
            ++totalSagas;
            logEvent(day, "SAGA: the " + t.name + " entered a golden age of art and plenty", "saga",
                     "kind=saga arc=\"golden_age\" tribe=\"" + t.name + "\""
                     + " culture=" + std::to_string((int)t.cultureScore));
        }
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
        case ERA_RENNAISSANCE:       return "Renaissance";
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

// ── War system ──────────────────────────────────────────────────────────────────

// Combat value held in the armoury. A tribe that has poured its wealth into
// swords, bows and trebuchets fields a stronger army than one fighting bare-
// handed; shields, armour and battlements harden its defence. This is what makes
// military strength "a function of the objects in storage".
float CivilizationEngine::weaponAttackStrength(const Tribe& tribe) const {
    float s = 0.0f;
    for (const MarketProduct& w : tribe.weaponStorage) s += (float)w.atk_value;
    return s;
}
float CivilizationEngine::weaponDefenseStrength(const Tribe& tribe) const {
    float s = 0.0f;
    for (const MarketProduct& w : tribe.weaponStorage) s += (float)w.def_value;
    return s;
}

// 0..1 : how the tribe is faring across every war it is currently in. Compares
// our combined strength to that of all our live enemies; 1 = dominant, 0 = being
// overrun. Drives whether to press the attack (buy swords) or dig in (buy
// shields), and feeds war-morale into government legitimacy.
float CivilizationEngine::calculateAdvancementWar(const Tribe& tribe,
                                                  std::vector<Entity>& entities) const {
    float own = calculateTribeMilitaryStrength(tribe, entities);
    float foe = 0.0f;
    for (const auto& kv : tribe.stances) {
        if (kv.second != TS_AT_WAR) continue;
        Tribe* e = const_cast<CivilizationEngine*>(this)->findTribe(kv.first);
        if (e && e->population() > 0) foe += calculateTribeMilitaryStrength(*e, entities);
    }
    if (foe <= 0.01f) return own > 0.01f ? 1.0f : 0.5f;   // no live enemy = winning
    return std::max(0.0f, std::min(1.0f, own / (own + foe)));
}

// Buy attack or defence goods into the armoury, funded by the treasury and
// wartime taxation. A tribe that's winning presses its edge with weapons; one
// that's losing buys defences to survive. FIXED: takes the tribe by reference so
// the purchase — and the raised tax — actually persist (the old by-value version
// mutated a throwaway copy, so no tribe ever accumulated any arms at all).
void CivilizationEngine::contributeToWarEffort(Tribe& tribe, std::vector<Entity>& entities) {
    float adv = calculateAdvancementWar(tribe, entities);
    // Wartime mobilisation raises taxes (heavier the worse the war goes). This
    // fills the treasury but corrodes legitimacy — see updateGovernment().
    tribe.taxeRate = std::min(0.6f, 0.15f + (1.0f - adv) * 0.45f);
    if (tribe.economy.token < 40.0f) return;              // too poor to rearm
    int want = (adv >= 0.5f) ? 0 : 1;                     // winning→attack, losing→defence
    MarketProduct item = g_market.findExpensiveWarItem(want, (int)(tribe.economy.token / 2));
    if (item.name.empty()) return;                        // nothing affordable in stock
    tribe.economy.spendMoney(item.price);
    tribe.weaponStorage.push_back(item);
    // The armoury is finite: old kit is spent/rusts so it can't grow without bound.
    if (tribe.weaponStorage.size() > 64)
        tribe.weaponStorage.erase(tribe.weaponStorage.begin());
}

// ── Government forms & the returning-soldier effect ──────────────────────────────

const char* governmentName(GovernmentType g) {
    switch (g) {
        case GOV_DEMOCRACY:       return "Democracy";
        case GOV_AUTHORITARIAN:   return "Authoritarian Regime";
        case GOV_DIVINE_MONARCHY: return "Divine-Right Monarchy";
        case GOV_OLIGARCHY:       return "Oligarchy";
    }
    return "Council";
}

const char* warReasonName(WarReason r) {
    switch (r) {
        case WAR_ETHNIC:   return "ethnic/holy war";
        case WAR_CONQUEST: return "war of conquest";
        case WAR_RESOURCE: return "war over resources";
        case WAR_TRIBUTE:  return "war of rebellion";
        case WAR_BORDER:   return "border war";
    }
    return "war";
}

// Returning-soldier effect: peace brings a surge of births.
// https://en.wikipedia.org/wiki/Returning_soldier_effect
// Soldiers coming home, relief that the killing is over, and the drive to
// replace the fallen combine into a sharp post-war fertility spike. We model it
// as a temporary multiplier on conception odds, sampled by the reproduction code.
float CivilizationEngine::postWarBirthBoost(int tribeId, int day) const {
    if (tribeId < 0) return 1.0f;
    const Tribe* t = nullptr;
    for (const Tribe& tr : tribes) if (tr.id == tribeId) { t = &tr; break; }
    if (!t) return 1.0f;
    if (t->postWarBoomUntilDay < 0 || day > t->postWarBoomUntilDay) return 1.0f;
    // Victors celebrate hardest; even the defeated rush to rebuild their numbers.
    return t->recentWarResult >= 0 ? 1.8f : 1.5f;
}

// Open a post-war baby-boom window and remember how the war went (+1 win / 0 draw
// / -1 loss) so morale, legitimacy and fertility all respond to the outcome.
void CivilizationEngine::endWarFor(Tribe& tribe, int result, int day) {
    tribe.postWarBoomUntilDay = day + 90;   // ~a season-and-a-half of raised fertility
    tribe.recentWarResult     = result;
}

// Pick the member best suited to lead under a given government form. Each regime
// crowns a different virtue: democracies the well-liked, autocracies the feared,
// theocracies the devout, oligarchies the rich.
int CivilizationEngine::chooseLeaderFor(const Tribe& tribe, GovernmentType gov,
                                        std::vector<Entity>& entities) const {
    int   best = -1;
    float bestScore = -1.0f;
    for (int mid : tribe.memberIds) {
        Entity* e = const_cast<CivilizationEngine*>(this)->entityById(entities, mid);
        if (!e || e->entityHealth <= 0.0f) continue;
        float score = 0.0f;
        switch (gov) {
            case GOV_DEMOCRACY:
                score = e->dominanceRank * 0.4f + e->personality.agreeableness * 0.4f
                      + e->personality.extraversion * 0.2f; break;
            case GOV_AUTHORITARIAN:
                score = e->dominanceRank * 0.5f + (100.0f - e->personality.agreeableness) * 0.3f
                      + e->entityGeneralAnger * 0.2f; break;
            case GOV_DIVINE_MONARCHY:
                score = e->dominanceRank * 0.3f + e->ValueSystem.spiritualNeed * 0.5f
                      + (e->religionId >= 0 ? 20.0f : 0.0f); break;
            case GOV_OLIGARCHY:
                score = e->salary.token * 0.5f + e->dominanceRank * 0.3f; break;
        }
        if (score > bestScore) { bestScore = score; best = mid; }
    }
    return best;
}

// A revolution: legitimacy has collapsed, and the people overthrow the ruling
// order. The new regime is chosen by the tribe's character, a fresh leader fit
// for it is installed, and — where the old or new order rules by force — the
// deposed ruler does not survive the turning.
void CivilizationEngine::stageCoup(Tribe& tribe, std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    tribe.lastCoupDay = day;
    tribe.totalCoups++;
    totalCoups++;

    GovernmentType oldGov = tribe.government;
    // The tribe's temperament decides what replaces the fallen order.
    GovernmentType neo;
    if      (tribe.militarism   > 65.0f)    neo = GOV_AUTHORITARIAN;   // warlike → strongman
    else if (tribe.spiritualism > 65.0f)    neo = GOV_DIVINE_MONARCHY; // devout → god-king
    else if (tribe.economy.token > 400.0f)  neo = GOV_OLIGARCHY;       // rich few → oligarchy
    else                                    neo = GOV_DEMOCRACY;       // otherwise → the people
    if (neo == oldGov)                       // a coup must actually change the order
        neo = (GovernmentType)(((int)oldGov + 1) % 4);

    Entity* oldLeader = entityById(entities, tribe.leaderId);
    int neoLeaderId = chooseLeaderFor(tribe, neo, entities);
    if (neoLeaderId >= 0) tribe.leaderId = neoLeaderId;
    Entity* newLeader = entityById(entities, tribe.leaderId);

    tribe.government      = neo;
    tribe.govSatisfaction = 55.0f;   // the new order enjoys a honeymoon

    // Authoritarian turns (into or out of tyranny) are bloody; the rest are not.
    bool violent = (neo == GOV_AUTHORITARIAN || oldGov == GOV_AUTHORITARIAN);
    if (violent && oldLeader && oldLeader != newLeader) {
        oldLeader->entityHealth = 0.0f;  // the deposed ruler is put to the sword
    }
    // The upheaval rattles everyone.
    for (int mid : tribe.memberIds) {
        Entity* e = entityById(entities, mid);
        if (!e) continue;
        e->entityStress = std::min(100.0f, e->entityStress + (violent ? 12.0f : 5.0f));
    }

    std::string desc = "COUP in the " + tribe.name + ": the "
        + std::string(governmentName(oldGov)) + " falls; a "
        + std::string(governmentName(neo)) + " rises"
        + (newLeader ? " under " + newLeader->name : "");
    logEvent(day, desc, "tribe",
             "kind=coup tribe=\"" + tribe.name + "\" tribeId=" + std::to_string(tribe.id)
             + " oldGov=\"" + governmentName(oldGov) + "\""
             + " newGov=\"" + governmentName(neo) + "\""
             + " violent=" + std::string(violent ? "1" : "0"));
}

// Recompute every tribe's legitimacy from taxes, famine and war, stage coups
// where it has collapsed, and run vassal upkeep / co-belligerence / rebellion.
// Gated to once per civ-day (tick() fires many times per day).
void CivilizationEngine::updateGovernment(std::vector<Entity>& entities, int day) {
    if (day == lastGovDay) return;
    lastGovDay = day;
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    auto clampf = [](float v){ return std::max(0.0f, std::min(100.0f, v)); };

    for (Tribe& t : tribes) {
        if (t.population() == 0) continue;

        // How much hardship the regime's people will swallow before they seethe.
        float taxTol, famineTol;
        switch (t.government) {
            case GOV_AUTHORITARIAN:   taxTol = 0.45f; famineTol = 1.6f; break; // fear keeps order
            case GOV_DIVINE_MONARCHY: taxTol = 0.30f; famineTol = 1.8f; break; // faith soothes want
            case GOV_OLIGARCHY:       taxTol = 0.25f; famineTol = 1.3f; break;
            default: /* DEMOCRACY */  taxTol = 0.20f; famineTol = 1.5f; break; // low taxes, loud voice
        }

        // Ground the mood in the actual state of the tribe's members.
        float avgHap = 0, avgHunger = 0, avgAnger = 0; int n = 0;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (!e || e->entityHealth <= 0.0f) continue;
            avgHap += e->entityHapiness; avgHunger += e->entityHunger;
            avgAnger += e->entityGeneralAnger; n++;
        }
        if (n == 0) continue;
        avgHap /= n; avgHunger /= n; avgAnger /= n;

        float target = 45.0f
                     + (avgHap - 50.0f) * 0.5f                                   // content people
                     - std::max(0.0f, avgHunger - 40.0f) * (0.5f / famineTol)     // famine bites
                     - std::max(0.0f, t.taxeRate - taxTol) * 120.0f               // over-taxation
                     - std::max(0.0f, avgAnger - 45.0f) * 0.25f                   // simmering rage
                     + t.recentWarResult * 12.0f;                                 // glory / shame
        if (t.government == GOV_DIVINE_MONARCHY)
            target += (t.spiritualism - 50.0f) * 0.2f;   // a devout people love their god-king
        target = std::max(0.0f, std::min(100.0f, target));

        // Democracies respond fast to the popular mood; autocracies lag.
        float speed = (t.government == GOV_DEMOCRACY) ? 0.25f : 0.12f;
        t.govSatisfaction = clampf(t.govSatisfaction + (target - t.govSatisfaction) * speed);

        // The memory of a war's outcome fades once the boom window closes.
        if (t.postWarBoomUntilDay >= 0 && day > t.postWarBoomUntilDay) t.recentWarResult = 0;

        // ── Coup: legitimacy has collapsed → revolution ──────────────────────
        if (t.govSatisfaction < 22.0f && (day - t.lastCoupDay) > 120 && t.population() >= 3) {
            float chance = (22.0f - t.govSatisfaction) / 22.0f * 0.18f;
            if (roll(rng) < chance) stageCoup(t, entities, day);
        }
    }

    // ── Vassal upkeep, co-belligerence and rebellion ─────────────────────────
    for (Tribe& t : tribes) {
        if (t.overlordTribeId < 0 || t.population() == 0) continue;
        Tribe* over = findTribe(t.overlordTribeId);
        if (!over || over->population() == 0) { t.overlordTribeId = -1; continue; } // master gone → free

        // Ongoing tribute: a tenth of the vassal's treasury flows to the overlord.
        float tribute = t.economy.token * 0.10f;
        t.economy.spendMoney(tribute);
        over->economy.earnMoney(tribute);

        // Co-belligerence: the vassal is dragged into its overlord's wars…
        for (auto& kv : over->stances) {
            if (kv.second != TS_AT_WAR || kv.first == t.id) continue;
            if (kv.first == t.overlordTribeId) continue;
            Tribe* foe = findTribe(kv.first);
            if (foe && foe->overlordTribeId != over->id) {  // don't war a fellow vassal
                t.stances[kv.first] = TS_AT_WAR;
                foe->stances[t.id]  = TS_AT_WAR;
            }
        }

        // …but a vassal that outgrows or comes to loathe its master rebels.
        float vStr = calculateTribeMilitaryStrength(t, entities);
        float oStr = calculateTribeMilitaryStrength(*over, entities);
        float chance = (vStr > oStr * 1.10f ? 0.10f : 0.0f)
                     + (t.govSatisfaction < 30.0f ? 0.05f : 0.0f);
        if (chance > 0.0f && roll(rng) < chance)
            rebelAgainstOverlord(t, *over, entities, day);
    }
}

// A vassal casts off its overlord — freedom won at sword-point, so relations
// crater and the war reignites (a WAR_TRIBUTE / rebellion casus belli).
void CivilizationEngine::rebelAgainstOverlord(Tribe& vassal, Tribe& overlord,
                                              std::vector<Entity>& entities, int day) {
    totalRebellions++;
    vassal.overlordTribeId = -1;
    overlord.vassalTribeIds.erase(vassal.id);
    vassal.vassalSinceDay = -1;

    vassal.relations[overlord.id] = -80.0f; overlord.relations[vassal.id] = -80.0f;
    vassal.stances[overlord.id]   = TS_AT_WAR; overlord.stances[vassal.id] = TS_AT_WAR;
    totalWarsDeclared++;

    for (int mid : vassal.memberIds) {
        Entity* e = entityById(entities, mid);
        if (!e) continue;
        e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + 15.0f);
        e->entityHapiness     = std::min(100.0f, e->entityHapiness + 6.0f);  // hope of freedom
    }
    vassal.govSatisfaction = std::min(100.0f, vassal.govSatisfaction + 25.0f);

    logEvent(day, "REBELLION: the " + vassal.name + " rise up against their overlords, the "
                  + overlord.name, "war",
             "kind=rebellion reason=\"" + std::string(warReasonName(WAR_TRIBUTE)) + "\""
             + " vassal=\"" + vassal.name + "\" vassalId=" + std::to_string(vassal.id)
             + " overlord=\"" + overlord.name + "\" overlordId=" + std::to_string(overlord.id));
}

// Subjugate rather than annihilate: the beaten tribe survives as a vassal — it
// keeps its name and lands but forfeits a tenth of its wealth now (and each turn
// after), marches to the victor's wars, and may one day rebel.
void CivilizationEngine::vassalizeTribe(Tribe& victor, Tribe& loser,
                                        std::vector<Entity>& entities, int day) {
    totalVassalizations++;
    victor.stances[loser.id] = TS_NEUTRAL; loser.stances[victor.id] = TS_NEUTRAL;
    victor.ethnicWarWith.erase(loser.id);  loser.ethnicWarWith.erase(victor.id);

    loser.overlordTribeId = victor.id;
    loser.vassalSinceDay  = day;
    victor.vassalTribeIds.insert(loser.id);

    // Immediate spoils: a tenth of the vassal's treasury changes hands.
    float spoils = loser.economy.token * 0.10f;
    loser.economy.spendMoney(spoils);
    victor.economy.earnMoney(spoils);

    for (int mid : loser.memberIds) {
        Entity* e = entityById(entities, mid);
        if (!e) continue;
        e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + 10.0f);
        e->entityHapiness     = std::max(0.0f, e->entityHapiness - 8.0f);
    }
    loser.govSatisfaction = std::max(0.0f, loser.govSatisfaction - 20.0f);

    endWarFor(victor, +1, day);
    endWarFor(loser, -1, day);

    logEvent(day, victor.name + " subjugated the " + loser.name
                  + ", who bend the knee as vassals", "war",
             "kind=vassalage victor=\"" + victor.name + "\" victorId=" + std::to_string(victor.id)
             + " vassal=\"" + loser.name + "\" vassalId=" + std::to_string(loser.id)
             + " spoils=" + std::to_string((int)spoils));
}

void CivilizationEngine::processWarTick(std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    // War weariness bleeds off slowly in peacetime (Plan 2.1.B). Tribes at war
    // re-accumulate it below faster than this decay, so it climbs while the war
    // drags on and recovers once the fighting stops.
    for (auto& t : tribes)
        t.warExhaustion = std::max(0.0f, t.warExhaustion - 0.2f);

    for (size_t i = 0; i < tribes.size(); ++i) {
        for (size_t j = i + 1; j < tribes.size(); ++j) {
            Tribe& A = tribes[i];
            Tribe& B = tribes[j];

            auto aIt = A.stances.find(B.id);
            if (aIt == A.stances.end() || aIt->second != TS_AT_WAR) continue;

            contributeToWarEffort(A, entities);
            contributeToWarEffort(B, entities);

            bool ethnicWear = A.ethnicWarWith.count(B.id) > 0;
            // Weariness climbs every tick at war — faster in bitter ethnic wars.
            float dExh = 0.5f + (ethnicWear ? 0.4f : 0.0f);
            A.warExhaustion = std::min(100.0f, A.warExhaustion + dExh);
            B.warExhaustion = std::min(100.0f, B.warExhaustion + dExh);

            // War attrition: each tick, both sides suffer casualties
            float aStr = calculateTribeMilitaryStrength(A, entities);
            float bStr = calculateTribeMilitaryStrength(B, entities);
            float aDef = calculateTribeDefenseStrength(A, entities);
            float bDef = calculateTribeDefenseStrength(B, entities);


            // Battle resolution with randomness
            float aPower = (aStr * 0.7f + aDef * 0.3f) * (0.8f + roll(rng) * 0.4f);
            float bPower = (bStr * 0.7f + bDef * 0.3f) * (0.8f + roll(rng) * 0.4f);

            bool ethnic = A.ethnicWarWith.count(B.id) > 0;

            if (roll(rng) < (ethnic ? 0.34f : 0.20f)) { // wars now erupt into pitched battle far more often
                executeBattle(A, B, entities, day);
            }

            // War exhaustion: both tribes lose health from attrition. Ethnic wars
            // grind down the civilian population, not just the front line.
            float attrMul = ethnic ? 2.4f : 1.3f;
            float aLossRate = (0.03f + (1.0f - aStr / std::max(1.0f, aStr + bStr)) * 0.05f) * attrMul;
            float bLossRate = (0.03f + (1.0f - bStr / std::max(1.0f, aStr + bStr)) * 0.05f) * attrMul;
            float attrDmg   = ethnic ? 6.0f : 3.0f;

            auto attrit = [&](Tribe& side, float rate) {
                for (int mid : side.memberIds) {
                    Entity* e = entityById(entities, mid);
                    if (!e || e->entityHealth <= 0.0f || roll(rng) >= rate) continue;
                    float before = e->entityHealth;
                    e->entityHealth -= 0.5f + roll(rng) * attrDmg;
                    e->entityStress = std::min(100.0f, e->entityStress + 1.5f);
                    e->entityHapiness = std::max(0.0f, e->entityHapiness - (ethnic ? 2.0f : 0.5f));
                    if (before > 0.0f && e->entityHealth <= 0.0f) totalWarDeaths++;
                }
            };
            attrit(A, aLossRate);
            attrit(B, bLossRate);

            // Peace negotiations. Two roads to peace now (Plan 2.1.B): the armies
            // are spent (little military strength left), OR the people are simply
            // war-weary — a thoroughly exhausted society sues for peace even while
            // still armed, which is what breaks the endless ritual-war pattern.
            // Peace sends the soldiers home — a baby boom follows (returning-
            // soldier effect).
            float maxExh    = std::max(A.warExhaustion, B.warExhaustion);
            bool armiesSpent = (A.relations[B.id] < -70.0f && aStr + bStr < 15.0f);
            bool warWeary    = (maxExh > 70.0f);
            if (armiesSpent || warWeary) {
                float peaceChance = armiesSpent ? 0.15f : 0.0f;
                if (warWeary)
                    peaceChance = std::max(peaceChance,
                                           0.20f + (maxExh - 70.0f) / 30.0f * 0.50f); // →0.70
                if (roll(rng) < peaceChance) {
                    A.relations[B.id] = -20.0f;
                    B.relations[A.id] = -20.0f;
                    A.stances[B.id] = TS_NEUTRAL;
                    B.stances[A.id] = TS_NEUTRAL;
                    A.ethnicWarWith.erase(B.id); B.ethnicWarWith.erase(A.id);
                    A.warExhaustion *= 0.3f; B.warExhaustion *= 0.3f;  // relief of peace
                    endWarFor(A, 0, day); endWarFor(B, 0, day);   // a draw for both
                    logEvent(day, "Exhausted war ends: " + A.name + " and " + B.name + " agree to peace", "war",
                             "kind=peace tribeA=\"" + A.name + "\" tribeAId=" + std::to_string(A.id)
                             + " tribeB=\"" + B.name + "\" tribeBId=" + std::to_string(B.id)
                             + " warExhaustion=" + std::to_string((int)maxExh));
                    continue;   // the war is over this tick — skip the conquest checks
                }
            }

            // ── War outcome: annihilation or subjugation ─────────────────────
            // A tribe ground down to nothing is conquered outright (destroyed and
            // absorbed). A tribe merely beaten — decisively out-powered and still
            // hated — may instead be made a vassal, unless the war is an ethnic
            // one, which tends toward extermination rather than mercy.
            // Wars must END, not grind on for centuries as bloodless ritual
            // (Plan 2.1): a gutted tribe is absorbed, and a merely-beaten one is
            // far more readily vassalised than before — so the map consolidates
            // and the endless-war count collapses toward a realistic handful.
            if (A.population() < 4 && B.population() >= 4) {
                conquerTribe(B, A, entities, day);
            } else if (B.population() < 4 && A.population() >= 4) {
                conquerTribe(A, B, entities, day);
            } else if (A.population() >= 4 && B.population() >= 4) {
                Tribe& strong = (aStr >= bStr) ? A : B;
                Tribe& weak   = (aStr >= bStr) ? B : A;
                float  sStr   = (aStr >= bStr) ? aStr : bStr;
                float  wStr   = std::max(1.0f, (aStr >= bStr) ? bStr : aStr);
                bool   crushed = (sStr > wStr * 2.0f)
                                 && weak.relations[strong.id] < -45.0f
                                 && weak.overlordTribeId < 0;
                if (crushed && roll(rng) < 0.45f) {
                    // Even a holy war can now end in subjugation rather than an
                    // eternal stalemate (the victor imposes their faith as tribute).
                    vassalizeTribe(strong, weak, entities, day);
                }
            }
        }
    }
}

void CivilizationEngine::executeBattle(Tribe& attacker, Tribe& defender, std::vector<Entity>& entities, int day) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    totalBattles++;
    // Stigmergy (Step 4): battle sites reek of danger - agents route around
    // them for days until the field decays.
    if (g_pheromoneField.ready()) {
        g_pheromoneField.deposit(defender.centerX, defender.centerY,
                                 PheromoneField::DANGER, 30.0f);
        g_pheromoneField.deposit(attacker.centerX, attacker.centerY,
                                 PheromoneField::DANGER, 12.0f);
    }
    // Ethnic / hate wars are far bloodier than ordinary border skirmishes.
    bool ethnic = attacker.ethnicWarWith.count(defender.id) > 0;
    float aStr = calculateTribeMilitaryStrength(attacker, entities);
    float dStr = calculateTribeMilitaryStrength(defender, entities);

    // Tech, fortifications and war-weariness shape the odds (Plan 2.2/2.3).
    float fort   = defender.fortificationLevel;
    float aBonus = (attacker.militarism / 100.0f) * 1.5f
                   * TechTreeSystem::militaryMultiplier(attacker)
                   * (1.0f - attacker.warExhaustion / 250.0f);          // exhaustion saps offense
    float dBonus = ((defender.militarism / 100.0f) * 0.8f + (defender.collectivism / 100.0f) * 0.7f)
                   * TechTreeSystem::defenseMultiplier(defender)
                   * (1.0f + fort / 100.0f * 0.8f)                       // works add up to +80% defense
                   * (1.0f - defender.warExhaustion / 250.0f);

    float aResult = aStr * (0.6f + roll(rng) * 0.8f) * aBonus;
    float dResult = dStr * (0.6f + roll(rng) * 0.8f) * dBonus;

    std::string desc;
    std::string outcome = "stalemate";
    std::string winner = "none";
    std::string loser  = "none";
    if (aResult > dResult * 1.3f) {
        defender.relations[attacker.id] = std::max(-100.0f, defender.relations[attacker.id] - 18.0f);
        attacker.relations[defender.id] = std::min(100.0f, attacker.relations[defender.id] + 8.0f);
        desc = attacker.name + " won a decisive battle against " + defender.name;
        outcome = "attacker_victory"; winner = attacker.name; loser = defender.name;
    } else if (dResult > aResult * 1.3f) {
        attacker.relations[defender.id] = std::max(-100.0f, attacker.relations[defender.id] - 18.0f);
        defender.relations[attacker.id] = std::min(100.0f, defender.relations[attacker.id] + 8.0f);
        desc = defender.name + " repelled " + attacker.name + "'s assault with a decisive victory";
        outcome = "defender_victory"; winner = defender.name; loser = attacker.name;
    } else {
        desc = attacker.name + " and " + defender.name + " fought to a bloody stalemate";
    }

    // ── Casualty distribution (Plan 2.2) ─────────────────────────────────────
    // Sample a bloodiness tier instead of the old "99% bloodless" flat roll, so
    // most battles now cost lives and a rare few are massacres. Ethnic/holy wars
    // skew hard toward blood; superior military tech makes every tier deadlier.
    float r = roll(rng);
    float pBloodless  = ethnic ? 0.12f : 0.30f;
    float pSkirmish   = ethnic ? 0.33f : 0.40f;
    float pEngagement = ethnic ? 0.30f : 0.20f;
    float pDecisive   = ethnic ? 0.17f : 0.08f;   // remainder → slaughter
    float loserFrac, winnerFrac; std::string tier;
    if      (r < pBloodless)                                      { loserFrac=0.00f; winnerFrac=0.00f; tier="bloodless"; }
    else if (r < pBloodless+pSkirmish)                           { loserFrac=0.04f; winnerFrac=0.02f; tier="skirmish"; }
    else if (r < pBloodless+pSkirmish+pEngagement)               { loserFrac=0.11f; winnerFrac=0.05f; tier="engagement"; }
    else if (r < pBloodless+pSkirmish+pEngagement+pDecisive)     { loserFrac=0.24f; winnerFrac=0.09f; tier="decisive"; }
    else                                                         { loserFrac=0.42f; winnerFrac=0.14f; tier="slaughter"; }
    float lethalMul = 0.7f + 0.3f * std::max(TechTreeSystem::militaryMultiplier(attacker),
                                             TechTreeSystem::militaryMultiplier(defender));
    loserFrac  *= lethalMul;
    winnerFrac *= lethalMul;

    int fallen = 0;
    auto killFrac = [&](Tribe& side, float frac, bool isDefender) {
        if (frac <= 0.0f) return;
        if (isDefender) frac *= (1.0f - std::min(0.5f, fort / 200.0f)); // walls shelter the defenders
        std::vector<Entity*> alive;
        for (int mid : side.memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f) alive.push_back(e);
        }
        int kills = (int)std::round(alive.size() * frac);
        for (int k = 0; k < kills && !alive.empty(); ++k) {
            int idx = std::min((int)alive.size() - 1, (int)(roll(rng) * alive.size()));
            Entity* e = alive[idx]; alive.erase(alive.begin() + idx);
            e->entityHealth = 0.0f;
            if (e->pendingDeathCause.empty()) e->pendingDeathCause = "fallen in battle";
            totalWarDeaths++; fallen++;
        }
        // The survivors are shaken.
        for (Entity* e : alive) e->entityStress = std::min(100.0f, e->entityStress + 6.0f);
    };
    if (outcome == "attacker_victory") { killFrac(defender, loserFrac, true);  killFrac(attacker, winnerFrac, false); }
    else if (outcome == "defender_victory") { killFrac(attacker, loserFrac, false); killFrac(defender, winnerFrac, true); }
    else { killFrac(attacker, loserFrac * 0.6f, false); killFrac(defender, loserFrac * 0.6f, true); }

    // ── Territorial stakes (Plan 2.1.A-lite) ─────────────────────────────────
    // A decisive assault doesn't just bleed the enemy — it takes their land and
    // plunders their stores, so victory finally moves the map and pays off.
    if (outcome == "attacker_victory" && (tier == "decisive" || tier == "slaughter")) {
        float land    = std::max(0.0f, defender.territory) * 0.15f;
        float plunder = std::max(0.0f, defender.granary)   * 0.20f;
        defender.territory -= land;    attacker.territory += land;
        defender.granary   -= plunder; attacker.granary   += plunder;
        desc += " and seized their land and stores";
    }

    if (fallen > 0)
        desc += " — " + std::to_string(fallen) + " fell in the fighting";

    logEvent(day, desc, "war",
             "kind=battle outcome=" + outcome + " tier=" + tier
             + " ethnic=" + std::string(ethnic ? "1" : "0")
             + " attacker=\"" + attacker.name + "\" attackerId=" + std::to_string(attacker.id)
             + " defender=\"" + defender.name + "\" defenderId=" + std::to_string(defender.id)
             + " winner=\"" + winner + "\" loser=\"" + loser + "\""
             + " fallen=" + std::to_string(fallen));
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

// ── Institutional resilience (Plan 1.1, Option D) ──────────────────────────────
// How well the civilisation preserves knowledge through a collapse. Grows with the
// current era (schools, writing, specialists) and with political/economic
// stability (treaties over wars, a broad specialist class). 0 = a fragile band
// that forgets easily; ~0.95 = an advanced society that barely notices a shock.
float CivilizationEngine::darkAgeResistance(const std::vector<Entity>& entities) const {
    (void)entities;
    // Era contributes the bulk: a Medieval society is structurally far more
    // shock-resistant than a Stone-Age one.
    float eraFrac = (float)era / (float)ERA_MODERN;            // 0..1
    float resist  = eraFrac * 0.70f;

    // Diplomatic stability: a world knit together by treaties, not consumed by
    // war, keeps its knowledge alive.
    int treaties = activeTreatyCount();
    int warring  = 0;
    for (const auto& t : tribes)
        for (const auto& s : t.stances)
            if (s.second == TS_AT_WAR) { warring++; break; }
    float diploStab = (float)treaties / (float)std::max(1, treaties + warring);
    resist += diploStab * 0.15f;

    // A society that can spare scholars/artisans from the fields preserves lore.
    int specialists = 0, pop = 0;
    for (const auto& t : tribes) { specialists += t.specialistCount; pop += t.population(); }
    if (pop > 0) resist += std::min(1.0f, (float)specialists / (float)pop) * 0.15f;

    return std::min(0.95f, std::max(0.0f, resist));
}

void CivilizationEngine::loseTechnology(int day, const std::string& regionName, float resistance) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    // A collapse erases fragile, rarely-known innovations -> a dark age. But an
    // advanced, resilient civilisation shields its era-critical foundations
    // (Metal/Iron/Fortification/Writing) so a single bad harvest cannot cascade
    // it back down the era ladder.
    std::vector<int> fragile;
    for (size_t i = 0; i < innovations.size(); ++i) {
        const auto& inv = innovations[i];
        if (inv.knowerCount <= 2 && inv.complexity > 45.0f) {
            bool critical = (inv.name == "Metal Working" || inv.name == "Iron Smelting"
                             || inv.name == "Fortification" || inv.name == "Writing"
                             || inv.name == "Gunpowder"     || inv.name == "Scientific Method"
                             || inv.name == "Steam Power");
            if (critical && roll(rng) < resistance) continue;   // shielded by institutions
            fragile.push_back((int)i);
        }
    }

    // The dark age still happened (a hardship for the people), but a resilient
    // society may weather it with no loss of knowledge at all.
    darkAgeCount++;
    lastCollapseDay = day;
    if (fragile.empty() || roll(rng) < resistance) {
        logEvent(day, "A dark age gripped " + regionName
                 + ", but its institutions preserved their knowledge", "innovation",
                 "kind=dark_age_weathered region=\"" + regionName + "\"");
        return;
    }

    std::uniform_int_distribution<int> pickD(0, (int)fragile.size() - 1);
    int idx = fragile[pickD(rng)];
    std::string lost = innovations[idx].name;
    innovations.erase(innovations.begin() + idx);
    // Strip it from every entity who knew it.
    logEvent(day, "Knowledge of " + lost + " was lost in the collapse of " + regionName, "innovation");
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
                 + "a crowded homeland toward open land", "migration",
                 "kind=migration people=" + std::to_string(moved)
                 + " fromRegion=" + std::to_string(fromRegion)
                 + " toRegion=" + std::to_string(target)
                 + " overcrowdedPop=" + std::to_string(livingPop)
                 + " capacity=" + std::to_string((int)capacity));
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
                e.entityHealth = std::max(0.0f, e.entityHealth - severity * 2.5f);
                e.entityHapiness = std::max(0.0f, e.entityHapiness - severity * 3.0f);
            }
            // Pressure release: migration toward open land.
            migrateOverflow(rid, pop, K, entities, day);

            // Severe, sustained overshoot can trigger a collapse & dark age. The
            // odds fall as the civilisation grows more resilient (Plan 1.1): an
            // advanced, peaceful, specialist-rich society rarely tips into a dark
            // age at all, and preserves its knowledge when it does.
            if (ratio > 1.8f) {
                std::uniform_real_distribution<float> roll(0.0f, 1.0f);
                float resist = darkAgeResistance(entities);
                if (roll(rng) < 0.05f * cata * (1.0f - resist)) {
                    std::string rn = "region " + std::to_string(rid);
                    logEvent(day, "Famine and collapse struck " + rn, "famine",
                             "kind=famine region=" + std::to_string(rid)
                             + " population=" + std::to_string(pop)
                             + " capacity=" + std::to_string((int)K)
                             + " overshootRatio=" + std::to_string(ratio));
                    loseTechnology(day, rn, resist);
                }
            }
        }
    }
}

void CivilizationEngine::breakCrossTribeCouples(Tribe& A, Tribe& B, std::vector<Entity>& entities, int day) {
    int broken = 0;
    auto removeCoupleLink = [](Entity* e, int partnerId) {
        auto& v = e->list_entityPointedCouple;
        v.erase(std::remove_if(v.begin(), v.end(),
            [&](const entityPointedCouple& c){
                return (c.pointedEntity && c.pointedEntity->entityId == partnerId) || c.id == partnerId;
            }), v.end());
    };
    for (int aid : A.memberIds) {
        Entity* ea = entityById(entities, aid);
        if (!ea) continue;
        for (int bid : B.memberIds) {
            Entity* eb = entityById(entities, bid);
            if (!eb) continue;
            bool paired = false;
            for (const auto& c : ea->list_entityPointedCouple)
                if ((c.pointedEntity && c.pointedEntity->entityId == eb->entityId) || c.id == eb->entityId) { paired = true; break; }
            if (!paired) continue;
            removeCoupleLink(ea, eb->entityId);
            removeCoupleLink(eb, ea->entityId);
            // Torn loyalties curdle into grief and resentment.
            ea->entityHapiness = std::max(0.0f, ea->entityHapiness - 12.0f);
            eb->entityHapiness = std::max(0.0f, eb->entityHapiness - 12.0f);
            ea->entityStress   = std::min(100.0f, ea->entityStress + 10.0f);
            eb->entityStress   = std::min(100.0f, eb->entityStress + 10.0f);
            broken++;
        }
    }
    if (broken > 0) {
        totalCouplesBroken += broken;
        logEvent(day, std::to_string(broken) + " couple(s) were torn apart as the "
                 + A.name + " and the " + B.name + " went to war", "war",
                 "kind=couples_broken count=" + std::to_string(broken)
                 + " tribeA=\"" + A.name + "\" tribeB=\"" + B.name + "\"");
    }
}

void CivilizationEngine::conquerTribe(Tribe& victor, Tribe& loser, std::vector<Entity>& entities, int day) {
    totalConquests++;

    // Count survivors absorbed before we empty the loser's roster.
    int survivors = 0;
    for (int mid : loser.memberIds) {
        Entity* e = entityById(entities, mid);
        if (e && e->entityHealth > 0.0f) survivors++;
    }
    logEvent(day, victor.name + " has conquered " + loser.name + "!", "war",
             "kind=conquest victor=\"" + victor.name + "\" victorId=" + std::to_string(victor.id)
             + " loser=\"" + loser.name + "\" loserId=" + std::to_string(loser.id)
             + " survivorsAbsorbed=" + std::to_string(survivors));

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

    // Sever any vassal bonds the destroyed tribe held (as overlord or as vassal).
    if (loser.overlordTribeId >= 0)
        if (Tribe* ov = findTribe(loser.overlordTribeId)) ov->vassalTribeIds.erase(loser.id);
    for (int vid : loser.vassalTribeIds)
        if (Tribe* v = findTribe(vid)) v->overlordTribeId = -1;
    loser.vassalTribeIds.clear();
    loser.overlordTribeId = -1;

    // Victory sends the warriors home to a triumphant baby boom.
    endWarFor(victor, +1, day);

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

    // Tech bonus from emergent innovations …
    for (int tid : tribe.knownTechIds) {
        Innovation* inv = const_cast<CivilizationEngine*>(this)->findInnovation(tid);
        if (inv && inv->category == "military") strength += 1.5f;
    }
    // … and from the deliberate tech tree (Bronze/Iron Working etc.).
    strength *= TechTreeSystem::militaryMultiplier(tribe);
    // Arms in the storehouse: a stockpile of swords and bows turns willing bodies
    // into a real army. Each held weapon adds its attack value to the tribe's might.
    strength += weaponAttackStrength(tribe);
    return strength;
}

float CivilizationEngine::calculateTribeDefenseStrength(const Tribe& tribe, std::vector<Entity>& entities) const {
    float defense = tribe.population() * 0.3f + tribe.collectivism * 0.08f;
    for (int tid : tribe.knownTechIds) {
        Innovation* inv = const_cast<CivilizationEngine*>(this)->findInnovation(tid);
        if (inv && inv->name == "Fortification") defense += 3.0f;
        if (inv && inv->name == "Shield Craft") defense += 2.0f;
    }
    // Masonry / Fortification on the tech tree harden the tribe's defences.
    defense *= TechTreeSystem::defenseMultiplier(tribe);
    // Shields, armour and battlements stockpiled in the armoury bolster defence.
    defense += weaponDefenseStrength(tribe);
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

bool CivilizationEngine::areTribesAtWar(int tribeIdA, int tribeIdB) const {
    if (tribeIdA < 0 || tribeIdB < 0 || tribeIdA == tribeIdB) return false;
    for (const auto& t : tribes) {
        if (t.id != tribeIdA) continue;
        auto it = t.stances.find(tribeIdB);
        return it != t.stances.end() && it->second == TS_AT_WAR;
    }
    return false;
}

std::string CivilizationEngine::getBigSummary() const {
    int livingPop = 0;
    for (const auto& kv : regionPopulation) livingPop += kv.second;

    int atWarTribes = 0, alliances = 0;
    for (const auto& t : tribes) {
        bool w = false;
        for (const auto& s : t.stances) {
            if (s.second == TS_AT_WAR) w = true;
            if (s.second == TS_ALLY)   alliances++;
        }
        if (w) atWarTribes++;
    }
    alliances /= 2; // each alliance counted from both sides

    // Largest tribe & dominant faith.
    std::string biggest = "none"; int biggestPop = 0;
    for (const auto& t : tribes)
        if (t.population() > biggestPop) { biggestPop = t.population(); biggest = t.name; }
    int bestRel = -1; size_t bestFollow = 0;
    for (const auto& r : religions)
        if (r.followerIds.size() > bestFollow) { bestFollow = r.followerIds.size(); bestRel = r.id; }
    std::string relName = "none";
    for (const auto& r : religions) if (r.id == bestRel) { relName = r.name; break; }

    std::stringstream ss;
    ss << "=== STATE OF THE WORLD ===\n";
    ss << getYearDisplay() << "   " << getEraName() << "\n";
    ss << getEraSummary() << "\n\n";
    ss << "Living people : " << livingPop << "   (peak " << peakPopulation << ")\n";
    ss << "Tribes        : " << tribes.size()
       << "   largest: " << biggest << " (" << biggestPop << ")\n";
    ss << "Religions     : " << religions.size() << "   dominant: " << relName << "\n";
    ss << "Technologies  : " << innovations.size()
       << "   dark ages survived: " << darkAgeCount << "\n";
    // Most advanced tribe on the structured tech tree.
    const Tribe* leadTech = nullptr;
    for (const auto& t : tribes)
        if (!leadTech || t.techTreeUnlocked.size() > leadTech->techTreeUnlocked.size())
            leadTech = &t;
    if (leadTech && !leadTech->techTreeUnlocked.empty())
        ss << "Tech leader   : " << leadTech->name << " — "
           << TechTreeSystem::summary(*leadTech) << "\n";
    ss << "\n";
    ss << "--- Vital statistics (whole run) ---\n";
    ss << "Births        : " << totalBirths << "\n";
    ss << "Deaths        : " << totalDeaths
       << "   (of war: " << totalWarDeaths << ")\n";
    ss << "Couples broken by war: " << totalCouplesBroken << "\n\n";
    ss << "--- Conflict ---\n";
    ss << "Wars declared : " << totalWarsDeclared
       << "   (ethnic/hate: " << totalEthnicWars << ")\n";
    ss << "Battles fought: " << totalBattles << "\n";
    ss << "Conquests     : " << totalConquests << "\n";
    ss << "Tribes now at war: " << atWarTribes
       << "   active alliances: " << alliances << "\n";
    ss << "\n--- Diplomacy ---\n";
    ss << "Treaties signed (all time): " << totalTreatiesSigned
       << "   now in force: " << activeTreatyCount() << "\n";
    ss << diplomacySummary() << "\n";
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

void CivilizationEngine::logEvent(int day, const std::string& desc, const std::string& cat,
                                  const std::string& data) {
    CivEvent ev;
    ev.day         = day;
    ev.description = desc;
    ev.category    = cat;
    eventLog.push_front(ev);
    if (eventLog.size() > 120) eventLog.pop_back();

    // Persist the event so the whole civilisation arc survives the run and can be
    // mined by the post-mortem analyst. The deque only ever holds the last 120
    // events; the file keeps them all.
    if (globalLogger) globalLogger->logCiv(day, cat, desc, data);
}
