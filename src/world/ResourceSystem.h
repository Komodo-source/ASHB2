#ifndef WORLD_RESOURCE_SYSTEM_H
#define WORLD_RESOURCE_SYSTEM_H

#include <vector>
#include <array>
#include <string>

class Planet;

// ── Primal resources ────────────────────────────────────────────────────────
// The six things an ancient settlement lives or dies by. Geography decides where
// they are abundant; use drains them; nature (and fallow rest) refills them.
enum ResourceType {
    RES_FOOD = 0,   // crops, game, forage — fertility + rivers
    RES_WOOD,       // forests / jungle
    RES_STONE,      // mountains, high ground, coasts
    RES_METAL,      // ore-rich rock (mountains)
    RES_WATER,      // rivers, moisture, coast
    RES_HERBS,      // medicinal/aromatic plants — damp forest & grassland
    RES_COUNT
};

const char* resourceName(ResourceType t);

// Per-region extractable stock. `baseCapacity` is the pristine ceiling the land
// recovers toward; `capacity` is the current (possibly degraded) ceiling.
// Sustained over-extraction degrades `capacity` — deforestation / soil
// exhaustion — and fallow rest lets it heal back toward `baseCapacity`.
struct ResourcePool {
    std::array<float, RES_COUNT> capacity{};      // current ceiling (can degrade)
    std::array<float, RES_COUNT> baseCapacity{};  // pristine ceiling (heal target)
    std::array<float, RES_COUNT> stock{};         // currently available
    std::array<float, RES_COUNT> regen{};         // natural refill / day (absolute)
    int   tileCount = 0;
    bool  active    = false;                      // habitable land region
};

// ── Resource distribution & depletion ───────────────────────────────────────
// Owns one ResourcePool per planet region. Geography sets the ceilings once;
// every simulation day stocks regenerate, depleted land degrades, rested land
// heals. Food production and (later) crafting draw from these pools, so where
// you settle genuinely matters and over-crowding a valley exhausts it.
class ResourceSystem {
public:
    std::vector<ResourcePool> pools;  // indexed by regionId

    // Derive each region's resource ceilings from its tiles' biome / fertility /
    // ore / rivers. Call once, right after the planet is generated.
    void init(const Planet& planet);

    // Advance one simulation day: regenerate stocks (food/herbs scale with the
    // season), then degrade exhausted land and heal rested land.
    void update(float seasonalModifier);

    // Take up to `requested` units of a resource from a region, depleting the
    // stock. Returns the amount actually obtained. No-op (returns 0) for an
    // invalid region.
    float extract(int regionId, ResourceType type, float requested);

    // 0.6..1.3 local abundance multiplier (stock vs ceiling) for scaling yields
    // without bypassing the existing survival balance. Neutral 1.0 for unknown
    // regions, so entities with no home region are never starved by this layer.
    float abundance(int regionId, ResourceType type) const;

    // Composite 0..1 "how good is it to live here" score (food/water/wood-led),
    // for settlement choice, migration pressure and the UI.
    float settlementQuality(int regionId) const;

    bool valid(int regionId) const {
        return regionId >= 0 && regionId < (int)pools.size() && pools[regionId].active;
    }
};

extern ResourceSystem g_resources;

#endif // WORLD_RESOURCE_SYSTEM_H
