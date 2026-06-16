#include "ResourceSystem.h"
#include "Planet.h"
#include <algorithm>
#include <cmath>

ResourceSystem g_resources;

const char* resourceName(ResourceType t) {
    switch (t) {
        case RES_FOOD:  return "Food";
        case RES_WOOD:  return "Wood";
        case RES_STONE: return "Stone";
        case RES_METAL: return "Metal";
        case RES_WATER: return "Water";
        case RES_HERBS: return "Herbs";
        default:        return "?";
    }
}

namespace {
// How much one tile contributes to each resource ceiling, given its geography.
// Returns absolute "stock units" so a big fertile river basin dwarfs a thin
// desert strip.
void tilePotential(const Tile& t, std::array<float, RES_COUNT>& out) {
    const float riverBonus = t.river ? 1.0f : 0.0f;

    // Food: fertility is the spine; rivers irrigate; grassland/jungle are kind.
    float food = t.fertility;
    if (t.biome == BIOME_GRASSLAND) food += 0.25f;
    if (t.biome == BIOME_JUNGLE)    food += 0.15f;
    food += riverBonus * 0.35f;

    // Wood: timber comes from trees.
    float wood = 0.0f;
    if (t.biome == BIOME_FOREST) wood = 1.0f;
    else if (t.biome == BIOME_JUNGLE) wood = 0.8f;
    else if (t.biome == BIOME_GRASSLAND) wood = 0.15f;

    // Stone: bare rock and high ground.
    float stone = 0.0f;
    if (t.biome == BIOME_MOUNTAIN) stone = 1.0f;
    else if (t.elevation > 0.6f) stone = 0.5f;
    else if (t.biome == BIOME_COAST) stone = 0.2f;

    // Metal: ore-bearing rock.
    float metal = t.oreRichness;

    // Water: rivers first, then ambient moisture and coastal access.
    float water = riverBonus + t.moisture * 0.5f;
    if (t.biome == BIOME_COAST) water += 0.4f;

    // Herbs: damp leafy ground.
    float herbs = 0.0f;
    if (t.biome == BIOME_JUNGLE) herbs = 0.9f;
    else if (t.biome == BIOME_FOREST) herbs = 0.6f;
    else if (t.biome == BIOME_GRASSLAND) herbs = 0.4f;
    herbs *= (0.5f + t.moisture * 0.5f);

    out[RES_FOOD]  += std::max(0.0f, food);
    out[RES_WOOD]  += wood;
    out[RES_STONE] += stone;
    out[RES_METAL] += metal;
    out[RES_WATER] += water;
    out[RES_HERBS] += herbs;
}

// Fraction of a resource's ceiling that regenerates per day. Crops and herbs
// regrow with the seasons; forests far slower; rivers refill fast; rock and ore
// are effectively non-renewable on a human timescale.
constexpr float kRegenFrac[RES_COUNT] = {
    /*FOOD */ 0.06f,
    /*WOOD */ 0.012f,
    /*STONE*/ 0.0015f,
    /*METAL*/ 0.0010f,
    /*WATER*/ 0.20f,
    /*HERBS*/ 0.05f,
};
// Each tile-unit of potential becomes this many stock units of ceiling.
constexpr float kCapacityScale = 60.0f;
} // namespace

void ResourceSystem::init(const Planet& planet) {
    pools.assign(planet.regions.size(), ResourcePool{});

    // Accumulate each tile's potential into its region.
    for (const Tile& t : planet.tiles) {
        if (t.regionId < 0 || t.regionId >= (int)pools.size()) continue;
        ResourcePool& p = pools[t.regionId];
        tilePotential(t, p.baseCapacity);
        p.tileCount++;
    }

    for (auto& p : pools) {
        p.active = p.tileCount > 0;
        for (int r = 0; r < RES_COUNT; ++r) {
            p.baseCapacity[r] *= kCapacityScale;
            p.capacity[r] = p.baseCapacity[r];
            p.stock[r]    = p.baseCapacity[r];                 // start pristine
            p.regen[r]    = p.baseCapacity[r] * kRegenFrac[r]; // absolute / day
        }
    }
}

void ResourceSystem::update(float seasonalModifier) {
    const float season = std::max(0.2f, seasonalModifier);
    for (auto& p : pools) {
        if (!p.active) continue;
        for (int r = 0; r < RES_COUNT; ++r) {
            if (p.capacity[r] <= 0.0f) continue;
            // Living resources track the season; rock/metal/water do not.
            float mod = (r == RES_FOOD || r == RES_HERBS || r == RES_WOOD) ? season : 1.0f;
            p.stock[r] = std::min(p.capacity[r], p.stock[r] + p.regen[r] * mod);

            // Land degradation: a chronically stripped region loses ceiling
            // (eroded soil, clear-cut hills); a rested one heals back.
            float fill = p.stock[r] / std::max(1.0f, p.capacity[r]);
            if (fill < 0.12f) {
                float floor = p.baseCapacity[r] * 0.40f;       // never collapses fully
                p.capacity[r] = std::max(floor, p.capacity[r] - p.baseCapacity[r] * 0.0008f);
            } else if (fill > 0.60f && p.capacity[r] < p.baseCapacity[r]) {
                p.capacity[r] = std::min(p.baseCapacity[r], p.capacity[r] + p.baseCapacity[r] * 0.0004f);
            }
        }
    }
}

float ResourceSystem::extract(int regionId, ResourceType type, float requested) {
    if (!valid(regionId) || requested <= 0.0f) return 0.0f;
    float& s = pools[regionId].stock[type];
    float taken = std::min(s, requested);
    s -= taken;
    return taken;
}

float ResourceSystem::abundance(int regionId, ResourceType type) const {
    if (!valid(regionId)) return 1.0f;  // unknown region: neutral, never punish
    const ResourcePool& p = pools[regionId];
    if (p.capacity[type] <= 0.0f) return 0.6f;
    float fill = p.stock[type] / p.capacity[type];      // 0..1
    return std::clamp(0.5f + fill, 0.6f, 1.3f);
}

float ResourceSystem::settlementQuality(int regionId) const {
    if (!valid(regionId)) return 0.0f;
    const ResourcePool& p = pools[regionId];
    // Food and water are existential; wood enables shelter/tools. Normalised by
    // a generous reference so only genuinely rich land scores near 1.
    auto fillOf = [&](ResourceType r) {
        return p.capacity[r] > 0.0f ? std::min(1.0f, p.stock[r] / p.capacity[r]) : 0.0f;
    };
    auto richOf = [&](ResourceType r) {
        return std::min(1.0f, p.baseCapacity[r] / (p.tileCount * kCapacityScale * 0.5f + 1.0f));
    };
    float food  = 0.5f * fillOf(RES_FOOD)  + 0.5f * richOf(RES_FOOD);
    float water = 0.5f * fillOf(RES_WATER) + 0.5f * richOf(RES_WATER);
    float wood  = richOf(RES_WOOD);
    return std::clamp(0.45f * food + 0.35f * water + 0.20f * wood, 0.0f, 1.0f);
}
