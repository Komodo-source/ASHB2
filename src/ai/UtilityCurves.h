#ifndef UTILITY_CURVES_H
#define UTILITY_CURVES_H

// ─── UtilityCurves — declarative response curves (Upgrade Plan, Step 3) ───────
// Replaces branchy if/else scoring with named mathematical curves, the standard
// Utility-AI construction (Mark, "Behavioral Mathematics"). A scorer is DATA —
// a curve shape plus parameters — so tuning is a value change, not a code path,
// and the same curve set drives item valuation, GOAP goal urgency, and the
// UtilityBrain action scoring.
//
// All curves map a normalized input x∈[0,1] to a utility in [0,1]:
//   LINEAR     y = m·x + b
//   QUADRATIC  y = m·(x^k) + b               (k<1 = fast rise, k>1 = slow rise)
//   LOGISTIC   y = 1 / (1 + e^(-k·(x - c)))  (soft threshold at c)
//   INV_EXP    y = 1 - e^(-k·x)              (diminishing returns)
// Outputs are clamped to [0,1]. Weighted sums of curves make the final score:
//   score = Σ weight_i · curve_i(input_i)    — exactly the spec's
//   "Score = (Hunger*2.0) + (Freshness*0.5) - (Distance*0.2)" form, generalized.

#include <cmath>
#include <algorithm>

struct UtilityCurve {
    enum Shape { LINEAR, QUADRATIC, LOGISTIC, INV_EXP };

    Shape shape = LINEAR;
    float m = 1.0f;   // slope / scale
    float k = 1.0f;   // exponent / steepness
    float b = 0.0f;   // vertical shift
    float c = 0.5f;   // logistic midpoint

    float eval(float x) const {
        x = std::min(1.0f, std::max(0.0f, x));
        float y;
        switch (shape) {
            case QUADRATIC: y = m * std::pow(x, k) + b;                    break;
            case LOGISTIC:  y = 1.0f / (1.0f + std::exp(-k * (x - c)));    break;
            case INV_EXP:   y = 1.0f - std::exp(-k * x);                   break;
            case LINEAR: default: y = m * x + b;                           break;
        }
        return std::min(1.0f, std::max(0.0f, y));
    }
};

// One weighted term of a utility score. Normalizer maps a raw stat (0..100
// hunger, world-units distance…) into the curve's [0,1] domain.
struct UtilityTerm {
    UtilityCurve curve;
    float        weight    = 1.0f;
    float        inputMax  = 100.0f;  // raw value that maps to x=1

    float score(float raw) const {
        return weight * curve.eval(inputMax > 0.0f ? raw / inputMax : 0.0f);
    }
};

namespace UtilityCurvesLib {
    // Survival urgency: quadratic — mild hunger barely registers, starvation
    // dominates everything (the spec's "starving agents value food exponentially").
    inline UtilityTerm hungerUrgency()   { return {{UtilityCurve::QUADRATIC, 1.0f, 2.2f, 0.0f, 0.5f}, 2.0f, 100.0f}; }
    // Distance penalty: inverse-exponential — nearby differences matter, far is far.
    inline UtilityTerm distancePenalty() { return {{UtilityCurve::INV_EXP,   1.0f, 3.0f, 0.0f, 0.5f}, -0.6f, 600.0f}; }
    // Danger avoidance: logistic — safe is safe, but past the midpoint fear spikes.
    inline UtilityTerm dangerAvoidance() { return {{UtilityCurve::LOGISTIC,  1.0f, 9.0f, 0.0f, 0.45f}, -1.5f, 3.0f}; }
    // Health emergency: quadratic on (1 - health).
    inline UtilityTerm healthUrgency()   { return {{UtilityCurve::QUADRATIC, 1.0f, 2.0f, 0.0f, 0.5f}, 1.6f, 100.0f}; }
    // Freshness / condition preference: gentle linear.
    inline UtilityTerm freshnessBonus()  { return {{UtilityCurve::LINEAR,    1.0f, 1.0f, 0.0f, 0.5f}, 0.5f, 100.0f}; }
}

#endif // UTILITY_CURVES_H
