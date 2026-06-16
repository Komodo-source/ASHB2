#include "./header/PlanningSystem.h"
#include "./header/Entity.h"
#include "./header/FreeWillSystem.h"
#include "./header/WorldSeed.h"
#include <algorithm>
#include <cmath>
#include <queue>

// ============================================================================
// PlanningSystem Implementation
// ============================================================================

PlanningSystem::PlanningSystem()
    : cumulativeFrustration(0.0f), ticksSinceLastPlan(0),
      rng(static_cast<std::mt19937::result_type>(nextDeterministicSeed(0x9111'A11ull)))
{}

PlanningSystem::PlanningSystem(const PlanningConfig& cfg)
    : config(cfg), cumulativeFrustration(0.0f), ticksSinceLastPlan(0),
      rng(static_cast<std::mt19937::result_type>(nextDeterministicSeed(0x9111'A11ull)))
{}

// ============================================================================
// Goal Analysis
// ============================================================================

std::vector<std::pair<std::string, float>> PlanningSystem::analyzeGoals(Entity* entity) {
    std::vector<std::pair<std::string, float>> prioritizedGoals;
    
    if (!entity) return prioritizedGoals;
    
    // 1. Check hierarchical needs (survival first)
    if (entity->entityHealth < 30.0f || entity->entityStress > 70.0f) {
        prioritizedGoals.push_back({"survival", 1.0f});
    }
    
    // 2. Check for critical hunger/stress needs
    for (const auto& [needName, need] : entity->needs) {
        if (need.urgency > 80.0f) {
            prioritizedGoals.push_back({"address_urgent_need", 0.9f});
            break;
        }
    }
    
    // 3. Social needs
    if (entity->entityLoneliness > 60.0f || entity->entityBoredom > 60.0f) {
        prioritizedGoals.push_back({"socialize", 0.7f});
    }
    
    // 4. Life goals
    for (const auto& goal : entity->m_goals) {
        if (goal.frustrationLevel > config.frustrationThreshold) {
            // This goal needs attention
            float priority = goal.priority * (1.0f + goal.frustrationLevel);
            if (goal.type == "find_partner") {
                prioritizedGoals.push_back({"find_partner", priority});
            } else if (goal.type == "build_career") {
                prioritizedGoals.push_back({"build_career", priority});
            } else if (goal.type == "make_friends") {
                prioritizedGoals.push_back({"make_friends", priority});
            } else if (goal.type == "build_family") {
                prioritizedGoals.push_back({"build_family", priority});
            } else if (goal.type == "happiness") {
                prioritizedGoals.push_back({"happiness", priority});
            }
        } else {
            float priority = goal.priority * 0.5f;
            if (goal.type == "find_partner") {
                prioritizedGoals.push_back({"find_partner", priority});
            } else if (goal.type == "build_career") {
                prioritizedGoals.push_back({"build_career", priority});
            } else if (goal.type == "make_friends") {
                prioritizedGoals.push_back({"make_friends", priority});
            } else if (goal.type == "build_family") {
                prioritizedGoals.push_back({"build_family", priority});
            } else if (goal.type == "happiness") {
                prioritizedGoals.push_back({"happiness", priority});
            }
        }
    }
    
    // 5. Maintain baseline needs
    if (entity->entityHapiness < 40.0f) {
        prioritizedGoals.push_back({"improve_mood", 0.4f});
    }
    
    // Sort by priority descending
    std::sort(prioritizedGoals.begin(), prioritizedGoals.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Remove duplicates
    auto last = std::unique(prioritizedGoals.begin(), prioritizedGoals.end(),
                            [](const auto& a, const auto& b) { return a.first == b.first; });
    prioritizedGoals.erase(last, prioritizedGoals.end());
    
    return prioritizedGoals;
}

// ============================================================================
// Step Generation
// ============================================================================

PlanStep PlanningSystem::createSocialStep(Entity* entity, const std::string& goalType) {
    PlanStep step;
    
    if (goalType == "find_partner") {
        step.actionName = "flirt";
        step.targetType = "specific_entity";
        step.priority = 0.8f;
        step.expectedDuration = 10;
        step.rationale = "Looking for a romantic partner";
    } else if (goalType == "make_friends") {
        step.actionName = "socialize";
        step.targetType = "any_entity";
        step.priority = 0.7f;
        step.expectedDuration = 8;
        step.rationale = "Building social connections";
    } else if (goalType == "build_family") {
        // First find partner, then build family
        if (!entity->list_entityPointedCouple.empty()) {
            step.actionName = "procreate";
            step.targetType = "specific_entity";
            step.targetEntityId = entity->list_entityPointedCouple[0].id;
            step.priority = 0.9f;
            step.expectedDuration = 5;
            step.rationale = "Building a family with partner";
        } else {
            step.actionName = "flirt";
            step.targetType = "specific_entity";
            step.priority = 0.6f;
            step.expectedDuration = 10;
            step.rationale = "Need to find a partner first to build a family";
        }
    }
    
    return step;
}

PlanStep PlanningSystem::createSelfImprovementStep(Entity* entity) {
    PlanStep step;
    step.actionName = "build_career";
    step.targetType = "self";
    step.priority = 0.6f;
    step.expectedDuration = 8;
    step.rationale = "Working on career development";
    return step;
}

PlanStep PlanningSystem::createSurvivalStep(Entity* entity) {
    PlanStep step;
    
    // Check what's most urgent
    if (entity->entityHealth < 30.0f) {
        step.actionName = "heal";
        step.targetType = "self";
        step.priority = 1.0f;
        step.expectedDuration = 5;
        step.rationale = "Critical health needs attention";
    } else if (entity->entityStress > 70.0f) {
        step.actionName = "rest";
        step.targetType = "self";
        step.priority = 0.9f;
        step.expectedDuration = 6;
        step.rationale = "Stress levels are too high, need to rest";
    } else if (entity->entityLoneliness > 60.0f) {
        step.actionName = "socialize";
        step.targetType = "any_entity";
        step.priority = 0.7f;
        step.expectedDuration = 8;
        step.rationale = "Feeling lonely, need social contact";
    } else {
        step.actionName = "maintain";
        step.targetType = "self";
        step.priority = 0.5f;
        step.expectedDuration = 4;
        step.rationale = "General maintenance and self-care";
    }
    
    return step;
}

float PlanningSystem::calculateStepPriority(Entity* entity, const std::string& actionName) {
    if (!entity) return 0.5f;
    
    // Map action names to entity stats for dynamic priority
    if (actionName == "rest" || actionName == "heal") {
        return std::min(1.0f, (100.0f - entity->entityHealth) / 100.0f + 
                              entity->entityStress / 100.0f * 0.5f);
    } else if (actionName == "socialize" || actionName == "flirt") {
        return std::min(1.0f, entity->entityLoneliness / 100.0f + 
                              entity->entityBoredom / 100.0f * 0.3f);
    } else if (actionName == "build_career" || actionName == "work") {
        return 0.4f + (1.0f - entity->entityStress / 100.0f) * 0.3f;
    } else if (actionName == "fun" || actionName == "entertainment") {
        return std::min(1.0f, (100.0f - entity->entityHapiness) / 100.0f + 
                              entity->entityBoredom / 100.0f * 0.5f);
    }
    
    return 0.5f;
}

std::vector<PlanStep> PlanningSystem::generateStepsForGoal(
    Entity* entity, const std::string& goalType, float goalPriority)
{
    std::vector<PlanStep> steps;
    
    if (goalType == "survival") {
        steps.push_back(createSurvivalStep(entity));
        steps.push_back(createSurvivalStep(entity)); // Double up for critical situations
        
        // Add hygiene if needed
        if (entity->entityHygiene < 30.0f) {
            PlanStep hygiene;
            hygiene.actionName = "hygiene";
            hygiene.targetType = "self";
            hygiene.priority = 0.7f;
            hygiene.expectedDuration = 3;
            hygiene.rationale = "Need to maintain personal hygiene";
            steps.push_back(hygiene);
        }
        
    } else if (goalType == "address_urgent_need") {
        // Create steps to address urgent needs
        for (const auto& [needName, need] : entity->needs) {
            if (need.urgency > 80.0f) {
                PlanStep step;
                step.actionName = "satisfy_" + needName;
                step.targetType = "self";
                step.priority = 0.9f;
                step.expectedDuration = 5;
                step.rationale = "Urgent need: " + needName;
                steps.push_back(step);
            }
        }
        
    } else if (goalType == "socialize" || goalType == "find_partner" || 
               goalType == "make_friends" || goalType == "build_family") {
        // 1. First, approach someone
        PlanStep approach = createSocialStep(entity, goalType);
        steps.push_back(approach);
        
        // 2. Then reinforce the bond
        PlanStep reinforce;
        reinforce.actionName = "kind_action";
        reinforce.targetType = "specific_entity";
        reinforce.priority = 0.6f;
        reinforce.expectedDuration = 5;
        reinforce.rationale = "Reinforcing social bond";
        steps.push_back(reinforce);
        
        // 3. Quality time
        PlanStep quality;
        quality.actionName = "socialize";
        quality.targetType = "specific_entity";
        quality.priority = 0.5f;
        quality.expectedDuration = 8;
        quality.rationale = "Spending quality time together";
        steps.push_back(quality);
        
    } else if (goalType == "build_career") {
        PlanStep work = createSelfImprovementStep(entity);
        steps.push_back(work);
        
        PlanStep network;
        network.actionName = "socialize";
        network.targetType = "any_entity";
        network.priority = 0.4f;
        network.expectedDuration = 5;
        network.rationale = "Professional networking";
        steps.push_back(network);
        
        PlanStep learn;
        learn.actionName = "build_career";
        learn.targetType = "self";
        learn.priority = 0.5f;
        learn.expectedDuration = 8;
        learn.rationale = "Skill development for career";
        steps.push_back(learn);
        
    } else if (goalType == "happiness" || goalType == "improve_mood") {
        PlanStep fun;
        fun.actionName = "fun";
        fun.targetType = "self";
        fun.priority = 0.7f;
        fun.expectedDuration = 6;
        fun.rationale = "Need some enjoyment";
        steps.push_back(fun);
        
        PlanStep social;
        social.actionName = "socialize";
        social.targetType = "any_entity";
        social.priority = 0.5f;
        social.expectedDuration = 5;
        social.rationale = "Socializing improves mood";
        steps.push_back(social);
    }
    
    // Apply priority scaling based on goalPriority
    for (auto& step : steps) {
        step.priority = std::min(1.0f, step.priority * (0.5f + goalPriority * 0.5f));
    }
    
    // Limit steps
    if ((int)steps.size() > config.maxPlanSteps) {
        steps.resize(config.maxPlanSteps);
    }
    
    return steps;
}

// ============================================================================
// Alternative Generation (Tree-of-Thoughts)
// ============================================================================

float PlanningSystem::evaluateAlternative(Entity* entity, const AlternativePlan& alt) {
    // Score based on what's currently needed
    float score = alt.expectedSuccess * 0.4f + alt.expectedSatisfaction * 0.6f;
    
    // Personality-based adjustment
    if (entity) {
        // Conscientious entities prefer well-planned alternatives
        if (entity->personality.conscientiousness > 0.6f) {
            score *= 1.1f;
        }
        // Open entities are more willing to try novel approaches
        if (entity->personality.openness > 0.6f) {
            score *= 1.05f;
        }
    }
    
    return score;
}

std::vector<AlternativePlan> PlanningSystem::generateAlternatives(
    Entity* entity, const std::vector<PlanStep>& mainSteps)
{
    std::vector<AlternativePlan> alternatives;
    
    if (!entity) return alternatives;
    
    // Alternative 1: Focus on immediate survival/health
    if (entity->entityHealth < 50.0f || entity->entityStress > 50.0f) {
        AlternativePlan survivalAlt;
        survivalAlt.reasoning = "Prioritizing survival and health over other goals";
        
        PlanStep rest;
        rest.actionName = "rest";
        rest.priority = 0.9f;
        rest.rationale = "Recuperating health";
        survivalAlt.steps.push_back(rest);
        
        if (entity->entityHapiness < 40.0f) {
            PlanStep fun;
            fun.actionName = "fun";
            fun.priority = 0.6f;
            fun.rationale = "Improving mood";
            survivalAlt.steps.push_back(fun);
        }
        
        // Add one step from the original plan to maintain progress
        if (!mainSteps.empty()) {
            survivalAlt.steps.push_back(mainSteps[0]);
        }
        
        survivalAlt.expectedSuccess = 0.7f;
        survivalAlt.expectedSatisfaction = 0.8f;
        survivalAlt.reasoning += " | Kept one step from original plan";
        
        survivalAlt.expectedSuccess = evaluateAlternative(entity, survivalAlt);
        alternatives.push_back(survivalAlt);
    }
    
    // Alternative 2: Full social focus
    if (entity->entityLoneliness > 40.0f) {
        AlternativePlan socialAlt;
        socialAlt.reasoning = "Focusing entirely on social needs";
        
        PlanStep social1;
        social1.actionName = "socialize";
        social1.priority = 0.8f;
        social1.rationale = "Primary social interaction";
        socialAlt.steps.push_back(social1);
        
        PlanStep social2;
        social2.actionName = "kind_action";
        social2.priority = 0.6f;
        social2.rationale = "Strengthening bonds through kindness";
        socialAlt.steps.push_back(social2);
        
        PlanStep social3;
        social3.actionName = "socialize";
        social3.priority = 0.5f;
        social3.rationale = "Continued social engagement";
        socialAlt.steps.push_back(social3);
        
        socialAlt.expectedSuccess = 0.6f;
        socialAlt.expectedSatisfaction = 0.7f;
        socialAlt.expectedSuccess = evaluateAlternative(entity, socialAlt);
        alternatives.push_back(socialAlt);
    }
    
    // Alternative 3: Work/career focus
    if (!entity->m_goals.empty() && entity->entityStress < 60.0f) {
        for (const auto& goal : entity->m_goals) {
            if (goal.type == "build_career" && goal.priority > 0.4f) {
                AlternativePlan careerAlt;
                careerAlt.reasoning = "Dedicating time to career advancement";
                
                PlanStep work1;
                work1.actionName = "build_career";
                work1.priority = 0.9f;
                work1.rationale = "Primary career work";
                careerAlt.steps.push_back(work1);
                
                PlanStep work2;
                work2.actionName = "build_career";
                work2.priority = 0.7f;
                work2.rationale = "Continued career development";
                careerAlt.steps.push_back(work2);
                
                PlanStep network;
                network.actionName = "socialize";
                network.priority = 0.4f;
                network.rationale = "Professional networking break";
                careerAlt.steps.push_back(network);
                
                careerAlt.expectedSuccess = 0.8f;
                careerAlt.expectedSatisfaction = 0.5f;
                careerAlt.expectedSuccess = evaluateAlternative(entity, careerAlt);
                alternatives.push_back(careerAlt);
                break;
            }
        }
    }
    
    // Limit alternatives
    if ((int)alternatives.size() > config.maxAlternatives) {
        alternatives.resize(config.maxAlternatives);
    }
    
    // Score and sort alternatives
    for (auto& alt : alternatives) {
        alt.expectedSuccess = evaluateAlternative(entity, alt);
    }
    std::sort(alternatives.begin(), alternatives.end(),
              [](const auto& a, const auto& b) { return a.expectedSuccess > b.expectedSuccess; });
    
    return alternatives;
}

// ============================================================================
// Plan Generation
// ============================================================================

DailyPlan PlanningSystem::generateDailyPlan(Entity* entity) {
    DailyPlan plan;
    
    if (!entity) return plan;
    
    plan.planGenerationDay = FreeWillSystem::day;
    plan.isActive = true;
    plan.currentStepIndex = 0;
    
    // Analyze goals and prioritize
    auto prioritizedGoals = analyzeGoals(entity);
    
    if (prioritizedGoals.empty()) {
        // Default: maintain and socialize
        prioritizedGoals.push_back({"socialize", 0.5f});
        prioritizedGoals.push_back({"improve_mood", 0.4f});
    }
    
    // Generate steps from top goals
    int maxGoals = std::min(3, (int)prioritizedGoals.size());
    for (int i = 0; i < maxGoals; ++i) {
        auto steps = generateStepsForGoal(entity, prioritizedGoals[i].first, prioritizedGoals[i].second);
        plan.steps.insert(plan.steps.end(), steps.begin(), steps.end());
    }
    
    // Remove duplicate consecutive actions
    std::vector<PlanStep> deduped;
    for (size_t i = 0; i < plan.steps.size(); ++i) {
        if (i == 0 || plan.steps[i].actionName != plan.steps[i-1].actionName) {
            deduped.push_back(plan.steps[i]);
        }
    }
    plan.steps = deduped;
    
    // Sort steps by priority
    std::sort(plan.steps.begin(), plan.steps.end(),
              [](const auto& a, const auto& b) { return a.priority > b.priority; });
    
    // Limit steps
    if ((int)plan.steps.size() > config.maxPlanSteps) {
        plan.steps.resize(config.maxPlanSteps);
    }
    
    // Set primary goal
    if (!prioritizedGoals.empty()) {
        plan.primaryGoal = prioritizedGoals[0].first;
    }
    
    // Generate alternatives (Tree-of-Thoughts)
    plan.alternatives = generateAlternatives(entity, plan.steps);
    
    // Decide whether to use an alternative (exploration)
    if (!plan.alternatives.empty() && 
        std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < config.explorationRate) {
        // Pick the best alternative
        int bestAlt = 0;
        for (size_t i = 1; i < plan.alternatives.size(); ++i) {
            if (plan.alternatives[i].expectedSuccess > plan.alternatives[bestAlt].expectedSuccess) {
                bestAlt = (int)i;
            }
        }
        
        plan.selectedAlternativeIndex = bestAlt;
        plan.steps = plan.alternatives[bestAlt].steps;
    }
    
    return plan;
}

// ============================================================================
// Plan Execution
// ============================================================================

std::string PlanningSystem::getNextPlannedAction(Entity* entity, 
                                                  const std::vector<Entity*>& neighbors,
                                                  float emergencyUrgency)
{
    // If there's an emergency, return empty to let the base system handle it
    if (emergencyUrgency > config.emergencyNeedThreshold) {
        return ""; // Emergency override
    }
    
    if (!currentPlan.isActive || currentPlan.isComplete()) {
        // Generate new plan if needed
        if (!currentPlan.isActive) {
            currentPlan = generateDailyPlan(entity);
            ticksSinceLastPlan = 0;
        }
        if (currentPlan.isComplete()) {
            return ""; // Plan complete, let base system decide
        }
    }
    
    PlanStep* currentStep = currentPlan.getCurrentStep();
    if (!currentStep) {
        return "";
    }
    
    // Check if the step is still valid based on current state
    if (currentStep->failed || currentStep->completed) {
        currentPlan.advanceStep();
        return getNextPlannedAction(entity, neighbors, emergencyUrgency);
    }
    
    return currentStep->actionName;
}

void PlanningSystem::reportActionResult(Entity* entity, const std::string& actionName, 
                                         float successScore, bool wasEmergency)
{
    if (!currentPlan.isActive || currentPlan.isComplete()) return;
    
    PlanStep* currentStep = currentPlan.getCurrentStep();
    if (!currentStep) return;
    
    // Check if this action matches the planned step
    if (currentStep->actionName == actionName && !wasEmergency) {
        currentStep->ticksExecuted++;
        currentStep->outcomeScore = successScore;
        
        if (successScore > 0.5f) {
            currentStep->completed = true;
            cumulativeFrustration = std::max(0.0f, cumulativeFrustration - 0.1f);
        } else {
            // Partial success - try again
            if (currentStep->ticksExecuted >= 3) {
                currentStep->failed = true;
                cumulativeFrustration += 0.2f;
            }
        }
    } else if (wasEmergency) {
        // Emergency actions don't count against the plan
        cumulativeFrustration += 0.05f;
    } else {
        // Entity deviated from plan
        cumulativeFrustration += 0.1f;
    }
}

// ============================================================================
// Re-planning Check
// ============================================================================

bool PlanningSystem::shouldReplan(Entity* entity) {
    if (!entity) return false;
    
    // Re-plan if frustration is too high
    if (cumulativeFrustration > config.frustrationThreshold) {
        return true;
    }
    
    // Re-plan if it's been too long
    if (ticksSinceLastPlan >= config.rePlanInterval) {
        return true;
    }
    
    // Re-plan if entity state has significantly changed
    for (const auto& [needName, need] : entity->needs) {
        if (need.urgency > config.emergencyNeedThreshold) {
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// Plan Evaluation
// ============================================================================

PlanEvaluation PlanningSystem::evaluatePlanProgress(Entity* entity) {
    PlanEvaluation eval;
    
    eval.totalSteps = (int)currentPlan.steps.size();
    eval.stepsCompleted = 0;
    eval.stepsFailed = 0;
    
    for (const auto& step : currentPlan.steps) {
        if (step.completed) eval.stepsCompleted++;
        if (step.failed) eval.stepsFailed++;
    }
    
    eval.overallProgress = eval.totalSteps > 0 ? 
        (float)eval.stepsCompleted / eval.totalSteps : 0.0f;
    eval.successRate = eval.stepsCompleted + eval.stepsFailed > 0 ?
        (float)eval.stepsCompleted / (eval.stepsCompleted + eval.stepsFailed) : 0.0f;
    eval.satisfaction = eval.successRate * (1.0f - cumulativeFrustration);
    
    eval.reflection = generatePlanReflection(entity, eval);
    
    return eval;
}

std::string PlanningSystem::generatePlanReflection(Entity* entity, const PlanEvaluation& eval) {
    std::ostringstream reflection;
    
    if (eval.totalSteps == 0) {
        reflection << "No plan was made. ";
        if (entity) {
            reflection << (entity->personality.conscientiousness > 0.5f ? 
                "Should be more organized." : "Going with the flow feels natural.");
        }
        return reflection.str();
    }
    
    float rate = eval.successRate;
    
    if (rate > 0.8f) {
        reflection << "The plan went well. ";
        if (entity && entity->personality.conscientiousness > 0.6f) {
            reflection << "Being organized paid off.";
        } else {
            reflection << "Things worked out nicely.";
        }
    } else if (rate > 0.4f) {
        reflection << "Mixed results. Some steps worked, others didn't. ";
        if (entity) {
            if (cumulativeFrustration > 0.5f) {
                reflection << "Getting frustrated with the lack of progress.";
            } else {
                reflection << "Will try a different approach next time.";
            }
        }
    } else {
        reflection << "The plan didn't work well. ";
        if (entity && entity->personality.neuroticism > 0.6f) {
            reflection << "Feeling anxious about the failures.";
        } else {
            reflection << "Need to reconsider priorities.";
        }
    }
    
    return reflection.str();
}

// ============================================================================
// Tick
// ============================================================================

void PlanningSystem::tick(Entity* entity, const std::vector<Entity*>& neighbors, float deltaTime) {
    if (!entity) return;
    
    ticksSinceLastPlan++;
    
    // Check if we need to re-plan
    if (shouldReplan(entity)) {
        // Generate reflection on the old plan
        PlanEvaluation eval = evaluatePlanProgress(entity);
        
        // Generate new plan
        currentPlan = generateDailyPlan(entity);
        ticksSinceLastPlan = 0;
        cumulativeFrustration = std::max(0.0f, cumulativeFrustration - 0.3f); // Reset some frustration
    }
}

// ============================================================================
// Persistence
// ============================================================================

void PlanningSystem::saveTo(std::ofstream& file) const {
    // Save current plan
    size_t stepsSize = currentPlan.steps.size();
    file.write(reinterpret_cast<const char*>(&stepsSize), sizeof(stepsSize));
    for (const auto& step : currentPlan.steps) {
        size_t nameLen = step.actionName.length();
        file.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        file.write(step.actionName.data(), nameLen);
        
        size_t targetLen = step.targetType.length();
        file.write(reinterpret_cast<const char*>(&targetLen), sizeof(targetLen));
        file.write(step.targetType.data(), targetLen);
        
        file.write(reinterpret_cast<const char*>(&step.targetEntityId), sizeof(step.targetEntityId));
        file.write(reinterpret_cast<const char*>(&step.priority), sizeof(step.priority));
        file.write(reinterpret_cast<const char*>(&step.expectedDuration), sizeof(step.expectedDuration));
        
        size_t rationaleLen = step.rationale.length();
        file.write(reinterpret_cast<const char*>(&rationaleLen), sizeof(rationaleLen));
        file.write(step.rationale.data(), rationaleLen);
        
        file.write(reinterpret_cast<const char*>(&step.completed), sizeof(step.completed));
        file.write(reinterpret_cast<const char*>(&step.failed), sizeof(step.failed));
        file.write(reinterpret_cast<const char*>(&step.ticksExecuted), sizeof(step.ticksExecuted));
        file.write(reinterpret_cast<const char*>(&step.outcomeScore), sizeof(step.outcomeScore));
    }
    
    file.write(reinterpret_cast<const char*>(&currentPlan.currentStepIndex), sizeof(currentPlan.currentStepIndex));
    
    size_t goalLen = currentPlan.primaryGoal.length();
    file.write(reinterpret_cast<const char*>(&goalLen), sizeof(goalLen));
    file.write(currentPlan.primaryGoal.data(), goalLen);
    
    file.write(reinterpret_cast<const char*>(&currentPlan.planGenerationDay), sizeof(currentPlan.planGenerationDay));
    file.write(reinterpret_cast<const char*>(&currentPlan.isActive), sizeof(currentPlan.isActive));
    
    file.write(reinterpret_cast<const char*>(&cumulativeFrustration), sizeof(cumulativeFrustration));
    file.write(reinterpret_cast<const char*>(&ticksSinceLastPlan), sizeof(ticksSinceLastPlan));
}

void PlanningSystem::loadFrom(std::ifstream& file) {
    // Load current plan
    size_t stepsSize;
    file.read(reinterpret_cast<char*>(&stepsSize), sizeof(stepsSize));
    currentPlan.steps.resize(stepsSize);
    for (auto& step : currentPlan.steps) {
        size_t nameLen;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        step.actionName.resize(nameLen);
        file.read(&step.actionName[0], nameLen);
        
        size_t targetLen;
        file.read(reinterpret_cast<char*>(&targetLen), sizeof(targetLen));
        step.targetType.resize(targetLen);
        file.read(&step.targetType[0], targetLen);
        
        file.read(reinterpret_cast<char*>(&step.targetEntityId), sizeof(step.targetEntityId));
        file.read(reinterpret_cast<char*>(&step.priority), sizeof(step.priority));
        file.read(reinterpret_cast<char*>(&step.expectedDuration), sizeof(step.expectedDuration));
        
        size_t rationaleLen;
        file.read(reinterpret_cast<char*>(&rationaleLen), sizeof(rationaleLen));
        step.rationale.resize(rationaleLen);
        file.read(&step.rationale[0], rationaleLen);
        
        file.read(reinterpret_cast<char*>(&step.completed), sizeof(step.completed));
        file.read(reinterpret_cast<char*>(&step.failed), sizeof(step.failed));
        file.read(reinterpret_cast<char*>(&step.ticksExecuted), sizeof(step.ticksExecuted));
        file.read(reinterpret_cast<char*>(&step.outcomeScore), sizeof(step.outcomeScore));
    }
    
    file.read(reinterpret_cast<char*>(&currentPlan.currentStepIndex), sizeof(currentPlan.currentStepIndex));
    
    size_t goalLen;
    file.read(reinterpret_cast<char*>(&goalLen), sizeof(goalLen));
    currentPlan.primaryGoal.resize(goalLen);
    file.read(&currentPlan.primaryGoal[0], goalLen);
    
    file.read(reinterpret_cast<char*>(&currentPlan.planGenerationDay), sizeof(currentPlan.planGenerationDay));
    file.read(reinterpret_cast<char*>(&currentPlan.isActive), sizeof(currentPlan.isActive));
    
    file.read(reinterpret_cast<char*>(&cumulativeFrustration), sizeof(cumulativeFrustration));
    file.read(reinterpret_cast<char*>(&ticksSinceLastPlan), sizeof(ticksSinceLastPlan));
}
