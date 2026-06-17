#include "header/SocialOrder.h"
#include "header/Entity.h"
#include "header/WorldSeed.h"
#include "header/CivilizationEngine.h"   // for globalCivEngine->logEvent (rare, important events)
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <cmath>

SocialOrderSystem* globalSocialOrder = nullptr;

const char* socialClassName(SocialClass c) {
    switch (c) {
        case CLASS_SLAVE:     return "Slave";
        case CLASS_PLEBEIAN:  return "Plebeian";
        case CLASS_PATRICIAN: return "Patrician";
    }
    return "Plebeian";
}

SocialOrderSystem::SocialOrderSystem()
    : rng(makeStream(g_worldSeed.master, STREAM_SOCIAL)) {}

// ── small helpers ───────────────────────────────────────────────────────────────
static inline float dist2(const Entity* a, const Entity* b);  // defined after Entity is complete below

namespace {
    float sqr(float x) { return x * x; }
}

// ── Queries ───────────────────────────────────────────────────────────────────
int SocialOrderSystem::patronOf(int clientId) const {
    for (const auto& b : bonds) if (b.clientId == clientId) return b.patronId;
    return -1;
}

std::vector<int> SocialOrderSystem::clientsOf(int patronId) const {
    std::vector<int> out;
    for (const auto& b : bonds) if (b.patronId == patronId) out.push_back(b.clientId);
    return out;
}

bool SocialOrderSystem::isClientOf(int clientId, int patronId) const {
    for (const auto& b : bonds)
        if (b.clientId == clientId && b.patronId == patronId) return true;
    return false;
}

float SocialOrderSystem::totalDebt(int debtorId) const {
    float t = 0.0f;
    for (const auto& d : debts) if (d.debtorId == debtorId) t += d.amount;
    return t;
}

int SocialOrderSystem::clientCount(int patronId) const {
    int n = 0;
    for (const auto& b : bonds) if (b.patronId == patronId) ++n;
    return n;
}

// ── Mutations ─────────────────────────────────────────────────────────────────
void SocialOrderSystem::formBond(int patronId, int clientId, int year) {
    if (patronId < 0 || clientId < 0 || patronId == clientId) return;
    if (patronOf(clientId) != -1) return;          // a client serves a single patron
    bonds.push_back({patronId, clientId, 50.0f, 50.0f, year});
}

void SocialOrderSystem::breakBond(int patronId, int clientId) {
    bonds.erase(std::remove_if(bonds.begin(), bonds.end(),
        [&](const Clientela& b){ return b.patronId == patronId && b.clientId == clientId; }),
        bonds.end());
}

void SocialOrderSystem::addDebt(int creditorId, int debtorId, float amount, int year) {
    if (creditorId < 0 || debtorId < 0 || creditorId == debtorId || amount <= 0.0f) return;
    for (auto& d : debts)
        if (d.creditorId == creditorId && d.debtorId == debtorId) { d.amount += amount; return; }
    debts.push_back({creditorId, debtorId, amount, year});
}

void SocialOrderSystem::pruneDeadEntries(const std::vector<Entity>& entities) {
    std::unordered_set<int> alive;
    alive.reserve(entities.size() * 2);
    for (const auto& e : entities) if (e.entityHealth > 0.0f) alive.insert(e.entityId);

    bonds.erase(std::remove_if(bonds.begin(), bonds.end(),
        [&](const Clientela& b){ return !alive.count(b.patronId) || !alive.count(b.clientId); }),
        bonds.end());
    debts.erase(std::remove_if(debts.begin(), debts.end(),
        [&](const Debt& d){ return !alive.count(d.creditorId) || !alive.count(d.debtorId); }),
        debts.end());
}

