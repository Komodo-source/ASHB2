#include "./header/EnvironmentalInteraction.h"
#include "./header/Entity.h"
#include <fstream>
#include <cmath>
#include <algorithm>

// ResourceNode implementation
void ResourceNode::harvest(float amount) {
    float actualAmount = std::min(amount, currentAmount);
    currentAmount -= actualAmount;
    
    // Update depletion level
    depletionLevel = 1.0f - (currentAmount / maxAmount);
    
    if (currentAmount <= 0.0f) {
        isDepleted = true;
        currentAmount = 0.0f;
    }
}

void ResourceNode::regenerate(float deltaTime) {
    if (isDepleted) {
        // Slow recovery from depletion
        currentAmount += regenerationRate * 0.1f * deltaTime;
        if (currentAmount > maxAmount * 0.2f) {
            isDepleted = false;
        }
    } else {
        // Normal regeneration
        currentAmount += regenerationRate * deltaTime;
    }
    
    currentAmount = std::min(currentAmount, maxAmount);
    depletionLevel = 1.0f - (currentAmount / maxAmount);
}

// SpatialPreference implementation
void SpatialPreference::updateAttachment(int locationId, float experience) {
    auto it = placeAttachments.find(locationId);
    if (it == placeAttachments.end()) {
        placeAttachments[locationId] = experience;
    } else {
        // Update with recency bias
        it->second = it->second * 0.8f + experience * 0.2f;
    }
}

// TechnologyAdoption implementation
void TechnologyAdoption::evaluateTechnology(const std::vector<Entity*>& peers) {
    daysInStage++;
    
    // Evaluate based on perceived factors
    float adoptionLikelihood = (perceivedUsefulness + perceivedEaseOfUse + 
                                socialInfluence + facilitatingConditions) / 400.0f;
    
    // Innovativeness affects evaluation speed
    float evaluationSpeed = 1.0f + innovativeness * 0.01f;
    
    // Progress through stages
    switch (stage) {
        case AWARENESS:
            if (daysInStage > 5 / evaluationSpeed) {
                stage = INTEREST;
                daysInStage = 0;
            }
            break;
            
        case INTEREST:
            if (adoptionLikelihood > 0.3f && daysInStage > 10 / evaluationSpeed) {
                stage = EVALUATION;
                daysInStage = 0;
            } else if (adoptionLikelihood < 0.2f) {
                stage = REJECTION;
            }
            break;
            
        case EVALUATION:
            if (adoptionLikelihood > 0.5f && daysInStage > 15 / evaluationSpeed) {
                stage = TRIAL;
                daysInStage = 0;
            } else if (adoptionLikelihood < 0.3f) {
                stage = REJECTION;
            }
            break;
            
        case TRIAL:
            if (adoptionLikelihood > 0.6f && daysInStage > 20 / evaluationSpeed) {
                stage = ADOPTION;
            } else if (adoptionLikelihood < 0.4f) {
                stage = REJECTION;
            }
            break;
            
        default:
            break;
    }
    
    // Social influence from peers who have adopted
    for (Entity* peer : peers) {
        // Would check if peer has adopted this technology
        // For now, simplified
    }
}

bool TechnologyAdoption::shouldAdopt() const {
    return stage == ADOPTION;
}

// InstitutionalParticipation implementation
void InstitutionalParticipation::updateParticipation(float experience) {
    // Update satisfaction
    satisfaction = satisfaction * 0.9f + experience * 0.1f;
    satisfaction = std::clamp(satisfaction, 0.0f, 100.0f);
    
    // Update participation based on satisfaction
    if (satisfaction > 60.0f) {
        participationLevel = std::min(100.0f, participationLevel + 2.0f);
        trustInInstitution = std::min(100.0f, trustInInstitution + 1.0f);
    } else if (satisfaction < 40.0f) {
        participationLevel = std::max(0.0f, participationLevel - 3.0f);
        trustInInstitution = std::max(0.0f, trustInInstitution - 2.0f);
    }
    
    membershipDuration++;
    
    // Benefits and costs accumulate
    benefitsReceived += participationLevel * 0.1f;
    costsPaid += (100.0f - participationLevel) * 0.05f;
}

