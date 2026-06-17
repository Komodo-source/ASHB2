#include "PlanetView.h"
#include "Planet.h"
#include "../header/Entity.h"
#include "../header/CivilizationEngine.h"
#include "../header/WorldSeed.h"
#include "imgui.h"
#include <algorithm>

static ImU32 biomeColor(const Tile& t) {
    if (t.river && t.isLand())            return IM_COL32(60, 120, 220, 255);
    switch (t.biome) {
        case BIOME_OCEAN:     return IM_COL32(28,  60, 120, 255);
        case BIOME_COAST:     return IM_COL32(70, 120, 170, 255);
        case BIOME_ICE:       return IM_COL32(230, 240, 250, 255);
        case BIOME_TUNDRA:    return IM_COL32(150, 165, 160, 255);
        case BIOME_DESERT:    return IM_COL32(215, 195, 120, 255);
        case BIOME_GRASSLAND: return IM_COL32(120, 180,  80, 255);
        case BIOME_FOREST:    return IM_COL32(45, 120,  55, 255);
        case BIOME_JUNGLE:    return IM_COL32(30,  95,  45, 255);
        case BIOME_MOUNTAIN:  return IM_COL32(110, 100,  95, 255);
    }
    return IM_COL32(0, 0, 0, 255);
}

void DrawPlanetWindow(const Planet* planet, std::vector<Entity*>& entities) {
    if (!planet || planet->W == 0) return;

    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("World Map")) { ImGui::End(); return; }

    static bool showRegions = false;
    static bool showEntities = true;
    ImGui::Checkbox("Regions", &showRegions); ImGui::SameLine();
    ImGui::Checkbox("People",  &showEntities); ImGui::SameLine();
    ImGui::Text("habitable regions: %d", planet->habitableRegionCount());

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float cell = std::max(1.0f, std::min(avail.x / planet->W, avail.y / planet->H));
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Tiles
    for (int y = 0; y < planet->H; ++y) {
        for (int x = 0; x < planet->W; ++x) {
            const Tile& t = planet->at(x, y);
            ImVec2 p0(origin.x + x * cell, origin.y + y * cell);
            ImVec2 p1(p0.x + cell + 0.5f, p0.y + cell + 0.5f);
            ImU32 c = biomeColor(t);
            if (showRegions && t.regionId >= 0) {
                // tint by region id so separate landmasses are visually distinct
                int r = (t.regionId * 73)  % 256;
                int g = (t.regionId * 151) % 256;
                int b = (t.regionId * 199) % 256;
                c = IM_COL32((r + 120) % 256, (g + 120) % 256, (b + 120) % 256, 255);
                if (!t.isLand()) c = IM_COL32(20, 40, 80, 255);
            }
            dl->AddRectFilled(p0, p1, c);
        }
    }

    // Entity dots
    if (showEntities) {
        for (Entity* e : entities) {
            if (!e) continue;
            int gx, gy; planet->worldToGrid(e->posX, e->posY, gx, gy);
            ImVec2 p(origin.x + (gx + 0.5f) * cell, origin.y + (gy + 0.5f) * cell);
            ImU32 col = e->selected ? IM_COL32(255, 80, 80, 255)
                                    : IM_COL32(255, 230, 120, 230);
            dl->AddCircleFilled(p, std::max(1.5f, cell * 0.6f), col);
        }
    }

    // reserve the drawn area so the window scrolls/sizes correctly
    ImGui::Dummy(ImVec2(planet->W * cell, planet->H * cell));
    ImGui::End();
}

// Distinct colour per event category so the History log reads at a glance.
static ImVec4 eventCatColor(const std::string& cat) {
    if (cat == "war")        return ImVec4(1.0f, 0.35f, 0.25f, 1.0f);
    if (cat == "religion")   return ImVec4(0.75f,0.5f, 1.0f,  1.0f);
    if (cat == "innovation") return ImVec4(0.3f, 0.9f, 0.7f,  1.0f);
    if (cat == "diplomacy")  return ImVec4(0.3f, 0.8f, 1.0f,  1.0f);
    if (cat == "tribe")      return ImVec4(1.0f, 0.78f,0.3f,  1.0f);
    if (cat == "birth")      return ImVec4(0.55f,0.95f,0.55f, 1.0f);
    if (cat == "death")      return ImVec4(0.7f, 0.7f, 0.72f, 1.0f);
    if (cat == "environment")return ImVec4(0.5f, 0.8f, 0.9f,  1.0f);
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
}

