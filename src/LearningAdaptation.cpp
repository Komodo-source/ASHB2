#include "./header/LearningAdaptation.h"
#include "./header/Entity.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <random>

// ActionValueFunction implementation
void ActionValueFunction::update(const std::string& state, 
                                  const std::string& action,
                                  float reward,
                                  const std::string& nextState) {
    // Q-learning update rule
    float currentQ = getQValue(state, action);
    
    // Find max Q-value for next state
    float maxNextQ = 0.0f;
    auto nextIt = qValues.find(nextState);
    if (nextIt != qValues.end() && !nextIt->second.empty()) {
        for (const auto& [act, qVal] : nextIt->second) {
            maxNextQ = std::max(maxNextQ, qVal);
        }
    }
    
    // Update Q-value
    float newQ = currentQ + learningRate * (reward + discountFactor * maxNextQ - currentQ);
    qValues[state][action] = newQ;
    
    // Update visit count
    visitCounts[state][action]++;
}

float ActionValueFunction::getQValue(const std::string& state, const std::string& action) const {
    auto stateIt = qValues.find(state);
    if (stateIt != qValues.end()) {
        auto actionIt = stateIt->second.find(action);
        if (actionIt != stateIt->second.end()) {
            return actionIt->second;
        }
    }
    return 50.0f;  // Default neutral value
}

std::string ActionValueFunction::selectAction(
    const std::string& state,
    const std::vector<std::string>& availableActions,
    bool explore) {
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0f, 1.0f);
    
    // Epsilon-greedy exploration
    if (explore && dis(gen) < explorationRate) {
        // Random action
        std::uniform_int_distribution<> actionDist(0, availableActions.size() - 1);
        return availableActions[actionDist(gen)];
    }
    
    // Greedy selection
    std::string bestAction = availableActions[0];
    float bestQ = -1e9f;
    
    for (const auto& action : availableActions) {
        float q = getQValue(state, action);
        
        // Add exploration bonus based on visit count
        auto stateIt = visitCounts.find(state);
        int visits = 0;
        if (stateIt != visitCounts.end()) {
            auto actionIt = stateIt->second.find(action);
            if (actionIt != stateIt->second.end()) {
                visits = actionIt->second;
            }
        }
        float explorationBonus = 10.0f / (1 + visits);
        
        float totalQ = q + explorationBonus;
        if (totalQ > bestQ) {
            bestQ = totalQ;
            bestAction = action;
        }
    }
    
    return bestAction;
}

// HabitStrength implementation
void HabitStrength::reinforce(float reward) {
    repetitions++;
    
    // Update strength based on reward and consistency
    float rewardFactor = std::max(0.1f, reward / 100.0f);
    strength = std::min(1.0f, strength + 0.1f * rewardFactor);
    
    // Update consistency based on context stability
    consistency = (consistency * (repetitions - 1) + 
                   (timeOfDayConsistency * locationConsistency * 
                    socialContextConsistency * emotionalStateConsistency)) / repetitions;
}

void HabitStrength::decay(float deltaTime) {
    // Habits decay over time without reinforcement
    float decayAmount = 0.001f * deltaTime * (1.0f - strength);
    strength = std::max(0.0f, strength - decayAmount);
}

float HabitStrength::calculateActivationStrength(const std::string& currentContext) const {
    float activation = strength;
    
    // Check context match
    if (currentContext == contextCue) {
        activation *= 1.5f;  // Boost for matching context
    } else {
        // Partial match based on similarity
        activation *= 0.7f;
    }
    
    // Apply consistency multiplier
    activation *= (0.5f + consistency * 0.5f);
    
    return std::min(1.0f, activation);
}

// Skill implementation
void Skill::practice(float quality) {
    practiceHours++;
    daysSincePractice = 0;
    
    // Learning curve: diminishing returns
    float learningPotential = (100.0f - proficiency) * 0.01f;
    float gain = learningRate * quality * learningPotential * (1.0f + automaticity * 0.5f);
    
    proficiency = std::min(100.0f, proficiency + gain);
    
    // Increase automaticity with practice
    automaticity = std::min(100.0f, automaticity + gain * 0.5f);
    
    // Knowledge increases more slowly
    knowledge = std::min(100.0f, knowledge + gain * 0.3f);
    
    // Adaptability grows with both knowledge and practice
    adaptability = (knowledge + automaticity) * 0.5f;
}

