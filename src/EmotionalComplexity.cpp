#include "./header/EmotionalComplexity.h"
#include "./header/Entity.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <random>

// EmotionalEpisode implementation
void EmotionalEpisode::update(int currentTime) {
    if (isResolved) {
        intensity = 0.0f;
        return;
    }
    
    int duration = currentTime - onsetTime;
    
    // Emotion lifecycle: onset -> peak -> decay
    if (duration < peakTime) {
        // Rising phase
        intensity = (static_cast<float>(duration) / peakTime) * 100.0f;
    } else {
        // Decay phase
        float decayProgress = static_cast<float>(duration - peakTime) / (offsetTime - peakTime);
        intensity = std::max(0.0f, 100.0f * (1.0f - decayProgress));
    }
    
    // Check if emotion has naturally expired
    if (currentTime >= offsetTime) {
        isResolved = true;
        intensity = 0.0f;
    }
}

float EmotionalEpisode::getCurrentIntensity() const {
    if (isResolved) return 0.0f;
    return intensity;
}

// MoodState implementation
void MoodState::update(float deltaTime) {
    duration += static_cast<int>(deltaTime);
    
    // Natural regression to baseline
    overallValence = overallValence * 0.99f + 50.0f * 0.01f;
    irritability = irritability * 0.98f;
    anxiety = anxiety * 0.98f;
    melancholy = melancholy * 0.98f;
    enthusiasm = enthusiasm * 0.99f + 50.0f * 0.01f;
    
    // Check duration limit
    if (duration >= maxDuration) {
        // Reset to baseline
        overallValence = 50.0f;
        irritability = 0.0f;
        anxiety = 0.0f;
        melancholy = 0.0f;
        enthusiasm = 50.0f;
        duration = 0;
    }
}

void MoodState::modulateFromEmotion(const EmotionalEpisode& emotion) {
    float intensity = emotion.getCurrentIntensity() / 100.0f;
    
    // Modulate mood based on emotion valence
    switch (emotion.emotion) {
        case BasicEmotion::JOY:
        case BasicEmotion::PRIDE:
        case BasicEmotion::GRATITUDE:
        case BasicEmotion::HOPE:
            overallValence += intensity * 10.0f;
            enthusiasm += intensity * 8.0f;
            break;
            
        case BasicEmotion::SADNESS:
        case BasicEmotion::DESPAIR:
            overallValence -= intensity * 10.0f;
            melancholy += intensity * 8.0f;
            break;
            
        case BasicEmotion::ANGER:
        case BasicEmotion::CONTEMPT:
        case BasicEmotion::ENVY:
            irritability += intensity * 8.0f;
            overallValence -= intensity * 5.0f;
            break;
            
        case BasicEmotion::FEAR:
            anxiety += intensity * 10.0f;
            overallValence -= intensity * 5.0f;
            break;
            
        case BasicEmotion::SHAME:
        case BasicEmotion::GUILT:
            overallValence -= intensity * 8.0f;
            anxiety += intensity * 5.0f;
            break;
            
        default:
            break;
    }
    
    // Clamp values
    overallValence = std::clamp(overallValence, 0.0f, 100.0f);
    irritability = std::clamp(irritability, 0.0f, 100.0f);
    anxiety = std::clamp(anxiety, 0.0f, 100.0f);
    melancholy = std::clamp(melancholy, 0.0f, 100.0f);
    enthusiasm = std::clamp(enthusiasm, 0.0f, 100.0f);
}

// EmotionalRegulation implementation
float EmotionalRegulation::calculateRegulationEffectiveness(const EmotionalEpisode& emotion) {
    float effectiveness = 50.0f;
    
    // Reappraisal is generally most effective
    effectiveness += reappraisalSkill * 0.3f;
    
    // Suppression has costs
    effectiveness -= suppressionCost * 0.2f;
    
    // Problem-focused coping better for controllable situations
    if (emotion.appraisalControl > 50.0f) {
        effectiveness += problemFocusedCoping * 0.2f;
    } else {
        effectiveness += emotionFocusedCoping * 0.2f;
    }
    
    // Rumination reduces effectiveness
    effectiveness -= ruminationTendency * 0.1f;
    
    return std::clamp(effectiveness, 0.0f, 100.0f);
}

