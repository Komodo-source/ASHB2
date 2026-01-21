#ifndef UI_H
#define UI_H

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <string>
#include <vector>
#include <map>

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
    void DrawGrid(std::vector<GridPoint>& points, float pointSize = 8.0f);
    int HandlePointMovement(std::vector<GridPoint>& points);
    void createPlayer(int& health, float& attackPower, char* playerName, char* message, std::string& displayText);
    void showSimulationInformation(int day, int num_entity, int tick, std::map<std::string, int> complementary_information);
    GridPoint getGridPoint();
private:
    GridPoint gridPoint;
};

#endif // UI_H
