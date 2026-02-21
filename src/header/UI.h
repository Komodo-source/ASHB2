#ifndef UI_H
#define UI_H

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <string>
#include <vector>
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

    void ShowEntityWindow(Entity* entity, bool* p_open = nullptr);
    void DrawGrid(std::vector<Entity*>& entities, float pointSize = 8.0f);
    int HandlePointMovement(std::vector<Entity*>& entities);
    void createPlayer(int& health, float& attackPower, char* playerName, char* message, std::string& displayText);
    void showSimulationInformation(int day, int num_entity, int tick, std::map<std::string, int> complementary_information);
    bool isSimulationPaused() const { return simulationPaused; }
    GridPoint getGridPoint();
    // Returns: 0=nothing, 1=save pressed, 2=load pressed
    // filename is set to the user-entered filename
    int showSaveLoadButtons(std::string& filename);
private:
    char saveLoadFilename[256] = "savegame.txt";
    bool simulationPaused = false;
    GridPoint gridPoint;
};

#endif // UI_H
