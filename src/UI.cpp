#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <string>
#include "./header/Entity.h"
#include "./header/UI.h"

#define BUFFER_SIZE

void ShowEntityWindow(Entity* entity, bool* p_open) {
    if (!ImGui::Begin("=== Entity Statistics ===", p_open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::Text("%d", entity->entityId);
    ImGui::Text("name: %s", entity->name);

    ImGui::Separator();
    ImGui::Text("Health: %d", entity->entityHealth);
    ImGui::Text("Age: %d", entity->entityAge);
    ImGui::Text("Sex: %d", entity->entitySex);
    ImGui::Text("Hapiness: %d", entity->entityHapiness);
    ImGui::Text("Stress: %d", entity->entityStress);
    ImGui::Text("Mental Health: %d", entity->entityMentalHealth);
    ImGui::Text("Loneliness: %d", entity->entityLoneliness);
    ImGui::Text("Anger: %d", entity->entityGeneralAnger);
    ImGui::Text("Birthday: %dth day", entity->entityBDay);
    ImGui::Text("Hygiene: %d", entity->entityHygiene);
    ImGui::Separator();

    ImGui::End();
}



void createPlayer(int& health, float& attackPower, char* playerName, char* message, std::string& displayText) {
    ImGui::Begin("Player Statistics");

    ImGui::Text("Health: %d", health);
    ImGui::Text("Attack Power: %.1f", attackPower);

    ImGui::Separator();

    ImGui::Text("Enter Player Name:");
    if (ImGui::InputText("##PlayerName", playerName, 128)) {
        displayText = std::string(playerName);
    }

    if (!displayText.empty()) {
        ImGui::Text("Welcome, %s!", displayText.c_str());
    }

    ImGui::Separator();

    ImGui::Text("Enter Message:");
    ImGui::InputTextMultiline("##Message", message, 256,
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4));

    ImGui::Separator();

    ImGui::SliderInt("Health Slider", &health, 0, 200);
    ImGui::SliderFloat("Attack Power", &attackPower, 0.0f, 100.0f);

    ImGui::Separator();

    if (ImGui::Button("Reset Values")) {
        health = 150;
        attackPower = 75.5f;
        playerName[0] = '\0';
        message[0] = '\0';
        displayText.clear();
    }

    ImGui::End();
}
