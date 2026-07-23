#ifndef MIND_UPGRADE_H
#define MIND_UPGRADE_H

// ─── MindUpgrade — AI Upgrade Plan phases A-E (plans/ai-upgrade-2026-07.md) ───
// One cohesive module for the new mind layers so the integration surface into
// the (very large) existing files stays small:
//
//   B1  EmotionState        — OCC-style discrete emotions with action tendencies
//   B2  prospection/regret  — expected-vs-actual outcome comparison
//   B3  threatPrediction    — active theory of mind over MentalModelOfOther
//   B4  Intention           — persistent multi-day goal pursuit
//   C2  SkillSet            — practice curves, learning by doing, teaching
//   C3  KnowledgeStore      — propositional facts, rumor distortion, lies
//   D2  sleep/circadian     — sleep pressure, quality, consolidation gating
//   D5  place attachment    — homesickness after tribe switches
//   A2/E1 ScoringPriors     — data-driven factor weights (priors_vN loop)
//   E2  MindRunStats        — run counters for the realism report
//
// Contract (ARCHITECTURE.md): deterministic (all randomness via BetterRand),
// append-only save keys, every multiplier neutral (×1.0) in the default state,
// and per-tick cost O(1) per agent.

#include <string>
#include <vector>
#include <cstdint>

class Entity;

// ─── B1: discrete emotions ─────────────────────────────────────────────────────
// Anger deliberately lives on the legacy entityGeneralAnger stat (it is read by
// dozens of existing sites); this struct adds the emotions the engine could not
// previously distinguish. All 0..100, decayed every tick.
struct EmotionState {
    float fear      = 0.0f;   // undesirable + uncontrollable  → flee, appease, ally
    float joy       = 0.0f;   // goal progress                 → share, create, bond
    float sadness   = 0.0f;   // loss (non-grief)              → withdraw, seek meaning
    float shame     = 0.0f;   // self-blame, witnessed         → withdraw, conform
    float guilt     = 0.0f;   // self-blame, private           → repair toward victim
    float envy      = 0.0f;   // rival's desirable outcome     → status-seek, sabotage
    float gratitude = 0.0f;   // other-credit for own gain     → reciprocate
    float pride     = 0.0f;   // self-credit for own gain      → display, persist
    float hope      = 0.0f;   // anticipated good outcome      → explore, invest
    float regret    = 0.0f;   // counterfactual "should have"  → repair, avoid repeat

    void decay();                                   // once per tick
    // Dominant emotion above `floor` intensity ("" if none) — for CoT/narrative.
    std::string dominant(float floor = 20.0f) const;
    float dominantIntensity() const;
};

// ─── B4: persistent intention (multi-day pursuit of one life goal) ────────────
struct Intention {
    std::string type;              // a LifeGoal type ("find_partner", ...)
    int   sinceDay        = -1;
    int   lastProgressDay = -1;
    float progress        = 0.0f;  // 0..100
    bool  active          = false;
};

// ─── C2: skills with practice curves ──────────────────────────────────────────
// Fixed catalog so serialization stays a simple CSV line.
enum SkillId { SK_HUNT = 0, SK_GATHER, SK_FARM, SK_CRAFT, SK_HEAL,
               SK_FIGHT, SK_ORATORY, SK_LORE, SK_COUNT };
extern const char* const kSkillNames[SK_COUNT];

struct SkillSet {
    float v[SK_COUNT] = {5,5,5,5,5,5,5,5};   // 0..100, everyone starts a novice
    // Learning by doing: log-shaped — fast early gains, slow mastery.
    void  practice(SkillId s, float effort);
    // Oblique transmission: teacher transfers a fraction of the gap.
    void  learnFrom(const SkillSet& teacher, SkillId s);
    float get(SkillId s) const { return v[s]; }
    void  decayTick();                        // very slow forgetting
};

// ─── C3: propositional knowledge (facts, rumors, lies) ────────────────────────
enum FactPredicate : uint8_t {
    FACT_KILLED   = 0,   // subject killed someone (value = severity)
    FACT_FOOD_AT  = 1,   // region `subject` is rich (value = abundance seen)
    FACT_DANGER   = 2,   // subject is dangerous / region unsafe
    FACT_GENEROUS = 3,   // subject helped people (value = warmth)
};

struct KnownFact {
    int     subjectId  = -1;    // entity id or region id (predicate-dependent)
    uint8_t predicate  = FACT_KILLED;
    float   value      = 0.0f;
    float   confidence = 0.5f;  // decays with retelling
    int     sourceId   = -1;    // who told me (-1 = witnessed myself)
    int     day        = 0;
    bool    isTrue     = true;  // ground-truth tag, for the E2 accuracy metric
};

struct KnowledgeStore {
    std::vector<KnownFact> facts;
    static const int MAX_FACTS = 48;

    // Dedupe on (subject, predicate): keep the higher-confidence version.
    void remember(const KnownFact& f);
    // The fact most worth retelling (fresh + confident + dramatic).
    const KnownFact* mostSalient(int today) const;
    // Confidence-weighted share of held facts that are actually true.
    float accuracy() const;
};

