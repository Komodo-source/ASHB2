#ifndef CIVILIZATION_ENGINE_H
#define CIVILIZATION_ENGINE_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <random>
#include "Economics.h"
#include "WorldSeed.h"
#include "../environment/EnvironmentModel.h"   // II-P2: InstitutionalSystem

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

    // ── Doctrinal axes (Improvement Plan 3.1.A), each 0-100 ──────────────────
    // Generated from the founder's personality; they give faiths *meaningfully
    // different* behaviour instead of the near-identical creeds of the reference
    // run. Two pacifist/tolerant faiths coexist; two crusader/exclusive ones war.
    float militarism    = 50.0f; // 0 Pacifist            ←→ 100 Crusader
    float tolerance     = 50.0f; // 0 Exclusive           ←→ 100 Syncretic
    float asceticism    = 50.0f; // 0 Material            ←→ 100 Spiritual
    float authority     = 50.0f; // 0 Egalitarian         ←→ 100 Hierarchical
    float afterlifeFocus= 50.0f; // 0 This-world          ←→ 100 Next-world

    // ── Institutions (Improvement Plan 3.1.C) ────────────────────────────────
    // Grows with the living congregation: 0 none · 1 Shrine · 2 Temple ·
    // 3 Monastery · 4 Cathedral · 5 Holy Order · 6 Religious Center. A higher
    // level gives followers a larger happiness/mental bonus — real stakes that a
    // conquest or extinction then destroys.
    int   institutionLevel = 0;

    // ── IV-P2: what this faith DOES, and how it has fractured ────────────────
    // A religion survives because its rites are worth attending, and it splits
    // when a body of its members has drifted from what it teaches.
    int   lastRiteDay   = -99999;
    int   ritesHeld     = 0;
    int   lastSchismDay = -99999;

    std::vector<int> followerIds;
    int parentReligionId = -1; // -1 = original; >=0 = schism from parent
    int absorbedIntoId   = -1; // >=0 → this faith was merged into another (syncretism)
    float influence      = 0.0f;

    const char* institutionName() const {
        switch (institutionLevel) {
            case 1: return "Shrine";        case 2: return "Temple";
            case 3: return "Monastery";     case 4: return "Cathedral";
            case 5: return "Holy Order";    case 6: return "Religious Center";
            default: return "none";
        }
    }
};

// ── Tribe ─────────────────────────────────────────────────────────────────────
enum TribeStance { TS_NEUTRAL, TS_ALLY, TS_RIVAL, TS_AT_WAR };

// ── Government ──────────────────────────────────────────────────────────────────
// How a tribe is led. The form shapes how much dissent it tolerates before a coup,
// how taxes and famine bite into legitimacy, and which trait crowns the next
// leader. A collapse of legitimacy (govSatisfaction hitting the floor) triggers a
// coup that installs a *different* form — a revolution.
enum GovernmentType {
    GOV_DEMOCRACY,        // leader = broad approval; low unrest, coups are peaceful votes
    GOV_AUTHORITARIAN,    // one strongman rules by fear; heavy taxes tolerated, coups bloody
    GOV_DIVINE_MONARCHY,  // leader legitimated by the dominant faith; spiritualism stabilises
    GOV_OLIGARCHY         // the wealthiest few rule; wealth buys the throne, poverty breeds revolt
};
const char* governmentName(GovernmentType g);

// ── Why a war was declared (casus belli) ────────────────────────────────────────
// Wars are no longer only ethnic/faith hatred; each declaration is tagged with the
// grievance that actually tipped relations over the war line, which also colours
// how bloody and how endable the war is.
enum WarReason {
    WAR_ETHNIC,     // clashing faiths / ancient hatreds (the old bloody kind)
    WAR_CONQUEST,   // a strong militaristic tribe simply takes what a weak neighbour has
    WAR_RESOURCE,   // a starving tribe fights a fat-granaried one for food/land
    WAR_TRIBUTE,    // a vassal throws off its overlord, or an overlord punishes a defaulter
    WAR_BORDER      // ordinary friction between mismatched cultures
};
const char* warReasonName(WarReason r);

struct Tribe {
    int         id;
    std::string name;
    int         leaderId      = -1;
    int         foundedOnDay  = 0;
    std::vector<int> memberIds;

    //Economics related
    float taxeRate = 0.0f;
    Economic economy;
    std::vector<MarketProduct> weaponStorage;

