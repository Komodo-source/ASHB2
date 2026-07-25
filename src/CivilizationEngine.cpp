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
#include "header/LiveConfig.h"  // corruptionMul: live graft-odds tunable (Society Plan 5)
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

// ── II-P1/II-P2: is this people literate? ─────────────────────────────────────
// Writing is the hinge the whole knowledge ratchet turns on (Henrich: recorded
// knowledge stops being lost when its holders die), so "does this tribe have
// writing" has to be asked of BOTH tech systems. There are two: the emergent
// innovation catalogue (`knownTechName`, discovered by individuals) and the
// deliberate prerequisite-gated research tree (`techTreeUnlocked`). Asking only
// the emergent one — as the first cut of II-P1 did — made literacy effectively
// unreachable: long runs reach Writing through the research tree, so the
// archive never opened and the ratchet never engaged. Either path counts.
static bool tribeIsLiterate(const Tribe& t) {
    if (t.knownTechName.count("Writing")) return true;
    static const int kWritingNode = [] {
        for (const TechNode& n : TechTreeSystem::tree())
            if (n.name == "Writing") return n.id;
        return -1;
    }();
    return kWritingNode >= 0 && t.techTreeUnlocked.count(kWritingNode) > 0;
}

// ── II-P1: does this people hold a given technique, by either road? ───────────
// The world has two tech systems that grew up separately: the emergent
// catalogue (`knownTechName`, stumbled upon by individuals) and the deliberate
// research tree (`techTreeUnlocked`, climbed by a people). They name the same
// human achievements differently — the tree's "Iron Working" is the catalogue's
// "Iron Smelting", its "Pottery" is "Clay Shaping" — and nothing translated
// between them. The consequence was severe and invisible: the catalogue's whole
// upper half is prerequisite-chained off Writing and Mathematics, both of which
// most peoples reach through the TREE, and a tribe that had researched writing
// and geometry the hard way still counted as having neither. Philosophy, and
// therefore the Scientific Method, and therefore Steam Power, were unreachable
// in practice no matter how learned a society became. Smelting iron is smelting
// iron however you came to it.
static bool tribeTreeHolds(const Tribe& t, const std::string& catalogName) {
    struct Equiv { const char* tree; const char* catalog; };
    static const Equiv kEquiv[] = {
        {"Writing",         "Writing"},
        {"Mathematics",     "Mathematics"},
        {"Masonry",         "Masonry"},
        {"Currency",        "Currency"},
        {"Fortification",   "Fortification"},
        {"Irrigation",      "Irrigation"},
        {"Iron Working",    "Iron Smelting"},
        {"Bronze Working",  "Metal Working"},
        {"Pottery",         "Clay Shaping"},
        {"Animal Husbandry","Animal Keeping"},
        {"Toolmaking",      "Edge Knapping"},
        {"Fire Mastery",    "Fire Making"},
    };
    for (const Equiv& e : kEquiv) {
        if (catalogName != e.catalog) continue;
        for (const TechNode& n : TechTreeSystem::tree())
            if (n.name == e.tree) return t.techTreeUnlocked.count(n.id) > 0;
    }
    return false;
}

// ── II-P1: the research climate of a people ───────────────────────────────────
// Henrich's collective brain is not only a headcount — it is a headcount that
// can talk to itself, keep records, and afford to put some of its people to
// thinking full time. This returns the multiplier on how OFTEN a member of this
// tribe has an idea worth chasing (the supply of attempts), as distinct from
// `brainOdds` below, which is how often a chased idea actually lands. The two
// together are what turn a flat per-capita invention rate into the accelerating
// curve real history shows: each rung — writing, the school, the press, the
// method — makes the next one cheaper.
//
// Every term is gated on knowledgeMul so that knowledgeMul=0 leaves the old
// flat rate untouched, bit for bit.
float CivilizationEngine::researchClimate(const Entity& ent, const Tribe* tribe) const {
    if (g_liveConfig.knowledgeMul == 0.0f) return 1.0f;
    float m = 1.0f;
    // A scholar's whole occupation is to think about what is not yet known.
    if (ent.isSpecialist && ent.specialization == "scholar") m += 1.6f;
    if (!tribe) return 1.0f + (m - 1.0f) * g_liveConfig.knowledgeMul;
    // Writing lets an idea be worked on across sittings, and across lifetimes.
    if (tribeIsLiterate(*tribe)) m += 0.45f;
    // II-P2: a school is standing institutional support for enquiry — somewhere
    // to consult what is already known before spending a life rediscovering it.
    if (g_liveConfig.institutionMul != 0.0f) {
        const auto* school = institutions.find(tribe->id, environment::InstitutionType::EDUCATION);
        if (school) m += 0.8f * school->legitimacy * school->efficiency * g_liveConfig.institutionMul;
    }
    // The press multiplies every reader; the method turns lucky accidents into
    // a procedure that can be repeated on purpose. These are the two techniques
    // in the catalogue that are *about* discovering techniques.
    if (tribe->knownTechName.count("Printing"))          m += 0.5f;
    if (tribe->knownTechName.count("Scientific Method")) m += 1.2f;
    return 1.0f + (m - 1.0f) * g_liveConfig.knowledgeMul;
}

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
    updateFestivals(entities, day);          // AI upgrade A3/D4: feasts, culture diffusion
    updateTechDiffusion(entities, day);      // Plan 1.4: techs spread tribe→tribe
    // Once-per-civ-day social passes (dynasties, classes, colonisation, sagas).
    if (day != lastDynastyDay) {
        lastDynastyDay = day;
        updateSocialClasses(entities, day);  // Plan 4.2: emergent wealth classes
        updateSettlements(entities, day);    // III-P1: cities emerge, grow, agglomerate, crowd
        updateInstitutions(entities, day);   // II-P2: schools archive & teach, guilds, bureaucracy
        updateTrade(entities, day);          // III-P2: regional prices, trade roads, caravans
        updateSecularCycle(entities, day);   // II-P3: elite overproduction, immiseration, strife
        updateClassReproduction(entities, day); // III-P4: cultural capital, heritable class
        updateLanguages(entities, day);      // IV-P3: tongues drift apart, creolise on contact
        updateCulturalTraits(entities, day);  // IV-P1: traits spread, tip at 25%, diverge
        updateDynasties(entities, day);      // Plan 4.1: family prestige & great families
        updateElections(entities, day);      // Society Plan 3: ballots, councils, taxes
        updateCorruption(entities, day);     // Society Plan 5: graft, scandal, downfall
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

        // §8: the knowledge ratchet, sampled. The plan's claim is that once a
        // people can write, what it knows stops going backwards — so the series
        // that would show a fall is recorded, and the report checks it never
        // does. `techCount` is both tech systems at once (the emergent
        // catalogue and the deliberate tree), because either can carry a
        // technique across a collapse.
        {
            KnowledgeSample k;
            k.day = day;
            // The UNION of what is known, not the sum over peoples: a tribe
            // dying out while its neighbours still hold the same technique is
            // not the world forgetting anything, and counting it as a loss
            // would make the ratchet look broken every time a band starved.
            std::set<int> unlocked;
            for (const Tribe& t : tribes) {
                unlocked.insert(t.techTreeUnlocked.begin(), t.techTreeUnlocked.end());
                if (tribeIsLiterate(t)) k.literate = true;
            }
            k.techCount = (int)innovations.size() + (int)unlocked.size();
            k.darkAges  = darkAgeCount;
            k.era       = (int)era;   // §10.1: did the world cross over, and did it hold?
            knowledgeHistory.push_back(k);
        }

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
               << " classOutcast=" << outcastCount
               << " elections=" << totalElections
               << " successions=" << totalSuccessions
               << " scandals=" << totalScandals
               << " depositions=" << totalDepositions;
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
        electLeader(tribe, entities, day);

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

    // The founding temperament picks the founding order — the same mapping a
    // revolution uses (stageCoup), so a peaceable band of founders actually
    // starts as a democracy instead of every people being born an oligarchy.
    if      (tribe.militarism   > 65.0f) tribe.government = GOV_AUTHORITARIAN;
    else if (tribe.spiritualism > 65.0f) tribe.government = GOV_DIVINE_MONARCHY;
    else                                 tribe.government = GOV_DEMOCRACY;

    // I-P3: a people remembers who founded it, by name, for as long as it lasts.
    tribe.founderId   = bestLeader->entityId;
    tribe.founderName = bestLeader->name;

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
  float collected = 0.0f;
  for (int id_entity : tribe.memberIds) {
    Entity* tribe_member = entityById(ent, id_entity);
    if (!tribe_member || tribe_member->entityHealth <= 0.0f) continue;
    float due = tribe_member->salary.token * tribe.taxeRate;
    tribe.economy.earnMoney(due);
    tribe_member->salary.token -= due;
    collected += due;
  }
  if (collected <= 0.0f) return;

  Entity* leader = entityById(ent, tribe.leaderId);
  if (!leader || leader->entityHealth <= 0.0f) return;

  // The perks of office: a legal stipend and slowly compounding authority —
  // the prize that makes the seat worth seeking, holding, and abusing.
  float stipend = collected * 0.05f;
  tribe.economy.spendMoney(stipend);
  leader->salary.earnMoney(stipend);
  leader->auctoritas = std::min(100.0f, leader->auctoritas + 0.2f);

  // ── Embezzlement (Society Plan 5) ──────────────────────────────────────────
  // A low-integrity ruler skims the take. Transparent regimes make theft risky;
  // opaque ones invite it. Graft accumulates on the tribe until a scandal
  // (updateCorruption) drags it into the light.
  float oversight;
  switch (tribe.government) {
      case GOV_DEMOCRACY:       oversight = 0.8f; break;
      case GOV_OLIGARCHY:       oversight = 0.4f; break;
      case GOV_DIVINE_MONARCHY: oversight = 0.3f; break;
      default: /* AUTHORITARIAN */ oversight = 0.2f; break;
  }
  float greed = (100.0f - leader->integrity) / 100.0f
              * (0.5f + leader->ValueSystem.achievementDrive / 200.0f);
  std::uniform_real_distribution<float> roll(0.0f, 1.0f);
  if (roll(rng) < greed * (1.0f - oversight) * g_liveConfig.corruptionMul) {
      float take = std::min(tribe.economy.token, collected * greed * 0.5f);
      if (take > 0.0f) {
          tribe.economy.spendMoney(take);
          leader->salary.earnMoney(take);
          tribe.corruption = std::min(100.0f, tribe.corruption + 1.0f + take * 0.05f);
      }
  }
}

void CivilizationEngine::electLeader(Tribe& tribe, std::vector<Entity>& entities, int day) {
    if (tribe.memberIds.empty()) return;

    // A living leader who is still one of us keeps the seat: turnover now runs
    // through ballots (updateElections), challenges and coups, not a daily
    // dominance contest that made every voted-in ruler vanish a tick later.
    Entity* sitting = entityById(entities, tribe.leaderId);
    if (sitting && sitting->entityHealth > 0.0f && tribe.isMember(tribe.leaderId)) return;

    // The seat is empty — a succession. The old ruler's body may still be in the
    // world vector even though the member roll dropped them, so their family is
    // recoverable for a hereditary claim.
    int heir = -1;
    if (sitting && globalKinship
        && (tribe.government == GOV_DIVINE_MONARCHY || tribe.government == GOV_OLIGARCHY)
        && sitting->familyId >= 0) {
        float bestAuc = -1.0f;
        for (int mid : tribe.memberIds) {
            Entity* e = entityById(entities, mid);
            if (!e || e->entityHealth <= 0.0f || e->entityAge < 16.0f) continue;
            if (e->familyId != sitting->familyId) continue;
            if (e->auctoritas > bestAuc) { bestAuc = e->auctoritas; heir = mid; }
        }
    }

    bool hereditary = (heir >= 0);
    if (heir < 0) heir = chooseLeaderFor(tribe, tribe.government, entities);
    if (heir < 0) return;

    tribe.leaderId = heir;
    totalSuccessions++;
    // A democracy doesn't inherit its dead ruler's mantle — it votes again.
    if (tribe.government == GOV_DEMOCRACY) tribe.nextElectionDay = day;

    Entity* neo = entityById(entities, heir);
    if (!neo) return;
    std::string famName = "-";
    if (hereditary && globalKinship) {
        Family* fam = globalKinship->findFamily(neo->familyId);
        if (fam) {
            fam->prestige = std::min(100.0f, fam->prestige + 2.0f);  // a ruling line
            famName = fam->name;
        }
    }
    logEvent(day, neo->name + (hereditary ? " inherits" : " assumes")
                  + " the leadership of " + tribe.name, "tribe",
             "kind=succession tribe=\"" + tribe.name + "\" tribeId=" + std::to_string(tribe.id)
             + " heir=\"" + neo->name + "\" heirId=" + std::to_string(neo->entityId)
             + " hereditary=" + (hereditary ? "1" : "0")
             + " family=\"" + famName + "\"");
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
        // IV-P3: a people speaks its homeland's tongue until it forks its own.
        // Assigned HERE and not at founding: regionId is only discovered once
        // the tribe's centre is known, so an assignment in formTribe() read -1
        // and left every people languageless (and the whole mechanism inert).
        if (tribe.languageId < 0 && tribe.regionId >= 0) tribe.languageId = tribe.regionId;
    }
}

void CivilizationEngine::updateTribeValues(Tribe& tribe, std::vector<Entity>& entities) {
    if (tribe.memberIds.empty()) return;
    float mil = 0, spi = 0, col = 0, inn = 0, fes = 0; float n = 0;
    for (int mid : tribe.memberIds) {
        Entity* e = entityById(entities, mid);
        if (!e || e->entityHealth <= 0.0f) continue;
        mil += (100.0f - e->personality.agreeableness);
        spi += e->ValueSystem.spiritualNeed;
        col += e->ValueSystem.collectivism;
        inn += e->personality.openness;
        fes += (e->ValueSystem.hedonism + e->personality.extraversion) * 0.5f;
        n++;
    }
    if (n == 0) return;
    // Slow drift toward member average
    auto drift = [&](float& v, float target) { v += (target / n - v) * 0.05f; };
    drift(tribe.militarism,   mil);
    drift(tribe.spiritualism, spi);
    drift(tribe.collectivism, col);
    drift(tribe.innovation,   inn);
    drift(tribe.festivity,    fes);   // A3: how much this people celebrates

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
        // III-P1: a real settlement is also administrative capacity — streets,
        // stores and offices that let strangers live together at a scale a camp
        // cannot hold. Each tier buys ~6 more people before the place fissions,
        // which is what lets a city keep growing instead of splitting at 24 for
        // ever. cityMul==0 leaves the threshold at exactly 24 (bit-exact off).
        // The size of the bonus is what decides whether the world has CITIES or
        // just a lot of villages. At +6 a tier and +10 for a bureaucracy the
        // ceiling topped out near 58 and in practice nothing outgrew about
        // twenty: thirty-three settlements of nearly identical size, which is
        // the one shape a rank-size law cannot fit (R^2 0.67 against a bar of
        // 0.75). A place with streets, stores, offices and standing rules holds
        // strangers together at a scale a camp cannot approach — that is the
        // whole of what urbanism buys — so the gap between the best-run place
        // and an ordinary one has to be large. It is the SPREAD that produces
        // Zipf, not the average.
        int splitAt = 24;
        if (g_liveConfig.cityMul != 0.0f)
            splitAt += (int)(12.0f * tribes[idx].settlementTier * g_liveConfig.cityMul);
        // II-P2: a bureaucracy governs strangers the way acquaintance cannot —
        // records, offices and standing rules add another ~10 people of reach.
        // This is the administrative ceiling that decides how large a society
        // can get before it fragments, and it is *earned* institutional work,
        // not a constant.
        splitAt += (int)(30.0f * adminCapacity(tribes[idx].id));
        if (tribes[idx].population() <= splitAt && !civilWar) continue;

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

        const int parentLang = tribes[idx].languageId;
        formTribe(groupB, day); // may push_back to tribes; idx still valid (no erase)

        // ── IV-P3: ethnogenesis ─────────────────────────────────────────────
        // A people that walks away takes its parents' speech and then stops
        // sharing their changes. This is where a language family branches, and
        // therefore where the barriers that the rest of IV-P3 gates on actually
        // come from — without it every people in a one-cradle world speaks the
        // same tongue for ever.
        if (g_liveConfig.languageMul != 0.0f && g_lexicon && !tribes.empty()) {
            Tribe& daughter = tribes.back();
            if (daughter.id != tribes[idx].id && parentLang >= 0) {
                daughter.languageId =
                    g_lexicon->cloneLanguage(parentLang, (uint64_t)(daughter.id + 1) * 0x9E37u);
                float intel = g_lexicon->intelligibility(daughter.languageId, parentLang);
                logEvent(day, "The speech of the " + daughter.name
                         + " begins to part from that of the " + splitName, "language",
                         "kind=language_split daughter=\"" + daughter.name + "\""
                         + " daughterId=" + std::to_string(daughter.id)
                         + " parent=\"" + splitName + "\""
                         + " languageId=" + std::to_string(daughter.languageId)
                         + " intelligibility=" + std::to_string(intel));
            }
        }

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

    // IV-P2: doctrine, rite and schism — the three things that make a faith a
    // faith rather than a label. Runs after congregations and institutions are
    // settled for the day, so all three read current numbers.
    updateDoctrine(entities, day);
}

// ── IV-P3: language that matters (Parallel-Earth plan Track IV) ──────────────
// `Lexicon` has generated per-region tongues since the world-gen work, and they
// have drifted and blended — but purely as decoration. Nothing ever *asked*
// whether two peoples could understand each other, so a technique crossed an
// unintelligible border exactly as fast as it passed between neighbours who
// shared a language, and a treaty was as easy to strike with strangers whose
// speech was opaque. That is the F8 gap: language existed and did nothing.
//
// Intelligibility now gates the things language really gates:
//   • DIFFUSION — techniques and practices travel on understanding. A language
//     barrier slows the collective brain (II-P1) exactly where real ones do.
//   • TRADE — a caravan whose crew cannot haggle carries less (III-P2).
//   • DIPLOMACY — peoples who cannot talk warm to each other more slowly.
//   • ASSIMILATION — a conquered people that shares its conqueror's tongue is
//     absorbed; one that does not stays a distinct nation under new masters.
// And contact works the other way: speakers who trade and ally creolise toward
// each other, while isolation lets drift pull them apart — so language
// boundaries are themselves an outcome of history, not a fixed backdrop.
float CivilizationEngine::mutualIntelligibility(const Tribe& a, const Tribe& b) const {
    if (g_liveConfig.languageMul == 0.0f) return 1.0f;   // kill switch: no barriers
    if (!g_lexicon) return 1.0f;
    float raw = g_lexicon->intelligibility(a.languageId >= 0 ? a.languageId : a.regionId,
                                          b.languageId >= 0 ? b.languageId : b.regionId);
    // The knob scales how much of a barrier a barrier is: at 1.0 an opaque
    // tongue is a real wall, at 0 there are no walls at all.
    return std::max(0.0f, std::min(1.0f, 1.0f - (1.0f - raw) * g_liveConfig.languageMul));
}

void CivilizationEngine::updateLanguages(std::vector<Entity>& entities, int day) {
    const float langMul = g_liveConfig.languageMul;
    if (langMul == 0.0f || !g_lexicon) return;   // director kill switch
    (void)entities;

    // Creolisation on contact. Peoples joined by a live trade road or a real
    // alliance are in each other's mouths every season: loanwords cross, and
    // the two tongues converge. This is the counter-force to drift, and why
    // isolation is what actually produces a language boundary.
    for (size_t i = 0; i < tribes.size(); ++i) {
        for (size_t j = i + 1; j < tribes.size(); ++j) {
            const Tribe& A = tribes[i];
            const Tribe& B = tribes[j];
            if (A.languageId < 0 || B.languageId < 0 || A.languageId == B.languageId) continue;

            bool channel = false;
            for (const auto& r : tradeRoutes)
                if (r.active && r.a == std::min(A.id, B.id) && r.b == std::max(A.id, B.id))
                { channel = true; break; }
            if (!channel) {
                auto st = A.stances.find(B.id);
                channel = (st != A.stances.end() && st->second == TS_ALLY);
            }
            if (!channel) continue;

            float before = g_lexicon->intelligibility(A.languageId, B.languageId);
            if (before > 0.92f) continue;                    // already one tongue
            // Slow: a generation of contact, not a season of it.
            g_lexicon->blend(A.languageId, B.languageId, 0.02f * langMul);
            g_lexicon->blend(B.languageId, A.languageId, 0.02f * langMul);
            float after = g_lexicon->intelligibility(A.languageId, B.languageId);
            if (before < 0.45f && after >= 0.45f) {
                ++totalCreolisations;
                logEvent(day, "The speech of the " + A.name + " and the " + B.name
                         + " has grown mutually intelligible", "language",
                         "kind=creolisation tribeA=\"" + A.name + "\" tribeB=\"" + B.name + "\""
                         + " langA=" + std::to_string(A.languageId)
                         + " langB=" + std::to_string(B.languageId)
                         + " intelligibility=" + std::to_string(after));
            }
        }
    }
}

