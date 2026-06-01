#ifndef PERSONA_SYSTEM_H
#define PERSONA_SYSTEM_H

#include <string>
#include <vector>
#include <deque>

// ─── Phase 3: PAD Emotional Model (Pleasure · Arousal · Dominance) ────────────
struct PADState {
    float pleasure   = 0.0f;   // −100…100: unhappy → content
    float arousal    = 50.0f;  //    0…100: calm    → excited
    float dominance  = 50.0f;  //    0…100: submissive → dominant
};

// ─── Phase 5: Non-verbal body language derived from PAD ──────────────────────
enum class BodyLanguageCue {
    ENGAGED,      // high pleasure + arousal : leaning in, eye contact
    NERVOUS,      // low pleasure  + high arousal : fidgeting, avoidant gaze
    DOMINANT,     // high dominance, moderate arousal : upright, direct gaze
    WITHDRAWN,    // low arousal   + low pleasure : closed, distant
    CONTENT,      // high pleasure + low arousal : relaxed, open
    AGGRESSIVE,   // low pleasure  + high arousal + high dominance
    GRIEVING      // very low on all dims
};

// ─── Phase 2: Core belief distilled from repeated life memories ───────────────
struct CoreBelief {
    std::string category;           // "relationships"|"self"|"world"|"achievement"
    std::string belief;             // natural-language summary
    float valence            = 0.0f;   // −100…100
    float strength           = 50.0f;  // 0…100
    int   formedOnDay        = 0;
    int   reinforcementCount = 1;
};

// ─── Phase 2: Working-memory slot (short-term significant events) ─────────────
struct WorkingMemoryEntry {
    std::string eventType;
    std::string description;
    float emotionalWeight = 0.0f;
    int   ticksAgo        = 0;
};

// ─── Phase 4: Chain-of-Thought deliberation record ───────────────────────────
struct CoTStep {
    std::string phase;    // "Perception"|"Appraisal"|"Candidates"|"Decision"|"Check"
    std::string content;
};

struct ChainOfThought {
    std::vector<CoTStep> steps;
    bool  isImpulsive        = false;
    float decisionComplexity = 0.0f;   // 0…1, higher → longer hesitation pause

    std::string toDisplayString() const;
};

// ─── Phase 5: Hesitation state for high-stakes decisions ─────────────────────
struct HesitationState {
    float       ticksRemaining     = 0.0f;
    std::string pendingActionName  = "";
    std::string fillerExpression   = "";  // "Hmm…", "Actually, wait—"
    float       decisionComplexity = 0.0f;
};

// ─── Free utility functions ───────────────────────────────────────────────────

PADState computePAD(float happiness, float stress, float mentalHealth,
                    float anger, float loneliness,
                    float extraversion, float agreeableness, float neuroticism,
                    float selfEsteem);

BodyLanguageCue deriveBodyLanguage(const PADState& pad, float griefIntensity = 0.0f);

const char* bodyLanguageCueLabel(BodyLanguageCue cue);
const char* bodyLanguageCueDesc (BodyLanguageCue cue);

// −1 = negative, 0 = neutral, +1 = positive
int   getActionSentiment (const std::string& actionName);

// 0.0 = trivial … 1.0 = high moral stakes
float getActionComplexity(const std::string& actionName);

std::string generateHesitationFiller(float complexity, float stress, float neuroticism);

ChainOfThought buildChainOfThought(
    float loneliness, float stress, float anger, float happiness,
    float mentalHealth, float boredom,
    const std::string& primaryGoal,
    const std::vector<std::string>& topCandidates,
    const std::vector<float>&       topScores,
    const std::string& chosenAction,
    bool  isImpulsive,
    const std::string& situationHint
);

#endif // PERSONA_SYSTEM_H
