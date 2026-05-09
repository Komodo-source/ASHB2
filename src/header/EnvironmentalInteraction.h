#ifndef ENVIRONMENTAL_INTERACTION_H
#define ENVIRONMENTAL_INTERACTION_H

#include <string>
#include <vector>
#include <map>
#include <memory>

class Entity;

// Resource types in environment
enum class ResourceType {
    FOOD,
    WATER,
    SHELTER,
    MONEY,
    ENERGY,
    MATERIALS,
    INFORMATION,
    SOCIAL_CAPITAL
};

struct ResourceNode {
    int nodeId;
    std::string name;
    ResourceType type;
    
    float currentAmount;
    float maxAmount;
    float regenerationRate;
    
    float posX, posY;
    
    // Ownership/access rights
    int ownerId = -1;  // -1 = unowned/public
    std::vector<int> accessList;
    
    // Depletion state
    bool isDepleted = false;
    float depletionLevel = 0.0f;
    
    void harvest(float amount);
    void regenerate(float deltaTime);
};

struct SpatialPreference {
    // Preferred environmental features
    float preferenceCrowding = 50.0f;    // Liked crowding level
    float preferenceNoise = 30.0f;        // Liked noise level
    float preferenceNature = 60.0f;       // Preference for natural settings
    float preferenceFamiliarity = 50.0f;  // Prefer familiar places
    
    // Place attachment
    std::map<int, float> placeAttachments;  // locationId -> attachment strength
    
    // Territoriality
    float territoriality = 50.0f;
    int homeLocationId = -1;
    
    void updateAttachment(int locationId, float experience);
};

struct TechnologyAdoption {
    std::string technologyName;
    float perceivedUsefulness = 50.0f;
    float perceivedEaseOfUse = 50.0f;
    float socialInfluence = 50.0f;
    float facilitatingConditions = 50.0f;
    
    // Adoption stage (Rogers' diffusion)
    enum AdoptionStage {
        AWARENESS,
        INTEREST,
        EVALUATION,
        TRIAL,
        ADOPTION,
        REJECTION
    };
    
    AdoptionStage stage = AWARENESS;
    int daysInStage = 0;
    
    // Innovativeness trait
    float innovativeness = 50.0f;
    
    void evaluateTechnology(const std::vector<Entity*>& peers);
    bool shouldAdopt() const;
};

struct InstitutionalParticipation {
    int institutionId;
    std::string institutionType;  // "government", "religious", "educational", etc.
    
    float participationLevel = 0.0f;  // 0-100
    float trustInInstitution = 50.0f;
    float complianceLevel = 50.0f;
    
    int membershipDuration = 0;
    float satisfaction = 50.0f;
    
    // Role within institution
    std::string role = "member";
    float influence = 10.0f;
    
    // Benefits received
    float benefitsReceived = 0.0f;
    float costsPaid = 0.0f;
    
    void updateParticipation(float experience);
    void evaluateContinuedMembership();
};

struct EnvironmentalBelief {
    std::string beliefTopic;
    float beliefStrength = 50.0f;  // -100 to 100
    
    // Sources of belief
    float personalExperienceWeight = 0.4f;
    float socialInfluenceWeight = 0.3f;
    float mediaInfluenceWeight = 0.3f;
    
    void updateFromExperience(float experienceValence);
    void updateFromSocial(const std::vector<Entity*>& peers);
};

struct ConsumptionPattern {
    std::string category;  // "food", "entertainment", "housing", etc.
    
    float consumptionRate = 50.0f;
    float qualityPreference = 50.0f;
    float priceSensitivity = 50.0f;
    
    // Habitual brands/types
    std::vector<std::string> preferredOptions;
    
    // Sustainability orientation
    float environmentalConcern = 50.0f;
    
    void makeConsumptionChoice(const std::vector<std::string>& options);
};

class EnvironmentalInteractionSystem {
private:
    std::map<int, std::shared_ptr<ResourceNode>> resources;
    std::map<int, SpatialPreference> spatialPreferences;
    std::map<int, std::vector<TechnologyAdoption>> technologyProfiles;
    std::map<int, std::vector<InstitutionalParticipation>> institutionalMemberships;
    std::map<int, std::map<std::string, EnvironmentalBelief>> environmentalBeliefs;
    std::map<int, std::vector<ConsumptionPattern>> consumptionPatterns;
    
    int nextResourceId = 1;
    int nextInstitutionId = 1;
    
public:
    EnvironmentalInteractionSystem();
    
    // Resource management
    ResourceNode* createResource(const std::string& name,
                                ResourceType type,
                                float amount,
                                float posX, float posY);
    
    ResourceNode* getResource(int resourceId);
    void harvestResource(int entityId, int resourceId, float amount);
    void updateResources(float deltaTime);
    
    // Spatial behavior
    SpatialPreference& getSpatialPreference(int entityId);
    void updateSpatialPreference(int entityId, int locationId, float experience);
    int selectLocation(Entity* entity, 
                      const std::vector<int>& availableLocations);
    
    // Technology adoption
    void introduceTechnology(int entityId, const std::string& techName);
    void updateTechnologyAdoption(int entityId, float deltaTime);
    bool hasAdoptedTechnology(int entityId, const std::string& techName);
    
    // Institutional participation
    InstitutionalParticipation* joinInstitution(int entityId,
                                               const std::string& type,
                                               const std::string& role);
    void updateInstitutionalParticipation(int entityId, float deltaTime);
    
    // Environmental beliefs
    void updateEnvironmentalBeliefs(int entityId,
                                   const std::string& topic,
                                   float experience);
    
    // Consumption
    ConsumptionPattern* getConsumptionPattern(int entityId,
                                             const std::string& category);
    void makeConsumptionDecision(int entityId,
                                const std::string& category,
                                const std::vector<std::string>& options);
    
    // Competition and conflict over resources
    void resolveResourceCompetition(int entityId1,
                                   int entityId2,
                                   int resourceId);
    
    // Accessors
    const std::map<int, std::shared_ptr<ResourceNode>>& getResources() const;
    
    // Serialization
    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
};

#endif // ENVIRONMENTAL_INTERACTION_H
