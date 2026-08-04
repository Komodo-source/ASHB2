#include "items/ItemSystem.h"      // Step 5b: emergence telemetry
#include "world/PheromoneField.h"
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
#include "./header/TechTree.h"
#include "./header/QISystem.h"
#include "./header/Kinship.h"
#include "world/ResourceSystem.h"
#include "world/Ecosystem.h"
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
#include <cmath>
#include "./header/BetterRand.h"
#include "./header/LiveConfig.h"
#include "./header/SaveLoad.h"

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

    if (ImGui::Button(simulationPaused ? "Resume Simulation" : "Stop Simulation") ) {
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
    ImGui::SameLine();
    if (ImGui::Button("Auto Backup")) {
        // Generate timestamped filename
        std::string backupName = autobackupFilename();
        strncpy(saveLoadFilename, backupName.c_str(), sizeof(saveLoadFilename) - 1);
        saveLoadFilename[sizeof(saveLoadFilename) - 1] = '\0';
        result = 1;  // trigger save
    }

    ImGui::Separator();
    ImGui::Text("Saves folder:");
    ImGui::BeginChild("FileBrowser", ImVec2(0, 150), true);
    try {
        if (ensureSavesDir()) {
            std::string savesDir = std::string(SAVES_DIR);
            for (const auto& entry : std::filesystem::directory_iterator(savesDir)) {
                if (entry.is_regular_file()) {
                    std::string name = entry.path().filename().string();
                    if (ImGui::Selectable(name.c_str())) {
                        strncpy(saveLoadFilename, name.c_str(), sizeof(saveLoadFilename) - 1);
                        saveLoadFilename[sizeof(saveLoadFilename) - 1] = '\0';
                    }
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
            if (!entity->specialization.empty() && entity->specialization != "farmer")
                role += " (latent " + entity->specialization + ")";
            ImGui::TextColored(ImVec4(0.7f, 0.85f, 0.7f, 1.0f), "Role: %s", role.c_str());
        }
        if (g_resources.valid(entity->originRegionId)) {
            int rid = entity->originRegionId;
            ImGui::Text("Homeland: food x%.2f  water x%.2f  wood x%.2f  | quality %.0f%%",
                        g_resources.abundance(rid, RES_FOOD),
                        g_resources.abundance(rid, RES_WATER),
                        g_resources.abundance(rid, RES_WOOD),
                        g_resources.settlementQuality(rid) * 100.0f);
            if (g_ecosystem.valid(rid)) {
                const RegionEcology& eco = g_ecosystem.regions[rid];
                ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f),
                    "Wildlife: %s  (plants %.0f, game %.0f, predators %.0f)",
                    g_ecosystem.healthLabel(rid), eco.plants, eco.herbivores, eco.predators);
            }
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
        ImGui::Text(" === Mind ===");
        ImGui::Text("QI: %.0f / potential %.0f", entity->qi, entity->qiPotential);
        ImGui::Text("school-years: %.1f%s", entity->schoolYears,
                    entity->isStudent ? "  (enrolled)" : "");
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
            entity->flushEntityStats();   // stats are buffered in memory (M4 perf)
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
            try
            {
                std::vector<std::string> labels = {
                "Anti Body", "Boredom", "Anger", "Happiness",
                "Health", "Hygiene", "Loneliness", "Mental Health", "Stress"
            };

            std::vector<std::vector<float>> plot_data(labels.size());
            std::vector<float> x_axis;

            entity->flushEntityStats();   // stats are buffered in memory (M4 perf)
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

// Shared pan/zoom state for the social graph.
//   * Right-mouse-drag scrolls the whole network on X/Y so clusters that sit
//     off-screen can be brought into view.
//   * Mouse-wheel zooms in/out, anchored on the cursor so the point under the
//     mouse stays put while you scale.
// Every drawn or hit-tested node position goes through viewTransform() below,
// so DrawGrid (what you see) and HandlePointMovement (what you click) always
// agree, whatever the current pan/zoom.
static ImVec2 g_graphPan(0.0f, 0.0f);
static float  g_graphZoom = 1.0f;

// The layout is built around this pivot (matches cx,cy in computeSocialLayout).
// Zoom scales distances from the pivot, so zooming keeps the world centered.
static const ImVec2 g_graphPivot(760.0f, 410.0f);

// Map a raw layout position to its on-screen position for the current view.
static inline ImVec2 viewTransform(const ImVec2& p) {
    return ImVec2(g_graphPivot.x + (p.x - g_graphPivot.x) * g_graphZoom + g_graphPan.x,
                  g_graphPivot.y + (p.y - g_graphPivot.y) * g_graphZoom + g_graphPan.y);
}

// Mouse-wheel zoom, kept anchored under the cursor. Call once per frame before
// laying out the graph. Only zooms when the pointer isn't over an ImGui window.
static void updateGraphZoom() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    float wheel = io.MouseWheel;
    if (wheel == 0.0f) return;

    float oldZoom = g_graphZoom;
    float newZoom = oldZoom * std::pow(1.12f, wheel);   // smooth exponential steps
    newZoom = std::max(0.25f, std::min(6.0f, newZoom));  // clamp: 4x out .. 6x in
    if (newZoom == oldZoom) return;

    // Keep the layout point currently under the cursor fixed on screen.
    ImVec2 m = io.MousePos;
    g_graphPan.x = m.x - g_graphPivot.x - (m.x - g_graphPivot.x - g_graphPan.x) * (newZoom / oldZoom);
    g_graphPan.y = m.y - g_graphPivot.y - (m.y - g_graphPivot.y - g_graphPan.y) * (newZoom / oldZoom);
    g_graphZoom = newZoom;
}

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
    // Spread the tribes wide so clusters don't crowd into a hairball. The big
    // ring grows with the number of tribes so neighbours keep their distance
    // even with many tribes; zoom/pan bring any of them back into view.
    float bigR = (C <= 1) ? 0.0f : std::min(cx, cy) * (1.35f + 0.05f * C);
    for (int k = 0; k < C; ++k) {
        float ca = (C <= 1) ? 0.0f : (2.0f * PI * k) / C - PI / 2.0f;
        ImVec2 center(cx + bigR * std::cos(ca), cy + bigR * std::sin(ca));
        if (clusterCenters) clusterCenters->push_back(center);
        if (clusterTribe)   clusterTribe->push_back(tribeOrder[k]);

        int m = (int)clusters[k].size();
        // Wider sub-rings so individuals within a tribe are easy to tell apart.
        float sr = std::min(340.0f, 48.0f + m * 4.2f);
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

    // Apply the user's pan + zoom to everything we draw.
    for (auto& p : pos)            p = viewTransform(p);
    for (auto& c : clusterCenters) c = viewTransform(c);

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

    // Dots scale with zoom, but stay within a legible range so they never
    // vanish when zoomed far out or swamp the screen when zoomed all the way in.
    float basePoint = std::max(3.0f, std::min(pointSize * g_graphZoom, pointSize * 2.5f));

    // ── Entity dots: filled by happiness, ringed by tribe color ──────────────
    for (int i = 0; i < n; ++i) {
        Entity* entity = entities[i];
        ImVec2  p      = pos[i];
        bool isSick    = (entity->entityDiseaseType != -1);

        float size = basePoint;
        ImU32 fill;
        if (entity->selected)      { fill = IM_COL32(255, 100, 100, 255); size = basePoint + 2.5f; }
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

    // Mouse-wheel zoom (anchored under the cursor) is handled here, once a frame.
    updateGraphZoom();

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
        for (auto& p : pos) p = viewTransform(p);
        // Pick radius follows the zoom so dots stay just as easy to click.
        float pickR = 12.0f * std::max(0.6f, std::min(g_graphZoom, 2.5f));
        float pickR2 = pickR * pickR;
        for (int i = 0; i < n; ++i) {
            float dx = mousePos.x - pos[i].x;
            float dy = mousePos.y - pos[i].y;
            if (dx * dx + dy * dy < pickR2) {
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
void UI::ShowCivilizationPanel(int simDay, std::vector<Entity*>& entities) {
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

            // Role breakdown: who does what in this tribe
            {
                int farmers = 0, craftsmen = 0, traders = 0, scholars = 0,
                    healers = 0, warriors = 0, priests = 0;
                for (Entity* e : entities) {
                    if (!e || e->tribeId != tribe.id || e->entityHealth <= 0.0f) continue;
                    const std::string& r = e->specialization;
                    if      (r == "craftsman") craftsmen++;
                    else if (r == "trader")    traders++;
                    else if (r == "scholar")   scholars++;
                    else if (r == "healer")    healers++;
                    else if (r == "warrior")   warriors++;
                    else if (r == "priest")    priests++;
                    else                       farmers++;
                }
                ImGui::TextDisabled("    Roles: %d farm | %d craft | %d trade | %d schol | %d heal | %d war | %d priest",
                                    farmers, craftsmen, traders, scholars, healers, warriors, priests);
            }

            // Governance: who rules, how, with whose counsel — and how honestly.
            {
                auto byId = [&](int id) -> Entity* {
                    if (id < 0) return nullptr;
                    for (Entity* e : entities)
                        if (e && e->entityId == id && e->entityHealth > 0.0f) return e;
                    return nullptr;
                };
                Entity* leader = byId(tribe.leaderId);
                ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.55f, 1.0f),
                    "    %s under %s (integrity %.0f)",
                    governmentName(tribe.government),
                    leader ? leader->name.c_str() : "no one",
                    leader ? leader->integrity : 0.0f);
                std::string council;
                for (int cid : tribe.councilIds) {
                    Entity* c = byId(cid);
                    if (!c) continue;
                    if (!council.empty()) council += ", ";
                    council += c->name;
                }
                if (!council.empty())
                    ImGui::TextDisabled("    Council: %s", council.c_str());
                std::string ballot;
                if (tribe.government == GOV_DEMOCRACY && tribe.nextElectionDay >= 0)
                    ballot = " | election in "
                           + std::to_string(std::max(0, tribe.nextElectionDay - simDay)) + "d";
                ImGui::TextDisabled("    Treasury %.0f | tax %.0f%%%s",
                    tribe.economy.token, tribe.taxeRate * 100.0f, ballot.c_str());
                ImGui::Text("    Sat"); ImGui::SameLine(80);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f,0.8f,0.4f,0.8f));
                char idS[32]; snprintf(idS,32,"##sat%d",tribe.id);
                ImGui::ProgressBar(tribe.govSatisfaction/100.0f, ImVec2(60,8), idS);
                ImGui::PopStyleColor();
                ImGui::SameLine(160); ImGui::Text("Corr"); ImGui::SameLine(190);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f,0.2f,0.2f,0.8f));
                char idC[32]; snprintf(idC,32,"##cor%d",tribe.id);
                ImGui::ProgressBar(tribe.corruption/100.0f, ImVec2(60,8), idC);
                ImGui::PopStyleColor();
            }
            if(!tribe.buildings_owned.empty()){
                std::string buildings_list = "Buildings: ";
                for(buildingStructure b: tribe.buildings_owned){
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.55f, 1.0f, 1.0f));
                    buildings_list += b.name + " | ";
                    ImGui::PopStyleColor();
                }
                ImGui::Text(buildings_list.c_str());
            }else{
                ImGui::Text("No buildings");
            }

            // QI: what this people can think with, and what it buys them.
            ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.45f, 1.0f), "    %s",
                               QISystem::summary(tribe).c_str());
            if (tribe.schoolQuality > 0.0f)
                ImGui::TextDisabled("    school quality %.0f%% | research x%.2f  war x%.2f  growth x%.2f",
                                    tribe.schoolQuality * 100.0f,
                                    QISystem::researchMul(tribe),
                                    QISystem::warMul(tribe),
                                    QISystem::growthMul(tribe));

            // Known tech count (emergent innovations)
            if (!tribe.knownTechIds.empty())
                ImGui::TextDisabled("    %d innovations known", (int)tribe.knownTechIds.size());

            // Structured tech tree: unlocked nodes, bonuses, and next goal.
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.95f, 1.0f),
                               "    Tech tree: %s", TechTreeSystem::summary(tribe).c_str());

            // Active treaties this tribe is party to.
            for (const Treaty& tr : civ.treaties) {
                if (!tr.active || !tr.involves(tribe.id)) continue;
                int otherId = (tr.tribeA == tribe.id) ? tr.tribeB : tr.tribeA;
                const Tribe* other = nullptr;
                for (const auto& ot : civ.tribes) if (ot.id == otherId) { other = &ot; break; }
                if (!other) continue;
                const char* dir = (tr.type == TREATY_TRIBUTE)
                                ? (tr.tribeA == tribe.id ? " receives from " : " pays ")
                                : " with ";
                ImGui::TextColored(ImVec4(0.85f, 0.8f, 0.5f, 1.0f),
                    "    %s%s%s", treatyTypeName(tr.type), dir, other->name.c_str());
            }
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

    if (ImGui::CollapsingHeader("ATK MILITARY MARKET", ImGuiTreeNodeFlags_DefaultOpen))
        DrawMarketRows(GoodCategory::ATK_OBJECT);

    if (ImGui::CollapsingHeader("DEF MILITARY MARKET", ImGuiTreeNodeFlags_DefaultOpen))
        DrawMarketRows(GoodCategory::DEF_OBJECT);

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

        // Role + tenure + integrity
        ImGui::TextDisabled("Role: %s (since day %d) | Int %.0f",
                            ent->specialization.empty() ? "farmer" : ent->specialization.c_str(),
                            ent->roleSinceDay, ent->integrity);

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