    // ── Government & legitimacy ──────────────────────────────────────────────
    // The tribe's form of rule and how content the people are with it (0-100).
    // Satisfaction erodes with heavy taxes, famine and lost wars; it recovers with
    // victory, plenty and light taxes. When it collapses, the people rise up
    // (see updateGovernment) and a coup installs a new — often different — regime.
    GovernmentType government      = GOV_OLIGARCHY;
    float          govSatisfaction = 60.0f;
    int            lastCoupDay     = -100000;  // cooldown so revolts don't chain
    int            totalCoups      = 0;        // how many times this tribe has revolted

    // ── Elections & council ──────────────────────────────────────────────────
    // Democracies choose their leader by real ballot every term; every regime
    // seats a small council of notables whose mood steadies or shakes the ruler
    // and who steer the tax rate toward what the government form will bear.
    int   nextElectionDay    = -1;    // democracy: day of the next ballot (-1 = unscheduled)
    int   termLengthDays     = 40;    // civ-days between ballots
    std::vector<int> councilIds;      // 3 advisors, rebuilt each governance pass
    float lastElectionMargin = 0.0f;  // winner's vote share 0-1 (electoral legitimacy)

    // ── Corruption ───────────────────────────────────────────────────────────
    // Accumulated graft: rises when a low-integrity leader skims the tax take
    // or packs the specialist rolls with kin, decays under clean rule, and
    // quietly erodes legitimacy until a scandal blows it open.
    float corruption = 0.0f;          // 0-100

    // ── Vassalage ────────────────────────────────────────────────────────────
    // A defeated-but-not-destroyed tribe becomes a vassal: it keeps its identity
    // but siphons a share of its economy to the overlord, marches to the
    // overlord's wars, and rebels when it grows strong or bitter.
    int            overlordTribeId = -1;       // >=0 → we are this tribe's vassal
    std::set<int>  vassalTribeIds;             // tribes that owe us fealty
    int            vassalSinceDay  = -1;

    // ── War aftermath (returning-soldier effect, war-outcome memory) ─────────
    // Set when a war ends: a window during which this tribe's couples are far more
    // fertile (the post-war "baby boom"), plus a decaying memory of the last result.
    int   postWarBoomUntilDay = -1;   // day the baby-boom window closes
    int   recentWarResult     = 0;    // +1 recent victory, -1 recent defeat, decays to 0

    // ── War weariness (Improvement Plan 2.1.B) ───────────────────────────────
    // Accumulates every tick a tribe spends at war (faster in ethnic/holy wars),
    // decays slowly in peacetime. When it grows high enough a people will sue for
    // peace even while still armed — this is what ends the "endless ritual war"
    // pathology seen in the report (860 wars, 11 deaths). Reset toward 0 on peace.
    float warExhaustion = 0.0f;       // 0-100

    // ── Fortifications (Improvement Plan 2.3) ────────────────────────────────
    // Defensive works the tribe raises over time (Palisade→Wall→Castle→Star Fort,
    // gated by era). Adds to defensive strength in battle and forces attackers to
    // grind through a siege before they can storm — so a dug-in people is costly
    // to crack and wars gain a spatial, attritional shape.
    float fortificationLevel = 0.0f;  // 0-100
    // Abstract land holdings (Plan 2.1.A-lite). Grows with population; a decisive
    // victory transfers a slice of the loser's territory to the victor, so war
    // finally moves the map instead of being pure ritual.
    float territory = 0.0f;

    // ── Resource economy (Improvement Plan 6.1) ──────────────────────────────
    // Beyond the food granary, specialists now generate distinct stockpiles that
    // feed concrete effects: materials (building/defense), metals (military),
    // luxury (happiness/prestige) and knowledge (research). Depleted by use and
    // war, replenished by the relevant specialists each civ tick.
    float matStock       = 0.0f;   // worked timber & stone
    float metalStock     = 0.0f;   // smelted ore
    float luxuryStock    = 0.0f;   // fine crafts & trade goods
    float knowledgeStock = 0.0f;   // accumulated learning (does not spoil)

    // ── Culture & arts (Improvement Plan 7) ──────────────────────────────────
    // Artists turn leisure and inspiration into culture. A tribe's cultural score
    // lifts happiness and unity and marks great works; dark ages erode it.
    float cultureScore          = 0.0f;   // 0-100 cultural vitality
    int   culturalAchievements  = 0;      // great works produced over the run

