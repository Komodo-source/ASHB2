#ifndef KINSHIP_H
#define KINSHIP_H

#include <string>
#include <vector>

class Entity;

// ── Family / clan ──────────────────────────────────────────────────────────────
// A named descent group. Entities born without known parents (e.g. the initial
// seeded population) found their own families; children inherit their senior
// parent's family. This replaces the old pointer-based Heritage graph, which was
// both crash-prone (raw Entity* across a reallocating vector) and silently broken
// (Heritage::add_child mutated a by-value loop copy and recorded nothing).
struct Family {
    int              id             = -1;
    std::string      name;                  // clan/surname, e.g. "House Tarn"
    int              founderId      = -1;
    int              originRegionId = -1;
    int              foundedYear    = 0;
    std::vector<int> memberIds;             // all members ever (living + dead) by id
    float            reputation     = 50.0f; // 0-100: marriage prospects / influence
    int              births         = 0;     // children born into the family
};

// ── KinshipSystem ──────────────────────────────────────────────────────────────
// Owns the family registry and the birth-registration logic. All cross-entity
// relationships are expressed as ids, so nothing here is invalidated when the
// global entity vector reallocates.
class KinshipSystem {
public:
    std::vector<Family> families;

    // Record a birth: stamps parent ids on the child, appends the child to each
    // parent's childrenIds, assigns the child a family (inheriting the senior
    // parent's, or founding a new one), and gives it that family's surname.
    // p1/p2 may be null (seeded founders with no parents).
    void registerBirth(Entity& child, Entity* p1, Entity* p2, int year);

    // Make an entity that has no family the founder of its own. Idempotent.
    Family* ensureFounderFamily(Entity& e, int year);

    // ── Relationship queries (id-based, O(children), no vector scan) ──────────
    static bool shareParent(const Entity& a, const Entity& b);    // full/half siblings
    static bool isParentChild(const Entity& a, const Entity& b);  // direct lineage
    // Bar these two from conceiving together? Blocks parent/child and
    // (half-)siblings. Cousins are intentionally allowed (historically common).
    static bool wouldBeIncest(const Entity& a, const Entity& b);

    Family*       findFamily(int id);
    const Family* findFamily(int id) const;

    // UI helper, e.g. "House Tarn · 3 children · rep 58".
    std::string describeKin(const Entity& e) const;

    // Clamped reputation nudge (deaths in battle, prestige, scandal, …).
    void adjustReputation(int familyId, float delta);

private:
    int     nextFamilyId = 0;
    Family* createFamily(Entity& founder, int year);
};

extern KinshipSystem* globalKinship;

#endif // KINSHIP_H
