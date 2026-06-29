#ifndef CIVILIZATION_ENGINE_H
#define CIVILIZATION_ENGINE_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <random>
#include "WorldSeed.h"

class Entity;

// ── Innovation ────────────────────────────────────────────────────────────────
struct Innovation {
    int         id;
    std::string name;
    std::string category;   // "agriculture"|"tool"|"medicine"|"social"|"military"|"spiritual"
    std::string description;
    int         discoveredByEntityId = -1;
    int         discoveredByTribeId  = -1;
    int         discoveredOnDay      = 0;
    float       complexity           = 40.0f;  // 0-100: higher = slower spread
    std::vector<std::string> prereqNames;       // names of prerequisite innovations
    int         knowerCount = 1;               // grows as it spreads
};

// ── Religion ──────────────────────────────────────────────────────────────────
enum MoralCode   { MC_STRICT, MC_PEACEFUL, MC_WARRIOR, MC_FLEXIBLE };
enum RitualType  { RT_DAILY_PRAYER, RT_WEEKLY_GATHERING, RT_MEDITATION, RT_CEREMONY, RT_SACRIFICE };

struct Religion {
    int         id;
    std::string name;
    int         founderEntityId;
    int         foundedOnDay;

    // Doctrine — generated from founder's personality
    MoralCode  moralCode          = MC_FLEXIBLE;
    RitualType ritual             = RT_WEEKLY_GATHERING;
    bool       isPolytheistic     = true;
    float      spiritualDemand    = 40.0f; // 0-100: how much commitment it asks
    std::string holyPrinciple     = "";    // a generated core belief statement

    std::vector<int> followerIds;
    int parentReligionId = -1; // -1 = original; >=0 = schism from parent
    float influence      = 0.0f;
};

// ── Tribe ─────────────────────────────────────────────────────────────────────
enum TribeStance { TS_NEUTRAL, TS_ALLY, TS_RIVAL, TS_AT_WAR };

struct Tribe {
    int         id;
    std::string name;
    int         leaderId      = -1;
    int         foundedOnDay  = 0;
    std::vector<int> memberIds;

    // Collective cultural values (0-100), evolve from member averages + drift
    float militarism   = 50.0f;
    float spiritualism = 50.0f;
    float collectivism = 50.0f;
    float innovation   = 50.0f;

    // Geographic center of mass
    float centerX = 700.0f;
    float centerY = 525.0f;
    int   regionId = -1;   // landmass/cradle the tribe currently sits in
    int   homeBiome = -1;  // Biome at the tribe centre (drives cultural drift)

    // ── Division of labour ───────────────────────────────────────────────────
    // Farmers deposit surplus food into the communal granary; it feeds the
    // non-farming specialists (artisans, priests, soldiers, traders, scholars).
    // When the granary runs dry, specialists revert to subsistence — economic
    // base determines superstructure.
    float granary        = 0.0f;
    int   specialistCount = 0;  // current non-farming specialists (UI / decisions)

    // Known technologies (by innovation id) — emergent innovation diffusion.
    std::set<int> knownTechIds;

    // ── Structured technology tree (see TechTree.h) ──────────────────────────
    // Distinct from the emergent `knownTechIds` above: this is a deliberate,
    // prerequisite-gated tech tree that a tribe researches over time. Research
    // points accumulate from population + scholars; unlocking a node also costs
    // stockpiled food (granary), so the economy gates advancement. Unlocked
    // nodes grant concrete, stacking bonuses (food, military, defense, research).
    float         researchPoints   = 0.0f;
    std::set<int> techTreeUnlocked;

    // Dominant religion among members (-1 = diverse)
    int dominantReligionId = -1;

    // Inter-tribe relations
    std::map<int, TribeStance> stances;
    std::map<int, float>       relations; // -100..+100
    std::set<int>              ethnicWarWith; // tribe ids this is in an ethnic/hate war with

    int  population()  const { return (int)memberIds.size(); }
    bool isMember(int id) const {
        for (int m : memberIds) if (m == id) return true;
        return false;
    }
};

// ── Diplomacy: formal treaties between tribes ──────────────────────────────────
// Distinct from the emergent `stances`/`relations` drift: a Treaty is a deliberate,
// persistent agreement two tribes enter into and that produces ongoing effects
// until it lapses or is broken. Diplomacy lets weaker peoples buy peace, trading
// partners enrich each other, and the strong extract tribute instead of blood.
enum TreatyType {
    TREATY_PEACE,      // ends a war and bars its rekindling for the term
    TREATY_ALLIANCE,   // mutual friendship & defence; holds tribes at TS_ALLY
    TREATY_TRADE,      // ongoing exchange: both granaries & relations grow
    TREATY_TRIBUTE     // tribeB pays tribeA food to avoid being raided
};

struct Treaty {
    TreatyType type;
    int   tribeA       = -1;   // proposer / (for tribute) the receiver
    int   tribeB       = -1;   // other party / (for tribute) the payer
    int   startDay     = 0;
    int   expiryDay    = -1;   // day it lapses (-1 = until broken)
    float tributeAmount = 0.0f; // food transferred per civ tick (TRIBUTE only)
    bool  active       = true;

