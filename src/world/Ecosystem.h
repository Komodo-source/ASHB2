#ifndef WORLD_ECOSYSTEM_H
#define WORLD_ECOSYSTEM_H

#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Ecosystem / food chain (Phase 3.1)
//
// A living layer that sits on top of the per-region ResourceSystem. Every region
// carries three trophic levels — vegetation (plants), herbivores (game), and
// predators — that interact through a discrete predator–prey model:
//
//     plants  grow logistically (scaled by the season) and are grazed down
//     herbivores eat plants, breed, starve when grass is thin, and are preyed on
//     predators eat herbivores, breed, and starve when game is scarce
//
// Agents hunt herbivores (depleting game) and forage plants. A drought starves
// the whole chain from the bottom up; overhunting collapses it from the top
// down. Either way game grows scarce and hunting yields fall — the bridge from
// the environment to famine and the economy.
// ─────────────────────────────────────────────────────────────────────────────

struct RegionEcology {
    bool  active     = false;
    float plants     = 0.0f;   // vegetation biomass
    float herbivores = 0.0f;   // grazer population (the game agents hunt)
    float predators  = 0.0f;   // carnivore population
    float plantCap   = 0.0f;   // vegetation carrying capacity (from fertility)
    float herbCap    = 0.0f;   // derived herbivore ceiling (for abundance scaling)

    // Running tallies (for the UI / introspection).
    float totalHunted = 0.0f;  // herbivores taken by agents over the whole run
    int   collapses   = 0;     // times game crashed toward extinction here
};

class EcosystemSystem {
public:
    std::vector<RegionEcology> regions;  // indexed by regionId

    // Seed each region's trophic levels from its food-resource ceiling. Call once,
    // after g_resources.init(planet).
    void init();

    // Advance the food chain one simulation day. `seasonalModifier` is the same
    // season+harvest factor that drives crop yields (a drought < 1 starves it).
    void update(float seasonalModifier, int day);

    bool valid(int regionId) const {
        return regionId >= 0 && regionId < (int)regions.size() && regions[regionId].active;
    }

    // 0.4 … 1.4 multiplier on hunting yield from current game density. Neutral
    // 1.0 for regions with no ecology, so agents are never starved by this layer.
    float gameAbundance(int regionId) const;
    // 0.5 … 1.3 multiplier on foraging yield from current vegetation density.
    float forageAbundance(int regionId) const;

    // An agent hunts the region: removes herbivores roughly in proportion to the
    // game actually taken (`taken` food units), so heavy hunting depletes stock.
    void huntPressure(int regionId, float taken);
    // An agent forages: lightly grazes the vegetation.
    void foragePressure(int regionId, float taken);

    // UI helpers.
    const char* healthLabel(int regionId) const;
};

extern EcosystemSystem g_ecosystem;

#endif // WORLD_ECOSYSTEM_H
