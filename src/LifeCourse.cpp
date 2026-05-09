#include "./header/LifeCourse.h"
#include "./header/Entity.h"
#include <fstream>
#include <cmath>
#include <algorithm>

// CareerPath implementation
void CareerPath::changeOccupation(const std::string& newOccupation) {
    if (currentOccupation != newOccupation) {
        pastOccupations.push_back(currentOccupation);
        pastSatisfactions.push_back(jobSatisfaction);
        
        currentOccupation = newOccupation;
        occupationLevel = 0;
        yearsInCurrentRole = 0;
        jobSatisfaction = 50.0f;
        jobStress = 30.0f;
    }
}

void CareerPath::updateSatisfaction(float experience) {
    // Update satisfaction based on recent experience
    float targetSatisfaction = std::clamp(experience, 0.0f, 100.0f);
    jobSatisfaction = jobSatisfaction * 0.9f + targetSatisfaction * 0.1f;
    
    // Stress affects satisfaction
    jobSatisfaction -= jobStress * 0.2f;
    jobSatisfaction = std::clamp(jobSatisfaction, 0.0f, 100.0f);
}

// SkillPortfolio implementation
void SkillPortfolio::addSkill(const std::string& name, float initialProficiency) {
    if (skills.find(name) == skills.end()) {
        skills[name] = initialProficiency;
        skillAges[name] = 0;
    }
}

void SkillPortfolio::practiceSkill(const std::string& name, float quality) {
    auto it = skills.find(name);
    if (it != skills.end()) {
        // Practice improves skill
        float improvement = quality * 0.1f * (100.0f - it->second) / 100.0f;
        it->second += improvement;
        it->second = std::min(100.0f, it->second);
        skillAges[name] = 0;
    }
}

void SkillPortfolio::decayUnusedSkills(float deltaTime) {
    for (auto& [skillName, age] : skillAges) {
        age += static_cast<int>(deltaTime);
        
        // Skills decay after not being used
        if (age > 30) {
            auto skillIt = skills.find(skillName);
            if (skillIt != skills.end()) {
                float decay = 0.001f * (age - 30);
                skillIt->second = std::max(0.0f, skillIt->second - decay);
            }
        }
    }
}

// RelationshipLifecycle implementation
void RelationshipLifecycle::updateStage() {
    daysInStage++;
    totalDaysTogether++;
    
    // Check for stage transitions
    switch (stage) {
        case RelationshipStage::STRANGERS:
            if (currentSatisfaction > intimacyThreshold) {
                stage = RelationshipStage::ACQUAINTANCES;
                daysInStage = 0;
            }
            break;
            
        case RelationshipStage::ACQUAINTANCES:
            if (currentSatisfaction > intimacyThreshold * 1.5f) {
                stage = RelationshipStage::CASUAL_FRIENDS;
                daysInStage = 0;
            } else if (currentSatisfaction < 20.0f) {
                stage = RelationshipStage::STRANGERS;
                daysInStage = 0;
            }
            break;
            
        case RelationshipStage::CASUAL_FRIENDS:
            if (currentSatisfaction > commitmentThreshold) {
                stage = RelationshipStage::CLOSE_FRIENDS;
                daysInStage = 0;
            } else if (currentSatisfaction < 15.0f) {
                stage = RelationshipStage::ACQUAINTANCES;
                daysInStage = 0;
            }
            break;
            
        case RelationshipStage::CLOSE_FRIENDS:
            if (currentSatisfaction > interdependenceThreshold && 
                totalDaysTogether > 100) {
                stage = RelationshipStage::INTIMATE_PARTNERS;
                daysInStage = 0;
            }
            break;
            
        case RelationshipStage::INTIMATE_PARTNERS:
            if (currentSatisfaction > commitmentThreshold && 
                totalDaysTogether > 200) {
                stage = RelationshipStage::COMMITTED_PARTNERS;
                daysInStage = 0;
            } else if (currentSatisfaction < 30.0f) {
                stage = RelationshipStage::CASUAL_FRIENDS;
                daysInStage = 0;
            }
            break;
            
        case RelationshipStage::COMMITTED_PARTNERS:
            if (currentSatisfaction > 80.0f && totalDaysTogether > 365) {
                stage = RelationshipStage::MARRIED;
                daysInStage = 0;
            } else if (currentSatisfaction < 25.0f) {
                stage = RelationshipStage::SEPARATED;
                daysInStage = 0;
            }
            break;
            
        case RelationshipStage::MARRIED:
            if (unresolvedConflicts > 70.0f && repairAttempts < 30.0f) {
                stage = RelationshipStage::SEPARATED;
                daysInStage = 0;
            }
            break;
            
        default:
            break;
    }
}

