#include "Ecosystem.h"
#include "ResourceSystem.h"

#include <algorithm>
#include <cmath>

EcosystemSystem g_ecosystem;

namespace {
// Predator–prey rate constants. Tuned for a slow, stable oscillation rather than
// violent boom/bust, so the food chain is a steady backdrop the economy can lean
// on — yet still collapses under a real drought or sustained overhunting.
constexpr float kPlantGrowth   = 0.10f;  // logistic regrowth rate of vegetation
constexpr float kGrazeRate     = 0.04f;  // how hard herbivores crop the plants
constexpr float kHerbConvert   = 0.45f;  // grazed plants → new herbivores
constexpr float kHerbDeath     = 0.04f;  // baseline herbivore mortality
constexpr float kPredRate      = 0.06f;  // predation pressure on herbivores
constexpr float kPredConvert   = 0.18f;  // kills → new predators
constexpr float kPredDeath     = 0.07f;  // baseline predator mortality
constexpr float kHerbCapFrac   = 0.25f;  // herbivore ceiling as fraction of plantCap

inline float clampMin(float v, float lo) { return v < lo ? lo : v; }
}

void EcosystemSystem::init() {
    regions.clear();
    regions.resize(g_resources.pools.size());
    for (size_t i = 0; i < g_resources.pools.size(); ++i) {
        const ResourcePool& p = g_resources.pools[i];
        RegionEcology& e = regions[i];
        e.active = p.active;
        if (!e.active) continue;
        // Vegetation carrying capacity tracks the land's pristine food ceiling.
        e.plantCap   = clampMin(p.baseCapacity[RES_FOOD], 10.0f);
        e.herbCap    = e.plantCap * kHerbCapFrac;
        // Seed a mature, balanced ecology so hunting works from day one.
        e.plants     = e.plantCap * 0.65f;
        e.herbivores = e.herbCap  * 0.70f;
        e.predators  = e.herbCap  * 0.10f;
    }
}

void EcosystemSystem::update(float seasonalModifier, int day) {
    (void)day;
    float season = std::max(0.2f, seasonalModifier);
    for (RegionEcology& e : regions) {
        if (!e.active || e.plantCap <= 0.0f) continue;

        // ── Vegetation: logistic regrowth (faster in good seasons), then grazed.
        float regrow = kPlantGrowth * season * e.plants * (1.0f - e.plants / e.plantCap);
        // A seed floor lets a wiped-out region slowly green again.
        e.plants += regrow + 0.01f * e.plantCap * season;

        float grazePressure = e.plants / e.plantCap;            // 0..1
        float grazed = kGrazeRate * e.herbivores * grazePressure;
        grazed = std::min(grazed, e.plants);                    // can't eat more than exists
        e.plants = clampMin(e.plants - grazed, 0.0f);
        e.plants = std::min(e.plants, e.plantCap);

        // ── Herbivores: born from grazing, die naturally, starve if grass thin,
        //    and are taken by predators.
        float births = kHerbConvert * grazed;
        float starve = (grazePressure < 0.25f)
                     ? (0.25f - grazePressure) * 0.4f * e.herbivores : 0.0f;
        float predationFrac = e.herbivores / (e.herbivores + e.herbCap * 0.5f + 1.0f);
        float killed = kPredRate * e.predators * predationFrac;
        killed = std::min(killed, e.herbivores * 0.5f);
        e.herbivores += births - kHerbDeath * e.herbivores - starve - killed;
        e.herbivores = clampMin(e.herbivores, 0.0f);

        // ── Predators: born from kills, die out when game is scarce.
        e.predators += kPredConvert * killed - kPredDeath * e.predators;
        e.predators = clampMin(e.predators, 0.0f);
    }
}

float EcosystemSystem::gameAbundance(int regionId) const {
    if (!valid(regionId)) return 1.0f;
    const RegionEcology& e = regions[regionId];
    if (e.herbCap <= 0.0f) return 1.0f;
    float density = e.herbivores / e.herbCap;          // 0 .. ~1+
    return std::max(0.4f, std::min(1.4f, 0.45f + 0.9f * density));
}

float EcosystemSystem::forageAbundance(int regionId) const {
    if (!valid(regionId)) return 1.0f;
    const RegionEcology& e = regions[regionId];
    if (e.plantCap <= 0.0f) return 1.0f;
    float density = e.plants / e.plantCap;             // 0 .. 1
    return std::max(0.5f, std::min(1.3f, 0.6f + 0.7f * density));
}

void EcosystemSystem::huntPressure(int regionId, float taken) {
    if (!valid(regionId) || taken <= 0.0f) return;
    RegionEcology& e = regions[regionId];
    // Each unit of game taken removes roughly one herbivore; never below zero.
    float removed = std::min(e.herbivores, taken);
    e.herbivores  = clampMin(e.herbivores - removed, 0.0f);
    e.totalHunted += removed;
    if (e.herbCap > 0.0f && e.herbivores < e.herbCap * 0.05f) e.collapses++;
}

void EcosystemSystem::foragePressure(int regionId, float taken) {
    if (!valid(regionId) || taken <= 0.0f) return;
    RegionEcology& e = regions[regionId];
    // Foraging crops vegetation lightly (humans take far less than the herds).
    float removed = std::min(e.plants, taken * 0.5f);
    e.plants = clampMin(e.plants - removed, 0.0f);
}

const char* EcosystemSystem::healthLabel(int regionId) const {
    if (!valid(regionId)) return "n/a";
    const RegionEcology& e = regions[regionId];
    float g = (e.herbCap > 0.0f) ? e.herbivores / e.herbCap : 1.0f;
    if (g < 0.08f) return "barren";
    if (g < 0.30f) return "overhunted";
    if (g < 0.70f) return "recovering";
    if (g < 1.10f) return "teeming";
    return "abundant";
}
