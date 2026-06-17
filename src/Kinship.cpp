#include "./header/Kinship.h"
#include "./header/Entity.h"
#include "../src/world/Lexicon.h"

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

    if (fam) {
        child.familyId = fam->id;
        fam->memberIds.push_back(child.entityId);
        fam->births++;
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
