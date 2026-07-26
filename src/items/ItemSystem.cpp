#include "ItemSystem.h"
#include "../header/Entity.h"
#include "../header/CivilizationEngine.h"
#include "../header/Economics.h"
#include "../header/WorldSeed.h"
#include "../header/LiveConfig.h"
#include "../core/SpatialGrid.h"
#include "../world/PheromoneField.h"
#include "../ai/UtilityCurves.h"
#include "../ai/GoapPlanner.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>          // atof

ItemManager g_itemManager;

namespace {
// Dedicated deterministic stream for the item/invention layer (same makeStream
// discipline as STREAM_INNOV etc.; new salt so existing streams are untouched).
constexpr uint64_t STREAM_ITEMS = 0x94D049BB133111EBull;

std::mt19937_64& itemRng() {
    static std::mt19937_64 rng = makeStream(g_worldSeed.master, STREAM_ITEMS);
    return rng;
}

// Suffix for an invented item, chosen by its dominant tag so generated names
// read like artifacts, not UUIDs ("Wildfruit-Clay Ware", "Hide-Iron Blade").
std::string inventedSuffix(const std::vector<std::string>& tags) {
    for (const auto& t : tags) {
        if (t == "food")     return "Brew";
        if (t == "weapon")   return "Blade";
        if (t == "armor")    return "Guard";
        if (t == "medicine") return "Balm";
        if (t == "tool")     return "Ware";
    }
    return "Charm";
}
} // namespace

// ── Registration ──────────────────────────────────────────────────────────────

int ItemManager::addDef(ItemDef d) {
    d.id = (int)defs_.size();
    defByName_[d.name] = d.id;
    defs_.push_back(std::move(d));
    return defs_.back().id;
}

int ItemManager::addRule(ActionRule r) {
    r.id = (int)rules_.size();
    rules_.push_back(std::move(r));
    return rules_.back().id;
}

void ItemManager::seed() {
    if (seededFlag_) return;
    seededFlag_ = true;

    // Raw environmental materials — the substrate every invention combines.
    addDef({-1, "Stone",     {"material","mineral"},          {{"durability",60},{"weight",8}},  6.f});
    addDef({-1, "Wood",      {"material","plant","fuel"},     {{"durability",30},{"weight",4}},  6.f});
    addDef({-1, "Fiber",     {"material","plant"},            {{"durability",12},{"weight",1}},  6.f});
    addDef({-1, "Hide",      {"material","animal"},           {{"durability",25},{"weight",3}},  8.f});
    addDef({-1, "Clay",      {"material","mineral","moldable"},{{"durability",15},{"weight",5}}, 8.f});
    idWildFood_ = addDef({-1, "Wild Food", {"food","plant","perishable"},
                          {{"nutrition",22},{"weight",1}}, 5.f});

    // Import the Market catalog so the trade economy and the item layer share
    // one vocabulary. Category → tags; per-unit props from the product row.
    if (g_market.initialized || !g_market.products.empty()) {
        for (const MarketProduct& p : g_market.products) {
            if (defByName_.count(p.name)) continue;
            ItemDef d;
            d.name = p.name;
            d.complexity = 10.0f + p.basePrice * 0.1f;
            switch (p.category) {
                case GoodCategory::FOOD:
                    d.tags = {"food","tradable"};
                    if (p.requiresAgriculture) d.tags.push_back("cultivated");
                    d.props["nutrition"] = p.nutrition();
                    break;
                case GoodCategory::DEF_OBJECT:
                    d.tags = {"armor","military","tradable"};
                    d.props["defense"] = (float)p.def_value;
                    break;
                case GoodCategory::ATK_OBJECT:
                    d.tags = {"weapon","military","tradable"};
                    d.props["attack"] = (float)p.atk_value;
                    break;
                case GoodCategory::OBJECT:
                default:
                    d.tags = {"material","tradable"};
                    d.props["durability"] = 20.0f;
                    break;
            }
            d.props["value"] = p.basePrice;
            addDef(std::move(d));
        }
    }

    seedBaseRules();
}