// ── IV-P2: religion with doctrine, ritual and schism (Parallel-Earth Track IV) ─
// The report on this world's religions was damning: 168 faiths founded, 162
// extinct, and every creed behaviourally identical to every other. Faiths were
// names attached to a follower list. Three things are missing, and this is all
// three:
//
//   • DOCTRINE. The axes already generated at founding — militarism, tolerance,
//     asceticism, authority, afterlife focus — are copied onto each believer as
//     a creed the decision loop actually reads (mind::doctrineModifier). A
//     pacifist faith now stays its members' hands; an ascetic one curbs their
//     appetites; a hierarchical one buys obedience.
//   • RITE. Durkheim's collective effervescence: gathering to do the same thing
//     at the same time discharges what people are carrying — suppressed anger,
//     grief, the accumulated stress of getting by — and returns cohesion and a
//     sense of purpose. This is the *function* a religion performs, and the
//     reason members stay: it measurably does something for them.
//   • SCHISM. Faiths do not split at random. They split when a body of members
//     has drifted doctrinally from what the faith teaches, and the split runs
//     along the fault lines already in the congregation — the devout against
//     the worldly, the poor against the comfortable — behind a charismatic
//     founder who is remembered by name (I-P3).
// Kill switch: doctrineMul == 0 returns before any state is read or written.
void CivilizationEngine::updateDoctrine(std::vector<Entity>& entities, int day) {
    const float docMul = g_liveConfig.doctrineMul;
    if (docMul == 0.0f) return;   // director kill switch — bit-exact off

    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    // ── 1. Hand every believer their creed ──────────────────────────────────
    // Devotion is personal: a spiritually hungry member of a demanding faith
    // holds it far harder than a nominal one who was born into it.
    for (Entity& e : entities) {
        if (e.entityHealth <= 0.0f) { e.creed.held = false; continue; }
        Religion* r = (e.religionId >= 0) ? findReligion(e.religionId) : nullptr;
        if (!r) { e.creed = Creed{}; continue; }
        e.creed.held           = true;
        e.creed.militarism     = r->militarism;
        e.creed.tolerance      = r->tolerance;
        e.creed.asceticism     = r->asceticism;
        e.creed.authority      = r->authority;
        e.creed.afterlifeFocus = r->afterlifeFocus;
        float devTarget = clamp(20.0f + e.ValueSystem.spiritualNeed * 0.55f
                                      + r->institutionLevel * 4.0f
                                      + (r->spiritualDemand - 50.0f) * 0.10f,
                                0.0f, 100.0f);
        e.creed.devotion += (devTarget - e.creed.devotion) * 0.05f;
    }

    // ── 2. Rite: what the faith does for the people who keep it ─────────────
    for (Religion& r : religions) {
        int interval = std::max(8, 30 - r.institutionLevel * 3);   // grander faiths gather oftener
        if (day - r.lastRiteDay < interval) continue;

        std::vector<Entity*> congregation;
        for (int fid : r.followerIds) {
            Entity* f = entityById(entities, fid);
            if (f && f->entityHealth > 0.0f) congregation.push_back(f);
        }
        if ((int)congregation.size() < 3) continue;
        r.lastRiteDay = day;
        ++r.ritesHeld;
        ++totalRites;

        // Effervescence: the discharge scales with how many gather and how
        // grand the setting — a cathedral does more for you than a roadside
        // shrine, which is exactly why congregations build them.
        float power = (0.5f + std::min(1.0f, congregation.size() / 40.0f)
                            + r.institutionLevel * 0.12f) * docMul;
        for (Entity* f : congregation) {
            float zeal = clamp(f->creed.devotion / 100.0f, 0.0f, 1.0f);
            float take = power * (0.4f + 0.6f * zeal);
            f->emotionalState.suppressionDebt =
                std::max(0.0f, f->emotionalState.suppressionDebt - 6.0f * take);
            f->entityStress     = clamp(f->entityStress     - 5.0f * take, 0.0f, 100.0f);
            f->entityLoneliness = clamp(f->entityLoneliness - 6.0f * take, 0.0f, 100.0f);
            f->senseOfPurpose   = clamp(f->senseOfPurpose   + 2.5f * take, 0.0f, 100.0f);
            // Grief is what rites were invented for: the mourners are carried.
            for (auto& g : f->griefStates)
                g.intensity = std::max(0.0f, g.intensity - 0.06f * take);
            if (f->emotions.joy < 20.0f) ++g_mindStats.joyEpisodes;
            f->emotions.joy       = clamp(f->emotions.joy + 8.0f * take, 0.0f, 100.0f);
            f->emotions.gratitude = clamp(f->emotions.gratitude + 5.0f * take, 0.0f, 100.0f);
        }
        if (r.ritesHeld % 20 == 1)
            logEvent(day, "The " + r.name + " gather for their rite — " +
                     std::to_string((int)congregation.size()) + " keep it", "religion",
                     "kind=religious_rite religionId=" + std::to_string(r.id)
                     + " faith=\"" + r.name + "\""
                     + " congregation=" + std::to_string((int)congregation.size())
                     + " institution=\"" + r.institutionName() + "\""
                     + " rites=" + std::to_string(r.ritesHeld));
    }

    // ── 3. Schism along the fault lines that are actually there ─────────────
    // A faith needs a real congregation before it has anything to split, and a
    // cooling-off period so a fracture is an event rather than a churn.
    const size_t religionCountBefore = religions.size();
    for (size_t ri = 0; ri < religionCountBefore && religions.size() < 12; ++ri) {
        Religion& r = religions[ri];
        if (day - r.foundedOnDay < 200 || day - r.lastSchismDay < 300) continue;

        std::vector<Entity*> congregation;
        for (int fid : r.followerIds) {
            Entity* f = entityById(entities, fid);
            if (f && f->entityHealth > 0.0f) congregation.push_back(f);
        }
        if (congregation.size() < 12) continue;

        // The dissenters: members whose own spiritual temperature is far from
        // what the faith asks of them — the too-devout in a lax church and the
        // worldly in a demanding one both have a grievance, in opposite
        // directions. Inequality inside the congregation sharpens it.
        std::vector<Entity*> dissenters;
        float strain = 0.0f;
        for (Entity* f : congregation) {
            float gap = std::abs(f->ValueSystem.spiritualNeed - r.spiritualDemand);
            if (gap > 32.0f) { dissenters.push_back(f); strain += gap; }
        }
        if (dissenters.size() < 5) continue;
        float dissentShare = (float)dissenters.size() / (float)congregation.size();
        if (dissentShare < 0.30f) continue;
        strain /= (float)dissenters.size();

        // A schism needs someone to lead it: the most charismatic dissenter.
        Entity* leader = nullptr;
        for (Entity* f : dissenters)
            if (!leader || f->auctoritas + f->skills.get(SK_ORATORY)
                         > leader->auctoritas + leader->skills.get(SK_ORATORY))
                leader = f;
        if (!leader || leader->auctoritas < 35.0f) continue;

        float chance = 0.010f * dissentShare * (strain / 40.0f) * docMul;
        if (roll(rng) >= chance) continue;

        // The breakaway faith takes its founder's temperature, not its parent's.
        Religion s;
        s.id               = nextReligionId++;
        s.name             = g_lexicon ? ("Reformed " + r.name) : (r.name + " Reformed");
        s.founderEntityId  = leader->entityId;
        s.foundedOnDay     = day;
        s.parentReligionId = r.id;
        s.moralCode        = r.moralCode;
        s.ritual           = r.ritual;
        s.isPolytheistic   = r.isPolytheistic;
        s.spiritualDemand  = clamp(leader->ValueSystem.spiritualNeed, 5.0f, 95.0f);
        s.holyPrinciple    = "the true reading of " + r.holyPrinciple;
        // Doctrine drifts toward the dissenters' actual disposition.
        s.militarism     = clamp(r.militarism     + (leader->personality.agreeableness < 45.0f ? 18.0f : -18.0f), 0.0f, 100.0f);
        s.tolerance      = clamp(r.tolerance      + (leader->personality.openness > 55.0f ? 15.0f : -20.0f), 0.0f, 100.0f);
        s.asceticism     = clamp(r.asceticism     + (leader->ValueSystem.hedonism < 45.0f ? 20.0f : -15.0f), 0.0f, 100.0f);
        s.authority      = clamp(r.authority      + (leader->auctoritas > 60.0f ? 15.0f : -15.0f), 0.0f, 100.0f);
        s.afterlifeFocus = clamp(r.afterlifeFocus + (strain > 45.0f ? 15.0f : -10.0f), 0.0f, 100.0f);

        int taken = 0;
        for (Entity* f : dissenters) {
            // Not everyone who grumbles walks out; the devout dissenters do.
            if (f != leader && roll(rng) > 0.55f) continue;
            f->religionId = s.id;
            s.followerIds.push_back(f->entityId);
            r.followerIds.erase(std::remove(r.followerIds.begin(), r.followerIds.end(),
                                            f->entityId), r.followerIds.end());
            mind::recordLifeChapter(f, "schism", leader->entityId,
                                    "broke with " + r.name + " to follow " + s.name, day);
            ++taken;
        }
        if (taken < 3) continue;   // a fizzled reform movement, not a schism

        r.lastSchismDay = day;
        s.lastSchismDay = day;
        ++totalSchisms;
        logEvent(day, leader->name + " breaks with the " + r.name + ", and "
                 + std::to_string(taken) + " follow them into " + s.name, "religion",
                 "kind=religion_schism faith=\"" + s.name + "\""
                 + " religionId=" + std::to_string(s.id)
                 + " parentId=" + std::to_string(r.id)
                 + " founder=\"" + leader->name + "\""
                 + " founderId=" + std::to_string(leader->entityId)
                 + " followers=" + std::to_string(taken)
                 + " dissentShare=" + std::to_string(dissentShare)
                 + " strain=" + std::to_string((int)strain));
        religions.push_back(s);
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
        // population gets a reliable food supply early. II-P1 then scales the
        // rate by the research climate the inventor actually lives in — a
        // scholar in a literate city with a school and the method has ideas
        // several times as often as a herdsman with none of it, which is the
        // asymmetry that decides which societies cross into modernity.
        Tribe* tribe = findTribe(ent.tribeId);
        if (roll(rng) < inventorScore * 0.0012f * agricultureUrgency
                        * g_worldSeed.divergence.innovationLuck
                        * researchClimate(ent, tribe)) {
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

        // Prerequisites met by this entity — or, once a people can write them
        // down, by the record the entity can consult.
        //
        // II-P1/II-P2 (Henrich, recombination in the collective brain): before
        // writing, standing on the shoulders of giants means having personally
        // met the giant, so a chain like Oral Record → Writing → Philosophy →
        // Mathematics → Scientific Method → Steam Power has to fit inside one
        // skull and one lifetime — which is why it never did, and why the world
        // stalled in the bronze age no matter how big it grew. After writing,
        // the people's own knowledge and its school's archive are consultable,
        // so the chain can be assembled across generations. This is the single
        // change that makes the road to modernity walkable at all.
        bool prereqsMet = true;
        for (const std::string& prereq : tmpl.prereqs) {
            bool known = false;
            for (int tid : inventor->knownTechIds) {
                Innovation* inv = findInnovation(tid);
                if (inv && inv->name == prereq) { known = true; break; }
            }
            if (!known && g_liveConfig.knowledgeMul != 0.0f && tribe) {
                // The record: a literate people can be consulted on anything it
                // knows, by anyone, long after the knowers are dead.
                if (tribeIsLiterate(*tribe) && tribe->knownTechName.count(prereq))
                    known = true;
                // The living craft: a technique the people mastered through
                // deliberate research is being practised in the open, and an
                // apprentice can watch — no writing required.
                if (!known && tribeTreeHolds(*tribe, prereq)) known = true;
                if (!known && g_liveConfig.institutionMul != 0.0f) {
                    const auto* school =
                        institutions.find(tribe->id, environment::InstitutionType::EDUCATION);
                    if (school && school->archive.count(prereq)) known = true;
                }
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

    const std::vector<const InnovTemplate*>& pool = preferred.empty() ? candidates : preferred;
    const InnovTemplate* chosen =
        pool[std::uniform_int_distribution<int>(0, (int)pool.size()-1)(rng)];

    // II-P1: enquiry becomes DIRECTED. An idle tinkerer stumbles onto whatever
    // is lying about — which is the uniform draw above, and it is why the hard
    // chokepoints never landed: with forty-odd techniques reachable at any
    // moment, a one-in-forty chance of even ATTEMPTING the Scientific Method,
    // times a one-in-eleven chance of it working, meant the upper catalogue was
    // decorative. A people with a school, and above all one that has the method
    // itself, does not wait to trip over the next thing: it works on the hardest
    // problem it can currently state. That is what systematic enquiry MEANS, and
    // it is the difference between a society that accumulates curiosities and
    // one that crosses into modernity. The tinkerer is still there — half the
    // time even a learned people gets its next idea by accident.
    if (g_liveConfig.knowledgeMul != 0.0f && tribe) {
        const bool hasMethod = tribe->knownTechName.count("Scientific Method") > 0;
        const bool hasSchool = (g_liveConfig.institutionMul != 0.0f)
                            && institutions.find(tribe->id, environment::InstitutionType::EDUCATION);
        float directed = (hasMethod ? 0.55f : 0.0f) + (hasSchool ? 0.25f : 0.0f);
        directed = std::min(0.8f, directed * g_liveConfig.knowledgeMul);
        std::uniform_real_distribution<float> aim(0.0f, 1.0f);
        if (directed > 0.0f && aim(rng) < directed) {
            // The frontier is a handful of open problems, not one. Aiming
            // always at the single hardest thing reachable turned out to be its
            // own trap: a people would spend four centuries throwing itself at
            // Printing (complexity 80) while Philosophy (72) — the prerequisite
            // for the Scientific Method, and so for everything past the
            // Renaissance — sat one rung below, never once attempted. Research
            // programmes run several problems at a time. Draw from the hardest
            // few, and the chain gets walked.
            std::vector<const InnovTemplate*> frontier = pool;
            std::sort(frontier.begin(), frontier.end(),
                      [](const InnovTemplate* x, const InnovTemplate* y) {
                          if (x->complexity != y->complexity) return x->complexity > y->complexity;
                          return x->name < y->name;   // deterministic tie-break
                      });
            const int band = std::min((int)frontier.size(), 3);
            chosen = frontier[std::uniform_int_distribution<int>(0, band - 1)(rng)];
        }
    }

    // Complexity gates the pace of progress: a simple trick (~18) almost always
    // lands once attempted, but odds fall off with the square of complexity —
    // Metal Working (78) fizzles ~95% of attempts, Gunpowder (90) ~96%. Without
    // this the whole 53-tech catalog emptied within a few decades because a
    // prereq-met tech was discovered on the first eureka regardless of difficulty.
    float odds = 18.0f / std::max(18.0f, chosen->complexity);
    odds *= odds;
    // II-P1 (Henrich collective brain): invention is emergent from population
    // size × interconnection × literacy — a bigger, better-connected, literate
    // society runs more parallel eurekas and recombination, so complex tech that
    // stalls in a small band becomes reachable at scale. This is what lets a
    // civilisation push THROUGH the medieval ceiling instead of oscillating.
    if (g_liveConfig.knowledgeMul != 0.0f) {
        int pop = 0; for (const auto& t : tribes) pop += t.population();
        float scale   = std::log2(1.0f + pop / 40.0f);          // ~0 @40, ~2.3 @160, ~3.6 @460
        bool  literate = tribe && tribeIsLiterate(*tribe);
        float brain   = 1.0f + scale * (literate ? 0.6f : 0.35f) * g_liveConfig.knowledgeMul;
        // RECOMBINATION — the other half of cumulative culture, and the half
        // that makes the curve bend upward instead of flattening. New technique
        // is old technique recombined (Henrich): the more a people already
        // holds, the more pairs of ideas there are to strike against each
        // other, so the hundredth invention is easier to reach than the tenth
        // even though it is harder in itself. Without this the world stalled
        // exactly where a Malthusian population stops growing — invention rate
        // flat, catalogue tail unreachable, medieval forever. Population size
        // cannot keep climbing on one planet; what a people KNOWS can.
        if (tribe) {
            const int held = (int)tribe->knownTechName.size()
                           + (int)tribe->techTreeUnlocked.size();
            brain *= 1.0f + 0.030f * (float)held * g_liveConfig.knowledgeMul;
        }
        odds = std::min(0.95f, odds * brain);
    }
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
            // II-P1: literacy multiplies transmission — a written record travels
            // far faster and further than oral demonstration, so complex knowledge
            // accumulates enough knowers to stop being "fragile" (the mechanism
            // that made every collapse erase hard-won tech). Turns the yo-yo into
            // a ratchet from the supply side.
            if (g_liveConfig.knowledgeMul != 0.0f && tribe && tribeIsLiterate(*tribe))
                spreadProb *= (1.0f + 2.5f * g_liveConfig.knowledgeMul);
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

            // IV-P1: strangeness is concrete. Two peoples who bury their dead
            // differently, keep different taboos and dress unlike each other are
            // harder to trust than two who merely score differently on a values
            // axis — and the distance is measured on what they actually do.
            if (g_liveConfig.traitMul != 0.0f)
                valDiff += environment::CulturalTransmissionSystem::distance(
                               A.cultureTraits, B.cultureTraits) * 18.0f * g_liveConfig.traitMul;

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

            // II-P4: unavenged wrongs keep dragging relations down and decay only
            // slowly (feuds outlive their generation). A shared grievance sets a
            // ceiling: two peoples cannot fully warm to each other while blood
            // debt stands between them.
            // IV-P3: peoples who cannot talk warm to one another more slowly —
            // every misread gesture costs what a shared word would have saved.
            // Only the WARMING is damped; hostility needs no translation.
            if (g_liveConfig.languageMul != 0.0f) {
                float delta = rel - (B.relations.count(A.id) ? B.relations[A.id] : rel);
                (void)delta;
                float understanding = mutualIntelligibility(A, B);
                float prev = B.relations.count(A.id) ? B.relations[A.id] : 0.0f;
                if (rel > prev) rel = prev + (rel - prev) * (0.45f + 0.55f * understanding);
            }
            if (g_liveConfig.feudMul != 0.0f) {
                float gA = A.grievance.count(B.id) ? A.grievance[B.id] : 0.0f;
                float gB = B.grievance.count(A.id) ? B.grievance[A.id] : 0.0f;
                float feud = std::max(gA, gB);
                if (feud > 0.0f) {
                    rel -= feud * 0.04f * g_liveConfig.feudMul;   // persistent drag
                    rel = std::min(rel, 60.0f - feud * 0.5f);     // can't fully reconcile
                }
                if (gA > 0.0f) A.grievance[B.id] = gA * 0.999f;   // ~generational half-life
                if (gB > 0.0f) B.grievance[A.id] = gB * 0.999f;
            }

            rel = std::max(-100.0f, std::min(100.0f, rel));
            B.relations[A.id] = rel;

            // War line climbs toward 0 with militarism. Plan 2.1's floor (-62..-48)
            // overcorrected the endemic ~860-war run to nearly zero wars: with
            // similar cultures warming at +1.0/tick, relations could never grind
            // that low. Raised so warlike pairs tip at ~-34 and even peaceful
            // ones at -52, while war-weary tribes (high exhaustion) still hold
            // back for a generation after a fight.
            const bool warModel = (g_liveConfig.warMul != 0.0f);
            float warLine   = -52.0f + aggression * 18.0f
                              - std::max(A.warExhaustion, B.warExhaustion)
                                * (warModel ? 0.35f : 0.15f);
            float rivalLine = -15.0f;

            TribeStance prev = A.stances.count(B.id) ? A.stances[B.id] : TS_NEUTRAL;

            TribeStance stance;
            if      (rel >  55.0f)    stance = TS_ALLY;
            else if (rel > rivalLine) stance = TS_NEUTRAL;
            else if (rel > warLine)   stance = TS_RIVAL;
            else                      stance = TS_AT_WAR;

            // II-P4: war has inertia at both ends. As a bare threshold on a
            // drifting relations scalar, a pair sitting near the line declared
            // war, made peace and declared again every few days — 147 wars in
            // 850 days between twenty-odd peoples, which is what drove the
            // war-death share to half of all deaths without any single war ever
            // meaning much. Real states cross into war deliberately (a decisive
            // breach, not a bad week) and out of it slowly (peace has to be
            // worth more than the grudge). The band below is that hysteresis:
            // fewer wars, each of them longer and consequential.
            if (warModel) {
                const bool wasWar = (prev == TS_AT_WAR);
                // Tribute is what a people pays INSTEAD of fighting. An
                // overlord and its vassal sitting at bad relations was tipping
                // into open war on the ordinary threshold, over and over — 522
                // of one run's 646 declared wars were an overlord and a client
                // who were, on paper, already at peace on terms. A vassal that
                // will actually fight its master is in revolt, and revolt has
                // its own machinery (`rebelAgainstOverlord`); short of that the
                // bond holds, and it takes a far deeper breach to break it.
                const bool bound = (A.overlordTribeId == B.id || B.overlordTribeId == A.id);
                const float entryMargin = bound ? 45.0f : 16.0f;
                if (!wasWar && stance == TS_AT_WAR && rel > warLine - entryMargin)
                    stance = TS_RIVAL;                 // simmering, not yet blood
                if (wasWar && stance != TS_AT_WAR && rel < warLine + 14.0f)
                    stance = TS_AT_WAR;                // the fighting outlasts the mood
            }
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

                    // II-P4: an old blood debt is its own casus belli — a feud
                    // reignites long after the original wound, ahead of lesser causes.
                    float feudAB = std::max(A.grievance.count(B.id) ? A.grievance[B.id] : 0.0f,
                                            B.grievance.count(A.id) ? B.grievance[A.id] : 0.0f);
                    bool vendetta = (g_liveConfig.feudMul != 0.0f && feudAB > 45.0f);

                    WarReason reason;
                    if      (holyWar)                    reason = WAR_ETHNIC;
                    else if (vendetta)                   reason = WAR_ETHNIC;   // revenge / old feud
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
                    // §8: war has to be plural in cause, not one grievance wearing
                    // five names — so the report counts declarations by cause.
                    if ((int)reason >= 0 && (int)reason < 5) ++warsByReason[(int)reason];
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

    // II-P1: the world has TWO tech systems — the emergent catalogue above, and
    // the deliberate research tree tribes climb in `updateTechTree` — and the
    // era ladder used to read only the first. A people that had researched
    // Writing, Masonry, Iron Working and Fortification the hard way was still
    // filed as stone-age, and the techniques it held counted for nothing
    // towards an era. Every other measure in the sim (the knowledge ratchet
    // sample, literacy, the archive) already reads both; so does the ladder
    // now. `techCount` is the union, and a chokepoint reached by either road
    // counts as reached — walls of dressed stone are walls however you learned
    // to lay them.
    int techCount = (int)innovations.size();
    if (g_liveConfig.knowledgeMul != 0.0f) {
        std::set<int> unlocked;
        for (const Tribe& t : tribes)
            unlocked.insert(t.techTreeUnlocked.begin(), t.techTreeUnlocked.end());
        techCount += (int)unlocked.size();
        auto treeHas = [&](const char* name) {
            for (const TechNode& n : TechTreeSystem::tree())
                if (n.name == name) return unlocked.count(n.id) > 0;
            return false;
        };
        if (treeHas("Agriculture") || treeHas("Irrigation")) hasAgriculture = true;
        if (treeHas("Bronze Working"))                       hasMetal       = true;
        if (treeHas("Fortification"))                        hasFort        = true;
        if (treeHas("Iron Working"))                         hasIronSmelt   = true;
        if (treeHas("Writing"))                              hasWriting     = true;
    }
    innCount = techCount;

    CivilizationEra prevEra = era;
    int year = getCurrentYear();

    // The last two rungs are gated on more than a tech count, because crossing
    // into modernity is not something a people stumbles into: the plan asks for
    // literacy, towns and standing institutions behind it (§5 II-P1). A
    // civilisation with the method but no school to teach it, or the engine but
    // no administration to organise the work, is an inventor's curiosity — not
    // an industrial society. These are what "achievable but earned" means, and
    // what makes crossing over *stick* rather than flicker.
    bool hasTown = false, hasSchool = false, hasBureau = false;
    int  largestPeople = 0;
    for (const Tribe& t : tribes) {
        if (t.settlementTier >= 2) hasTown = true;
        largestPeople = std::max(largestPeople, t.population());
        if (g_liveConfig.institutionMul != 0.0f) {
            if (institutions.find(t.id, environment::InstitutionType::EDUCATION))  hasSchool = true;
            if (institutions.find(t.id, environment::InstitutionType::GOVERNMENT)) hasBureau = true;
        }
    }
    // With the feature off, the old ladder is the ladder — same counts read off
    // the same single tech system, so knowledgeMul=0 reproduces the pre-II-P1
    // era history exactly. With it on the rungs sit higher, because the count
    // they are read against now includes the research tree as well.
    const bool onKS = (g_liveConfig.knowledgeMul == 0.0f);
    auto gate = [&](int oldN, int newN) { return onKS ? oldN : newN; };
    const bool modernGates = onKS || (hasTown && hasSchool && largestPeople >= 12);

    if      (innCount >= gate(44, 50) && hasSteam && modernGates
             && (hasBureau || g_liveConfig.institutionMul == 0.0f)) era = ERA_MODERN;
    else if (innCount >= gate(38, 44) && hasScientific && modernGates) era = ERA_EARLY_MODERN;
    else if (innCount >= gate(32, 37) && hasGunpowder)      era = ERA_RENNAISSANCE;
    else if (innCount >= gate(26, 31) && hasFort && hasWriting) era = ERA_MEDIEVAL;
    else if (innCount >= gate(20, 24) && hasIronSmelt)      era = ERA_CLASSICAL;
    else if (innCount >= gate(14, 17) && hasIronSmelt)      era = ERA_IRON_AGE;
    else if (innCount >= gate( 9, 11) && hasMetal)          era = ERA_BRONZE_AGE;
    else if (innCount >= gate( 5,  7) && (hasAgriculture || hasReligion) && tribeCount >= 2)
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
    // ── III-P3: a labour market rather than a promotion ladder ──────────────
    // What was here promoted whoever ranked highest and handed them a trade
    // picked from their loudest personality trait: rank decided who worked, and
    // nothing decided what the work was FOR. A labour market has three parts
    // this pass now has — demand (what this place actually needs), matching
    // (who can do it, by skill rather than by standing), and how people hear of
    // the opening at all, which is famously not through their closest friends
    // (Granovetter: weak ties carry the opportunities strong ties already know
    // about). Guilds (II-P2) then hold a trade in place across a bad year.
    // Kill switch: laborMul == 0 leaves every line below untouched.
    const float laborMul = g_liveConfig.laborMul;
    const bool  market   = (laborMul != 0.0f);
    // The trades, and the skill each is actually judged on.
    static const char*   kRoles[6]      = { "craftsman", "scholar", "trader",
                                            "warrior", "healer", "priest" };
    static const SkillId kRoleSkill[6]  = { SK_CRAFT, SK_LORE, SK_ORATORY,
                                            SK_FIGHT, SK_HEAL, SK_ORATORY };

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
            // III-P3: farming is a skill like any other, and a lifetime of it
            // shows — which is also what makes leaving the fields a real cost.
            if (market) e->skills.practice(SK_FARM, 0.35f * laborMul);
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

        // ── III-P3: what does this place need done? ─────────────────────────
        // Demand is read off the world, not assumed: dear goods pull people
        // into the workshops, a war pulls them into the ranks, sickness into
        // the healers, a town's crowds and courts into letters and law. A camp
        // needs almost none of it, which is why the specialist mix tracks
        // settlement size instead of personality.
        float demand[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        bool  hasGuild = false, hasSchool = false;
        if (market) {
            const int tier = tribe.settlementTier;
            bool atWar = false;
            for (const auto& p : tribe.stances) if (p.second == TS_AT_WAR) { atWar = true; break; }
            int sick = 0;
            for (Entity* e : members)
                if (e->entityDiseaseType != -1 || e->entityHealth < 45.0f) ++sick;
            int roads = 0;
            for (const TradeRoute& r : tradeRoutes)
                if (r.active && (r.a == tribe.id || r.b == tribe.id)) ++roads;
            hasGuild  = institutions.find(tribe.id, environment::InstitutionType::ECONOMY)   != nullptr;
            hasSchool = institutions.find(tribe.id, environment::InstitutionType::EDUCATION) != nullptr;

            const float pop = (float)members.size();
            demand[0] = 0.6f + tribe.priceGoods * 0.8f + 0.25f * (float)tier
                      + (hasGuild ? 0.5f : 0.0f);                       // craftsman
            demand[1] = 0.3f + tribe.innovation / 120.0f + 0.30f * (float)tier
                      + (hasSchool ? 0.6f : 0.0f);                      // scholar
            demand[2] = 0.3f + 0.35f * (float)roads + 0.25f * (float)tier
                      + (tribe.priceFood - 1.0f);                       // trader
            demand[3] = 0.4f + (atWar ? 1.6f : 0.0f) + tribe.militarism / 90.0f;   // warrior
            demand[4] = 0.3f + 2.0f * ((float)sick / std::max(1.0f, pop));         // healer
            demand[5] = 0.3f + tribe.spiritualism / 90.0f
                      + (tribe.dominantReligionId >= 0 ? 0.4f : 0.0f);   // priest
            for (int r = 0; r < 6; ++r) demand[r] = clamp(demand[r] * laborMul, 0.05f, 4.0f);
        }
        // How well this person would do a given trade, and whether they have
        // heard the work is going. Weak ties are the ones that carry news from
        // outside your own circle, so an acquaintance in the trade is worth
        // more here than a bosom friend already standing beside you.
        auto skillFit = [&](const Entity* e, int role) {
            float s = e->skills.get(kRoleSkill[role]) / 100.0f;
            switch (role) {
                case 0: s += e->personality.conscientiousness / 260.0f; break;
                case 1: s += e->personality.openness          / 260.0f; break;
                case 2: s += e->personality.extraversion      / 260.0f; break;
                case 3: s += (100.0f - e->personality.agreeableness) / 260.0f; break;
                case 4: s += e->personality.agreeableness     / 260.0f; break;
                case 5: s += e->ValueSystem.spiritualNeed     / 260.0f; break;
            }
            return s;
        };
        auto weakTieToRole = [&](const Entity* e, int role) {
            for (const auto& s : e->list_entityPointedSocial) {
                if (!s.pointedEntity || s.pointedEntity->entityHealth <= 0.0f) continue;
                if (s.social < 5.0f || s.social > 30.0f) continue;   // the weak band
                if (s.pointedEntity->isSpecialist
                    && s.pointedEntity->specialization == kRoles[role]) return true;
            }
            return false;
        };

        // 3. Promote toward target (highest dominance/talent first), or demote the
        //    surplus (lowest first — survival keeps the ablest provisioned).
        //    Favoritism (Society Plan 5): a corrupt ruler bumps their own kin to
        //    the front of the queue regardless of merit — nepotism the tribe
        //    notices as graft, and the ruling house banks as standing.
        Entity* dolLeader = entityById(entities, tribe.leaderId);
        int favoredFamily = (dolLeader && dolLeader->entityHealth > 0.0f
                             && dolLeader->integrity < 40.0f) ? dolLeader->familyId : -1;
        if (current < target) {
            // III-P3: the queue is no longer a rank order. Whoever can best do
            // something this place actually wants done goes first — with the
            // ruler's kin still jumping it, because nepotism does not care what
            // the market wants, and standing still counting for something,
            // because it always has.
            std::sort(members.begin(), members.end(), [&](Entity* a, Entity* b) {
                bool ka = favoredFamily >= 0 && a->familyId == favoredFamily;
                bool kb = favoredFamily >= 0 && b->familyId == favoredFamily;
                if (ka != kb) return ka;
                if (!market) return a->dominanceRank > b->dominanceRank;
                auto worth = [&](const Entity* e) {
                    float best = 0.0f;
                    for (int r = 0; r < 6; ++r) {
                        float v = skillFit(e, r) * demand[r];
                        if (weakTieToRole(e, r)) v += 0.20f * laborMul;
                        best = std::max(best, v);
                    }
                    return best + e->dominanceRank / 400.0f;
                };
                float wa = worth(a), wb = worth(b);
                if (wa != wb) return wa > wb;
                return a->entityId < b->entityId;   // stable, and deterministic
            });
            for (Entity* e : members) {
                if (current >= target) break;
                if (e->isSpecialist) continue;
                if (e->roleSinceDay >= 0 && day - e->roleSinceDay < 3) continue;
                if (favoredFamily >= 0 && e->familyId == favoredFamily && globalKinship) {
                    tribe.corruption = std::min(100.0f, tribe.corruption + 0.5f);
                    globalKinship->adjustReputation(favoredFamily, 1.0f);
                }
                e->isSpecialist = true;
                // III-P3: the trade is the one this person is best at among the
                // ones the place is short of — a match, not a personality quiz.
                bool viaWeakTie = false;
                if (market && (e->specialization.empty() || e->specialization == "farmer")) {
                    int   bestRole = 0;
                    float bestVal  = -1.0f;
                    for (int r = 0; r < 6; ++r) {
                        // A trade already crowded here is worth less to enter.
                        int held = 0;
                        for (Entity* o : members)
                            if (o->isSpecialist && o != e && o->specialization == kRoles[r]) ++held;
                        float glut = 1.0f / (1.0f + 0.6f * (float)held);
                        bool  tie  = weakTieToRole(e, r);
                        float v    = skillFit(e, r) * demand[r] * glut + (tie ? 0.20f * laborMul : 0.0f);
                        if (v > bestVal) { bestVal = v; bestRole = r; viaWeakTie = tie; }
                    }
                    e->specialization = kRoles[bestRole];
                    ++hiresTotal;
                    if (viaWeakTie) ++hiresViaWeakTie;
                    // A guild does not merely employ: it teaches. The best hand
                    // in the trade brings the newcomer along (II-P2).
                    if (hasGuild) {
                        Entity* master = nullptr;
                        for (Entity* o : members) {
                            if (o == e || !o->isSpecialist) continue;
                            if (o->specialization != kRoles[bestRole]) continue;
                            if (!master || o->skills.get(kRoleSkill[bestRole])
                                         > master->skills.get(kRoleSkill[bestRole])) master = o;
                        }
                        if (master) {
                            e->skills.learnFrom(master->skills, kRoleSkill[bestRole]);
                            ++apprenticeships;
                        }
                    }
                }
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
            // III-P3: who goes back to the fields first. Without a market that
            // is simply the lowest-ranked; with one it is the least skilled at
            // what they do — and a guild town sheds its guildsmen last, which is
            // the whole point of a guild (apprenticeship is a promise that the
            // trade survives a bad harvest).
            std::sort(members.begin(), members.end(), [&](Entity* a, Entity* b) {
                if (!market) return a->dominanceRank < b->dominanceRank;
                auto keep = [&](const Entity* e) {
                    float k = e->dominanceRank / 400.0f;
                    for (int r = 0; r < 6; ++r)
                        if (e->specialization == kRoles[r]) {
                            k += e->skills.get(kRoleSkill[r]) / 100.0f * (hasGuild ? 1.6f : 1.0f);
                            k += demand[r] * 0.25f;
                            break;
                        }
                    return k;
                };
                float ka = keep(a), kb = keep(b);
                if (ka != kb) return ka < kb;
                return a->entityId < b->entityId;
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
            // III-P3: a trade practised is a trade improved, and the wage
            // follows the mastery — which is what makes specialisation stick
            // rather than churn (and gives skill a route into inequality).
            if (market)
                for (int r = 0; r < 6; ++r)
                    if (s == kRoles[r]) {
                        e->skills.practice(kRoleSkill[r], 0.6f * laborMul);
                        e->salary.earnMoney(e->skills.get(kRoleSkill[r]) * 0.03f * laborMul);
                        break;
                    }
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

        // ── III-P3: what the fertility decision will read tonight ───────────
        // Two summaries of this people, refreshed daily: how well off an
        // ordinary household here is, and how many of its children are living.
        // Both are inputs to the transition, and both have to be measured from
        // the people rather than assumed, because "wealthy" and "safe" mean
        // different things in a fishing camp and in a walled city.
        if (market) {
            float wealth = 0.0f;
            int   kids = 0, kidsWell = 0;
            for (Entity* e : members) {
                wealth += std::max(0.0f, e->salary.token);
                if (e->entityAge < 12.0f) {
                    ++kids;
                    if (e->entityHealth > 55.0f && e->entityDiseaseType == -1) ++kidsWell;
                }
            }
            tribe.meanWealth = wealth / (float)members.size();
            const float observed = kids > 0 ? (float)kidsWell / (float)kids : tribe.childSurvival;
            // Smoothed: parents respond to how children have fared over years,
            // not to how they fared this morning.
            tribe.childSurvival = clamp(tribe.childSurvival + (observed - tribe.childSurvival) * 0.05f,
                                        0.0f, 1.0f);

            // §8-style exposure accounting for the report: population-days, so
            // birth RATES can be compared between a city and a camp.
            const int tier = std::max(0, std::min(4, tribe.settlementTier));
            specialistShareByTier[tier] +=
                (double)tribe.specialistCount / (double)members.size();
            ++specialistSamplesByTier[tier];
            // Exposure is counted in FERTILE ADULTS, not in people: a town full
            // of children and elders is not a town with a high birth rate, and
            // comparing raw headcounts would say it was.
            for (Entity* e : members) {
                if (e->entityAge < 16.0f || e->entityAge > 55.0f) continue;   // the fertile years
                popDaysByTier[tier] += 1.0;
                if (e->salary.token >= tribe.meanWealth) popDaysRich += 1.0;
                else                                     popDaysPoor += 1.0;
            }
        }
    }
}

// ── III-P3: the demographic transition ───────────────────────────────────────
// The single best-attested regularity in modern demography: as households get
// richer, move into towns, educate their women and stop burying their children,
// they stop having six. Every one of those four is a different reason, and this
// function is all four at once —
//
//   • Wealth, because a child in a poor farming household is a pair of hands
//     and in a prosperous urban one is twenty years of investment.
//   • Urbanisation, because a town charges rent for space a farm gives free,
//     and because children there work later and cost longer.
//   • Women's standing, the strongest single predictor there is: literacy, a
//     trade of her own, and standing to refuse are what turn "how many arrive"
//     into a decision.
//   • Child survival, which is the one people find least intuitive and which
//     runs the whole thing: parents who expect to lose children have more of
//     them, and stop within a generation of no longer expecting it.
//
// Returns EXACTLY 1.0f when the feature is off, so the conception sites can
// multiply unconditionally and still be bit-identical with laborMul == 0.
float CivilizationEngine::fertilityModifier(const Entity& a, const Entity& b) const {
    if (g_liveConfig.laborMul == 0.0f) return 1.0f;
    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };

    const Tribe* home = nullptr;
    for (const Tribe& t : tribes)
        if (t.id == a.tribeId || t.id == b.tribeId) { home = &t; break; }
    if (!home) return 1.0f;

    // 1. Wealth, relative to what wealth means here — measured against what an
    //    ordinary household in this same people holds, so what is read is the
    //    couple's standing among its neighbours and not the era it lives in.
    const float household = std::max(0.0f, a.salary.token) + std::max(0.0f, b.salary.token);
    const float typical   = std::max(1.0f, home->meanWealth * 2.0f);

    // 2. Urbanisation.
    const float urban = clamp((float)home->settlementTier / 4.0f, 0.0f, 1.0f);

    // 3. The woman's standing — hers specifically, not the couple's.
    const Entity& she = (a.entitySex == 'F') ? a : b;
    const float status = clamp(she.skills.get(SK_LORE) / 100.0f * 0.45f
                             + (she.isSpecialist ? 0.30f : 0.0f)
                             + std::max(0.0f, she.Esteem - 50.0f) / 100.0f * 0.25f,
                               0.0f, 1.0f);

    // 4. Children living. Only the part ABOVE a coin-flip counts: a people
    //    still losing half its children has no reason to change anything.
    const float survival = clamp((home->childSurvival - 0.5f) / 0.5f, 0.0f, 1.0f);

    // The transition is TWO different things, and collapsing them into one
    // slope is what made this hard to get right. Tuning a single downward ramp
    // put the world on a knife edge: steep enough to show the rich-under-poor
    // gradient (the thing III-P3 asserts) and a modernising society sterilised
    // itself out of existence within a few centuries; shallow enough to
    // survive and the gradient sank under the plain confound that prosperous
    // couples are healthier, better partnered and more committed, so the rich
    // out-bred the poor and the measurement came back inverted. Both halves
    // were failing at once because they are not the same quantity.
    //
    //   • WITHIN a people, wealth is a REDISTRIBUTION around the local norm,
    //     not a subtraction from it. A household with twice its neighbours'
    //     means has markedly fewer children than one with half; the average
    //     household is unaffected by definition. Mean-preserving, so it can be
    //     as steep as the real gradient is without costing the world a soul.
    //     Her own standing belongs here too, for the same reason and one more:
    //     the report's rich/poor split is drawn on these very households, and
    //     prosperous women are also the literate, the tradeswomen and the
    //     esteemed — fold that effect into a people-wide constant and the
    //     gradient hands itself straight back to the confound.
    //   • BETWEEN peoples, town life and children who live lower fertility for
    //     everyone together — the part that is a genuine fall in the level.
    //     This one is shallow, because a real post-transition society settles
    //     near replacement and keeps going.
    const float relWealth  = clamp(household / typical, 0.0f, 2.0f);  // 1 = typical here
    const float wealthTerm = 1.40f - 0.40f * relWealth;               // 1.2 poor … 0.6 rich
    const float statusTerm = 1.14f - 0.28f * status;                  // 1.14 … 0.86
    const float modernTerm = 1.0f - 0.18f * clamp(urban * 0.55f + survival * 0.45f, 0.0f, 1.0f);
    const float km = g_liveConfig.laborMul;
    float modifier = clamp((1.0f + (wealthTerm - 1.0f) * km)
                         * (1.0f + (statusTerm - 1.0f) * km)
                         * (1.0f + (modernTerm - 1.0f) * km),
                           0.50f, 1.30f);

    // THE MALTHUSIAN CHECK, which is the older half of demography and the one
    // that keeps a world alive long enough to modernise. Prosperity suppressing
    // fertility is a modern story; for the whole span before it, births track
    // the harvest — hungry people conceive less and carry fewer to term, and a
    // people with full granaries fills the land. Without this the sim had only
    // the modern half and no negative feedback at all, so a world was either
    // dying slowly (too deep a transition) or booming into an overshoot that
    // famine, then war, then collapse resolved by killing everyone: tuned to
    // land between the two, it fell off one side or the other. A brake that
    // reads the granary self-corrects instead, and the population settles near
    // what the land will bear rather than oscillating through it.
    const int mouths = std::max(1, home->population());
    const float foodPerHead = home->granary / (float)mouths;
    const float scarcity    = clamp(1.0f - foodPerHead / 3.0f, 0.0f, 1.0f);
    modifier *= (1.0f - 0.55f * scarcity * g_liveConfig.laborMul);
    return modifier;
}

// III-P3: file a birth under the conditions that produced it, so the report can
// compare birth RATES between rich and poor, town and country.
void CivilizationEngine::recordBirthDemography(const Entity& a, const Entity& b) {
    if (g_liveConfig.laborMul == 0.0f) return;
    const Tribe* home = nullptr;
    for (const Tribe& t : tribes)
        if (t.id == a.tribeId || t.id == b.tribeId) { home = &t; break; }
    if (!home) return;
    // Filed per PARENT, because the exposure it will be divided by is counted
    // per fertile adult. Mixing a couple-level numerator with a per-adult
    // denominator quietly doubles the rate of whichever bracket the richer
    // partner belongs to — which is exactly how a wealth gradient can come out
    // backwards without any mechanism being wrong.
    const int tier = std::max(0, std::min(4, home->settlementTier));
    for (const Entity* p : { &a, &b }) {
        ++birthsByTier[tier];
        if (p->salary.token >= home->meanWealth) ++birthsRich; else ++birthsPoor;
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
            // Making things requires leisure and a patron, not serenity — and
            // this world is a stressful place to live (a third of it dies
            // pinned near the ceiling). At a bar of 50 the tribes essentially
            // never had a maker free at the moment inspiration was rolled, so
            // no great work was ever produced in any run; art was a mechanism
            // on paper only. Wretchedness still silences a maker, but ordinary
            // hard living does not.
            const float artBar = (g_liveConfig.giftMul != 0.0f) ? 75.0f : 50.0f;
            if (e->isSpecialist && (e->specialization == "scholar" || e->specialization == "craftsman")
                && e->entityStress < artBar)
                ++artists;
        }
        if (pop == 0) { t.cultureScore *= 0.98f; continue; }

        // Inspiration flows from artists, luxury (patronage) and a living faith.
        float inspiration = artists * 0.5f + t.luxuryStock * 0.02f
                            + (t.dominantReligionId >= 0 ? 0.3f : 0.0f);
        t.cultureScore = clamp(t.cultureScore + inspiration * 0.1f - 0.05f, 0.0f, 100.0f);

        // A great work: rare, needs a vibrant culture and at least one artist.
        // IV-P4: the old bar was cultureScore > 40, but culture accrues at
        // roughly artists x 0.05 per civ-day — about +12 over a 1200-tick run —
        // so the threshold was unreachable and NO great work was ever made in
        // any run. Art is the part of IV-P4 that outlives everyone, so the gate
        // is now what a people can actually reach: a living culture (>18) with
        // hands free to make something. Rarity is preserved in the roll.
        const float workBar = (g_liveConfig.giftMul != 0.0f) ? 18.0f : 40.0f;
        if (artists > 0 && t.cultureScore > workBar
            && roll(rng) < 0.01f * (t.cultureScore / 100.0f)) {
            ++t.culturalAchievements;
            t.cultureScore = clamp(t.cultureScore + 5.0f, 0.0f, 100.0f);

            // I-P3: a great work is made by a HAND, not by a statistic. Find the
            // artist who made it — the most accomplished, least burdened of the
            // tribe's makers — and put their name on it for good. This is what
            // lets the Chronicle say "the Hymn of Kael" instead of "+5 culture".
            Entity* maker = nullptr;
            if (g_liveConfig.legacyMul != 0.0f) {
                for (int mid : t.memberIds) {
                    Entity* e = entityById(entities, mid);
                    if (!e || e->entityHealth <= 0.0f) continue;
                    if (!e->isSpecialist) continue;
                    if (e->specialization != "scholar" && e->specialization != "craftsman") continue;
                    if (!maker || e->skills.get(SK_CRAFT) + e->skills.get(SK_LORE)
                                > maker->skills.get(SK_CRAFT) + maker->skills.get(SK_LORE))
                        maker = e;
                }
            }
            std::string workName = "a great work of the " + t.name;
            if (maker) {
                static const char* kForms[5] = { "Hymn", "Chronicle", "Carving", "Saga", "Monument" };
                workName = std::string("the ") + kForms[maker->entityId % 5] + " of " + maker->name;
                GreatWork w;
                w.name = workName; w.founderId = maker->entityId;
                w.founderName = maker->name; w.tribeId = t.id; w.day = day;
                greatWorks.push_back(w);
                if (greatWorks.size() > 300) greatWorks.erase(greatWorks.begin());

                // ── IV-P4: a great work MARKS A PLACE ───────────────────────
                // Art is how a culture leaves something standing. The work
                // becomes a memorial on the ground where it was made (I-P3),
                // so people who live beside it feel it long after the maker is
                // dust — and it carries the traits of the culture that made it
                // (IV-P1), which is how a practice outlives the practitioners.
                if (g_liveConfig.giftMul != 0.0f) {
                    Memorial m;
                    m.x = maker->posX; m.y = maker->posY;
                    m.personName = maker->name;
                    m.entityId   = maker->entityId;
                    m.day        = day;
                    m.weight     = 6.0f;      // a monument looms larger than a grave
                    m.placeName  = workName;
                    memorials.push_back(m);
                    ++totalMemorials;
                    if (memorials.size() > 200) memorials.erase(memorials.begin());
                    // The work fixes the people's practices in a form that can
                    // be seen and copied, so the culture holds them harder.
                    t.knownTraits |= t.cultureTraits;
                }
                // Making something that outlasts you is the clearest purpose there is.
                maker->senseOfPurpose = clamp(maker->senseOfPurpose + 6.0f, 0.0f, 100.0f);
                mind::recordLifeChapter(maker, "made_great_work", -1,
                                        "made " + workName, day);
            }
            logEvent(day, maker ? (maker->name + " of the " + t.name + " made " + workName)
                                : ("The " + t.name + " produced a great work of culture"),
                     "culture",
                     "kind=cultural_achievement tribe=\"" + t.name + "\""
                     + " tribeId=" + std::to_string(t.id)
                     + " work=\"" + workName + "\""
                     + " founderId=" + std::to_string(maker ? maker->entityId : -1)
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

// ── A3/D4 (AI upgrade): festivals & horizontal culture transmission ──────────
// A people with food in the granary and festive spirit periodically feasts:
// suppressed anger discharges, joy and cohesion rise, roots deepen, and the
// Chronicle gains rhythm. Between feasts, allied tribes' cultures slowly
// converge (horizontal transmission) while wars keep enemies' apart — the
// diplomacy pass already turns the resulting cultural distance into friction.
void CivilizationEngine::updateFestivals(std::vector<Entity>& entities, int day) {
    if (g_liveConfig.cultureMul <= 0.0f) return;   // director kill switch

    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };

    for (Tribe& t : tribes) {
        if (t.memberIds.size() < 3) continue;
        // Festive peoples feast often (every ~60 days at festivity 100,
        // ~130 at festivity 0); the live knob stretches or squeezes cadence.
        int interval = (int)((130.0f - t.festivity * 0.7f) / std::max(0.25f, g_liveConfig.cultureMul));
        if (day - t.lastFestivalDay < interval) continue;
        // No feast during famine: the granary must carry a little surplus.
        if (t.granary < (float)t.memberIds.size() * 0.2f) continue;
        t.lastFestivalDay = day;
        t.granary = std::max(0.0f, t.granary - (float)t.memberIds.size() * 0.15f);

        int celebrants = 0;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (!e || e->entityHealth <= 0.0f) continue;
            ++celebrants;
            // The feast is where a people breathes out.
            const float stressBefore = e->entityStress;
            e->emotionalState.suppressionDebt *= 0.5f;
            if (e->emotions.joy < 20.0f) ++g_mindStats.joyEpisodes;
            e->emotions.joy       = clamp(e->emotions.joy + 18.0f, 0.0f, 100.0f);
            e->emotions.gratitude = clamp(e->emotions.gratitude + 6.0f, 0.0f, 100.0f);
            e->entityStress       = clamp(e->entityStress - 8.0f, 0.0f, 100.0f);
            e->entityHapiness     = clamp(e->entityHapiness + 5.0f, 0.0f, 100.0f);
            e->entityLoneliness   = clamp(e->entityLoneliness - 10.0f, 0.0f, 100.0f);
            e->homeAttachment     = clamp(e->homeAttachment + 3.0f, 0.0f, 100.0f);

            // ── IV-P4: collective effervescence (Durkheim) ──────────────────
            // The crowd is the point. A feast in a full settlement does more
            // for the people in it than the same feast among a handful,
            // because what discharges tension is the synchrony of everyone
            // doing the same thing at once. This is the population-scale
            // relief valve for the chronic-stress artifact (F5) — and it is
            // where grief is actually carried, which is what funerary and
            // seasonal rites are FOR.
            if (g_liveConfig.giftMul != 0.0f) {
                const float gm = g_liveConfig.giftMul;
                float crowd = std::min(1.6f, 0.4f + celebrants / 25.0f + t.settlementTier * 0.15f);
                e->entityStress = clamp(e->entityStress - 7.0f * crowd * gm, 0.0f, 100.0f);
                e->emotionalState.suppressionDebt =
                    std::max(0.0f, e->emotionalState.suppressionDebt - 8.0f * crowd * gm);
                for (auto& g : e->griefStates)
                    g.intensity = std::max(0.0f, g.intensity - 0.08f * crowd * gm);
                // Belonging to something larger is meaning, not just relief.
                e->senseOfPurpose = clamp(e->senseOfPurpose + 1.6f * crowd * gm, 0.0f, 100.0f);
            }
            g_mindStats.festivalStressDischarged += (stressBefore - e->entityStress);
        }
        if (celebrants == 0) continue;
        t.govSatisfaction = clamp(t.govSatisfaction + 2.5f, 0.0f, 100.0f);
        t.cultureScore    = clamp(t.cultureScore + 1.5f, 0.0f, 100.0f);

        // ── IV-P4: potlatch — surplus burned for standing (Mauss) ───────────
        // The feast is also a contest. Whoever hosts it gives away what they
        // have and gains, in exchange, the only thing that cannot be eaten:
        // standing. That is the structural point — a society with a potlatch
        // has a way to convert wealth into rank WITHOUT killing anyone, so the
        // ambitious have somewhere to put their ambition other than the
        // violence pipeline.
        if (g_liveConfig.giftMul != 0.0f && t.luxuryStock > 4.0f) {
            const float gm = g_liveConfig.giftMul;
            Entity* host = nullptr;
            for (int mid : t.memberIds) {
                Entity* e = entityById(entities, mid);
                if (!e || e->entityHealth <= 0.0f) continue;
                if (!host || e->salary.token > host->salary.token) host = e;
            }
            // A big man is big RELATIVE to his neighbours. The absolute bar of
            // 120 tokens described nobody in most worlds — no potlatch was ever
            // held in any run — and it also mis-states the anthropology: the
            // potlatch is a contest of standing among the people you live with,
            // not a wealth bracket. Whoever is meaningfully richer than the
            // people around them (twice the local mean, and with something worth
            // giving) is who hosts.
            const float potlatchBar = std::max(30.0f, 2.0f * t.meanWealth);
            if (host && host->salary.token > potlatchBar) {
                float given = std::min(host->salary.token * 0.25f, 600.0f);
                host->salary.spendMoney(given);
                // Redistributed across everyone who came — this is what makes a
                // potlatch levelling as well as status-making.
                float each = given / (float)celebrants;
                for (int mid : t.memberIds) {
                    Entity* e = entityById(entities, mid);
                    if (e && e->entityHealth > 0.0f) e->salary.earnMoney(each);
                }
                float renown = std::min(9.0f, given / 70.0f) * gm;
                host->auctoritas = std::min(100.0f, host->auctoritas + renown);
                host->Esteem     = std::min(100.0f, host->Esteem + renown * 0.7f);
                host->senseOfPurpose = clamp(host->senseOfPurpose + 3.0f * gm, 0.0f, 100.0f);
                t.luxuryStock *= 0.6f;
                ++totalPotlatches;
                mind::recordLifeChapter(host, "hosted_feast", -1,
                                        "gave away a fortune to feast the " + t.name, day);
                logEvent(day, host->name + " feasts the whole " + t.name
                         + ", giving away a fortune — and is honoured for it", "culture",
                         "kind=potlatch tribe=\"" + t.name + "\""
                         + " host=\"" + host->name + "\""
                         + " hostId=" + std::to_string(host->entityId)
                         + " given=" + std::to_string((int)given)
                         + " celebrants=" + std::to_string(celebrants)
                         + " auctoritas=" + std::to_string((int)host->auctoritas));
            }
        }
        ++g_mindStats.festivalsHeld;
        logEvent(day, "The " + t.name + " hold a great feast — old grudges soften, "
                 "bonds renew and the fires burn late", "culture",
                 "kind=festival tribe=\"" + t.name + "\""
                 + " tribeId=" + std::to_string(t.id)
                 + " celebrants=" + std::to_string(celebrants)
                 + " festivity=" + std::to_string((int)t.festivity));
    }

    // Horizontal transmission: friendly neighbors trade songs, gods and habits.
    // Convergence is slow (0.4%/day at full warmth) and only under contact —
    // hostile or distant tribes keep their own ways and drift apart naturally.
    for (size_t i = 0; i < tribes.size(); ++i) {
        for (size_t j = i + 1; j < tribes.size(); ++j) {
            Tribe& A = tribes[i];
            Tribe& B = tribes[j];
            auto ra = A.relations.find(B.id);
            if (ra == A.relations.end() || ra->second < 40.0f) continue;
            float w = 0.004f * (ra->second / 100.0f) * g_liveConfig.cultureMul;
            auto converge = [&](float& a, float& b) {
                float mid = (a + b) * 0.5f;
                a += (mid - a) * w;
                b += (mid - b) * w;
            };
            converge(A.militarism,   B.militarism);
            converge(A.spiritualism, B.spiritualism);
            converge(A.collectivism, B.collectivism);
            converge(A.innovation,   B.innovation);
            converge(A.festivity,    B.festivity);
        }
    }
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
            // IV-P3: a technique has to be explained before it can be copied.
            // Across an opaque tongue the explaining mostly fails, so the same
            // contact yields far less transfer — the language barrier slowing
            // the collective brain. Tallied both ways so the report can compare
            // the RATE inside a language against the rate across one.
            const float understanding = mutualIntelligibility(A, B);
            const bool  sameTongue    = (understanding >= 0.45f);
            if (sameTongue) ++diffusionOpportunitySame; else ++diffusionOpportunityCross;
            chance *= (0.25f + 0.75f * understanding);
            if (roll(rng) >= chance) continue;
            if (sameTongue) ++diffusionSameTongue; else ++diffusionCrossTongue;

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

// ── III-P1: settlements & cities (Parallel-Earth plan, Track III) ─────────────
// A tribe's home stops being a bare coordinate and hardens into a PLACE with a
// size: camp → village → town → city → great city. A tier has to be earned on
// three axes at once — bodies, food surplus, and built fabric — plus, for the
// upper tiers, the era's techniques (masonry, drainage, record-keeping). Because
// peoples clear those bars at different times and lose them again in famine and
// war, sizes spread into a heavy-tailed rank-size hierarchy instead of everyone
// converging on one settlement size (Zipf).
//
// Size then pays back in both directions, exactly as it does in real urban
// history:
//   • agglomeration — density puts more minds, hands and goods within reach of
//     one another, so research and culture rise with tier (the collective brain
//     of II-P1, made spatial), and a bigger place can administer more people
//     before it fissions;
//   • crowding — the same density concentrates filth, contagion and strangers,
//     so a sanitation-poor town pays in epidemics, stress and urban anomie.
// Kill switch: g_liveConfig.cityMul == 0 returns before any read or RNG draw, so
// the determinism pair reproduces the pre-settlement world bit-for-bit.
void CivilizationEngine::updateSettlements(std::vector<Entity>& entities, int day) {
    const float cityMul = g_liveConfig.cityMul;
    if (cityMul == 0.0f) return;   // director kill switch — bit-exact no-op

    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    static const char* TIER_NAME[5] = { "camp", "village", "town", "city", "great city" };

    int tierCount[5] = { 0, 0, 0, 0, 0 };

    for (Tribe& t : tribes) {
        int pop = 0;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f) ++pop;
        }
        if (pop == 0) { t.settlementTier = 0; continue; }

        // Surplus per head is what lets people stop farming and pile up in one
        // spot; worked stone and timber per head is the built fabric that keeps
        // them there through a winter.
        const float surplus = t.granary  / (float)pop;
        const float fabric  = t.matStock / (float)pop;

        int want = 0;
        if (pop >=  6 && surplus >= 0.6f)                                        want = 1;
        if (pop >= 12 && surplus >= 1.0f && fabric >= 0.4f)                       want = 2;
        if (pop >= 20 && surplus >= 1.4f && fabric >= 0.9f &&
            era >= ERA_EARLY_AGRICULTURE)                                         want = 3;
        if (pop >= 30 && surplus >= 1.8f && fabric >= 1.5f &&
            era >= ERA_BRONZE_AGE)                                                want = 4;

        // Places grow and shrink one step at a time, and shrink reluctantly:
        // stone and streets outlive the harvest that paid for them, so a town
        // stays a town through a lean year before it empties.
        const int old = t.settlementTier;
        if (want > old)      t.settlementTier = old + 1;
        else if (want < old && roll(rng) < 0.25f) t.settlementTier = old - 1;

        const int tier = t.settlementTier;
        if (tier > t.peakTier) t.peakTier = tier;
        ++tierCount[tier];

        if (tier != old) {
            const bool grew = tier > old;
            logEvent(day, grew ? "The " + t.name + " raise their " + TIER_NAME[old]
                                 + " into a " + TIER_NAME[tier]
                               : "The " + t.name + "'s " + TIER_NAME[old]
                                 + " dwindles back to a " + TIER_NAME[tier],
                     "settlement",
                     std::string("kind=settlement_") + (grew ? "growth" : "decline")
                     + " tribe=\"" + t.name + "\""
                     + " tribeId="   + std::to_string(t.id)
                     + " tier="      + std::to_string(tier)
                     + " peakTier="  + std::to_string(t.peakTier)
                     + " population="+ std::to_string(pop)
                     + " surplus="   + std::to_string(surplus));
        }
        if (tier == 0) continue;   // a camp is just people standing near a fire

        // ── Agglomeration: the payoff for living close together ──────────────
        // Research rises ~12% per tier over the population term TechTree accrues,
        // culture thickens, and the collective temperament turns inventive — the
        // spatial face of the collective brain.
        const float agglom = 0.12f * (float)tier * cityMul;
        t.researchPoints += (float)pop * 0.5f * agglom;
        t.cultureScore    = clamp(t.cultureScore + 0.02f * (float)tier * cityMul, 0.0f, 100.0f);
        t.innovation     += ((50.0f + 10.0f * (float)tier) - t.innovation) * 0.002f * cityMul;

        if (tier < 2) continue;    // a village is not yet crowded

        // ── Crowding: the bill density presents ──────────────────────────────
        // Sanitation techniques (pots to store clean water, canals to carry waste,
        // masonry underfoot) buy most of it back — which is why real towns only
        // became survivable once they were engineered.
        float sanitation = 1.0f;
        if (t.techTreeUnlocked.count(5)) sanitation -= 0.20f;   // Pottery
        if (t.techTreeUnlocked.count(9)) sanitation -= 0.25f;   // Irrigation
        if (t.techTreeUnlocked.count(6)) sanitation -= 0.15f;   // Masonry
        sanitation = std::max(0.35f, sanitation);

        const float crowd = (float)(tier - 1) * sanitation * cityMul;

        // Epidemics trace density: the bigger and filthier the place, the more
        // often a sickness finds enough hosts to become an outbreak.
        if (roll(rng) < 0.004f * crowd) {
            int seeded = 0;
            for (int mid : t.memberIds) {
                if (seeded >= 3) break;
                Entity* e = entityById(entities, mid);
                if (!e || e->entityHealth <= 0.0f) continue;
                e->exposeToPathogen(1, day);
                ++seeded;
            }
            if (seeded > 0)
                logEvent(day, "Sickness breaks out in the crowded " + std::string(TIER_NAME[tier])
                         + " of the " + t.name, "disease",
                         std::string("kind=urban_epidemic tribe=\"") + t.name + "\""
                         + " tribeId=" + std::to_string(t.id)
                         + " tier="    + std::to_string(tier)
                         + " exposed=" + std::to_string(seeded));
        }

        // The daily grind of the crowd: noise, strangers, and the particular
        // loneliness of being unknown among many (urban anomie — the counterpart
        // to I-P1's sense of purpose, which is what buffers it).
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (!e || e->entityHealth <= 0.0f) continue;
            e->entityStress     = clamp(e->entityStress     + 0.10f * crowd, 0.0f, 100.0f);
            e->entityLoneliness = clamp(e->entityLoneliness + 0.06f * crowd, 0.0f, 100.0f);
        }
        if (tier >= 3)
            t.govSatisfaction = clamp(t.govSatisfaction - 0.05f * crowd, 0.0f, 100.0f);
    }

    // A periodic census so the realism report can fit the rank-size curve without
    // replaying the whole event log.
    if (day % 200 == 0 && !tribes.empty())
        logEvent(day, "Census of the settled world", "settlement",
                 "kind=settlement_census"
                 " camps="       + std::to_string(tierCount[0]) +
                 " villages="    + std::to_string(tierCount[1]) +
                 " towns="       + std::to_string(tierCount[2]) +
                 " cities="      + std::to_string(tierCount[3]) +
                 " greatCities=" + std::to_string(tierCount[4]));
}

// ── III-P4: class as heritable reproduction (Parallel-Earth Track III) ───────
// Cultural capital is *earned* here and *inherited* elsewhere (Kinship.cpp at
// birth, SocialOrder::onDeath at the grave). What builds it is exactly what
// Bourdieu says marks a class position: letters and learning, the standing of
// the house you belong to, a life with enough slack to cultivate taste — and
// what erodes it is doing without any of those. Because children start with
// three-quarters of their parents' share, advantage compounds down a line while
// a commoner's climb has to be made from nothing, every generation.
// Kill switch: classMul == 0 returns before any state is read or written.
void CivilizationEngine::updateClassReproduction(std::vector<Entity>& entities, int day) {
    const float classMul = g_liveConfig.classMul;
    if (classMul == 0.0f) return;   // director kill switch — bit-exact off
    (void)day;
    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };

    for (Tribe& t : tribes) {
        const bool hasSchool =
            institutions.find(t.id, environment::InstitutionType::EDUCATION) != nullptr;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (!e || e->entityHealth <= 0.0f) continue;

            // Letters and learning are the backbone of it; a house's standing
            // and a life with room to breathe do the rest.
            float target = 10.0f
                         + e->skills.get(SK_LORE) * 0.35f
                         + (hasSchool ? 12.0f : 0.0f)
                         + (e->isSpecialist ? 8.0f : 0.0f)
                         + std::max(0.0f, e->Esteem - 50.0f) * 0.20f
                         - std::max(0.0f, e->entityStress - 60.0f) * 0.15f;
            if (globalKinship && e->familyId >= 0)
                if (const Family* fam = globalKinship->findFamily(e->familyId))
                    target += fam->prestige * 0.20f;
            target = clamp(target, 0.0f, 100.0f);

            // It moves slowly — a lifetime's cultivation, not a season's.
            e->culturalCapital = clamp(e->culturalCapital
                                       + (target - e->culturalCapital) * 0.02f * classMul,
                                       0.0f, 100.0f);
        }
    }
}

// ── IV-P1: culture as transmissible content (Parallel-Earth Track IV) ────────
// The flagship run's culture was a number that went up. A number cannot diverge
// between valleys, cannot be carried home by a caravan, and cannot flip. So
// culture becomes *content*: a catalogue of practices, beliefs, taboos, tastes
// and fashions (environment::CulturalTransmissionSystem), each of which a person
// either keeps or does not. A people's culture is then simply what enough of its
// members hold, two peoples can be compared, and cultural history is the record
// of traits moving between heads.
//
// Four things move them, and this pass runs all four:
//
//   • Inheritance (vertical) — handled at birth in Kinship.cpp: a child starts
//     life holding most of what its parents held. This is why culture persists
//     across generations at all, and why isolated valleys stay themselves.
//   • Peers (horizontal) and elders (oblique) — this pass: people take up what
//     the people around them do, the young far more readily than the old, and
//     faster still where there is a school to teach them (II-P2).
//   • Invention (mutation) — a novelty struck by one person in one people. Most
//     of them die; this is the raw material selection acts on.
//   • Contact — traits travel the trade roads of III-P2 and the bonds of
//     alliance, and are pressed on the conquered by their overlords. Isolation
//     is therefore what makes regions culturally distinct: not a rule, a
//     consequence of nobody walking there.
//
// The one rule that shapes all of it is Centola's critical mass. Adoption below
// ~25% carriers is deliberately feeble and abandonment is real, so nearly every
// novelty fizzles; at 25% adoption multiplies and the rest of the people follow
// quickly. That is what makes cultural change *punctuated* — long flat stretches
// and sudden flips — instead of the smooth drift a scalar can only ever produce.
//
// Kill switch: traitMul == 0 returns before any state is read or written.
void CivilizationEngine::updateCulturalTraits(std::vector<Entity>& entities, int day) {
    const float traitMul = g_liveConfig.traitMul;
    if (traitMul == 0.0f) return;   // director kill switch — bit-exact off

    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    const int nTraits = culture.traitCount();
    if (nTraits <= 0) return;
    auto BIT = [](int id) { return environment::CulturalTransmissionSystem::bit(id); };

    // Walk the set bits of a trait set (a culture is usually a handful of
    // traits, so this is far cheaper than testing all 64 slots).
    auto forEachTrait = [](unsigned long long set, auto&& fn) {
        while (set) {
            unsigned long long low = set & (~set + 1ull);
            int id = 0;
            while ((low >> id) != 1ull) ++id;
            fn(id);
            set &= set - 1ull;
        }
    };

    for (Tribe& t : tribes) {
        std::vector<Entity*> members;
        members.reserve(t.memberIds.size());
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f) members.push_back(e);
        }
        if (members.size() < 3) continue;
        const float pop = (float)members.size();
        // The first time this people is looked at — a tribe just formed, split
        // off, or sailed out to found a colony — its members already carry the
        // ways they were raised in. That is inheritance arriving, not a norm
        // spreading, so the tally below latches it silently instead of
        // announcing a cascade nobody lived through.
        const bool firstLook = (t.knownTraits == 0ull);

        // ── 1. Founding stock ────────────────────────────────────────────────
        // A new people is not born blank: it carries the ways of the country it
        // came out of. Seeding from region and biome (not from the die) is what
        // makes two valleys that never meet start apart and stay apart — the
        // divergence the acceptance test looks for is structural, not luck.
        unsigned long long known = 0ull;
        for (Entity* e : members) known |= e->cultureTraits;
        if (known == 0ull) {
            const int base = std::abs(t.regionId * 7 + t.homeBiome * 13 + 3);
            unsigned long long stock = 0ull;
            for (int k = 0; k < 4; ++k) stock |= BIT((base + k * 11) % nTraits);
            // One trait of its own, so sibling tribes of one cradle are kin, not copies.
            stock |= BIT((base + t.id * 5) % nTraits);
            // One way per family: a people that both burns and buries its dead
            // is not a people, it is a list. Later options displace earlier.
            {
                unsigned long long picked = 0ull;
                unsigned long long left = stock;
                while (left) {
                    unsigned long long low = left & (~left + 1ull);
                    int id = 0; while ((low >> id) != 1ull) ++id;
                    picked = (picked & ~culture.rivals(id)) | low;
                    left &= left - 1ull;
                }
                stock = picked;
            }
            for (Entity* e : members) e->cultureTraits |= stock;
            known = stock;
            // Inherited ways are not a cascade — they arrive already universal.
            // Latching them here keeps the tally a count of actual conversions.
            t.cascadedTraits |= stock;
            std::string names;
            forEachTrait(stock, [&](int id) {
                names += (names.empty() ? "" : ", ") + culture.trait(id).name;
            });
            logEvent(day, "The " + t.name + " keep the old ways of their country: " + names,
                     "culture",
                     "kind=culture_founded tribe=\"" + t.name + "\""
                     + " tribeId="  + std::to_string(t.id)
                     + " regionId=" + std::to_string(t.regionId)
                     + " traits=\"" + names + "\"");
        }

        // ── 2. Who currently carries what ────────────────────────────────────
        // A snapshot: every adoption and abandonment below is judged against
        // this morning's prevalence, so the whole people updates at once rather
        // than the first-listed member deciding for everyone after them.
        // Alongside the headcount, WHO carries a trait. People do not weigh a
        // new way of doing things by how many hold it but by whose it is
        // (Henrich's prestige bias): a practice the chief and the tribe's
        // makers keep travels through a large people that a practice held by
        // three nobodies never would. The 25% test below still counts heads —
        // that is Centola's measure — but what gets you there is standing.
        int carriers[environment::CulturalTransmissionSystem::MAX_TRAITS] = { 0 };
        int standing[environment::CulturalTransmissionSystem::MAX_TRAITS] = { 0 };
        for (Entity* e : members) {
            const int weight = (e->entityId == t.leaderId ? 3 : 0) + (e->isSpecialist ? 1 : 0);
            forEachTrait(e->cultureTraits & known, [&](int id) {
                ++carriers[id];
                standing[id] += weight;
            });
        }

        // ── 3. Invention ─────────────────────────────────────────────────────
        // Somebody does something nobody here has done before. Inventive,
        // literate peoples strike novelties more often — the collective brain
        // (II-P1) applies to ways of living as much as to techniques.
        const float inventiveness = 0.5f + t.innovation / 120.0f;
        if (culture.count(known) < nTraits
            && roll(rng) < 0.030f * inventiveness * traitMul) {
            // Pick from what this people does NOT already do.
            int pick = -1;
            for (int attempt = 0; attempt < 8; ++attempt) {
                std::uniform_int_distribution<int> pd(0, nTraits - 1);
                int cand = pd(rng);
                if (!environment::CulturalTransmissionSystem::holds(known, cand)) { pick = cand; break; }
            }
            if (pick >= 0) {
                // Whoever happens to be about, with a bias toward the openest
                // of them — novelty comes from the curious, but not always from
                // the same curious person.
                std::uniform_int_distribution<int> md(0, (int)members.size() - 1);
                Entity* inventor = members[md(rng)];
                for (int k = 0; k < 2; ++k) {
                    Entity* other = members[md(rng)];
                    if (other->personality.openness > inventor->personality.openness) inventor = other;
                }
                inventor->cultureTraits   &= ~culture.rivals(pick);   // the old way goes
                inventor->cultureTraits   |= BIT(pick);
                inventor->committedTraits |= BIT(pick);   // their own thing; they keep it
                ++carriers[pick];
                known |= BIT(pick);
                ++totalTraitsInvented;
                logEvent(day, inventor->name + " of the " + t.name + " takes up something new: "
                              + culture.trait(pick).name, "culture",
                         "kind=trait_invented tribe=\"" + t.name + "\""
                         + " tribeId="  + std::to_string(t.id)
                         + " trait=\""  + culture.trait(pick).name + "\""
                         + " category=\"" + culture.trait(pick).category + "\""
                         + " entity=\"" + inventor->name + "\""
                         + " entityId=" + std::to_string(inventor->entityId));
            }
        }

        // ── 4. Transmission: peers, elders, and giving a thing up ────────────
        // A school (II-P2) is oblique transmission made deliberate: the young
        // are taught the people's ways instead of merely catching them.
        const bool hasSchool =
            institutions.find(t.id, environment::InstitutionType::EDUCATION) != nullptr;
        forEachTrait(known, [&](int id) {
            const float p = (float)carriers[id] / pop;
            // What a would-be adopter actually sees: the practice plus the
            // standing of the people keeping it.
            const float seen = std::min(1.0f, (float)(carriers[id] + standing[id]) / pop);
            for (Entity* e : members) {
                const bool carrier =
                    environment::CulturalTransmissionSystem::holds(e->cultureTraits, id);
                if (carrier) {
                    // What almost nobody does any more is quietly dropped —
                    // unless this is the person who started it, who will keep
                    // doing it while everyone else finds it strange. That
                    // stubbornness is the whole reason anything new ever
                    // reaches the critical mass.
                    if (environment::CulturalTransmissionSystem::holds(e->committedTraits, id))
                        continue;
                    if (roll(rng) < culture.abandonChance(id, p) * traitMul)
                        e->cultureTraits &= ~BIT(id);
                    continue;
                }
                // How ready this person is to take up someone else's way: their
                // own openness, how inventive their people is, and — most of
                // all — youth. Children absorb; the old have their habits.
                float recept = 0.55f + e->personality.openness / 130.0f
                                     + t.innovation / 400.0f;
                if (e->entityAge < 20.0f) recept *= 1.0f + culture.obliqueRate()
                                                   + (hasSchool ? 0.35f : 0.0f);
                if (roll(rng) < culture.adoptionChance(id, seen, recept) * traitMul) {
                    // Taking up one way of doing a thing is giving up the other
                    // — unless the old way is one they will not part with.
                    e->cultureTraits &= ~(culture.rivals(id) & ~e->committedTraits);
                    e->cultureTraits |= BIT(id);
                }
            }
        });

        // ── 5. What the day changed ──────────────────────────────────────────
        unsigned long long nowKnown = 0ull, majority = 0ull;
        int after[environment::CulturalTransmissionSystem::MAX_TRAITS] = { 0 };
        for (Entity* e : members)
            forEachTrait(e->cultureTraits, [&](int id) { ++after[id]; });
        for (int id = 0; id < nTraits; ++id) {
            if (after[id] <= 0) {
                // A trait has died out here. If it had once taken hold, that is
                // a way of life ending, and the ledger should say so.
                if (environment::CulturalTransmissionSystem::holds(t.knownTraits, id)) {
                    ++totalFizzles;
                    if (environment::CulturalTransmissionSystem::holds(t.cascadedTraits, id))
                        logEvent(day, "The " + t.name + " no longer keep " + culture.trait(id).name,
                                 "culture",
                                 "kind=trait_lost tribe=\"" + t.name + "\""
                                 + " tribeId=" + std::to_string(t.id)
                                 + " trait=\"" + culture.trait(id).name + "\"");
                    t.cascadedTraits &= ~BIT(id);
                }
                continue;
            }
            nowKnown |= BIT(id);
            const float p = (float)after[id] / pop;
            if (p * 100.0f >= (float)environment::CulturalTransmissionSystem::CRITICAL_MASS_PCT
                && !environment::CulturalTransmissionSystem::holds(t.cascadedTraits, id)) {
                t.cascadedTraits |= BIT(id);
                if (firstLook) continue;   // brought in, not converted to
                // The critical mass, crossed. From here the rest of the people
                // follow quickly — this is the moment a quirk becomes a custom.
                ++totalCascades;
                logEvent(day, culture.trait(id).name + " passes from a few hands to the many "
                              "among the " + t.name, "culture",
                         "kind=norm_cascade tribe=\"" + t.name + "\""
                         + " tribeId="   + std::to_string(t.id)
                         + " trait=\""   + culture.trait(id).name + "\""
                         + " category=\"" + culture.trait(id).category + "\""
                         + " carriers="  + std::to_string(after[id])
                         + " population=" + std::to_string((int)pop)
                         + " share="     + std::to_string((int)(p * 100.0f)));
            }
            if (p >= 0.5f) majority |= BIT(id);
        }
        t.knownTraits   = nowKnown;
        t.cultureTraits = majority;

        // ── 6. What culture is worth in the class system (III-P4) ────────────
        // Some ways of living are how a household shows what it is: letters,
        // fine ornament, a table people want to be invited to. Keeping them
        // cultivates the cultural capital that reproduces class position.
        for (Entity* e : members) {
            int prestige = 0;
            forEachTrait(e->cultureTraits, [&](int id) {
                if (culture.trait(id).prestigious) ++prestige;
            });
            if (prestige > 0)
                e->culturalCapital = clamp(e->culturalCapital
                                           + 0.010f * (float)prestige * traitMul, 0.0f, 100.0f);
        }
    }

    // ── 7. Contact: culture travels, and that is why isolation diverges ──────
    // Along a standing trade road (III-P2), between allies, and downward from
    // an overlord onto the people it holds — three channels, each of which is a
    // real historical route for a practice to cross a border. Everywhere else,
    // nothing crosses, and the two cultures drift apart on their own.
    for (size_t i = 0; i < tribes.size(); ++i) {
        for (size_t j = 0; j < tribes.size(); ++j) {
            if (i == j) continue;
            Tribe& src = tribes[i];
            Tribe& dst = tribes[j];
            if (src.cultureTraits == 0ull) continue;

            bool road = false;
            for (const TradeRoute& r : tradeRoutes)
                if (r.active && r.a == std::min(src.id, dst.id) && r.b == std::max(src.id, dst.id)) {
                    road = true; break;
                }
            auto rel = src.relations.find(dst.id);
            const bool friendly = (rel != src.relations.end() && rel->second >= 45.0f);
            const bool ruled    = (dst.overlordTribeId == src.id);
            // Living next door is itself a channel, and the plainest one: people
            // who share a valley see how the others do things whether or not
            // anyone signed anything. It falls off fast with the walk between
            // them, and it stops at a hostile border — a people that dislikes
            // its neighbour does not take up its neighbour's ways; it keeps its
            // own the harder, which is what an ethnic marker IS. Together those
            // two facts are why the world does not converge on one culture
            // however long it runs.
            const float dx = src.centerX - dst.centerX, dy = src.centerY - dst.centerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float NEIGHBOUR_RANGE = 160.0f;
            const bool  hostile = (rel != src.relations.end() && rel->second < 0.0f);
            const float nearness = (dist < NEIGHBOUR_RANGE && !hostile)
                                 ? (1.0f - dist / NEIGHBOUR_RANGE) : 0.0f;
            if (!road && !friendly && !ruled && nearness <= 0.0f) continue;
            if (areTribesAtWar(src.id, dst.id)) continue;

            // A road carries the most, and a conqueror presses hardest.
            float chance = (road ? 0.10f : 0.0f) + (friendly ? 0.05f : 0.0f)
                         + (ruled ? 0.12f : 0.0f) + 0.05f * nearness;
            // A people confident in its own ways borrows less.
            chance *= clamp(1.2f - dst.collectivism / 200.0f, 0.4f, 1.2f);
            // IV-P3: you cannot take up a practice you cannot have explained to
            // you. A shared tongue carries a way of doing things; a language
            // boundary is where cultures stop bleeding into one another, which
            // is precisely why the world's cultural map has ever had edges.
            chance *= mutualIntelligibility(src, dst);
            if (roll(rng) >= chance * traitMul) continue;

            // Something they do there that is not done here.
            const unsigned long long candidates = src.cultureTraits & ~dst.knownTraits;
            if (candidates == 0ull) continue;
            int pool[environment::CulturalTransmissionSystem::MAX_TRAITS];
            int n = 0;
            forEachTrait(candidates, [&](int id) { pool[n++] = id; });
            std::uniform_int_distribution<int> pd(0, n - 1);
            const int id = pool[pd(rng)];

            // It arrives in one head: a trader who came back changed, a young
            // person in a conquered town copying the conqueror. From there it
            // either catches on here or it does not — the tipping point decides.
            Entity* taker = nullptr;
            for (int mid : dst.memberIds) {
                Entity* e = entityById(entities, mid);
                if (!e || e->entityHealth <= 0.0f) continue;
                if (!taker
                    || (e->specialization == "trader" && taker->specialization != "trader")
                    || (e->personality.openness > taker->personality.openness
                        && taker->specialization != "trader"))
                    taker = e;
            }
            if (!taker) continue;
            // It arrives held by one person and is NOT theirs the way an
            // invention is: a borrowed practice has to win its keep here or be
            // dropped like any other novelty. (Committing importers the way
            // inventors are committed made every import eventually stick, and a
            // long run converged on a single world culture.)
            taker->cultureTraits &= ~(culture.rivals(id) & ~taker->committedTraits);
            taker->cultureTraits |= BIT(id);
            dst.knownTraits      |= BIT(id);
            ++totalTraitsDiffused;
            logEvent(day, taker->name + " brings " + culture.trait(id).name + " home to the "
                          + dst.name + " from the " + src.name, "culture",
                     "kind=trait_diffused"
                     " from=\""   + src.name + "\""
                     " to=\""     + dst.name + "\""
                     " tribeId="  + std::to_string(dst.id)
                     + " trait=\"" + culture.trait(id).name + "\""
                     + " channel=" + std::string(road ? "road" : (ruled ? "conquest"
                                                : (friendly ? "alliance" : "neighbours")))
                     + " distance=" + std::to_string((int)dist));
        }
    }
}

// ── II-P3: secular cycles & elite overproduction (Parallel-Earth Track II) ───
// Turchin's structural-demographic answer to why complex societies come apart
// on a rhythm rather than at random. Two pressures build at once and feed each
// other:
//
//   • Popular immiseration — population presses on what the land yields, so
//     what an ordinary life is actually like (food in the store, health, the
//     stress of getting by) degrades.
//   • Elite overproduction — the number of people with the wealth and standing
//     to expect a position grows faster than the positions there are to hold.
//     Surplus aspirants do not quietly go away; they compete, and competition
//     among elites is far more destabilising to a state than discontent below.
//
// Together they drive a political-stress indicator. Stress does NOT rise for
// ever: past a threshold it discharges as strife, which culls or ruins the
// surplus elite, redistributes some of what they held, and resets the clock.
// That discharge is the whole mechanism — it is why history oscillates instead
// of flat-lining at a plateau, which is exactly the featureless plateau the
// flagship run complained of. A people that has meanwhile built literacy and
// institutions (II-P1/II-P2) carries its knowledge across the trough and can
// ride the next upswing higher; one that has not simply cycles.
//
// Kill switch: cycleMul == 0 returns before any state is read or written.
void CivilizationEngine::updateSecularCycle(std::vector<Entity>& entities, int day) {
    const float cycleMul = g_liveConfig.cycleMul;
    if (cycleMul == 0.0f) return;   // director kill switch — bit-exact off

    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    const float gini = wealthGini(entities);

    float sumWell = 0.0f, sumInst = 0.0f, sumElite = 0.0f, sumOver = 0.0f;
    int   counted = 0;

    for (Tribe& t : tribes) {
        std::vector<Entity*> living;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f) living.push_back(e);
        }
        const int pop = (int)living.size();
        if (pop < 4) continue;

        // ── 1. What is life like at the bottom? ─────────────────────────────
        // The "real wage": food actually in the store per head, what health the
        // poorer half is carrying, and how hard they are having to work at it.
        std::vector<float> wealth;
        wealth.reserve(pop);
        for (Entity* e : living) wealth.push_back(std::max(0.0f, e->salary.token));
        std::sort(wealth.begin(), wealth.end());
        float poorHealth = 0.0f, poorStress = 0.0f;
        int   poorN = std::max(1, pop / 2);
        {
            // The poorer half, by wealth — recomputed against the sorted median.
            float median = wealth[pop / 2];
            int   n = 0;
            for (Entity* e : living) {
                if (e->salary.token > median) continue;
                poorHealth += e->entityHealth;
                poorStress += e->entityStress;
                if (++n >= poorN) break;
            }
            if (n > 0) { poorHealth /= n; poorStress /= n; }
        }
        float foodPerHead = t.granary / (float)pop;
        // Malthusian pressure, which is the actual engine of immiseration: when
        // a region carries more people than it can feed, an ordinary life gets
        // worse regardless of what is in this particular granary today. The
        // carrying-capacity model already computes both halves each civ-day.
        float overshoot = 0.0f;
        if (t.regionId >= 0) {
            auto pIt = regionPopulation.find(t.regionId);
            auto cIt = regionCapacity.find(t.regionId);
            if (pIt != regionPopulation.end() && cIt != regionCapacity.end() && cIt->second > 1.0f)
                overshoot = std::max(0.0f, (float)pIt->second / cIt->second - 1.0f);
        }
        float wellTarget = clamp(20.0f + std::min(40.0f, foodPerHead * 12.0f)
                                 + (poorHealth - 50.0f) * 0.4f
                                 - poorStress * 0.25f
                                 - std::min(45.0f, overshoot * 45.0f), 0.0f, 100.0f);
        t.popularWellbeing += (wellTarget - t.popularWellbeing) * 0.10f;

        // ── 2. How many aspirants, and how many chairs? ─────────────────────
        // Offices are the real positions of power this people actually has: the
        // leadership, the seats on its council, and the institutions of II-P2
        // that someone must run. Aspirants are the wealthy and high-standing who
        // expect one. When aspirants outnumber chairs, the surplus does not
        // disperse — it competes.
        // Positions of real power are SCARCE, and deliberately do not grow with
        // the population one-for-one — that scarcity is the whole mechanism. A
        // first pass counted the leader, every council seat and every
        // institution, which in a fifteen-person band came to six offices for
        // two aspirants: an elite ratio of 0.31, and a cycle that could never
        // start. A chiefdom has a chief and perhaps a war-leader; a state adds
        // a chair for roughly every dozen more people it governs, plus one for
        // a standing bureaucracy.
        int offices = 1 + pop / 12;
        if (institutions.find(t.id, environment::InstitutionType::GOVERNMENT)) ++offices;
        float p80 = wealth[std::min((size_t)pop - 1, (size_t)(0.80f * pop))];
        int aspirants = 0;
        for (Entity* e : living)
            if (e->salary.token >= p80 && (e->isSpecialist || e->auctoritas > 60.0f))
                ++aspirants;
        t.eliteOverproduction = (float)aspirants / (float)std::max(1, offices);

        // ── 3. Political stress ─────────────────────────────────────────────
        // Elite competition weighs heaviest (Turchin's central claim: states are
        // broken from above far more often than from below), immiseration next,
        // inequality last. Stress bleeds off slowly when none of them are biting.
        // Immiseration is measured against a decent life, not against
        // destitution. At a reference of 45 the term was dead almost all the
        // time — this world's ordinary wellbeing sits around 60 — so political
        // stress was driven by elite competition and inequality alone and had
        // no coupling to how the people were living at all. That is precisely
        // the coupling Turchin's cycle is made of, and without it the two
        // series drift independently and the anti-phase never appears in the
        // telemetry however well the rest of the machinery runs. Referenced at
        // 70, every real dip in living standards now feeds the stress that
        // eventually discharges as strife.
        float pressure = 2.2f * std::max(0.0f, t.eliteOverproduction - 1.0f)
                       + 1.9f * std::max(0.0f, (70.0f - t.popularWellbeing) / 70.0f)
                       + 1.2f * std::max(0.0f, gini - 0.35f);
        t.instability = clamp(t.instability + (pressure - 0.55f) * cycleMul, 0.0f, 100.0f);

        // Stress is felt, not just recorded: a people that can see its own
        // notables jockeying and its poor going short is an unhappy one.
        if (t.instability > 40.0f)
            t.govSatisfaction = clamp(t.govSatisfaction - 0.04f * (t.instability - 40.0f) * cycleMul,
                                      0.0f, 100.0f);

        // ── 4. Discharge: the strife phase ──────────────────────────────────
        // Past the threshold the accumulated stress breaks. Surplus elites are
        // the ones it breaks on — ruined, stripped of standing, some killed —
        // and what they held is scattered. This is what resets the cycle, and
        // why the next generation starts from a lower, flatter base.
        if (t.instability > 72.0f && day - t.lastStrifeDay > 150
            && roll(rng) < 0.25f * cycleMul) {
            t.lastStrifeDay = day;
            ++t.strifeCount;
            ++totalStrifes;

            int ruined = 0, killed = 0;
            for (Entity* e : living) {
                if (e->salary.token < p80) continue;
                if (!(e->isSpecialist || e->auctoritas > 60.0f)) continue;
                // Most of the surplus is ruined rather than killed: fortunes
                // confiscated, standing lost, back down among everyone else.
                float loss = e->salary.token * 0.45f;
                e->salary.spendMoney(loss);
                e->auctoritas = std::max(0.0f, e->auctoritas - 18.0f);
                e->Esteem     = std::max(0.0f, e->Esteem - 12.0f);
                e->entityStress = std::min(100.0f, e->entityStress + 20.0f);
                mind::recordLifeChapter(e, "ruined", -1,
                                        "was ruined when the " + t.name + " turned on its own", day);
                ++ruined;
                // The confiscated share is spread across the commons — the
                // levelling that makes the trough of a cycle less unequal.
                float sharePer = loss / (float)pop;
                for (Entity* other : living) other->salary.earnMoney(sharePer);
                // A minority of the purge is lethal.
                if (roll(rng) < 0.15f) {
                    e->entityHealth = 0.0f;
                    if (e->pendingDeathCause.empty()) e->pendingDeathCause = "killed in civil strife";
                    ++killed;
                }
            }
            t.instability     = clamp(t.instability - 45.0f, 0.0f, 100.0f);
            t.govSatisfaction = clamp(t.govSatisfaction - 10.0f, 0.0f, 100.0f);
            // Strife is not free for the people it happens to. Fields go
            // unworked, stores are seized or burnt, and the ordinary household
            // is poorer for a while after its betters have finished fighting —
            // which is the OTHER half of the secular cycle and the half that
            // was missing. Without it, stress accumulated and discharged with
            // no visible effect on how anyone lived, so the two series drifted
            // independently and the anti-phase Turchin describes never showed
            // up in the telemetry (measured correlation ~0 on a world whose
            // mechanism was otherwise right). A disintegrative phase has to be
            // felt at the bottom, and recovery has to take time.
            // Felt through the granary rather than written straight onto
            // wellbeing: the harvest is what a disintegrative phase actually
            // costs an ordinary household, and letting it arrive with the lag
            // that food shortage really has keeps the phase relationship
            // right (stress peaks, strife breaks, hardship follows) instead of
            // dropping both series together on the same day.
            t.granary = std::max(0.0f, t.granary * 0.70f);

            logEvent(day, "The " + t.name + " turn on themselves — " + std::to_string(ruined)
                     + " of their notables are brought down"
                     + (killed ? (", " + std::to_string(killed) + " killed") : ""),
                     "strife",
                     "kind=civil_strife tribe=\"" + t.name + "\""
                     + " tribeId=" + std::to_string(t.id)
                     + " ruined=" + std::to_string(ruined)
                     + " killed=" + std::to_string(killed)
                     + " eliteOverproduction=" + std::to_string(t.eliteOverproduction)
                     + " wellbeing=" + std::to_string((int)t.popularWellbeing)
                     + " instability=" + std::to_string((int)t.instability)
                     + " episode=" + std::to_string(t.strifeCount));
        }

        sumWell += t.popularWellbeing;
        sumInst += t.instability;
        sumElite += t.eliteOverproduction;
        sumOver  += overshoot;
        ++counted;
    }

    // Sample the world so the cycle can be correlated (and plotted) afterwards.
    if (counted > 0 && (day % 10 == 0)) {
        CycleSample s;
        s.day = day;
        s.wellbeing   = sumWell / counted;
        s.instability = sumInst / counted;
        s.elites      = sumElite / counted;
        s.overshoot   = sumOver / counted;
        s.gini        = gini;
        cycleHistory.push_back(s);
        if (cycleHistory.size() > 4000) cycleHistory.erase(cycleHistory.begin());
    }
}

