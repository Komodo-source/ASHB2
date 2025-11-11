#include <string>
#include <vector>
#include <map>
#include <random>
#include <cmath>
#include <algorithm>
#include <memory>
#include <deque>
#include "./header/Entity.h"
#include "./header/FreeWillSystem.h"
#include <iostream>
#include "./header/BetterRand.h"

//class Entity;


    // Calculate memory weight (more recent = more weight)
    float FreeWillSystem::getMemoryWeight(int memoryAge) {
        float decay = std::exp(-memoryAge / 20.0f); // Exponential decay
        return decay;
    }

    // Calculate how well entity meets action requirements
    float FreeWillSystem::calculateRequirementFitness(Entity* entity, const Action& action) {
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
    float FreeWillSystem::calculateNeedSatisfaction(const Action& action) {
        float satisfaction = 0.0f;

        auto needIt = needs.find(action.needCategory);
        if (needIt != needs.end()) {
            // Higher urgency = more satisfaction from doing this action
            satisfaction += needIt->second.urgency * 0.01f * action.baseSatisfaction;
        }

        return satisfaction;
    }

    // Calculate bias from memory (learn from past experiences)
    float FreeWillSystem::calculateMemoryBias(int actionId) {
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
    float FreeWillSystem::calculateVarietyBonus(int actionId) {
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


    FreeWillSystem::FreeWillSystem() : currentTime(0), rng(std::random_device{}()) {
        initializeNeeds();
        initializeActions();
    }

    void FreeWillSystem::initializeNeeds() {
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

    //implementation of a list of action
    void FreeWillSystem::initializeActions() {
        //a implem action pointé
        //hurt someone action


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
            {"stress", -10.0f},
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
    Action* FreeWillSystem::chooseAction(Entity* entity) {
            std::cout << "\n=== Choosing Action ===\n";
            std::vector<std::pair<Action*, float>> actionWeights;

            for (auto& action : availableActions) {
                float requirementFitness = calculateRequirementFitness(entity, action);
                float needSatisfaction = calculateNeedSatisfaction(action);
                float memoryBias = calculateMemoryBias(action.actionId);
                float varietyBonus = calculateVarietyBonus(action.actionId);

                std::cout << "\nAction: " << action.name << "\n";
                std::cout << "  RequirementFitness: " << requirementFitness << "\n";
                std::cout << "  NeedSatisfaction:   " << needSatisfaction << "\n";
                std::cout << "  MemoryBias:         " << memoryBias << "\n";
                std::cout << "  VarietyBonus:       " << varietyBonus << "\n";

                float weight =
                    requirementFitness * 0.3f +
                    needSatisfaction * 0.4f +
                    memoryBias * 0.2f +
                    varietyBonus * 0.1f;

                std::uniform_real_distribution<float> dist(0.8f, 1.2f);
                float randomFactor = dist(rng);
                weight *= randomFactor;

                std::cout << "  Combined Weight (pre-sort): " << weight
                        << " (RandomFactor: " << randomFactor << ")\n";

                actionWeights.push_back({&action, weight});
            }

            std::sort(actionWeights.begin(), actionWeights.end(),
                    [](const auto& a, const auto& b) { return a.second > b.second; });

            std::cout << "\n-- Sorted Action Weights --\n";
            for (auto& aw : actionWeights) {
                std::cout << "  " << aw.first->name << ": " << aw.second << "\n";
            }

            float totalWeight = 0.0f;
            for (const auto& aw : actionWeights) totalWeight += aw.second;

            std::uniform_real_distribution<float> selectDist(0.0f, totalWeight);
            float selection = selectDist(rng);
            std::cout << "\nTotalWeight: " << totalWeight
                    << " | SelectionPoint: " << selection << "\n";

            float cumulative = 0.0f;
            for (const auto& aw : actionWeights) {
                cumulative += aw.second;
                if (selection <= cumulative) {
                    std::cout << ">>> Chosen Action: " << aw.first->name << " <<<\n";
                    return aw.first;
                }
            }

            std::cout << ">>> Default fallback to: " << availableActions[0].name << " <<<\n";
            return &availableActions[0];
        }


        // Execute chosen action
    void FreeWillSystem::executeAction(Entity* entity, Action* action) {
        std::cout << "\n=== Executing Action: " << action->name << " ===\n";

        std::map<std::string, float> statsBefore = captureEntityStats(entity);
        std::cout << "Stats Before:\n";
        for (auto& [k, v] : statsBefore) std::cout << "  " << k << ": " << v << "\n";

        for (const auto& change : action->statChanges) {
            float currentValue = getEntityStat(entity, change.statName);
            float newValue = currentValue + BetterRand::genNrInInterval(change.changeValue -4, change.changeValue+4); // nouvelle valeur appliqué
            //on fait une variation de 4
            setEntityStat(entity, change.statName, newValue);
            std::cout << "  Changed " << change.statName << ": " << currentValue
                    << " -> " << newValue << "\n";
        }

        auto needIt = needs.find(action->needCategory);
        if (needIt != needs.end()) {
            std::cout << "Satisfying need category: " << action->needCategory << "\n";
            needIt->second.satisfy(action->baseSatisfaction);
        }

        std::map<std::string, float> statsAfter = captureEntityStats(entity);
        std::cout << "Stats After:\n";
        for (auto& [k, v] : statsAfter) std::cout << "  " << k << ": " << v << "\n";

        float outcomeSuccess = calculateOutcomeSuccess(statsBefore, statsAfter);
        std::cout << "Outcome Success: " << outcomeSuccess << "\n";

        ActionMemory memory;
        memory.actionId = action->actionId;
        memory.actionName = action->name;
        memory.timestamp = currentTime;
        memory.outcomeSuccess = outcomeSuccess;
        memory.statsBefore = statsBefore;
        memory.statsAfter = statsAfter;

        actionHistory.push_front(memory);
        if (actionHistory.size() > MAX_MEMORY) actionHistory.pop_back();

        currentTime++;
        std::cout << "Action completed. Memory recorded. Time now: " << currentTime << "\n";
    }

    // Update needs over time
    void FreeWillSystem::updateNeeds(float deltaTime) {
        for (auto& needPair : needs) {
            needPair.second.update(deltaTime);
        }
    }

    // Add custom action
    void FreeWillSystem::addAction(const Action& action) {
        availableActions.push_back(action);
    }

    // Get action history
    const std::deque<ActionMemory>& FreeWillSystem::getActionHistory() const {
        return actionHistory;
    }

    // Get current needs
    const std::map<std::string, Need>& FreeWillSystem::getNeeds() const {
        return needs;
    }


    std::map<std::string, float> FreeWillSystem::captureEntityStats(Entity* entity) {
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

    float FreeWillSystem::calculateOutcomeSuccess(const std::map<std::string, float>& before,
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
