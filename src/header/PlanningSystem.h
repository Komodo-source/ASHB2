#ifndef PLANNING_SYSTEM_H
#define PLANNING_SYSTEM_H

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <random>
#include <fstream>
#include <algorithm>
#include <deque>
#include <sstream>
#include <functional>

class Entity;

// ============================================================================
// PlanStep: A single step in a plan
// ============================================================================
struct PlanStep {
    std::string actionName;           // The action to take
    std::string targetType;           // "self", "specific_entity", "any_entity", "location"
    int targetEntityId;               // Specific entity target (-1 if none/any)
    float priority;                   // Priority of this step (0-1)
    int expectedDuration;             // Expected ticks to complete
    std::string rationale;            // Why this step was chosen
    
    // Execution state
    bool completed;                   // Whether this step was completed
    bool failed;                      // Whether this step failed
    int ticksExecuted;                // How many ticks spent on this step
    float outcomeScore;               // How successful the step was (0-1)
    
    PlanStep() 
        : targetEntityId(-1), priority(0.5f), expectedDuration(5),
          completed(false), failed(false), ticksExecuted(0), outcomeScore(0.0f) {}
    
    PlanStep(const std::string& action, float prio, const std::string& reason)
        : actionName(action), targetType("self"), targetEntityId(-1), 
          priority(prio), expectedDuration(5), rationale(reason),
          completed(false), failed(false), ticksExecuted(0), outcomeScore(0.0f) {}
};

// ============================================================================
// PlanEvaluation: How a plan is performing
// ============================================================================
struct PlanEvaluation {
    float overallProgress;           // 0-1: How much of the plan is done
    float successRate;               // 0-1: How successful steps have been
    int stepsCompleted;              // Number of completed steps
    int stepsFailed;                 // Number of failed steps
    int totalSteps;                  // Total steps in plan
    float satisfaction;              // How satisfied entity is with plan progress
    std::string reflection;          // Text reflection on what worked/didn't
    
    PlanEvaluation() 
        : overallProgress(0.0f), successRate(0.0f), stepsCompleted(0), 
          stepsFailed(0), totalSteps(0), satisfaction(0.5f) {}
};

// ============================================================================
// AlternativePlan: An alternative plan generated during Tree-of-Thoughts
// ============================================================================
struct AlternativePlan {
    std::vector<PlanStep> steps;
    float expectedSuccess;           // Estimated success probability
    float expectedSatisfaction;      // Estimated need satisfaction
    std::string reasoning;           // Why this alternative was considered
    
    AlternativePlan() : expectedSuccess(0.0f), expectedSatisfaction(0.0f) {}
};

// ============================================================================
// DailyPlan: A plan for the current day/cycle
// ============================================================================
struct DailyPlan {
    std::vector<PlanStep> steps;     // Ordered steps to execute
    int currentStepIndex;            // Which step the entity is on
    std::string primaryGoal;         // The main goal driving this plan
    int planGenerationDay;           // Day this plan was generated
    bool isActive;                   // Whether this plan is currently being followed
    
    // Sub-plans for Tree-of-Thoughts exploration
    std::vector<AlternativePlan> alternatives;
    int selectedAlternativeIndex;    // Which alternative was chosen (-1 if main plan)
    
    DailyPlan() 
        : currentStepIndex(0), planGenerationDay(0), isActive(false), 
          selectedAlternativeIndex(-1) {}
    
    bool isComplete() const {
        return currentStepIndex >= (int)steps.size();
    }
    
    PlanStep* getCurrentStep() {
        if (currentStepIndex < (int)steps.size()) {
            return &steps[currentStepIndex];
        }
        return nullptr;
    }
    
    void advanceStep() {
        if (currentStepIndex < (int)steps.size()) {
            currentStepIndex++;
        }
    }
};

// ============================================================================
// PlanningConfig: Configuration for the planning system
// ============================================================================
struct PlanningConfig {
    int maxPlanSteps;                // Maximum steps in a daily plan
    int planHorizonDays;             // How many days to plan ahead
    int rePlanInterval;              // Ticks between re-planning
    float explorationRate;           // How often to explore alternative plans
    int maxAlternatives;             // Max alternative plans (Tree-of-Thoughts branching)
    
