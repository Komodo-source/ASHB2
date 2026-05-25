#ifndef WORLDMAP_H
#define WORLDMAP_H

#include <string>
#include <vector>
#include <random>

class Entity;

enum LocationType {
    LOC_HOME,
    LOC_WORKPLACE,
    LOC_MARKET,
    LOC_PARK,
    LOC_BAR,
    LOC_SCHOOL,
    LOC_HOSPITAL,
    LOC_CHURCH
};

struct WorldLocation {
    int  id;
    LocationType type;
    std::string  name;
    float posX, posY;
    float radius;
    int   capacity;
    unsigned char zoneR, zoneG, zoneB;
};

class WorldMap {
public:
    std::vector<WorldLocation> locations;

    WorldMap() = default;

    // Build all locations for a given entity count
    void initialize(int entityCount);

    // Assign a home and workplace to a freshly-spawned entity
    void assignEntity(Entity* ent);

    // Return the location id where this entity *should* be right now
    int getScheduledLocationId(const Entity* ent, int hour, int dayOfWeek) const;

    const WorldLocation* getLocation(int id) const;
    std::vector<int>     getLocationsByType(LocationType type) const;

private:
    int         nextId = 0;
    std::mt19937 rng{42};

    int addLocation(LocationType type, const std::string& name,
                    float x, float y, float radius, int cap,
                    unsigned char r, unsigned char g, unsigned char b);
};

extern WorldMap* globalWorldMap;

#endif