bool RelationshipLifecycle::canAdvanceStage() const {
    return currentSatisfaction > intimacyThreshold && unresolvedConflicts < 50.0f;
}

bool RelationshipLifecycle::shouldDeteriorate() const {
    return currentSatisfaction < 30.0f || unresolvedConflicts > 60.0f;
}

// ParentingStyle implementation
ParentingStyle::StyleType ParentingStyle::getStyle() const {
    if (warmth > 50.0f && control > 50.0f) {
        return AUTHORITATIVE;
    } else if (warmth <= 50.0f && control > 50.0f) {
        return AUTHORITARIAN;
    } else if (warmth > 50.0f && control <= 50.0f) {
        return PERMISSIVE;
    } else {
        return NEGLECTFUL;
    }
}

// AgingEffects implementation
void AgingEffects::updateAging(float age, float deltaTime) {
    // Physical decline accelerates with age
    if (age > 30) {
        physicalDecline += 0.01f * deltaTime * ((age - 30) / 50.0f);
    }
    
    // Cognitive decline starts later
    if (age > 60) {
        cognitiveDecline += 0.005f * deltaTime * ((age - 60) / 30.0f);
    }
    
    // Wisdom accumulates with age and experience
    wisdomAccumulation += 0.02f * deltaTime;
    wisdomAccumulation = std::min(100.0f, wisdomAccumulation);
    
    // Emotional complexity increases with age
    if (age < 50) {
        emotionalComplexity += 0.01f * deltaTime;
    }
    emotionalComplexity = std::min(100.0f, emotionalComplexity);
    
    // Time horizon shrinks with age
    timeHorizon = std::max(0.0f, 100.0f - age);
    
    // Present focus increases as time horizon shrinks
    presentFocus = 30.0f + (100.0f - timeHorizon) * 0.5f;
    
    // Generativity peaks in middle age
    if (age >= 40 && age <= 65) {
        generativity = 60.0f + (100.0f - std::abs(age - 52.5f)) * 0.4f;
    } else if (age < 40) {
        generativity = 40.0f + age * 0.5f;
    } else {
        generativity = 70.0f - (age - 65) * 0.5f;
    }
    generativity = std::clamp(generativity, 0.0f, 100.0f);
}

// LifeCourseSystem implementation
LifeCourseSystem::LifeCourseSystem() {}

void LifeCourseSystem::initializeEntity(Entity* entity) {
    if (!entity) return;
    
    int id = entity->getId();
    
    // Initialize career
    CareerPath career;
    career.ambition = 40.0f + entity->personality.conscientiousness * 0.4f;
    career.workLifeBalance = entity->personality.extraversion * 0.3f + 
                             (100.0f - entity->personality.conscientiousness) * 0.3f;
    careers[id] = career;
    
    // Initialize empty relationships
    relationships[id] = std::vector<RelationshipLifecycle>();
    
    // Initialize parenting style
    ParentingStyle parenting;
    parenting.warmth = 40.0f + entity->personality.agreeableness * 0.4f;
    parenting.control = 40.0f + entity->personality.conscientiousness * 0.4f;
    parenting.autonomy_granting = entity->personality.openness * 0.5f;
    parentingStyles[id] = parenting;
    
    // Initialize empty life events
    lifeEvents[id] = std::vector<LifeEvent>();
    
    // Initialize milestones
    milestones[id] = DevelopmentalMilestones();
    
    // Initialize aging effects
    AgingEffects aging;
    agingProfiles[id] = aging;
}

