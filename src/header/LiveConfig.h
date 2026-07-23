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
    float corruptionMul = 1.0f;  // leader graft & scandal odds (CivilizationEngine.cpp)

    // ── Emergence upgrade knobs (Steps 4–5). Same contract: ×1.0 = baseline. ──
    float mutationRateMul   = 1.0f;  // genome + NEAT mutation sigma (Genome.h, Neat.h)
    float pheromoneDecayMul = 1.0f;  // field fade speed (PheromoneField::decay)
    float inventionRateMul  = 1.0f;  // item-combination attempt odds (ItemSystem.cpp)
    float neatBrainShare    = 0.0f;  // fraction of newborns wired with a NEAT brain (0=off)

    // ── AI upgrade knobs (phases A-E). Same contract: ×1.0 = new baseline,
    //    0.0 = feature contributes nothing (bit-exact neutral factor). ────────
    float emotionMul  = 1.0f;  // B1 discrete-emotion action tendencies (MindUpgrade.cpp)
    float cultureMul  = 1.0f;  // A3/D4 tribal culture drift + festival cadence (CivilizationEngine.cpp)

    // ── ASHB2 overhaul knobs (epigenetics, disease vectors). Same contract:
    //    ×1.0 = baseline, ×0.0 = feature contributes nothing. ─────────────────
    float epigeneticsMul = 1.0f;  // Phase 6 marker acquisition odds (Entity.cpp, CivilizationEngine.cpp)
    float pathogenMul    = 1.0f;  // Phase 5 pathogen exposure odds + progression (Entity.cpp, main.cpp)
    float bioHomeostasisMul = 1.0f; // Phase 5 starvation/overeating mood-health penalties (Entity.cpp)

    // Overlay toggles (render-only; no simulation effect).
    bool showDensityHeatmap = false;
    bool showPheromones     = false;
    bool showGeneticTint    = false;
};

extern LiveConfig g_liveConfig;

#endif // LIVE_CONFIG_H