    // ── III-P2: this place's prices (Track III) ──────────────────────────────
    // The global `g_market` sets what a good is worth in the world at large;
    // these are the multipliers that make it worth something DIFFERENT here.
    // A people sitting on full granaries prices food cheaply; a hungry one
    // bids it up. That gap between neighbours is what a caravan exists to
    // close, and closing it is how a merchant earns. 1.0 = the world price.
    // ── II-P3: the secular cycle (Track II) ──────────────────────────────────
    // Turchin's structural-demographic reading of why societies come apart on a
    // rhythm rather than at random. Three quantities do the work: how well the
    // common people are actually living, how many aspirants there are for the
    // few positions that confer power, and the political stress that builds when
    // the first falls while the second rises. Stress does not accumulate for
    // ever — it discharges as strife, which culls the surplus elite and resets
    // the cycle. That discharge is what makes history oscillate instead of
    // flat-lining. See plans/parallel-earth-upgrade.md §5 II-P3.
    float popularWellbeing     = 50.0f;  // "real wages": what life is like at the bottom
    float eliteOverproduction  = 1.0f;   // aspirants per available office (>1 = too many)
    float instability          = 0.0f;   // 0-100 political stress indicator
    int   strifeCount          = 0;      // times this people has torn itself apart
    int   lastStrifeDay        = -99999;

    // II-P4: was this people an empire (>=3 vassals) as of the last check? The
    // latch that makes "an empire rose" and "an empire fell" single events
    // rather than a status reprinted every civ-day.
    bool  wasEmpire   = false;

    float priceFood   = 1.0f;   // 0.5 (glut) .. 2.0 (dearth)
    float priceGoods  = 1.0f;   // materials, metal and luxuries
    float tradeWealth = 0.0f;   // cumulative earnings from commerce

    // ── I-P3: attribution (Track I) ──────────────────────────────────────────
    // Who made this. A people that cannot name its founder has no history, only
    // a present; the Chronicle reads these back so a reader can follow a person
    // from their birth to the nation that still carries their mark.
    int         founderId = -1;
    std::string founderName;

    // ── III-P1: settlement tier (Track III) ──────────────────────────────────
    // The tribe's home hardens into a physical place that grows with population:
    // camp→village→town→city→great city. Bigger tiers agglomerate (faster
    // research/culture — the collective brain) but pay a crowding cost (disease,
    // sanitation, unrest). Emergent sizes skew into a Zipf-like hierarchy.
    int   settlementTier = 0;   // 0 camp,1 village,2 town,3 city,4 great city
    int   peakTier       = 0;   // highest tier ever reached (for the chronicle)

    // ── IV-P1: this people's culture, as content (Track IV) ──────────────────
    // Not a score — a set. `cultureTraits` is what a majority of the members
    // actually hold (the ways an outsider would recognise this people by);
    // `knownTraits` is everything anyone here carries, majority or not, which is
    // the pool a novelty can still cascade out of. Both are recomputed from the
    // members each civ-day, so they need no serialisation: the truth lives in
    // the people, exactly as it should.
    unsigned long long cultureTraits   = 0ull; // held by >= half the members
    unsigned long long knownTraits     = 0ull; // held by at least one member
    // Latch: traits that have already crossed the critical mass here, so a
    // cascade is announced once when it happens rather than every day it holds.
    unsigned long long cascadedTraits  = 0ull;

    // Collective cultural values (0-100), evolve from member averages + drift
    float militarism   = 50.0f;
    float spiritualism = 50.0f;
    float collectivism = 50.0f;
    float innovation   = 50.0f;
    // ── A3/D4 (AI upgrade): festivity — how much this people celebrates.
    // Drifts from member hedonism/extraversion; sets the festival cadence.
    float festivity       = 50.0f;
    int   lastFestivalDay = -99999;  // civ-day of the last feast held

    // Geographic center of mass
    float centerX = 700.0f;
    float centerY = 525.0f;
    int   regionId = -1;   // landmass/cradle the tribe currently sits in
    // ── IV-P3: the tongue this people speaks ─────────────────────────────────
    // Seeded from the homeland's language, but a people that breaks away forks
    // its own and drifts. Keyed per TRIBE rather than per region because a
    // single-cradle world has one region — and with language keyed to regions
    // every people spoke the same tongue, so no barrier could ever exist and
    // the whole mechanism was inert. This is the ethnolinguistic identity the
    // plan asks for: distinct from the tribe, inherited by its daughters.
    int   languageId = -1;
    int   homeBiome = -1;  // Biome at the tribe centre (drives cultural drift)

