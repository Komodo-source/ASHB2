#include "./header/Entity.h"
#include "./header/random.hpp"
#include <cstddef>
#include <string>
#include "./header/FreeWillSystem.h"


#include <iostream>
//#include "../libs/BetterRand/BetterRand.h"
#include <time.h>
#include <random>
#include <algorithm>
#include "./header/FreeWillSystem.h"
#include "header/BetterRand.h"
#include "./header/SocialNormSystem.h"
#include "./header/NeedLevel.h"

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

    char* type[5] = {(char*)"find_partner", (char*)"build_career", (char*)"make_friends", (char*)"happiness", (char*)"self"};
    LifeGoal initialGoal;
    initialGoal.progressToward = 0.0;
    initialGoal.type = type[BetterRand::genNrInInterval(0,4)];
    initialGoal.priority = 100.0f;
    initialGoal.frustrationLevel = 0.0f;
    initialGoal.ticksSinceProgress = 0;
    m_goals.push_back(initialGoal);
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
    this->list_entityPointedDesire.push_back(pointed);
}


void Entity::addAnger(entityPointedAnger pointed) {
    this->list_entityPointedAnger.push_back(pointed);
}

void Entity::addCouple(entityPointedCouple pointed) {
    this->list_entityPointedCouple.push_back(pointed);
}

void Entity::addSocial(entityPointedSocial pointed) {
    this->list_entityPointedSocial.push_back(pointed);
}


void Entity::IncrementBDay(){
    this->entityAge ++;
    if(this->entityAge < 10){
        this->entityLifeStage = LifeStage::CHILD;
    }else if(this->entityAge < 18){
        this->entityLifeStage = LifeStage::ADOLESCENT;
    }else if(this->entityAge < 65){
        this->entityLifeStage = LifeStage::ADULT;
    }else{
        this->entityLifeStage = LifeStage::ELDER;
    }
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
    if (!m_goals.empty()) return m_goals.front().type;
    return "none";
}

double Entity::progressGoal(){
    if (!m_goals.empty()) return m_goals.front().progressToward;
    return 0.0;
}

