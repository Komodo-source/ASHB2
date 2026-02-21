#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <optional>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>
//#include "../../libs/BetterRand/BetterRand.h"
#include "FreeWillSystem.h"

class Entity;
class Action;

static std::vector<std::string> male_name = {
"Sammy",
"Pierre",
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
"Irys",
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

struct Goal {
    std::string type;
    // "find_partner", "build_career", "make_friends", "happiness", "self"
    float priority;
    int progressToward; // 0-100
};

// Big Five personality traits - affects behavior and need rates
struct Personality {
    float extraversion;      // 0-100: affects social need rates
    float agreeableness;     // 0-100: reduces anger, increases forgiveness
    float conscientiousness; // 0-100: affects work/achievement drive
    float neuroticism;       // 0-100: affects stress/anxiety thresholds
    float openness;          // 0-100: affects variety seeking

    Personality()
        : extraversion(50.0f), agreeableness(50.0f), conscientiousness(50.0f),
          neuroticism(50.0f), openness(50.0f) {}

    Personality(float ext, float agr, float con, float neu, float ope)
        : extraversion(ext), agreeableness(agr), conscientiousness(con),
          neuroticism(neu), openness(ope) {}
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
};

class Entity {
public:
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
    int entityBDay;
    int entityAntiBody; // pourcentage
    int entityDiseaseType; //-1 if no disease
    float posX;
    float posY;
    bool selected = false;
    std::vector<entityPointedDesire> list_entityPointedDesire;
    std::vector<entityPointedAnger> list_entityPointedAnger;
    std::vector<entityPointedCouple> list_entityPointedCouple;
    std::vector<entityPointedSocial> list_entityPointedSocial;
    Goal m_goal;
    Personality personality;
    FreeWillSystem fws;
    std::vector<GriefState> griefStates;

    // Optional attributes
    entityPointedDesire pointedDesire;
    entityPointedAnger pointedAnger;
    entityPointedCouple pointedCouple;
    entityPointedSocial social;


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
           std::string goalType);

    // Methods
    std::string getName();
    float getHealth();
    int getId();
    void addDesire(entityPointedDesire pointed);
    void addAnger(entityPointedAnger pointed);
    void addCouple(entityPointedCouple pointed);
    void addSocial(entityPointedSocial pointed);
    void IncrementBDay();
    void saveEntityStats(Action* act);
    Entity* mostAngryConn();
    Entity* mostDesireConn();
    Entity* mostSocialConn();

    // Template method - must be in header for template instantiation
    template<typename T>
    int contains(const T& vec, Entity* ptr, int num_list);

    std::vector<entityPointedDesire> getListDesire();
    std::vector<entityPointedAnger> getListAnger();
    std::vector<entityPointedCouple> getListCouple();
    std::vector<entityPointedSocial> getListSocial();
    std::string getTypeGoal();
    int progressGoal();
    FreeWillSystem& getFreeWill(){return fws;};
    bool checkCouple(Entity* ent);

    // Grief system
    void addGrief(int lostId, float intensity, bool isDeath);
    void tickGrief(float deltaTime);
    float getGriefIntensity() const;

    // Save/Load
    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
    void resolvePointers(std::vector<Entity>& allEntities);

    // Temporary storage for IDs during loading (before pointer resolution)
    std::vector<std::pair<int, float>> tempDesireIds;
    std::vector<std::pair<int, float>> tempAngerIds;
    std::vector<std::pair<int, float>> tempSocialIds;
    std::vector<int> tempCoupleIds;
};

// Move these outside the header to avoid multiple definition errors
extern std::vector<entityPointedDesire> list_entityPointedDesire;
extern std::vector<entityPointedAnger> list_entityPointedAnger;
extern std::vector<entityPointedCouple> list_entityPointedCouple;
extern std::vector<entityPointedSocial> list_entityPointedSocial;

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
