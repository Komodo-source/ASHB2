#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <string>
#include <vector>
#include "./header/Entity.h"
#include "./header/UI.h"
#include <iostream>
#include "./header/Disease.h"
#include <map>
#include "./header/implot.h"
#include "./header/implot_internal.h"
#include <sstream>

// ShowEntityWindow implementation

UI::GridPoint UI::getGridPoint() {
    return gridPoint;
}

//complementary_info = nombre de couple, nombre de mort etc etc
void UI::showSimulationInformation(int day, int num_entity, int tick, std::map<std::string, int> complementary_information){
    ImGui::Begin("=== Simulation Statistics ===");
    ImGui::Text("Number Entities: %d", num_entity);
    ImGui::Text("day: %d", day);
    ImGui::Text("actual tick: %d", tick);
    ImGui::Separator();
    if (ImGui::Button("Create Entity")) {
        //createPlayer()
        ;
    }
    ImGui::Separator();
    for(auto& p : complementary_information){
        ImGui::Text("%s: %d", p.first,p.second);
    }
    ImGui::End();
}

/*
void UI::showSystemInformation(){
    ImGui::Begin("=== System Statistics ===");
    ImGui::Text("Number Entities: %d", num_entity);
    ImGui::Text("day: %d", day);
    ImGui::Text("actual tick: %d", tick);
    ImGui::Separator();
    for(auto& p : complementary_information){
        ImGui::Text("%s: %d", p.first,p.second);
    }
    ImGui::End();
}*/

    void UI::ShowEntityWindow(Entity* entity, bool* p_open) {
        if (!ImGui::Begin("=== Entity Statistics ===", p_open, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            return;
        }


        ImGui::Text("ID: %d", entity->entityId);
        ImGui::Text("Name: %s", entity->name.c_str());

        ImGui::Spacing();
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
        ImGui::Spacing();
        ImGui::Text("AI Entity Information");

        if(ImGui::Button("Show statistics")){
            showDetailedInfo = !showDetailedInfo;
        }
        if(ImGui::Button("Show detailed Action Informatino")){
            showActionInfo = !showActionInfo;
        }



        if(showActionInfo){
            ImGui::Begin("Action statistics", p_open, ImGuiWindowFlags_NoCollapse);
            std::ifstream statsFile("./src/data/act_" + std::to_string(entity->entityId) + ".csv");
            std::string line;
            int c = 1;
            while (std::getline(statsFile, line)) {

                std::stringstream ss(line);
                std::string cell;

                while (std::getline(ss, cell, ',')) {
                    ImGui::Text(cell.c_str());
                    c++;
                    if( c == 5){
                        c = 0;
                        ImGui::Separator();
                    }
                }

            }
            ImGui::End();
        }

        if (showDetailedInfo) {
            std::vector<std::string> labels = {
                "Anti Body", "Boredom", "Anger", "Happiness",
                "Health", "Hygiene", "Loneliness", "Mental Health", "Stress"
            };

            std::vector<std::vector<float>> plot_data(labels.size());
            std::vector<float> x_axis;

            std::ifstream statsFile("./src/data/" + std::to_string(entity->entityId) + ".csv");
            std::string line;
            int rowCount = 0;

            while (std::getline(statsFile, line)) {
                std::stringstream ss(line);
                std::string cell;
                int colIndex = 0;

                while (std::getline(ss, cell, ',')) {
                    if (colIndex < labels.size()) {
                        plot_data[colIndex].push_back(std::stof(cell));
                    }
                    colIndex++;
                }
                x_axis.push_back((float)rowCount);
                rowCount++;
            }

            if (ImPlot::BeginPlot("Entity Statistics Over Time", ImVec2(-1, 300))) {
                ImPlot::SetupAxes("Time (Ticks)", "Value");

                // Loop through each stat and plot it
                for (size_t i = 0; i < labels.size(); ++i) {
                    if (!plot_data[i].empty()) {
                        ImPlot::PlotLine(
                            labels[i].c_str(),
                            x_axis.data(),
                            plot_data[i].data(),
                            (int)x_axis.size()
                        );
                    }
                }
                ImPlot::EndPlot();
            }
        }
        ImGui::Separator();
        if(entity->entityDiseaseType == -1){
            ImGui::Text("No actual disease" );
        }else{
            Disease d;
            ImGui::Text("Contaminated by %s", Disease::getDiseaseName(entity->entityDiseaseType));
            ImGui::Text("AntiBody Percentage %d", entity->entityAntiBody);
        }
        ImGui::Separator();

        ImGui::Text("== Pointed Attributes ==");
        if(entity->list_entityPointedDesire.size() > 0){
            ImGui::Text("Desire List");
            for(int i=0; i < entity->list_entityPointedDesire.size(); i++){
                Entity* pointed = entity->list_entityPointedDesire.at(i).pointedEntity;
                ImGui::Text("%.1f -> %d : %s", entity->list_entityPointedDesire.at(i).desire,
                        pointed->entityId, pointed->name.c_str());
            }
        }

        if(entity->list_entityPointedAnger.size() > 0){
            ImGui::Text("Anger List");
            for(int i=0; i < entity->list_entityPointedAnger.size(); i++){
                Entity* pointed = entity->list_entityPointedAnger.at(i).pointedEntity;
                ImGui::Text("%.1f -> %d : %s", entity->list_entityPointedAnger.at(i).anger,
                        pointed->entityId, pointed->name.c_str());
            }
        }

        if(entity->list_entityPointedCouple.size() > 0){
            ImGui::Text("Couple List");
            for(int i=0; i < entity->list_entityPointedCouple.size(); i++){
                Entity* pointed = entity->list_entityPointedCouple.at(i).pointedEntity;
                ImGui::Text("=> %.1f : %s", pointed->entityId, pointed->name.c_str());
            }
        }

        if(entity->list_entityPointedSocial.size() > 0){
            ImGui::Text("Social List");
            for(int i=0; i < entity->list_entityPointedSocial.size(); i++){
                Entity* pointed = entity->list_entityPointedSocial.at(i).pointedEntity;
                ImGui::Text("%.1f -> %d : %s", entity->list_entityPointedSocial.at(i).social,
                        pointed->entityId, pointed->name.c_str());

            }
        }

        ImGui::End();
    }

// DrawGrid implementation
void UI::DrawGrid(std::vector<Entity*>& entities, float pointSize) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    for (auto& entity : entities) {
        ImU32 color = entity->selected ? IM_COL32(255, 100, 100, 255) : IM_COL32(200, 200, 200, 255);
        draw_list->AddCircleFilled(ImVec2(entity->posX, entity->posY), pointSize, color);
    }
}

// HandlePointMovement implementation
// Returns the index of the selected/moved entity, -1 if none
int UI::HandlePointMovement(std::vector<Entity*>& entities) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool mouseClicked = ImGui::IsMouseClicked(0);
    bool mouseHeld = ImGui::IsMouseDown(0);

    static int selectedIndex = -1;

    if (mouseClicked) {
        selectedIndex = -1;
        for (int i = 0; i < entities.size(); i++) {
            float dx = mousePos.x - entities[i]->posX;
            float dy = mousePos.y - entities[i]->posY;
            if (dx * dx + dy * dy < 64.0f) { // radius²
                selectedIndex = i;
                entities[i]->selected = true;
                break;
            } else {
                entities[i]->selected = false;
            }
        }
    }

    if (mouseHeld && selectedIndex >= 0) {
        entities[selectedIndex]->posX = mousePos.x;
        entities[selectedIndex]->posY = mousePos.y;
    }

    return selectedIndex; // returns -1 if no entity is selected
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
