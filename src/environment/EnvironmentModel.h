#ifndef ENVIRONMENT_MODEL_H
#define ENVIRONMENT_MODEL_H

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <functional>
#include <cmath>

namespace environment {

// Seasonal configuration
enum class Season {
    SPRING,
    SUMMER,
    AUTUMN,
    WINTER
};

struct SeasonalConfig {
    Season season;
    float temperature;      // 0-100
    float daylightHours;    // 0-24
    float precipitation;    // 0-100
    float resourceModifier; // Multiplier for resource availability
    
    std::string toString() const;
    static SeasonalConfig fromMonth(int month);
};

// Resource types
enum class ResourceType {
    FOOD,
    WATER,
    SHELTER,
    INFORMATION,
    ENERGY,
    MATERIALS
};

struct ResourceNode {
    int id;
    ResourceType type;
    float quantity;
    float maxQuantity;
    float regenerationRate;
    float x, y, z;  // Position
    bool renewable;
    int ownerId;    // -1 if unowned
    
    ResourceNode();
    ResourceNode(ResourceType t, float qty, float regen, 
                float px, float py, float pz);
    
    void update(float deltaTime);
    float harvest(float amount);
    bool isDepleted() const;
};

// Environmental state
struct EnvironmentalState {
    SeasonalConfig currentSeason;
    float globalTemperature;
    float humidity;
    float airQuality;
    float ambientNoise;
    int timeOfDay;        // 0-23 hours
    int dayOfYear;        // 1-365
    std::map<std::string, float> regionalModifiers;
    
    EnvironmentalState();
    void advanceTime(float hours);
    void updateSeasonalEffects();
};

// Resource dynamics manager
class ResourceManager {
private:
    std::vector<std::shared_ptr<ResourceNode>> resources;
    std::map<ResourceType, float> globalAvailability;
    float competitionFactor;
    
public:
    ResourceManager();
    
    void addResource(std::shared_ptr<ResourceNode> resource);
    void removeResource(int resourceId);
    
    // Find nearby resources
    std::vector<std::shared_ptr<ResourceNode>> findNearbyResources(
        float x, float y, float z, float radius, 
        ResourceType type = ResourceType::FOOD);
    
    // Harvest resource
    float harvestResource(int resourceId, int harvesterId, float amount);
    
    // Claim/release resources
    bool claimResource(int resourceId, int entityId);
    void releaseResource(int resourceId);
    
    // Update all resources
    void update(float deltaTime, const EnvironmentalState& env);
    
    // Get statistics
    float getTotalResource(ResourceType type) const;
    float getAvailabilityIndex(ResourceType type) const;
    
    // Competition modeling
    void setCompetitionFactor(float factor);
    float calculateCompetitionCost(int entityId, ResourceType type);
};

// Cultural transmission system
struct CulturalTrait {
    std::string name;
    std::string category;  // "belief", "practice", "norm", "skill"
    float prevalence;      // 0-1 in population
    float transmissionRate;
    float mutationRate;
    std::vector<std::string> prerequisites;
    std::function<float(float)> fitnessFunction;  // How trait affects survival
};

struct CulturalGroup {
    int id;
    std::string name;
    std::vector<CulturalTrait> traits;
    std::vector<int> memberIds;
    float cohesion;      // 0-1
    float openness;      // Willingness to adopt external traits
    std::map<int, float> memberInfluence;  // Entity ID -> influence level
    
    void updateTraits();
    float calculateTraitAdoption(const CulturalTrait& trait);
};

class CulturalTransmissionSystem {
private:
    std::vector<CulturalGroup> groups;
    std::map<int, int> entityToGroup;  // Entity ID -> Group ID
    std::vector<CulturalTrait> traitLibrary;
    
    float verticalTransmissionRate;   // Parent to child
    float horizontalTransmissionRate; // Peer to peer
    float obliqueTransmissionRate;    // Elder to young
    
public:
    CulturalTransmissionSystem();
    
    void addGroup(const CulturalGroup& group);
    void assignEntityToGroup(int entityId, int groupId);
    
