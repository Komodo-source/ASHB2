#include "Planet.h"
#include "Noise.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <climits>

Planet* g_planet = nullptr;

// ── Generation ──────────────────────────────────────────────────────────────
void Planet::generate(const WorldSeed& seed, int width, int height,
                      float worldWidth, float worldHeight) {
    W = width; H = height;
    worldW = worldWidth; worldH = worldHeight;
    tiles.assign((size_t)W * H, Tile{});

    Noise elevN(makeStream(seed.master, STREAM_TERRAIN, 1)());
    Noise moistN(makeStream(seed.master, STREAM_TERRAIN, 2)());
    Noise warpN (makeStream(seed.master, STREAM_TERRAIN, 3)());

    // Random continental layout per seed: scale & offsets shift the whole map.
    std::mt19937_64 cfg = makeStream(seed.master, STREAM_TERRAIN, 99);
    std::uniform_real_distribution<float> offD(0.0f, 1000.0f);
    std::uniform_real_distribution<float> scaleD(2.6f, 4.4f);
    float ox = offD(cfg), oy = offD(cfg);
    float continentScale = scaleD(cfg);          // higher = more, smaller continents
    // Negative sea level pushes more terrain above water (Earth ~29% land);
    // small per-seed variance keeps ocean/land ratio interesting but always playable.
    float seaLevel = -0.10f + (offD(cfg) / 1000.0f - 0.5f) * 0.14f;

    for (int y = 0; y < H; ++y) {
        float ny = (float)y / (H - 1);            // 0..1 north->south
        // latitude: 0 at poles, 1 at equator
        float lat = 1.0f - std::fabs(ny - 0.5f) * 2.0f;
        for (int x = 0; x < W; ++x) {
            float nx = (float)x / (W - 1);
            float u = nx * continentScale + ox;
            float v = ny * continentScale + oy;

            // domain-warp for less grid-aligned coastlines
            float wx = warpN.fbm(u + 5.2f, v + 1.3f, 3) * 0.5f;
            float wy = warpN.fbm(u + 9.7f, v + 8.1f, 3) * 0.5f;

            float e = elevN.fbm(u + wx, v + wy, 6);   // ~[-1,1]
            // Bias edges of the map downward so continents don't run off-screen.
            float edge = std::min({nx, 1 - nx, ny, 1 - ny});
            float edgeFalloff = std::min(1.0f, edge * 6.0f);
            e = e * edgeFalloff - (1.0f - edgeFalloff) * 0.5f;
            e -= seaLevel;

            float m = (moistN.fbm(u * 1.3f + 12.0f, v * 1.3f + 4.0f, 5) + 1.0f) * 0.5f;

            // temperature: warm equator, cold poles, cooler at altitude
            float t = lat - std::max(0.0f, e) * 0.45f;
            t = std::clamp(t, 0.0f, 1.0f);

            Tile& tl = at(x, y);
            tl.elevation   = std::clamp(e, -1.0f, 1.0f);
            tl.temperature = t;
            tl.moisture    = std::clamp(m, 0.0f, 1.0f);
        }
    }

    assignBiomes();
    carveRivers(seed);
    labelRegions();
    summariseRegions();
}

// ── Biomes (temperature × moisture × elevation lookup) ──────────────────────
void Planet::assignBiomes() {
    for (auto& tl : tiles) {
        float e = tl.elevation, t = tl.temperature, m = tl.moisture;
        if (e < 0.0f) {
            tl.biome = BIOME_OCEAN;
            tl.fertility = 0.0f; tl.oreRichness = 0.0f;
            continue;
        }
        if (e < 0.03f) {                 // shoreline band
            tl.biome = BIOME_COAST;
        } else if (e > 0.62f) {
            tl.biome = (t < 0.25f) ? BIOME_ICE : BIOME_MOUNTAIN;
        } else if (t < 0.18f) {
            tl.biome = BIOME_ICE;
        } else if (t < 0.34f) {
            tl.biome = BIOME_TUNDRA;
        } else if (m < 0.28f) {
            tl.biome = (t > 0.55f) ? BIOME_DESERT : BIOME_GRASSLAND;
        } else if (m < 0.6f) {
            tl.biome = BIOME_GRASSLAND;
        } else {
            tl.biome = (t > 0.62f) ? BIOME_JUNGLE : BIOME_FOREST;
        }

        // fertility & ore by biome
        switch (tl.biome) {
            case BIOME_GRASSLAND: tl.fertility = 0.85f; tl.oreRichness = 0.20f; break;
            case BIOME_FOREST:    tl.fertility = 0.70f; tl.oreRichness = 0.25f; break;
            case BIOME_JUNGLE:    tl.fertility = 0.65f; tl.oreRichness = 0.15f; break;
            case BIOME_COAST:     tl.fertility = 0.75f; tl.oreRichness = 0.10f; break;
            case BIOME_TUNDRA:    tl.fertility = 0.25f; tl.oreRichness = 0.30f; break;
            case BIOME_DESERT:    tl.fertility = 0.10f; tl.oreRichness = 0.45f; break;
            case BIOME_MOUNTAIN:  tl.fertility = 0.15f; tl.oreRichness = 0.90f; break;
            case BIOME_ICE:       tl.fertility = 0.02f; tl.oreRichness = 0.20f; break;
            default:              tl.fertility = 0.0f;  tl.oreRichness = 0.0f;  break;
        }
    }
}