void ItemManager::seedBaseRules() {
    // Forage: no inputs, produces wild food. The agent layer adds the movement
    // and success-odds around it; as a RULE it exists so GOAP can plan with it.
    ActionRule forage;
    forage.name = "Forage";
    forage.produces  = { ItemStack{idWildFood_, 1.0f, 100.0f} };
    forage.statEffects = { {"fatigue", +4.0f} };
    forage.duration  = 2.0f;
    forage.complexity = 4.0f;
    idForageRule_ = addRule(std::move(forage));

    // Eat: consumes any "food" tag, lowers hunger by the item's nutrition
    // (executeRule reads the actual consumed def's props at execution time).
    ActionRule eat;
    eat.name = "Eat";
    eat.consumes = { TagQty{"food", 1.0f} };
    eat.statEffects = { {"hunger", -22.0f}, {"happiness", +2.0f} };
    eat.duration = 1.0f;
    eat.complexity = 2.0f;
    idEatRule_ = addRule(std::move(eat));
}

// ── Valuation (Step 2: need-based economy) ────────────────────────────────────

float ItemManager::subjectiveValue(const Entity& agent, const ItemDef& def) const {
    using namespace UtilityCurvesLib;
    float v = 0.1f + def.prop("value", 0.0f) * 0.01f;   // faint objective anchor

    if (def.hasTag("food")) {
        // Starvation prices food superlinearly (quadratic curve, weight 2.0),
        // scaled by how nourishing this particular item is.
        v += hungerUrgency().score(agent.entityHunger)
             * (0.5f + def.prop("nutrition", 10.0f) / 30.0f);
    }
    if (def.hasTag("medicine"))
        v += healthUrgency().score(100.0f - agent.entityHealth)
             * (0.5f + def.prop("healing", 10.0f) / 30.0f);
    if (def.hasTag("weapon") || def.hasTag("armor")) {
        // Fear prices arms: anger and low safety raise the worth of steel.
        v += (agent.entityGeneralAnger / 100.0f) * 0.8f
             + (def.prop("attack", 0.0f) + def.prop("defense", 0.0f)) * 0.004f;
    }
    if (def.hasTag("material"))
        v += (agent.personality.openness / 100.0f) * 0.25f;  // makers value inputs

    return v;
}

// ── Tag helpers ───────────────────────────────────────────────────────────────

float ItemManager::countTag(const Inventory& inv, const std::string& tag) const {
    float n = 0.0f;
    for (const ItemStack& s : inv.stacks) {
        const ItemDef* d = def(s.defId);
        if (d && d->hasTag(tag)) n += s.qty;
    }
    return n;
}

int ItemManager::firstStackWithTag(const Inventory& inv, const std::string& tag) const {
    for (int i = 0; i < (int)inv.stacks.size(); ++i) {
        const ItemDef* d = def(inv.stacks[i].defId);
        if (d && d->hasTag(tag) && inv.stacks[i].qty >= 1.0f) return i;
    }
    return -1;
}

// ── Rule execution + cultural diffusion (Steps 2 & 4) ─────────────────────────

static void applyStatEffect(Entity& e, const std::string& stat, float delta) {
    auto clamp01 = [](float v){ return std::min(100.0f, std::max(0.0f, v)); };
    if      (stat == "hunger")    e.entityHunger    = clamp01(e.entityHunger + delta);
    else if (stat == "health")    e.entityHealth    = clamp01(e.entityHealth + delta);
    else if (stat == "happiness") e.entityHapiness  = clamp01(e.entityHapiness + delta);
    else if (stat == "fatigue")   e.fatigueLevel    = clamp01(e.fatigueLevel + delta);
    else if (stat == "stress")    e.entityStress    = clamp01(e.entityStress + delta);
}