void Skill::update() {
    if (daysSincePractice > 0) {
        // Skill decay when not practiced
        float decay = decayRate * daysSincePractice * (proficiency / 100.0f);
        proficiency = std::max(0.0f, proficiency - decay);
        automaticity = std::max(0.0f, automaticity - decay * 0.5f);
    }
    daysSincePractice++;
}

float Skill::getEffectiveProficiency() const {
    // Combine components for effective proficiency
    return (proficiency * 0.5f + automaticity * 0.3f + knowledge * 0.2f);
}

// CulturalTransmission implementation
void CulturalTransmission::learnFromObservation(Entity* model, 
                                                 const std::string& behavior,
                                                 float outcomeObserved) {
    // Determine learning weight based on biases
    float learningWeight = contentBias;
    
    // Prestige bias: learn more from high-status models
    // (Would need to access model's status here)
    learningWeight += prestigeBias * 0.3f;
    
    // Similarity bias
    learningWeight += similarityBias * 0.2f;
    
    // Conformity bias: stronger if many others do it
    // (Would need population-level data)
    
    // Update learned behavior fidelity
    float currentFidelity = 0.0f;
    auto it = learnedBehaviors.find(behavior);
    if (it != learnedBehaviors.end()) {
        currentFidelity = it->second;
    }
    
    float newFidelity = currentFidelity + (outcomeObserved / 100.0f - currentFidelity) * learningWeight * 0.1f;
    learnedBehaviors[behavior] = std::clamp(newFidelity, 0.0f, 1.0f);
}

void CulturalTransmission::teachBehavior(Entity* learner, const std::string& behavior) {
    int learnerId = learner->getId();
    
    // Check if we've taught this learner before
    auto it = taughtTo.find(learnerId);
    if (it == taughtTo.end()) {
        taughtTo[learnerId] = std::vector<std::string>();
    }
    
    // Add behavior to teaching record if not already there
    auto& behaviors = taughtTo[learnerId];
    if (std::find(behaviors.begin(), behaviors.end(), behavior) == behaviors.end()) {
        behaviors.push_back(behavior);
    }
}

// PersonalityChangeTracker implementation
void PersonalityChangeTracker::recordChange(const std::string& trait, 
                                             float delta,
                                             const std::string& cause) {
    ChangeEvent event;
    event.eventType = cause;
    event.impactMagnitude = delta;
    event.affectedTrait = trait;
    event.simulationDay = 0;  // Would be set by simulation
    
    changeHistory.push_back(event);
    
    // Keep only recent history
    while (changeHistory.size() > 50) {
        changeHistory.pop_front();
    }
    
    // Update cumulative drift
    if (trait == "extraversion") extraversionDrift += delta;
    else if (trait == "agreeableness") agreeablenessDrift += delta;
    else if (trait == "conscientiousness") conscientiousnessDrift += delta;
    else if (trait == "neuroticism") neuroticismDrift += delta;
    else if (trait == "openness") opennessDrift += delta;
}

void PersonalityChangeTracker::applyCumulativeChanges(Entity* entity) {
    if (!entity) return;
    
    // Apply drift gradually (small changes per application)
    float changeRate = 0.01f;
    
    entity->personality.extraversion = std::clamp(
        entity->personality.extraversion + extraversionDrift * changeRate, 0.0f, 100.0f);
    entity->personality.agreeableness = std::clamp(
        entity->personality.agreeableness + agreeablenessDrift * changeRate, 0.0f, 100.0f);
    entity->personality.conscientiousness = std::clamp(
        entity->personality.conscientiousness + conscientiousnessDrift * changeRate, 0.0f, 100.0f);
    entity->personality.neuroticism = std::clamp(
        entity->personality.neuroticism + neuroticismDrift * changeRate, 0.0f, 100.0f);
    entity->personality.openness = std::clamp(
        entity->personality.openness + opennessDrift * changeRate, 0.0f, 100.0f);
    
    // Reset drift after applying
    extraversionDrift = 0.0f;
    agreeablenessDrift = 0.0f;
    conscientiousnessDrift = 0.0f;
    neuroticismDrift = 0.0f;
    opennessDrift = 0.0f;
}

// LearningAdaptationSystem implementation
LearningAdaptationSystem::LearningAdaptationSystem() {}

