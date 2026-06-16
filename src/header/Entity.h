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
    int entityInvolvedId;         // qui était impliqué (-1 si personne)
    float emotionalIntensity;
    int simulationDay;
    bool isFormative;             // change la personnalité de façon permanente
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

struct LifeGoal {
    std::string type;
        // "find_partner", "build_career", "make_friends", "happiness", "self", "build_family"
    float priority;          // dynamic, shifts with context
    float progressToward;
    float frustrationLevel;  // increases when blocked
    int ticksSinceProgress;
};


struct PheromoneRelease {
    std::string type = "";
        //social, breeding, procreation_simulation
    float releasing_level; //0-100
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
    Entity* entityPointed;
    float perceivedExtraversion;
    float perceivedAgreeableness;
    float perceivedNeuroticism;

    float estimatedHappiness;
    float estimatedAnger;
    float estimatedStress;

    float trustLevel;          // accumulated from interactions
    float predictability;      // how well I can predict their behavior
    float lastInteractionDay;
    std::string lastInteractionOutcome;

    float perceivedIntentionality; // do I think they act deliberately?

    void updateFromObservation(Entity* observed, float observerAccuracy);

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
    LifeStage entityLifeStage;
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

    LifeStage lifeStage = INFANT;
    Entity* parent1 = nullptr;
    Entity* parent2 = nullptr;

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
    float Esteem;


    int meetingCount;
    // Optional attributes
    entityPointedDesire pointedDesire;
    entityPointedAnger pointedAnger;
    entityPointedCouple pointedCouple;
    entityPointedSocial social;

    PheromoneRelease pheromone;
    Economic salary;


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

           entityPointedDesire*,
           entityPointedAnger*,
           entityPointedCouple*,
           entityPointedSocial*,
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
    void loadFrom(std::ifstream& file);
    void resolvePointers(std::vector<Entity>& allEntities);

    // New: Initialize semantic memory from life memories
    void rebuildSemanticMemory() { semanticMemory.rebuildFromLifeMemories(this); }

    // New: Plan management
    void generateDailyPlan() { planner.generateDailyPlan(this); }


    std::string lastActionName = "";
    std::string lastNarrative  = "";

    // ── Phases 1-5: AI Enhancement ────────────────────────────────────────────
    PADState         pad;
    BodyLanguageCue  bodyLanguage = BodyLanguageCue::CONTENT;
    std::vector<CoreBelief>        coreBeliefs;
    std::deque<WorkingMemoryEntry> workingMemory;
    ChainOfThought   lastCoT;
    HesitationState  hesitation;
    std::string      selfGrounding = "";

    // ── Geography / Civilization ─────────────────────────────────────────────
    int   originRegionId = -1;   // cradle/landmass this lineage started in (-1 = none)
    int   tribeId       = -1;    // which tribe this entity belongs to (-1 = none)
    int   religionId    = -1;    // which religion they follow (-1 = none)
    float dominanceRank = 0.0f;  // emergent social hierarchy position (0-100)
    std::string specialization = ""; // "scholar"|"craftsman"|"trader"|"healer"|"warrior"
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
    // Phase 2: push a new significant event into working memory
    void addToWorkingMemory(const std::string& eventType,
                            const std::string& desc, float weight);
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

#endif // ENTITY_ENT