bool ItemManager::executeRule(Entity& agent, const ActionRule& rule,
                              const std::vector<Entity*>& watchers, int day,
                              CivilizationEngine* civ) {
    // Preconditions: required tools present, inputs affordable.
    for (const std::string& t : rule.requiresTags)
        if (countTag(agent.inventory, t) < 1.0f) return false;
    for (const TagQty& c : rule.consumes)
        if (countTag(agent.inventory, c.tag) < c.qty) return false;

    // Consume inputs. For "food" consumption remember the def eaten so its own
    // nutrition drives the hunger effect (data over hardcode).
    float consumedNutrition = -1.0f;
    for (const TagQty& c : rule.consumes) {
        float left = c.qty;
        while (left > 0.0f) {
            int si = firstStackWithTag(agent.inventory, c.tag);
            if (si < 0) return false;   // shouldn't happen after the check above
            ItemStack& s = agent.inventory.stacks[si];
            const ItemDef* d = def(s.defId);
            if (c.tag == "food" && d) consumedNutrition = d->prop("nutrition", 20.0f);
            float take = std::min(left, s.qty);
            s.qty -= take; left -= take;
            if (s.qty <= 0.001f) {
                agent.inventory.stacks[si] = agent.inventory.stacks.back();
                agent.inventory.stacks.pop_back();
            }
        }
    }

    for (const ItemStack& p : rule.produces)
        agent.inventory.add(p.defId, p.qty, p.condition);

    for (const StatEffect& fx : rule.statEffects) {
        float delta = fx.delta;
        if (fx.stat == "hunger" && delta < 0.0f && consumedNutrition > 0.0f)
            delta = -consumedNutrition;   // the eaten item's real nutrition
        applyStatEffect(agent, fx.stat, delta);
    }

    // Cultural transmission (Step 4): watchers within visual range who see an
    // INVENTED rule performed learn the recipe. Seeded rules are common
    // knowledge already; only novel culture diffuses.
    if (rule.invented) {
        for (Entity* w : watchers) {
            if (!w || w->entityId == agent.entityId || w->entityHealth <= 0.0f) continue;
            if (!w->knowsRecipe(rule.id)) {
                w->learnRecipe(rule.id);
                ++recipeSpreads_;
            }
        }
    }
    (void)day; (void)civ;
    return true;
}

// ── Invention (Step 2: combinatorial, complexity-gated) ───────────────────────

