#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <string>
#include <vector>
#include "./header/Entity.h"
#include "./header/UI.h"
#include "./header/NarrativeEngine.h"
#include "./header/CivilizationEngine.h"
#include "./header/PersonaSystem.h"
#include <iostream>
#include "./header/Disease.h"
#include <map>
#include "./header/implot.h"
#include "./header/implot_internal.h"
#include <sstream>
#include <filesystem>
#include <fstream>
#include <algorithm>

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


    ImGui::Separator();
    if (ImGui::CollapsingHeader("LEGEND")) {
        ImGui::TextDisabled("--- Social Graph Dots ---");
        ImGui::ColorButton("##sel",    ImVec4(1.0f,0.39f,0.39f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Selected");
        ImGui::ColorButton("##sick",   ImVec4(0.67f,0.86f,0.24f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Sick entity");
        ImGui::ColorButton("##norm",   ImVec4(0.67f,0.64f,0.75f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Normal (blue-purple = happy)");
        ImGui::TextDisabled("--- Relationship Lines ---");
        ImGui::ColorButton("##des",    ImVec4(1.0f,0.31f,0.71f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Desire");
        ImGui::ColorButton("##ang",    ImVec4(1.0f,0.16f,0.16f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Anger");
        ImGui::ColorButton("##soc",    ImVec4(0.24f,0.86f,0.86f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Social bond");
        ImGui::ColorButton("##coup",   ImVec4(1.0f,0.84f,0.0f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Couple (thick)");
        ImGui::TextDisabled("--- Mind Board Bars ---");
        ImGui::ColorButton("##bh",     ImVec4(0.2f,0.85f,0.3f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Health");
        ImGui::ColorButton("##bhp",    ImVec4(0.9f,0.75f,0.1f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Happiness");
        ImGui::ColorButton("##bst",    ImVec4(0.9f,0.25f,0.2f,1.0f), 0, ImVec2(12,12)); ImGui::SameLine(); ImGui::Text("Stress");
    }
    ImGui::Separator();

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

    void UI::ShowEntityWindow(Entity* entity, bool* p_open, std::vector<Entity*> entities) {
        if (!ImGui::Begin("=== Entity Statistics ===", p_open, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            return;
        }


        ImGui::Text("ID: %d", entity->entityId);
        ImGui::Text("Name: %s", entity->name.c_str());

        ImGui::Separator();

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
        //ImGui::Text("Hygiene: %d", entity->entityHygiene);

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
        if(ImGui::Button("Show detailed Action Information")){
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
            ImPlot::CreateContext();
            try
            {
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
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
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
        ImGui::Text("Wealth: %.0f tokens", entity->salary.token);
        ImGui::Text("Monthly revenue: %.0f", entity->salary.getMonthlyRevenue());
        if (entity->salary.producedProduct >= 0 &&
            entity->salary.producedProduct < (int)g_market.products.size())
            ImGui::Text("Sells: %s",
                        g_market.products[entity->salary.producedProduct].name.c_str());

        ImGui::Text("== Pointed Attributes ==");
// DESIRE — pink
if (!entity->list_entityPointedDesire.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.7f, 1.0f), "Desire (%d)", (int)entity->list_entityPointedDesire.size());
    for (auto& d : entity->list_entityPointedDesire) {

        if (!d.pointedEntity) continue;
        if(std::find(entities.begin(), entities.end(), d.pointedEntity) != entities.end()){

        ImGui::Text("  %s (#%d)", d.pointedEntity->name.c_str(), d.pointedEntity->entityId);
        ImGui::SameLine(160);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.4f, 0.7f, 0.85f));
        char label[32]; snprintf(label, sizeof(label), "##des%d", d.pointedEntity->entityId);
        ImGui::ProgressBar(d.desire / 100.0f, ImVec2(100.0f, 12.0f), label);
        ImGui::PopStyleColor();
        ImGui::SameLine(); ImGui::Text("%.0f", d.desire);
        }
    }
    ImGui::Spacing();
}

// ANGER — red
if (!entity->list_entityPointedAnger.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Anger (%d)", (int)entity->list_entityPointedAnger.size());
    for (auto& a : entity->list_entityPointedAnger) {
        if (!a.pointedEntity) continue;
        if(std::find(entities.begin(), entities.end(), a.pointedEntity) != entities.end()){

        ImGui::Text("  %s (#%d)", a.pointedEntity->name.c_str(), a.pointedEntity->entityId);
        ImGui::SameLine(160);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.2f, 0.2f, 0.85f));
        char label[32]; snprintf(label, sizeof(label), "##ang%d", a.pointedEntity->entityId);
        ImGui::ProgressBar(a.anger / 100.0f, ImVec2(100.0f, 12.0f), label);
        ImGui::PopStyleColor();
        ImGui::SameLine(); ImGui::Text("%.0f", a.anger);
        }
    }
    ImGui::Spacing();
}

    // SOCIAL — cyan
    if (!entity->list_entityPointedSocial.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.9f, 1.0f), "Social bonds (%d)", (int)entity->list_entityPointedSocial.size());
        for (auto& s : entity->list_entityPointedSocial) {
            if (!s.pointedEntity) continue;
            if(std::find(entities.begin(), entities.end(), s.pointedEntity) != entities.end()){

            ImGui::Text("  %s (#%d)", s.pointedEntity->name.c_str(), s.pointedEntity->entityId);
            ImGui::SameLine(160);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.9f, 0.9f, 0.85f));
            char label[32]; snprintf(label, sizeof(label), "##soc%d", s.pointedEntity->entityId);
            ImGui::ProgressBar(s.social / 100.0f, ImVec2(100.0f, 12.0f), label);
            ImGui::PopStyleColor();
            ImGui::SameLine(); ImGui::Text("%.0f", s.social);
            }
        }
        ImGui::Spacing();
    }

    // COUPLE — gold
    if (!entity->list_entityPointedCouple.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Partner");
        for (auto& c : entity->list_entityPointedCouple) {
            if (!c.pointedEntity) continue;
            if(std::find(entities.begin(), entities.end(), c.pointedEntity) != entities.end()){
                if(c.pointedEntity->entityHealth > 0.0f){
                    ImGui::Text("  Couple  %s (#%d)", c.pointedEntity->name.c_str(), c.pointedEntity->entityId);
                }
            }
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

        // ── PersonaSystem ──────────────────────────────────────────────────────
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.5f, 1.0f, 1.0f), "== Inner State ==");
        ImGui::Spacing();

        // Body language
        ImGui::Text("Presence: %s", bodyLanguageCueLabel(entity->bodyLanguage));
        ImGui::SameLine(160);
        ImGui::TextDisabled("(%s)", bodyLanguageCueDesc(entity->bodyLanguage));

        // PAD bars
        ImGui::Spacing();
        ImGui::TextDisabled("PAD Emotional Model");

        auto padBar = [](const char* label, float val, ImVec4 col) {
            ImGui::Text("%-12s", label);
            ImGui::SameLine(110);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
            char id[32]; snprintf(id, sizeof(id), "##pad_%s", label);
            float norm = (val + 100.0f) / 200.0f;
            ImGui::ProgressBar(norm, ImVec2(120.0f, 10.0f), id);
            ImGui::PopStyleColor();
            ImGui::SameLine(); ImGui::Text("%.0f", val);
        };
        padBar("Pleasure",  entity->pad.pleasure,  ImVec4(0.3f, 0.9f, 0.4f, 0.85f));
        padBar("Arousal",   entity->pad.arousal,   ImVec4(0.9f, 0.6f, 0.1f, 0.85f));
        padBar("Dominance", entity->pad.dominance, ImVec4(0.4f, 0.5f, 1.0f, 0.85f));

        // Self-grounding sentence
        if (!entity->selfGrounding.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Self:");
            ImGui::TextWrapped("%s", entity->selfGrounding.c_str());
        }

        // Core beliefs
        if (!entity->coreBeliefs.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Core Beliefs (%d)", (int)entity->coreBeliefs.size());
            for (const auto& b : entity->coreBeliefs) {
                ImVec4 col = b.valence >= 0
                    ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f)
                    : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                ImGui::TextColored(col, "  [%.0f] %s", b.strength, b.belief.c_str());
            }
        }

        // Last Chain-of-Thought
        if (!entity->lastCoT.steps.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Last Decision Trace:");
            for (const auto& step : entity->lastCoT.steps) {
                ImGui::TextWrapped("  [%s] %s", step.phase.c_str(), step.content.c_str());
            }
            if (entity->lastCoT.isImpulsive)
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f), "  * Impulsive choice");
        }

        ImGui::End();
    }

// Compute a stable circular layout position for entity at index i out of n total.
// Used by both DrawGrid and HandlePointMovement so click detection is consistent.
static ImVec2 socialGraphPos(int i, int n, float cx = 700.0f, float cy = 330.0f) {
    if (n <= 0) return ImVec2(cx, cy);
    float radius = std::min(cx, cy) * 0.85f;
    float angle  = (2.0f * 3.14159265f * i) / n - 3.14159265f / 2.0f;
    return ImVec2(cx + radius * std::cos(angle), cy + radius * std::sin(angle));
}

// DrawGrid — social network graph.  No spatial positions: layout is circular,
// relationship lines reveal who knows / loves / hates whom.
void UI::DrawGrid(std::vector<Entity*>& entities, float pointSize) {
    if (entities.empty()) return;
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    int n = (int)entities.size();

    // Build id→index map for relationship lines
    std::map<int, int> idToIdx;
    for (int i = 0; i < n; ++i)
        idToIdx[entities[i]->entityId] = i;

    // ── 1. Relationship lines ─────────────────────────────────────────────────
    for (int i = 0; i < n; ++i) {
        Entity* ent = entities[i];
        ImVec2 from = socialGraphPos(i, n);

        for (auto& d : ent->list_entityPointedDesire) {
            if (!d.pointedEntity) continue;
            auto it = idToIdx.find(d.pointedEntity->entityId);
            if (it == idToIdx.end()) continue;
            ImVec2 to = socialGraphPos(it->second, n);
            float alpha = std::min(1.0f, d.desire / 100.0f);
            draw_list->AddLine(from, to, IM_COL32(255, 80, 180, (int)(alpha * 160)),
                               1.0f + (d.desire / 100.0f) * 2.5f);
        }
        for (auto& a : ent->list_entityPointedAnger) {
            if (!a.pointedEntity) continue;
            auto it = idToIdx.find(a.pointedEntity->entityId);
            if (it == idToIdx.end()) continue;
            ImVec2 to = socialGraphPos(it->second, n);
            float alpha = std::min(1.0f, a.anger / 100.0f);
            draw_list->AddLine(from, to, IM_COL32(255, 40, 40, (int)(alpha * 160)),
                               1.0f + (a.anger / 100.0f) * 2.5f);
        }
        for (auto& s : ent->list_entityPointedSocial) {
            if (!s.pointedEntity) continue;
            auto it = idToIdx.find(s.pointedEntity->entityId);
            if (it == idToIdx.end()) continue;
            ImVec2 to = socialGraphPos(it->second, n);
            float alpha = std::min(1.0f, s.social / 100.0f);
            draw_list->AddLine(from, to, IM_COL32(60, 220, 220, (int)(alpha * 130)),
                               1.0f + (s.social / 100.0f) * 2.0f);
        }
        for (auto& c : ent->list_entityPointedCouple) {
            if (!c.pointedEntity) continue;
            auto it = idToIdx.find(c.pointedEntity->entityId);
            if (it == idToIdx.end()) continue;
            ImVec2 to = socialGraphPos(it->second, n);
            draw_list->AddLine(from, to, IM_COL32(255, 215, 0, 210), 3.0f);
        }
    }



    // ── 2. Entity dots ────────────────────────────────────────────────────────
    for (int i = 0; i < n; ++i) {
        Entity* entity = entities[i];
        ImVec2  pos    = socialGraphPos(i, n);
        bool isSick    = (entity->entityDiseaseType != -1);

        ImU32 color;
        float size = pointSize;

        if (entity->selected) {
            color = IM_COL32(255, 100, 100, 255);
        } else if (isSick) {
            color = IM_COL32(170, 220, 60, 255);
        } else {
            float h = std::max(0.0f, std::min(1.0f, entity->entityHapiness / 100.0f));
            color = IM_COL32((int)(160 + h * 60), (int)(160 + h * 20), (int)(200 - h * 60), 255);
        }

        draw_list->AddCircleFilled(pos, size, color);

        if (entity->selected) {
            draw_list->AddText(ImVec2(pos.x + size + 3.0f, pos.y - 7.0f),
                               IM_COL32(255, 160, 160, 255),
                               entity->name.c_str());
        }
    }
}

// HandlePointMovement — click on a node in the social graph to select it.
int UI::HandlePointMovement(std::vector<Entity*>& entities) {
    ImVec2 mousePos  = ImGui::GetIO().MousePos;
    bool mouseClicked = ImGui::IsMouseClicked(0);
    static int selectedIndex = -1;

    if (mouseClicked) {
        int n = (int)entities.size();
        selectedIndex = -1;
        for (int i = 0; i < n; ++i) {
            ImVec2 pos = socialGraphPos(i, n);
            float dx = mousePos.x - pos.x;
            float dy = mousePos.y - pos.y;
            if (dx * dx + dy * dy < 100.0f) {
                selectedIndex = i;
                for (auto* e : entities) e->selected = false;
                entities[i]->selected = true;
                break;
            }
        }
    }

    return selectedIndex;
}


// ── ShowCivilizationPanel ─────────────────────────────────────────────────────
void UI::ShowCivilizationPanel(int simDay) {
    if (!globalCivEngine) return;
    CivilizationEngine& civ = *globalCivEngine;

    ImGui::SetNextWindowSize(ImVec2(460, 680), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 420),   ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);

    if (!ImGui::Begin("CIVILIZATION", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End(); return;
    }

    // Era header
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", civ.getEraName().c_str());
    ImGui::SameLine(220);
    ImGui::TextDisabled("Day %d  |  Tribes: %d  |  Religions: %d  |  Tech: %d",
                        simDay, (int)civ.tribes.size(),
                        (int)civ.religions.size(), (int)civ.innovations.size());
    ImGui::Separator();

    // ── TRIBES ───────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("TRIBES", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (civ.tribes.empty()) {
            ImGui::TextDisabled("  No tribes yet — awaiting a leader...");
        }
        for (const auto& tribe : civ.tribes) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.75f, 0.35f, 1.0f));
            ImGui::Text("  %s  [%d members]", tribe.name.c_str(), tribe.population());
            ImGui::PopStyleColor();
            ImGui::SameLine(280);

            // Stance summary
            int atWar = 0, allied = 0;
            for (auto& p : tribe.stances) {
                if (p.second == TS_AT_WAR) atWar++;
                if (p.second == TS_ALLY)   allied++;
            }
            if (atWar > 0)
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "WAR x%d", atWar);
            else if (allied > 0)
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Allied x%d", allied);
            else
                ImGui::TextDisabled("Neutral");

            // Values mini-bars
            ImGui::Text("    Mil"); ImGui::SameLine(80);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f,0.3f,0.2f,0.8f));
            char id1[32]; snprintf(id1,32,"##mil%d",tribe.id);
            ImGui::ProgressBar(tribe.militarism/100.0f, ImVec2(60,8), id1);
            ImGui::PopStyleColor();
            ImGui::SameLine(160); ImGui::Text("Spi"); ImGui::SameLine(190);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.6f,0.4f,0.9f,0.8f));
            char id2[32]; snprintf(id2,32,"##spi%d",tribe.id);
            ImGui::ProgressBar(tribe.spiritualism/100.0f, ImVec2(60,8), id2);
            ImGui::PopStyleColor();
            ImGui::SameLine(260); ImGui::Text("Inn"); ImGui::SameLine(290);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f,0.8f,0.9f,0.8f));
            char id3[32]; snprintf(id3,32,"##inn%d",tribe.id);
            ImGui::ProgressBar(tribe.innovation/100.0f, ImVec2(60,8), id3);
            ImGui::PopStyleColor();

            // Known tech count
            if (!tribe.knownTechIds.empty())
                ImGui::TextDisabled("    %d technologies known", (int)tribe.knownTechIds.size());
            ImGui::Spacing();
        }
    }

    ImGui::Separator();

    // ── RELIGIONS ─────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("RELIGIONS", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (civ.religions.empty()) {
            ImGui::TextDisabled("  No religion yet — awaiting a prophet...");
        }
        for (const auto& rel : civ.religions) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.55f, 1.0f, 1.0f));
            ImGui::Text("  %s", rel.name.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine(260);
            ImGui::TextDisabled("%d followers", (int)rel.followerIds.size());

            // Principle
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.85f, 1.0f));
            ImGui::TextWrapped("    \"%s\"", rel.holyPrinciple.c_str());
            ImGui::PopStyleColor();

            // Doctrine tags
            const char* moralStr[] = {"Strict","Peaceful","Warrior","Flexible"};
            const char* ritualStr[]= {"Daily Prayer","Weekly Gathering","Meditation","Ceremony","Sacrifice"};
            ImGui::TextDisabled("    %s  |  %s  |  %s",
                moralStr[rel.moralCode],
                ritualStr[rel.ritual],
                rel.isPolytheistic ? "Polytheistic" : "Monotheistic");
            ImGui::Spacing();
        }
    }

    ImGui::Separator();

    // ── INNOVATIONS ───────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("INNOVATIONS")) {
        if (civ.innovations.empty()) {
            ImGui::TextDisabled("  No discoveries yet...");
        }
        // Group by category
        std::map<std::string, std::vector<const Innovation*>> byCategory;
        for (const auto& inv : civ.innovations)
            byCategory[inv.category].push_back(&inv);

        static const std::map<std::string, ImVec4> catColor = {
            {"agriculture", ImVec4(0.4f,0.85f,0.3f,1.0f)},
            {"tool",        ImVec4(0.8f,0.7f, 0.2f,1.0f)},
            {"medicine",    ImVec4(0.3f,0.85f,0.85f,1.0f)},
            {"social",      ImVec4(0.6f,0.8f, 1.0f,1.0f)},
            {"military",    ImVec4(0.9f,0.35f,0.2f,1.0f)},
            {"spiritual",   ImVec4(0.75f,0.5f,1.0f,1.0f)},
        };
        for (const auto& cat : byCategory) {
            ImVec4 col = catColor.count(cat.first) ? catColor.at(cat.first)
                                                    : ImVec4(0.8f,0.8f,0.8f,1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::Text("  [%s]", cat.first.c_str());
            ImGui::PopStyleColor();
            for (const Innovation* inv : cat.second) {
                ImGui::Text("    • %s", inv->name.c_str());
                ImGui::SameLine(220);
                ImGui::TextDisabled("Day %d  |  %d know it", inv->discoveredOnDay, inv->knowerCount);
            }
        }
    }

    ImGui::Separator();

    // ── EVENT LOG ─────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("HISTORY", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("##civlog", ImVec2(0, 160), true);
        for (const auto& ev : civ.eventLog) {
            ImVec4 col = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            if      (ev.category == "war")       col = ImVec4(1.0f, 0.35f, 0.25f, 1.0f);
            else if (ev.category == "religion")  col = ImVec4(0.75f,0.5f, 1.0f,  1.0f);
            else if (ev.category == "innovation")col = ImVec4(0.3f, 0.9f, 0.7f,  1.0f);
            else if (ev.category == "diplomacy") col = ImVec4(0.3f, 0.8f, 1.0f,  1.0f);
            else if (ev.category == "tribe")     col = ImVec4(1.0f, 0.78f,0.3f,  1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextWrapped("[Day %d] %s", ev.day, ev.description.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }

    ImGui::End();
}


// ── ShowMarketPanel ───────────────────────────────────────────────────────────
// Live supply & demand for every tradable good. Prices climb when the
// population wants more than is produced and fall when shelves overflow.
static void DrawMarketRows(GoodCategory cat) {
    if (!ImGui::BeginTable("##market", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_SizingStretchProp))
        return;

    ImGui::TableSetupColumn("Good",   ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("Price",  ImGuiTableColumnFlags_WidthStretch, 0.9f);
    ImGui::TableSetupColumn(u8"Δ%",   ImGuiTableColumnFlags_WidthStretch, 0.7f);
    ImGui::TableSetupColumn("Supply", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Demand", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Sold",   ImGuiTableColumnFlags_WidthStretch, 0.6f);
    ImGui::TableSetupColumn("Trend",  ImGuiTableColumnFlags_WidthStretch, 1.4f);
    ImGui::TableHeadersRow();

    for (const auto& p : g_market.products) {
        if (p.category != cat) continue;
        ImGui::TableNextRow();

        // Good name (highlight wartime rations).
        ImGui::TableNextColumn();
        if (p.isArmyRation)
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.35f, 1.0f), "%s", p.name.c_str());
        else
            ImGui::TextUnformatted(p.name.c_str());

        // Current price, coloured by how far it sits from its natural value.
        float pct = p.basePrice > 0.01f ? (p.price - p.basePrice) / p.basePrice : 0.0f;
        ImVec4 priceCol = pct > 0.05f  ? ImVec4(1.0f, 0.45f, 0.4f, 1.0f)   // expensive
                        : pct < -0.05f ? ImVec4(0.45f, 0.95f, 0.55f, 1.0f) // cheap
                                       : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        ImGui::TableNextColumn();
        ImGui::TextColored(priceCol, "%.0f", p.price);

        ImGui::TableNextColumn();
        ImGui::TextColored(priceCol, "%+.0f%%", pct * 100.0f);

        // Supply / demand bars on a shared scale so imbalance is obvious.
        float scale = std::max(1.0f, std::max(p.supply, p.demand));
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.35f, 0.7f, 0.95f, 0.85f));
        ImGui::ProgressBar(p.supply / scale, ImVec2(-1, 12), "");
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.95f, 0.55f, 0.3f, 0.85f));
        ImGui::ProgressBar(p.demand / scale, ImVec2(-1, 12), "");
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::Text("%.0f", p.lastVolume);

        // Price history sparkline. Auto-scale to the window's own min/max (with a
        // little padding) so the actual price wiggle is visible instead of a flat
        // line lost inside a fixed 0.3x-4x range.
        ImGui::TableNextColumn();
        if (p.priceHistory.size() > 1) {
            std::vector<float> hist(p.priceHistory.begin(), p.priceHistory.end());
            float lo = hist[0], hi = hist[0];
            for (float v : hist) { lo = std::min(lo, v); hi = std::max(hi, v); }
            float pad = std::max(1.0f, (hi - lo) * 0.15f);
            lo -= pad; hi += pad;
            ImGui::PushStyleColor(ImGuiCol_PlotLines, priceCol);
            // Unique id per good so ImGui doesn't merge the plots.
            std::string id = "##trend_" + p.name;
            ImGui::PlotLines(id.c_str(), hist.data(), (int)hist.size(), 0, nullptr,
                             lo, hi, ImVec2(-1.0f, 24.0f));
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("...");
        }
    }
    ImGui::EndTable();
}

