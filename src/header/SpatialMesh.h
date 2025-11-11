#ifndef QUADTREE_H
#define QUADTREE_H

#include <vector>
#include <memory>
#include "imgui.h"
#include "UI.h"
#include "Entity.h"

// Forward declaration de ta classe Entity
//class Entity;

// Structure wrapper pour stocker position avec Entity
struct EntityPosition {
    Entity* entity;
    float x, y;

    EntityPosition(Entity* entity, float x, float y);
};

// Axis-Aligned Bounding Box
struct AABB {
    float x, y;
    float halfWidth, halfHeight;

    AABB(float x, float y, float halfWidth, float halfHeight);
    bool contains(const EntityPosition& entityPos) const;
    bool intersects(const AABB& other) const;
};

// QuadTree Class
class QuadTree {
private:
    static const int MAX_CAPACITY = 4;
    static const int MAX_DEPTH = 8;

    AABB boundary;
    std::vector<EntityPosition*> entities;
    int depth;

    std::unique_ptr<QuadTree> northWest;
    std::unique_ptr<QuadTree> northEast;
    std::unique_ptr<QuadTree> southWest;
    std::unique_ptr<QuadTree> southEast;

    bool divided;

    void subdivide();

public:
    QuadTree(const AABB& boundary, int depth = 0);

    bool insert(EntityPosition* entityPos);
    void query(const AABB& range, std::vector<EntityPosition*>& found) const;
    void queryRadius(float x, float y, float radius, std::vector<EntityPosition*>& found) const;
    void clear();
};

// Fonction principale pour obtenir les groupes d'entités proches
std::vector<std::vector<Entity*>> getCloseEntityGroups(
    const std::vector<UI::GridPoint>& gridPoints,
    const std::vector<Entity*>& entities,
    float worldWidth,
    float worldHeight,
    float interactionRadius
);

#endif // QUADTREE_H
