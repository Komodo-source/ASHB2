#include "header/Drive.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Drive — bipolar homeostatic / novelty axis. See header for the model.
// ─────────────────────────────────────────────────────────────────────────────

float Drive::pressure() const {
    const float esp = effectiveSetpoint();
    const float err = esp - value;            // + => too low (raise), - => too high (lower)
    // Normalise by the survivable room between the setpoint and the lethal pole
    // on the relevant side, so the same absolute error feels more urgent when
    // there's less room before death.
    const float span = (err >= 0.0f) ? std::max(1e-3f, esp - deathLow)
                                      : std::max(1e-3f, deathHigh - esp);
    const float n = err / span;               // ~[-1,1] within the survivable range
    // Cube term keeps small deviations cheap and makes the approach to a lethal
    // pole explode — most behaviour fires only once a drive nears its edge.
    const float urg = n * (1.0f + 2.0f * n * n);
    return std::max(-1.0f, std::min(1.0f, urg));
}

float Drive::wellbeing() const {
    const float esp = effectiveSetpoint();
    const float d   = std::fabs(value - esp);
    const float toDeath = std::max(1e-3f, (value >= esp) ? (deathHigh - esp)
                                                         : (esp - deathLow));
    float w;
    if (d <= comfort) {
        w = 1.0f;                             // anywhere in the safe band is fine
    } else {
        // Linear fall-off from the comfort edge to the lethal edge.
        w = std::max(0.0f, 1.0f - (d - comfort) / std::max(1e-3f, toDeath - comfort));
    }
    // Chronic wear caps the wellbeing you can actually feel.
    return clamp01(w * (1.0f - 0.6f * load));
}

float Drive::tick(float dt, bool applyEntropy) {
    // 1. Entropy: slide toward the decay pole at a constant rate, so a neglected
    //    drive actually reaches its lethal edge (a proportional pull would only
    //    asymptote). Skipped in bridge mode, where an external owner moves value.
    if (applyEntropy) {
        const float step = driftRate * dt;
        if (driftPole > value) value = std::min(driftPole, value + step);
        else                   value = std::max(driftPole, value - step);
        value = clamp01(value);
    }

    // 2. Habituation (novelty axes only): held high => tolerance climbs; held low
    //    => it decays (abstinence resensitises). This is the dopamine treadmill.
    if (axis == DriveAxis::Novelty) {
        if (value > setpoint) tolerance += tolGain  * dt * (value - setpoint);
        else                  tolerance -= tolDecay * dt;
        tolerance = clamp01(tolerance);
    }

    // 3. Allostatic load: time spent in the danger zone (outside comfort, inside
    //    lethal) wears the body down; comfort sheds it slowly. This is the
    //    "short-term fine, sustained kills" texture (cortisol-style wear).
    const float esp = effectiveSetpoint();
    const float d   = std::fabs(value - esp);
    if (d <= comfort) {
        load = std::max(0.0f, load - loadRelief * dt);
    } else {
        const float toDeath = std::max(1e-3f, (value >= esp) ? (deathHigh - esp)
                                                             : (esp - deathLow));
        const float depth = (d - comfort) / std::max(1e-3f, toDeath - comfort); // 0..1+
        load = clamp01(load + loadGain * dt * std::max(0.0f, depth));
    }

    // 4. Damage this tick: chronic wear from standing load, plus acute damage
    //    that escalates once the value crosses either lethal pole.
    float dmg = chronicDamage * load * dt;
    if (value <= deathLow) {
        const float over = (deathLow - value) / std::max(1e-3f, deathLow + 1e-3f);
        dmg += acuteDamage * dt * (1.0f + over);
    }
    if (value >= deathHigh) {
        const float over = (value - deathHigh) / std::max(1e-3f, (1.0f - deathHigh) + 1e-3f);
        dmg += acuteDamage * dt * (1.0f + over);
    }
    return dmg;
}

