#include "Neat.h"

#include <algorithm>

namespace neat {

InnovationDB& innovations() {
    static InnovationDB db;
    return db;
}

void Genome::evaluate(const float* in, float* out) const {
    // Activation buffer keyed by node id. Node ids are small in practice
    // (inputs+outputs+a few hidden), so a flat vector sized to max id is fine.
    int maxId = 0;
    for (const NodeGene& n : nodes) maxId = std::max(maxId, n.id);
    std::vector<float> act((size_t)maxId + 1, 0.0f);
    std::vector<bool>  computed((size_t)maxId + 1, false);

    for (int i = 0; i < N_INPUTS && i <= maxId; ++i) {
        act[i] = in[i];
        computed[i] = true;
    }

    // Process nodes in layer order (inputs 0.0 → hidden → outputs 1.0); the
    // add-node mutation only creates forward links, so one pass suffices.
    std::vector<NodeGene> order = nodes;
    std::stable_sort(order.begin(), order.end(),
                     [](const NodeGene& a, const NodeGene& b){
                         if (a.layer != b.layer) return a.layer < b.layer;
                         return a.id < b.id;
                     });

    for (const NodeGene& n : order) {
        if (n.id < N_INPUTS) continue;   // inputs already set
        float sum = 0.0f;
        for (const ConnGene& c : conns) {
            if (!c.enabled || c.out != n.id) continue;
            if (c.in <= maxId && computed[c.in]) sum += act[c.in] * c.weight;
        }
        // Hidden nodes: tanh. Output activation is applied below per-slot.
        act[n.id] = (n.layer < 1.0f) ? std::tanh(sum) : sum;
        computed[n.id] = true;
    }

    // moveX/moveY want [-1,1] (tanh); urges want [0,1] (sigmoid).
    for (int o = 0; o < N_OUTPUTS; ++o) {
        int   id  = N_INPUTS + o;
        float raw = (id <= maxId) ? act[id] : 0.0f;
        out[o] = (o < 2) ? std::tanh(raw)
                         : 1.0f / (1.0f + std::exp(-raw));
    }
}

float Genome::distance(const Genome& a, const Genome& b) {
    if (a.conns.empty() && b.conns.empty()) return 0.0f;
    // δ = c1·(disjoint+excess)/N + c3·avg weight diff of matching genes.
    std::map<int, const ConnGene*> ai;
    for (const ConnGene& c : a.conns) ai[c.innovation] = &c;
    int matching = 0, mismatched = 0;
    float wdiff = 0.0f;
    for (const ConnGene& c : b.conns) {
        auto it = ai.find(c.innovation);
        if (it != ai.end()) {
            ++matching;
            wdiff += std::fabs(c.weight - it->second->weight);
        } else {
            ++mismatched;
        }
    }
    mismatched += (int)a.conns.size() - matching;
    float n = (float)std::max(a.conns.size(), b.conns.size());
    float d = 1.0f * (float)mismatched / std::max(1.0f, n);
    if (matching > 0) d += 0.4f * (wdiff / (float)matching);
    return d;
}

void Genome::saveTo(std::ofstream& f) const {
    f << "NEAT_V1 " << nodes.size() << " " << conns.size() << "\n";
    for (const NodeGene& n : nodes)
        f << n.id << " " << n.layer << "\n";
    for (const ConnGene& c : conns)
        f << c.in << " " << c.out << " " << c.weight << " "
          << (c.enabled ? 1 : 0) << " " << c.innovation << "\n";
}

bool Genome::loadFrom(std::ifstream& f) {
    std::string marker;
    if (!(f >> marker) || marker != "NEAT_V1") return false;
    size_t nn, nc;
    f >> nn >> nc;
    nodes.assign(nn, {});
    conns.assign(nc, {});
    for (NodeGene& n : nodes) f >> n.id >> n.layer;
    for (ConnGene& c : conns) {
        int en;
        f >> c.in >> c.out >> c.weight >> en >> c.innovation;
        c.enabled = (en != 0);
        // Keep the global registries ahead of anything we load, so future
        // mutations never collide with restored historical markings.
        innovations().nextInnovation = std::max(innovations().nextInnovation,
                                                c.innovation + 1);
    }
    for (const NodeGene& n : nodes)
        innovations().nextNodeId = std::max(innovations().nextNodeId, n.id + 1);
    return true;
}

} // namespace neat
