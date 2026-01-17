#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <string>
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

void applyFreeWill(std::vector<std::vector<Entity*>>& entityGroups){
    // Process each group of close entities
    for(auto& group : entityGroups){
        // For each entity in the group
        for(Entity* entity : group){
            if(entity->entityHealth <= 0.0f) continue; // Skip dead entities

            FreeWillSystem sys;
            sys.updateNeeds(1.0f);

            // Get neighbors (all entities in the same group except self)
            std::vector<Entity*> neighbors;
            for(Entity* potential_neighbor : group){
                if(potential_neighbor != entity && potential_neighbor->entityHealth > 0.0f){
                    neighbors.push_back(potential_neighbor);
                }
            }

            // Choose action based on needs and social environment
            Action* chosen = sys.chooseAction(entity, neighbors);

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

                // Update relationship based on action
                sys.pointedAssimilation(entity, target, chosen);
            } else {
                // Execute self-directed action
                sys.executeAction(entity, chosen);
            }
        }
    }
}

std::vector<std::vector<Entity*>> separationQuad(std::vector<Entity*> entities, std::vector<UI::GridPoint> position, float width, float height, float radius=100){
    std::cout << "== Patial Mesh Separation ==\n";
    auto groups = getCloseEntityGroups(position, entities, width, height, radius);
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
    srand(time(NULL));
    if (!glfwInit()) return -1;


    const float height = 600;
    const float width = 800;

    GLFWwindow* window = glfwCreateWindow(width, height, "ASHB2 BUILD", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");


    int nb_entity = 2;

    // vector of GridPoints
    UI instanceUI;
    std::vector<UI::GridPoint> points;
    int count = 0;

    for (int y = 0; y < 1; ++y){
        for (int x = 0; x < nb_entity; ++x){
            UI::GridPoint gp;
            gp.pos = ImVec2(100 + x * 60, 100 + y * 60);
            gp.selected = false;
            gp.id = count;
            points.push_back(gp);
            count ++;
        }
    }

    //vector of Entity
    std::vector<Entity> entities;
    for(int i=0; i<nb_entity; i++){
        Entity entity = Entity(
            i, 0.0f, 100.0f, 50.0f, 0.0f, 100.0f, "", 0.0f, 0.0f, 0.0f, 100.0f, 'A', 0, nullptr, nullptr, nullptr, nullptr);
        entities.push_back(entity);
    }

    static bool showEntityWindow = false;
    static int selectedEntityIndex = -1;
    std::vector<Entity*> ent_quad;
    for(int i=0; i<entities.size(); i++){
        ent_quad.push_back(&entities[i]);
    }

    std::vector<std::vector<Entity*>> close_entity_together = separationQuad(ent_quad, points, width, height);

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

                // Remove from points vector
                if(i < points.size()){
                    points.erase(points.begin() + i);
                    // Update IDs for remaining points
                    for(int j = i; j < points.size(); j++){
                        points[j].id = j;
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
            close_entity_together = separationQuad(ent_quad, points, width, height);

            // Apply free will to all entity groups
            std::cout << "CHECK SIZE GROUP: " << close_entity_together.size() << std::endl;
            applyFreeWill(close_entity_together);
        }

        // 60 tick
        instanceUI.showSimulationInformation(day / 60 , entities.size(), UPDATE_FREQUENCY, {});

        int moved_entity = instanceUI.HandlePointMovement(points);
        if (moved_entity != -1) {
            selectedEntityIndex = moved_entity;
            showEntityWindow = true;
        }

        if (showEntityWindow && selectedEntityIndex >= 0 && selectedEntityIndex < entities.size()) {
            instanceUI.ShowEntityWindow(&entities.at(selectedEntityIndex), &showEntityWindow);
        }

        instanceUI.DrawGrid(points);

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