    bool involves(int id) const { return tribeA == id || tribeB == id; }
    bool between(int a, int b) const {
        return (tribeA == a && tribeB == b) || (tribeA == b && tribeB == a);
    }
};

const char* treatyTypeName(TreatyType t);

// ── Civilization event ────────────────────────────────────────────────────────
struct CivEvent {
    int         day;
    std::string description;
    std::string category; // "tribe"|"religion"|"innovation"|"war"|"diplomacy"
};

// ── Year System (BC/AD equivalent) ─────────────────────────────────────────
// Simulation starts at year 5000 BC equivalent
// 1 sim-year ≈ 365 in-game days (but compressed: 1 year = 8 birthday ticks)
// Years < 0 = BC/BCE, Years >= 0 = AD/CE
// ── Era ───────────────────────────────────────────────────────────────────────
enum CivilizationEra {
    ERA_STONE_AGE,            // ~5000-3000 BC: scattered bands, basic tools
    ERA_TRIBAL,               // ~3000-1500 BC: stable tribes, oral tradition
    ERA_EARLY_AGRICULTURE,    // ~1500-500 BC: agriculture, first religions
    ERA_BRONZE_AGE,           // ~500 BC-0: metal working, trade networks
    ERA_IRON_AGE,             // 0-500 AD: iron, fortifications, empires
    ERA_CLASSICAL,            // 500-1200 AD: complex societies, philosophy
    ERA_MEDIEVAL,             // 1200-1700 AD: kingdoms, organized religion
    ERA_EARLY_MODERN,         // 1700-1900 AD: science, industry, exploration
    ERA_MODERN                 // 1900+ AD: advanced civilization
};

// ── CivilizationEngine ────────────────────────────────────────────────────────
class CivilizationEngine {
public:
    std::vector<Tribe>      tribes;
    std::vector<Religion>   religions;
    std::vector<Innovation> innovations;
    std::vector<Treaty>     treaties;   // active & lapsed formal agreements
    std::deque<CivEvent>    eventLog;   // last 120 civilization events
    CivilizationEra         era        = ERA_STONE_AGE;
    int                     currentYear = -5000;  // BC/AD year: starts at 5000 BC
    static constexpr int    START_YEAR   = -5000;
    int                     yearsPerTick = 10;     // simulation years advanced per era tick

    CivilizationEngine();

    // Main tick — call every N simulation days
    void tick(std::vector<Entity>& entities, int day);

    // UI helpers
    std::string getEraName()    const;
    std::string getEraSummary() const;
    std::string getYearDisplay() const;  // e.g. "4500 BC" or "1200 AD"
    int         getCurrentYear() const { return currentYear; }

    // Phase 4: Malthusian dynamics — populations per region this tick,
    // exposed for the UI/History panel.
    std::map<int,int>   regionPopulation;
    std::map<int,float> regionCapacity;
    int                 lastCollapseDay = -1;
    int                 darkAgeCount    = 0;
    int                 lastCapacityDay = -1;  // famine effects apply once per civ-day
    int                 lastHistoryDay  = -1;  // history fingerprint logged once per day

    // ── Running tallies for the report / big summary ─────────────────────────
    // Incremented from across the simulation so the History panel can show a
    // cumulative picture of the whole run, not just the live snapshot.
    int  totalBirths       = 0;
    int  totalDeaths       = 0;
    int  totalWarDeaths    = 0;   // deaths directly caused by battle/war attrition
    int  totalBattles      = 0;
    int  totalWarsDeclared = 0;
    int  totalEthnicWars   = 0;   // wars rooted in tribal/religious hatred
    int  totalConquests    = 0;
    int  totalCouplesBroken= 0;   // couples torn apart by war between their tribes
    int  peakPopulation    = 0;
    int  totalTreatiesSigned = 0; // formal treaties ever concluded

    // True when tribes a and b are currently in an open war.
    bool areTribesAtWar(int tribeIdA, int tribeIdB) const;
    // A multi-line cumulative report of the whole civilisation so far.
    std::string getBigSummary() const;

    // A compact fingerprint of the civilisation's state (era, dominant religions,
    // top techs, population). Two seeds -> different signatures = proof of divergence.
    uint64_t    historySignature() const;
    std::string historyLine() const;  // human-readable summary for the History panel

    Tribe*      findTribe(int id);
    Religion*   findReligion(int id);
    Innovation* findInnovation(int id);
    Innovation* findInnovationByName(const std::string& name);

    // Records a civilization-scale event. It is kept in the in-memory `eventLog`
    // deque (for the live History panel) AND flushed to the persistent
    // civilization_log.txt via the global Logger. The optional `data` is a
    // " key=value" structured block (file-only) for the post-mortem analyst —
    // it never appears in the UI description.
    void    logEvent(int day, const std::string& desc, const std::string& cat,
                     const std::string& data = "");

