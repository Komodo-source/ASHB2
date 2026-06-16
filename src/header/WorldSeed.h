#ifndef WORLD_SEED_H
#define WORLD_SEED_H

#include <cstdint>
#include <random>
#include <string>

// ── Master seed & deterministic RNG sub-streams ─────────────────────────────
// Everything random in the simulation must draw from one of these streams so
// that a given (master seed + DivergenceConfig) reproduces the same history,
// while a different seed produces a wildly different one.

// Named sub-streams so independent systems don't fight over a single generator.
enum RngStream : uint64_t {
    STREAM_TERRAIN   = 0x9E3779B97F4A7C15ull,
    STREAM_CULTURE   = 0xC2B2AE3D27D4EB4Full,
    STREAM_INNOV     = 0x165667B19E3779F9ull,
    STREAM_DISEASE   = 0xD6E8FEB86659FD93ull,
    STREAM_NAMES     = 0xA0761D6478BD642Full,
    STREAM_SPAWN     = 0xE7037ED1A0B428DBull,
    STREAM_MIGRATION = 0x8EBC6AF09C88C6E3ull
};

// How "wild" each run is. Same seed + same config = identical history.
struct DivergenceConfig {
    float butterfly        = 1.0f; // 0 = tame, 2 = chaotic: scales per-tick perturbations
    float innovationLuck   = 1.0f; // multiplies discovery probability
    float catastropheRate  = 1.0f; // multiplies plague/famine/collapse odds
    float migrationPressure = 1.0f; // multiplies split/migrate thresholds (inverse)
};

struct WorldSeed {
    uint64_t master = 0;
    DivergenceConfig divergence;

    // Parse a user-typed seed: numeric stays numeric, text is hashed.
    static WorldSeed fromString(const std::string& s);
    static uint64_t  hashString(const std::string& s);
};

// splitmix64 — mixes master seed with a stream salt into a well-distributed seed.
inline uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

// Build an independent generator for a given stream (optionally per-region/entity).
inline std::mt19937_64 makeStream(uint64_t master, uint64_t salt, uint64_t index = 0) {
    uint64_t s = splitmix64(master ^ splitmix64(salt + 0x9E3779B97F4A7C15ull * index));
    return std::mt19937_64(s);
}

// Global world seed, set once at startup before any generation.
extern WorldSeed g_worldSeed;

// Deterministic per-object seed source. Replaces std::random_device for any
// generator that must be reproducible (per-entity cognitive RNGs, etc.).
// Construction order is fixed for a given world seed + config, so a monotonic
// counter mixed with the master seed yields a distinct-yet-replayable seed for
// each caller. Same seed → identical history; different instances → independent
// streams (so every entity isn't making the same "random" choice).
// NOTE: assumes generators are constructed on a single thread (true for the
// simulation/agent loop); concurrent construction would lose determinism.
inline uint64_t nextDeterministicSeed(uint64_t salt = 0) {
    static uint64_t counter = 0;
    return splitmix64(g_worldSeed.master ^ splitmix64(salt + 0x9E3779B97F4A7C15ull * (++counter)));
}

#endif // WORLD_SEED_H
