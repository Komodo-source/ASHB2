#ifndef EMOTIONAL_COMPLEXITY_H
#define EMOTIONAL_COMPLEXITY_H

#include <string>
#include <vector>
#include <map>

class Entity;

// Basic emotions (Ekman's six + additional)
enum class BasicEmotion {
    JOY,
    SADNESS,
    ANGER,
    FEAR,
    DISGUST,
    SURPRISE,
    CONTEMPT,
    SHAME,
    GUILT,
    PRIDE,
    ENVY,
    GRATITUDE,
    HOPE,
    DESPAIR
};

struct EmotionDimension {
    float valence = 0.0f;      // -100 to 100 (negative to positive)
    float arousal = 0.0f;      // 0 to 100 (calm to excited)
    float dominance = 0.0f;    // 0 to 100 (submissive to dominant)
};

struct EmotionalEpisode {
    BasicEmotion emotion;
    float intensity = 0.0f;    // 0 to 100
    int onsetTime = 0;
    int peakTime = 0;
    int offsetTime = 0;
    
    std::string cause;
    int causingEntityId = -1;
    
    bool isResolved = false;
    
    // Appraisal components that caused this emotion
    float appraisalRelevance = 0.5f;
    float appraisalDesirability = 0.5f;
    float appraisalCoping = 0.5f;
    float appraisalControl = 0.5f;
    float appraisalNormCompatibility = 0.5f;
    
    void update(int currentTime);
    float getCurrentIntensity() const;
};

struct MoodState {
    // Longer-lasting affective states
    float overallValence = 50.0f;   // Baseline mood
    float irritability = 0.0f;      // Prone to anger
    float anxiety = 0.0f;           // Prone to fear/worry
    float melancholy = 0.0f;        // Prone to sadness
    float enthusiasm = 50.0f;       // Prone to joy
    
    // Duration in simulation ticks
    int duration = 0;
    int maxDuration = 100;
    
    void update(float deltaTime);
    void modulateFromEmotion(const EmotionalEpisode& emotion);
};

struct EmotionalRegulation {
    // Strategies entity uses to manage emotions
    
    // Cognitive reappraisal ability
    float reappraisalSkill = 50.0f;
    
    // Suppression tendency
    float suppressionTendency = 30.0f;
    float suppressionCost = 0.0f;  // Accumulated psychological cost
    
    // Rumination tendency
    float ruminationTendency = 30.0f;
    
    // Expression control
    float expressionControl = 50.0f;
    
    // Seeking social support
    float supportSeeking = 50.0f;
    
    // Problem-focused coping
    float problemFocusedCoping = 50.0f;
    
    // Emotion-focused coping
    float emotionFocusedCoping = 50.0f;
    
    float calculateRegulationEffectiveness(const EmotionalEpisode& emotion);
    void applyRegulationStrategy(EmotionalEpisode& emotion, Entity* entity);
};

struct EmotionalContagionSusceptibility {
    float susceptibility = 50.0f;
    float empathyLevel = 50.0f;
    float emotionalResilience = 50.0f;
    
    // Specific susceptibilities
    float angerContagion = 40.0f;
    float joyContagion = 60.0f;
    float anxietyContagion = 50.0f;
    float sadnessContagion = 45.0f;
};

struct AffectiveStyle {
    // Davidson's affective style dimensions
    
    // Resilience: speed of recovery from negative events
    float resilienceSpeed = 50.0f;
    
    // Outlook: ability to sustain positive emotion
    float positiveOutlook = 50.0f;
    
    // Social intuition: sensitivity to social signals
    float socialIntuition = 50.0f;
    
    // Self-awareness: awareness of own feelings
    float selfAwareness = 50.0f;
    
    // Context sensitivity: appropriateness to context
    float contextSensitivity = 50.0f;
    
    // Attention: ability to focus attention emotionally
    float attentionalControl = 50.0f;
};

class EmotionalComplexitySystem {
private:
    std::map<int, std::vector<EmotionalEpisode>> activeEmotions;
    std::map<int, MoodState> moodStates;
    std::map<int, EmotionalRegulation> regulationStrategies;
    std::map<int, EmotionalContagionSusceptibility> contagionProfiles;
    std::map<int, AffectiveStyle> affectiveStyles;
    
public:
    EmotionalComplexitySystem();
    
    // Initialize for entity
    void initializeEntity(Entity* entity);
    
    // Emotion generation from appraisal
    EmotionalEpisode generateEmotion(Entity* entity,
                                    const std::string& event,
                                    float relevance,
                                    float desirability,
                                    float coping,
                                    float control,
                                    float normCompatibility,
                                    int causingEntityId = -1);
    
    // Add emotion to entity's state
    void addEmotion(int entityId, const EmotionalEpisode& emotion);
    
    // Update all emotions for entity
    void updateEmotions(int entityId, int currentTime);
    
    // Get current emotional state
    float getCurrentEmotionIntensity(int entityId, BasicEmotion emotion);
    EmotionDimension getOverallEmotionalState(int entityId);
    
    // Mood management
    void updateMood(int entityId, float deltaTime);
    MoodState& getMood(int entityId);
    
    // Emotional regulation
    void applyRegulation(int entityId, EmotionalEpisode& emotion);
    
    // Emotional contagion between entities
    void processEmotionalContagion(Entity* target,
                                  const std::vector<Entity*>& nearby);
    
    // Secondary emotions (meta-emotions)
    void generateMetaEmotions(Entity* entity);
    
    // Accessors
    const std::vector<EmotionalEpisode>& getActiveEmotions(int entityId) const;
    const EmotionalRegulation& getRegulation(int entityId) const;
    const AffectiveStyle& getAffectiveStyle(int entityId) const;
    
    // Serialization
    void saveTo(std::ofstream& file) const;
    void loadFrom(std::ifstream& file);
};

#endif // EMOTIONAL_COMPLEXITY_H
