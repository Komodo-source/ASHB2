#include <string>
#include <vector>
#include <map>
#include <random>
#include <cmath>
#include <algorithm>
#include <memory>
#include <deque>
#include "../header/Entity.h"

// Forward declaration
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

// Need system
struct Need {
    std::string name;
    float urgency; // 0-100, how urgent is this need
    float decayRate; // How fast this need grows over time
    std::vector<std::string> satisfyingCategories; // Which action categories satisfy this

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

    // Calculate memory weight (more recent = more weight)
    float getMemoryWeight(int memoryAge) {
        float decay = std::exp(-memoryAge / 20.0f); // Exponential decay
        return decay;
    }

    // Calculate how well entity meets action requirements
    float calculateRequirementFitness(Entity* entity, const Action& action) {
        float totalFitness = 0.0f;
        float totalWeight = 0.0f;

        for (const auto& req : action.requirements) {
            float currentValue = getEntityStat(entity, req.statName);
            float difference = currentValue - req.requiredValue;

            float fitness;
            if (difference >= 0) {
                fitness = 1.0f; // Full fitness if requirement met
            } else {
                // Partial fitness based on how close we are
                fitness = std::max(0.0f, 1.0f + (difference / req.requiredValue));
            }

            totalFitness += fitness * req.weight;
            totalWeight += req.weight;
        }

        return totalWeight > 0 ? totalFitness / totalWeight : 1.0f;
    }

    // Calculate how much this action addresses current needs
    float calculateNeedSatisfaction(const Action& action) {
        float satisfaction = 0.0f;

        auto needIt = needs.find(action.needCategory);
        if (needIt != needs.end()) {
            // Higher urgency = more satisfaction from doing this action
            satisfaction += needIt->second.urgency * 0.01f * action.baseSatisfaction;
        }

        return satisfaction;
    }

    // Calculate bias from memory (learn from past experiences)
    float calculateMemoryBias(int actionId) {
        float bias = 0.0f;
        float totalWeight = 0.0f;
        int memoryIndex = 0;

        for (auto it = actionHistory.rbegin(); it != actionHistory.rend(); ++it) {
            if (it->actionId == actionId) {
                float weight = getMemoryWeight(memoryIndex);
                bias += it->outcomeSuccess * weight;
                totalWeight += weight;
            }
            memoryIndex++;
        }

        return totalWeight > 0 ? bias / totalWeight : 0.5f; // Default neutral bias
    }

    // Calculate variety bonus (avoid repetition)
    float calculateVarietyBonus(int actionId) {
        int recentCount = 0;
        int checkDepth = std::min(10, (int)actionHistory.size());

        for (int i = 0; i < checkDepth; i++) {
            if (actionHistory[i].actionId == actionId) {
                recentCount++;
            }
        }

        // Penalty for repeated actions
        return std::max(0.0f, 1.0f - (recentCount * 0.2f));
    }

public:
    FreeWillSystem() : currentTime(0), rng(std::random_device{}()) {
        initializeNeeds();
        initializeActions();
    }

    void initializeNeeds() {
        needs["social"] = Need("social", 0.15f);
        needs["social"].satisfyingCategories = {"social", "entertainment"};

        needs["health"] = Need("health", 0.08f);
        needs["health"].satisfyingCategories = {"health", "food", "sleep"};

        needs["hygiene"] = Need("hygiene", 0.12f);
        needs["hygiene"].satisfyingCategories = {"hygiene"};

        needs["entertainment"] = Need("entertainment", 0.2f);
        needs["entertainment"].satisfyingCategories = {"entertainment", "social"};

        needs["safety"] = Need("safety", 0.05f);
        needs["safety"].satisfyingCategories = {"safety", "health"};

        needs["happiness"] = Need("happiness", 0.1f);
        needs["happiness"].satisfyingCategories = {"entertainment", "social", "achievement"};
    }

