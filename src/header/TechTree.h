#ifndef TECH_TREE_H
#define TECH_TREE_H

#include <string>
#include <vector>
#include "CivilizationEngine.h"   // for Tribe

class Entity;
class CivilizationEngine;

// ─────────────────────────────────────────────────────────────────────────────
// Structured technology tree (Phase 4.1)
//
// A static, prerequisite-gated tree of technologies. Each tribe researches it
// independently: research points accrue from its population and scholars, and
// unlocking a node costs both knowledge (research points) and stockpiled food
// (granary) — so the economy gates advancement. Unlocked nodes grant concrete,
// multiplicatively-stacking bonuses that feed the war and economy systems.
//
// This complements the existing emergent `Innovation` diffusion: innovations
// are random discoveries that spread between individuals; the tech tree is the
// deliberate, cumulative climb of a whole tribe up the ladder of eras.
// ─────────────────────────────────────────────────────────────────────────────

struct TechNode {
    int                id;
    std::string        name;
    std::string        category;     // "tool"|"agriculture"|"military"|"social"
    int                tier;         // 0 = foundational … higher = more advanced
    std::vector<int>   prereqs;      // node ids that must be unlocked first
    float              knowledgeCost; // research points required
    float              resourceCost;  // food drawn from the granary to unlock
    // Stacking bonuses (fraction, e.g. 0.25 = +25%); applied as products.
    float              foodBonus     = 0.0f;
    float              militaryBonus = 0.0f;
    float              defenseBonus  = 0.0f;
    float              researchBonus = 0.0f;
    std::string        desc;
};

namespace TechTreeSystem {
    // The immutable tech tree definition.
    const std::vector<TechNode>& tree();
    const TechNode*              findNode(int id);

    // Whether every prerequisite of `n` is already unlocked by `t`.
    bool prereqsMet(const Tribe& t, const TechNode& n);

    // Stacking multipliers from all of a tribe's unlocked techs (>= 1.0).
    float foodMultiplier(const Tribe& t);
    float militaryMultiplier(const Tribe& t);
    float defenseMultiplier(const Tribe& t);
    float researchMultiplier(const Tribe& t);

    // Advance research for every tribe and unlock affordable nodes (≤1/tribe/tick).
    // Logs unlocks through the engine's event log.
    void  tick(CivilizationEngine& eng,
               std::vector<Entity>& entities,
               int day);

    // Human-readable one-line summary of a tribe's tech state (for the UI).
    std::string summary(const Tribe& t);
    // Name of the next reachable (prereqs met) node a tribe is saving toward,
    // with progress, or "" if everything reachable is already unlocked.
    std::string nextGoal(const Tribe& t);
}

#endif // TECH_TREE_H