CareerPath& LifeCourseSystem::getCareer(int entityId) {
    return careers[entityId];
}

void LifeCourseSystem::updateCareer(int entityId, float workExperience) {
    auto& career = careers[entityId];
    
    // Update satisfaction from experience
    career.updateSatisfaction(workExperience);
    
    // Increase seniority over time
    if (career.occupationLevel < 10) {
        career.yearsInField++;
        career.yearsInCurrentRole++;
        
        // Promotion chance based on satisfaction and ambition
        float promotionChance = career.ambition * 0.01f * 
                               (career.jobSatisfaction / 100.0f);
        if (promotionChance > 0.5f && career.yearsInCurrentRole > 3) {
            career.occupationLevel++;
            career.yearsInCurrentRole = 0;
        }
    }
}

void LifeCourseSystem::evaluateCareerChange(int entityId) {
    auto& career = careers[entityId];
    
    // Consider career change if satisfaction is low
    if (career.jobSatisfaction < 30.0f) {
        // Ambitious entities tolerate dissatisfaction longer
        float changeThreshold = 30.0f - career.ambition * 0.2f;
        
        if (career.jobSatisfaction < changeThreshold) {
            // Would trigger career search behavior
            // For now, just mark as considering change
        }
    }
}

RelationshipLifecycle* LifeCourseSystem::getRelationship(int entityId, int partnerId) {
    auto& rels = relationships[entityId];
    
    for (auto& rel : rels) {
        if (rel.partnerId == partnerId) {
            return &rel;
        }
    }
    return nullptr;
}

void LifeCourseSystem::createRelationship(int entityId, int partnerId) {
    // Check if relationship already exists
    if (getRelationship(entityId, partnerId) != nullptr) {
        return;
    }
    
    RelationshipLifecycle newRel;
    newRel.partnerId = partnerId;
    newRel.stage = RelationshipStage::ACQUAINTANCES;
    newRel.currentSatisfaction = 50.0f;
    
    relationships[entityId].push_back(newRel);
}

void LifeCourseSystem::updateRelationship(int entityId, int partnerId,
                                           float interactionQuality) {
    auto* rel = getRelationship(entityId, partnerId);
    if (!rel) return;
    
    // Update satisfaction based on interaction
    float satisfactionChange = (interactionQuality - 50.0f) * 0.1f;
    rel->currentSatisfaction += satisfactionChange;
    rel->currentSatisfaction = std::clamp(rel->currentSatisfaction, 0.0f, 100.0f);
    
    // Update average satisfaction
    rel->averageSatisfaction = rel->averageSatisfaction * 0.95f + 
                               rel->currentSatisfaction * 0.05f;
    
    // Track conflicts
    if (interactionQuality < 30.0f) {
        rel->unresolvedConflicts += (50.0f - interactionQuality) * 0.1f;
        rel->unresolvedConflicts = std::min(100.0f, rel->unresolvedConflicts);
    } else if (interactionQuality > 70.0f) {
        // Repair attempts reduce conflicts
        rel->repairAttempts += (interactionQuality - 50.0f) * 0.1f;
        if (rel->unresolvedConflicts > 0) {
            rel->unresolvedConflicts -= 2.0f;
            rel->unresolvedConflicts = std::max(0.0f, rel->unresolvedConflicts);
        }
    }
    
    // Update stage
    rel->updateStage();
}

void LifeCourseSystem::processRelationshipTransitions(int entityId) {
    auto& rels = relationships[entityId];
    
    for (auto& rel : rels) {
        rel.updateStage();
    }
}

ParentingStyle& LifeCourseSystem::getParentingStyle(int entityId) {
    return parentingStyles[entityId];
}

