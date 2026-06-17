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
#include "./header/Kinship.h"
#include "world/ResourceSystem.h"
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

    // ── Climate / harvest readout (EnvironmentModel) ──────────────────────────
    extern std::string g_seasonName;
    extern float g_seasonTemperature;
    extern float g_seasonalFoodModifier;
    extern float g_harvestLuck;
    ImVec4 foodCol = g_seasonalFoodModifier < 0.7f ? ImVec4(1.0f, 0.4f, 0.3f, 1.0f)
                    : g_seasonalFoodModifier > 1.2f ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f)
                                                    : ImVec4(0.85f, 0.85f, 0.6f, 1.0f);
    ImGui::Text("Season: %s  (%.0f temp)", g_seasonName.c_str(), g_seasonTemperature);
    ImGui::TextColored(foodCol, "Food yield x%.2f  | harvest x%.2f",
                       g_seasonalFoodModifier, g_harvestLuck);
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
        // Subsistence readout: red when starving so the player feels the stakes.
        ImVec4 hungerCol = entity->entityHunger > 70.0f ? ImVec4(1.0f, 0.35f, 0.3f, 1.0f)
                          : entity->entityHunger > 40.0f ? ImVec4(1.0f, 0.8f, 0.3f, 1.0f)
                                                         : ImVec4(0.6f, 0.9f, 0.6f, 1.0f);
        ImGui::TextColored(hungerCol, "Hunger: %.1f   Food store: %.1f", entity->entityHunger, entity->foodStore);
        ImVec4 fatCol = entity->fatigueLevel > 70.0f ? ImVec4(1.0f, 0.45f, 0.3f, 1.0f)
                                                      : ImVec4(0.7f, 0.8f, 0.9f, 1.0f);
        ImGui::TextColored(fatCol, "Fatigue: %.1f", entity->fatigueLevel);
        // ── Division of labour & homeland resources ──────────────────────────
        if (entity->isSpecialist) {
            ImGui::TextColored(ImVec4(0.85f, 0.7f, 1.0f, 1.0f), "Role: %s (specialist, fed by granary)",
                               entity->specialization.empty() ? "artisan" : entity->specialization.c_str());
        } else {
            std::string role = "subsistence farmer";
            if (!entity->specialization.empty()) role += " (latent " + entity->specialization + ")";
            ImGui::TextColored(ImVec4(0.7f, 0.85f, 0.7f, 1.0f), "Role: %s", role.c_str());
        }
        if (g_resources.valid(entity->originRegionId)) {
            int rid = entity->originRegionId;
            ImGui::Text("Homeland: food x%.2f  water x%.2f  wood x%.2f  | quality %.0f%%",
                        g_resources.abundance(rid, RES_FOOD),
                        g_resources.abundance(rid, RES_WATER),
                        g_resources.abundance(rid, RES_WOOD),
                        g_resources.settlementQuality(rid) * 100.0f);
        }
        ImGui::Text("Birthday: %dth day", entity->entityBDay);
        //ImGui::Text("Hygiene: %d", entity->entityHygiene);

        // ── Kinship / family ─────────────────────────────────────────────────
        if (globalKinship) {
            ImGui::TextColored(ImVec4(0.85f, 0.78f, 0.55f, 1.0f), "Family: %s",
                               globalKinship->describeKin(*entity).c_str());
            if (entity->parent1Id >= 0 || entity->parent2Id >= 0)
                ImGui::Text("Parents: #%d, #%d", entity->parent1Id, entity->parent2Id);
        }

        // ── Social standing (class & clientela) ──────────────────────────────
        if (globalSocialOrder) {
            ImVec4 classCol = (entity->socialClass == CLASS_PATRICIAN) ? ImVec4(1.0f, 0.84f, 0.4f, 1.0f)
                            : (entity->socialClass == CLASS_SLAVE)     ? ImVec4(0.7f, 0.55f, 0.5f, 1.0f)
                                                                       : ImVec4(0.75f, 0.85f, 0.85f, 1.0f);
            ImGui::TextColored(classCol, "Standing: %s",
                               globalSocialOrder->describe(*entity, entities).c_str());
        }

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

// Stable, distinct-ish color for a tribe id (so clusters read at a glance).
static ImU32 tribeColor(int tribeId, int alpha = 255) {
    if (tribeId < 0) return IM_COL32(150, 150, 160, alpha); // "no tribe" = grey
    // Spread hues around the wheel using a large step so neighbors differ.
    float hue = std::fmod(tribeId * 0.61803398875f, 1.0f); // golden-ratio hashing
    float r, g, b;
    float h6 = hue * 6.0f; int seg = (int)h6; float f = h6 - seg;
    float q = 1.0f - f, t = f;
    switch (seg % 6) {
        case 0: r = 1; g = t; b = 0; break;
        case 1: r = q; g = 1; b = 0; break;
        case 2: r = 0; g = 1; b = t; break;
        case 3: r = 0; g = q; b = 1; break;
        case 4: r = t; g = 0; b = 1; break;
        default:r = 1; g = 0; b = q; break;
    }
    return IM_COL32((int)(70 + r * 185), (int)(70 + g * 185), (int)(70 + b * 185), alpha);
}