void EmotionalRegulation::applyRegulationStrategy(EmotionalEpisode& emotion, Entity* entity) {
    // Choose strategy based on situation and individual tendencies
    float control = emotion.appraisalControl;
    float intensity = emotion.intensity;
    
    if (control > 60.0f && problemFocusedCoping > 50.0f) {
        // Use problem-focused coping for controllable situations
        emotion.appraisalCoping = std::min(100.0f, emotion.appraisalCoping + 15.0f);
        intensity *= 0.8f;  // Reduce intensity
    } else if (reappraisalSkill > 50.0f) {
        // Use cognitive reappraisal
        emotion.appraisalDesirability = emotion.appraisalDesirability * 0.7f + 50.0f * 0.3f;
        intensity *= 0.7f;
    } else if (suppressionTendency > 50.0f) {
        // Use suppression (less effective, has costs)
        intensity *= 0.6f;
        suppressionCost += 5.0f;  // Accumulate cost
    } else {
        // Default: emotion-focused coping
        emotion.appraisalCoping = std::min(100.0f, emotion.appraisalCoping + 10.0f);
        intensity *= 0.85f;
    }
    
    emotion.intensity = std::max(0.0f, intensity);
}

// EmotionalComplexitySystem implementation
EmotionalComplexitySystem::EmotionalComplexitySystem() {}

void EmotionalComplexitySystem::initializeEntity(Entity* entity) {
    if (!entity) return;
    
    int id = entity->getId();
    
    // Initialize with default values
    activeEmotions[id] = std::vector<EmotionalEpisode>();
    
    // Set initial mood based on personality
    MoodState mood;
    mood.overallValence = 40.0f + entity->personality.extraversion * 0.2f;
    mood.enthusiasm = 40.0f + entity->personality.extraversion * 0.3f;
    mood.anxiety = entity->personality.neuroticism * 0.6f;
    mood.irritability = entity->personality.neuroticism * 0.4f;
    mood.melancholy = entity->personality.neuroticism * 0.3f;
    moodStates[id] = mood;
    
    // Set regulation strategies based on personality
    EmotionalRegulation reg;
    reg.reappraisalSkill = 40.0f + entity->personality.openness * 0.4f;
    reg.suppressionTendency = (100.0f - entity->personality.extraversion) * 0.3f;
    reg.ruminationTendency = entity->personality.neuroticism * 0.5f;
    reg.expressionControl = entity->personality.conscientiousness * 0.5f;
    reg.problemFocusedCoping = 40.0f + entity->personality.conscientiousness * 0.4f;
    reg.emotionFocusedCoping = 40.0f + entity->personality.agreeableness * 0.3f;
    reg.supportSeeking = entity->personality.extraversion * 0.4f + entity->personality.agreeableness * 0.3f;
    regulationStrategies[id] = reg;
    
    // Set contagion susceptibility
    EmotionalContagionSusceptibility contagion;
    contagion.susceptibility = entity->personality.agreeableness * 0.5f;
    contagion.empathyLevel = entity->personality.agreeableness * 0.6f;
    contagion.emotionalResilience = 50.0f + (100.0f - entity->personality.neuroticism) * 0.3f;
    contagionProfiles[id] = contagion;
    
    // Set affective style
    AffectiveStyle style;
    style.resilienceSpeed = 40.0f + (100.0f - entity->personality.neuroticism) * 0.4f;
    style.positiveOutlook = 40.0f + entity->personality.extraversion * 0.3f;
    style.socialIntuition = entity->personality.agreeableness * 0.5f;
    style.selfAwareness = 40.0f + entity->personality.openness * 0.3f;
    style.contextSensitivity = entity->personality.agreeableness * 0.4f;
    style.attentionalControl = entity->personality.conscientiousness * 0.5f;
    affectiveStyles[id] = style;
}