void DrawHistoryWindow() {
    if (!globalCivEngine) return;
    ImGui::SetNextWindowSize(ImVec2(460, 540), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("History & Report")) { ImGui::End(); return; }

    ImGui::Text("Seed: %llu", (unsigned long long)g_worldSeed.master);
    ImGui::SameLine();
    ImGui::Text("  Chaos: %.2f", g_worldSeed.divergence.butterfly);
    ImGui::Text("Run signature: %016llx",
                (unsigned long long)globalCivEngine->historySignature());
    ImGui::Separator();

    // ── BIG SUMMARY: the whole civilisation at a glance ──────────────────────
    if (ImGui::CollapsingHeader("WORLD SUMMARY", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.9f, 0.78f, 1.0f));
        ImGui::TextUnformatted(globalCivEngine->getBigSummary().c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // ── Current wars ─────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("ACTIVE WARS", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool any = false;
        const auto& tribes = globalCivEngine->tribes;
        for (size_t i = 0; i < tribes.size(); ++i) {
            for (size_t j = i + 1; j < tribes.size(); ++j) {
                auto it = tribes[i].stances.find(tribes[j].id);
                if (it == tribes[i].stances.end() || it->second != TS_AT_WAR) continue;
                any = true;
                bool ethnic = tribes[i].ethnicWarWith.count(tribes[j].id) > 0;
                ImGui::TextColored(ethnic ? ImVec4(1.0f,0.3f,0.3f,1.0f) : ImVec4(1.0f,0.55f,0.3f,1.0f),
                                   "  %s  vs  %s%s",
                                   tribes[i].name.c_str(), tribes[j].name.c_str(),
                                   ethnic ? "   [ETHNIC WAR]" : "");
            }
        }
        if (!any) ImGui::TextDisabled("  The world is at peace.");
    }

    ImGui::Separator();

    // ── Social order: classes & clientela ────────────────────────────────────
    if (globalSocialOrder && ImGui::CollapsingHeader("SOCIETY", ImGuiTreeNodeFlags_DefaultOpen)) {
        int sl = globalSocialOrder->cSlaves;
        int pl = globalSocialOrder->cPlebs;
        int pa = globalSocialOrder->cPatricians;
        int tot = std::max(1, sl + pl + pa);
        ImGui::TextColored(ImVec4(1.0f,0.84f,0.4f,1.0f), "  Patricians: %d (%.0f%%)",
                           pa, 100.0f * pa / tot);
        ImGui::TextColored(ImVec4(0.75f,0.85f,0.85f,1.0f), "  Plebeians:  %d (%.0f%%)",
                           pl, 100.0f * pl / tot);
        ImGui::TextColored(ImVec4(0.7f,0.55f,0.5f,1.0f), "  Slaves:     %d (%.0f%%)",
                           sl, 100.0f * sl / tot);
        ImGui::Text("  Patron-client bonds: %d", globalSocialOrder->bondCount());
        ImGui::TextDisabled("  ascended %d  enslaved %d  freed %d",
                            globalSocialOrder->totalAscended,
                            globalSocialOrder->totalEnslaved,
                            globalSocialOrder->totalFreed);
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("POPULATION BY REGION", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& kv : globalCivEngine->regionPopulation) {
            float cap = globalCivEngine->regionCapacity.count(kv.first)
                        ? globalCivEngine->regionCapacity[kv.first] : 0.0f;
            bool famine = cap > 0 && kv.second > cap;
            ImGui::TextColored(famine ? ImVec4(1,0.4f,0.3f,1) : ImVec4(0.7f,0.9f,0.7f,1),
                               "  region %d: %d people (capacity %.0f)%s",
                               kv.first, kv.second, cap, famine ? "  FAMINE" : "");
        }
    }

    ImGui::Separator();
    ImGui::Text("Recent events:");
    ImGui::BeginChild("evt", ImVec2(0, 0), true);
    int shown = 0;
    const auto& log = globalCivEngine->eventLog;
    for (auto it = log.rbegin(); it != log.rend() && shown < 60; ++it, ++shown) {
        ImGui::PushStyleColor(ImGuiCol_Text, eventCatColor(it->category));
        ImGui::TextWrapped("[%d] %s", it->day, it->description.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::End();
}