// Shared pan offset for the social graph. Right-mouse-drag scrolls the whole
// network on X/Y so clusters that sit off-screen can be brought into view.
// Both DrawGrid and HandlePointMovement add this to every node position, so
// click-detection stays aligned with what's drawn.
static ImVec2 g_graphPan(0.0f, 0.0f);

// Cluster-by-tribe layout: entities of the same tribe sit together in their own
// little ring; the tribes themselves are arranged on a big ring. This turns the
// old single-circle "hairball" into readable social neighborhoods at scale.
// Both DrawGrid and HandlePointMovement call this so click-detection stays exact.
static void computeSocialLayout(const std::vector<Entity*>& entities,
                                std::vector<ImVec2>& out,
                                std::vector<ImVec2>* clusterCenters = nullptr,
                                std::vector<int>* clusterTribe = nullptr) {
    const float PI = 3.14159265f;
    const float cx = 760.0f, cy = 410.0f;
    int n = (int)entities.size();
    out.assign(n, ImVec2(cx, cy));
    if (n == 0) return;

    // Group indices by tribe, preserving first-seen order for determinism.
    std::vector<int> tribeOrder;
    std::map<int, int> tribeSlot;                 // tribeId -> cluster index
    std::vector<std::vector<int>> clusters;
    for (int i = 0; i < n; ++i) {
        int t = entities[i]->tribeId;
        auto it = tribeSlot.find(t);
        if (it == tribeSlot.end()) {
            tribeSlot[t] = (int)clusters.size();
            tribeOrder.push_back(t);
            clusters.push_back({});
        }
        clusters[tribeSlot[t]].push_back(i);
    }

    int C = (int)clusters.size();
    float bigR = (C <= 1) ? 0.0f : std::min(cx, cy) * 0.78f;
    for (int k = 0; k < C; ++k) {
        float ca = (C <= 1) ? 0.0f : (2.0f * PI * k) / C - PI / 2.0f;
        ImVec2 center(cx + bigR * std::cos(ca), cy + bigR * std::sin(ca));
        if (clusterCenters) clusterCenters->push_back(center);
        if (clusterTribe)   clusterTribe->push_back(tribeOrder[k]);

        int m = (int)clusters[k].size();
        float sr = std::min(160.0f, 26.0f + m * 2.4f);
        for (int j = 0; j < m; ++j) {
            int idx = clusters[k][j];
            if (m == 1) { out[idx] = center; continue; }
            float a = (2.0f * PI * j) / m - PI / 2.0f;
            out[idx] = ImVec2(center.x + sr * std::cos(a), center.y + sr * std::sin(a));
        }
    }
}

