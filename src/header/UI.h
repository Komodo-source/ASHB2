#ifndef UI_H
#define UI_H

#ifndef HEADLESS
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#endif
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <filesystem>

// Forward declaration of Entity
class Entity;

class UI {
private:
    bool showDetailedInfo = false;
    bool showActionInfo = false;
public:
    struct GridPoint {
        int id;
        ImVec2 pos;
        bool selected = false;
    };

    void ShowEntityWindow(Entity* entity, bool* p_open, std::vector<Entity*> entities);
    void DrawGrid(std::vector<Entity*>& entities, float pointSize = 8.0f);
    int HandlePointMovement(std::vector<Entity*>& entities);
    void createPlayer(int& health, float& attackPower, char* playerName, char* message, std::string& displayText);

    // Social network board replacing the spatial dot grid
    int ShowMindBoard(std::vector<Entity*>& entities);

    // Civilization overview panel
    void ShowCivilizationPanel(int simDay, std::vector<Entity*>& entities);

    // Supply & demand market panel
    void ShowMarketPanel();

    bool isSimulationPaused() const { return simulationPaused; }
    GridPoint getGridPoint();
    // Returns: 0=nothing, 1=save pressed, 2=load pressed
    int showSaveLoadButtons(std::string& filename, int day, int num_entity, int tick, std::map<std::string, int> complementary_information);

    // M10: God Console — divine interventions on the living world. `selected`
    // may be nullptr (entity-targeted buttons grey out). Every act is also
    // written to the event log so history remembers the meddling.
    void ShowGodConsole(std::vector<Entity*>& entities, Entity* selected, int simDay);

    // M10: Possess mode — take direct control of the selected entity: its next
    // deliberations perform the commanded action instead of free will.
    void ShowPossessWindow(Entity* selected, int simDay);

    // M10: Interview mode — templated Q&A with the selected entity, answered
    // from its real state, memories, beliefs and relationships.
    void ShowInterviewWindow(Entity* selected, std::vector<Entity*>& entities, int simDay);

    // M10: Live config console — world tunables (LiveConfig multipliers)
    // adjustable while the simulation runs.
    void ShowConfigConsole();

    // Emergence upgrade (Step 5b): live telemetry (ImPlot) for population,
    // mean age, genetic diversity, items/recipes, plus a world overlay map
    // with density heatmap, pheromone trails, and genetic-similarity tint.
    void ShowEmergencePanel(std::vector<Entity*>& entities, int simDay);
private:
    char saveLoadFilename[256] = "savegame.txt";
    bool simulationPaused = false;
    GridPoint gridPoint;
};

#endif // UI_H
