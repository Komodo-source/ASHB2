#ifndef LIVE_CONFIG_H
#define LIVE_CONFIG_H

// M10: live config console — a small set of world tunables the GUI can turn
// while the simulation runs. Every field is a multiplier with default 1.0, and
// each is applied at exactly one read site, so leaving the console untouched
// is bit-identical to not having it (×1.0f is exact in IEEE float): headless
// determinism is unaffected.
struct LiveConfig {
    float moveForceMul  = 1.0f;  // movement force clamp (main.cpp updateMovement)
    float mortalityMul  = 1.0f;  // old-age Gompertz hazard (Entity.cpp IncrementBDay)
    float foodYieldMul  = 1.0f;  // seasonal food modifier (main.cpp updateEnvironment)
    float aggressionMul = 1.0f;  // crime-of-passion odds (implem_free_will.cpp)
};

extern LiveConfig g_liveConfig;

#endif // LIVE_CONFIG_H
