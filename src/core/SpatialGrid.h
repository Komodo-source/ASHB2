#pragma once
// ─── SpatialGrid — uniform hash grid for O(1)-ish radius queries ──────────────
// Replaces the O(n²) all-pairs scans in movement, grouping, and perception.
//
// Design constraints (see plans/MASTER_PLAN.md, M4):
//  * Rebuilt from scratch each frame — entities move every frame, incremental
//    updates cost more than a flat rebuild at our scale (≤ ~10k agents).
//  * DETERMINISTIC: cells are a flat std::vector indexed by (cy*cols+cx), and
//    entities are inserted in the caller's iteration order, so forEachInRadius
//    visits candidates in a reproducible order for identical runs.
//  * No allocation per query: query walks the 3×3 (or wider) cell neighborhood
//    in place and invokes a callback.
//
// Usage:
//    SpatialGrid grid(worldW, worldH, /*cellSize=*/130.0f);
//    grid.rebuild(entities);                       // entities: vector<Entity*>
//    grid.forEachInRadius(x, y, r, [&](Entity* e){ ... });

#include <vector>
#include <cmath>
#include <algorithm>

template <typename T>
class SpatialGridT {
public:
    SpatialGridT() = default;

    SpatialGridT(float worldW, float worldH, float cellSize)
    {
        reset(worldW, worldH, cellSize);
    }

    void reset(float worldW, float worldH, float cellSize)
    {
        cell_ = std::max(1.0f, cellSize);
        cols_ = std::max(1, (int)std::ceil(worldW / cell_));
        rows_ = std::max(1, (int)std::ceil(worldH / cell_));
        cells_.assign((size_t)cols_ * rows_, {});
    }

    // Clear and re-insert. Positions are read via T's posX/posY members.
    // Items outside the world bounds are clamped into the border cells, so
    // nothing is ever silently dropped.
    template <typename Container>
    void rebuild(const Container& items)
    {
        for (auto& c : cells_) c.clear();
        for (T* item : items) {
            if (!item) continue;
            cells_[cellIndex(item->posX, item->posY)].push_back(item);
        }
    }

    // Invoke fn(T*) for every item whose center lies within `radius` of (x,y).
    // Iterates cells row-major, items in insertion order → deterministic.
    template <typename Fn>
    void forEachInRadius(float x, float y, float radius, Fn&& fn) const
    {
        const float r2 = radius * radius;
        int cx0, cy0, cx1, cy1;
        cellRange(x - radius, y - radius, x + radius, y + radius, cx0, cy0, cx1, cy1);
        for (int cy = cy0; cy <= cy1; ++cy) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                for (T* item : cells_[(size_t)cy * cols_ + cx]) {
                    float dx = item->posX - x;
                    float dy = item->posY - y;
                    if (dx * dx + dy * dy <= r2) fn(item);
                }
            }
        }
    }

    // Convenience: collect instead of visiting. `out` is appended to.
    void queryRadius(float x, float y, float radius, std::vector<T*>& out) const
    {
        forEachInRadius(x, y, radius, [&](T* item) { out.push_back(item); });
    }

    int   cols() const { return cols_; }
    int   rows() const { return rows_; }
    float cellSize() const { return cell_; }

private:
    size_t cellIndex(float x, float y) const
    {
        int cx = clampi((int)(x / cell_), 0, cols_ - 1);
        int cy = clampi((int)(y / cell_), 0, rows_ - 1);
        return (size_t)cy * cols_ + cx;
    }

    void cellRange(float x0, float y0, float x1, float y1,
                   int& cx0, int& cy0, int& cx1, int& cy1) const
    {
        cx0 = clampi((int)(x0 / cell_), 0, cols_ - 1);
        cy0 = clampi((int)(y0 / cell_), 0, rows_ - 1);
        cx1 = clampi((int)(x1 / cell_), 0, cols_ - 1);
        cy1 = clampi((int)(y1 / cell_), 0, rows_ - 1);
    }

    static int clampi(int v, int lo, int hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    float cell_ = 130.0f;
    int   cols_ = 1;
    int   rows_ = 1;
    std::vector<std::vector<T*>> cells_;
};

class Entity;
using SpatialGrid = SpatialGridT<Entity>;
