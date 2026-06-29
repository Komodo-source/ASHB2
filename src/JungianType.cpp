#include "header/JungianType.h"
#include "header/Drive.h"

#include <random>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  Stack construction (Beebe ordering)
//
//  The 4 letters fully determine the stack:
//    • J/P says which function is EXTRAVERTED (J -> judging, P -> perceiving).
//    • E/I + that tells us the dominant's CATEGORY:
//        E -> dominant is the extraverted category;
//        I -> dominant is the OTHER (introverted) category.   <- the classic trap
//    • spine (1,4): same category, opposite function, opposite attitude.
//    • arms  (2,3): the other category, opposite function, opposite attitude.
//    • attitudes alternate I/E down the conscious stack; shadow (5-8) flips each.
// ─────────────────────────────────────────────────────────────────────────────
void JungianStack::buildStack() {
    const bool isE                 = (ei == 'E');
    const bool extravertedIsJudging = (jp == 'J');
    const bool domIsJudging         = isE ? extravertedIsJudging : !extravertedIsJudging;
    const bool domIsPerceiving      = !domIsJudging;

    const char domAtt = ei;
    const char oppAtt = (ei == 'E') ? 'I' : 'E';

    const char percL = sn;   // 'S' or 'N'
    const char judgL = tf;   // 'T' or 'F'

    // function builders: pick the function of a given category/attitude, or its
    // opposite-letter sibling within the same category.
    auto perc = [&](char att) -> CogFn {
        if (percL == 'S') return (att == 'E') ? CogFn::Se : CogFn::Si;
        else              return (att == 'E') ? CogFn::Ne : CogFn::Ni;
    };
    auto percOpp = [&](char att) -> CogFn {
        if (percL == 'S') return (att == 'E') ? CogFn::Ne : CogFn::Ni;   // flip S->N
        else              return (att == 'E') ? CogFn::Se : CogFn::Si;   // flip N->S
    };
    auto judg = [&](char att) -> CogFn {
        if (judgL == 'T') return (att == 'E') ? CogFn::Te : CogFn::Ti;
        else              return (att == 'E') ? CogFn::Fe : CogFn::Fi;
    };
    auto judgOpp = [&](char att) -> CogFn {
        if (judgL == 'T') return (att == 'E') ? CogFn::Fe : CogFn::Fi;   // flip T->F
        else              return (att == 'E') ? CogFn::Te : CogFn::Ti;   // flip F->T
    };

    CogFn dom, aux, ter, inf;
    if (domIsPerceiving) {
        dom = perc(domAtt);
        inf = percOpp(oppAtt);   // spine
        aux = judg(oppAtt);      // arm
        ter = judgOpp(domAtt);   // arm
    } else {
        dom = judg(domAtt);
        inf = judgOpp(oppAtt);
        aux = perc(oppAtt);
        ter = percOpp(domAtt);
    }

    fn[0] = dom; fn[1] = aux; fn[2] = ter; fn[3] = inf;
    fn[4] = fnFlipAttitude(dom);   // Opposing  (shadow of Hero)
    fn[5] = fnFlipAttitude(aux);   // Senex     (shadow of Parent)
    fn[6] = fnFlipAttitude(ter);   // Trickster (shadow of Child)
    fn[7] = fnFlipAttitude(inf);   // Demon     (shadow of Anima)

    // Developed strength: conscious functions taper 1..4, shadow weaker but the
    // Opposing Personality stays notably strong (it backs up the Hero).
    static const float st[8] = { 1.00f, 0.80f, 0.55f, 0.40f,
                                 0.45f, 0.35f, 0.28f, 0.22f };
    for (int i = 0; i < 8; ++i) strength[i] = st[i];

    built = true;
}

