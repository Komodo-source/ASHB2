#ifndef UI_H
#define UI_H

#include <string>
#include "Entity.h" 

void ShowEntityWindow(Entity* entity, bool* p_open = nullptr);
void createPlayer(int& health, float& attackPower, char* playerName, char* message, std::string& displayText);

#endif // UI_H