void LifeCourseSystem::updateParenting(Entity* parent, Entity* child,
                                        const std::string& interaction) {
    if (!parent || !child) return;
    
    int parentId = parent->getId();
    int childId = child->getId();
    
    auto& style = parentingStyles[parentId];
    
    // Analyze interaction type
    if (interaction.find("warm") != std::string::npos ||
        interaction.find("support") != std::string::npos) {
        style.warmth = std::min(100.0f, style.warmth + 2.0f);
        style.involvement = std::min(100.0f, style.involvement + 1.0f);
    } else if (interaction.find("strict") != std::string::npos ||
               interaction.find("discipline") != std::string::npos) {
        style.control = std::min(100.0f, style.control + 2.0f);
        style.consistency = std::min(100.0f, style.consistency + 1.0f);
    } else if (interaction.find("neglect") != std::string::npos) {
        style.warmth = std::max(0.0f, style.warmth - 3.0f);
        style.involvement = std::max(0.0f, style.involvement - 2.0f);
    }
    
    // Track child outcomes (simplified)
    float outcomeQuality = (style.warmth + style.consistency) * 0.5f;
    style.childOutcomes[childId] = outcomeQuality;
}

void LifeCourseSystem::recordLifeEvent(int entityId, const LifeEvent& event) {
    lifeEvents[entityId].push_back(event);
    
    // Process immediate effects
    processLifeEventEffects(entityId);
}

void LifeCourseSystem::processLifeEventEffects(int entityId) {
    auto& events = lifeEvents[entityId];
    
    // Find unprocessed events
    for (auto& event : events) {
        if (event.isProcessed) continue;
        
        // Apply personality impacts
        // These would be applied to the actual entity in a full implementation
        
        event.isProcessed = true;
    }
}

std::vector<LifeEvent> LifeCourseSystem::getRecentEvents(int entityId, int window) {
    std::vector<LifeEvent> recent;
    const auto& events = lifeEvents[entityId];
    
    // Get events within the time window
    for (const auto& event : events) {
        if (event.simulationDay >= window) {
            recent.push_back(event);
        }
    }
    
    return recent;
}

void LifeCourseSystem::checkMilestones(int entityId, float age, LifeStage stage) {
    auto& milestoneData = milestones[entityId];
    
    // Define age-appropriate milestones
    std::vector<std::pair<std::string, float>> ageMilestones;
    
    if (age < 18) {
        ageMilestones = {
            {"first_school", 6.0f},
            {"adolescence", 13.0f},
            {"graduation", 18.0f}
        };
    } else if (age < 30) {
        ageMilestones = {
            {"career_start", 22.0f},
            {"independence", 25.0f},
            {"partnership", 28.0f}
        };
    } else if (age < 50) {
        ageMilestones = {
            {"career_established", 35.0f},
            {"parenthood", 40.0f},
            {"midlife", 45.0f}
        };
    } else {
        ageMilestones = {
            {"empty_nest", 55.0f},
            {"retirement", 65.0f},
            {"elder", 75.0f}
        };
    }
    
    // Check each milestone
    for (const auto& [milestone, targetAge] : ageMilestones) {
        if (milestoneData.achievedMilestones.find(milestone) == 
            milestoneData.achievedMilestones.end()) {
            
            if (age >= targetAge) {
                // Milestone achieved
                milestoneData.achievedMilestones[milestone] = true;
                
                // Check timing
                float timing = age / targetAge;
                if (timing < 0.9f) {
                    milestoneData.developmentalSynchrony += 5.0f;  // Early
                } else if (timing > 1.1f) {
                    milestoneData.developmentalSynchrony -= 5.0f;  // Late
                } else {
                    milestoneData.developmentalSynchrony += 2.0f;  // On-time
                }
            }
        }
    }
    
    milestoneData.developmentalSynchrony = 
        std::clamp(milestoneData.developmentalSynchrony, 0.0f, 100.0f);
}

void LifeCourseSystem::updateAging(int entityId, float age, float deltaTime) {
    auto& aging = agingProfiles[entityId];
    aging.updateAging(age, deltaTime);
}

const AgingEffects& LifeCourseSystem::getAgingProfile(int entityId) const {
    return agingProfiles.at(entityId);
}

