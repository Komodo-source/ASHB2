#ifndef GENOME_H
#define GENOME_H

// ─── Genome — heritable physical traits (Upgrade Plan, Step 4) ────────────────
// Every trait is a multiplier around 1.0 applied at exactly one read site, so a
// genome of all-1.0 is bit-identical to the pre-genetics simulation. Traits are
// inherited by uniform crossover (each gene from a random parent) plus Gaussian
// mutation whose sigma is scaled by LiveConfig::mutationRateMul, and clamped to
// a viable band so mutation load can't produce absurd phenotypes.
//
// Read sites:
//   speed      → movement force        (Movement::applyMovement)
//   sightRange → perception radius     (ItemManager agent tick / vision queries)
//   metabolism → hunger growth rate    (ItemManager agent tick)
//   fertility  → conception odds       (implem_free_will breeding rolls)
//   resilience → disease/health damage (ItemManager agent tick famine damage)

#include <random>
#include <algorithm>

struct Genome {
    float speed      = 1.0f;
    float sightRange = 1.0f;
    float metabolism = 1.0f;
    float fertility  = 1.0f;
    float resilience = 1.0f;

    static float clampGene(float v) { return std::min(1.8f, std::max(0.4f, v)); }

    // Uniform crossover + Gaussian mutation. `mutationScale` is the live-tunable
    // multiplier (1.0 default); base sigma 0.035 keeps drift generational, not chaotic.
    template <typename Rng>
    static Genome combine(const Genome& a, const Genome& b, Rng& rng, float mutationScale) {
        std::uniform_int_distribution<int> pick(0, 1);
        std::normal_distribution<float>    mut(0.0f, 0.035f * std::max(0.0f, mutationScale));
        auto gene = [&](float ga, float gb) {
            float v = pick(rng) ? ga : gb;
            return clampGene(v + mut(rng));
        };
        Genome g;
        g.speed      = gene(a.speed,      b.speed);
        g.sightRange = gene(a.sightRange, b.sightRange);
        g.metabolism = gene(a.metabolism, b.metabolism);
        g.fertility  = gene(a.fertility,  b.fertility);
        g.resilience = gene(a.resilience, b.resilience);
        return g;
    }

    // Euclidean distance in gene space — the UI's genetic-similarity coloring
    // and the diversity telemetry both key off this.
    float distanceTo(const Genome& o) const {
        float d0 = speed - o.speed, d1 = sightRange - o.sightRange,
              d2 = metabolism - o.metabolism, d3 = fertility - o.fertility,
              d4 = resilience - o.resilience;
        return std::sqrt(d0*d0 + d1*d1 + d2*d2 + d3*d3 + d4*d4);
    }
};

#endif // GENOME_H
