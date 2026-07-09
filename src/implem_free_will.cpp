#include "util/Trace.h"
#include "./header/BetterRand.h"
#include "./header/WorldSeed.h"
#include "world/ResourceSystem.h"
#include "world/Ecosystem.h"
#include "./header/SemanticMemory.h"
#include "./header/PlanningSystem.h"
#include "./header/Entity.h"
#include "./header/FreeWillSystem.h"
#include "./header/PersonaSystem.h"
#include "./header/Logging.h"
#include "./header/ExternalData.h"
#include "./header/SocialNormSystem.h"
#include "./header/Kinship.h"
#include "./header/CivilizationEngine.h"
#include "world/Lexicon.h"
#include "./header/LiveConfig.h"


#include <algorithm>
#include <cerrno>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

// ── Emergence upgrade (Steps 4/5a): heritable genome + NEAT brain at birth ────
// One deterministic gene stream shared by every birth path (same makeStream
// discipline as the civ streams; fresh salt so existing streams are untouched).
static void inheritEmergenceTraits(Entity& baby, Entity* pa, Entity* pb) {
    static std::mt19937_64 s_geneRng = makeStream(g_worldSeed.master, 0xB5297A4D3F84D5B5ull);
    baby.genome = Genome::combine(pa->genome, pb->genome, s_geneRng,
                                  g_liveConfig.mutationRateMul);
    std::uniform_real_distribution<float> integrityNoise(-10.0f, 10.0f);
    baby.integrity = std::max(0.0f, std::min(100.0f,
        (pa->integrity + pb->integrity) * 0.5f + integrityNoise(s_geneRng)));
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    if (pa->useNeatBrain && pb->useNeatBrain
        && !pa->neatGenome.empty() && !pb->neatGenome.empty()) {
        // Both parents carry evolved brains: innovation-aligned crossover with
        // the healthier parent treated as fitter, then mutation. This IS the
        // genetic algorithm - no backprop anywhere.
        Entity* fit   = (pa->entityHealth >= pb->entityHealth) ? pa : pb;
        Entity* other = (fit == pa) ? pb : pa;
        baby.neatGenome = neat::Genome::crossover(fit->neatGenome, other->neatGenome, s_geneRng);
        baby.neatGenome.mutate(s_geneRng, g_liveConfig.mutationRateMul);
        baby.useNeatBrain = true;
    } else if (roll(s_geneRng) < g_liveConfig.neatBrainShare) {
        // Seed fraction: some newborns get a fresh minimal network so a NEAT
        // population can bootstrap even from zero carriers.
        baby.neatGenome = neat::Genome::minimal(s_geneRng);
        baby.neatGenome.mutate(s_geneRng, g_liveConfig.mutationRateMul);
        baby.useNeatBrain = true;
    }
}


std::vector<Entity> FreeWillSystem::new_borns;
int FreeWillSystem::day;
std::vector<Action> FreeWillSystem::availableActions;
std::vector<Action> FreeWillSystem::desireLinkedAction;
std::vector<Action> FreeWillSystem::hatredLinkedAction;
int FreeWillSystem::possessedEntityId = -1;   // M10 possess mode
int FreeWillSystem::possessedActionIdx = -1;

std::vector<std::string> FreeWillSystem::actionNames() {
    std::vector<std::string> names;
    names.reserve(availableActions.size());
    for (const Action& a : availableActions) names.push_back(a.name);
    return names;
}
extern Logger* globalLogger;
// Seasonal/harvest food multiplier, driven by the EnvironmentModel in main.cpp.
extern float g_seasonalFoodModifier;

// Calculate memory weight (more recent = more weight)
float FreeWillSystem::getMemoryWeight(int memoryAge) {
    float decay = std::exp(-memoryAge / 20.0f);
    return decay;
}

// Calculate how well entity meets action requirements.
//
// Two classes of stat:
//   EXCESS stats  (stress, anger, loneliness, boredom) — action fires when stat IS HIGH
//   DEFICIENCY stats (hygiene, health, mentalHealth)   — action fires when stat IS LOW
//
// For EXCESS:   fitness peaks when currentValue >= requiredValue
// For DEFICIENCY: fitness peaks when currentValue <= requiredValue
float FreeWillSystem::calculateRequirementFitness(Entity* entity, const Action& action) {
    float totalFitness = 0.0f;
    float totalWeight = 0.0f;


    auto isDeficiencyStat = [](const std::string& s) -> bool {
        return s == "hygiene" || s == "health" || s == "mentalHealth";
    };

    for (const auto& req : action.requirements) {
        float currentValue = getEntityStat(entity, req.statName);
        if (entity->dv.attachmentStyle == AVOIDANT && req.statName == "loneliness") {
            currentValue = 0.0f; // avoidants suppress loneliness perception
        }

        float fitness;
        if (isDeficiencyStat(req.statName)) {
            float gap = req.requiredValue - currentValue; // positive = stat is too low
            if (gap <= 0.0f) {
                // Stat is fine — very low fitness for this action
                fitness = std::max(0.0f, 1.0f - (-gap / (req.requiredValue + 1.0f)) * 2.0f);
                fitness *= 0.15f; // strong suppression when not needed
            } else {
                // Stat is below threshold — fitness rises with urgency
                fitness = 1.0f + std::min(1.0f, gap / (req.requiredValue + 1.0f));
            }
        } else {
            float difference = currentValue - req.requiredValue;
            if (difference >= 0) {
                fitness = 1.0f + std::min(0.5f, difference / (req.requiredValue + 1.0f));
            } else {
                fitness = std::max(0.15f, 1.0f + (difference / (req.requiredValue + 1.0f)));
            }
        }

        totalFitness += fitness * req.weight;
        totalWeight += req.weight;
    }


    bool isSocialAction = (action.needCategory == "social");

    return totalWeight > 0 ? totalFitness / totalWeight : 1.0f;
}

float FreeWillSystem::getMaxUrgencyForLevel(const Entity* target, NeedLevel lvl) {
    float maxUrgency = 0.0f;
    for (const auto& [name, need] : target->needs) {
        if (need.level == lvl) {
            maxUrgency = std::max(maxUrgency, need.urgency);
        }
    }
    return maxUrgency;
}

// Calculate how much this action addresses current needs
float FreeWillSystem::calculateNeedSatisfaction(const Action& action, Entity* targetNeed) {
    float satisfaction = 0.0f;
    float hierarchyMultiplier = 0.0f;

    // Base satisfaction from action
    auto needIt = targetNeed->needs.find(action.needCategory);
    if (needIt != targetNeed->needs.end()) {
        satisfaction += needIt->second.urgency * 0.01f * action.baseSatisfaction;
    }

    // Update the need and get the chosen level
    NeedLevel choosen_need_level = updateHieratchicalNeed(targetNeed, action);
    float urgency = getMaxUrgencyForLevel(targetNeed, choosen_need_level);

    if (action.needCategory == "social") {
        satisfaction += targetNeed->socialDeficit * 0.02f;
    }

    switch (choosen_need_level) {
    case PHYSIOLOGICAL:
        hierarchyMultiplier = 1.0f + (urgency / 100.0f) * 2.0f;
        break;
    case SAFETY:
        hierarchyMultiplier = 1.0f + (urgency / 100.0f) * 2.0f;
        break;
    case BELONGING:
        hierarchyMultiplier = 1.0f + (urgency / 100.0f) * 3.0f;
        break;
    case ESTEEM:
        hierarchyMultiplier = 1.0f + (urgency / 100.0f) * 0.5f;
        break;
    case SELF_ACTUALIZATION:
        hierarchyMultiplier = 1.0f + (urgency / 100.0f) * 0.3f;
        break;
    }

    return satisfaction * hierarchyMultiplier;
}

void FreeWillSystem::applyEmotionalContagion(Entity* entity, const std::vector<Entity*>& neighbors) {
    if (neighbors.empty()) return;
    float avgNeighborHappiness = 0.0f;
    float avgNeighborStress = 0.0f;
    for (Entity* n : neighbors) {
        avgNeighborHappiness += n->entityHapiness;
        avgNeighborStress += n->entityStress;
    }
    avgNeighborHappiness /= neighbors.size();
    avgNeighborStress /= neighbors.size();
    float contagionFactor = 0.02f * (entity->personality.extraversion / 100.0f);
    entity->entityHapiness += (avgNeighborHappiness - entity->entityHapiness) * contagionFactor;
}

// Apply direct stat changes from environmental conditions each tick
void FreeWillSystem::applyEnvironmentalEffects(Entity* entity, const EnvironmentalFactors& env) {
    // Weather: sun boosts happiness, rain lowers it
    float weatherDelta = (env.weatherQuality - 50.0f) / 50.0f; // -1 to +1
    entity->entityHapiness += weatherDelta * 0.8f;

    // Noise: raises stress proportional to noise level above a baseline
    if (env.noiseLevel > 30.0f) {
        entity->entityStress += (env.noiseLevel - 30.0f) / 70.0f * 1.5f;
    }

    // Low safety: raises anxiety (stress + mentalHealth damage)
    if (env.safetyLevel < 50.0f) {
        float dangerFactor = (50.0f - env.safetyLevel) / 50.0f;
        entity->entityStress += dangerFactor * 1.3f;
        entity->entityMentalHealth -= dangerFactor * 0.5f;
    }

    // Crowd discomfort for introverts
    if (env.crowdDensity > 60.0f) {
        float introversion = 1.0f - (entity->personality.extraversion / 100.0f);
        float crowdDiscomfort = ((env.crowdDensity - 60.0f) / 40.0f) * introversion;
        entity->entityStress += crowdDiscomfort * 1.5f;
        entity->entityHapiness -= crowdDiscomfort * 1.0f;
    }

    // Clamp all stats
    entity->entityHapiness = std::max(0.0f, std::min(100.0f, entity->entityHapiness));
    entity->entityStress = std::max(0.0f, std::min(100.0f, entity->entityStress));
    entity->entityMentalHealth = std::max(0.0f, std::min(100.0f, entity->entityMentalHealth));
}

float FreeWillSystem::calculateLifeMemoryBias(Entity* entity, const Action& action) {
    float bias = 1.0f;
    int currentDay = day;
    for (const LifeMemory& mem : entity->lifeMemories) {
        int age = currentDay - mem.simulationDay;
        float decayFactor = std::exp(-age / 300.0f); // fait que la mémoire devient flou
        // Formative memories have stronger and longer effect
        float intensity = mem.emotionalIntensity * decayFactor;
        if (mem.isFormative) intensity *= 2.5f;

        if (mem.eventType == "loss_death" || mem.eventType == "breakup") {
            if (action.needCategory == "social") bias -= intensity * 0.4f;
            if (action.name == "DrinkAlcohol" || action.name == "Smoke") bias += intensity * 0.5f;
        }
        if (mem.eventType == "positive_bond" || mem.eventType == "first_love") {
            if (action.name == "Flirt" || action.name == "Date") bias += intensity * 0.3f;
        }
        if (mem.eventType == "trauma") {
            if (action.name == "SelfHarm" || action.name == "Anxiety") bias += intensity * 0.4f;
            if (action.needCategory == "social") bias -= intensity * 0.2f;
        }
    }
    return std::max(0.1f, std::min(3.0f, bias));
}

// Calculate bias from memory (learn from past experiences)
float FreeWillSystem::calculateMemoryBias(int actionId) {
    float bias = 0.0f;
    float totalWeight = 0.0f;
    int memoryIndex = 0;
    float recentBias = 0.0f;
    float oldBias = 0.0f;
    int recentCount = 0, oldCount = 0;
    for (int i = 0; i < actionHistory.size(); i++) {
        if (actionHistory[i].actionId == actionId) {
            float weight = getMemoryWeight(i);
            if (i < 5) { // Recent memories
                recentBias += actionHistory[i].outcomeSuccess * weight;
                recentCount++;
            } else {
                oldBias += actionHistory[i].outcomeSuccess * weight;
                oldCount++;
            }
        }
    }
    if (recentCount == 0 && oldCount == 0) return 0.5f;
    float finalBias = (recentBias * 1.5f + oldBias) / (recentCount * 1.5f + oldCount);
    return finalBias;
}

// Calculate variety bonus (avoid repetition)
float FreeWillSystem::calculateVarietyBonus(int actionId, const Action& action) {
    int recentCount = 1;            // Fixed: was 1.0
    int checkDepth = std::min(10, (int)actionHistory.size());
    for (int i = 0; i < checkDepth; i++) {
        if (actionHistory[i].actionId == actionId) {
            recentCount++;
        }
    }
    if (action.name == "Socialize" || action.name == "GoodConnection" || action.name == "Date" || action.name == "couple") {
        return std::pow(0.92f, recentCount);
    }
    return std::pow(0.8f, recentCount);
}

// Calculate contextual weight based on environment
float FreeWillSystem::calculateContextualWeight(const Action& action, const ActionContext& context) {
    float modifier = 1.0f;

    // Sleep is much more likely at night
    if (action.name == "Sleep" && context.isNightTime) modifier *= 0.9f;
    if (action.name == "Sleep" && !context.isNightTime) modifier *= 0.3f;

    // Work is less likely on weekends
    if (action.name == "Work on Project" && context.isWeekend) modifier *= 0.7f;
    if (action.name == "Work on Project" && context.isAtWork) modifier *= 2.0f;

    if (action.name == "Socialize") {
        if (context.numPeopleNearby == 0) modifier *= 0.75f;
        if (context.numPeopleNearby > 0 && context.numPeopleNearby <= 2) modifier *= 1.2f;
    }
    if (action.name == "Socialize" && context.numPeopleNearby > 3) modifier *= 2.5f;

    // Extreme negative actions drastically reduced in public
    if (action.name == "Murder" && context.isInPublic) modifier *= 0.05f;
    if (action.name == "SelfHarm" && context.isInPublic) modifier *= 0.1f;
    if (action.name == "Suicide" && context.isInPublic) modifier *= 0.06f;

    // Substance use less likely at work
    if ((action.name == "DrinkAlcohol" || action.name == "Smoke") && context.isAtWork) modifier *= 0.2f;

    // Entertainment more likely on weekends
    if ((action.name == "Gaming" || action.name == "WatchEntertainment") && context.isWeekend) modifier *= 0.7f;

    // Creative activities and learning more likely when not at work
    if ((action.name == "CreativeActivity" || action.name == "LearnSkill") && !context.isAtWork && context.isWeekend) modifier *= 1.3f;

    // Flirting and dating more likely in public social settings
    if ((action.name == "Flirt" || action.name == "Date") && context.isInPublic && context.numPeopleNearby > 0) modifier *= 3.5f;

    // Hygiene actions more likely at home (not in public/at work)
    if (action.name == "Take Shower" && (context.isInPublic || context.isAtWork)) modifier *= 0.01f;

    // Rest more likely at night or when not at work
    if (action.name == "Rest" && context.isNightTime) modifier *= 0.15f;
    if (action.name == "Sleep" && !context.isNightTime && !context.isWeekend) modifier *= 0.15f;
    if (action.name == "Rest" && context.isAtWork) modifier *= 0.2f;

    // Prayer/meditation more likely at night or morning (not during work)
    if (action.name == "Prayer" && !context.isAtWork) modifier *= 0.9f;

    // Gossip more likely when multiple people around
    if (action.name == "Gossip" && context.numPeopleNearby >= 2) modifier *= 1.8f;

    // Professional actions more likely at work
    if (action.name == "LearnSkill" && context.isAtWork) modifier *= 1.4f;

    // Situational proximity bonuses
    if (context.situationHint == "couple_nearby") {
        if (action.name == "Date" || action.name == "Flirt" || action.name == "Reconcile" ||
            action.name == "GoodConnection" || action.name == "couple" || action.name == "Marry") modifier *= 4.0f;
        if (action.name == "breeding" || action.name == "Desire") modifier *= 3.5f;
        if (action.name == "BreakUp") modifier *= 0.3f;
    }
    if (context.situationHint == "enemy_nearby") {
        if (action.name == "AngerConnection" || action.name == "Insult" ||
            action.name == "Discrimination" || action.name == "Duel" || action.name == "Raid") modifier *= 4.0f;
        if (action.name == "Apologize" || action.name == "SetBoundaries" || action.name == "DefendTribe") modifier *= 1.8f;
        if (action.name == "Socialize" || action.name == "GoodConnection") modifier *= 0.4f;
    }
    if (context.situationHint == "family_nearby") {
        if (action.name == "HelpSupport" || action.name == "GoodConnection" ||
            action.name == "Socialize" || action.name == "TellStory" || action.name == "Celebrate") modifier *= 2.0f;
    }
    if (context.situationHint == "desire_nearby") {
        if (action.name == "Flirt" || action.name == "Date" || action.name == "Desire" ||
            action.name == "couple" || action.name == "breeding" || action.name == "Marry") modifier *= 4.5f;
    }

    return modifier;
}

// modifier l'attachement
void FreeWillSystem::tickChildDevelopment(Entity* child, float deltaTime) {
    if (child->lifeStage == ADULT) return;
    if (child->parent1 == nullptr || child->parent2 == nullptr) {
        // Orphan — severe trauma accumulation
        child->dv.childhoodTraumaScore += 2.5f * deltaTime;
        return;
    }

    // Parent presence modelled by social bond strength (no spatial positions)
    float bond1 = child->searchConnSocial(child->parent1);
    float bond2 = child->searchConnSocial(child->parent2);
    float parentBond = child->parent1->searchConnSocial(child->parent2);

    bool parent1Absent = (bond1 < 10.0f);
    bool parent2Absent = (bond2 < 10.0f);
    bool parentsApart  = (parentBond < 5.0f);

    // calcul des traumas
    if (parent1Absent && parent2Absent) {
        child->dv.childhoodTraumaScore += 3.0f * deltaTime;
    } else if (parent1Absent || parent2Absent) {
        child->dv.childhoodTraumaScore += 1.5f * deltaTime;
    } else if (parentsApart) {
        child->dv.childhoodTraumaScore += 0.8f * deltaTime;
        child->ValueSystem.familyOrientation -= 0.1f;
    } else {
        child->dv.childhoodNurturingScore += 2.0f * deltaTime;
        child->ValueSystem.familyOrientation += 0.1f;
    }

    if (child->parent1->entityMentalHealth < 30 || child->parent2->entityMentalHealth < 30) {
        child->dv.childhoodTraumaScore += 1.0f * deltaTime;
        child->ValueSystem.familyOrientation -= 0.2f;
    }
    if (child->parent1->entityHapiness < 25 || child->parent2->entityHapiness < 25) {
        child->dv.childhoodTraumaScore += 0.5f * deltaTime;
        child->ValueSystem.familyOrientation -= 0.35f;
    }

    child->dv.childhoodTraumaScore = std::min(100.0f, child->dv.childhoodTraumaScore);
    child->dv.childhoodNurturingScore = std::min(100.0f, child->dv.childhoodNurturingScore);
    child->dv.developmentTicksRemaining--;
    if (child->dv.developmentTicksRemaining <= 0) {
        finalizeChildhood(child);
    }
}

void FreeWillSystem::finalizeChildhood(Entity* child) {
    float trauma = child->dv.childhoodTraumaScore;
    float nurture = child->dv.childhoodNurturingScore;

    child->personality.neuroticism += trauma * 0.35f;
    child->personality.agreeableness -= trauma * 0.20f;
    child->personality.extraversion -= trauma * 0.15f;
    child->personality.openness += nurture * 0.20f;
    child->personality.extraversion += nurture * 0.15f;
    child->personality.agreeableness += nurture * 0.18f;

    auto clamp = [](float v) {
        return std::max(0.0f, std::min(100.0f, v));
    };
    child->personality.neuroticism = clamp(child->personality.neuroticism);
    child->personality.agreeableness = clamp(child->personality.agreeableness);
    child->personality.extraversion = clamp(child->personality.extraversion);
    child->personality.openness = clamp(child->personality.openness);

    if (trauma < 20 && nurture > 60) {
        child->dv.attachmentStyle = SECURE;
        child->dv.hadSecureAttachment = true;
    } else if (trauma > 60) {
        child->dv.attachmentStyle = (nurture < 30) ? DISORGANIZED : ANXIOUS;
    } else if (trauma > 30 && nurture < 40) {
        child->dv.attachmentStyle = AVOIDANT;
    } else {
        child->dv.attachmentStyle = ANXIOUS;
    }

    child->lifeStage = ADULT;

    ASHB_TRACE_STREAM << "=== CHILDHOOD COMPLETE: " << child->getName() << " ===\n";
    ASHB_TRACE_STREAM << "  Trauma: " << trauma << " | Nurture: " << nurture << "\n";
    ASHB_TRACE_STREAM << "  Attachment: " << child->dv.attachmentStyle << "\n";
    ASHB_TRACE_STREAM << "  Final Neuroticism: " << child->personality.neuroticism << "\n";
}

// Grief reduces social drive and raises substance use / coping action likelihood
float FreeWillSystem::calculateGriefModifier(Entity* entity, const Action& action) {
    float grief = entity->getGriefIntensity();
    if (grief <= 0.0f) return 1.0f;

    float modifier = 1.0f;
    // Grieving significantly reduces desire to socialise
    if (action.needCategory == "social") {
        modifier *= 1.0f - (grief * 0.55f); // up to -55% at full grief
    }
    // Grieving raises substance use risk (coping mechanism)
    if (action.name == "DrinkAlcohol" || action.name == "Smoke") {
        modifier *= 1.0f + (grief * 1.8f); // up to +180% at full grief
    }
    // Grieving raises self-harm / anxiety risk
    if (action.name == "SelfHarm" || action.name == "Anxiety") {
        modifier *= 1.0f + (grief * 0.9f);
    }
    // Grieving raises therapy / prayer seeking
    if (action.name == "SeekTherapy" || action.name == "Prayer") {
        modifier *= 1.0f + (grief * 0.6f);
    }
    // Grieving slightly raises rest/sleep (withdrawal)
    if (action.name == "Sleep" || action.name == "Rest") {
        modifier *= 1.0f + (grief * 0.3f);
    }
    return modifier;
}

// Basing in only neighboorhood pheromones try to calculate how much impact does it have on anyone
//
float FreeWillSystem::calculateEnvironningPheromones(const std::vector<Entity*>& neighbors, const Action* action) {
    float modifier = 2.5f;
    if(season == "spring"){
        modifier = 1.2f;
    }else if(season == "summer"){
        modifier = 1.1f;
    }else if(season == "winter"){
        modifier = 0.8f;
    }else if(season == "autumn"){
        modifier = 0.9f;
    }

    for(Entity* neighboor : neighbors){
        std::string an = action->name;
        if (an == "Socialize" || an == "GoodConnection" || an == "AngerConnection" ||
        an == "Gossip" || an == "HelpSupport" || an == "Apologize" || an == "Insult" ||  an == "Discrimination" || an == "IgnoreAvoid" ){
            if(neighboor->pheromone.type == "social"){
                modifier += ((neighboor->pheromone.releasing_level) / 100) * 2.5;
            }
        }else if(an == "Breeding"){
            if(neighboor->pheromone.type == "breeding"){
                modifier += ((neighboor->pheromone.releasing_level) / 100) * 2.5;
            }

        }else if  (an == "Desire" || an == "Flirt" || an == "Date" || an == "Reconcile" ||
                                      an == "couple" || an == "breeding" || an == "Jealousy" || an == "SetBoundaries"){
            if(neighboor->pheromone.type == "procreation_simulation" || neighboor->pheromone.type == "sex"){
                modifier += ((neighboor->pheromone.releasing_level) / 100) * 2.5;
            }
        }else{
            modifier = 4.0f;
        }
    }
    return modifier;
}

// Environmental modifier — crowd avoidance for introverts, noise stress, safety flee
float FreeWillSystem::calculateEnvironmentalModifier(Entity* entity, const Action& action, const EnvironmentalFactors& env) {
    float modifier = 1.0f;
    const Personality& p = entity->personality;
    float crowdPenalty = (env.crowdDensity / 100.0f) * (1.0f - p.extraversion / 100.0f);

    if (action.needCategory == "social") {
        modifier *= 1.0f - (crowdPenalty * 0.6f);
    }
    if ((action.name == "Socialize" || action.name == "GoodConnection") && env.crowdDensity > 50.0f) {
        modifier *= 1.0f + ((p.extraversion / 100.0f) * 0.4f);
    }

    if (env.noiseLevel > 60.0f) {
        float noiseFactor = (env.noiseLevel - 60.0f) / 40.0f;
        if (action.name == "Anxiety") modifier *= 1.0f + noiseFactor * 0.8f;
        if (action.name == "IgnoreAvoid") modifier *= 1.0f + noiseFactor * 0.5f;
        if (action.name == "Socialize") modifier *= 1.0f - noiseFactor * 0.3f;
    }

    if (env.safetyLevel < 40.0f) {
        float dangerFactor = 1.0f - (env.safetyLevel / 40.0f);
        if (action.name == "Anxiety") modifier *= 1.0f + dangerFactor * 1.2f;
        if (action.name == "SelfHarm" || action.name == "Suicide") modifier *= 1.0f + dangerFactor * 0.5f;
        // Suppresses leisure in dangerous areas
        if (action.name == "Gaming" || action.name == "WatchEntertainment" || action.name == "CreativeActivity") modifier *= 1.0f - dangerFactor * 0.5f;
    }

    if (env.weatherQuality > 65.0f) {
        float sunBonus = (env.weatherQuality - 65.0f) / 35.0f;
        if (action.name == "Exercise" || action.name == "Socialize" || action.name == "GoodConnection") modifier *= 1.0f + sunBonus * 0.4f;
    }
    if (env.weatherQuality < 35.0f) {
        float rainPenalty = (35.0f - env.weatherQuality) / 35.0f;
        if (action.name == "WatchEntertainment" || action.name == "Gaming" || action.name == "DrinkAlcohol") modifier *= 1.0f + rainPenalty * 0.4f;
        if (action.name == "Anxiety" || action.name == "Procrastinate") modifier *= 1.0f + rainPenalty * 0.3f;
    }

    return modifier;
}

float FreeWillSystem::applyValueSatisfaction(Entity* entity, const Action& action) {
    ValueSystem v = entity->ValueSystem;
    float modifier = 1.0f;

    if (action.name == "Work on Project" || action.name == "LearnSkill") {
        modifier += (v.achievementDrive / 100.0f) * 0.8f;
    }
    if (action.needCategory == "social") {
        modifier += (v.collectivism / 100.0f) * 0.8f;
    }
    if (action.needCategory == "hedonism") {
        modifier += (v.hedonism / 100.0f) * 0.8f;
    }
    if (action.needCategory == "spiritualNeed") {
        modifier += (v.spiritualNeed / 100.0f) * 0.8f;
    }
    if (action.name == "couple" || action.name == "Breeding") {
        modifier += (v.familyOrientation / 100.0f) * 0.8f;
    }

    if (action.name == "Gaming" || action.name == "WatchEntertainment" ||
        action.name == "DrinkAlcohol" || action.name == "Smoke") {
        modifier *= std::max(0.4f, v.hedonism / 100.0f * 1.5f);
    }
    if (action.name == "Work on Project" || action.name == "LearnSkill") {
        modifier = std::max(0.3f, (v.achievementDrive / 100.0f) * 1.5f);
    }
    if (action.name == "Socialize" || action.name == "GoodConnection" ||
        action.name == "HelpSupport" || action.name == "Apologize" || action.name == "Reconcile") {
        modifier = std::max(0.5f, (v.collectivism / 100.0f) * 1.5f);
    }
    if (action.name == "Prayer") {
        modifier += (v.spiritualNeed / 100.0f) * 1.2f;
    }
    return std::max(0.1f, modifier);
}