    // ── Division of labour ───────────────────────────────────────────────────
    // Farmers deposit surplus food into the communal granary; it feeds the
    // non-farming specialists (artisans, priests, soldiers, traders, scholars).
    // When the granary runs dry, specialists revert to subsistence — economic
    // base determines superstructure.
    float granary        = 0.0f;
    int   specialistCount = 0;  // current non-farming specialists (UI / decisions)

    // ── III-P3: what a labour market and a fertility decision need to know ───
    // Both are recomputed from the members each civ-day in updateDivisionOfLabour,
    // so neither is serialized: they are summaries of the people, not state of
    // their own. `meanWealth` is what "rich" means HERE (a wealthy household in
    // a poor valley is not a wealthy household in a trading city), and
    // `childSurvival` is the share of this people's children who are living
    // through it — the quantity that, once it rises, stops parents replacing
    // children they no longer expect to lose.
    float meanWealth    = 0.0f;   // mean tokens held by living members
    float childSurvival = 0.5f;   // 0-1, smoothed share of under-12s in good health

    // Known technologies (by innovation id) — emergent innovation diffusion.
    std::set<int> knownTechIds;
    std::set<std::string> knownTechName;

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

    // ── II-P4: persistent grievance ledger (Track II) ────────────────────────
    // Unlike `relations` (which warms back toward 0 once fighting stops, so
    // atrocities are forgotten in a generation), grievance is a DURABLE memory
    // of concrete harms — battle dead, seized land, subjugation — that decays
    // only very slowly. It drags relations down for generations and gives war a
    // revenge cause that outlives the people who first drew blood (Axelrod:
    // unforgiving tit-for-tat). Runtime-only, like the rest of Tribe.
    std::map<int, float> grievance;   // tribeId -> accumulated unavenged harm 0..100

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
    ERA_RENNAISSANCE,
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

    // ── Institutional memory (Improvement Plan 1.1, Option D) ────────────────
    // The highest era the civilisation has ever reached. updateEra() will never
    // let the live era fall more than ONE step below this floor, so a dark age
    // can bruise progress but never cascade a Medieval society back to the Stone
    // Age. Combined with darkAgeResistance() this breaks the endless era-
    // regression loop that trapped the reference run below modernity.
    CivilizationEra     maxEraAchieved  = ERA_STONE_AGE;
    int                 lastCapacityDay = -1;  // famine effects apply once per civ-day
    int                 lastHistoryDay  = -1;  // history fingerprint logged once per day
    int                 lastGovDay      = -1;  // government/coup logic runs once per civ-day
    int                 lastClimateDay  = -1;  // climate / natural-disaster roll once per civ-day
    int                 totalDisasters  = 0;   // natural disasters that have struck (Plan 12)
    int                 totalCivilWars  = 0;   // tribes torn apart by internal revolt (Plan 8)
    int                 totalColonies   = 0;   // colony tribes founded in new land (Plan 13)
    int                 totalGreatFamilies = 0;// dynasties that rose to prominence (Plan 4.1)
    int                 totalSagas      = 0;   // narrative chains recorded (Plan 14)
    int                 totalTechSpreads= 0;   // tribe→tribe tech transfers (Plan 1.4)
    int                 lastDynastyDay  = -1;  // dynasty/class passes run once per civ-day
    // Live social-class census (Plan 4.2), refreshed each civ-day for the report.
    int eliteCount=0, upperCount=0, middleCount=0, lowerCount=0, outcastCount=0;

    // ── II-P2: institutions that store and transmit (Track II) ───────────────
    // The formerly-dead `InstitutionalSystem` (0 call sites) now ticks from the
    // civ loop. Schools/archives hold techniques independently of any living
    // member, guilds preserve a craft, and a bureaucracy is the administrative
    // capacity that lets a people grow past the size a camp can govern. Runtime
    // state (not serialized): founding conditions are re-evaluated every
    // civ-day, so a loaded world re-founds its institutions and re-archives the
    // techs its tribes still know within a few ticks.
    environment::InstitutionalSystem institutions;
    int totalInstitutions   = 0;   // ever founded (report)
    int totalArchiveSaves   = 0;   // techs a collapse would have erased, but didn't

