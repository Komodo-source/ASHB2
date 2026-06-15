#ifndef WORLD_NOISE_H
#define WORLD_NOISE_H

#include <cstdint>

// Self-contained, seeded value-noise + fractal Brownian motion (no deps).
// One instance per noise field; deterministic from its seed.
class Noise {
public:
    explicit Noise(uint64_t seed);

    // Single-octave value noise in [-1, 1] for any real (x, y).
    float value(float x, float y) const;

    // Fractal Brownian motion: sum of `octaves` value-noise layers.
    // Returns roughly [-1, 1]. lacunarity ~2.0, gain ~0.5 are classic.
    float fbm(float x, float y, int octaves,
              float lacunarity = 2.0f, float gain = 0.5f) const;

private:
    uint32_t perm[512];               // permutation table (doubled)
    float gradAt(int ix, int iy) const;
};

#endif // WORLD_NOISE_H
