#include <SDL.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "./header/UI.h"
#include "./header/Entity.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <time.h>
#include <random>
#include "./header/FreeWillSystem.h"
#include "./header/SpatialMesh.h"
#include "./header/BetterRand.h"
#include <iostream>
#include <sstream>
#include <thread>
#include "./header/Disease.h"
#include "util/Debbug.h"
#include "./header/implot.h"
#include "./header/implot_internal.h"
#include "./header/Movement.h"
#include "./util/clear.h"
#include "./header/SaveLoad.h"
#include "./header/heritage.h"
#include "./header/Logging.h"
#include "header/SDLEngine.h"
#include "header/Image.h"

using GroupEntity = std::vector<std::vector<Entity*>>;

/*
int main(){
    Entity entity = Entity(1, 0, 100, 50, 0, 100, "", 0, 0, 0, 100, 'A', 0, nullptr, nullptr, nullptr);
    FreeWillSystem sys;
    while(true){
        sys.updateNeeds(1.0f);
        Action* chosen =sys.chooseAction(&entity);
        sys.executeAction(&entity, chosen);
    }
    //sys.addAction()
}

*/

void applyDisease(Entity* ent, int neighSize, int sickClose){
    //si est seul la proba de tombé malade est null
    Disease d;
    if(ent->entityDiseaseType != -1){
        //already sick we manage it
        d.manageSickness(ent);
    }
    if(neighSize >= 3){
        d.reduceAntiBody(ent);
        int disease = d.calculateDisease(neighSize, ent, sickClose);
        if(disease != -1){
            std::stringstream ss;
            ss << "Entity contaminated: " << ent->getId() << " " << ent->getName()
                      << " => " << d.getDiseaseName(disease);
            globalLogger->logCmd(ss.str());
            globalLogger->logDisease(ent->getId(), ent->getName(), d.getDiseaseName(disease));
            ent->entityDiseaseType = disease;
        }
    }
}

int getNBSickClose(std::vector<Entity*> grp){
    int c =0;
    for(Entity* ent : grp){
        if(ent->entityDiseaseType != -1){
            c++;
        }
    }
    return c;
}

void applyMovement(Entity* ent, std::vector<Entity*> grp,
                   const EnvironmentalFactors& env = EnvironmentalFactors(),
                   float windowWidth = 1400.0f, float windowHeight = 1050.0f){
    Movement m;
    m.applyMovement(ent, getNBSickClose(grp), (int)grp.size(), env, windowWidth, windowHeight);
}

// Generate simple pseudo-random EnvironmentalFactors that shift slowly over time
EnvironmentalFactors generateEnvFactors(int day) {
    // Use a slow sine-based drift so conditions change gradually
    float weather  = 50.0f + 40.0f * std::sin(day * 0.003f);
    float noise    = 20.0f + 30.0f * std::abs(std::sin(day * 0.007f + 1.0f));
    float safety   = 60.0f + 30.0f * std::cos(day * 0.005f + 2.0f);
    float crowd    = 20.0f + 40.0f * std::abs(std::sin(day * 0.004f + 0.5f));

    // Clamp to [0, 100]
    auto clamp = [](float v){ return std::max(0.0f, std::min(100.0f, v)); };
    return EnvironmentalFactors(clamp(weather), clamp(crowd), clamp(noise), clamp(safety));
}

// Generate ActionContext based on simulation time
ActionContext createContextFromTime(int day, int numPeopleNearby) {
    int hour = (day % 60) * 24 / 60;
    int dayOfWeek = (day / 60) % 7;

    bool isNightTime = (hour >= 22 || hour < 6);
    bool isWeekend = (dayOfWeek >= 5);
    bool isAtWork = (!isWeekend && hour >= 9 && hour < 17);
    bool isInPublic = (numPeopleNearby > 2);

    EnvironmentalFactors env = generateEnvFactors(day);
    return ActionContext(isNightTime, isWeekend, isAtWork, isInPublic, numPeopleNearby, env);
}

