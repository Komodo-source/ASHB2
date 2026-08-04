#include "./header/Kinship.h"
#include "./header/Entity.h"
#include "../src/world/Lexicon.h"
#include "./header/LiveConfig.h"   // III-P4 classMul / IV-P1 traitMul kill switches
#include "./header/BetterRand.h"    // IV-P1 vertical transmission rolls
#include "./header/CivilizationEngine.h"  // IV-P1 trait catalogue (rival ways)

#include <algorithm>

KinshipSystem* globalKinship = nullptr;

// ── Family lookup ───────────────────────────────────────────────────────────────
Family* KinshipSystem::findFamily(int id) {
    for (auto& f : families) if (f.id == id) return &f;
    return nullptr;

}
const Family* KinshipSystem::findFamily(int id) const {
    for (auto& f : families) if (f.id == id) return &f;
    return nullptr;
}

// ── Family creation / naming ────────────────────────────────────────────────────
Family* KinshipSystem::createFamily(Entity& founder, int year) {
    Family fam;
    fam.id             = nextFamilyId++;
    fam.founderId      = founder.entityId;
    fam.originRegionId = founder.originRegionId;
    fam.foundedYear    = year;
    fam.reputation     = 50.0f;

    // Surname from the world lexicon when available, else a stable fallback.
    if (g_lexicon) {
        fam.name = "House " + g_lexicon->genTribeName(founder.originRegionId);
    } else {
        fam.name = "Clan #" + std::to_string(fam.id);
    }

    fam.memberIds.push_back(founder.entityId);
    families.push_back(fam);

    Family* stored = &families.back();
    founder.familyId = stored->id;
    return stored;
}

Family* KinshipSystem::ensureFounderFamily(Entity& e, int year) {
    if (e.familyId >= 0) {
        if (Family* f = findFamily(e.familyId)) return f;
    }
    return createFamily(e, year);
}