void InstitutionalParticipation::evaluateContinuedMembership() {
    // Calculate net benefit
    float netBenefit = benefitsReceived - costsPaid;
    
    // Satisfaction threshold for continued membership
    if (satisfaction < 20.0f || netBenefit < -50.0f) {
        // Consider leaving institution
        participationLevel = std::max(0.0f, participationLevel - 10.0f);
    }
    
    // Increase influence with duration and participation
    if (membershipDuration > 100 && participationLevel > 70.0f) {
        influence = std::min(100.0f, influence + 1.0f);
    }
}

// EnvironmentalBelief implementation
void EnvironmentalBelief::updateFromExperience(float experienceValence) {
    float beliefChange = experienceValence * personalExperienceWeight * 0.1f;
    beliefStrength = std::clamp(beliefStrength + beliefChange, -100.0f, 100.0f);
}

void EnvironmentalBelief::updateFromSocial(const std::vector<Entity*>& peers) {
    if (peers.empty()) return;
    
    // Aggregate peer beliefs (simplified)
    float socialInfluenceSum = 0.0f;
    int count = 0;
    
    // Would need to access peer beliefs here
    // For now, placeholder
    
    if (count > 0) {
        float avgPeerBelief = socialInfluenceSum / count;
        float change = (avgPeerBelief - beliefStrength) * socialInfluenceWeight * 0.05f;
        beliefStrength = std::clamp(beliefStrength + change, -100.0f, 100.0f);
    }
}

// ConsumptionPattern implementation
void ConsumptionPattern::makeConsumptionChoice(const std::vector<std::string>& options) {
    if (options.empty()) return;
    
    // Choose based on preferences
    std::string bestOption = options[0];
    float bestScore = -1e9f;
    
    for (const auto& option : options) {
        float score = 50.0f;
        
        // Quality preference
        // Would need quality data for each option
        
        // Price sensitivity
        // Would need price data
        
        // Environmental concern
        score += environmentalConcern * 0.2f;
        
        // Habitual preference
        for (const auto& preferred : preferredOptions) {
            if (option == preferred) {
                score += 20.0f;
            }
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestOption = option;
        }
    }
    
    // Update consumption rate based on choice
    consumptionRate = std::min(100.0f, consumptionRate + 1.0f);
}

// EnvironmentalInteractionSystem implementation
EnvironmentalInteractionSystem::EnvironmentalInteractionSystem() {}

ResourceNode* EnvironmentalInteractionSystem::createResource(
    const std::string& name,
    ResourceType type,
    float amount,
    float posX, float posY) {
    
    ResourceNode node;
    node.nodeId = nextResourceId++;
    node.name = name;
    node.type = type;
    node.currentAmount = amount;
    node.maxAmount = amount;
    node.regenerationRate = 1.0f;  // Default
    node.posX = posX;
    node.posY = posY;
    node.ownerId = -1;
    node.isDepleted = false;
    node.depletionLevel = 0.0f;
    
    resources[node.nodeId] = std::make_shared<ResourceNode>(node);
    return &(*resources[node.nodeId]);
}

ResourceNode* EnvironmentalInteractionSystem::getResource(int resourceId) {
    auto it = resources.find(resourceId);
    if (it != resources.end()) {
        return &(*it->second);
    }
    return nullptr;
}

void EnvironmentalInteractionSystem::harvestResource(int entityId, 
                                                      int resourceId, 
                                                      float amount) {
    auto it = resources.find(resourceId);
    if (it == resources.end()) return;
    
    auto& resource = it->second;
    
    // Check access rights
    if (resource->ownerId >= 0 && resource->ownerId != entityId) {
        bool hasAccess = false;
        for (int accessId : resource->accessList) {
            if (accessId == entityId) {
                hasAccess = true;
                break;
            }
        }
        if (!hasAccess) {
            // No access - could trigger conflict
            return;
        }
    }
    
    resource->harvest(amount);
}

