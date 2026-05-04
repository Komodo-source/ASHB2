#include "EnvironmentModel.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>

namespace environment {

// ============= SeasonalConfig Implementation =============

std::string SeasonalConfig::toString() const {
    switch (season) {
        case Season::SPRING: return "Spring";
        case Season::SUMMER: return "Summer";
        case Season::AUTUMN: return "Autumn";
        case Season::WINTER: return "Winter";
        default: return "Unknown";
    }
}

SeasonalConfig SeasonalConfig::fromMonth(int month) {
    SeasonalConfig config;
    
    // Northern hemisphere approximation
    if (month >= 3 && month <= 5) {
        config.season = Season::SPRING;
        config.temperature = 60.0f;
        config.daylightHours = 14.0f;
        config.precipitation = 50.0f;
        config.resourceModifier = 1.2f;
    } else if (month >= 6 && month <= 8) {
        config.season = Season::SUMMER;
        config.temperature = 80.0f;
        config.daylightHours = 16.0f;
        config.precipitation = 30.0f;
        config.resourceModifier = 1.5f;
    } else if (month >= 9 && month <= 11) {
        config.season = Season::AUTUMN;
        config.temperature = 50.0f;
        config.daylightHours = 11.0f;
        config.precipitation = 45.0f;
        config.resourceModifier = 0.8f;
    } else {
        config.season = Season::WINTER;
        config.temperature = 30.0f;
        config.daylightHours = 8.0f;
        config.precipitation = 40.0f;
        config.resourceModifier = 0.5f;
    }
    
    return config;
}

// ============= ResourceNode Implementation =============

ResourceNode::ResourceNode() 
    : id(0), type(ResourceType::FOOD), quantity(100.0f), maxQuantity(100.0f),
      regenerationRate(1.0f), x(0), y(0), z(0), renewable(true), ownerId(-1) {}

ResourceNode::ResourceNode(ResourceType t, float qty, float regen,
                           float px, float py, float pz)
    : id(0), type(t), quantity(qty), maxQuantity(qty),
      regenerationRate(regen), x(px), y(py), z(pz), renewable(true), ownerId(-1) {}

void ResourceNode::update(float deltaTime) {
    if (renewable && quantity < maxQuantity) {
        quantity = std::min(maxQuantity, quantity + regenerationRate * deltaTime);
    }
}

float ResourceNode::harvest(float amount) {
    float actualAmount = std::min(amount, quantity);
    quantity -= actualAmount;
    return actualAmount;
}

bool ResourceNode::isDepleted() const {
    return quantity <= 0.01f;
}

// ============= EnvironmentalState Implementation =============

EnvironmentalState::EnvironmentalState()
    : globalTemperature(60.0f), humidity(50.0f), airQuality(80.0f),
      ambientNoise(20.0f), timeOfDay(12), dayOfYear(1) {
    currentSeason = SeasonalConfig::fromMonth(1);
}

void EnvironmentalState::advanceTime(float hours) {
    timeOfDay = (timeOfDay + static_cast<int>(hours)) % 24;
    
    float daysPassed = hours / 24.0f;
    dayOfYear += static_cast<int>(daysPassed);
    if (dayOfYear > 365) dayOfYear = 1;
    
    // Update season based on day of year
    int month = ((dayOfYear - 1) / 30) % 12 + 1;
    currentSeason = SeasonalConfig::fromMonth(month);
    
    updateSeasonalEffects();
}

void EnvironmentalState::updateSeasonalEffects() {
    globalTemperature = currentSeason.temperature;
    // Apply regional modifiers
    for (const auto& [region, modifier] : regionalModifiers) {
        // Would apply region-specific adjustments
    }
}

// ============= ResourceManager Implementation =============

ResourceManager::ResourceManager() : competitionFactor(1.0f) {
    // Initialize global availability
    globalAvailability[ResourceType::FOOD] = 1000.0f;
    globalAvailability[ResourceType::WATER] = 1000.0f;
    globalAvailability[ResourceType::SHELTER] = 100.0f;
    globalAvailability[ResourceType::INFORMATION] = 500.0f;
    globalAvailability[ResourceType::ENERGY] = 500.0f;
    globalAvailability[ResourceType::MATERIALS] = 500.0f;
}

void ResourceManager::addResource(std::shared_ptr<ResourceNode> resource) {
    static int nextId = 1;
    resource->id = nextId++;
    resources.push_back(resource);
    globalAvailability[resource->type] += resource->quantity;
}

void ResourceManager::removeResource(int resourceId) {
    auto it = std::find_if(resources.begin(), resources.end(),
        [resourceId](const auto& r) { return r->id == resourceId; });
    
    if (it != resources.end()) {
        globalAvailability[(*it)->type] -= (*it)->quantity;
        resources.erase(it);
    }
}

std::vector<std::shared_ptr<ResourceNode>> ResourceManager::findNearbyResources(
    float x, float y, float z, float radius, ResourceType type) {
    
    std::vector<std::shared_ptr<ResourceNode>> nearby;
    
    for (const auto& resource : resources) {
        if (resource->type != type && type != ResourceType::FOOD) continue;
        
        float dx = resource->x - x;
        float dy = resource->y - y;
        float dz = resource->z - z;
        float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (distance <= radius && !resource->isDepleted()) {
            nearby.push_back(resource);
        }
    }
    
    return nearby;
}

float ResourceManager::harvestResource(int resourceId, int harvesterId, float amount) {
    for (auto& resource : resources) {
        if (resource->id == resourceId) {
            // Check ownership
            if (resource->ownerId != -1 && resource->ownerId != harvesterId) {
                return 0.0f;  // Not allowed to harvest
            }
            
            float harvested = resource->harvest(amount);
            globalAvailability[resource->type] -= harvested;
            return harvested;
        }
    }
    return 0.0f;
}

bool ResourceManager::claimResource(int resourceId, int entityId) {
    for (auto& resource : resources) {
        if (resource->id == resourceId) {
            if (resource->ownerId == -1) {
                resource->ownerId = entityId;
                return true;
            }
            return false;
        }
    }
    return false;
}

void ResourceManager::releaseResource(int resourceId) {
    for (auto& resource : resources) {
        if (resource->id == resourceId) {
            resource->ownerId = -1;
            return;
        }
    }
}

void ResourceManager::update(float deltaTime, const EnvironmentalState& env) {
    float seasonModifier = env.currentSeason.resourceModifier;
    
    for (auto& resource : resources) {
        resource->regenerationRate *= seasonModifier;
        resource->update(deltaTime);
        
        if (resource->isDepleted() && !resource->renewable) {
            globalAvailability[resource->type] -= resource->maxQuantity;
        }
    }
}

float ResourceManager::getTotalResource(ResourceType type) const {
    float total = 0.0f;
    for (const auto& resource : resources) {
        if (resource->type == type) {
            total += resource->quantity;
        }
    }
    return total;
}

float ResourceManager::getAvailabilityIndex(ResourceType type) const {
    auto it = globalAvailability.find(type);
    if (it != globalAvailability.end()) {
        return it->second / 1000.0f;  // Normalized
    }
    return 0.5f;
}

void ResourceManager::setCompetitionFactor(float factor) {
    competitionFactor = factor;
}

float ResourceManager::calculateCompetitionCost(int entityId, ResourceType type) {
    // Cost increases with scarcity and number of competitors
    float availability = getAvailabilityIndex(type);
    float scarcity = 1.0f - availability;
    return scarcity * competitionFactor * 10.0f;
}

// ============= CulturalGroup Implementation =============

void CulturalGroup::updateTraits() {
    for (auto& trait : traits) {
        // Update prevalence based on member adoptions
        int adopters = 0;
        for (int memberId : memberIds) {
            // Would check if member has trait
        }
        trait.prevalence = static_cast<float>(adopters) / memberIds.size();
    }
}

float CulturalGroup::calculateTraitAdoption(const CulturalTrait& trait) {
    float baseRate = trait.transmissionRate;
    float cohesionBonus = cohesion * 0.2f;
    float opennessBonus = openness * 0.3f;
    
    return std::min(1.0f, baseRate + cohesionBonus + opennessBonus);
}

// ============= CulturalTransmissionSystem Implementation =============

CulturalTransmissionSystem::CulturalTransmissionSystem()
    : verticalTransmissionRate(0.8f), horizontalTransmissionRate(0.3f),
      obliqueTransmissionRate(0.5f) {}

void CulturalTransmissionSystem::addGroup(const CulturalGroup& group) {
    groups.push_back(group);
}

void CulturalTransmissionSystem::assignEntityToGroup(int entityId, int groupId) {
    entityToGroup[entityId] = groupId;
    
    for (auto& group : groups) {
        if (group.id == groupId) {
            group.memberIds.push_back(entityId);
            break;
        }
    }
}

void CulturalTransmissionSystem::transmitVertically(int parentId, int childId) {
    // Pass traits from parent to child
    auto parentGroupIt = entityToGroup.find(parentId);
    if (parentGroupIt == entityToGroup.end()) return;
    
    int groupId = parentGroupIt->second;
    for (auto& group : groups) {
        if (group.id == groupId) {
            for (const auto& trait : group.traits) {
                // Child inherits trait with probability
                if (static_cast<float>(rand()) / RAND_MAX < verticalTransmissionRate) {
                    // Would add trait to child
                }
            }
            break;
        }
    }
}

void CulturalTransmissionSystem::transmitHorizontally(int sourceId, int targetId) {
    // Peer-to-peer transmission
    auto sourceGroupIt = entityToGroup.find(sourceId);
    auto targetGroupIt = entityToGroup.find(targetId);
    
    if (sourceGroupIt == entityToGroup.end() || targetGroupIt == entityToGroup.end()) return;
    
    // Transmission more likely within same group
    float rate = (sourceGroupIt->second == targetGroupIt->second) ? 
                 horizontalTransmissionRate : horizontalTransmissionRate * 0.5f;
    
    // Would transmit traits
}

void CulturalTransmissionSystem::transmitObliquely(int elderId, int youthId) {
    // Elder to young transmission (teachers, leaders)
    // Higher prestige transmission
}

void CulturalTransmissionSystem::updateCulturalTraits(float deltaTime) {
    for (auto& group : groups) {
        group.updateTraits();
        
        // Trait evolution
        for (auto& trait : group.traits) {
            // Mutation
            if (static_cast<float>(rand()) / RAND_MAX < trait.mutationRate) {
                // Would mutate trait
            }
            
            // Selection based on fitness
            if (trait.fitnessFunction) {
                float fitness = trait.fitnessFunction(trait.prevalence);
                trait.prevalence += fitness * deltaTime * 0.01f;
                trait.prevalence = std::clamp(trait.prevalence, 0.0f, 1.0f);
            }
        }
    }
}

void CulturalTransmissionSystem::introduceInnovation(int groupId, 
                                                      const CulturalTrait& trait) {
    for (auto& group : groups) {
        if (group.id == groupId) {
            group.traits.push_back(trait);
            break;
        }
    }
}

std::vector<CulturalTrait> CulturalTransmissionSystem::getEntityTraits(int entityId) const {
    std::vector<CulturalTrait> traits;
    
    auto groupIt = entityToGroup.find(entityId);
    if (groupIt == entityToGroup.end()) return traits;
    
    int groupId = groupIt->second;
    for (const auto& group : groups) {
        if (group.id == groupId) {
            return group.traits;
        }
    }
    
    return traits;
}

float CulturalTransmissionSystem::calculateCulturalDistance(int entityId1, 
                                                             int entityId2) const {
    auto traits1 = getEntityTraits(entityId1);
    auto traits2 = getEntityTraits(entityId2);
    
    if (traits1.empty() || traits2.empty()) return 1.0f;
    
    // Calculate Jaccard distance
    std::set<std::string> names1, names2;
    for (const auto& t : traits1) names1.insert(t.name);
    for (const auto& t : traits2) names2.insert(t.name);
    
    std::vector<std::string> intersection, union_;
    std::set_intersection(names1.begin(), names1.end(), 
                         names2.begin(), names2.end(),
                         std::back_inserter(intersection));
    std::set_union(names1.begin(), names1.end(),
                  names2.begin(), names2.end(),
                  std::back_inserter(union_));
    
    return 1.0f - static_cast<float>(intersection.size()) / union_.size();
}

// ============= Institution Implementation =============

void Institution::updateLegitimacy() {
    // Legitimacy based on resource distribution fairness and rule compliance
    float avgSupport = 0.0f;
    for (int memberId : memberIds) {
        // Would calculate individual support
    }
    legitimacy = avgSupport / memberIds.size();
}

float Institution::enforceRule(const std::string& ruleName, int entityId) {
    auto it = rules.find(ruleName);
    if (it == rules.end()) return 0.0f;
    
    float ruleStrength = it->second;
    // Apply sanction or reward
    return ruleStrength * legitimacy;
}

void Institution::distributeResources() {
    if (resources.empty() || memberIds.empty()) return;
    
    float perMember = 0.0f;
    for (const auto& [resType, amount] : resources) {
        perMember = amount / memberIds.size();
        // Would distribute to members
    }
}

// ============= InstitutionalSystem Implementation =============

InstitutionalSystem::InstitutionalSystem() {}

void InstitutionalSystem::createInstitution(InstitutionType type, 
                                             const std::string& name) {
    static int nextId = 1;
    auto inst = std::make_shared<Institution>();
    inst->id = nextId++;
    inst->type = type;
    inst->name = name;
    inst->legitimacy = 0.5f;
    inst->efficiency = 0.5f;
    inst->foundingDay = 0;
    
    institutions.push_back(inst);
}

void InstitutionalSystem::addMember(int institutionId, int entityId) {
    for (auto& inst : institutions) {
        if (inst->id == institutionId) {
            inst->memberIds.push_back(entityId);
            entityMemberships[entityId].push_back(institutionId);
            break;
        }
    }
}

void InstitutionalSystem::removeMember(int institutionId, int entityId) {
    for (auto& inst : institutions) {
        if (inst->id == institutionId) {
            inst->memberIds.erase(
                std::remove(inst->memberIds.begin(), inst->memberIds.end(), entityId),
                inst->memberIds.end());
            break;
        }
    }
    
    auto& memberships = entityMemberships[entityId];
    memberships.erase(
        std::remove(memberships.begin(), memberships.end(), institutionId),
        memberships.end());
}

float InstitutionalSystem::getInstitutionalSupport(int entityId) const {
    auto it = entityMemberships.find(entityId);
    if (it == entityMemberships.end()) return 0.0f;
    
    float totalSupport = 0.0f;
    for (int instId : it->second) {
        for (const auto& inst : institutions) {
            if (inst->id == instId) {
                totalSupport += inst->legitimacy;
                break;
            }
        }
    }
    
    return totalSupport / it->second.size();
}

std::vector<std::shared_ptr<Institution>> 
InstitutionalSystem::getEntityInstitutions(int entityId) const {
    std::vector<std::shared_ptr<Institution>> result;
    
    auto it = entityMemberships.find(entityId);
    if (it == entityMemberships.end()) return result;
    
    for (int instId : it->second) {
        for (const auto& inst : institutions) {
            if (inst->id == instId) {
                result.push_back(inst);
                break;
            }
        }
    }
    
    return result;
}

float InstitutionalSystem::enforceNorms(int entityId, const std::string& action) {
    float totalPressure = 0.0f;
    
    auto it = entityMemberships.find(entityId);
    if (it != entityMemberships.end()) {
        for (int instId : it->second) {
            for (const auto& inst : institutions) {
                if (inst->id == instId) {
                    totalPressure += inst->enforceRule(action, entityId);
                }
            }
        }
    }
    
    return totalPressure;
}

void InstitutionalSystem::update(float deltaTime) {
    for (auto& inst : institutions) {
        inst->updateLegitimacy();
        inst->distributeResources();
    }
}

size_t InstitutionalSystem::getInstitutionCount() const {
    return institutions.size();
}

size_t InstitutionalSystem::getMembershipCount(int entityId) const {
    auto it = entityMemberships.find(entityId);
    return (it != entityMemberships.end()) ? it->second.size() : 0;
}

// ============= EnvironmentalFeedbackSystem Implementation =============

EnvironmentalFeedbackSystem::EnvironmentalFeedbackSystem(EnvironmentalState* envState)
    : state(envState) {}

void EnvironmentalFeedbackSystem::addFeedbackLoop(const FeedbackLoop& loop) {
    loops.push_back(loop);
}

void EnvironmentalFeedbackSystem::update(float deltaTime) {
    for (auto& loop : loops) {
        float sensorValue = loop.sensor(*state);
        loop.accumulatedEffect += sensorValue * loop.gain * deltaTime;
        
        // Apply delay
        if (loop.accumulatedEffect >= loop.delay) {
            loop.effect(*state, loop.accumulatedEffect);
            loop.accumulatedEffect = 0.0f;
        }
    }
}

void EnvironmentalFeedbackSystem::setupPopulationPressureLoop() {
    FeedbackLoop loop;
    loop.name = "population_pressure";
    loop.positive = false;  // Balancing
    
    loop.sensor = [](const EnvironmentalState& state) -> float {
        // Would measure population density
        return 0.5f;
    };
    
    loop.effect = [](EnvironmentalState& state, float effect) {
        // Adjust resource availability based on pressure
    };
    
    loop.gain = 0.1f;
    loop.delay = 10.0f;
    
    addFeedbackLoop(loop);
}

void EnvironmentalFeedbackSystem::setupResourceDepletionLoop() {
    FeedbackLoop loop;
    loop.name = "resource_depletion";
    loop.positive = false;
    
    loop.sensor = [](const EnvironmentalState& state) -> float {
        // Measure resource levels
        return 0.5f;
    };
    
    loop.effect = [](EnvironmentalState& state, float effect) {
        // Adjust regeneration rates
    };
    
    addFeedbackLoop(loop);
}

void EnvironmentalFeedbackSystem::setupClimateFeedbackLoop() {
    FeedbackLoop loop;
    loop.name = "climate_feedback";
    loop.positive = true;  // Reinforcing (ice-albedo etc.)
    
    loop.sensor = [](const EnvironmentalState& state) -> float {
        return state.globalTemperature / 100.0f;
    };
    
    loop.effect = [](EnvironmentalState& state, float effect) {
        state.globalTemperature += effect * 0.01f;
    };
    
    loop.gain = 0.05f;
    loop.delay = 100.0f;
    
    addFeedbackLoop(loop);
}

// ============= WorldEnvironment Implementation =============

WorldEnvironment::WorldEnvironment(float size, const std::string& climate)
    : worldSize(size), climateZone(climate),
      feedbackSystem(&state) {
    
    // Setup default feedback loops
    feedbackSystem.setupPopulationPressureLoop();
    feedbackSystem.setupResourceDepletionLoop();
    feedbackSystem.setupClimateFeedbackLoop();
}

void WorldEnvironment::tick(float deltaTime) {
    state.advanceTime(deltaTime);
    resourceManager.update(deltaTime, state);
    cultureSystem.updateCulturalTraits(deltaTime);
    institutionalSystem.update(deltaTime);
    feedbackSystem.update(deltaTime);
}

void WorldEnvironment::advanceDay() {
    tick(24.0f);
}

void WorldEnvironment::advanceSeason() {
    tick(24.0f * 90);  // ~90 days per season
}

float WorldEnvironment::getTemperatureAt(float x, float y) const {
    // Base temperature from season
    float baseTemp = state.currentSeason.temperature;
    
    // Modify by position (latitude simulation)
    float latitudeFactor = std::abs(y - worldSize/2) / (worldSize/2);
    baseTemp -= latitudeFactor * 20.0f;
    
    return baseTemp;
}

float WorldEnvironment::getResourceDensity(ResourceType type, 
                                            float x, float y, 
                                            float radius) const {
    auto nearby = const_cast<ResourceManager&>(resourceManager)
        .findNearbyResources(x, y, 0, radius, type);
    
    float totalQuantity = 0.0f;
    for (const auto& resource : nearby) {
        totalQuantity += resource->quantity;
    }
    
    float area = 3.14159f * radius * radius;
    return totalQuantity / area;
}

void WorldEnvironment::saveState(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    file << "WorldEnvironment\n";
    file << "climate=" << climateZone << "\n";
    file << "day=" << state.dayOfYear << "\n";
    file << "time=" << state.timeOfDay << "\n";
    file << "season=" << static_cast<int>(state.currentSeason.season) << "\n";
    
    // Would save resources, cultural groups, institutions
}

void WorldEnvironment::loadState(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            
            if (key == "climate") climateZone = value;
            else if (key == "day") state.dayOfYear = std::stoi(value);
            else if (key == "time") state.timeOfDay = std::stoi(value);
            else if (key == "season") {
                state.currentSeason.season = static_cast<Season>(std::stoi(value));
            }
        }
    }
}

} // namespace environment
