#include "Noise.h"
#include "../header/WorldSeed.h"
#include <cmath>
#include <numeric>

Noise::Noise(uint64_t seed) {
    // Build a 0..255 permutation, shuffled deterministically from the seed.
    uint32_t p[256];
    for (int i = 0; i < 256; ++i) p[i] = (uint32_t)i;

    uint64_t s = splitmix64(seed ? seed : 0x1ull);
    // Fisher-Yates with a seeded LCG-ish stream from splitmix64.
    for (int i = 255; i > 0; --i) {
        s = splitmix64(s);
        int j = (int)(s % (uint64_t)(i + 1));
        uint32_t tmp = p[i]; p[i] = p[j]; p[j] = tmp;
    }
    for (int i = 0; i < 512; ++i) perm[i] = p[i & 255];
}

// Pseudo-random gradient value in [-1,1] at integer lattice point.
float Noise::gradAt(int ix, int iy) const {
    uint32_t h = perm[(ix + perm[iy & 255]) & 255];
    return (h / 255.0f) * 2.0f - 1.0f;
}

static inline float smootherstep(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float Noise::value(float x, float y) const {
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float fx = x - x0, fy = y - y0;

    float v00 = gradAt(x0,     y0);
    float v10 = gradAt(x0 + 1, y0);
    float v01 = gradAt(x0,     y0 + 1);
    float v11 = gradAt(x0 + 1, y0 + 1);

    float ux = smootherstep(fx), uy = smootherstep(fy);
    float a = v00 + ux * (v10 - v00);
    float b = v01 + ux * (v11 - v01);
    return a + uy * (b - a);   // already ~[-1,1]
}

float Noise::fbm(float x, float y, int octaves, float lacunarity, float gain) const {
    float sum = 0.0f, amp = 1.0f, freq = 1.0f, norm = 0.0f;
    for (int o = 0; o < octaves; ++o) {
        sum  += amp * value(x * freq, y * freq);
        norm += amp;
        amp  *= gain;
        freq *= lacunarity;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}