void LearningAdaptationSystem::initializeEntity(Entity* entity) {
    if (!entity) return;
    
    int id = entity->getId();
    
    // Initialize Q-function with default parameters
    ActionValueFunction qFunc;
    qFunc.learningRate = 0.1f + entity->personality.openness * 0.001f;
    qFunc.explorationRate = 0.1f + entity->personality.openness * 0.002f;
    entityQFunctions[id] = qFunc;
    
    // Initialize empty habit list
    entityHabits[id] = std::vector<HabitStrength>();
    
    // Initialize empty skill map
    entitySkills[id] = std::map<std::string, Skill>();
    
    // Initialize cultural transmission
    CulturalTransmission culture;
    culture.conformityBias = 30.0f + entity->personality.agreeableness * 0.3f;
    culture.prestigeBias = entity->personality.extraversion * 0.4f;
    culture.similarityBias = 40.0f + (100.0f - entity->personality.openness) * 0.3f;
    culturalLearning[id] = culture;
    
    // Initialize personality tracker
    personalityChanges[id] = PersonalityChangeTracker();
}

void LearningAdaptationSystem::processExperience(Entity* entity,
                                                  const std::string& state,
                                                  const std::string& action,
                                                  float reward,
                                                  const std::string& nextState) {
    int id = entity->getId();
    auto& qFunc = entityQFunctions[id];
    
    // Update Q-values
    qFunc.update(state, action, reward, nextState);
    
    // Reinforce habits if action matches existing habit
    for (auto& habit : entityHabits[id]) {
        if (habit.actionName == action && habit.contextCue == state) {
            habit.reinforce(reward);
        }
    }
    
    // Track personality changes from significant experiences
    if (std::abs(reward) > 70.0f) {
        updatePersonalityFromExperience(entity, state, reward);
    }
}

std::string LearningAdaptationSystem::selectActionRL(Entity* entity,
                                                      const std::string& state,
                                                      const std::vector<std::string>& actions) {
    int id = entity->getId();
    auto& qFunc = entityQFunctions[id];
    
    // Check for strong habits first
    for (const auto& habit : entityHabits[id]) {
        float activation = habit.calculateActivationStrength(state);
        if (activation > 0.8f) {
            // Strong habit overrides deliberative choice
            return habit.actionName;
        }
    }
    
    // Use Q-learning to select action
    return qFunc.selectAction(state, actions, true);
}

void LearningAdaptationSystem::reinforceHabit(Entity* entity,
                                               const std::string& action,
                                               const std::string& context,
                                               float reward) {
    int id = entity->getId();
    auto& habits = entityHabits[id];
    
    // Find or create habit
    for (auto& habit : habits) {
        if (habit.actionName == action && habit.contextCue == context) {
            habit.reinforce(reward);
            return;
        }
    }
    
    // Create new habit
    HabitStrength newHabit;
    newHabit.actionName = action;
    newHabit.contextCue = context;
    newHabit.strength = 0.1f;
    newHabit.repetitions = 1;
    newHabit.consistency = 0.5f;
    habits.push_back(newHabit);
}

void LearningAdaptationSystem::updateHabits(Entity* entity, float deltaTime) {
    int id = entity->getId();
    for (auto& habit : entityHabits[id]) {
        habit.decay(deltaTime);
    }
}

float LearningAdaptationSystem::getHabitStrength(Entity* entity,
                                                  const std::string& action,
                                                  const std::string& context) {
    int id = entity->getId();
    for (const auto& habit : entityHabits[id]) {
        if (habit.actionName == action && habit.contextCue == context) {
            return habit.calculateActivationStrength(context);
        }
    }
    return 0.0f;
}

void LearningAdaptationSystem::practiceSkill(Entity* entity,
                                              const std::string& skillName,
                                              float quality) {
    int id = entity->getId();
    auto& skills = entitySkills[id];
    
    auto it = skills.find(skillName);
    if (it == skills.end()) {
        // Create new skill
        Skill newSkill;
        newSkill.skillName = skillName;
        newSkill.proficiency = 10.0f;
        newSkill.learningRate = 1.0f + entity->personality.openness * 0.01f;
        newSkill.practice(quality);
        skills[skillName] = newSkill;
    } else {
        it->second.practice(quality);
    }
}

Skill* LearningAdaptationSystem::getSkill(Entity* entity, const std::string& skillName) {
    int id = entity->getId();
    auto& skills = entitySkills[id];
    
    auto it = skills.find(skillName);
    if (it != skills.end()) {
        return &it->second;
    }
    return nullptr;
}

void LearningAdaptationSystem::observeAndLearn(Entity* observer,
                                                Entity* model,
                                                const std::string& behavior,
                                                float observedOutcome) {
    int observerId = observer->getId();
    auto& culture = culturalLearning[observerId];
    
    culture.learnFromObservation(model, behavior, observedOutcome);
}

