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

// Escapes a string for JSON format
std::string escapeJSONString(const std::string& input) {
    std::string output;
    for (char c : input) {
        switch (c) {
            case '\"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default: output += c; break;
        }
    }
    return output;
}

void exportTickHistory(const std::string& filepath, const std::vector<Entity>& entities, int day) {
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open tick history file: " << filepath << std::endl;
        return;
    }

    // Build the JSON line manually to avoid external library dependencies
    file << "{";
    file << "\"day\":" << day << ",";
    file << "\"entityCount\":" << entities.size() << ",";
    file << "\"entities\":[";

    for (size_t i = 0; i < entities.size(); ++i) {
        const Entity& entity = entities[i];

        file << "{";
        file << "\"id\":" << entity.entityId << ",";
        // To access the const name we have to bypass the non-const getName() if one doesn't exist, but Name is public
        file << "\"name\":\"" << escapeJSONString(entity.name) << "\",";
        file << "\"age\":" << entity.entityAge << ",";
        file << "\"health\":" << entity.entityHealth << ",";
        file << "\"happiness\":" << entity.entityHapiness << ",";
        file << "\"stress\":" << entity.entityStress << ",";
        file << "\"mentalHealth\":" << entity.entityMentalHealth << ",";
        file << "\"loneliness\":" << entity.entityLoneliness << ",";
        file << "\"boredom\":" << entity.entityBoredom << ",";
        file << "\"anger\":" << entity.entityGeneralAnger << ",";
        file << "\"hygiene\":" << entity.entityHygiene << ",";

        // Ensure character string uses double quotes as string
        file << "\"sex\":\"" << std::string(1, entity.entitySex) << "\",";

        file << "\"disease\":" << entity.entityDiseaseType << ",";
        file << "\"posX\":" << entity.posX << ",";
        file << "\"posY\":" << entity.posY << ",";

        // Additional info like personality can be easily extended
        file << "\"personality\":{";
        file << "\"e\":" << entity.personality.extraversion << ",";
        file << "\"a\":" << entity.personality.agreeableness << ",";
        file << "\"c\":" << entity.personality.conscientiousness << ",";
        file << "\"n\":" << entity.personality.neuroticism << ",";
        file << "\"o\":" << entity.personality.openness;
        file << "}";

        file << "}";
        if (i < entities.size() - 1) {
            file << ",";
        }
    }

    file << "]" << "}\n";
    file.close();
}