    void initializeActions() {
        // Social actions
        Action socialize("Socialize", 1, "social");
        socialize.requirements = {
            {"loneliness", 30.0f, 0.8f},
            {"stress", 50.0f, 0.3f}
        };
        socialize.statChanges = {
            {"loneliness", -20.0f},
            {"happiness", 15.0f},
            {"stress", -10.0f},
            {"boredom", -15.0f}
        };
        socialize.baseSatisfaction = 25.0f;
        availableActions.push_back(socialize);

        // Health actions
        Action exercise("Exercise", 2, "health");
        exercise.requirements = {
            {"health", 30.0f, 0.7f},
            {"stress", 60.0f, 0.4f}
        };
        exercise.statChanges = {
            {"health", 10.0f},
            {"stress", -15.0f},
            {"happiness", 10.0f},
            {"boredom", -10.0f}
        };
        exercise.baseSatisfaction = 20.0f;
        availableActions.push_back(exercise);

        // Hygiene actions
        Action shower("Take Shower", 3, "hygiene");
        shower.requirements = {
            {"hygiene", 50.0f, 0.9f}
        };
        shower.statChanges = {
            {"hygiene", 40.0f},
            {"happiness", 5.0f},
            {"stress", -5.0f}
        };
        shower.baseSatisfaction = 15.0f;
        availableActions.push_back(shower);

        // Entertainment actions
        Action play("Play Game", 4, "entertainment");
        play.requirements = {
            {"boredom", 40.0f, 0.8f},
            {"stress", 70.0f, 0.3f}
        };
        play.statChanges = {
            {"boredom", -30.0f},
            {"happiness", 20.0f},
            {"stress", -10.0f},
            {"loneliness", 5.0f}
        };
        play.baseSatisfaction = 30.0f;
        availableActions.push_back(play);

        // Rest action
        Action rest("Rest", 5, "health");
        rest.requirements = {
            {"stress", 60.0f, 0.7f},
            {"health", 40.0f, 0.5f}
        };
        rest.statChanges = {
            {"stress", -25.0f},
            {"health", 15.0f},
            {"mentalHealth", 10.0f},
            {"boredom", 10.0f}
        };
        rest.baseSatisfaction = 20.0f;
        availableActions.push_back(rest);

        // Work/Achievement action
        Action work("Work on Project", 6, "achievement");
        work.requirements = {
            {"stress", 40.0f, 0.5f},
            {"health", 50.0f, 0.4f}
        };
        work.statChanges = {
            {"stress", 15.0f},
            {"happiness", 15.0f},
            {"boredom", -20.0f},
            {"loneliness", 10.0f}
        };
        work.baseSatisfaction = 25.0f;
        availableActions.push_back(work);
    }