void EnvironmentalInteractionSystem::updateResources(float deltaTime) {
    for (auto& [id, resource] : resources) {
        resource->regenerate(deltaTime);
    }
}

SpatialPreference& EnvironmentalInteractionSystem::getSpatialPreference(int entityId) {
    return spatialPreferences[entityId];
}

void EnvironmentalInteractionSystem::updateSpatialPreference(
    int entityId, int locationId, float experience) {
    auto& pref = spatialPreferences[entityId];
    pref.updateAttachment(locationId, experience);
}

int EnvironmentalInteractionSystem::selectLocation(
    Entity* entity,
    const std::vector<int>& availableLocations) {
    
    if (availableLocations.empty()) return -1;
    
    int id = entity->getId();
    auto& pref = spatialPreferences[id];
    
    int bestLocation = availableLocations[0];
    float bestScore = -1e9f;
    
    for (int locId : availableLocations) {
        float score = 50.0f;
        
        // Place attachment
        auto it = pref.placeAttachments.find(locId);
        if (it != pref.placeAttachments.end()) {
            score += it->second * 0.3f;
        }
        
        // Home location bonus
        if (locId == pref.homeLocationId) {
            score += 20.0f;
        }
        
        // Territoriality - prefer less crowded places
        // Would need crowding data
        
        if (score > bestScore) {
            bestScore = score;
            bestLocation = locId;
        }
    }
    
    return bestLocation;
}

void EnvironmentalInteractionSystem::introduceTechnology(
    int entityId, const std::string& techName) {
    
    auto& techs = technologyProfiles[entityId];
    
    // Check if already introduced
    for (const auto& tech : techs) {
        if (tech.technologyName == techName) {
            return;
        }
    }
    
    // Add new technology
    TechnologyAdoption newTech;
    newTech.technologyName = techName;
    newTech.perceivedUsefulness = 50.0f;
    newTech.perceivedEaseOfUse = 50.0f;
    newTech.socialInfluence = 30.0f;  // Low initially
    newTech.facilitatingConditions = 50.0f;
    newTech.innovativeness = 50.0f;  // Would be set from entity traits
    
    techs.push_back(newTech);
}

void EnvironmentalInteractionSystem::updateTechnologyAdoption(
    int entityId, float deltaTime) {
    
    auto& techs = technologyProfiles[entityId];
    
    for (auto& tech : techs) {
        if (tech.stage != TechnologyAdoption::ADOPTION &&
            tech.stage != TechnologyAdoption::REJECTION) {
            tech.evaluateTechnology({});  // Empty peer list for now
        }
    }
}

bool EnvironmentalInteractionSystem::hasAdoptedTechnology(
    int entityId, const std::string& techName) {
    
    auto& techs = technologyProfiles[entityId];
    
    for (const auto& tech : techs) {
        if (tech.technologyName == techName && tech.shouldAdopt()) {
            return true;
        }
    }
    return false;
}

InstitutionalParticipation* EnvironmentalInteractionSystem::joinInstitution(
    int entityId,
    const std::string& type,
    const std::string& role) {
    
    auto& memberships = institutionalMemberships[entityId];
    
    // Check if already member
    for (auto& membership : memberships) {
        if (membership.institutionType == type) {
            return &membership;
        }
    }
    
    // Create new membership
    InstitutionalParticipation newMembership;
    newMembership.institutionId = nextInstitutionId++;
    newMembership.institutionType = type;
    newMembership.role = role;
    newMembership.participationLevel = 20.0f;  // Start low
    newMembership.trustInInstitution = 50.0f;
    newMembership.complianceLevel = 50.0f;
    newMembership.satisfaction = 50.0f;
    newMembership.membershipDuration = 0;
    newMembership.influence = 10.0f;
    
    memberships.push_back(newMembership);
    return &memberships.back();
}

void EnvironmentalInteractionSystem::updateInstitutionalParticipation(
    int entityId, float deltaTime) {
    
    auto& memberships = institutionalMemberships[entityId];
    
    for (auto& membership : memberships) {
        membership.updateParticipation(membership.satisfaction);
        membership.evaluateContinuedMembership();
    }
}