// ── III-P2: regional markets & trade routes (Parallel-Earth plan Track III) ──
// Until now this world had ONE price for a loaf, everywhere, simultaneously —
// which is not a market, it is a rumour that travels at infinite speed. Real
// prices are local: a granary full here and empty a week's walk away is the
// whole reason anyone ever loaded a mule. This pass makes the global market the
// *aggregate* rather than the primitive:
//
//   • Every people prices food and goods against its own stores per head. Plenty
//     is cheap, dearth is dear, and neighbours therefore disagree about what
//     things are worth. That disagreement is the gradient.
//   • A trade route is a standing link along ground that can actually be walked
//     (mountains and ice block it; open water needs Sailing) between peoples not
//     at war. Caravans run it, carrying goods from the cheap end to the dear end.
//   • Carrying goods narrows the gap — that is arbitrage, and it is what makes
//     the price gradient between linked peoples measurably smaller than between
//     unlinked ones. Cut the route (war, a closed pass) and the gap springs open
//     again. Traders take a cut, so commerce builds a merchant class.
//   • Caravans carry more than cargo: techniques and habits ride along, which is
//     the collective brain (II-P1) travelling on the map instead of by magic.
//
// Kill switch: tradeMul == 0 returns before any state is read or written.
void CivilizationEngine::updateTrade(std::vector<Entity>& entities, int day) {
    const float tradeMul = g_liveConfig.tradeMul;
    if (tradeMul == 0.0f) return;   // director kill switch — bit-exact off

    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    // ── 1. What is a thing worth HERE? ───────────────────────────────────────
    // Stores per head against what a household needs. A glut halves the price,
    // a dearth doubles it, and prices move over days rather than instantly —
    // markets have memory.
    auto priceFrom = [&](float perHead, float need) {
        float r = perHead / std::max(0.01f, need);
        return clamp(2.0f - 1.0f * std::min(2.0f, r), 0.5f, 2.0f);
    };
    for (Tribe& t : tribes) {
        int pop = 0;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f) ++pop;
        }
        if (pop == 0) continue;
        float targetFood  = priceFrom(t.granary / (float)pop, 2.0f);
        float targetGoods = priceFrom((t.matStock + t.luxuryStock) / (float)pop, 1.0f);
        t.priceFood  += (targetFood  - t.priceFood)  * 0.15f * tradeMul;
        t.priceGoods += (targetGoods - t.priceGoods) * 0.15f * tradeMul;
    }

    // ── 2. Which links can exist at all? ─────────────────────────────────────
    // Ground first: sample the straight line between two peoples. Mountain and
    // ice stop a caravan dead; open water stops one that has never built a boat.
    auto pathBetween = [&](const Tribe& A, const Tribe& B, bool& bySea) -> bool {
        bySea = false;
        if (!g_planet) return true;              // no world model: assume walkable
        const int SAMPLES = 10;
        for (int s = 1; s < SAMPLES; ++s) {
            float f = (float)s / SAMPLES;
            float x = A.centerX + (B.centerX - A.centerX) * f;
            float y = A.centerY + (B.centerY - A.centerY) * f;
            const Tile* tile = g_planet->tileAtWorld(x, y);
            if (!tile) continue;
            if (!tile->isLand()) { bySea = true; continue; }   // a crossing
            if (!tile->isPassable()) return false;             // mountain / ice wall
        }
        if (bySea) {
            bool sails = A.knownTechName.count("Sailing") || B.knownTechName.count("Sailing");
            if (!sails) return false;
        }
        return true;
    };

    auto findRoute = [&](int a, int b) -> TradeRoute* {
        for (TradeRoute& r : tradeRoutes)
            if (r.a == std::min(a, b) && r.b == std::max(a, b)) return &r;
        return nullptr;
    };

    for (size_t i = 0; i < tribes.size(); ++i) {
        for (size_t j = i + 1; j < tribes.size(); ++j) {
            Tribe& A = tribes[i];
            Tribe& B = tribes[j];
            if (A.population() < 3 || B.population() < 3) continue;

            const bool atWar = (A.stances.count(B.id) && A.stances[B.id] == TS_AT_WAR)
                            || (B.stances.count(A.id) && B.stances[A.id] == TS_AT_WAR);
            float rel = A.relations.count(B.id) ? A.relations[B.id] : 0.0f;
            float dx = A.centerX - B.centerX, dy = A.centerY - B.centerY;
            float dist = std::sqrt(dx * dx + dy * dy);

            TradeRoute* route = findRoute(A.id, B.id);

            // A route dies when the peoples fight, fall out, or drift apart.
            if (route && route->active) {
                if (atWar || rel < -15.0f || dist > 900.0f) {
                    route->active = false;
                    ++totalRoutesCut;
                    logEvent(day, "The trade road between the " + A.name + " and the "
                             + B.name + (atWar ? " is cut by war" : " falls out of use"),
                             "trade",
                             std::string("kind=route_cut reason=") + (atWar ? "war" : "estrangement")
                             + " tribeA=\"" + A.name + "\" tribeB=\"" + B.name + "\""
                             + " tribeAId=" + std::to_string(A.id)
                             + " tribeBId=" + std::to_string(B.id)
                             + " volume=" + std::to_string((int)route->volume));
                }
                continue;
            }

            // Opening one takes peace, proximity, warmth (or a formal pact) and
            // passable ground.
            if (atWar || dist > 700.0f) continue;
            bool pact = hasActiveTreaty(A.id, B.id, TREATY_TRADE);
            if (rel < 15.0f && !pact) continue;
            bool bySea = false;
            if (!pathBetween(A, B, bySea)) continue;

            if (route) {   // an old road reopens
                route->active = true;
                route->bySea  = bySea;
                route->distance = dist;
            } else {
                TradeRoute r;
                r.a = std::min(A.id, B.id); r.b = std::max(A.id, B.id);
                r.establishedDay = day; r.distance = dist; r.bySea = bySea;
                tradeRoutes.push_back(r);
            }
            ++totalRoutesOpened;
            logEvent(day, "A trade road opens between the " + A.name + " and the " + B.name
                     + (bySea ? " across the water" : ""), "trade",
                     "kind=route_opened tribeA=\"" + A.name + "\" tribeB=\"" + B.name + "\""
                     + " tribeAId=" + std::to_string(A.id)
                     + " tribeBId=" + std::to_string(B.id)
                     + " distance=" + std::to_string((int)dist)
                     + " bySea=" + std::to_string(bySea ? 1 : 0));
        }
    }

    // ── 3. Run the caravans ──────────────────────────────────────────────────
    // A caravan is slow: the longer the road, the fewer round trips it makes.
    for (TradeRoute& r : tradeRoutes) {
        if (!r.active) continue;
        int interval = 3 + (int)(r.distance / 120.0f);      // days between runs
        if (day - r.lastRunDay < interval) continue;

        Tribe* A = nullptr; Tribe* B = nullptr;
        for (Tribe& t : tribes) { if (t.id == r.a) A = &t; else if (t.id == r.b) B = &t; }
        if (!A || !B) { r.active = false; continue; }
        r.lastRunDay = day;
        // IV-P3: commerce is conducted in words. A caravan trading across an
        // opaque tongue moves less goods per trip — bargains are harder to
        // strike and easier to get wrong.
        const float tongue = 0.55f + 0.45f * mutualIntelligibility(*A, *B);

        // Goods flow from where they are cheap to where they are dear. Whoever
        // is short pays; whoever has a surplus sells. This is the whole engine.
        // `valueMoved` is what the cargo fetched at the dear end — the gross the
        // caravan is paid out of, and the reason trading is a living rather than
        // a hobby (a cut of the arbitrage margin alone is pocket change against
        // a farmer's yearly income, and would never build a merchant class).
        float valueMoved = 0.0f;
        auto haul = [&](float& sellerStock, float& buyerStock,
                        float& sellerPrice, float& buyerPrice, float sellerHas) {
            float gap = buyerPrice - sellerPrice;
            if (gap < 0.08f) return 0.0f;                  // not worth the walk
            float cargo = std::min(sellerHas * 0.15f, 12.0f) * tradeMul * tongue;
            if (cargo < 0.3f) return 0.0f;
            sellerStock -= cargo;
            buyerStock  += cargo * 0.92f;                  // a little spoils on the road
            valueMoved  += cargo * buyerPrice;
            // Arbitrage: carrying goods there closes part of the gap that paid
            // for the trip. Repeated runs keep linked prices near each other.
            float close = 0.25f * gap * tradeMul;
            sellerPrice += close;                          // scarcer at home now
            buyerPrice  -= close;                          // relieved here
            return cargo * gap;                            // the merchant's margin
        };

        float margin = 0.0f;
        if (A->priceFood < B->priceFood)
            margin += haul(A->granary, B->granary, A->priceFood, B->priceFood, A->granary);
        else
            margin += haul(B->granary, A->granary, B->priceFood, A->priceFood, B->granary);
        if (A->priceGoods < B->priceGoods)
            margin += haul(A->matStock, B->matStock, A->priceGoods, B->priceGoods, A->matStock);
        else
            margin += haul(B->matStock, A->matStock, B->priceGoods, A->priceGoods, B->matStock);

        if (margin <= 0.0f) continue;
        ++totalCaravans;
        r.volume += margin;

        // The cargo is real money, and it lands in the hands of the people who
        // carried it — which is how a merchant class comes to exist at all.
        // TOKENS_PER_UNIT converts a unit of hauled stock into the same currency
        // the market pays producers in, so a trader's season is comparable to a
        // farmer's rather than rounding to nothing beside it.
        // Anchored to the market's own price list, not tuned to taste: the food
        // CATALOG in Economics.cpp runs 9–175 tokens a unit (mushrooms to
        // venison), so a unit of hauled granary is worth a mid-priced good.
        constexpr float TOKENS_PER_UNIT = 50.0f;
        const float gross = valueMoved * TOKENS_PER_UNIT * tradeMul;
        for (Tribe* t : { A, B }) {
            t->tradeWealth += gross * 0.25f;
            t->luxuryStock += margin * 0.05f;
            std::vector<Entity*> traders;
            for (int mid : t->memberIds) {
                Entity* e = entityById(entities, mid);
                if (e && e->entityHealth > 0.0f && e->isSpecialist &&
                    (e->specialization == "trader" || e->specialization == "craftsman"))
                    traders.push_back(e);
            }
            if (traders.empty()) continue;
            float cut = gross * 0.5f / (float)traders.size();
            for (Entity* e : traders) {
                e->salary.earnMoney(cut);
                e->skills.practice(SK_ORATORY, 0.2f);   // haggling is a skill
            }
        }

        // ── 4. What else rides with the cargo ────────────────────────────────
        // Techniques travel on trade roads — this is the collective brain (II-P1)
        // moving across real ground instead of teleporting. And people who deal
        // with each other every season start to resemble each other.
        if (roll(rng) < 0.25f * tradeMul) {
            Tribe* from = (A->knownTechName.size() > B->knownTechName.size()) ? A : B;
            Tribe* to   = (from == A) ? B : A;
            for (const std::string& tech : from->knownTechName) {
                if (to->knownTechName.count(tech)) continue;
                to->knownTechName.insert(tech);
                ++totalTechSpreads;
                logEvent(day, "Merchants carry the art of " + tech + " from the "
                         + from->name + " to the " + to->name, "trade",
                         "kind=trade_diffusion tech=\"" + tech + "\""
                         + " fromId=" + std::to_string(from->id)
                         + " toId=" + std::to_string(to->id));
                break;   // one idea per caravan
            }
        }
        float pull = 0.004f * tradeMul;
        auto converge = [&](float& a, float& b) { float m = (a + b) * 0.5f;
                                                  a += (m - a) * pull; b += (m - b) * pull; };
        converge(A->innovation,   B->innovation);
        converge(A->collectivism, B->collectivism);

        if (A->relations.count(B->id)) {
            A->relations[B->id] = clamp(A->relations[B->id] + 0.4f * tradeMul, -100.0f, 100.0f);
            B->relations[A->id] = A->relations[B->id];
        }
    }

    // Retire long-dead routes so the list cannot grow without bound.
    if (tradeRoutes.size() > 400) {
        tradeRoutes.erase(std::remove_if(tradeRoutes.begin(), tradeRoutes.end(),
            [](const TradeRoute& r) { return !r.active; }), tradeRoutes.end());
    }
}

