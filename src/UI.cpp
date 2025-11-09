#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <string>
#include <vector>
#include "./header/Entity.h"
#include "./header/UI.h"
#include <iostream>

// ShowEntityWindow implementation

UI::GridPoint UI::getGridPoint() {
    return gridPoint;
}

void UI::ShowEntityWindow(Entity* entity, bool* p_open) {
    if (!ImGui::Begin("=== Entity Statistics ===", p_open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::Text("ID: %d", entity->entityId);
    ImGui::Text("Name: %s", entity->name.c_str());

    ImGui::Separator();
    ImGui::Text("Health: %.2f", entity->entityHealth);
    ImGui::Text("Age: %.2f", entity->entityAge);
    ImGui::Text("Sex: %c", entity->entitySex);
    ImGui::Text("Happiness: %.2f", entity->entityHapiness);
    ImGui::Text("Stress: %.2f", entity->entityStress);
    ImGui::Text("Mental Health: %.2f", entity->entityMentalHealth);
    ImGui::Text("Loneliness: %.2f", entity->entityLoneliness);
    ImGui::Text("Anger: %.2f", entity->entityGeneralAnger);
    ImGui::Text("Birthday: %dth day", entity->entityBDay);
    ImGui::Text("Hygiene: %d", entity->entityHygiene);
    ImGui::Separator();

       if(entity->list_entityPointedDesire.size() > 0){
        ImGui::Text("Desire List");
        for(int i=0; i < entity->list_entityPointedDesire.size(); i++){
            Entity* pointed = entity->list_entityPointedDesire.at(i).pointedEntity;
            ImGui::Text("%d -> %d : %s", entity->list_entityPointedDesire.at(i).desire,
                       pointed->entityId, pointed->name.c_str());
        }
    }

    if(entity->list_entityPointedAnger.size() > 0){
        ImGui::Text("Anger List");
        for(int i=0; i < entity->list_entityPointedAnger.size(); i++){
            Entity* pointed = entity->list_entityPointedAnger.at(i).pointedEntity;
            ImGui::Text("%d -> %d : %s", entity->list_entityPointedAnger.at(i).anger,
                       pointed->entityId, pointed->name.c_str());
        }
    }

    if(entity->list_entityPointedCouple.size() > 0){
        ImGui::Text("Couple List");
        for(int i=0; i < entity->list_entityPointedCouple.size(); i++){
            Entity* pointed = entity->list_entityPointedCouple.at(i).pointedEntity;
            ImGui::Text("=> %d : %s", pointed->entityId, pointed->name.c_str());
        }
    }

    ImGui::End();
}

// DrawGrid implementation
void UI::DrawGrid(std::vector<GridPoint>& points, float pointSize) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    for (auto& p : points) {
        ImU32 color = p.selected ? IM_COL32(255, 100, 100, 255) : IM_COL32(200, 200, 200, 255);
        draw_list->AddCircleFilled(p.pos, pointSize, color);
    }
}

// HandlePointMovement implementation
// Returns the index of the selected/moved point, -1 if none
int UI::HandlePointMovement(std::vector<GridPoint>& points) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool mouseClicked = ImGui::IsMouseClicked(0);
    bool mouseHeld = ImGui::IsMouseDown(0);

    static int selectedIndex = -1;

    if (mouseClicked) {
        selectedIndex = -1;
        for (int i = 0; i < points.size(); i++) {
            float dx = mousePos.x - points[i].pos.x;
            float dy = mousePos.y - points[i].pos.y;
            if (dx * dx + dy * dy < 64.0f) { // radius²
                selectedIndex = i;
                points[i].selected = true;
                break;
            } else {
                points[i].selected = false;
            }
        }
    }

    if (mouseHeld && selectedIndex >= 0) {
        points[selectedIndex].pos = mousePos;
    }

    return selectedIndex; // returns -1 if no point is selected
}


// createPlayer implementation
void UI::createPlayer(int& health, float& attackPower, char* playerName, char* message, std::string& displayText) {
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
