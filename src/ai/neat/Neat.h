#ifndef NEAT_H
#define NEAT_H

// ─── NEAT — NeuroEvolution of Augmenting Topologies (Upgrade Plan, Step 5a) ───
// Lightweight NEAT after Stanley & Miikkulainen (2002): genomes are lists of
// connection genes with historical innovation numbers; networks grow by
// structural mutation (add-connection, add-node) and adapt by weight
// perturbation; offspring genomes are built by innovation-aligned crossover.
// There is NO backpropagation — selection pressure is survival/reproduction:
// agents that starve never reproduce, so their brains leave the gene pool.
//
// Feed-forwardness is guaranteed structurally: every node carries a `layer`
// coordinate (inputs 0.0, outputs 1.0, hidden nodes split the difference) and
// connections are only ever created from a lower to a strictly higher layer.
// Evaluation is a single pass over connections sorted by target layer.
//
// Sensor/actuator contract (fixed widths so genomes stay crossover-compatible):
//   inputs  [0] hunger/100      [1] health/100     [2] fatigue/100
//           [3] foodPher gx     [4] foodPher gy    [5] dangerPher/100
//           [6] holdsFood(0|1)  [7] loneliness/100 [8] bias (=1)
//   outputs [0] moveX(tanh) [1] moveY(tanh) [2] forageUrge(σ) [3] eatUrge(σ)

#include <vector>
#include <map>
#include <random>
#include <cmath>
#include <cstdint>
#include <fstream>

namespace neat {

constexpr int N_INPUTS  = 9;   // includes bias
constexpr int N_OUTPUTS = 4;

struct NodeGene {
    int   id    = 0;
    float layer = 0.0f;   // 0=input, 1=output, hidden in between
};

struct ConnGene {
    int   in = 0, out = 0;
    float weight  = 0.0f;
    bool  enabled = true;
    int   innovation = 0;
};

// Global historical-marking registry: the same (in,out) structural mutation
// gets the same innovation number in every genome, which is what makes
// crossover alignment meaningful.
struct InnovationDB {
    int nextInnovation = 0;
    int nextNodeId     = N_INPUTS + N_OUTPUTS;
    std::map<std::pair<int,int>, int> known;

    int innovationFor(int in, int out) {
        auto key = std::make_pair(in, out);
        auto it = known.find(key);
        if (it != known.end()) return it->second;
        known[key] = nextInnovation;
        return nextInnovation++;
    }
    void reset() { nextInnovation = 0; nextNodeId = N_INPUTS + N_OUTPUTS; known.clear(); }
};

InnovationDB& innovations();   // process-wide registry

struct Genome {
    std::vector<NodeGene> nodes;   // inputs+outputs always present, then hidden
    std::vector<ConnGene> conns;

    bool  empty() const { return nodes.empty(); }

    // Minimal starter topology: every input wired to every output with small
    // random weights (the canonical NEAT "fully connected, no hidden" start).
    template <typename Rng>
    static Genome minimal(Rng& rng) {
        std::uniform_real_distribution<float> w(-1.0f, 1.0f);
        Genome g;
        for (int i = 0; i < N_INPUTS; ++i)  g.nodes.push_back({i, 0.0f});
        for (int o = 0; o < N_OUTPUTS; ++o) g.nodes.push_back({N_INPUTS + o, 1.0f});
        for (int i = 0; i < N_INPUTS; ++i)
            for (int o = 0; o < N_OUTPUTS; ++o)
                g.conns.push_back({i, N_INPUTS + o, w(rng) * 0.5f, true,
                                   innovations().innovationFor(i, N_INPUTS + o)});
        return g;
    }

    // Evaluate: activations propagate in layer order. `in` must have N_INPUTS
    // entries (bias already set by caller); `out` receives N_OUTPUTS values,
    // moveX/moveY through tanh, urges through sigmoid.
    void evaluate(const float* in, float* out) const;

    // Weight perturbation + structural mutation. `scale` is the live mutation
    // knob (LiveConfig::mutationRateMul).
    template <typename Rng>
    void mutate(Rng& rng, float scale);

    // Innovation-aligned crossover; `a` is treated as the fitter parent
    // (disjoint/excess genes are taken from it).
    template <typename Rng>
    static Genome crossover(const Genome& a, const Genome& b, Rng& rng);