EmotionalEpisode EmotionalComplexitySystem::generateEmotion(
    Entity* entity,
    const std::string& event,
    float relevance,
    float desirability,
    float coping,
    float control,
    float normCompatibility,
    int causingEntityId) {
    
    EmotionalEpisode episode;
    episode.cause = event;
    episode.causingEntityId = causingEntityId;
    episode.appraisalRelevance = relevance;
    episode.appraisalDesirability = desirability;
    episode.appraisalCoping = coping;
    episode.appraisalControl = control;
    episode.appraisalNormCompatibility = normCompatibility;
    
    // Determine primary emotion based on appraisal pattern
    if (desirability > 60.0f) {
        if (control > 50.0f) {
            episode.emotion = BasicEmotion::JOY;
        } else {
            episode.emotion = BasicEmotion::SURPRISE;
        }
    } else if (desirability < 40.0f) {
        if (control < 30.0f) {
            if (relevance > 70.0f) {
                episode.emotion = BasicEmotion::FEAR;
            } else {
                episode.emotion = BasicEmotion::SADNESS;
            }
        } else if (causingEntityId >= 0) {
            episode.emotion = BasicEmotion::ANGER;
        } else {
            episode.emotion = BasicEmotion::SADNESS;
        }
    } else {
        if (normCompatibility < 30.0f) {
            episode.emotion = BasicEmotion::DISGUST;
        } else {
            episode.emotion = BasicEmotion::SURPRISE;
        }
    }
    
    // Calculate initial intensity
    float intensity = relevance * (100.0f - std::abs(desirability - 50.0f)) * 0.02f;
    intensity = std::clamp(intensity, 10.0f, 100.0f);
    episode.intensity = intensity;
    
    // Set timing based on emotion type
    switch (episode.emotion) {
        case BasicEmotion::ANGER:
            episode.peakTime = 5;
            episode.offsetTime = 30;
            break;
        case BasicEmotion::FEAR:
            episode.peakTime = 3;
            episode.offsetTime = 20;
            break;
        case BasicEmotion::SADNESS:
            episode.peakTime = 10;
            episode.offsetTime = 60;
            break;
        case BasicEmotion::JOY:
            episode.peakTime = 5;
            episode.offsetTime = 25;
            break;
        default:
            episode.peakTime = 5;
            episode.offsetTime = 30;
            break;
    }
    
    episode.onsetTime = 0;  // Will be set when added
    
    return episode;
}

void EmotionalComplexitySystem::addEmotion(int entityId, const EmotionalEpisode& emotion) {
    EmotionalEpisode newEmotion = emotion;
    newEmotion.onsetTime = 0;  // Simplified: assume current time is 0
    activeEmotions[entityId].push_back(newEmotion);
    
    // Update mood from this emotion
    moodStates[entityId].modulateFromEmotion(newEmotion);
}

void EmotionalComplexitySystem::updateEmotions(int entityId, int currentTime) {
    auto& emotions = activeEmotions[entityId];
    
    // Update each emotion
    for (auto& emotion : emotions) {
        emotion.update(currentTime);
    }
    
    // Remove resolved emotions
    emotions.erase(
        std::remove_if(emotions.begin(), emotions.end(),
            [](const EmotionalEpisode& e) { return e.isResolved; }),
        emotions.end());
    
    // Generate meta-emotions (emotions about emotions)
    // This would be called periodically
}

float EmotionalComplexitySystem::getCurrentEmotionIntensity(int entityId, BasicEmotion emotion) {
    auto it = activeEmotions.find(entityId);
    if (it == activeEmotions.end()) return 0.0f;
    
    float totalIntensity = 0.0f;
    for (const auto& ep : it->second) {
        if (ep.emotion == emotion && !ep.isResolved) {
            totalIntensity += ep.getCurrentIntensity();
        }
    }
    
    return std::min(100.0f, totalIntensity);
}