float FreeWillSystem::calculatePersonalityModifier(Entity* entity, const Action& action) {
    float modifier = 1.0f;
    const Personality& p = entity->personality;

    if (action.needCategory == "social") {
        modifier *= 0.8f + (p.extraversion / 100.0f) * 1.4f; // 0.8x to 2.2x
    }
    if (action.name == "Murder" || action.name == "Discrimination" || action.name == "Insult" || action.name == "Betray" || action.name == "AngerConnection" || action.name == "Manipulate") {
        modifier *= 1.5f - (p.agreeableness / 100.0f); // 1.5x to 0.5x
    }
    if (action.name == "Apologize" || action.name == "HelpSupport" || action.name == "Reconcile" || action.name == "GoodConnection") {
        modifier *= 0.5f + (p.agreeableness / 100.0f); // 0.5x to 1.5x
    }
    if (action.needCategory == "achievement") {
        modifier *= 0.5f + (p.conscientiousness / 100.0f); // 0.5x to 1.5x
    }
    if (action.name == "Procrastinate" || action.name == "QuitGiveUp") {
        modifier *= 1.5f - (p.conscientiousness / 100.0f); // 1.5x to 0.5x
    }
    if (action.name == "Anxiety" || action.name == "SelfHarm" || action.name == "Suicide") {
        modifier *= 0.3f + (p.neuroticism / 100.0f); // 0.5x to 1.5x
    }
    if (action.name == "DrinkAlcohol" || action.name == "Smoke") {
        modifier *= 0.7f + (p.neuroticism / 150.0f); // 0.7x to ~1.37x
    }
    if (action.name == "AngerConnection" || action.name == "Insult" || action.name == "Discrimination" ) {
        modifier *= 2.0f - (p.agreeableness / 100.0f); // 2.0x to 1.0x instead of 1.5x to 0.5x
    }
   //if (action.name == "Prayer" || action.name == "SeekTherapy" || action.name == "Exercise") {
   //    modifier *= 1.0f + ((100.0f - p.neuroticism) / 200.0f);
   //    entity->ValueSystem.spiritualNeed += 0.1f;
   //}
    // Openness affects variety seeking and creative actions
    if (action.name == "CreativeActivity" || action.name == "LearnSkill" || action.name == "Read") {
        modifier *= 0.5f + (p.openness / 100.0f); // 0.5x to 1.5x
    }
    // Low openness increases routine actions
    if (action.name == "WatchEntertainment" || action.name == "Gaming") {
        modifier *= 1.3f - (p.openness / 200.0f); // 1.3x to 0.8x
    }
    if (action.name == "Jealousy") {
        modifier *= 1.3f - (p.extraversion / 200.0f); // 1.3x to 0.8x
    }
    // High extraversion increases flirting and dating
    if (action.name == "Flirt" || action.name == "Date" || action.name == "couple"
        || action.name == "Marry" || action.name == "Celebrate") {
        modifier *= 0.6f + (p.extraversion / 125.0f); // 0.6x to 1.4x
    }
    // Era-aware actions tied to personality
    if (action.name == "Hunt" || action.name == "Raid" || action.name == "Duel") {
        modifier *= 1.3f - (p.agreeableness / 200.0f); // warriors are less agreeable
    }
    if (action.name == "Farm" || action.name == "Gather") {
        modifier *= 0.5f + (p.conscientiousness / 100.0f);
    }
    if (action.name == "Explore" || action.name == "TellStory") {
        modifier *= 0.5f + (p.openness / 100.0f);
    }
    if (action.name == "Build" || action.name == "DefendTribe") {
        modifier *= 0.5f + (p.conscientiousness / 100.0f);
    }
    if (action.name == "Mourn") {
        modifier *= 0.3f + (p.neuroticism / 100.0f);
    }

    AttachmentStyle att = entity->dv.attachmentStyle;
    if (action.name == "couple" || action.name == "Date" || action.name == "Flirt") {
        if (att == ANXIOUS) modifier *= 1.4f;
        if (att == AVOIDANT) modifier *= 0.5f;
        if (att == DISORGANIZED) modifier *= 0.8f;
    }
    if (action.name == "BreakUp") {
        if (att == ANXIOUS) modifier *= 0.3f;
        if (att == AVOIDANT) modifier *= 1.6f;
    }
    if (action.name == "Socialize") {
        if (att == AVOIDANT) modifier *= 0.7f;
        if (att == SECURE) modifier *= 1.2f;
    }

    return modifier;
}

