#ifndef PHEROMONE_FIELD_H
#define PHEROMONE_FIELD_H

// ─── PheromoneField — environmental stigmergy (Upgrade Plan, Step 4) ──────────
// A coarse multi-channel scalar field over the world. Agents DEPOSIT when
// something notable happens where they stand (found food, died violently,
// socialized); the field DECAYS every simulation day; other agents SAMPLE the
// gradient and let it bias their movement. Unlike the legacy agent-borne
// PheromoneRelease (which vanishes with its carrier), field deposits persist
// in the environment after the agent leaves — true stigmergy.
//
// Determinism: plain arrays mutated in caller iteration order, no RNG.

#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>

class PheromoneField {
public:
    enum Channel { FOOD = 0, DANGER = 1, SOCIAL = 2, CHANNELS = 3 };

    void reset(float worldW, float worldH, float cellSize = 40.0f) {
        cell_ = std::max(8.0f, cellSize);
        cols_ = std::max(1, (int)std::ceil(worldW / cell_));
        rows_ = std::max(1, (int)std::ceil(worldH / cell_));
        for (auto& g : grid_) g.assign((size_t)cols_ * rows_, 0.0f);
        ready_ = true;
    }
    bool ready() const { return ready_; }

    void deposit(float x, float y, Channel ch, float amount) {
        if (!ready_ || amount <= 0.0f) return;
        float& v = grid_[ch][idx(x, y)];
        v = std::min(100.0f, v + amount);
    }

    float sample(float x, float y, Channel ch) const {
        if (!ready_) return 0.0f;
        return grid_[ch][idx(x, y)];
    }

    // Central-difference gradient (points toward higher concentration).
    void gradient(float x, float y, Channel ch, float& gx, float& gy) const {
        gx = gy = 0.0f;
        if (!ready_) return;
        gx = sample(x + cell_, y, ch) - sample(x - cell_, y, ch);
        gy = sample(x, y + cell_, ch) - sample(x, y - cell_, ch);
    }

    // Exponential decay, once per simulation day. `decayMul` is the live
    // UI knob (LiveConfig::pheromoneDecayMul); 1.0 = baseline half-life.
    void decay(float decayMul) {
        if (!ready_) return;
        // Baseline retention per day per channel: food fades fastest (sources
        // move), danger lingers (battlefields are remembered), social medium.
        static const float kRetain[CHANNELS] = { 0.90f, 0.96f, 0.93f };
        for (int ch = 0; ch < CHANNELS; ++ch) {
            float r = std::pow(kRetain[ch], std::max(0.05f, decayMul));
            for (float& v : grid_[ch]) {
                v *= r;
                if (v < 0.01f) v = 0.0f;
            }
        }
    }

    int   cols() const { return cols_; }
    int   rows() const { return rows_; }
    float cellSize() const { return cell_; }
    float cellValue(int cx, int cy, Channel ch) const {
        return grid_[ch][(size_t)cy * cols_ + cx];
    }

    void saveTo(std::ofstream& f) const;
    void loadFrom(std::ifstream& f);

private:
    size_t idx(float x, float y) const {
        int cx = std::min(cols_ - 1, std::max(0, (int)(x / cell_)));
        int cy = std::min(rows_ - 1, std::max(0, (int)(y / cell_)));
        return (size_t)cy * cols_ + cx;
    }

    float cell_ = 40.0f;
    int   cols_ = 1, rows_ = 1;
    bool  ready_ = false;
    std::vector<float> grid_[CHANNELS];
};

extern PheromoneField g_pheromoneField;

#endif // PHEROMONE_FIELD_H