    // Main decision-making function
    Action* chooseAction(Entity* entity) {
        std::vector<std::pair<Action*, float>> actionWeights;

        for (auto& action : availableActions) {
            // Calculate multiple factors
            float requirementFitness = calculateRequirementFitness(entity, action);
            float needSatisfaction = calculateNeedSatisfaction(action);
            float memoryBias = calculateMemoryBias(action.actionId);
            float varietyBonus = calculateVarietyBonus(action.actionId);

            // Combined weight with different factor importance
            float weight =
                requirementFitness * 0.3f +  // Can we do it?
                needSatisfaction * 0.4f +     // Does it satisfy our needs?
                memoryBias * 0.2f +            // Did it work well before?
                varietyBonus * 0.1f;           // Avoid repetition

            // Add randomness factor (simulate unpredictability)
            std::uniform_real_distribution<float> dist(0.8f, 1.2f);
            weight *= dist(rng);

            actionWeights.push_back({&action, weight});
        }

        // Sort by weight
        std::sort(actionWeights.begin(), actionWeights.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        // Weighted random selection (top actions more likely)
        float totalWeight = 0.0f;
        for (const auto& aw : actionWeights) {
            totalWeight += aw.second;
        }

        std::uniform_real_distribution<float> selectDist(0.0f, totalWeight);
        float selection = selectDist(rng);

        float cumulative = 0.0f;
        for (const auto& aw : actionWeights) {
            cumulative += aw.second;
            if (selection <= cumulative) {
                return aw.first;
            }
        }

        return &availableActions[0]; // Fallback
    }

    // Execute chosen action
    void executeAction(Entity* entity, Action* action) {
        // Store state before action
        std::map<std::string, float> statsBefore = captureEntityStats(entity);

        // Apply stat changes
        for (const auto& change : action->statChanges) {
            float currentValue = getEntityStat(entity, change.statName);
            setEntityStat(entity, change.statName, currentValue + change.changeValue);
        }

        // Satisfy related needs
        auto needIt = needs.find(action->needCategory);
        if (needIt != needs.end()) {
            needIt->second.satisfy(action->baseSatisfaction);
        }

        // Store state after action
        std::map<std::string, float> statsAfter = captureEntityStats(entity);

        // Calculate outcome success
        float outcomeSuccess = calculateOutcomeSuccess(statsBefore, statsAfter);

        // Record in memory
        ActionMemory memory;
        memory.actionId = action->actionId;
        memory.actionName = action->name;
        memory.timestamp = currentTime;
        memory.outcomeSuccess = outcomeSuccess;
        memory.statsBefore = statsBefore;
        memory.statsAfter = statsAfter;

        actionHistory.push_front(memory);

        // Keep only last 50 actions
        if (actionHistory.size() > MAX_MEMORY) {
            actionHistory.pop_back();
        }

        currentTime++;
    }

    // Update needs over time
    void updateNeeds(float deltaTime) {
        for (auto& needPair : needs) {
            needPair.second.update(deltaTime);
        }
    }

    // Add custom action
    void addAction(const Action& action) {
        availableActions.push_back(action);
    }

    // Get action history
    const std::deque<ActionMemory>& getActionHistory() const {
        return actionHistory;
    }

    // Get current needs
    const std::map<std::string, Need>& getNeeds() const {
        return needs;
    }

private:
    std::map<std::string, float> captureEntityStats(Entity* entity) {
        return {
            {"health", entity->entityHealth},
            {"happiness", entity->entityHapiness},
            {"stress", entity->entityStress},
            {"mentalHealth", entity->entityMentalHealth},
            {"loneliness", entity->entityLoneliness},
            {"boredom", entity->entityBoredom},
            {"anger", entity->entityGeneralAnger},
            {"hygiene", (float)entity->entityHygiene}
        };
    }

    float calculateOutcomeSuccess(const std::map<std::string, float>& before,
                                   const std::map<std::string, float>& after) {
        // Compare positive stats (higher is better)
        float success = 0.0f;
        int count = 0;

        std::vector<std::string> positiveStats = {"health", "happiness", "mentalHealth", "hygiene"};
        for (const auto& stat : positiveStats) {
            if (after.at(stat) > before.at(stat)) success += 1.0f;
            count++;
        }

        std::vector<std::string> negativeStats = {"stress", "loneliness", "boredom", "anger"};
        for (const auto& stat : negativeStats) {
            if (after.at(stat) < before.at(stat)) success += 1.0f;
            count++;
        }

        return count > 0 ? success / count : 0.5f;
    }
};

// Implementation of stat getter/setter
float FreeWillSystem::getEntityStat(Entity* entity, const std::string& statName) {
    if (statName == "health") return entity->entityHealth;
    if (statName == "happiness") return entity->entityHapiness;
    if (statName == "stress") return entity->entityStress;
    if (statName == "mentalHealth") return entity->entityMentalHealth;
    if (statName == "loneliness") return entity->entityLoneliness;
    if (statName == "boredom") return entity->entityBoredom;
    if (statName == "anger") return entity->entityGeneralAnger;
    if (statName == "hygiene") return (float)entity->entityHygiene;
    return 0.0f;
}

void FreeWillSystem::setEntityStat(Entity* entity, const std::string& statName, float value) {
    if (statName == "health") entity->entityHealth = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "happiness") entity->entityHapiness = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "stress") entity->entityStress = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "mentalHealth") entity->entityMentalHealth = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "loneliness") entity->entityLoneliness = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "boredom") entity->entityBoredom = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "anger") entity->entityGeneralAnger = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "hygiene") entity->entityHygiene = (int)std::max(0.0f, std::min(100.0f, value));
}
