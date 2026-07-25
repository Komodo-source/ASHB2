#include "./header/Entity.h"
#include "world/Lexicon.h"
#include "./header/random.hpp"
#include <cstddef>
#include <list>
#include <string>
#include "./header/FreeWillSystem.h"
#include "./header/SemanticMemory.h"
#include "./header/PlanningSystem.h"
#include "./header/CivilizationEngine.h"

#include <iostream>
#include <sstream>
//#include "../libs/BetterRand/BetterRand.h"
#include <time.h>
#include <random>
#include <algorithm>
#include <cmath>
#include "./header/FreeWillSystem.h"
#include "header/BetterRand.h"
#include "./header/LiveConfig.h"
#include "./header/SocialNormSystem.h"
#include "./header/ExternalData.h"

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
      entityBirthYear(-5000),
      entityAntiBody(15),
      entityDiseaseType(-1),
      posX(0.0f),
      posY(0.0f),
      selected(false),
      epigeneticMarkers(),
      intergenerationalTraumaLoad(0.0f)
{
    // Rebuild semantic memory index from any existing life memories
    semanticMemory.rebuildFromLifeMemories(this);
    initDrives();
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
               int antiBody,
               int diseaseType,
               std::string goalType,
               int birthYear)
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
      entityBirthYear(birthYear),
      entityAntiBody(antiBody),
      entityDiseaseType(diseaseType),
      posX(0.0f),
      posY(0.0f),
      selected(false),
      epigeneticMarkers(),
      intergenerationalTraumaLoad(0.0f)
{
    if(entitySex == 'A'){
        if(BetterRand::genNrInInterval(0,1)){
            entitySex = 'M';
        }else{
            entitySex = 'F';
        }
    }

    if(name.empty()){
        if (g_lexicon) {
            // Region-specific procedural name (neutral language until the
            // lineage's originRegionId is known; regenerated after assignment).
            name = g_lexicon->genName(originRegionId, entitySex);
        } else {
            int taille = male_name.size() - 1;
            if (entitySex == 'M') {
                int index = BetterRand::genNrInInterval(0, taille - 1);
                name = male_name.at(index);
            } else {
                int index = BetterRand::genNrInInterval(0, taille - 1);
                name = female_name.at(index);
            }
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

    // Rebuild semantic memory index from any existing life memories
    semanticMemory.rebuildFromLifeMemories(this);
    initDrives();
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
    entityBDay = (entityBDay + 1) % 365;
    if(this->entityAge < 10){
        this->entityLifeStage = LifeStage::CHILD;
    }else if(this->entityAge < 18){
        this->entityLifeStage = LifeStage::ADOLESCENT;
    }else if(this->entityAge < 65){
        this->entityLifeStage = LifeStage::ADULT;
    }else{
        this->entityLifeStage = LifeStage::ELDER;
    }

    // ── §8: infant and child mortality ──────────────────────────────────────
    // The hump at the bottom of every human lifespan distribution, and the
    // reason pre-modern life expectancy reads as ~40 while the adults who got
    // through childhood mostly died in their sixties and seventies (the note
    // on the Gompertz block below already assumed this hump existed; it did
    // not). Without it a world's demography has the wrong shape: no family
    // ever loses a child, so nothing about fertility, grief or heirs is
    // pressured the way it was for every human society before medicine.
    //
    // The odds are worst in the first year and fall away fast, and what moves
    // them is what actually moved them: the health the child is carrying,
    // whether it is going hungry, and whether it has any immunity built up.
    // Kill switch: demographyMul == 0 returns before the roll is drawn.
    if (entityAge >= 1.0f && entityAge < 6.0f && entityHealth > 0.0f
        && g_liveConfig.demographyMul != 0.0f) {
        float base    = (entityAge < 2.0f) ? 0.085f : 0.030f / (entityAge - 1.0f);
        float frailty = 1.0f + std::max(0.0f, 60.0f - entityHealth) / 60.0f;
        float hunger  = (entityHunger > 60.0f) ? 1.5f : 1.0f;
        float immune  = (entityAntiBody > 50) ? 0.7f : 1.0f;
        float hazard  = std::min(0.35f, base * frailty * hunger * immune)
                        * g_liveConfig.demographyMul;
        if (BetterRand::genNrInInterval<BetterRand::BERNOULI>(hazard)) {
            entityHealth = 0.0f;
            pendingDeathCause = "died in infancy";
        }
    }

    // Era-aware aging: elders lose health faster based on their era's life expectancy
    if (this->entityLifeStage == LifeStage::ELDER) {
        int currentYear = globalCivEngine ? globalCivEngine->getCurrentYear() : 0;
        float lifeExpectancy = getLifeExpectancy(currentYear);
        float agePenalty = std::max(0.0f, (entityAge - lifeExpectancy) * 0.04f);
        entityHealth -= agePenalty;
        if (entityAge > lifeExpectancy * 1.2f) {
            entityHealth -= 0.3f; // rapid decline past life expectancy
        }

        // Gompertz mortality anchored on a MODAL adult death age, not raw life
        // expectancy. (Historical life expectancy ~40 is dragged down by child
        // mortality; adults who survived childhood mostly died in their 60s-80s.)
        // Basing the hazard on lifeExpectancy made it ~70%/yr by elderhood, so
        // the whole cohort died the year it turned 65 — a demographic cliff
        // that dissolved every tribe at once around year 50.
        float modalAge  = std::max(lifeExpectancy * 1.5f, 66.0f);
        float yearsOver = entityAge - modalAge;
        if (yearsOver > 0.0f) {
            // ~5% at the modal age, doubling every 8 years, capped at 50%:
            // deaths spread across a ~25-year window instead of one cliff.
            float hazard = 0.05f * std::pow(2.0f, yearsOver / 8.0f);
            hazard = std::min(0.50f, hazard) * g_liveConfig.mortalityMul;  // M10 live console
            if (BetterRand::genNrInInterval<BetterRand::BERNOULI>(hazard)) {
                entityHealth = 0.0f;
                pendingDeathCause = "old age";
            }
        }
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


// Per-action stat history. Buffered in memory: the old version opened and
// closed TWO files on every action of every entity, which at Windows file-
// open latency dominated the whole tick (measured ~0.9 s/tick at only 100
// agents). flushEntityStats() writes the accumulated rows out — called when
// the buffer grows past a threshold, when the entity dies, and by the UI
// right before it reads the CSVs, so readers still see current data.
void Entity::saveEntityStats(Action* act) {
    {
        std::ostringstream row;
        row << this->entityAntiBody << ',' << this->entityBoredom << ',' << this->entityGeneralAnger << ',' << this->entityHapiness << ',' << this->entityHealth << ',' << this->entityHygiene << ',' << this->entityLoneliness << ',' << this->entityMentalHealth << ',' << this->entityStress << ',' << "\n";
        statsCsvBuffer += row.str();
    }
    {
        std::string changes;
        for (const StatChange& s : act->statChanges) {
            changes += s.statName + " => " + std::to_string(s.changeValue) + " ";
        }
        actsCsvBuffer += ',' + act->name + ",category: " + act->needCategory
                       + ",satisfaction: " + std::to_string(act->baseSatisfaction)
                       + ", outcome: " + std::to_string(act->outcomeSuccess)
                       + ',' + changes;
    }
    if (statsCsvBuffer.size() + actsCsvBuffer.size() > 32768) {
        flushEntityStats();
    }
}

void Entity::flushEntityStats() {
    if (!statsCsvBuffer.empty()) {
        std::ofstream file("./src/data/" + std::to_string(this->entityId) + ".csv", std::ios::app);
        if (file.is_open()) file << statsCsvBuffer;
        statsCsvBuffer.clear();
    }
    if (!actsCsvBuffer.empty()) {
        std::ofstream file2("./src/data/act_" + std::to_string(this->entityId) + ".csv", std::ios::app);
        if (file2.is_open()) file2 << actsCsvBuffer;
        actsCsvBuffer.clear();
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

    // Phase 6: a severe bereavement leaves a methylation mark future children
    // may inherit (grief that never fully resolves reshapes the next
    // generation too) — gated on death + real intensity, not every breakup.
    if (isDeath && gs.intensity > 0.5f) {
        addEpigeneticMarker("loss", gs.intensity * 100.0f, 0);
    }
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
    file << "ORIGIN_REGION:" << originRegionId << "\n";
    file << "PERSONALITY:" << personality.extraversion << "," << personality.agreeableness << ","
         << personality.conscientiousness << "," << personality.neuroticism << "," << personality.openness << "\n";
    file << m_goals.size() << "\n"; //on inscrit d'abord la taille
    for(LifeGoal m_goal : m_goals){
        file << "GOAL:" << m_goal.type << "," << m_goal.priority << "," << m_goal.progressToward << "\n";
    }

    // Save relationship lists (store target entity IDs, not pointers).
    // Skip links whose target has died and been nulled out — dereferencing a
    // null pointedEntity here used to crash the whole save. We count the valid
    // links first so COUNT always matches the lines that follow.
    auto countValid = [](const auto& list) {
        int n = 0; for (const auto& e : list) if (e.pointedEntity) ++n; return n;
    };

    file << "DESIRE_COUNT:" << countValid(list_entityPointedDesire) << "\n";
    for (const auto& d : list_entityPointedDesire) {
        if (!d.pointedEntity) continue;
        file << "DESIRE:" << d.pointedEntity->entityId << "," << d.desire << "\n";
    }
    file << "ANGER_COUNT:" << countValid(list_entityPointedAnger) << "\n";
    for (const auto& a : list_entityPointedAnger) {
        if (!a.pointedEntity) continue;
        file << "ANGER_LINK:" << a.pointedEntity->entityId << "," << a.anger << "\n";
    }
    file << "SOCIAL_COUNT:" << countValid(list_entityPointedSocial) << "\n";
    for (const auto& s : list_entityPointedSocial) {
        if (!s.pointedEntity) continue;
        file << "SOCIAL:" << s.pointedEntity->entityId << "," << s.social << "\n";
    }
    file << "COUPLE_COUNT:" << countValid(list_entityPointedCouple) << "\n";
    for (const auto& c : list_entityPointedCouple) {
        if (!c.pointedEntity) continue;
        file << "COUPLE:" << c.pointedEntity->entityId << "\n";
    }

    // Economy: wallet balance + which good this entity produces.
    file << "SALARY:" << salary.token << "," << salary.producedProduct << "\n";

    // Save FreeWillSystem
    fws.saveTo(file);

    // Save SemanticMemorySystem
    semanticMemory.saveTo(file);

    // Save PlanningSystem
    planner.saveTo(file);

    // Save Epigenetic and Generational Trauma Systems
    file << "EPIGENETIC_COUNT:" << epigeneticMarkers.size() << "\n";
    for (const auto& marker : epigeneticMarkers) {
        file << "EPIGENETIC:" << marker.traumaSource << ","
             << marker.methylationLevel << ","
             << marker.generationOffset << ","
             << marker.expressionLevel << "\n";
    }
    file << "INTERGEN_TRAUMA_LOAD:" << intergenerationalTraumaLoad << "\n";

    // Save Biological Homeostasis & Disease Vector Systems (Phase 5)
    file << "BIOSTATE:" << biology.nutritionalStatus << ","
         << biology.hydrationLevel << "," << biology.energyLevel << "\n";
    file << "BASEIMMUNITY:" << baseImmunity << "\n";
    file << "PATHOGEN_COUNT:" << pathogenExposures.size() << "\n";
    for (const auto& p : pathogenExposures) {
        file << "PATHOGEN:" << p.pathogenId << "," << p.exposureDay << ","
             << p.viralLoad << "," << (p.isInfected ? 1 : 0) << ","
             << (p.isContagious ? 1 : 0) << "," << p.daysInfected << ","
             << p.immunityLevel << "\n";
    }

    // Society layer: tribe/religion/family membership, role and standing.
    // Appended at the very end so older loaders simply never reach these lines.
    file << "TRIBEID:" << tribeId << "\n";
    file << "RELIGIONID:" << religionId << "\n";
    file << "FAMILYID:" << familyId << "\n";
    file << "SPECIALIZATION:" << specialization << "\n";
    file << "ISSPEC:" << (isSpecialist ? 1 : 0) << "\n";
    file << "ROLESINCE:" << roleSinceDay << "\n";
    file << "AUCTORITAS:" << auctoritas << "\n";
    file << "INTEGRITY:" << integrity << "\n";
    file << "DOMRANK:" << dominanceRank << "\n";

    // ── AI upgrade phases A-E: emotions, skills, sleep, injury, intention,
    // knowledge. Append-only: older loaders never reach unknown keys because
    // the tail dispatcher below knows all of them; saves that predate them
    // simply keep the defaults.
    file << "EMOTIONS:" << emotions.fear << ',' << emotions.joy << ','
         << emotions.sadness << ',' << emotions.shame << ',' << emotions.guilt << ','
         << emotions.envy << ',' << emotions.gratitude << ',' << emotions.pride << ','
         << emotions.hope << ',' << emotions.regret << "\n";
    file << "SKILLS:";
    for (int i = 0; i < SK_COUNT; ++i) file << (i ? "," : "") << skills.v[i];
    file << "\n";
    file << "SLEEPST:" << sleepPressure << ',' << sleepQuality << "\n";
    file << "INJURY:" << injuryLevel << "\n";
    file << "HOMEATT:" << homeAttachment << ',' << tribeSwitchDay << "\n";
    file << "INTENT:" << (intention.active ? 1 : 0) << ',' << intention.sinceDay << ','
         << intention.lastProgressDay << ',' << intention.progress << ','
         << intention.type << "\n";
    for (const KnownFact& k : knowledge.facts)
        file << "FACT:" << k.subjectId << ',' << (int)k.predicate << ',' << k.value << ','
             << k.confidence << ',' << k.sourceId << ',' << k.day << ','
             << (k.isTrue ? 1 : 0) << "\n";

    // ── I-P1: narrative identity, sense of purpose, life-story chapters ──
    file << "NARRID:" << narrativeIdentity.selfStory << ',' << narrativeIdentity.dominantValue << ','
         << narrativeIdentity.definingMemoryIdx << ',' << narrativeIdentity.coherence << "\n";
    file << "PURPOSE:" << senseOfPurpose << "\n";
    file << "STRESSBL:" << stressBaseline << "\n";
    file << "LINEAGE:" << lineageDepth << "\n";
    // III-P4 cultural capital and IV-P1 the trait set that signals it: both are
    // slow, inherited quantities, so a reloaded world that lost them would
    // restart every lineage's cultural standing from scratch.
    file << "CULTCAP:" << culturalCapital << "\n";
    file << "CULTTRAITS:" << cultureTraits << "\n";
    file << "CULTHELD:" << committedTraits << "\n";
    file << "CHAPTERS:" << lifeChapters.size() << "\n";
    for (const LifeChapter& c : lifeChapters)
        file << "CHAPTER:" << c.day << ',' << c.otherId << ',' << c.title << ',' << c.note << "\n";

    // Web bridge: MySQL characters.id this entity embodies (-1 = pure AI).
    // MUST stay the LAST key — older loaders rewind on the unknown line and
    // newer loaders presence-guard it, so saves stay append-only compatible.
    file << "WEBCHARID:" << webCharId << "\n";

    file << "--- END ENTITY ---\n";
}

bool Entity::loadFrom(std::ifstream& file) {
    std::string line;

    // Read entity header
    std::getline(file, line); // "--- ENTITY <id> ---"
    if (line.rfind("--- ENTITY", 0) != 0) {
        // Truncated or corrupt save (e.g. a pre-V2 binary planner blob whose
        // 0x1A byte read as DOS EOF): report and let the caller stop cleanly
        // instead of crashing on the field parses below.
        std::cerr << "loadFrom: misaligned entity block (header='" << line
                  << "' eof=" << file.eof() << ") — stopping entity load\n";
        return false;
    }

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
    std::getline(file, line); originRegionId = std::stoi(line.substr(14)); // "ORIGIN_REGION:"
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

    // Economy: wallet balance + produced good. Tolerant of older saves that
    // predate this line: peek one line, and if it isn't a SALARY record, rewind
    // the stream so FreeWillSystem reads it as its own header.
    std::streampos beforeSalary = file.tellg();
    std::getline(file, line);
    if (line.rfind("SALARY:", 0) == 0) {
        std::string sdata = line.substr(7);
        size_t comma = sdata.find(',');
        salary.token = std::stof(sdata.substr(0, comma));
        salary.producedProduct = (comma != std::string::npos)
                                 ? std::stoi(sdata.substr(comma + 1)) : -1;
    } else {
        file.seekg(beforeSalary); // old save — give the line back to fws
    }

    // Load FreeWillSystem
    fws.loadFrom(file);

    // Load SemanticMemorySystem
    semanticMemory.loadFrom(file);

    // Load PlanningSystem
    planner.loadFrom(file);

    // ── Append-only tail: society layer, web identity, end marker ────────────
    // Read lines until the "--- END ENTITY ---" marker, dispatching every key
    // we know; keys a save predates are simply absent and keep their defaults.
    // This deliberately does NOT use the tellg/peek/seekg rewind idiom the old
    // society guard used: on this toolchain (MinGW text-mode streams) tellg()
    // mid-buffer is offset by the CRLFs still sitting in the internal buffer,
    // so seekg() back landed PAST unread lines and corrupted the stream for
    // any save missing an optional key (crashed loading pre-society saves).
    // CSV splitter for the AI-upgrade keys (EMOTIONS/SKILLS/... below).
    auto splitFloats = [](const std::string& s) {
        std::vector<float> out;
        std::stringstream ss(s); std::string tok;
        while (std::getline(ss, tok, ','))
            { try { out.push_back(std::stof(tok)); } catch (...) { out.push_back(0.0f); } }
        return out;
    };

    // One unreadable key must not take the whole world down with it. Every
    // conversion below is a std::sto* that THROWS on an empty or malformed
    // value, and the throw was unhandled: loading any save whose tail carried
    // one such line aborted the process with nothing but "terminate called
    // after throwing an instance of 'std::invalid_argument'" — no key, no line,
    // no world. A save you cannot open is not a save, and the whole point of
    // this tail is that a file may legitimately be from an older or newer
    // build. So a line that cannot be parsed is skipped, its field keeps its
    // default, and the first few are named on stderr so the mismatch is
    // findable instead of fatal.
    int loadComplaints = 0;
    while (std::getline(file, line)) {
      try {
        if      (line.rfind("TRIBEID:", 0) == 0)        tribeId       = std::stoi(line.substr(8));
        else if (line.rfind("RELIGIONID:", 0) == 0)     religionId    = std::stoi(line.substr(11));
        else if (line.rfind("FAMILYID:", 0) == 0)       familyId      = std::stoi(line.substr(9));
        else if (line.rfind("SPECIALIZATION:", 0) == 0) {
            specialization = line.substr(15);
            if (specialization.empty()) specialization = "farmer";
        }
        else if (line.rfind("ISSPEC:", 0) == 0)         isSpecialist  = (std::stoi(line.substr(7)) != 0);
        else if (line.rfind("ROLESINCE:", 0) == 0)      roleSinceDay  = std::stoi(line.substr(10));
        else if (line.rfind("AUCTORITAS:", 0) == 0)     auctoritas    = std::stof(line.substr(11));
        else if (line.rfind("INTEGRITY:", 0) == 0)      integrity     = std::stof(line.substr(10));
        else if (line.rfind("DOMRANK:", 0) == 0)        dominanceRank = std::stof(line.substr(8));
        else if (line.rfind("EMOTIONS:", 0) == 0) {
            auto f = splitFloats(line.substr(9));
            if (f.size() >= 10) {
                emotions.fear = f[0]; emotions.joy = f[1]; emotions.sadness = f[2];
                emotions.shame = f[3]; emotions.guilt = f[4]; emotions.envy = f[5];
                emotions.gratitude = f[6]; emotions.pride = f[7];
                emotions.hope = f[8]; emotions.regret = f[9];
            }
        }
        else if (line.rfind("SKILLS:", 0) == 0) {
            auto f = splitFloats(line.substr(7));
            for (int i = 0; i < SK_COUNT && i < (int)f.size(); ++i) skills.v[i] = f[i];
        }
        else if (line.rfind("SLEEPST:", 0) == 0) {
            auto f = splitFloats(line.substr(8));
            if (f.size() >= 2) { sleepPressure = f[0]; sleepQuality = f[1]; }
        }
        else if (line.rfind("INJURY:", 0) == 0)         injuryLevel   = std::stof(line.substr(7));
        else if (line.rfind("HOMEATT:", 0) == 0) {
            auto f = splitFloats(line.substr(8));
            if (f.size() >= 2) { homeAttachment = f[0]; tribeSwitchDay = (int)f[1]; }
        }
        else if (line.rfind("INTENT:", 0) == 0) {
            // INTENT:active,sinceDay,lastProgressDay,progress,type — type last
            // because it is the only non-numeric field.
            std::string body = line.substr(7);
            size_t p = 0; int fieldNo = 0; std::string tok;
            std::stringstream ss(body);
            std::vector<std::string> parts;
            while (std::getline(ss, tok, ',')) parts.push_back(tok);
            (void)p; (void)fieldNo;
            if (parts.size() >= 5) {
                try {
                    intention.active          = (std::stoi(parts[0]) != 0);
                    intention.sinceDay        = std::stoi(parts[1]);
                    intention.lastProgressDay = std::stoi(parts[2]);
                    intention.progress        = std::stof(parts[3]);
                    intention.type            = parts[4];
                } catch (...) { intention = Intention{}; }
            }
        }
        else if (line.rfind("FACT:", 0) == 0) {
            auto f = splitFloats(line.substr(5));
            if (f.size() >= 7) {
                KnownFact k;
                k.subjectId  = (int)f[0];
                k.predicate  = (uint8_t)(int)f[1];
                k.value      = f[2];
                k.confidence = f[3];
                k.sourceId   = (int)f[4];
                k.day        = (int)f[5];
                k.isTrue     = (f[6] != 0.0f);
                knowledge.remember(k);
            }
        }
        else if (line.rfind("EPIGENETIC_COUNT:", 0) == 0) {
            // "EPIGENETIC_COUNT:" is seventeen characters; reading from
            // eighteen skipped the digit and handed std::stoi an empty string,
            // which threw and — before the guard above existed — aborted the
            // program. This one off-by-one is why no save in saves/ could be
            // opened at all.
            int cnt = std::stoi(line.substr(17));
            epigeneticMarkers.clear();
            for (int i = 0; i < cnt && std::getline(file, line); ++i) {
                if (line.rfind("EPIGENETIC:", 0) != 0) break;
                std::stringstream ss(line.substr(11));
                std::string tok; std::vector<std::string> parts;
                while (std::getline(ss, tok, ',')) parts.push_back(tok);
                if (parts.size() >= 4) {
                    try {
                        EpigeneticMarker m;
                        m.traumaSource     = parts[0];
                        m.methylationLevel = std::stof(parts[1]);
                        m.generationOffset = std::stoi(parts[2]);
                        m.expressionLevel  = std::stof(parts[3]);
                        epigeneticMarkers.push_back(m);
                    } catch (...) {}
                }
            }
        }
        else if (line.rfind("INTERGEN_TRAUMA_LOAD:", 0) == 0)
            intergenerationalTraumaLoad = std::stof(line.substr(21));
        else if (line.rfind("BIOSTATE:", 0) == 0) {
            auto f = splitFloats(line.substr(9));
            if (f.size() >= 3) {
                biology.nutritionalStatus = f[0];
                biology.hydrationLevel    = f[1];
                biology.energyLevel       = f[2];
            }
        }
        else if (line.rfind("BASEIMMUNITY:", 0) == 0)
            baseImmunity = std::stof(line.substr(13));
        else if (line.rfind("PATHOGEN_COUNT:", 0) == 0) {
            int cnt = std::stoi(line.substr(15));
            pathogenExposures.clear();
            for (int i = 0; i < cnt && std::getline(file, line); ++i) {
                if (line.rfind("PATHOGEN:", 0) != 0) break;
                auto f = splitFloats(line.substr(9));
                if (f.size() >= 7) {
                    PathogenExposure p;
                    p.pathogenId    = (int)f[0];
                    p.exposureDay   = (int)f[1];
                    p.viralLoad     = f[2];
                    p.isInfected    = (f[3] != 0.0f);
                    p.isContagious  = (f[4] != 0.0f);
                    p.daysInfected  = (int)f[5];
                    p.immunityLevel = (int)f[6];
                    pathogenExposures.push_back(p);
                }
            }
        }
        else if (line.rfind("NARRID:", 0) == 0) {
            std::stringstream ss(line.substr(7)); std::string tok; std::vector<std::string> p;
            while (std::getline(ss, tok, ',')) p.push_back(tok);
            if (p.size() >= 4) {
                narrativeIdentity.selfStory     = p[0];
                narrativeIdentity.dominantValue = p[1];
                try { narrativeIdentity.definingMemoryIdx = std::stoi(p[2]);
                      narrativeIdentity.coherence         = std::stof(p[3]); } catch (...) {}
                narrativeIdentity.lastStory = narrativeIdentity.selfStory;
            }
        }
        else if (line.rfind("PURPOSE:", 0) == 0)         senseOfPurpose = std::stof(line.substr(8));
        else if (line.rfind("STRESSBL:", 0) == 0)        stressBaseline = std::stof(line.substr(9));
        else if (line.rfind("LINEAGE:", 0) == 0)         lineageDepth  = std::stoi(line.substr(8));
        else if (line.rfind("CULTCAP:", 0) == 0)         culturalCapital = std::stof(line.substr(8));
        else if (line.rfind("CULTTRAITS:", 0) == 0)      cultureTraits = std::stoull(line.substr(11));
        else if (line.rfind("CULTHELD:", 0) == 0)        committedTraits = std::stoull(line.substr(9));
        else if (line.rfind("CHAPTERS:", 0) == 0) {
            int cnt = 0; try { cnt = std::stoi(line.substr(9)); } catch (...) { cnt = 0; }
            lifeChapters.clear();
            for (int i = 0; i < cnt && std::getline(file, line); ++i) {
                if (line.rfind("CHAPTER:", 0) != 0) break;
                // day,otherId,title,note — note may contain commas, so take the rest.
                std::string body = line.substr(8);
                size_t c1 = body.find(','), c2 = body.find(',', c1 + 1),
                       c3 = (c2 == std::string::npos) ? std::string::npos : body.find(',', c2 + 1);
                if (c1 == std::string::npos || c2 == std::string::npos || c3 == std::string::npos) continue;
                LifeChapter ch;
                try { ch.day     = std::stoi(body.substr(0, c1));
                      ch.otherId = std::stoi(body.substr(c1 + 1, c2 - c1 - 1)); } catch (...) { continue; }
                ch.title = body.substr(c2 + 1, c3 - c2 - 1);
                ch.note  = body.substr(c3 + 1);
                lifeChapters.push_back(ch);
            }
        }
        else if (line.rfind("WEBCHARID:", 0) == 0)      webCharId     = std::stoi(line.substr(10));
        else if (line.rfind("--- END ENTITY", 0) == 0)  break;
        // Unknown key (a future format, or pre-V2 planner residue): skip it
        // and resync on the END marker so one stray line can't desync every
        // entity that follows.
      } catch (const std::exception& ex) {
        if (loadComplaints++ < 5)
            std::cerr << "LOAD: skipped unreadable line \""
                      << line.substr(0, std::min<size_t>(line.size(), 48))
                      << "\" (" << ex.what() << ") — field left at its default\n";
      }
    }

    // Rebuild semantic memory index from loaded life memories
    semanticMemory.rebuildFromLifeMemories(this);

    selected = false;
    return true;
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

void Entity::initDrives() {
    // Personality shapes the sweet spot: open minds crave more stimulation,
    // extraverts need more company, neurotics tolerate less deviation, the
    // disciplined decay a little slower.
    drives.initHuman(personality.openness, personality.extraversion,
                     personality.neuroticism, personality.conscientiousness);
}

void Entity::initPsychology() {
    initDrives();
    // Type derives from the Big Five (one coherent person); seed off the entity
    // id + traits so it's deterministic per individual yet varies between them.
    unsigned seed = static_cast<unsigned>(
        entityId * 2654435761u
        ^ (static_cast<unsigned>(personality.openness * 7.0f)      << 1)
        ^ (static_cast<unsigned>(personality.extraversion * 13.0f) << 5)
        ^ (static_cast<unsigned>(personality.agreeableness * 17.0f)<< 9));
    cognition.deriveFromBigFive(personality.extraversion, personality.openness,
                                personality.agreeableness, personality.conscientiousness,
                                personality.neuroticism, seed);
    cognition.applyToDrives(drives);
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
        addOrBoostGoal("find_partner", 0.8f);
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



void MentalModelOfOther::updateFromObservation(Entity* observed, float observerAccuracy, int simDay) {
    // Lerp perceived traits toward observed reality, dampened by observer accuracy
    perceivedExtraversion  += (observed->personality.extraversion  - perceivedExtraversion)  * observerAccuracy * 0.1f;
    perceivedAgreeableness += (observed->personality.agreeableness - perceivedAgreeableness) * observerAccuracy * 0.1f;
    perceivedNeuroticism   += (observed->personality.neuroticism   - perceivedNeuroticism)   * observerAccuracy * 0.1f;
    estimatedHappiness = observed->entityHapiness * observerAccuracy + estimatedHappiness * (1.0f - observerAccuracy);
    estimatedAnger     = observed->entityGeneralAnger * observerAccuracy + estimatedAnger * (1.0f - observerAccuracy);
    estimatedStress    = observed->entityStress * observerAccuracy + estimatedStress * (1.0f - observerAccuracy);

    // Repeated observation makes the other more legible: confidence and
    // predictability both rise, faster for a perceptive (accurate) observer.
    lastObservedDay = simDay;
    confidence      = std::min(1.0f, confidence + 0.15f * observerAccuracy);
    predictability  = std::min(1.0f, predictability + 0.03f * observerAccuracy);
    lastInteractionDay = (float)simDay;
}

float MentalModelOfOther::effectiveConfidence(int today) const {
    float age = (float)std::max(0, today - lastObservedDay);
    return confidence * std::exp(-age / 120.0f);   // ~half strength after ~83 days
}

MentalModelOfOther* Entity::getModelOf(Entity* ent) {
    for (MentalModelOfOther* md : list_MentalModelOfOther) {
        if (md->entityPointed == ent) return md;
    }
    return nullptr;
}


void Entity::upgradeDesire(Entity* pointed, float value){
    for (entityPointedDesire list_ptr: list_entityPointedDesire) {
        if(pointed == list_ptr.pointedEntity){
            list_ptr.desire += value;
            return ;
        }
    }
}
void Entity::upgradeAnger(Entity* pointed, float value){
    for (entityPointedAnger list_ptr : list_entityPointedAnger) {
        if(pointed == list_ptr.pointedEntity){
            list_ptr.anger += value;
            return ;
        }

    }
}
void Entity::upgradeSocial(Entity* pointed, float value){
    for (entityPointedSocial list_ptr:  list_entityPointedSocial) {
        if(list_ptr.pointedEntity == pointed){
            list_ptr.social += value;
            return ;
        }

    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 3 – PAD Emotional Model
// ─────────────────────────────────────────────────────────────────────────────
void Entity::updatePAD() {
    pad = computePAD(
        entityHapiness, entityStress, entityMentalHealth,
        entityGeneralAnger, entityLoneliness,
        personality.extraversion, personality.agreeableness, personality.neuroticism,
        SelfConcept.selfEsteem
    );
    bodyLanguage = deriveBodyLanguage(pad, getGriefIntensity());
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 4 – Contextual self-grounding
// ─────────────────────────────────────────────────────────────────────────────
void Entity::updateSelfGrounding(int simDay) {
    std::ostringstream ss;
    ss << "I am " << name << ". Day " << simDay << ". ";
    ss << "Primary goal: " << (m_goals.empty() ? "none" : m_goals[0].type) << ". ";
    ss << "Health: " << (int)entityHealth
       << "  Stress: " << (int)entityStress
       << "  Mood: " << bodyLanguageCueLabel(bodyLanguage) << ".";
    if (tribeId >= 0)    ss << "  Tribe: #" << tribeId << ".";
    if (religionId >= 0) ss << "  Faith: #" << religionId << ".";
    selfGrounding = ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2 – Memory consolidation into core beliefs
// ─────────────────────────────────────────────────────────────────────────────
void Entity::consolidateMemories(int simDay) {
    std::map<std::string, int>   eventCounts;
    std::map<std::string, float> eventIntensities;

    for (const auto& mem : lifeMemories) {
        eventCounts[mem.eventType]++;
        eventIntensities[mem.eventType] += mem.emotionalIntensity;
    }

    for (auto& [type, count] : eventCounts) {
        if (count < 3) continue;
        float avgIntensity = eventIntensities[type] / count;

        // Reinforce existing belief
        bool found = false;
        for (auto& belief : coreBeliefs) {
            if (belief.category == type) {
                belief.reinforcementCount++;
                belief.strength = std::min(100.0f, belief.strength + 5.0f);
                found = true;
                break;
            }
        }
        if (found) continue;

        // Form new core belief
        CoreBelief b;
        b.category           = type;
        b.formedOnDay        = simDay;
        b.reinforcementCount = count;
        b.strength           = std::min(100.0f, (float)count * 8.0f);

        if      (type == "loss_death")       { b.belief = "Loss is inevitable. I must not get too attached."; b.valence = -60.0f; }
        else if (type == "romantic_success") { b.belief = "I am capable of being loved.";                     b.valence =  70.0f; }
        else if (type == "conflict")         { b.belief = "People cannot be fully trusted.";                  b.valence = -35.0f; }
        else if (type == "trauma")           { b.belief = "The world is not safe. I must protect myself.";    b.valence = -50.0f; }
        else if (type == "positive_bond")    { b.belief = "Connection with others gives my life meaning.";    b.valence =  55.0f; }
        else {
            b.belief  = "Experiences like '" + type + "' have shaped who I am.";
            b.valence = avgIntensity > 0.5f ? -20.0f : 30.0f;
        }
        coreBeliefs.push_back(b);
    }

    // Prune to 5 strongest beliefs
    if (coreBeliefs.size() > 5) {
        std::sort(coreBeliefs.begin(), coreBeliefs.end(),
            [](const CoreBelief& a, const CoreBelief& b){ return a.strength > b.strength; });
        coreBeliefs.resize(5);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// M4 – Episodic forgetting
// Memories fade on the same exp(-age/300) curve the decision biases use, so a
// memory is only dropped once it no longer influences behavior anyway. By then
// consolidateMemories has had many chances to distil it into a core belief —
// the event's *lesson* outlives its episode, which is how human memory works.
// ─────────────────────────────────────────────────────────────────────────────
bool Entity::pruneLifeMemories(int simDay) {
    const float  MIN_SALIENCE = 0.05f;  // below the bias functions' noise floor
    const int    GRACE_DAYS   = 60;     // nothing is forgotten while still fresh
    const size_t HARD_CAP     = 48;     // absolute episodic capacity

    const size_t before = lifeMemories.size();

    auto salience = [simDay](const LifeMemory& m) {
        float decay = std::exp(-(float)(simDay - m.simulationDay) / 300.0f);
        return m.emotionalIntensity * decay;
    };

    lifeMemories.erase(
        std::remove_if(lifeMemories.begin(), lifeMemories.end(),
            [&](const LifeMemory& m) {
                if (m.isFormative) return false;
                if (simDay - m.simulationDay < GRACE_DAYS) return false;
                return salience(m) < MIN_SALIENCE;
            }),
        lifeMemories.end());

    // Capacity overflow: evict the faintest non-formative episodes first.
    if (lifeMemories.size() > HARD_CAP) {
        std::stable_sort(lifeMemories.begin(), lifeMemories.end(),
            [&](const LifeMemory& a, const LifeMemory& b) {
                if (a.isFormative != b.isFormative) return a.isFormative;
                return salience(a) > salience(b);
            });
        lifeMemories.resize(HARD_CAP);
        // Restore chronological order — narrative and consolidation code
        // treats the vector as a life story.
        std::stable_sort(lifeMemories.begin(), lifeMemories.end(),
            [](const LifeMemory& a, const LifeMemory& b) {
                return a.simulationDay < b.simulationDay;
            });
    }

    return lifeMemories.size() != before;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2 – Working memory push
// ─────────────────────────────────────────────────────────────────────────────
void Entity::addToWorkingMemory(const std::string& eventType,
                                const std::string& desc, float weight) {
    for (auto& e : workingMemory) e.ticksAgo++;

    WorkingMemoryEntry entry;
    entry.eventType     = eventType;
    entry.description   = desc;
    entry.emotionalWeight = weight;
    entry.ticksAgo      = 0;
    workingMemory.push_front(entry);

    if (workingMemory.size() > 5)
        workingMemory.resize(5);
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 6: Epigenetic and Generational Trauma Systems
// ─────────────────────────────────────────────────────────────────────────────
void Entity::addEpigeneticMarker(std::string traumaSource, float methylationLevel, int generationOffset) {
    // LiveConfig kill switch: ×0 = the whole system contributes nothing
    // (bit-exact no-op), matching the M10 multiplier contract.
    if (g_liveConfig.epigeneticsMul <= 0.0f) return;
    EpigeneticMarker marker;
    marker.traumaSource = traumaSource;
    marker.methylationLevel = std::clamp(methylationLevel * g_liveConfig.epigeneticsMul, 0.0f, 100.0f);
    marker.generationOffset = generationOffset;
    marker.expressionLevel = marker.methylationLevel; // Initially expressed at same level as methylation
    epigeneticMarkers.push_back(marker);

    // Bounded like every other unbounded-in-principle per-entity list
    // (lifeMemories, knowledge.facts): a lifetime of repeated famines/wars
    // shouldn't grow this vector without limit. Evict the weakest-expressed
    // mark first so the traits that actually still matter survive.
    constexpr size_t MARKER_CAP = 24;
    if (epigeneticMarkers.size() > MARKER_CAP) {
        auto weakest = std::min_element(epigeneticMarkers.begin(), epigeneticMarkers.end(),
            [](const EpigeneticMarker& a, const EpigeneticMarker& b) {
                return a.expressionLevel < b.expressionLevel;
            });
        epigeneticMarkers.erase(weakest);
    }
}

void Entity::updateEpigeneticExpression(float environmentalSupport) {
    // environmentalSupport: 0-100, higher = more supportive environment
    // In supportive environments, trauma-related methylation is expressed less
    // In harsh environments, trauma-related methylation is expressed more

    float supportFactor = environmentalSupport / 100.0f; // 0-1
    for (auto& marker : epigeneticMarkers) {
        // Inverse relationship: high support = low expression of trauma markers
        // Low support = high expression of trauma markers
        float expressionModifier = 1.0f - supportFactor;
        marker.expressionLevel = marker.methylationLevel * (0.5f + 0.5f * expressionModifier);
        // Clamp to 0-100 range
        marker.expressionLevel = std::clamp(marker.expressionLevel, 0.0f, 100.0f);
    }
}

void Entity::applyEpigeneticEffects() {
    // Apply epigenetic effects to personality, drives, and mental health
    float totalTraumaImpact = 0.0f;
    int activeMarkers = 0;

    for (const auto& marker : epigeneticMarkers) {
        // Only consider markers with significant expression
        if (marker.expressionLevel > 10.0f) {
            float impact = marker.expressionLevel / 100.0f; // 0-1
            totalTraumaImpact += impact;
            activeMarkers++;

            // Different trauma sources affect different aspects
            if (marker.traumaSource == "war" || marker.traumaSource == "conflict") {
                // War trauma increases aggression, anxiety, decreases trust
                personality.neuroticism += impact * 5.0f;
                personality.agreeableness -= impact * 3.0f;
                entityGeneralAnger += impact * 4.0f;
            } else if (marker.traumaSource == "famine" || marker.traumaSource == "starvation") {
                // Famine trauma increases hoarding, decreases trust, increases neuroticism
                personality.neuroticism += impact * 4.0f;
                personality.agreeableness -= impact * 2.0f;
                ValueSystem.hedonism -= impact * 3.0f; // Less pleasure-seeking, more survival-focused
            } else if (marker.traumaSource == "loss" || marker.traumaSource == "grief") {
                // Loss trauma increases neuroticism, decreases extraversion
                personality.neuroticism += impact * 5.0f;
                personality.extraversion -= impact * 3.0f;
                entityMentalHealth -= impact * 3.0f;
            } else if (marker.traumaSource == "abuse" || marker.traumaSource == "violence") {
                // Abuse trauma increases anxiety, aggression, decreases trust
                personality.neuroticism += impact * 6.0f;
                personality.agreeableness -= impact * 4.0f;
                entityGeneralAnger += impact * 5.0f;
                entityMentalHealth -= impact * 4.0f;
            }
        }
    }

    // Update intergenerational trauma load (average of active markers)
    if (activeMarkers > 0) {
        intergenerationalTraumaLoad = (totalTraumaImpact / activeMarkers) * 100.0f;
    } else {
        intergenerationalTraumaLoad = 0.0f;
    }

    // Apply general mental health impact from accumulated trauma
    if (intergenerationalTraumaLoad > 10.0f) {
        entityMentalHealth -= (intergenerationalTraumaLoad - 10.0f) * 0.5f;
        entityMentalHealth = std::max(0.0f, std::min(100.0f, entityMentalHealth));
    }

    // Ensure personality traits stay in valid range
    personality.extraversion = std::clamp(personality.extraversion, 0.0f, 100.0f);
    personality.agreeableness = std::clamp(personality.agreeableness, 0.0f, 100.0f);
    personality.conscientiousness = std::clamp(personality.conscientiousness, 0.0f, 100.0f);
    personality.neuroticism = std::clamp(personality.neuroticism, 0.0f, 100.0f);
    personality.openness = std::clamp(personality.openness, 0.0f, 100.0f);
}

void Entity::inheritEpigeneticMarkers(Entity* parent1, Entity* parent2) {
    // Inherit epigenetic markers from parents with some reduction
    // Each generation, epigenetic marks fade somewhat unless reinforced
    if (g_liveConfig.epigeneticsMul <= 0.0f) return; // kill switch: bit-exact no-op

    auto inheritFromParent = [this](Entity* parent) {
        if (!parent) return;

        for (const auto& marker : parent->epigeneticMarkers) {
            // Only inherit markers that have significant expression
            if (marker.expressionLevel > 5.0f) {
                EpigeneticMarker inheritedMarker;
                inheritedMarker.traumaSource = marker.traumaSource;
                // Methylation level decreases with each generation (epigenetic decay)
                inheritedMarker.methylationLevel = marker.methylationLevel * 0.7f;
                // Generation offset increases by 1
                inheritedMarker.generationOffset = marker.generationOffset + 1;
                // Expression level starts same as methylation but will be updated by environment
                inheritedMarker.expressionLevel = inheritedMarker.methylationLevel;
                epigeneticMarkers.push_back(inheritedMarker);
            }
        }
    };

    inheritFromParent(parent1);
    inheritFromParent(parent2);

    // Also update the individual's own intergenerational trauma load based on inherited markers
    updateEpigeneticExpression(50.0f); // Assume neutral environment initially
    applyEpigeneticEffects();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 5: Biological Homeostasis & Disease Vectors
// ─────────────────────────────────────────────────────────────────────────────
void Entity::resolveBiologicalHomeostasis() {
    // Net energy balance for today, in the same arbitrary units the caloric
    // bumps are recorded in at the action-execution sites (implem_free_will.cpp).
    float net = biology.caloricIntakeToday - biology.caloricExpenditureToday;

    // nutritionalStatus drifts toward a target set by today's balance, blended
    // with the existing hunger/foodStore subsistence stat so this layer adds
    // texture on top of survival rather than fighting it for authority.
    float target = std::clamp(50.0f + net * 4.0f + (100.0f - entityHunger) * 0.3f,
                               0.0f, 100.0f);
    biology.nutritionalStatus += (target - biology.nutritionalStatus) * 0.3f;
    biology.nutritionalStatus = std::clamp(biology.nutritionalStatus, 0.0f, 100.0f);

    // Energy tracks nutrition, worn down by accumulated fatigue.
    biology.energyLevel = std::clamp(biology.nutritionalStatus - fatigueLevel * 0.4f,
                                      0.0f, 100.0f);

    // Hydration: no dedicated drinking action exists, so this is a slow drift
    // toward a comfortable setpoint that heavy same-day exertion knocks down.
    float hydrationTarget = 70.0f - std::min(30.0f, biology.caloricExpenditureToday * 1.5f);
    biology.hydrationLevel += (hydrationTarget - biology.hydrationLevel) * 0.25f;
    biology.hydrationLevel = std::clamp(biology.hydrationLevel, 0.0f, 100.0f);

    // Starvation: a chronic caloric deficit hits mood/cognition beyond what
    // the hunger stat alone already inflicts. Overeating: a chronic, unneeded
    // surplus buys nothing and costs a little sluggish discomfort —
    // diminishing/negative returns past satiety. Gated behind the LiveConfig
    // multiplier (×0 = bit-exact no-op) like every other new subsystem.
    if (g_liveConfig.bioHomeostasisMul > 0.0f) {
        if (biology.nutritionalStatus < 20.0f) {
            float deficit = (20.0f - biology.nutritionalStatus) * 0.15f * g_liveConfig.bioHomeostasisMul;
            entityStress      = std::min(100.0f, entityStress + deficit);
            entityMentalHealth = std::max(0.0f, entityMentalHealth - deficit * 0.5f);
        } else if (biology.nutritionalStatus > 90.0f &&
                   biology.caloricIntakeToday > biology.caloricExpenditureToday * 1.8f) {
            entityHapiness = std::max(0.0f, entityHapiness - 1.0f * g_liveConfig.bioHomeostasisMul);
        }
    }

    biology.caloricIntakeToday      = 0.0f;
    biology.caloricExpenditureToday = 0.0f;
}

void Entity::exposeToPathogen(int pathogenId, int today) {
    // LiveConfig kill switch: ×0 = the whole system contributes nothing.
    if (g_liveConfig.pathogenMul <= 0.0f) return;
    for (const auto& p : pathogenExposures)
        if (p.pathogenId == pathogenId) return; // already carrying/clearing this one

    float immunity = std::clamp(baseImmunity * genome.resilience, 0.0f, 100.0f);
    float roll = BetterRand::genNrInInterval(0.0f, 100.0f);
    if (roll >= (100.0f - immunity) * 0.5f) return; // immune system fought it off on contact

    PathogenExposure p;
    p.pathogenId     = pathogenId;
    p.exposureDay    = today;
    p.viralLoad      = 10.0f;
    p.isInfected     = false; // incubating
    p.isContagious   = false;
    p.daysInfected   = 0;
    p.immunityLevel  = (int)immunity;
    pathogenExposures.push_back(p);
}

void Entity::tickPathogens(int today) {
    constexpr int INCUBATION_DAYS = 3;
    constexpr int CHRONIC_TIMEOUT = 30;

    for (auto it = pathogenExposures.begin(); it != pathogenExposures.end(); ) {
        PathogenExposure& p = *it;
        if (!p.isInfected && today - p.exposureDay >= INCUBATION_DAYS) {
            p.isInfected   = true;
            p.isContagious = true;
        }
        if (!p.isInfected) { ++it; continue; }

        p.daysInfected++;
        float immuneResponse = std::clamp(baseImmunity * genome.resilience, 10.0f, 150.0f);
        p.viralLoad = std::clamp(p.viralLoad + 6.0f - immuneResponse * 0.08f, 0.0f, 100.0f);

        entityHealth = std::max(0.0f, entityHealth - p.viralLoad * 0.01f);
        entityStress = std::min(100.0f, entityStress + p.viralLoad * 0.02f);
        p.isContagious = (p.viralLoad > 15.0f);

        if (p.viralLoad <= 0.0f || p.daysInfected > CHRONIC_TIMEOUT) {
            // Recovered: a lasting immunity bump (naive vaccination-equivalent).
            baseImmunity = std::min(100.0f, baseImmunity + 4.0f);
            it = pathogenExposures.erase(it);
            continue;
        }
        ++it;
    }
}
