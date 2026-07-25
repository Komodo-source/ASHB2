#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <optional>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>
#include "FreeWillSystem.h"
#include "SocialNormSystem.h"
#include "SemanticMemory.h"
#include "PlanningSystem.h"
#include "PersonaSystem.h"
#include "Economics.h"
#include "SocialOrder.h"   // SocialClass enum + clientela/class fields
#include "Drive.h"         // bipolar homeostatic/novelty drives (0 -> setpoint <- 1)
#include "JungianType.h"   // Jung's 8 functions in Beebe's 8-archetype stack
#include "Genome.h"        // heritable traits: speed/sight/metabolism/fertility/resilience
#include "../ai/EpisodicMap.h"     // spatial event memory: food here, danger there
#include "../items/ItemSystem.h"   // tag/property items, Inventory, ActionRule ids
#include "../ai/neat/Neat.h"       // optional evolved neural brain (NEAT)
#include "../ai/MindUpgrade.h"     // AI upgrade phases A-E: emotions, skills, facts,
                                   // intentions, sleep, priors (plans/ai-upgrade-2026-07.md)

class Entity;
class Action;

static std::vector<std::string> male_name = {
"Sammy",
"Alexander",
"Griffin",
"Rayan",
"Yahir",
"Marques",
"Julien",
"Casey",
"Fletcher",
"Isiah",
"Keegan"};

static std::vector<std::string> female_name = {
"Amari",
"Campbell",
"Selah",
"Kamila",
"Makaila",
"Bethany",
"Jazlynn",
"Hadley",
"Stella",
"Mckinley",
"Eliza"
};

enum AttachmentStyle {
    SECURE,      // Comfortable with intimacy and independence
    ANXIOUS,     // Craves closeness, fears abandonment
    AVOIDANT,    // Values independence, suppresses need for closeness
    DISORGANIZED // Simultaneous desire and fear of intimacy
};


struct LifeMemory {
    std::string eventType;
    int entityInvolvedId = -1;    // qui était impliqué (-1 si personne)
    float emotionalIntensity = 0.0f;
    int simulationDay = 0;        // several push sites forgot to set this
    bool isFormative = false;     // change la personnalité de façon permanente
    std::string internalNarrative;
};

// How entity X perceives entity Y (from direct experience + heard information)
struct PerceivedReputation {
    int entityId;
    float positiveScore = 50.0f;
    float negativeScore = 50.0f;
    int timesGossipedAbout = 0;
    float trustworthiness  = 50.0f;
};

//self concept
struct SelfConcept {
    float perceivedExtraversion   = 50.0f;
    float perceivedAgreeableness  = 50.0f;
    float perceivedNeuroticism    = 50.0f;

    // Identity labels the entity has adopted
    // "loner", "caregiver", "achiever", "rebel"
    std::string primaryIdentity = "undefined";
    //pas utile à mon avis

    float selfEsteem     = 50.0f;

    float selfEfficacy   = 50.0f; //croyance que nos actions ont vraiment une sortie utile

    float calibration    = 0.0f; //comment ma vision correspond à celle de comment les autres me percoit
};


// ── I-P1 (Track I): narrative identity + purpose + life-story spine ──────────
// The raw material for a "meaningful person": an identity distilled from what
// the agent actually values and remembers (so it drives choice, not just UI),
// a sense of purpose that buffers despair (Durkheim: integration/meaning
// protect), and an ordered autobiography the Chronicle/Interview can read back.
// See plans/parallel-earth-upgrade.md §4 I-P1.
struct NarrativeIdentity {
    std::string selfStory     = "seeker"; // survivor|provider|carer|maker|leader|believer|fighter|seeker
    std::string dominantValue = "";       // which ValueSystem axis rules the self
    int   definingMemoryIdx   = -1;       // anchor formative memory (index into lifeMemories)
    float coherence           = 30.0f;    // 0-100 how settled/consistent the identity is
    std::string lastStory     = "";       // did the story hold since last recompute? (coherence)
};

struct LifeChapter {
    std::string title;     // born|first_love|bereaved|first_kill|triumph|betrayed|migrated|elevated|ruined
    int         day     = 0;
    int         otherId = -1;
    std::string note;      // one human-readable line
};