// ── Class derivation ──────────────────────────────────────────────────────────
// Standing = wealth percentile (0.6) + earned auctoritas (0.4). Hysteresis makes
// mobility hard: you only ascend to Patrician from very high standing, and only
// fall back when you sink well below the bar — so rank is sticky, like real life.
void SocialOrderSystem::updateClasses(std::vector<Entity>& entities) {
    std::vector<float> tokens;
    tokens.reserve(entities.size());
    for (const auto& e : entities)
        if (e.entityHealth > 0.0f) tokens.push_back(e.salary.token);
    std::sort(tokens.begin(), tokens.end());

    auto wealthPct = [&](float t) -> float {
        if (tokens.empty()) return 0.5f;
        int below = (int)(std::lower_bound(tokens.begin(), tokens.end(), t) - tokens.begin());
        return (float)below / (float)tokens.size();
    };

    for (auto& e : entities) {
        if (e.entityHealth <= 0.0f) continue;

        // Auctoritas drifts toward an influence target: emergent dominance, the
        // number of clients one patronises, and raw wealth all confer standing.
        float wpct    = wealthPct(e.salary.token);
        float clients = (float)clientCount(e.entityId);
        float influence = e.dominanceRank * 0.45f
                        + std::min(45.0f, clients * 7.0f)
                        + wpct * 35.0f;
        e.auctoritas += (influence - e.auctoritas) * 0.06f;
        if (e.auctoritas < 0.0f)   e.auctoritas = 0.0f;
        if (e.auctoritas > 100.0f) e.auctoritas = 100.0f;

        if (e.socialClass == CLASS_SLAVE) continue; // bondage isn't escaped by wealth

        float standing = wpct * 0.6f + (e.auctoritas / 100.0f) * 0.4f;
        if (e.socialClass == CLASS_PATRICIAN) {
            // Fall from grace only on real collapse of fortune and standing.
            if (standing < 0.45f && e.auctoritas < 45.0f) {
                e.socialClass = CLASS_PLEBEIAN;
                if (globalCivEngine)
                    globalCivEngine->logEvent(-1, e.name + " has fallen from the patrician class", "tribe");
            }
        } else { // plebeian
            // The climb is steep: sustained wealth AND earned standing.
            if (standing > 0.88f && e.auctoritas > 60.0f) {
                e.socialClass = CLASS_PATRICIAN;
                ++totalAscended;
                if (globalCivEngine)
                    globalCivEngine->logEvent(-1, e.name + " has risen into the patrician class", "tribe");
            }
        }
    }
}

// ── Clientela formation ─────────────────────────────────────────────────────────
void SocialOrderSystem::updateClientela(std::vector<Entity>& entities, int year) {
    std::vector<Entity*> living;
    living.reserve(entities.size());
    for (auto& e : entities) if (e.entityHealth > 0.0f) living.push_back(&e);
    if (living.size() < 2) return;

    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    const float REACH2 = 220.0f * 220.0f;

    for (Entity* c : living) {
        if (c->socialClass == CLASS_SLAVE) continue;       // the owned can't choose a patron
        if (c->socialClass == CLASS_PATRICIAN) continue;   // elites are patrons, not clients
        if (patronOf(c->entityId) != -1) continue;         // already attached

        // Only the genuinely vulnerable seek protection: poor, indebted, or unsafe.
        bool needy = c->salary.token < 30.0f
                  || totalDebt(c->entityId) > 5.0f
                  || c->entityStress > 60.0f;
        if (!needy) continue;

        Entity* best = nullptr; float bestScore = 0.0f;
        for (Entity* p : living) {
            if (p == c) continue;
            if (p->auctoritas < 40.0f) continue;           // must have standing to patronise
            if (p->salary.token < 40.0f) continue;          // and means to provide
            int cap = std::max(1, (int)(p->auctoritas / 12.0f));
            if (clientCount(p->entityId) >= cap) continue;  // patron at capacity
            if (dist2(p, c) > REACH2) continue;             // must be within reach

            float score = p->auctoritas + p->salary.token * 0.1f;
            if (p->tribeId == c->tribeId && p->tribeId != -1) score *= 1.5f; // kin/tribe favour
            if (score > bestScore) { bestScore = score; best = p; }
        }
        if (best && roll(rng) < 0.18f) {
            formBond(best->entityId, c->entityId, year);
            if (globalCivEngine && best->socialClass == CLASS_PATRICIAN && roll(rng) < 0.15f)
                globalCivEngine->logEvent(-1, c->name + " entered the clientela of " + best->name, "tribe");
        }
    }
}