// ── I-P3: visible causal legacy (Parallel-Earth plan Track I) ────────────────
// The plan's first priority is that every entity be a person you could write a
// biography of, and a biography needs a last chapter: what did this life leave
// behind? Until now a death was a subtraction — the agent vanished and the world
// closed over the gap. This is the pass that makes a life *causal after it ends*.
//
// Three marks, in ascending permanence:
//   • the ledger — what they founded, invented, ruled or fathered, gathered by
//     asking the world what still carries their id;
//   • the inheritance — the standing others granted them is transferred, at a
//     discount, onto their children, so a great (or hated) name is a thing the
//     next generation is born holding (Bourdieu: reputation is capital, and
//     capital is inherited);
//   • the memorial — a notable death names the ground it happened on, so the
//     map itself remembers. "The ford where Kael drowned" outlives everyone who
//     saw it happen.
// Kill switch: legacyMul == 0 returns before anything is read or written.
void CivilizationEngine::recordLegacy(std::vector<Entity>& entities,
                                      const Entity& dead, int day) {
    const float legMul = g_liveConfig.legacyMul;
    if (legMul == 0.0f) return;   // director kill switch — bit-exact off

    Legacy leg;
    leg.entityId     = dead.entityId;
    leg.name         = dead.name;
    leg.familyId     = dead.familyId;
    leg.deathDay     = day;
    leg.lineageDepth = dead.lineageDepth;

    // ── 1. What does the world still carry their id on? ─────────────────────
    // Nothing here is bookkeeping invented for the occasion: every mark is an
    // existing authorship field finally being read back.
    for (const Innovation& inv : innovations)
        if (inv.discoveredByEntityId == dead.entityId) {
            leg.marks.push_back("invented " + inv.name);
            leg.weight += 3.0f;
        }
    for (const Religion& r : religions)
        if (r.founderEntityId == dead.entityId) {
            leg.marks.push_back("founded the faith of " + r.name);
            leg.weight += 4.0f;
        }
    for (const Tribe& t : tribes) {
        if (t.founderId == dead.entityId) {
            leg.marks.push_back("founded the " + t.name);
            leg.weight += 4.0f;
        }
        if (t.leaderId == dead.entityId) {
            leg.marks.push_back("led the " + t.name);
            leg.weight += 2.0f;
        }
    }
    for (const GreatWork& w : greatWorks)
        if (w.founderId == dead.entityId) {
            leg.marks.push_back("made " + w.name);
            leg.weight += 2.5f;
        }
    if (globalKinship) {
        if (const Family* fam = globalKinship->findFamily(dead.familyId)) {
            leg.familyName = fam->name;
            if (fam->founderId == dead.entityId) {
                leg.marks.push_back("founded " + fam->name);
                leg.weight += 2.0f;
            }
            if (fam->prominent) leg.weight += 1.5f;
        }
    }
    // Children are the commonest legacy of all, and the one most people get.
    for (int cid : dead.childrenIds) {
        const Entity* c = entityById(entities, cid);
        if (c && c->entityHealth > 0.0f) ++leg.descendants;
    }
    leg.weight += leg.descendants * 0.8f;
    // A long life of high standing counts for something even without monuments.
    leg.weight += std::max(0.0f, dead.auctoritas - 50.0f) * 0.04f;

    // ── 2. Inheritance of standing ──────────────────────────────────────────
    // Everyone who held an opinion of the deceased hands a fraction of it to
    // their children. This is why a feud or a good name outlives its owner —
    // the next generation inherits a world that has already made up its mind
    // about them.
    if (leg.descendants > 0) {
        for (Entity& other : entities) {
            if (other.entityHealth <= 0.0f || other.entityId == dead.entityId) continue;
            auto it = other.reputationMap.find(dead.entityId);
            if (it == other.reputationMap.end()) continue;
            const PerceivedReputation& src = it->second;
            // Only strong feelings are worth passing on.
            if (std::abs(src.positiveScore - src.negativeScore) < 10.0f) continue;
            for (int cid : dead.childrenIds) {
                Entity* child = entityById(entities, cid);
                if (!child || child->entityHealth <= 0.0f) continue;
                PerceivedReputation& dst = other.reputationMap[cid];
                dst.entityId = cid;
                float carry = 0.35f * legMul;
                dst.positiveScore += (src.positiveScore - 50.0f) * carry;
                dst.negativeScore += (src.negativeScore - 50.0f) * carry;
                dst.positiveScore = std::max(0.0f, std::min(100.0f, dst.positiveScore));
                dst.negativeScore = std::max(0.0f, std::min(100.0f, dst.negativeScore));
            }
        }
    }

    // ── 3. The mark on the ground ───────────────────────────────────────────
    // A life heavy enough to be remembered names the place it ended, and the
    // Chronicle records the whole account.
    const bool notable = leg.weight >= 5.0f;
    if (notable) {
        ++totalNotableLives;
        Memorial m;
        m.x = dead.posX; m.y = dead.posY;
        m.personName = dead.name;
        m.entityId   = dead.entityId;
        m.day        = day;
        m.weight     = leg.weight;
        // Name the ground for what happened on it.
        const char* place = "the place where ";
        if      (dead.entityDiseaseType >= 0) place = "the plague-ground where ";
        else if (dead.entityAge > 60.0f)      place = "the long home of ";
        else if (!leg.marks.empty())          place = "the memorial of ";
        m.placeName = std::string(place) + dead.name + " fell";
        memorials.push_back(m);
        ++totalMemorials;
        if (memorials.size() > 200) memorials.erase(memorials.begin());

        std::string account;
        for (size_t i = 0; i < leg.marks.size(); ++i)
            account += (i ? ", " : "") + leg.marks[i];
        if (account.empty()) account = "left " + std::to_string(leg.descendants) + " children";

        logEvent(day, dead.name + " of " + (leg.familyName.empty() ? "no house" : leg.familyName)
                 + " is dead. They " + account + ".", "legacy",
                 "kind=notable_life person=\"" + dead.name + "\""
                 + " entityId=" + std::to_string(dead.entityId)
                 + " family=\"" + leg.familyName + "\""
                 + " familyId=" + std::to_string(leg.familyId)
                 + " generation=" + std::to_string(leg.lineageDepth)
                 + " descendants=" + std::to_string(leg.descendants)
                 + " weight=" + std::to_string((int)leg.weight)
                 + " marks=" + std::to_string(leg.marks.size()));
    }

    if (leg.weight > 0.0f) {
        legacies.push_back(leg);
        if (legacies.size() > 400) legacies.erase(legacies.begin());
    }
}

