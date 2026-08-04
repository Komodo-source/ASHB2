#include "header/QISystem.h"
#include "header/LiveConfig.h"

#include <algorithm>
#include <cmath>

namespace QISystem {

// The one shape every payoff has. `base` is how elastic this domain is to
// intelligence; `eliteWeight` splits the credit between the whole people and
// its top decile — war is mostly decided by whoever is giving the orders,
// while food yields are decided by everybody's hands.
//
// The upper clamps are not decoration. Without them the loop compounds: a
// clever people researches faster, which makes it richer, which schools more of
// its children, which makes it cleverer. Bounded multipliers turn that runaway
// into a strong-but-finite advantage that a bigger, dumber neighbour can still
// beat with numbers.
//
// The LOWER clamp is 1.0 — QI can only ever add. That is a deliberate reversal
// of the first version, which let an unlucky people fall below 1.0, and it is
// worth explaining because the reasoning is not obvious:
//
//   • `eliteQI` is the mean of the top decile, and a decile is meaningless for
//     a tribe of four — its "elite" is just its best member, which sits far
//     closer to its mean than a twenty-person tribe's top decile does. Scoring
//     both against one fixed baseline therefore penalises SMALL tribes for
//     being small, no matter how clever they are.
//   • A penalty lands on `growthMul`, which multiplies the food supply, which
//     feeds nutrition, which feeds QI. That is a closed loop with the sign
//     pointing down: a tribe pushed below the line cannot climb back. Seed s3
//     went extinct at tick 2129 with 7 technologies exactly this way, against
//     31 for the same seed with QI off.
//
// A dark age still costs a people dearly — it loses a research bonus worth up
// to +150% — it simply cannot drive that people below where it would have been
// with no schools at all. Intelligence is an advantage here, never a handicap.
static float mul(float base, float eliteWeight, const Tribe& t, float hi) {
    if (g_liveConfig.qiMul == 0.0f) return 1.0f;   // bit-exact kill switch
    float m = (t.meanQI  - kUnschooledMean)  / 100.0f * (1.0f - eliteWeight)
            + (t.eliteQI - kUnschooledElite) / 100.0f * eliteWeight;
    return std::clamp(1.0f + base * m * g_liveConfig.qiMul, 1.0f, hi);
}

float researchMul(const Tribe& t) { return mul(2.0f, 0.5f, t, 2.50f); }
float warMul(const Tribe& t)      { return mul(1.2f, 0.7f, t, 1.45f); }
float defenseMul(const Tribe& t)  { return mul(1.0f, 0.6f, t, 1.35f); }
float growthMul(const Tribe& t)   { return mul(0.8f, 0.3f, t, 1.25f); }

float misjudgement(const Tribe& t) {
    if (g_liveConfig.qiMul == 0.0f) return 0.0f;   // kill switch: perfect sight, as before
    // A council that can count, read a map and remember the last war estimates
    // its enemy well. One that cannot, guesses — and guesses are what send a
    // people over the border into an army twice its size.
    float clarity = std::clamp((130.0f - t.eliteQI) / 30.0f, 0.2f, 1.5f);
    return 0.35f * clarity * g_liveConfig.qiMul;
}

std::string summary(const Tribe& t) {
    int pop = t.population();
    int pct = (pop > 0) ? (t.schooledCount * 100 / pop) : 0;
    return "QI " + std::to_string((int)(t.meanQI + 0.5f))
         + " (elite " + std::to_string((int)(t.eliteQI + 0.5f)) + ") - "
         + std::to_string(pct) + "% schooled";
}

} // namespace QISystem