// Generate random personality using Big Five distribution
Personality generateRandomPersonality() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(50.0f, 20.0f);

    auto clamp = [](float val) { return std::max(0.0f, std::min(100.0f, val)); };

    return Personality(
        clamp(dist(gen)),  // extraversion
        clamp(dist(gen)),  // agreeableness
        clamp(dist(gen)),  // conscientiousness
        clamp(dist(gen)),  // neuroticism
        clamp(dist(gen))   // openness
    );
}


    void handleDeath(Entity* dead, std::vector<Entity*>& allEntities) {
        for (Entity* ent : allEntities) {
            if (ent == dead || ent->entityHealth <= 0) continue;

            bool isPartner  = ent->checkCouple(dead);
            bool isParent   = (ent == dead->parent1 || ent == dead->parent2);
            bool isChild    = (dead == ent->parent1  || dead == ent->parent2);
            bool hasSocial  = ent->searchConnSocial(dead) > 10.0f;
            bool hasDesire  = ent->searchConnDesire(dead) > 15.0f;

            float griefIntensity = 0.0f;
            std::string narrative;

            if (isPartner) {
                griefIntensity = 0.85f + BetterRand::genNrInInterval(0,15)/100.0f;
                narrative = "lost life partner";
                ent->ValueSystem.familyOrientation += 10.0f; // réalisation tardive
                ent->onMajorEventAddOrBoostGoal("loss_death");
            } else if (isChild) {
                griefIntensity = 1.0f; // perte d'un enfant = grief maximal
                narrative = "lost a child";
                ent->personality.neuroticism += 8.0f;
                ent->onMajorEventAddOrBoostGoal("loss_death");
            } else if (isParent) {
                griefIntensity = 0.7f;
                narrative = "lost a parent";
                ent->ValueSystem.spiritualNeed += 5.0f;
                ent->onMajorEventAddOrBoostGoal("loss_death");
            } else if (hasDesire) {
                griefIntensity = 0.5f;
                narrative = "lost someone desired";
                ent->onMajorEventAddOrBoostGoal("loss_death");
            } else if (hasSocial) {
                griefIntensity = 0.3f;
                narrative = "lost a social connection";
                ent->onMajorEventAddOrBoostGoal("loss_death");
            }

            if (griefIntensity > 0.0f) {
                ent->addGrief(dead->entityId, griefIntensity, true);

                // Mémoire formative
                LifeMemory mem;
                mem.eventType = "loss_death";
                mem.entityInvolvedId = dead->entityId;
                mem.emotionalIntensity = griefIntensity;
                mem.isFormative = (griefIntensity > 0.6f);
                mem.internalNarrative = narrative;
                ent->lifeMemories.push_back(mem);
            }
        }
    }


    Entity* weightedRandomSelect(std::vector<std::pair<Entity*, float>> scores);

    Entity* selectSocialTarget(Entity* self, const std::vector<Entity*>& neighbors,
                           Action* action) {
    std::vector<std::pair<Entity*, float>> scores;

    for (Entity* neighbor : neighbors) {
        float score = 0.0f;
        MentalModelOfOther* model = self->getModelOf(neighbor);

        if (action->name == "Socialize" || action->name == "GoodConnection") {
            score += self->searchConnSocial(neighbor) * 2.0f;
            score += (model ? model->trustLevel : 0) * 1.5f;
            score -= self->searchConnAng(neighbor) * 3.0f;
        } else if (action->name == "Desire" || action->name == "Flirt") {
            score += self->searchConnDesire(neighbor) * 3.0f;
            score -= self->searchConnAng(neighbor) * 2.0f;
            // Attachment style affects target preference
            if (self->dv.attachmentStyle == ANXIOUS)
                score += (model ? (1.0f - model->predictability) : 0) * 2.0f; // anxious drawn to unpredictable
        } else if (action->name == "AngerConnection" || action->name == "Murder") {
            score += self->searchConnAng(neighbor) * 3.0f;     // target enemies
        } else if (action->name == "HelpSupport") {
            score += (model ? (100.0f - model->estimatedHappiness) : 50) * 0.02f; // help those suffering
            score += self->searchConnSocial(neighbor) * 1.0f;
        } else if (action->name == "Apologize") {
            score += self->searchConnAng(neighbor) * 2.0f;  // apologize to those angry at us
        }

        // Proximity bonus — closer entities more likely targets
        float dist = std::sqrt(std::pow(self->posX - neighbor->posX, 2) +
                              std::pow(self->posY - neighbor->posY, 2));
        score *= (1.0f / (1.0f + dist * 0.01f));

        scores.push_back({neighbor, std::max(0.01f, score)});
    }

    return weightedRandomSelect(scores);
}

void implementRegion(){
    std::cout << "\n";
    std::cout << "Choose a world region (h to help): \n";
    std::cout << "1 /// Paris, France | 48 51' 24'' north, 2 21' 07'' east /// Oceanic\n";
    std::cout << "2 /// Philadelphia, United States | 39 57' 10'' north, 75 09' 49'' west /// Continental\n";
    std::cout << "3 /// Guangzhou, China | 23 07' 48'' north, 113 15' 36'' east /// monsoon\n";
    std::cout << "4 /// Addis-Abeba, Ethiopia | 9 1' 48'' north, 38 44' 24'' east /// arid\n";
    std::cout << ">";

    char choice;
    std::cin >> choice;
    if(choice == 'h'){
        std::cout << "This feature is principally implemented for disease spreading, it barely affects the simulation links or its global functionning\n";
        implementRegion();
    }else{
        Disease::region = (int)choice;
    }
}

Entity* weightedRandomSelect(std::vector<std::pair<Entity*, float>> scores){
    float total = 0.0f;
    for (auto& s : scores) total += s.second;
    float roll = BetterRand::genNrInInterval(0.0f, total);
    float cum = 0.0f;
    for (auto& s : scores) {
        cum += s.second;
        if (roll <= cum) return s.first;
    }
    return scores.back().first;
}

std::vector<Entity> get_new_borns(){
    return FreeWillSystem::new_borns;
}