EmotionDimension EmotionalComplexitySystem::getOverallEmotionalState(int entityId) {
    EmotionDimension dim;
    
    auto it = activeEmotions.find(entityId);
    if (it == activeEmotions.end()) {
        // Return mood-based dimensions if no active emotions
        dim.valence = moodStates[entityId].overallValence - 50.0f;
        dim.arousal = 50.0f;
        dim.dominance = 50.0f;
        return dim;
    }
    
    float totalIntensity = 0.0f;
    float weightedValence = 0.0f;
    float weightedArousal = 0.0f;
    float weightedDominance = 0.0f;
    
    for (const auto& ep : it->second) {
        if (ep.isResolved) continue;
        
        float intensity = ep.getCurrentIntensity() / 100.0f;
        totalIntensity += intensity;
        
        // Map emotions to dimensional space
        float emotionValence = 0.0f;
        float emotionArousal = 50.0f;
        float emotionDominance = 50.0f;
        
        switch (ep.emotion) {
            case BasicEmotion::JOY:
                emotionValence = 80.0f;
                emotionArousal = 70.0f;
                emotionDominance = 60.0f;
                break;
            case BasicEmotion::SADNESS:
                emotionValence = -70.0f;
                emotionArousal = 30.0f;
                emotionDominance = 30.0f;
                break;
            case BasicEmotion::ANGER:
                emotionValence = -60.0f;
                emotionArousal = 80.0f;
                emotionDominance = 70.0f;
                break;
            case BasicEmotion::FEAR:
                emotionValence = -70.0f;
                emotionArousal = 80.0f;
                emotionDominance = 20.0f;
                break;
            case BasicEmotion::DISGUST:
                emotionValence = -60.0f;
                emotionArousal = 50.0f;
                emotionDominance = 60.0f;
                break;
            case BasicEmotion::SURPRISE:
                emotionValence = 10.0f;
                emotionArousal = 90.0f;
                emotionDominance = 40.0f;
                break;
            default:
                emotionValence = 0.0f;
                emotionArousal = 50.0f;
                emotionDominance = 50.0f;
                break;
        }
        
        weightedValence += emotionValence * intensity;
        weightedArousal += emotionArousal * intensity;
        weightedDominance += emotionDominance * intensity;
    }
    
    if (totalIntensity > 0.0f) {
        dim.valence = weightedValence / totalIntensity;
        dim.arousal = weightedArousal / totalIntensity;
        dim.dominance = weightedDominance / totalIntensity;
    } else {
        dim.valence = moodStates[entityId].overallValence - 50.0f;
        dim.arousal = 50.0f;
        dim.dominance = 50.0f;
    }
    
    return dim;
}

void EmotionalComplexitySystem::updateMood(int entityId, float deltaTime) {
    auto it = moodStates.find(entityId);
    if (it != moodStates.end()) {
        it->second.update(deltaTime);
    }
}

MoodState& EmotionalComplexitySystem::getMood(int entityId) {
    return moodStates[entityId];
}

void EmotionalComplexitySystem::applyRegulation(int entityId, EmotionalEpisode& emotion) {
    auto it = regulationStrategies.find(entityId);
    if (it != regulationStrategies.end()) {
        it->second.applyRegulationStrategy(emotion, nullptr);
    }
}

void EmotionalComplexitySystem::processEmotionalContagion(
    Entity* target,
    const std::vector<Entity*>& nearby) {
    
    int targetId = target->getId();
    auto contagionIt = contagionProfiles.find(targetId);
    if (contagionIt == contagionProfiles.end()) return;
    
    const auto& contagion = contagionIt->second;
    
    // Aggregate emotions from nearby entities
    std::map<BasicEmotion, float> nearbyEmotions;
    
    for (Entity* source : nearby) {
        if (source == target) continue;
        
        int sourceId = source->getId();
        auto sourceEmotionsIt = activeEmotions.find(sourceId);
        if (sourceEmotionsIt == activeEmotions.end()) continue;
        
        for (const auto& ep : sourceEmotionsIt->second) {
            if (ep.isResolved) continue;
            
            float intensity = ep.getCurrentIntensity() / 100.0f;
            
            // Modulate by empathy and distance (simplified)
            float transmissionStrength = intensity * contagion.empathyLevel * 0.01f;
            
            nearbyEmotions[ep.emotion] += transmissionStrength;
        }
    }
    
    // Apply contagion to target based on susceptibility
    for (const auto& [emotion, strength] : nearbyEmotions) {
        float susceptibility = contagion.susceptibility * 0.01f;
        
        // Specific susceptibilities
        switch (emotion) {
            case BasicEmotion::ANGER:
                susceptibility *= contagion.angerContagion * 0.01f;
                break;
            case BasicEmotion::JOY:
                susceptibility *= contagion.joyContagion * 0.01f;
                break;
            case BasicEmotion::FEAR:
                susceptibility *= contagion.anxietyContagion * 0.01f;
                break;
            case BasicEmotion::SADNESS:
                susceptibility *= contagion.sadnessContagion * 0.01f;
                break;
            default:
                break;
        }
        
        // Resilience reduces contagion
        susceptibility *= (100.0f - contagion.emotionalResilience) * 0.01f;
        
        if (strength * susceptibility > 0.3f) {
            // Create a mild version of the emotion in target
            EmotionalEpisode contagiousEmotion;
            contagiousEmotion.emotion = emotion;
            contagiousEmotion.intensity = std::min(30.0f, strength * susceptibility * 100.0f);
            contagiousEmotion.peakTime = 3;
            contagiousEmotion.offsetTime = 15;
            contagiousEmotion.cause = "emotional_contagion";
            
            addEmotion(targetId, contagiousEmotion);
        }
    }
}

