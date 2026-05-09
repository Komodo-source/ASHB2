#ifndef COGNITIVE_ARCHITECTURE_H
#define COGNITIVE_ARCHITECTURE_H

#include <string>
#include <vector>
#include <map>
#include <memory>

class Entity;

// Cognitive biases that filter perception and reasoning
enum class CognitiveBias {
    CONFIRMATION_BIAS,      // Favor information confirming existing beliefs
    AVAILABILITY_HEURISTIC, // Overweight recent/vivid events
    ANCHORING_BIAS,         // Rely too heavily on first piece of information
    FUNDAMENTAL_ATTRIBUTION_ERROR, // Attribute others' behavior to character, not situation
    SELF_SERVING_BIAS,      // Attribute success to self, failure to external
    NEGATIVITY_BIAS,        // Give more weight to negative events
    OPTIMISM_BIAS,          // Underestimate negative outcomes
    BANDWAGON_EFFECT,       // Adopt beliefs because many others do
    HALO_EFFECT             // Overall impression colors specific judgments
};

struct BeliefSystem {
    // Core worldview dimensions (0-100)
    float internalLocusOfControl = 50.0f;  // Believe outcomes controlled by self vs external
    float justWorldBelief = 50.0f;         // Believe world is fundamentally fair
    float trustInInstitutions = 50.0f;
    float spiritualOrientation = 50.0f;
    
    // Specific beliefs (key = belief topic, value = strength -100 to 100)
    std::map<std::string, float> specificBeliefs;
    
    // Belief update parameters
    float beliefPersistence = 0.7f;  // Resistance to changing beliefs
    float opennessToEvidence = 0.5f; // Willingness to update based on evidence
    
    void updateBelief(const std::string& topic, float newEvidence, float evidenceStrength);
    float getBelief(const std::string& topic) const;
};

struct CognitiveStyle {
    // Thinking style preferences
    float analyticVsIntuitive = 50.0f;  // 0 = purely intuitive, 100 = purely analytic
    float needForCognition = 50.0f;     // Enjoyment of effortful thinking
    float toleranceForAmbiguity = 50.0f;
    float cognitiveReflection = 50.0f;  // Tendency to override intuitive responses
    
    // Processing speed vs accuracy tradeoff
    float impulsivity = 50.0f;
    float deliberativeness = 50.0f;
    
    // Attention characteristics
    float attentionalBreadth = 50.0f;   // Broad vs narrow focus
    float distractibility = 30.0f;
};

struct MentalModel {
    // Understanding of how the world works
    std::map<std::string, float> causalBeliefs;  // "action" -> perceived effectiveness
    
    // Theory of mind about specific entities
    std::map<int, float> perceivedTrustworthiness;
    std::map<int, float> perceivedCompetence;
    std::map<int, float> perceivedWarmth;
    
    // Scripts for common situations
    std::map<std::string, std::vector<std::string>> situationalScripts;
    
    void updateCausalBelief(const std::string& action, float outcome);
    float predictOutcome(const std::string& action) const;
};

struct MoralFoundation {
    // Haidt's moral foundations (0-100)
    float careHarm = 50.0f;
    float fairnessCheating = 50.0f;
    float loyaltyBetrayal = 50.0f;
    float authoritySubversion = 50.0f;
    float sanctityDegradation = 50.0f;
    float libertyOppression = 50.0f;
    
    // Personal moral code
    std::vector<std::string> moralRules;
    
    float evaluateActionMorality(const std::string& actionType) const;
};

class CognitiveArchitecture {
private:
    BeliefSystem beliefs;
    CognitiveStyle style;
    MentalModel mentalModel;
    MoralFoundation morals;
    std::vector<CognitiveBias> activeBiases;
    
public:
    CognitiveArchitecture();
    
    // Initialize based on personality and development
    void initializeFromEntity(Entity* entity);
    
    // Perception filtering
    std::vector<std::string> filterPerceptions(
        const std::vector<std::string>& rawPerceptions,
        Entity* entity);
    
    // Interpretation with biases
    float interpretEvent(float objectiveSeverity, 
                        bool isPositive,
                        Entity* entity);
    
    // Reasoning and inference
    std::string generateExplanation(const std::string& event,
                                   Entity* entity);
    
    // Decision support
    float evaluateOption(const std::string& option,
                        const std::map<std::string, float>& attributes,
                        Entity* entity);
    
    // Belief updates from experience
    void updateFromExperience(const std::string& domain,
                             float outcome,
                             Entity* entity);
    
    // Moral reasoning
    bool passesMoralEvaluation(const std::string& action, Entity* entity);
    
    // Accessors
    const BeliefSystem& getBeliefs() const { return beliefs; }
    const CognitiveStyle& getStyle() const { return style; }
    const MentalModel& getMentalModel() const { return mentalModel; }
    const MoralFoundation& getMorals() const { return morals; }
    
    // Serialization
    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
};

#endif // COGNITIVE_ARCHITECTURE_H
