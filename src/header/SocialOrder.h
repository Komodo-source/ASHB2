#ifndef SOCIAL_ORDER_H
#define SOCIAL_ORDER_H

#include <string>
#include <vector>
#include <random>

class Entity;

// ── Social stratification ──────────────────────────────────────────────────────
// A Roman-esque class ladder. Mobility is possible but statistically hard: it is
// driven by accumulated wealth (token) and earned standing (auctoritas), and the
// downward slide into debt-bondage is far easier than the climb out of it.
enum SocialClass {
    CLASS_SLAVE     = 0,  // owned/bonded labour — no property, no vote
    CLASS_PLEBEIAN  = 1,  // free commoner — the bulk of society
    CLASS_PATRICIAN = 2   // landed elite — patrons, office-holders, glory-seekers
};

const char* socialClassName(SocialClass c);

// ── Clientela: the patron–client bond ──────────────────────────────────────────
// A directed obligation from patron (the stronger party) to client. The patron
// extends protection / coin / food; the client returns labour, military support
// and political backing (votes). Stored by entity id so it survives the entity
// vector reallocating — the same discipline the Kinship system enforces.
struct Clientela {
    int   patronId  = -1;
    int   clientId  = -1;
    float loyalty   = 50.0f;  // 0-100: client's devotion; decays if patron neglects
    float obligation= 50.0f;  // 0-100: how much the patron has invested / is owed
    int   sinceYear = 0;
};

// ── Debt: a credit obligation between two agents ────────────────────────────────
// Famine/scarcity pushes the poor into debt; debt that cannot be serviced is the
// road into clientela and, at the extreme, into slavery.
struct Debt {
    int   creditorId = -1;
    int   debtorId   = -1;
    float amount     = 0.0f;
    int   sinceYear  = 0;
};

// ── SocialOrderSystem ───────────────────────────────────────────────────────────
// Owns the clientela network, the debt ledger and class assignment. Everything is
// id-based; nothing stores a raw Entity*. Deterministic: its RNG is seeded from
// the world seed's STREAM_SOCIAL so a given seed reproduces the same social order.
class SocialOrderSystem {
public:
    std::vector<Clientela> bonds;
    std::vector<Debt>      debts;

    // Cached census, refreshed every tick so the report panel can read society's
    // shape without re-scanning the population.
    int cSlaves = 0, cPlebs = 0, cPatricians = 0;
    int totalEnslaved = 0;   // cumulative enslavements over the whole run
    int totalFreed    = 0;   // cumulative manumissions
    int totalAscended = 0;   // cumulative rises into the patrician class

    SocialOrderSystem();

    // Run once per civilisation tick: refresh class from wealth/standing, let
    // patrons recruit clients, accrue/settle debt, and apply the consequences of
    // those bonds to the agents (food/protection down, loyalty/labour up).
    void tick(std::vector<Entity>& entities, int year);

    // ── Queries (id-based) ───────────────────────────────────────────────────
    int               patronOf(int clientId) const;       // -1 if independent
    std::vector<int>  clientsOf(int patronId) const;
    bool              isClientOf(int clientId, int patronId) const;
    float             totalDebt(int debtorId) const;
    int               clientCount(int patronId) const;

    // ── Mutations ────────────────────────────────────────────────────────────
    void formBond(int patronId, int clientId, int year);
    void breakBond(int patronId, int clientId);
    void addDebt(int creditorId, int debtorId, float amount, int year);

    // Inheritance hook (Pillar 4): when an agent dies, an heir inherits its debts,
    // its clients (it becomes their new patron) and its creditors. Pass heirId < 0
    // to simply dissolve the dead agent's obligations.
    void onDeath(std::vector<Entity>& entities, int deadId, int heirId, int year);

    // UI helper, e.g. "Patrician · auctoritas 64 · 5 clients" or "Plebeian · client of Marcus".
    std::string describe(const Entity& e, const std::vector<Entity*>& entities) const;

    // Aggregate counts for the report panel.
    void classCounts(const std::vector<Entity>& entities,
                     int& slaves, int& plebs, int& patricians) const;
    int  bondCount() const { return (int)bonds.size(); }

private:
    std::mt19937_64 rng;

    // Recompute each agent's class from its wealth percentile and standing.
    void updateClasses(std::vector<Entity>& entities);
    // Patrons (high wealth/auctoritas) take on needy clients within reach.
    void updateClientela(std::vector<Entity>& entities, int year);
    // Patron provides; client reciprocates; neglect erodes loyalty and breaks bonds.
    void applyBondEffects(std::vector<Entity>& entities);
    // Unpayable debt → clientela → bondage.
    void updateDebtConsequences(std::vector<Entity>& entities, int year);

    // Drop ledger entries whose endpoints no longer exist (dead/removed agents).
    void pruneDeadEntries(const std::vector<Entity>& entities);
};

extern SocialOrderSystem* globalSocialOrder;

#endif // SOCIAL_ORDER_H
