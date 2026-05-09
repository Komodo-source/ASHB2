#include "./header/SocialDynamics.h"
#include "./header/Entity.h"
#include <fstream>
#include <algorithm>
#include <cmath>

// SocialGroup implementation
void SocialGroup::addMember(int entityId) {
    memberIds.insert(entityId);
}

void SocialGroup::removeMember(int entityId) {
    memberIds.erase(entityId);
}

float SocialGroup::calculateNormPressure(const std::string& action) const {
    auto it = normStrengths.find(action);
    if (it != normStrengths.end()) {
        return it->second * cohesion / 100.0f;
    }
    return 0.0f;
}

// SocialIdentity implementation
void SocialIdentity::updateFromInteraction(bool positiveExperience) {
    if (positiveExperience) {
        satisfaction = std::min(100.0f, satisfaction + 2.0f);
        commitment = std::min(100.0f, commitment + 1.0f);
    } else {
        satisfaction = std::max(0.0f, satisfaction - 3.0f);
        commitment = std::max(0.0f, commitment - 1.5f);
    }
}

// StatusPerception implementation
void StatusPerception::updateFromBehavior(const std::string& behaviorType, float intensity) {
    if (behaviorType == "dominant" || behaviorType == "assertive") {
        perceivedDominance = std::min(100.0f, perceivedDominance + intensity * 0.3f);
    } else if (behaviorType == "helpful" || behaviorType == "generous") {
        perceivedWarmth = std::min(100.0f, perceivedWarmth + intensity * 0.4f);
        respect = std::min(100.0f, respect + intensity * 0.2f);
    } else if (behaviorType == "competent" || behaviorType == "skilled") {
        perceivedCompetence = std::min(100.0f, perceivedCompetence + intensity * 0.5f);
        respect = std::min(100.0f, respect + intensity * 0.3f);
    }
}

// Reputation implementation
void Reputation::updateFromAction(const std::string& actionType, 
                                 float visibility,
                                 bool isPositive) {
    float change = visibility * (isPositive ? 1.0f : -1.5f);
    
    if (actionType.find("honest") != std::string::npos || 
        actionType.find("truth") != std::string::npos) {
        honesty = std::clamp(honesty + change, 0.0f, 100.0f);
    }
    if (actionType.find("help") != std::string::npos ||
        actionType.find("share") != std::string::npos) {
        generosity = std::clamp(generosity + change, 0.0f, 100.0f);
        trustworthiness = std::clamp(trustworthiness + change * 0.5f, 0.0f, 100.0f);
    }
    if (actionType.find("reliable") != std::string::npos ||
        actionType.find("keep_promise") != std::string::npos) {
        trustworthiness = std::clamp(trustworthiness + change, 0.0f, 100.0f);
        loyalty = std::clamp(loyalty + change * 0.7f, 0.0f, 100.0f);
    }
}

void Reputation::spreadGossip(int gossiperId, int listenerId, float gossipAmount) {
    gossipReceived[gossiperId] += gossipAmount;
    gossipCount++;
}

// RelationshipQuality implementation
void RelationshipQuality::updateFromInteraction(const std::string& interactionType,
                                               float intensity,
                                               bool isPositive) {
    totalInteractions++;
    
    if (isPositive) {
        positiveRatio = std::min(100.0f, positiveRatio + intensity * 0.3f);
        intimacy = std::min(100.0f, intimacy + intensity * 0.2f);
    } else {
        positiveRatio = std::max(0.0f, positiveRatio - intensity * 0.5f);
        conflictFrequency = std::min(100.0f, conflictFrequency + intensity * 0.3f);
    }
    
    // Update stage based on accumulated metrics
    float strength = calculateRelationshipStrength();
    if (strength > 80.0f && relationshipStage < 4) relationshipStage = 4;
    else if (strength > 60.0f && relationshipStage < 3) relationshipStage = 3;
    else if (strength > 40.0f && relationshipStage < 2) relationshipStage = 2;
    else if (strength > 20.0f && relationshipStage < 1) relationshipStage = 1;
    else if (strength < 15.0f && relationshipStage > 0) relationshipStage = 0;
}

float RelationshipQuality::calculateRelationshipStrength() const {
    return (intimacy * 0.4f + commitment * 0.3f + positiveRatio * 0.3f);
}

bool RelationshipQuality::isRelationshipViable() const {
    return (positiveRatio > 30.0f && conflictFrequency < 70.0f);
}

