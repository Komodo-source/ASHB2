#ifndef GOAP_PLANNER_H
#define GOAP_PLANNER_H

// ─── GoapPlanner — A* over ActionRules (Upgrade Plan, Step 3) ─────────────────
// Goal-Oriented Action Planning: the agent states a GOAL as desired facts
// ("stat:hunger ≤ 30"), the planner searches the space of world states
// reachable through its KNOWN ActionRules (plus a synthetic Gather step for
// tags obtainable from the environment), and returns the cheapest rule
// sequence. Facts are numeric:
//   "tag:<t>"    — how many items carrying tag <t> the agent holds
//   "stat:<s>"   — agent stat (hunger, health, fatigue…), 0..100
//
// A* is forward-chaining with the admissible heuristic "remaining deficit /
// best single-action improvement". Search is bounded (≤ depth 6, ≤ 256 nodes)
// so a 2000-agent world can afford per-day replans for the agents that need
// one. Deterministic: rules are expanded in id order, ties broken by id.

#include <string>
#include <vector>
#include <map>

class Entity;
class ItemManager;

namespace goap {

using WorldState = std::map<std::string, float>;

// One planned step: an ActionRule to execute, or the synthetic gather step.
struct Step {
    int  ruleId = -1;               // >=0 → execute this ActionRule
    bool gatherFood = false;        // synthetic: forage until a "food" tag is held
    std::string describe(const ItemManager& mgr) const;
};

struct Goal {
    std::string statKey;    // e.g. "stat:hunger"
    float       target;     // desired value
    bool        below;      // true → want stat ≤ target, false → want ≥
};

// Snapshot the agent's inventory + stats into planner facts.
WorldState captureState(const Entity& agent, const ItemManager& mgr);

// Plan toward `goal`. Empty result = no plan found (or already satisfied).
std::vector<Step> plan(const Entity& agent, const ItemManager& mgr,
                       const Goal& goal, const WorldState& start);

// Convenience: the everyday survival goal.
inline Goal reduceHungerGoal() { return Goal{"stat:hunger", 30.0f, true}; }

} // namespace goap

#endif // GOAP_PLANNER_H