int ItemManager::tryInvent(Entity& agent, int day, CivilizationEngine* civ) {
    // Need two distinct defs in hand to combine.
    if (agent.inventory.stacks.size() < 2) return -1;
    auto& rng = itemRng();
    std::uniform_int_distribution<int> pick(0, (int)agent.inventory.stacks.size() - 1);
    int ia = pick(rng), ib = pick(rng);
    if (ia == ib) return -1;
    const ItemDef* A = def(agent.inventory.stacks[ia].defId);
    const ItemDef* B = def(agent.inventory.stacks[ib].defId);
    if (!A || !B || A->id == B->id) return -1;

    // Same (18/complexity)² law that paces the civilization's innovations.
    // Floor of 26 so even the simplest raw+raw combination fails half its
    // attempts — without it two cheap raws (complexity 6-8) combined below
    // the 18 knee and EVERY experiment succeeded (146 inventions in 400 days).
    float complexity = std::max(26.0f, (A->complexity + B->complexity) * 0.8f);
    float odds = 18.0f / complexity;
    odds *= odds;
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    if (roll(rng) > odds) return -1;   // the experiment fizzles

    // Snapshot everything needed from A/B NOW: addDef() below grows defs_ and
    // reallocation would leave A and B dangling (this produced empty parent
    // names in the invention log).
    const std::string nameA = A->name, nameB = B->name;
    const std::string tagA = A->tags.empty() ? "material" : A->tags[0];
    const std::string tagB = B->tags.empty() ? "material" : B->tags[0];
    const int idA = A->id, idB = B->id;

    // New def: tag union + "crafted"; properties blend toward the better parent
    // with a small synthesis bonus — combining is worth more than carrying two.
    ItemDef nd;
    nd.name = A->name + "-" + B->name;
    nd.tags = A->tags;
    for (const auto& t : B->tags)
        if (!nd.hasTag(t)) nd.tags.push_back(t);
    if (!nd.hasTag("crafted")) nd.tags.push_back("crafted");
    nd.name += " " + inventedSuffix(nd.tags);
    if (defByName_.count(nd.name)) return -1;   // this combination already exists
    for (const auto& kv : A->props) nd.props[kv.first] = kv.second;
    for (const auto& kv : B->props) {
        auto it = nd.props.find(kv.first);
        nd.props[kv.first] = (it == nd.props.end()) ? kv.second
                             : std::max(it->second, kv.second) * 1.15f;
    }
    nd.complexity = complexity;
    nd.invented   = true;
    nd.parentA = idA; nd.parentB = idB;
    int newDefId = addDef(nd);   // A and B are dangling from here on
    ++inventedDefs_;

    // New rule: consumes one of each parent's primary tag, produces the child.
    ActionRule nr;
    nr.name = "Craft " + defs_[newDefId].name;
    nr.consumes = { TagQty{tagA, 1.0f}, TagQty{tagB, 1.0f} };
    nr.produces = { ItemStack{newDefId, 1.0f, 100.0f} };
    if (defs_[newDefId].hasTag("food"))
        nr.statEffects.push_back({"happiness", +1.0f});
    nr.duration = 2.0f;
    nr.complexity = complexity;
    nr.invented = true;
    nr.inventorEntityId = agent.entityId;
    nr.inventedOnDay = day;
    int newRuleId = addRule(std::move(nr));
    ++inventedRules_;

    agent.learnRecipe(newRuleId);
    if (civ) civ->logEvent(day, agent.name + " invented \"" + defs_[newDefId].name
                                + "\" by combining " + nameA + " and " + nameB,
                           "innovation");
    return newRuleId;
}

// ── The per-day agent layer (Steps 2–5 integration point) ─────────────────────