// ── II-P2: institutions that store and transmit (Parallel-Earth plan Track II) ─
// The `InstitutionalSystem` scaffold sat in environment/EnvironmentModel.h with
// zero call sites. This is the wire-up, and the reason it matters: until now
// every scrap of knowledge in this world lived inside a skull. Techniques were
// held by individuals, so a hard winter that took the wrong three people took
// the craft with them — which is exactly how the flagship run managed 181 dark
// ages. An institution is the fix real history used: a thing that outlives its
// members and keeps holding what they knew.
//
// Three kinds do the work the plan asks for:
//   • EDUCATION (school/archive) — writes techniques down, so a collapse can
//     starve and depopulate a people without erasing what it learned, and
//     teaches the young far faster than the 6% oblique drip of casual imitation.
//   • ECONOMY (guild) — keeps a craft alive between masters and makes its
//     members better at it than lone practice ever would.
//   • GOVERNMENT (bureaucracy) — administrative capacity: the streets, stores,
//     records and offices that let strangers be governed together, so a people
//     can grow past the size a camp can hold before it fissions.
// FAMILY and RELIGION are cheap to found and mostly supply *integration* — the
// belonging that I-P1's sense of purpose is built from (Durkheim).
//
// Founding is earned, never granted: each kind has real preconditions, and an
// institution whose legitimacy collapses is wound up and its archive lost.
// Kill switch: institutionMul == 0 returns before any state is read or written.
void CivilizationEngine::updateInstitutions(std::vector<Entity>& entities, int day) {
    const float instMul = g_liveConfig.institutionMul;
    if (instMul == 0.0f) return;   // director kill switch — bit-exact off

    using environment::InstitutionType;
    auto clamp = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };

    // Institutions of a people that no longer exists die with it. Collect first,
    // then dissolve: tribes can be wiped out several at a time (war, famine),
    // and dissolving mid-iteration would invalidate the container.
    {
        std::set<int> liveTribes;
        for (const Tribe& t : tribes) liveTribes.insert(t.id);
        std::set<int> orphaned;
        for (const auto& inst : institutions.all())
            if (!liveTribes.count(inst->tribeId)) orphaned.insert(inst->tribeId);
        for (int dead : orphaned) institutions.dissolveTribe(dead);
    }

    for (Tribe& t : tribes) {
        // Census the people: who is alive, who is young enough to be taught,
        // and which trades are practised here.
        std::vector<Entity*> living, pupils;
        int scholars = 0, crafts = 0, priests = 0;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (!e || e->entityHealth <= 0.0f) continue;
            living.push_back(e);
            if (e->entityAge < 20.0f) pupils.push_back(e);
            if (!e->isSpecialist) continue;
            if      (e->specialization == "scholar")   ++scholars;
            else if (e->specialization == "craftsman") ++crafts;
            else if (e->specialization == "priest")    ++priests;
        }
        const int pop = (int)living.size();
        if (pop == 0) { institutions.dissolveTribe(t.id); continue; }

        // ── Founding ─────────────────────────────────────────────────────────
        // Each kind answers a question the people can only now afford to ask.
        auto found = [&](InstitutionType type, const char* label, const char* story) {
            if (institutions.find(t.id, type)) return;
            int id = institutions.createInstitution(type, t.name + " " + label, t.id, day);
            ++totalInstitutions;
            logEvent(day, "The " + t.name + " " + story, "institution",
                     std::string("kind=institution_founded institution=\"") + label + "\""
                     + " tribe=\"" + t.name + "\""
                     + " tribeId=" + std::to_string(t.id)
                     + " institutionId=" + std::to_string(id)
                     + " population=" + std::to_string(pop));
        };
        if (pop >= 4)
            found(InstitutionType::FAMILY, "kinship", "formalise the obligations of kin");
        if (t.dominantReligionId >= 0 && priests >= 1)
            found(InstitutionType::RELIGION, "priesthood", "ordain a standing priesthood");
        // A school needs writing to record with and a scholar to do the recording.
        if (scholars >= 1 && tribeIsLiterate(t))
            found(InstitutionType::EDUCATION, "school", "found a school, and begin to write down what they know");
        // A guild needs enough craftsmen that the trade outlives any one of them.
        if (crafts >= 2 && t.settlementTier >= 1)
            found(InstitutionType::ECONOMY, "guild", "charter a guild to keep the craft");
        // A bureaucracy needs a people too large to run by acquaintance.
        if (pop >= 12 && t.settlementTier >= 1 && era >= ERA_EARLY_AGRICULTURE)
            found(InstitutionType::GOVERNMENT, "bureau", "raise a standing administration");

        // ── Membership, efficiency, and what each kind actually does ─────────
        for (const auto& inst : institutions.all()) {
            if (inst->tribeId != t.id) continue;

            // Membership is rebuilt from the living each civ-day (people die,
            // and an institution is only ever the people currently in it).
            inst->memberIds.clear();
            for (Entity* e : living) {
                bool belongs = false;
                switch (inst->type) {
                    case InstitutionType::FAMILY:     belongs = true; break;
                    case InstitutionType::RELIGION:   belongs = (e->religionId == t.dominantReligionId); break;
                    case InstitutionType::EDUCATION:  belongs = (e->isSpecialist && e->specialization == "scholar")
                                                             || e->entityAge < 20.0f; break;
                    case InstitutionType::ECONOMY:    belongs = (e->isSpecialist && e->specialization == "craftsman"); break;
                    case InstitutionType::GOVERNMENT: belongs = (e->entityId == t.leaderId); break;
                    default: break;
                }
                if (belongs) inst->memberIds.push_back(e->entityId);
            }

            // Efficiency is earned from real conditions, and legitimacy follows
            // efficiency (people keep faith with institutions that work).
            float eff = 0.35f;
            switch (inst->type) {
                case InstitutionType::EDUCATION:
                    eff = clamp(0.25f + scholars * 0.20f + t.knowledgeStock * 0.002f, 0.0f, 1.0f); break;
                case InstitutionType::ECONOMY:
                    eff = clamp(0.25f + crafts * 0.15f + t.matStock * 0.002f, 0.0f, 1.0f); break;
                case InstitutionType::GOVERNMENT:
                    eff = clamp(t.govSatisfaction / 140.0f + t.settlementTier * 0.08f, 0.0f, 1.0f); break;
                case InstitutionType::RELIGION:
                    eff = clamp(0.30f + priests * 0.15f + t.spiritualism / 300.0f, 0.0f, 1.0f); break;
                default:  // FAMILY: kin obligation holds while the people is fed
                    eff = clamp(0.40f + std::min(0.4f, t.granary / std::max(1.0f, (float)pop) * 0.2f), 0.0f, 1.0f);
            }
            inst->efficiency = eff;
            inst->updateLegitimacy();

            // Starved of legitimacy, an institution is wound up — and whatever
            // only it remembered is lost with it.
            if (inst->legitimacy < 0.08f && day - inst->foundingDay > 60) {
                logEvent(day, "The " + inst->name + " is abandoned — nobody believes in it any more",
                         "institution",
                         "kind=institution_dissolved institution=\"" + inst->name + "\""
                         + " tribeId=" + std::to_string(t.id)
                         + " archived=" + std::to_string(inst->archive.size()));
                institutions.dissolve(inst->id);
                break;   // the container shifted; the rest waits for tomorrow
            }

            const float reach = inst->legitimacy * inst->efficiency * instMul;

            if (inst->type == InstitutionType::EDUCATION) {
                // 1. WRITE IT DOWN. Everything the people currently knows goes
                //    into the archive, where it no longer depends on anyone
                //    staying alive. This is the mechanism that turns the
                //    dark-age ratchet around (II-P1's other half).
                for (const std::string& tech : t.knownTechName) inst->archive.insert(tech);
                // 2. TEACH. A school compresses a lifetime of watching into
                //    lessons: pupils learn from the tribe's best hand at each
                //    craft far faster than the 6% oblique drip of imitation.
                if (!pupils.empty() && reach > 0.1f) {
                    for (int s = 0; s < SK_COUNT; ++s) {
                        Entity* best = nullptr;
                        for (Entity* e : living)
                            if (!best || e->skills.get((SkillId)s) > best->skills.get((SkillId)s)) best = e;
                        if (!best || best->skills.get((SkillId)s) < 20.0f) continue;
                        for (Entity* p : pupils) {
                            if (p == best) continue;
                            // Schooling ≈ 3 extra passes of oblique transmission
                            // per civ-day, scaled by how good the school is.
                            int lessons = 1 + (int)(2.0f * reach);
                            for (int k = 0; k < lessons; ++k)
                                p->skills.learnFrom(best->skills, (SkillId)s);
                        }
                    }
                }
            } else if (inst->type == InstitutionType::ECONOMY) {
                // A guild holds the craft itself: its members' techniques are
                // archived, and belonging makes a craftsman measurably better.
                for (const std::string& tech : t.knownTechName)
                    if (tech == "Metal Working" || tech == "Iron Smelting" ||
                        tech == "Pottery" || tech == "Masonry" || tech == "Weaving")
                        inst->archive.insert(tech);
                for (int mid : inst->memberIds) {
                    Entity* e = entityById(entities, mid);
                    if (e) e->skills.practice(SK_CRAFT, 0.30f * reach);
                }
                t.matStock += crafts * 0.5f * reach;   // organised work yields more
            } else if (inst->type == InstitutionType::GOVERNMENT) {
                // A working administration steadies a people: it collects, it
                // records, it is harder to rob blind. (adminCapacity() reads
                // this same product for the fission threshold.)
                t.govSatisfaction = clamp(t.govSatisfaction + 0.15f * reach, 0.0f, 100.0f);
            }

            // Belonging to something that works is *integration* — the Durkheim
            // half of I-P1's sense of purpose. This is why a person in a living
            // society resists despair better than one in a dissolving one.
            if (reach > 0.0f) {
                for (int mid : inst->memberIds) {
                    Entity* e = entityById(entities, mid);
                    if (!e) continue;
                    e->senseOfPurpose = clamp(e->senseOfPurpose + 0.08f * reach, 0.0f, 100.0f);
                }
            }
        }
    }
}