    // Thresholds for re-planning
    float frustrationThreshold;      // Frustration level that triggers re-plan
    float emergencyNeedThreshold;    // Need urgency that overrides plans
    
    PlanningConfig()
        : maxPlanSteps(8), planHorizonDays(1), rePlanInterval(50),
          explorationRate(0.15f), maxAlternatives(3),
          frustrationThreshold(0.7f), emergencyNeedThreshold(85.0f) {}
};

// ============================================================================
// PlanningSystem: Main Tree-of-Thoughts planning system
// ============================================================================
class PlanningSystem {
private:
    DailyPlan currentPlan;
    PlanningConfig config;
    
    // Entity state tracking
    float cumulativeFrustration;
    int ticksSinceLastPlan;
    
    // Random generation
    std::mt19937 rng;
    
    // ========================================================================
    // Internal Planning Methods
    // ========================================================================
    
    // Analyze entity state to determine primary goals
    std::vector<std::pair<std::string, float>> analyzeGoals(Entity* entity);
    
    // Generate steps to accomplish a specific goal type
    std::vector<PlanStep> generateStepsForGoal(Entity* entity, 
                                                const std::string& goalType,
                                                float goalPriority);
    
    // Generate alternative plans (Tree-of-Thoughts branching)
    std::vector<AlternativePlan> generateAlternatives(Entity* entity,
                                                       const std::vector<PlanStep>& mainSteps);
    
    // Evaluate the expected success of an alternative plan
    float evaluateAlternative(Entity* entity, const AlternativePlan& alt);
    
    // Create a step for finding/approaching a specific entity type
    PlanStep createSocialStep(Entity* entity, const std::string& goalType);
    
    // Create a self-improvement step
    PlanStep createSelfImprovementStep(Entity* entity);
    
    // Create a survival/maintenance step
    PlanStep createSurvivalStep(Entity* entity);
    
    // Calculate step priority based on entity state
    float calculateStepPriority(Entity* entity, const std::string& actionName);
    
    // ========================================================================
    // Reflection Methods
    // ========================================================================
    
    // Evaluate how the current plan is progressing
    PlanEvaluation evaluatePlanProgress(Entity* entity);
    
    // Generate textual reflection from plan evaluation
    std::string generatePlanReflection(Entity* entity, const PlanEvaluation& eval);

public:
    PlanningSystem();
    explicit PlanningSystem(const PlanningConfig& cfg);
    
    // ========================================================================
    // Main API
    // ========================================================================
    
    // Generate a new daily plan based on entity's current state, goals, and values
    DailyPlan generateDailyPlan(Entity* entity);
    
    // Get the next action the entity should take according to the plan
    // Returns action name, or empty string if no plan step is needed
    std::string getNextPlannedAction(Entity* entity, 
                                     const std::vector<Entity*>& neighbors,
                                     float emergencyUrgency = 0.0f);
    
    // Report the outcome of a planned action (for plan evaluation)
    void reportActionResult(Entity* entity, const std::string& actionName, 
                            float successScore, bool wasEmergency = false);
    
    // Check if re-planning is needed
    bool shouldReplan(Entity* entity);
    
    // Force re-planning
    void forceReplan() { currentPlan.isActive = false; ticksSinceLastPlan = config.rePlanInterval + 1; }
    
    // ========================================================================
    // Tick / Update
    // ========================================================================
    
    // Update planning state each tick
    void tick(Entity* entity, const std::vector<Entity*>& neighbors, float deltaTime);
    
    // ========================================================================
    // Accessors
    // ========================================================================
    
    bool hasActivePlan() const { return currentPlan.isActive && !currentPlan.isComplete(); }
    const DailyPlan& getCurrentPlan() const { return currentPlan; }
    PlanEvaluation getLastEvaluation() const { PlanEvaluation e; e.totalSteps = (int)currentPlan.steps.size(); return e; }
    
    // ========================================================================
    // Persistence
    // ========================================================================
    
    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
};

#endif // PLANNING_SYSTEM_H