void ItemManager::tickAgents(std::vector<Entity>& entities, int day,
                             float worldW, float worldH, PheromoneField* field,
                             CivilizationEngine* civ) {
    if (!seededFlag_) seed();
    auto& rng = itemRng();
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    // Spatial index for visual-range queries (diffusion, NEAT senses).
    static SpatialGrid grid;
    static std::vector<Entity*> ptrs;
    ptrs.clear();
    for (Entity& e : entities)
        if (e.entityHealth > 0.0f) ptrs.push_back(&e);
    grid.reset(std::max(worldW, 1.0f), std::max(worldH, 1.0f), 130.0f);
    grid.rebuild(ptrs);

    std::vector<Entity*> watchers;

    for (Entity* pe : ptrs) {
        Entity& e = *pe;
        if (e.entityAge < 10.0f) continue;   // children carried by parents

        const float vision = 120.0f * e.genome.sightRange;

        // ── Sense the field once ─────────────────────────────────────────────
        float gxF = 0.0f, gyF = 0.0f, dangerHere = 0.0f;
        if (field && field->ready()) {
            field->gradient(e.posX, e.posY, PheromoneField::FOOD, gxF, gyF);
            dangerHere = field->sample(e.posX, e.posY, PheromoneField::DANGER);
        }
        dangerHere += e.episodicMap.dangerAt(e.posX, e.posY, 100.0f, day);

        // ── Decide urges: NEAT brain or utility curves ───────────────────────
        bool wantForage, wantEat;
        float moveX = 0.0f, moveY = 0.0f;
        if (e.useNeatBrain && !e.neatGenome.empty()) {
            float in[neat::N_INPUTS] = {
                e.entityHunger / 100.0f, e.entityHealth / 100.0f,
                e.fatigueLevel / 100.0f,
                std::max(-1.0f, std::min(1.0f, gxF * 0.1f)),
                std::max(-1.0f, std::min(1.0f, gyF * 0.1f)),
                std::min(1.0f, dangerHere / 100.0f),
                countTag(e.inventory, "food") > 0.0f ? 1.0f : 0.0f,
                e.entityLoneliness / 100.0f,
                1.0f  // bias
            };
            float out[neat::N_OUTPUTS];
            e.neatGenome.evaluate(in, out);
            moveX = out[0]; moveY = out[1];
            wantForage = out[2] > 0.5f;
            wantEat    = out[3] > 0.5f;
        } else {
            using namespace UtilityCurvesLib;
            // Score = hungerCurve − dangerCurve, the declarative form of the
            // old "if starving then forage".
            float forageScore = hungerUrgency().score(e.entityHunger)
                              + dangerAvoidance().score(dangerHere);   // negative weight
            wantForage = forageScore > 0.45f;
            wantEat    = e.entityHunger > 45.0f && countTag(e.inventory, "food") > 0.0f;
            // Movement urge: toward remembered food when hungry, with the
            // pheromone gradient as tie-breaker; away from danger regardless.
            if (wantForage) {
                if (const EpisodicNode* m =
                        e.episodicMap.recallBest(EpisodicMap::FOOD, e.posX, e.posY, day)) {
                    float dx = m->x - e.posX, dy = m->y - e.posY;
                    float len = std::sqrt(dx*dx + dy*dy);
                    if (len > 20.0f) { moveX = dx / len; moveY = dy / len; }
                } else if (gxF != 0.0f || gyF != 0.0f) {
                    float len = std::sqrt(gxF*gxF + gyF*gyF);
                    moveX = gxF / len; moveY = gyF / len;
                }
            }
            if (dangerHere > 25.0f && field && field->ready()) {
                float dgx, dgy;
                field->gradient(e.posX, e.posY, PheromoneField::DANGER, dgx, dgy);
                float len = std::sqrt(dgx*dgx + dgy*dgy);
                if (len > 0.0f) { moveX -= dgx / len; moveY -= dgy / len; }  // downhill
            }
        }

        // ── Move (stigmergy-guided daily displacement; frame-level wandering
        // stays with the legacy Movement code) ───────────────────────────────
        if (moveX != 0.0f || moveY != 0.0f) {
            float step = 14.0f * e.genome.speed * g_liveConfig.moveForceMul;
            e.posX = std::min(worldW - 10.0f, std::max(10.0f, e.posX + moveX * step));
            e.posY = std::min(worldH - 10.0f, std::max(10.0f, e.posY + moveY * step));
        }

        // ── GOAP: hungry utility-agents plan; the first step runs today ─────
        if (!e.useNeatBrain && e.entityHunger > 60.0f) {
            goap::WorldState st = goap::captureState(e, *this);
            std::vector<goap::Step> plan =
                goap::plan(e, *this, goap::reduceHungerGoal(), st);
            if (!plan.empty()) {
                if (plan.front().gatherFood) wantForage = true;
                else if (const ActionRule* r = rule(plan.front().ruleId)) {
                    watchers.clear();
                    grid.forEachInRadius(e.posX, e.posY, vision,
                                         [&](Entity* w){ watchers.push_back(w); });
                    executeRule(e, *r, watchers, day, civ);
                }
            }
        }

        // ── Forage attempt ───────────────────────────────────────────────────
        if (wantForage && e.fatigueLevel < 90.0f) {
            float chance = 0.55f * g_liveConfig.foodYieldMul;
            if (roll(rng) < chance) {
                float qty = 1.0f + (roll(rng) < 0.3f ? 1.0f : 0.0f);
                e.inventory.add(idWildFood_, qty);
                applyStatEffect(e, "fatigue", +4.0f);
                e.episodicMap.remember(e.posX, e.posY, EpisodicMap::FOOD, day, 1.0f);
                if (field) field->deposit(e.posX, e.posY, PheromoneField::FOOD, 6.0f * qty);
            }
        }

        // ── Gather raw materials (the substrate inventions combine) ─────────
        // Tinkerers stockpile parts: openness-weighted daily chance to pick up
        // one of the five environmental raws, capped so inventories stay small.
        // Without this nobody ever holds two DISTINCT defs and tryInvent can
        // never fire — food alone is not an experiment.
        if (roll(rng) < 0.15f + e.personality.openness * 0.002f
            && countTag(e.inventory, "material") < 6.0f) {
            static const char* kRaw[5] = {"Stone", "Wood", "Fiber", "Hide", "Clay"};
            std::uniform_int_distribution<int> pickRaw(0, 4);
            int did = defIdByName(kRaw[pickRaw(rng)]);
            if (did >= 0) e.inventory.add(did, 1.0f);
        }

        // ── Eat ──────────────────────────────────────────────────────────────
        if (wantEat) {
            if (const ActionRule* r = rule(idEatRule_)) {
                watchers.clear();   // Eat is seeded knowledge; no diffusion needed
                executeRule(e, *r, watchers, day, civ);
            }
        }

        // ── Invention attempt (openness-gated, live-tunable) ─────────────────
        if (e.personality.openness > 62.0f
            && roll(rng) < 0.02f * g_liveConfig.inventionRateMul
                           * g_worldSeed.divergence.innovationLuck) {
            tryInvent(e, day, civ);
        }

        // ── Perform a known invented recipe when inputs allow → diffusion ────
        if (!e.knownRecipeIds.empty() && roll(rng) < 0.15f) {
            std::uniform_int_distribution<int> pr(0, (int)e.knownRecipeIds.size() - 1);
            if (const ActionRule* r = rule(e.knownRecipeIds[pr(rng)])) {
                if (r->invented) {
                    watchers.clear();
                    grid.forEachInRadius(e.posX, e.posY, vision,
                                         [&](Entity* w){ watchers.push_back(w); });
                    executeRule(e, *r, watchers, day, civ);
                }
            }
        }

        // ── Stigmergy deposits from ambient state ────────────────────────────
        if (field && field->ready()) {
            if (e.personality.extraversion > 60.0f && e.entityLoneliness < 40.0f)
                field->deposit(e.posX, e.posY, PheromoneField::SOCIAL, 1.5f);
            if (e.entityHealth < 25.0f || e.entityGeneralAnger > 80.0f) {
                field->deposit(e.posX, e.posY, PheromoneField::DANGER, 3.0f);
                e.episodicMap.remember(e.posX, e.posY, EpisodicMap::DANGER, day, 0.6f);
            }
        }
    }
}

