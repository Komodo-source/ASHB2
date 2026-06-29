#ifndef JUNGIAN_TYPE_H
#define JUNGIAN_TYPE_H

#include <string>
#include <algorithm>

struct DriveSet;   // defined in Drive.h; only referenced by pointer/ref here

// ─────────────────────────────────────────────────────────────────────────────
//  JungianStack — Jung's 8 cognitive functions ordered into John Beebe's
//  eight-archetype model.
//
//  Each of the 16 types has a function stack. Positions 1-4 are ego-syntonic;
//  5-8 are the shadow, each the SAME function as its conscious partner with the
//  OPPOSITE attitude (1&5, 2&6, 3&7, 4&8).
//
//     1 Hero      2 Parent    3 Child     4 Anima/Animus   (conscious)
//     5 Opposing  6 Senex     7 Trickster 8 Demon          (shadow)
//
//  spine = 1<->4 (Hero/Anima), arms = 2<->3 (Parent/Child).
//
//  Behaviourally the payoff is the GRIP: under psychic load the ego falls out of
//  the Hero and into the inferior (4); under extreme, context-specific load the
//  shadow archetypes erupt. That maps directly onto the Drive `load` accumulator
//  — chronic tension literally changes who is driving.
// ─────────────────────────────────────────────────────────────────────────────

enum class CogFn : unsigned char {
    Se, Si, Ne, Ni, Te, Ti, Fe, Fi, None
};

enum class Archetype : unsigned char {
    Hero, Parent, Child, Anima,        // 1-4 conscious
    Opposing, Senex, Trickster, Demon  // 5-8 shadow
};

// Context triggers that, under extreme load, summon a specific shadow archetype.
enum GripTrigger : unsigned {
    GT_NONE       = 0u,
    GT_CHALLENGED = 1u << 0,  // contradiction / competition  -> Opposing
    GT_AUTHORITY  = 1u << 1,  // judging / controlling others -> Senex
    GT_DOUBLEBIND = 1u << 2,  // conflicting bonds or goals    -> Trickster
    GT_THREAT     = 1u << 3   // existential threat            -> Demon
};

// ── small free helpers (reusable by UI / narrative) ──────────────────────────
inline bool fnIsExtraverted(CogFn f) {
    return f == CogFn::Se || f == CogFn::Ne || f == CogFn::Te || f == CogFn::Fe;
}
inline bool fnIsPerceiving(CogFn f) {
    return f == CogFn::Se || f == CogFn::Si || f == CogFn::Ne || f == CogFn::Ni;
}
inline const char* fnLabel(CogFn f) {
    switch (f) {
        case CogFn::Se: return "Se"; case CogFn::Si: return "Si";
        case CogFn::Ne: return "Ne"; case CogFn::Ni: return "Ni";
        case CogFn::Te: return "Te"; case CogFn::Ti: return "Ti";
        case CogFn::Fe: return "Fe"; case CogFn::Fi: return "Fi";
        default:        return "--";
    }
}
inline CogFn fnFlipAttitude(CogFn f) {
    switch (f) {
        case CogFn::Se: return CogFn::Si; case CogFn::Si: return CogFn::Se;
        case CogFn::Ne: return CogFn::Ni; case CogFn::Ni: return CogFn::Ne;
        case CogFn::Te: return CogFn::Ti; case CogFn::Ti: return CogFn::Te;
        case CogFn::Fe: return CogFn::Fi; case CogFn::Fi: return CogFn::Fe;
        default:        return CogFn::None;
    }
}
inline const char* archetypeName(Archetype a) {
    switch (a) {
        case Archetype::Hero:      return "Hero";
        case Archetype::Parent:    return "Parent";
        case Archetype::Child:     return "Child";
        case Archetype::Anima:     return "Anima/Animus";
        case Archetype::Opposing:  return "Opposing Personality";
        case Archetype::Senex:     return "Senex/Witch";
        case Archetype::Trickster: return "Trickster";
        case Archetype::Demon:     return "Demon";
    }
    return "?";
}

struct JungianStack {
    // ── 4-letter type (derived from Big Five) ─────────────────────────────────
    char ei = 'I';  // E / I
    char sn = 'N';  // S / N  (which perceiving function)
    char tf = 'T';  // T / F  (which judging function)
    char jp = 'J';  // J / P  (which function is extraverted)

    // ── the ordered stack ─────────────────────────────────────────────────────
    CogFn fn[8]      = { CogFn::None };  // position 0..7
    float strength[8] = { 0 };           // developed strength per position [0,1]
    bool  built = false;

    // ── grip / shadow runtime state ───────────────────────────────────────────
    float     gripThreshold = 0.55f;       // psychic load at which the inferior takes over
    Archetype active        = Archetype::Hero;
    bool      inGrip        = false;

    // Build the 8-function stack + strengths from the 4 letters (Beebe order).
    void buildStack();

    // Pick the 4 letters from Big Five (probabilistic), set gripThreshold from
    // neuroticism, then build the stack. seed keeps it deterministic per entity.
    void deriveFromBigFive(float ext, float open, float agree, float consc,
                           float neuro, unsigned seed);

    CogFn dominant()  const { return fn[0]; }
    CogFn auxiliary() const { return fn[1]; }
    CogFn tertiary()  const { return fn[2]; }
    CogFn inferior()  const { return fn[3]; }
    std::string typeCode() const { return std::string() + ei + sn + tf + jp; }

    // Shape the drive setpoints/bands from the conscious functions (Se-hero wants
    // stimulation, Si wants stability, Fe wants company, …). Call once after both
    // personality and drives are initialised.
    void applyToDrives(DriveSet& drives) const;

    // Resolve who is driving from psychic load + context flags. Sets active/inGrip.
    Archetype resolveActive(float load01, unsigned contextFlags);

    // 0 at the grip threshold, 1 at maximum load — how strongly the grip/shadow
    // expresses this tick.
    float gripIntensity(float load01) const {
        if (load01 <= gripThreshold) return 0.0f;
        return std::min(1.0f, (load01 - gripThreshold) /
                              std::max(1e-3f, 1.0f - gripThreshold));
    }

    // First-person note for the inner monologue / narrative when not Hero-led.
    std::string gripNarrative() const;
};

#endif // JUNGIAN_TYPE_H