// ── Bond consequences ───────────────────────────────────────────────────────────
// Patron provides coin/food (lifting the client's means and easing stress); the
// client returns labour and political weight, enriching the patron and burnishing
// their auctoritas. A patron who can no longer provide loses the client's loyalty.
void SocialOrderSystem::applyBondEffects(std::vector<Entity>& entities) {
    std::unordered_map<int, Entity*> byId;
    byId.reserve(entities.size() * 2);
    for (auto& e : entities) if (e.entityHealth > 0.0f) byId[e.entityId] = &e;

    std::vector<std::pair<int,int>> toBreak;
    for (auto& b : bonds) {
        auto pit = byId.find(b.patronId);
        auto cit = byId.find(b.clientId);
        if (pit == byId.end() || cit == byId.end()) continue;
        Entity* patron = pit->second;
        Entity* client = cit->second;

        float provide = std::min(patron->salary.token * 0.05f, 4.0f);
        if (provide > 0.5f && patron->salary.token > provide) {
            patron->salary.token -= provide;
            client->salary.token += provide * 0.8f;            // some lost in the giving
            client->entityStress = std::max(0.0f, client->entityStress - 2.0f);

            // The client's labour & loyalty flow back to the patron.
            float labour = provide * 0.6f;
            patron->salary.token += labour;
            patron->auctoritas    = std::min(100.0f, patron->auctoritas + 0.15f);
            b.loyalty    = std::min(100.0f, b.loyalty + 2.0f);
            b.obligation = std::min(100.0f, b.obligation + 1.0f);
        } else {
            // Neglected clients drift away.
            b.loyalty -= 3.0f;
        }
        if (b.loyalty <= 12.0f) toBreak.push_back({b.patronId, b.clientId});
    }
    for (auto& pr : toBreak) breakBond(pr.first, pr.second);
}

// ── Debt → bondage → slavery cascade ─────────────────────────────────────────────
void SocialOrderSystem::updateDebtConsequences(std::vector<Entity>& entities, int year) {
    std::unordered_map<int, Entity*> byId;
    byId.reserve(entities.size() * 2);
    for (auto& e : entities) if (e.entityHealth > 0.0f) byId[e.entityId] = &e;

    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    std::vector<size_t> settled;

    for (size_t i = 0; i < debts.size(); ++i) {
        Debt& d = debts[i];
        auto cit = byId.find(d.creditorId);
        auto dit = byId.find(d.debtorId);
        if (cit == byId.end() || dit == byId.end()) continue;
        Entity* creditor = cit->second;
        Entity* debtor   = dit->second;

        d.amount *= 1.015f;                                 // interest accrues
        if (d.amount > 500.0f) d.amount = 500.0f;            // sanity cap

        // Service the debt when the debtor has anything to spare.
        if (debtor->salary.token > 4.0f) {
            float pay = std::min(debtor->salary.token * 0.25f, d.amount);
            debtor->salary.token   -= pay;
            creditor->salary.token += pay;
            d.amount               -= pay;
        }
        if (d.amount <= 1.0f) { settled.push_back(i); continue; }

        // Crushing, unpayable debt drives the debtor under the creditor's power.
        if (debtor->salary.token < 3.0f && d.amount > 25.0f &&
            debtor->socialClass != CLASS_SLAVE) {
            if (patronOf(debtor->entityId) == -1)
                formBond(d.creditorId, d.debtorId, year);    // debt-bondage as a client first

            // At the extreme, the debtor is enslaved to clear the debt.
            if (d.amount > 120.0f && roll(rng) < 0.04f) {
                debtor->socialClass = CLASS_SLAVE;
                breakBond(d.creditorId, d.debtorId);          // a slave is owned, not a client
                d.amount = 0.0f; settled.push_back(i);
                debtor->auctoritas = 0.0f;
                ++totalEnslaved;
                if (globalCivEngine)
                    globalCivEngine->logEvent(-1, debtor->name + " was enslaved over an unpayable debt to "
                        + creditor->name, "tribe");
            }
        }
    }

    // Erase settled debts (back-to-front to keep indices valid).
    for (auto it = settled.rbegin(); it != settled.rend(); ++it)
        debts.erase(debts.begin() + *it);

    // Rare manumission: a slave who has somehow built standing is freed.
    for (auto& e : entities) {
        if (e.entityHealth <= 0.0f || e.socialClass != CLASS_SLAVE) continue;
        if (roll(rng) < 0.003f) {
            e.socialClass = CLASS_PLEBEIAN;
            e.auctoritas  = std::max(e.auctoritas, 8.0f);
            ++totalFreed;
            if (globalCivEngine)
                globalCivEngine->logEvent(-1, e.name + " was freed from slavery", "tribe");
        }
    }
}

