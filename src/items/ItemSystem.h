#ifndef ITEM_SYSTEM_H
#define ITEM_SYSTEM_H

// ─── ItemSystem — data-driven items & action rules (Upgrade Plan, Step 2) ─────
// No item classes, no hardcoded verbs. An ItemDef is a bag of Tags plus a bag
// of numeric Properties; an ActionRule is Preconditions (tags consumed or
// required) plus Effects (items produced, stats changed). Everything an agent
// can hold, eat, craft, or invent flows through these two tables.
//
//   * Seeded defs/rules come from ItemManager::seed(), which imports the
//     existing Market catalog (Economics.h) so the old economy and the new
//     item layer share one vocabulary.
//   * Invented defs/rules are GENERATED at runtime by combining two inventory
//     items (tag union, property blend). Success is gated by the same
//     (18/complexity)² law the innovation catalog uses, so invention pacing
//     matches the civilization's discovery pacing.
//   * Item VALUE is never stored: ItemManager::subjectiveValue() computes it
//     per-agent, per-moment from need-response curves (UtilityCurves.h) — a
//     starving agent literally prices "food" tags exponentially higher.
//
// This header is Entity-free (forward declaration only); everything that
// touches agent internals lives in ItemSystem.cpp.

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <fstream>

class Entity;
class CivilizationEngine;
class PheromoneField;

// ── Items ─────────────────────────────────────────────────────────────────────
struct ItemDef {
    int                          id = -1;
    std::string                  name;
    std::vector<std::string>     tags;    // e.g. {"food","plant","perishable"}
    std::map<std::string, float> props;   // e.g. {{"nutrition",22},{"durability",10}}
    float                        complexity = 10.0f; // invention difficulty
    bool                         invented   = false; // generated at runtime?
    int                          parentA = -1, parentB = -1; // lineage of invented defs

    bool  hasTag(const std::string& t) const {
        for (const auto& tag : tags) if (tag == t) return true;
        return false;
    }
    float prop(const std::string& k, float dflt = 0.0f) const {
        auto it = props.find(k);
        return it == props.end() ? dflt : it->second;
    }
};

struct ItemStack {
    int   defId = -1;
    float qty   = 0.0f;
    float condition = 100.0f;  // freshness/durability, decays for perishables
};

struct Inventory {
    std::vector<ItemStack> stacks;

    float count(int defId) const {
        float n = 0.0f;
        for (const auto& s : stacks) if (s.defId == defId) n += s.qty;
        return n;
    }
    void add(int defId, float qty, float condition = 100.0f) {
        if (qty <= 0.0f) return;
        for (auto& s : stacks)
            if (s.defId == defId) { s.qty += qty; return; }
        stacks.push_back(ItemStack{defId, qty, condition});
    }
    bool remove(int defId, float qty) {
        for (auto& s : stacks) {
            if (s.defId != defId || s.qty < qty) continue;
            s.qty -= qty;
            if (s.qty <= 0.001f) { s = stacks.back(); stacks.pop_back(); }
            return true;
        }
        return false;
    }
    bool empty() const { return stacks.empty(); }
};

// ── Action rules ──────────────────────────────────────────────────────────────
struct TagQty     { std::string tag;  float qty = 1.0f; };
struct StatEffect { std::string stat; float delta = 0.0f; }; // "hunger", "health", "happiness", "fatigue"

struct ActionRule {
    int                      id = -1;
    std::string              name;
    std::vector<TagQty>      consumes;      // inputs destroyed by the action
    std::vector<std::string> requiresTags;  // tools: must be held, not consumed
    std::vector<ItemStack>   produces;      // outputs added to inventory
    std::vector<StatEffect>  statEffects;   // direct agent-stat deltas
    float                    duration   = 1.0f;  // GOAP cost
    float                    complexity = 10.0f;
    bool                     invented   = false;
    int                      inventorEntityId = -1;
    int                      inventedOnDay    = -1;
};

// ── Manager ───────────────────────────────────────────────────────────────────
class ItemManager {
public:
    // Build the seeded def/rule tables. Imports the Market product catalog so
    // both economies speak the same names; safe to call once at startup.
    void seed();

    // The per-day agent layer: metabolism, foraging (memory- and pheromone-
    // guided), eating, invention, invented-rule execution, and visual-range
    // recipe diffusion. Called once per simulation day from the main loop.
    void tickAgents(std::vector<Entity>& entities, int day,
                    float worldW, float worldH, PheromoneField* field,
                    CivilizationEngine* civ);

    // Need-based value of a def for THIS agent RIGHT NOW (UtilityCurves).
    float subjectiveValue(const Entity& agent, const ItemDef& def) const;

    // Execute `rule` for `agent` if preconditions hold. Returns success.
    // On success, invented rules diffuse to watchers (cultural transmission) —
    // the caller passes the watcher list it already has.
    bool executeRule(Entity& agent, const ActionRule& rule,
                     const std::vector<Entity*>& watchers, int day,
                     CivilizationEngine* civ);

    // Attempt to invent by combining two held stacks. Complexity-gated.
    // Returns new rule id, or -1 on fizzle/no materials.
    int tryInvent(Entity& agent, int day, CivilizationEngine* civ);

    // Tag-aware inventory helpers (Inventory itself is def-blind).
    float countTag(const Inventory& inv, const std::string& tag) const;
    int   firstStackWithTag(const Inventory& inv, const std::string& tag) const;

    const std::vector<ItemDef>&    defs()  const { return defs_;  }
    const std::vector<ActionRule>& rules() const { return rules_; }
    const ItemDef*    def(int id)  const { return (id >= 0 && id < (int)defs_.size())  ? &defs_[id]  : nullptr; }
    const ActionRule* rule(int id) const { return (id >= 0 && id < (int)rules_.size()) ? &rules_[id] : nullptr; }
    int   defIdByName(const std::string& n) const {
        auto it = defByName_.find(n);
        return it == defByName_.end() ? -1 : it->second;
    }

    int inventedDefCount()  const { return inventedDefs_;  }
    int inventedRuleCount() const { return inventedRules_; }
    int recipeSpreadCount() const { return recipeSpreads_; }

    // SaveLoad v2: only runtime-generated content is persisted; seeded content
    // is rebuilt from seed() so save files survive catalog tuning.
    void saveTo(std::ofstream& f) const;
    void loadFrom(std::ifstream& f);

    bool seeded() const { return seededFlag_; }

private:
    int  addDef(ItemDef d);
    int  addRule(ActionRule r);
    void seedBaseRules();

    std::vector<ItemDef>       defs_;
    std::vector<ActionRule>    rules_;
    std::map<std::string, int> defByName_;
    bool seededFlag_    = false;
    int  inventedDefs_  = 0;
    int  inventedRules_ = 0;
    int  recipeSpreads_ = 0;

    // Well-known seeded ids the agent layer keys off.
    int idWildFood_ = -1, idEatRule_ = -1, idForageRule_ = -1;

public:
    int wildFoodDefId() const { return idWildFood_; }
    int eatRuleId()     const { return idEatRule_; }
    int forageRuleId()  const { return idForageRule_; }
};

extern ItemManager g_itemManager;

#endif // ITEM_SYSTEM_H
