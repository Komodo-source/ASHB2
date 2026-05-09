#include "./header/CognitiveArchitecture.h"
#include "./header/Entity.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <random>

// BeliefSystem implementation
void BeliefSystem::updateBelief(const std::string& topic, float newEvidence, float evidenceStrength) {
    float currentBelief = getBelief(topic);
    
    // Bayesian-style update with persistence
    float updateAmount = (newEvidence - currentBelief) * evidenceStrength * (1.0f - beliefPersistence);
    
    specificBeliefs[topic] = std::clamp(currentBelief + updateAmount, -100.0f, 100.0f);
}

float BeliefSystem::getBelief(const std::string& topic) const {
    auto it = specificBeliefs.find(topic);
    if (it != specificBeliefs.end()) {
        return it->second;
    }
    return 0.0f;  // Neutral if no belief exists
}

// MentalModel implementation
void MentalModel::updateCausalBelief(const std::string& action, float outcome) {
    float current = 0.0f;
    auto it = causalBeliefs.find(action);
    if (it != causalBeliefs.end()) {
        current = it->second;
    }
    
    // Update with recency bias
    float updated = current * 0.7f + outcome * 0.3f;
    causalBeliefs[action] = std::clamp(updated, 0.0f, 100.0f);
}

float MentalModel::predictOutcome(const std::string& action) const {
    auto it = causalBeliefs.find(action);
    if (it != causalBeliefs.end()) {
        return it->second;
    }
    return 50.0f;  // Uncertain if no prior experience
}

// MoralFoundation implementation
float MoralFoundation::evaluateActionMorality(const std::string& actionType) const {
    float moralityScore = 50.0f;
    
    // Simple heuristic based on action type
    if (actionType.find("help") != std::string::npos || 
        actionType.find("share") != std::string::npos) {
        moralityScore += careHarm * 0.3f + fairnessCheating * 0.2f;
    } else if (actionType.find("harm") != std::string::npos ||
               actionType.find("steal") != std::string::npos) {
        moralityScore -= careHarm * 0.3f + fairnessCheating * 0.2f;
    }
    
    if (actionType.find("betray") != std::string::npos) {
        moralityScore -= loyaltyBetrayal * 0.2f;
    }
    
    if (actionType.find("disrespect") != std::string::npos) {
        moralityScore -= authoritySubversion * 0.15f;
    }
    
    return std::clamp(moralityScore, 0.0f, 100.0f);
}

// CognitiveArchitecture implementation
CognitiveArchitecture::CognitiveArchitecture() {
    // Default biases based on typical human cognition
    activeBiases = {
        CognitiveBias::CONFIRMATION_BIAS,
        CognitiveBias::AVAILABILITY_HEURISTIC,
        CognitiveBias::SELF_SERVING_BIAS,
        CognitiveBias::NEGATIVITY_BIAS
    };
}

void CognitiveArchitecture::initializeFromEntity(Entity* entity) {
    if (!entity) return;
    
    // Initialize from personality traits
    style.analyticVsIntuitive = 50.0f + (entity->personality.openness - 50.0f) * 0.5f;
    style.needForCognition = 40.0f + entity->personality.openness * 0.6f;
    style.toleranceForAmbiguity = 30.0f + entity->personality.openness * 0.7f;
    
    style.impulsivity = 70.0f - entity->personality.conscientiousness * 0.6f;
    style.deliberativeness = entity->personality.conscientiousness * 0.8f;
    
    // Initialize beliefs from developmental history
    beliefs.internalLocusOfControl = 50.0f;
    if (entity->dv.childhoodNurturingScore > 60.0f) {
        beliefs.internalLocusOfControl += 20.0f;
        beliefs.justWorldBelief += 15.0f;
    } else if (entity->dv.childhoodTraumaScore > 40.0f) {
        beliefs.internalLocusOfControl -= 25.0f;
        beliefs.trustInInstitutions -= 20.0f;
    }
    
    // Initialize moral foundations from values
    morals.careHarm = entity->ValueSystem.collectivism * 0.6f + 20.0f;
    morals.fairnessCheating = 50.0f + (entity->personality.agreeableness - 50.0f) * 0.5f;
    morals.loyaltyBetrayal = entity->ValueSystem.collectivism * 0.7f;
    
    // Add basic causal beliefs
    mentalModel.causalBeliefs["social_interaction"] = 60.0f;
    mentalModel.causalBeliefs["work_effort"] = 55.0f;
    mentalModel.causalBeliefs["self_care"] = 65.0f;
}

