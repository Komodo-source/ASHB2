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
#include <filesystem>
#include <fstream>

// ShowEntityWindow implementation

UI::GridPoint UI::getGridPoint() {
    return gridPoint;
}

//complementary_info = nombre de couple, nombre de mort etc etc


int UI::showSaveLoadButtons(std::string& filename, int day, int num_entity, int tick, std::map<std::string, int> complementary_information) {
    int result = 0;
    ImGui::Begin("=== Stats  + Save ===");

    ImGui::Text("Number Entities: %d", num_entity);
    ImGui::Text("day: %d", day);
    ImGui::Text("actual tick: %d", tick);
    ImGui::Separator();
    if (ImGui::Button("Create Entity")) {
        //createPlayer()
        ;
    }
    if (ImGui::Button(simulationPaused ? "Resume Simulation" : "Stop Simulation")) {
        simulationPaused = !simulationPaused;
    }
    if (simulationPaused) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "PAUSED");
    }
    ImGui::Separator();
    for(auto& p : complementary_information){
        ImGui::Text("%s: %d", p.first,p.second);
    }


    ImGui::InputText("File", saveLoadFilename, sizeof(saveLoadFilename));
    if (ImGui::Button("Save Game")) {
        result = 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Game")) {
        result = 2;
    }

    ImGui::Separator();
    ImGui::Text("Files in directory:");
    ImGui::BeginChild("FileBrowser", ImVec2(0, 150), true);
    try {
        std::filesystem::path exeDir = std::filesystem::current_path();
        for (const auto& entry : std::filesystem::directory_iterator(exeDir)) {
            if (entry.is_regular_file()) {
                std::string name = entry.path().filename().string();
                if (ImGui::Selectable(name.c_str())) {
                    strncpy(saveLoadFilename, name.c_str(), sizeof(saveLoadFilename) - 1);
                    saveLoadFilename[sizeof(saveLoadFilename) - 1] = '\0';
                }
            }
        }
    } catch (...) {}
    ImGui::EndChild();

    filename = std::string(saveLoadFilename);
    ImGui::End();
    return result;
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

        ImGui::Text("Life Goal: %s", entity->getTypeGoal().c_str());
        ImGui::Text("   ->: %.2f %%", (float)entity->progressGoal());
        ImGui::Spacing();
        ImGui::Text(" === Personnality ===");
        ImGui::Text("extraversion: %.2f", entity->personality.extraversion);
        ImGui::Text("agreeableness: %.2f", entity->personality.agreeableness);
        ImGui::Text("conscientiousness: %.2f", entity->personality.conscientiousness);
        ImGui::Text("neuroticism: %.2f", entity->personality.neuroticism);
        ImGui::Text("openness: %.2f", entity->personality.openness);
        ImGui::Spacing();
        ImGui::Text(" === AI Entity Information ===");

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

        //std::ofstream MyFile("test_links.txt");

        ImGui::Text("== Pointed Attributes ==");

// DESIRE — pink
if (!entity->list_entityPointedDesire.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.7f, 1.0f), "Desire (%d)", (int)entity->list_entityPointedDesire.size());
    for (auto& d : entity->list_entityPointedDesire) {
        if (!d.pointedEntity) continue;
        ImGui::Text("  %s (#%d)", d.pointedEntity->name.c_str(), d.pointedEntity->entityId);
        ImGui::SameLine(160);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.4f, 0.7f, 0.85f));
        char label[32]; snprintf(label, sizeof(label), "##des%d", d.pointedEntity->entityId);
        ImGui::ProgressBar(d.desire / 100.0f, ImVec2(100.0f, 12.0f), label);
        ImGui::PopStyleColor();
        ImGui::SameLine(); ImGui::Text("%.0f", d.desire);
    }
    ImGui::Spacing();
}

