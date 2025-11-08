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


int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(800, 600, "ASHB2 BUILD", nullptr, nullptr);
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
    for(int i=0;i<nb_entity;i++){
        Entity entity = Entity(i);
        entities.push_back(entity);
    }

    static bool showEntityWindow = true;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

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
