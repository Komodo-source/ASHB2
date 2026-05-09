#ifndef LIFE_COURSE_H
#define LIFE_COURSE_H

#include <string>
#include <vector>
#include <map>
#include "./Entity.h"

class Entity;

// Career and achievement tracking
struct CareerPath {
    std::string currentOccupation = "unemployed";
    int occupationLevel = 0;      // 0-10 seniority
    float jobSatisfaction = 50.0f;
    float jobStress = 30.0f;
    
    std::vector<std::string> pastOccupations;
    std::vector<float> pastSatisfactions;
    
    int yearsInField = 0;
    int yearsInCurrentRole = 0;
    
    float ambition = 50.0f;
    float workLifeBalance = 50.0f;
    
    void changeOccupation(const std::string& newOccupation);
    void updateSatisfaction(float experience);
};

struct SkillPortfolio {
    std::map<std::string, float> skills;  // skill -> proficiency
    std::map<std::string, int> skillAges; // days since last use
    
    void addSkill(const std::string& name, float initialProficiency = 10.0f);
    void practiceSkill(const std::string& name, float quality);
    void decayUnusedSkills(float deltaTime);
};

// Relationship lifecycle
enum class RelationshipStage {
    STRANGERS,
    ACQUAINTANCES,
    CASUAL_FRIENDS,
    CLOSE_FRIENDS,
    INTIMATE_PARTNERS,
    COMMITTED_PARTNERS,
    MARRIED,
    PARENTS_TOGETHER,
    EMPTY_NEST,
    RETIRED_TOGETHER,
    BEREAVED,
    SEPARATED,
    DIVORCED
};

struct RelationshipLifecycle {
    int partnerId;
    RelationshipStage stage = RelationshipStage::STRANGERS;
    
    int daysInStage = 0;
    int totalDaysTogether = 0;
    
    // Stage transition conditions
    float intimacyThreshold = 30.0f;
    float commitmentThreshold = 60.0f;
    float interdependenceThreshold = 80.0f;
    
    // Satisfaction tracking
    float currentSatisfaction = 50.0f;
    float averageSatisfaction = 50.0f;
    
    // Conflict patterns
    float unresolvedConflicts = 0.0f;
    float repairAttempts = 0.0f;
    
    void updateStage();
    bool canAdvanceStage() const;
    bool shouldDeteriorate() const;
};

struct ParentingStyle {
    // Baumrind's parenting styles dimensions
    float warmth = 50.0f;         // Responsiveness to child's needs
    float control = 50.0f;        // Demandingness/expectations
    float autonomy_granting = 50.0f;
    
    // Derived style
    enum StyleType {
        AUTHORITATIVE,  // High warmth, high control
        AUTHORITARIAN,  // Low warmth, high control
        PERMISSIVE,     // High warmth, low control
        NEGLECTFUL      // Low warmth, low control
    };
    
    StyleType getStyle() const;
    
    // Parenting behaviors
    float involvement = 50.0f;
    float consistency = 50.0f;
    float supportiveness = 50.0f;
    
    // Child outcomes (tracked by parent)
    std::map<int, float> childOutcomes;  // childId -> outcome measure
};

struct LifeEvent {
    std::string eventType;
    int simulationDay;
    float impactMagnitude;  // -100 to 100
    float stressfulness;    // 0 to 100
    float desirability;     // 0 to 100
    
    std::string description;
    std::vector<int> involvedEntities;
    
    bool isProcessed = false;
    
    // Long-term effects
    float personalityImpact[5] = {0, 0, 0, 0, 0};  // Big Five changes
};

struct DevelopmentalMilestones {
    // Age-appropriate milestones
    std::map<std::string, bool> achievedMilestones;
    
    // Timing
    float milestoneTiming;  // <1 = early, >1 = late
    
    // Effects of on-time vs off-time transitions
    float developmentalSynchrony = 50.0f;
    
    void checkMilestone(const std::string& milestone, 
                       float entityAge,
                       LifeStage lifeStage);
};

struct AgingEffects {
    // Physical aging
    float physicalDecline = 0.0f;
    float cognitiveDecline = 0.0f;
    
    // Psychological aging
    float wisdomAccumulation = 0.0f;
    float emotionalComplexity = 50.0f;
    
    // Socioemotional selectivity
    float timeHorizon = 100.0f;  // Perceived time left
    float presentFocus = 30.0f;   // Focus on present vs future
    
    // Generativity vs stagnation (Erikson)
    float generativity = 50.0f;   // Contributing to next generation
    
    void updateAging(float age, float deltaTime);
};

class LifeCourseSystem {
private:
    std::map<int, CareerPath> careers;
    std::map<int, std::vector<RelationshipLifecycle>> relationships;
    std::map<int, ParentingStyle> parentingStyles;
    std::map<int, std::vector<LifeEvent>> lifeEvents;
    std::map<int, DevelopmentalMilestones> milestones;
    std::map<int, AgingEffects> agingProfiles;
    
public:
    LifeCourseSystem();
    
    // Initialize for entity
    void initializeEntity(Entity* entity);
    
    // Career management
    CareerPath& getCareer(int entityId);
    void updateCareer(int entityId, float workExperience);
    void evaluateCareerChange(int entityId);
    
    // Relationship lifecycle
    RelationshipLifecycle* getRelationship(int entityId, int partnerId);
    void createRelationship(int entityId, int partnerId);
    void updateRelationship(int entityId, int partnerId,
                           float interactionQuality);
    void processRelationshipTransitions(int entityId);
    
    // Parenting
    ParentingStyle& getParentingStyle(int entityId);
    void updateParenting(Entity* parent, Entity* child,
                        const std::string& interaction);
    
    // Life events
    void recordLifeEvent(int entityId, const LifeEvent& event);
    void processLifeEventEffects(int entityId);
    std::vector<LifeEvent> getRecentEvents(int entityId, int window);
    
    // Developmental milestones
    void checkMilestones(int entityId, float age, LifeStage stage);
    
    // Aging
    void updateAging(int entityId, float age, float deltaTime);
    const AgingEffects& getAgingProfile(int entityId) const;
    
    // Life satisfaction calculation
    float calculateLifeSatisfaction(int entityId) const;
    
    // Accessors
    const std::vector<LifeEvent>& getLifeEvents(int entityId) const;
    
    // Serialization
    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
};

#endif // LIFE_COURSE_H