std::vector<std::string> CognitiveArchitecture::filterPerceptions(
    const std::vector<std::string>& rawPerceptions,
    Entity* entity) {
    
    std::vector<std::string> filtered;
    
    for (const auto& perception : rawPerceptions) {
        bool include = true;
        
        // Negativity bias: negative events more likely to be noticed
        bool isNegative = perception.find("bad") != std::string::npos ||
                         perception.find("fail") != std::string::npos ||
                         perception.find("threat") != std::string::npos;
        
        if (isNegative && entity->personality.neuroticism > 60.0f) {
            include = true;  // High neuroticism amplifies negative perception
        } else if (!isNegative && entity->personality.neuroticism > 70.0f) {
            include = (rand() % 100) < 60;  // Miss some positive events
        }
        
        // Confirmation bias: filter based on existing beliefs
        for (const auto& [topic, belief] : beliefs.specificBeliefs) {
            if (perception.find(topic) != std::string::npos) {
                float beliefStrength = std::abs(belief) / 100.0f;
                if ((belief > 0 && perception.find("negative") != std::string::npos) ||
                    (belief < 0 && perception.find("positive") != std::string::npos)) {
                    include = (rand() % 100) > (beliefStrength * 50);
                }
            }
        }
        
        if (include) {
            filtered.push_back(perception);
        }
    }
    
    return filtered;
}

float CognitiveArchitecture::interpretEvent(float objectiveSeverity,
                                           bool isPositive,
                                           Entity* entity) {
    float interpretedSeverity = objectiveSeverity;
    
    // Apply negativity bias
    if (!isPositive) {
        interpretedSeverity *= (1.0f + entity->personality.neuroticism / 100.0f);
    }
    
    // Apply optimism/pessimism based on beliefs
    if (beliefs.justWorldBelief > 60.0f && !isPositive) {
        interpretedSeverity *= 0.8f;  // Optimists downplay negatives
    } else if (beliefs.justWorldBelief < 40.0f) {
        interpretedSeverity *= 1.2f;  // Pessimists amplify negatives
    }
    
    // Self-serving bias for outcomes
    if (isPositive) {
        interpretedSeverity *= (1.0f + beliefs.internalLocusOfControl / 100.0f);
    }
    
    return std::clamp(interpretedSeverity, 0.0f, 100.0f);
}

std::string CognitiveArchitecture::generateExplanation(const std::string& event,
                                                      Entity* entity) {
    // Fundamental attribution error: others' behavior = character, own = situation
    std::string explanation;
    
    if (event.find("other_failed") != std::string::npos) {
        if (entity->personality.agreeableness < 40.0f) {
            explanation = "They failed because they're incompetent";
        } else {
            explanation = "They had bad luck or difficult circumstances";
        }
    } else if (event.find("self_failed") != std::string::npos) {
        if (beliefs.internalLocusOfControl > 60.0f) {
            explanation = "I need to improve my approach";
        } else {
            explanation = "External factors worked against me";
        }
    } else if (event.find("success") != std::string::npos) {
        if (event.find("self") != std::string::npos) {
            explanation = "My skills and effort paid off";
        } else {
            explanation = "They got lucky or had advantages";
        }
    }
    
    return explanation.empty() ? "Unclear causation" : explanation;
}

