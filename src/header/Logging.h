#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

class Logger {
private:
    std::ofstream cmdLogFile;
    std::ofstream deathsLogFile;
    std::ofstream diseasesLogFile;
    std::ofstream actionsLogFile;
    std::ofstream relationshipsLogFile;
    std::ofstream movementsLogFile;
    std::ofstream birthsLogFile;
    std::ofstream eventsLogFile;

    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

public:
    Logger() {
        // Create data directory if it doesn't exist
        std::filesystem::create_directories("./data");

        // Open all log files in append mode
        cmdLogFile.open("./data/cmd_log.txt", std::ios::app);
        deathsLogFile.open("./data/deaths_log.txt", std::ios::app);
        diseasesLogFile.open("./data/diseases_log.txt", std::ios::app);
        actionsLogFile.open("./data/actions_log.txt", std::ios::app);
        relationshipsLogFile.open("./data/relationships_log.txt", std::ios::app);
        movementsLogFile.open("./data/movements_log.txt", std::ios::app);
        birthsLogFile.open("./data/births_log.txt", std::ios::app);
        eventsLogFile.open("./data/events_log.txt", std::ios::app);

        // Redirect std::cout to cmd_log.txt
        if (cmdLogFile.is_open()) {
            std::cout.rdbuf(cmdLogFile.rdbuf());
        }
    }

    ~Logger() {
        // Close all files
        cmdLogFile.close();
        deathsLogFile.close();
        diseasesLogFile.close();
        actionsLogFile.close();
        relationshipsLogFile.close();
        movementsLogFile.close();
        birthsLogFile.close();
        eventsLogFile.close();
    }

    void logDeath(int entityId, const std::string& name, int age, const std::string& cause) {
        std::string timestamp = getTimestamp();
        deathsLogFile << "[" << timestamp << "] Entity " << entityId << " (" << name << ", age " << age << ") died: " << cause << std::endl;
    }

    void logDisease(int entityId, const std::string& name, const std::string& disease, bool cured = false) {
        std::string timestamp = getTimestamp();
        std::string action = cured ? "cured from" : "contracted";
        diseasesLogFile << "[" << timestamp << "] Entity " << entityId << " (" << name << ") " << action << " " << disease << std::endl;
    }

    void logAction(int entityId, const std::string& name, const std::string& action, const std::string& target = "", const std::string& details = "") {
        std::string timestamp = getTimestamp();
        actionsLogFile << "[" << timestamp << "] Entity " << entityId << " (" << name << ") performed: " << action;
        if (!target.empty()) {
            actionsLogFile << " targeting " << target;
        }
        if (!details.empty()) {
            actionsLogFile << " - " << details;
        }
        actionsLogFile << std::endl;
    }

    void logRelationship(int entityId1, const std::string& name1, int entityId2, const std::string& name2, const std::string& relationshipType, const std::string& details = "") {
        std::string timestamp = getTimestamp();
        relationshipsLogFile << "[" << timestamp << "] Relationship: " << name1 << " (" << entityId1 << ") and " << name2 << " (" << entityId2 << ") - " << relationshipType;
        if (!details.empty()) {
            relationshipsLogFile << " - " << details;
        }
        relationshipsLogFile << std::endl;
    }

    void logMovement(int entityId, const std::string& name, float oldX, float oldY, float newX, float newY, const std::string& reason = "") {
        std::string timestamp = getTimestamp();
        movementsLogFile << "[" << timestamp << "] Entity " << entityId << " (" << name << ") moved from (" << oldX << "," << oldY << ") to (" << newX << "," << newY << ")";
        if (!reason.empty()) {
            movementsLogFile << " - " << reason;
        }
        movementsLogFile << std::endl;
    }

    void logBirth(int entityId, const std::string& name, int parent1Id, int parent2Id, const std::string& parent1Name, const std::string& parent2Name) {
        std::string timestamp = getTimestamp();
        birthsLogFile << "[" << timestamp << "] Birth: Entity " << entityId << " (" << name << ") born to " << parent1Name << " (" << parent1Id << ") and " << parent2Name << " (" << parent2Id << ")" << std::endl;
    }

    void logEvent(const std::string& eventType, const std::string& details) {
        std::string timestamp = getTimestamp();
        eventsLogFile << "[" << timestamp << "] " << eventType << ": " << details << std::endl;
    }

    // Utility function to clear all log files
    void clearAllLogs() {
        cmdLogFile.close();
        deathsLogFile.close();
        diseasesLogFile.close();
        actionsLogFile.close();
        relationshipsLogFile.close();
        movementsLogFile.close();
        birthsLogFile.close();
        eventsLogFile.close();

        // Reopen in truncate mode to clear
        cmdLogFile.open("./data/cmd_log.txt", std::ios::trunc);
        deathsLogFile.open("./data/deaths_log.txt", std::ios::trunc);
        diseasesLogFile.open("./data/diseases_log.txt", std::ios::trunc);
        actionsLogFile.open("./data/actions_log.txt", std::ios::trunc);
        relationshipsLogFile.open("./data/relationships_log.txt", std::ios::trunc);
        movementsLogFile.open("./data/movements_log.txt", std::ios::trunc);
        birthsLogFile.open("./data/births_log.txt", std::ios::trunc);
        eventsLogFile.open("./data/events_log.txt", std::ios::trunc);

        // Re-redirect cout
        if (cmdLogFile.is_open()) {
            std::cout.rdbuf(cmdLogFile.rdbuf());
        }
    }
};

// Global logger instance
extern Logger* globalLogger;

#endif
