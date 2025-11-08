#ifndef ENTITY_H
#define ENTITY_H

#include <string>
#include <optional>
#include <vector>

struct entityPointedDesire {
    int Id;
    int IdPointer;
    float desire;
};

struct entityPointedAnger {
    int Id;
    int IdPointer;
    float anger;
};

struct entityPointedCouple {
    int id;
    int IdPointer;
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

    // Optional attributes
    entityPointedDesire pointedDesire;
    entityPointedAnger pointedAnger;
    entityPointedCouple pointedCouple;

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
           entityPointedDesire desire,
           entityPointedAnger anger,
           entityPointedCouple couple);

    // Methods
    std::string getName();
    float getHealth();
};

#endif // ENTITY_H