void EnvironmentalInteractionSystem::updateEnvironmentalBeliefs(
    int entityId,
    const std::string& topic,
    float experience) {
    
    auto& beliefs = environmentalBeliefs[entityId];
    
    auto it = beliefs.find(topic);
    if (it == beliefs.end()) {
        EnvironmentalBelief newBelief;
        newBelief.beliefTopic = topic;
        newBelief.beliefStrength = experience * 0.5f;
        beliefs[topic] = newBelief;
    } else {
        it->second.updateFromExperience(experience);
    }
}

ConsumptionPattern* EnvironmentalInteractionSystem::getConsumptionPattern(
    int entityId, const std::string& category) {
    
    auto& patterns = consumptionPatterns[entityId];
    
    for (auto& pattern : patterns) {
        if (pattern.category == category) {
            return &pattern;
        }
    }
    
    // Create new pattern if not found
    ConsumptionPattern newPattern;
    newPattern.category = category;
    newPattern.consumptionRate = 50.0f;
    newPattern.qualityPreference = 50.0f;
    newPattern.priceSensitivity = 50.0f;
    newPattern.environmentalConcern = 50.0f;
    
    patterns.push_back(newPattern);
    return &patterns.back();
}

void EnvironmentalInteractionSystem::makeConsumptionDecision(
    int entityId,
    const std::string& category,
    const std::vector<std::string>& options) {
    
    auto* pattern = getConsumptionPattern(entityId, category);
    if (pattern) {
        pattern->makeConsumptionChoice(options);
    }
}

void EnvironmentalInteractionSystem::resolveResourceCompetition(
    int entityId1, int entityId2, int resourceId) {
    
    auto it = resources.find(resourceId);
    if (it == resources.end()) return;
    
    auto& resource = it->second;
    
    // Simple resolution: split remaining resources
    float available = resource->currentAmount;
    float halfAmount = available / 2.0f;
    
    resource->harvest(halfAmount);
    // Both entities would receive halfAmount
}

const std::map<int, std::shared_ptr<ResourceNode>>& 
EnvironmentalInteractionSystem::getResources() const {
    return resources;
}

void EnvironmentalInteractionSystem::saveTo(std::ofstream& file) const {
    // Save resources
    size_t numResources = resources.size();
    file.write(reinterpret_cast<const char*>(&numResources), sizeof(numResources));
    
    for (const auto& [id, resource] : resources) {
        file.write(reinterpret_cast<const char*>(&id), sizeof(id));
        file.write(reinterpret_cast<const char*>(&resource->nodeId), 
                   sizeof(resource->nodeId));
        file.write(reinterpret_cast<const char*>(&resource->currentAmount), 
                   sizeof(resource->currentAmount));
        file.write(reinterpret_cast<const char*>(&resource->depletionLevel), 
                   sizeof(resource->depletionLevel));
    }
    
    // Save institutional memberships
    size_t numMemberships = institutionalMemberships.size();
    file.write(reinterpret_cast<const char*>(&numMemberships), sizeof(numMemberships));
}

void EnvironmentalInteractionSystem::loadFrom(std::ifstream& file) {
    size_t numResources;
    file.read(reinterpret_cast<char*>(&numResources), sizeof(numResources));
    
    for (size_t i = 0; i < numResources; ++i) {
        int id;
        file.read(reinterpret_cast<char*>(&id), sizeof(id));
        
        auto resource = std::make_shared<ResourceNode>();
        file.read(reinterpret_cast<char*>(&resource->nodeId), 
                  sizeof(resource->nodeId));
        file.read(reinterpret_cast<char*>(&resource->currentAmount), 
                  sizeof(resource->currentAmount));
        file.read(reinterpret_cast<char*>(&resource->depletionLevel), 
                  sizeof(resource->depletionLevel));
        
        resources[id] = resource;
    }
    
    size_t numMemberships;
    file.read(reinterpret_cast<char*>(&numMemberships), sizeof(numMemberships));
}
