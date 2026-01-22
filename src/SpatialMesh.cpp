#include "header/SpatialMesh.h"
#include "header/UI.h"
#include "header/Entity.h"
#include <cmath>

// Implémentation de EntityPosition
EntityPosition::EntityPosition(Entity* entity, float x, float y)
    : entity(entity), x(x), y(y) {}

// Implémentation de AABB
AABB::AABB(float x, float y, float halfWidth, float halfHeight)
    : x(x), y(y), halfWidth(halfWidth), halfHeight(halfHeight) {}

bool AABB::contains(const EntityPosition& entityPos) const {
    return (entityPos.x >= x - halfWidth &&
            entityPos.x <= x + halfWidth &&
            entityPos.y >= y - halfHeight &&
            entityPos.y <= y + halfHeight);
}

bool AABB::intersects(const AABB& other) const {
    return !(x + halfWidth < other.x - other.halfWidth ||
             x - halfWidth > other.x + other.halfWidth ||
             y + halfHeight < other.y - other.halfHeight ||
             y - halfHeight > other.y + other.halfHeight);
}

// Implémentation de QuadTree
QuadTree::QuadTree(const AABB& boundary, int depth)
    : boundary(boundary), depth(depth), divided(false) {
    entities.reserve(MAX_CAPACITY);
}

void QuadTree::subdivide() {
    float x = boundary.x;
    float y = boundary.y;
    float hw = boundary.halfWidth / 2;
    float hh = boundary.halfHeight / 2;

    northWest = std::make_unique<QuadTree>(AABB(x - hw, y - hh, hw, hh), depth + 1);
    northEast = std::make_unique<QuadTree>(AABB(x + hw, y - hh, hw, hh), depth + 1);
    southWest = std::make_unique<QuadTree>(AABB(x - hw, y + hh, hw, hh), depth + 1);
    southEast = std::make_unique<QuadTree>(AABB(x + hw, y + hh, hw, hh), depth + 1);

    divided = true;
}

bool QuadTree::insert(EntityPosition* entityPos) {
    if (!boundary.contains(*entityPos)) {
        return false;
    }

    if (entities.size() < MAX_CAPACITY || depth >= MAX_DEPTH) {
        entities.push_back(entityPos);
        return true;
    }

    if (!divided) {
        subdivide();
    }

    if (northWest->insert(entityPos)) return true;
    if (northEast->insert(entityPos)) return true;
    if (southWest->insert(entityPos)) return true;
    if (southEast->insert(entityPos)) return true;

    return false;
}

void QuadTree::query(const AABB& range, std::vector<EntityPosition*>& found) const {
    if (!boundary.intersects(range)) {
        return;
    }

    for (EntityPosition* entityPos : entities) {
        if (range.contains(*entityPos)) {
            found.push_back(entityPos);
        }
    }

    if (divided) {
        northWest->query(range, found);
        northEast->query(range, found);
        southWest->query(range, found);
        southEast->query(range, found);
    }
}

void QuadTree::queryRadius(float x, float y, float radius, std::vector<EntityPosition*>& found) const {
    AABB range(x, y, radius, radius);

    if (!boundary.intersects(range)) {
        return;
    }

    float radiusSquared = radius * radius;
    for (EntityPosition* entityPos : entities) {
        float dx = entityPos->x - x;
        float dy = entityPos->y - y;
        if (dx * dx + dy * dy <= radiusSquared) {
            found.push_back(entityPos);
        }
    }

    if (divided) {
        northWest->queryRadius(x, y, radius, found);
        northEast->queryRadius(x, y, radius, found);
        southWest->queryRadius(x, y, radius, found);
        southEast->queryRadius(x, y, radius, found);
    }
}

void QuadTree::clear() {
    entities.clear();
    if (divided) {
        northWest.reset();
        northEast.reset();
        southWest.reset();
        southEast.reset();
        divided = false;
    }
}

// Fonction principale

std::vector<std::vector<Entity*>> getCloseEntityGroups(
    const std::vector<Entity*>& entities,
    float worldWidth,
    float worldHeight,
    float interactionRadius)
{
    // 1. Prepare data
    std::vector<EntityPosition> entityPositions;
    entityPositions.reserve(entities.size());

    for (Entity* entity : entities) {
        if (entity != nullptr) {
            entityPositions.emplace_back(entity, entity->posX, entity->posY);
        }
    }

    // 2. Build QuadTree
    QuadTree tree(AABB(worldWidth / 2, worldHeight / 2, worldWidth / 2, worldHeight / 2));
    for (auto& entityPos : entityPositions) {
        tree.insert(&entityPos);
    }

    std::vector<std::vector<Entity*>> groups;
    std::vector<bool> processed(entityPositions.size(), false);

    // 3. Grouping using BFS (Flood Fill)
    for (size_t i = 0; i < entityPositions.size(); ++i) {
        if (processed[i]) continue;

        // Start a new group
        std::vector<Entity*> currentGroup;
        std::vector<size_t> queue;

        queue.push_back(i);
        processed[i] = true;

        size_t head = 0;
        while(head < queue.size()){
            size_t currentIdx = queue[head++];
            EntityPosition& currentEnt = entityPositions[currentIdx];
            currentGroup.push_back(currentEnt.entity);

            // Find all neighbors
            std::vector<EntityPosition*> neighbors;
            tree.queryRadius(currentEnt.x, currentEnt.y, interactionRadius, neighbors);

            for (EntityPosition* neighborPtr : neighbors) {
                // Calculate index safely
                size_t neighborIdx = neighborPtr - &entityPositions[0];

                if (!processed[neighborIdx]) {
                    processed[neighborIdx] = true;
                    queue.push_back(neighborIdx);
                }
            }
        }

        if (!currentGroup.empty()) {
            groups.push_back(currentGroup);
        }
    }

    return groups;
}
