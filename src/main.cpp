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
#include <thread>
#include "./header/FreeWillSystem.h"
#include "./header/SpatialMesh.h"
#include "./header/BetterRand.h"
#include <iostream>


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
        Entity entity = Entity(i, 0, 100, 50, 0, 100, "", 0, 0, 0, 100, 'A', 0, nullptr, nullptr, nullptr);
        entities.push_back(entity);
    }

    static bool showEntityWindow = true;
    std::vector<Entity*> ent_quad;
    for(int i=0; i<entities.size(); i++){
        ent_quad.push_back(&entities[i]);
    }

    std::vector<std::vector<Entity*>> close = separationQuad(ent_quad, points, width, height);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        instanceUI.showSimulationInformation(0, entities.size(), 1, {});

        int moved_entity = instanceUI.HandlePointMovement(points);
        if (moved_entity != -1) {
            if (showEntityWindow) {
                instanceUI.ShowEntityWindow(&entities.at(moved_entity), &showEntityWindow);
            }
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
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