FreeWillSystem::FreeWillSystem() : currentTime(0), rng(static_cast<std::mt19937::result_type>(nextDeterministicSeed(0xF2EE'C0DEull))) {
    initializeNeeds();
    initializeActions();
}

void FreeWillSystem::initializeNeeds() {
    needs["social"] = Need("social", 0.15f);
    needs["social"].satisfyingCategories = { "social", "entertainment" };
    needs["health"] = Need("health", 0.08f);
    needs["health"].satisfyingCategories = { "health", "food", "sleep" };
    needs["hygiene"] = Need("hygiene", 0.12f);
    needs["hygiene"].satisfyingCategories = { "hygiene" };
    needs["safety"] = Need("safety", 0.05f);
    needs["safety"].satisfyingCategories = { "safety", "health" };
    needs["happiness"] = Need("happiness", 0.1f);
    needs["happiness"].satisfyingCategories = { "entertainment", "social", "achievement" };
}

// les satisfactions doivent venir de ValueSystem
// implementation of a list of action
void FreeWillSystem::initializeActions() {
    // The catalog is shared by all agents; only the first constructed
    // FreeWillSystem populates it.
    if (!availableActions.empty()) return;
    // POINTED actions (require a target entity)
    //  Socialize: fires whenever lonely OR bored — very accessible, the baseline human act
    Action socialize("Socialize", 1, "social");
    socialize.requirements = { {"loneliness", 20.0f, 0.7f}, {"boredom", 20.0f, 0.4f} };
    socialize.statChanges = { {"loneliness", -8.0f}, {"happiness", 7.0f}, {"stress", -5.0f}, {"boredom", -10.0f} };
    socialize.baseSatisfaction = 25.0f;
    availableActions.push_back(socialize);
    desireLinkedAction.push_back(socialize);

    Action desire("Desire", 2, "social");
    desire.requirements = { {"loneliness", 20.0f, 0.7f}, {"happiness", 20.0f, 0.3f} };
    desire.statChanges = { {"loneliness", -12.0f}, {"happiness", 13.0f}, {"stress", -6.0f} };
    desire.baseSatisfaction = 55.0f;
    availableActions.push_back(desire);
    desireLinkedAction.push_back(desire);

    Action goodconn("GoodConnection", 3, "social");
    goodconn.requirements = { {"loneliness", 25.0f, 0.8f}, {"stress", 65.0f, 0.3f} };
    goodconn.statChanges = { {"happiness", 13.0f}, {"loneliness", -14.0f}, {"mentalHealth", 9.0f}, {"stress", -4.0f} };
    goodconn.baseSatisfaction = 35.0f;
    availableActions.push_back(goodconn);


    Action angconn("AngerConnection", 4, "social");
    angconn.requirements = { {"anger", 25.0f, 0.8f}, {"stress", 30.0f, 0.3f} };
    angconn.statChanges = { {"anger", 13.0f}, {"stress", 17.0f}, {"happiness", -12.0f}, {"mentalHealth", -7.0f} };
    angconn.baseSatisfaction = 15.0f;
    availableActions.push_back(angconn);
    hatredLinkedAction.push_back(angconn);

    // Extreme actions requiring high negative stats
    Action murder("Murder", 5, "safety");
    murder.requirements = { {"anger", 80.0f, 1.0f}, {"mentalHealth", 20.0f, 0.9f}, {"stress", 70.0f, 0.8f} };
    murder.statChanges = { {"anger", -33.0f}, {"mentalHealth", -23.0f}, {"stress", 17.0f}, {"happiness", 3.0f} };
    murder.baseSatisfaction = 10.0f;
    availableActions.push_back(murder);
    hatredLinkedAction.push_back(murder);

    Action discrimination("Discrimination", 6, "social");
    discrimination.requirements = { {"anger", 45.0f, 0.8f}, {"mentalHealth", 15.0f, 0.6f}, {"stress", 40.0f, 0.5f} };
    discrimination.statChanges = { {"anger", -14.0f}, {"mentalHealth", -12.0f}, {"stress", 5.0f}, {"happiness", -1.0f} };
    discrimination.baseSatisfaction = 3.0f;
    availableActions.push_back(discrimination);
    hatredLinkedAction.push_back(discrimination);

    // Self-harm actions
    Action suicide("Suicide", 7, "safety");
    suicide.requirements = { {"mentalHealth", 8.0f, 1.0f}, {"stress", 95.0f, 1.0f}, {"happiness", 3.0f, 0.9f} };
    suicide.statChanges = { {"health", -100.0f}, {"mentalHealth", -100.0f} };
    suicide.baseSatisfaction = 0.0f;
    availableActions.push_back(suicide);


    //Action anxiety("Anxiety", 8, "health");
    //anxiety.requirements = { {"stress", 85.0f, 0.9f}, {"mentalHealth", 40.0f, 0.7f} };
    //anxiety.statChanges = { {"stress", -8.0f}, {"mentalHealth", -4.0f}, {"health", -2.0f}, {"happiness", -5.0f} };
    //anxiety.baseSatisfaction = 8.0f;
    //availableActions.push_back(anxiety);

    Action remoteSocial("DigitalSocial", 50, "social");
    remoteSocial.requirements = { {"loneliness", 30.0f, 0.8f} };
    remoteSocial.statChanges = { {"loneliness", -10.0f}, {"happiness", 6.0f}, {"stress", -5.0f}, {"mentalHealth", 2.0f} };
    remoteSocial.baseSatisfaction = 15.0f;
    availableActions.push_back(remoteSocial);

    Action Prayer("Prayer", 14, "health");
    Prayer.requirements = { {"stress", 80.0f, 0.8f}, {"mentalHealth", 55.0f, 0.6f} };
    Prayer.statChanges = { {"stress", -15.0f}, {"mentalHealth", 15.0f}, {"loneliness", 5.0f}, {"happiness", 8.0f} };
    Prayer.baseSatisfaction = 9.0f;
    availableActions.push_back(Prayer);

    // Read/Study (achievement)
    Action read("Read", 16, "achievement");
    read.requirements = { {"boredom", 30.0f, 0.7f}, {"mentalHealth", 40.0f, 0.5f}, {"happiness", 25.0f, 0.4f} };
    read.statChanges = { {"boredom", -15.0f}, {"happiness", 10.0f}, {"mentalHealth", 8.0f}, {"loneliness", 3.0f}, {"stress", -5.0f} };
    read.baseSatisfaction = 15.0f;
    availableActions.push_back(read);

    // Procrastinate (entertainment)
    Action procrastinate("Procrastinate", 17, "entertainment");
    procrastinate.requirements = { {"stress", 40.0f, 0.7f}, {"boredom", 40.0f, 0.7f} };
    procrastinate.statChanges = { {"boredom", -10.0f}, {"happiness", 5.0f}, {"stress", 16.0f}, {"mentalHealth", -5.0f} };
    procrastinate.baseSatisfaction = 5.0f;
    availableActions.push_back(procrastinate);

    // ==================== SOCIAL ACTIONS ====================
    // Gossip (social)
    Action gossip("Gossip", 18, "social");
    gossip.requirements = { {"loneliness", 40.0f, 0.7f}, {"boredom", 50.0f, 0.6f} };
    gossip.statChanges = { {"loneliness", -8.0f}, {"boredom", -12.0f}, {"happiness", 5.0f} };
    gossip.baseSatisfaction = 12.0f;
    availableActions.push_back(gossip);

    // Apologize (social)
    Action apologize("Apologize", 19, "social");
    apologize.requirements = { {"anger", 30.0f, 0.8f}, {"mentalHealth", 40.0f, 0.6f} };
    apologize.statChanges = { {"happiness", 8.0f}, {"stress", -10.0f}, {"mentalHealth", 5.0f} };
    apologize.baseSatisfaction = 18.0f;
    availableActions.push_back(apologize);

    // Help/Support (social)
    Action helpSupport("HelpSupport", 20, "social");
    helpSupport.requirements = { {"happiness", 50.0f, 0.7f}, {"stress", 60.0f, 0.6f}, {"mentalHealth", 50.0f, 0.5f} };
    helpSupport.statChanges = { {"happiness", 10.0f}, {"mentalHealth", 8.0f}, {"stress", 5.0f}, {"loneliness", -5.0f} };
    helpSupport.baseSatisfaction = 20.0f;
    availableActions.push_back(helpSupport);

    // Ignore/Avoid (social)
    Action ignoreAvoid("IgnoreAvoid", 21, "social");
    ignoreAvoid.requirements = { {"stress", 60.0f, 0.7f}, {"anger", 40.0f, 0.6f} };
    ignoreAvoid.statChanges = { {"stress", -5.0f}, {"loneliness", 8.0f} };
    ignoreAvoid.baseSatisfaction = 8.0f;
    availableActions.push_back(ignoreAvoid);

    // ==================== SELF-CARE ACTIONS ====================
    // Eat Meal (health) — proactive: fires when health < 80, not just in crisis
    Action eatMeal("EatMeal", 22, "health");
    // Hunger is now the dominant driver — a hungry entity eats from its food store.
    eatMeal.requirements = { {"hunger", 30.0f, 0.9f}, {"health", 80.0f, 0.4f} };
    eatMeal.statChanges = { {"health", 9.0f}, {"happiness", 4.0f}, {"hygiene", -1.0f}, {"boredom", 2.0f} };
    eatMeal.baseSatisfaction = 12.0f;
    availableActions.push_back(eatMeal);

    // Sleep — proactive: fires when stress > 50 or health < 78
    Action sleep("Sleep", 23, "health");
    sleep.requirements = { {"fatigue", 45.0f, 0.7f}, {"stress", 50.0f, 0.5f}, {"health", 78.0f, 0.4f} };
    sleep.statChanges = { {"stress", -12.0f}, {"health", 8.0f}, {"mentalHealth", 7.0f}, {"hygiene", -4.0f}, {"boredom", 4.0f} };
    sleep.baseSatisfaction = 13.0f;
    availableActions.push_back(sleep);

    // Self-Harm (health) - concerning but realistic
    Action selfHarm("SelfHarm", 15, "health");
    selfHarm.requirements = { {"mentalHealth", 25.0f, 1.0f}, {"stress", 80.0f, 0.9f}, {"loneliness", 70.0f, 0.8f} };
    selfHarm.statChanges = { {"mentalHealth", -15.0f}, {"stress", -20.0f}, {"health", -10.0f} };
    selfHarm.baseSatisfaction = 5.0f;
    availableActions.push_back(selfHarm);

    Action watchEntertainment("WatchEntertainment", 24, "entertainment");
    watchEntertainment.requirements = { {"boredom", 70.0f, 0.8f}, {"stress", 70.0f, 0.5f} };
    watchEntertainment.statChanges = { {"boredom", -20.0f}, {"happiness", 8.0f}, {"stress", -8.0f} };
    watchEntertainment.baseSatisfaction = 11.0f;
    availableActions.push_back(watchEntertainment);

    // Creative Activity (achievement)
    Action creativeActivity("CreativeActivity", 25, "achievement");
    creativeActivity.requirements = { {"boredom", 40.0f, 0.7f}, {"stress", 70.0f, 0.5f} };
    creativeActivity.statChanges = { {"happiness", 15.0f}, {"boredom", -25.0f}, {"mentalHealth", 10.0f}, {"stress", 5.0f} };
    creativeActivity.baseSatisfaction = 22.0f;
    availableActions.push_back(creativeActivity);

    // Gaming/Play (entertainment)
    Action gaming("Gaming", 26, "entertainment");
    gaming.requirements = { {"boredom", 70.0f, 0.8f}, {"stress", 80.0f, 0.5f} };
    gaming.statChanges = { {"boredom", -20.0f}, {"happiness", 12.0f}, {"loneliness", 8.0f}, {"stress", -10.0f} };
    gaming.baseSatisfaction = 8.0f;
    availableActions.push_back(gaming);

    // ==================== NEGATIVE SOCIAL ACTIONS ====================
    // Insult/Verbal Attack (social)
    Action insult("Insult", 27, "social");
    insult.requirements = { {"anger", 35.0f, 0.8f}, {"stress", 40.0f, 0.5f} };
    insult.statChanges = { {"anger", -10.0f}, {"stress", 10.0f}, {"happiness", -5.0f} };
    insult.baseSatisfaction = 12.0f;
    availableActions.push_back(insult);

    // Manipulate (social)
    Action manipulate("Manipulate", 28, "social");
    manipulate.requirements = { {"mentalHealth", 50.0f, 0.8f} };
    manipulate.statChanges = { {"happiness", 5.0f}, {"mentalHealth", -8.0f}, {"stress", 5.0f} };
    manipulate.baseSatisfaction = 14.0f;
    availableActions.push_back(manipulate);

    // Jealousy/Envy Display (social)
    Action jealousy("Jealousy", 29, "social");
    jealousy.requirements = { {"happiness", 40.0f, 0.8f} };
    jealousy.statChanges = { {"anger", 15.0f}, {"happiness", -10.0f}, {"mentalHealth", -5.0f} };
    jealousy.baseSatisfaction = 14.0f;
    availableActions.push_back(jealousy);
    desireLinkedAction.push_back(jealousy);

    // Betray (social)
    Action betray("Betray", 30, "social");
    betray.requirements = { {"anger", 60.0f, 0.9f} };
    betray.statChanges = { {"mentalHealth", -12.0f}, {"stress", 20.0f} };
    betray.baseSatisfaction = 9.0f;
    availableActions.push_back(betray);

    // ==================== PROFESSIONAL/ACHIEVEMENT ACTIONS ====================
    // Learn New Skill (achievement)
    Action learnSkill("LearnSkill", 31, "achievement");
    learnSkill.requirements = { {"boredom", 40.0f, 0.6f}, {"mentalHealth", 50.0f, 0.5f}, {"stress", 70.0f, 0.5f} };
    learnSkill.statChanges = { {"happiness", 18.0f}, {"boredom", -25.0f}, {"mentalHealth", 10.0f}, {"stress", 8.0f} };
    learnSkill.baseSatisfaction = 25.0f;
    availableActions.push_back(learnSkill);

    // Quit/Give Up (achievement)
    Action quitGiveUp("QuitGiveUp", 32, "achievement");
    quitGiveUp.requirements = { {"stress", 80.0f, 0.9f}, {"mentalHealth", 40.0f, 0.8f}, {"anger", 50.0f, 0.7f} };
    quitGiveUp.statChanges = { {"stress", -15.0f}, {"happiness", -12.0f}, {"mentalHealth", -8.0f}, {"boredom", 20.0f} };
    quitGiveUp.baseSatisfaction = 8.0f;
    availableActions.push_back(quitGiveUp);

    creativeActivity.requirements = { {"boredom", 40.0f, 0.5f}, {"happiness", 35.0f, 0.6f}, {"stress", 60.0f, 0.6f}, {"mentalHealth", 40.0f, 0.4f} };
    creativeActivity.statChanges = { {"happiness", 18.0f}, {"boredom", -28.0f}, {"mentalHealth", 12.0f}, {"stress", -8.0f}, {"loneliness", -4.0f} };
    creativeActivity.baseSatisfaction = 26.0f;
    availableActions.push_back(creativeActivity);

    // ==================== SUBSTANCE/COPING ACTIONS ====================
    // Drink Alcohol (entertainment/health)
    Action drinkAlcohol("DrinkAlcohol", 33, "entertainment");
    drinkAlcohol.requirements = { {"stress", 75.0f, 0.8f} };
    drinkAlcohol.statChanges = { {"stress", -15.0f}, {"happiness", 12.0f}, {"health", -3.0f}, {"mentalHealth", -6.0f}, {"hygiene", -4.0f} };
    drinkAlcohol.baseSatisfaction = 13.0f;
    availableActions.push_back(drinkAlcohol);

    // Smoke/Vape (health)
    Action smoke("Smoke", 34, "health");
    smoke.requirements = { {"stress", 70.0f, 0.8f} };
    smoke.statChanges = { {"stress", -12.0f}, {"health", -3.0f}, {"hygiene", -2.0f} };
    smoke.baseSatisfaction = 10.0f;
    availableActions.push_back(smoke);

    // Social media Scrolling dopamine
    Action scrolling("Scrolling", 13, "entertainment");
    scrolling.requirements = { {"boredom", 55.0f, 0.8f}, {"stress", 45.0f, 0.5f} };
    scrolling.statChanges = { {"stress", -4.0f}, {"happiness", -11.0f}, {"boredom", 8.0f}, {"loneliness", 19.0f}, {"mentalHealth", -9.0f} };
    scrolling.baseSatisfaction = 4.0f;
    availableActions.push_back(scrolling);

    // ==================== INTIMATE/ROMANTIC ACTIONS ====================
    // Flirt: fires when lonely — accessible early-game
    Action flirt("Flirt", 35, "social");
    flirt.requirements = { {"loneliness", 20.0f, 0.6f} };
    flirt.statChanges = { {"loneliness", -5.0f}, {"happiness", 8.0f} };
    flirt.baseSatisfaction = 38.0f;
    availableActions.push_back(flirt);
    desireLinkedAction.push_back(flirt);

    // Date: deeper romantic engagement
    Action date("Date", 36, "social");
    date.requirements = { {"loneliness", 25.0f, 0.5f} };
    date.statChanges = { {"loneliness", -15.0f}, {"happiness", 15.0f}, {"stress", -8.0f} };
    date.baseSatisfaction = 50.0f;
    availableActions.push_back(date);
    desireLinkedAction.push_back(date);

    // Break Up (social)
    Action breakUp("BreakUp", 37, "social");
    breakUp.requirements = { {"anger", 40.0f, 0.8f} };
    breakUp.statChanges = { {"happiness", -20.0f}, {"stress", 15.0f}, {"loneliness", 25.0f}, {"mentalHealth", -10.0f} };
    breakUp.baseSatisfaction = 5.0f;
    availableActions.push_back(breakUp);
    desireLinkedAction.push_back(breakUp);

    // Reconcile (social)
    Action reconcile("Reconcile", 38, "social");
    reconcile.requirements = { {"anger", 30.0f, 0.8f} };
    reconcile.statChanges = { {"happiness", 20.0f}, {"stress", -10.0f} };
    reconcile.baseSatisfaction = 22.0f;
    availableActions.push_back(reconcile);
    desireLinkedAction.push_back(reconcile);

    // ==================== DEFENSIVE/BOUNDARY ACTIONS ====================
    // Set Boundaries (social)
    Action setBoundaries("SetBoundaries", 39, "social");
    setBoundaries.requirements = { {"stress", 80.0f, 0.7f}, {"anger", 30.0f, 0.6f}, {"mentalHealth", 50.0f, 0.5f} };
    setBoundaries.statChanges = { {"stress", -12.0f}, {"mentalHealth", 8.0f}, {"anger", -5.0f} };
    setBoundaries.baseSatisfaction = 15.0f;
    availableActions.push_back(setBoundaries);

    // Seek Therapy/Help (health)
    Action seekTherapy("SeekTherapy", 40, "health");
    seekTherapy.requirements = { {"mentalHealth", 30.0f, 0.9f}, {"stress", 80.0f, 0.8f} };
    seekTherapy.statChanges = { {"mentalHealth", 20.0f}, {"stress", -15.0f}, {"happiness", 9.0f}, {"anger", -10.0f} };
    seekTherapy.baseSatisfaction = 25.0f;
    availableActions.push_back(seekTherapy);

    Action couple("couple", 41, "social");
    couple.requirements = { {"loneliness", 15.0f, 0.4f} };
    couple.statChanges = { {"happiness", 45.0f}, {"loneliness", -25.0f}, {"stress", 20.0f} };
    couple.baseSatisfaction = 75.0f;
    availableActions.push_back(couple);
    desireLinkedAction.push_back(couple);


    Action breeding("breeding", 42, "social");
    breeding.requirements = { {"loneliness", 15.0f, 0.3f} };
    breeding.statChanges = { {"happiness", 45.0f}, {"loneliness", -25.0f}, {"stress", 20.0f} };
    breeding.baseSatisfaction = 75.0f;
    availableActions.push_back(breeding);
    desireLinkedAction.push_back(breeding);

    // Health actions
    //Action exercise("Exercise", 10, "health");
    //exercise.requirements = { {"health", 30.0f, 0.7f}, {"stress", 70.0f, 0.7f} };
    //exercise.statChanges = { {"health", 10.0f}, {"stress", -10.0f}, {"happiness", 10.0f}, {"boredom", -10.0f}, {"hygiene", -3.0f} };
    //exercise.baseSatisfaction = 10.0f;
    //availableActions.push_back(exercise);

    // Hygiene actions
    Action shower("Take Shower", 11, "hygiene");
    shower.requirements = { {"hygiene", 10.0f, 0.9f} };
    shower.statChanges = { {"hygiene", 15.0f}, {"happiness", 3.0f}, {"stress", -3.0f} };
    shower.baseSatisfaction = 4.0f;
    availableActions.push_back(shower);

    // Rest action — proactive: fires when stress > 58 or health < 68
    Action rest("Rest", 12, "health");
    rest.requirements = { {"stress", 58.0f, 0.6f}, {"health", 68.0f, 0.4f} };
    rest.statChanges = { {"stress", -20.0f}, {"health", 15.0f}, {"mentalHealth", 10.0f}, {"boredom", 10.0f} };
    rest.baseSatisfaction = 20.0f;
    availableActions.push_back(rest);

    // Work/Achievement action
    Action work("Work on Project", 13, "achievement");
    work.requirements = { {"stress", 40.0f, 0.5f}, {"health", 50.0f, 0.4f} };
    work.statChanges = { {"stress", 17.0f}, {"happiness", 15.0f}, {"boredom", -20.0f}, {"loneliness", 10.0f} };
    work.baseSatisfaction = 25.0f;
    availableActions.push_back(work);

    Action basicManualwork("Basic Manual Work", 2212, "achievement");
    basicManualwork.requirements = {  {"health", 60.0f, 0.4f} };
    basicManualwork.statChanges = { {"stress", 13.0f}, {"happiness", -7.0f}, {"boredom", -7.0f}, {"loneliness", 12.0f} };
    basicManualwork.baseSatisfaction = 19.0f;
    availableActions.push_back(basicManualwork);

    // ════════════════════════════════════════════════════════════════════════
    // CIVILISATION-SCALE ACTIONS
    // These actions, chosen rarely by the right personality types, generate
    // the emergent phenomena of leadership, religion, and innovation.
    // ════════════════════════════════════════════════════════════════════════

    // LeadGroup — high-extraversion entities gather followers and assert authority
    // SELF-DIRECTED: the entity rallies nearby people, raising their own esteem
    Action leadGroup("LeadGroup", 200, "leadership");
    leadGroup.requirements = { {"happiness", 55.0f, 0.6f}, {"loneliness", 20.0f, 0.3f} };
    leadGroup.statChanges  = { {"loneliness", -10.0f}, {"stress", 4.0f},
                                {"boredom", -14.0f},   {"happiness", 6.0f} };
    leadGroup.baseSatisfaction = 12.0f;
    availableActions.push_back(leadGroup);

    // Preach — spiritually-inclined entities share their belief with a target
    // POINTED: spreads religion / raises follower connection
    Action preach("Preach", 201, "spiritual");
    preach.requirements = { {"mentalHealth", 55.0f, 0.6f}, {"boredom", 25.0f, 0.4f} };
    preach.statChanges  = { {"loneliness", -12.0f}, {"stress", -4.0f},
                             {"boredom", -10.0f},   {"happiness", 8.0f} };
    preach.baseSatisfaction = 10.0f;
    availableActions.push_back(preach);

    // PerformRitual — communal ceremony; reduces stress, builds group identity
    // SELF-DIRECTED but boosts nearby entity cohesion through contagion
    Action ritual("PerformRitual", 202, "spiritual");
    ritual.requirements = { {"stress", 35.0f, 0.5f}, {"mentalHealth", 40.0f, 0.4f} };
    ritual.statChanges  = { {"stress", -14.0f}, {"loneliness", -10.0f},
                             {"mentalHealth", 9.0f}, {"happiness", 6.0f} };
    ritual.baseSatisfaction = 10.0f;
    availableActions.push_back(ritual);

    // Invent — high-openness/curiosity entities make discoveries
    // SELF-DIRECTED: triggers the innovation system in CivilizationEngine
    Action invent("Invent", 203, "achievement");
    invent.requirements = { {"boredom", 45.0f, 0.7f}, {"mentalHealth", 50.0f, 0.4f} };
    invent.statChanges  = { {"boredom", -22.0f}, {"happiness", 14.0f},
                             {"stress", 3.0f},   {"loneliness", 4.0f} };
    invent.baseSatisfaction = 10.0f;
    availableActions.push_back(invent);

    // TeachSkill — pass on known techniques to another entity
    // POINTED: spreads innovations through social bonds
    Action teach("TeachSkill", 204, "social");
    teach.requirements = { {"happiness", 50.0f, 0.5f}, {"loneliness", 20.0f, 0.4f} };
    teach.statChanges  = { {"loneliness", -12.0f}, {"happiness", 9.0f},
                            {"boredom", -8.0f},    {"stress", -3.0f} };
    teach.baseSatisfaction = 10.0f;
    availableActions.push_back(teach);

    // FulfillDuty — act in service of one's tribe or community
    // SELF-DIRECTED: moderate across-the-board benefit; highest for collectivist entities
    Action duty("FulfillDuty", 205, "social");
    duty.requirements = { {"stress", 20.0f, 0.3f}, {"health", 40.0f, 0.4f} };
    duty.statChanges  = { {"stress", -6.0f}, {"happiness", 6.0f},
                           {"loneliness", -6.0f}, {"boredom", -5.0f} };
    duty.baseSatisfaction = 8.0f;
    availableActions.push_back(duty);

    // ChallengeLeader — ambitious entity contests tribal hierarchy
    // POINTED: direct challenge to the leader; high anger requirement
    Action challenge("ChallengeLeader", 206, "leadership");
    challenge.requirements = { {"anger", 48.0f, 0.7f}, {"happiness", 38.0f, 0.4f} };
    challenge.statChanges  = { {"anger", -18.0f}, {"stress", 12.0f},
                                {"happiness", 8.0f}, {"mentalHealth", -5.0f} };
    challenge.baseSatisfaction = 8.0f;
    availableActions.push_back(challenge);

    // DeclareWar — very high-anger, high-militarism entity initiates group conflict
    // POINTED: triggers tribal war stance in CivilizationEngine
    Action declWar("DeclareWar", 207, "safety");
    declWar.requirements = { {"anger", 65.0f, 0.8f}, {"stress", 55.0f, 0.4f} };
    declWar.statChanges  = { {"anger", -12.0f}, {"stress", 16.0f},
                              {"health", -4.0f}, {"happiness", -3.0f} };
    declWar.baseSatisfaction = 9.0f;
    availableActions.push_back(declWar);

    Action negotiate("Negotiate", 208, "social");
    negotiate.requirements = { {"anger", 30.0f, 0.5f}, {"mentalHealth", 55.0f, 0.5f} };
    negotiate.statChanges  = { {"anger", -16.0f}, {"stress", -9.0f},
                                {"loneliness", -6.0f}, {"happiness", 7.0f} };
    negotiate.baseSatisfaction = 10.0f;
    availableActions.push_back(negotiate);

    // Specialize — entity commits to a social role (farmer, warrior, priest, etc.)
    // SELF-DIRECTED: high long-term boredom reduction, purpose boost
    Action specialize("Specialize", 209, "achievement");
    specialize.requirements = { {"happiness", 45.0f, 0.5f}, {"boredom", 38.0f, 0.5f} };
    specialize.statChanges  = { {"boredom", -18.0f}, {"happiness", 12.0f},
                                 {"stress", -5.0f},  {"loneliness", 2.0f} };
    specialize.baseSatisfaction = 11.0f;
    availableActions.push_back(specialize);

    // ════════════════════════════════════════════════════════════════════════
    // ERA-AWARE SURVIVAL & CRAFT ACTIONS (Stone through Modern age)
    // ════════════════════════════════════════════════════════════════════════
    // Hunt — bring back food. Fires early (health < 85) and restores a solid
    // chunk of health so a population can always feed itself, even before
    // agriculture exists. Costs effort (stress, hygiene).
    Action hunt("Hunt", 210, "survival");
    // Foraging actions are pulled harder the hungrier the entity (and its kin) get.
    hunt.requirements = { {"hunger", 25.0f, 0.7f}, {"health", 85.0f, 0.5f} };
    hunt.statChanges  = { {"boredom", -12.0f}, {"happiness", 7.0f},
                           {"health", 14.0f},   {"stress", 4.0f}, {"hygiene", -3.0f} };
    hunt.baseSatisfaction = 18.0f;
    availableActions.push_back(hunt);

    Action gather("Gather", 211, "survival");
    gather.requirements = { {"hunger", 20.0f, 0.7f}, {"health", 60.0f, 0.3f} };
    gather.statChanges  = { {"boredom", -10.0f}, {"happiness", 5.0f},
                             {"health", 3.0f},    {"stress", -3.0f} };
    gather.baseSatisfaction = 10.0f;
    availableActions.push_back(gather);

    Action farm("Farm", 212, "survival");
    farm.requirements = { {"hunger", 20.0f, 0.6f}, {"health", 65.0f, 0.4f} };
    farm.statChanges  = { {"boredom", -14.0f}, {"happiness", 6.0f},
                           {"health", 4.0f},    {"stress", 5.0f} };
    farm.baseSatisfaction = 12.0f;
    availableActions.push_back(farm);

    Action build("Build", 213, "achievement");
    build.requirements = { {"health", 70.0f, 0.6f}, {"stress", 55.0f, 0.4f} };
    build.statChanges  = { {"boredom", -16.0f}, {"happiness", 10.0f},
                            {"stress", 6.0f},    {"health", -2.0f} };
    build.baseSatisfaction = 16.0f;
    availableActions.push_back(build);

    Action trade("Trade", 214, "social");
    trade.requirements = { {"happiness", 45.0f, 0.5f}, {"loneliness", 25.0f, 0.4f} };
    trade.statChanges  = { {"loneliness", -10.0f}, {"happiness", 9.0f},
                            {"boredom", -8.0f},    {"stress", -4.0f} };
    trade.baseSatisfaction = 14.0f;
    availableActions.push_back(trade);

    Action explore("Explore", 215, "achievement");
    explore.requirements = { {"boredom", 45.0f, 0.7f}, {"health", 65.0f, 0.3f} };
    explore.statChanges  = { {"boredom", -20.0f}, {"happiness", 12.0f},
                              {"stress", 3.0f},    {"loneliness", 3.0f} };
    explore.baseSatisfaction = 18.0f;
    availableActions.push_back(explore);

    Action duel("Duel", 216, "safety");
    duel.requirements = { {"anger", 55.0f, 0.7f}, {"health", 60.0f, 0.5f} };
    duel.statChanges  = { {"anger", -20.0f}, {"stress", -8.0f},
                           {"health", -8.0f}, {"happiness", 5.0f} };
    duel.baseSatisfaction = 12.0f;
    availableActions.push_back(duel);

    Action raid("Raid", 217, "safety");
    raid.requirements = { {"anger", 50.0f, 0.6f}, {"health", 65.0f, 0.5f},
                           {"stress", 45.0f, 0.4f} };
    raid.statChanges  = { {"anger", -15.0f}, {"stress", -6.0f},
                           {"health", -4.0f}, {"happiness", 4.0f} };
    raid.baseSatisfaction = 10.0f;
    availableActions.push_back(raid);

    Action defendTribe("DefendTribe", 218, "safety");
    defendTribe.requirements = { {"health", 55.0f, 0.5f}, {"anger", 35.0f, 0.4f} };
    defendTribe.statChanges  = { {"stress", -8.0f}, {"happiness", 6.0f},
                                  {"loneliness", -5.0f}, {"boredom", -8.0f} };
    defendTribe.baseSatisfaction = 14.0f;
    availableActions.push_back(defendTribe);

    Action marry("Marry", 219, "social");
    marry.requirements = { {"loneliness", 20.0f, 0.5f}, {"happiness", 50.0f, 0.6f} };
    marry.statChanges  = { {"happiness", 30.0f}, {"loneliness", -20.0f},
                            {"mentalHealth", 12.0f}, {"stress", -10.0f} };
    marry.baseSatisfaction = 60.0f;
    availableActions.push_back(marry);
    desireLinkedAction.push_back(marry);

    Action mourn("Mourn", 220, "health");
    mourn.requirements = { {"mentalHealth", 30.0f, 0.6f}, {"loneliness", 50.0f, 0.5f} };
    mourn.statChanges  = { {"stress", -12.0f}, {"mentalHealth", 8.0f},
                            {"happiness", -5.0f}, {"loneliness", -8.0f} };
    mourn.baseSatisfaction = 8.0f;
    availableActions.push_back(mourn);

    Action celebrate("Celebrate", 221, "social");
    celebrate.requirements = { {"happiness", 55.0f, 0.5f}, {"loneliness", 20.0f, 0.4f} };
    celebrate.statChanges  = { {"happiness", 15.0f}, {"loneliness", -12.0f},
                                {"stress", -8.0f},   {"boredom", -10.0f} };
    celebrate.baseSatisfaction = 22.0f;
    availableActions.push_back(celebrate);

    Action storytell("TellStory", 222, "social");
    storytell.requirements = { {"boredom", 30.0f, 0.5f}, {"loneliness", 20.0f, 0.3f} };
    storytell.statChanges  = { {"boredom", -14.0f}, {"loneliness", -8.0f},
                                {"happiness", 8.0f}, {"mentalHealth", 4.0f} };
    storytell.baseSatisfaction = 16.0f;
    availableActions.push_back(storytell);

    // ── Reflexive survival action (subsumption reactive layer) ───────────────
    // Flee is almost never chosen deliberately (tiny baseSatisfaction); it is
    // forced by reflexLayer() when the entity perceives imminent danger. The
    // act of escaping spikes fatigue/stress but removes the entity from harm.
    Action flee("Flee", 230, "safety");
    flee.requirements = { {"health", 25.0f, 0.2f} };
    flee.statChanges  = { {"stress", 16.0f},  {"fatigue", 12.0f},
                           {"happiness", -7.0f}, {"health", -2.0f},
                           {"boredom", -6.0f} };
    flee.baseSatisfaction = 2.0f;
    availableActions.push_back(flee);
}

// ── findActionByName ─────────────────────────────────────────────────────────
Action* FreeWillSystem::findActionByName(const std::string& name) {
    for (Action& a : availableActions) {
        if (a.name == name) return &a;
    }
    return nullptr;
}

// ── Subsumption architecture: reactive survival layer ────────────────────────
// Evaluated before deliberation. Returns an overriding survival action when a
// hard threshold is crossed, else nullptr (let the higher layers reason). The
// checks are ordered by immediacy of the threat to life.
Action* FreeWillSystem::reflexLayer(Entity* entity,
                                    const std::vector<Entity*>& neighbors,
                                    const ActionContext& context) {
    if (!entity) return nullptr;

    auto fire = [&](const char* name, const std::string& reason) -> Action* {
        Action* a = findActionByName(name);
        if (!a) return nullptr;
        ++reflexOverrideCount;
        lastReflexReason = reason;
        entity->innerMonologue = "[reflex] " + reason;
        return a;
    };

    // (1) IMMINENT DANGER — environmental hazard (fire/predator/disaster) or an
    //     aggressor nearby who is enraged at this entity. Highest priority: a
    //     threat to life right now overrides everything, including hunger.
    bool envHazard = context.env.safetyLevel < 22.0f;
    Entity* aggressor = nullptr;
    for (Entity* nb : neighbors) {
        if (!nb) continue;
        for (const auto& pa : nb->list_entityPointedAnger) {
            if (pa.pointedEntity == entity && pa.anger > 55.0f) { aggressor = nb; break; }
        }
        if (aggressor) break;
    }
    if (envHazard || aggressor) {
        // Fight-or-flight: a healthy, bold, angry agent with allies will stand
        // its ground; otherwise the primitive layer chooses flight.
        bool canFight = entity->entityHealth > 55.0f &&
                        entity->personality.neuroticism < 48.0f &&
                        (entity->entityGeneralAnger > 45.0f || !neighbors.empty());
        if (aggressor && canFight) {
            if (Action* d = fire("DefendTribe", "stand and defend against aggressor"))
                return d;
            if (Action* du = fire("Duel", "confront the aggressor")) return du;
        }
        if (Action* f = fire("Flee", envHazard ? "flee environmental danger"
                                               : "flee from hostile aggressor"))
            return f;
    }

    // (2) STARVATION — life-threatening hunger. Eat stored rations if any,
    //     otherwise go get food before the store runs dry. We engage well before
    //     health starts bleeding (starvation damage begins at hunger 80), and we
    //     reach for the best food source the agent can manage rather than only
    //     the lowest-yield forage — a single Gather can't out-pace real hunger.
    if (entity->entityHunger >= 70.0f) {
        if (entity->foodStore >= 1.0f) {
            if (Action* e = fire("EatMeal", "eat to stop starving")) return e;
        }
        // No stored food: produce some. Hunt/Farm yield far more than Gather, so
        // prefer them when the agent is still fit enough to work; fall back to
        // gathering (the lowest barrier) when badly weakened.
        if (entity->entityHealth > 50.0f) {
            if (Action* h = fire("Hunt", "hunt — larder is empty")) return h;
        }
        if (entity->entityHealth > 40.0f) {
            if (Action* f = fire("Farm", "work the fields — larder is empty")) return f;
        }
        if (Action* g = fire("Gather", "forage urgently — no food left")) return g;
    }

    // (3) TOTAL EXHAUSTION — collapse if not rested.
    if (entity->fatigueLevel >= 92.0f) {
        if (Action* s = fire("Sleep", "collapse from exhaustion")) return s;
    }

    // (4) HEALTH COLLAPSE — barely alive: stop and recover.
    if (entity->entityHealth < 12.0f) {
        if (Action* r = fire("Rest", "rest to survive near-fatal injury")) return r;
    }

    return nullptr; // no reflex fired — defer to deliberation
}

void FreeWillSystem::updatePersonalityFromExperience(Entity* ent, const Action& act, float outcomeSuccess) {
    std::vector<std::string> actions_possible = {
        "Murder", "SelfHarm", "Gossip", "Socialize", "Prayer", "Prayer",
        "DrinkAlcohol", "Smoke", "Rest", "Sleep", "Flirt", "Date", "Take Shower"
    };
    if (std::find(actions_possible.begin(), actions_possible.end(), act.name) != actions_possible.end()) { // si une action qui change la personnalité de l'expérience on entre
        if (act.name == "Murder" || act.name == "SelfHarm") {
            ent->personality.neuroticism += 1.2f;
            ent->personality.agreeableness -= 1.5f;
            ent->SelfConcept.perceivedAgreeableness -= 1.2f;
            ent->SelfConcept.perceivedNeuroticism += 1.12f;
            ent->SelfConcept.selfEsteem += 1.1f;
            ent->SelfConcept.selfEfficacy += 1.1f;
        } else if ((act.name == "Socialize" || act.name == "Gossip") && outcomeSuccess > 0.7) {
            ent->personality.agreeableness += 1.0f;
            ent->personality.openness += 0.9f;
            ent->SelfConcept.perceivedExtraversion += 0.9f;
            ent->SelfConcept.perceivedAgreeableness += 0.6f;
            ent->SelfConcept.selfEsteem += 1.1f;
            ent->SelfConcept.selfEfficacy += 1.1f;
        } else if (act.name == "Prayer" && outcomeSuccess > 0.55) {
            ent->personality.conscientiousness += 1.3f;
        } else if (act.name == "DrinkAlcohol" || act.name == "Smoke") {
            ent->personality.neuroticism += 0.9f;
            ent->SelfConcept.selfEsteem += 1.1f;
            ent->SelfConcept.selfEfficacy += 1.1f;
            ent->SelfConcept.perceivedAgreeableness -= 0.2f;
            ent->SelfConcept.perceivedNeuroticism += 0.7f;
        } else if (act.name == "Take Shower" || act.name == "Rest" || act.name == "Sleep") {
            ent->personality.neuroticism -= 0.2f;
        } else if ((act.name == "Flirt" || act.name == "Date") && outcomeSuccess > 0.6) {
            ent->personality.neuroticism -= 0.3f;
            ent->SelfConcept.selfEsteem += 1.1f;
            ent->SelfConcept.selfEfficacy += 1.1f;
            ent->personality.agreeableness += 1.6f;
            ent->personality.openness += 1.6f;
            ent->SelfConcept.perceivedExtraversion += 1.2f;
        }
    } else {
        ent->SelfConcept.perceivedAgreeableness -= 0.2f;
        ent->SelfConcept.perceivedExtraversion -= 0.2f;
        ent->SelfConcept.perceivedNeuroticism -= 0.2f;
        ent->SelfConcept.selfEsteem -= 0.2f;
        ent->SelfConcept.selfEfficacy -= 0.5f;
    }
}

static float socialLinkIncrement(SocialTier tier, std::mt19937& rng) {
    // Raw ranges per tier
    float lo, hi;
    switch (tier) {
    case STRANGER:      lo = 2.5f; hi = 5.0f; break;
    case ACQUAINTANCE:  lo = 2.0f; hi = 4.0f; break;
    case FAMILIAR:      lo = 1.2f; hi = 2.8f; break;
    case FRIEND:        lo = 0.6f; hi = 1.6f; break;
    case CLOSE_FRIEND:  lo = 0.3f; hi = 0.8f; break;
    default:            lo = 0.5f; hi = 1.5f; break;
    }
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

// Calculate social influence from neighbors
float FreeWillSystem::calculateSocialInfluence(Entity* entity, const std::vector<Entity*>& neighbors, const Action& action) {
    if (neighbors.empty()) return 0.5f;
    float influence = 1.0f;
    float totalWeight = 1.0f;
    // M11: reference the relationship lists in place. The old getList*()
    // accessors return BY VALUE — three heap-allocating vector copies per
    // neighbor per action, ~1,000 copies per decision, which made this the
    // single hottest function in the whole simulation.
    const auto& desireList = entity->list_entityPointedDesire;
    const auto& angerList  = entity->list_entityPointedAnger;
    const auto& socialList = entity->list_entityPointedSocial;
    for (Entity* neighbor : neighbors) {
        float relationshipWeight = 0.5f;

        // Positive relationships increase weight for positive actions
        for (const auto& social : socialList) {
            if (social.pointedEntity == neighbor) {
                relationshipWeight += social.social * 0.3f;
            }
        }
        for (const auto& desire : desireList) {
            if (desire.pointedEntity == neighbor) {
                relationshipWeight += desire.desire * 0.45f;
            }
        }
        // Negative relationships increase weight for negative actions
        for (const auto& anger : angerList) {
            if (anger.pointedEntity == neighbor) {
                relationshipWeight -= anger.anger * 0.8f;
            }
        }

        // Neighbors with similar stats influence action choices
        float neighborMentalHealth = neighbor->entityMentalHealth;
        float neighborAnger = neighbor->entityGeneralAnger;
        float neighborHappiness = neighbor->entityHapiness;

        // If neighbors are angry/stressed, negative actions become more likely
        if (action.name == "Murder" || action.name == "Discrimination" || action.name == "AngerConnection" ||
            action.name == "Insult" || action.name == "Betray" || action.name == "Jealousy" ||
            action.name == "Manipulate" || action.name == "IgnoreAvoid") {
            influence += (neighborAnger / 100.0f) * 0.5f;
        }
        // If neighbors are happy/healthy, positive actions become more likely
        else if (action.name == "Socialize" || action.name == "GoodConnection" || action.name == "breeding" ||
                 action.name == "couple" || action.name == "HelpSupport" || action.name == "Apologize" ||
                 action.name == "Flirt" || action.name == "Date" || action.name == "Reconcile" ||
                 action.name == "CreativeActivity" || action.name == "LearnSkill") {
            influence += (neighborHappiness / 100.0f) * 0.6f;
            if (action.needCategory == "social") {
                influence += relationshipWeight * 0.6f;
            }
        }
        // Coping actions influenced by neighbor stress
        else if (action.name == "DrinkAlcohol" || action.name == "Smoke" || action.name == "SelfHarm" ||
                 action.name == "Procrastinate" || action.name == "QuitGiveUp") {
            float neighborStress = neighbor->entityStress;
            influence += (neighborStress / 100.0f) * 0.6f;
        }
        totalWeight += 1.0f + std::max(-0.5f, relationshipWeight);
    }
    return totalWeight > 0 ? ((influence + totalWeight * 2.6f) / neighbors.size()) : 2.1f;
}

void FreeWillSystem::tickEmotionalSuppression(Entity* entity) {
    float& raw = entity->emotionalState.rawAnger;
    float& expressed = entity->emotionalState.expressedAnger;
    float& debt = entity->emotionalState.suppressionDebt;
    float suppressionTendency = entity->personality.agreeableness / 100.0f;

    raw = entity->entityGeneralAnger;
    expressed = raw * (1.0f - suppressionTendency * 0.6f);
    debt += (raw - expressed) * 0.1f;

    if (debt > 20.0f) {
        entity->entityMentalHealth -= debt * 0.05f;
        entity->entityStress += debt * 0.03f;
    }
    if (debt > 60.0f) {
        entity->entityGeneralAnger = std::min(100.0f, raw + debt * 0.5f);
        debt = 0.0f;
        ASHB_TRACE_STREAM << entity->getName() << " EXPLOSION ÉMOTIONNELLE après suppression prolongée!\n";
    }
}

void FreeWillSystem::tickValueGoalAlignment(Entity* entity) {
    ValueSystem& v = entity->ValueSystem;
    std::string currentGoal = entity->getTypeGoal();

    if (v.familyOrientation > 75.0f && currentGoal == "build_career") {
        for (LifeGoal& goal : entity->m_goals) {
            if (goal.type == "build_career") {
                goal.priority -= 0.5f;
                if (goal.priority < 20.0f) {
                    entity->addOrBoostGoal("find_partner", 1.0f);
                    ASHB_TRACE_STREAM << entity->getName() << " shifts life goal: career -> find_partner\n";
                }
            }
        }
    }
    if (v.achievementDrive > 80.0f && currentGoal == "happiness") {
        entity->addOrBoostGoal("build_career", 1.0f);
        ASHB_TRACE_STREAM << entity->getName() << " realizes happiness goal feels directionless, shifts to career\n";
    }
    if (v.spiritualNeed > 85.0f && entity->entityMentalHealth < 40.0f) {
        entity->addOrBoostGoal("self", 1.0f);
        ASHB_TRACE_STREAM << entity->getName() << " enters spiritual withdrawal (mental health crisis)\n";
    }
}

// update values from experiences
void FreeWillSystem::updateValuesFromExperiences(Entity* ent, Action*& action, float outcomeSuccess) {
    ValueSystem& v = ent->ValueSystem;
    if ((action->name == "couple" || action->name == "breeding") && outcomeSuccess > 0.75) {
        v.familyOrientation = std::min(100.0f, v.familyOrientation + 2.5f);
    }
    if (action->name == "Betray" || action->name == "BreakUp") {
        v.collectivism = std::max(0.0f, v.collectivism - 3.0f);
        v.familyOrientation = std::max(0.0f, v.familyOrientation - 2.0f);
    }
    if (action->name == "Smoke" || action->name == "DrinkAlcohol") {
        v.hedonism = std::min(100.0f, v.hedonism + 1.5f);
        v.spiritualNeed = std::min(100.0f, v.spiritualNeed + 0.8f);
        v.collectivism = std::min(100.0f, v.collectivism + 0.4f);
    }
    if ((action->name == "Work on Project" || action->name == "LearnSkill") && outcomeSuccess > 0.8f) {
        v.achievementDrive = std::min(100.0f, v.achievementDrive + 1.5f);
    }
    if ((action->name == "Work on Project" || action->name == "LearnSkill") && outcomeSuccess < 0.2f) {
        v.achievementDrive = std::max(0.0f, v.achievementDrive - 1.5f);
    }

    if ((action->name == "Basic Manual Work" || action->name == "LearnSkill") && outcomeSuccess > 0.8f) {
        v.achievementDrive = std::min(100.0f, v.achievementDrive + 1.5f);
    }
    if ((action->name == "Basic Manual Work" || action->name == "LearnSkill") && outcomeSuccess < 0.2f) {
        v.achievementDrive = std::max(0.0f, v.achievementDrive - 1.5f);
    }
    if ((action->name == "Socialize" || action->name == "Flirt" || action->name == "Date") && outcomeSuccess < 0.2f) {
        v.hedonism = std::max(0.0f, v.hedonism - 1.5f);
    }
    if ((action->name == "HelpSupport" || action->name == "Apologize" || action->name == "Reconcile") && outcomeSuccess > 0.8f) {
        v.collectivism = std::min(100.0f, v.collectivism + 1.5f);
    }
    if ((action->name == "HelpSupport" || action->name == "Apologize" || action->name == "Reconcile") && outcomeSuccess < 0.2f) {
        v.collectivism = std::max(0.0f, v.collectivism - 1.5f);
    }
    if ((action->name == "SelfHarm" || action->name == "QuitGiveUp") && outcomeSuccess > 0.8f) {
        v.spiritualNeed = std::min(100.0f, v.spiritualNeed + 1.5f);
    }
    if ((action->name == "SelfHarm" || action->name == "QuitGiveUp") && outcomeSuccess < 0.2f) {
        v.spiritualNeed = std::max(0.0f, v.spiritualNeed - 1.5f);
    }
    if ((action->name == "Murder" || action->name == "Discrimination" || action->name == "AngerConnection" ||
         action->name == "Insult" || action->name == "Betray" || action->name == "Jealousy" ||
         action->name == "Manipulate" || action->name == "IgnoreAvoid") && outcomeSuccess > 0.8f) {
        v.collectivism = std::min(100.0f, v.collectivism + 1.5f);
    }
    if ((action->name == "Murder" || action->name == "Discrimination" || action->name == "AngerConnection" ||
         action->name == "Insult" || action->name == "Betray" || action->name == "Jealousy" ||
         action->name == "Manipulate" || action->name == "IgnoreAvoid") && outcomeSuccess < 0.2f) {
        v.collectivism = std::max(0.0f, v.collectivism - 1.5f);
    }
    float grief = ent->getGriefIntensity();
    if (grief > 0.7f) {
        v.spiritualNeed = std::min(100.0f, v.spiritualNeed + grief * 5.0f);
    }
}

float calculateNormModifier(Entity* entity, const Action& action, const SocialNorm& norm) {
    if (norm.actionName != action.name) return 1.0f;
    float prevalence = norm.prevalence;
    float pressure = norm.normPressure;
    float conformityDrive = entity->personality.agreeableness / 100.0f;
    conformityDrive += entity->ValueSystem.collectivism / 100.0f;
    conformityDrive *= 0.5f;
    if (prevalence > 0.3f) {
        return 1.0f + (prevalence * pressure * conformityDrive);
    } else if (prevalence < 0.1f) {
        float rebellion = entity->personality.openness / 100.0f;
        return 1.0f - (pressure * conformityDrive) + (rebellion * 0.3f);
    }
    return 1.0f;
}

// Reinforcement-learning state signature.
// Encodes the handful of drives most predictive of which action pays off into a
// short string (e.g. "Ma_p"). Buckets: loneliness L/M/H, anger present, hunger
// present, social context p(eers)/s(olo). ~24 distinct states keep the per-agent
// Q-table small while still letting agents learn context-dependent preferences.
std::string FreeWillSystem::rlStateSignature(Entity* entity, int numNearby) const {
    if (!entity) return "none";
    auto b3 = [](float v) -> const char* {
        return v < 33.0f ? "L" : (v < 66.0f ? "M" : "H");
    };
    std::string s;
    s.reserve(4);
    s += b3(entity->entityLoneliness);
    s += (entity->entityGeneralAnger > 50.0f ? "a" : "_");
    s += (entity->entityHunger      > 50.0f ? "h" : "_");
    s += (numNearby > 0              ? "p" : "s");
    return s;
}

// Main decision-making function
// main entry = entree principale de fichier
Action* FreeWillSystem::chooseAction(Entity* entity, const std::vector<Entity*>& neighbors, const ActionContext& context) {
    // ── Single decision pipeline (the ~330-line legacy scorer that used to
    //    live below was unreachable: the cognitive path pads its candidate
    //    list and never returns null, so the two near-duplicate scorers only
    //    drifted apart silently). Control order is a subsumption hierarchy:
    //
    //    1. REFLEX  — hard survival thresholds veto everything.
    //    2. HABIT   — calm, familiar contexts run on cached policy.
    //    3. DELIBERATION — the cognitive pipeline scores candidates.
    // M11 micro-profiler (ASHB_PROFILE=1): attributes decision time to the
    // pipeline layers; prints an aggregate every 20k decisions.
    static const bool s_prof = std::getenv("ASHB_PROFILE") != nullptr;
    static double s_msReflex = 0, s_msGoal = 0, s_msHabit = 0, s_msCog = 0;
    static long   s_calls = 0;
    auto t0 = std::chrono::steady_clock::now();
    auto lap = [&](double& sink) {
        if (!s_prof) return;
        auto t1 = std::chrono::steady_clock::now();
        sink += std::chrono::duration<double, std::milli>(t1 - t0).count();
        t0 = t1;
    };
    auto report = [&]() {
        if (!s_prof || ++s_calls % 20000 != 0) return;
        std::cout << "PROFILE-DECIDE avg ms/call over " << s_calls << ": reflex="
                  << s_msReflex / s_calls << " goal=" << s_msGoal / s_calls
                  << " habit=" << s_msHabit / s_calls << " cognitive=" << s_msCog / s_calls << "\n";
    };

    // M10 possess mode: the player steers this entity — a queued command
    // overrides reflex, habit and deliberation until released or changed.
    if (entity && entity->entityId == possessedEntityId &&
        possessedActionIdx >= 0 && possessedActionIdx < (int)availableActions.size()) {
        return &availableActions[possessedActionIdx];
    }

    if (Action* reflex = reflexLayer(entity, neighbors, context)) {
        lap(s_msReflex); report();
        return reflex;
    }
    lap(s_msReflex);

    // Goal↔value drift used to live only in the dead legacy path; it now
    // actually runs once per decision.
    tickValueGoalAlignment(entity);
    lap(s_msGoal);

    if (Action* habitual = checkHabitTrigger(context)) {
        ASHB_TRACE_STREAM << ">>> Habit Triggered: " << habitual->name << " <<<\n";
        lap(s_msHabit); report();
        return habitual;
    }
    lap(s_msHabit);

    if (Action* cp = cognitiveChooseAction(entity, neighbors, context)) {
        lap(s_msCog); report();
        return cp;
    }
    lap(s_msCog);

    // Reaching here means the action catalog is empty — a programming error,
    // not a game state.
    return availableActions.empty() ? nullptr : &availableActions[0];
}

NeedLevel FreeWillSystem::updateHieratchicalNeed(Entity* ent, const Action& action) {
    if (!ent) return SELF_ACTUALIZATION;
    auto& needs = ent->needs;
    // BELONGING
    if (action.name == "Desire" || action.name == "GoodConnection" || action.name == "Gossip" ||
        action.name == "Betray" || action.name == "Flirt" || action.name == "couple") {
        needs["social"].satisfactionThreshold += 5.0f;
        needs["social"].satisfactionThreshold -= needs["social"].decayRate;
        needs["love"].satisfactionThreshold += 5.0f;
        needs["love"].satisfactionThreshold -= needs["love"].decayRate;
        needs["social"].urgency -= 0.5f;
        needs["love"].urgency -= 0.5f;
        return BELONGING;
    }
    // SELF ACTUALIZATION
    else if (action.name == "Prayer" || action.name == "Read" || action.name == "WatchEntertainment" || action.name == "Gaming") {
        needs["meaning"].satisfactionThreshold += 5.0f;
        needs["creativity"].satisfactionThreshold += 5.0f;
        needs["meaning"].urgency -= 0.5f;
        needs["creativity"].urgency -= 0.5f;
        return SELF_ACTUALIZATION;
    }
    // PHYSIOLOGICAL
    else if (action.name == "EatMeal" || action.name == "Sleep" || action.name == "Take Shower" || action.name == "Rest") {
        needs["hunger"].satisfactionThreshold += 5.0f;
        needs["sleep"].satisfactionThreshold += 5.0f;
        needs["hygiene"].satisfactionThreshold += 5.0f;
        return PHYSIOLOGICAL;
    }
    // ESTEEM
    else if (action.name == "LearnSkill" || action.name == "Work on Project" || action.name == "Basic Manual Work") {
        needs["achievement"].satisfactionThreshold += 5.0f;
        needs["recognition"].satisfactionThreshold += 5.0f;
        needs["achievement"].urgency -= 5.0f;
        needs["recognition"].urgency -= 5.0f;
        return ESTEEM;
    }
    // SAFETY
    else if (action.name == "Discrimination" || action.name == "Murder" || action.name == "SetBoundaries") {
        needs["safety"].satisfactionThreshold += 5.0f;
        needs["safety"].satisfactionThreshold -= needs["safety"].decayRate;
        needs["safety"].urgency -= 5.0f;
        return SAFETY;
    }
    // NEGATIVE CASE
    else if (action.name == "QuitGiveUp") {
        needs["meaning"].satisfactionThreshold -= 2.0f;
        needs["creativity"].satisfactionThreshold -= 2.0f;
        needs["creativity"].urgency += 2.0f;
        return SELF_ACTUALIZATION;
    }
    return BELONGING;
}

SocialTier FreeWillSystem::getSocialTier(Entity* from, Entity* to) const {
    int idx = from->contains(from->list_entityPointedSocial, to, 4);
    if (idx == -1) return STRANGER;
    float s = from->list_entityPointedSocial[idx].social;
    if (s < 25.0f) return ACQUAINTANCE;
    if (s < 55.0f) return FAMILIAR;
    if (s < 80.0f) return FRIEND;
    return CLOSE_FRIEND;
}

Action* FreeWillSystem::TriggerDesireLinkedAction(){
  int choice = BetterRand::genNrInInterval(0, (int)desireLinkedAction.size() -1);
  return &desireLinkedAction.at(choice);
}

Action* FreeWillSystem::TriggerHatredLinkedAction(){
  int choice = BetterRand::genNrInInterval(0, (int)hatredLinkedAction.size()-1);
  return &hatredLinkedAction.at(choice);
}

Action* FreeWillSystem::ChooseSpecificSocialAction(Entity* ent){
    //afin de favoriser les actions en dehors des actions choisis
    // on peut les déclencher nous même
    // social tick entity hacking
    //std::map<Action&, float> action_lst
    std::vector<std::pair<Action*, float>> action_lst;
    for (Action& act : availableActions) {
        float score;
        if(isActionSocial(&act)){
            //action social
            if(act.name == "Socialize" || act.name == "GoodConnection" || act.name == "DigitalSocial" || act.name == "Apologize" || act.name == "Reconcile" || act.name == "HelpSupport"){
                if(ent->isGoalType("make_friends")){

                    action_lst.push_back({ &act, static_cast<float>(ent->list_entityPointedSocial.size() * 2 )});
                }else if(ent->isGoalType("happiness")){
                    action_lst.push_back({&act, static_cast<float>(ent->list_entityPointedSocial.size() * 1.25)});
                }else{
                    action_lst.push_back({&act, static_cast<float>(ent->list_entityPointedSocial.size())});
                }
            }

            if(act.name == "Desire" || act.name == "Jealousy"
                || act.name == "Manipulate" || act.name == "Flirt"
                || act.name == "Date" || act.name == "Reconcile" || act.name == "couple" || act.name == "breeding"){
                if(ent->isGoalType("find_partner")){
                    action_lst.push_back({&act, static_cast<float>(std::max(ent->list_entityPointedDesire.size(), ent->list_entityPointedCouple.size()) * 2)});
                }else if(ent->isGoalType("build_family")){
                    action_lst.push_back({&act, static_cast<float>(std::max(ent->list_entityPointedDesire.size(), ent->list_entityPointedCouple.size()) * 1.75)});
                }else if(ent->isGoalType("happiness")){
                    action_lst.push_back({&act, static_cast<float>(std::max(ent->list_entityPointedDesire.size(), ent->list_entityPointedCouple.size()) * 1.25)});
                }else{
                    action_lst.push_back({&act, static_cast<float>(std::max(ent->list_entityPointedDesire.size(), ent->list_entityPointedCouple.size()))});
                }
            }

            if(act.name == "AngerConnection" || act.name == "Murder" || act.name == "Discrimination"
                || act.name == "Suicide" || act.name == "Gossip"  || act.name == "Betray"
                || act.name == "SelfHarm" || act.name == "Insult" || act.name == "BreakUp" || act.name == "SetBoundaries"){
                if(ent->isGoalType("self")){
                    action_lst.push_back({&act, static_cast<float>(ent->list_entityPointedAnger.size() * 2)});
                }else if(ent->isGoalType("build_career")){
                    action_lst.push_back({&act, static_cast<float>(ent->list_entityPointedAnger.size() * 1.15)});
                }else if(ent->isGoalType("happiness")){
                    action_lst.push_back({&act, static_cast<float>(ent->list_entityPointedAnger.size() * 1.25)});
                }else{
                    action_lst.push_back({&act, static_cast<float>(ent->list_entityPointedAnger.size() * 2)});
                }
            }
        }
    }
    if (action_lst.empty()) {
        return nullptr;
    }
    Action* bestSocialAction = nullptr;
    float bestScore = 0.0;
    for (auto const& x : action_lst){
        if(x.second > bestScore){
            bestSocialAction = x.first;
            bestScore = x.second;
        }
    }
    if (bestSocialAction) {
        ASHB_TRACE_STREAM << "======> side action chosen: " << bestSocialAction->name << " with a score of " << bestScore;
    }
    return bestSocialAction;
}

// ici on assimile l'action pointé vers sur celui qui est pointé par le pointeur
void FreeWillSystem::pointedAssimilation(Entity* pointer, Entity* pointed, Action* action, CivilizationEngine* engineCivilization) {
    if (!pointer || !pointed) {
        return;
    }

    if(isActionSocial(action)){
        //on réduit le déficit
        pointer->socialDeficit -= BetterRand::genNrInInterval(1.0f,2.0f);
        pointer->dayWithoutSocialAction = 0;
    }else{
        pointer->dayWithoutSocialAction++;
    }

    // M4: interacting IS observing — both parties form/refresh a mental model
    // of the other. This is what makes theory-of-mind real: selectSocialTarget
    // and gossip read these models, and until now they were never created for
    // anyone but the self. Runs before the per-action branches on purpose:
    // even a rebuffed advance teaches you something about the person.
    {
        const int   today     = day;
        const int   sentiment = getActionSentiment(action->name);   // −1 / 0 / +1
        auto observe = [&](Entity* observer, Entity* observed) {
            MentalModelOfOther* m = observer->getModelOf(observed);
            if (!m) {
                // ToM capacity: nobody tracks more than ~16 people in detail.
                if (observer->list_MentalModelOfOther.size() >= 16) return;
                m = new MentalModelOfOther();
                m->entityPointed = observed;
                observer->list_MentalModelOfOther.push_back(m);
            }
            // Empathic accuracy: agreeable people read others better.
            float accuracy = 0.3f + observer->personality.agreeableness / 200.0f;
            m->updateFromObservation(observed, accuracy, today);
            m->trustLevel = std::max(0.0f, std::min(100.0f,
                                m->trustLevel + 2.0f * (float)sentiment));
            m->lastInteractionOutcome = action->name;
        };
        observe(pointer, pointed);
        observe(pointed, pointer);
    }

    if (action->name == "Desire") {
        // Natural attraction checks
        if (pointed->entityHygiene < 30) {
            ASHB_TRACE_STREAM << "Desire blocked: hygiene too low\n";
            return;
        }
        if (pointed->entityHapiness < 20) {
            ASHB_TRACE_STREAM << "Desire blocked: too unhappy\n";
            return;
        }
        if (pointed->entityHealth < 25) {
            ASHB_TRACE_STREAM << "Desire blocked: health too low\n";
            return;
        }

        SocialTier tier = getSocialTier(pointer, pointed);
        if (tier == STRANGER) {
            std::uniform_real_distribution<float> roll(0.0f, 1.0f);
            if (roll(rng) > 0.40f) {  // 40% instant-attraction chance for strangers
                ASHB_TRACE_STREAM << "Desire blocked: " << pointer->getName() << " hasn't had enough interaction with " << pointed->getName() << "\n";
                return;
            }
            ASHB_TRACE_STREAM << "Desire: instant attraction!\n";
        }

        int anger_index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (anger_index != -1 && pointer->list_entityPointedAnger[anger_index].anger > 50) {
            pointer->list_entityPointedAnger[anger_index].anger -= BetterRand::genNrInInterval(3, 5);
            ASHB_TRACE_STREAM << "Desire blocked: too much anger\n";
            return;
        }

        int index = pointer->contains(pointer->list_entityPointedDesire, pointed, 1);
        float attractiveness = (pointed->entityHygiene / 100.0f) * 0.3f +
                               (pointed->entityHapiness / 100.0f) * 0.4f +
                               (pointed->entityHealth / 100.0f) * 0.3f;

        if (index == -1) {
            float desire = static_cast<float>(BetterRand::genNrInInterval(10, 25)) * std::max(0.7f, attractiveness);
            pointer->addDesire({ 1, pointed, desire });
            pointed->addSocial({ 1, pointer, desire * 0.5f });
            ASHB_TRACE_STREAM << "Desire: new link " << pointer->getName() << " -> " << pointed->getName() << " (" << desire << ")\n";
        } else {
            float currentDesire = pointer->list_entityPointedDesire[index].desire;
            float lo = 0.3f, hi = 4.0f;
            if (currentDesire < 20.0f) { lo = 5.5f; hi = 7.0f; }
            else if (currentDesire < 45.0f) { lo = 2.0f; hi = 7.9f; }
            else if (currentDesire < 70.0f) { lo = 0.3f; hi = 4.0f; }
            std::uniform_real_distribution<float> incDist(lo, hi);
            float increment = incDist(rng) * std::max(0.5f, attractiveness);
            pointer->list_entityPointedDesire[index].desire = std::min(100.0f, pointer->list_entityPointedDesire[index].desire + increment);
            ASHB_TRACE_STREAM << "Desire reinforced " << pointer->getName() << " -> " << pointed->getName() << " +" << increment << "\n";
        }

        // ── Reciprocal attraction ────────────────────────────────────────────
        // Being pursued stirs some desire back, scaled by how attractive the
        // admirer is to the target. WITHOUT this, desire was one-directional and
        // the mutual-desire requirement for couples almost never bootstrapped —
        // the root cause of "barely any couples / no breeding".
        float myAttractiveness = (pointer->entityHygiene  / 100.0f) * 0.3f +
                                 (pointer->entityHapiness / 100.0f) * 0.4f +
                                 (pointer->entityHealth   / 100.0f) * 0.3f;
        if (pointer->entitySex != pointed->entitySex) myAttractiveness += 0.20f;
        // The target resists if they already resent the admirer.
        int recipAnger = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
        bool resents = (recipAnger != -1 && pointed->list_entityPointedAnger[recipAnger].anger > 35.0f);
        if (!resents && pointed->entityHealth > 0.0f) {
            int pidx = pointed->contains(pointed->list_entityPointedDesire, pointer, 1);
            float recip = static_cast<float>(BetterRand::genNrInInterval(3, 7)) * std::max(0.4f, myAttractiveness);
            if (pidx == -1) {
                pointed->addDesire({ 1, pointer, recip });
            } else {
                pointed->list_entityPointedDesire[pidx].desire =
                    std::min(100.0f, pointed->list_entityPointedDesire[pidx].desire + recip * 0.6f);
            }
        }
    }
    else if (action->name == "AngerConnection") {
        int index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (index == -1) {
            float anger = static_cast<float>(BetterRand::genNrInInterval(2, 7));
            ASHB_TRACE_STREAM << "Nouveau lien anger ajouté entre: (" << pointer->getId() << ")" << pointer->getName()
                      << " -> (" << pointed->getId() << ")" << pointed->getName() << " " << anger << std::endl;
            pointer->addAnger({ 1, pointed, anger });
            pointed->addAnger({ 1, pointer, anger * 0.4f });
        } else {
            float increment = static_cast<float>(BetterRand::genNrInInterval(3, 8));
            if (pointed->entityDiseaseType != -1) {
                pointer->list_entityPointedAnger[index].anger += increment + BetterRand::genNrInInterval(3, 9);
            } else {
                pointer->list_entityPointedAnger[index].anger += increment;
            }
            ASHB_TRACE_STREAM << "Anger renforcé entre: (" << pointer->getId() << ")" << pointer->getName()
                      << " -> (" << pointed->getId() << ")" << pointed->getName() << " +" << increment << std::endl;
        }
    }
    else if (action->name == "Socialize") {
        pointed->meetingCount++;
        SocialTier tier = getSocialTier(pointer, pointed);
        int index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (index == -1) {
            float seed = 12.0f + BetterRand::genNrInInterval(5, 15);
            pointer->addSocial({ 1, pointed, seed });
            pointed->addSocial({ 1, pointer, seed * 0.80f });
            ASHB_TRACE_STREAM << "Socialize: first link formed " << pointer->getName() << " -> " << pointed->getName() << " (" << seed << ")\n";
        } else {
            float increment = socialLinkIncrement(tier, rng);
            // Personality multiplier: extraverts build bonds faster
            float personalityMult = 0.6f + (pointer->personality.extraversion / 100.0f) * 0.8f;
            increment *= personalityMult;
            if (pointed->entityDiseaseType != -1) increment *= 0.4f;
            pointer->list_entityPointedSocial[index].social = std::min(100.0f, pointer->list_entityPointedSocial[index].social + increment);
            ASHB_TRACE_STREAM << "Socialize: link deepened " << pointer->getName() << " -> " << pointed->getName()
                      << " +" << increment << " (tier=" << tier << ", total=" << pointer->list_entityPointedSocial[index].social << ")\n";
        }
    }
    else if (action->name == "GoodConnection") {
        SocialTier tier = getSocialTier(pointer, pointed);
        int index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        float currentSocial = (index != -1) ? pointer->list_entityPointedSocial[index].social : 0.0f;
        if (tier == STRANGER && pointed->meetingCount < 3) {
            float seed = 8.0f + BetterRand::genNrInInterval(5, 12);
            pointer->addSocial({ 1, pointed, seed });
            pointed->addSocial({ 1, pointer, seed * 0.75f });
            ASHB_TRACE_STREAM << "GoodConnection (stranger): chance encounter " << pointer->getName() << " <-> " << pointed->getName() << " (" << seed << ")\n";
            pointed->onMajorEventAddOrBoostGoal("good_connection");
            pointer->onMajorEventAddOrBoostGoal("good_connection");
            return;
        }
        if (index == -1) {
            float seed = 12.0f + BetterRand::genNrInInterval(6, 14);
            pointer->addSocial({ 1, pointed, seed });
            pointed->addSocial({ 1, pointer, seed * 0.70f });
            ASHB_TRACE_STREAM << "GoodConnection: new bond " << pointer->getName() << " -> " << pointed->getName() << " (" << seed << ")\n";
        } else {
            float baseIncrement = socialLinkIncrement(tier, rng);
            float qualityMult = 2.8f;
            // Personality multiplier: extraverts build bonds faster
            float personalityMult = 0.6f + (pointer->personality.extraversion / 100.0f) * 0.8f;
            qualityMult *= personalityMult;
            float increment = baseIncrement * qualityMult;
            pointer->list_entityPointedSocial[index].social = std::min(100.0f, pointer->list_entityPointedSocial[index].social + increment);
            int pidx = pointed->contains(pointed->list_entityPointedSocial, pointer, 4);
            if (pidx == -1) {
                pointed->addSocial({ 1, pointer, increment * 0.65f });
            } else {
                pointed->list_entityPointedSocial[pidx].social = std::min(100.0f, pointed->list_entityPointedSocial[pidx].social + increment * 0.65f);
            }
            ASHB_TRACE_STREAM << "GoodConnection: bond deepened " << pointer->getName() << " -> " << pointed->getName()
                      << " +" << increment << " (tier=" << tier << ", total=" << pointer->list_entityPointedSocial[index].social << ")\n";
        }
        pointed->onMajorEventAddOrBoostGoal("good_connection");
        pointer->onMajorEventAddOrBoostGoal("good_connection");
    }
    else if (action->name == "breeding") {
        // Kin avoidance: parents/children and (half-)siblings never conceive.
        // Lineage is tracked by id, so this holds even after the entity vector
        // reallocates. Cousins are intentionally permitted.
        if (globalKinship && KinshipSystem::wouldBeIncest(*pointer, *pointed)) {
            ASHB_TRACE_STREAM << "Reproduction bloque: " << pointer->getName() << " et "
                      << pointed->getName() << " sont parents proches\n";
            return;
        }

        // No children are born across the front line of a war: members of two
        // warring tribes will not breed with one another.
        if (globalCivEngine && pointer->tribeId != pointed->tribeId &&
            globalCivEngine->areTribesAtWar(pointer->tribeId, pointed->tribeId)) {
            ASHB_TRACE_STREAM << "Reproduction bloque: " << pointer->getName() << " et "
                      << pointed->getName() << " sont de tribus en guerre\n";
            return;
        }

        int desire_index = pointer->contains(pointer->list_entityPointedDesire, pointed, 1);

        // FIX: Create relationship entry if it doesn't exist
        if (desire_index == -1) {
            entityPointedDesire newDesire;
            newDesire.Id = pointed->entityId;
            newDesire.pointedEntity = pointed;
            newDesire.desire = 0.0f;
            pointer->list_entityPointedDesire.push_back(newDesire);
            desire_index = static_cast<int>(pointer->list_entityPointedDesire.size()) - 1;
        }

        if (pointer->list_entityPointedDesire[desire_index].desire < 18) {
            float current_desire = pointer->list_entityPointedDesire[desire_index].desire;
            if (!pointer || !pointed) {
                return;
            }
            ASHB_TRACE_STREAM << "Couple bloque: " << pointer->getName() << " n'a pas assez de desir pour " << pointed->getName()
                      << " (" << current_desire << " < 18)\n";

            // Desire grows toward the fertility threshold. Courtship still takes a
            // few interactions, but not so many that lineages die out faster than
            // they form.
            pointer->list_entityPointedDesire[desire_index].desire = std::min(100.0f,
                pointer->list_entityPointedDesire[desire_index].desire + BetterRand::genNrInInterval(6, 11));
            return;
        }

        int pointed_desire_index = pointed->contains(pointed->list_entityPointedDesire, pointer, 1);

        // FIX: Create relationship entry if it doesn't exist
        if (pointed_desire_index == -1) {
            entityPointedDesire newDesire;
            newDesire.Id = pointer->entityId;
            newDesire.pointedEntity = pointer;
            newDesire.desire = 0.0f;
            pointed->list_entityPointedDesire.push_back(newDesire);
            pointed_desire_index = static_cast<int>(pointed->list_entityPointedDesire.size()) - 1;
        }

        if (pointed->list_entityPointedDesire[pointed_desire_index].desire < 18) {
            float pointed_desire = pointed->list_entityPointedDesire[pointed_desire_index].desire;
            if (!pointer || !pointed) {
                return;
            }
            ASHB_TRACE_STREAM << "Couple bloque: " << pointed->getName() << " n'a pas assez de desir pour " << pointer->getName()
                      << " (" << pointed_desire << " < 18)\n";

            pointed->list_entityPointedDesire[pointed_desire_index].desire = std::min(100.0f,
                pointed->list_entityPointedDesire[pointed_desire_index].desire + BetterRand::genNrInInterval(6, 11));

            return;
        }

        int anger_index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (anger_index != -1 && pointer->list_entityPointedAnger[anger_index].anger > 30) {
            if (!pointer || !pointed) {
                return;
            }
            ASHB_TRACE_STREAM << "Reproduction bloque: " << pointer->getName() << " a trop de colere envers " << pointed->getName() << "\n";
            // FIX: Reduce anger instead of increasing desire incorrectly
            pointer->list_entityPointedAnger[anger_index].anger -= BetterRand::genNrInInterval(5, 10);
            return;
        }

        int pointed_anger_index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
        if (pointed_anger_index != -1 && pointed->list_entityPointedAnger[pointed_anger_index].anger > 10) {
            if (!pointer || !pointed) {
                return;
            }
            ASHB_TRACE_STREAM << "Reproduction bloque: " << pointed->getName() << " a trop de colere envers " << pointer->getName() << "\n";
            pointed->list_entityPointedAnger[pointed_anger_index].anger -= BetterRand::genNrInInterval(3, 8);
            return;
        }
        if (pointer->entityAge < 15 || pointed->entityAge < 15) {
          ASHB_TRACE_STREAM << "cannot have children under the age of 18";
          return ;
        }
        // Fertility wanes with age — elders rarely conceive.
        if (pointer->entityAge > 60 || pointed->entityAge > 60) {
          ASHB_TRACE_STREAM << "too old to have children\n";
          return ;
        }

        if (pointer->checkCouple(pointed)) {

            LifeMemory mem;
            mem.eventType = "breeding";
            mem.entityInvolvedId = pointed->entityId;
            mem.emotionalIntensity = 2.2f;
            mem.isFormative = (pointer->lifeMemories.size() < 3);
            mem.internalNarrative = "trusted someone and got betrayed";
            pointer->lifeMemories.push_back(mem);

            pointer->personality.agreeableness += 11.0f;
            pointer->ValueSystem.collectivism += 6.0f;
            pointer->personality.neuroticism -= 3.0f;
            pointed->personality.agreeableness += 11.0f;
            pointed->ValueSystem.collectivism += 6.0f;
            pointed->personality.neuroticism -= 4.0f;

            pointer->onMajorEventAddOrBoostGoal("reproduction");
            pointed->onMajorEventAddOrBoostGoal("reproduction");

            ASHB_TRACE_STREAM << "@@@@@@ Nouvelle reproduction  entre: (" << pointer->getId() << ")" << pointer->getName()
                      << " -> (" << pointed->getId() << ")" << pointed->getName() << std::endl;


            if (globalLogger) globalLogger->logEvent("breeding", "Reproduction between " + pointer->name + " and " + pointed->name);


            static int nextBabyId = 1000;
            Entity baby = Entity(nextBabyId++, 0, 75, 85, 0, 100, "", 10, 0, 0, 75, 'A', 0, 75, -1, "happiness");

            if (engineCivilization) {
                engineCivilization->logEvent(-1, baby.getName() + " was born to "
                    + pointer->name + " and " + pointed->name, "birth");
                engineCivilization->totalBirths++;
            }
            if (globalLogger) globalLogger->logBirth(baby.entityId, baby.getName(), pointer->getId(), pointed->getId(), pointer->getName(), pointed->getName());
            baby.posX = pointer->posX + BetterRand::genNrInInterval(-15, 15);
            baby.posY = pointer->posY + BetterRand::genNrInInterval(-15, 15);
            baby.parent1 = pointed;
            baby.parent2 = pointer;
            // Inherit homeland so lineages stay tied to their cradle.
            baby.originRegionId = (pointer->originRegionId >= 0) ? pointer->originRegionId
                                                                 : pointed->originRegionId;
            // Inherit the parents' tribe so the newborn appears in their cluster /
            // tribe view immediately, instead of drifting in the grey "no tribe" pool.
            baby.tribeId = (pointer->tribeId >= 0) ? pointer->tribeId : pointed->tribeId;
            // M7: enroll in the tribe's member roll too. tribeId alone is
            // invisible to Tribe::population(), so tribes "hollowed out" as
            // founders aged and dissolved even while dozens of living members
            // carried their id — this was the late-run tribes→0 collapse.
            if (baby.tribeId >= 0 && globalCivEngine)
                if (Tribe* bt = globalCivEngine->findTribe(baby.tribeId))
                    bt->memberIds.push_back(baby.entityId);
            // Name the child in the family's language.
            if (g_lexicon) baby.name = g_lexicon->genName(baby.originRegionId, baby.entitySex);

            //add pheromone to eahcentity
            pointer->pheromone.type = "breeding";
            pointer->pheromone.releasing_level = BetterRand::genNrInInterval(20.0, 50.0);

            pointed->pheromone.type = "breeding";
            pointed->pheromone.releasing_level = BetterRand::genNrInInterval(20.0, 50.0);

            baby.pheromone.type = "procreation_simulation";
            baby.pheromone.releasing_level = BetterRand::genNrInInterval(40.0, 70.0);


            if (anger_index != -1 && pointer->list_entityPointedAnger[anger_index].anger > 10) {
                baby.dv.childhoodTraumaScore = pointer->list_entityPointedAnger[anger_index].anger;
                baby.dv.childhoodNurturingScore = 0.0f;
                baby.dv.hadSecureAttachment = false;
            } else {
                baby.dv.childhoodNurturingScore = pointer->list_entityPointedDesire[desire_index].desire / 25.0f;
                baby.dv.childhoodTraumaScore = 0.0f;
                baby.dv.hadSecureAttachment = true;
            }
            baby.addOrBoostGoal("self", 100.0f);

            // Inherit averaged parent traits BEFORE storing the baby — previously
            // push_back copied the baby first, so these inherited values were
            // silently thrown away.
            float avg_openness_parent = (pointed->personality.openness + pointer->personality.openness) / 2;
            float avg_extraversion_parent = (pointed->personality.extraversion + pointer->personality.extraversion) / 2;
            baby.personality.openness = avg_openness_parent;
            baby.personality.extraversion = avg_extraversion_parent;
            inheritEmergenceTraits(baby, pointer, pointed);
            if (globalKinship) globalKinship->registerBirth(baby, pointed, pointer,
                globalCivEngine ? globalCivEngine->getCurrentYear() : 0);

            new_borns.push_back(baby);
        } else {
            // Not a formal couple yet. If both already desire each other strongly,
            // they pair up AND conceive now; otherwise just form the bond so the
            // relationship can keep growing toward a child next time.
            ASHB_TRACE_STREAM << "INFO: couple doesnt exist, creating a new one\n";
            pointer->addCouple({ 1, pointed });
            pointed->addCouple({ 1, pointer });

            float dHere  = pointer->list_entityPointedDesire[desire_index].desire;
            float dThere = pointed->list_entityPointedDesire[pointed_desire_index].desire;
            // Conceiving on the spot now demands a genuinely strong mutual bond and
            // both partners being of fertile age — no more snap pregnancies.
            // Returning-soldier effect: a tribe fresh out of a war enters a baby
            // boom, so its couples conceive on a weaker spark for a while.
            // https://en.wikipedia.org/wiki/Returning_soldier_effect
            float boomBoost = 1.0f;
            if (globalCivEngine) {
                int boomTribe = (pointer->tribeId >= 0) ? pointer->tribeId : pointed->tribeId;
                boomBoost = globalCivEngine->postWarBirthBoost(boomTribe, FreeWillSystem::day / 60);
            }
            float desireGate = 35.0f / boomBoost;   // 1.0 normally; <35 during a post-war boom
            if (dHere >= desireGate && dThere >= desireGate &&
                pointer->entityAge >= 18 && pointed->entityAge >= 18 &&
                pointer->entityAge <= 55 && pointed->entityAge <= 55) {
                static int nextBabyId2 = 5000;
                Entity baby = Entity(nextBabyId2++, 0, 75, 85, 0, 100, "", 10, 0, 0, 75, 'A', 0, 75, -1, "happiness");
                baby.posX = pointer->posX + BetterRand::genNrInInterval(-15, 15);
                baby.posY = pointer->posY + BetterRand::genNrInInterval(-15, 15);
                baby.parent1 = pointed;
                baby.parent2 = pointer;
                baby.originRegionId = (pointer->originRegionId >= 0) ? pointer->originRegionId
                                                                     : pointed->originRegionId;
                baby.tribeId = (pointer->tribeId >= 0) ? pointer->tribeId : pointed->tribeId;
                if (baby.tribeId >= 0 && globalCivEngine)
                    if (Tribe* bt = globalCivEngine->findTribe(baby.tribeId))
                        bt->memberIds.push_back(baby.entityId);
                if (g_lexicon) baby.name = g_lexicon->genName(baby.originRegionId, baby.entitySex);
                baby.personality.openness     = (pointed->personality.openness + pointer->personality.openness) / 2;
                baby.personality.extraversion = (pointed->personality.extraversion + pointer->personality.extraversion) / 2;
                baby.dv.hadSecureAttachment = true;
                baby.addOrBoostGoal("self", 100.0f);
                inheritEmergenceTraits(baby, pointer, pointed);
                if (globalKinship) globalKinship->registerBirth(baby, pointed, pointer,
                    globalCivEngine ? globalCivEngine->getCurrentYear() : 0);
                if (globalCivEngine) {
                    globalCivEngine->logEvent(-1, baby.getName() + " was born to "
                        + pointer->name + " and " + pointed->name, "birth");
                    globalCivEngine->totalBirths++;
                }
                if (globalLogger) globalLogger->logBirth(baby.entityId, baby.getName(),
                    pointer->getId(), pointed->getId(), pointer->getName(), pointed->getName());
                new_borns.push_back(baby);
            }
        }
    }
    else if (action->name == "couple") {
        pointer->onMajorEventAddOrBoostGoal("couple");
        pointed->onMajorEventAddOrBoostGoal("couple");
        if (getSocialTier(pointer, pointed) < FAMILIAR) {
            ASHB_TRACE_STREAM << "Couple blocked: not familiar enough yet (" << pointer->getName() << " <-> " << pointed->getName() << ")\n";
            return;
        }
        float required_desire = 14.0f;
        if (pointer->dv.attachmentStyle == AVOIDANT) required_desire = 30.0f;
        else if (pointer->dv.attachmentStyle == ANXIOUS) required_desire = 10.0f;

        int desire_index = pointer->contains(pointer->list_entityPointedDesire, pointed, 1);

        // FIX: Create relationship entry if it doesn't exist
        if (desire_index == -1) {
            entityPointedDesire newDesire;
            newDesire.Id = pointed->entityId;
            newDesire.pointedEntity = pointed;
            newDesire.desire = 0.0f;
            pointer->list_entityPointedDesire.push_back(newDesire);
            desire_index = static_cast<int>(pointer->list_entityPointedDesire.size()) - 1;
        }

        if (pointer->list_entityPointedDesire[desire_index].desire < required_desire) {
            float current_desire = pointer->list_entityPointedDesire[desire_index].desire;
            ASHB_TRACE_STREAM << "Couple bloqué: " << pointer->getName() << " n'a pas assez de désir pour " << pointed->getName()
                      << " (" << current_desire << " < " << required_desire << ")\n";
            pointer->list_entityPointedDesire[desire_index].desire = std::min(100.0f,
                pointer->list_entityPointedDesire[desire_index].desire + BetterRand::genNrInInterval(3, 8));
            return;
        }

        float required_pointed_desire = 15.0f;
        if (pointed->dv.attachmentStyle == AVOIDANT) required_pointed_desire = 35.0f;
        else if (pointed->dv.attachmentStyle == ANXIOUS) required_pointed_desire = 10.0f;

        int pointed_desire_index = pointed->contains(pointed->list_entityPointedDesire, pointer, 1);

        // FIX: Create relationship entry if it doesn't exist
        if (pointed_desire_index == -1) {
            entityPointedDesire newDesire;
            newDesire.Id = pointer->entityId;
            newDesire.pointedEntity = pointer;
            newDesire.desire = 0.0f;
            pointed->list_entityPointedDesire.push_back(newDesire);
            pointed_desire_index = static_cast<int>(pointed->list_entityPointedDesire.size()) - 1;
        }

        if (pointed->list_entityPointedDesire[pointed_desire_index].desire < required_pointed_desire) {
            float pointed_desire = pointed->list_entityPointedDesire[pointed_desire_index].desire;
            ASHB_TRACE_STREAM << "Couple bloqué: " << pointed->getName() << " n'a pas assez de désir pour " << pointer->getName()
                      << " (" << pointed_desire << " < " << required_pointed_desire << ")\n";
            // FIX: Use correct index on pointed entity
            pointed->list_entityPointedDesire[pointed_desire_index].desire = std::min(100.0f,
                pointed->list_entityPointedDesire[pointed_desire_index].desire + BetterRand::genNrInInterval(3, 8));
            return;
        }

        int anger_index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (anger_index != -1 && pointer->list_entityPointedAnger[anger_index].anger > 10) {
            ASHB_TRACE_STREAM << "Couple bloqué: " << pointer->getName() << " a trop de colère envers " << pointed->getName() << "\n";
            // FIX: Reduce anger instead of incorrectly modifying desire
            pointer->list_entityPointedAnger[anger_index].anger -= BetterRand::genNrInInterval(5, 10);
            return;
        }

        int pointed_anger_index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
        if (pointed_anger_index != -1 && pointed->list_entityPointedAnger[pointed_anger_index].anger > 10) {
            ASHB_TRACE_STREAM << "Couple bloqué: " << pointed->getName() << " a trop de colère envers " << pointer->getName() << "\n";
            // FIX: Reduce anger instead of incorrectly modifying desire
            pointed->list_entityPointedAnger[pointed_anger_index].anger -= BetterRand::genNrInInterval(3, 8);
            return;
        }

        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);

        // FIX: Create social entry if it doesn't exist
        if (social_index == -1) {
            entityPointedSocial newSocial;
            newSocial.Id = pointed->entityId;
            newSocial.pointedEntity = pointed;
            newSocial.social = 0.0f;
            pointer->list_entityPointedSocial.push_back(newSocial);
            social_index = static_cast<int>(pointer->list_entityPointedSocial.size()) - 1;
        }

        if (pointer->list_entityPointedSocial[social_index].social < 15) {
            float current_social = pointer->list_entityPointedSocial[social_index].social;
            ASHB_TRACE_STREAM << "Couple bloqué: lien social insuffisant entre " << pointer->getName() << " et " << pointed->getName()
                      << " (" << current_social << " < 15)\n";
            // FIX: Increase social not desire
            pointer->list_entityPointedSocial[social_index].social = std::min(100.0f,
                pointer->list_entityPointedSocial[social_index].social + BetterRand::genNrInInterval(3, 7));
            return;
        }
        int index = pointer->contains(pointer->list_entityPointedCouple, pointed, 3);
        if (index == -1) {
            LifeMemory mem;
            mem.eventType = "couple";
            mem.entityInvolvedId = pointed->entityId;
            mem.emotionalIntensity = 1.0f;
            mem.isFormative = true;
            mem.internalNarrative = "Entered in a couple";
            pointer->lifeMemories.push_back(mem);
            for (LifeGoal& goal : pointer->m_goals) {
                if (goal.type == "find_partner") {
                    pointer->ValueSystem.achievementDrive += 8.0f;
                    goal.progressToward += 8.0f;
                    pointed->Esteem += 8.0f;
                    pointer->Esteem += 8.0f;
                }
            }
            pointer->ValueSystem.collectivism += 1.5f;
            pointer->ValueSystem.familyOrientation += 0.7f;
            pointer->ValueSystem.hedonism += 0.4f;
            ASHB_TRACE_STREAM << "Nouveau couple ajouté entre: (" << pointer->getId() << ")" << pointer->getName()
                      << " -> (" << pointed->getId() << ")" << pointed->getName() << std::endl;
            if (globalLogger) globalLogger->logRelationship(pointer->entityId, pointer->name, pointed->entityId, pointed->name, "couple formed");
            pointer->addCouple({ 1, pointed });
            pointed->addCouple({ 1, pointer });
        } else {
            ASHB_TRACE_STREAM << "INFO: Couple existe déjà, renforcement du lien\n";
        }
    }
    else if (action->name == "Murder") {
        LifeMemory mem;
        mem.eventType = "Murder";
        mem.entityInvolvedId = pointed->entityId;
        mem.emotionalIntensity = 2.1f;
        mem.isFormative = true;
        mem.internalNarrative = "I murdered someone";
        pointer->lifeMemories.push_back(mem);
        pointer->personality.neuroticism += 3.2f;
        pointer->ValueSystem.collectivism -= 1.2f;
        ASHB_TRACE_STREAM << "--- MURDER: (" << pointer->getId() << ")" << pointer->getName() << " a tué (" << pointed->getId() << ")" << pointed->getName() << std::endl;
        if (pointed->searchConnAng(pointer) > 40.0 || pointed->personality.neuroticism > 50.0 || pointed->entityMentalHealth < 30) {
            pointed->entityHealth = 0.0f;
        }
        engineCivilization->logEvent(-1,pointer->getName() + " Murdered " + pointed->getName() , "birth");
    }
    else if (action->name == "Discrimination") {
        int index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (index == -1) {
            float anger = static_cast<float>(BetterRand::genNrInInterval(3, 8));
            ASHB_TRACE_STREAM << " /// Discrimination: lien anger ajouté entre: (" << pointer->getId() << ")" << pointer->getName()
                      << " -> (" << pointed->getId() << ")" << pointed->getName() << " " << anger << std::endl;
            pointer->addAnger({ 1, pointed, anger });
        } else {
            float increment = static_cast<float>(BetterRand::genNrInInterval(2, 6));
            pointer->list_entityPointedAnger[index].anger += increment;
            ASHB_TRACE_STREAM << "// Discrimination renforcée entre: (" << pointer->getId() << ")" << pointer->getName()
                      << " -> (" << pointed->getId() << ")" << pointed->getName() << " +" << increment << std::endl;
        }
    }
    else if (action->name == "Gossip") {
        if (BetterRand::genNrInInterval(1, 100) <= 30) {
            int index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
            if (index == -1) {
                float anger = static_cast<float>(BetterRand::genNrInInterval(2, 7));
                ASHB_TRACE_STREAM << " ++ Gossip découvert! " << pointed->getName() << " ajoute anger envers " << pointer->getName() << " +" << anger << std::endl;
                pointed->addAnger({ 1, pointer, anger });
            } else {
                float increment = static_cast<float>(BetterRand::genNrInInterval(4, 10));
                pointed->list_entityPointedAnger[index].anger += increment;
                ASHB_TRACE_STREAM << "++ Gossip découvert! " << pointed->getName() << " augmente anger envers " << pointer->getName() << " +" << increment << std::endl;
            }
        } else {
            ASHB_TRACE_STREAM << "++ Gossip: " << pointer->getName() << " parle de " << pointed->getName() << " (non découvert)\n";
        }
        // M5: gossip transfers BELIEFS, not just anger. The gossiper's opinion
        // of the subject (`pointed`) propagates to their bonded contacts at
        // hearsay accuracy — listeners update the subject's reputation and
        // their own mental model of the subject without ever having met them.
        // This is how a reputation gets assassinated by someone the victim
        // never wronged. (The old code wrote the gossiper's SELF-model into
        // the subject's own ledger — no third party ever learned anything.)
        {
            auto clampf = [](float v, float lo, float hi) {
                return v < lo ? lo : (v > hi ? hi : v);
            };
            MentalModelOfOther* gossiperView = pointer->getModelOf(pointed);
            float angerAt  = std::max(0.0f, pointer->searchConnAng(pointed));
            float bondWith = std::max(0.0f, pointer->searchConnSocial(pointed));
            // −1 (smear) … +1 (praise): resentment colors the telling more
            // strongly than friendship does.
            float opinion = clampf((bondWith - angerAt * 1.5f) / 100.0f, -1.0f, 1.0f);

            int audience = 0;
            for (const auto& s : pointer->list_entityPointedSocial) {
                Entity* listener = s.pointedEntity;
                if (!listener || listener == pointed || listener == pointer ||
                    listener->entityHealth <= 0.0f || s.social < 10.0f) continue;
                if (++audience > 4) break;   // a whisper network, not a broadcast

                const float noise = 0.4f;    // hearsay distorts
                auto& rep = listener->reputationMap[pointed->entityId];
                rep.positiveScore = clampf(rep.positiveScore + opinion * 8.0f * (1.0f - noise), 0.0f, 100.0f);
                rep.negativeScore = clampf(rep.negativeScore - opinion * 8.0f * (1.0f - noise), 0.0f, 100.0f);
                rep.timesGossipedAbout++;

                // Secondhand mental model: forms (or drifts) from the
                // gossiper's view at a fraction of firsthand accuracy.
                MentalModelOfOther* lm = listener->getModelOf(pointed);
                if (!lm && listener->list_MentalModelOfOther.size() < 16) {
                    lm = new MentalModelOfOther();
                    lm->entityPointed = pointed;
                    listener->list_MentalModelOfOther.push_back(lm);
                }
                if (lm) {
                    const float secondhand = 0.15f;
                    if (gossiperView) {
                        lm->perceivedExtraversion  += (gossiperView->perceivedExtraversion  - lm->perceivedExtraversion)  * secondhand;
                        lm->perceivedAgreeableness += (gossiperView->perceivedAgreeableness - lm->perceivedAgreeableness) * secondhand;
                        lm->perceivedNeuroticism   += (gossiperView->perceivedNeuroticism   - lm->perceivedNeuroticism)   * secondhand;
                        lm->estimatedHappiness     += (gossiperView->estimatedHappiness     - lm->estimatedHappiness)     * secondhand;
                    }
                    lm->trustLevel      = clampf(lm->trustLevel + opinion * 5.0f, 0.0f, 100.0f);
                    lm->confidence      = std::min(1.0f, lm->confidence + 0.05f); // hearsay barely counts
                    lm->lastObservedDay = day;   // "heard news of them" resets staleness
                }
            }
        }
    }
    else if (action->name == "Apologize") {
        int index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (index != -1) {
            float reduction = static_cast<float>(BetterRand::genNrInInterval(5, 12));
            pointer->list_entityPointedAnger[index].anger = std::max(0.0f, pointer->list_entityPointedAnger[index].anger - reduction);
            ASHB_TRACE_STREAM << "Apologize: " << pointer->getName() << " réduit anger envers " << pointed->getName() << " -" << reduction << std::endl;
        }
        int pointed_index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
        if (pointed_index != -1) {
            float reduction = static_cast<float>(BetterRand::genNrInInterval(4, 8));
            pointed->list_entityPointedAnger[pointed_index].anger = std::max(0.0f, pointed->list_entityPointedAnger[pointed_index].anger - reduction);
            ASHB_TRACE_STREAM << "Apologize: " << pointed->getName() << " accepte excuses, anger réduit -" << reduction << std::endl;
        }
    }
    else if (action->name == "HelpSupport") {
        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (social_index == -1) {
            float social = static_cast<float>(BetterRand::genNrInInterval(3, 7));
            ASHB_TRACE_STREAM << "HelpSupport: nouveau lien social entre " << pointer->getName() << " et " << pointed->getName() << " +" << social << std::endl;
            pointer->addSocial({ 1, pointed, social });
        } else {
            float increment = static_cast<float>(BetterRand::genNrInInterval(3, 7));
            pointer->list_entityPointedSocial[social_index].social += increment;
            ASHB_TRACE_STREAM << "HelpSupport: lien social renforcé entre " << pointer->getName() << " et " << pointed->getName() << " +" << increment << std::endl;
        }
        int pointed_social_index = pointed->contains(pointed->list_entityPointedSocial, pointer, 4);
        if (pointed_social_index == -1) {
            float social = static_cast<float>(BetterRand::genNrInInterval(2, 7));
            pointed->addSocial({ 1, pointer, social });
        } else {
            float increment = static_cast<float>(BetterRand::genNrInInterval(2, 6));
            pointed->list_entityPointedSocial[pointed_social_index].social += increment;
        }
    }
    else if (action->name == "IgnoreAvoid") {
        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (social_index != -1) {
            float reduction = static_cast<float>(BetterRand::genNrInInterval(2, 7));
            pointer->list_entityPointedSocial[social_index].social = std::max(0.0f, pointer->list_entityPointedSocial[social_index].social - reduction);
            ASHB_TRACE_STREAM << "IgnoreAvoid: " << pointer->getName() << " évite " << pointed->getName() << ", lien social -" << reduction << std::endl;
        }
    }
    else if (action->name == "Insult") {
        int index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (index == -1) {
            float anger = static_cast<float>(BetterRand::genNrInInterval(2, 6));
            pointer->addAnger({ 1, pointed, anger });
        }
        int pointed_index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
        if (pointed_index == -1) {
            float anger = static_cast<float>(BetterRand::genNrInInterval(5, 11));
            ASHB_TRACE_STREAM << "Insult: " << pointed->getName() << " devient angry envers " << pointer->getName() << " +" << anger << std::endl;
            pointed->addAnger({ 1, pointer, anger });
        } else {
            float increment = static_cast<float>(BetterRand::genNrInInterval(4, 9));
            pointed->list_entityPointedAnger[pointed_index].anger += increment;
            ASHB_TRACE_STREAM << "Insult: " << pointed->getName() << " augmente anger envers " << pointer->getName() << " +" << increment << std::endl;
        }
        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (social_index != -1) {
            pointer->list_entityPointedSocial[social_index].social = std::max(0.0f, pointer->list_entityPointedSocial[social_index].social - 3.0f);
        }
    }
    else if (action->name == "Manipulate") {
        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (social_index == -1) {
            float social = static_cast<float>(BetterRand::genNrInInterval(1, 5));
            pointer->addSocial({ 1, pointed, social });
        }
        if (BetterRand::genNrInInterval(1, 100) <= 40) {
            int pointed_anger_index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
            if (pointed_anger_index == -1) {
                float anger = static_cast<float>(BetterRand::genNrInInterval(1, 5));
                pointed->addAnger({ 1, pointer, anger });
                ASHB_TRACE_STREAM << "Manipulate: " << pointed->getName() << " ressent manipulation, anger +" << anger << std::endl;
            } else {
                float increment = static_cast<float>(BetterRand::genNrInInterval(1, 5));
                pointed->list_entityPointedAnger[pointed_anger_index].anger += increment;
            }
        }
    }
    else if (action->name == "Jealousy") {
        int index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (index == -1) {
            float anger = static_cast<float>(BetterRand::genNrInInterval(5, 10));
            ASHB_TRACE_STREAM << "Jealousy: " << pointer->getName() << " envieux de " << pointed->getName() << ", anger +" << anger << std::endl;
            pointer->addAnger({ 1, pointed, anger });
        } else {
            float increment = static_cast<float>(BetterRand::genNrInInterval(4, 8));
            pointer->list_entityPointedAnger[index].anger += increment;
            ASHB_TRACE_STREAM << "Jealousy: " << pointer->getName() << " renforce jalousie envers " << pointed->getName() << " +" << increment << std::endl;
        }
    }
    else if (action->name == "Betray") {
        pointer->onMajorEventAddOrBoostGoal("betrayal");
        LifeMemory mem;
        mem.eventType = "betrayal";
        mem.entityInvolvedId = pointed->entityId;
        mem.emotionalIntensity = 0.8f;
        mem.isFormative = (pointer->lifeMemories.size() < 3);
        mem.internalNarrative = "trusted someone and got betrayed";
        pointer->lifeMemories.push_back(mem);
        if (mem.isFormative) {
            pointer->personality.agreeableness -= 5.0f;
            pointer->ValueSystem.collectivism -= 8.0f;
        }
        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (social_index != -1) {
            ASHB_TRACE_STREAM << "Betray: Lien social détruit entre " << pointer->getName() << " et " << pointed->getName() << std::endl;
            pointer->list_entityPointedSocial.erase(pointer->list_entityPointedSocial.begin() + social_index);
        }
        int pointed_anger_index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
        if (pointed_anger_index == -1) {
            float anger = static_cast<float>(BetterRand::genNrInInterval(15, 25));
            ASHB_TRACE_STREAM << "Betray: " << pointed->getName() << " trahi par " << pointer->getName() << ", anger +" << anger << std::endl;
            pointed->addAnger({ 1, pointer, anger });
        } else {
            float increment = static_cast<float>(BetterRand::genNrInInterval(12, 25));
            pointed->list_entityPointedAnger[pointed_anger_index].anger += increment;
        }
        int pointed_social_index = pointed->contains(pointed->list_entityPointedSocial, pointer, 4);
        if (pointed_social_index != -1) {
            pointed->list_entityPointedSocial.erase(pointed->list_entityPointedSocial.begin() + pointed_social_index);
        }
    }
    else if (action->name == "Flirt") {
        if (pointer->entityHygiene < 30) {
            ASHB_TRACE_STREAM << "Flirt bloqué: " << pointer->getName() << " a une hygiène trop basse pour flirter\n";
            return;
        }
        float desire = static_cast<float>(BetterRand::genNrInInterval(3, 6));
        int index = pointer->contains(pointer->list_entityPointedDesire, pointed, 1);
        if (index == -1) {
            ASHB_TRACE_STREAM << "Flirt: nouveau désir faible entre " << pointer->getName() << " et " << pointed->getName() << " +" << desire << std::endl;
            pointer->addDesire({ 1, pointed, desire });
        } else {
            float increment = static_cast<float>(BetterRand::genNrInInterval(1, 2));
            pointer->list_entityPointedDesire[index].desire += increment;
        }
        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (social_index == -1) {
            pointer->addSocial({ 1, pointed, desire });
        }
    }
    else if (action->name == "Date") {
        int desire_index = pointer->contains(pointer->list_entityPointedDesire, pointed, 1);
        if (desire_index == -1 || pointer->list_entityPointedDesire[desire_index].desire < 15) {
            if (!pointed || !pointer){
                return ;
            }
            ASHB_TRACE_STREAM << "Date bloqué: pas assez de désir entre " << pointer->getName() << " et " << pointed->getName() << std::endl;
            return;
        }
        if (pointer->dv.attachmentStyle == AVOIDANT && pointer->list_entityPointedDesire[desire_index].desire > 40.0f) {
            float flee_reduction = static_cast<float>(BetterRand::genNrInInterval(5, 15));
            pointer->list_entityPointedDesire[desire_index].desire = std::max(0.0f, pointer->list_entityPointedDesire[desire_index].desire - flee_reduction);
            pointer->entityStress = std::min(100.0f, pointer->entityStress + 10.0f);
            ASHB_TRACE_STREAM << "Date annulé: " << pointer->getName() << " (Avoidant) fuit l'intimité! Désir réduit -" << flee_reduction << std::endl;
            return;
        }
        if (pointer->entityHygiene < 45) {
            ASHB_TRACE_STREAM << "Date bloqué: " << pointer->getName() << " a une hygiène trop basse\n";
            return;
        }
        float desire_increment = static_cast<float>(BetterRand::genNrInInterval(3, 6));
        pointer->list_entityPointedDesire[desire_index].desire += desire_increment;
        ASHB_TRACE_STREAM << "Date: désir renforcé entre " << pointer->getName() << " et " << pointed->getName() << " +" << desire_increment << std::endl;
        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (social_index == -1) {
            pointer->addSocial({ 1, pointed, static_cast<float>(BetterRand::genNrInInterval(3, 6)) });
        } else {
            pointer->list_entityPointedSocial[social_index].social += static_cast<float>(BetterRand::genNrInInterval(3, 6));
        }
        int pointed_desire_index = pointed->contains(pointed->list_entityPointedDesire, pointer, 1);
        if (pointed_desire_index == -1) {
            pointed->addDesire({ 1, pointer, static_cast<float>(BetterRand::genNrInInterval(3, 6)) });
        } else {
            pointed->list_entityPointedDesire[pointed_desire_index].desire += static_cast<float>(BetterRand::genNrInInterval(2, 5));
        }
    }
    else if (action->name == "BreakUp") {
        LifeMemory mem;
        mem.eventType = "BreakUp";
        mem.entityInvolvedId = pointer->entityId;
        mem.emotionalIntensity = 0.8f;
        mem.isFormative = (pointed->lifeMemories.size() < 3);
        mem.internalNarrative = "I breaked up with someone that I loved";
        pointed->lifeMemories.push_back(mem);
        if (mem.isFormative) {
            pointed->personality.agreeableness -= 2.0f;
            pointed->ValueSystem.collectivism -= 4.0f;
        }
        int couple_index = pointer->contains(pointer->list_entityPointedCouple, pointed, 3);
        if (couple_index == -1) {
            ASHB_TRACE_STREAM << "BreakUp bloqué: pas de couple entre " << pointer->getName() << " et " << pointed->getName() << std::endl;
            return;
        }
        ASHB_TRACE_STREAM << "BreakUp: Couple détruit entre " << pointer->getName() << " et " << pointed->getName() << std::endl;
        pointer->list_entityPointedCouple.erase(pointer->list_entityPointedCouple.begin() + couple_index);
        int pointed_couple_index = pointed->contains(pointed->list_entityPointedCouple, pointer, 3);
        if (pointed_couple_index != -1) {
            pointed->list_entityPointedCouple.erase(pointed->list_entityPointedCouple.begin() + pointed_couple_index);
        }
        int desire_index = pointer->contains(pointer->list_entityPointedDesire, pointed, 1);
        if (desire_index != -1) {
            pointer->list_entityPointedDesire[desire_index].desire *= 0.3f;
        }
        int pointed_anger_index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
        if (pointed_anger_index == -1) {
            float anger = static_cast<float>(BetterRand::genNrInInterval(5, 15));
            pointed->addAnger({ 1, pointer, anger });
        } else {
            pointed->list_entityPointedAnger[pointed_anger_index].anger += static_cast<float>(BetterRand::genNrInInterval(5, 12));
        }
        float breakerIntensity = 0.4f + BetterRand::genNrInInterval(0, 20) / 100.0f;
        float brokenpIntensity = 0.6f + BetterRand::genNrInInterval(0, 20) / 100.0f;
        if (pointer->dv.attachmentStyle == ANXIOUS) breakerIntensity = std::min(1.0f, breakerIntensity + 0.4f);
        if (pointer->dv.attachmentStyle == AVOIDANT) breakerIntensity = std::max(0.0f, breakerIntensity - 0.2f);
        if (pointed->dv.attachmentStyle == ANXIOUS) brokenpIntensity = std::min(1.0f, brokenpIntensity + 0.4f);
        if (pointed->dv.attachmentStyle == AVOIDANT) brokenpIntensity = std::max(0.0f, brokenpIntensity - 0.2f);
        pointer->addGrief(pointed->entityId, breakerIntensity, false);
        pointed->addGrief(pointer->entityId, brokenpIntensity, false);
    }
    else if (action->name == "Reconcile") {
        int anger_index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (anger_index != -1 && pointer->list_entityPointedAnger[anger_index].anger > 30) {
            ASHB_TRACE_STREAM << "Reconcile bloqué: trop de colère entre " << pointer->getName() << " et " << pointed->getName() << std::endl;
            return;
        }
        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (social_index == -1) {
            pointer->addSocial({ 1, pointed, static_cast<float>(BetterRand::genNrInInterval(3, 6)) });
        } else {
            pointer->list_entityPointedSocial[social_index].social += static_cast<float>(BetterRand::genNrInInterval(3, 5));
        }
        if (anger_index != -1) {
            pointer->list_entityPointedAnger[anger_index].anger = std::max(0.0f, pointer->list_entityPointedAnger[anger_index].anger - 10.0f);
        }
        int pointed_anger_index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
        if (pointed_anger_index != -1) {
            pointed->list_entityPointedAnger[pointed_anger_index].anger = std::max(0.0f, pointed->list_entityPointedAnger[pointed_anger_index].anger - 8.0f);
        }
        ASHB_TRACE_STREAM << "Reconcile: " << pointer->getName() << " et " << pointed->getName() << " se réconcilient\n";
    }
    else if (action->name == "SetBoundaries") {
        int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
        if (social_index != -1) {
            pointer->list_entityPointedSocial[social_index].social = std::max(0.0f, pointer->list_entityPointedSocial[social_index].social - 2.0f);
        }
        int anger_index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
        if (anger_index != -1) {
            pointer->list_entityPointedAnger[anger_index].anger = std::max(0.0f, pointer->list_entityPointedAnger[anger_index].anger - 5.0f);
        }
        ASHB_TRACE_STREAM << "SetBoundaries: " << pointer->getName() << " établit des limites avec " << pointed->getName() << std::endl;
    }
}

// Execute chosen action
void FreeWillSystem::executeAction(Entity* entity, Action*& action, const ActionContext& context, Entity* pointed) {
    ASHB_TRACE_STREAM << "\n=== Executing Action: " << action->name << " ===\n";
    // RL: capture the situation *before* the action so the outcome below can be
    // attributed to the right (state, action) pair.
    const std::string rlPreState = rlStateSignature(entity, context.numPeopleNearby);
    std::map<std::string, float> statsBefore = captureEntityStats(entity);
    //ASHB_TRACE_STREAM << "Stats Before:\n";
    //for (auto& [k, v] : statsBefore) ASHB_TRACE_STREAM << "  " << k << ": " << v << "\n";

    for (const auto& change : action->statChanges) {
        float currentValue = getEntityStat(entity, change.statName);
        float newValue = currentValue + BetterRand::genNrInInterval(change.changeValue - 2, change.changeValue + 2);
        setEntityStat(entity, change.statName, newValue);
        //ASHB_TRACE_STREAM << "  Changed " << change.statName << ": " << currentValue << " -> " << newValue << "\n";
    }

    auto needIt = needs.find(action->needCategory);
    if (needIt != needs.end()) {
        ASHB_TRACE_STREAM << "Satisfying need category: " << action->needCategory << "\n";
        needIt->second.satisfy(action->baseSatisfaction);
    }

    std::map<std::string, float> statsAfter = captureEntityStats(entity);
    //ASHB_TRACE_STREAM << "Stats After:\n";
    //for (auto& [k, v] : statsAfter) ASHB_TRACE_STREAM << "  " << k << ": " << v << "\n";
    //

    //check work action -> implements economics
    if(action->name == "Basic Manual Work"){
      entity->salary.earnMoney(BetterRand::genNrInInterval(100, 200));
      entity->foodStore = std::min(20.0f, entity->foodStore + 2.5f); // wages buy bread at market
      entity->entityHunger = std::max(0.0f, entity->entityHunger - 4.0f); // and a meal on the way home
    }

    // ── Subsistence: produce and consume food ────────────────────────────────
    // Food is a survival good first, a trade good second. Foraging/farming fills
    // the store; eating draws it down to beat back hunger. With an empty store,
    // "eating" only scrounges scraps, so starvation still bites.
    {
        const std::string& an = action->name;
        // Seasons and harvest luck scale how much food the land gives up. A clamp
        // keeps even a hard winter / drought from yielding literally nothing.
        float envMod = std::max(0.2f, g_seasonalFoodModifier);
        // Local resource pool: a fertile river valley yields more than barren
        // ground, and a region worked to exhaustion gives up less until it rests.
        // Harvesting draws the regional food stock down (see ResourceSystem).
        int   rid    = entity->originRegionId;
        float localAb = g_resources.abundance(rid, RES_FOOD);  // 0.6..1.3, 1.0 if no region
        auto landYield = [&](float gross) {
            // The land only surrenders what it has; if depleted, the harvester
            // still scrounges a fraction so a bad spot starves slowly, not instantly.
            float taken = g_resources.extract(rid, RES_FOOD, gross);
            return g_resources.valid(rid) ? std::max(taken, gross * 0.45f) : gross;
        };
        if (an == "Hunt") {
            // Game is less seasonal than crops — blend toward 1.0. The local
            // food chain scales the kill: a teeming region rewards the hunter,
            // an overhunted one yields little. Hunting then depletes the herd.
            float huntMod = 0.5f + 0.5f * envMod;
            float game = g_ecosystem.gameAbundance(rid);
            float gross = BetterRand::genNrInInterval(3, 7) * huntMod * (0.7f + 0.3f * localAb) * game;
            float got = landYield(gross);
            entity->foodStore = std::min(20.0f, entity->foodStore + got);
            entity->entityHunger = std::max(0.0f, entity->entityHunger - 14.0f); // eat fresh kill
            g_ecosystem.huntPressure(rid, got);
        } else if (an == "Gather") {
            float forage = g_ecosystem.forageAbundance(rid);
            float gross = BetterRand::genNrInInterval(2, 4) * envMod * localAb * forage;
            float got = landYield(gross);
            entity->foodStore = std::min(20.0f, entity->foodStore + got);
            entity->entityHunger = std::max(0.0f, entity->entityHunger - 9.0f);
            g_ecosystem.foragePressure(rid, got);
        } else if (an == "Farm") {
            float gross = BetterRand::genNrInInterval(4, 8) * envMod * localAb; // best sustained yield, most seasonal
            entity->foodStore = std::min(20.0f, entity->foodStore + landYield(gross));
        } else if (an == "EatMeal") {
            float eaten = std::min(entity->foodStore, 1.5f);
            entity->foodStore   -= eaten;
            entity->entityHunger = std::max(0.0f, entity->entityHunger - (eaten * 25.0f + 3.0f));
        }

        // ── Fatigue: physical labour tires you out; rest restores you ────────
        if (an == "Sleep")                    entity->fatigueLevel = std::max(0.0f, entity->fatigueLevel - 45.0f);
        else if (an == "Rest")                entity->fatigueLevel = std::max(0.0f, entity->fatigueLevel - 25.0f);
        else if (an == "Hunt" || an == "Farm" || an == "Build" || an == "Raid" ||
                 an == "Duel" || an == "Work on Project" || an == "Basic Manual Work")
            entity->fatigueLevel = std::min(100.0f, entity->fatigueLevel + BetterRand::genNrInInterval(8, 16));
        else if (an == "Gather")              entity->fatigueLevel = std::min(100.0f, entity->fatigueLevel + 6.0f);
    }


    float outcomeSuccess = calculateOutcomeSuccess(statsBefore, statsAfter);
    action->outcomeSuccess = outcomeSuccess;
    ASHB_TRACE_STREAM << "Outcome Success: " << outcomeSuccess << "\n";

    // RL: reinforce this (state, action) pair from the realised outcome. The
    // reward is the outcome success scaled to the Q-value range (0..100); the
    // post-action situation becomes the next state for bootstrapping.
    {
        float reward = outcomeSuccess * 100.0f;
        const std::string rlNextState = rlStateSignature(entity, context.numPeopleNearby);
        rlSystem.processExperience(entity, rlPreState, action->name, reward, rlNextState);
    }

    // Phase 3: sentiment-modulated emotional impact on pointed target
    if (pointed != nullptr) {
        int   sentiment = getActionSentiment(action->name);
        float intensity = std::abs((float)sentiment) * (0.3f + outcomeSuccess * 0.4f);
        if (sentiment > 0) {
            pointed->entityHapiness = std::min(100.0f, pointed->entityHapiness + intensity * 4.0f);
            pointed->entityStress   = std::max(0.0f,   pointed->entityStress   - intensity * 2.5f);
            pointed->addToWorkingMemory("positive_interaction",
                entity->name + " showed genuine kindness", intensity);
        } else if (sentiment < 0) {
            pointed->entityStress       = std::min(100.0f, pointed->entityStress       + intensity * 5.0f);
            pointed->entityGeneralAnger = std::min(100.0f, pointed->entityGeneralAnger + intensity * 4.0f);
            pointed->entityHapiness     = std::max(0.0f,   pointed->entityHapiness     - intensity * 3.0f);
            pointed->addToWorkingMemory("negative_interaction",
                entity->name + " was hostile", intensity);
        }
        pointed->updatePAD();
    }

    // Phase 4: self-correction — poor outcomes recalibrate self-efficacy
    if (outcomeSuccess < 0.3f) {
        entity->SelfConcept.selfEfficacy = std::max(0.0f,
            entity->SelfConcept.selfEfficacy - 2.0f);
        entity->addToWorkingMemory("action_failed",
            "Tried '" + action->name + "' — it didn't work out.", 0.45f);
    }

    updateValuesFromExperiences(entity, action, outcomeSuccess);

    ActionMemory memory;
    memory.actionId = action->actionId;
    memory.actionName = action->name;
    memory.timestamp = currentTime;
    memory.outcomeSuccess = outcomeSuccess;
    memory.deliberationReasoning = lastDeliberation.internalReasoning;
    memory.isImpulsive = lastDeliberation.isImpulsive;
    memory.statsBefore = statsBefore;
    memory.statsAfter = statsAfter;

    updateHabits(action->actionId, context, outcomeSuccess);
    ASHB_TRACE_STREAM << "Updating Personnality\n";
    updatePersonalityFromExperience(entity, *action, outcomeSuccess);

    actionHistory.push_front(memory);
    if (actionHistory.size() > MAX_MEMORY) actionHistory.pop_back();

    currentTime++;
    ASHB_TRACE_STREAM << ">>>> Action completed. Memory recorded. Time now: " << currentTime << "\n";

    MentalModelOfOther* model = entity->getModelOf(entity);
    if (model == nullptr) {
        MentalModelOfOther* newModel = new MentalModelOfOther();
        newModel->entityPointed = entity;
        newModel->trustLevel = 50.0f;
        newModel->predictability = 0.5f;
        entity->list_MentalModelOfOther.push_back(newModel);
        model = newModel;
    }
    float accuracy = 0.3f + entity->personality.agreeableness / 200.0f;
    model->updateFromObservation(entity, accuracy, day);
}

void FreeWillSystem::clear_new_borns() {
    new_borns.clear();
}

void FreeWillSystem::updateNeeds(float deltaTime, Entity* ent) {
    float socialBuildRate = 0.15f;
    if (ent) {
        int ticksSinceSocial = 0;
        for (const auto& mem : ent->fws.getActionHistory()) {
            if (mem.actionName == "Socialize" || mem.actionName == "GoodConnection" || mem.actionName == "Date" || mem.actionName == "couple") {
                break;
            }
            ticksSinceSocial++;
            if (ticksSinceSocial >= 20) break;
        }
        if (ticksSinceSocial > 10) {
            socialBuildRate += (ticksSinceSocial - 10) * 0.02f;
        }
    }
    needs["social"].urgency += 0.15f * deltaTime;
    needs["health"].urgency += 0.08f * deltaTime;
    needs["hygiene"].urgency += 0.12f * deltaTime;
    for (auto& [name, need] : needs) {
        need.urgency = std::min(100.0f, need.urgency);
    }
}

void FreeWillSystem::addAction(const Action& action) {
    availableActions.push_back(action);
}

const std::deque<ActionMemory>& FreeWillSystem::getActionHistory() const {
    return actionHistory;
}

const std::map<std::string, Need>& FreeWillSystem::getNeeds() const {
    return needs;
}

float FreeWillSystem::calculateGoalAlignmentModifier(Entity* entity, Action* action) {
    float modifier = 1.0f;
    for (const LifeGoal& goal : entity->m_goals) {
        if (goal.priority < 20.0f) continue;
        float frustrationBoost = 1.0f + (goal.frustrationLevel / 100.0f) * 0.5f;
        if (goal.type == "build_career") {
            if (action->name == "Work on Project" || action->name == "LearnSkill") modifier *= 1.5f * frustrationBoost;
            if (action->needCategory == "entertainment") modifier *= 0.7f;
        } else if (goal.type == "find_partner") {
            if (action->name == "Flirt" || action->name == "Date" || action->name == "couple") modifier *= 1.6f * frustrationBoost;
            if (action->name == "Socialize") modifier *= 1.2f;
        } else if (goal.type == "make_friends") {
            if (action->needCategory == "social") modifier *= 1.3f * frustrationBoost;
        } else if (goal.type == "self") {
            if (action->name == "Prayer" || action->name == "SeekTherapy" || action->name == "CreativeActivity" || action->name == "Read") modifier *= 1.4f * frustrationBoost;
        } else if (goal.type == "happiness") {
            if (action->statChanges.size() > 0) {
                float happinessGain = 0.0f;
                for (const auto& sc : action->statChanges) {
                    if (sc.statName == "happiness") happinessGain += sc.changeValue;
                }
                if (happinessGain > 0) modifier *= 1.0f + happinessGain * 0.02f;
            }
        }
    }
    return std::max(0.3f, std::min(3.0f, modifier));
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



float FreeWillSystem::calculateOutcomeSuccess(const std::map<std::string, float>& before, const std::map<std::string, float>& after) {
    float success = 0.0f;
    int count = 0;
    std::vector<std::string> positiveStats = { "health", "happiness", "mentalHealth", "hygiene" };
    for (const auto& stat : positiveStats) {
        if (after.at(stat) > before.at(stat)) success += 1.0f;
        count++;
    }
    std::vector<std::string> negativeStats = { "stress", "loneliness", "boredom", "anger" };
    for (const auto& stat : negativeStats) {
        if (after.at(stat) < before.at(stat)) success += 1.0f;
        count++;
    }
    return count > 0 ? success / count : 0.5f;
}

float FreeWillSystem::getEntityStat(Entity* entity, const std::string& statName) {
    if (statName == "health") return entity->entityHealth;
    if (statName == "happiness") return entity->entityHapiness;
    if (statName == "stress") return entity->entityStress;
    if (statName == "mentalHealth") return entity->entityMentalHealth;
    if (statName == "loneliness") return entity->entityLoneliness;
    if (statName == "boredom") return entity->entityBoredom;
    if (statName == "anger") return entity->entityGeneralAnger;
    if (statName == "hygiene") return (float)entity->entityHygiene;
    if (statName == "hunger") return entity->entityHunger;
    if (statName == "fatigue") return entity->fatigueLevel;
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
    else if (statName == "hunger") entity->entityHunger = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "fatigue") entity->fatigueLevel = std::max(0.0f, std::min(100.0f, value));
}

void FreeWillSystem::saveTo(std::ofstream& file) const {
    file << "FWS_TIME:" << currentTime << "\n";
    file << "NEED_COUNT:" << needs.size() << "\n";
    for (const auto& pair : needs) {
        file << "NEED:" << pair.first << "," << pair.second.urgency << "\n";
    }
    file << "MEMORY_COUNT:" << actionHistory.size() << "\n";
    for (const auto& mem : actionHistory) {
        file << "MEMORY:" << mem.actionId << "," << mem.actionName << "," << mem.timestamp << "," << mem.outcomeSuccess << "\n";
        file << "STATSBEFORE:";
        bool first = true;
        for (const auto& s : mem.statsBefore) {
            if (!first) file << ";";
            file << s.first << "=" << s.second;
            first = false;
        }
        file << "\n";
        file << "STATSAFTER:";
        first = true;
        for (const auto& s : mem.statsAfter) {
            if (!first) file << ";";
            file << s.first << "=" << s.second;
            first = false;
        }
        file << "\n";
    }
}

void FreeWillSystem::loadFrom(std::ifstream& file) {
    std::string line;
    std::getline(file, line);
    currentTime = std::stoi(line.substr(9));
    initializeNeeds();
    initializeActions();
    std::getline(file, line);
    int needCount = std::stoi(line.substr(11));
    for (int i = 0; i < needCount; i++) {
        std::getline(file, line);
        std::string data = line.substr(5);
        size_t comma = data.find(',');
        std::string needName = data.substr(0, comma);
        float urgency = std::stof(data.substr(comma + 1));
        auto it = needs.find(needName);
        if (it != needs.end()) it->second.urgency = urgency;
    }
    std::getline(file, line);
    int memoryCount = std::stoi(line.substr(13));
    actionHistory.clear();
    for (int i = 0; i < memoryCount; i++) {
        ActionMemory mem;
        std::getline(file, line);
        std::string data = line.substr(7);
        size_t p1 = data.find(',');
        mem.actionId = std::stoi(data.substr(0, p1));
        size_t p2 = data.find(',', p1 + 1);
        mem.actionName = data.substr(p1 + 1, p2 - p1 - 1);
        size_t p3 = data.find(',', p2 + 1);
        mem.timestamp = std::stoi(data.substr(p2 + 1, p3 - p2 - 1));
        mem.outcomeSuccess = std::stof(data.substr(p3 + 1));
        std::getline(file, line);
        std::string beforeData = line.substr(12);
        if (!beforeData.empty()) {
            size_t pos = 0;
            while (pos < beforeData.size()) {
                size_t eq = beforeData.find('=', pos);
                if (eq == std::string::npos) break;
                size_t semi = beforeData.find(';', pos);
                std::string key = beforeData.substr(pos, eq - pos);
                std::string val = (semi == std::string::npos) ? beforeData.substr(eq + 1) : beforeData.substr(eq + 1, semi - eq - 1);
                mem.statsBefore[key] = std::stof(val);
                pos = (semi == std::string::npos) ? beforeData.size() : semi + 1;
            }
        }
        std::getline(file, line);
        std::string afterData = line.substr(11);
        if (!afterData.empty()) {
            size_t pos = 0;
            while (pos < afterData.size()) {
                size_t eq = afterData.find('=', pos);
                if (eq == std::string::npos) break;
                size_t semi = afterData.find(';', pos);
                std::string key = afterData.substr(pos, eq - pos);
                std::string val = (semi == std::string::npos) ? afterData.substr(eq + 1) : afterData.substr(eq + 1, semi - eq - 1);
                mem.statsAfter[key] = std::stof(val);
                pos = (semi == std::string::npos) ? afterData.size() : semi + 1;
            }
        }
        actionHistory.push_back(mem);
    }
}

Action* FreeWillSystem::checkHabitTrigger(const ActionContext& context) {
    for (auto& habit : habits) {
        if (habit.triggerContext == context && habit.strength > 0.7f) {
            // M6 entropy floor: no habit ever becomes a certainty — the cap
            // guarantees the deliberative path keeps sampling alternatives,
            // so an agent can't lock into a rut it never re-evaluates.
            // (Replaces an old hack that hard-reset strength to 0.65 for an
            // arbitrary subset of action ids.)
            float triggerChance = std::min(0.75f, habit.strength * 0.8f);
            // Satiation: a habit that has fired many times in a row loses its
            // grip — boredom is the mind's own entropy source.
            if (habit.consecutiveExecutions >= 10) {
                habit.strength = std::max(0.5f, habit.strength - 0.15f);
                habit.consecutiveExecutions = 0;
            }
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < triggerChance) {
                for (auto& action : availableActions) {
                    if (action.actionId == habit.actionId) {
                        return &action;
                    }
                }
            }
        }
    }
    return nullptr;
}

void FreeWillSystem::updateHabits(int actionId, const ActionContext& context, float outcomeSuccess) {
    // M6: reward-modulated habit learning. A satisfying outcome cements the
    // context→action link; a failure actively erodes it. Under the old flat
    // +0.05 reinforcement, an agent could grind a useless routine forever.
    bool found = false;
    for (auto& habit : habits) {
        if (habit.triggerContext == context) {
            if (habit.actionId == actionId) {
                if (outcomeSuccess >= 0.4f) {
                    habit.reinforce(0.02f + 0.08f * outcomeSuccess);  // 0.05 at neutral, 0.10 at best
                } else {
                    habit.decay(0.06f * (0.4f - outcomeSuccess));     // failing habits die
                }
                habit.consecutiveExecutions++;
                found = true;
            } else {
                habit.decay(0.02f);
                habit.consecutiveExecutions = 0;
            }
        }
    }
    if (!found) {
        habits.push_back(Habit(actionId, context));
    }
}

void FreeWillSystem::learnByObservation(Entity* observer, Entity* model,
                                        const std::string& actionName,
                                        float observedOutcome, int numPeopleNearby) {
    if (!observer || !model || observer == model) return;
    // Prestige bias: high-standing models are copied more readily; openness
    // gates how much an agent learns from watching at all (Henrich-style
    // cultural transmission, scaled way down from firsthand experience).
    float prestige = 0.5f + model->Esteem / 200.0f;                            // 0.5..1.0
    float weight   = 0.3f * prestige * (0.5f + observer->personality.openness / 200.0f);
    const std::string st = rlStateSignature(observer, numPeopleNearby);
    rlSystem.vicariousExperience(observer, st, actionName, observedOutcome * 100.0f, weight);
    rlSystem.observeAndLearn(observer, model, actionName, observedOutcome);

    // Cultural transmission of invented recipes (Step 4): watching a skilled,
    // prestigious model can also hand over one technique the observer lacks.
    // Deterministic gate - openness times prestige must clear a bar.
    if ((observer->personality.openness / 100.0f) * prestige > 0.45f) {
        for (int rid : model->knownRecipeIds) {
            if (!observer->knowsRecipe(rid)) {
                observer->learnRecipe(rid);
                break;   // at most one technique per observation
            }
        }
    }
}

void SocialNormSystem::update(const std::vector<Entity*>& allEntities) {
    std::map<std::string, int> actionCounts;
    int totalActions = 0;
    for (Entity* ent : allEntities) {
        if (ent->fws.getActionHistory().empty()) continue;
        const std::string& lastAction = ent->fws.getActionHistory().front().actionName;
        actionCounts[lastAction]++;
        totalActions++;
    }
    if (totalActions == 0) return;
    for (auto& [name, count] : actionCounts) {
        float prevalence = (float)count / totalActions;
        auto& norm = norms[name];
        norm.actionName = name;
        norm.prevalence = norm.prevalence * 0.95f + prevalence * 0.05f;
        norm.normPressure = std::abs(norm.prevalence - 0.5f) * 2.0f;
    }
}


bool FreeWillSystem::isKnown(Entity* entity, Entity* target) {
    if (entity->contains(entity->list_entityPointedSocial, target, 4) != -1) return true;
    if (entity->contains(entity->list_entityPointedDesire, target, 1) != -1) return true;
    if (entity->contains(entity->list_entityPointedAnger, target, 2) != -1) return true;
    if (entity->contains(entity->list_entityPointedCouple, target, 3) != -1) return true;
    return false;
}




Entity* FreeWillSystem::selectSocialTarget(Entity* entity, const std::vector<Entity*>& neighbors, const Action* action) {
    if (neighbors.empty()) return nullptr;

    // ── Action-aware targeting ────────────────────────────────────────────────
    // Romantic and hostile actions must CONCENTRATE on a consistent target,
    // otherwise desire/anger scatter randomly across neighbors every tick and
    // never accumulate past the thresholds needed to form a couple or a real
    // rivalry. So we deepen the strongest existing bond most of the time, and
    // otherwise seed a new one on the most fitting candidate.
    const std::string an = action ? action->name : "";
    bool romantic = (an == "Desire" || an == "Flirt" || an == "Date" ||
                     an == "couple" || an == "breeding");
    bool hostile  = (an == "AngerConnection" || an == "Insult" || an == "Jealousy" ||
                     an == "Betray" || an == "Discrimination");
    bool friendly = (an == "Socialize" || an == "GoodConnection" || an == "HelpSupport" ||
                     an == "Gossip"    || an == "Apologize"      || an == "Reconcile");
    std::uniform_real_distribution<float> concentrate(0.0f, 1.0f);

    // ── Friendly actions: deepen real friendships instead of scattering ───────
    // Without this, every positive interaction lands on a random neighbour and
    // no bond ever grows strong. Most of the time we re-invest in whoever we are
    // already closest to (weighted a bit by how much we like being around them).
    if (friendly) {
        if (concentrate(rng) < 0.70f) {
            Entity* best = nullptr; float bestBond = 5.0f; // need a minimum to "prefer"
            for (Entity* n : neighbors) {
                if (n == entity || n->entityHealth <= 0.0f) continue;
                float bond = entity->searchConnSocial(n);
                // Happier, friendlier company is more rewarding to seek out.
                float appeal = bond + (n->entityHapiness / 100.0f) * 4.0f;
                if (appeal > bestBond) { bestBond = appeal; best = n; }
            }
            if (best) return best;
        }
        // Otherwise reach out to someone new (handled by the generic logic below).
    }

    if (romantic) {
        // 75%: reinforce whoever is already most desired and still nearby.
        if (concentrate(rng) < 0.75f) {
            Entity* best = nullptr; float bestDesire = 0.0f;
            for (Entity* n : neighbors) {
                if (n == entity || n->entityHealth <= 0.0f) continue;
                float d = entity->searchConnDesire(n);
                if (d > bestDesire) { bestDesire = d; best = n; }
            }
            if (best) return best;
        }
        // Otherwise fall for the most attractive nearby entity.
        Entity* best = nullptr; float bestScore = -1.0f;
        for (Entity* n : neighbors) {
            if (n == entity || n->entityHealth <= 0.0f) continue;
            float attractiveness = (n->entityHygiene  / 100.0f) * 0.3f +
                                   (n->entityHapiness / 100.0f) * 0.4f +
                                   (n->entityHealth   / 100.0f) * 0.3f;
            if (entity->entitySex != n->entitySex) attractiveness += 0.25f;
            if (attractiveness > bestScore) { bestScore = attractiveness; best = n; }
        }
        if (best) return best;
    } else if (hostile) {
        // 75%: escalate against whoever is already most resented and still nearby.
        if (concentrate(rng) < 0.75f) {
            Entity* worst = nullptr; float worstAnger = 0.0f;
            for (Entity* n : neighbors) {
                if (n == entity || n->entityHealth <= 0.0f) continue;
                float a = entity->searchConnAng(n);
                if (a > worstAnger) { worstAnger = a; worst = n; }
            }
            if (worst) return worst;
        }
        // Otherwise pick a fresh grievance: reciprocate someone's hostility, else
        // resent the least-bonded neighbour (a stranger or distant acquaintance).
        Entity* worst = nullptr; float worstBond = 1e9f;
        for (Entity* n : neighbors) {
            if (n == entity || n->entityHealth <= 0.0f) continue;
            if (n->searchConnAng(entity) > 20.0f) return n; // they already hate us
            float bond = entity->searchConnSocial(n);
            if (bond < 0.0f) bond = 0.0f;                   // -1 => stranger
            if (bond < worstBond) { worstBond = bond; worst = n; }
        }
        if (worst) return worst;
    }

    std::vector<Entity*> strangers;
    std::vector<Entity*> acquaintances;

    for (Entity* n : neighbors) {
        if (n == entity) continue;
        if (isKnown(entity, n)) {
            acquaintances.push_back(n);
        } else {
            strangers.push_back(n);
        }
    }

    float strangerChance = 0.07f;
    strangerChance += (entity->personality.extraversion / 100.0f) * 0.1f;
    strangerChance += (entity->personality.openness    / 100.0f) * 0.15f;
    strangerChance += (entity->entityLoneliness        / 100.0f) * 0.1f;
    strangerChance  = std::clamp(strangerChance, 0.04f, 0.75f);
    strangerChance -= (entity->personality.neuroticism / 100.0f) * 0.1f;
    strangerChance  = std::clamp(strangerChance, 0.05f, 0.85f);

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    bool pickStranger = (dist(rng) < strangerChance);

    if (!acquaintances.empty() && !pickStranger) {
        int idx = BetterRand::genNrInInterval(0, (int)acquaintances.size() - 1);
        return acquaintances[idx];
    }

    if (!strangers.empty()) {
        int idx = BetterRand::genNrInInterval(0, (int)strangers.size() - 1);
        return strangers[idx];
    }

    if (!acquaintances.empty()) {
        int idx = BetterRand::genNrInInterval(0, (int)acquaintances.size() - 1);
        return acquaintances[idx];
    }

    return nullptr;
}

bool FreeWillSystem::isActionSocial(const Action* act){
    std::string an = act->name;
    return (an == "Socialize" || an == "GoodConnection" || an == "Desire" || an == "AngerConnection" ||
                                   an == "Gossip" || an == "HelpSupport" || an == "Flirt" || an == "Date" || an == "Reconcile" ||
                                   an == "couple" || an == "breeding" || an == "Apologize" || an == "Insult" || an == "Manipulate" ||
                                   an == "Jealousy" || an == "Betray" || an == "Discrimination" || an == "IgnoreAvoid" || an == "SetBoundaries");
}


Action* FreeWillSystem::cognitiveChooseAction(Entity* entity,
    const std::vector<Entity*>& neighbors,
    const ActionContext& context) {
    // M11 segment profiler (ASHB_PROFILE=1)
    static const bool s_prof2 = std::getenv("ASHB_PROFILE") != nullptr;
    static double s_msPercept = 0, s_msPlan = 0, s_msScore = 0, s_msMemPass = 0, s_msPick = 0;
    static long   s_calls2 = 0;
    auto seg_t = std::chrono::steady_clock::now();
    auto seg = [&](double& sink) {
        if (!s_prof2) return;
        auto n = std::chrono::steady_clock::now();
        sink += std::chrono::duration<double, std::milli>(n - seg_t).count();
        seg_t = n;
    };

    // Build a lightweight Perception
    Perception perception;
    float stressFactor = entity->entityStress / 100.0f;
    perception.attentionalFocus = std::max(0.0f, 1.0f - stressFactor);
    perception.perceivedEnv = context.env;
    perception.events.clear();
    perception.nearbyEntities.clear();

    // M4: percept budget — attention is a finite resource that stress narrows.
    // A calm agent registers everyone nearby; a panicked one registers only the
    // most salient few (threats first, then strong bonds). Everything after
    // this block reasons over `attended`, not the raw neighbor list, so a
    // stressed agent genuinely acts on less information.
    struct Salient { Entity* nb; float salience; float bond; };
    std::vector<Salient> ranked;
    ranked.reserve(neighbors.size());
    for (Entity* nb : neighbors) {
        float bond = entity->searchConnSocial(nb);
        float salience = std::max(0.0f, bond) / 100.0f;              // familiarity
        if (nb->entityHealth < 40.0f)
            salience += 1.0f + (40.0f - nb->entityHealth) / 40.0f;   // threats dominate
        if (nb->entityHapiness > 60.0f)
            salience += (nb->entityHapiness - 60.0f) / 80.0f;        // social pull
        ranked.push_back({nb, salience, bond});
    }
    std::stable_sort(ranked.begin(), ranked.end(),
        [](const Salient& a, const Salient& b) { return a.salience > b.salience; });
    const int perceptBudget = 3 + (int)(perception.attentionalFocus * 9.0f); // 3..12
    if ((int)ranked.size() > perceptBudget) ranked.resize(perceptBudget);

    std::vector<Entity*> attended;
    attended.reserve(ranked.size());
    for (const Salient& s : ranked) {
        attended.push_back(s.nb);
        PerceivedEntity pe;
        pe.entity = s.nb;
        // Social distance: well-known people feel "close", strangers feel "far"
        pe.distance = 100.0f - std::min(100.0f, s.bond);
        perception.nearbyEntities.push_back(pe);
        if (s.nb->entityHealth < 40.0f) {
            PerceivedEvent ev;
            ev.eventType = "Threat";
            ev.intensity = (40.0f - s.nb->entityHealth) / 40.0f;
            ev.source = s.nb;
            perception.events.push_back(ev);
        } else if (s.nb->entityHapiness > 60.0f) {
            PerceivedEvent ev;
            ev.eventType = "SocialOpportunity";
            ev.intensity = (s.nb->entityHapiness - 60.0f) / 40.0f;
            ev.source = s.nb;
            perception.events.push_back(ev);
        }
    }

    // Appraisal
    Appraisal appraisal;
    auto clamp01 = [](float v) {
        if (v <= 0.0f) return 0.0f;
        if (v >= 1.0f) return 1.0f;
        return v;
    };
    appraisal.novelty = 0.5f + 0.1f * (float)perception.events.size();
    appraisal.relevance = clamp01(0.4f + (entity->entityHapiness / 100.0f) * 0.25f - (entity->entityStress / 100.0f) * 0.25f);
    appraisal.desirability = clamp01(0.5f + (entity->personality.extraversion - 50.0f) / 200.0f);
    appraisal.controllability = clamp01(0.5f - (entity->entityStress / 100.0f) * 0.2f);
    appraisal.normCompliance = clamp01(0.5f + (entity->personality.agreeableness - 50.0f) / 200.0f);
    appraisal.agentBlame = 0.0f;
    appraisal.novelty = clamp01(appraisal.novelty);



    // M11: the social-influence and pheromone factors used to loop over every
    // neighbor for every action (~40×8 iterations). Both reduce exactly to
    // per-decision aggregates (their per-neighbor terms are additive and
    // action-independent), so compute the aggregates ONCE here and evaluate
    // each action against them in O(1). Same math, ~3µs/action saved.
    float agSumAnger = 0.0f, agSumHappy = 0.0f, agSumStress = 0.0f;
    float agSumRelWeight = 0.0f, agTotalWeight = 1.0f;
    float agPherSocial = 0.0f, agPherBreeding = 0.0f, agPherMating = 0.0f;
    for (Entity* nb : attended) {
        float w = 0.5f;
        for (const auto& s : entity->list_entityPointedSocial)
            if (s.pointedEntity == nb) w += s.social * 0.3f;
        for (const auto& d : entity->list_entityPointedDesire)
            if (d.pointedEntity == nb) w += d.desire * 0.45f;
        for (const auto& a : entity->list_entityPointedAnger)
            if (a.pointedEntity == nb) w -= a.anger * 0.8f;
        agSumRelWeight += w;
        agTotalWeight  += 1.0f + std::max(-0.5f, w);
        agSumAnger  += nb->entityGeneralAnger / 100.0f;
        agSumHappy  += nb->entityHapiness     / 100.0f;
        agSumStress += nb->entityStress       / 100.0f;
        if (nb->pheromone.type == "social")
            agPherSocial += nb->pheromone.releasing_level;
        else if (nb->pheromone.type == "breeding")
            agPherBreeding += nb->pheromone.releasing_level;
        else if (nb->pheromone.type == "procreation_simulation" || nb->pheromone.type == "sex")
            agPherMating += nb->pheromone.releasing_level;
    }
    float pherSeasonBase = 2.5f;
    if      (season == "spring") pherSeasonBase = 1.2f;
    else if (season == "summer") pherSeasonBase = 1.1f;
    else if (season == "winter") pherSeasonBase = 0.8f;
    else if (season == "autumn") pherSeasonBase = 0.9f;

    auto fastSocialInfluence = [&](const Action& act) -> float {
        if (attended.empty()) return 0.5f;
        float influence = 1.0f;
        const std::string& an = act.name;
        if (an == "Murder" || an == "Discrimination" || an == "AngerConnection" ||
            an == "Insult" || an == "Betray" || an == "Jealousy" ||
            an == "Manipulate" || an == "IgnoreAvoid") {
            influence += agSumAnger * 0.5f;
        } else if (an == "Socialize" || an == "GoodConnection" || an == "breeding" ||
                   an == "couple" || an == "HelpSupport" || an == "Apologize" ||
                   an == "Flirt" || an == "Date" || an == "Reconcile" ||
                   an == "CreativeActivity" || an == "LearnSkill") {
            influence += agSumHappy * 0.6f;
            if (act.needCategory == "social") influence += agSumRelWeight * 0.6f;
        } else if (an == "DrinkAlcohol" || an == "Smoke" || an == "SelfHarm" ||
                   an == "Procrastinate" || an == "QuitGiveUp") {
            influence += agSumStress * 0.6f;
        }
        return (influence + agTotalWeight * 2.6f) / (float)attended.size();
    };
    auto fastPheromones = [&](const Action& act) -> float {
        const std::string& an = act.name;
        if (an == "Socialize" || an == "GoodConnection" || an == "AngerConnection" ||
            an == "Gossip" || an == "HelpSupport" || an == "Apologize" || an == "Insult" ||
            an == "Discrimination" || an == "IgnoreAvoid")
            return pherSeasonBase + (agPherSocial / 100.0f) * 2.5f;
        if (an == "Breeding")
            return pherSeasonBase + (agPherBreeding / 100.0f) * 2.5f;
        if (an == "Desire" || an == "Flirt" || an == "Date" || an == "Reconcile" ||
            an == "couple" || an == "breeding" || an == "Jealousy" || an == "SetBoundaries")
            return pherSeasonBase + (agPherMating / 100.0f) * 2.5f;
        // Legacy quirk preserved: any other action with at least one neighbor
        // present always resolved to a flat 4.0.
        return attended.empty() ? pherSeasonBase : 4.0f;
    };

    seg(s_msPercept);

    // RL: snapshot the situation once; each candidate is scored against it below.
    const std::string rlState = rlStateSignature(entity, (int)attended.size());

    // Generate 3-7 candidates using existing scoring utilities
    std::vector<ActionCandidate> candidates;


    if (entity->entityLoneliness > 50.0f || entity->socialDeficit > 40.0f) {
        for (auto& action : availableActions) {
            if (action.needCategory == "social" && !attended.empty()) {
                ActionCandidate c(&action);
                c.score = 5.0f * (entity->entityLoneliness / 100.0f);
                candidates.push_back(c);
            }
        }
    }

    // Determine planned action from Tree of Thoughts planner
    std::string plannedActionName = getPlannedAction(entity, attended, 0.0f);
    seg(s_msPlan);

    // M11: per-factor cost attribution (ASHB_PROFILE=1) across the 12 scorers.
    static const bool s_prof3 = std::getenv("ASHB_PROFILE") != nullptr;
    static double s_f[12] = {};
    static long   s_factorCalls = 0;
    std::chrono::steady_clock::time_point f_t;
    auto fseg = [&](int i) {
        if (!s_prof3) return;
        auto n = std::chrono::steady_clock::now();
        s_f[i] += std::chrono::duration<double, std::milli>(n - f_t).count();
        f_t = n;
    };

    for (const Action& act : availableActions) {
        const Action* aPtr = &act;
        bool isSocialCat = (act.needCategory == "social" ||
                            act.name == "Murder" || act.name == "Betray");
        if (isSocialCat && attended.empty()) {
            continue;
        }
        if (s_prof3) { f_t = std::chrono::steady_clock::now(); ++s_factorCalls; }
        float requirementFitness = calculateRequirementFitness(entity, act);   fseg(0);
        float needSatisfaction = calculateNeedSatisfaction(act, entity);       fseg(1);
        float memoryBias = calculateMemoryBias(act.actionId);                  fseg(2);
        float varietyBonus = calculateVarietyBonus(act.actionId, act);         fseg(3);
        float socialInfluence = fastSocialInfluence(act);                      fseg(4);
        float contextualWeight = calculateContextualWeight(act, context);      fseg(5);
        float personalityModifier = calculatePersonalityModifier(entity, act); fseg(6);
        float pheromoneInfluence = fastPheromones(act);                        fseg(7);
        float valueSatisfaction = applyValueSatisfaction(entity, act);         fseg(8);
        float griefModifier = calculateGriefModifier(entity, act);             fseg(9);
        float envModifier = calculateEnvironmentalModifier(entity, act, context.env); fseg(10);
        float normModifier = calculateNormModifier(entity, act, entity->socialNorm); fseg(11);
        if (s_prof3 && s_factorCalls % 800000 == 0) {
            static const char* fn[12] = {"require","need","membias","variety","social","context",
                                         "persona","pherom","value","grief","env","norm"};
            std::cout << "PROFILE-FACTORS us/action-eval over " << s_factorCalls << ":";
            for (int i = 0; i < 12; ++i)
                std::cout << " " << fn[i] << "=" << (s_f[i] * 1000.0 / s_factorCalls);
            std::cout << "\n";
        }

        float score = requirementFitness * 0.20f + needSatisfaction * 0.25f + memoryBias * 0.10f +
                      varietyBonus * 0.10f + socialInfluence * 0.19f;
        score *= contextualWeight;
        score *= personalityModifier;
        score *= valueSatisfaction;
        score *= griefModifier;
        score *= pheromoneInfluence;
        score *= envModifier;
        score *= normModifier;

        // Planned action bias: boost if this matches the planned action
        if (!plannedActionName.empty() && act.name == plannedActionName) {
            score *= 1.5f;
        }


        float rarityMult = 1.0f;
        const std::string& an = act.name;
        bool isSocialAction = (an == "Socialize" || an == "GoodConnection" || an == "Desire" || an == "AngerConnection" ||
                               an == "Gossip" || an == "HelpSupport" || an == "Flirt" || an == "Date" || an == "Reconcile" ||
                               an == "couple" || an == "breeding" || an == "Apologize" || an == "Insult" || an == "Manipulate" ||
                               an == "Jealousy" || an == "Betray" || an == "Discrimination" || an == "IgnoreAvoid" || an == "SetBoundaries");

        if (isSocialAction) {
            if (attended.empty()) {
                //hard zero -> ne peut pas faire de social
                continue;
            }
            // Each multiplier applies exactly ONCE. (A botched merge used to
            // run the neighborBonus and per-action boosts a second time
            // unconditionally, giving social actions up to ~11.6× their
            // intended weight — the world was wall-to-wall socializing.)
            float neighborBonus = std::min(2.0f, 1.0f + attended.size() * 0.25f);
            rarityMult *= neighborBonus;
            if (an == "Socialize" || an == "GoodConnection" || an == "HelpSupport") rarityMult *= 3.4f;
            if (an == "Flirt" || an == "Date" || an == "couple") rarityMult *= 3.2f;
            if (an == "Gossip") rarityMult *= 3.7f;
            // Romance and rivalry get comparable weight so the social graph
            // isn't friendship-only.
            if (an == "Desire") rarityMult *= 3.3f;
            if (an == "AngerConnection") rarityMult *= 3.3f;
            if (an == "breeding") rarityMult *= 3.2f;
            // Social starvation adds urgency on top of (not instead of) the
            // base weighting.
            if (entity->socialDeficit > 1.0f)
                rarityMult += entity->socialDeficit * 0.5f + entity->dayWithoutSocialAction * 0.3f;
        }

        if (an == "Murder") rarityMult = 0.02f;
        else if (an == "Suicide") rarityMult = 0.02f;
        else if (an == "SelfHarm") rarityMult = 0.04f;
        else if (an == "Betray") rarityMult = 0.25f;
        else if (an == "Exercise") rarityMult = 0.09f;
        else if (an == "Discrimination") rarityMult = 0.15f;
        else if (an == "Sleep") rarityMult = 0.2f;
        else if (an == "Take Shower") rarityMult = 0.05f;
        else if (an == "SeekTherapy") rarityMult = 0.2f;
        else if (an == "Prayer") rarityMult = (entity->entityMentalHealth < 40.0f || entity->entityStress > 70.0f) ? 0.2f : 0.15f;

        if (an == "DrinkAlcohol") rarityMult *= 0.15f;
        if (an == "Smoke") rarityMult *= 0.20f;
        if (an == "Jealousy") rarityMult *= 0.15f;
        if (an == "Scrolling") rarityMult *= 0.20f;
        if (an == "EatMeal") rarityMult *= 0.20f;
        if (an == "Procrastinate") rarityMult *= 0.20f;
        if (an == "WatchEntertainment") rarityMult *= 0.35f;
        if (an == "Gaming") rarityMult *= 0.30f;
        if (an == "Take Shower") rarityMult *= 0.9f;
        score *= rarityMult;

        // RL: bias toward actions that have historically paid off in this state.
        // Neutral at the default Q (50); ±40% at the extremes.
        {
            float q = rlSystem.getActionValue(entity->getId(), rlState, an);
            score *= (0.6f + 0.8f * (q / 100.0f));
        }

        std::uniform_real_distribution<float> jitter(0.93f, 1.07f);
        score *= jitter(rng);
        if (score > 0.0f) {
            candidates.emplace_back(aPtr);
            candidates.back().score = score;
        }
    }

    if (candidates.size() < 3) {
        for (const Action& act : availableActions) {
            ActionCandidate c(&act);
            c.score = calculateNeedSatisfaction(act, entity) + calculateContextualWeight(act, context);
            candidates.push_back(c);
            if (candidates.size() >= 3) break;
        }
    }

    seg(s_msScore);

    // M11: two-pass scoring. The memory biases are the expensive factors —
    // the semantic bias alone runs an embedding search per call, and doing it
    // for all ~40 actions made deliberation 97% of the tick at 1k agents.
    // They are ~0.5-2× multipliers, so they can reorder near-winners but
    // can't rescue a bottom-ranked action: applying them only to the top 14
    // (2× the final candidate cut) preserves the outcome while cutting the
    // expensive calls ~3×.
    std::sort(candidates.begin(), candidates.end(),
        [](const ActionCandidate& a, const ActionCandidate& b) { return a.score > b.score; });
    if (candidates.size() > 14) candidates.resize(14);
    for (ActionCandidate& c : candidates) {
        // Autobiographical + semantic memory shape preference: trauma avoids
        // social, loss pulls toward coping, learned facts about people/actions
        // bias encounters. Both are multipliers centered on 1.0.
        c.score *= calculateLifeMemoryBias(entity, *c.action);
        c.score *= calculateSemanticMemoryBias(entity, *c.action, attended);
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const ActionCandidate& a, const ActionCandidate& b) { return a.score > b.score; });
    if (candidates.size() > 7) candidates.resize(7);
    seg(s_msMemPass);

    Deliberation delib;
    delib.candidates = candidates;
    delib.chosenAction = candidates.empty() ? nullptr : candidates.front().action;
    delib.internalReasoning = "Cognitive Pipeline: chosen by top-scoring candidate";
    delib.isImpulsive = (entity->entityStress > 60.0f) || (appraisal.novelty > 0.6f);

    float totalScore = 0.0f;
    for (const auto& c : candidates) {
        totalScore += c.score;
    }
    std::uniform_real_distribution<float> selectDist(0.0f, totalScore);
    float selection = selectDist(rng);
    float cumulative = 0.0f;
    for (const auto& c : candidates) {
        cumulative += c.score;
        if (selection <= cumulative) {
            delib.chosenAction = c.action;
            break;
        }
    }
    if (!delib.chosenAction && !candidates.empty()) delib.chosenAction = candidates.front().action;

    // Inner value conflict: a torn mind sometimes acts against its dominant
    // preference. Applied to the ACTUAL choice — the old code swapped two
    // entries of a local candidate copy *after* selection, which provably
    // changed nothing.
    ValueSystem& v = entity->ValueSystem;
    float conflictLevel = 0.0f;
    if (v.familyOrientation > 70.0f && v.achievementDrive > 70.0f) conflictLevel += 0.3f;
    if (v.hedonism > 70.0f && v.spiritualNeed > 70.0f) conflictLevel += 0.4f;
    if (v.collectivism > 70.0f && v.hedonism > 60.0f) conflictLevel += 0.2f;
    if (conflictLevel > 0.0f && candidates.size() >= 2) {
        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        if (roll(rng) < conflictLevel) {
            delib.chosenAction = (delib.chosenAction == candidates[0].action)
                                 ? candidates[1].action : candidates[0].action;
            delib.internalReasoning = "Value conflict: chose against dominant preference";
        }
    }

    // Introspection artifacts: the UI renders lastCoT, and hesitation feeds
    // the narrative layer. Built from the FINAL choice so the displayed
    // reasoning matches the executed action.
    if (delib.chosenAction != nullptr) {
        std::vector<std::string> topNames;
        std::vector<float> topScores;
        for (size_t i = 0; i < candidates.size() && i < 3; ++i) {
            topNames.push_back(candidates[i].action->name);
            topScores.push_back(candidates[i].score);
        }
        std::string situationHint = attended.empty()
            ? "alone"
            : std::to_string(attended.size()) + " people noticed nearby";
        entity->lastCoT = buildChainOfThought(
            entity->entityLoneliness, entity->entityStress, entity->entityGeneralAnger,
            entity->entityHapiness, entity->entityMentalHealth, entity->entityBoredom,
            plannedActionName, topNames, topScores,
            delib.chosenAction->name, delib.isImpulsive, situationHint);
        delib.internalReasoning = entity->lastCoT.toDisplayString();

        // Morally heavy choices pause the agent: complexity scales the dwell
        // time, stress and neuroticism color the spoken filler.
        float complexity = getActionComplexity(delib.chosenAction->name);
        if (complexity > 0.3f && !delib.isImpulsive) {
            entity->hesitation.pendingActionName = delib.chosenAction->name;
            entity->hesitation.decisionComplexity = complexity;
            entity->hesitation.ticksRemaining =
                complexity * (2.0f + entity->entityStress / 50.0f);
            entity->hesitation.fillerExpression = generateHesitationFiller(
                complexity, entity->entityStress, entity->personality.neuroticism);
        } else {
            entity->hesitation = HesitationState{};
        }
    }

    // Record what was actually decided (previously captured before selection,
    // so the stored deliberation never matched the executed action).
    lastDeliberation = delib;

    if (s_prof2) {
        seg(s_msPick);
        if (++s_calls2 % 20000 == 0)
            std::cout << "PROFILE-COG avg ms/call over " << s_calls2 << ": percept="
                      << s_msPercept / s_calls2 << " plan=" << s_msPlan / s_calls2
                      << " score=" << s_msScore / s_calls2 << " mempass=" << s_msMemPass / s_calls2
                      << " pick=" << s_msPick / s_calls2 << "\n";
    }

    if (delib.chosenAction != nullptr) return const_cast<Action*>(delib.chosenAction);
    return nullptr;
}

