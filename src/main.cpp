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
#include <thread>
#include "./header/Disease.h"
#include "util/Debbug.h"
#include "./header/implot.h"
#include "./header/implot_internal.h"
#include "./header/Movement.h"
#include "./util/clear.h"

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
    if(neighSize >= 2){
        d.reduceAntiBody(ent);
        int disease = d.calculateDisease(neighSize, ent, sickClose);
        if(disease != -1){
            std::cout << "Entity contaminated: " + ent->getId() + ' ' + ent->getName() + " => " + d.getDiseaseName(disease) << std::endl;
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

void applyMovement(Entity* ent, std::vector<Entity*> grp){
    Movement m;
    m.applyMovement(ent, getNBSickClose(grp));
}

// Generate ActionContext based on simulation time
ActionContext createContextFromTime(int day, int numPeopleNearby) {
    int hour = (day % 60) * 24 / 60;  // Map frame to hour (0-23)
    int dayOfWeek = (day / 60) % 7;   // Day of week (0-6)

    bool isNightTime = (hour >= 22 || hour < 6);
    bool isWeekend = (dayOfWeek >= 5);  // Saturday=5, Sunday=6
    bool isAtWork = (!isWeekend && hour >= 9 && hour < 17);
    bool isInPublic = (numPeopleNearby > 2);

    return ActionContext(isNightTime, isWeekend, isAtWork, isInPublic, numPeopleNearby);
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

void applyFreeWill(std::vector<std::vector<Entity*>>& entityGroups, int currentDay){
    // Process each group of close entities
    for(auto& group : entityGroups){
        // For each entity in the group
        for(Entity* entity : group){
            //on applique aussi les paramètres de maladies
            applyDisease(entity, group.size(), getNBSickClose(group));

            //apply movement
            applyMovement(entity, group);

            if(entity->entityHealth <= 0.0f) continue; // Skip dead entities

            FreeWillSystem& sys = entity->getFreeWill();
            sys.updateNeeds(1.0f);

            std::vector<Entity*> neighbors;
            for(Entity* potential_neighbor : group){
                if(potential_neighbor != entity && potential_neighbor->entityHealth > 0.0f){
                    neighbors.push_back(potential_neighbor);
                }
            }

            // Create context based on current simulation time
            ActionContext context = createContextFromTime(currentDay, neighbors.size());

            // Choose action based on needs, social environment, context, and personality
            Action* chosen = sys.chooseAction(entity, neighbors, context);

            // we ondulate the loneliness wether it has neighboor or not
            entity->entityLoneliness += 4 * neighbors.size() + entityGroups.size();


            // Determine if this is a pointed action (requires a target)
            bool isPointedAction = (chosen->name == "Socialize" ||
                                   chosen->name == "Desire" ||
                                   chosen->name == "GoodConnection" ||
                                   chosen->name == "AngerConnection" ||
                                   chosen->name == "Murder" ||
                                   chosen->name == "Discrimination" ||
                                   chosen->name == "Breeding");

            Entity* target = nullptr;
            if(isPointedAction && !neighbors.empty()){
                // Select a random neighbor as target
                int targetIndex = BetterRand::genNrInInterval(0, (int) neighbors.size() - 1);
                target = neighbors[targetIndex];

                // Execute the action with the target
                sys.executeAction(entity, chosen, target);
                //saving data
                entity->saveEntityStats(chosen);

                // Update relationship based on action
                sys.pointedAssimilation(entity, target, chosen);
            } else {
                // Execute self-directed action
                sys.executeAction(entity, chosen);
                //saving data
                entity->saveEntityStats(chosen);
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
    std::cout << "clearing files... \n" ;
    rm_data_file();
    rm_data_act_file();
    std::cout << "done \n" ;
    srand(time(NULL));
    if (!glfwInit()) return -1;


    const float height = 768;
    const float width = 1024;

    GLFWwindow* window = glfwCreateWindow(width, height, "ASHB2 TEST", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");


    int nb_entity = 4;

    UI instanceUI;

    //vector of Entity with positions
    std::vector<Entity> entities;
    int count = 0;
    for (int y = 0; y < 1; ++y){
        for (int x = 0; x < nb_entity; ++x){
            Entity entity = Entity(
                count, 0.0f, 100.0f, 50.0f, 0.0f, 100.0f, "", 0.0f, 0.0f, 0.0f, 100.0f, 'A', 0, 0, -1, nullptr, nullptr, nullptr, nullptr, "happiness");
            entity.posX = 100 + x * 60;
            entity.posY = 100 + y * 60;
            entity.selected = false;
            // Assign random personality to each entity
            entity.personality = generateRandomPersonality();
            std::cout << "Entity " << count << " personality: E=" << entity.personality.extraversion
                      << " A=" << entity.personality.agreeableness
                      << " C=" << entity.personality.conscientiousness
                      << " N=" << entity.personality.neuroticism
                      << " O=" << entity.personality.openness << "\n";
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
    int day = 0;
    const int UPDATE_FREQUENCY = 60; // Update free will every 60 frames

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Check for dead entities and remove them
        for(int i = entities.size() - 1; i >= 0; i--){
            if(entities[i].entityHealth <= 0.0f){
                std::cout << "Entity " << entities[i].getId() << " has died and is being removed from the scene." << std::endl;

                //Birthday
                if((day / 60) % 365 ){
                    for(Entity& ent : entities){
                        ent.IncrementBDay();
                    }
                }

                // Remove from entities vector
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

        // Update free will system periodically
        frameCounter++;
        if(frameCounter >= UPDATE_FREQUENCY){
            frameCounter = 0;

            // Recalculate entity groups based on current positions
            close_entity_together = separationQuad(ent_quad, width, height);

            // Apply free will to all entity groups with current day for context
            std::cout << "CHECK SIZE GROUP: " << close_entity_together.size() << std::endl;
            applyFreeWill(close_entity_together, day);

        }


        instanceUI.showSimulationInformation(day / 60 , entities.size(), UPDATE_FREQUENCY, {});

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
        day ++;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