float CognitiveArchitecture::evaluateOption(const std::string& option,
                                           const std::map<std::string, float>& attributes,
                                           Entity* entity) {
    float score = 50.0f;
    
    // Weight attributes by values
    for (const auto& [attr, value] : attributes) {
        float weight = 1.0f;
        
        if (attr == "risk") {
            weight = 1.0f - entity->personality.openness / 100.0f;
            score -= value * weight;
        } else if (attr == "social_benefit") {
            weight = entity->personality.extraversion / 100.0f;
            score += value * weight;
        } else if (attr == "effort_required") {
            weight = entity->personality.conscientiousness / 100.0f;
            score -= value * (1.0f - weight);
        } else if (attr == "novelty") {
            weight = entity->personality.openness / 100.0f;
            score += value * weight;
        }
    }
    
    // Apply moral evaluation
    if (!passesMoralEvaluation(option, entity)) {
        score -= 20.0f;
    }
    
    return std::clamp(score, 0.0f, 100.0f);
}

void CognitiveArchitecture::updateFromExperience(const std::string& domain,
                                                float outcome,
                                                Entity* entity) {
    // Update causal beliefs
    mentalModel.updateCausalBelief(domain, outcome);
    
    // Update relevant beliefs
    if (outcome > 70.0f) {
        beliefs.updateBelief("self_efficacy", outcome, 0.3f);
        if (beliefs.internalLocusOfControl > 50.0f) {
            beliefs.updateBelief("world_responsiveness", outcome, 0.2f);
        }
    } else if (outcome < 30.0f) {
        beliefs.updateBelief("self_efficacy", outcome, 0.2f);
    }
}

bool CognitiveArchitecture::passesMoralEvaluation(const std::string& action, Entity* entity) {
    float moralityScore = morals.evaluateActionMorality(action);
    return moralityScore >= 40.0f;  // Threshold for moral acceptability
}

void CognitiveArchitecture::saveTo(std::ofstream& file) const {
    // Save belief system
    file << beliefs.internalLocusOfControl << " "
         << beliefs.justWorldBelief << " "
         << beliefs.trustInInstitutions << " "
         << beliefs.spiritualOrientation << " "
         << beliefs.beliefPersistence << " "
         << beliefs.opennessToEvidence << "\n";
    
    file << beliefs.specificBeliefs.size() << "\n";
    for (const auto& [topic, strength] : beliefs.specificBeliefs) {
        file << topic << " " << strength << "\n";
    }
    
    // Save cognitive style
    file << style.analyticVsIntuitive << " "
         << style.needForCognition << " "
         << style.toleranceForAmbiguity << " "
         << style.cognitiveReflection << " "
         << style.impulsivity << " "
         << style.deliberativeness << "\n";
    
    // Save mental model (sample of causal beliefs)
    file << mentalModel.causalBeliefs.size() << "\n";
    for (const auto& [action, effectiveness] : mentalModel.causalBeliefs) {
        file << action << " " << effectiveness << "\n";
    }
    
    // Save moral foundations
    file << morals.careHarm << " "
         << morals.fairnessCheating << " "
         << morals.loyaltyBetrayal << " "
         << morals.authoritySubversion << " "
         << morals.sanctityDegradation << " "
         << morals.libertyOppression << "\n";
}

void CognitiveArchitecture::loadFrom(std::ifstream& file) {
    // Load belief system
    file >> beliefs.internalLocusOfControl
         >> beliefs.justWorldBelief
         >> beliefs.trustInInstitutions
         >> beliefs.spiritualOrientation
         >> beliefs.beliefPersistence
         >> beliefs.opennessToEvidence;
    
    int beliefCount;
    file >> beliefCount;
    for (int i = 0; i < beliefCount; ++i) {
        std::string topic;
        float strength;
        file >> topic >> strength;
        beliefs.specificBeliefs[topic] = strength;
    }
    
    // Load cognitive style
    file >> style.analyticVsIntuitive
         >> style.needForCognition
         >> style.toleranceForAmbiguity
         >> style.cognitiveReflection
         >> style.impulsivity
         >> style.deliberativeness;
    
    // Load mental model
    int beliefNum;
    file >> beliefNum;
    for (int i = 0; i < beliefNum; ++i) {
        std::string action;
        float effectiveness;
        file >> action >> effectiveness;
        mentalModel.causalBeliefs[action] = effectiveness;
    }
    
    // Load moral foundations
    file >> morals.careHarm
         >> morals.fairnessCheating
         >> morals.loyaltyBetrayal
         >> morals.authoritySubversion
         >> morals.sanctityDegradation
         >> morals.libertyOppression;
}
