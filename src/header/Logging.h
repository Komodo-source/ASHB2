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
    std::ofstream completeLogFile;

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
        completeLogFile.open("./data/complete_logs.txt", std::ios::app);

        // Redirect std::cout to cmd_log.txt
        // if (cmdLogFile.is_open()) {
        //     std::cout.rdbuf(cmdLogFile.rdbuf());
        // }
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
        completeLogFile.close();
    }

    void logDeath(int entityId, const std::string& name, int age, const std::string& cause) {
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Entity " + std::to_string(entityId) + " (" + name + ", age " + std::to_string(age) + ") died: " + cause;
        deathsLogFile << msg << std::endl;
        completeLogFile << msg << std::endl;
    }

    void logDisease(int entityId, const std::string& name, const std::string& disease, bool cured = false) {
        std::string timestamp = getTimestamp();
        std::string action = cured ? "cured from" : "contracted";
        std::string msg = "[" + timestamp + "] Entity " + std::to_string(entityId) + " (" + name + ") " + action + " " + disease;
        diseasesLogFile << msg << std::endl;
        completeLogFile << msg << std::endl;
    }

    void logAction(int entityId, const std::string& name, const std::string& action, const std::string& target = "", const std::string& details = "") {
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Entity " + std::to_string(entityId) + " (" + name + ") performed: " + action;
        if (!target.empty()) {
            msg += " targeting " + target;
        }
        if (!details.empty()) {
            msg += " - " + details;
        }
        actionsLogFile << msg << std::endl;
        completeLogFile << msg << std::endl;
    }

    void logRelationship(int entityId1, const std::string& name1, int entityId2, const std::string& name2, const std::string& relationshipType, const std::string& details = "") {
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Relationship: " + name1 + " (" + std::to_string(entityId1) + ") and " + name2 + " (" + std::to_string(entityId2) + ") - " + relationshipType;
        if (!details.empty()) {
            msg += " - " + details;
        }
        relationshipsLogFile << msg << std::endl;
        completeLogFile << msg << std::endl;
    }

    void logMovement(int entityId, const std::string& name, float oldX, float oldY, float newX, float newY, const std::string& reason = "") {
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Entity " + std::to_string(entityId) + " (" + name + ") moved from (" + std::to_string(oldX) + "," + std::to_string(oldY) + ") to (" + std::to_string(newX) + "," + std::to_string(newY) + ")";
        if (!reason.empty()) {
            msg += " - " + reason;
        }
        movementsLogFile << msg << std::endl;
        completeLogFile << msg << std::endl;
    }

    void logBirth(int entityId, const std::string& name, int parent1Id, int parent2Id, const std::string& parent1Name, const std::string& parent2Name) {
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Birth: Entity " + std::to_string(entityId) + " (" + name + ") born to " + parent1Name + " (" + std::to_string(parent1Id) + ") and " + parent2Name + " (" + std::to_string(parent2Id) + ")";
        birthsLogFile << msg << std::endl;
        completeLogFile << msg << std::endl;
    }

    void logEvent(const std::string& eventType, const std::string& details) {
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] " + eventType + ": " + details;
        eventsLogFile << msg << std::endl;
        completeLogFile << msg << std::endl;
    }

    void logCmd(const std::string& message) {
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] " + message;
        cmdLogFile << msg << std::endl;
        completeLogFile << msg << std::endl;
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
        completeLogFile.close();

        // Reopen in truncate mode to clear
        cmdLogFile.open("./data/cmd_log.txt", std::ios::trunc);
        deathsLogFile.open("./data/deaths_log.txt", std::ios::trunc);
        diseasesLogFile.open("./data/diseases_log.txt", std::ios::trunc);
        actionsLogFile.open("./data/actions_log.txt", std::ios::trunc);
        relationshipsLogFile.open("./data/relationships_log.txt", std::ios::trunc);
        movementsLogFile.open("./data/movements_log.txt", std::ios::trunc);
        birthsLogFile.open("./data/births_log.txt", std::ios::trunc);
        eventsLogFile.open("./data/events_log.txt", std::ios::trunc);
        completeLogFile.open("./data/complete_logs.txt", std::ios::trunc);

        // Re-redirect cout
        // if (cmdLogFile.is_open()) {
        //     std::cout.rdbuf(cmdLogFile.rdbuf());
        // }
    }
};

// Global logger instance
extern Logger* globalLogger;

#endif