// ── IV-P2 (Track IV): the creed a believer actually carries ──────────────────
// A faith's doctrinal axes live on the Religion, but the scoring loop runs
// thousands of times a tick and has no business looking up a religion registry.
// So the axes of whatever a person believes are cached here and refreshed once
// per civ-day. This is the wire that finally gives doctrine *teeth*: a pacifist
// creed suppresses the violence pipeline for its believers, an ascetic one
// curbs their appetites, an authoritarian one buys obedience — so two faiths
// become behaviourally distinguishable instead of the near-identical creeds the
// flagship report complained of. Runtime-only (rebuilt from the faith each
// civ-day, so a loaded world restores it within a day).
// See plans/parallel-earth-upgrade.md §7 IV-P2.
struct Creed {
    bool  held           = false;  // does this person actually follow a faith?
    float militarism     = 50.0f;  // 0 pacifist      ←→ 100 crusader
    float tolerance      = 50.0f;  // 0 exclusive     ←→ 100 syncretic
    float asceticism     = 50.0f;  // 0 material      ←→ 100 spiritual
    float authority      = 50.0f;  // 0 egalitarian   ←→ 100 hierarchical
    float afterlifeFocus = 50.0f;  // 0 this-world    ←→ 100 next-world
    float devotion       = 50.0f;  // how strongly THIS person holds it (0-100)
};

struct GriefState {
    int lostPersonId;      // ID of the lost person
    int stagesRemaining;   // Kübler-Ross: 5 stages remaining
    float intensity;       // 0-1, decreases over time as entity recovers
    bool isDeath;          // true = death, false = breakup
};

struct PersonalityChange {
    float extraversionDrift = 0.0f;
    float agreeablenessDrift = 0.0f;
    float conscientiousnessDrift = 0.0f;
    float neuroticismDrift = 0.0f; //névrosité
    float opennessDrift = 0.0f;
};

struct EmotionalState {

    float rawAnger    = 0.0f;   // colère réelle interne
    float expressedAnger = 0.0f;  // ce qu'on montre
    float suppressionDebt = 0.0f; // accumulation du coût de la suppression
};

// NOTE (determinism): every field carries a default initializer. These structs
// are built as stack locals before being copied onto an Entity, so a member the
// constructor happens not to touch would otherwise be read as whatever the
// stack held — which is how two identical-seed runs diverged. Design rule #1
// (plans/parallel-earth-upgrade.md §2) depends on this staying true: never add
// a POD field here without a default.
struct LifeGoal {
    std::string type;
        // "find_partner", "build_career", "make_friends", "happiness", "self", "build_family"
    float priority         = 0.0f;   // dynamic, shifts with context
    float progressToward   = 0.0f;
    float frustrationLevel = 0.0f;   // increases when blocked
    int ticksSinceProgress = 0;
};


struct PheromoneRelease {
    std::string type = "";
        //social, breeding, procreation_simulation
    float releasing_level = 0.0f; //0-100
};

// Big Five personality traits
struct Personality {
    float extraversion;      // affects social need rates
    float agreeableness;     // reduces anger, increases forgiveness
    float conscientiousness; // affects work/achievement drive
    float neuroticism;       // affects stress/anxiety thresholds
    float openness;          // affects variety seeking

    Personality()
        : extraversion(50.0f), agreeableness(50.0f), conscientiousness(50.0f),
          neuroticism(50.0f), openness(50.0f) {}

    Personality(float ext, float agr, float con, float neu, float ope)
        : extraversion(ext), agreeableness(agr), conscientiousness(con),
          neuroticism(neu), openness(ope) {}
};


struct ValueSystem {
    float familyOrientation = 50.0f;     // How much family matters
    float achievementDrive = 50.0f;      // Career/status
    float spiritualNeed = 50.0f;         // Prayer/meaning-making
    float hedonism = 50.0f;              // Pleasure-seeking
    float collectivism = 50.0f;          // Group vs individual priority
};

enum LifeStage { INFANT, CHILD, ADOLESCENT, ADULT, ELDER };