void tickRelationshipDecay(Entity* ent, float deltaTime) {
    // FIX: Relationship decay now depends on relationship strength
    // Strong relationships decay much slower than weak ones
    for (auto& social : ent->list_entityPointedSocial) {
        // Decay formula: base_decay / (1 + strength_factor)
        // Strong bonds (80+) decay at 20% of normal rate
        // Medium bonds (40-80) decay at 50% of normal rate
        // Weak bonds (<40) decay at full rate
        float decayRate = 0.01f;
        if (social.social > 80.0f) {
            decayRate = 0.002f; // Very slow decay for strong bonds
        } else if (social.social > 50.0f) {
            decayRate = 0.005f; // Moderate decay for medium bonds
        } else if (social.social > 20.0f) {
            decayRate = 0.008f; // Light decay for forming bonds
        }
        social.social -= decayRate * deltaTime;
        if (social.social < 0.5f) social.social = 0.0f;
    }

    // FIX: Desire decay also slowed for established attractions
    for (auto& desire : ent->list_entityPointedDesire) {
        float decayRate = 0.015f;
        if (desire.desire > 60.0f) {
            decayRate = 0.003f; // Strong attraction persists
        } else if (desire.desire > 30.0f) {
            decayRate = 0.008f;
        }
        desire.desire -= decayRate * deltaTime;
        desire.desire = std::max(0.0f, desire.desire);
    }

    // Anger fades based on agreeableness — already fine, keep small rate
    float forgivenessRate = 0.02f * (ent->personality.agreeableness / 100.0f) * deltaTime;
    for (auto& anger : ent->list_entityPointedAnger) {
        anger.anger -= forgivenessRate;
        anger.anger = std::max(0.0f, anger.anger);
    }

    // Clean up zeroed bonds
    ent->list_entityPointedSocial.erase(
        std::remove_if(ent->list_entityPointedSocial.begin(), ent->list_entityPointedSocial.end(),
            [](const entityPointedSocial& s){ return s.social <= 0.0f; }),
        ent->list_entityPointedSocial.end());
}


