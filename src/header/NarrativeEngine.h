#ifndef NARRATIVE_ENGINE_H
#define NARRATIVE_ENGINE_H

#include <string>
#include <deque>

class Entity;

class NarrativeEngine {
public:
    // Convert a simulation action into a readable English sentence
    static std::string actionToSentence(const Entity* ent, const std::string& action,
                                        const Entity* target, int hour,
                                        const std::string& locationName);

    // Generate a short inner-monologue line based on dominant emotional state
    static std::string innerMonologue(const Entity* ent);

    // "HH:MM" formatted string from simulation hour (0-23)
    static std::string formatHour(int hour);
};

// ── Global narrative state ───────────────────────────────────────────────────
// Populated every tick by applyFreeWill; read by the UI Truman panel.
extern std::deque<std::string> globalNarrativeLog;      // all entities
extern std::deque<std::string> protagonistNarrativeLog; // protagonist only
extern int protagonistEntityId;                         // -1 = none selected

#endif