// Life expectancy based on era
inline float getLifeExpectancy(int currentYear) {
    if (currentYear < -2000)  return 45.0f;   // Stone/Tribal age: short lives
    if (currentYear < 0)      return 52.0f;   // Bronze age
    if (currentYear < 500)    return 58.0f;   // Iron age
    if (currentYear < 1200)   return 62.0f;   // Classical
    if (currentYear < 1700)   return 65.0f;   // Medieval
    if (currentYear < 1900)   return 70.0f;   // Early Modern
    return 78.0f;                              // Modern
}

struct DevelopmentalHistory {
    bool hadSecureAttachment;      // relation des parents était comment bien ou mauvaise ??
    float childhoodTraumaScore;    // négligé, violenté, etc...
    float childhoodNurturingScore; // Inverse de ce qui est au dessus
    int developmentTicksRemaining = 80; //avant que l'enfance se termine
    AttachmentStyle attachmentStyle = SECURE;

    DevelopmentalHistory(){
        hadSecureAttachment = false;
        childhoodTraumaScore = 0.0f;
        childhoodNurturingScore = 0.0f;
    }

    DevelopmentalHistory(bool sec, float trauma, float nuturing){
        hadSecureAttachment = sec;
        childhoodTraumaScore = trauma;
        childhoodNurturingScore = nuturing;
    }
};

struct MentalModelOfOther {
    Entity* entityPointed = nullptr;
    float perceivedExtraversion = 50.0f;
    float perceivedAgreeableness = 50.0f;
    float perceivedNeuroticism = 50.0f;

    float estimatedHappiness = 50.0f;
    float estimatedAnger = 0.0f;
    float estimatedStress = 0.0f;

    float trustLevel = 50.0f;      // accumulated from interactions
    float predictability = 0.5f;   // how well I can predict their behavior
    float lastInteractionDay = 0.0f;
    std::string lastInteractionOutcome;

    float perceivedIntentionality = 0.5f; // do I think they act deliberately?

    // M4: beliefs about people go stale. confidence grows with each direct
    // observation and decays with time since the last one — decisions weight
    // a model by effectiveConfidence, so an outdated impression of someone
    // counts for less than a fresh one (and can simply be wrong meanwhile).
    int   lastObservedDay = 0;
    float confidence      = 0.1f;   // 0..1

    // B3: active deception. `fakedByThem` marks that the subject's trust was
    // inflated by the OTHER faking warmth (mind::attemptDeception), not earned
    // — `fakeTrustGain` is how much, so a later reveal (mind::detectDeception)
    // knows how far the prediction error snap should crater trust. Neither
    // field is persisted (list_MentalModelOfOther isn't saved at all — theory
    // of mind is runtime-only), so no save/load work is needed here.
    bool  fakedByThem   = false;
    float fakeTrustGain = 0.0f;

    void  updateFromObservation(Entity* observed, float observerAccuracy, int simDay);
    float effectiveConfidence(int today) const;

};


struct entityPointedDesire {
    int Id;
    Entity* pointedEntity;  // Changed to pointer
    float desire;
};

struct entityPointedAnger {
    int Id;
    Entity* pointedEntity;  // Changed to pointer
    float anger;
};

struct entityPointedSocial {
    int Id;
    Entity* pointedEntity;  // Changed to pointer
    float social;
};


struct entityPointedCouple {
    int id;
    Entity* pointedEntity;  // Changed to pointer
    // ── Relationship realism (runtime state, not serialized → reset on load) ──
    float commitment   = 50.0f; // how devoted THIS entity is to the bond (grows over time)
    float satisfaction = 60.0f; // contentment within the relationship
    float trust        = 65.0f; // belief that the partner is faithful
    float suspicion    = 0.0f;  // accumulated evidence/feeling of infidelity (0-100)
    int   daysTogether = 0;     // how long the bond has lasted
};