void applyFreeWill(std::vector<std::vector<Entity*>>& entityGroups, int currentDay){
    EnvironmentalFactors env = generateEnvFactors(currentDay);


    // Process each group of close entities
    for(auto& group : entityGroups){

        std::map<std::string, int> actionCounts;
        int totalRecentActions = 0;

        for(Entity* groupMember : group) {
            if(groupMember->entityHealth <= 0.0f) continue;

            const auto& history = groupMember->getFreeWill().getActionHistory();
            int actionsToCheck = std::min(10, (int)history.size());
            for(int i = 0; i < actionsToCheck; ++i) {
                actionCounts[history[i].actionName]++;
                totalRecentActions++;
            }
        }

        // Calculate prevalence and define norms
        if(totalRecentActions > 0) {
            for(const auto& pair : actionCounts) {
                float prevalence = (float)pair.second / totalRecentActions;
                // Only consider it a norm if it's somewhat common (> 10%)
                if(prevalence > 0.10f) {
                    // Norm pressure scales with prevalence.
                    //currentNorms[pair.first] = SocialNorm(pair.first, prevalence, prevalence);
                    ;
                }
            }
        }

        for(Entity* entity : group){
            //on applique aussi les paramètres de maladies
            applyDisease(entity, group.size(), getNBSickClose(group));

            // Tick grief recovery
            entity->tickGrief(1.0f);

            //lower pheromones
            if(!entity->pheromone.type.empty()){
                entity->pheromone.releasing_level -= BetterRand::genNrInInterval(3.0,6.0);
            }


            //apply movement (now env-aware)
            float oldX = entity->posX;
            float oldY = entity->posY;
            applyMovement(entity, group, env, 1400.0f, 1050.0f);
            if(oldX != entity->posX || oldY != entity->posY){
                globalLogger->logMovement(entity->entityId, entity->name, oldX, oldY, entity->posX, entity->posY, "environmental factors");
            }

            if(entity->entityHealth <= 0.0f){
                handleDeath(entity, group);
            }



            FreeWillSystem& sys = entity->getFreeWill();

            // FIX: Remove duplicate social decay - now handled properly in tickRelationshipDecay
            // This was causing double decay which prevented relationships from forming
            // float deltaTime = 1.0f;
            // for (auto &link : entity->list_entityPointedSocial) {
            //     link.social = std::max(0.0f, link.social - (0.005f * deltaTime));
            // }
            float deltaTime = 1.0f;

            //va avec l'update de needs
            entity->entityLoneliness += 0.05f * deltaTime;
            entity->entityBoredom   += 0.04f * deltaTime;
            entity->entityLoneliness = std::min(100.0f, entity->entityLoneliness);
            entity->entityBoredom   = std::min(100.0f, entity->entityBoredom);

            // Apply direct environmental stat effects
            sys.applyEnvironmentalEffects(entity, env);


            {
                const Personality& p = entity->personality;
                auto pdclamp = [](float v, float lo, float hi){ return std::max(lo, std::min(hi, v)); };

                // Hygiene degrades very slowly — noticeable over many ticks, not per tick
                float hygieneDecay = 0.08f + (1.0f - p.conscientiousness / 100.0f) * 0.10f;
                entity->entityHygiene = pdclamp(entity->entityHygiene - hygieneDecay, 0.0f, 100.0f);

                // Stress builds; neurotic entities accumulate it faster
                float stressGrowth = 0.15f + (p.neuroticism / 100.0f) * 0.20f;
                if (entity->entityLoneliness > 60.0f) stressGrowth += 0.08f;
                entity->entityStress = pdclamp(entity->entityStress + stressGrowth, 0.0f, 100.0f);

                // Boredom builds gently
                float boredomGrowth = 0.10f + (p.openness / 100.0f) * 0.10f;
                entity->entityBoredom = pdclamp(entity->entityBoredom + boredomGrowth, 0.0f, 100.0f);

                // Anger decays naturally; agreeable people let it go faster
                float angerDecay = 0.2f + (p.agreeableness / 100.0f) * 0.4f;
                entity->entityGeneralAnger = pdclamp(entity->entityGeneralAnger - angerDecay, 0.0f, 100.0f);

                // Loneliness builds slowly always; extraverts feel it faster
                float lonelinessGrowth = 0.25f + (p.extraversion / 100.0f) * 0.25f;
                entity->entityLoneliness = pdclamp(entity->entityLoneliness + lonelinessGrowth, 0.0f, 100.0f);

                // Happiness drifts toward a personality-based setpoint
                float happinessSetpoint = 40.0f + (p.agreeableness / 100.0f) * 15.0f
                                                 - (p.neuroticism   / 100.0f) * 20.0f
                                                 + (p.extraversion  / 100.0f) * 10.0f;
                float happinessDrift = (happinessSetpoint - entity->entityHapiness) * 0.02f;
                entity->entityHapiness = pdclamp(entity->entityHapiness + happinessDrift, 0.0f, 100.0f);

                // Mental health degrades under chronic stress, recovers in calm
                if (entity->entityStress > 70.0f)
                    entity->entityMentalHealth = pdclamp(entity->entityMentalHealth - 0.15f, 0.0f, 100.0f);
                else if (entity->entityStress < 30.0f)
                    entity->entityMentalHealth = pdclamp(entity->entityMentalHealth + 0.08f, 0.0f, 100.0f);

                // Health decays very slowly; high stress accelerates, conscientiousness slows it
                float healthDecay = 0.005f + (entity->entityStress / 100.0f) * 0.015f
                                          - (p.conscientiousness  / 100.0f) * 0.003f;
                entity->entityHealth = pdclamp(entity->entityHealth - healthDecay, 0.0f, 100.0f);

                // Low hygiene cascades into stress and happiness
                if (entity->entityHygiene < 25.0f) {
                    entity->entityStress   = pdclamp(entity->entityStress   + 0.4f, 0.0f, 100.0f);
                    entity->entityHapiness = pdclamp(entity->entityHapiness - 0.3f, 0.0f, 100.0f);
                }

                // Extreme loneliness erodes mental health
                if (entity->entityLoneliness > 80.0f)
                    entity->entityMentalHealth = pdclamp(entity->entityMentalHealth - 0.1f, 0.0f, 100.0f);
            }

            //apply tick relationship
            tickRelationshipDecay(entity, 1.0f);

            std::vector<Entity*> neighbors;
            for(Entity* potential_neighbor : group){
                if(potential_neighbor != entity && potential_neighbor->entityHealth > 0.0f){
                    neighbors.push_back(potential_neighbor);
                }
            }

            // Create context based on current simulation time (includes env and norms)
            ActionContext context = createContextFromTime(currentDay, neighbors.size());

            // Choose action based on needs, social environment, context, personality, grief, and env
            Action* chosenAction = sys.chooseAction(entity, neighbors, context);



            //social deficit
            if (chosenAction && chosenAction->needCategory == "social" &&
                chosenAction->name != "Murder" && chosenAction->name != "Betray" &&
                chosenAction->name != "Discrimination") {
                // Only real social fulfillment clears deficit
                entity->socialDeficit = std::max(0.0f, entity->socialDeficit - 8.0f);
                entity->dayWithoutSocialAction = 0;
            } else {
                if (entity->entityLoneliness > 10.0f) {
                    entity->socialDeficit += 2.0f;
                }
                entity->dayWithoutSocialAction++;
            }
            entity->socialDeficit = std::min(50.0f, entity->socialDeficit);


            //Update Hierachical need
            sys.updateHieratchicalNeed(entity, *chosenAction);
            sys.updateNeeds(currentDay, entity);

            if (neighbors.size() > 0) {
                float socialDrain = 0.0f;
                for (Entity* n : neighbors) {
                    float bond = entity->searchConnSocial(n);
                    if (bond > 10.0f) socialDrain += 0.15f;
                    else socialDrain += 0.05f;
                }
                entity->entityLoneliness = std::max(0.0f, entity->entityLoneliness - socialDrain);
            } else {
                entity->entityLoneliness += 2.5f;
            }
            entity->entityLoneliness = std::min(100.0f, entity->entityLoneliness);

            //chaque 10 ticks on applique le développement
            // et on update les goals
            if(currentDay % 10 == 0){
                sys.tickChildDevelopment(entity, 1.0f);
                entity->recalculatePriority();
            }

            // Determine if this is a pointed action (requires a target)
            bool isPointedAction = (chosenAction->name == "Socialize" ||
                                   chosenAction->name == "Desire" ||
                                   chosenAction->name == "GoodConnection" ||
                                   chosenAction->name == "AngerConnection" ||
                                   chosenAction->name == "Murder" ||
                                   chosenAction->name == "Discrimination" ||
                                   chosenAction->name == "breeding" ||
                                   chosenAction->name == "couple" ||
                                   chosenAction->name == "Gossip" ||
                                   chosenAction->name == "Apologize" ||
                                   chosenAction->name == "HelpSupport" ||
                                   chosenAction->name == "IgnoreAvoid" ||
                                   chosenAction->name == "Insult" ||
                                   chosenAction->name == "Manipulate" ||
                                   chosenAction->name == "Jealousy" ||
                                   chosenAction->name == "Betray" ||
                                   chosenAction->name == "Flirt" ||
                                   chosenAction->name == "Date" ||
                                   chosenAction->name == "BreakUp" ||
                                   chosenAction->name == "Reconcile" ||
                                   chosenAction->name == "SetBoundaries");

            Entity* target = nullptr;
            if(isPointedAction && !neighbors.empty()){

                //better neightboor selection
                //int targetIndex = BetterRand::genNrInInterval(0, (int) neighbors.size() - 1);
                //target = neighbors[targetIndex];
                target = selectSocialTarget(entity, neighbors, chosenAction);

                bool targetWasAlive = (target->entityHealth > 0.0f);

                // Execute the action with the target
                sys.executeAction(entity, chosenAction, context, target);

                //choosing side social action
                if(entity->dayWithoutSocialAction > 5){
                    Action* side_social_act = sys.ChooseSpecificSocialAction(entity);
                    sys.executeAction(entity, side_social_act, context, target);
                }
                //saving data
                entity->saveEntityStats(chosenAction);
                globalLogger->logAction(entity->entityId, entity->name, chosenAction->name, target->name, "targeted action");

                // Update relationship based on action
                sys.pointedAssimilation(entity, target, chosenAction);


                // Detect murder: if target just died, trigger grief in all connected entities
                if(targetWasAlive && target->entityHealth <= 0.0f){
                    std::stringstream ss;
                    ss << "DEATH EVENT: " << target->name << " was killed. Propagating grief.";
                    globalLogger->logCmd(ss.str());
                    globalLogger->logDeath(target->entityId, target->name, target->entityAge, "murder by " + entity->name);
                    for(Entity* other : group){
                        if(other == target || other->entityHealth <= 0.0f) continue;
                        bool hadBond = false;
                        for(const auto& s : other->list_entityPointedSocial)
                            if(s.pointedEntity == target && s.social > 8.0f){ hadBond = true; break; }
                        if(!hadBond) for(const auto& d : other->list_entityPointedDesire)
                            if(d.pointedEntity == target && d.desire > 8.0f){ hadBond = true; break; }
                        if(!hadBond) for(const auto& c : other->list_entityPointedCouple)
                            if(c.pointedEntity == target){ hadBond = true; break; }
                        if(hadBond){
                            float intensity = 0.65f + BetterRand::genNrInInterval(0, 25) / 100.0f;
                            other->addGrief(target->entityId, intensity, true);
                        }
                    }
                }
            } else {
                // Execute self-directed action
                sys.executeAction(entity, chosenAction, context);
                //saving data
                entity->saveEntityStats(chosenAction);
                globalLogger->logAction(entity->entityId, entity->name, chosenAction->name, "", "self-directed action");
            }

            sys.applyEmotionalContagion(entity, group);
        }
    }
}