// ANGER — red
if (!entity->list_entityPointedAnger.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Anger (%d)", (int)entity->list_entityPointedAnger.size());
    for (auto& a : entity->list_entityPointedAnger) {
        if (!a.pointedEntity) continue;
        ImGui::Text("  %s (#%d)", a.pointedEntity->name.c_str(), a.pointedEntity->entityId);
        ImGui::SameLine(160);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.2f, 0.2f, 0.85f));
        char label[32]; snprintf(label, sizeof(label), "##ang%d", a.pointedEntity->entityId);
        ImGui::ProgressBar(a.anger / 100.0f, ImVec2(100.0f, 12.0f), label);
        ImGui::PopStyleColor();
        ImGui::SameLine(); ImGui::Text("%.0f", a.anger);
    }
    ImGui::Spacing();
}

    // SOCIAL — cyan
    if (!entity->list_entityPointedSocial.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.9f, 1.0f), "Social bonds (%d)", (int)entity->list_entityPointedSocial.size());
        for (auto& s : entity->list_entityPointedSocial) {
            if (!s.pointedEntity) continue;
            ImGui::Text("  %s (#%d)", s.pointedEntity->name.c_str(), s.pointedEntity->entityId);
            ImGui::SameLine(160);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.9f, 0.9f, 0.85f));
            char label[32]; snprintf(label, sizeof(label), "##soc%d", s.pointedEntity->entityId);
            ImGui::ProgressBar(s.social / 100.0f, ImVec2(100.0f, 12.0f), label);
            ImGui::PopStyleColor();
            ImGui::SameLine(); ImGui::Text("%.0f", s.social);
        }
        ImGui::Spacing();
    }

    // COUPLE — gold
    if (!entity->list_entityPointedCouple.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Partner");
        for (auto& c : entity->list_entityPointedCouple) {
            if (!c.pointedEntity) continue;
            ImGui::Text("  Couple  %s (#%d)", c.pointedEntity->name.c_str(), c.pointedEntity->entityId);
        }
        ImGui::Spacing();
    }

    // If no relationships at all
    if (entity->list_entityPointedDesire.empty() &&
        entity->list_entityPointedAnger.empty() &&
        entity->list_entityPointedSocial.empty() &&
        entity->list_entityPointedCouple.empty()) {
        ImGui::TextDisabled("  No relationships yet.");
    }


        //MyFile.close();

        ImGui::End();
    }

// DrawGrid implementation
void UI::DrawGrid(std::vector<Entity*>& entities, float pointSize) {
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    // Draw lines between entities FIRST so they appear under the circles
    for (auto& entity : entities) {
        // Draw couple ties (pink, thick)
        for (auto& d : entity->list_entityPointedDesire) {
            if (!d.pointedEntity) continue;
            float alpha = std::min(1.0f, d.desire / 100.0f);
            ImU32 col = IM_COL32(255, 80, 180, (int)(alpha * 180));
            float thickness = 1.0f + (d.desire / 100.0f) * 3.0f;
            draw_list->AddLine(
                ImVec2(entity->posX, entity->posY),
                ImVec2(d.pointedEntity->posX, d.pointedEntity->posY),
                col, thickness);
        }

        // ANGER → red
        for (auto& a : entity->list_entityPointedAnger) {
            if (!a.pointedEntity) continue;
            float alpha = std::min(1.0f, a.anger / 100.0f);
            ImU32 col = IM_COL32(255, 40, 40, (int)(alpha * 180));
            float thickness = 1.0f + (a.anger / 100.0f) * 3.0f;
            draw_list->AddLine(
                ImVec2(entity->posX, entity->posY),
                ImVec2(a.pointedEntity->posX, a.pointedEntity->posY),
                col, thickness);
        }

        // SOCIAL → cyan/teal
        for (auto& s : entity->list_entityPointedSocial) {
            if (!s.pointedEntity) continue;
            float alpha = std::min(1.0f, s.social / 100.0f);
            ImU32 col = IM_COL32(60, 220, 220, (int)(alpha * 150));
            float thickness = 1.0f + (s.social / 100.0f) * 2.5f;
            draw_list->AddLine(
                ImVec2(entity->posX, entity->posY),
                ImVec2(s.pointedEntity->posX, s.pointedEntity->posY),
                col, thickness);
        }

        // COUPLE → gold, always thick
        for (auto& c : entity->list_entityPointedCouple) {
            if (!c.pointedEntity) continue;
            draw_list->AddLine(
                ImVec2(entity->posX, entity->posY),
                ImVec2(c.pointedEntity->posX, c.pointedEntity->posY),
                IM_COL32(255, 215, 0, 220), 3.5f);
        }
    }

    for (auto& entity : entities) {
        ImU32 color = entity->selected ? IM_COL32(255, 100, 100, 255) : IM_COL32(200, 200, 200, 255);
        draw_list->AddCircleFilled(ImVec2(entity->posX, entity->posY), pointSize, color);
    }
    for (auto& entity : entities) {
        ImU32 color = entity->selected
            ? IM_COL32(255, 100, 100, 255)
            : IM_COL32(200, 200, 200, 255);
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