class Entity {
public:
    void initHieracgicalNeeds();
    // Attributes
    int entityId;
    float entityAge;
    float entityHealth;
    float entityHapiness;
    float entityStress;
    float entityMentalHealth;
    std::string name;
    float entityLoneliness;
    float entityBoredom;
    float entityGeneralAnger;
    float entityHygiene;
    char entitySex;
    int entityBDay;       // birth day of the year (0-364)
    int entityBirthYear;   // BC/AD year of birth (e.g. -4985 for 4985 BC)
    int entityAntiBody; // pourcentage
    int entityDiseaseType; //-1 if no disease
    // When a death is caused by an external agent (a killing) the cause is
    // attributed at the moment of the lethal blow and stashed here, so the
    // single central death-handler can log it verbatim instead of guessing.
    // Empty = death (if any) should be attributed from the entity's own state.
    std::string pendingDeathCause = "";
    // ── Structured killer attribution (Improvement Plan 4.3, fixes DQ-3) ──────
    // When the death is a killing, the aggressor's identity and motive are stashed
    // here at the moment of the blow so the death log can emit machine-readable
    // killer/victim fields (killer_id, tribes, motive) instead of only a prose
    // cause. -1 / empty when the death had no killer.
    int         pendingKillerId      = -1;
    int         pendingKillerTribeId = -1;
    std::string pendingKillMotive    = "";
    LifeStage entityLifeStage = INFANT;   // was uninitialized until first birthday tick
    float posX = 0.0f;
    float posY = 0.0f;
    float velX = 0.0f;
    float velY = 0.0f;
    bool selected = false;
    std::string innerMonologue = "";
    std::vector<entityPointedDesire> list_entityPointedDesire;
    std::vector<entityPointedAnger> list_entityPointedAnger;
    std::vector<entityPointedCouple> list_entityPointedCouple;
    std::vector<entityPointedSocial> list_entityPointedSocial;
    //LifeGoal m_goal;
    Personality personality;
    DevelopmentalHistory dv;
    FreeWillSystem fws;
    SemanticMemorySystem semanticMemory;
    PlanningSystem planner;
    std::vector<GriefState> griefStates;
    ValueSystem ValueSystem;
    SocialNorm socialNorm;
    std::vector<LifeGoal> m_goals; //une entité peut avoir entre 1 - 5 but de vie
    std::map<std::string, HierarchicalNeed> needs;

    // ── Bipolar drives: every axis has a lethal floor AND a lethal ceiling, a
    // comfort band around a setpoint, and behaviour is the pull back toward it
    // (0 -> setpoint <- 1). Adds chronic allostatic load + dopamine habituation
    // on top of the legacy unipolar stats. See Drive.h.
    DriveSet drives;

    // ── Jungian type (Beebe 8-function stack): the entity's cognitive spine and
    // its shadow. Derived from the Big Five at spawn; under psychic load the ego
    // falls into the grip of the inferior, then the shadow archetypes. See
    // JungianType.h.
    JungianStack cognition;

    LifeStage lifeStage = INFANT;
    Entity* parent1 = nullptr;
    Entity* parent2 = nullptr;

    // ── Kinship (ID-based: safe across entity-vector reallocation) ───────────
    // The legacy parent1/parent2 pointers are kept for back-compat, but these
    // IDs are the durable source of truth for lineage. Siblings are derived by
    // shared parent IDs; children are tracked explicitly for fast lineage walks.
    int              parent1Id = -1;
    int              parent2Id = -1;
    std::vector<int> childrenIds;
    int              familyId  = -1;   // clan/family this entity belongs to (-1 = none)

    float fatigueLevel = 0.0f;

    float socialDrain = 0.0f;
    int dayWithoutSocialAction = 0;
    float socialDeficit = 0.0f;

    // ── Subsistence: food is a survival necessity, not just a trade good ──
    // hunger climbs every tick; eating from foodStore lowers it. A depleted
    // store + rising hunger starves the entity (health damage → death). Food is
    // PRODUCED by hunting/gathering/farming and CONSUMED to stay alive.
    float entityHunger = 25.0f;  // 0 = sated, 100 = starving
    float foodStore    = 4.0f;   // days of rations this entity holds

    std::vector<LifeMemory> lifeMemories;
    EmotionalState emotionalState;
    std::vector<MentalModelOfOther*> list_MentalModelOfOther;
    SelfConcept SelfConcept;
    std::map<int, PerceivedReputation> reputationMap;
    // NOTE: these were uninitialized for years. meetingCount gates stranger
    // bonding (freewill:~2170), so entities built on recycled heap memory got
    // garbage values — the main source of run-to-run nondeterminism.
    float Esteem = 50.0f;
    int meetingCount = 0;

    PheromoneRelease pheromone;
    Economic salary;