    // ── I-P3: visible causal legacy (Track I) ────────────────────────────────
    // The plan's demand is that every life leave marks a reader can *see*. Three
    // structures carry it: great works are attributed to the hand that made
    // them; memorials fix a death to a place ("the ford where Kael drowned") so
    // the map itself remembers; and a Legacy is the closing entry on a life —
    // what that person founded, invented, led and left behind, kept after they
    // are gone so descendants can be measured against them. All runtime state.
    struct GreatWork {
        std::string name;
        std::string founderName;
        int   founderId = -1;
        int   tribeId   = -1;
        int   day       = 0;
    };
    struct Memorial {
        float x = 0.0f, y = 0.0f;
        std::string placeName;   // "the ford where Kael drowned"
        std::string personName;
        int   entityId = -1;
        int   day      = 0;
        float weight   = 1.0f;   // how large the memory looms
    };
    struct Legacy {
        int         entityId  = -1;
        std::string name;
        std::string familyName;
        int         familyId       = -1;
        int         deathDay       = 0;
        int         lineageDepth   = 1;
        int         descendants    = 0;   // living children at death
        float       weight         = 0.0f;// how much this life actually changed
        std::vector<std::string> marks;   // "founded the Tarn", "invented Writing"
    };
    std::vector<GreatWork> greatWorks;
    std::vector<Memorial>  memorials;
    std::vector<Legacy>    legacies;      // notable lives only, capped
    int totalNotableLives  = 0;
    int totalMemorials     = 0;

    // ── III-P2: trade routes (Track III) ─────────────────────────────────────
    // A standing commercial link between two peoples: a path that goods, and
    // with them techniques and habits, actually travel along. A route exists
    // only while the ground is passable, the peoples are not at war, and there
    // is someone to walk it — which is precisely why cutting one (a war, a
    // closed pass) is a thing worth doing, and why the price gap it was holding
    // shut springs back open when it goes.
    struct TradeRoute {
        int   a = -1, b = -1;      // tribe ids (a < b)
        int   establishedDay = 0;
        int   lastRunDay     = -999;
        float distance       = 0.0f;
        float volume         = 0.0f;   // cumulative goods carried
        bool  bySea          = false;  // needed Sailing to open
        bool  active         = true;
    };
    std::vector<TradeRoute> tradeRoutes;
    int   totalCaravans     = 0;
    int   totalRoutesOpened = 0;
    int   totalRoutesCut    = 0;

    // II-P4: empires assembled by conquest and lost to overstretch.
    int   totalEmpires       = 0;
    int   totalEmpiresFallen = 0;

    // ── II-P3: the cycle, sampled so it can be *seen* ────────────────────────
    // A claim that well-being and instability move in anti-phase is only worth
    // making if it is measured. These are world-level averages taken on a fixed
    // cadence; the end-of-run report correlates them, and the series is what a
    // plot of the secular cycle would be drawn from.
    struct CycleSample {
        int   day        = 0;
        float wellbeing  = 0.0f;
        float instability= 0.0f;
        float gini       = 0.0f;
        float elites     = 0.0f;   // mean elite overproduction
        float overshoot  = 0.0f;   // mean Malthusian pressure (pop/capacity - 1)
    };
    std::vector<CycleSample> cycleHistory;
    int totalStrifes = 0;

    // IV-P2 counters for the report.
    int totalRites   = 0;
    int totalSchisms = 0;
    // IV-P4: feasts where surplus was burned for standing instead of blood.
    int totalPotlatches = 0;

    // ── IV-P3: does a language boundary actually slow things down? ───────────
    // Diffusion events tallied by whether donor and recipient shared a tongue,
    // with the opportunities that produced them, so the report can compare
    // RATES rather than raw counts (there are far more same-tongue pairs).
    long diffusionSameTongue = 0,  diffusionOpportunitySame  = 0;
    long diffusionCrossTongue = 0, diffusionOpportunityCross = 0;
    int  totalCreolisations = 0;
    // IV-P3: how well two peoples understand one another (0-1). Returns 1 when
    // the feature is off, so every gate it guards stays wide open (bit-exact).
    // Public: the end-of-run report measures the world's mean intelligibility.
    float mutualIntelligibility(const Tribe& a, const Tribe& b) const;

    // ── IV-P1: the culture layer, and how to tell it is alive (Track IV) ─────
    // The catalogue and the transmission maths live in the environment layer;
    // the engine drives them from its own seeded stream so a run replays.
    environment::CulturalTransmissionSystem culture;
    // ── §8: instrumentation for "parallel Earth" ─────────────────────────────
    // You cannot claim realism you do not measure. These series and tallies
    // exist purely so the end-of-run report can put a number on claims the plan
    // makes — they are never read by the simulation, so they cannot change it.
    // See plans/parallel-earth-upgrade.md §8.
    int warsByReason[5] = { 0, 0, 0, 0, 0 };   // indexed by WarReason