// ── Birth registration ──────────────────────────────────────────────────────────
void KinshipSystem::registerBirth(Entity& child, Entity* p1, Entity* p2, int year) {
    child.parent1Id = p1 ? p1->entityId : -1;
    child.parent2Id = p2 ? p2->entityId : -1;

    if (p1) p1->childrenIds.push_back(child.entityId);
    if (p2) p2->childrenIds.push_back(child.entityId);

    // Phase 6: epigenetic inheritance. Must happen HERE, at birth, while p1/p2
    // are still fresh pointers into the live entities vector — NOT later at
    // childhood-finalization time (FreeWillSystem::finalizeChildhood), which
    // only has the legacy child.parent1/parent2 raw pointers. Those dangle
    // once the entities vector reallocates for a later birth (this file's own
    // comment above the class: "safe across entity-vector reallocation" is
    // exactly why parent1Id/parent2Id exist instead), and a dangling read of
    // a std::vector member (epigeneticMarkers) is far more dangerous than of
    // a scalar float — it broke the determinism pair on a 300-tick run.
    child.inheritEpigeneticMarkers(p1, p2);

    // Decide which family the child joins. Prefer parent1's line; if that parent
    // has no family yet, found one for them so lineage is never orphaned.
    Family* fam = nullptr;
    if (p1) {
        fam = ensureFounderFamily(*p1, year);
    } else if (p2) {
        fam = ensureFounderFamily(*p2, year);
    } else {
        // No known parents: the child itself founds a family.
        fam = createFamily(child, year);
    }

    // I-P3: place the child in the line. Depth counts from the founder of the
    // house, so a family's `generation` becomes the real answer to "how many
    // generations has this bloodline run" — the through-line the Chronicle
    // follows across a whole world's history.
    int parentDepth = 0;
    if (p1) parentDepth = std::max(parentDepth, p1->lineageDepth);
    if (p2) parentDepth = std::max(parentDepth, p2->lineageDepth);
    child.lineageDepth = parentDepth + 1;

    // III-P4: cultural capital is inherited before it is earned (Bourdieu). A
    // child raised in a literate, well-regarded house starts life holding most
    // of what that house holds; one raised without it starts from nothing and
    // must accumulate. This single line is why advantage compounds across
    // generations instead of being redealt at every birth.
    if ((p1 || p2) && g_liveConfig.classMul != 0.0f) {
        float inherited = p1 && p2 ? (p1->culturalCapital + p2->culturalCapital) * 0.5f
                                   : (p1 ? p1->culturalCapital : p2->culturalCapital);
        child.culturalCapital = std::max(0.0f, std::min(100.0f, inherited * 0.75f));
    }

    // IV-P1: vertical transmission — the strongest channel culture has. A child
    // takes up nearly everything both its parents do, and about half of what
    // only one of them does; the share that fails to pass is where a lineage's
    // ways quietly change. This is why a people stays itself across generations
    // without anything enforcing it, and why an isolated valley diverges.
    if ((p1 || p2) && g_liveConfig.traitMul != 0.0f) {
        const unsigned long long a = p1 ? p1->cultureTraits : 0ull;
        const unsigned long long b = p2 ? p2->cultureTraits : 0ull;
        unsigned long long got = 0ull, pool = a | b;
        while (pool) {
            const unsigned long long bit = pool & (~pool + 1ull);
            const bool both = (a & bit) && (b & bit);
            if (BetterRand::genNrInInterval(0.0f, 1.0f) < (both ? 0.92f : 0.55f)) {
                // Parents who married across a difference (she buries, he burns)
                // do not hand the child both ways — it grows up doing one.
                if (globalCivEngine) {
                    int id = 0;
                    while ((bit >> id) != 1ull) ++id;
                    got &= ~globalCivEngine->culture.rivals(id);
                }
                got |= bit;
            }
            pool &= pool - 1ull;
        }
        child.cultureTraits = got;
    }

    if (fam) {
        child.familyId = fam->id;
        fam->memberIds.push_back(child.entityId);
        fam->births++;
        fam->generation = std::max(fam->generation, child.lineageDepth);
        // A healthy growing line gains a little standing.
        fam->reputation = std::min(100.0f, fam->reputation + 0.5f);
    }
}

// ── Relationship queries ────────────────────────────────────────────────────────
bool KinshipSystem::shareParent(const Entity& a, const Entity& b) {
    if (a.entityId == b.entityId) return false;
    auto match = [](int x, int y) { return x >= 0 && x == y; };
    return match(a.parent1Id, b.parent1Id) || match(a.parent1Id, b.parent2Id) ||
           match(a.parent2Id, b.parent1Id) || match(a.parent2Id, b.parent2Id);
}

bool KinshipSystem::isParentChild(const Entity& a, const Entity& b) {
    if (a.parent1Id == b.entityId || a.parent2Id == b.entityId) return true;
    if (b.parent1Id == a.entityId || b.parent2Id == a.entityId) return true;
    return false;
}

bool KinshipSystem::wouldBeIncest(const Entity& a, const Entity& b) {
    if (a.entityId == b.entityId) return true;
    if (isParentChild(a, b)) return true;
    if (shareParent(a, b))    return true;
    return false;
}

// ── UI / reputation ─────────────────────────────────────────────────────────────
std::string KinshipSystem::describeKin(const Entity& e) const {
    std::string out;
    if (e.familyId >= 0) {
        if (const Family* f = findFamily(e.familyId)) {
            out = f->name + " \xC2\xB7 ";  // " · "
            out += std::to_string((int)e.childrenIds.size()) + " children \xC2\xB7 ";
            out += "rep " + std::to_string((int)f->reputation);
            return out;
        }
    }
    return "No family \xC2\xB7 " + std::to_string((int)e.childrenIds.size()) + " children";
}

void KinshipSystem::adjustReputation(int familyId, float delta) {
    if (Family* f = findFamily(familyId)) {
        f->reputation = std::max(0.0f, std::min(100.0f, f->reputation + delta));
    }
}