// ============================================================================
// M5: Social sanction — violence has an audience, and the audience answers.
// Every witness reacts in proportion to their bond with the victim: vendetta
// anger toward the offender, reputation collapse in their private ledger,
// mental-model trust cratering, social bonds cut. Kin and partners of the
// victim take it hardest — this is the seed of feuds. A same-tribe killing
// can additionally get the offender exiled by their community.
// ============================================================================
void FreeWillSystem::applySocialSanction(Entity* offender, Entity* victim,
                                         const std::vector<Entity*>& witnesses,
                                         bool lethal, int simDay) {
    if (!offender || !victim || offender == victim) return;
    auto clampf = [](float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };
    const float severity = lethal ? 1.0f : 0.45f;

    for (Entity* w : witnesses) {
        if (!w || w == offender || w == victim || w->entityHealth <= 0.0f) continue;

        // How much this witness cared about the victim.
        float bondV     = std::max(0.0f, w->searchConnSocial(victim));
        bool  isKin     = (victim->parent1 == w) || (victim->parent2 == w) ||
                          (w->parent1 == victim) || (w->parent2 == victim) ||
                          (std::find(w->childrenIds.begin(), w->childrenIds.end(),
                                     victim->entityId) != w->childrenIds.end());
        bool  isPartner = (w->contains(w->list_entityPointedCouple, victim, 3) != -1);

        // Moral outrage: baseline for any witness (norms exist even between
        // strangers), amplified by attachment to the victim and by the
        // witness's own agreeableness (empathy).
        float outrage = severity
                      * (12.0f + bondV * 0.35f + (isKin ? 45.0f : 0.0f) + (isPartner ? 45.0f : 0.0f))
                      * (0.5f + w->personality.agreeableness / 100.0f);

        // 1. Vendetta anger toward the offender.
        int aidx = w->contains(w->list_entityPointedAnger, offender, 2);
        if (aidx == -1) w->addAnger({ 1, offender, clampf(outrage, 0.0f, 100.0f) });
        else w->list_entityPointedAnger[aidx].anger =
                 clampf(w->list_entityPointedAnger[aidx].anger + outrage, 0.0f, 100.0f);

        // 2. Reputation collapse in this witness's private ledger.
        auto& rep = w->reputationMap[offender->entityId];
        rep.negativeScore   = clampf(rep.negativeScore + 30.0f * severity, 0.0f, 100.0f);
        rep.positiveScore   = clampf(rep.positiveScore - 25.0f * severity, 0.0f, 100.0f);
        rep.trustworthiness = clampf(rep.trustworthiness - 35.0f * severity, 0.0f, 100.0f);

        // 3. Mental model: you now KNOW what they are.
        MentalModelOfOther* m = w->getModelOf(offender);
        if (!m && w->list_MentalModelOfOther.size() < 16) {
            m = new MentalModelOfOther();
            m->entityPointed = offender;
            w->list_MentalModelOfOther.push_back(m);
        }
        if (m) {
            m->trustLevel = m->trustLevel * (lethal ? 0.2f : 0.5f);
            m->perceivedIntentionality = std::min(1.0f, m->perceivedIntentionality + 0.3f);
            m->confidence      = std::min(1.0f, m->confidence + 0.4f);  // vivid, certain
            m->lastObservedDay = simDay;
            m->lastInteractionOutcome = lethal ? "witnessed_killing" : "witnessed_assault";
        }

        // 4. The friendship, if any, is cut.
        int sidx = w->contains(w->list_entityPointedSocial, offender, 4);
        if (sidx != -1)
            w->list_entityPointedSocial[sidx].social *= (lethal ? 0.15f : 0.55f);

        // 5. Those who cared remember the scene for life.
        if (lethal && (isKin || isPartner || bondV > 20.0f)) {
            LifeMemory mem;
            mem.eventType          = "trauma";
            mem.entityInvolvedId   = offender->entityId;
            mem.emotionalIntensity = isKin || isPartner ? 2.4f : 1.6f;
            mem.simulationDay      = simDay;
            mem.isFormative        = isKin || isPartner;
            mem.internalNarrative  = "I saw " + offender->name + " kill " + victim->name;
            w->lifeMemories.push_back(mem);
            w->entityStress = clampf(w->entityStress + 20.0f, 0.0f, 100.0f);
        }
    }

    // 6. Tribal justice: killing one of your own can cost you your place.
    if (lethal && globalCivEngine && offender->tribeId >= 0 &&
        offender->tribeId == victim->tribeId) {
        if (BetterRand::genNrInInterval(0, 100) < 60) {
            Tribe* tribe = globalCivEngine->findTribe(offender->tribeId);
            if (tribe) {
                tribe->memberIds.erase(
                    std::remove(tribe->memberIds.begin(), tribe->memberIds.end(),
                                offender->entityId),
                    tribe->memberIds.end());
            }
            if (globalLogger) globalLogger->logCiv(simDay, "justice",
                offender->name + " exiled from tribe #" + std::to_string(offender->tribeId) +
                " for the killing of " + victim->name,
                "kind=exile offender=" + std::to_string(offender->entityId) +
                " victim=" + std::to_string(victim->entityId));
            offender->tribeId = -1;
            // Exile is social death: the outcast knows it.
            offender->entityStress   = clampf(offender->entityStress + 35.0f, 0.0f, 100.0f);
            offender->entityHapiness = clampf(offender->entityHapiness - 30.0f, 0.0f, 100.0f);
            offender->socialDeficit  = clampf(offender->socialDeficit + 40.0f, 0.0f, 100.0f);
            offender->onMajorEventAddOrBoostGoal("exiled");
        }
    }

    // 7. The offender feels the eyes on them even without exile.
    offender->entityStress = clampf(offender->entityStress + 15.0f * severity, 0.0f, 100.0f);
    offender->Esteem       = clampf(offender->Esteem - 12.0f * severity, 0.0f, 100.0f);

    // 8. M7: a cross-tribe killing is a grievance between PEOPLES. The victim's
    // tribe remembers, relations crater, and their blood is up — this is the
    // micro→macro channel by which one murder can eventually become a war.
    if (lethal && globalCivEngine && offender->tribeId >= 0 && victim->tribeId >= 0 &&
        offender->tribeId != victim->tribeId) {
        Tribe* aggrieved = globalCivEngine->findTribe(victim->tribeId);
        Tribe* offenders = globalCivEngine->findTribe(offender->tribeId);
        if (aggrieved && offenders) {
            float& r = aggrieved->relations[offenders->id];
            r = std::max(-100.0f, r - 15.0f);
            offenders->relations[aggrieved->id] = r;
            aggrieved->militarism = std::min(100.0f, aggrieved->militarism + 4.0f);
            if (globalLogger) globalLogger->logCiv(simDay, "grievance",
                "The " + aggrieved->name + " hold the " + offenders->name +
                " to blame for the killing of " + victim->name,
                "kind=blood_grievance tribeA=" + std::to_string(aggrieved->id) +
                " tribeB=" + std::to_string(offenders->id) +
                " relations=" + std::to_string((int)r));
        }
    }
}