    // ── III-P3: the labour market and the demographic transition, measured ───
    // Births alone prove nothing about fertility: a tier-3 city holds more
    // people than a camp, so it has more births whatever its birth RATE. What
    // the plan claims is that the RATE falls with wealth and urbanisation, so
    // both the births and the population-days that produced them are counted,
    // split the same two ways. `specialistShare*` does the matching job for the
    // labour half: whether the specialist mix really does track settlement size.
    long birthsByTier[5]   = { 0, 0, 0, 0, 0 };
    double popDaysByTier[5] = { 0, 0, 0, 0, 0 };
    long   birthsRich = 0, birthsPoor = 0;
    double popDaysRich = 0.0, popDaysPoor = 0.0;
    double specialistShareByTier[5] = { 0, 0, 0, 0, 0 };
    long   specialistSamplesByTier[5] = { 0, 0, 0, 0, 0 };
    long   hiresTotal = 0, hiresViaWeakTie = 0, apprenticeships = 0;
    // Called once per birth from the conception sites, while both parents are
    // still in hand, so the birth is filed under the tier and wealth bracket it
    // actually happened in.
    void recordBirthDemography(const Entity& a, const Entity& b);
    // The demographic transition itself: what this couple's circumstances do to
    // their odds of conceiving. Returns exactly 1.0f when the feature is off.
    float fertilityModifier(const Entity& a, const Entity& b) const;
    struct KnowledgeSample {
        int day       = 0;
        int techCount = 0;    // innovations known to the world + tree nodes unlocked
        bool literate = false;// has anybody written anything down yet?
        int  darkAges = 0;    // cumulative collapses, so a fall can be attributed
        int  era      = 0;    // §10.1: the rung the world stood on that day
    };
    std::vector<KnowledgeSample> knowledgeHistory;

    int totalTraitsInvented = 0;   // novelties struck by somebody, ever
    int totalCascades       = 0;   // times a trait crossed the 25% critical mass
    int totalFizzles        = 0;   // times a trait died out of a people entirely
    int totalTraitsDiffused = 0;   // traits carried into a people from outside

    // I-P3: close the book on a life — gather what it changed, pass its standing
    // to its children, and mark the ground where it ended. Called once per death
    // from the death pass, while every pointer is still valid.
    void recordLegacy(std::vector<Entity>& entities, const Entity& dead, int day);

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
    int  totalCoups          = 0; // regime changes forced by popular revolt
    int  totalVassalizations = 0; // wars ended by subjugation rather than slaughter
    int  totalRebellions     = 0; // vassals that threw off their overlord
    int  totalElections      = 0; // democratic ballots held (Society Plan 3)
    int  totalSuccessions    = 0; // leaders replaced on a predecessor's death (Plan 4)
    int  totalChallenges     = 0; // open challenges for the leadership (Plan 4)
    int  totalScandals       = 0; // corruption scandals exposed (Plan 5)
    int  totalDepositions    = 0; // leaders exiled in disgrace over graft (Plan 5)

    // True when tribes a and b are currently in an open war.
    bool areTribesAtWar(int tribeIdA, int tribeIdB) const;

    // ── Returning-soldier effect (post-war baby boom) ────────────────────────
    // https://en.wikipedia.org/wiki/Returning_soldier_effect
    // Returns a fertility multiplier (>=1.0) for a tribe that recently ended a war;
    // 1.0 when the tribe is not in a post-war window. The reproduction decision in
    // implem_free_will.cpp multiplies conception odds by this so peace brings a
    // surge of births, exactly as observed after real wars.
    float postWarBirthBoost(int tribeId, int day) const;
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

    // Resource economy (Plan 6): specialists produce materials/metals/luxury/
    // knowledge into tribal stockpiles; those stockpiles then feed defense,
    // military, happiness and research. Also grows fortifications. Once/civ tick.
    void updateEconomyResources(std::vector<Entity>& entities, int day);

    // Culture & arts (Plan 7): artists convert leisure + inspiration into cultural
    // score, occasionally producing great works; dark ages erode it. Once/civ tick.
    void updateCulture(std::vector<Entity>& entities, int day);

    // A3/D4 (AI upgrade): festivals — tribes with food and festive spirit hold
    // periodic feasts (suppression discharge, joy, cohesion, chronicle entry);
    // allied cultures slowly converge (horizontal transmission). Once/civ tick.
    void updateFestivals(std::vector<Entity>& entities, int day);

    // Climate & natural disasters (Plan 12): droughts, floods, earthquakes and
    // rarer volcanoes/meteors strike regions, harming people and works. Once/civ tick.
    void updateClimate(std::vector<Entity>& entities, int day);

