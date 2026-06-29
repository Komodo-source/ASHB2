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
    std::ofstream civilizationLogFile;
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
        // Create the data directory the logs actually live in. This used to be
        // "./data" while every file below opened under "./src/data", so when that
        // folder was missing the streams silently failed and nothing was logged.
        std::filesystem::create_directories("./src/data");

        // Open all log files in append mode
        cmdLogFile.open("./src/data/cmd_log.txt", std::ios::app);
        deathsLogFile.open("./src/data/deaths_log.txt", std::ios::app);
        diseasesLogFile.open("./src/data/diseases_log.txt", std::ios::app);
        actionsLogFile.open("./src/data/actions_log.txt", std::ios::app);
        relationshipsLogFile.open("./src/data/relationships_log.txt", std::ios::app);
        movementsLogFile.open("./src/data/movements_log.txt", std::ios::app);
        birthsLogFile.open("./src/data/births_log.txt", std::ios::app);
        eventsLogFile.open("./src/data/events_log.txt", std::ios::app);
        civilizationLogFile.open("./src/data/civilization_log.txt", std::ios::app);
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
        civilizationLogFile.close();
        completeLogFile.close();
    }

    // A death line carries the specific cause, and (optionally) a structured
    // context block that snapshots the deceased's terminal state — life stage,
    // kin, partner status and the stat that undid them. The leading
    // "Entity <id> (<name>, age <n>) died: <cause>" shape is kept stable for
    // parsers; richer fields are appended after a " | " delimiter as key=value
    // pairs so they can be mined without breaking the base format.
    void logDeath(int entityId, const std::string& name, int age, const std::string& cause,
                  const std::string& details = "") {
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] Entity " + std::to_string(entityId) + " (" + name + ", age " + std::to_string(age) + ") died: " + cause;
        if (!details.empty()) {
            msg += " | " + details;
        }
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

    // Civilization-scale events — everything that happens *above* the individual:
    // tribes forming/splitting/dissolving, wars declared & battles fought,
    // conquests, alliances & treaties, religions founded & spread, innovations
    // discovered & lost, famines, migrations, era changes and the rise of farming
    // specialists. The base shape stays parser-stable —
    //   "[ts] <category>: <description>"
    // — and a structured " | key=value ..." block (always carrying day=<n>, plus
    // event-specific fields like kind=, winner=, fallen=, members=) is appended so
    // the post-mortem analyst can mine them without scraping prose. Mirrors into
    // both its own civilization_log.txt and the unified complete_logs.txt.
    void logCiv(int day, const std::string& category, const std::string& description,
                const std::string& data = "") {
        std::string timestamp = getTimestamp();
        std::string msg = "[" + timestamp + "] " + category + ": " + description
                        + " | day=" + std::to_string(day);
        if (!data.empty()) {
            msg += " " + data;
        }
        civilizationLogFile << msg << std::endl;
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
        civilizationLogFile.close();
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
        civilizationLogFile.open("./src/data/civilization_log.txt", std::ios::trunc);
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