    // ── Diplomacy (see Diplomacy.cpp) ────────────────────────────────────────
    // Is there an active treaty of `type` between tribes a and b?
    bool        hasActiveTreaty(int a, int b, TreatyType type) const;
    // Count of currently-active treaties (for the UI / report).
    int         activeTreatyCount() const;
    // A short human-readable list of active treaties for the History panel.
    std::string diplomacySummary() const;

private:
    int nextTribeId      = 0;
    int nextReligionId   = 0;
    int nextInnovId      = 0;
    std::mt19937_64 rng;  // seeded deterministically from the global world seed

    // ── Per-tick phases ───────────────────────────────────────────────────────
    void updateDominanceRanks(std::vector<Entity>& entities);
    void updateTribes(std::vector<Entity>& entities, int day);
    void updateReligions(std::vector<Entity>& entities, int day);
    void updateInnovations(std::vector<Entity>& entities, int day);
    void updateTribeRelations(std::vector<Entity>& entities, int day);
    // Formal treaties: apply ongoing effects, expire/break stale ones, and let
    // tribes deliberately propose peace, alliances, trade pacts and tribute.
    void updateDiplomacy(std::vector<Entity>& entities, int day);
    void applyTreatyEffects(std::vector<Entity>& entities, int day);
    void proposeTreaties(std::vector<Entity>& entities, int day);
    void updateEra(const std::vector<Entity>& entities);
    void applyEffectsToEntities(std::vector<Entity>& entities, int day);

    // Division of labour: surplus food frees a fraction of each tribe from the
    // fields to become artisans/priests/soldiers/traders/scholars; famine
    // forces them back. Runs once per civ tick.
    void updateDivisionOfLabour(std::vector<Entity>& entities, int day);

    // Structured technology tree: accumulate research and unlock prerequisite-
    // gated tech nodes that grant stacking bonuses. Runs once per civ tick.
    void updateTechTree(std::vector<Entity>& entities, int day);

    // ── Phase 4: carrying capacity, famine, migration, dark ages ─────────────
    void updateCarryingCapacity(std::vector<Entity>& entities, int day);
    float regionAgTechMultiplier(int regionId, std::vector<Entity>& entities) const;
    void migrateOverflow(int fromRegion, int livingPop, float capacity,
                         std::vector<Entity>& entities, int day);
    void loseTechnology(int day, const std::string& regionName);

    // ── Tribe operations ──────────────────────────────────────────────────────
    bool formTribe(std::vector<Entity*>& cluster, int day);
    void electLeader(Tribe& tribe, std::vector<Entity>& entities);
    void updateTribeCenter(Tribe& tribe, std::vector<Entity>& entities);
    void updateTribeValues(Tribe& tribe, std::vector<Entity>& entities);
    void updateTribeTech(Tribe& tribe, std::vector<Entity>& entities);
    void updateTribeReligion(Tribe& tribe, std::vector<Entity>& entities);
    void absorbEntityIntoTribe(Tribe& tribe, Entity* ent);
    void removeDeadFromTribes(std::vector<Entity>& entities);
    void dissolveSmallTribes(std::vector<Entity>& entities, int day);
    void splitLargeTribes(std::vector<Entity>& entities, int day);

    // ── War system ───────────────────────────────────────────────────────────
    void processWarTick(std::vector<Entity>& entities, int day);
    void executeBattle(Tribe& attacker, Tribe& defender, std::vector<Entity>& entities, int day);
    void conquerTribe(Tribe& victor, Tribe& loser, std::vector<Entity>& entities, int day);
    // War sunders romances that cross enemy lines: members of two warring tribes
    // who were a couple are forced apart, breeding resentment instead of children.
    void breakCrossTribeCouples(Tribe& A, Tribe& B, std::vector<Entity>& entities, int day);
    float calculateTribeMilitaryStrength(const Tribe& tribe, std::vector<Entity>& entities) const;
    float calculateTribeDefenseStrength(const Tribe& tribe, std::vector<Entity>& entities) const;

    // ── Religion operations ───────────────────────────────────────────────────
    bool foundReligion(Entity* prophet, int day);
    void spreadReligions(std::vector<Entity>& entities, int day);
    void checkSchisms(std::vector<Entity>& entities, int day);

    // ── Innovation operations ─────────────────────────────────────────────────
    bool discoverInnovation(Entity* inventor, Tribe* tribe, int day);
    void spreadInnovations(std::vector<Entity>& entities, int day);
    bool entityKnowsPrereqs(const Entity* ent, const Innovation& inn) const;
    bool tribeKnowsPrereqs(const Tribe* tribe, const Innovation& inn) const;
    std::string pickCategory(const Entity* ent, const Tribe* tribe) const;

    // ── Naming ────────────────────────────────────────────────────────────────
    std::string tribeName(const Entity* leader);
    std::string religionName(const Entity* founder);

    // ── Helpers ───────────────────────────────────────────────────────────────
    float computeCharisma(const Entity* ent) const;
    Entity* entityById(std::vector<Entity>& entities, int id);


    template<class T>
    const T& pick(const std::vector<T>& v) {
        std::uniform_int_distribution<int> d(0, (int)v.size() - 1);
        return v[d(rng)];
    }
};

extern CivilizationEngine* globalCivEngine;

#endif
