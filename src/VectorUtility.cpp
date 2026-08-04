#include "header/VectorUtility.h"
#include "header/Entity.h"

#include <algorithm>
#include <cmath>

namespace vu {

static inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

Vec stateVector(const Entity& e, int numNearby, bool isNight, float seasonPhase01) {
    Vec s{};
    s[D_HUNGER]    = clamp01(e.entityHunger      / 100.0f);
    s[D_LONELY]    = clamp01(e.entityLoneliness  / 100.0f);
    s[D_FATIGUE]   = clamp01(e.sleepPressure     / 100.0f);
    s[D_STRESS]    = clamp01(e.entityStress      / 100.0f);
    s[D_UNWELL]    = clamp01((100.0f - e.entityHealth) / 100.0f);
    s[D_FEAR]      = clamp01(e.emotions.fear     / 100.0f);
    s[D_ANGER]     = clamp01(e.entityGeneralAnger/ 100.0f);
    s[D_JOY]       = clamp01(e.emotions.joy      / 100.0f);
    s[D_GUILT]     = clamp01(e.emotions.guilt    / 100.0f);
    // Crowding saturates: the difference between eight people and eighty is not
    // eight times anything, socially.
    s[D_CROWD]     = clamp01(static_cast<float>(numNearby) / 8.0f);
    s[D_NIGHT]     = isNight ? 1.0f : 0.0f;
    const float ang = seasonPhase01 * 6.2831853f;
    s[D_SEASON_S]  = std::sin(ang);
    s[D_SEASON_C]  = std::cos(ang);
    s[D_PROVISION] = clamp01(e.foodStore / 8.0f);
    s[D_INJURY]    = clamp01(e.injuryLevel / 100.0f);
    s[D_BIAS]      = 1.0f;
    return s;
}

// ─── Action embeddings ───────────────────────────────────────────────────────
// An action's embedding says which pressures it answers. It is built from the
// action's own need category (which the catalogue already assigns) plus a small
// table for the actions whose meaning is narrower than their category — EatMeal
// and SelfHarm are both "health", and they do not answer the same thing.
//
// These are hand-authored rather than randomly initialised on purpose: a random
// init would make a fresh world's early behaviour depend on generator state, and
// the whole point of the module is that it does not.
static Vec buildEmbedding(const std::string& n, const std::string& cat) {
    Vec a{};
    a[D_BIAS] = 1.0f;   // every action can learn a flat prior

    // Category gives the broad shape.
    if (cat == "survival") {                 // Farm, Gather, Hunt
        a[D_HUNGER] = 1.0f; a[D_PROVISION] = -0.6f; a[D_SEASON_C] = 0.3f;
    } else if (cat == "social") {
        a[D_LONELY] = 1.0f; a[D_CROWD] = 0.8f; a[D_JOY] = 0.3f;
    } else if (cat == "health") {
        a[D_UNWELL] = 0.8f; a[D_FATIGUE] = 0.5f; a[D_STRESS] = 0.4f;
    } else if (cat == "entertainment") {
        a[D_STRESS] = 0.7f; a[D_JOY] = 0.4f; a[D_HUNGER] = -0.5f; a[D_FATIGUE] = -0.3f;
    } else if (cat == "achievement") {
        a[D_JOY] = 0.4f; a[D_FATIGUE] = -0.7f; a[D_HUNGER] = -0.4f; a[D_NIGHT] = -0.3f;
    } else if (cat == "safety") {
        a[D_ANGER] = 1.0f; a[D_FEAR] = 0.6f; a[D_STRESS] = 0.4f;
    } else if (cat == "spiritual") {
        a[D_STRESS] = 0.6f; a[D_GUILT] = 0.8f; a[D_FEAR] = 0.4f;
    } else if (cat == "hygiene") {
        a[D_UNWELL] = 0.3f; a[D_STRESS] = 0.2f;
    } else if (cat == "leadership") {
        a[D_ANGER] = 0.5f; a[D_CROWD] = 0.7f;
    }

    // Name overrides where the category is too coarse to be honest.
    if (n == "EatMeal")            { a = Vec{}; a[D_BIAS] = 1.0f; a[D_HUNGER] = 1.0f; a[D_PROVISION] = 0.8f; }
    else if (n == "Sleep")         { a = Vec{}; a[D_BIAS] = 1.0f; a[D_FATIGUE] = 1.0f; a[D_NIGHT] = 0.9f; a[D_UNWELL] = 0.3f; }
    else if (n == "Rest")          { a = Vec{}; a[D_BIAS] = 1.0f; a[D_FATIGUE] = 0.8f; a[D_INJURY] = 0.6f; a[D_UNWELL] = 0.5f; }
    else if (n == "SelfHarm")      { a = Vec{}; a[D_BIAS] = 1.0f; a[D_STRESS] = 0.9f; a[D_GUILT] = 0.7f; a[D_JOY] = -0.8f; }
    else if (n == "Prayer")        { a = Vec{}; a[D_BIAS] = 1.0f; a[D_FEAR] = 0.7f; a[D_GUILT] = 0.7f; a[D_STRESS] = 0.5f; }
    else if (n == "SeekTherapy")   { a = Vec{}; a[D_BIAS] = 1.0f; a[D_STRESS] = 1.0f; a[D_GUILT] = 0.4f; }
    else if (n == "Mourn")         { a = Vec{}; a[D_BIAS] = 1.0f; a[D_GUILT] = 0.5f; a[D_JOY] = -0.6f; }
    else if (n == "Exercise")      { a = Vec{}; a[D_BIAS] = 1.0f; a[D_UNWELL] = -0.5f; a[D_FATIGUE] = -0.6f; a[D_STRESS] = 0.4f; }
    else if (n == "Explore")       { a[D_FEAR] = -0.8f; a[D_CROWD] = -0.4f; a[D_NIGHT] = -0.5f; }
    else if (n == "Gossip")        { a[D_CROWD] = 1.0f; a[D_ANGER] = 0.4f; }
    else if (n == "HelpSupport")   { a[D_GUILT] = 0.6f; a[D_CROWD] = 0.7f; }
    else if (n == "Apologize")     { a[D_GUILT] = 1.0f; }
    else if (n == "Murder" || n == "Duel" || n == "Raid") { a[D_ANGER] = 1.0f; a[D_FEAR] = -0.4f; }
    else if (n == "Suicide")       { a = Vec{}; a[D_BIAS] = 1.0f; a[D_JOY] = -1.0f; a[D_STRESS] = 0.9f; }
    else if (n == "DrinkAlcohol" || n == "Smoke") { a[D_STRESS] = 1.0f; a[D_JOY] = -0.3f; }

    return a;
}

const Vec& actionEmbedding(const std::string& actionName, const std::string& needCategory) {
    // Ordered map: the catalogue is small and fixed, and ordered iteration keeps
    // any future debug dump reproducible.
    static std::map<std::string, Vec> cache;
    auto it = cache.find(actionName);
    if (it != cache.end()) return it->second;
    return cache.emplace(actionName, buildEmbedding(actionName, needCategory)).first->second;
}

// ─── Learner ─────────────────────────────────────────────────────────────────

// Weights for `action`, seeded from its hand-authored prior the first time it
// is ever considered. Seeding from the embedding (rather than from zero) is what
// gives a newborn agent instincts: it already expects eating to answer hunger.
Vec& Learner::weightsFor(const std::string& action, const std::string& needCategory) {
    auto it = w.find(action);
    if (it != w.end()) return it->second;
    // The embedding is a direction, not a magnitude. Seeded at half strength so
    // a newborn's instincts bias its choices without pinning the ±40% authority
    // this term is allowed at its clamp before a single thing has been learned.
    Vec seed = actionEmbedding(action, needCategory);
    for (int i = 0; i < DIM; ++i) seed[i] *= 0.5f;
    return w.emplace(action, seed).first->second;
}

float Learner::utility(const Vec& s, const std::string& action,
                       const std::string& needCategory) {
    const Vec& wa = weightsFor(action, needCategory);
    float u = 0.0f;
    for (int i = 0; i < DIM; ++i) u += wa[i] * s[i];
    auto it = bias.find(action);
    if (it != bias.end()) u += it->second;
    return u;
}

void Learner::learn(const Vec& s, const std::string& action, const std::string& needCategory,
                    float reward, float bestNextUtility, float discount) {
    const float predicted = utility(s, action, needCategory);
    const float delta     = reward + discount * bestNextUtility - predicted;

    // This step: gradient on THIS action's own weights. A bad outcome from
    // `Betray` can no longer teach an agent to stop reproducing.
    {
        Vec& wa = weightsFor(action, needCategory);
        for (int i = 0; i < DIM; ++i) wa[i] = std::clamp(wa[i] + ALPHA * delta * s[i], -WCLAMP, WCLAMP);
        float& b = bias[action];
        b = std::clamp(b + ALPHA * delta * 0.1f, -WCLAMP, WCLAMP);
    }

    // ...and back down the trace, geometrically decayed, each step crediting the
    // action that was actually taken then. This is what lets a harvest pay the
    // days of ploughing that earned it.
    float decay = LAMBDA;
    for (int k = 1; k <= traceLen; ++k) {
        const int idx = (traceHead - k + TRACE * 2) % TRACE;
        if (traceAction[idx].empty()) { decay *= LAMBDA; continue; }
        // Its OWN category — the trace remembers what each past action was, so a
        // replay cannot seed an action's instincts from an unrelated category.
        Vec& wOld = weightsFor(traceAction[idx], traceCat[idx]);
        const Vec& sOld = traceS[idx];
        for (int i = 0; i < DIM; ++i)
            wOld[i] = std::clamp(wOld[i] + ALPHA * delta * decay * sOld[i], -WCLAMP, WCLAMP);
        decay *= LAMBDA;
        if (decay < 0.01f) break;
    }

    traceS[traceHead]      = s;
    traceAction[traceHead] = action;
    traceCat[traceHead]    = needCategory;
    traceHead              = (traceHead + 1) % TRACE;
    if (traceLen < TRACE) ++traceLen;
}

void Learner::clearTrace() {
    traceHead = 0;
    traceLen  = 0;
}

} // namespace vu