    // Structural distance in [0,∞): used for the genetic-similarity overlay
    // and diversity telemetry (compatible with NEAT's δ = c1·E/N + c3·W̄).
    static float distance(const Genome& a, const Genome& b);

    void saveTo(std::ofstream& f) const;
    bool loadFrom(std::ifstream& f);

private:
    float layerOf(int nodeId) const {
        for (const NodeGene& n : nodes) if (n.id == nodeId) return n.layer;
        return 0.0f;
    }
    template <typename Rng> void mutateAddConnection(Rng& rng);
    template <typename Rng> void mutateAddNode(Rng& rng);
};

// ── template method definitions ───────────────────────────────────────────────

template <typename Rng>
void Genome::mutate(Rng& rng, float scale) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    std::normal_distribution<float>       perturb(0.0f, 0.5f * std::max(0.0f, scale));
    std::uniform_real_distribution<float> fresh(-1.0f, 1.0f);
    for (ConnGene& c : conns) {
        if (roll(rng) < 0.8f) {
            if (roll(rng) < 0.9f) c.weight += perturb(rng);   // nudge
            else                  c.weight  = fresh(rng);     // replace
            c.weight = std::max(-4.0f, std::min(4.0f, c.weight));
        }
    }
    if (roll(rng) < 0.05f * scale) mutateAddConnection(rng);
    if (roll(rng) < 0.03f * scale) mutateAddNode(rng);
}

template <typename Rng>
void Genome::mutateAddConnection(Rng& rng) {
    if (nodes.size() < 2) return;
    std::uniform_int_distribution<int> pick(0, (int)nodes.size() - 1);
    for (int attempt = 0; attempt < 8; ++attempt) {
        const NodeGene& src = nodes[pick(rng)];
        const NodeGene& dst = nodes[pick(rng)];
        if (src.layer >= dst.layer) continue;              // forward only
        bool exists = false;
        for (const ConnGene& c : conns)
            if (c.in == src.id && c.out == dst.id) { exists = true; break; }
        if (exists) continue;
        std::uniform_real_distribution<float> w(-1.0f, 1.0f);
        conns.push_back({src.id, dst.id, w(rng), true,
                         innovations().innovationFor(src.id, dst.id)});
        return;
    }
}

template <typename Rng>
void Genome::mutateAddNode(Rng& rng) {
    if (conns.empty()) return;
    std::uniform_int_distribution<int> pick(0, (int)conns.size() - 1);
    ConnGene& split = conns[pick(rng)];
    if (!split.enabled) return;
    split.enabled = false;
    int   nid   = innovations().nextNodeId++;
    float layer = (layerOf(split.in) + layerOf(split.out)) * 0.5f;
    nodes.push_back({nid, layer});
    // in→new keeps weight 1, new→out inherits the old weight (NEAT convention:
    // the split is initially behavior-preserving modulo the activation).
    conns.push_back({split.in, nid, 1.0f, true, innovations().innovationFor(split.in, nid)});
    conns.push_back({nid, split.out, split.weight, true, innovations().innovationFor(nid, split.out)});
}

template <typename Rng>
Genome Genome::crossover(const Genome& a, const Genome& b, Rng& rng) {
    std::uniform_int_distribution<int> coin(0, 1);
    Genome child;
    child.nodes = a.nodes;   // fitter parent's node set covers its conn genes
    std::map<int, const ConnGene*> bByInnov;
    for (const ConnGene& c : b.conns) bByInnov[c.innovation] = &c;
    for (const ConnGene& ca : a.conns) {
        auto it = bByInnov.find(ca.innovation);
        if (it != bByInnov.end() && coin(rng)) {
            ConnGene g = *it->second;
            // Gene may reference a hidden node the fitter parent lacks — keep
            // topology consistent by falling back to the fitter parent's gene.
            bool known = true;
            for (int nid : {g.in, g.out}) {
                bool found = false;
                for (const NodeGene& n : child.nodes) if (n.id == nid) { found = true; break; }
                if (!found) known = false;
            }
            child.conns.push_back(known ? g : ca);
        } else {
            child.conns.push_back(ca);   // matching (other coin face), disjoint, excess
        }
    }
    return child;
}

} // namespace neat

#endif // NEAT_H
