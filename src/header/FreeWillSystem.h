#ifndef FREE_WILL_SYSTEM_H
#define FREE_WILL_SYSTEM_H

#include <string>
#include <vector>
#include <map>
#include <random>
#include <cmath>
#include <algorithm>
#include <memory>
#include <deque>
#include "./Entity.h"

class Entity;

struct StatRequirement {
    std::string statName;
    float requiredValue;
    float weight; // How important this requirement is (0-1)
};

// Stat changes that result from an action
struct StatChange {
    std::string statName;
    float changeValue;
};



// Action definition
struct Action {
    std::string name;
    int actionId;
    std::vector<StatRequirement> requirements;
    std::vector<StatChange> statChanges;
    float baseSatisfaction; // How much this action satisfies base needs
    std::string needCategory; // "social", "health", "entertainment", "hygiene", etc.
    float duration; // Time this action takes
    float outcomeSuccess;  //needed for statistics

    Action(std::string n, int id, std::string category = "general")
        : name(n), actionId(id), needCategory(category), baseSatisfaction(10.0f), duration(1.0f) {}
};

// Memory of past actions
struct ActionMemory {
    int actionId;
    std::string actionName;
    int timestamp;
    float outcomeSuccess; // How successful was the action (0-1)
    std::map<std::string, float> statsBefore;
    std::map<std::string, float> statsAfter;
};

// Context for action evaluation
struct ActionContext {
    bool isNightTime;
    bool isWeekend;
    bool isAtWork;
    bool isInPublic;
    int numPeopleNearby;

    ActionContext()
        : isNightTime(false), isWeekend(false), isAtWork(false),
          isInPublic(false), numPeopleNearby(0) {}

    ActionContext(bool night, bool weekend, bool work, bool pub, int people)
        : isNightTime(night), isWeekend(weekend), isAtWork(work),
          isInPublic(pub), numPeopleNearby(people) {}
};

// Need system
struct Need {
    std::string name;
    float urgency; // 0-100, how urgent is this need
    float decayRate; // How fast this need grows over time
    std::vector<std::string> satisfyingCategories; // Which action categories satisfy this

    Need() : name(""), urgency(50.0f), decayRate(0.1f) {}
    Need(std::string n, float decay = 0.1f)
        : name(n), urgency(50.0f), decayRate(decay) {}

    void update(float deltaTime) {
        urgency = std::min(100.0f, urgency + decayRate * deltaTime);
    }

    void satisfy(float amount) {
        urgency = std::max(0.0f, urgency - amount);
    }

};

// Free Will System
class FreeWillSystem {
private:
    std::deque<ActionMemory> actionHistory; // Last 50 actions
    static const int MAX_MEMORY = 50;
    std::map<std::string, Need> needs;
    std::vector<Action> availableActions;
    int currentTime;
    std::mt19937 rng;

    // Get stat value from entity
    float getEntityStat(Entity* entity, const std::string& statName);

    // Set stat value on entity
    void setEntityStat(Entity* entity, const std::string& statName, float value);

    float getMemoryWeight(int memoryAge);
    float calculateRequirementFitness(Entity* entity, const Action& action);
    float calculateNeedSatisfaction(const Action& action);
    float calculateMemoryBias(int actionId);
    float calculateVarietyBonus(int actionId);
    float calculateContextualWeight(const Action& action, const ActionContext& context);
    float calculatePersonalityModifier(Entity* entity, const Action& action);
    std::map<std::string, float> captureEntityStats(Entity* entity);
    float calculateOutcomeSuccess(const std::map<std::string, float>& before,
                                  const std::map<std::string, float>& after);


public:
    FreeWillSystem();

    void initializeNeeds();
    void initializeActions();

    Action* chooseAction(Entity* entity, const std::vector<Entity*>& neighbors = {}, const ActionContext& context = ActionContext());
    void executeAction(Entity* entity, Action* &action, Entity* pointed=nullptr);
    void pointedAssimilation(Entity* pointer, Entity* pointed, Action* action);

    void updateNeeds(float deltaTime);
    void addAction(const Action& action);

    const std::deque<ActionMemory>& getActionHistory() const;
    const std::map<std::string, Need>& getNeeds() const;

    // Helper for social environment influence
    float calculateSocialInfluence(Entity* entity, const std::vector<Entity*>& neighbors, const Action& action);

    void applyEmotionalContagion(Entity* entity, const std::vector<Entity*>& neighbors);
};

#endif
