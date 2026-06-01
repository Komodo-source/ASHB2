#include "header/PersonaSystem.h"
#include <algorithm>
#include <cmath>
#include <sstream>

std::string ChainOfThought::toDisplayString() const {
    std::string out;
    for (const auto& s : steps)
        out += "[" + s.phase + "] " + s.content + "\n";
    return out;
}

// ─── PAD computation ──────────────────────────────────────────────────────────
PADState computePAD(float happiness, float stress, float mentalHealth,
                    float anger, float loneliness,
                    float extraversion, float agreeableness, float neuroticism,
                    float selfEsteem) {
    PADState pad;

    // Pleasure: happiness + mental health push it up; stress + loneliness drag it down
    pad.pleasure = (happiness   - 50.0f) * 1.6f
                 - (stress      - 30.0f) * 0.55f
                 - (loneliness  - 30.0f) * 0.35f
                 + (mentalHealth - 50.0f) * 0.45f;
    pad.pleasure = std::max(-100.0f, std::min(100.0f, pad.pleasure));

    // Arousal: stress + anger drive it up; severe depression tanks it
    pad.arousal = 25.0f
                + stress      * 0.40f
                + anger       * 0.25f
                + extraversion * 0.18f
                - (mentalHealth < 30.0f ? 22.0f : 0.0f);
    pad.arousal = std::max(0.0f, std::min(100.0f, pad.arousal));

    // Dominance: self-esteem + low neuroticism + agreeableness (slightly lowers it)
    pad.dominance = 15.0f
                  + (100.0f - neuroticism) * 0.30f
                  + selfEsteem             * 0.30f
                  + agreeableness          * 0.10f;
    pad.dominance = std::max(0.0f, std::min(100.0f, pad.dominance));

    return pad;
}

// ─── Body language derivation ─────────────────────────────────────────────────
BodyLanguageCue deriveBodyLanguage(const PADState& pad, float griefIntensity) {
    if (griefIntensity > 0.4f)                                           return BodyLanguageCue::GRIEVING;
    if (pad.pleasure < -25.0f && pad.arousal > 55.0f && pad.dominance > 55.0f) return BodyLanguageCue::AGGRESSIVE;
    if (pad.pleasure < -15.0f && pad.arousal > 50.0f)                   return BodyLanguageCue::NERVOUS;
    if (pad.dominance > 65.0f && pad.arousal > 38.0f)                   return BodyLanguageCue::DOMINANT;
    if (pad.pleasure  > 25.0f && pad.arousal > 52.0f)                   return BodyLanguageCue::ENGAGED;
    if (pad.pleasure  > 15.0f && pad.arousal < 48.0f)                   return BodyLanguageCue::CONTENT;
    if (pad.pleasure  <  5.0f && pad.arousal < 42.0f)                   return BodyLanguageCue::WITHDRAWN;
    return BodyLanguageCue::CONTENT;
}

const char* bodyLanguageCueLabel(BodyLanguageCue cue) {
    switch (cue) {
        case BodyLanguageCue::ENGAGED:    return "Engaged";
        case BodyLanguageCue::NERVOUS:    return "Nervous";
        case BodyLanguageCue::DOMINANT:   return "Dominant";
        case BodyLanguageCue::WITHDRAWN:  return "Withdrawn";
        case BodyLanguageCue::CONTENT:    return "Content";
        case BodyLanguageCue::AGGRESSIVE: return "Aggressive";
        case BodyLanguageCue::GRIEVING:   return "Grieving";
        default:                          return "Neutral";
    }
}

const char* bodyLanguageCueDesc(BodyLanguageCue cue) {
    switch (cue) {
        case BodyLanguageCue::ENGAGED:    return "Leaning in, sustained eye contact";
        case BodyLanguageCue::NERVOUS:    return "Fidgeting, breaks eye contact frequently";
        case BodyLanguageCue::DOMINANT:   return "Upright posture, direct unwavering gaze";
        case BodyLanguageCue::WITHDRAWN:  return "Closed posture, avoids eye contact";
        case BodyLanguageCue::CONTENT:    return "Relaxed, open body language";
        case BodyLanguageCue::AGGRESSIVE: return "Tense, invading personal space";
        case BodyLanguageCue::GRIEVING:   return "Hunched, slow deliberate movements";
        default:                          return "";
    }
}

// ─── Action sentiment classification ─────────────────────────────────────────
int getActionSentiment(const std::string& actionName) {
    static const std::vector<std::string> positive = {
        "Socialize","GoodConnection","HelpSupport","Apologize","Reconcile",
        "Flirt","Date","couple","Pray","Meditate","Celebrate","TeachSkill",
        "CreateArt","Negotiate","Befriend","Preach","SetBoundaries"
    };
    static const std::vector<std::string> negative = {
        "AngerConnection","Murder","Discrimination","Insult","Manipulate",
        "Betray","Jealousy","DeclareWar","ChallengeLeader","IgnoreAvoid"
    };
    for (const auto& p : positive) if (actionName == p) return  1;
    for (const auto& n : negative) if (actionName == n) return -1;
    return 0;
}

