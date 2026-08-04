#ifndef LIVE_CONFIG_H
#define LIVE_CONFIG_H

#include <string>

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
    float identityMul = 1.0f;  // I-P1 identity-congruence scorer + purpose buffering (MindUpgrade.cpp); 0 = bit-exact off
    float relationshipMul = 1.0f; // I-P2 couple stickiness + jealousy tempering + forgiveness (implem_free_will.cpp); 0 = bit-exact off
    float moodMul = 1.0f;      // I-P4 hedonic adaptation + coping discharge + post-traumatic drift (MindUpgrade.cpp); 0 = bit-exact off
    float knowledgeMul = 1.0f; // II-P1 collective-brain innovation rate + writing-as-archive (CivilizationEngine.cpp); 0 = bit-exact off
    float feudMul = 1.0f;      // II-P4 persistent grievance ledger — feuds across generations (CivilizationEngine.cpp); 0 = bit-exact off
    float cityMul = 1.0f;      // III-P1 settlement tiers: agglomeration benefits + crowding costs (CivilizationEngine.cpp); 0 = bit-exact off
    float institutionMul = 1.0f; // II-P2 schools/guilds/bureaucracies: archives, teaching, admin capacity (CivilizationEngine.cpp); 0 = bit-exact off
    float legacyMul = 1.0f;    // I-P3 causal legacy: attribution, memorials, inherited reputation (CivilizationEngine.cpp); 0 = bit-exact off
    float tradeMul = 1.0f;     // III-P2 regional prices, trade routes, caravans (CivilizationEngine.cpp); 0 = bit-exact off
    float warMul = 1.0f;       // II-P4 consequential war: unbiased casualties + empires (CivilizationEngine.cpp); 0 = bit-exact off
    float giftMul = 1.0f;      // IV-P4 gift/potlatch, festival effervescence, great works (implem_free_will.cpp/CivilizationEngine.cpp); 0 = bit-exact off
    float languageMul = 1.0f;  // IV-P3 language: intelligibility gates diffusion, trade, diplomacy, assimilation (CivilizationEngine.cpp); 0 = bit-exact off
    float doctrineMul = 1.0f;  // IV-P2 religion with teeth: doctrine scorer, rites, schisms (MindUpgrade.cpp/CivilizationEngine.cpp); 0 = bit-exact off
    float classMul = 1.0f;     // III-P4 heritable class: cultural capital, estates, sticky mobility (SocialOrder.cpp/CivilizationEngine.cpp); 0 = bit-exact off
    float cycleMul = 1.0f;     // II-P3 secular cycles: elite overproduction, immiseration, strife (CivilizationEngine.cpp); 0 = bit-exact off
    float traitMul = 1.0f;     // IV-P1 cultural traits: transmission, 25% tipping point, cultural distance (CivilizationEngine.cpp/Kinship.cpp); 0 = bit-exact off
    float laborMul = 1.0f;     // III-P3 labour market (skills, demand, weak ties, guilds) + demographic transition (CivilizationEngine.cpp/implem_free_will.cpp); 0 = bit-exact off
    float demographyMul = 1.0f;// §8 infant & child mortality — the hump at the bottom of the lifespan curve (Entity.cpp); 0 = bit-exact off
    float buildMul = 1.0f;     // tribes actually constructing buildings (CivilizationEngine.cpp considerConstruction); 0 = bit-exact off, i.e. the old world where Building::build was never called
    float qiMul = 1.0f;        // QI: schooling, research/war/growth payoff of a clever people (Entity.cpp/QISystem.cpp/CivilizationEngine.cpp); 0 = bit-exact off
    float networkMul = 1.0f;   // §8 personal Dunbar layers — role- and temperament-scaled social capacity (main.cpp); 0 = bit-exact off (flat 5-9 cap)

    // ── Phase 6 cognitive-plausibility knobs (plans/cognitive-plausibility.md).
    //    Same contract: ×1.0 = new baseline, 0.0 = bit-exact pre-feature. ─────
    float utilityMul = 1.0f;   // M13 continuous vector utility + per-entity eligibility trace
                               // (VectorUtility.cpp/implem_free_will.cpp); 0 = bit-exact off,
                               // i.e. the tabular string Q-table AND the world-shared trace it
                               // used to run on — the bug is preserved under the switch on
                               // purpose, because that is what "reproduces the old world" means.
    float appraisalMul = 1.0f; // M14 appraisal gates (hard utility masking, tunnel vision,
                               // impulsive discounting) + sleep-time persona drift
                               // (implem_free_will.cpp/Entity.cpp); 0 = bit-exact off
    float memoryLodMul = 1.0f; // M16-P1 semantic-memory working set: bounds the cosine scan and
                               // caches one retrieval per decision (SemanticMemory.cpp/
                               // implem_free_will.cpp); 0 = bit-exact off, i.e. the full
                               // unbounded scan once per candidate action

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

// Set one knob by name, e.g. setLiveConfigKnob("cityMul", 0.0f). Returns false
// for an unknown name. This is what makes the kill switches testable without a
// GUI: `--set cityMul=0` in a headless run must reproduce the pre-feature world
// bit-for-bit, which is the acceptance condition every phase of
// plans/parallel-earth-upgrade.md is held to (§2 rule 2, §10 point 7), and what
// scripts/butterfly.py needs to measure a feature's divergence.
bool setLiveConfigKnob(const std::string& name, float value);

#endif // LIVE_CONFIG_H