// ── M10: God Console ──────────────────────────────────────────────────────────
// Divine interventions for experimentation and storytelling. Each act writes
// itself into the civilization event log so the Chronicle remembers exactly
// when the world stopped being fair.
void UI::ShowGodConsole(std::vector<Entity*>& entities, Entity* selected, int simDay) {
    ImGui::Begin("God Console");
    ImGui::TextDisabled("Interventions echo through the event log.");
    ImGui::Separator();

    // ── Acts upon one soul ───────────────────────────────────────────────────
    ImGui::Text(selected ? "Chosen one: %s" : "Chosen one: (select an entity)",
                selected ? selected->name.c_str() : "");
    ImGui::BeginDisabled(selected == nullptr);
    if (ImGui::Button("Smite") && selected) {
        selected->entityHealth = 0.0f;
        selected->pendingDeathCause = "divine wrath";
        if (globalCivEngine) globalCivEngine->logEvent(simDay,
            "The heavens struck down " + selected->name, "god");
    }
    ImGui::SameLine();
    if (ImGui::Button("Bless") && selected) {
        selected->entityHealth = 100.0f;
        selected->entityStress = 0.0f;
        selected->entityMentalHealth = 100.0f;
        if (globalCivEngine) globalCivEngine->logEvent(simDay,
            selected->name + " was touched by grace", "god");
    }
    ImGui::SameLine();
    if (ImGui::Button("Torment") && selected) {
        selected->entityStress = 100.0f;
        selected->entityMentalHealth = std::max(0.0f, selected->entityMentalHealth - 40.0f);
        if (globalCivEngine) globalCivEngine->logEvent(simDay,
            "Dark visions haunt " + selected->name, "god");
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    // ── Acts upon the world ──────────────────────────────────────────────────
    if (ImGui::Button("Feast (all fed)")) {
        for (Entity* e : entities) {
            if (!e || e->entityHealth <= 0.0f) continue;
            e->foodStore = std::min(20.0f, e->foodStore + 10.0f);
            e->entityHunger = std::max(0.0f, e->entityHunger - 25.0f);
        }
        if (globalCivEngine) globalCivEngine->logEvent(simDay,
            "A miraculous harvest feeds every mouth", "god");
    }
    ImGui::SameLine();
    if (ImGui::Button("Famine (larders emptied)")) {
        for (Entity* e : entities) {
            if (!e || e->entityHealth <= 0.0f) continue;
            e->foodStore = 0.0f;
            e->entityHunger = std::min(100.0f, e->entityHunger + 30.0f);
        }
        if (globalCivEngine) globalCivEngine->logEvent(simDay,
            "The granaries turn to dust — famine grips the land", "god");
    }
    if (ImGui::Button("Meteor (random strike)")) {
        if (!entities.empty()) {
            int idx = BetterRand::genNrInInterval(0, (int)entities.size() - 1);
            float cx = entities[idx]->posX, cy = entities[idx]->posY;
            int slain = 0;
            for (Entity* e : entities) {
                if (!e || e->entityHealth <= 0.0f) continue;
                float dx = e->posX - cx, dy = e->posY - cy;
                if (dx * dx + dy * dy < 120.0f * 120.0f) {
                    e->entityHealth = 0.0f;
                    e->pendingDeathCause = "meteor strike";
                    ++slain;
                }
            }
            if (globalCivEngine) globalCivEngine->logEvent(simDay,
                "A star fell from the sky, claiming " + std::to_string(slain) + " lives", "god");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Great Calm (soothe all)")) {
        for (Entity* e : entities) {
            if (!e || e->entityHealth <= 0.0f) continue;
            e->entityStress = std::max(0.0f, e->entityStress - 40.0f);
            e->entityGeneralAnger = std::max(0.0f, e->entityGeneralAnger - 40.0f);
            for (auto& a : e->list_entityPointedAnger) a.anger *= 0.4f;
        }
        if (globalCivEngine) globalCivEngine->logEvent(simDay,
            "An unnatural peace settles over every heart", "god");
    }

    ImGui::End();
}

// ── M10: Possess mode ─────────────────────────────────────────────────────────
void UI::ShowPossessWindow(Entity* selected, int simDay) {
    ImGui::Begin("Possess");

    const bool anyonePossessed = FreeWillSystem::possessedEntityId != -1;
    if (anyonePossessed) {
        ImGui::Text("Possessing entity #%d", FreeWillSystem::possessedEntityId);
        static std::vector<std::string> names = FreeWillSystem::actionNames();
        int& cmd = FreeWillSystem::possessedActionIdx;
        ImGui::TextDisabled(cmd >= 0 && cmd < (int)names.size()
                                ? "Command: they will keep doing this until told otherwise"
                                : "No command queued: acting on free will");
        const char* current = (cmd >= 0 && cmd < (int)names.size())
                                  ? names[cmd].c_str() : "(choose an action)";
        if (ImGui::BeginCombo("##possessAction", current)) {
            for (int i = 0; i < (int)names.size(); ++i) {
                if (ImGui::Selectable(names[i].c_str(), i == cmd)) {
                    cmd = i;
                    if (globalCivEngine) globalCivEngine->logEvent(simDay,
                        "An unseen will commands entity #" +
                        std::to_string(FreeWillSystem::possessedEntityId) +
                        " to " + names[i], "god");
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Free will (keep possession)")) cmd = -1;
        ImGui::SameLine();
        if (ImGui::Button("Release")) {
            FreeWillSystem::possessedEntityId = -1;
            FreeWillSystem::possessedActionIdx = -1;
        }
    } else {
        ImGui::Text(selected ? "Target: %s" : "Target: (select an entity)",
                    selected ? selected->name.c_str() : "");
        ImGui::BeginDisabled(selected == nullptr);
        if (ImGui::Button("Possess selected") && selected) {
            FreeWillSystem::possessedEntityId = selected->entityId;
            FreeWillSystem::possessedActionIdx = -1;
            if (globalCivEngine) globalCivEngine->logEvent(simDay,
                "Something else looks out from behind " + selected->name + "'s eyes", "god");
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("Commands override reflex, habit and deliberation.");
    }

    ImGui::End();
}

// ── M10: Interview mode ───────────────────────────────────────────────────────
// Templated Q&A: every answer is assembled from the entity's real state, so the
// player can cross-examine a mind rather than read raw stat bars.
void UI::ShowInterviewWindow(Entity* selected, std::vector<Entity*>& entities, int simDay) {
    ImGui::Begin("Interview");
    if (!selected) {
        ImGui::TextDisabled("Select an entity to interview.");
        ImGui::End();
        return;
    }

    auto nameOf = [&](int id) -> std::string {
        for (Entity* e : entities)
            if (e && e->entityId == id) return e->name;
        return "someone long gone";
    };
    auto feelWord = [](float v, const char* low, const char* mid, const char* high) {
        return v < 33.0f ? low : (v < 66.0f ? mid : high);
    };

    ImGui::Text("Interviewing %s (age %.0f)", selected->name.c_str(), selected->entityAge);
    ImGui::Separator();

    static int question = 0;
    const char* questions[] = {
        "How are you feeling?",
        "Who matters to you?",
        "What do you believe?",
        "What do you remember?",
        "What do you want from life?",
        "What do you think of the others?",
        "Who are you, and what have you lived through?",
    };
    for (int i = 0; i < (int)(sizeof(questions) / sizeof(questions[0])); ++i) {
        if (ImGui::RadioButton(questions[i], question == i)) question = i;
    }
    ImGui::Separator();

    std::string ans;
    switch (question) {
    case 0: { // feelings
        ans = "\"I feel " +
              std::string(feelWord(selected->entityHapiness, "hollow", "alright, I suppose", "genuinely happy")) + ". ";
        if (selected->entityStress > 60.0f)  ans += "The pressure never lets up. ";
        if (selected->entityGeneralAnger > 50.0f) ans += "There is an anger in me I can barely hold down. ";
        if (selected->entityLoneliness > 60.0f)   ans += "Mostly, I am alone. ";
        if (selected->entityHunger > 60.0f)  ans += "And I am hungry — we all are, lately. ";
        if (selected->entityHealth < 40.0f)  ans += "My body is failing me. ";
        if (selected->entityDiseaseType != -1) ans += "This sickness... I try not to think about it. ";
        ans += "\"";
        if (!selected->innerMonologue.empty())
            ans += "\n\n(under their breath) \"" + selected->innerMonologue + "\"";
        break;
    }
    case 1: { // relationships
        ans = "\"";
        if (!selected->list_entityPointedCouple.empty()) {
            const auto& c = selected->list_entityPointedCouple[0];
            if (c.pointedEntity) {
                ans += c.pointedEntity->name + " — " +
                       std::to_string(c.daysTogether) + " days together. " +
                       (c.satisfaction > 60.0f ? "They are my whole world. "
                        : c.satisfaction > 35.0f ? "We manage, like anyone. "
                                                 : "It has grown... difficult between us. ");
                if (c.suspicion > 40.0f) ans += "Though lately I wonder where they go. ";
            }
        } else {
            ans += "I have no partner. ";
        }
        auto socials = selected->list_entityPointedSocial;
        std::sort(socials.begin(), socials.end(),
                  [](const entityPointedSocial& a, const entityPointedSocial& b) {
                      return a.social > b.social;
                  });
        int listed = 0;
        for (const auto& s : socials) {
            if (!s.pointedEntity || listed >= 3) break;
            ans += (listed == 0 ? "I hold close " : ", and ") + s.pointedEntity->name;
            ++listed;
        }
        if (listed > 0) ans += ". ";
        if (!selected->list_entityPointedAnger.empty() &&
            selected->list_entityPointedAnger[0].pointedEntity)
            ans += "And " + selected->list_entityPointedAnger[0].pointedEntity->name +
                   "... them, I have not forgiven.";
        ans += "\"";
        break;
    }
    case 2: { // beliefs
        auto beliefs = selected->coreBeliefs;
        std::sort(beliefs.begin(), beliefs.end(),
                  [](const CoreBelief& a, const CoreBelief& b) { return a.strength > b.strength; });
        if (beliefs.empty()) {
            ans = "\"I am still working out what I believe. The world has not settled it for me yet.\"";
        } else {
            ans = "\"";
            int listed = 0;
            for (const auto& b : beliefs) {
                if (listed >= 4) break;
                ans += b.belief + (b.strength > 75.0f ? " — of this I am certain. " : ". ");
                ++listed;
            }
            ans += "\"";
        }
        break;
    }
    case 3: { // memories
        auto mems = selected->lifeMemories;
        std::sort(mems.begin(), mems.end(),
                  [](const LifeMemory& a, const LifeMemory& b) {
                      return a.emotionalIntensity > b.emotionalIntensity;
                  });
        if (mems.empty()) {
            ans = "\"Nothing has marked me yet. My story is still unwritten.\"";
        } else {
            ans = "";
            int listed = 0;
            for (const auto& m : mems) {
                if (listed >= 3) break;
                ans += "\"" + (m.internalNarrative.empty()
                                   ? ("I remember the " + m.eventType +
                                      (m.entityInvolvedId >= 0 ? " with " + nameOf(m.entityInvolvedId) : ""))
                                   : m.internalNarrative) + "\"";
                ans += " (day " + std::to_string(m.simulationDay) +
                       (m.isFormative ? ", it changed me)\n\n" : ")\n\n");
                ++listed;
            }
        }
        break;
    }
    case 4: { // goals
        auto goals = selected->m_goals;
        std::sort(goals.begin(), goals.end(),
                  [](const LifeGoal& a, const LifeGoal& b) { return a.priority > b.priority; });
        if (goals.empty()) {
            ans = "\"To survive the day. Ask me again when the winters are kinder.\"";
        } else {
            ans = "\"";
            for (size_t i = 0; i < goals.size() && i < 3; ++i) {
                const auto& g = goals[i];
                std::string want =
                    g.type == "find_partner" ? "to find someone to share this life with"
                    : g.type == "build_family" ? "to raise a family"
                    : g.type == "build_career" ? "to be good at what I do"
                    : g.type == "make_friends" ? "to be surrounded by people I trust"
                    : g.type == "happiness"    ? "to be content"
                                               : "to understand myself";
                ans += (i == 0 ? "Above all I want " : "And I want ") + want +
                       " (" + std::to_string((int)g.progressToward) + "% of the way there";
                ans += g.frustrationLevel > 50.0f ? ", though it keeps slipping away). " : "). ";
            }
            ans += "\"";
        }
        break;
    }
    case 5: { // mental models of others
        auto models = selected->list_MentalModelOfOther;
        std::sort(models.begin(), models.end(),
                  [](const MentalModelOfOther* a, const MentalModelOfOther* b) {
                      return a && b && std::fabs(a->trustLevel - 50.0f) > std::fabs(b->trustLevel - 50.0f);
                  });
        ans = "";
        int listed = 0;
        for (const auto* m : models) {
            if (!m || !m->entityPointed || listed >= 4) continue;
            float conf = m->effectiveConfidence(simDay);
            ans += "\"" + m->entityPointed->name + "? " +
                   std::string(m->trustLevel > 65.0f ? "I would trust them with my life"
                               : m->trustLevel > 40.0f ? "They seem fair enough"
                                                        : "I keep my distance from them") +
                   std::string(conf < 0.25f ? " — though it has been a long time since we spoke." : ".") +
                   "\"\n";
            ++listed;
        }
        if (listed == 0)
            ans = "\"Truthfully, I do not know anyone well enough to judge.\"";
        break;
    }
    case 6: {
        // §8: the life read back as a life. Everything here is a real field —
        // the identity I-P1 distilled from what this person values and
        // remembers, the purpose that buffers their despair, the chapters of
        // their story in the order they happened, how deep in their line they
        // stand (I-P3), and the ways they keep (IV-P1). This is the acceptance
        // bar "you can open any agent and read a coherent life", made openable.
        const auto& id = selected->narrativeIdentity;
        ans = "\"I am a " + id.selfStory + "";
        if (!id.dominantValue.empty()) ans += ", and what I hold to is " + id.dominantValue;
        ans += ".\" ";
        ans += std::string(id.coherence > 66.0f ? "(They say it without hesitating.) "
                           : id.coherence > 33.0f ? "(They say it as though still deciding.) "
                                                  : "(They do not sound sure of it.) ");
        ans += "\n\n\"";
        ans += selected->senseOfPurpose > 66.0f
                   ? "My days mean something. I know what they are for."
               : selected->senseOfPurpose > 33.0f
                   ? "Some days feel like they are for something. Others, less."
                   : "I do not know what any of it is for.";
        ans += "\"\n";
        if (selected->lineageDepth > 1)
            ans += "\n(The " + std::to_string(selected->lineageDepth)
                 + std::string(selected->lineageDepth == 2 ? "nd" :
                               selected->lineageDepth == 3 ? "rd" : "th")
                 + " generation of their house.)\n";
        if (!selected->lifeChapters.empty()) {
            ans += "\nTheir story so far:\n";
            for (const LifeChapter& c : selected->lifeChapters)
                ans += "  · day " + std::to_string(c.day) + " — "
                     + (c.note.empty() ? c.title : c.note)
                     + (c.otherId >= 0 ? " (" + nameOf(c.otherId) + ")" : "") + "\n";
        }
        if (globalCivEngine && selected->cultureTraits != 0ull) {
            std::string ways;
            unsigned long long set = selected->cultureTraits;
            int shown = 0;
            while (set && shown < 8) {
                unsigned long long low = set & (~set + 1ull);
                int tid = 0;
                while ((low >> tid) != 1ull) ++tid;
                ways += (ways.empty() ? "" : ", ")
                      + globalCivEngine->culture.trait(tid).name;
                set &= set - 1ull;
                ++shown;
            }
            if (set) ways += ", …";
            ans += "\nThe ways they keep: " + ways + "\n";
        }
        break;
    }
    }

    ImGui::TextWrapped("%s", ans.c_str());
    ImGui::End();
}

// ── M10: Live config console ──────────────────────────────────────────────────
void UI::ShowConfigConsole() {
    ImGui::Begin("Config Console");
    ImGui::TextDisabled("World tunables, applied live. 1.0 = untouched physics.");
    ImGui::TextDisabled("(Determinism holds only while these stay at 1.0.)");
    ImGui::Separator();
    ImGui::SliderFloat("Movement force", &g_liveConfig.moveForceMul, 0.0f, 3.0f, "%.2fx");
    ImGui::SliderFloat("Old-age mortality", &g_liveConfig.mortalityMul, 0.0f, 4.0f, "%.2fx");
    ImGui::SliderFloat("Food yield", &g_liveConfig.foodYieldMul, 0.1f, 3.0f, "%.2fx");
    ImGui::SliderFloat("Aggression", &g_liveConfig.aggressionMul, 0.0f, 4.0f, "%.2fx");
    ImGui::SliderFloat("Corruption", &g_liveConfig.corruptionMul, 0.0f, 4.0f, "%.2fx");
    ImGui::Separator();
    ImGui::TextDisabled("Emergence (Steps 2-5)");
    ImGui::SliderFloat("Mutation rate",   &g_liveConfig.mutationRateMul,   0.0f, 4.0f, "%.2fx");
    ImGui::SliderFloat("Pheromone decay", &g_liveConfig.pheromoneDecayMul, 0.1f, 4.0f, "%.2fx");
    ImGui::SliderFloat("Invention rate",  &g_liveConfig.inventionRateMul,  0.0f, 4.0f, "%.2fx");
    ImGui::SliderFloat("NEAT newborn share", &g_liveConfig.neatBrainShare, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Density heatmap",  &g_liveConfig.showDensityHeatmap); ImGui::SameLine();
    ImGui::Checkbox("Pheromones",       &g_liveConfig.showPheromones);     ImGui::SameLine();
    ImGui::Checkbox("Genetic tint",     &g_liveConfig.showGeneticTint);
    if (ImGui::Button("Reset all to 1.0")) g_liveConfig = LiveConfig{};
    ImGui::End();
}


// ── Emergence panel (Upgrade Plan, Step 5b) ───────────────────────────────────
// Live telemetry: population, mean age, genetic diversity, invented content.
// Below the plots, an overlay map of the WORLD (not the social layout): agent
// density heatmap, the three pheromone channels, and agents tinted by genome.
void UI::ShowEmergencePanel(std::vector<Entity*>& entities, int simDay) {
    ImGui::Begin("Emergence");

    // ── Sample telemetry once per sim-day ────────────────────────────────────
    static std::vector<float> tDay, tPop, tAge, tDiv, tDefs, tRules, tSpreads, tNeat;
    static int lastDay = -1;
    if (simDay != lastDay) {
        lastDay = simDay;
        int alive = 0, neatN = 0;
        float ageSum = 0.0f;
        Genome mean; mean.speed = mean.sightRange = mean.metabolism = mean.fertility = mean.resilience = 0.0f;
        for (Entity* e : entities) {
            if (!e || e->entityHealth <= 0.0f) continue;
            ++alive; ageSum += e->entityAge;
            if (e->useNeatBrain) ++neatN;
            mean.speed += e->genome.speed;           mean.sightRange += e->genome.sightRange;
            mean.metabolism += e->genome.metabolism; mean.fertility  += e->genome.fertility;
            mean.resilience += e->genome.resilience;
        }
        float div = 0.0f;
        if (alive > 0) {
            mean.speed /= alive; mean.sightRange /= alive; mean.metabolism /= alive;
            mean.fertility /= alive; mean.resilience /= alive;
            for (Entity* e : entities)
                if (e && e->entityHealth > 0.0f) div += e->genome.distanceTo(mean);
            div /= alive;
        }
        tDay.push_back((float)simDay);
        tPop.push_back((float)alive);
        tAge.push_back(alive ? ageSum / alive : 0.0f);
        tDiv.push_back(div);
        tDefs.push_back((float)g_itemManager.inventedDefCount());
        tRules.push_back((float)g_itemManager.inventedRuleCount());
        tSpreads.push_back((float)g_itemManager.recipeSpreadCount());
        tNeat.push_back((float)neatN);
        if (tDay.size() > 4096) {   // bound memory on very long runs
            for (auto* v : {&tDay,&tPop,&tAge,&tDiv,&tDefs,&tRules,&tSpreads,&tNeat})
                v->erase(v->begin(), v->begin() + 2048);
        }
    }

    if (!tDay.empty() && ImPlot::BeginPlot("Population & Brains", ImVec2(-1, 180))) {
        ImPlot::SetupAxes("day", "count");
        ImPlot::PlotLine("population", tDay.data(), tPop.data(), (int)tDay.size());
        ImPlot::PlotLine("NEAT brains", tDay.data(), tNeat.data(), (int)tDay.size());
        ImPlot::EndPlot();
    }
    if (!tDay.empty() && ImPlot::BeginPlot("Age & Genetic diversity", ImVec2(-1, 180))) {
        ImPlot::SetupAxes("day", "value");
        ImPlot::PlotLine("mean age", tDay.data(), tAge.data(), (int)tDay.size());
        ImPlot::PlotLine("gene diversity x100", tDay.data(), tDiv.data(), (int)tDiv.size());
        ImPlot::EndPlot();
    }
    if (!tDay.empty() && ImPlot::BeginPlot("Invented culture", ImVec2(-1, 180))) {
        ImPlot::SetupAxes("day", "count");
        ImPlot::PlotLine("item defs", tDay.data(), tDefs.data(), (int)tDay.size());
        ImPlot::PlotLine("recipes",   tDay.data(), tRules.data(), (int)tRules.size());
        ImPlot::PlotLine("recipe spreads", tDay.data(), tSpreads.data(), (int)tSpreads.size());
        ImPlot::EndPlot();
    }
    ImGui::Text("Items: %d defs (%d invented) | Rules: %d (%d invented) | Spreads: %d",
                (int)g_itemManager.defs().size(),  g_itemManager.inventedDefCount(),
                (int)g_itemManager.rules().size(), g_itemManager.inventedRuleCount(),
                g_itemManager.recipeSpreadCount());

    // ── World overlay map ─────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("World overlays (toggle in Config Console)");
    const float mapW = ImGui::GetContentRegionAvail().x, mapH = 240.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##emap", ImVec2(std::max(mapW, 50.0f), mapH));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + mapW, origin.y + mapH), IM_COL32(12, 12, 18, 255));

    float worldW = g_pheromoneField.ready()
                 ? g_pheromoneField.cols() * g_pheromoneField.cellSize() : 1280.0f;
    float worldH = g_pheromoneField.ready()
                 ? g_pheromoneField.rows() * g_pheromoneField.cellSize() : 720.0f;
    auto toMap = [&](float wx, float wy) {
        return ImVec2(origin.x + wx / worldW * mapW, origin.y + wy / worldH * mapH);
    };

    if (g_liveConfig.showPheromones && g_pheromoneField.ready()) {
        float cw = mapW / g_pheromoneField.cols(), ch = mapH / g_pheromoneField.rows();
        for (int cy = 0; cy < g_pheromoneField.rows(); ++cy) {
            for (int cx = 0; cx < g_pheromoneField.cols(); ++cx) {
                float fo = g_pheromoneField.cellValue(cx, cy, PheromoneField::FOOD);
                float da = g_pheromoneField.cellValue(cx, cy, PheromoneField::DANGER);
                float so = g_pheromoneField.cellValue(cx, cy, PheromoneField::SOCIAL);
                if (fo + da + so < 1.0f) continue;
                ImVec2 a(origin.x + cx * cw, origin.y + cy * ch);
                ImVec2 b(a.x + cw, a.y + ch);
                int r = (int)std::min(255.0f, da * 2.5f);
                int g = (int)std::min(255.0f, fo * 2.5f);
                int bl = (int)std::min(255.0f, so * 2.5f);
                dl->AddRectFilled(a, b, IM_COL32(r, g, bl, 110));
            }
        }
    }

    if (g_liveConfig.showDensityHeatmap) {
        const int BX = 32, BY = 20;
        static std::vector<int> bins; bins.assign(BX * BY, 0);
        int peak = 1;
        for (Entity* e : entities) {
            if (!e || e->entityHealth <= 0.0f) continue;
            int bx = std::min(BX - 1, std::max(0, (int)(e->posX / worldW * BX)));
            int by = std::min(BY - 1, std::max(0, (int)(e->posY / worldH * BY)));
            peak = std::max(peak, ++bins[by * BX + bx]);
        }
        float cw = mapW / BX, ch = mapH / BY;
        for (int by = 0; by < BY; ++by)
            for (int bx = 0; bx < BX; ++bx) {
                int cnt = bins[by * BX + bx];
                if (!cnt) continue;
                float t = (float)cnt / peak;
                ImVec2 a(origin.x + bx * cw, origin.y + by * ch);
                dl->AddRectFilled(a, ImVec2(a.x + cw, a.y + ch),
                                  IM_COL32(255, (int)(180 * (1 - t)), 40, (int)(30 + 120 * t)));
            }
    }

    // Agent dots — genetic tint maps (speed, sight, metabolism) onto RGB, so
    // families and diverging lineages literally show as color families.
    for (Entity* e : entities) {
        if (!e || e->entityHealth <= 0.0f) continue;
        ImU32 col;
        if (g_liveConfig.showGeneticTint) {
            auto ch = [](float g){ return (int)std::min(255.0f, std::max(0.0f, (g - 0.4f) / 1.4f * 255.0f)); };
            col = IM_COL32(ch(e->genome.speed), ch(e->genome.sightRange), ch(e->genome.metabolism), 220);
        } else {
            col = e->useNeatBrain ? IM_COL32(120, 220, 255, 220) : IM_COL32(200, 200, 200, 160);
        }
        dl->AddCircleFilled(toMap(e->posX, e->posY), 2.0f, col);
    }

    ImGui::End();
}