std::vector<std::vector<Entity*>> separationQuad(std::vector<Entity*> entities, float width, float height, float radius=250){
    std::cout << "== Patial Mesh Separation ==\n";
    auto groups = getCloseEntityGroups(entities, width, height, radius);
    std::cout << "Found " << groups.size() << " groups of close entities:\n\n";

    for (size_t i = 0; i < groups.size(); ++i) {
        std::cout << "Group " << i + 1 << " (" << groups[i].size() << " entities):\n";
        for (Entity* entity : groups[i]) {
            std::cout << "  Entity " << entity->getId() << "\n";  // Assuming getId() method
        }
        std::cout << "\n";
    }
    std::cout << "== end of separation ==\n";
    return groups;
}


void sync_clock_stats(Entity* ent, int neighboors){
    FreeWillSystem fs;
    fs.chooseAction(ent);
}


void updateSimulationStep(std::vector<Entity>& entities, std::vector<Entity*>& ent_quad, std::vector<std::vector<Entity*>>& close_entity_together, int& day, int& frameCounter, const int UPDATE_FREQUENCY, bool isPaused, int width, int height, int& selectedEntityIndex, bool& showEntityWindow) {
    //Birthday,
    // une année = 100 jours
    if((day / 60)  % 100 == 1){
        for(Entity& ent : entities){
            ent.IncrementBDay();
        }
    }

    // Check for dead entities and remove them
    for(int i = (int)entities.size() - 1; i >= 0; i--){
        if(entities[i].entityHealth <= 0.0f){
            std::cout << "Entity " << entities[i].getId() << " has died and is being removed from the scene." << std::endl;
            entities.erase(entities.begin() + i);

            // Rebuild ent_quad pointer vector
            ent_quad.clear();
            for(int j = 0; j < (int)entities.size(); j++){
                ent_quad.push_back(&entities[j]);
            }

            // Reset selected entity if it was removed
            if(selectedEntityIndex == i){
                showEntityWindow = false;
                selectedEntityIndex = -1;
            } else if(selectedEntityIndex > i){
                selectedEntityIndex--;
            }
        }
    }

    // Update free will system periodically (only when not paused)
    if (!isPaused) {
        frameCounter++;
        if(frameCounter >= UPDATE_FREQUENCY){
            frameCounter = 0;

            // Recalculate entity groups based on current positions
            close_entity_together = separationQuad(ent_quad, width, height);

            // Apply free will to all entity groups with current day for context
            applyFreeWill(close_entity_together, day);
            std::vector<Entity> new_borns = get_new_borns();
            for(Entity ent: new_borns){
                entities.push_back(ent);
            }
            FreeWillSystem::clear_new_borns();

            // Rebuild ent_quad so new entities appear on map and pointers are fresh
            if (!new_borns.empty()) {
                ent_quad.clear();
                for(int j = 0; j < (int)entities.size(); j++){
                    ent_quad.push_back(&entities[j]);
                }
                // Repair all pointedEntity pointers
                for(Entity& e : entities){
                    for(auto& d : e.list_entityPointedDesire){
                        if(d.pointedEntity){
                            int id = d.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ d.pointedEntity = &other; break; }
                        }
                    }
                    for(auto& a : e.list_entityPointedAnger){
                        if(a.pointedEntity){
                            int id = a.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ a.pointedEntity = &other; break; }
                        }
                    }
                    for(auto& s : e.list_entityPointedSocial){
                        if(s.pointedEntity){
                            int id = s.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ s.pointedEntity = &other; break; }
                        }
                    }
                    for(auto& c : e.list_entityPointedCouple){
                        if(c.pointedEntity){
                            int id = c.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ c.pointedEntity = &other; break; }
                        }
                    }
                }
            }

            // Export current state to JSON lines for HTML viewer
            exportTickHistory("./src/data/tick_history.jsonl", entities, day);
        }
        day++;
    }
}