// DrawGrid — social network graph. Entities are clustered by tribe; relationship
// lines reveal who loves / knows / hates whom. With many entities it switches to
// level-of-detail (only strong links + couples) and, when a node is selected, to
// an "ego focus" that shows just that person's relationships.
void UI::DrawGrid(std::vector<Entity*>& entities, float pointSize) {
    if (entities.empty()) return;
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    int n = (int)entities.size();

    std::vector<ImVec2> pos;
    std::vector<ImVec2> clusterCenters;
    std::vector<int>    clusterTribe;
    computeSocialLayout(entities, pos, &clusterCenters, &clusterTribe);

    // Apply the user's pan offset to everything we draw.
    for (auto& p : pos)            { p.x += g_graphPan.x; p.y += g_graphPan.y; }
    for (auto& c : clusterCenters) { c.x += g_graphPan.x; c.y += g_graphPan.y; }

    std::map<int, int> idToIdx;
    for (int i = 0; i < n; ++i) idToIdx[entities[i]->entityId] = i;

    // Faint tribe labels at each cluster centroid.
    for (size_t k = 0; k < clusterCenters.size(); ++k) {
        char buf[32];
        if (clusterTribe[k] < 0) snprintf(buf, sizeof(buf), "(no tribe)");
        else                     snprintf(buf, sizeof(buf), "Tribe %d", clusterTribe[k]);
        draw_list->AddText(ImVec2(clusterCenters[k].x - 22.0f, clusterCenters[k].y - 6.0f),
                           tribeColor(clusterTribe[k], 90), buf);
    }

    // All links are always drawn (no click required, no ego-focus gating).
    float thr = 0.0f;                    // show every relationship, even faint ones
    int   maxLines = 40000, lines = 0;   // hard cap so huge worlds stay responsive

    // Draw one entity's outgoing links.
    auto drawFor = [&](int i) {
        Entity* ent = entities[i];
        ImVec2 from = pos[i];
        // Couples first — always shown (they're the backbone of the network).
        for (auto& c : ent->list_entityPointedCouple) {
            if (!c.pointedEntity || lines >= maxLines) continue;
            auto it = idToIdx.find(c.pointedEntity->entityId);
            if (it == idToIdx.end()) continue;
            draw_list->AddLine(from, pos[it->second], IM_COL32(255, 215, 0, 220), 3.0f); ++lines;
        }
        for (auto& d : ent->list_entityPointedDesire) {
            if (!d.pointedEntity || d.desire < thr || lines >= maxLines) continue;
            auto it = idToIdx.find(d.pointedEntity->entityId);
            if (it == idToIdx.end()) continue;
            float al = std::min(1.0f, d.desire / 100.0f);
            draw_list->AddLine(from, pos[it->second], IM_COL32(255, 80, 180, (int)(60 + al * 150)),
                               1.0f + al * 2.5f); ++lines;
        }
        for (auto& a : ent->list_entityPointedAnger) {
            if (!a.pointedEntity || a.anger < thr || lines >= maxLines) continue;
            auto it = idToIdx.find(a.pointedEntity->entityId);
            if (it == idToIdx.end()) continue;
            float al = std::min(1.0f, a.anger / 100.0f);
            draw_list->AddLine(from, pos[it->second], IM_COL32(255, 45, 45, (int)(60 + al * 155)),
                               1.0f + al * 2.5f); ++lines;
        }
        for (auto& s : ent->list_entityPointedSocial) {
            if (!s.pointedEntity || s.social < thr || lines >= maxLines) continue;
            auto it = idToIdx.find(s.pointedEntity->entityId);
            if (it == idToIdx.end()) continue;
            float al = std::min(1.0f, s.social / 100.0f);
            draw_list->AddLine(from, pos[it->second], IM_COL32(60, 220, 220, (int)(45 + al * 110)),
                               1.0f + al * 2.0f); ++lines;
        }
    };

    for (int i = 0; i < n; ++i) drawFor(i);

    // ── Entity dots: filled by happiness, ringed by tribe color ──────────────
    for (int i = 0; i < n; ++i) {
        Entity* entity = entities[i];
        ImVec2  p      = pos[i];
        bool isSick    = (entity->entityDiseaseType != -1);

        float size = pointSize;
        ImU32 fill;
        if (entity->selected)      { fill = IM_COL32(255, 100, 100, 255); size = pointSize + 2.5f; }
        else if (isSick)           fill = IM_COL32(170, 220, 60, 255);
        else {
            float h = std::max(0.0f, std::min(1.0f, entity->entityHapiness / 100.0f));
            fill = IM_COL32((int)(150 + h * 70), (int)(150 + h * 25), (int)(205 - h * 70), 255);
        }

        draw_list->AddCircleFilled(p, size, fill);
        // Tribe-colored ring around each dot.
        draw_list->AddCircle(p, size + 1.5f, tribeColor(entity->tribeId, 230), 0, 1.6f);

        if (entity->selected) {
            draw_list->AddText(ImVec2(p.x + size + 3.0f, p.y - 7.0f),
                               IM_COL32(255, 200, 200, 255), entity->name.c_str());
        }
    }
}

// HandlePointMovement — click on a node in the social graph to select it.
int UI::HandlePointMovement(std::vector<Entity*>& entities) {
    ImVec2 mousePos  = ImGui::GetIO().MousePos;
    bool mouseClicked = ImGui::IsMouseClicked(0);
    static int selectedIndex = -1;

    // Right-mouse drag pans the whole social graph on X and Y, so you can scroll
    // across all the tribes. Only pans when not hovering an ImGui window/widget.
    if (!ImGui::GetIO().WantCaptureMouse && ImGui::IsMouseDragging(1, 0.0f)) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        g_graphPan.x += d.x;
        g_graphPan.y += d.y;
    }

    if (mouseClicked) {
        int n = (int)entities.size();
        selectedIndex = -1;
        std::vector<ImVec2> pos;
        computeSocialLayout(entities, pos);
        for (auto& p : pos) { p.x += g_graphPan.x; p.y += g_graphPan.y; }
        for (int i = 0; i < n; ++i) {
            float dx = mousePos.x - pos[i].x;
            float dy = mousePos.y - pos[i].y;
            if (dx * dx + dy * dy < 144.0f) {  // ~12px pick radius
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
            else if (ev.category == "birth")     col = ImVec4(0.55f,0.95f,0.55f, 1.0f);
            else if (ev.category == "death")     col = ImVec4(0.7f, 0.7f, 0.72f, 1.0f);
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
