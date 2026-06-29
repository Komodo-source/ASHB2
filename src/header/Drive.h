#ifndef DRIVE_H
#define DRIVE_H

#include <string>
#include <map>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Drive — a single bipolar homeostatic / novelty axis.
//
//  Every drive is a value in [0,1] with a lethal floor AND a lethal ceiling.
//  Too little kills (deficiency); too much kills (excess / overload). Between
//  the two deaths sits a comfort band around a setpoint where wellbeing lives,
//  and the entity's whole behavioural pressure is the signed error that pushes
//  it back toward that band:
//
//        0 ──────▶  setpoint  ◀────── 1
//      death                        death
//
//  Two flavours of axis:
//    • Homeostatic (body): entropy pushes the value toward one pole (hunger
//      rises, fatigue rises, warmth bleeds away). The agent acts to pull it
//      back to the setpoint. Stability is the goal.
//    • Novelty (mind): entropy lets the value decay toward deprivation
//      (boredom, anhedonia). The agent acts to push it up — but pushing it up
//      breeds tolerance (habituation), so the bar keeps rising and the same
//      input stops reaching comfort. Stimulation is the goal; dependency is the
//      trap.
//
//  Two slow accumulators give the "short-term fine, sustained kills" texture
//  that a bare clamp can't:
//    • load      — allostatic wear from time spent in the danger zone (between
//                  the comfort edge and the lethal edge). Chronic anger /
//                  chronic hunger damage even before the hard threshold, and
//                  the wear lingers after the value returns to comfort.
//    • tolerance — habituation on novelty axes. Rises while the value is held
//                  high, decays during abstinence; it drags the effective
//                  setpoint toward the high pole (you need more just to feel
//                  normal) and erodes the payoff of further satisfaction.
// ─────────────────────────────────────────────────────────────────────────────

enum class DriveAxis : unsigned char {
    Homeostatic,   // body wants a stable setpoint
    Novelty        // mind wants to be pushed off-centre; habituates
};

struct Drive {
    DriveAxis axis = DriveAxis::Homeostatic;

    // ── live state ───────────────────────────────────────────────────────────
    float value     = 0.5f;  // current position on the axis [0,1]
    float load      = 0.0f;  // accumulated allostatic wear [0,1]
    float tolerance = 0.0f;  // habituation (novelty axis only) [0,1]

    // ── shape: where the sweet spot and the two deaths sit ─────────────────────
    float setpoint  = 0.5f;  // wellbeing-maximising value (before tolerance)
    float comfort   = 0.18f; // half-width of the safe band around the setpoint
    float deathLow  = 0.0f;  // at/below this, acute deficiency damage
    float deathHigh = 1.0f;  // at/above this, acute excess damage

    // ── dynamics ───────────────────────────────────────────────────────────────
    float driftRate = 0.01f; // per-tick entropy magnitude
    float driftPole = 1.0f;  // pole entropy pushes 'value' toward (0 or 1)

    // ── damage / accumulation tuning (in health-points per tick at the limit) ──
    float acuteDamage   = 0.20f;  // health/tick at a lethal pole, escalating past it
    float chronicDamage = 0.04f;  // health/tick at full allostatic load
    float loadGain      = 0.020f; // load accrued per tick, deep in the danger zone
    float loadRelief    = 0.010f; // load shed per tick while sitting in comfort
    float tolGain       = 0.015f; // habituation growth while over-stimulated
    float tolDecay      = 0.004f; // resensitisation per tick while deprived

    static float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

    // Tolerance drags the effective setpoint toward the high pole: the addict
    // needs more just to feel "normal".
    float effectiveSetpoint() const {
        if (axis != DriveAxis::Novelty) return setpoint;
        return std::min(0.98f, setpoint + tolerance * (deathHigh - setpoint));
    }

    // Signed urgency in [-1,1]. Positive => value sits too LOW, act to raise it;
    // negative => too HIGH, act to lower it. Magnitude explodes near a lethal
    // pole. This is the behaviour generator the planner reads.
    float pressure() const;

    // 0 (at a lethal pole / fully worn) .. 1 (sitting happily at the setpoint).
    float wellbeing() const;

    // Advance one tick. Returns health damage this tick (>= 0, in health points).
    // applyEntropy=false lets an external owner keep moving 'value' itself while
    // the drive still accrues load / tolerance and reports damage (bridge mode).
    float tick(float dt = 1.0f, bool applyEntropy = true);

    // An action nudges the value; +amount pushes toward the high pole. On a
    // novelty axis a satisfying spike also feeds tolerance (the treadmill).
    void satisfy(float amount);
};

// A named bundle of drives hung off an entity.
struct DriveSet {
    std::map<std::string, Drive> axes;

    Drive&       operator[](const std::string& k)       { return axes[k]; }
    bool         has(const std::string& k)        const { return axes.count(k) != 0; }

    // Tick every drive; return summed health damage.
    float tick(float dt = 1.0f, bool applyEntropy = true);

    // Name of the most urgent drive (largest |pressure|); "" if empty.
    std::string mostUrgent() const;

    // Mean wellbeing across axes [0,1].
    float wellbeing() const;

    // Build the standard human axis set with personality-derived shape.
    //   openness          widens the mind's appetite for stimulation
    //   extraversion      raises the social setpoint (needs more company)
    //   neuroticism       narrows comfort bands (less tolerant of deviation)
    //   conscientiousness slightly slows bodily entropy (discipline)
    void initHuman(float openness, float extraversion,
                   float neuroticism, float conscientiousness);
};

#endif // DRIVE_H
