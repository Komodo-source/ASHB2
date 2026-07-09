#ifndef EPISODIC_MAP_H
#define EPISODIC_MAP_H

// ─── EpisodicMap — per-agent spatial event memory (Upgrade Plan, Step 3) ──────
// A compact, capped log of *where* things happened: food found here, danger
// seen there. Complements SemanticMemorySystem (which remembers *what* happened
// emotionally, with embeddings) with the spatial layer the movement/GOAP code
// needs: "walk back to the berry patch", "don't cross the battlefield".
//
// Deterministic and allocation-light: a fixed-cap vector, decayed lazily when
// queried or when new memories are recorded. Strength decays exponentially per
// simulation day; memories below the floor are dropped. On overflow the weakest
// memory is evicted, so vivid recent events displace stale ones.

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdint>

struct EpisodicNode {
    float       x = 0.0f, y = 0.0f;
    uint8_t     kind = 0;          // EpisodicMap::FOOD / DANGER / SOCIAL / RESOURCE
    int         day = 0;           // simulation day the event happened
    float       strength = 1.0f;   // decays toward 0; eviction priority
};

class EpisodicMap {
public:
    enum Kind : uint8_t { FOOD = 0, DANGER = 1, SOCIAL = 2, RESOURCE = 3 };

    static constexpr int   CAP          = 48;
    static constexpr float DECAY_PER_DAY = 0.985f;  // ~50% strength after ~46 days
    static constexpr float FLOOR         = 0.06f;   // below this the memory is gone

    void remember(float x, float y, Kind kind, int day, float strength = 1.0f) {
        decayTo(day);
        // Same-kind memory within merge radius: reinforce instead of duplicating,
        // so a daily berry patch is one strong node, not 48 clones.
        for (EpisodicNode& n : nodes_) {
            if (n.kind != kind) continue;
            float dx = n.x - x, dy = n.y - y;
            if (dx*dx + dy*dy < 60.0f * 60.0f) {
                n.x = (n.x + x) * 0.5f; n.y = (n.y + y) * 0.5f;
                n.day = day;
                n.strength = std::min(2.0f, n.strength + strength * 0.5f);
                return;
            }
        }
        if ((int)nodes_.size() >= CAP) {
            auto weakest = std::min_element(nodes_.begin(), nodes_.end(),
                [](const EpisodicNode& a, const EpisodicNode& b){ return a.strength < b.strength; });
            *weakest = EpisodicNode{x, y, (uint8_t)kind, day, strength};
            return;
        }
        nodes_.push_back(EpisodicNode{x, y, (uint8_t)kind, day, strength});
    }

    // Strongest memory of `kind`, scored by strength / (1 + distance/300).
    // Returns nullptr when nothing relevant is remembered.
    const EpisodicNode* recallBest(Kind kind, float fromX, float fromY, int today) {
        decayTo(today);
        const EpisodicNode* best = nullptr;
        float bestScore = 0.0f;
        for (const EpisodicNode& n : nodes_) {
            if (n.kind != kind) continue;
            float d = std::sqrt((n.x-fromX)*(n.x-fromX) + (n.y-fromY)*(n.y-fromY));
            float score = n.strength / (1.0f + d / 300.0f);
            if (score > bestScore) { bestScore = score; best = &n; }
        }
        return best;
    }

    // Summed danger weight within `radius` of a point — path scoring uses this
    // to steer around remembered battlefields / kill sites.
    float dangerAt(float x, float y, float radius, int today) {
        decayTo(today);
        float sum = 0.0f;
        const float r2 = radius * radius;
        for (const EpisodicNode& n : nodes_) {
            if (n.kind != DANGER) continue;
            float dx = n.x - x, dy = n.y - y;
            if (dx*dx + dy*dy <= r2) sum += n.strength;
        }
        return sum;
    }

    const std::vector<EpisodicNode>& nodes() const { return nodes_; }
    void clear() { nodes_.clear(); lastDecayDay_ = 0; }

    // Serialization hooks (SaveLoad v2 section).
    void setNodes(std::vector<EpisodicNode> n, int day) { nodes_ = std::move(n); lastDecayDay_ = day; }
    int  lastDecayDay() const { return lastDecayDay_; }

private:
    void decayTo(int day) {
        if (day <= lastDecayDay_) return;
        float f = std::pow(DECAY_PER_DAY, (float)(day - lastDecayDay_));
        lastDecayDay_ = day;
        for (EpisodicNode& n : nodes_) n.strength *= f;
        nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
                     [](const EpisodicNode& n){ return n.strength < FLOOR; }),
                     nodes_.end());
    }

    std::vector<EpisodicNode> nodes_;
    int lastDecayDay_ = 0;
};

#endif // EPISODIC_MAP_H