// ============================================================================
// Semantic Memory Integration
// ============================================================================

float FreeWillSystem::calculateSemanticMemoryBias(Entity* entity, const Action& action, const std::vector<Entity*>& neighbors) {
    if (!entity) return 1.0f;

    // Query semantic memory for relevant context
    MemoryQuery query;
    query.contextType = "action_type";
    query.actionType = action.name;

    // If there are neighbors, check memory about them
    if (!neighbors.empty()) {
        // Pick the most socially connected neighbor to query about
        Entity* nearest = nullptr;
        float maxBond = -1.0f;
        for (auto* n : neighbors) {
            if (n) {
                float bond = entity->searchConnSocial(n);
                if (bond > maxBond) { maxBond = bond; nearest = n; }
            }
        }
        if (!nearest && !neighbors.empty()) nearest = neighbors[0];
        if (nearest) {
            query.targetEntityId = nearest->entityId;
            query.contextType = "encounter_entity";
        }
    }

    // Calculate memory bias for this action
    float memoryBias = entity->semanticMemory.calculateMemoryActionBias(action.name, query);

    // Return as multiplicative modifier (1.0 = neutral)
    return 1.0f + memoryBias;
}

// ============================================================================
// Plan-Aware Action Selection
// ============================================================================

std::string FreeWillSystem::getPlannedAction(Entity* entity, const std::vector<Entity*>& neighbors, float emergencyUrgency) {
    if (!entity) return "";

    // Check if there's an active plan
    if (!entity->planner.hasActivePlan()) {
        // Generate a new plan
        entity->planner.generateDailyPlan(entity);
    }

    // Get the next planned action
    return entity->planner.getNextPlannedAction(entity, neighbors, emergencyUrgency);
}