    // Tech diffusion (Plan 1.4): technologies spread tribe→tribe through contact,
    // alliance, shared faith and conquest — knowledge no longer stays siloed.
    void updateTechDiffusion(std::vector<Entity>& entities, int day);

    // Social classes (Plan 4.2): derive an emergent elite/upper/middle/lower/outcast
    // class from each living person's wealth percentile and apply class effects.
    void updateSocialClasses(std::vector<Entity>& entities, int day);
    // III-P1: settlement tiers — cities emerge/grow, agglomerate, and crowd.
    void updateSettlements(std::vector<Entity>& entities, int day);
    // II-P2: found/tick/dissolve institutions — schools archive and teach,
    // guilds keep a craft alive, bureaucracies govern beyond a camp's reach.
    void updateInstitutions(std::vector<Entity>& entities, int day);
    // III-P2: regional prices, trade routes and the caravans that run them.
    void updateTrade(std::vector<Entity>& entities, int day);
    // II-P3: elite overproduction, popular immiseration, and the strife that
    // discharges the political stress they build — the secular cycle.
    void updateSecularCycle(std::vector<Entity>& entities, int day);
    // III-P4: cultivate cultural capital — the third axis of class, inherited
    // at birth and at the grave so advantage compounds down a bloodline.
    void updateClassReproduction(std::vector<Entity>& entities, int day);
    // IV-P2: hand believers their creed, hold the rites, and split faiths along
    // the doctrinal fault lines their congregations actually have.
    void updateDoctrine(std::vector<Entity>& entities, int day);
    // IV-P3: languages drift apart in isolation and creolise on contact.
    void updateLanguages(std::vector<Entity>& entities, int day);
    // IV-P1: culture as content — traits invented, caught, taught, dropped and
    // carried down trade roads, with Centola's 25% tipping point deciding
    // whether a novelty stays a quirk or cascades into a people's way of life.
    void updateCulturalTraits(std::vector<Entity>& entities, int day);
    // II-P2 helper: how much administrative capacity a people's bureaucracy
    // gives it (0 = none). Read by splitLargeTribes and the government pass.
    float adminCapacity(int tribeId) const;

    // Family dynasties (Plan 4.1): accrue prestige to the families of leaders,
    // the wealthy and the devout; announce the rise of "great families".
    void updateDynasties(std::vector<Entity>& entities, int day);

    // Elections & councils (Society Plan 3): democracies ballot for their leader
    // each term; every regime seats a council that judges the ruler and steers
    // the tax rate, and the tax take is finally collected. Once per civ-day.
    void updateElections(std::vector<Entity>& entities, int day);

    // Corruption (Society Plan 5): graft quietly erodes legitimacy each day;
    // oversight sometimes drags it into the light as a scandal that feeds the
    // existing election/coup machinery — or exiles the worst offenders outright.
    void updateCorruption(std::vector<Entity>& entities, int day);

    // Migration & colonization (Plan 13): send surplus population from a crowded
    // homeland to found a colony tribe in empty habitable land.
    void updateColonization(std::vector<Entity>& entities, int day);

    // Narrative chains (Plan 14): detect multi-step story arcs (plague years, a
    // dynasty's rise-and-fall, war→recovery) and record them as sagas.
    void updateNarrativeChains(std::vector<Entity>& entities, int day);

    // Gini coefficient of individual wealth (0 equal … 1 maximally unequal), for
    // the History panel / report (Plan 6.2).
    float wealthGini(const std::vector<Entity>& entities) const;

    // ── Phase 4: carrying capacity, famine, migration, dark ages ─────────────
    void updateCarryingCapacity(std::vector<Entity>& entities, int day);
    float regionAgTechMultiplier(int regionId, std::vector<Entity>& entities) const;
    void migrateOverflow(int fromRegion, int livingPop, float capacity,
                         std::vector<Entity>& entities, int day);
    // `resistance` (0..1) is the civilisation's institutional resilience this tick:
    // it both shields era-critical foundations and can preserve all knowledge
    // through the shock entirely (see darkAgeResistance).
    void loseTechnology(int day, const std::string& regionName, float resistance = 0.0f);

    // How well the civilisation weathers a collapse without losing knowledge
    // (0 = fragile Stone-Age band, up to ~0.95 = resilient advanced society).
    // Scales with the current era plus diplomatic and economic stability.
    float darkAgeResistance(const std::vector<Entity>& entities) const;

