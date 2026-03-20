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
#include <fstream>
#include "./NeedLevel.h"

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
/*
// Represents a social norm in the entity's current group
struct SocialNorm {
    std::string actionName;
    float prevalence;       // 0-1: how common this action is in the group
    float normPressure;     // How much deviation from it costs socially

    SocialNorm() : actionName(""), prevalence(0.0f), normPressure(0.0f) {}
    SocialNorm(std::string name, float prev, float pressure)
        : actionName(name), prevalence(prev), normPressure(pressure) {}
};
*/


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
    // Deliberation context (optional, for storytelling/debugging)
    std::string deliberationReasoning;
    bool isImpulsive;
};

// Environmental factors affecting entity behavior
struct EnvironmentalFactors {
    float weatherQuality; // 0-100: 0=stormy/depressing, 100=sunny/uplifting
    float crowdDensity;   // 0-100: 0=isolated, 100=very crowded (introverts flee)
    float noiseLevel;     // 0-100: 0=silent, 100=very loud (increases stress)
    float safetyLevel;    // 0-100: 0=very dangerous, 100=fully safe

    EnvironmentalFactors()
        : weatherQuality(60.0f), crowdDensity(20.0f),
          noiseLevel(20.0f), safetyLevel(80.0f) {}

    EnvironmentalFactors(float weather, float crowd, float noise, float safety)
        : weatherQuality(weather), crowdDensity(crowd),
          noiseLevel(noise), safetyLevel(safety) {}
};

// Cognitive Architecture: lightweight cognitive pipeline components
// Perception: what the entity notices filtered by personality/state
struct PerceivedEvent {
    int eventId{0};
    std::string eventType; // e.g., "Threat", "Opportunity", etc.
    float intensity{0.0f};
    Entity* source{nullptr};
};

struct PerceivedEntity {
    Entity* entity{nullptr};
    float distance{0.0f};
};

struct Perception {
    std::vector<PerceivedEvent> events;
    std::vector<PerceivedEntity> nearbyEntities;
    EnvironmentalFactors perceivedEnv;
    float attentionalFocus{0.5f}; // 0..1
};

// Appraisal: OCC-style evaluation relative to self
struct Appraisal {
    float relevance{0.5f};      // does this relate to my goals?
    float desirability{0.5f};   // is this good for me?
    float novelty{0.5f};        // is this unexpected?
    float controllability{0.5f};
    float normCompliance{0.5f}; // fits social norms?
    float agentBlame{0.0f};     // attribution
};

// Deliberation: candidate actions and chosen action with reasoning
struct ActionCandidate {
    const Action* action{nullptr};
    float score{0.0f};
    ActionCandidate() = default;
    explicit ActionCandidate(const Action* a) : action(a) {}
};

struct Deliberation {
    std::vector<ActionCandidate> candidates;
    const Action* chosenAction{nullptr};
    std::string internalReasoning;
    bool isImpulsive{false}; // System 1 vs System 2
};

// Context for action evaluation
struct ActionContext {
    bool isNightTime;
    bool isWeekend;
    bool isAtWork;
    bool isInPublic;
    int numPeopleNearby;
    EnvironmentalFactors env;
    //std::map<std::string, SocialNorm> activeNorms; // Extracted societal norms

    ActionContext()
        : isNightTime(false), isWeekend(false), isAtWork(false),
          isInPublic(false), numPeopleNearby(0) {}

    ActionContext(bool night, bool weekend, bool work, bool pub, int people,
                  EnvironmentalFactors environment = EnvironmentalFactors())
        : isNightTime(night), isWeekend(weekend), isAtWork(work),
          isInPublic(pub), numPeopleNearby(people), env(environment) {}

    bool operator==(const ActionContext& other) const {
        return isNightTime == other.isNightTime &&
               isWeekend == other.isWeekend &&
               isAtWork == other.isAtWork &&
               isInPublic == other.isInPublic;
        // Not comparing exact numPeopleNearby and env to allow general habits to form
    }
};

struct Habit {
    int actionId;
    float strength;          // 0-1, grows with repetition
    ActionContext triggerContext;  // when does this habit fire?
    int consecutiveExecutions;

    Habit(int id, ActionContext ctx)
        : actionId(id), strength(0.1f), triggerContext(ctx), consecutiveExecutions(1) {}

    void reinforce(float amount) { strength = std::min(1.0f, strength + amount); }
    void decay(float amount) { strength = std::max(0.0f, strength - amount); }
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
    std::vector<Habit> habits;
    int currentTime;
    std::mt19937 rng;

    // Last deliberation result (pipeline state) for reflection/logging
    Deliberation lastDeliberation;

    // Get stat value from entity
    float getEntityStat(Entity* entity, const std::string& statName);

    // Set stat value on entity
    void setEntityStat(Entity* entity, const std::string& statName, float value);

    float getMemoryWeight(int memoryAge);
    float calculateRequirementFitness(Entity* entity, const Action& action);
    float calculateNeedSatisfaction(const Action& action, Entity* targetNeed);
    float calculateMemoryBias(int actionId);
    float calculateVarietyBonus(int actionId);
    float calculateContextualWeight(const Action& action, const ActionContext& context);
    float calculatePersonalityModifier(Entity* entity, const Action& action);
    float calculateGriefModifier(Entity* entity, const Action& action);
    float calculateEnvironmentalModifier(Entity* entity, const Action& action, const EnvironmentalFactors& env);
    std::map<std::string, float> captureEntityStats(Entity* entity);
    float calculateOutcomeSuccess(const std::map<std::string, float>& before,
                                  const std::map<std::string, float>& after);






public:
    FreeWillSystem();

    void initializeNeeds();
    void initializeActions();



    Action* checkHabitTrigger(const ActionContext& context);
    void updateHabits(int actionId, const ActionContext& context);

    Action* chooseAction(Entity* entity, const std::vector<Entity*>& neighbors = {}, const ActionContext& context = ActionContext());
    // New cognitive pipeline entry point (separate from legacy scoring)
    Action* cognitiveChooseAction(Entity* entity, const std::vector<Entity*>& neighbors, const ActionContext& context);
    void executeAction(Entity* entity, Action* &action, const ActionContext& context = ActionContext(), Entity* pointed=nullptr);
    void pointedAssimilation(Entity* pointer, Entity* pointed, Action* action);

    void updateNeeds(float deltaTime);
    NeedLevel updateHieratchicalNeed(Entity* ent, const Action& action);
    void addAction(const Action& action);

    const std::deque<ActionMemory>& getActionHistory() const;
    const std::map<std::string, Need>& getNeeds() const;

    // Helper for social environment influence
    float calculateSocialInfluence(Entity* entity, const std::vector<Entity*>& neighbors, const Action& action);

    void applyEmotionalContagion(Entity* entity, const std::vector<Entity*>& neighbors);
    void applyEnvironmentalEffects(Entity* entity, const EnvironmentalFactors& env);

    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
    void updatePersonalityFromExperience(Entity* ent, const Action& act, float outcomeSuccess);
        void finalizeChildhood(Entity* child);
    void tickChildDevelopment(Entity* child, float deltaTime);

    void updateValuesFromExperiences(Entity* ent, Action* &action, float outcomeSuccess);
    void tickValueGoalAlignment(Entity* entity);
    float applyValueSatisfaction(Entity* entity,  const Action& action);
    void tickEmotionalSuppression(Entity* entity);

    float getMaxUrgencyForLevel(const Entity* target, NeedLevel lvl);
};

#endif
