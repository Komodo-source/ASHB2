#ifndef SAVELOAD_H
#define SAVELOAD_H

#include <string>
#include <vector>

class Entity;

void saveGame(const std::string& filepath, const std::vector<Entity>& entities, int day, int frameCounter);
bool loadGame(const std::string& filepath, std::vector<Entity>& entities, int& day, int& frameCounter);

#endif // SAVELOAD_H