// II-P2: how much a people's bureaucracy extends the reach of its government.
// 0 when it has none (or the feature is off), up to ~1 for a legitimate,
// effective administration.
float CivilizationEngine::adminCapacity(int tribeId) const {
    if (g_liveConfig.institutionMul == 0.0f) return 0.0f;
    const environment::Institution* gov =
        institutions.find(tribeId, environment::InstitutionType::GOVERNMENT);
    if (!gov) return 0.0f;
    return gov->legitimacy * gov->efficiency * g_liveConfig.institutionMul;
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

    // ── I-P3: dynastic through-lines ────────────────────────────────────────
    // A bloodline that keeps going is itself an event. Announcing the depth a
    // house has reached gives the Chronicle the spine of a saga: the same name,
    // generations apart, with the founder still attached to it. Announced once
    // per milestone (a house cannot get shallower).
    if (g_liveConfig.legacyMul != 0.0f) {
        for (auto& fam : globalKinship->families) {
            if (fam.generation < 3) continue;
            // `births` is reused as the announced-depth watermark's companion:
            // we announce at 3, 5, 7 … and only when this is the deepest yet.
            if (fam.generation % 2 == 0) continue;              // odd milestones only
            if (fam.generation <= fam.announcedGeneration) continue;
            fam.announcedGeneration = fam.generation;

            int living = 0;
            for (const Entity& e : entities)
                if (e.entityHealth > 0.0f && e.familyId == fam.id) ++living;
            if (living == 0) continue;   // a line that has already ended

            // Name the founder the house still descends from, and what they did.
            std::string founderNote;
            for (const Legacy& l : legacies)
                if (l.entityId == fam.founderId && !l.marks.empty()) {
                    founderNote = " — founded by " + l.name + ", who " + l.marks[0];
                    break;
                }
            logEvent(day, "The line of " + fam.name + " has run "
                     + std::to_string(fam.generation) + " generations"
                     + founderNote, "dynasty",
                     "kind=lineage_depth family=\"" + fam.name + "\""
                     + " familyId="   + std::to_string(fam.id)
                     + " generation=" + std::to_string(fam.generation)
                     + " founderId="  + std::to_string(fam.founderId)
                     + " living="     + std::to_string(living)
                     + " prestige="   + std::to_string((int)fam.prestige));
        }
    }

    // ── I-P3: the ground remembers ──────────────────────────────────────────
    // Passing where someone notable died is not nothing. The recent memorials
    // (the ones still in living memory) deepen the attachment of anyone who
    // lives beside them, and give the passer-by a moment of meaning — the
    // mechanism by which a place becomes *a place* rather than coordinates.
    if (g_liveConfig.legacyMul != 0.0f && !memorials.empty()) {
        const size_t look = std::min<size_t>(memorials.size(), 24);   // most recent
        for (Entity& e : entities) {
            if (e.entityHealth <= 0.0f) continue;
            for (size_t k = memorials.size() - look; k < memorials.size(); ++k) {
                const Memorial& m = memorials[k];
                if (day - m.day > 900) continue;         // beyond living memory
                float dx = e.posX - m.x, dy = e.posY - m.y;
                if (dx * dx + dy * dy > 90.0f * 90.0f) continue;
                float pull = 0.05f * std::min(3.0f, m.weight / 5.0f) * g_liveConfig.legacyMul;
                e.homeAttachment  = std::min(100.0f, e.homeAttachment + pull);
                e.senseOfPurpose  = std::min(100.0f, e.senseOfPurpose + pull * 0.5f);
                break;   // one memorial's worth of weight per person per day
            }
        }
    }
}

