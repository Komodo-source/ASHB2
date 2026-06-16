#include "header/Economics.h"
#include "header/Entity.h"

#include <algorithm>
#include <cmath>

// The one shared market for the whole simulation.
Market g_market;

// ─────────────────────────────────────────────────────────────────────────────
//  Economic (per-entity wallet)
// ─────────────────────────────────────────────────────────────────────────────
void Economic::earnMoney(float qty) {
    if (qty <= 0.0f) return;
    token          += qty;
    monthlyRevenue += qty;
    memory_revenue.push_back(qty);
    if (memory_revenue.size() > 30) memory_revenue.erase(memory_revenue.begin());
}

void Economic::spendMoney(float qty) {
    token = std::max(0.0f, token - qty);
}

void Economic::resetMonth() {
    monthlyRevenue = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Product catalogue — [productionCost, profitabilityRatio, basicSellPrice]
//  Foods marked `farmed` only become tradable once agriculture is discovered.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

struct GoodDef {
    const char*  name;
    GoodCategory cat;
    float        productionCost;
    float        basePrice;
    bool         farmed;       // needs agriculture
    bool         ration;       // armies hoard it in war
};

const GoodDef CATALOG[] = {
    // ── FOOD ──────────────────────────────────────────────────────────────
    // Foraged / hunted: available from the very start.
    {"fish",         GoodCategory::FOOD,  30.0f,  88.0f,  false, true },
    {"venison",      GoodCategory::FOOD,  90.0f, 175.0f,  false, true },
    {"rabbit",       GoodCategory::FOOD,  40.0f,  95.0f,  false, false},
    {"duck",         GoodCategory::FOOD,  65.0f, 150.0f,  false, false},
    {"fruits",       GoodCategory::FOOD,  19.0f,  55.0f,  false, false},
    {"nuts",         GoodCategory::FOOD,  16.0f,  33.0f,  false, false},
    {"mushrooms",    GoodCategory::FOOD,   7.0f,  14.0f,  false, false},
    {"honey",        GoodCategory::FOOD,  36.0f, 100.0f,  false, false},
    {"eggs",         GoodCategory::FOOD,   8.0f,  18.0f,  false, false},
    {"bay",          GoodCategory::FOOD,   5.0f,   9.0f,  false, false},
    {"salt",         GoodCategory::FOOD,  10.0f,  35.0f,  false, true },
    // Cultivated / herded: unlock with agriculture.
    {"wheat",        GoodCategory::FOOD,  15.0f,  20.0f,  true,  true },
    {"barney",       GoodCategory::FOOD,  20.0f,  30.0f,  true,  true },
    {"bread",        GoodCategory::FOOD,  18.0f,  40.0f,  true,  true },
    {"vegetables",   GoodCategory::FOOD,  13.0f,  25.0f,  true,  false},
    {"olives/oil",   GoodCategory::FOOD,  28.0f,  70.0f,  true,  false},
    {"wine",         GoodCategory::FOOD,  45.0f, 130.0f,  true,  false},
    {"beer",         GoodCategory::FOOD,  22.0f,  60.0f,  true,  false},
    {"cheese/milk",  GoodCategory::FOOD,  12.0f,  26.0f,  true,  false},
    {"pork",         GoodCategory::FOOD, 100.0f, 200.0f,  true,  false},
    {"cow",          GoodCategory::FOOD, 150.0f, 220.0f,  true,  false},
    {"chicken",      GoodCategory::FOOD,  75.0f, 185.0f,  true,  false},
    {"lamb/sheep",   GoodCategory::FOOD,  85.0f, 164.0f,  true,  false},
    {"goat",         GoodCategory::FOOD,  70.0f, 140.0f,  true,  false},
    {"spices",       GoodCategory::FOOD, 120.0f, 320.0f,  true,  false},

    // ── OBJECTS ───────────────────────────────────────────────────────────
    {"flint_tool",      GoodCategory::OBJECT,  10.0f,  25.0f, false, false},
    {"stone_axe",       GoodCategory::OBJECT,  30.0f,  70.0f, false, false},
    {"wooden_spear",    GoodCategory::OBJECT,  18.0f,  45.0f, false, true },
    {"bow",             GoodCategory::OBJECT,  55.0f, 130.0f, false, true },
    {"clay_pot",        GoodCategory::OBJECT,  12.0f,  30.0f, false, false},
    {"basket",          GoodCategory::OBJECT,   8.0f,  20.0f, false, false},
    {"animal_hide",     GoodCategory::OBJECT,  25.0f,  60.0f, false, false},
    {"leather_clothes", GoodCategory::OBJECT,  60.0f, 150.0f, false, false},
    {"wool_cloth",      GoodCategory::OBJECT,  40.0f,  95.0f, false, false},
    {"fur_coat",        GoodCategory::OBJECT, 110.0f, 260.0f, false, false},
    {"bronze_tool",     GoodCategory::OBJECT, 130.0f, 300.0f, false, false},
    {"iron_tool",       GoodCategory::OBJECT, 160.0f, 380.0f, false, false},
    {"sword",           GoodCategory::OBJECT, 220.0f, 520.0f, false, true },
    {"shield",          GoodCategory::OBJECT, 140.0f, 310.0f, false, true },
    {"jewelry",         GoodCategory::OBJECT, 180.0f, 500.0f, false, false},
    {"pottery",         GoodCategory::OBJECT,  35.0f,  85.0f, false, false},
    {"rope",            GoodCategory::OBJECT,   9.0f,  22.0f, false, false},
    {"firewood",        GoodCategory::OBJECT,   4.0f,  10.0f, false, false},
    {"candle",          GoodCategory::OBJECT,  14.0f,  32.0f, false, false},
    {"plough",          GoodCategory::OBJECT,  95.0f, 210.0f, true,  false},
    {"cart",            GoodCategory::OBJECT, 200.0f, 450.0f, false, false},
    {"boat",            GoodCategory::OBJECT, 350.0f, 800.0f, false, false},
    {"medicine_herbs",  GoodCategory::OBJECT,  50.0f, 140.0f, false, false},
    {"parchment",       GoodCategory::OBJECT,  70.0f, 170.0f, false, false},
};

constexpr size_t PRICE_HISTORY_LEN = 64;
constexpr float  PRICE_FLOOR_MULT   = 0.30f;  // never crash below 30% of base
constexpr float  PRICE_CEIL_MULT    = 4.00f;  // never spike above 400% of base
constexpr float  PRICE_SENSITIVITY  = 0.12f;  // how hard imbalance moves price
constexpr float  REVERSION          = 0.04f;  // gentle pull back toward base

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Market
// ─────────────────────────────────────────────────────────────────────────────
void Market::init() {
    products.clear();
    for (const auto& g : CATALOG) {
        MarketProduct p;
        p.name                = g.name;
        p.category            = g.cat;
        p.productionCost      = g.productionCost;
        p.basePrice           = g.basePrice;
        p.price               = g.basePrice;
        p.requiresAgriculture = g.farmed;
        p.isArmyRation        = g.ration;
        products.push_back(std::move(p));
    }
    initialized = true;
}

MarketProduct* Market::find(const std::string& name) {
    for (auto& p : products)
        if (p.name == name) return &p;
    return nullptr;
}

// Give an entity a trade to ply. Producers are spread across goods by id so the
// shelves stay diverse; cultivated goods are only assigned once agriculture
// exists, otherwise the producer falls back to a foraged/craftable good.
void Market::assignProducer(Entity& ent, bool agricultureUnlocked) {
    if (products.empty()) return;
    int idx = std::abs(ent.entityId) % (int)products.size();
    if (products[idx].requiresAgriculture && !agricultureUnlocked) {
        // Walk forward to the next good this era can actually make.
        for (int step = 0; step < (int)products.size(); ++step) {
            int j = (idx + step) % (int)products.size();
            if (!products[j].requiresAgriculture) { idx = j; break; }
        }
    }
    ent.salary.producedProduct = idx;
}

void Market::update(std::vector<Entity>& entities, float warIntensity,
                    bool agricultureUnlocked) {
    if (!initialized) init();

    tickCount++;
    lastWarIntensity = std::max(0.0f, std::min(1.0f, warIntensity));

    // Reset the per-tick flows.
    for (auto& p : products) { p.supply = 0.0f; p.demand = 0.0f; }

    // Collect the producers of each good so revenue can be shared out later.
    std::vector<std::vector<Entity*>> producers(products.size());

    // ── Phase 1: SUPPLY ──────────────────────────────────────────────────────
    for (Entity& ent : entities) {
        if (ent.entityHealth <= 0.0f || ent.entityAge < 12.0f) continue;
        if (ent.salary.producedProduct < 0 ||
            ent.salary.producedProduct >= (int)products.size())
            assignProducer(ent, agricultureUnlocked);

        int idx = ent.salary.producedProduct;
        if (idx < 0 || idx >= (int)products.size()) continue;
        MarketProduct& p = products[idx];

        // Cultivated goods produce nothing until agriculture is known.
        if (p.requiresAgriculture && !agricultureUnlocked) continue;

        // Healthier, more conscientious, less bored workers make more.
        float capacity = 0.5f + ent.personality.conscientiousness / 80.0f
                              + ent.entityHealth / 200.0f;
        p.supply += capacity;
        producers[idx].push_back(&ent);
    }

    // ── Phase 2: DEMAND ──────────────────────────────────────────────────────
    // Everyone needs to eat; the hungrier they are the more they want. Soldiers
    // (and anyone in a tribe at war) stockpile rations, pulling them away from
    // ordinary people and bidding their price up.
    for (Entity& ent : entities) {
        if (ent.entityHealth <= 0.0f) continue;

        bool isSoldier = ent.specialization == "warrior";
        float hunger   = std::max(0.0f, (100.0f - ent.entityHealth)) / 100.0f;
        float foodWant = 0.6f + hunger * 1.6f;

        for (auto& p : products) {
            if (p.category != GoodCategory::FOOD) continue;
            if (p.requiresAgriculture && !agricultureUnlocked) continue;
            // Spread appetite across the affordable foods.
            float want = foodWant * (p.price <= ent.salary.token ? 1.0f : 0.25f)
                                  / (float)products.size();
            if (p.isArmyRation) {
                // Wartime rationing: armies buy a lot, civilians hoard a little.
                float mult = 1.0f + lastWarIntensity * (isSoldier ? 6.0f : 1.5f);
                want *= mult;
            }
            p.demand += want;
        }

        // Wealthy folk also buy objects (tools, clothes, luxuries).
        if (ent.salary.token > 120.0f) {
            float wealthDrive = std::min(2.5f, ent.salary.token / 300.0f);
            for (auto& p : products) {
                if (p.category != GoodCategory::OBJECT) continue;
                if (p.requiresAgriculture && !agricultureUnlocked) continue;
                if (p.price > ent.salary.token) continue;
                float want = wealthDrive / (float)products.size();
                if (p.isArmyRation)
                    want *= 1.0f + lastWarIntensity * (isSoldier ? 5.0f : 1.0f);
                p.demand += want;
            }
        }
    }

    // ── Phase 3: PRICE DISCOVERY ─────────────────────────────────────────────
    for (auto& p : products) {
        float total = p.supply + p.demand;
        if (total > 0.01f) {
            float imbalance = (p.demand - p.supply) / total; // -1..+1
            p.price *= (1.0f + PRICE_SENSITIVITY * imbalance);
        }
        // Gently revert toward the natural price so things don't drift forever.
        p.price += (p.basePrice - p.price) * REVERSION;
        p.price  = std::max(p.basePrice * PRICE_FLOOR_MULT,
                   std::min(p.basePrice * PRICE_CEIL_MULT, p.price));

        p.priceHistory.push_back(p.price);
        if (p.priceHistory.size() > PRICE_HISTORY_LEN)
            p.priceHistory.pop_front();
    }

    // ── Phase 4: TRADE & PAYOUT ──────────────────────────────────────────────
    totalTradeVolume = 0.0f;
    for (size_t i = 0; i < products.size(); ++i) {
        MarketProduct& p = products[i];
        float volume = std::min(p.supply, p.demand);
        p.lastVolume = volume;
        totalTradeVolume += volume;

        // Sellers split the profit of everything that moved.
        if (!producers[i].empty() && volume > 0.0f) {
            float profit  = volume * std::max(0.0f, p.price - p.productionCost);
            float perHead = profit / (float)producers[i].size();
            for (Entity* seller : producers[i])
                seller->salary.earnMoney(perHead);
        }
    }

    // ── Phase 5: CONSUMPTION (the anti-starvation pressure valve) ────────────
    // Hungry entities buy the cheapest food they can afford and eat it, trading
    // tokens for health. This is what keeps a fed, paying population alive.
    for (Entity& ent : entities) {
        if (ent.entityHealth <= 0.0f) continue;
        if (ent.entityHealth >= 85.0f) continue;       // not hungry enough yet

        MarketProduct* cheapest = nullptr;
        for (auto& p : products) {
            if (p.category != GoodCategory::FOOD) continue;
            if (p.requiresAgriculture && !agricultureUnlocked) continue;
            if (p.lastVolume <= 0.0f) continue;          // nothing on the shelf
            if (p.price > ent.salary.token) continue;    // can't afford it
            if (!cheapest || p.price < cheapest->price) cheapest = &p;
        }
        if (cheapest) {
            ent.salary.spendMoney(cheapest->price);
            ent.entityHealth = std::min(100.0f,
                                        ent.entityHealth + cheapest->nutrition());
        }
    }

    // Tally the money supply for the UI.
    totalMoneySupply = 0.0f;
    for (Entity& ent : entities)
        if (ent.entityHealth > 0.0f) totalMoneySupply += ent.salary.token;
}
