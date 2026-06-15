#ifndef WORLD_PLANET_H
#define WORLD_PLANET_H

#include <vector>
#include <cstdint>
#include "../header/WorldSeed.h"

enum Biome {
    BIOME_OCEAN,
    BIOME_COAST,
    BIOME_ICE,
    BIOME_TUNDRA,
    BIOME_DESERT,
    BIOME_GRASSLAND,
    BIOME_FOREST,
    BIOME_JUNGLE,
    BIOME_MOUNTAIN
};

struct Tile {
    float elevation  = 0.0f;  // -1..1   (<0 = ocean)
    float temperature= 0.0f;  // 0..1    (poles -> equator)
    float moisture   = 0.0f;  // 0..1
    Biome biome      = BIOME_OCEAN;
    float fertility  = 0.0f;  // 0..1   food potential
    float oreRichness= 0.0f;  // 0..1   metal potential
    int   regionId   = -1;    // contiguous land/basin id (-1 = ocean)
    bool  river      = false;

    bool isLand()      const { return elevation >= 0.0f; }
    bool isPassable()  const { return isLand() && biome != BIOME_MOUNTAIN && biome != BIOME_ICE; }
};

struct RegionInfo {
    int   id = -1;
    int   tileCount = 0;
    float centerGX = 0, centerGY = 0;   // grid centroid
    float avgFertility = 0, avgOre = 0;
    bool  habitable = false;            // big enough & has fertile land
};

class Planet {
public:
    int W = 0, H = 0;                   // grid dimensions
    float worldW = 1400.0f, worldH = 1050.0f; // entity pixel-space extent
    std::vector<Tile> tiles;
    std::vector<RegionInfo> regions;

    void generate(const WorldSeed& seed, int width, int height,
                  float worldWidth, float worldHeight);

    const Tile& at(int x, int y) const { return tiles[idx(x, y)]; }
    Tile&       at(int x, int y)       { return tiles[idx(x, y)]; }
    bool        inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
    int         idx(int x, int y)      const { return y * W + x; }

    // entity pixel-space  <->  grid coords
    void gridToWorld(int gx, int gy, float& wx, float& wy) const;
    void worldToGrid(float wx, float wy, int& gx, int& gy) const;

    const Tile* tileAtWorld(float wx, float wy) const;

    // A deterministic 64-bit fingerprint of the generated terrain (for verifying replay).
    uint64_t hash() const;

    int habitableRegionCount() const;
    const RegionInfo* regionById(int id) const;

    // The K best separate cradles to seed starting populations into,
    // ranked by farmable size (tileCount * avgFertility). Returns region ids.
    std::vector<int> pickCradles(int k) const;

    // K well-separated fertile spawn points (grid coords), via farthest-point
    // sampling. Robust to single-continent worlds: isolation then emerges from
    // distance + barriers, like independent civilizations across one landmass.
    struct Cradle { int gx, gy, regionId; };
    std::vector<Cradle> pickCradlePoints(int k, std::mt19937_64& rng) const;

    // Find a fertile, passable land tile near a region's centre (for spawning).
    bool randomLandTileInRegion(int regionId, std::mt19937_64& rng,
                                int& outGX, int& outGY) const;

private:
    void assignBiomes();
    void carveRivers(const WorldSeed& seed);
    void labelRegions();
    void summariseRegions();
};

extern Planet* g_planet;

#endif // WORLD_PLANET_H
