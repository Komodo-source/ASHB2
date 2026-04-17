
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
                   const EnvironmentalFactors& env = EnvironmentalFactors()){
    Movement m;
    m.applyMovement(ent, getNBSickClose(grp), (int)grp.size(), env);
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
            score += self->searchConnSocial(neighbor) * 2.0f;  // prefer friends
            score += (model ? model->trustLevel : 0) * 1.5f;
            score -= self->searchConnAng(neighbor) * 3.0f;     // avoid enemies
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
    std::cout << "2 /// Guangzhou, China | 23 07' 48'' north, 113 15' 36'' east /// monsoon\n";
    std::cout << "3 /// Addis-Abeba, Ethiopia | 9 1' 48'' north, 38 44' 24'' east /// arid\n";
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
    // Social bonds decay slowly without reinforcement
    for (auto& social : ent->list_entityPointedSocial) {
        social.social -= 0.02f * deltaTime;
        if (social.social < 0.5f) {
            social.social = 0.0f;
        }
    }

    for (auto& desire : ent->list_entityPointedDesire) {
        desire.desire -= 0.04f * deltaTime;
        desire.desire = std::max(0.0f, desire.desire);
    }

    float forgivenessRate = 0.015f * (ent->personality.agreeableness / 100.0f) * deltaTime;
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



            //apply movement (now env-aware)
            float oldX = entity->posX;
            float oldY = entity->posY;
            applyMovement(entity, group, env);
            if(oldX != entity->posX || oldY != entity->posY){
                globalLogger->logMovement(entity->entityId, entity->name, oldX, oldY, entity->posX, entity->posY, "environmental factors");
            }

            if(entity->entityHealth <= 0.0f){
                handleDeath(entity, group);
            }



            FreeWillSystem& sys = entity->getFreeWill();
            sys.updateNeeds(1.0f);

            // Apply direct environmental stat effects
            sys.applyEnvironmentalEffects(entity, env);

            //apply tick relationship
            tickRelationshipDecay(entity, currentDay);

            std::vector<Entity*> neighbors;
            for(Entity* potential_neighbor : group){
                if(potential_neighbor != entity && potential_neighbor->entityHealth > 0.0f){
                    neighbors.push_back(potential_neighbor);
                }
            }

            // Create context based on current simulation time (includes env and norms)
            ActionContext context = createContextFromTime(currentDay, neighbors.size());

            // Choose action based on needs, social environment, context, personality, grief, and env
            Action* chosen = sys.chooseAction(entity, neighbors, context);



            //Update Hierachical need
            sys.updateHieratchicalNeed(entity, *chosen);
            sys.updateNeeds(currentDay);

            // we ondulate the loneliness wether it has neighboor or not
            if (neighbors.size() > 0) {
                entity->entityLoneliness -= 2.0f * neighbors.size(); // decrease when with others
                entity->entityLoneliness = std::max(0.0f, entity->entityLoneliness);
            } else {
                entity->entityLoneliness += 1.5f;
            }

            //chaque 10 ticks on applique le développement
            // et on update les goals
            if(currentDay % 10 == 0){
                sys.tickChildDevelopment(entity, 1.0f);
                entity->recalculatePriority();
            }

            // Determine if this is a pointed action (requires a target)
            bool isPointedAction = (chosen->name == "Socialize" ||
                                   chosen->name == "Desire" ||
                                   chosen->name == "GoodConnection" ||
                                   chosen->name == "AngerConnection" ||
                                   chosen->name == "Murder" ||
                                   chosen->name == "Discrimination" ||
                                   chosen->name == "breeding" ||
                                   chosen->name == "couple" ||
                                   chosen->name == "Gossip" ||
                                   chosen->name == "Apologize" ||
                                   chosen->name == "HelpSupport" ||
                                   chosen->name == "IgnoreAvoid" ||
                                   chosen->name == "Insult" ||
                                   chosen->name == "Manipulate" ||
                                   chosen->name == "Jealousy" ||
                                   chosen->name == "Betray" ||
                                   chosen->name == "Flirt" ||
                                   chosen->name == "Date" ||
                                   chosen->name == "BreakUp" ||
                                   chosen->name == "Reconcile" ||
                                   chosen->name == "SetBoundaries");

            Entity* target = nullptr;
            if(isPointedAction && !neighbors.empty()){

                //better neightboor selection
                //int targetIndex = BetterRand::genNrInInterval(0, (int) neighbors.size() - 1);
                //target = neighbors[targetIndex];
                target = selectSocialTarget(entity, neighbors, chosen);

                bool targetWasAlive = (target->entityHealth > 0.0f);

                // Execute the action with the target
                sys.executeAction(entity, chosen, context, target);
                //saving data
                entity->saveEntityStats(chosen);
                globalLogger->logAction(entity->entityId, entity->name, chosen->name, target->name, "targeted action");

                // Update relationship based on action
                sys.pointedAssimilation(entity, target, chosen);

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
                sys.executeAction(entity, chosen, context);
                //saving data
                entity->saveEntityStats(chosen);
                globalLogger->logAction(entity->entityId, entity->name, chosen->name, "", "self-directed action");
            }

            sys.applyEmotionalContagion(entity, group);
        }
    }
}