float LifeCourseSystem::calculateLifeSatisfaction(int entityId) const {
    const auto& career = careers.at(entityId);
    const auto& rels = relationships.at(entityId);
    const auto& events = lifeEvents.at(entityId);
    
    // Career satisfaction component
    float careerSat = career.jobSatisfaction * 0.3f;
    
    // Relationship satisfaction component
    float relationshipSat = 0.0f;
    if (!rels.empty()) {
        float totalSat = 0.0f;
        for (const auto& rel : rels) {
            totalSat += rel.averageSatisfaction;
        }
        relationshipSat = (totalSat / rels.size()) * 0.4f;
    }
    
    // Recent events component
    float eventSat = 50.0f * 0.3f;
    if (!events.empty()) {
        float recentImpact = 0.0f;
        int count = 0;
        for (const auto& event : events) {
            if (!event.isProcessed) {
                recentImpact += event.impactMagnitude;
                count++;
            }
        }
        if (count > 0) {
            eventSat = (50.0f + recentImpact / count) * 0.3f;
        }
    }
    
    return std::clamp(careerSat + relationshipSat + eventSat, 0.0f, 100.0f);
}

const std::vector<LifeEvent>& LifeCourseSystem::getLifeEvents(int entityId) const {
    return lifeEvents.at(entityId);
}

void LifeCourseSystem::saveTo(std::ofstream& file) const {
    // Save careers
    size_t numCareers = careers.size();
    file.write(reinterpret_cast<const char*>(&numCareers), sizeof(numCareers));
    
    for (const auto& [entityId, career] : careers) {
        file.write(reinterpret_cast<const char*>(&entityId), sizeof(entityId));
        // Save key career fields
        file.write(reinterpret_cast<const char*>(&career.occupationLevel), 
                   sizeof(career.occupationLevel));
        file.write(reinterpret_cast<const char*>(&career.jobSatisfaction), 
                   sizeof(career.jobSatisfaction));
        file.write(reinterpret_cast<const char*>(&career.yearsInField), 
                   sizeof(career.yearsInField));
    }
    
    // Save aging profiles
    size_t numAging = agingProfiles.size();
    file.write(reinterpret_cast<const char*>(&numAging), sizeof(numAging));
    
    for (const auto& [entityId, aging] : agingProfiles) {
        file.write(reinterpret_cast<const char*>(&entityId), sizeof(entityId));
        file.write(reinterpret_cast<const char*>(&aging.physicalDecline), 
                   sizeof(aging.physicalDecline));
        file.write(reinterpret_cast<const char*>(&aging.wisdomAccumulation), 
                   sizeof(aging.wisdomAccumulation));
    }
}

void LifeCourseSystem::loadFrom(std::ifstream& file) {
    size_t numCareers;
    file.read(reinterpret_cast<char*>(&numCareers), sizeof(numCareers));
    
    for (size_t i = 0; i < numCareers; ++i) {
        int entityId;
        file.read(reinterpret_cast<char*>(&entityId), sizeof(entityId));
        
        CareerPath career;
        file.read(reinterpret_cast<char*>(&career.occupationLevel), 
                  sizeof(career.occupationLevel));
        file.read(reinterpret_cast<char*>(&career.jobSatisfaction), 
                  sizeof(career.jobSatisfaction));
        file.read(reinterpret_cast<char*>(&career.yearsInField), 
                  sizeof(career.yearsInField));
        
        careers[entityId] = career;
        relationships[entityId] = std::vector<RelationshipLifecycle>();
        parentingStyles[entityId] = ParentingStyle();
        lifeEvents[entityId] = std::vector<LifeEvent>();
        milestones[entityId] = DevelopmentalMilestones();
    }
    
    size_t numAging;
    file.read(reinterpret_cast<char*>(&numAging), sizeof(numAging));
    
    for (size_t i = 0; i < numAging; ++i) {
        int entityId;
        file.read(reinterpret_cast<char*>(&entityId), sizeof(entityId));
        
        AgingEffects aging;
        file.read(reinterpret_cast<char*>(&aging.physicalDecline), 
                  sizeof(aging.physicalDecline));
        file.read(reinterpret_cast<char*>(&aging.wisdomAccumulation), 
                  sizeof(aging.wisdomAccumulation));
        
        agingProfiles[entityId] = aging;
    }
}