// SocialNetwork implementation
SocialGroup* SocialNetwork::getOrCreateGroup(const std::string& name, GroupType type) {
    for (auto& [id, group] : groups) {
        if (group->name == name) return group.get();
    }
    
    auto newGroup = std::make_shared<SocialGroup>();
    newGroup->groupId = nextGroupId++;
    newGroup->name = name;
    newGroup->type = type;
    groups[newGroup->groupId] = newGroup;
    return newGroup.get();
}

RelationshipQuality* SocialNetwork::getRelationship(int idA, int idB) {
    if (relationships.find(idA) != relationships.end() &&
        relationships[idA].find(idB) != relationships[idA].end()) {
        return &relationships[idA][idB];
    }
    return nullptr;
}

void SocialNetwork::createRelationship(int idA, int idB) {
    RelationshipQuality rq;
    rq.entityA = idA;
    rq.entityB = idB;
    relationships[idA][idB] = rq;
    relationships[idB][idA] = rq;
}

std::vector<int> SocialNetwork::findMutualConnections(int idA, int idB) {
    std::vector<int> mutuals;
    if (relationships.find(idA) == relationships.end()) return mutuals;
    
    for (const auto& [otherId, rel] : relationships[idA]) {
        if (relationships.find(idB) != relationships.end() &&
            relationships[idB].find(otherId) != relationships[idB].end()) {
            mutuals.push_back(otherId);
        }
    }
    return mutuals;
}

float SocialNetwork::calculateSocialDistance(int idA, int idB) {
    if (idA == idB) return 0.0f;
    
    auto* rel = getRelationship(idA, idB);
    if (rel) {
        return 100.0f - rel->calculateRelationshipStrength();
    }
    
    // Check for mutual connections
    auto mutuals = findMutualConnections(idA, idB);
    if (!mutuals.empty()) {
        return 50.0f - mutuals.size() * 10.0f;
    }
    
    return 100.0f;  // No connection
}

std::vector<int> SocialNetwork::getInGroupMembers(int entityId) {
    std::vector<int> members;
    for (const auto& [id, group] : groups) {
        if (group->memberIds.count(entityId)) {
            for (int memberId : group->memberIds) {
                if (memberId != entityId) {
                    members.push_back(memberId);
                }
            }
        }
    }
    return members;
}

void SocialNetwork::propagateGossip(int sourceId, int targetId,
                                   const std::string& content,
                                   float intensity) {
    // Simplified gossip propagation
    if (reputations.find(targetId) == reputations.end()) return;
    
    bool isNegative = content.find("bad") != std::string::npos ||
                     content.find("fail") != std::string::npos;
    
    reputations[targetId].spreadGossip(sourceId, sourceId, intensity);
    
    if (isNegative) {
        reputations[targetId].trustworthiness = 
            std::max(0.0f, reputations[targetId].trustworthiness - intensity * 0.3f);
    }
}

float SocialNetwork::calculateNormViolationCost(int entityId,
                                               const std::string& action,
                                               int groupId) {
    if (groups.find(groupId) == groups.end()) return 0.0f;
    
    float normPressure = groups[groupId]->calculateNormPressure(action);
    float cohesion = groups[groupId]->cohesion;
    
    return normPressure * cohesion / 100.0f;
}

// SocialDynamicsSystem implementation
SocialDynamicsSystem::SocialDynamicsSystem() {}

void SocialDynamicsSystem::initializeEntity(Entity* entity) {
    if (!entity) return;
    
    // Create initial social identities
    SocialIdentity familyIdentity;
    familyIdentity.groupId = 0;  // Family group
    familyIdentity.identificationStrength = 70.0f;
    entityIdentities[entity->entityId].push_back(familyIdentity);
}

void SocialDynamicsSystem::updateGroupCohesion(int groupId) {
    if (network.groups.find(groupId) == network.groups.end()) return;
    
    auto& group = network.groups[groupId];
    float avgPositiveRatio = 0.0f;
    int count = 0;
    
    for (int memberId : group->memberIds) {
        for (int otherId : group->memberIds) {
            if (memberId != otherId) {
                auto* rel = network.getRelationship(memberId, otherId);
                if (rel) {
                    avgPositiveRatio += rel->positiveRatio;
                    count++;
                }
            }
        }
    }
    
    if (count > 0) {
        group->cohesion = avgPositiveRatio / count;
    }
}

void SocialDynamicsSystem::processGroupEntry(int entityId, int groupId) {
    if (network.groups.find(groupId) != network.groups.end()) {
        network.groups[groupId]->addMember(entityId);
        
        // Add identity
        SocialIdentity identity;
        identity.groupId = groupId;
        identity.identificationStrength = 30.0f;
        entityIdentities[entityId].push_back(identity);
    }
}