// ── Rivers: trace downhill from wet highlands toward the sea ─────────────────
void Planet::carveRivers(const WorldSeed& seed) {
    std::mt19937_64 r = makeStream(seed.master, STREAM_TERRAIN, 7);
    std::uniform_int_distribution<int> xd(0, W - 1), yd(0, H - 1);

    int sources = std::max(6, (W * H) / 900);
    for (int s = 0; s < sources; ++s) {
        int sx = xd(r), sy = yd(r);
        const Tile& src = at(sx, sy);
        if (!src.isLand() || src.elevation < 0.35f || src.moisture < 0.45f) continue;

        int cx = sx, cy = sy;
        for (int step = 0; step < W + H; ++step) {
            at(cx, cy).river = true;
            at(cx, cy).fertility = std::min(1.0f, at(cx, cy).fertility + 0.15f); // river valleys are fertile
            // move to lowest 8-neighbour
            float best = at(cx, cy).elevation; int bx = cx, by = cy;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    int nx = cx + dx, ny = cy + dy;
                    if (!inBounds(nx, ny)) continue;
                    if (at(nx, ny).elevation < best) { best = at(nx, ny).elevation; bx = nx; by = ny; }
                }
            if (bx == cx && by == cy) break;       // local minimum / lake
            cx = bx; cy = by;
            if (at(cx, cy).elevation < 0.0f) break; // reached the sea
        }
    }
}

// ── Regions: flood-fill contiguous passable-ish land masses ─────────────────
void Planet::labelRegions() {
    const int N = W * H;
    std::vector<int> region(N, -1);
    int next = 0;
    std::vector<int> stack;
    stack.reserve(1024);

    // Flood only over *passable* land (oceans, mountains and ice act as barriers),
    // so a single continent split by a mountain range or desert ice-cap becomes
    // several separate cradles — the real-Earth mechanism for divergent cultures.
    for (int i = 0; i < N; ++i) {
        if (region[i] != -1) continue;
        if (!tiles[i].isPassable()) continue;
        region[i] = next;
        stack.clear();
        stack.push_back(i);
        while (!stack.empty()) {
            int cur = stack.back(); stack.pop_back();
            int cx = cur % W, cy = cur / W;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    int nx = cx + dx, ny = cy + dy;
                    if (!inBounds(nx, ny)) continue;
                    int ni = idx(nx, ny);
                    if (region[ni] != -1) continue;
                    if (!tiles[ni].isPassable()) continue;
                    region[ni] = next;
                    stack.push_back(ni);
                }
        }
        ++next;
    }

    // Attach impassable land (mountains/ice) to a neighbouring region so its
    // resources still belong somewhere, without bridging two cradles.
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int i = idx(x, y);
            if (region[i] != -1 || !tiles[i].isLand()) continue;
            for (int dy = -1; dy <= 1 && region[i] == -1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    if (!inBounds(nx, ny)) continue;
                    if (region[idx(nx, ny)] >= 0) { region[i] = region[idx(nx, ny)]; break; }
                }
        }

    for (int i = 0; i < N; ++i) tiles[i].regionId = region[i];
}

void Planet::summariseRegions() {
    int maxId = -1;
    for (const auto& t : tiles) maxId = std::max(maxId, t.regionId);
    regions.assign(maxId + 1, RegionInfo{});
    for (int id = 0; id <= maxId; ++id) regions[id].id = id;

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const Tile& t = at(x, y);
            if (t.regionId < 0) continue;
            RegionInfo& r = regions[t.regionId];
            r.tileCount++;
            r.centerGX += x; r.centerGY += y;
            r.avgFertility += t.fertility;
            r.avgOre += t.oreRichness;
        }
    for (auto& r : regions) {
        if (r.tileCount > 0) {
            r.centerGX /= r.tileCount;
            r.centerGY /= r.tileCount;
            r.avgFertility /= r.tileCount;
            r.avgOre /= r.tileCount;
            // habitable = a basin big enough to host a starting band with farmable land
            r.habitable = (r.tileCount >= std::max(15, (W * H) / 700)) && r.avgFertility > 0.13f;
        }
    }
}