std::vector<std::vector<Entity*>> separationQuad(std::vector<Entity*> entities, float width, float height, float radius=100){
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




int main() {

    std::cout << " I was meant to be perfect, ";
    std::cout << "I was meant to be beautiful  \n\n";
   //// Initialize logger (this redirects std::cout to cmd_log.txt)
   Logger logger;
   globalLogger = &logger;

    globalLogger->logCmd("clearing files...");
    try{

        rm_data_file();
        rm_data_act_file();
        rm_log_files();  // Clear all log files
        globalLogger->clearAllLogs();  // Ensure logs are cleared
    }catch(...){
        ;
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
    std::cout << "enter entity number (int): ";
    std::cin >> entity_num;


    srand(time(NULL));
    if (!glfwInit()) return -1;


    const int height = 1050;
    const int width = 1400;

    GLFWwindow* window = glfwCreateWindow(width, height, "ASHB2 TEST", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    UI instanceUI;

    //vector of Entity with positions
    std::vector<Entity> entities;
    int count = 0;
    for (int y = 0; y < 1; ++y){
        for (int x = 0; x < entity_num; ++x){
            Entity entity = Entity(
                count, 0.0f, 100.0f, 50.0f, 0.0f, 100.0f, "", 0.0f, 0.0f, 0.0f, 100.0f, 'A', 0, 0, -1, nullptr, nullptr, nullptr, nullptr, "happiness");


            entity.posX = BetterRand::genNrInInterval(10, width - 10);
            entity.posY = BetterRand::genNrInInterval(10, height - 10);;
            entity.selected = false;
            Heritage::UnlinkedNode(&entity);
            // Assign random personality to each entity
            entity.personality = generateRandomPersonality();
            std::stringstream ss;
            ss << "Entity " << count << " personality: E=" << entity.personality.extraversion
                      << " A=" << entity.personality.agreeableness
                      << " C=" << entity.personality.conscientiousness
                      << " N=" << entity.personality.neuroticism
                      << " O=" << entity.personality.openness;
            globalLogger->logCmd(ss.str());
            entities.push_back(entity);
            count++;
        }
    }

    static bool showEntityWindow = false;
    static int selectedEntityIndex = -1;
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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        //Birthday,
        // une année = 100 jours
        if(day  % 100 ){
            for(Entity& ent : entities){
                ent.IncrementBDay();
            }
        }

        // Check for dead entities and remove them
        for(int i = entities.size() - 1; i >= 0; i--){
            if(entities[i].entityHealth <= 0.0f){
                std::cout << "Entity " << entities[i].getId() << " has died and is being removed from the scene." << std::endl;
                globalLogger->logDeath(entities[i].getId(), entities[i].getName(), entities[i].entityAge, "health depletion");

                entities.erase(entities.begin() + i);

                // Rebuild ent_quad pointer vector
                ent_quad.clear();
                for(int j = 0; j < entities.size(); j++){
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
        if (!instanceUI.isSimulationPaused()) {
            frameCounter++;
            if(frameCounter >= UPDATE_FREQUENCY){
                frameCounter = 0;

                // Recalculate entity groups based on current positions
                close_entity_together = separationQuad(ent_quad, width, height);

                // Apply free will to all entity groups with current day for context
                std::cout << "CHECK SIZE GROUP: " << close_entity_together.size() << std::endl;
                applyFreeWill(close_entity_together, day);
                std::vector<Entity> new_borns = get_new_borns();
                for(Entity ent: new_borns){
                    entities.push_back(ent);
                }
                FreeWillSystem::clear_new_borns();

                // Export current state to JSON lines for HTML viewer
                exportTickHistory("./src/data/tick_history.jsonl", entities, day);

            }
        }


        instanceUI.showSimulationInformation(day / 60 , entities.size(), UPDATE_FREQUENCY, {});

        std::string saveFilename;
        int saveLoadAction = instanceUI.showSaveLoadButtons(saveFilename);
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
            instanceUI.ShowEntityWindow(&entities.at(selectedEntityIndex), &showEntityWindow);
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
        if (!instanceUI.isSimulationPaused()) {
            day++;
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
