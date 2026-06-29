#include "TechTree.h"
#include "Entity.h"
#include "CivilizationEngine.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace {

// The canonical technology tree. Ids are stable and used in Tribe::techTreeUnlocked.
// Tiers roughly track the eras: stone tools → agriculture → bronze/writing → iron.
const std::vector<TechNode> kTree = {
    // ── Tier 0: foundations (no prerequisites) ───────────────────────────────
    { 0,  "Toolmaking",      "tool",        0, {},        40.f,  0.f,  0.00f, 0.05f, 0.00f, 0.05f,
      "Knapped stone tools — the root of all crafts." },
    { 1,  "Foraging Lore",   "agriculture", 0, {},        40.f,  0.f,  0.10f, 0.00f, 0.00f, 0.00f,
      "Knowing which plants feed and which kill." },
    { 2,  "Fire Mastery",    "tool",        0, {},        50.f,  0.f,  0.05f, 0.05f, 0.00f, 0.00f,
      "Cooking, warmth, and the first weapon of fear." },

    // ── Tier 1: the neolithic package ────────────────────────────────────────
    { 3,  "Agriculture",     "agriculture", 1, {1},      120.f, 30.f,  0.25f, 0.00f, 0.00f, 0.05f,
      "Sown fields free a people from the chase." },
    { 4,  "Animal Husbandry","agriculture", 1, {1},      120.f, 25.f,  0.18f, 0.05f, 0.00f, 0.00f,
      "Herds for meat, milk, muscle, and war." },
    { 5,  "Pottery",         "tool",        1, {2},      110.f, 20.f,  0.10f, 0.00f, 0.00f, 0.05f,
      "Vessels to store the surplus against famine." },

    // ── Tier 2: the bronze & literate world ──────────────────────────────────
    { 6,  "Masonry",         "tool",        2, {5},      220.f, 50.f,  0.00f, 0.00f, 0.20f, 0.00f,
      "Walls of dressed stone that outlast their builders." },
    { 7,  "Bronze Working",  "military",    2, {0, 2},   240.f, 60.f,  0.00f, 0.25f, 0.05f, 0.00f,
      "Alloyed blades that shame the flint." },
    { 8,  "Writing",         "social",      2, {5},      250.f, 40.f,  0.00f, 0.00f, 0.00f, 0.25f,
      "Memory made permanent; knowledge that compounds." },
    { 9,  "Irrigation",      "agriculture", 2, {3},      230.f, 50.f,  0.25f, 0.00f, 0.00f, 0.00f,
      "Canals that make the desert bloom." },

    // ── Tier 3: the iron age ─────────────────────────────────────────────────
    { 10, "Iron Working",    "military",    3, {7},      380.f, 90.f,  0.00f, 0.30f, 0.10f, 0.00f,
      "Cheap, brutal iron arms whole armies." },
    { 11, "Mathematics",     "social",      3, {8},      360.f, 60.f,  0.00f, 0.00f, 0.05f, 0.20f,
      "Number and proof — the engine of engineering." },
    { 12, "Currency",        "social",      3, {8},      340.f, 50.f,  0.05f, 0.00f, 0.00f, 0.10f,
      "Coined value lubricates every exchange." },
    { 13, "Fortification",   "military",    3, {6, 7},   400.f,110.f,  0.00f, 0.00f, 0.35f, 0.00f,
      "Citadels behind which a people can defy an age." },
};

} // namespace

namespace TechTreeSystem {

const std::vector<TechNode>& tree() { return kTree; }

const TechNode* findNode(int id) {
    for (const TechNode& n : kTree) if (n.id == id) return &n;
    return nullptr;
}

bool prereqsMet(const Tribe& t, const TechNode& n) {
    for (int p : n.prereqs)
        if (t.techTreeUnlocked.find(p) == t.techTreeUnlocked.end()) return false;
    return true;
}

static float multiplier(const Tribe& t, float TechNode::* field) {
    float m = 1.0f;
    for (int id : t.techTreeUnlocked) {
        const TechNode* n = findNode(id);
        if (n) m *= (1.0f + n->*field);
    }
    return m;
}

float foodMultiplier(const Tribe& t)     { return multiplier(t, &TechNode::foodBonus); }
float militaryMultiplier(const Tribe& t) { return multiplier(t, &TechNode::militaryBonus); }
float defenseMultiplier(const Tribe& t)  { return multiplier(t, &TechNode::defenseBonus); }
float researchMultiplier(const Tribe& t) { return multiplier(t, &TechNode::researchBonus); }

void tick(CivilizationEngine& eng, std::vector<Entity>& entities, int day) {
    for (Tribe& tribe : eng.tribes) {
        int pop = tribe.population();
        if (pop <= 0) continue;

        // Count scholars — they are the multiplier on a tribe's research.
        int scholars = 0;
        for (int mid : tribe.memberIds) {
            for (Entity& e : entities) {
                if (e.entityId == mid && e.entityHealth > 0.0f) {
                    if (e.isSpecialist && e.specialization == "scholar") scholars++;
                    break;
                }
            }
        }

        // Research accrues from raw population + (heavily) scholars + cultural
        // inventiveness, amplified by techs like Writing and Mathematics.
        float gain = (pop * 0.5f + scholars * 2.5f + tribe.innovation * 0.04f)
                   * researchMultiplier(tribe);
        tribe.researchPoints += gain;

        // Unlock the single cheapest affordable, prerequisite-met node this tick
        // (pacing the climb). A node costs both research points and granary food.
        const TechNode* best = nullptr;
        for (const TechNode& n : kTree) {
            if (tribe.techTreeUnlocked.count(n.id)) continue;
            if (!prereqsMet(tribe, n)) continue;
            if (tribe.researchPoints < n.knowledgeCost) continue;
            if (tribe.granary       < n.resourceCost)  continue;
            if (!best || n.knowledgeCost < best->knowledgeCost) best = &n;
        }
        if (best) {
            tribe.researchPoints -= best->knowledgeCost;
            tribe.granary        -= best->resourceCost;
            tribe.techTreeUnlocked.insert(best->id);
            eng.logEvent(day, tribe.name + " mastered " + best->name
                              + " (" + best->desc + ")", "innovation");
        }
    }
}

std::string nextGoal(const Tribe& t) {
    const TechNode* best = nullptr;
    for (const TechNode& n : kTree) {
        if (t.techTreeUnlocked.count(n.id)) continue;
        if (!prereqsMet(t, n)) continue;
        if (!best || n.knowledgeCost < best->knowledgeCost) best = &n;
    }
    if (!best) return "";
    std::ostringstream os;
    os << best->name << " ("
       << (int)std::min(t.researchPoints, best->knowledgeCost) << "/"
       << (int)best->knowledgeCost << " rp)";
    return os.str();
}

std::string summary(const Tribe& t) {
    std::ostringstream os;
    os << t.techTreeUnlocked.size() << "/" << kTree.size() << " techs";
    os << std::fixed << std::setprecision(2)
       << "  [food x" << foodMultiplier(t)
       << ", mil x"   << militaryMultiplier(t)
       << ", def x"   << defenseMultiplier(t) << "]";
    std::string goal = nextGoal(t);
    if (!goal.empty()) os << "  next: " << goal;
    return os.str();
}

} // namespace TechTreeSystem