// ── Persistence (SaveLoad v2) ─────────────────────────────────────────────────
// Text sections; only runtime-invented content is stored — seeded defs/rules
// are rebuilt by seed() so saves survive catalog tuning.

void ItemManager::saveTo(std::ofstream& f) const {
    f << "ITEMMGR_V1\n";
    int nd = 0;
    for (const auto& d : defs_) if (d.invented) nd++;
    f << nd << "\n";
    for (const auto& d : defs_) {
        if (!d.invented) continue;
        f << d.id << "|" << d.name << "|" << d.complexity
          << "|" << d.parentA << "|" << d.parentB << "|";
        for (size_t i = 0; i < d.tags.size(); ++i) f << (i ? "," : "") << d.tags[i];
        f << "|";
        bool first = true;
        for (const auto& kv : d.props) {
            f << (first ? "" : ",") << kv.first << "=" << kv.second;
            first = false;
        }
        f << "\n";
    }
    int nr = 0;
    for (const auto& r : rules_) if (r.invented) nr++;
    f << nr << "\n";
    for (const auto& r : rules_) {
        if (!r.invented) continue;
        f << r.id << "|" << r.name << "|" << r.complexity << "|"
          << r.inventorEntityId << "|" << r.inventedOnDay << "|";
        for (size_t i = 0; i < r.consumes.size(); ++i)
            f << (i ? "," : "") << r.consumes[i].tag << "=" << r.consumes[i].qty;
        f << "|";
        for (size_t i = 0; i < r.produces.size(); ++i)
            f << (i ? "," : "") << r.produces[i].defId << "=" << r.produces[i].qty;
        f << "\n";
    }
    f << inventedDefs_ << " " << inventedRules_ << " " << recipeSpreads_ << "\n";
}