    // ── Emergence upgrade (Steps 2–5): data-driven items, spatial memory,
    // heritable genome, optional NEAT brain. All default-initialized so
    // pre-upgrade spawn paths keep working unchanged.
    Inventory        inventory;         // tag/property item stacks
    std::vector<int> knownRecipeIds;    // ActionRules this agent can perform
    Genome           genome;            // multipliers ×1.0 by default
    EpisodicMap      episodicMap;       // where food/danger was seen
    bool             useNeatBrain = false;
    neat::Genome     neatGenome;        // empty unless useNeatBrain

    // ── AI upgrade phases A-E (plans/ai-upgrade-2026-07.md) ──────────────────
    // All default-initialized to neutral values so every existing spawn path
    // keeps working; serialized append-only in saveTo/loadFrom.
    EmotionState   emotions;            // B1: discrete OCC emotions
    SkillSet       skills;              // C2: practice-curve skills
    KnowledgeStore knowledge;           // C3: propositional facts / rumors
    Intention      intention;           // B4: the season's persistent pursuit
    float sleepPressure  = 30.0f;       // D2: climbs every tick, sleep clears it
    float sleepQuality   = 70.0f;       // D2: how well the last sleep went
    float injuryLevel    = 0.0f;        // D3: wounds from fights/hunts, heals slowly
    float homeAttachment = 20.0f;       // D5: roots in current community
    int   lastTribeIdSeen = -2;         // D5: change detector (-2 = never checked)
    int   tribeSwitchDay  = -99999;     // D5: when the entity was last uprooted
    // B2: expected outcome of the action just chosen (-1 = no expectation);
    // consumed by mind::settleOutcome to produce regret/relief. Runtime only.
    float lastExpectedOutcome = -1.0f;
    float decisionCloseness   = 0.0f;   // B5: top-2 score ratio (1 = torn mind)

    // ── I-P1 (Track I): a meaningful person — an identity that drives choice, a
    // protective sense of purpose, and a readable life story. NARRID/PURPOSE/
    // CHAPTERS serialized append-only. lastIdentityDay is runtime-only.
    NarrativeIdentity        narrativeIdentity;
    float                    senseOfPurpose  = 40.0f; // meaning/integration; buffers stress, resists despair
    std::vector<LifeChapter> lifeChapters;
    int                      lastIdentityDay = -1;    // last narrative recompute (staggered cadence)

    // ── III-P4 (Track III): cultural capital (Bourdieu) ──────────────────────
    // The third axis of class, alongside money and legal standing: literacy,
    // taste, manners, the bearing that comes of being raised in a house that
    // already had them. It is what lets a family stay elite through a bad
    // generation of finances, and what a newly rich commoner conspicuously
    // lacks — status is *signalled* by it and *reproduced* through it, because
    // children absorb their parents' share of it before they ever earn any.
    // Serialized (CULTCAP).
    float                    culturalCapital = 30.0f;   // 0-100

    // ── IV-P1 (Track IV): the culture this person actually carries ───────────
    // One bit per trait in the world catalogue
    // (environment::CulturalTransmissionSystem): the practices they keep, the
    // things they believe, what they will not do, what they find fine, and how
    // they wear their hair. Culture is therefore a property of PEOPLE that
    // moves between heads — inherited from parents at birth, caught from peers,
    // taught by elders, carried down trade roads — and a tribe's culture is
    // just what enough of its members happen to hold. Serialized (CULTTRAITS).
    unsigned long long       cultureTraits   = 0ull;
    // The subset of those this person will NOT give up: what they invented
    // themselves, and what they carried home from somewhere else. Centola's
    // committed minority, in one word — the reason a novelty gets any chance at
    // all to reach the critical mass instead of being dropped the first season
    // it makes its holder look odd. Serialized (CULTHELD).
    unsigned long long       committedTraits = 0ull;

    // ── I-P3 (Track I): how deep in its family line this person stands. The
    // founder of a house is 1, their children 2, and so on — the spine the
    // Chronicle walks to render a multi-generation saga, and what makes
    // "this line has run five generations" a fact rather than a guess.
    // Serialized (LINEAGE).
    int                      lineageDepth    = 1;

