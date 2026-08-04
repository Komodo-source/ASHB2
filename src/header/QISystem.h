#ifndef QI_SYSTEM_H
#define QI_SYSTEM_H

#include <string>
#include "CivilizationEngine.h"   // for Tribe

// ─────────────────────────────────────────────────────────────────────────────
// What a clever people can do (QI payoff layer)
//
// `Tribe::meanQI` / `Tribe::eliteQI` are recomputed from the living members
// every civ-day by CivilizationEngine::updateEducation. NOTHING outside this
// module may read them to change the world: every effect QI has goes through
// one of the four multipliers below, each applied at exactly ONE read site.
// That is what keeps the feature auditable — the complete list of places
// intelligence changes anything is this list:
//
//   researchMul  TechTree.cpp        research point accrual + unlock cost
//   researchMul  CivilizationEngine  researchClimate() (per-person invention odds)
//   warMul       CivilizationEngine  calculateTribeMilitaryStrength()
//   defenseMul   CivilizationEngine  calculateTribeDefenseStrength(), battle casualties
//   growthMul    CivilizationEngine  food, taxes, culture, child survival, corruption decay
//
// Every multiplier is clamped, and every one returns exactly 1.0f when
// g_liveConfig.qiMul is 0 — the kill switch that reproduces the pre-QI world.
// ─────────────────────────────────────────────────────────────────────────────

namespace QISystem {

    // What a people with no schools at all actually reaches. NOT 100: a child
    // raised on adequate food but taught nothing converges on roughly
    // four-fifths of its inherited ceiling (see Entity::developQI). Centring
    // the multipliers here rather than on 100 is what stops every pre-modern
    // people being handed a permanent penalty for the crime of not having
    // invented the university yet — an unschooled tribe must multiply by 1.0,
    // and everything above that has to be paid for in schooling.
    //
    // The two are DIFFERENT numbers and that matters: the top decile of any
    // distribution sits well above its mean, so scoring both against one
    // constant handed every unschooled tribe a standing war bonus it had not
    // earned. Measured over an unschooled 900-tick world (--set buildMul=0):
    // mean 80.5 (range 74-90), elite 91.5 (range 78-99).
    constexpr float kUnschooledMean  = 80.5f;
    constexpr float kUnschooledElite = 91.5f;

    float researchMul(const Tribe& t);   // thinking — the most QI-elastic thing a society does
    float warMul(const Tribe& t);        // generalship, drill, supply
    float defenseMul(const Tribe& t);    // siegecraft, walls, keeping your own alive
    float growthMul(const Tribe& t);     // broad but shallow: it touches everything

    // How badly this people misjudges an enemy's strength before declaring war.
    // Returns a fractional standard deviation (0 = sees clearly). Falls as the
    // elite gets cleverer, so dull peoples march into wars they cannot win.
    float misjudgement(const Tribe& t);

    // One line for the UI / chronicle, e.g. "QI 94 (elite 112) - 38% schooled".
    std::string summary(const Tribe& t);
}

#endif // QI_SYSTEM_H