void Drive::satisfy(float amount) {
    value = clamp01(value + amount);
    // Chasing the high builds tolerance faster than passive drift does.
    if (axis == DriveAxis::Novelty && amount > 0.0f) {
        tolerance = clamp01(tolerance + tolGain * 2.0f * amount);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  DriveSet
// ─────────────────────────────────────────────────────────────────────────────

float DriveSet::tick(float dt, bool applyEntropy) {
    float dmg = 0.0f;
    for (auto& kv : axes) dmg += kv.second.tick(dt, applyEntropy);
    return dmg;
}

std::string DriveSet::mostUrgent() const {
    std::string best;
    float bestMag = 0.0f;
    for (const auto& kv : axes) {
        const float mag = std::fabs(kv.second.pressure());
        if (mag > bestMag) { bestMag = mag; best = kv.first; }
    }
    return best;
}

float DriveSet::wellbeing() const {
    if (axes.empty()) return 1.0f;
    float sum = 0.0f;
    for (const auto& kv : axes) sum += kv.second.wellbeing();
    return sum / static_cast<float>(axes.size());
}

void DriveSet::initHuman(float openness, float extraversion,
                         float neuroticism, float conscientiousness) {
    const float o = openness          / 100.0f;
    const float e = extraversion      / 100.0f;
    const float n = neuroticism       / 100.0f;
    const float c = conscientiousness / 100.0f;

    // Neurotic minds tolerate less deviation before it hurts (narrower bands);
    // disciplined bodies decay a touch more slowly.
    const float bandScale  = 1.0f - 0.45f * n;   // 1.0 .. ~0.55
    const float bodyDrift  = 1.0f - 0.30f * c;   // 1.0 .. 0.70

    // ── BODY (homeostatic): low setpoint, entropy climbs toward the lethal high.
    {
        Drive& h = axes["hunger"];
        h.axis = DriveAxis::Homeostatic;
        h.setpoint = 0.18f; h.comfort = 0.20f * bandScale;
        h.deathLow = 0.0f;  h.deathHigh = 0.92f;     // starvation kills; can't starve from the low end
        h.driftPole = 1.0f; h.driftRate = 0.0065f * bodyDrift;
        h.value = h.setpoint;
    }
    {
        Drive& f = axes["fatigue"];
        f.axis = DriveAxis::Homeostatic;
        f.setpoint = 0.20f; f.comfort = 0.22f * bandScale;
        f.deathLow = 0.0f;  f.deathHigh = 0.96f;
        f.driftPole = 1.0f; f.driftRate = 0.012f * bodyDrift;
        f.acuteDamage = 0.06f;                        // exhaustion rarely kills outright
        f.value = f.setpoint;
    }

    // ── MIND (both poles lethal): decays toward deprivation; agent must push up.
    {
        Drive& s = axes["stimulation"];               // boredom <-> overwhelm
        s.axis = DriveAxis::Novelty;                   // habituates: curious minds tire of routine
        s.setpoint = 0.45f + 0.25f * o;                // open minds want more stimulation
        s.comfort = 0.16f * bandScale;
        s.deathLow = 0.05f; s.deathHigh = 0.95f;       // chronic boredom AND overload both kill
        s.driftPole = 0.0f; s.driftRate = 0.010f + 0.008f * o; // open minds get bored faster
        s.acuteDamage = 0.10f;
        s.value = s.setpoint;
    }
    {
        Drive& soc = axes["social"];                   // loneliness <-> social burnout
        soc.axis = DriveAxis::Homeostatic;             // no real habituation to company
        soc.setpoint = 0.30f + 0.45f * e;              // extraverts need far more contact
        soc.comfort = 0.18f * bandScale;
        soc.deathLow = 0.06f; soc.deathHigh = 0.94f;   // isolation kills; so does no solitude
        soc.driftPole = 0.0f; soc.driftRate = 0.008f + 0.010f * e; // extraverts deplete faster
        soc.acuteDamage = 0.12f;
        soc.value = soc.setpoint;
    }
    {
        Drive& p = axes["pleasure"];                   // anhedonia <-> mania / addiction
        p.axis = DriveAxis::Novelty;                   // the dopamine axis: strong habituation
        p.setpoint = 0.52f;
        p.comfort = 0.20f * bandScale;
        p.deathLow = 0.05f; p.deathHigh = 0.97f;       // chronic joylessness AND overload both kill
        p.driftPole = 0.30f; p.driftRate = 0.010f;     // pleasure fades back toward a dull baseline
        p.tolGain = 0.020f; p.tolDecay = 0.005f;       // down-regulates readily, resensitises slowly
        p.acuteDamage = 0.14f;
        p.value = p.setpoint;
    }
}
