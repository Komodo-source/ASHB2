#include "./header/Entity.h"

// Constructor with only ID
Entity::Entity(int id)
    : entityId(id),
      entityAge(0.0f),
      entityHealth(100.0f),
      entityHapiness(50.0f),
      entityStress(0.0f),
      entityMentalHealth(100.0f),
      name(""),
      entityLoneliness(0.0f),
      entityBoredom(0.0f),
      entityGeneralAnger(0.0f),
      entityHygiene(100),
      entitySex('U'),
      entityBDay(0),
      pointedDesire({}),
      pointedAnger({}),
      pointedCouple({})
{
}

// Constructor with all attributes
Entity::Entity(int id,
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
               entityPointedCouple couple)
    : entityId(id),
      entityAge(age),
      entityHealth(health),
      entityHapiness(hapiness),
      entityStress(stress),
      entityMentalHealth(mentalHealth),
      name(entityName),
      entityLoneliness(loneliness),
      entityBoredom(boredom),
      entityGeneralAnger(generalAnger),
      entityHygiene(hygiene),
      entitySex(sex),
      entityBDay(bDay),
      pointedDesire(desire),
      pointedAnger(anger),
      pointedCouple(couple)
{
}

// Getter for name
std::string Entity::getName() {
    return this->name;
}

// Getter for health
float Entity::getHealth() {
    return this->entityHealth;
}
