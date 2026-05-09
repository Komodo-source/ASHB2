#ifndef LEARNING_ADAPTATION_H
#define LEARNING_ADAPTATION_H

#include <string>
#include <vector>
#include <map>
#include <deque>

class Entity;
struct Action;

// Reinforcement learning components
struct ExperienceTuple {
    std::string state;        // Situation description
    std::string action;       // Action taken
    float reward;             // Outcome value
    std::string nextState;    // Resulting situation
    
    int timestamp;
};

struct ActionValueFunction {
    // Q-values for state-action pairs
    std::map<std::string, std::map<std::string, float>> qValues;
    
    // Visit counts for exploration
    std::map<std::string, std::map<std::string, int>> visitCounts;
    
    float learningRate = 0.1f;
    float discountFactor = 0.9f;
    float explorationRate = 0.1f;
    
    void update(const std::string& state, 
                const std::string& action,
                float reward,
                const std::string& nextState);
    
    float getQValue(const std::string& state, const std::string& action) const;
    std::string selectAction(const std::string& state,
                            const std::vector<std::string>& availableActions,
                            bool explore = true);
};

// Habit formation system
struct HabitStrength {
    std::string actionName;
    std::string contextCue;   // What triggers this habit
    
    float strength = 0.0f;    // 0-1, automaticity level
    int repetitions = 0;
    float consistency = 0.0f; // How regularly performed in same context
    
    // Context features
    float timeOfDayConsistency = 1.0f;
    float locationConsistency = 1.0f;
    float socialContextConsistency = 1.0f;
    float emotionalStateConsistency = 1.0f;
    
    void reinforce(float reward);
    void decay(float deltaTime);
    float calculateActivationStrength(const std::string& currentContext) const;
};

struct Skill {
    std::string skillName;
    float proficiency = 0.0f;     // 0-100
    float learningRate = 1.0f;
    float decayRate = 0.01f;
    
    int practiceHours = 0;
    int daysSincePractice = 0;
    
    // Skill components
    float knowledge = 0.0f;
    float automaticity = 0.0f;
    float adaptability = 0.0f;
    
    void practice(float quality);
    void update();
    
    float getEffectiveProficiency() const;
};

struct CulturalTransmission {
    // What this entity has learned from others
    std::map<std::string, float> learnedBehaviors;  // behavior -> fidelity
    std::map<std::string, float> learnedBeliefs;
    
    // Teaching record
    std::map<int, std::vector<std::string>> taughtTo;  // entityId -> behaviors taught
    
    // Cultural learning biases
    float conformityBias = 0.3f;      // Tend to copy majority
    float prestigeBias = 0.4f;        // Tend to copy high-status
    float similarityBias = 0.3f;      // Tend to copy similar others
    float contentBias = 0.5f;         // Some content more memorable
    
    void learnFromObservation(Entity* model, 
                             const std::string& behavior,
                             float outcomeObserved);
    
    void teachBehavior(Entity* learner, const std::string& behavior);
};

struct PersonalityChangeTracker {
    // Track drift over time
    float extraversionDrift = 0.0f;
    float agreeablenessDrift = 0.0f;
    float conscientiousnessDrift = 0.0f;
    float neuroticismDrift = 0.0f;
    float opennessDrift = 0.0f;
    
    // Life events that caused changes
    struct ChangeEvent {
        std::string eventType;
        float impactMagnitude;
        int simulationDay;
        std::string affectedTrait;
    };
    
    std::deque<ChangeEvent> changeHistory;
    
    void recordChange(const std::string& trait, 
                     float delta,
                     const std::string& cause);
    
    void applyCumulativeChanges(Entity* entity);
};

class LearningAdaptationSystem {
private:
    std::map<int, ActionValueFunction> entityQFunctions;
    std::map<int, std::vector<HabitStrength>> entityHabits;
    std::map<int, std::map<std::string, Skill>> entitySkills;
    std::map<int, CulturalTransmission> culturalLearning;
    std::map<int, PersonalityChangeTracker> personalityChanges;
    
public:
    LearningAdaptationSystem();
    
    // Initialize systems for entity
    void initializeEntity(Entity* entity);
    
    // Reinforcement learning
    void processExperience(Entity* entity,
                          const std::string& state,
                          const std::string& action,
                          float reward,
                          const std::string& nextState);
    
    std::string selectActionRL(Entity* entity,
                              const std::string& state,
                              const std::vector<std::string>& actions);
    
    // Habit system
    void reinforceHabit(Entity* entity,
                       const std::string& action,
                       const std::string& context,
                       float reward);
    
    void updateHabits(Entity* entity, float deltaTime);
    
    float getHabitStrength(Entity* entity,
                          const std::string& action,
                          const std::string& context);
    
    // Skill development
    void practiceSkill(Entity* entity,
                      const std::string& skill,
                      float quality);
    
    Skill* getSkill(Entity* entity, const std::string& skillName);
    
    // Cultural transmission
    void observeAndLearn(Entity* observer,
                        Entity* model,
                        const std::string& behavior,
                        float observedOutcome);
    
    void teachBehavior(Entity* teacher,
                      Entity* learner,
                      const std::string& behavior);
    
    // Personality adaptation
    void updatePersonalityFromExperience(Entity* entity,
                                        const std::string& experienceType,
                                        float outcomeValence);
    
    void applyPersonalityChanges(Entity* entity);
    
    // Accessors
    const ActionValueFunction& getQFunction(int entityId) const;
    const std::vector<HabitStrength>& getHabits(int entityId) const;
    
    // Serialization
    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
};

#endif // LEARNING_ADAPTATION_H
