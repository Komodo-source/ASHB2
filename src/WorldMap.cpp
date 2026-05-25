#include "header/WorldMap.h"
#include "header/Entity.h"
#include <algorithm>
#include <cmath>

WorldMap* globalWorldMap = nullptr;

int WorldMap::addLocation(LocationType type, const std::string& name,
                          float x, float y, float radius, int cap,
                          unsigned char r, unsigned char g, unsigned char b) {
    WorldLocation loc;
    loc.id       = nextId++;
    loc.type     = type;
    loc.name     = name;
    loc.posX     = x;
    loc.posY     = y;
    loc.radius   = radius;
    loc.capacity = cap;
    loc.zoneR = r;  loc.zoneG = g;  loc.zoneB = b;
    locations.push_back(loc);
    return loc.id;
}

void WorldMap::initialize(int entityCount) {
    locations.clear();
    nextId = 0;

    // ── Shared civic zones ──────────────────────────────────────────────────
    addLocation(LOC_MARKET,   "Market Square",  700.0f, 490.0f, 115.0f, 999, 200, 160,  60);
    addLocation(LOC_PARK,     "City Park",      270.0f, 760.0f, 145.0f, 999,  60, 160,  80);
    addLocation(LOC_BAR,      "The Tavern",    1120.0f, 790.0f,  90.0f, 999, 140,  60, 160);
    addLocation(LOC_SCHOOL,   "School",         290.0f, 490.0f,  80.0f, 999,  80, 160, 200);
    addLocation(LOC_HOSPITAL, "Hospital",      1180.0f, 460.0f,  75.0f, 999, 220,  80,  80);
    addLocation(LOC_CHURCH,   "Church",         700.0f, 880.0f,  65.0f, 999, 200, 200, 120);

    // ── Homes  (NW cluster: 150-500, 50-380) ───────────────────────────────
    int numHomes = std::max(4, entityCount / 2);
    std::uniform_real_distribution<float> hx(150.0f, 500.0f);
    std::uniform_real_distribution<float> hy( 50.0f, 380.0f);
    for (int i = 0; i < numHomes; ++i)
        addLocation(LOC_HOME, "Home #" + std::to_string(i + 1),
                    hx(rng), hy(rng), 28.0f, 6, 100, 120, 200);

    // ── Workplaces (NE cluster: 880-1300, 50-360) ──────────────────────────
    int numWork = std::max(3, entityCount / 5);
    std::uniform_real_distribution<float> wx(880.0f, 1290.0f);
    std::uniform_real_distribution<float> wy( 50.0f, 360.0f);
    for (int i = 0; i < numWork; ++i)
        addLocation(LOC_WORKPLACE, "Workplace #" + std::to_string(i + 1),
                    wx(rng), wy(rng), 38.0f, 20, 200, 140, 80);
}

void WorldMap::assignEntity(Entity* ent) {
    // Assign home
    auto homes = getLocationsByType(LOC_HOME);
    if (!homes.empty()) {
        std::uniform_int_distribution<int> pick(0, (int)homes.size() - 1);
        ent->homeLocationId = homes[pick(rng)];
    }

    // Assign workplace / school based on life stage
    LifeStage stage = ent->lifeStage;
    if (stage == ADULT || stage == ELDER) {
        auto works = getLocationsByType(LOC_WORKPLACE);
        if (!works.empty()) {
            std::uniform_int_distribution<int> pick(0, (int)works.size() - 1);
            ent->workLocationId = works[pick(rng)];
        }
    } else if (stage == CHILD || stage == ADOLESCENT) {
        auto schools = getLocationsByType(LOC_SCHOOL);
        if (!schools.empty()) ent->workLocationId = schools[0];
    }

    // Start at home
    ent->currentLocationId = (ent->homeLocationId >= 0) ? ent->homeLocationId : 0;
    if (const WorldLocation* loc = getLocation(ent->currentLocationId)) {
        // Place entity near the home
        std::uniform_real_distribution<float> jitter(-20.0f, 20.0f);
        ent->posX = std::max(5.0f, std::min(1395.0f, loc->posX + jitter(rng)));
        ent->posY = std::max(5.0f, std::min(1045.0f, loc->posY + jitter(rng)));
    }
}

int WorldMap::getScheduledLocationId(const Entity* ent, int hour, int dayOfWeek) const {
    const bool isWeekend   = (dayOfWeek >= 5);
    const bool isSleeping  = (hour >= 23 || hour < 6);
    const bool isMorning   = (hour >= 6  && hour < 9);
    const bool isWorkHours = (!isWeekend && hour >= 9  && hour <= 17);
    const bool isEvening   = (hour >= 18 && hour <= 22);

    // Night / early morning → home
    if (isSleeping || isMorning)
        return (ent->homeLocationId >= 0) ? ent->homeLocationId : 0;

    // Emergency: very sick → hospital
    if (ent->entityHealth < 25.0f) {
        auto h = getLocationsByType(LOC_HOSPITAL);
        if (!h.empty()) return h[0];
    }

    // Work / school hours
    if (isWorkHours && ent->workLocationId >= 0)
        return ent->workLocationId;

    const float ext    = ent->personality.extraversion;
    const float stress = ent->entityStress;
    const float lonely = ent->entityLoneliness;
    const float spirit = ent->ValueSystem.spiritualNeed;

    // Evening
    if (isEvening) {
        if (ext > 65.0f) {
            auto bars = getLocationsByType(LOC_BAR);
            if (!bars.empty()) return bars[0];
        }
        if (stress > 65.0f) {
            auto parks = getLocationsByType(LOC_PARK);
            if (!parks.empty()) return parks[0];
        }
        if (lonely > 55.0f) {
            auto markets = getLocationsByType(LOC_MARKET);
            if (!markets.empty()) return markets[0];
        }
        if (spirit > 65.0f) {
            auto churches = getLocationsByType(LOC_CHURCH);
            if (!churches.empty()) return churches[0];
        }
        // Introverts / calm types stay home
        return (ent->homeLocationId >= 0) ? ent->homeLocationId : 0;
    }

    // Daytime free time (weekends / lunch breaks)
    if (lonely > 60.0f) {
        auto m = getLocationsByType(LOC_MARKET);
        if (!m.empty()) return m[0];
    }
    if (stress > 55.0f || ent->personality.openness > 60.0f) {
        auto p = getLocationsByType(LOC_PARK);
        if (!p.empty()) return p[0];
    }
    {
        auto m = getLocationsByType(LOC_MARKET);
        if (!m.empty()) return m[0];
    }

    return (ent->homeLocationId >= 0) ? ent->homeLocationId : 0;
}

const WorldLocation* WorldMap::getLocation(int id) const {
    for (const auto& loc : locations)
        if (loc.id == id) return &loc;
    return nullptr;
}

std::vector<int> WorldMap::getLocationsByType(LocationType type) const {
    std::vector<int> result;
    for (const auto& loc : locations)
        if (loc.type == type) result.push_back(loc.id);
    return result;
}
