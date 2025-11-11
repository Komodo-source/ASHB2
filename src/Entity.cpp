#include "./header/Entity.h"
#include "./header/random.hpp"
#include <string>
//#include "../libs/BetterRand/BetterRand.h"
#include <time.h>
#include <random>
#include <algorithm>

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
      entitySex('A'),
      entityBDay(0),
      pointedDesire({}),
      pointedAnger({}),
      pointedCouple({})
{

}

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
               entityPointedDesire* desire = nullptr,
               entityPointedAnger* anger = nullptr,
               entityPointedCouple* couple = nullptr,
               entityPointedSocial* social = nullptr)
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
      entityBDay(bDay)
{
    if(desire != nullptr){
        list_entityPointedDesire.push_back(*desire);
    }else if(anger != nullptr){
        list_entityPointedAnger.push_back(*anger);
    }else if(couple != nullptr){
        list_entityPointedCouple.push_back(*couple);
    }else if(social != nullptr){
        list_entityPointedSocial.push_back(*social);
    }


    if(entitySex == 'A'){
        if(rand() % 1){
            entitySex = 'M';
        }else{
            entitySex = 'F';
        }
    }

    if(name.empty()){
        int taille = male_name.size() - 1;
        if (entitySex == 'M') {
            int index = (rand() % taille);
                name = male_name.at(index);
            } else {
                int index = (rand() % taille);
                name = female_name.at(index);
            }
    }
}

// Getter for name
std::string Entity::getName() {
    return this->name;
}

int Entity::getId() {
    return this->entityId;
}

// Getter for health
float Entity::getHealth() {
    return this->entityHealth;
}

void Entity::addDesire(entityPointedDesire pointed) {
    list_entityPointedDesire.push_back(pointed);
}


void Entity::addAnger(entityPointedAnger pointed) {
    list_entityPointedAnger.push_back(pointed);
}

void Entity::addCouple(entityPointedCouple pointed) {
    list_entityPointedCouple.push_back(pointed);
}

void Entity::addSocial(entityPointedSocial pointed) {
    list_entityPointedSocial.push_back(pointed);
}


std::vector<entityPointedDesire> Entity::getListDesire(){ return list_entityPointedDesire;}
std::vector<entityPointedAnger> Entity::getListAnger(){ return list_entityPointedAnger;}
std::vector<entityPointedCouple> Entity::getListCouple(){ return list_entityPointedCouple;}
std::vector<entityPointedSocial> Entity::getListSocial(){ return list_entityPointedSocial;}

//retourne 1 si trouve la struct dans la liste passé, num_list= 1: Desire, 2: Anger, 3: Couple, 4: Social
int Entity::contains(const auto vec, Entity* ptr, int num_list) {
    switch (num_liste)
    {
    case 1:
        auto it =  std::find_if(vec.begin(), vec.end(),
        [ptr](const entityPointedDesire& e) {
            return e.pointedEntity == ptr;
        }) != vec.end();
        return std::distance(vec.begin(), it);
        if(it != vec.end())
            return static_cast<int>(std::distance(vec.begin(), it));
        return -1;
        break;
    case 2:
        auto it =  std::find_if(vec.begin(), vec.end(),
        [ptr](const entityPointedAnger& e) {
            return e.pointedEntity == ptr;
        }) != vec.end();
        break;
        if(it != vec.end())
            return static_cast<int>(std::distance(vec.begin(), it));
        return -1;
    case 3:
        auto it =  std::find_if(vec.begin(), vec.end(),
        [ptr](const entityPointedCouple& e) {
            return e.pointedEntity == ptr;
        }) != vec.end();
        break;
        if(it != vec.end())
            return static_cast<int>(std::distance(vec.begin(), it));
        return -1;
    case 4:
        auto it =  std::find_if(vec.begin(), vec.end(),
        [ptr](const entityPointedSocial& e) {
            return e.pointedEntity == ptr;
        }) != vec.end();
        break;
        if(it != vec.end())
            return static_cast<int>(std::distance(vec.begin(), it));
        return -1;

    default:
        return false;
        break;
    }

}
