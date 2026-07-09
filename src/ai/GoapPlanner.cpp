#include "GoapPlanner.h"
#include "../header/Entity.h"
#include "../items/ItemSystem.h"

#include <queue>
#include <set>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace goap {

std::string Step::describe(const ItemManager& mgr) const {
    if (gatherFood) return "Gather food";
    const ActionRule* r = mgr.rule(ruleId);
    return r ? r->name : "?";
}

WorldState captureState(const Entity& agent, const ItemManager& mgr) {
    WorldState st;
    st["stat:hunger"]  = agent.entityHunger;
    st["stat:health"]  = agent.entityHealth;
    st["stat:fatigue"] = agent.fatigueLevel;
    for (const ItemStack& s : agent.inventory.stacks) {
        const ItemDef* d = mgr.def(s.defId);
        if (!d) continue;
        for (const std::string& t : d->tags)
            st["tag:" + t] += s.qty;
    }
    return st;
}

namespace {

struct Node {
    WorldState        state;
    std::vector<Step> steps;
    float             g = 0.0f;   // accumulated cost
    float             f = 0.0f;   // g + heuristic
};

struct NodeCmp {
    bool operator()(const Node& a, const Node& b) const { return a.f > b.f; }
};

float heuristic(const WorldState& st, const Goal& goal) {
    auto it = st.find(goal.statKey);
    float v = it == st.end() ? (goal.below ? 100.0f : 0.0f) : it->second;
    float deficit = goal.below ? std::max(0.0f, v - goal.target)
                               : std::max(0.0f, goal.target - v);
    // Best single action moves a stat by ~25 (a good meal); admissible-ish.
    return deficit / 25.0f;
}

bool satisfied(const WorldState& st, const Goal& goal) {
    auto it = st.find(goal.statKey);
    float v = it == st.end() ? (goal.below ? 100.0f : 0.0f) : it->second;
    return goal.below ? v <= goal.target : v >= goal.target;
}

// Rounded fingerprint so float noise doesn't defeat the visited set.
std::string fingerprint(const WorldState& st) {
    std::ostringstream os;
    for (const auto& kv : st)
        if (kv.second > 0.01f)
            os << kv.first << ":" << (int)std::lround(kv.second) << ";";
    return os.str();
}

// Apply a rule symbolically. Returns false if preconditions unmet.
bool applyRule(const ActionRule& r, const ItemManager& mgr, WorldState& st,
               float& nutritionOfConsumedFood) {
    for (const std::string& t : r.requiresTags) {
        auto it = st.find("tag:" + t);
        if (it == st.end() || it->second < 1.0f) return false;
    }
    for (const TagQty& c : r.consumes) {
        auto it = st.find("tag:" + c.tag);
        if (it == st.end() || it->second < c.qty) return false;
    }
    for (const TagQty& c : r.consumes) {
        st["tag:" + c.tag] -= c.qty;
        if (c.tag == "food") nutritionOfConsumedFood = 22.0f; // planning estimate
    }
    for (const ItemStack& p : r.produces) {
        const ItemDef* d = mgr.def(p.defId);
        if (!d) continue;
        for (const std::string& t : d->tags)
            st["tag:" + t] += p.qty;
    }
    for (const StatEffect& fx : r.statEffects) {
        float delta = fx.delta;
        if (fx.stat == "hunger" && delta < 0.0f && nutritionOfConsumedFood > 0.0f)
            delta = -nutritionOfConsumedFood;
        float& v = st["stat:" + fx.stat];
        v = std::min(100.0f, std::max(0.0f, v + delta));
    }
    return true;
}

} // namespace

std::vector<Step> plan(const Entity& agent, const ItemManager& mgr,
                       const Goal& goal, const WorldState& start) {
    if (satisfied(start, goal)) return {};

    // The agent's action repertoire: every seeded rule (common knowledge) plus
    // its personally known (invented / learned) recipes. Expanded in id order
    // for determinism.
    std::vector<int> repertoire;
    for (const ActionRule& r : mgr.rules())
        if (!r.invented) repertoire.push_back(r.id);
    for (int rid : agent.knownRecipeIds)
        if (mgr.rule(rid) && mgr.rule(rid)->invented) repertoire.push_back(rid);
    std::sort(repertoire.begin(), repertoire.end());

    std::priority_queue<Node, std::vector<Node>, NodeCmp> open;
    std::set<std::string> visited;

    Node root{start, {}, 0.0f, heuristic(start, goal)};
    open.push(root);

    const int MAX_EXPANSIONS = 256;
    const int MAX_DEPTH      = 6;
    int expansions = 0;

    while (!open.empty() && expansions < MAX_EXPANSIONS) {
        Node cur = open.top(); open.pop();
        ++expansions;
        if (satisfied(cur.state, goal)) return cur.steps;
        if ((int)cur.steps.size() >= MAX_DEPTH) continue;
        std::string fp = fingerprint(cur.state);
        if (!visited.insert(fp).second) continue;

        // Expand: known rules…
        for (int rid : repertoire) {
            const ActionRule* r = mgr.rule(rid);
            if (!r) continue;
            WorldState next = cur.state;
            float nutr = -1.0f;
            if (!applyRule(*r, mgr, next, nutr)) continue;
            Node child;
            child.state = std::move(next);
            child.steps = cur.steps;
            child.steps.push_back(Step{rid, false});
            child.g = cur.g + r->duration;
            child.f = child.g + heuristic(child.state, goal);
            open.push(child);
        }
        // …plus the synthetic gather step: the environment can always be asked
        // for wild food, at a cost that makes crafting/held food preferable.
        {
            WorldState next = cur.state;
            next["tag:food"]       += 1.0f;
            next["tag:plant"]      += 1.0f;
            next["tag:perishable"] += 1.0f;
            next["stat:fatigue"] = std::min(100.0f, next["stat:fatigue"] + 4.0f);
            Node child;
            child.state = std::move(next);
            child.steps = cur.steps;
            Step s; s.gatherFood = true;
            child.steps.push_back(s);
            child.g = cur.g + 3.0f;
            child.f = child.g + heuristic(child.state, goal);
            open.push(child);
        }
    }
    return {};   // no plan within bounds
}

} // namespace goap