    // Transmission events
    void transmitVertically(int parentId, int childId);
    void transmitHorizontally(int sourceId, int targetId);
    void transmitObliquely(int elderId, int youthId);
    
    // Cultural evolution
    void updateCulturalTraits(float deltaTime);
    
    // Innovation and mutation
    void introduceInnovation(int groupId, const CulturalTrait& trait);
    
    // Get cultural state of entity
    std::vector<CulturalTrait> getEntityTraits(int entityId) const;
    
    // Calculate cultural distance between entities
    float calculateCulturalDistance(int entityId1, int entityId2) const;
};

// Institutional structures
enum class InstitutionType {
    FAMILY,
    EDUCATION,
    GOVERNMENT,
    RELIGION,
    ECONOMY,
    HEALTHCARE
};

struct Institution {
    int id;
    InstitutionType type;
    std::string name;
    std::vector<int> memberIds;
    std::map<std::string, float> rules;
    std::map<std::string, float> resources;
    float legitimacy;    // 0-1, how much members accept the institution
    float efficiency;    // How well it achieves its purpose
    int foundingDay;
    
    void updateLegitimacy();
    float enforceRule(const std::string& ruleName, int entityId);
    void distributeResources();
};

class InstitutionalSystem {
private:
    std::vector<std::shared_ptr<Institution>> institutions;
    std::map<int, std::vector<int>> entityMemberships;  // Entity -> Institutions
    
public:
    InstitutionalSystem();
    
    void createInstitution(InstitutionType type, const std::string& name);
    void addMember(int institutionId, int entityId);
    void removeMember(int institutionId, int entityId);
    
    // Institutional effects on entities
    float getInstitutionalSupport(int entityId) const;
    std::vector<std::shared_ptr<Institution>> getEntityInstitutions(int entityId) const;
    
    // Rule enforcement
    float enforceNorms(int entityId, const std::string& action);
    
    // Update all institutions
    void update(float deltaTime);
    
    // Statistics
    size_t getInstitutionCount() const;
    size_t getMembershipCount(int entityId) const;
};

// Environmental feedback loops
struct FeedbackLoop {
    std::string name;
    bool positive;  // true = reinforcing, false = balancing
    
    std::function<float(const EnvironmentalState&)> sensor;
    std::function<void(EnvironmentalState&, float)> effect;
    
    float gain;
    float delay;
    float accumulatedEffect;
};

class EnvironmentalFeedbackSystem {
private:
    std::vector<FeedbackLoop> loops;
    EnvironmentalState* state;
    
public:
    explicit EnvironmentalFeedbackSystem(EnvironmentalState* envState);
    
    void addFeedbackLoop(const FeedbackLoop& loop);
    void update(float deltaTime);
    
    // Common feedback loops
    void setupPopulationPressureLoop();
    void setupResourceDepletionLoop();
    void setupClimateFeedbackLoop();
};

// World simulation
class WorldEnvironment {
private:
    EnvironmentalState state;
    ResourceManager resourceManager;
    CulturalTransmissionSystem cultureSystem;
    InstitutionalSystem institutionalSystem;
    EnvironmentalFeedbackSystem feedbackSystem;
    
    float worldSize;
    std::string climateZone;
    
public:
    WorldEnvironment(float size = 1000.0f, 
                    const std::string& climate = "temperate");
    
    // Time progression
    void tick(float deltaTime);
    void advanceDay();
    void advanceSeason();
    
    // Accessors
    EnvironmentalState& getState() { return state; }
    const EnvironmentalState& getState() const { return state; }
    ResourceManager& getResources() { return resourceManager; }
    CulturalTransmissionSystem& getCulture() { return cultureSystem; }
    InstitutionalSystem& getInstitutions() { return institutionalSystem; }
    
    // Environmental queries
    float getTemperatureAt(float x, float y) const;
    float getResourceDensity(ResourceType type, float x, float y, float radius) const;
    Season getCurrentSeason() const { return state.currentSeason.season; }
    
    // Serialization
    void saveState(const std::string& filename) const;
    void loadState(const std::string& filename);
};

} // namespace environment

#endif // ENVIRONMENT_MODEL_H
