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
        cmdLogFile.open("./src/data/cmd_log.txt", std::ios::app);
        deathsLogFile.open("./src/data/deaths_log.txt", std::ios::app);
        diseasesLogFile.open("./src/data/diseases_log.txt", std::ios::app);
        actionsLogFile.open("./src/data/actions_log.txt", std::ios::app);
        relationshipsLogFile.open("./src/data/relationships_log.txt", std::ios::app);
        movementsLogFile.open("./src/data/movements_log.txt", std::ios::app);
        birthsLogFile.open("./src/data/births_log.txt", std::ios::app);
        eventsLogFile.open("./src/data/events_log.txt", std::ios::app);
        completeLogFile.open("./src/data/complete_logs.txt", std::ios::app);


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
        cmdLogFile.open("./src/data/cmd_log.txt", std::ios::trunc);
        deathsLogFile.open("./src/data/deaths_log.txt", std::ios::trunc);
        diseasesLogFile.open("./src/data/diseases_log.txt", std::ios::trunc);
        actionsLogFile.open("./src/data/actions_log.txt", std::ios::trunc);
        relationshipsLogFile.open("./src/data/relationships_log.txt", std::ios::trunc);
        movementsLogFile.open("./src/data/movements_log.txt", std::ios::trunc);
        birthsLogFile.open("./src/data/births_log.txt", std::ios::trunc);
        eventsLogFile.open("./src/data/events_log.txt", std::ios::trunc);
        completeLogFile.open("./src/data/complete_logs.txt", std::ios::trunc);

        // Re-redirect cout
        // if (cmdLogFile.is_open()) {
        //     std::cout.rdbuf(cmdLogFile.rdbuf());
        // }
    }
};

// Global logger instance
extern Logger* globalLogger;

#endif

+++ src/header/Logging.h (修改后)
#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <atomic>

class Logger {
private:
    std::ofstream deathsLogFile;
    std::ofstream birthsLogFile;
    std::ofstream relationshipsLogFile;
    std::ofstream eventsLogFile;

    // Sampling rates to reduce data volume (log only 1 of N events)
    static constexpr int ACTION_SAMPLE_RATE = 100;      // Log 1% of actions
    static constexpr int MOVEMENT_SAMPLE_RATE = 1000;   // Log 0.1% of movements
    static constexpr int RELATIONSHIP_SAMPLE_RATE = 10; // Log 10% of relationship changes

    std::atomic<uint64_t> actionCounter{0};
    std::atomic<uint64_t> movementCounter{0};
    std::atomic<uint64_t> relationshipCounter{0};

    bool enableDetailedLogging;  // Set to false for production runs

    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

public:
    Logger() : enableDetailedLogging(false) {
        // Create data directory if it doesn't exist
        std::filesystem::create_directories("./data");

        // Open only essential log files in append mode
        deathsLogFile.open("./data/deaths_log.txt", std::ios::app);
        birthsLogFile.open("./data/births_log.txt", std::ios::app);
        relationshipsLogFile.open("./data/relationships_log.txt", std::ios::app);
        eventsLogFile.open("./data/events_log.txt", std::ios::app);
    }

    ~Logger() {
        // Close all files
        deathsLogFile.close();
        birthsLogFile.close();
        relationshipsLogFile.close();
        eventsLogFile.close();
    }

    // Enable/disable detailed logging (for debugging only)
    void setDetailedLogging(bool enable) {
        enableDetailedLogging = enable;
    }

    bool isDetailedLoggingEnabled() const {
        return enableDetailedLogging;
    }

    void logDeath(int entityId, const std::string& name, int age, const std::string& cause) {
        if (!deathsLogFile.is_open()) return;
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Entity " + std::to_string(entityId) + " (" + name + ", age " + std::to_string(age) + ") died: " + cause;
        deathsLogFile << msg << std::endl;
    }

    void logBirth(int entityId, const std::string& name, int parent1Id, int parent2Id, const std::string& parent1Name, const std::string& parent2Name) {
        if (!birthsLogFile.is_open()) return;
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Birth: Entity " + std::to_string(entityId) + " (" + name + ") born to " + parent1Name + " (" + std::to_string(parent1Id) + ") and " + parent2Name + " (" + std::to_string(parent2Id) + ")";
        birthsLogFile << msg << std::endl;
    }

    void logRelationship(int entityId1, const std::string& name1, int entityId2, const std::string& name2, const std::string& relationshipType, const std::string& details = "") {
        // Use sampling to reduce data volume unless detailed logging is enabled
        if (!enableDetailedLogging) {
            if (relationshipCounter.fetch_add(1) % RELATIONSHIP_SAMPLE_RATE != 0) {
                return;  // Skip this log entry
            }
        }

        if (!relationshipsLogFile.is_open()) return;
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Relationship: " + name1 + " (" + std::to_string(entityId1) + ") and " + name2 + " (" + std::to_string(entityId2) + ") - " + relationshipType;
        if (!details.empty()) {
            msg += " - " + details;
        }
        relationshipsLogFile << msg << std::endl;
    }

    void logAction(int entityId, const std::string& name, const std::string& action, const std::string& target = "", const std::string& details = "") {
        // Use sampling to reduce data volume unless detailed logging is enabled
        if (!enableDetailedLogging) {
            if (actionCounter.fetch_add(1) % ACTION_SAMPLE_RATE != 0) {
                return;  // Skip this log entry
            }
        }

        // Only log significant actions when not in detailed mode
        if (!enableDetailedLogging &&
            action != "couple formed" &&
            action != "break up" &&
            action != "reproduce") {
            return;
        }

        if (!eventsLogFile.is_open()) return;
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Entity " + std::to_string(entityId) + " (" + name + ") performed: " + action;
        if (!target.empty()) {
            msg += " targeting " + target;
        }
        if (!details.empty()) {
            msg += " - " + details;
        }
        eventsLogFile << msg << std::endl;
    }

    void logMovement(int entityId, const std::string& name, float oldX, float oldY, float newX, float newY, const std::string& reason = "") {
        // Skip movement logging entirely unless detailed logging is enabled
        if (!enableDetailedLogging) {
            return;
        }

        // Even in detailed mode, sample movements heavily
        if (movementCounter.fetch_add(1) % MOVEMENT_SAMPLE_RATE != 0) {
            return;
        }

        if (!eventsLogFile.is_open()) return;
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Entity " + std::to_string(entityId) + " (" + name + ") moved from (" +
                          std::to_string(oldX) + "," + std::to_string(oldY) + ") to (" +
                          std::to_string(newX) + "," + std::to_string(newY) + ")";
        if (!reason.empty()) {
            msg += " - " + reason;
        }
        eventsLogFile << msg << std::endl;
    }

    void logEvent(const std::string& eventType, const std::string& details) {
        if (!eventsLogFile.is_open()) return;
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] " + eventType + ": " + details;
        eventsLogFile << msg << std::endl;
    }

    // Removed verbose methods: logDisease, logCmd, logMovement (unless detailed), clearAllLogs
};

// Global logger instance
extern Logger* globalLogger;

#endif