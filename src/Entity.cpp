#include "./header/Entity.h"
#include "./header/random.hpp"
#include <string>
//#include "../libs/BetterRand/BetterRand.h"
#include <time.h>
#include <random>
#include <algorithm>
#include "./header/FreeWillSystem.h"


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
      entityHygiene(100.0f),
      entitySex('A'),
      entityBDay(0),
      entityAntiBody(15),
      entityDiseaseType(-1),
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
               float hygiene,
               char sex,
               int bDay,
               int antiBody = 15,
               int diseaseType = -1,
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
      entityBDay(bDay),
      entityAntiBody(antiBody),
      entityDiseaseType(diseaseType)
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


void Entity::IncrementBDay(){
    this->entityAge ++;
}

void Entity::saveEntityStats(Action* act) {
    std::string file_name = "./src/data/" + std::to_string(this->entityId) + ".csv";
    std::ofstream file(file_name, std::ios::app);

    if (file.is_open()) {
        file << this->entityAntiBody << ',' << this->entityBoredom << ',' << this->entityGeneralAnger << ',' << this->entityHapiness << ',' << this->entityHealth << ',' << this->entityHygiene << ',' << this->entityLoneliness << ',' << this->entityMentalHealth << ',' << this->entityStress << ',' << "\n";
        file.close();
    }


    std::string changes;
    for(StatChange s : act->statChanges){
        changes += s.statName + " => " + std::to_string(s.changeValue) + " ";
    }
    std::string file_name2 = "./src/data/act_" + std::to_string(this->entityId) + ".csv";
    std::ofstream file2(file_name2, std::ios::app);

    if (file2.is_open()) {
        file2 << ',' << act->name << ", category: " << act->needCategory << ", satisfaction: " << std::to_string(act->baseSatisfaction) << ", outcome: " << std::to_string(act->outcomeSuccess) << ',' << changes ;
        file2.close();
    }
}

std::vector<entityPointedDesire> Entity::getListDesire(){ return list_entityPointedDesire;}
std::vector<entityPointedAnger> Entity::getListAnger(){ return list_entityPointedAnger;}
std::vector<entityPointedCouple> Entity::getListCouple(){ return list_entityPointedCouple;}
std::vector<entityPointedSocial> Entity::getListSocial(){ return list_entityPointedSocial;}

// Note: contains() template implementation moved to Entity.h