// ── Elections & councils (Society Plan 3) ──────────────────────────────────────
// Every regime seats a council of three notables — the crowd each government
// form actually listens to — whose mood steadies or shakes the ruler and who
// steer the tax rate toward what the people will bear. Democracies go further:
// on a fixed term every member casts a real ballot, scored by personal opinion,
// standing, blood and the incumbent's record, and unpopular rulers actually lose.
void CivilizationEngine::updateElections(std::vector<Entity>& entities, int day) {
    auto clampf = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    // A member's net opinion of another, from their private reputation ledger.
    auto opinionOf = [](Entity* voter, int aboutId) -> float {
        auto it = voter->reputationMap.find(aboutId);
        if (it == voter->reputationMap.end()) return 0.0f;
        return it->second.positiveScore - it->second.negativeScore;
    };

    for (Tribe& t : tribes) {
        if (t.population() < 3) { t.councilIds.clear(); continue; }

        std::vector<Entity*> members;
        members.reserve(t.memberIds.size());
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f && e->entityAge >= 16.0f) members.push_back(e);
        }
        if (members.size() < 3) { t.councilIds.clear(); continue; }

        // 1. Seat the council: each regime elevates its own kind of notable.
        auto councilVirtue = [&](Entity* e) -> float {
            switch (t.government) {
                case GOV_OLIGARCHY:       return e->salary.token;
                case GOV_DIVINE_MONARCHY: return e->ValueSystem.spiritualNeed;
                case GOV_AUTHORITARIAN:   return e->dominanceRank;
                default: /* DEMOCRACY */  return e->auctoritas;
            }
        };
        std::vector<Entity*> notables;
        for (Entity* e : members) if (e->entityId != t.leaderId) notables.push_back(e);
        std::sort(notables.begin(), notables.end(), [&](Entity* a, Entity* b) {
            return councilVirtue(a) > councilVirtue(b);
        });
        t.councilIds.clear();
        for (size_t i = 0; i < notables.size() && i < 3; ++i)
            t.councilIds.push_back(notables[i]->entityId);

        // 2. The council's mood steadies or shakes the ruler.
        if (!t.councilIds.empty() && t.leaderId >= 0) {
            float mood = 0.0f;
            for (int cid : t.councilIds) {
                Entity* c = entityById(entities, cid);
                if (c) mood += opinionOf(c, t.leaderId);
            }
            mood /= (float)t.councilIds.size();
            t.govSatisfaction = clampf(t.govSatisfaction + clampf(mood / 25.0f, -2.0f, 2.0f),
                                       0.0f, 100.0f);
        }

        // 3. The council steers taxation toward what the regime can extract.
        float taxTarget;
        switch (t.government) {
            case GOV_AUTHORITARIAN:   taxTarget = 0.20f; break;
            case GOV_OLIGARCHY:       taxTarget = 0.15f; break;
            case GOV_DIVINE_MONARCHY: taxTarget = 0.10f; break;
            default: /* DEMOCRACY */  taxTarget = 0.05f; break;
        }
        if      (t.taxeRate < taxTarget) t.taxeRate = std::min(taxTarget, t.taxeRate + 0.01f);
        else if (t.taxeRate > taxTarget) t.taxeRate = std::max(taxTarget, t.taxeRate - 0.01f);
        t.taxeRate = clampf(t.taxeRate, 0.0f, 0.30f);

        // 4. With a rate finally set, the tax take is actually collected — the
        //    treasury the leader draws a stipend from, and may steal from.
        collectTaxes(t, entities);

        // 5. Democracies ballot on a fixed term (or a snap election after a
        //    death or scandal); other regimes never ask the people at all.
        if (t.government != GOV_DEMOCRACY) { t.nextElectionDay = -1; continue; }
        if (t.nextElectionDay < 0) { t.nextElectionDay = day + t.termLengthDays; continue; }
        if (day < t.nextElectionDay) continue;

        // Candidates: the incumbent defends the seat against the most respected.
        std::vector<Entity*> candidates;
        Entity* incumbent = entityById(entities, t.leaderId);
        if (incumbent && incumbent->entityHealth > 0.0f) candidates.push_back(incumbent);
        std::sort(members.begin(), members.end(), [](Entity* a, Entity* b) {
            return a->auctoritas > b->auctoritas;
        });
        for (Entity* e : members) {
            if (candidates.size() >= 5) break;
            if (e != incumbent) candidates.push_back(e);
        }
        if (candidates.size() < 2) { t.nextElectionDay = day + t.termLengthDays; continue; }

        // Every member votes: private opinion, public standing, blood, dynastic
        // glamour — and the incumbent answers for the state of the nation.
        std::map<int, int> votes;
        std::uniform_real_distribution<float> jitter(0.0f, 5.0f);
        for (Entity* voter : members) {
            Entity* pick = nullptr; float bestScore = -1e9f;
            for (Entity* cand : candidates) {
                float score = opinionOf(voter, cand->entityId)
                            + cand->auctoritas * 0.3f
                            + (voter->familyId >= 0 && voter->familyId == cand->familyId ? 15.0f : 0.0f)
                            + jitter(rng);
                if (globalKinship && cand->familyId >= 0) {
                    Family* fam = globalKinship->findFamily(cand->familyId);
                    if (fam) score += fam->prestige * 0.1f;
                }
                if (cand == incumbent) score += (t.govSatisfaction - 60.0f) * 0.5f;
                if (score > bestScore) { bestScore = score; pick = cand; }
            }
            if (pick) votes[pick->entityId]++;
        }

        int winnerId = -1, winnerVotes = -1, turnout = 0;
        for (const auto& kv : votes) {
            turnout += kv.second;
            if (kv.second > winnerVotes) { winnerVotes = kv.second; winnerId = kv.first; }
        }
        if (winnerId < 0 || turnout <= 0) { t.nextElectionDay = day + t.termLengthDays; continue; }

        Entity* winner = entityById(entities, winnerId);
        bool upset = (winnerId != t.leaderId);
        t.leaderId           = winnerId;
        t.lastElectionMargin = (float)winnerVotes / (float)turnout;
        t.govSatisfaction    = clampf(t.govSatisfaction + 10.0f * t.lastElectionMargin, 0.0f, 100.0f);
        t.nextElectionDay    = day + t.termLengthDays;
        totalElections++;
        if (winner) {
            winner->auctoritas = std::min(100.0f, winner->auctoritas + 3.0f);
            logEvent(day, t.name + ": " + winner->name
                          + (upset ? " unseats the leadership, winning " : " is re-elected with ")
                          + std::to_string((int)(t.lastElectionMargin * 100.0f)) + "% of "
                          + std::to_string(turnout) + " votes", "tribe",
                     "kind=election tribe=\"" + t.name + "\" tribeId=" + std::to_string(t.id)
                     + " winner=\"" + winner->name + "\" winnerId=" + std::to_string(winnerId)
                     + " margin=" + std::to_string(t.lastElectionMargin)
                     + " turnout=" + std::to_string(turnout)
                     + " upset=" + (upset ? "1" : "0"));
        }
    }
}