void FreeWillSystem::reportActionResult(Entity* entity, const std::string& actionName, float successScore, bool wasEmergency) {
    if (!entity) return;
    entity->planner.reportActionResult(entity, actionName, successScore, wasEmergency);
}

// ============================================================================
//  SOCIAL REALISM: JEALOUSY - RIVALRY - MATE-GUARDING - CRIMES OF PASSION
//  --------------------------------------------------------------------------
//  Relationships in this sim are *vectors* - an entity can accumulate several
//  couple bonds at once (polygamy) and desire people outside the couple. On its
//  own that produced no friction. This pass reads those configurations every
//  tick and turns them into the human drama they imply: a partner who takes a
//  second spouse breeds resentment among the others; an outsider lusting after
//  someone's mate becomes a hated rival; betrayal and possessiveness can curdle
//  into violence; collapsed trust ends relationships in grief.
// ============================================================================

// Personality-derived jealousy proneness, 0 (serene) .. 1 (volatile possessive).
float FreeWillSystem::jealousyDisposition(Entity* e) const {
    if (!e) return 0.0f;
    float neuro  = e->personality.neuroticism  / 100.0f;          // insecurity, anxiety
    float disagr = 1.0f - (e->personality.agreeableness / 100.0f); // hostility, possessiveness
    float base = 0.15f + 0.35f * neuro + 0.30f * disagr;
    switch (e->dv.attachmentStyle) {
        case ANXIOUS:      base += 0.25f; break;  // terrified of abandonment
        case DISORGANIZED: base += 0.20f; break;  // chaotic push/pull
        case AVOIDANT:     base -= 0.05f; break;  // detaches rather than fights
        case SECURE:       base -= 0.12f; break;  // trusting, self-assured
        default: break;
    }
    // People who prize family/exclusivity guard partners harder.
    base += (e->ValueSystem.familyOrientation / 100.0f - 0.5f) * 0.15f;
    return std::clamp(base, 0.0f, 1.0f);
}

