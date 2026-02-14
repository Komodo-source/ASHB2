#include "./header/SaveLoad.h"
#include "./header/Entity.h"
#include "./header/FreeWillSystem.h"
#include <fstream>
#include <iostream>
#include <string>

void saveGame(const std::string& filepath, const std::vector<Entity>& entities, int day, int frameCounter) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open save file: " << filepath << std::endl;
        return;
    }

    file << "ASHB2_SAVE\n";
    file << "DAY:" << day << "\n";
    file << "FRAME:" << frameCounter << "\n";
    file << "ENTITY_COUNT:" << entities.size() << "\n";

    for (const Entity& entity : entities) {
        entity.saveTo(file);
    }

    file.close();
    std::cout << "Game saved to " << filepath << std::endl;
}

bool loadGame(const std::string& filepath, std::vector<Entity>& entities, int& day, int& frameCounter) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open save file: " << filepath << std::endl;
        return false;
    }

    std::string line;

    // Read header
    std::getline(file, line);
    if (line != "ASHB2_SAVE") {
        std::cerr << "Invalid save file format" << std::endl;
        return false;
    }

    // Read day
    std::getline(file, line);
    day = std::stoi(line.substr(4)); // "DAY:"

    // Read frame counter
    std::getline(file, line);
    frameCounter = std::stoi(line.substr(6)); // "FRAME:"

    // Read entity count
    std::getline(file, line);
    int entityCount = std::stoi(line.substr(13)); // "ENTITY_COUNT:"

    entities.clear();
    for (int i = 0; i < entityCount; i++) {
        Entity entity(0);
        entity.loadFrom(file);
        entities.push_back(entity);
    }

    // Resolve pointers after all entities are loaded
    for (Entity& entity : entities) {
        entity.resolvePointers(entities);
    }

    file.close();
    std::cout << "Game loaded from " << filepath << std::endl;
    return true;
}
