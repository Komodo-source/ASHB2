#include "./header/Entity.h"
#include "./header/random.hpp"
#include <string>
//#include "../libs/BetterRand/BetterRand.h"
#include <time.h>
#include <random>
#include <algorithm>
#include "./header/FreeWillSystem.h"
#include "header/BetterRand.h"

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
      posX(0.0f),
      posY(0.0f),
      selected(false),
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
               entityPointedSocial* social = nullptr,
               std::string goalType = "happiness"
                )
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
      entityDiseaseType(diseaseType),
      posX(0.0f),
      posY(0.0f),
      selected(false)
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
        if(BetterRand::genNrInInterval(0,1)){
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

    char* type[5] = {"find_partner", "build_career", "make_friends", "happiness", "self"};
    m_goal.progressToward = 0.0;
    m_goal.type = type[BetterRand::genNrInInterval(0,4)];
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

Entity* Entity::mostAngryConn(){
    Entity* ent = nullptr;
    float max = 0;
    for(entityPointedAnger pointed: this->list_entityPointedAnger){
        if(pointed.anger >= max){
            max = pointed.anger;
            ent = pointed.pointedEntity;
        }
    }
    return ent;
}
Entity* Entity::mostDesireConn(){
    Entity* ent = nullptr;
    float max = 0;
    for(entityPointedDesire pointed: this->list_entityPointedDesire){
        if(pointed.desire >= max){
            max = pointed.desire;
            ent = pointed.pointedEntity;
        }
    }
    return ent;
}



Entity* Entity::mostSocialConn(){
    Entity* ent = nullptr;
    float max = 0;
    for(entityPointedSocial pointed: this->list_entityPointedSocial){
        if(pointed.social >= max){
            max = pointed.social;
            ent = pointed.pointedEntity;
        }
    }
    return ent;
}

std::string Entity::getTypeGoal(){
    return m_goal.type;
}

int Entity::progressGoal(){
    return m_goal.progressToward;
}

bool Entity::checkCouple(Entity* ent){
    for(int i=0;i<list_entityPointedCouple.size();i++){
        if(list_entityPointedAnger[i].pointedEntity == ent){
            return true;
        }
    }
    return false;
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
        file2 << ',' << act->name << ",category: " << act->needCategory << ",satisfaction: " << std::to_string(act->baseSatisfaction) << ", outcome: " << std::to_string(act->outcomeSuccess) << ',' << changes ;
        file2.close();
    }
}

std::vector<entityPointedDesire> Entity::getListDesire(){ return list_entityPointedDesire;}
std::vector<entityPointedAnger> Entity::getListAnger(){ return list_entityPointedAnger;}
std::vector<entityPointedCouple> Entity::getListCouple(){ return list_entityPointedCouple;}
std::vector<entityPointedSocial> Entity::getListSocial(){ return list_entityPointedSocial;}

// Note: contains() template implementation moved to Entity.h

void Entity::saveTo(std::ofstream& file) const {
    file << "--- ENTITY " << entityId << " ---\n";
    file << "ID:" << entityId << "\n";
    file << "NAME:" << name << "\n";
    file << "AGE:" << entityAge << "\n";
    file << "HEALTH:" << entityHealth << "\n";
    file << "HAPPINESS:" << entityHapiness << "\n";
    file << "STRESS:" << entityStress << "\n";
    file << "MENTAL_HEALTH:" << entityMentalHealth << "\n";
    file << "LONELINESS:" << entityLoneliness << "\n";
    file << "BOREDOM:" << entityBoredom << "\n";
    file << "ANGER:" << entityGeneralAnger << "\n";
    file << "HYGIENE:" << entityHygiene << "\n";
    file << "SEX:" << entitySex << "\n";
    file << "BDAY:" << entityBDay << "\n";
    file << "ANTIBODY:" << entityAntiBody << "\n";
    file << "DISEASE:" << entityDiseaseType << "\n";
    file << "POSX:" << posX << "\n";
    file << "POSY:" << posY << "\n";
    file << "PERSONALITY:" << personality.extraversion << "," << personality.agreeableness << ","
         << personality.conscientiousness << "," << personality.neuroticism << "," << personality.openness << "\n";
    file << "GOAL:" << m_goal.type << "," << m_goal.priority << "," << m_goal.progressToward << "\n";

    // Save relationship lists (store target entity IDs, not pointers)
    file << "DESIRE_COUNT:" << list_entityPointedDesire.size() << "\n";
    for (const auto& d : list_entityPointedDesire) {
        file << "DESIRE:" << d.pointedEntity->entityId << "," << d.desire << "\n";
    }
    file << "ANGER_COUNT:" << list_entityPointedAnger.size() << "\n";
    for (const auto& a : list_entityPointedAnger) {
        file << "ANGER_LINK:" << a.pointedEntity->entityId << "," << a.anger << "\n";
    }
    file << "SOCIAL_COUNT:" << list_entityPointedSocial.size() << "\n";
    for (const auto& s : list_entityPointedSocial) {
        file << "SOCIAL:" << s.pointedEntity->entityId << "," << s.social << "\n";
    }
    file << "COUPLE_COUNT:" << list_entityPointedCouple.size() << "\n";
    for (const auto& c : list_entityPointedCouple) {
        file << "COUPLE:" << c.pointedEntity->entityId << "\n";
    }

    // Save FreeWillSystem
    fws.saveTo(file);

    file << "--- END ENTITY ---\n";
}