void initialiseSDL(std::vector<Entity>& entities, std::vector<Entity*>& ent_quad, std::vector<std::vector<Entity*>>& close_entity_together, int& day, int& frameCounter, const int UPDATE_FREQUENCY, int width, int height, int& selectedEntityIndex, bool& showEntityWindow){
    SDLEngine SDLEngine("ASHB2 DEBUG");
    Image obj(SDLEngine, "assets/background.jpg");

    bool running = true;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Apply free will and simulation updates
        updateSimulationStep(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, false, width, height, selectedEntityIndex, showEntityWindow);

        SDLEngine.initialiserRendu();
        obj.dessiner(0,0);

        // Draw all entities as dots in SDL mode
        SDL_SetRenderDrawColor(SDLEngine.getRenderer(), 255, 255, 255, 255);
        for(Entity* ent : ent_quad){
            SDL_Rect r;
            r.x = static_cast<int>(ent->posX);
            r.y = static_cast<int>(ent->posY);
            r.w = 5;
            r.h = 5;
            SDL_RenderFillRect(SDLEngine.getRenderer(), &r);
        }

        SDLEngine.finaliserRendu();
    }
}


int getRenderingChoice(){
    std::cout << "Choose your rendering method(1 or 2)\n  1-Simple Dots representing entities (=more statistics, less beautiful) \n  2-Graphic rendering with character moving(=less statistics, more beautiful)\n>";
    std::string input;
    std::cin >> input;
    if(input == "1"){
        return 1;
    }else{
        return 2;
    }
}