void ItemManager::loadFrom(std::ifstream& f) {
    std::string marker;
    if (!(f >> marker) || marker != "ITEMMGR_V1") return;
    // Rebuild seeded tables first so invented ids append past them. Invented
    // ids are re-assigned on load; agents' knownRecipeIds refer to rule ids
    // that stay valid because invented content is appended in saved order.
    defs_.clear(); rules_.clear(); defByName_.clear();
    seededFlag_ = false;
    seed();

    int nd = 0; f >> nd; f.ignore();
    for (int i = 0; i < nd; ++i) {
        std::string line; std::getline(f, line);
        // id|name|complexity|parentA|parentB|tags|props
        ItemDef d; d.invented = true;
        size_t pos = 0; int fieldIdx = 0; std::string fields[7];
        for (int fi = 0; fi < 7; ++fi) {
            size_t bar = line.find('|', pos);
            fields[fi] = line.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
            if (bar == std::string::npos) break;
            pos = bar + 1;
            fieldIdx = fi + 1;
        }
        (void)fieldIdx;
        d.name = fields[1];
        d.complexity = (float)atof(fields[2].c_str());
        d.parentA = atoi(fields[3].c_str());
        d.parentB = atoi(fields[4].c_str());
        size_t tp = 0;
        while (tp < fields[5].size()) {
            size_t c = fields[5].find(',', tp);
            d.tags.push_back(fields[5].substr(tp, c == std::string::npos ? std::string::npos : c - tp));
            if (c == std::string::npos) break;
            tp = c + 1;
        }
        size_t pp = 0;
        while (pp < fields[6].size()) {
            size_t c  = fields[6].find(',', pp);
            std::string kv = fields[6].substr(pp, c == std::string::npos ? std::string::npos : c - pp);
            size_t eq = kv.find('=');
            if (eq != std::string::npos)
                d.props[kv.substr(0, eq)] = (float)atof(kv.substr(eq + 1).c_str());
            if (c == std::string::npos) break;
            pp = c + 1;
        }
        addDef(std::move(d));
        ++inventedDefs_;
    }

    int nr = 0; f >> nr; f.ignore();
    for (int i = 0; i < nr; ++i) {
        std::string line; std::getline(f, line);
        ActionRule r; r.invented = true;
        size_t pos = 0; std::string fields[7];
        for (int fi = 0; fi < 7; ++fi) {
            size_t bar = line.find('|', pos);
            fields[fi] = line.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
            if (bar == std::string::npos) break;
            pos = bar + 1;
        }
        r.name = fields[1];
        r.complexity = (float)atof(fields[2].c_str());
        r.inventorEntityId = atoi(fields[3].c_str());
        r.inventedOnDay    = atoi(fields[4].c_str());
        size_t cp = 0;
        while (cp < fields[5].size()) {
            size_t c  = fields[5].find(',', cp);
            std::string kv = fields[5].substr(cp, c == std::string::npos ? std::string::npos : c - cp);
            size_t eq = kv.find('=');
            if (eq != std::string::npos)
                r.consumes.push_back(TagQty{kv.substr(0, eq), (float)atof(kv.substr(eq + 1).c_str())});
            if (c == std::string::npos) break;
            cp = c + 1;
        }
        size_t op = 0;
        while (op < fields[6].size()) {
            size_t c  = fields[6].find(',', op);
            std::string kv = fields[6].substr(op, c == std::string::npos ? std::string::npos : c - op);
            size_t eq = kv.find('=');
            if (eq != std::string::npos)
                r.produces.push_back(ItemStack{atoi(kv.substr(0, eq).c_str()),
                                               (float)atof(kv.substr(eq + 1).c_str()), 100.0f});
            if (c == std::string::npos) break;
            op = c + 1;
        }
        addRule(std::move(r));
        ++inventedRules_;
    }
    f >> inventedDefs_ >> inventedRules_ >> recipeSpreads_;
}