// ─── Action moral complexity score ───────────────────────────────────────────
float getActionComplexity(const std::string& actionName) {
    static const std::vector<std::pair<std::string,float>> table = {
        {"Murder",0.95f},       {"Betray",0.90f},     {"DeclareWar",0.88f},
        {"Manipulate",0.75f},   {"BreakUp",0.72f},    {"ChallengeLeader",0.70f},
        {"Reconcile",0.60f},    {"Apologize",0.55f},  {"Flirt",0.45f},
        {"Date",0.40f},         {"Negotiate",0.38f},  {"couple",0.35f},
        {"Gossip",0.30f},       {"Socialize",0.20f},  {"Rest",0.05f},
        {"EatDrink",0.05f},     {"Hygiene",0.05f},    {"Take Shower",0.05f}
    };
    for (const auto& p : table) if (actionName == p.first) return p.second;
    return 0.15f;
}

// ─── Hesitation filler generator ─────────────────────────────────────────────
std::string generateHesitationFiller(float complexity, float stress, float neuroticism) {
    if (complexity < 0.35f) return "";

    struct Filler { std::string text; float minComplexity; float minStress; };
    static const std::vector<Filler> fillers = {
        {"Hmm.",                                 0.35f,  0.0f},
        {"Actually...",                          0.40f,  0.0f},
        {"One moment.",                          0.45f,  0.0f},
        {"This isn't simple.",                   0.55f, 40.0f},
        {"I need to think about this.",          0.60f,  0.0f},
        {"I... actually, wait\xe2\x80\x94",      0.65f, 55.0f},
        {"This is harder than it looks.",        0.70f,  0.0f},
        {"Hmm. This is... complicated.",         0.75f, 50.0f},
        {"I keep going back and forth on this.", 0.80f, 60.0f},
    };

    std::string best = "Hmm.";
    for (const auto& f : fillers)
        if (complexity >= f.minComplexity && stress >= f.minStress)
            best = f.text;
    return best;
}

// ─── Chain-of-Thought builder ─────────────────────────────────────────────────
ChainOfThought buildChainOfThought(
    float loneliness, float stress, float anger, float happiness,
    float mentalHealth, float boredom,
    const std::string& primaryGoal,
    const std::vector<std::string>& topCandidates,
    const std::vector<float>&       topScores,
    const std::string& chosenAction,
    bool  isImpulsive,
    const std::string& situationHint)
{
    ChainOfThought cot;
    cot.isImpulsive = isImpulsive;

    // Step 1 – Perception: what is most salient right now?
    {
        struct S { std::string name; float value; };
        std::vector<S> stats = {
            {"loneliness", loneliness}, {"stress", stress},
            {"anger",      anger},      {"boredom", boredom}
        };
        auto top = std::max_element(stats.begin(), stats.end(),
            [](const S& a, const S& b){ return a.value < b.value; });
        std::ostringstream ss;
        ss << "Most urgent: " << top->name << " (" << (int)top->value << ")";
        if (!situationHint.empty()) ss << "  |  Context: " << situationHint;
        cot.steps.push_back({"Perception", ss.str()});
    }

    // Step 2 – Appraisal: goal relevance + capacity assessment
    {
        std::ostringstream ss;
        ss << "Goal [" << primaryGoal << "]. ";
        if      (happiness   > 65.0f) ss << "Feeling capable and motivated.";
        else if (mentalHealth < 30.0f) ss << "Mind is struggling — options feel limited.";
        else if (stress      > 70.0f)  ss << "High stress is narrowing decision-making.";
        else                           ss << "Moderate deliberation capacity.";
        cot.steps.push_back({"Appraisal", ss.str()});
    }

    // Step 3 – Candidates: top scored options
    if (!topCandidates.empty()) {
        std::ostringstream ss;
        ss << "Considered: ";
        int show = std::min(3, (int)topCandidates.size());
        for (int i = 0; i < show; ++i) {
            ss << topCandidates[i];
            if (i < (int)topScores.size())
                ss << " [" << (int)(topScores[i] * 10) << "]";
            if (i < show - 1) ss << ", ";
        }
        cot.steps.push_back({"Candidates", ss.str()});
    }

    // Step 4 – Decision
    {
        std::ostringstream ss;
        ss << "Chose: " << chosenAction << " \xe2\x80\x94 ";
        ss << (isImpulsive ? "instinctive reaction (System 1)."
                           : "deliberate reasoning (System 2).");
        cot.steps.push_back({"Decision", ss.str()});
    }

    // Step 5 – Self-check / metacognition
    {
        std::ostringstream ss;
        if      (stress > 72.0f && !isImpulsive) ss << "Risk of regret under high stress. Proceeding carefully.";
        else if (mentalHealth < 28.0f)            ss << "Mental state fragile \xe2\x80\x94 outcome uncertain.";
        else if (anger > 65.0f)                   ss << "Anger may be coloring judgment. Noted.";
        else                                       ss << "Confidence nominal. Proceeding.";
        cot.steps.push_back({"Check", ss.str()});
    }

    // Complexity: conflict between competing stats + action's inherent moral weight
    float conflict = 0.0f;
    if (loneliness > 60.0f && stress  > 55.0f) conflict += 0.25f;
    if (anger      > 50.0f && happiness > 45.0f) conflict += 0.20f;
    if (mentalHealth < 40.0f)                     conflict += 0.20f;
    if (stress > 75.0f)                           conflict += 0.15f;
    cot.decisionComplexity = std::min(1.0f, conflict + getActionComplexity(chosenAction) * 0.5f);

    return cot;
}