void UI::ShowMarketPanel() {
    if (!g_market.initialized) return;

    ImGui::SetNextWindowSize(ImVec2(560, 640), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(480, 420),  ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.92f);

    if (!ImGui::Begin("MARKET", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End(); return;
    }

    ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "Supply & Demand");
    ImGui::SameLine(180);
    ImGui::TextDisabled("Traded: %.0f  |  Money supply: %.0f tokens",
                        g_market.totalTradeVolume, g_market.totalMoneySupply);

    // War pressure — soldiers stockpiling rations bids food prices up.
    ImGui::Text("War pressure"); ImGui::SameLine(110);
    ImVec4 warCol = g_market.lastWarIntensity > 0.2f
                        ? ImVec4(1.0f, 0.35f, 0.3f, 1.0f)
                        : ImVec4(0.4f, 0.8f, 0.5f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, warCol);
    char wbuf[32]; snprintf(wbuf, 32, "%.0f%%", g_market.lastWarIntensity * 100.0f);
    ImGui::ProgressBar(g_market.lastWarIntensity, ImVec2(-1, 14), wbuf);
    ImGui::PopStyleColor();

    ImGui::Separator();
    ImGui::TextDisabled("Blue = supply   Orange = demand   rations in gold");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("FOOD MARKET", ImGuiTreeNodeFlags_DefaultOpen))
        DrawMarketRows(GoodCategory::FOOD);

    ImGui::Spacing();

    if (ImGui::CollapsingHeader("OBJECT MARKET", ImGuiTreeNodeFlags_DefaultOpen))
        DrawMarketRows(GoodCategory::OBJECT);

    ImGui::End();
}