void JungianStack::deriveFromBigFive(float ext, float open, float agree,
                                     float consc, float neuro, unsigned seed) {
    std::mt19937 r(seed);
    std::uniform_real_distribution<float> noise(-12.0f, 12.0f);

    // Big Five maps cleanly onto the dichotomies; noise keeps borderline traits
    // from snapping to a single type and lets siblings differ.
    ei = (ext   + noise(r) >= 50.0f) ? 'E' : 'I';
    sn = (open  + noise(r) >= 50.0f) ? 'N' : 'S';   // openness -> iNtuition
    tf = (agree + noise(r) >= 50.0f) ? 'F' : 'T';   // agreeableness -> Feeling
    jp = (consc + noise(r) >= 50.0f) ? 'J' : 'P';   // conscientiousness -> Judging

    // Neurotic psyches fall into the grip far sooner; stable ones rarely do.
    gripThreshold = std::max(0.30f, std::min(0.80f, 0.70f - (neuro / 100.0f) * 0.35f));

    buildStack();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Function -> drive shaping
// ─────────────────────────────────────────────────────────────────────────────
void JungianStack::applyToDrives(DriveSet& d) const {
    if (!d.has("stimulation")) return;
    Drive& stim = d["stimulation"];
    Drive& soc  = d["social"];
    Drive& ple  = d["pleasure"];
    Drive& hun  = d["hunger"];
    Drive& fat  = d["fatigue"];

    for (int i = 0; i < 4; ++i) {           // conscious functions only
        const float w = strength[i];
        switch (fn[i]) {
            case CogFn::Se:                 // raw sensation: craves stimulation
                stim.setpoint += 0.10f * w; stim.driftRate += 0.004f * w; break;
            case CogFn::Si:                 // stability: tight, steady, routine
                stim.setpoint -= 0.06f * w;
                stim.comfort  *= (1.0f - 0.10f * w);
                hun.comfort   *= (1.0f - 0.08f * w);
                fat.comfort   *= (1.0f - 0.08f * w); break;
            case CogFn::Ne:                 // novelty: ideas, possibilities, restless
                stim.setpoint += 0.08f * w; stim.tolGain += 0.004f * w; break;
            case CogFn::Ni:                 // inward vision: less external input needed
                stim.setpoint += 0.03f * w; soc.driftRate *= (1.0f - 0.10f * w); break;
            case CogFn::Te:                 // task over people
                soc.setpoint  -= 0.05f * w; break;
            case CogFn::Ti:                 // inward analysis
                soc.setpoint  -= 0.04f * w; stim.setpoint += 0.03f * w; break;
            case CogFn::Fe:                 // harmony: needs company, wide social band
                soc.setpoint  += 0.10f * w; soc.comfort *= (1.0f + 0.08f * w); break;
            case CogFn::Fi:                 // inner values: authenticity over contact
                ple.setpoint  += 0.05f * w; soc.driftRate *= (1.0f - 0.08f * w); break;
            default: break;
        }
    }

    auto settle = [](Drive& x) {
        x.setpoint = std::min(0.85f, std::max(0.10f, x.setpoint));
        x.value    = x.setpoint;   // start a fresh entity at its own comfort
    };
    settle(stim); settle(soc); settle(ple);
    hun.value = hun.setpoint; fat.value = fat.setpoint;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Grip / shadow resolution
// ─────────────────────────────────────────────────────────────────────────────
Archetype JungianStack::resolveActive(float load, unsigned flags) {
    inGrip = (load >= gripThreshold);
    if (!inGrip) { active = Archetype::Hero; return active; }

    // In the grip, the inferior (Anima) erupts by default.
    Archetype a = Archetype::Anima;

    const float extreme = gripIntensity(load);   // 0..1 past the threshold
    if (extreme > 0.5f) {
        // Deep load: context summons a specific shadow archetype.
        if      (flags & GT_THREAT)     a = Archetype::Demon;
        else if (flags & GT_DOUBLEBIND) a = Archetype::Trickster;
        else if (flags & GT_AUTHORITY)  a = Archetype::Senex;
        else if (flags & GT_CHALLENGED) a = Archetype::Opposing;
    } else {
        // Shallow grip: only the most reactive shadows surface.
        if      (flags & GT_CHALLENGED) a = Archetype::Opposing;
        else if (flags & GT_AUTHORITY)  a = Archetype::Senex;
    }

    active = a;
    return active;
}

std::string JungianStack::gripNarrative() const {
    const std::string inf = fnLabel(inferior());
    switch (active) {
        case Archetype::Hero:
            return "";
        case Archetype::Anima:
            return "[grip] falling out of " + std::string(fnLabel(dominant())) +
                   ", swamped by inferior " + inf + " — the thing they're worst at.";
        case Archetype::Opposing:
            return "[shadow] the Opposing Personality digs in — stubborn, "
                   "contrarian, refusing the very thing that would help.";
        case Archetype::Senex:
            return "[shadow] the Senex turns cold and judging, withholding and "
                   "belittling where the Parent would have nurtured.";
        case Archetype::Trickster:
            return "[shadow] the Trickster sets a double bind — slippery, "
                   "sabotaging, making sure no choice can win.";
        case Archetype::Demon:
            return "[shadow] the Demon surfaces — the destructive edge of " + inf +
                   ", capable of burning down self or others.";
    }
    return "";
}