void SocialDynamicsSystem::processGroupExit(int entityId, int groupId) {
    if (network.groups.find(groupId) != network.groups.end()) {
        network.groups[groupId]->removeMember(entityId);
        
        // Remove or reduce identity
        for (auto& identity : entityIdentities[entityId]) {
            if (identity.groupId == groupId) {
                identity.commitment = std::max(0.0f, identity.commitment - 30.0f);
            }
        }
    }
}

void SocialDynamicsSystem::updateRelationship(Entity* entityA, Entity* entityB,
                                             const std::string& interactionType,
                                             float intensity,
                                             bool isPositive) {
    if (!entityA || !entityB) return;
    
    int idA = entityA->entityId;
    int idB = entityB->entityId;
    
    auto* rel = network.getRelationship(idA, idB);
    if (!rel) {
        network.createRelationship(idA, idB);
        rel = network.getRelationship(idA, idB);
    }
    
    rel->updateFromInteraction(interactionType, intensity, isPositive);
}

void SocialDynamicsSystem::updateStatusPerceptions(Entity* actor,
                                                  const std::vector<Entity*>& observers,
                                                  const std::string& behaviorType) {
    if (!actor) return;
    
    for (Entity* observer : observers) {
        if (!observer) continue;
        // Update observer's perception of actor
        // Simplified for now
    }
}

void SocialDynamicsSystem::processGossip(Entity* gossiper, Entity* listener,
                                        int targetId, const std::string& content) {
    if (!gossiper || !listener) return;
    
    network.propagateGossip(gossiper->entityId, targetId, content, 15.0f);
}

void SocialDynamicsSystem::enforceNorms(Entity* actor,
                                       const std::vector<Entity*>& observers,
                                       const std::string& action) {
    if (!actor) return;
    
    // Check each observer's groups for norm violations
    for (Entity* observer : observers) {
        if (!observer) continue;
        
        // Find shared groups and calculate norm pressure
        // Apply social sanctions if violation detected
    }
}

float SocialDynamicsSystem::calculateInGroupBias(Entity* perceiver, Entity* target) {
    if (!perceiver || !target) return 0.0f;
    
    // Check if in same group
    auto inGroupMembers = network.getInGroupMembers(perceiver->entityId);
    for (int memberId : inGroupMembers) {
        if (memberId == target->entityId) {
            return 20.0f;  // In-group favoritism bonus
        }
    }
    
    return -10.0f;  // Slight out-group bias
}

float SocialDynamicsSystem::calculateConformityPressure(Entity* entity,
                                                       int groupId,
                                                       const std::string& behavior) {
    if (!entity || network.groups.find(groupId) == network.groups.end()) {
        return 0.0f;
    }
    
    auto& group = network.groups[groupId];
    float normStrength = group->calculateNormPressure(behavior);
    
    // Find average identification strength of members
    float avgIdentification = 0.0f;
    int count = 0;
    for (int memberId : group->memberIds) {
        for (const auto& identity : entityIdentities[memberId]) {
            if (identity.groupId == groupId) {
                avgIdentification += identity.identificationStrength;
                count++;
            }
        }
    }
    
    if (count > 0) {
        avgIdentification /= count;
    }
    
    return normStrength * avgIdentification / 100.0f;
}

void SocialDynamicsSystem::saveTo(std::ofstream& file) const {
    file << network.groups.size() << "\n";
    for (const auto& [id, group] : network.groups) {
        file << id << " " << group->name << " " 
             << static_cast<int>(group->type) << " "
             << group->cohesion << " " << group->prestige << "\n";
        file << group->memberIds.size() << "\n";
        for (int memberId : group->memberIds) {
            file << memberId << " ";
        }
        file << "\n";
    }
}

void SocialDynamicsSystem::loadFrom(std::ifstream& file) {
    size_t groupCount;
    file >> groupCount;
    
    for (size_t i = 0; i < groupCount; ++i) {
        int id, typeInt;
        std::string name;
        float cohesion, prestige;
        file >> id >> name >> typeInt >> cohesion >> prestige;
        
        auto group = std::make_shared<SocialGroup>();
        group->groupId = id;
        group->name = name;
        group->type = static_cast<GroupType>(typeInt);
        group->cohesion = cohesion;
        group->prestige = prestige;
        
        int memberCount;
        file >> memberCount;
        for (int j = 0; j < memberCount; ++j) {
            int memberId;
            file >> memberId;
            group->memberIds.insert(memberId);
        }
        
        network.groups[id] = group;
    }
}