void Entity::loadFrom(std::ifstream& file) {
    std::string line;

    // Read entity header
    std::getline(file, line); // "--- ENTITY <id> ---"

    std::getline(file, line); entityId = std::stoi(line.substr(3));
    std::getline(file, line); name = line.substr(5);
    std::getline(file, line); entityAge = std::stof(line.substr(4));
    std::getline(file, line); entityHealth = std::stof(line.substr(7));
    std::getline(file, line); entityHapiness = std::stof(line.substr(10));
    std::getline(file, line); entityStress = std::stof(line.substr(7));
    std::getline(file, line); entityMentalHealth = std::stof(line.substr(14));
    std::getline(file, line); entityLoneliness = std::stof(line.substr(11));
    std::getline(file, line); entityBoredom = std::stof(line.substr(8));
    std::getline(file, line); entityGeneralAnger = std::stof(line.substr(6));
    std::getline(file, line); entityHygiene = std::stof(line.substr(8));
    std::getline(file, line); entitySex = line.substr(4)[0];
    std::getline(file, line); entityBDay = std::stoi(line.substr(5));
    std::getline(file, line); entityAntiBody = std::stoi(line.substr(9));
    std::getline(file, line); entityDiseaseType = std::stoi(line.substr(8));
    std::getline(file, line); posX = std::stof(line.substr(5));
    std::getline(file, line); posY = std::stof(line.substr(5));

    // Personality
    std::getline(file, line);
    std::string pdata = line.substr(12);
    size_t c1 = pdata.find(',');
    size_t c2 = pdata.find(',', c1 + 1);
    size_t c3 = pdata.find(',', c2 + 1);
    size_t c4 = pdata.find(',', c3 + 1);
    personality.extraversion = std::stof(pdata.substr(0, c1));
    personality.agreeableness = std::stof(pdata.substr(c1 + 1, c2 - c1 - 1));
    personality.conscientiousness = std::stof(pdata.substr(c2 + 1, c3 - c2 - 1));
    personality.neuroticism = std::stof(pdata.substr(c3 + 1, c4 - c3 - 1));
    personality.openness = std::stof(pdata.substr(c4 + 1));

    // Goal
    std::getline(file, line);
    std::string gdata = line.substr(5);
    size_t g1 = gdata.find(',');
    size_t g2 = gdata.find(',', g1 + 1);
    m_goal.type = gdata.substr(0, g1);
    m_goal.priority = std::stof(gdata.substr(g1 + 1, g2 - g1 - 1));
    m_goal.progressToward = std::stoi(gdata.substr(g2 + 1));

    // Load relationship IDs (will resolve to pointers later)
    list_entityPointedDesire.clear();
    tempDesireIds.clear();
    std::getline(file, line);
    int desireCount = std::stoi(line.substr(13));
    for (int i = 0; i < desireCount; i++) {
        std::getline(file, line);
        std::string d = line.substr(7);
        size_t comma = d.find(',');
        int targetId = std::stoi(d.substr(0, comma));
        float desire = std::stof(d.substr(comma + 1));
        tempDesireIds.push_back({targetId, desire});
    }

    list_entityPointedAnger.clear();
    tempAngerIds.clear();
    std::getline(file, line);
    int angerCount = std::stoi(line.substr(12));
    for (int i = 0; i < angerCount; i++) {
        std::getline(file, line);
        std::string a = line.substr(11);
        size_t comma = a.find(',');
        int targetId = std::stoi(a.substr(0, comma));
        float anger = std::stof(a.substr(comma + 1));
        tempAngerIds.push_back({targetId, anger});
    }

    list_entityPointedSocial.clear();
    tempSocialIds.clear();
    std::getline(file, line);
    int socialCount = std::stoi(line.substr(13));
    for (int i = 0; i < socialCount; i++) {
        std::getline(file, line);
        std::string s = line.substr(7);
        size_t comma = s.find(',');
        int targetId = std::stoi(s.substr(0, comma));
        float social = std::stof(s.substr(comma + 1));
        tempSocialIds.push_back({targetId, social});
    }

    list_entityPointedCouple.clear();
    tempCoupleIds.clear();
    std::getline(file, line);
    int coupleCount = std::stoi(line.substr(13));
    for (int i = 0; i < coupleCount; i++) {
        std::getline(file, line);
        int targetId = std::stoi(line.substr(7));
        tempCoupleIds.push_back(targetId);
    }

    // Load FreeWillSystem
    fws.loadFrom(file);

    // Read end marker
    std::getline(file, line); // "--- END ENTITY ---"

    selected = false;
}

void Entity::resolvePointers(std::vector<Entity>& allEntities) {
    // Helper to find entity by ID
    auto findEntity = [&](int id) -> Entity* {
        for (auto& e : allEntities) {
            if (e.entityId == id) return &e;
        }
        return nullptr;
    };

    for (const auto& pair : tempDesireIds) {
        Entity* target = findEntity(pair.first);
        if (target) list_entityPointedDesire.push_back({1, target, pair.second});
    }
    tempDesireIds.clear();

    for (const auto& pair : tempAngerIds) {
        Entity* target = findEntity(pair.first);
        if (target) list_entityPointedAnger.push_back({1, target, pair.second});
    }
    tempAngerIds.clear();

    for (const auto& pair : tempSocialIds) {
        Entity* target = findEntity(pair.first);
        if (target) list_entityPointedSocial.push_back({1, target, pair.second});
    }
    tempSocialIds.clear();

    for (int id : tempCoupleIds) {
        Entity* target = findEntity(id);
        if (target) list_entityPointedCouple.push_back({1, target});
    }
    tempCoupleIds.clear();
}