    // ── I-P4 (Track I): hedonic adaptation — a slowly-drifting personal stress
    // baseline the mind habituates to, so acute stress recovers instead of
    // staying pinned at 100 for a whole life. Serialized (STRESSBL).
    float                    stressBaseline  = 40.0f;

    bool knowsRecipe(int ruleId) const {
        for (int r : knownRecipeIds) if (r == ruleId) return true;
        return false;
    }
    void learnRecipe(int ruleId) {
        if (!knowsRecipe(ruleId)) knownRecipeIds.push_back(ruleId);
    }


    // Constructors
    Entity(int id);
    Entity(int id,
           float age,
           float health,
           float hapiness,
           float stress,
           float mentalHealth,
           std::string entityName,
           float loneliness,
           float boredom,
           float generalAnger,
           float hygiene,
           char sex,
           int bDay, //bday = jour de l'annee / 365
           int antiBody,
           int diseaseType,
           std::string goalType,
           int birthYear = -5000);

    // Methods
    std::string getName();
    float getHealth();
    int getId();
    void addDesire(entityPointedDesire pointed);
    void addAnger(entityPointedAnger pointed);
    void addCouple(entityPointedCouple pointed);
    void addSocial(entityPointedSocial pointed);

    void upgradeDesire(Entity* pointed, float value);
    void upgradeAnger(Entity* pointed, float value);
    void upgradeSocial(Entity* pointed, float value);

    void IncrementBDay();
    void saveEntityStats(Action* act);
    // M4 perf: saveEntityStats accumulates rows here; this writes them out.
    void flushEntityStats();
    std::string statsCsvBuffer;
    std::string actsCsvBuffer;
    Entity* mostAngryConn();
    Entity* mostDesireConn();
    Entity* mostSocialConn();

    float searchConnAng(Entity* ent);
    float searchConnDesire(Entity* ent);
    float searchConnSocial(Entity* ent);

    // Template method - must be in header for template instantiation
    template<typename T>
    int contains(const T& vec, Entity* ptr, int num_list);

    std::vector<entityPointedDesire> getListDesire();
    std::vector<entityPointedAnger> getListAnger();
    std::vector<entityPointedCouple> getListCouple();
    std::vector<entityPointedSocial> getListSocial();
    std::string getTypeGoal();
    double progressGoal();
    FreeWillSystem& getFreeWill(){return fws;};
    bool checkCouple(Entity* ent);
    void initializeHierarchicalNeeds();

    // Build the bipolar drive set from this entity's personality.
    void initDrives();

    // Build drives + Jungian stack and let the stack shape the drives. Call once
    // personality is finalized (spawn / birth); also lazily run on first tick.
    void initPsychology();

    void setGoal(std::string type);

    bool isGoalType(std::string name){
        for(LifeGoal goal : m_goals){
            if(goal.type == name){
                return true;
            }
        }
        return false;
    }

    // Grief system
    void addGrief(int lostId, float intensity, bool isDeath);
    void tickGrief(float deltaTime);
    float getGriefIntensity() const;

    LifeGoal SearchGoal(const std::string& goal_name);

    MentalModelOfOther* getModelOf(Entity* ent);

    // Save/Load
    void saveTo(std::ofstream& file) const;
    bool loadFrom(std::ifstream& file);  // false = truncated/corrupt block
    void resolvePointers(std::vector<Entity>& allEntities);

    // New: Initialize semantic memory from life memories
    void rebuildSemanticMemory() { semanticMemory.rebuildFromLifeMemories(this); }

    // New: Plan management
    void generateDailyPlan() { planner.generateDailyPlan(this); }


    std::string lastActionName = "";
    std::string lastNarrative  = "";

    std::string getCurrentActionName() const { return lastActionName; }

    // ── Phases 1-5: AI Enhancement ────────────────────────────────────────────
    PADState         pad;
    BodyLanguageCue  bodyLanguage = BodyLanguageCue::CONTENT;
    std::vector<CoreBelief>        coreBeliefs;
    std::deque<WorkingMemoryEntry> workingMemory;
    ChainOfThought   lastCoT;
    HesitationState  hesitation;
    std::string      selfGrounding = "";

    // ── Phase 6: Enhanced Cognitive Systems (ASHB2 Overhaul) ────────────────────

