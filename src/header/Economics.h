#ifndef ECONOMICS_H
#define ECONOMICS_H

#include <string>
#include <vector>
#include <deque>

// Economics.h is pulled in (transitively) by Entity.h and FreeWillSystem.h, so
// it must NOT include Entity.h back — that would be a circular include. We only
// need a forward declaration; anything that actually touches Entity members
// lives in Economics.cpp where the full definition is available.
class Entity;

// ─────────────────────────────────────────────────────────────────────────────
//  Supply & demand market
//
//  Ancient money is the "token". Every tradable good has a natural equilibrium
//  price (basePrice). Each simulation tick the population expresses how much it
//  wants (demand) versus how much is produced (supply); the price drifts up when
//  people want more than is made, and down when the shelves are full. Wars make
//  armies hoard rations (bread/grain/salt/fish), draining them from common folk
//  and driving those prices up — exactly the kind of emergent scarcity we want.
// ─────────────────────────────────────────────────────────────────────────────

enum class GoodCategory { FOOD, OBJECT };

struct MarketProduct {
    std::string  name;
    GoodCategory category = GoodCategory::FOOD;

    float productionCost = 0.0f;  // tokens needed to make one unit
    float basePrice      = 0.0f;  // natural equilibrium / starting price
    float price          = 0.0f;  // current live market price

    float supply         = 0.0f;  // units offered this tick
    float demand         = 0.0f;  // units wanted this tick
    float lastVolume     = 0.0f;  // units actually traded last tick

    // Cultivated goods only appear once a tribe discovers agriculture; before
    // that, only foraged / hunted foods can be traded.
    bool  requiresAgriculture = false;
    // Staple rations armies stockpile in wartime.
    bool  isArmyRation        = false;

    std::deque<float> priceHistory;  // rolling window for the UI trend line

    float nutrition() const {           // health restored when eaten
        return category == GoodCategory::FOOD ? 5.0f + basePrice * 0.04f : 0.0f;
    }
};

// Per-entity wallet + role in the economy. Lives on every Entity ("salary").
class Economic {
public:
    float token          = 0.0f;   // current wealth
    float monthlyRevenue = 0.0f;   // earnings accumulated this period
    std::vector<float> memory_revenue;
    int   producedProduct = -1;    // index into Market::products this entity sells

    Economic(float startToken = 0.0f) : token(startToken) {}
    // Allow `entity.salary = 200;` style assignment of a starting balance.
    Economic& operator=(float startToken) { token = startToken; return *this; }

    void  earnMoney(float qty);
    void  spendMoney(float qty);
    float getMonthlyRevenue() const { return monthlyRevenue; }
    void  resetMonth();
};

class Market {
public:
    std::vector<MarketProduct> products;
    bool  initialized = false;

    // Headline numbers surfaced in the UI.
    float totalTradeVolume = 0.0f;  // units traded last tick
    float totalMoneySupply = 0.0f;  // sum of every wallet
    float lastWarIntensity = 0.0f;  // 0..1, drives ration demand
    int   tickCount        = 0;

    void init();

    // Recompute supply, demand and prices, then run the trades for this tick.
    //   warIntensity        : 0..1 share of the population caught up in war
    //   agricultureUnlocked : have any cultivated foods been discovered yet?
    void update(std::vector<Entity>& entities, float warIntensity,
                bool agricultureUnlocked);

    MarketProduct* find(const std::string& name);

private:
    void assignProducer(Entity& ent, bool agricultureUnlocked);
};

extern Market g_market;

#endif // ECONOMICS_H