// ── ShowMindBoard ─────────────────────────────────────────────────────────────
// Scrollable card grid showing every entity's inner state at a glance.
// Returns the index of a clicked entity, -1 otherwise.
int UI::ShowMindBoard(std::vector<Entity*>& entities) {
    int selected = -1;

    ImGui::SetNextWindowSize(ImVec2(1395, 340), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0, 710),     ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.88f);

    if (!ImGui::Begin("MIND BOARD", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::End(); return selected;
    }

    const float cardW   = 210.0f;
    const float cardH   = 120.0f;
    const float padX    = 6.0f;
    float availW        = ImGui::GetContentRegionAvail().x;
    int   cols          = std::max(1, (int)((availW + padX) / (cardW + padX)));
    int   col           = 0;

    for (int i = 0; i < (int)entities.size(); ++i) {
        Entity* ent = entities[i];
        if (ent->entityHealth <= 0.0f) continue;

        if (col > 0) ImGui::SameLine(0.0f, padX);

        float s = ent->entityStress / 100.0f;
        float h = ent->entityHapiness / 100.0f;
        ImVec4 bg = ImVec4(0.12f + s * 0.08f, 0.12f + h * 0.06f, 0.18f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.35f, 0.5f, 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        char cid[32]; snprintf(cid, 32, "##mc%d", ent->entityId);
        ImGui::BeginChild(cid, ImVec2(cardW, cardH), true);

        // Name + age line
        ImGui::Text("%s, %d", ent->name.c_str(), (int)ent->entityAge);

        // Last action
        if (!ent->lastActionName.empty())
            ImGui::TextDisabled("[%s]", ent->lastActionName.c_str());

        ImGui::Separator();

        // Inner monologue (truncated to ~90 chars)
        if (!ent->innerMonologue.empty()) {
            std::string mono = ent->innerMonologue;
            if (mono.size() > 88) mono = mono.substr(0, 85) + "...";
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.74f, 1.0f));
            ImGui::TextWrapped("\"%s\"", mono.c_str());
            ImGui::PopStyleColor();
        }

        // Mini stat bars (health / happiness / stress)
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 1));
        char b1[32], b2[32], b3[32];
        snprintf(b1, 32, "##h%d",  ent->entityId);
        snprintf(b2, 32, "##hp%d", ent->entityId);
        snprintf(b3, 32, "##s%d",  ent->entityId);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.85f, 0.3f, 0.8f));
        ImGui::ProgressBar(ent->entityHealth    / 100.0f, ImVec2(56, 5), b1);
        ImGui::PopStyleColor(); ImGui::SameLine(0.0f, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f, 0.75f, 0.1f, 0.8f));
        ImGui::ProgressBar(ent->entityHapiness  / 100.0f, ImVec2(56, 5), b2);
        ImGui::PopStyleColor(); ImGui::SameLine(0.0f, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f, 0.25f, 0.2f, 0.8f));
        ImGui::ProgressBar(ent->entityStress     / 100.0f, ImVec2(56, 5), b3);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // Click-to-select
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
            selected = i;
            for (auto* e : entities) e->selected = false;
            ent->selected = true;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        col = (col + 1) % cols;
    }

    ImGui::End();
    return selected;
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