    // Gossip and Information Systems
    struct Rumor {
        std::string topic;           // What the rumor is about (event, entity, etc.)
        std::string content;         // The actual information/spin
        float reliability;           // 0-1, how trustworthy this information is
        int sourceEntityId;          // Who originally shared this (if known)
        int transmissionCount;       // How many times this has been passed along
        float beliefStrength;        // How strongly the entity believes this (0-1)
        int firstHeardDay;           // When first encountered
        int lastHeardDay;            // Most recent transmission

        Rumor() : reliability(0.5f), sourceEntityId(-1), transmissionCount(0),
                  beliefStrength(0.5f), firstHeardDay(0), lastHeardDay(0) {}
    };

    std::map<std::string, Rumor> knownRumors;   // Topic -> Rumor (what we've heard)
    std::map<int, float> informationExpertise;  // Topic areas we're knowledgeable about

    // Ideological Systems
    struct IdeologicalPosition {
        std::string issue;           // What this position is about (e.g., "tradition_vs_innovation")
        float position;              // -100 to +100 (extreme disagreement to extreme agreement)
        float conviction;            // 0-100, how strongly held this belief is
        float opennessToChange;      // 0-100, willingness to update this belief

        IdeologicalPosition() : position(0.0f), conviction(50.0f), opennessToChange(50.0f) {}
    };

    std::map<std::string, IdeologicalPosition> ideologicalStances;  // Issue -> Position

    // Biological Homeostasis Systems
    struct BiologicalState {
        float caloricIntakeToday;    // Calories consumed today
        float caloricExpenditureToday; // Calories burned today
        float nutritionalStatus;     // 0-100, overall nourishment level
        float hydrationLevel;        // 0-100, hydration status
        float energyLevel;           // 0-100, current energy availability

        BiologicalState() : caloricIntakeToday(0.0f), caloricExpenditureToday(0.0f),
                           nutritionalStatus(50.0f), hydrationLevel(50.0f), energyLevel(50.0f) {}
    };

    BiologicalState biology;

    // Disease and Immunity Systems
    struct PathogenExposure {
        int pathogenId;              // Type of pathogen
        int exposureDay;             // When exposed
        float viralLoad;             // Current pathogen load in body
        bool isInfected;             // Whether actively infected
        bool isContagious;             // Whether can transmit to others
        int daysInfected;              // How long infected
        int immunityLevel;             // 0-100, current immunity to this pathogen

        PathogenExposure() : pathogenId(-1), exposureDay(0), viralLoad(0.0f),
                           isInfected(false), isContagious(false), daysInfected(0), immunityLevel(0) {}
    };

    std::vector<PathogenExposure> pathogenExposures;
    float baseImmunity = 50.0f;      // Base immune system strength (0-100)

    // Epigenetic and Generational Trauma Systems
    struct EpigeneticMarker {
        std::string traumaSource;    // What caused this epigenetic change (war, famine, loss, etc.)
        float methylationLevel;      // 0-100, degree of epigenetic modification
        int generationOffset;        // How many generations ago this originated (0=self)
        float expressionLevel;       // 0-100, how strongly this trait is expressed

        EpigeneticMarker() : methylationLevel(0.0f), generationOffset(0), expressionLevel(0.0f) {}
    };

    std::vector<EpigeneticMarker> epigeneticMarkers;  // Trauma-induced epigenetic changes
    float intergenerationalTraumaLoad;   // 0-100, accumulated trauma from ancestors

    // ── Geography / Civilization ─────────────────────────────────────────────
    int   originRegionId = -1;   // cradle/landmass this lineage started in (-1 = none)
    int   tribeId       = -1;    // which tribe this entity belongs to (-1 = none)
    int   religionId    = -1;    // which religion they follow (-1 = none)
    // IV-P2: the doctrine of that faith, cached for the scoring loop (runtime).
    Creed creed;
    float dominanceRank = 0.0f;  // emergent social hierarchy position (0-100)