void EmotionalComplexitySystem::generateMetaEmotions(Entity* entity) {
    int entityId = entity->getId();
    auto emotions = getActiveEmotions(entityId);
    
    // Check for emotions that might trigger meta-emotions
    for (const auto& ep : emotions) {
        if (ep.isResolved) continue;
        
        // Guilt about anger
        if (ep.emotion == BasicEmotion::ANGER && ep.intensity > 60.0f) {
            if (ep.causingEntityId >= 0) {
                // Might feel guilty about being angry at someone
                float guiltIntensity = ep.intensity * 0.3f;
                if (guiltIntensity > 20.0f) {
                    EmotionalEpisode guilt;
                    guilt.emotion = BasicEmotion::GUILT;
                    guilt.intensity = guiltIntensity;
                    guilt.peakTime = 8;
                    guilt.offsetTime = 40;
                    guilt.cause = "meta_emotion_anger_guilt";
                    guilt.causingEntityId = ep.causingEntityId;
                    addEmotion(entityId, guilt);
                }
            }
        }
        
        // Pride about joy/achievement
        if (ep.emotion == BasicEmotion::JOY && ep.intensity > 70.0f) {
            float prideIntensity = ep.intensity * 0.4f;
            EmotionalEpisode pride;
            pride.emotion = BasicEmotion::PRIDE;
            pride.intensity = prideIntensity;
            pride.peakTime = 6;
            pride.offsetTime = 30;
            pride.cause = "meta_emotion_pride";
            addEmotion(entityId, pride);
        }
    }
}

const std::vector<EmotionalEpisode>& EmotionalComplexitySystem::getActiveEmotions(int entityId) const {
    static std::vector<EmotionalEpisode> empty;
    auto it = activeEmotions.find(entityId);
    if (it != activeEmotions.end()) {
        return it->second;
    }
    return empty;
}

const EmotionalRegulation& EmotionalComplexitySystem::getRegulation(int entityId) const {
    static EmotionalRegulation empty;
    auto it = regulationStrategies.find(entityId);
    if (it != regulationStrategies.end()) {
        return it->second;
    }
    return empty;
}

const AffectiveStyle& EmotionalComplexitySystem::getAffectiveStyle(int entityId) const {
    static AffectiveStyle empty;
    auto it = affectiveStyles.find(entityId);
    if (it != affectiveStyles.end()) {
        return it->second;
    }
    return empty;
}

void EmotionalComplexitySystem::saveTo(std::ofstream& file) const {
    // Save emotional states
    size_t numEntities = moodStates.size();
    file.write(reinterpret_cast<const char*>(&numEntities), sizeof(numEntities));
    
    for (const auto& [entityId, mood] : moodStates) {
        file.write(reinterpret_cast<const char*>(&entityId), sizeof(entityId));
        file.write(reinterpret_cast<const char*>(&mood), sizeof(mood));
    }
    
    // Save regulation strategies
    for (const auto& [entityId, reg] : regulationStrategies) {
        file.write(reinterpret_cast<const char*>(&entityId), sizeof(entityId));
        file.write(reinterpret_cast<const char*>(&reg), sizeof(reg));
    }
}

void EmotionalComplexitySystem::loadFrom(std::ifstream& file) {
    size_t numEntities;
    file.read(reinterpret_cast<char*>(&numEntities), sizeof(numEntities));
    
    for (size_t i = 0; i < numEntities; ++i) {
        int entityId;
        file.read(reinterpret_cast<char*>(&entityId), sizeof(entityId));
        
        MoodState mood;
        file.read(reinterpret_cast<char*>(&mood), sizeof(mood));
        moodStates[entityId] = mood;
        
        EmotionalRegulation reg;
        file.read(reinterpret_cast<char*>(&reg), sizeof(reg));
        regulationStrategies[entityId] = reg;
    }
}