bool Entity::checkCouple(Entity* ent){
    for(int i=0;i<list_entityPointedCouple.size();i++){
        if(list_entityPointedCouple[i].pointedEntity == ent){
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

std::vector<entityPointedDesire> Entity::getListDesire(){ return this->list_entityPointedDesire;}
std::vector<entityPointedAnger> Entity::getListAnger(){ return this->list_entityPointedAnger;}
std::vector<entityPointedCouple> Entity::getListCouple(){ return this->list_entityPointedCouple;}
std::vector<entityPointedSocial> Entity::getListSocial(){ return this->list_entityPointedSocial;}

void Entity::addGrief(int lostId, float intensity, bool isDeath) {
    // If already grieving this person, refresh and intensify
    for (auto& g : griefStates) {
        if (g.lostPersonId == lostId) {
            g.stagesRemaining = 5;
            g.intensity = std::min(1.0f, g.intensity + intensity * 0.5f);
            std::cout << "Grief refreshed for entity " << entityId
                      << " (lost person " << lostId << ", intensity: " << g.intensity << ")\n";
            return;
        }
    }
    GriefState gs;
    gs.lostPersonId = lostId;
    gs.stagesRemaining = 5;
    gs.intensity = std::min(1.0f, intensity);
    gs.isDeath = isDeath;
    griefStates.push_back(gs);
    std::cout << "Grief added for entity " << entityId
              << " (lost person " << lostId
              << (isDeath ? ", cause: death" : ", cause: breakup")
              << ", intensity: " << gs.intensity << ")\n";
}

void Entity::tickGrief(float deltaTime) {
    // Gradual recovery: intensity decreases each tick
    float recoveryRate = 0.0008f * deltaTime; // very slow recovery
    if (this->dv.attachmentStyle == ANXIOUS) {
        recoveryRate = 0.0004f * deltaTime; // Recover slowly
    } else if (this->dv.attachmentStyle == AVOIDANT) {
        recoveryRate = 0.0016f * deltaTime; // Recover quickly
    }
    for (auto& g : griefStates) {
        g.intensity -= recoveryRate;
        // Advance stage every time intensity crosses a threshold
        float stageThreshold = (float)g.stagesRemaining / 5.0f;
        if (g.intensity < stageThreshold - 0.1f && g.stagesRemaining > 0) {
            g.stagesRemaining--;
            std::cout << "Entity " << entityId << " grief stage -> " << g.stagesRemaining << "\n";
        }
        if (g.intensity < 0.0f) g.intensity = 0.0f;
    }
    griefStates.erase(
        std::remove_if(griefStates.begin(), griefStates.end(),
            [](const GriefState& g) { return g.intensity <= 0.0f; }),
        griefStates.end()
    );
}

float Entity::getGriefIntensity() const {
    float total = 0.0f;
    for (const auto& g : griefStates) {
        total += g.intensity;
    }
    return std::min(1.0f, total);
}

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
    file << m_goals.size() << "\n"; //on inscrit d'abord la taille
    for(LifeGoal m_goal : m_goals){
        file << "GOAL:" << m_goal.type << "," << m_goal.priority << "," << m_goal.progressToward << "\n";
    }

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
    int nb_goals = stoi(line);
    for(int i=0; i<nb_goals;i++){
        std::getline(file, line);
        LifeGoal goal;
        std::string gdata = line.substr(5);
        size_t g1 = gdata.find(',');
        size_t g2 = gdata.find(',', g1 + 1);
        size_t g3 = gdata.find(',', g2 + 1);
        size_t g4 = gdata.find(',', g3 + 1);
        goal.type = gdata.substr(0, g1);
        goal.priority = std::stof(gdata.substr(g1 + 1, g2 - g1 - 1));
        goal.progressToward = std::stoi(gdata.substr(g2 + 1, g3 - g2 - 1));

        if (g3 != std::string::npos && g4 != std::string::npos) {
            goal.frustrationLevel = std::stof(gdata.substr(g3 + 1, g4 - g3 - 1));
            goal.ticksSinceProgress = std::stoi(gdata.substr(g4 + 1));
        } else {
            goal.frustrationLevel = 0.0f;
            goal.ticksSinceProgress = 0;
        }

        m_goals.push_back(goal);
    }

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

//remplacer par AddOrBoostGoal
//void Entity::setGoal(std::string type){
//    this->m_goal.type = type;
//    this->m_goal.progressToward = 0.0;
//}
//

void Entity::initializeHierarchicalNeeds() {
    // PHYSIOLOGICAL — fast decay, always fighting to stay satisfied
    needs["hunger"]  = HierarchicalNeed("hunger",  PHYSIOLOGICAL, 0.25f);
    needs["sleep"]   = HierarchicalNeed("sleep",   PHYSIOLOGICAL, 0.18f);
    needs["health"]  = HierarchicalNeed("health",  PHYSIOLOGICAL, 0.08f);
    needs["hygiene"] = HierarchicalNeed("hygiene", PHYSIOLOGICAL, 0.12f);

    // SAFETY — medium decay
    needs["safety"]  = HierarchicalNeed("safety",  SAFETY, 0.06f);

    // BELONGING — medium decay
    needs["social"]  = HierarchicalNeed("social",  BELONGING, 0.15f);
    needs["love"]    = HierarchicalNeed("love",    BELONGING, 0.10f);

    // ESTEEM — slow decay
    needs["achievement"] = HierarchicalNeed("achievement", ESTEEM, 0.05f);
    needs["recognition"] = HierarchicalNeed("recognition", ESTEEM, 0.04f);

    // SELF-ACTUALIZATION — slowest decay
    needs["meaning"]    = HierarchicalNeed("meaning",    SELF_ACTUALIZATION, 0.03f);
    needs["creativity"] = HierarchicalNeed("creativity", SELF_ACTUALIZATION, 0.02f);
}


LifeGoal Entity::SearchGoal(const std::string& goal_name) {
    //si on ne trouve pas on renvoie un nouveau
    for(LifeGoal goal : m_goals){
        if(goal.type == goal_name){
            return goal;
        }
    }
    LifeGoal new_life_goal;
    return new_life_goal;
}

void Entity::recalculatePriority() {
    for (LifeGoal& goal : m_goals) {
        if (goal.type == "find_partner" && !list_entityPointedCouple.empty()) {
            goal.priority *= 0.3f;
        }
        if (goal.type == "build_family" && list_entityPointedCouple.empty()) {
            goal.priority *= 0.1f;
        }
        if (goal.type == "build_career" && entityAge > 60) {
            goal.priority *= 0.5f;
        }
        if (goal.frustrationLevel > 70.0f) {
            goal.priority *= 0.6f;
        }
    }
}

void Entity::addOrBoostGoal(const std::string& goal_name, float value){
    for(LifeGoal& goal : m_goals){
        if(goal.type == goal_name){
            goal.priority += value;
            goal.progressToward += 2.0f;
            return ;
        }
    }
    LifeGoal new_life_goal;
    new_life_goal.type = goal_name;
    new_life_goal.priority = value;
    new_life_goal.progressToward = 1.0f;
    new_life_goal.frustrationLevel = 0.0f;
    new_life_goal.ticksSinceProgress = 0;
    m_goals.push_back(new_life_goal);
}

void Entity::onMajorEventAddOrBoostGoal(const std::string& eventType) {
    if (eventType == "loss_death") {
        addOrBoostGoal("find_meaning", 2.3f);
    }
    else if (eventType == "couple") {
        addOrBoostGoal("find_partner", 1.1f);
        addOrBoostGoal("build_family", 0.6f);
    }
    else if (eventType == "reproduction") {
        addOrBoostGoal("build_family", 2.5f);
    }
    else if(eventType == "good_connection"){
        addOrBoostGoal("make_friends", 0.7f);
    }
    else if (eventType == "betrayal") {
        addOrBoostGoal("self_protection", 0.8f);
    }
}


 float Entity::searchConnAng(Entity* ent){ // return just the value
    for(entityPointedAnger pointed: this->list_entityPointedAnger){
        if(ent == pointed.pointedEntity){
            return pointed.anger;
        }
    }
    return -1;
}

float Entity::searchConnDesire(Entity* ent){
    for(entityPointedDesire pointed: this->list_entityPointedDesire){
        if(ent == pointed.pointedEntity){
            return pointed.desire;
        }
    }
    return -1;
}


float Entity::searchConnSocial(Entity* ent){
    for(entityPointedSocial pointed: this->list_entityPointedSocial){
        if(ent == pointed.pointedEntity){
            return pointed.social;
        }
    }
    return -1;
}



void MentalModelOfOther::updateFromObservation(Entity* observed, float observerAccuracy) {
    // Lerp perceived traits toward observed reality, dampened by observer accuracy
    perceivedExtraversion  += (observed->personality.extraversion  - perceivedExtraversion)  * observerAccuracy * 0.1f;
    perceivedAgreeableness += (observed->personality.agreeableness - perceivedAgreeableness) * observerAccuracy * 0.1f;
    perceivedNeuroticism   += (observed->personality.neuroticism   - perceivedNeuroticism)   * observerAccuracy * 0.1f;
    estimatedHappiness = observed->entityHapiness * observerAccuracy + estimatedHappiness * (1.0f - observerAccuracy);
    estimatedAnger     = observed->entityGeneralAnger * observerAccuracy + estimatedAnger * (1.0f - observerAccuracy);
    estimatedStress    = observed->entityStress * observerAccuracy + estimatedStress * (1.0f - observerAccuracy);


}

MentalModelOfOther* Entity::getModelOf(Entity* ent) {
    for (MentalModelOfOther* md : list_MentalModelOfOther) {
        if (md->entityPointed == ent) return md;
    }
    return nullptr;
}