int main(int argc, char* argv[]) {
    std::cout << " \"I was meant to be perfect, ";
    std::cout << "I was meant to be beautiful\" \n\n";
   //// Initialize logger (this redirects std::cout to cmd_log.txt)
   Logger logger;
   globalLogger = &logger;

    globalLogger->logCmd("clearing files...");
    try{

        rm_data_file();
        rm_data_act_file();
        rm_log_files();
        globalLogger->clearAllLogs();
      std::cout << "file clearing done!\n";
    }catch(...){
        std::cout << "error with file clearing! if it persist just reclone the repo!\n";
    }

    // Clear tick history file
    std::ofstream tick_history("./src/data/tick_history.json", std::ios::trunc);
    tick_history.close();

    globalLogger->logCmd("done");
    int entity_num;
    std::cout << "Welcome to Artificial Simulation of Human Behavior (ASHB)\n";
    std::cout << "complete simulation can be found at /data/complete_logs.txt\n";
    std::cout << "you can save and load simulation at any moment\n";
    std::cout << "@author: Komodo \n";
    implementRegion();
    std::cout << "enter entity number (int : 40 is ok ): ";
    std::cin >> entity_num;
    int renderingType = getRenderingChoice();

    srand(time(NULL));
    const int height = 1050;
    const int width = 1400;

        std::vector<Entity> entities;
        entities.reserve(2048);
        int count = 0;
        for (int y = 0; y < 1; ++y){
            for (int x = 0; x < entity_num; ++x){
                Entity entity = Entity(
                    count, 15.0f, BetterRand::genNrInInterval(80.0f, 100.0f), BetterRand::genNrInInterval(30.0f, 70.0f), BetterRand::genNrInInterval(0.0f, 50.0f)
                    , BetterRand::genNrInInterval(80.0f, 100.0f), "", BetterRand::genNrInInterval(0.0f, 20.0f), BetterRand::genNrInInterval(0.0f, 20.0f),
                    BetterRand::genNrInInterval(0.0f, 40.0f), BetterRand::genNrInInterval(60.0f, 100.0f), 'A', 0, BetterRand::genNrInInterval(0.0f, 50.0f), -1, nullptr, nullptr, nullptr, nullptr, "happiness");

                entity.posX = BetterRand::genNrInInterval(10, width - 10);
                entity.posY = BetterRand::genNrInInterval(10, height - 10);;
                entity.selected = false;
                Heritage::UnlinkedNode(&entity);

                // --- Personality (Big Five, already randomized) ---
                entity.personality = generateRandomPersonality();

                // --- ValueSystem: the "soul" — what this person cares about ---
                // Each value is correlated with personality for realism
                std::mt19937 rng_spawn(std::random_device{}());
                std::normal_distribution<float> vd(50.0f, 18.0f);
                auto vc = [](float v){ return std::max(0.0f, std::min(100.0f, v)); };

                entity.ValueSystem.familyOrientation  = vc(vd(rng_spawn) + (entity.personality.agreeableness - 50.0f) * 0.3f);
                entity.ValueSystem.achievementDrive   = vc(vd(rng_spawn) + (entity.personality.conscientiousness - 50.0f) * 0.4f);
                entity.ValueSystem.spiritualNeed      = vc(vd(rng_spawn) - (entity.personality.openness - 50.0f) * 0.2f);
                entity.ValueSystem.hedonism           = vc(vd(rng_spawn) + (entity.personality.extraversion - 50.0f) * 0.3f - (entity.personality.conscientiousness - 50.0f) * 0.2f);
                entity.ValueSystem.collectivism       = vc(vd(rng_spawn) + (entity.personality.agreeableness - 50.0f) * 0.35f - (entity.personality.openness - 50.0f) * 0.1f);

                // --- Developmental History: childhood shapes the adult ---
                float traumaRoll   = vc(static_cast<float>(BetterRand::genNrInInterval(0, 50)));
                float nurtureRoll  = vc(100.0f - traumaRoll + static_cast<float>(BetterRand::genNrInInterval(-20, 20)));
                entity.dv.childhoodTraumaScore    = traumaRoll;
                entity.dv.childhoodNurturingScore = nurtureRoll;
                entity.dv.hadSecureAttachment     = (traumaRoll < 20.0f && nurtureRoll > 55.0f);

                // Attachment style derived from childhood
                if (traumaRoll < 20.0f && nurtureRoll > 60.0f)
                    entity.dv.attachmentStyle = SECURE;
                else if (traumaRoll > 55.0f)
                    entity.dv.attachmentStyle = (nurtureRoll < 30.0f) ? DISORGANIZED : ANXIOUS;
                else if (traumaRoll > 30.0f && nurtureRoll < 40.0f)
                    entity.dv.attachmentStyle = AVOIDANT;
                else
                    entity.dv.attachmentStyle = ANXIOUS;

                // Childhood permanently shifts personality at spawn (replicate finalizeChildhood effect)
                entity.personality.neuroticism    = vc(entity.personality.neuroticism    + traumaRoll  * 0.25f);
                entity.personality.agreeableness  = vc(entity.personality.agreeableness  - traumaRoll  * 0.15f + nurtureRoll * 0.12f);
                entity.personality.extraversion   = vc(entity.personality.extraversion   - traumaRoll  * 0.10f + nurtureRoll * 0.10f);
                entity.personality.openness       = vc(entity.personality.openness       + nurtureRoll * 0.15f);

                // --- Starting emotional stats: no two people start at zero ---
                // Neuroticism → more stress; conscientiousness → better hygiene; extraversion → less loneliness
                entity.entityStress       = vc(static_cast<float>(BetterRand::genNrInInterval(5, 30))
                                            + (entity.personality.neuroticism - 50.0f) * 0.25f
                                            + traumaRoll * 0.15f);
                entity.entityHygiene      = vc(static_cast<float>(BetterRand::genNrInInterval(55, 95))
                                            + (entity.personality.conscientiousness - 50.0f) * 0.20f);
                entity.entityLoneliness   = vc(static_cast<float>(BetterRand::genNrInInterval(5, 45))
                                            - (entity.personality.extraversion - 50.0f) * 0.25f
                                            + traumaRoll * 0.10f);
                entity.entityBoredom      = vc(static_cast<float>(BetterRand::genNrInInterval(10, 50))
                                            + (entity.personality.openness - 50.0f) * 0.15f);
                entity.entityGeneralAnger = vc(static_cast<float>(BetterRand::genNrInInterval(0, 25))
                                            + traumaRoll * 0.12f
                                            - (entity.personality.agreeableness - 50.0f) * 0.20f);
                entity.entityHapiness     = vc(static_cast<float>(BetterRand::genNrInInterval(30, 72))
                                            - traumaRoll * 0.15f
                                            + nurtureRoll * 0.10f
                                            + (entity.personality.extraversion - 50.0f) * 0.10f);
                entity.entityMentalHealth = vc(static_cast<float>(BetterRand::genNrInInterval(55, 95))
                                            - traumaRoll * 0.20f
                                            + nurtureRoll * 0.08f);
                entity.entityHealth       = vc(static_cast<float>(BetterRand::genNrInInterval(70, 100)));

                // --- Life goals: seeded by values and personality ---
                {
                    // Clear the default goal and assign based on dominant value
                    entity.m_goals.clear();
                    struct GoalSeed { std::string type; float weight; };
                    std::vector<GoalSeed> seeds = {
                        {"find_partner",  entity.ValueSystem.familyOrientation},
                        {"build_career",  entity.ValueSystem.achievementDrive},
                        {"make_friends",  entity.ValueSystem.collectivism},
                        {"happiness",     entity.ValueSystem.hedonism},
                        {"self",          entity.ValueSystem.spiritualNeed}
                    };
                    // Primary goal = highest value
                    auto best = std::max_element(seeds.begin(), seeds.end(),
                        [](const GoalSeed& a, const GoalSeed& b){ return a.weight < b.weight; });
                    LifeGoal primary;
                    primary.type = best->type;
                    primary.priority = 100.0f;
                    primary.progressToward = 0.0f;
                    primary.frustrationLevel = 0.0f;
                    primary.ticksSinceProgress = 0;
                    entity.m_goals.push_back(primary);

                    // Secondary goal: random from remaining if value > 35
                    for (auto& s : seeds) {
                        if (s.type != best->type && s.weight > 35.0f && entity.m_goals.size() < 3) {
                            LifeGoal sec;
                            sec.type = s.type;
                            sec.priority = s.weight * 0.6f;
                            sec.progressToward = 0.0f;
                            sec.frustrationLevel = 0.0f;
                            sec.ticksSinceProgress = 0;
                            entity.m_goals.push_back(sec);
                        }
                    }
                }

                std::stringstream ss;
                ss << "Entity " << count
                << " | Personality E=" << (int)entity.personality.extraversion
                << " A=" << (int)entity.personality.agreeableness
                << " C=" << (int)entity.personality.conscientiousness
                << " N=" << (int)entity.personality.neuroticism
                << " O=" << (int)entity.personality.openness
                << " | Values Fam=" << (int)entity.ValueSystem.familyOrientation
                << " Ach=" << (int)entity.ValueSystem.achievementDrive
                << " Hed=" << (int)entity.ValueSystem.hedonism
                << " Col=" << (int)entity.ValueSystem.collectivism
                << " Spi=" << (int)entity.ValueSystem.spiritualNeed
                << " | Attachment=" << entity.dv.attachmentStyle
                << " Trauma=" << (int)entity.dv.childhoodTraumaScore
                << " Nurture=" << (int)entity.dv.childhoodNurturingScore
                << " | Start: Happy=" << (int)entity.entityHapiness
                << " Stress=" << (int)entity.entityStress
                << " Lonely=" << (int)entity.entityLoneliness
                << " Goal=" << entity.m_goals[0].type;
                globalLogger->logCmd(ss.str());
                entities.push_back(entity);
                count++;
            }
        }

        bool showEntityWindow = false;
        int selectedEntityIndex = -1;
        std::vector<Entity*> ent_quad;
        for(int i=0; i<entities.size(); i++){
            ent_quad.push_back(&entities[i]);
        }

        std::vector<std::vector<Entity*>> close_entity_together = separationQuad(ent_quad, width, height);

        // ici on applique l'algorithme pour modification stats
        // Note: For now, we'll run this in the main loop instead of a separate thread
        // to avoid threading complexity with the UI
        // std::thread statistics(applyFreeWill, std::ref(close_entity_together));

        int frameCounter = 0;
        int day = FreeWillSystem::day;

        const int UPDATE_FREQUENCY = 60; // Update free will every 60 frames
    if(renderingType == 1){
        if (!glfwInit()) return -1;

        GLFWwindow* window = glfwCreateWindow(width, height, "ASHB", NULL, NULL);
        if (!window) {
            glfwTerminate();
            return -1;
        }
        glfwMakeContextCurrent(window);

        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        glfwSwapInterval(1);

        UI instanceUI;


        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            updateSimulationStep(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, instanceUI.isSimulationPaused(), width, height, selectedEntityIndex, showEntityWindow);

            std::string saveFilename;
            int saveLoadAction = instanceUI.showSaveLoadButtons(saveFilename, day / 60 , entities.size(), UPDATE_FREQUENCY, {});
            if (saveLoadAction == 1) {
                saveGame(saveFilename, entities, day, frameCounter);
            } else if (saveLoadAction == 2) {
                if (loadGame(saveFilename, entities, day, frameCounter)) {
                    // Rebuild pointer vectors after loading
                    ent_quad.clear();
                    for (int j = 0; j < (int)entities.size(); j++) {
                        ent_quad.push_back(&entities[j]);
                    }
                    close_entity_together = separationQuad(ent_quad, width, height);
                    showEntityWindow = false;
                    selectedEntityIndex = -1;
                }
            }

            int moved_entity = instanceUI.HandlePointMovement(ent_quad);
            if (moved_entity != -1) {
                selectedEntityIndex = moved_entity;
                showEntityWindow = true;
            }

            if (showEntityWindow && selectedEntityIndex >= 0 && selectedEntityIndex < entities.size()) {
                instanceUI.ShowEntityWindow(&entities.at(selectedEntityIndex), &showEntityWindow, ent_quad);
            }

            instanceUI.DrawGrid(ent_quad);

            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
    }else{// sdl rendering
        initialiseSDL(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, width, height, selectedEntityIndex, showEntityWindow);
    }
    return 0;
}