    // ── Social stratification (Clientela & Class) ────────────────────────────
    // socialClass is derived each civ-tick from wealth + auctoritas by the
    // SocialOrderSystem; auctoritas is earned standing/prestige (office, glory,
    // patronage of many clients) that, with wealth, governs class mobility.
    SocialClass socialClass = CLASS_PLEBEIAN;
    float       auctoritas  = 10.0f; // 0-100+ standing in the eyes of society
    float       integrity   = 50.0f; // 0-100 honesty/resistance to corruption (personality-derived)

    std::string specialization = "farmer"; // "farmer"|"scholar"|"craftsman"|"trader"|"healer"|"warrior"|"priest"
    bool  isSpecialist = false;      // released from subsistence farming, fed from the tribe granary
    int   roleSinceDay = -1;         // civ-day the current role was assigned (-1 = never)
    int   webCharId    = -1;         // web bridge: MySQL characters.id this entity embodies (-1 = pure AI)
    std::vector<int> knownTechIds;   // IDs of discovered/learned innovations

    // Temporary storage for IDs during loading (before pointer resolution)
    std::vector<std::pair<int, float>> tempDesireIds;
    std::vector<std::pair<int, float>> tempAngerIds;
    std::vector<std::pair<int, float>> tempSocialIds;
    std::vector<int> tempCoupleIds;


    void recalculatePriority();
    void addOrBoostGoal(const std::string& goal_name, float value);
    void onMajorEventAddOrBoostGoal(const std::string& eventType);

    // Phase 3: PAD model update
    void updatePAD();
    // Phase 4: contextual self-grounding string
    void updateSelfGrounding(int simDay);
    // Phase 2: distil repeated life memories into core beliefs
    void consolidateMemories(int simDay);
    // M4: forget — drop episodic memories whose decayed salience faded; formative
    // memories survive forever. Returns true if anything was removed (the caller
    // must then rebuild the semantic index, whose entries hold vector indices).
    bool pruneLifeMemories(int simDay);
    // Phase 2: push a new significant event into working memory
    void addToWorkingMemory(const std::string& eventType,
                            const std::string& desc, float weight);

    // ── Phase 6: Epigenetics & generational trauma ──────────────────────────
    // Record a new trauma-induced methylation mark (war/famine/loss/abuse/...).
    void addEpigeneticMarker(std::string traumaSource, float methylationLevel,
                              int generationOffset = 0);
    // Environmental support (0-100) modulates how strongly existing markers
    // are expressed; call when living conditions change materially.
    void updateEpigeneticExpression(float environmentalSupport);
    // Apply currently-expressed markers to personality/mood/mental health and
    // refresh intergenerationalTraumaLoad. Call periodically (not every tick).
    void applyEpigeneticEffects();
    // At birth: copy down decayed markers from both parents (either may be
    // null for founders) and immediately resolve their initial expression.
    void inheritEpigeneticMarkers(Entity* parent1, Entity* parent2);

    // ── Phase 5: biological homeostasis ──────────────────────────────────────
    // Once-per-day resolve: turns today's accumulated caloric intake/expenditure
    // (bumped directly on biology.caloricIntakeToday/ExpenditureToday at the
    // action-execution sites) into nutritionalStatus/energyLevel/hydrationLevel,
    // applies starvation/overeating effects on mood and health, then resets the
    // daily counters. Cheap (few float ops), so it can run for every living
    // entity every in-world day.
    void resolveBiologicalHomeostasis();

    // ── Phase 5: disease vectors ──────────────────────────────────────────────
    // Roll exposure to a pathogen from a contagious neighbour; survives the
    // entity's baseImmunity + genome resilience. No-op if already exposed to
    // the same pathogenId while it's still active.
    void exposeToPathogen(int pathogenId, int today);
    // Per-day progression: incubation -> symptomatic/contagious -> clears with
    // immunityLevel raised (naive vaccination-equivalent) or the entity stays
    // chronically contagious past a long timeout. Cheap, O(activePathogens).
    void tickPathogens(int today);
};

// Removed duplicate extern globals for pointed relationships
// Template implementation must be in header
template<typename T>
int Entity::contains(const T& vec, Entity* ptr, int num_list) {
    auto it = std::find_if(vec.begin(), vec.end(),
        [ptr](const auto& e) {
            return e.pointedEntity == ptr;
        });

    if(it != vec.end())
        return static_cast<int>(std::distance(vec.begin(), it));
    return -1;
}

#endif // ENTITY_H