// ── Corruption: pressure, detection, scandal (Society Plan 5) ──────────────────
// Graft banked by collectTaxes and nepotism doesn't stay free: the people feel
// the missing granary money every day, and sooner or later — soonest where rule
// is transparent and scholars keep records — the books are opened. A scandal
// craters the ruler's name, feeds the election/coup machinery, and the worst
// offenders are driven out of the tribe altogether.
void CivilizationEngine::updateCorruption(std::vector<Entity>& entities, int day) {
    auto clampf = [](float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); };
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    for (Tribe& t : tribes) {
        if (t.population() == 0) { t.corruption *= 0.9f; continue; }
        t.corruption = std::max(0.0f, t.corruption - 0.3f);   // clean rule is forgotten
        if (t.corruption <= 0.5f) continue;

        // The people feel the theft long before they can prove it.
        t.govSatisfaction = clampf(t.govSatisfaction - t.corruption / 50.0f, 0.0f, 100.0f);

        // Petty graft stays beneath notice; only real plunder risks the books.
        if (t.corruption < 10.0f) continue;

        Entity* leader = entityById(entities, t.leaderId);
        if (!leader || leader->entityHealth <= 0.0f) continue;

        float oversight;
        switch (t.government) {
            case GOV_DEMOCRACY:       oversight = 0.8f; break;
            case GOV_OLIGARCHY:       oversight = 0.4f; break;
            case GOV_DIVINE_MONARCHY: oversight = 0.3f; break;
            default: /* AUTHORITARIAN */ oversight = 0.2f; break;
        }
        int scholars = 0;
        for (int mid : t.memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f && e->specialization == "scholar") scholars++;
        }
        float exposure = (t.corruption / 100.0f) * oversight
                       * (1.0f + 0.1f * scholars) * 0.25f * g_liveConfig.corruptionMul;
        if (roll(rng) >= exposure) continue;

        // ── Scandal: the books are opened ────────────────────────────────────
        bool severe = t.corruption > 70.0f;
        totalScandals++;
        logEvent(day, "SCANDAL in the " + t.name + ": " + leader->name
                      + " is exposed for plundering the common stores", "tribe",
                 "kind=scandal tribe=\"" + t.name + "\" tribeId=" + std::to_string(t.id)
                 + " leader=\"" + leader->name + "\" leaderId=" + std::to_string(leader->entityId)
                 + " graft=" + std::to_string((int)t.corruption)
                 + " severe=" + (severe ? "1" : "0"));

        // The ruler's name collapses in every household ledger.
        int shamed = 0;
        for (int mid : t.memberIds) {
            if (shamed >= 20) break;
            Entity* w = entityById(entities, mid);
            if (!w || w->entityHealth <= 0.0f || w->entityId == leader->entityId) continue;
            auto& rep = w->reputationMap[leader->entityId];
            rep.negativeScore   = clampf(rep.negativeScore + 30.0f, 0.0f, 100.0f);
            rep.trustworthiness = clampf(rep.trustworthiness - 30.0f, 0.0f, 100.0f);
            shamed++;
        }
        if (globalKinship && leader->familyId >= 0)
            globalKinship->adjustReputation(leader->familyId, -10.0f);

        t.govSatisfaction = clampf(t.govSatisfaction - (15.0f + t.corruption / 5.0f), 0.0f, 100.0f);
        t.corruption *= 0.5f;

        if (severe) {
            // Driven out of the tribe entirely — the exile pattern.
            totalDepositions++;
            t.memberIds.erase(std::remove(t.memberIds.begin(), t.memberIds.end(),
                                          leader->entityId), t.memberIds.end());
            leader->tribeId     = -1;
            leader->entityStress = clampf(leader->entityStress + 25.0f, 0.0f, 100.0f);
            leader->Esteem       = clampf(leader->Esteem - 30.0f, 0.0f, 100.0f);
            t.leaderId = -1;
            logEvent(day, leader->name + " is cast out of the " + t.name
                          + " in disgrace", "tribe",
                     "kind=exile reason=\"corruption\" entity=\"" + leader->name + "\""
                     + " entityId=" + std::to_string(leader->entityId)
                     + " tribe=\"" + t.name + "\" tribeId=" + std::to_string(t.id));
            electLeader(t, entities, day);   // the succession machinery fills the seat
        } else if (t.government == GOV_DEMOCRACY) {
            t.nextElectionDay = day;         // a snap election — let the people answer
        }
        // Otherwise the cratered satisfaction feeds the existing coup machinery.
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

        // II-P4: imperial overstretch. A conqueror can hold only as many peoples
        // as it can actually administer — that reach is the bureaucracy of II-P2
        // plus the streets and stores of its settlement (III-P1), not its army.
        // Past that limit the far provinces slip, which is why empires assembled
        // by conquest come apart from the edges instead of growing for ever.
        if (g_liveConfig.warMul != 0.0f) {
            int held = 0;
            for (const Tribe& v : tribes) if (v.overlordTribeId == over->id) ++held;
            float reach = 1.0f + 2.5f * adminCapacity(over->id) + 0.5f * over->settlementTier;
            if ((float)held > reach)
                chance += 0.05f * ((float)held - reach) * g_liveConfig.warMul;
        }
        // II-P3: provinces read the centre. An overlord holding itself together
        // is obeyed; one visibly tearing at its own elite invites the edges to
        // try their luck — which is how empires come apart from the periphery
        // during the disintegrative phase of a cycle, and hold during the
        // integrative one.
        if (g_liveConfig.cycleMul != 0.0f) {
            float centre = over->instability;
            if (centre > 50.0f) chance += 0.04f * ((centre - 50.0f) / 50.0f) * g_liveConfig.cycleMul;
            else                chance *= (1.0f - 0.5f * (50.0f - centre) / 50.0f * g_liveConfig.cycleMul);
        }
        if (chance > 0.0f && roll(rng) < chance)
            rebelAgainstOverlord(t, *over, entities, day);
    }

    // ── II-P4: empires rise and fall ─────────────────────────────────────────
    // A people that holds three or more others is no longer a tribe with
    // clients; it is an empire, and the Chronicle should say so — once when it
    // is assembled and once when it comes apart. This is the grand rhythm the
    // plan asks for, and it is *earned* by conquest and *kept* by administration.
    if (g_liveConfig.warMul != 0.0f) {
        for (Tribe& t : tribes) {
            int held = 0;
            for (const Tribe& v : tribes) if (v.overlordTribeId == t.id) ++held;
            const bool isEmpire = (held >= 3);
            if (isEmpire && !t.wasEmpire) {
                t.wasEmpire = true;
                ++totalEmpires;
                logEvent(day, "An empire is born: the " + t.name + " now rule "
                         + std::to_string(held) + " peoples", "war",
                         "kind=empire_risen tribe=\"" + t.name + "\""
                         + " tribeId=" + std::to_string(t.id)
                         + " vassals=" + std::to_string(held)
                         + " adminCapacity=" + std::to_string(adminCapacity(t.id)));
            } else if (!isEmpire && t.wasEmpire) {
                t.wasEmpire = false;
                ++totalEmpiresFallen;
                logEvent(day, "The empire of the " + t.name + " has come apart", "war",
                         "kind=empire_fallen tribe=\"" + t.name + "\""
                         + " tribeId=" + std::to_string(t.id)
                         + " vassals=" + std::to_string(held));
            }
        }
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
    ++warsByReason[(int)WAR_TRIBUTE];   // §8: a rebellion is its own kind of war

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

            // How often a war comes to a pitched battle. The old model fought
            // one every five days and killed almost nobody in each (the
            // performative war of F2). II-P4 made every battle cost lives —
            // and at that frequency it emptied the world: over a 3000-day run
            // 52% of ALL deaths were battle deaths and the population fell from
            // 217 to 19, which is not a war-torn history, it is an extinction.
            // Real wars are mostly waiting: campaign seasons, marches, sieges
            // and standoffs, punctuated by a handful of engagements. So battles
            // are rare and bloody rather than constant and bloody, and a people
            // that has been bled white stops offering battle at all — which is
            // what war exhaustion is for.
            float battleOdds;
            if (g_liveConfig.warMul == 0.0f) {
                battleOdds = ethnic ? 0.34f : 0.20f;   // pre-II-P4, bit-exact
            } else {
                battleOdds = (ethnic ? 0.075f : 0.045f)
                           * (1.0f - 0.6f * std::max(A.warExhaustion, B.warExhaustion) / 100.0f);
            }
            if (roll(rng) < battleOdds) {
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
                    if (before > 0.0f && e->entityHealth <= 0.0f) {
                        totalWarDeaths++;
                    }
#if 0
                    else if (before > 0.0f) {
                        if (roll(rng) < 0.05f)
                            e->addEpigeneticMarker("war", (ethnic ? 55.0f : 35.0f), 0);
                    }
#endif
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
    // Populations at the moment the fighting starts — a loss only means
    // something relative to how many there were to lose.
    const int popABefore = attacker.population();
    const int popBBefore = defender.population();
    auto killFrac = [&](Tribe& side, float frac, bool isDefender) {
        if (frac <= 0.0f) return;
        if (isDefender) frac *= (1.0f - std::min(0.5f, fort / 200.0f)); // walls shelter the defenders
        std::vector<Entity*> alive;
        for (int mid : side.memberIds) {
            Entity* e = entityById(entities, mid);
            if (e && e->entityHealth > 0.0f) alive.push_back(e);
        }
        // II-P4 (fixes F2, "war is performative"): casualties used to be
        // std::round()ed, which silently truncated every small battle to zero
        // dead. A skirmish costs the loser 4% of its strength — but 4% of a
        // twelve-person band is 0.48, and round(0.48) = 0. Since almost every
        // tribe in this world is under twenty-five people, the overwhelming
        // majority of engagements were mathematically incapable of killing
        // anyone, which is exactly the bloodless ritual war the flagship report
        // found (1,157 battles, 11 deaths). Rounding stochastically keeps the
        // expected loss identical while letting a small fight actually cost a
        // life — the fractional part becomes the chance of one more casualty.
        float expected = alive.size() * frac;
        int   kills;
        if (g_liveConfig.warMul == 0.0f) {
            kills = (int)std::round(expected);   // pre-II-P4 behaviour, bit-exact
        } else {
            kills = (int)expected;
            if (roll(rng) < (expected - (float)kills)) ++kills;
            // Beaten men run. A rout is how nearly every pre-modern battle
            // actually ended — the losing side breaks and the killing stops,
            // because pursuit is dangerous and captives are worth more than
            // corpses. Without this floor a band of eight could be scrubbed off
            // the map in an afternoon, and the world lost peoples faster than
            // it could make them (tribes 21 → 1 over a long run). A people can
            // still be destroyed — but by conquest and absorption, which leaves
            // its members alive under a new name, not by attrition to nothing.
            const int survivors = (int)alive.size() - kills;
            const int floorLeft = std::max(3, (int)(alive.size() * 0.5f));
            if (survivors < floorLeft) kills = std::max(0, (int)alive.size() - floorLeft);
        }
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

    // II-P4: the bereaved people remembers who spilled its blood. The heavier the
    // toll (and the more hateful the war), the deeper the grievance — carried for
    // generations by the durable ledger, not the forgiving `relations` scalar.
    if (g_liveConfig.feudMul != 0.0f && fallen > 0) {
        Tribe* loserT  = (outcome == "attacker_victory") ? &defender
                        : (outcome == "defender_victory") ? &attacker : nullptr;
        Tribe* victorT = (outcome == "attacker_victory") ? &attacker
                        : (outcome == "defender_victory") ? &defender : nullptr;
        // Grief is PROPORTIONAL, not absolute. Losing three people out of twelve
        // guts a village and is remembered for generations; losing three out of
        // three hundred is a bad afternoon. Counting bodies alone (the first cut
        // of this ledger) meant grievance never came near the threshold a
        // vendetta needs, so blood feuds — the whole point of a durable ledger —
        // could not start a war in practice. Scale by the share of the people
        // that was killed and a devastating defeat lands where it should.
        Tribe* bereaved = loserT ? loserT : &attacker;
        int    popBefore = (bereaved == &attacker) ? popABefore : popBBefore;
        float  share = (popBefore > 0) ? (float)fallen / (float)popBefore : 0.0f;
        float g = std::min(60.0f, 100.0f * share * 1.5f * (ethnic ? 1.5f : 1.0f))
                  * g_liveConfig.feudMul;
        if (loserT && victorT)
            loserT->grievance[victorT->id] = std::min(100.0f, loserT->grievance[victorT->id] + g);
        else {   // a bloody stalemate wounds both sides
            attacker.grievance[defender.id] = std::min(100.0f, attacker.grievance[defender.id] + g * 0.5f);
            defender.grievance[attacker.id] = std::min(100.0f, defender.grievance[attacker.id] + g * 0.5f);
        }
    }

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
    float mult = 1.0f + 0.6f * (float)agTechs;   // each agri tech raises capacity

    // II-P1: how many people a stretch of land can feed is the ceiling on the
    // collective brain, and it was reading only five techniques out of the
    // emergent catalogue — so a people that had researched Agriculture,
    // Irrigation and Animal Husbandry the deliberate way fed no more mouths
    // than one that had not, and every world stayed Malthusian at a few
    // hundred souls no matter what it knew. A population that cannot grow
    // cannot build cities, and a world without cities never gets a scholar
    // class, which is the whole road to modernity. So the tree's food
    // techniques count, and the two later techniques that historically moved
    // the ceiling most — clean water under a dense settlement, and mechanical
    // power in the fields — count as well.
    if (g_liveConfig.knowledgeMul != 0.0f) {
        float best = 1.0f;
        for (const Tribe& t : tribes) {
            // Which region a people farms is where its settlement stands — read
            // off the centre tile, not by walking every member (this is called
            // for the capacity panel on every tick, not just civ-days).
            const Tile* home = g_planet ? g_planet->tileAtWorld(t.centerX, t.centerY) : nullptr;
            if (!home || home->regionId != regionId) continue;
            float m = TechTreeSystem::foodMultiplier(t);
            if (t.knownTechName.count("Sanitation"))  m *= 1.25f;
            if (t.knownTechName.count("Steam Power")) m *= 1.40f;
            best = std::max(best, m);
        }
        mult *= best;
    }
    return mult;
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
    // II-P1: a literate institution ARCHIVES knowledge. If any surviving tribe
    // that knows a tech also has Writing, that tech is recorded and cannot be
    // lost to a mere collapse — the single change that converts the endless
    // era-regression (181 dark ages in the flagship run) into a ratchet.
    auto literateKnows = [&](const std::string& nm) {
        if (g_liveConfig.knowledgeMul == 0.0f) return false;
        for (const auto& t : tribes)
            if (tribeIsLiterate(t) && t.knownTechName.count(nm)) return true;
        return false;
    };
    std::vector<int> fragile;
    for (size_t i = 0; i < innovations.size(); ++i) {
        const auto& inv = innovations[i];
        if (inv.knowerCount <= 2 && inv.complexity > 45.0f) {
            if (literateKnows(inv.name)) continue;   // archived by a literate people
            // II-P2: and even if every living knower is gone, a school or guild
            // that wrote the technique down still holds it. This is the strong
            // form of the knowledge ratchet: survival no longer depends on the
            // right individuals living through the winter.
            if (institutions.archiveHolds(inv.name)) {
                ++totalArchiveSaves;
                logEvent(day, "The archives preserve " + inv.name
                         + " through the collapse of " + regionName, "institution",
                         "kind=archive_save tech=\"" + inv.name + "\""
                         + " region=\"" + regionName + "\""
                         + " totalSaves=" + std::to_string(totalArchiveSaves));
                continue;
            }
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
#if 0
                if (severity > 0.35f) {
                    std::uniform_real_distribution<float> markRoll(0.0f, 1.0f);
                    if (markRoll(rng) < 0.08f)
                        e.addEpigeneticMarker("famine", severity * 80.0f, 0);
                }
#endif
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
#if 0
                    int seeded = 0;
                    for (Entity& e : entities) {
                        if (seeded >= 5) break;
                        if (e.entityHealth <= 0.0f) continue;
                        const Tile* t = g_planet->tileAtWorld(e.posX, e.posY);
                        int erid = (t && t->regionId >= 0) ? t->regionId : e.originRegionId;
                        if (erid != rid) continue;
                        e.exposeToPathogen(1, day / 60);
                        seeded++;
                    }
#endif
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
    if (g_lexicon && victor.regionId >= 0 && loser.regionId >= 0) {
        g_lexicon->blend(victor.regionId, loser.regionId, 0.25f);
        // IV-P3: conquest imposes a PRESTIGE language. The flow is not
        // symmetric — the victor's speech is the one that opens doors, so it
        // presses into the conquered region far harder than the reverse, and a
        // subject people's tongue blends toward its masters' over generations.
        // (Verification: the conquered region's intelligibility with the
        // victor's rises after conquest.)
        if (g_liveConfig.languageMul != 0.0f && loser.languageId >= 0 && victor.languageId >= 0) {
            float before = g_lexicon->intelligibility(loser.languageId, victor.languageId);
            g_lexicon->blend(loser.languageId, victor.languageId, 0.55f * g_liveConfig.languageMul);
            float after  = g_lexicon->intelligibility(loser.languageId, victor.languageId);
            logEvent(day, "The speech of " + victor.name + " spreads among the conquered",
                     "language",
                     "kind=prestige_language victor=\"" + victor.name + "\""
                     + " conqueredLang=" + std::to_string(loser.languageId)
                     + " victorLang=" + std::to_string(victor.languageId)
                     + " before=" + std::to_string(before)
                     + " after=" + std::to_string(after));
        }
    }

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
    ss << "\n--- Governance & society ---\n";
    ss << "Elections held: " << totalElections
       << "   successions: " << totalSuccessions << "\n";
    ss << "Leadership challenges: " << totalChallenges
       << "   coups: " << totalCoups << "   rebellions: " << totalRebellions << "\n";
    ss << "Corruption scandals: " << totalScandals
       << "   leaders exiled in disgrace: " << totalDepositions << "\n";
    // Mean live graft under each form of rule — transparency should show here.
    {
        float sum[4] = {0,0,0,0}; int cnt[4] = {0,0,0,0};
        for (const auto& t : tribes) {
            if (t.population() == 0) continue;
            sum[(int)t.government] += t.corruption; cnt[(int)t.government]++;
        }
        ss << "Avg corruption : ";
        for (int g = 0; g < 4; ++g)
            if (cnt[g] > 0)
                ss << governmentName((GovernmentType)g) << " "
                   << (int)(sum[g] / cnt[g]) << "  ";
        ss << "\n";
    }
    // ── IV-P1: the cultures themselves, in words ────────────────────────────
    // A number saying "culture: 62" tells a reader nothing. What their ways
    // actually are, and how far the biggest peoples have drifted from one
    // another, is the thing worth reading.
    if (g_liveConfig.traitMul != 0.0f) {
        std::vector<const Tribe*> byPop;
        for (const auto& t : tribes)
            if (t.population() > 0 && t.cultureTraits != 0ull) byPop.push_back(&t);
        std::sort(byPop.begin(), byPop.end(),
                  [](const Tribe* a, const Tribe* b) { return a->population() > b->population(); });
        if (!byPop.empty()) {
            ss << "\n--- Cultures ---\n";
            ss << "Traits invented: " << totalTraitsInvented
               << "   cascades: " << totalCascades
               << "   died out: " << totalFizzles
               << "   carried abroad: " << totalTraitsDiffused << "\n";
            for (size_t i = 0; i < byPop.size() && i < 4; ++i) {
                std::string ways;
                unsigned long long set = byPop[i]->cultureTraits;
                int shown = 0;
                while (set && shown < 6) {
                    unsigned long long low = set & (~set + 1ull);
                    int id = 0;
                    while ((low >> id) != 1ull) ++id;
                    ways += (ways.empty() ? "" : ", ") + culture.trait(id).name;
                    set &= set - 1ull;
                    ++shown;
                }
                if (set) ways += ", …";
                ss << byPop[i]->name << ": " << ways << "\n";
            }
            if (byPop.size() >= 2)
                ss << "Distance between the two largest: "
                   << (int)(environment::CulturalTransmissionSystem::distance(
                                byPop[0]->cultureTraits, byPop[1]->cultureTraits) * 100.0f)
                   << "%\n";
        }
    }

    // ── §8: the evidence of realism, rendered where a reader can see it ──────
    // The end-of-run report proves these to a machine; this proves them to a
    // person reading the panel, which is the other half of the requirement.
    ss << "\n--- The shape of this world ---\n";
    {
        // Rank-size: the settlement hierarchy, largest first. A world that grew
        // its cities properly reads roughly halving down the list (Zipf).
        std::vector<int> sizes;
        for (const auto& t : tribes) if (t.population() > 0) sizes.push_back(t.population());
        std::sort(sizes.begin(), sizes.end(), std::greater<int>());
        if (!sizes.empty()) {
            ss << "Settlements   : ";
            for (size_t i = 0; i < sizes.size() && i < 8; ++i)
                ss << (i ? " · " : "") << sizes[i];
            if (sizes.size() > 8) ss << " · …";
            ss << "   (largest " << sizes[0] << ")\n";
        }
        // The secular cycle as a line you can actually see turning: wellbeing
        // against political stress, sampled across the run.
        if (!cycleHistory.empty()) {
            static const char* kBars = " .:-=+*#%@";
            std::string well, inst;
            const size_t step = std::max<size_t>(1, cycleHistory.size() / 40);
            for (size_t i = 0; i < cycleHistory.size(); i += step) {
                int w = (int)(cycleHistory[i].wellbeing / 10.1f);
                int s = (int)(cycleHistory[i].instability / 10.1f);
                well += kBars[std::max(0, std::min(9, w))];
                inst += kBars[std::max(0, std::min(9, s))];
            }
            ss << "Wellbeing     : " << well << "\n";
            ss << "Instability   : " << inst << "   (strifes " << totalStrifes << ")\n";
        }
        // The deepest standing blood debt, which is what a feud looks like as a
        // number, and who owes it to whom.
        float worstFeud = 0.0f; std::string feudA, feudB;
        for (const auto& t : tribes)
            for (const auto& g : t.grievance)
                if (g.second > worstFeud) {
                    worstFeud = g.second; feudA = t.name;
                    for (const auto& o : tribes) if (o.id == g.first) { feudB = o.name; break; }
                }
        if (worstFeud > 0.0f)
            ss << "Deepest feud  : " << feudA << " against " << feudB
               << " (" << (int)worstFeud << "/100 unavenged)\n";
        int kinds = 0;
        for (int i = 0; i < 5; ++i) if (warsByReason[i] > 0) ++kinds;
        if (kinds > 0) {
            ss << "Wars fought over: ";
            bool first = true;
            for (int i = 0; i < 5; ++i)
                if (warsByReason[i] > 0) {
                    ss << (first ? "" : ", ") << warReasonName((WarReason)i)
                       << " ×" << warsByReason[i];
                    first = false;
                }
            ss << "\n";
        }
        if (!knowledgeHistory.empty())
            ss << "Knowledge     : " << knowledgeHistory.back().techCount
               << " techniques held"
               << (knowledgeHistory.back().literate ? " (writing in use)" : " (unwritten)")
               << ", dark ages " << darkAgeCount << "\n";
    }
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