void FreeWillSystem::processSocialConsequences(Entity* e, const std::vector<Entity*>& group, int simDay) {
    if (!e || e->entityHealth <= 0.0f) return;
    if (e->list_entityPointedCouple.empty()) return;  // only the partnered feel romantic jealousy

    const float jeal = jealousyDisposition(e);

    auto pos = [](float v){ return v < 0.0f ? 0.0f : v; };

    // Boost (or create) e's anger toward a target.
    auto addAngerToward = [&](Entity* tgt, float amount) {
        if (!tgt || tgt == e || amount <= 0.0f) return;
        int idx = e->contains(e->list_entityPointedAnger, tgt, 2);
        if (idx == -1) e->addAnger({ 1, tgt, std::min(100.0f, amount) });
        else e->list_entityPointedAnger[idx].anger =
                 std::min(100.0f, e->list_entityPointedAnger[idx].anger + amount);
        e->entityGeneralAnger = std::min(100.0f, e->entityGeneralAnger + amount * 0.25f);
    };

    // Iterate couples by index because breakups erase entries mid-loop.
    for (size_t ci = 0; ci < e->list_entityPointedCouple.size(); ) {
        entityPointedCouple& cp = e->list_entityPointedCouple[ci];
        Entity* P = cp.pointedEntity;
        if (!P || P->entityHealth <= 0.0f) { ++ci; continue; }

        cp.daysTogether++;

        // How much this entity is invested: lived attraction + built commitment.
        float love = pos(e->searchConnDesire(P)) * 0.6f + cp.commitment * 0.4f;

        // -- 1. Read the relationship for threats --------------------------
        Entity* rival = nullptr;
        float    rivalThreat = 0.0f;
        bool     partnerStrayed = false;   // partner is the one betraying (polygamy / wandering)

        // (a) Partner keeps OTHER couple bonds - the "multiple wives" case.
        for (auto& pcp : P->list_entityPointedCouple) {
            if (!pcp.pointedEntity || pcp.pointedEntity == e) continue;
            if (pcp.pointedEntity->entityHealth <= 0.0f) continue;
            partnerStrayed = true;
            float threat = 45.0f + pcp.commitment * 0.3f;
            if (threat > rivalThreat) { rivalThreat = threat; rival = pcp.pointedEntity; }
        }
        // (b) Partner openly desires someone else.
        for (auto& pd : P->list_entityPointedDesire) {
            if (!pd.pointedEntity || pd.pointedEntity == e) continue;
            if (pd.pointedEntity->entityHealth <= 0.0f) continue;
            if (pd.desire > 25.0f) {
                partnerStrayed = true;
                if (pd.desire > rivalThreat) { rivalThreat = pd.desire; rival = pd.pointedEntity; }
            }
        }
        // (c) Some outsider lusts after MY partner - a poacher/rival to guard against.
        for (Entity* o : group) {
            if (!o || o == e || o == P || o->entityHealth <= 0.0f) continue;
            float od = pos(o->searchConnDesire(P));
            if (od > 30.0f) {
                float dx = o->posX - P->posX, dy = o->posY - P->posY;
                float prox = (dx*dx + dy*dy < 80.0f*80.0f) ? 1.6f : 1.0f;  // seeing it stings more
                float threat = od * 0.8f * prox;
                if (threat > rivalThreat) { rivalThreat = threat; rival = o; }
            }
        }

        // -- 2. Suspicion / trust / satisfaction dynamics ------------------
        if (rivalThreat > 0.0f) {
            float build = rivalThreat * (0.15f + 0.35f * jeal);
            cp.suspicion    = std::min(100.0f, cp.suspicion + build * 0.05f);
            cp.trust        = std::max(0.0f,   cp.trust        - build * 0.020f);
            cp.satisfaction = std::max(0.0f,   cp.satisfaction - build * 0.015f);
            e->entityStress   = std::min(100.0f, e->entityStress   + build * 0.030f);
            e->entityHapiness = std::max(0.0f,   e->entityHapiness - build * 0.020f);
        } else {
            // No threat in sight: wounds heal, the bond quietly strengthens.
            cp.suspicion    = std::max(0.0f,   cp.suspicion - 0.30f);
            cp.trust        = std::min(100.0f, cp.trust        + 0.10f);
            cp.satisfaction = std::min(100.0f, cp.satisfaction + 0.05f);
            cp.commitment   = std::min(100.0f, cp.commitment   + 0.05f);
        }

        // -- 3. Jealous reaction: resent the rival, and the disloyal partner
        if (rival && cp.suspicion > 20.0f) {
            float jealAnger = (cp.suspicion / 100.0f) * (8.0f + love * 0.12f) * (0.5f + jeal);
            addAngerToward(rival, jealAnger);          // mate-guarding aggression (rival catches most)
            if (partnerStrayed) addAngerToward(P, jealAnger * 0.45f); // betrayal cuts both ways
            if (cp.suspicion > 40.0f && BetterRand::genNrInInterval(0, 100) < 8) {
                if (globalLogger) globalLogger->logEvent("jealousy",
                    e->name + " is consumed by jealousy over " + P->name +
                    " - sees " + rival->name + " as a rival");
                e->addToWorkingMemory("jealousy",
                    "I can't bear how " + rival->name + " circles " + P->name, 0.7f);
                LifeMemory mem;
                mem.eventType = "jealousy";
                mem.entityInvolvedId = rival->entityId;
                mem.emotionalIntensity = 1.4f;
                mem.simulationDay = simDay;
                mem.isFormative = false;
                mem.internalNarrative = "jealousy gnawed at me";
                e->lifeMemories.push_back(mem);
            }
        }

        // -- 4. Crime of passion -------------------------------------------
        // Sustained rage + emotional instability + the target within reach.
        Entity* victim = nullptr;
        float angAtRival   = rival ? pos(e->searchConnAng(rival)) : 0.0f;
        float angAtPartner = pos(e->searchConnAng(P));
        if (rival && angAtRival >= 70.0f)                 victim = rival;   // strike the interloper
        else if (partnerStrayed && angAtPartner >= 78.0f) victim = P;       // or the betrayer

        if (victim) {
            float instability = (100.0f - e->entityMentalHealth) / 100.0f * 0.40f
                              + e->entityStress / 100.0f * 0.30f
                              + jeal * 0.30f;
            float dx = victim->posX - e->posX, dy = victim->posY - e->posY;
            bool near = (dx*dx + dy*dy) < 90.0f * 90.0f;   // must be able to reach them
            float roll = BetterRand::genNrInInterval(0, 100) / 100.0f;
            // This pass runs every tick, so a high per-tick chance made jealous
            // killing the dominant cause of death (it dwarfed every other cause).
            // Keep outbursts uncommon, and make them lethal only rarely — most
            // jealous attacks are assaults, not murders.
            // M5 deterrence: eyes on you stay the hand — every bystander who
            // would witness (and sanction) the crime cuts the odds.
            int bystanders = 0;
            for (Entity* o : group)
                if (o && o != e && o != victim && o->entityHealth > 0.0f) ++bystanders;
            float deterrence = 1.0f - std::min(0.75f, (float)bystanders * 0.15f);
            if (near && roll < instability * 0.06f * deterrence * g_liveConfig.aggressionMul) {
                // M5 tuning: with sanctions/deterrence live, lethality is the
                // remaining lever on homicide share. Most outbursts wound; a
                // killing needs either near-total breakdown or an already
                // near-dead victim. (The old health<25 clause quietly turned
                // repeat assaults into executions.)
                bool lethal = ((instability > 0.85f) && BetterRand::genNrInInterval(0, 100) < 18)
                              || (victim->entityHealth < 12.0f);
                std::string mode = lethal ? "murder" : "assault";
                if (lethal) {
                    victim->entityHealth = 0.0f;
                } else {
                    victim->entityHealth = std::max(1.0f,
                        victim->entityHealth - (float)BetterRand::genNrInInterval(20, 45));
                    victim->entityStress  = std::min(100.0f, victim->entityStress + 30.0f);
                    int vidx = victim->contains(victim->list_entityPointedAnger, e, 2);
                    if (vidx == -1) victim->addAnger({ 1, e, 40.0f });
                    else victim->list_entityPointedAnger[vidx].anger =
                             std::min(100.0f, victim->list_entityPointedAnger[vidx].anger + 40.0f);
                }
                // The deed marks the attacker.
                e->entityMentalHealth      = std::max(0.0f,   e->entityMentalHealth - 15.0f);
                e->entityStress            = std::min(100.0f, e->entityStress + 20.0f);
                e->personality.neuroticism = std::min(100.0f, e->personality.neuroticism + 4.0f);
                // The outburst discharges the rage: anger must rebuild past the
                // threshold before another attack can fire, turning a per-tick
                // killing spree into an occasional flare-up.
                int aidx = e->contains(e->list_entityPointedAnger, victim, 2);
                if (aidx != -1) e->list_entityPointedAnger[aidx].anger *= 0.35f;
                LifeMemory mem;
                mem.eventType = "crime_of_passion";
                mem.entityInvolvedId = victim->entityId;
                mem.emotionalIntensity = 2.6f;
                mem.simulationDay = simDay;
                mem.isFormative = true;
                mem.internalNarrative = lethal ? "I killed out of jealousy"
                                               : "I attacked my rival in a blind rage";
                e->lifeMemories.push_back(mem);
                if (globalLogger) {
                    globalLogger->logEvent("crime_of_passion",
                        e->name + " committed " + mode + " against " + victim->name +
                        " out of jealousy over " + P->name);
                }
                // Attribute the cause; the central death-handler writes the single
                // death line, so a jealous killing is no longer also tallied as
                // generic "hardship".
                if (lethal) {
                    victim->pendingDeathCause = "crime of passion by " + e->name;
                    // Structured attribution (Plan 4.3): who, from where, and why.
                    victim->pendingKillerId      = e->entityId;
                    victim->pendingKillerTribeId = e->tribeId;
                    victim->pendingKillMotive    = "jealousy";
                }
                // M5: the community answers — vendetta, reputation, possible exile.
                applySocialSanction(e, victim, group, lethal, simDay);
                // Grief ripples out to everyone who was bonded to the victim.
                if (lethal) {
                    for (Entity* o : group) {
                        if (!o || o == victim || o->entityHealth <= 0.0f) continue;
                        bool bond = false;
                        for (auto& s : o->list_entityPointedSocial)
                            if (s.pointedEntity == victim && s.social > 8.0f) { bond = true; break; }
                        if (!bond) for (auto& c : o->list_entityPointedCouple)
                            if (c.pointedEntity == victim) { bond = true; break; }
                        if (bond) o->addGrief(victim->entityId,
                            0.7f + BetterRand::genNrInInterval(0, 20) / 100.0f, true);
                    }
                }
            }
        }

        // -- 4b. Continuity: stable couples have children ------------------
        // The breeding *action* almost never won the decision lottery, so lineages
        // died out. A committed, healthy adult pair now conceives on its own at a
        // measured pace. Only the lower-id partner initiates, so a couple doesn't
        // double-conceive from both sides on the same tick.
        if (P->entityHealth > 0.0f && e->entityId < P->entityId &&
            cp.daysTogether > 12 && (cp.daysTogether % 14 == 0) &&
            e->entityAge >= 16.0f && e->entityAge <= 55.0f &&
            P->entityAge >= 16.0f && P->entityAge <= 55.0f &&
            e->entityHealth > 40.0f && P->entityHealth > 40.0f &&
            cp.suspicion < 45.0f &&
            !(globalKinship && KinshipSystem::wouldBeIncest(*e, *P)) &&
            !(globalCivEngine && e->tribeId != P->tribeId &&
              globalCivEngine->areTribesAtWar(e->tribeId, P->tribeId)) &&
            pos(e->searchConnAng(P)) < 35.0f && pos(P->searchConnAng(e)) < 35.0f) {
            // Fertility rises with mutual desire / commitment. Tuned so a committed
            // couple reliably raises several children across their fertile years —
            // enough for lineages to outpace mortality — without spawning a child
            // every cycle.
            float fertility = 38.0f + cp.commitment * 0.28f + pos(e->searchConnDesire(P)) * 0.28f;
            // Young couples in their prime are markedly more fertile, so the
            // population pyramid refills from the bottom instead of greying out.
            float youngerAge = std::min(e->entityAge, P->entityAge);
            if (youngerAge <= 30.0f)      fertility *= 1.6f;
            else if (youngerAge <= 40.0f) fertility *= 1.25f;
            if (BetterRand::genNrInInterval(0, 100) < fertility) {
                static int nextLineageBabyId = 20000;
                Entity baby = Entity(nextLineageBabyId++, 0, 75, 85, 0, 100, "", 10, 0, 0, 75,
                                     'A', 0, 75, -1, "happiness");
                baby.posX = e->posX + BetterRand::genNrInInterval(-15, 15);
                baby.posY = e->posY + BetterRand::genNrInInterval(-15, 15);
                baby.parent1 = P;
                baby.parent2 = e;
                baby.originRegionId = (e->originRegionId >= 0) ? e->originRegionId : P->originRegionId;
                baby.tribeId = (e->tribeId >= 0) ? e->tribeId : P->tribeId;
                if (baby.tribeId >= 0 && globalCivEngine)
                    if (Tribe* bt = globalCivEngine->findTribe(baby.tribeId))
                        bt->memberIds.push_back(baby.entityId);
                if (g_lexicon) baby.name = g_lexicon->genName(baby.originRegionId, baby.entitySex);
                baby.personality.openness     = (P->personality.openness + e->personality.openness) / 2.0f;
                baby.personality.extraversion = (P->personality.extraversion + e->personality.extraversion) / 2.0f;
                baby.personality.agreeableness = (P->personality.agreeableness + e->personality.agreeableness) / 2.0f;
                baby.dv.hadSecureAttachment = (cp.satisfaction > 40.0f && cp.trust > 40.0f);
                baby.dv.childhoodNurturingScore = cp.satisfaction / 25.0f;
                baby.addOrBoostGoal("self", 100.0f);
                inheritEmergenceTraits(baby, e, P);
                if (globalKinship) globalKinship->registerBirth(baby, P, e,
                    globalCivEngine ? globalCivEngine->getCurrentYear() : 0);
                cp.commitment   = std::min(100.0f, cp.commitment + 6.0f);  // a child deepens the bond
                cp.satisfaction = std::min(100.0f, cp.satisfaction + 4.0f);
                e->onMajorEventAddOrBoostGoal("reproduction");
                P->onMajorEventAddOrBoostGoal("reproduction");
                if (globalCivEngine) {
                    globalCivEngine->logEvent(-1, baby.getName() + " was born to the couple "
                        + e->name + " & " + P->name, "birth");
                    globalCivEngine->totalBirths++;
                }
                if (globalLogger) {
                    globalLogger->logEvent("breeding",
                        "Child born to the committed couple " + e->name + " & " + P->name);
                    globalLogger->logBirth(baby.entityId, baby.getName(),
                        e->getId(), P->getId(), e->getName(), P->getName());
                }
                new_borns.push_back(baby);
            }
        }

        // -- 5. Breakup when trust or satisfaction collapses ---------------
        if (P->entityHealth > 0.0f &&
            (cp.trust < 12.0f || cp.satisfaction < 12.0f) &&
            BetterRand::genNrInInterval(0, 100) < 12) {
            Entity* ex = P;
            if (globalLogger) globalLogger->logRelationship(
                e->entityId, e->name, ex->entityId, ex->name,
                "separation", "trust collapsed under jealousy");
            e->entityLoneliness = std::min(100.0f, e->entityLoneliness + 20.0f);
            e->addGrief(ex->entityId, 0.4f, false);   // breakup grief, not death
            int exIdx = ex->contains(ex->list_entityPointedCouple, e, 3);
            if (exIdx != -1)
                ex->list_entityPointedCouple.erase(ex->list_entityPointedCouple.begin() + exIdx);
            e->list_entityPointedCouple.erase(e->list_entityPointedCouple.begin() + ci);
            continue;  // vector shifted - don't advance ci
        }

        ++ci;
    }
}