int Planet::habitableRegionCount() const {
    int c = 0;
    for (const auto& r : regions) if (r.habitable) ++c;
    return c;
}

const RegionInfo* Planet::regionById(int id) const {
    if (id < 0 || id >= (int)regions.size()) return nullptr;
    return &regions[id];
}

std::vector<int> Planet::pickCradles(int k) const {
    std::vector<std::pair<float,int>> ranked;
    for (const auto& r : regions) {
        if (r.tileCount < std::max(8, (W * H) / 900)) continue; // ignore scraps
        float score = r.tileCount * (0.25f + r.avgFertility);
        ranked.push_back({score, r.id});
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b){ return a.first > b.first; });
    std::vector<int> out;
    for (int i = 0; i < (int)ranked.size() && (int)out.size() < k; ++i)
        out.push_back(ranked[i].second);
    return out;
}

std::vector<Planet::Cradle> Planet::pickCradlePoints(int k, std::mt19937_64& rng) const {
    // Candidate pool: fertile, passable land tiles.
    std::vector<int> pool;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const Tile& t = at(x, y);
            if (t.isPassable() && t.fertility > 0.35f) pool.push_back(idx(x, y));
        }
    if (pool.empty())
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (at(x, y).isPassable()) pool.push_back(idx(x, y));

    std::vector<Cradle> out;
    if (pool.empty()) return out;

    // First point: random from the pool.
    std::uniform_int_distribution<int> d(0, (int)pool.size() - 1);
    auto push = [&](int cell){
        out.push_back({ cell % W, cell / W, tiles[cell].regionId });
    };
    push(pool[d(rng)]);

    // Greedy farthest-point sampling for the rest.
    while ((int)out.size() < k) {
        int bestCell = -1; long bestDist = -1;
        for (int cell : pool) {
            int cx = cell % W, cy = cell / W;
            long nearest = LONG_MAX;
            for (const auto& c : out) {
                long dx = cx - c.gx, dy = cy - c.gy;
                long dd = dx * dx + dy * dy;
                if (dd < nearest) nearest = dd;
            }
            if (nearest > bestDist) { bestDist = nearest; bestCell = cell; }
        }
        if (bestCell < 0) break;
        push(bestCell);
        // stop if remaining points would be uselessly close
        if (bestDist < 64) break; // < 8 tiles apart -> not a separate cradle
    }
    return out;
}

bool Planet::randomLandTileInRegion(int regionId, std::mt19937_64& rng,
                                    int& outGX, int& outGY) const {
    // collect passable, reasonably fertile tiles in the region
    std::vector<int> candidates;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const Tile& t = at(x, y);
            if (t.regionId == regionId && t.isPassable() && t.fertility > 0.2f)
                candidates.push_back(idx(x, y));
        }
    if (candidates.empty()) {
        // fall back to any passable tile in the region
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (at(x, y).regionId == regionId && at(x, y).isPassable())
                    candidates.push_back(idx(x, y));
    }
    if (candidates.empty()) return false;
    std::uniform_int_distribution<int> d(0, (int)candidates.size() - 1);
    int i = candidates[d(rng)];
    outGX = i % W; outGY = i / W;
    return true;
}

// ── Coordinate mapping ──────────────────────────────────────────────────────
void Planet::gridToWorld(int gx, int gy, float& wx, float& wy) const {
    wx = (gx + 0.5f) / W * worldW;
    wy = (gy + 0.5f) / H * worldH;
}

void Planet::worldToGrid(float wx, float wy, int& gx, int& gy) const {
    gx = std::clamp((int)(wx / worldW * W), 0, W - 1);
    gy = std::clamp((int)(wy / worldH * H), 0, H - 1);
}

const Tile* Planet::tileAtWorld(float wx, float wy) const {
    if (W == 0 || H == 0) return nullptr;
    int gx, gy; worldToGrid(wx, wy, gx, gy);
    return &at(gx, gy);
}

// ── Replay fingerprint ──────────────────────────────────────────────────────
uint64_t Planet::hash() const {
    uint64_t h = 1469598103934665603ull;
    for (const auto& t : tiles) {
        uint32_t q = (uint32_t)((t.elevation + 1.0f) * 1000) ^ ((uint32_t)t.biome << 20);
        h ^= q; h *= 1099511628211ull;
    }
    return h;
}