// ── Inheritance hook ──────────────────────────────────────────────────────────
void SocialOrderSystem::onDeath(std::vector<Entity>& entities, int deadId, int heirId, int year) {
    (void)entities; (void)year;
    if (heirId < 0 || heirId == deadId) {
        // No heir: the dead agent's obligations simply dissolve.
        bonds.erase(std::remove_if(bonds.begin(), bonds.end(),
            [&](const Clientela& b){ return b.patronId == deadId || b.clientId == deadId; }),
            bonds.end());
        debts.erase(std::remove_if(debts.begin(), debts.end(),
            [&](const Debt& d){ return d.creditorId == deadId || d.debtorId == deadId; }),
            debts.end());
        return;
    }

    // Heir inherits the estate of obligations: debts owed and owed to, and the
    // patronage of the deceased's clients (debt and enemies pass down the line).
    for (auto it = bonds.begin(); it != bonds.end();) {
        if (it->clientId == deadId) { it = bonds.erase(it); continue; } // a dead client's bond ends
        if (it->patronId == deadId) {
            if (heirId == it->clientId) { it = bonds.erase(it); continue; }
            if (patronOf(it->clientId) != heirId) it->patronId = heirId; // heir becomes their patron
            else { it = bonds.erase(it); continue; }
        }
        ++it;
    }
    for (auto& d : debts) {
        if (d.debtorId   == deadId) d.debtorId   = heirId;   // heir owes
        if (d.creditorId == deadId) d.creditorId = heirId;   // heir is owed
    }
    debts.erase(std::remove_if(debts.begin(), debts.end(),
        [&](const Debt& d){ return d.creditorId == d.debtorId; }), debts.end());
}

// ── Reporting ───────────────────────────────────────────────────────────────────
void SocialOrderSystem::classCounts(const std::vector<Entity>& entities,
                                    int& slaves, int& plebs, int& patricians) const {
    slaves = plebs = patricians = 0;
    for (const auto& e : entities) {
        if (e.entityHealth <= 0.0f) continue;
        switch (e.socialClass) {
            case CLASS_SLAVE:     ++slaves; break;
            case CLASS_PATRICIAN: ++patricians; break;
            default:              ++plebs; break;
        }
    }
}

std::string SocialOrderSystem::describe(const Entity& e, const std::vector<Entity*>& entities) const {
    auto nameOf = [&](int id) -> std::string {
        for (const auto* o : entities) if (o && o->entityId == id) return o->name;
        return "someone";
    };
    std::stringstream ss;
    ss << socialClassName(e.socialClass) << " \xc2\xb7 auctoritas " << (int)e.auctoritas;

    int pid = patronOf(e.entityId);
    if (pid != -1) ss << " \xc2\xb7 client of " << nameOf(pid);
    int cc = clientCount(e.entityId);
    if (cc > 0) ss << " \xc2\xb7 " << cc << " client" << (cc == 1 ? "" : "s");
    float dbt = totalDebt(e.entityId);
    if (dbt > 1.0f) ss << " \xc2\xb7 debt " << (int)dbt;
    return ss.str();
}

// ── Main tick ───────────────────────────────────────────────────────────────────
void SocialOrderSystem::tick(std::vector<Entity>& entities, int year) {
    if (entities.empty()) return;
    pruneDeadEntries(entities);
    updateClasses(entities);
    updateDebtConsequences(entities, year);
    updateClientela(entities, year);
    applyBondEffects(entities);
    classCounts(entities, cSlaves, cPlebs, cPatricians);
}

// dist2 needs the full Entity definition, which is available here.
static inline float dist2(const Entity* a, const Entity* b) {
    return sqr(a->posX - b->posX) + sqr(a->posY - b->posY);
}
