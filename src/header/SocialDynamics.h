#ifndef SOCIAL_DYNAMICS_H
#define SOCIAL_DYNAMICS_H

#include <string>
#include <vector>
#include <map>
#include <set>

class Entity;

// Group membership and identity
enum class GroupType {
    FAMILY,
    FRIENDSHIP,
    WORK,
    RELIGIOUS,
    RECREATIONAL,
    POLITICAL,
    ETHNIC,
    AGE_COHORT,
    INTEREST_BASED
};

struct SocialGroup {
    int groupId;
    std::string name;
    GroupType type;
    std::set<int> memberIds;
    
    // Group characteristics
    float cohesion = 50.0f;        // How bonded members are
    float entitativity = 50.0f;    // How much perceived as unified entity
    float prestige = 50.0f;        // Social status of group
    float openness = 50.0f;        // How easy to join
    
    // Group norms
    std::map<std::string, float> normStrengths;  // action -> strength
    
    // Shared beliefs/values
    std::map<std::string, float> sharedBeliefs;
    
    void addMember(int entityId);
    void removeMember(int entityId);
    float calculateNormPressure(const std::string& action) const;
};

struct SocialIdentity {
    int groupId;
    float identificationStrength = 50.0f;  // How central to self-concept
    float typicality = 50.0f;              // How prototypical member I am
    float satisfaction = 50.0f;            // How satisfied with membership
    float commitment = 50.0f;              // Likelihood to stay
    
    // Identity salience in current context
    float currentSalience = 0.0f;
    
    void updateFromInteraction(bool positiveExperience);
};

struct StatusPerception {
    int perceiverId;
    int targetId;
    
    float perceivedStatus = 50.0f;
    float perceivedDominance = 50.0f;
    float perceivedPrestige = 50.0f;
    float perceivedCompetence = 50.0f;
    float perceivedWarmth = 50.0f;
    
    // Respect and liking components
    float respect = 50.0f;
    float liking = 50.0f;
    
    void updateFromBehavior(const std::string& behaviorType, float intensity);
};

struct Reputation {
    int entityId;
    
    // Dimensional reputation
    float trustworthiness = 50.0f;
    float competence = 50.0f;
    float generosity = 50.0f;
    float honesty = 50.0f;
    float loyalty = 50.0f;
    
    // Gossip propagation
    std::map<int, float> gossipReceived;  // from entityId -> amount
    int gossipCount = 0;
    
    // Reputation volatility
    float stability = 70.0f;  // How resistant to change
    
    void updateFromAction(const std::string& actionType, 
                         float visibility,
                         bool isPositive);
    
    void spreadGossip(int gossiperId, int listenerId, float gossipAmount);
};

struct RelationshipQuality {
    int entityA;
    int entityB;
    
    // Sternberg's triangular theory components
    float intimacy = 0.0f;
    float passion = 0.0f;
    float commitment = 0.0f;
    
    // Attachment-specific dynamics
    float securityLevel = 50.0f;
    float anxietyLevel = 50.0f;
    float avoidanceLevel = 50.0f;
    
    // Interaction history
    int totalInteractions = 0;
    float positiveRatio = 50.0f;  // Positivity ratio (Gottman: 5:1 for healthy)
    float conflictFrequency = 0.0f;
    float repairSuccessRate = 50.0f;
    
    // Power dynamics
    float powerBalance = 50.0f;  // 50 = equal, <50 = A has more power
    
    // Relationship stage
    int relationshipStage = 0;  // 0=strangers, 1=acquaintances, 2=friends, 3=close, 4=intimate
    
    void updateFromInteraction(const std::string& interactionType,
                              float intensity,
                              bool isPositive);
    
    float calculateRelationshipStrength() const;
    bool isRelationshipViable() const;
};

struct SocialNetwork {
    std::map<int, std::shared_ptr<SocialGroup>> groups;
    std::map<int, std::map<int, RelationshipQuality>> relationships;
    std::map<int, Reputation> reputations;
    
    int nextGroupId = 1;
    
    // Create or get group
    SocialGroup* getOrCreateGroup(const std::string& name, GroupType type);
    
    // Relationship management
    RelationshipQuality* getRelationship(int idA, int idB);
    void createRelationship(int idA, int idB);
    
    // Network analysis
    std::vector<int> findMutualConnections(int idA, int idB);
    float calculateSocialDistance(int idA, int idB);
    std::vector<int> getInGroupMembers(int entityId);
    
    // Gossip propagation
    void propagateGossip(int sourceId, int targetId, 
                        const std::string& content,
                        float intensity);
    
    // Norm enforcement
    float calculateNormViolationCost(int entityId, 
                                    const std::string& action,
                                    int groupId);
};

class SocialDynamicsSystem {
private:
    SocialNetwork network;
    std::map<int, std::vector<SocialIdentity>> entityIdentities;
    
public:
    SocialDynamicsSystem();
    
    // Initialize social structures
    void initializeEntity(Entity* entity);
    
    // Group dynamics
    void updateGroupCohesion(int groupId);
    void processGroupEntry(int entityId, int groupId);
    void processGroupExit(int entityId, int groupId);
    
    // Relationship dynamics
    void updateRelationship(Entity* entityA, Entity* entityB,
                           const std::string& interactionType,
                           float intensity,
                           bool isPositive);
    
    // Status dynamics
    void updateStatusPerceptions(Entity* actor,
                                const std::vector<Entity*>& observers,
                                const std::string& behaviorType);
    
    // Gossip and reputation
    void processGossip(Entity* gossiper, Entity* listener,
                      int targetId, const std::string& content);
    
    // Norm enforcement
    void enforceNorms(Entity* actor, 
                     const std::vector<Entity*>& observers,
                     const std::string& action);
    
    // In-group/out-group effects
    float calculateInGroupBias(Entity* perceiver, Entity* target);
    
    // Social influence
    float calculateConformityPressure(Entity* entity,
                                     int groupId,
                                     const std::string& behavior);
    
    // Accessors
    SocialNetwork& getNetwork() { return network; }
    const SocialNetwork& getNetwork() const { return network; }
    
    // Serialization
    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
};

#endif // SOCIAL_DYNAMICS_H
