// ─── MindUpgrade.cpp — AI Upgrade Plan phases A-E implementation ──────────────
// See MindUpgrade.h for the module map and plans/ai-upgrade-2026-07.md for the
// design. Everything stochastic goes through BetterRand (world-seeded).

#include "MindUpgrade.h"
#include "../header/Entity.h"
#include "../header/BetterRand.h"
#include "../header/LiveConfig.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

ScoringPriors g_priors;
MindRunStats  g_mindStats;

const char* const kSkillNames[SK_COUNT] = {
    "hunt", "gather", "farm", "craft", "heal", "fight", "oratory", "lore"
};

static float clamp01f(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static float clamp100(float v) { return v < 0.0f ? 0.0f : (v > 100.0f ? 100.0f : v); }

// ─── EmotionState ─────────────────────────────────────────────────────────────

void EmotionState::decay() {
    // Arousal-type emotions fade fast; moral/self-conscious ones linger.
    fear      = std::max(0.0f, fear      - 3.0f);
    joy       = std::max(0.0f, joy       - 2.5f);
    sadness   = std::max(0.0f, sadness   - 1.0f);
    shame     = std::max(0.0f, shame     - 1.2f);
    guilt     = std::max(0.0f, guilt     - 1.0f);
    envy      = std::max(0.0f, envy      - 1.2f);
    gratitude = std::max(0.0f, gratitude - 1.5f);
    pride     = std::max(0.0f, pride     - 2.0f);
    hope      = std::max(0.0f, hope      - 2.0f);
    regret    = std::max(0.0f, regret    - 1.0f);
}

std::string EmotionState::dominant(float floor) const {
    const char* names[10] = {"fear","joy","sadness","shame","guilt",
                             "envy","gratitude","pride","hope","regret"};
    const float vals[10]  = {fear, joy, sadness, shame, guilt,
                             envy, gratitude, pride, hope, regret};
    int best = -1; float bv = floor;
    for (int i = 0; i < 10; ++i)
        if (vals[i] > bv) { bv = vals[i]; best = i; }
    return best < 0 ? "" : names[best];
}

float EmotionState::dominantIntensity() const {
    return std::max({fear, joy, sadness, shame, guilt,
                     envy, gratitude, pride, hope, regret});
}

// ─── SkillSet ─────────────────────────────────────────────────────────────────

void SkillSet::practice(SkillId s, float effort) {
    // Log-shaped: a novice leaps, a master crawls. `effort` ~0.5-2.0 per act.
    float gap = 100.0f - v[s];
    v[s] = clamp100(v[s] + effort * (gap / 100.0f) * 0.9f);
}

void SkillSet::learnFrom(const SkillSet& teacher, SkillId s) {
    if (teacher.v[s] <= v[s]) return;               // nothing to teach
    v[s] = clamp100(v[s] + (teacher.v[s] - v[s]) * 0.06f);
}

void SkillSet::decayTick() {
    // Skills rust extremely slowly (~1 point / 250 days unused).
    for (int i = 0; i < SK_COUNT; ++i)
        v[i] = std::max(1.0f, v[i] - 0.004f);
}

// ─── KnowledgeStore ───────────────────────────────────────────────────────────

void KnowledgeStore::remember(const KnownFact& f) {
    for (KnownFact& k : facts) {
        if (k.subjectId == f.subjectId && k.predicate == f.predicate) {
            if (f.confidence >= k.confidence) k = f;   // fresher/firmer wins
            return;
        }
    }
    if ((int)facts.size() >= MAX_FACTS) {
        // Forget the least confident, oldest fact (deterministic scan).
        int worst = 0;
        for (int i = 1; i < (int)facts.size(); ++i)
            if (facts[i].confidence < facts[worst].confidence ||
                (facts[i].confidence == facts[worst].confidence &&
                 facts[i].day < facts[worst].day))
                worst = i;
        facts.erase(facts.begin() + worst);
    }
    facts.push_back(f);
}

const KnownFact* KnowledgeStore::mostSalient(int today) const {
    const KnownFact* best = nullptr;
    float bestScore = 0.0f;
    for (const KnownFact& k : facts) {
        float freshness = std::exp(-(float)std::max(0, today - k.day) / 60.0f);
        float drama = (k.predicate == FACT_KILLED || k.predicate == FACT_DANGER)
                      ? 1.5f : 1.0f;
        float s = k.confidence * freshness * drama;
        if (s > bestScore) { bestScore = s; best = &k; }
    }
    return bestScore > 0.15f ? best : nullptr;   // stale trivia isn't worth telling
}

float KnowledgeStore::accuracy() const {
    float num = 0.0f, den = 0.0f;
    for (const KnownFact& k : facts) {
        num += k.isTrue ? k.confidence : 0.0f;
        den += k.confidence;
    }
    return den > 0.0f ? num / den : 1.0f;
}

// ─── A2/E1: priors loading ────────────────────────────────────────────────────

void mind::loadPriors(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return;                     // absent file = engine defaults
    std::string line;
    auto val = [&](const std::string& l, size_t o) { return std::stof(l.substr(o)); };
    while (std::getline(in, line)) {
        try {
            if      (line.rfind("W_REQUIRE:", 0) == 0) g_priors.wRequire = val(line, 10);
            else if (line.rfind("W_NEED:",    0) == 0) g_priors.wNeed    = val(line, 7);
            else if (line.rfind("W_MEMBIAS:", 0) == 0) g_priors.wMemBias = val(line, 10);
            else if (line.rfind("W_VARIETY:", 0) == 0) g_priors.wVariety = val(line, 10);
            else if (line.rfind("W_SOCIAL:",  0) == 0) g_priors.wSocial  = val(line, 9);
            else if (line.rfind("M_CONTEXT:", 0) == 0) g_priors.mContext = val(line, 10);
            else if (line.rfind("M_PERSONA:", 0) == 0) g_priors.mPersona = val(line, 10);
            else if (line.rfind("M_VALUE:",   0) == 0) g_priors.mValue   = val(line, 8);
            else if (line.rfind("M_GRIEF:",   0) == 0) g_priors.mGrief   = val(line, 8);
            else if (line.rfind("M_PHEROM:",  0) == 0) g_priors.mPherom  = val(line, 9);
            else if (line.rfind("M_ENV:",     0) == 0) g_priors.mEnv     = val(line, 6);
            else if (line.rfind("M_NORM:",    0) == 0) g_priors.mNorm    = val(line, 7);
            else if (line.rfind("M_EMOTION:", 0) == 0) g_priors.mEmotion = val(line, 10);
            else if (line.rfind("M_INTENT:",  0) == 0) g_priors.mIntent  = val(line, 9);
            else if (line.rfind("VERSION:",   0) == 0) g_priors.version  = (int)val(line, 8);
        } catch (...) { /* malformed line: keep default */ }
    }
    g_priors.loaded = true;
    std::cout << "PRIORS: loaded v" << g_priors.version << " from " << path << "\n";
}

// ─── Per-tick upkeep: appraisal (B1), sleep (D2), homesickness (D5) ───────────

void mind::upkeep(Entity* e, const std::vector<Entity*>& group,
                  bool isNight, int simDay) {
    if (!e) return;
    EmotionState& em = e->emotions;
    em.decay();
    e->skills.decayTick();

    // ── B1 appraisal from current state (OCC: undesirable+uncontrollable=fear,
    //    progress=joy/hope, rival prosperity=envy...) ─────────────────────────
    // Fear: bodily peril the agent can't will away.
    if (e->entityHealth < 30.0f || e->entityHunger > 80.0f) {
        float before = em.fear;
        em.fear = clamp100(em.fear + 4.0f +
                           e->personality.neuroticism * 0.03f);
        if (before < 20.0f && em.fear >= 20.0f) ++g_mindStats.fearEpisodes;
    }
    // D1-lite: darkness + isolation reads as danger to an anxious mind.
    if (isNight && group.size() <= 1 && e->personality.neuroticism > 60.0f)
        em.fear = clamp100(em.fear + 1.5f);

    // Joy/hope: things going well.
    if (e->entityHapiness > 70.0f && e->entityStress < 40.0f) {
        float before = em.joy;
        em.joy  = clamp100(em.joy + 2.0f);
        em.hope = clamp100(em.hope + 1.0f);
        if (before < 20.0f && em.joy >= 20.0f) ++g_mindStats.joyEpisodes;
    }

    // Envy: a nearby acquaintance visibly better off (wealth or standing) while
    // I struggle. Scan is capped — envy needs a face, not a census.
    if (e->entityHapiness < 45.0f) {
        int scanned = 0;
        for (Entity* nb : group) {
            if (nb == e || !nb || nb->entityHealth <= 0.0f) continue;
            if (++scanned > 6) break;
            if (nb->auctoritas > e->auctoritas + 25.0f ||
                nb->salary.token > e->salary.token * 2.0f + 100.0f) {
                float before = em.envy;
                em.envy = clamp100(em.envy + 1.5f +
                                   (100.0f - e->personality.agreeableness) * 0.02f);
                if (before < 20.0f && em.envy >= 20.0f) ++g_mindStats.envyEpisodes;
                break;
            }
        }
    }

    // Grief bleeds into sadness (the discrete channel narrative can name).
    float grief = e->getGriefIntensity();
    if (grief > 0.3f) em.sadness = clamp100(em.sadness + grief * 2.5f);

    // ── Emotion → body feedback: strong emotions are not free ────────────────
    if (em.fear > 60.0f)   e->entityStress = clamp100(e->entityStress + 0.8f);
    if (em.shame > 60.0f || em.guilt > 60.0f)
        e->entityMentalHealth = clamp100(e->entityMentalHealth - 0.06f);
    if (em.joy > 60.0f)    e->entityStress = clamp100(e->entityStress - 0.6f);

    // ── D2: sleep pressure ────────────────────────────────────────────────────
    e->sleepPressure = clamp100(e->sleepPressure + 3.0f + e->fatigueLevel * 0.02f
                                                 + e->entityStress * 0.01f);
    if (e->sleepPressure > 75.0f) {
        // Sleep-deprived: irritable, foggy, fragile.
        e->entityStress       = clamp100(e->entityStress + 1.2f);
        e->entityMentalHealth = clamp100(e->entityMentalHealth - 0.10f);
        e->entityGeneralAnger = clamp100(e->entityGeneralAnger + 0.5f);
        if (e->sleepPressure >= 99.0f) ++g_mindStats.sleeplessNights;
    }

    // ── D5: place attachment / homesickness ──────────────────────────────────
    if (e->tribeId != e->lastTribeIdSeen) {
        if (e->lastTribeIdSeen != -2)          // -2 = first tick, not a "move"
            e->tribeSwitchDay = simDay;
        e->lastTribeIdSeen = e->tribeId;
    }
    int sinceMove = simDay - e->tribeSwitchDay;
    if (e->tribeSwitchDay >= 0 && sinceMove >= 0 && sinceMove < 60) {
        // Uprooted: the old fires still burn in memory.
        float pang = (1.0f - sinceMove / 60.0f) * e->homeAttachment / 100.0f;
        e->entityStress = clamp100(e->entityStress + pang * 0.8f);
        em.sadness      = clamp100(em.sadness + pang * 1.2f);
    } else {
        // Roots deepen where life happens among familiar faces.
        int familiar = 0, scanned = 0;
        for (Entity* nb : group) {
            if (nb == e || !nb) continue;
            if (++scanned > 8) break;
            if (e->searchConnSocial(nb) > 30.0f) ++familiar;
        }
        e->homeAttachment = clamp100(e->homeAttachment + 0.05f + familiar * 0.05f);
    }
}

// ─── B1: emotion → action tendencies (the 13th scoring factor) ────────────────

float mind::emotionActionModifier(const Entity* e, const std::string& an) {
    if (!e) return 1.0f;
    const EmotionState& em = e->emotions;
    float m = 1.0f;

    // Fear: flight, safety, appeal to higher powers; not romance or art.
    if (em.fear > 15.0f) {
        float f = em.fear / 100.0f;
        if (an == "IgnoreAvoid" || an == "SetBoundaries" || an == "Anxiety") m *= 1.0f + f * 0.9f;
        if (an == "Prayer" || an == "Rest")                                  m *= 1.0f + f * 0.5f;
        if (an == "Flirt" || an == "Date" || an == "CreativeActivity" ||
            an == "Gaming" || an == "WatchEntertainment")                    m *= 1.0f - f * 0.5f;
    }
    // Joy: share it, make things, bond; despair-actions become unthinkable.
    if (em.joy > 15.0f) {
        float f = em.joy / 100.0f;
        if (an == "Socialize" || an == "GoodConnection" || an == "CreativeActivity" ||
            an == "HelpSupport")                                             m *= 1.0f + f * 0.6f;
        if (an == "SelfHarm" || an == "Suicide" || an == "QuitGiveUp")       m *= 1.0f - f * 0.8f;
    }
    // Sadness: withdraw, rest, look for meaning or help.
    if (em.sadness > 15.0f) {
        float f = em.sadness / 100.0f;
        if (an == "Rest" || an == "Prayer" || an == "SeekTherapy")           m *= 1.0f + f * 0.7f;
        if (an == "Socialize" || an == "Gaming")                             m *= 1.0f - f * 0.3f;
    }
    // Shame: hide and appease; the last thing wanted is an audience.
    if (em.shame > 15.0f) {
        float f = em.shame / 100.0f;
        if (an == "IgnoreAvoid" || an == "Apologize")                        m *= 1.0f + f * 0.8f;
        if (an == "Socialize" || an == "Flirt" || an == "Gossip")            m *= 1.0f - f * 0.5f;
    }
    // Guilt: repair the harm.
    if (em.guilt > 15.0f) {
        float f = em.guilt / 100.0f;
        if (an == "Apologize" || an == "Reconcile" || an == "HelpSupport")   m *= 1.0f + f * 1.0f;
        if (an == "Murder" || an == "Betray" || an == "Insult")              m *= 1.0f - f * 0.6f;
    }
    // Envy: climb or sabotage.
    if (em.envy > 15.0f) {
        float f = em.envy / 100.0f;
        if (an == "Work on Project" || an == "LearnSkill" ||
            an == "Basic Manual Work")                                       m *= 1.0f + f * 0.6f;
        if (an == "Insult" || an == "Manipulate" || an == "Gossip")          m *= 1.0f + f * 0.4f;
    }
    // Gratitude: give back.
    if (em.gratitude > 15.0f) {
        float f = em.gratitude / 100.0f;
        if (an == "HelpSupport" || an == "GoodConnection" || an == "Socialize")
                                                                              m *= 1.0f + f * 0.7f;
    }
    // Pride: keep doing the thing that worked, and let people see it.
    if (em.pride > 15.0f) {
        float f = em.pride / 100.0f;
        if (an == "Work on Project" || an == "Socialize" || an == "LearnSkill")
                                                                              m *= 1.0f + f * 0.4f;
    }
    // Hope: invest in tomorrow.
    if (em.hope > 15.0f) {
        float f = em.hope / 100.0f;
        if (an == "LearnSkill" || an == "CreativeActivity" || an == "Farm")  m *= 1.0f + f * 0.4f;
    }
    // Regret: process it, make amends, don't wallow into worse.
    if (em.regret > 15.0f) {
        float f = em.regret / 100.0f;
        if (an == "Apologize" || an == "SeekTherapy" || an == "Prayer")      m *= 1.0f + f * 0.6f;
        if (an == "DrinkAlcohol")                                            m *= 1.0f + f * 0.3f; // the human failure mode
    }
    // D2: sleep debt makes bed the world's best idea; fresh minds don't nap.
    if (e->sleepPressure > 60.0f && (an == "Sleep" || an == "Rest"))
        m *= 1.0f + (e->sleepPressure - 60.0f) / 40.0f;
    else if (e->sleepPressure < 25.0f && an == "Sleep")
        m *= 0.5f;

    m = std::max(0.4f, std::min(2.2f, m));
    // LiveConfig kill switch (0 = feature off) and priors intensity.
    m = adjustFactor(m, g_liveConfig.emotionMul * g_priors.mEmotion);
    return m;
}

// ─── B4: intentions ───────────────────────────────────────────────────────────

void mind::updateIntention(Entity* e, int simDay) {
    if (!e) return;
    Intention& in = e->intention;

    // Abandon a stalled pursuit: 30 days without progress is a life lesson.
    if (in.active && in.lastProgressDay >= 0 && simDay - in.lastProgressDay > 30) {
        LifeMemory mem;
        mem.eventType = "abandoned_pursuit";
        mem.emotionalIntensity = 0.5f;
        mem.simulationDay = simDay;
        mem.internalNarrative = "I chased '" + in.type + "' and it came to nothing.";
        e->lifeMemories.push_back(mem);
        e->emotions.regret  = clamp100(e->emotions.regret + 20.0f);
        e->emotions.sadness = clamp100(e->emotions.sadness + 10.0f);
        in.active = false;
    }
    if (in.active) return;

    // Adopt the highest-priority life goal as the season's pursuit.
    const LifeGoal* top = nullptr;
    for (const LifeGoal& g : e->m_goals)
        if (!top || g.priority > top->priority) top = &g;
    if (!top) return;
    in.type            = top->type;
    in.sinceDay        = simDay;
    in.lastProgressDay = simDay;
    in.progress        = 0.0f;
    in.active          = true;
}

float mind::intentionModifier(const Entity* e, const std::string& an) {
    if (!e || !e->intention.active) return 1.0f;
    const std::string& t = e->intention.type;
    float m = 1.0f;
    if (t == "find_partner") {
        if (an == "Flirt" || an == "Date" || an == "couple") m = 1.35f;
        else if (an == "Socialize")                          m = 1.15f;
    } else if (t == "build_family") {
        if (an == "breeding" || an == "couple")              m = 1.3f;
        else if (an == "HelpSupport")                        m = 1.1f;
    } else if (t == "build_career") {
        if (an == "Work on Project" || an == "LearnSkill" ||
            an == "Basic Manual Work")                       m = 1.3f;
    } else if (t == "make_friends") {
        if (an == "Socialize" || an == "GoodConnection" ||
            an == "HelpSupport")                             m = 1.25f;
    } else if (t == "self") {
        if (an == "Prayer" || an == "Read" ||
            an == "CreativeActivity" || an == "SeekTherapy") m = 1.25f;
    } else if (t == "happiness") {
        if (an == "CreativeActivity" || an == "Socialize" ||
            an == "Gaming")                                  m = 1.15f;
    }
    return adjustFactor(m, g_priors.mIntent);
}

// ─── B3: active theory of mind ────────────────────────────────────────────────

float mind::threatPrediction(Entity* self, Entity* other, int simDay) {
    if (!self || !other) return 0.0f;
    // Visible channel: body language is public information.
    float visible = 0.0f;
    if (other->bodyLanguage == BodyLanguageCue::AGGRESSIVE) visible = 0.35f;
    else if (other->bodyLanguage == BodyLanguageCue::DOMINANT) visible = 0.12f;

    // Believed channel: my (possibly stale, possibly wrong) model of them.
    float believed = 0.0f;
    if (MentalModelOfOther* m = self->getModelOf(other)) {
        float conf = m->effectiveConfidence(simDay);
        believed = conf * ((m->estimatedAnger / 100.0f) * 0.5f +
                           ((50.0f - m->trustLevel) / 100.0f) * 0.4f);
    }
    // Reputational channel: what I've been told about them (C3 facts).
    float heard = 0.0f;
    for (const KnownFact& k : self->knowledge.facts) {
        if (k.subjectId == other->entityId &&
            (k.predicate == FACT_KILLED || k.predicate == FACT_DANGER)) {
            heard = std::max(heard, k.confidence * 0.4f);
        }
    }
    return clamp01f(visible + believed + heard);
}

bool mind::attemptDeception(Entity* deceiver, Entity* target, const std::string& an, int simDay) {
    if (!deceiver || !target) return false;
    // Gate: a low-integrity agent only bothers staging warmth when there is
    // something to eventually collect on — reuse the grudge cue that already
    // gates C3 lies, so "high stakes" means the same thing in both places.
    if (deceiver->integrity >= 35.0f) return false;
    bool grudge = false;
    for (const auto& a : deceiver->list_entityPointedAnger)
        if (a.pointedEntity == target && a.anger > 30.0f) { grudge = true; break; }
    if (!grudge) return false;
    if (BetterRand::genNrInInterval(0, 100) >= 30) return false; // not staged every time

    MentalModelOfOther* m = target->getModelOf(deceiver);
    if (!m) return false;
    // Fake warmth lands on top of the genuine sentiment bump already applied
    // by the caller — trust climbs further than the interaction earned, while
    // predictability (the target's TRUE read on them) erodes: the surface
    // signal and the underlying person are diverging, even if trust can't
    // see that yet.
    float boost = 3.0f + deceiver->personality.extraversion / 40.0f;
    m->trustLevel      = std::min(100.0f, m->trustLevel + boost);
    m->predictability  = std::max(0.0f, m->predictability - 0.02f);
    m->fakeTrustGain  += boost;
    m->fakedByThem     = true;
    ++g_mindStats.deceptionsAttempted;
    return true;
}

void mind::detectDeception(Entity* victim, Entity* offender, const std::string& an, int simDay) {
    if (!victim || !offender) return;
    MentalModelOfOther* m = victim->getModelOf(offender);
    if (!m) return;
    bool wasFooled = m->fakedByThem && m->fakeTrustGain > 5.0f;
    if (!wasFooled && m->trustLevel < 55.0f) return;  // nothing to snap

    // Prediction-error snap: the more trust had built up (genuinely or
    // manufactured), the harder detection craters it — this is what makes a
    // betrayal register as a shock rather than the graded per-anger update
    // the caller already applies alongside this.
    float priorTrust  = m->trustLevel;
    m->trustLevel     = std::min(m->trustLevel, wasFooled ? 5.0f : 15.0f);
    m->predictability = wasFooled ? 0.05f : std::min(m->predictability, 0.2f);
    m->confidence      = std::min(1.0f, m->confidence + 0.5f);   // now certain — painfully so
    m->perceivedIntentionality = std::min(1.0f, m->perceivedIntentionality + 0.4f);
    m->lastObservedDay = simDay;
    m->lastInteractionOutcome = "deception_revealed";
    m->fakedByThem   = false;
    m->fakeTrustGain = 0.0f;

    if (wasFooled) {
        ++g_mindStats.deceptionsDetected;
        float shock = std::min(1.0f, priorTrust / 60.0f);
        victim->emotions.shame = clamp100(victim->emotions.shame + 8.0f * shock); // fooled — stings
        float before = victim->emotions.fear;
        victim->emotions.fear = clamp100(victim->emotions.fear + 15.0f * shock);
        if (before < 20.0f && victim->emotions.fear >= 20.0f) ++g_mindStats.fearEpisodes;

        LifeMemory mem;
        mem.eventType = "deception_revealed";
        mem.entityInvolvedId = offender->entityId;
        mem.emotionalIntensity = 0.75f;
        mem.simulationDay = simDay;
        mem.internalNarrative = offender->getName() + " had fooled me completely — I never saw it coming.";
        victim->lifeMemories.push_back(mem);
    }
}

// ─── B2/C2/D3: post-action settlement ─────────────────────────────────────────

float mind::settleOutcome(Entity* e, const std::string& an, float outcome, int simDay) {
    if (!e) return 0.0f;
    EmotionState& em = e->emotions;

    // ── B2: expected vs actual ────────────────────────────────────────────────
    // Thresholds are asymmetric on purpose: outcomeSuccess is quantized in
    // 1/8 steps and stat caps pull lived results below history, so a symmetric
    // band makes regret chronic and relief unreachable (measured, run1/run2).
    float surprise = 0.0f;
    if (e->lastExpectedOutcome >= 0.0f) {
        surprise = outcome - e->lastExpectedOutcome;
        if (surprise < -0.35f) {
            // Regret: the imagined world was better than the lived one.
            em.regret = clamp100(em.regret + (-surprise) * 30.0f);
            ++g_mindStats.regrets;
            if (surprise < -0.5f) {
                LifeMemory mem;
                mem.eventType = "regret";
                mem.emotionalIntensity = std::min(0.8f, -surprise);
                mem.simulationDay = simDay;
                mem.internalNarrative = "I should not have chosen '" + an + "'.";
                e->lifeMemories.push_back(mem);
            }
        } else if (surprise > 0.12f) {
            // Relief/delight: better than dared hope.
            float before = em.joy;
            em.joy  = clamp100(em.joy + surprise * 20.0f);
            em.hope = clamp100(em.hope + surprise * 12.0f);
            ++g_mindStats.reliefs;
            if (before < 20.0f && em.joy >= 20.0f) ++g_mindStats.joyEpisodes;
        }
        e->lastExpectedOutcome = -1.0f;   // consumed
    }

    // Success feeds pride; a mean act that ALSO failed feeds guilt (no audience
    // information is available here, so shame is reserved for the moral cases
    // below where it is warranted regardless of witnesses).
    if (outcome > 0.7f) em.pride = clamp100(em.pride + 6.0f);
    if (outcome < 0.25f && (an == "Insult" || an == "Betray" || an == "Manipulate")) {
        float before = em.guilt;
        em.guilt = clamp100(em.guilt + 10.0f * (e->personality.agreeableness / 100.0f));
        if (before < 20.0f && em.guilt >= 20.0f) ++g_mindStats.guiltEpisodes;
    }
    // Moral emotions for harm done, success or not: conscience keeps books.
    if (an == "Murder" || an == "Betray") {
        float before = em.guilt;
        em.guilt = clamp100(em.guilt + 25.0f * (e->personality.agreeableness / 100.0f)
                                     + e->integrity * 0.1f);
        em.shame = clamp100(em.shame + 12.0f);
        if (before < 20.0f && em.guilt >= 20.0f) ++g_mindStats.guiltEpisodes;
        ++g_mindStats.shameEpisodes;
    }
    // Being helped → gratitude is settled at the RECEIVER in executeAction.

    // ── C2: learning by doing ─────────────────────────────────────────────────
    if      (an == "Hunt")              e->skills.practice(SK_HUNT,   1.2f);
    else if (an == "Gather")            e->skills.practice(SK_GATHER, 1.0f);
    else if (an == "Farm")              e->skills.practice(SK_FARM,   1.0f);
    else if (an == "Build" || an == "Work on Project" || an == "Basic Manual Work")
                                        e->skills.practice(SK_CRAFT,  0.9f);
    else if (an == "HelpSupport")       e->skills.practice(SK_HEAL,   0.5f);
    else if (an == "Duel" || an == "Raid" || an == "Murder")
                                        e->skills.practice(SK_FIGHT,  1.2f);
    else if (an == "Socialize" || an == "Gossip")
                                        e->skills.practice(SK_ORATORY, 0.4f);
    else if (an == "Read" || an == "LearnSkill" || an == "Prayer")
                                        e->skills.practice(SK_LORE,   0.7f);

    // ── D3: embodiment — violence and hard labour leave marks ────────────────
    if (an == "Duel" || an == "Raid") {
        if (BetterRand::genNrInInterval(0, 100) < 35)
            e->injuryLevel = clamp100(e->injuryLevel + BetterRand::genNrInInterval(10, 30));
    } else if (an == "Hunt") {
        if (BetterRand::genNrInInterval(0, 100) < 8)
            e->injuryLevel = clamp100(e->injuryLevel + BetterRand::genNrInInterval(5, 15));
    }
    if (e->injuryLevel > 0.0f) {
        // Wounds heal — faster with rest, slower when overworked; a bad wound
        // saps health and shows in the body.
        float heal = (an == "Rest" || an == "Sleep") ? 3.0f : 1.0f;
        heal *= (0.7f + e->genome.resilience * 0.3f);
        e->injuryLevel = std::max(0.0f, e->injuryLevel - heal);
        if (e->injuryLevel > 50.0f) {
            e->entityHealth = std::max(0.0f, e->entityHealth - 0.15f);
            e->bodyLanguage = BodyLanguageCue::WITHDRAWN;
        }
    }

    // ── D2: sleeping settles the day ──────────────────────────────────────────
    if (an == "Sleep") {
        float quality = clamp100(90.0f - e->entityStress * 0.4f
                                       - e->emotions.fear * 0.2f
                                       - e->injuryLevel * 0.15f);
        e->sleepQuality  = quality;
        e->sleepPressure = std::max(0.0f, e->sleepPressure - 40.0f * (quality / 70.0f));
        if (quality > 55.0f) {
            // Consolidation happens in sleep: episodes get their shot at
            // becoming beliefs while the body is offline.
            e->consolidateMemories(simDay);
            ++g_mindStats.deepSleeps;
        }
    }

    // ── B4: did this act advance the season's pursuit? ────────────────────────
    if (e->intention.active && outcome > 0.55f &&
        intentionModifier(e, an) > 1.05f) {
        e->intention.progress = clamp100(e->intention.progress + outcome * 6.0f);
        e->intention.lastProgressDay = simDay;
        if (e->intention.progress >= 100.0f) {
            em.pride = clamp100(em.pride + 25.0f);
            em.joy   = clamp100(em.joy + 15.0f);
            LifeMemory mem;
            mem.eventType = "fulfilled_pursuit";
            mem.emotionalIntensity = 0.7f;
            mem.simulationDay = simDay;
            mem.internalNarrative = "I set out to '" + e->intention.type + "' — and I did.";
            e->lifeMemories.push_back(mem);
            e->intention.active = false;
        }
    }
    return surprise;
}

// ─── C3: knowledge exchange ───────────────────────────────────────────────────

void mind::shareKnowledge(Entity* speaker, Entity* listener, int simDay) {
    if (!speaker || !listener || speaker == listener) return;
    const KnownFact* pick = speaker->knowledge.mostSalient(simDay);
    if (!pick) return;

    KnownFact told = *pick;
    told.sourceId = speaker->entityId;
    told.day      = simDay;
    // The telephone game: each retelling shaves confidence and smears value —
    // and occasionally corrupts the content outright (the wrong name sticks,
    // the place shifts a valley over). A retold fact can simply become false.
    told.confidence *= 0.85f;
    told.value += BetterRand::genNrInInterval(-1.0f, 1.0f) * 0.1f * std::fabs(told.value);
    if (told.isTrue && BetterRand::genNrInInterval(0, 100) < 8)
        told.isTrue = false;   // misremembered in transit

    // Lies: a low-integrity speaker with a grudge against the subject poisons
    // the record — the fact keeps its confident tone but loses its truth.
    if (speaker->integrity < 30.0f && told.subjectId >= 0) {
        bool grudge = false;
        for (const auto& a : speaker->list_entityPointedAnger)
            if (a.pointedEntity && a.pointedEntity->entityId == told.subjectId &&
                a.anger > 40.0f) { grudge = true; break; }
        if (grudge && BetterRand::genNrInInterval(0, 100) < 35) {
            told.predicate  = FACT_DANGER;
            told.value      = 1.0f;
            told.confidence = std::min(0.9f, told.confidence + 0.2f); // liars sound sure
            told.isTrue     = false;
            ++g_mindStats.liesTold;
        }
    }
    if (told.confidence < 0.12f) return;   // too diluted to land
    listener->knowledge.remember(told);
    ++g_mindStats.factsShared;
}

void mind::observeCrime(Entity* witness, Entity* offender, bool lethal, int simDay) {
    if (!witness || !offender) return;
    KnownFact f;
    f.subjectId  = offender->entityId;
    f.predicate  = FACT_KILLED;
    f.value      = lethal ? 1.0f : 0.5f;
    f.confidence = 0.95f;              // I saw it with my own eyes
    f.sourceId   = -1;
    f.day        = simDay;
    f.isTrue     = true;
    witness->knowledge.remember(f);
    float before = witness->emotions.fear;
    witness->emotions.fear = clamp100(witness->emotions.fear + (lethal ? 25.0f : 12.0f));
    if (before < 20.0f && witness->emotions.fear >= 20.0f) ++g_mindStats.fearEpisodes;
}

// ─── C2: skill-scaled subsistence yields ──────────────────────────────────────

float mind::skillYieldMul(const Entity* e, SkillId s) {
    if (!e) return 1.0f;
    return 0.8f + (e->skills.get(s) / 100.0f) * 0.7f;   // novice 0.8 → master 1.5
}

// ─── E2: report helpers ───────────────────────────────────────────────────────

float mind::populationBeliefAccuracy(const std::vector<Entity>& entities) {
    float num = 0.0f, den = 0.0f;
    for (const Entity& e : entities) {
        if (e.entityHealth <= 0.0f) continue;
        for (const KnownFact& k : e.knowledge.facts) {
            num += k.isTrue ? k.confidence : 0.0f;
            den += k.confidence;
        }
    }
    return den > 0.0f ? num / den : -1.0f;   // -1 = no facts in the world yet
}

float mind::skillGini(const std::vector<Entity>& entities) {
    // Gini over each agent's best skill: 0 = everyone identical.
    std::vector<float> best;
    for (const Entity& e : entities) {
        if (e.entityHealth <= 0.0f) continue;
        float b = 0.0f;
        for (int i = 0; i < SK_COUNT; ++i) b = std::max(b, e.skills.get((SkillId)i));
        best.push_back(b);
    }
    if (best.size() < 2) return 0.0f;
    std::sort(best.begin(), best.end());
    double cum = 0.0, weighted = 0.0;
    for (size_t i = 0; i < best.size(); ++i) {
        cum += best[i];
        weighted += best[i] * (double)(i + 1);
    }
    if (cum <= 0.0) return 0.0f;
    double n = (double)best.size();
    return (float)((2.0 * weighted) / (n * cum) - (n + 1.0) / n);
}