// ─── A2/E1: data-driven scoring priors ────────────────────────────────────────
// Defaults reproduce the hardcoded scorer exactly. `bridge/priors_active.txt`
// (KEY:value lines) can retune them without a recompile — the tuning surface of
// the human-in-the-loop upgrade loop. Multiplicative factors are reweighted via
// mind::adjustFactor(), guarded so that weight==1.0 is bit-identical.
struct ScoringPriors {
    // additive blend weights (must mirror the constants in cognitiveChooseAction)
    float wRequire = 0.20f, wNeed = 0.25f, wMemBias = 0.10f,
          wVariety = 0.10f, wSocial = 0.19f;
    // multiplicative factor intensities (1.0 = engine default)
    float mContext = 1.0f, mPersona = 1.0f, mValue = 1.0f, mGrief = 1.0f,
          mPherom = 1.0f, mEnv = 1.0f, mNorm = 1.0f,
          mEmotion = 1.0f, mIntent = 1.0f;
    int  version = 0;
    bool loaded  = false;
};
extern ScoringPriors g_priors;

// ─── E2: run counters for the realism report ──────────────────────────────────
struct MindRunStats {
    long fearEpisodes = 0, joyEpisodes = 0, shameEpisodes = 0, guiltEpisodes = 0,
         envyEpisodes = 0, gratitudeEpisodes = 0;
    long regrets = 0, reliefs = 0;
    long factsShared = 0, liesTold = 0;
    long festivalsHeld = 0;
    long deepSleeps = 0, sleeplessNights = 0;
    long deceptionsAttempted = 0, deceptionsDetected = 0;
};
extern MindRunStats g_mindStats;

namespace mind {

// A2: load priors file if present (silent no-op when absent). Call once at boot.
void loadPriors(const std::string& path);
// Reweight a multiplicative factor: w==1 returns f untouched (bit-exact).
inline float adjustFactor(float f, float w) {
    return (w == 1.0f) ? f : 1.0f + (f - 1.0f) * w;
}

// ── Per-tick upkeep (B1 appraisal, D2 sleep pressure, D5 homesickness). ──────
// Runs for EVERY living agent every tick (like needs/disease), independent of
// the deliberation cohort stagger.
void upkeep(Entity* e, const std::vector<Entity*>& group, bool isNight, int simDay);

// B1: emotion → action-tendency multiplier (the 13th scoring factor).
// Neutral 1.0 when all emotions are quiet; clamped to [0.4, 2.2].
float emotionActionModifier(const Entity* e, const std::string& actionName);

// B4: weekly intention selection / frustration-driven abandonment.
void updateIntention(Entity* e, int simDay);
// B4: bias toward actions that advance the active intention (neutral 1.0).
float intentionModifier(const Entity* e, const std::string& actionName);

// B3: how threatening does `other` look, judged ONLY through the observer's
// (possibly stale, possibly wrong) mental model + visible body language. 0..1.
float threatPrediction(Entity* self, Entity* other, int simDay);

// B3: active deception. Gated to low-integrity agents who hold a grudge
// against `target` (the "high stakes" cue) — during a positive social
// interaction they fake warmth, inflating the target's trust in them beyond
// what the genuine interaction earned, at the cost of the target's true read
// on them (predictability). Call after the normal sentiment-driven trust
// update on a positive-sentiment action. No-op (returns false) off-gate.
bool attemptDeception(Entity* deceiver, Entity* target, const std::string& actionName, int simDay);

// B3: detection. Call at the moment `offender` commits an act (Betray,
// Manipulate) against `victim` that a faked-friendly mask would no longer
// explain. If the victim's trust in the offender was inflated by deception
// (or simply high), the prediction error "snaps": trust and predictability
// collapse harder than a graded update, and — only when the victim was truly
// fooled — a distinct shock (fear/shame + a named memory) registers instead
// of the ordinary anger the action already applies elsewhere.
void detectDeception(Entity* victim, Entity* offender, const std::string& actionName, int simDay);

// B2 + C2 + D3: post-action settlement — regret/relief from expected-vs-actual,
// skill practice, injuries from violent actions, intention progress.
// Returns the surprise value (outcome − expected; 0 when no expectation stored).
float settleOutcome(Entity* e, const std::string& actionName, float outcome, int simDay);

// C3: one gossip/social exchange moves the speaker's most salient fact to the
// listener, with distortion — and low-integrity speakers sometimes lie.
void shareKnowledge(Entity* speaker, Entity* listener, int simDay);

// C3/B1: a witnessed crime becomes a propositional fact + fear spike.
void observeCrime(Entity* witness, Entity* offender, bool lethal, int simDay);

// C2: yield multiplier for subsistence actions (0.8 novice → 1.5 master).
float skillYieldMul(const Entity* e, SkillId s);

// E2 report helpers.
float populationBeliefAccuracy(const std::vector<Entity>& entities);
float skillGini(const std::vector<Entity>& entities);

} // namespace mind

#endif // MIND_UPGRADE_H
