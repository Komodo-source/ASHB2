#ifndef VECTOR_UTILITY_H
#define VECTOR_UTILITY_H

#include <array>
#include <map>
#include <string>
#include <vector>

class Entity;

// ─────────────────────────────────────────────────────────────────────────────
// M13 — continuous vector utility (plans/cognitive-plausibility.md)
//
// What this replaces: `FreeWillSystem::rlStateSignature` compresses the whole
// situation into a 6-character string — loneliness in three buckets, then four
// yes/no flags and a season letter. That is ~192 distinguishable situations for
// a whole life. Two agents at hunger 51 and hunger 99 are the same agent to the
// learner; the same agent at 49 and 51 is two different ones. Behaviour cannot
// vary smoothly across a gradient because the learner cannot *see* a gradient,
// which is why agents settle into fixed short cycles and then switch all at once
// when a threshold trips.
//
// What this is instead: a linear bandit over a continuous state. The situation
// is a 16-vector of normalised drives, affect and context; each action carries a
// 16-vector saying which of those it speaks to; their elementwise product is the
// feature vector, and one learned weight vector scores it.
//
//     U(s,a) = w · (s ⊙ a) + bias[a]
//
// Sixteen multiply-adds, no matrix, no hidden layer, no allocation. Every number
// is inspectable and the whole learner serialises to 16 floats plus one scalar
// per action. It is deliberately NOT a neural net: this simulation's contract is
// that the same seed replays byte-for-byte, and a linear model in float32 with a
// fixed evaluation order is the largest thing that stays trivially reproducible.
//
// Determinism notes:
//   • `bias` is a std::map, not unordered_map. Iteration order is part of the
//     contract; a hash-ordered container would reorder learning updates across
//     libstdc++ versions and silently break byte-identity.
//   • Nothing here draws a random number. Exploration stays where it already
//     was, on the engine's seeded stream.
//   • The learner is runtime-only and is not serialised, exactly like the
//     tabular Q-table it sits beside.
// ─────────────────────────────────────────────────────────────────────────────
namespace vu {

constexpr int DIM = 16;
using Vec = std::array<float, DIM>;

// Index meaning of every dimension. State and action embeddings share this
// layout — that is what makes the elementwise product meaningful: component i of
// `s ⊙ a` is "how much this action speaks to how much this drive presses".
enum Dim {
    D_HUNGER = 0,   // 1 = starving
    D_LONELY,       // 1 = isolated
    D_FATIGUE,      // 1 = exhausted
    D_STRESS,       // 1 = overwhelmed
    D_UNWELL,       // 1 = near death (inverted health)
    D_FEAR,
    D_ANGER,
    D_JOY,
    D_GUILT,
    D_CROWD,        // people actually attended to, normalised against Dunbar-ish 8
    D_NIGHT,        // 1 = dark
    D_SEASON_S,     // sin of the year phase
    D_SEASON_C,     // cos of the year phase
    D_PROVISION,    // 1 = holds plenty of food
    D_INJURY,
    D_BIAS          // always 1.0 — lets the model learn an action's flat prior
};

// The agent's situation, every component clamped to [0,1] (season sin/cos to
// [-1,1]) so no single dimension dominates the dot product by unit choice.
Vec stateVector(const Entity& e, int numNearby, bool isNight, float seasonPhase01);

// Which drives an action speaks to. Derived once per action name from its need
// category and a small table of names, then cached — so it is fixed for the
// life of the process and identical across runs. This is the PRIOR an agent
// starts with, not a fixed property: `Learner::w` is seeded from it and then
// moves with experience.
const Vec& actionEmbedding(const std::string& actionName, const std::string& needCategory);

struct Learner {
    // One weight vector PER ACTION, seeded from that action's embedding.
    //
    // This started life as a single shared 16-vector scoring `w · (s ⊙ a)`, on
    // the theory that the action embedding carried enough per-action signal.
    // It does not, and the failure is worth recording because it is not
    // obvious: every action in a need category shares one embedding, so with a
    // single shared `w` the state-dependent utility of `breeding`, `couple`,
    // `Betray` and `Insult` is *identical* — only a stateless scalar bias told
    // them apart. One bad betrayal therefore suppressed reproduction by exactly
    // as much as it suppressed treachery. Measured over 2500 ticks on one seed:
    // births fell 480 → 37 and the world went extinct at tick 2161.
    //
    // The tabular Q-table this replaces was coarse about STATE but exact about
    // ACTION — it held a separate value per (state, action). Any replacement has
    // to keep that, so the weights are per-action and only the *state* side is
    // continuous. Cost is ~16 floats × catalogue size ≈ 2.7 KB per agent.
    std::map<std::string, Vec>   w;     // ordered — see the determinism note above
    std::map<std::string, float> bias;  // per-action flat prior

    // Eligibility trace. The pre-M13 code credited exactly one step back, at a
    // flat 0.3 — enough for "hunt then eat", useless for a season of farming
    // that pays off at harvest. A depth-8 geometric trace lets a reward reach
    // back over the whole chain of decisions that set it up.
    static constexpr int TRACE = 8;
    std::array<Vec, TRACE>         traceS{};       // the STATE at each past step
    std::array<std::string, TRACE> traceAction{};
    std::array<std::string, TRACE> traceCat{};     // and its need category
    int traceHead = 0;    // next slot to write
    int traceLen  = 0;    // how many slots are live

    static constexpr float ALPHA  = 0.10f;  // carried over from ActionValueFunction
    static constexpr float LAMBDA = 0.70f;  // trace decay per step back
    static constexpr float WCLAMP = 4.0f;   // one freak reward must not own the policy

    // This action's weights, seeded from its prior on first sight.
    Vec& weightsFor(const std::string& action, const std::string& needCategory);

    // Non-const: an action seen for the first time seeds its weights from the
    // hand-authored prior, so a fresh agent already has sensible instincts.
    float utility(const Vec& s, const std::string& action,
                  const std::string& needCategory);

    // One TD update plus the decayed trace. `discount` is passed in rather than
    // fixed so M14's anger gate can collapse it — an enraged agent genuinely
    // stops valuing the future.
    void learn(const Vec& s, const std::string& action, const std::string& needCategory,
               float reward, float bestNextUtility, float discount);

    void clearTrace();
};

} // namespace vu

#endif // VECTOR_UTILITY_H
