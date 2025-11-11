#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <optional>
#include <vector>
//#include "../../libs/BetterRand/BetterRand.h"


class Entity;

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
"Iris",
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
    int entityHygiene;
    char entitySex;
    int entityBDay;
    std::vector<entityPointedDesire> list_entityPointedDesire;
    std::vector<entityPointedAnger> list_entityPointedAnger;
    std::vector<entityPointedCouple> list_entityPointedCouple;
    std::vector<entityPointedSocial> list_entityPointedSocial;

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
           int hygiene,
           char sex,
           int bDay,
           entityPointedDesire*,
           entityPointedAnger*,
           entityPointedCouple*,
           entityPointedSocial*);

    // Methods
    std::string getName();
    float getHealth();
    int getId();
    void Entity::addDesire(entityPointedDesire pointed);
    void Entity::addAnger(entityPointedAnger pointed);
    void Entity::addCouple(entityPointedCouple pointed);
    void Entity::addSocial(entityPointedSocial pointed);
    int contains(const auto vec, Entity* ptr, int num_list);
    std::vector<entityPointedDesire> getListDesire();
    std::vector<entityPointedAnger> getListAnger();
    std::vector<entityPointedCouple> getListCouple();
    std::vector<entityPointedSocial> getListSocial();
};

// Move these outside the header to avoid multiple definition errors
extern std::vector<entityPointedDesire> list_entityPointedDesire;
extern std::vector<entityPointedAnger> list_entityPointedAnger;
extern std::vector<entityPointedCouple> list_entityPointedCouple;
extern std::vector<entityPointedSocial> list_entityPointedSocial;

#endif // ENTITY_H