    // ── Tribe operations ──────────────────────────────────────────────────────
    bool formTribe(std::vector<Entity*>& cluster, int day);
    // Succession only: installs a new leader when the seat is empty or its
    // holder has died — hereditary where the regime allows, a snap ballot in
    // democracies. Day-to-day turnover now goes through elections, challenges
    // and coups instead of a per-tick dominance grab.
    void electLeader(Tribe& tribe, std::vector<Entity>& entities, int day);
    void updateTribeCenter(Tribe& tribe, std::vector<Entity>& entities);
    void updateTribeValues(Tribe& tribe, std::vector<Entity>& entities);
    void updateTribeTech(Tribe& tribe, std::vector<Entity>& entities);
    void updateTribeReligion(Tribe& tribe, std::vector<Entity>& entities);
    void absorbEntityIntoTribe(Tribe& tribe, Entity* ent);
    void removeDeadFromTribes(std::vector<Entity>& entities);
    void dissolveSmallTribes(std::vector<Entity>& entities, int day);
    void splitLargeTribes(std::vector<Entity>& entities, int day);
    void collectTaxes(Tribe& tribe, std::vector<Entity>& ent);

    // ── Government & coups ─────────────────────────────────────────────────────
    // Recompute every tribe's legitimacy from taxes/famine/war and, where it has
    // collapsed, stage a coup that installs a new leader and (often) a new regime.
    void updateGovernment(std::vector<Entity>& entities, int day);
    void stageCoup(Tribe& tribe, std::vector<Entity>& entities, int day);
    // Pick the successor best suited to the (possibly new) government form.
    int  chooseLeaderFor(const Tribe& tribe, GovernmentType gov,
                         std::vector<Entity>& entities) const;

    // ── War system ───────────────────────────────────────────────────────────
    void processWarTick(std::vector<Entity>& entities, int day);
    void executeBattle(Tribe& attacker, Tribe& defender, std::vector<Entity>& entities, int day);
    void conquerTribe(Tribe& victor, Tribe& loser, std::vector<Entity>& entities, int day);
    // Subjugate instead of annihilate: the loser survives as a tribute-paying,
    // co-belligerent vassal that may later rebel.
    void vassalizeTribe(Tribe& victor, Tribe& loser, std::vector<Entity>& entities, int day);
    // A vassal throws off its overlord (regains independence, relations sour).
    void rebelAgainstOverlord(Tribe& vassal, Tribe& overlord,
                              std::vector<Entity>& entities, int day);
    // Buy attack/defence goods into the tribe's armoury (fixed: mutates the tribe).
    void contributeToWarEffort(Tribe& tribe, std::vector<Entity>& entities);
    // 0..1 : how the tribe is faring across its current wars (1 = winning).
    float calculateAdvancementWar(const Tribe& tribe, std::vector<Entity>& entities) const;
    // Combat value stored in the armoury: summed attack / defence of held weapons.
    float weaponAttackStrength(const Tribe& tribe) const;
    float weaponDefenseStrength(const Tribe& tribe) const;
    // Begin a post-war baby boom for a tribe (returning-soldier effect) & remember
    // the result (+1 win / -1 loss) so morale and fertility respond to the outcome.
    void  endWarFor(Tribe& tribe, int result, int day);
    // War sunders romances that cross enemy lines: members of two warring tribes
    // who were a couple are forced apart, breeding resentment instead of children.
    void breakCrossTribeCouples(Tribe& A, Tribe& B, std::vector<Entity>& entities, int day);
    float calculateTribeMilitaryStrength(const Tribe& tribe, std::vector<Entity>& entities) const;
    float calculateTribeDefenseStrength(const Tribe& tribe, std::vector<Entity>& entities) const;

    // ── Religion operations ───────────────────────────────────────────────────
    bool foundReligion(Entity* prophet, int day);
    void spreadReligions(std::vector<Entity>& entities, int day);
    void checkSchisms(std::vector<Entity>& entities, int day);
    // Religious syncretism (Plan 3.1.B): two mutually-tolerant faiths that share
    // congregations merge into one (the smaller folded into the larger, doctrine
    // averaged) instead of endlessly splintering and going extinct. Returns the
    // number of merges performed this tick.
    int  trySyncretism(std::vector<Entity>& entities, int day);

    // ── Innovation operations ─────────────────────────────────────────────────
    // II-P1: how often a member of this people has an idea worth chasing, as a
    // multiple of the bare per-capita rate — scholars, literacy, schools, the
    // press and the method each raise it. 1.0 when knowledgeMul is off.
    float researchClimate(const Entity& ent, const Tribe* tribe) const;
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