void LearningAdaptationSystem::teachBehavior(Entity* teacher,
                                              Entity* learner,
                                              const std::string& behavior) {
    int teacherId = teacher->getId();
    auto& culture = culturalLearning[teacherId];
    
    culture.teachBehavior(learner, behavior);
}

void LearningAdaptationSystem::updatePersonalityFromExperience(Entity* entity,
                                                                const std::string& experienceType,
                                                                float outcomeValence) {
    int id = entity->getId();
    auto& tracker = personalityChanges[id];
    
    // Different experiences affect different traits
    if (experienceType.find("social") != std::string::npos) {
        if (outcomeValence > 0) {
            tracker.recordChange("extraversion", 2.0f, experienceType);
            tracker.recordChange("agreeableness", 1.5f, experienceType);
        } else {
            tracker.recordChange("extraversion", -1.5f, experienceType);
            tracker.recordChange("neuroticism", 2.0f, experienceType);
        }
    } else if (experienceType.find("achievement") != std::string::npos) {
        if (outcomeValence > 0) {
            tracker.recordChange("conscientiousness", 1.5f, experienceType);
            tracker.recordChange("openness", 1.0f, experienceType);
        } else {
            tracker.recordChange("conscientiousness", -1.0f, experienceType);
            tracker.recordChange("neuroticism", 1.5f, experienceType);
        }
    } else if (experienceType.find("novel") != std::string::npos) {
        if (outcomeValence > 0) {
            tracker.recordChange("openness", 2.5f, experienceType);
        } else {
            tracker.recordChange("openness", -1.0f, experienceType);
            tracker.recordChange("neuroticism", 1.5f, experienceType);
        }
    } else if (experienceType.find("stress") != std::string::npos ||
               experienceType.find("trauma") != std::string::npos) {
        tracker.recordChange("neuroticism", 3.0f, experienceType);
        tracker.recordChange("extraversion", -2.0f, experienceType);
    }
}

void LearningAdaptationSystem::applyPersonalityChanges(Entity* entity) {
    int id = entity->getId();
    auto& tracker = personalityChanges[id];
    tracker.applyCumulativeChanges(entity);
}

const ActionValueFunction& LearningAdaptationSystem::getQFunction(int entityId) const {
    return entityQFunctions.at(entityId);
}

const std::vector<HabitStrength>& LearningAdaptationSystem::getHabits(int entityId) const {
    return entityHabits.at(entityId);
}

void LearningAdaptationSystem::saveTo(std::ofstream& file) const {
    // Save Q-functions
    size_t numEntities = entityQFunctions.size();
    file.write(reinterpret_cast<const char*>(&numEntities), sizeof(numEntities));
    
    // Simplified save: just save entity IDs and basic stats
    // Full Q-table serialization would be very large
    for (const auto& [entityId, qFunc] : entityQFunctions) {
        file.write(reinterpret_cast<const char*>(&entityId), sizeof(entityId));
        // Save learning parameters
        file.write(reinterpret_cast<const char*>(&qFunc.learningRate), sizeof(qFunc.learningRate));
        file.write(reinterpret_cast<const char*>(&qFunc.discountFactor), sizeof(qFunc.discountFactor));
        file.write(reinterpret_cast<const char*>(&qFunc.explorationRate), sizeof(qFunc.explorationRate));
    }
}

void LearningAdaptationSystem::loadFrom(std::ifstream& file) {
    size_t numEntities;
    file.read(reinterpret_cast<char*>(&numEntities), sizeof(numEntities));
    
    for (size_t i = 0; i < numEntities; ++i) {
        int entityId;
        file.read(reinterpret_cast<char*>(&entityId), sizeof(entityId));
        
        ActionValueFunction qFunc;
        file.read(reinterpret_cast<char*>(&qFunc.learningRate), sizeof(qFunc.learningRate));
        file.read(reinterpret_cast<char*>(&qFunc.discountFactor), sizeof(qFunc.discountFactor));
        file.read(reinterpret_cast<char*>(&qFunc.explorationRate), sizeof(qFunc.explorationRate));
        
        entityQFunctions[entityId] = qFunc;
        entityHabits[entityId] = std::vector<HabitStrength>();
        entitySkills[entityId] = std::map<std::string, Skill>();
        culturalLearning[entityId] = CulturalTransmission();
        personalityChanges[entityId] = PersonalityChangeTracker();
    }
}
