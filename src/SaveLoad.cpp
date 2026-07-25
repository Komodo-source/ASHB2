#include "./header/SaveLoad.h"
#include "./header/Entity.h"
#include "./header/FreeWillSystem.h"
#include "./header/CivilizationEngine.h"
#include "./header/BetterRand.h"
#include "./header/LiveConfig.h"
#include "./header/Economics.h"
#include "./header/SocialOrder.h"
#include "./header/Kinship.h"
#include "core/SimClock.h"
#include "items/ItemSystem.h"      // Step 5b: emergence save section
#include "world/PheromoneField.h"
#include "world/ResourceSystem.h"
#include "world/Ecosystem.h"
#include "environment/EnvironmentModel.h"

// Define the constants that were moved to extern in the header
const char* SAVES_DIR = "saves";
const char* SAVE_FORMAT_V3 = "ASHB2_SAVE_V3";
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <sys/stat.h>
#include <direct.h>

// ── Global state we save/restore ─────────────────────────────────────────────
// Declared extern elsewhere; we simply include the headers that declare them.
//   g_liveConfig     — LiveConfig (header/LiveConfig.h)
//   g_market         — Market (header/Economics.h)
//   g_resources      — ResourceSystem (world/ResourceSystem.h)
//   g_ecosystem      — EcosystemSystem (world/Ecosystem.h)
//   g_env            — EnvironmentalState (environment/EnvironmentModel.h) — declared static in main.cpp
//   g_seasonalFoodModifier, g_seasonTemperature, g_harvestLuck, g_seasonName — declared in main.cpp
//   g_deathLedger, g_actionTally — declared static in main.cpp
//   globalSocialOrder — SocialOrderSystem* (header/SocialOrder.h)
//   globalKinship     — KinshipSystem* (header/Kinship.h)
//
// Because these are declared in main.cpp (static) or as globals in their own
// .cpp files, we declare them extern here so the linker finds them.

// Environment globals (defined in main.cpp)
extern float       g_seasonalFoodModifier;
extern float       g_seasonTemperature;
extern float       g_harvestLuck;
extern std::string g_seasonName;

// Death ledger & action tally (defined static in main.cpp)
// We access these through dedicated load/save functions that take references.
// They are defined as `static` in main.cpp, so we cannot extern them directly.
// Instead, the caller passes them in through the civ parameter's totals, and we
// provide separate serialization markers for them.

// ── V2 helpers: one-line records with '|' fields; ids joined by ',' ──────────
static std::string joinInts(const std::vector<int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) { if (i) s += ','; s += std::to_string(v[i]); }
    return s;
}
static std::vector<int> splitInts(const std::string& s) {
    std::vector<int> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) if (!tok.empty()) out.push_back(std::stoi(tok));
    return out;
}
// map<int,float> / map<int,enum> as "id:value" pairs joined by ','
template <typename M>
static std::string joinMap(const M& m) {
    std::string s;
    bool first = true;
    for (const auto& [k, v] : m) {
        if (!first) s += ',';
        first = false;
        s += std::to_string(k) + ':' + std::to_string((float)v);
    }
    return s;
}
static void parseFloatMap(const std::string& s, std::map<int, float>& out) {
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        size_t c = tok.find(':');
        if (c != std::string::npos)
            out[std::stoi(tok.substr(0, c))] = std::stof(tok.substr(c + 1));
    }
}

// ── Backup directory helpers ─────────────────────────────────────────────────
std::string savePath(const std::string& filename) {
    // If filename already contains a path separator, use it as-is.
    if (filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos) {
        return filename;
    }
    return std::string(SAVES_DIR) + "/" + filename;
}

std::string autobackupFilename() {
    std::time_t t = std::time(nullptr);
    char buf[64];
    // Use std::localtime (single-threaded save operation — safe enough).
    // MinGW-w64 doesn't reliably expose localtime_s, and localtime_r requires
    // POSIX feature-test macros, so we use the C89 function with a local copy.
    struct tm* ptm = std::localtime(&t);
    if (ptm) {
        struct tm tm = *ptm;  // copy so nothing races us
        std::strftime(buf, sizeof(buf), "autosave_%Y-%m-%d_%H%M%S.txt", &tm);
    } else {
        strcpy(buf, "autosave_fallback.txt");
    }
    return std::string(buf);
}

bool ensureSavesDir() {
    struct stat st;
    if (stat(SAVES_DIR, &st) == 0 && (st.st_mode & S_IFDIR)) return true;
    int ret = _mkdir(SAVES_DIR);
    if (ret == 0) {
        std::cout << "Created saves directory: " << SAVES_DIR << std::endl;
        return true;
    }
    std::cerr << "Failed to create saves directory: " << SAVES_DIR << std::endl;
    return false;
}

// ── Save helpers for individual subsystems ───────────────────────────────────

// --- Environment state ---
static void saveEnvironment(std::ofstream& file) {
    file << "ENV_V1\n";
    file << "SEASON_NAME:" << g_seasonName << "\n";
    file << "SEASON_TEMP:" << g_seasonTemperature << "\n";
    file << "FOOD_MOD:" << g_seasonalFoodModifier << "\n";
    file << "HARVEST_LUCK:" << g_harvestLuck << "\n";
    file << "ENV_END\n";
}

static bool loadEnvironment(std::ifstream& file) {
    std::streampos at = file.tellg();
    std::string line;
    file >> std::ws;
    if (!std::getline(file, line) || line != "ENV_V1") {
        file.clear();
        file.seekg(at);
        return false;
    }
    while (std::getline(file, line)) {
        if (line == "ENV_END") break;
        else if (line.rfind("SEASON_NAME:", 0) == 0)  g_seasonName = line.substr(12);
        else if (line.rfind("SEASON_TEMP:", 0) == 0)  g_seasonTemperature = std::stof(line.substr(12));
        else if (line.rfind("FOOD_MOD:", 0) == 0)     g_seasonalFoodModifier = std::stof(line.substr(9));
        else if (line.rfind("HARVEST_LUCK:", 0) == 0) g_harvestLuck = std::stof(line.substr(13));
    }
    return true;
}

// --- LiveConfig ---
static void saveLiveConfig(std::ofstream& file) {
    file << "LIVECFG_V1\n";
    file << "MOVEFORCE:" << g_liveConfig.moveForceMul << "\n";
    file << "MORTALITY:" << g_liveConfig.mortalityMul << "\n";
    file << "FOODYIELD:" << g_liveConfig.foodYieldMul << "\n";
    file << "AGGRESSION:" << g_liveConfig.aggressionMul << "\n";
    file << "CORRUPTION:" << g_liveConfig.corruptionMul << "\n";
    file << "MUTATION:" << g_liveConfig.mutationRateMul << "\n";
    file << "PHEROMONE:" << g_liveConfig.pheromoneDecayMul << "\n";
    file << "INVENTION:" << g_liveConfig.inventionRateMul << "\n";
    file << "NEATBRAIN:" << g_liveConfig.neatBrainShare << "\n";
    file << "EMOTION:" << g_liveConfig.emotionMul << "\n";
    file << "CULTURE:" << g_liveConfig.cultureMul << "\n";
    file << "HEATMAP:" << (g_liveConfig.showDensityHeatmap ? 1 : 0) << "\n";
    file << "PHEROSHOW:" << (g_liveConfig.showPheromones ? 1 : 0) << "\n";
    file << "GENETINT:" << (g_liveConfig.showGeneticTint ? 1 : 0) << "\n";
    file << "LCFG_END\n";
}

static bool loadLiveConfig(std::ifstream& file) {
    std::streampos at = file.tellg();
    std::string line;
    file >> std::ws;
    if (!std::getline(file, line) || line != "LIVECFG_V1") {
        file.clear();
        file.seekg(at);
        return false;
    }
    LiveConfig cfg; // defaults are 1.0
    while (std::getline(file, line)) {
        if (line == "LCFG_END") break;
        else if (line.rfind("MOVEFORCE:", 0) == 0)   cfg.moveForceMul = std::stof(line.substr(10));
        else if (line.rfind("MORTALITY:", 0) == 0)   cfg.mortalityMul = std::stof(line.substr(10));
        else if (line.rfind("FOODYIELD:", 0) == 0)   cfg.foodYieldMul = std::stof(line.substr(10));
        else if (line.rfind("AGGRESSION:", 0) == 0)  cfg.aggressionMul = std::stof(line.substr(11));
        else if (line.rfind("CORRUPTION:", 0) == 0)  cfg.corruptionMul = std::stof(line.substr(11));
        else if (line.rfind("MUTATION:", 0) == 0)    cfg.mutationRateMul = std::stof(line.substr(9));
        else if (line.rfind("PHEROMONE:", 0) == 0)   cfg.pheromoneDecayMul = std::stof(line.substr(10));
        else if (line.rfind("INVENTION:", 0) == 0)   cfg.inventionRateMul = std::stof(line.substr(10));
        else if (line.rfind("NEATBRAIN:", 0) == 0)   cfg.neatBrainShare = std::stof(line.substr(10));
        else if (line.rfind("EMOTION:", 0) == 0)     cfg.emotionMul = std::stof(line.substr(8));
        else if (line.rfind("CULTURE:", 0) == 0)     cfg.cultureMul = std::stof(line.substr(8));
        else if (line.rfind("HEATMAP:", 0) == 0)     cfg.showDensityHeatmap = (std::stoi(line.substr(8)) != 0);
        else if (line.rfind("PHEROSHOW:", 0) == 0)   cfg.showPheromones = (std::stoi(line.substr(10)) != 0);
        else if (line.rfind("GENETINT:", 0) == 0)    cfg.showGeneticTint = (std::stoi(line.substr(9)) != 0);
    }
    g_liveConfig = cfg;
    return true;
}

// --- Resource system ---
static void saveResources(std::ofstream& file) {
    file << "RESOURCES_V1\n";
    file << "POOL_COUNT:" << g_resources.pools.size() << "\n";
    for (const ResourcePool& p : g_resources.pools) {
        file << "RP|";
        for (int r = 0; r < RES_COUNT; ++r) {
            if (r) file << ',';
            file << p.stock[r] << ':' << p.capacity[r] << ':' << p.baseCapacity[r]
                 << ':' << p.regen[r];
        }
        file << '|' << p.tileCount << '|' << (p.active ? 1 : 0) << "\n";
    }
    file << "RSC_END\n";
}

static bool loadResources(std::ifstream& file) {
    std::streampos at = file.tellg();
    std::string line;
    file >> std::ws;
    if (!std::getline(file, line) || line != "RESOURCES_V1") {
        file.clear();
        file.seekg(at);
        return false;
    }
    std::getline(file, line); // POOL_COUNT:
    if (line.rfind("POOL_COUNT:", 0) != 0) return false;
    int count = std::stoi(line.substr(11));
    g_resources.pools.resize(count);
    for (int i = 0; i < count; ++i) {
        std::getline(file, line);
        if (line.rfind("RP|", 0) != 0) continue;
        std::string body = line.substr(3);
        size_t bar = body.find('|');
        // Parse stocks:capacity:baseCapacity:regen tuples
        std::string stocksPart = body.substr(0, bar);
        std::stringstream ss(stocksPart);
        std::string tok;
        int ri = 0;
        while (std::getline(ss, tok, ',') && ri < RES_COUNT) {
            size_t c1 = tok.find(':');
            size_t c2 = tok.find(':', c1 + 1);
            size_t c3 = tok.rfind(':');
            if (c1 != std::string::npos && c2 != std::string::npos && c3 != std::string::npos && c3 > c2) {
                g_resources.pools[i].stock[ri]        = std::stof(tok.substr(0, c1));
                g_resources.pools[i].capacity[ri]     = std::stof(tok.substr(c1 + 1, c2 - c1 - 1));
                g_resources.pools[i].baseCapacity[ri] = std::stof(tok.substr(c2 + 1, c3 - c2 - 1));
                g_resources.pools[i].regen[ri]        = std::stof(tok.substr(c3 + 1));
            }
            ++ri;
        }
        // Parse trailing fields
        std::string rest = body.substr(bar + 1);
        size_t bar2 = rest.find('|');
        g_resources.pools[i].tileCount = std::stoi(rest.substr(0, bar2));
        g_resources.pools[i].active    = (std::stoi(rest.substr(bar2 + 1)) != 0);
    }
    std::getline(file, line); // RSC_END
    return true;
}

// --- Ecosystem ---
static void saveEcosystem(std::ofstream& file) {
    file << "ECOSYSTEM_V1\n";
    file << "EREG_COUNT:" << g_ecosystem.regions.size() << "\n";
    for (const RegionEcology& e : g_ecosystem.regions) {
        file << "ER|" << (e.active ? 1 : 0) << '|'
             << e.plants << '|' << e.herbivores << '|' << e.predators << '|'
             << e.plantCap << '|' << e.herbCap << '|'
             << e.totalHunted << '|' << e.collapses << "\n";
    }
    file << "ECO_END\n";
}

static bool loadEcosystem(std::ifstream& file) {
    std::streampos at = file.tellg();
    std::string line;
    file >> std::ws;
    if (!std::getline(file, line) || line != "ECOSYSTEM_V1") {
        file.clear();
        file.seekg(at);
        return false;
    }
    std::getline(file, line); // EREG_COUNT:
    if (line.rfind("EREG_COUNT:", 0) != 0) return false;
    int count = std::stoi(line.substr(11));
    g_ecosystem.regions.resize(count);
    for (int i = 0; i < count; ++i) {
        std::getline(file, line);
        if (line.rfind("ER|", 0) != 0) continue;
        std::string body = line.substr(3);
        std::stringstream ss(body);
        std::string tok;
        std::vector<std::string> parts;
        while (std::getline(ss, tok, '|')) parts.push_back(tok);
        if (parts.size() >= 8) {
            g_ecosystem.regions[i].active     = (std::stoi(parts[0]) != 0);
            g_ecosystem.regions[i].plants     = std::stof(parts[1]);
            g_ecosystem.regions[i].herbivores = std::stof(parts[2]);
            g_ecosystem.regions[i].predators  = std::stof(parts[3]);
            g_ecosystem.regions[i].plantCap   = std::stof(parts[4]);
            g_ecosystem.regions[i].herbCap    = std::stof(parts[5]);
            g_ecosystem.regions[i].totalHunted= std::stof(parts[6]);
            g_ecosystem.regions[i].collapses  = std::stoi(parts[7]);
        }
    }
    std::getline(file, line); // ECO_END
    return true;
}

// --- Market ---
static void saveMarket(std::ofstream& file) {
    file << "MARKET_V1\n";
    file << "INIT:" << (g_market.initialized ? 1 : 0) << "\n";
    file << "TRADEVOL:" << g_market.totalTradeVolume << "\n";
    file << "MONEYSUPPLY:" << g_market.totalMoneySupply << "\n";
    file << "WARINTENSITY:" << g_market.lastWarIntensity << "\n";
    file << "TICKCOUNT:" << g_market.tickCount << "\n";
    file << "PROD_COUNT:" << g_market.products.size() << "\n";
    for (const MarketProduct& p : g_market.products) {
        file << "PROD|" << p.name << '|' << (int)p.category << '|'
             << p.productionCost << '|' << p.basePrice << '|' << p.price << '|'
             << p.supply << '|' << p.demand << '|' << p.lastVolume << '|'
             << (p.requiresAgriculture ? 1 : 0) << '|' << (p.isArmyRation ? 1 : 0) << '|'
             << p.def_value << '|' << p.atk_value << "\n";
    }
    file << "MKT_END\n";
}

static bool loadMarket(std::ifstream& file) {
    std::streampos at = file.tellg();
    std::string line;
    file >> std::ws;
    if (!std::getline(file, line) || line != "MARKET_V1") {
        file.clear();
        file.seekg(at);
        return false;
    }
    while (std::getline(file, line)) {
        if (line == "MKT_END") break;
        else if (line.rfind("INIT:", 0) == 0)       g_market.initialized = (std::stoi(line.substr(5)) != 0);
        else if (line.rfind("TRADEVOL:", 0) == 0)   g_market.totalTradeVolume = std::stof(line.substr(9));
        else if (line.rfind("MONEYSUPPLY:", 0) == 0) g_market.totalMoneySupply = std::stof(line.substr(12));
        else if (line.rfind("WARINTENSITY:", 0) == 0) g_market.lastWarIntensity = std::stof(line.substr(13));
        else if (line.rfind("TICKCOUNT:", 0) == 0)  g_market.tickCount = std::stoi(line.substr(10));
        else if (line.rfind("PROD_COUNT:", 0) == 0) {
            int n = std::stoi(line.substr(11));
            g_market.products.resize(n);
            for (int i = 0; i < n; ++i) {
                std::getline(file, line);
                if (line.rfind("PROD|", 0) != 0) continue;
                std::vector<std::string> f;
                std::stringstream ss(line);
                std::string tok;
                while (std::getline(ss, tok, '|')) f.push_back(tok);
                if (f.size() >= 12) {
                    MarketProduct& p = g_market.products[i];
                    p.name                = f[1];
                    p.category            = (GoodCategory)std::stoi(f[2]);
                    p.productionCost      = std::stof(f[3]);
                    p.basePrice           = std::stof(f[4]);
                    p.price               = std::stof(f[5]);
                    p.supply              = std::stof(f[6]);
                    p.demand              = std::stof(f[7]);
                    p.lastVolume          = std::stof(f[8]);
                    p.requiresAgriculture = (std::stoi(f[9]) != 0);
                    p.isArmyRation        = (std::stoi(f[10]) != 0);
                    p.def_value           = std::stoi(f[11]);
                    p.atk_value           = f.size() > 12 ? std::stoi(f[12]) : 0;
                }
            }
        }
    }
    // Rebuild the catalog from scratch as fallback if nothing was loaded
    if (!g_market.initialized) g_market.init();
    return true;
}

// --- Social Order ---
static void saveSocialOrder(std::ofstream& file) {
    file << "SOCIAL_V1\n";
    if (!globalSocialOrder) { file << "SOCIAL_NULL:1\nSOC_END\n"; return; }
    file << "BOND_COUNT:" << globalSocialOrder->bonds.size() << "\n";
    for (const Clientela& b : globalSocialOrder->bonds) {
        file << "BOND|" << b.patronId << '|' << b.clientId << '|'
             << b.loyalty << '|' << b.obligation << '|' << b.sinceYear << "\n";
    }
    file << "DEBT_COUNT:" << globalSocialOrder->debts.size() << "\n";
    for (const Debt& d : globalSocialOrder->debts) {
        file << "DEBT|" << d.creditorId << '|' << d.debtorId << '|'
             << d.amount << '|' << d.sinceYear << "\n";
    }
    file << "SLAVES:" << globalSocialOrder->cSlaves << "\n";
    file << "PLEBS:" << globalSocialOrder->cPlebs << "\n";
    file << "PATRICIANS:" << globalSocialOrder->cPatricians << "\n";
    file << "ENSLAVED:" << globalSocialOrder->totalEnslaved << "\n";
    file << "FREED:" << globalSocialOrder->totalFreed << "\n";
    file << "ASCENDED:" << globalSocialOrder->totalAscended << "\n";
    file << "SOC_END\n";
}

static bool loadSocialOrder(std::ifstream& file) {
    std::streampos at = file.tellg();
    std::string line;
    file >> std::ws;
    if (!std::getline(file, line) || line != "SOCIAL_V1") {
        file.clear();
        file.seekg(at);
        return false;
    }
    if (!globalSocialOrder) return true;
    while (std::getline(file, line)) {
        if (line == "SOC_END") break;
        else if (line.rfind("SOCIAL_NULL:", 0) == 0) return true;
        else if (line.rfind("BOND_COUNT:", 0) == 0) {
            int n = std::stoi(line.substr(11));
            globalSocialOrder->bonds.clear();
            for (int i = 0; i < n; ++i) {
                std::getline(file, line);
                if (line.rfind("BOND|", 0) != 0) continue;
                std::vector<std::string> f;
                std::stringstream ss(line);
                std::string tok;
                while (std::getline(ss, tok, '|')) f.push_back(tok);
                if (f.size() >= 6) {
                    Clientela b;
                    b.patronId   = std::stoi(f[1]);
                    b.clientId   = std::stoi(f[2]);
                    b.loyalty    = std::stof(f[3]);
                    b.obligation = std::stof(f[4]);
                    b.sinceYear  = std::stoi(f[5]);
                    globalSocialOrder->bonds.push_back(b);
                }
            }
        }
        else if (line.rfind("DEBT_COUNT:", 0) == 0) {
            int n = std::stoi(line.substr(11));
            globalSocialOrder->debts.clear();
            for (int i = 0; i < n; ++i) {
                std::getline(file, line);
                if (line.rfind("DEBT|", 0) != 0) continue;
                std::vector<std::string> f;
                std::stringstream ss(line);
                std::string tok;
                while (std::getline(ss, tok, '|')) f.push_back(tok);
                if (f.size() >= 5) {
                    Debt d;
                    d.creditorId = std::stoi(f[1]);
                    d.debtorId   = std::stoi(f[2]);
                    d.amount     = std::stof(f[3]);
                    d.sinceYear  = std::stoi(f[4]);
                    globalSocialOrder->debts.push_back(d);
                }
            }
        }
        else if (line.rfind("SLAVES:", 0) == 0)      globalSocialOrder->cSlaves = std::stoi(line.substr(7));
        else if (line.rfind("PLEBS:", 0) == 0)       globalSocialOrder->cPlebs = std::stoi(line.substr(6));
        else if (line.rfind("PATRICIANS:", 0) == 0)  globalSocialOrder->cPatricians = std::stoi(line.substr(11));
        else if (line.rfind("ENSLAVED:", 0) == 0)    globalSocialOrder->totalEnslaved = std::stoi(line.substr(9));
        else if (line.rfind("FREED:", 0) == 0)       globalSocialOrder->totalFreed = std::stoi(line.substr(6));
        else if (line.rfind("ASCENDED:", 0) == 0)    globalSocialOrder->totalAscended = std::stoi(line.substr(9));
    }
    return true;
}

// --- Kinship ---
static void saveKinship(std::ofstream& file) {
    file << "KINSHIP_V1\n";
    if (!globalKinship) { file << "KIN_NULL:1\nKIN_END\n"; return; }
    file << "FAM_COUNT:" << globalKinship->families.size() << "\n";
    for (const Family& f : globalKinship->families) {
        file << "FAM|" << f.id << '|' << f.name << '|'
             << f.founderId << '|' << f.originRegionId << '|' << f.foundedYear << '|'
             << joinInts(f.memberIds) << '|'
             << f.reputation << '|' << f.births << '|' << f.prestige << '|'
             << (f.prominent ? 1 : 0) << '|' << f.generation << "\n";
    }
    // nextFamilyId is private; we'll compute it after load from max(id)+1
    file << "KIN_END\n";
}

static bool loadKinship(std::ifstream& file) {
    std::streampos at = file.tellg();
    std::string line;
    file >> std::ws;
    if (!std::getline(file, line) || line != "KINSHIP_V1") {
        file.clear();
        file.seekg(at);
        return false;
    }
    if (!globalKinship) return true;
    int maxFamilyId = 0;
    while (std::getline(file, line)) {
        if (line == "KIN_END") break;
        else if (line.rfind("KIN_NULL:", 0) == 0) return true;
        else if (line.rfind("FAM_COUNT:", 0) == 0) {
            int n = std::stoi(line.substr(10));
            globalKinship->families.clear();
            globalKinship->families.reserve(n);
            for (int i = 0; i < n; ++i) {
                std::getline(file, line);
                if (line.rfind("FAM|", 0) != 0) continue;
                std::vector<std::string> f;
                std::stringstream ss(line);
                std::string tok;
                while (std::getline(ss, tok, '|')) f.push_back(tok);
                if (f.size() >= 12) {
                    Family fam;
                    fam.id             = std::stoi(f[1]);
                    fam.name           = f[2];
                    fam.founderId      = std::stoi(f[3]);
                    fam.originRegionId = std::stoi(f[4]);
                    fam.foundedYear    = std::stoi(f[5]);
                    fam.memberIds      = splitInts(f[6]);
                    fam.reputation     = std::stof(f[7]);
                    fam.births         = std::stoi(f[8]);
                    fam.prestige       = std::stof(f[9]);
                    fam.prominent      = (std::stoi(f[10]) != 0);
                    fam.generation     = std::stoi(f[11]);
                    globalKinship->families.push_back(fam);
                    if (fam.id > maxFamilyId) maxFamilyId = fam.id;
                }
            }
        }
    }
    // Advance nextFamilyId past the highest loaded ID so new families
    // don't collide with restored ones.
    globalKinship->resetNextFamilyId(maxFamilyId + 1);
    return true;
}

// --- Civ engine stats ---
static void saveCivStats(std::ofstream& file, const CivilizationEngine* civ) {
    file << "CIVSTATS_V1\n";
    if (!civ) { file << "CS_NULL:1\nCS_END\n"; return; }
    file << "TOTAL_BIRTHS:" << civ->totalBirths << "\n";
    file << "TOTAL_DEATHS:" << civ->totalDeaths << "\n";
    file << "TOTAL_WARDEATHS:" << civ->totalWarDeaths << "\n";
    file << "TOTAL_BATTLES:" << civ->totalBattles << "\n";
    file << "TOTAL_WARS:" << civ->totalWarsDeclared << "\n";
    file << "TOTAL_ETHNIC:" << civ->totalEthnicWars << "\n";
    file << "TOTAL_CONQUESTS:" << civ->totalConquests << "\n";
    file << "TOTAL_COUPLES_BROKEN:" << civ->totalCouplesBroken << "\n";
    file << "PEAK_POP:" << civ->peakPopulation << "\n";
    file << "TOTAL_TREATIES:" << civ->totalTreatiesSigned << "\n";
    file << "TOTAL_COUPS:" << civ->totalCoups << "\n";
    file << "TOTAL_VASSAL:" << civ->totalVassalizations << "\n";
    file << "TOTAL_REBELLIONS:" << civ->totalRebellions << "\n";
    file << "TOTAL_ELECTIONS:" << civ->totalElections << "\n";
    file << "TOTAL_SUCCESSIONS:" << civ->totalSuccessions << "\n";
    file << "TOTAL_CHALLENGES:" << civ->totalChallenges << "\n";
    file << "TOTAL_SCANDALS:" << civ->totalScandals << "\n";
    file << "TOTAL_DEPOSITIONS:" << civ->totalDepositions << "\n";
    file << "TOTAL_TECHSPREADS:" << civ->totalTechSpreads << "\n";
    file << "TOTAL_COLONIES:" << civ->totalColonies << "\n";
    file << "TOTAL_GREAT_FAMILIES:" << civ->totalGreatFamilies << "\n";
    file << "TOTAL_SAGAS:" << civ->totalSagas << "\n";
    file << "TOTAL_DISASTERS:" << civ->totalDisasters << "\n";
    file << "TOTAL_CIVILWARS:" << civ->totalCivilWars << "\n";
    file << "ELITE_COUNT:" << civ->eliteCount << "\n";
    file << "UPPER_COUNT:" << civ->upperCount << "\n";
    file << "MIDDLE_COUNT:" << civ->middleCount << "\n";
    file << "LOWER_COUNT:" << civ->lowerCount << "\n";
    file << "OUTCAST_COUNT:" << civ->outcastCount << "\n";
    file << "DARKAGE_COUNT:" << civ->darkAgeCount << "\n";
    file << "MAX_ERA:" << (int)civ->maxEraAchieved << "\n";
    file << "LAST_COLLAPSE_DAY:" << civ->lastCollapseDay << "\n";
    // Diplomacy: treaties
    file << "TREATY_COUNT:" << civ->treaties.size() << "\n";
    for (const Treaty& t : civ->treaties) {
        file << "TREATY|" << (int)t.type << '|' << t.tribeA << '|' << t.tribeB << '|'
             << t.startDay << '|' << t.expiryDay << '|' << t.tributeAmount << '|'
             << (t.active ? 1 : 0) << "\n";
    }
    file << "CS_END\n";
}

static bool loadCivStats(std::ifstream& file, CivilizationEngine* civ) {
    std::streampos at = file.tellg();
    std::string line;
    file >> std::ws;
    if (!std::getline(file, line) || line != "CIVSTATS_V1") {
        file.clear();
        file.seekg(at);
        return false;
    }
    if (!civ) {
        // Skip until CS_END
        while (std::getline(file, line)) if (line == "CS_END") break;
        return true;
    }
    while (std::getline(file, line)) {
        if (line == "CS_END") break;
        else if (line.rfind("CS_NULL:", 0) == 0) return true;
        else if (line.rfind("TOTAL_BIRTHS:", 0) == 0)  civ->totalBirths = std::stoi(line.substr(13));
        else if (line.rfind("TOTAL_DEATHS:", 0) == 0)  civ->totalDeaths = std::stoi(line.substr(13));
        else if (line.rfind("TOTAL_WARDEATHS:", 0) == 0) civ->totalWarDeaths = std::stoi(line.substr(16));
        else if (line.rfind("TOTAL_BATTLES:", 0) == 0) civ->totalBattles = std::stoi(line.substr(14));
        else if (line.rfind("TOTAL_WARS:", 0) == 0)    civ->totalWarsDeclared = std::stoi(line.substr(11));
        else if (line.rfind("TOTAL_ETHNIC:", 0) == 0)  civ->totalEthnicWars = std::stoi(line.substr(13));
        else if (line.rfind("TOTAL_CONQUESTS:", 0) == 0) civ->totalConquests = std::stoi(line.substr(16));
        else if (line.rfind("TOTAL_COUPLES_BROKEN:", 0) == 0) civ->totalCouplesBroken = std::stoi(line.substr(21));
        else if (line.rfind("PEAK_POP:", 0) == 0)      civ->peakPopulation = std::stoi(line.substr(9));
        else if (line.rfind("TOTAL_TREATIES:", 0) == 0) civ->totalTreatiesSigned = std::stoi(line.substr(15));
        else if (line.rfind("TOTAL_COUPS:", 0) == 0)   civ->totalCoups = std::stoi(line.substr(12));
        else if (line.rfind("TOTAL_VASSAL:", 0) == 0)  civ->totalVassalizations = std::stoi(line.substr(13));
        else if (line.rfind("TOTAL_REBELLIONS:", 0) == 0) civ->totalRebellions = std::stoi(line.substr(17));
        else if (line.rfind("TOTAL_ELECTIONS:", 0) == 0) civ->totalElections = std::stoi(line.substr(16));
        else if (line.rfind("TOTAL_SUCCESSIONS:", 0) == 0) civ->totalSuccessions = std::stoi(line.substr(18));
        else if (line.rfind("TOTAL_CHALLENGES:", 0) == 0) civ->totalChallenges = std::stoi(line.substr(17));
        else if (line.rfind("TOTAL_SCANDALS:", 0) == 0) civ->totalScandals = std::stoi(line.substr(15));
        else if (line.rfind("TOTAL_DEPOSITIONS:", 0) == 0) civ->totalDepositions = std::stoi(line.substr(18));
        else if (line.rfind("TOTAL_TECHSPREADS:", 0) == 0) civ->totalTechSpreads = std::stoi(line.substr(18));
        else if (line.rfind("TOTAL_COLONIES:", 0) == 0) civ->totalColonies = std::stoi(line.substr(15));
        else if (line.rfind("TOTAL_GREAT_FAMILIES:", 0) == 0) civ->totalGreatFamilies = std::stoi(line.substr(21));
        else if (line.rfind("TOTAL_SAGAS:", 0) == 0)    civ->totalSagas = std::stoi(line.substr(12));
        else if (line.rfind("TOTAL_DISASTERS:", 0) == 0) civ->totalDisasters = std::stoi(line.substr(16));
        else if (line.rfind("TOTAL_CIVILWARS:", 0) == 0) civ->totalCivilWars = std::stoi(line.substr(16));
        else if (line.rfind("ELITE_COUNT:", 0) == 0)    civ->eliteCount = std::stoi(line.substr(12));
        else if (line.rfind("UPPER_COUNT:", 0) == 0)    civ->upperCount = std::stoi(line.substr(12));
        else if (line.rfind("MIDDLE_COUNT:", 0) == 0)   civ->middleCount = std::stoi(line.substr(13));
        else if (line.rfind("LOWER_COUNT:", 0) == 0)    civ->lowerCount = std::stoi(line.substr(12));
        else if (line.rfind("OUTCAST_COUNT:", 0) == 0)  civ->outcastCount = std::stoi(line.substr(14));
        else if (line.rfind("DARKAGE_COUNT:", 0) == 0)  civ->darkAgeCount = std::stoi(line.substr(14));
        else if (line.rfind("MAX_ERA:", 0) == 0)        civ->maxEraAchieved = (CivilizationEra)std::stoi(line.substr(8));
        else if (line.rfind("LAST_COLLAPSE_DAY:", 0) == 0) civ->lastCollapseDay = std::stoi(line.substr(18));
        else if (line.rfind("TREATY_COUNT:", 0) == 0) {
            int n = std::stoi(line.substr(13));
            civ->treaties.clear();
            for (int i = 0; i < n; ++i) {
                std::getline(file, line);
                if (line.rfind("TREATY|", 0) != 0) continue;
                std::vector<std::string> f;
                std::stringstream ss(line);
                std::string tok;
                while (std::getline(ss, tok, '|')) f.push_back(tok);
                if (f.size() >= 8) {
                    Treaty t;
                    t.type           = (TreatyType)std::stoi(f[1]);
                    t.tribeA         = std::stoi(f[2]);
                    t.tribeB         = std::stoi(f[3]);
                    t.startDay       = std::stoi(f[4]);
                    t.expiryDay      = std::stoi(f[5]);
                    t.tributeAmount  = std::stof(f[6]);
                    t.active         = (std::stoi(f[7]) != 0);
                    civ->treaties.push_back(t);
                }
            }
        }
    }
    return true;
}

// ── Emergence save section (Upgrade Plan, Step 5b) ───────────────────────────
// Appended AFTER the legacy save body under its own marker, so old saves load
// unchanged (loader seeks the marker; absence = defaults). Covers: invented
// item defs/rules, the pheromone field, and per-entity genome / NEAT brain /
// inventory / known recipes / episodic memories.
static void saveEmergence(std::ofstream& file, const std::vector<Entity>& entities) {
    file << "EMERGENCE_V1\n";
    g_itemManager.saveTo(file);
    g_pheromoneField.saveTo(file);
    file << "EMERGENT_ENTITIES:" << entities.size() << "\n";
    for (const Entity& e : entities) {
        file << "EM|\"" << e.entityId << "\"|\""
             << e.genome.speed << "\" \"" << e.genome.sightRange << "\" \""
             << e.genome.metabolism << "\" \"" << e.genome.fertility << "\" \""
             << e.genome.resilience << "\"|\"" << (e.useNeatBrain ? 1 : 0) << "\"|\"";
        for (size_t i = 0; i < e.inventory.stacks.size(); ++i)
            file << (i ? "," : "") << e.inventory.stacks[i].defId << "="
                 << e.inventory.stacks[i].qty;
        file << "\"|\"";
        for (size_t i = 0; i < e.knownRecipeIds.size(); ++i)
            file << (i ? "," : "") << e.knownRecipeIds[i];
        file << "\"|\"";
        const auto& nodes = e.episodicMap.nodes();
        for (size_t i = 0; i < nodes.size(); ++i)
            file << (i ? "," : "") << nodes[i].x << " " << nodes[i].y << " "
                 << (int)nodes[i].kind << " " << nodes[i].day << " " << nodes[i].strength;
        file << "\"\n";
        if (e.useNeatBrain) e.neatGenome.saveTo(file);
    }
}

static std::vector<std::string> splitOn(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (true) {
        size_t p = s.find(sep, pos);
        out.push_back(s.substr(pos, p == std::string::npos ? std::string::npos : p - pos));
        if (p == std::string::npos) break;
        pos = p + 1;
    }
    return out;
}

static void loadEmergence(std::ifstream& file, std::vector<Entity>& entities) {
    std::streampos at = file.tellg();
    std::string line;
    file >> std::ws;
    if (!std::getline(file, line) || line != "EMERGENCE_V1") {
        file.clear();
        file.seekg(at);           // pre-upgrade save: leave defaults in place
        return;
    }
    g_itemManager.loadFrom(file);
    g_pheromoneField.loadFrom(file);
    std::map<int, Entity*> byId;
    for (Entity& e : entities) byId[e.entityId] = &e;
    file >> std::ws;
    if (!std::getline(file, line) || line.rfind("EMERGENT_ENTITIES:", 0) != 0) return;
    int count = std::stoi(line.substr(18));
    for (int i = 0; i < count; ++i) {
        file >> std::ws;
        if (!std::getline(file, line) || line.rfind("EM|\"", 0) != 0) return;
        // Strip quotes — the V3 format wraps fields in quotes for robustness,
        // but the V2 format didn't. Handle both by just removing all quotes.
        std::string clean;
        for (char c : line) if (c != '\"') clean += c;
        std::vector<std::string> f = splitOn(clean, '|');
        if (f.size() < 7) continue;
        Entity* e = byId.count(std::stoi(f[1])) ? byId[std::stoi(f[1])] : nullptr;
        bool wantsNeat = false;
        if (e) {
            std::stringstream gs(f[2]);
            gs >> e->genome.speed >> e->genome.sightRange >> e->genome.metabolism
               >> e->genome.fertility >> e->genome.resilience;
            wantsNeat = (f[3] == "1");
            e->inventory.stacks.clear();
            if (!f[4].empty())
                for (const std::string& kv : splitOn(f[4], ',')) {
                    size_t eq = kv.find('=');
                    if (eq != std::string::npos)
                        e->inventory.add(std::stoi(kv.substr(0, eq)),
                                         (float)atof(kv.substr(eq + 1).c_str()));
                }
            e->knownRecipeIds.clear();
            if (!f[5].empty())
                for (const std::string& r : splitOn(f[5], ','))
                    e->knownRecipeIds.push_back(std::stoi(r));
            std::vector<EpisodicNode> nodes;
            if (!f[6].empty())
                for (const std::string& ns : splitOn(f[6], ',')) {
                    std::stringstream nss(ns);
                    EpisodicNode n; int kind;
                    nss >> n.x >> n.y >> kind >> n.day >> n.strength;
                    n.kind = (uint8_t)kind;
                    nodes.push_back(n);
                }
            e->episodicMap.setNodes(std::move(nodes), 0);
        } else {
            wantsNeat = (f.size() > 3 && f[3] == "1");
        }
        if (wantsNeat) {
            neat::Genome g;
            if (g.loadFrom(file) && e) {
                e->neatGenome = std::move(g);
                e->useNeatBrain = true;
            }
        }
    }
}


// ── Main save function ───────────────────────────────────────────────────────
void saveGame(const std::string& filepath, const std::vector<Entity>& entities,
              int day, int frameCounter, const CivilizationEngine* civ) {
    // Ensure the saves directory exists
    ensureSavesDir();

    std::string fullPath = savePath(filepath);
    std::ofstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open save file: " << fullPath << std::endl;
        return;
    }

    file << SAVE_FORMAT_V3 << "\n";
    file << "DAY:" << day << "\n";
    file << "FRAME:" << frameCounter << "\n";
    file << "CLOCK_FRAME:" << g_clock.frame << "\n";
    file << "RNG:" << BetterRand::gen() << "\n";   // mt19937 stream state (624 words)

    if (civ) {
        file << "ERA:" << (int)civ->era << " YEAR:" << civ->currentYear << "\n";
        file << "TRIBE_COUNT:" << civ->tribes.size() << "\n";
        for (const Tribe& t : civ->tribes) {
            // Fields 17-18 (festivity, lastFestivalDay) are the AI-upgrade
            // culture axis — appended so older loaders (which use f[1..16])
            // and older saves (presence-guarded read) both keep working.
            file << "TRIBE|" << t.id << '|' << t.name << '|' << t.leaderId << '|'
                 << t.foundedOnDay << '|' << t.militarism << '|' << t.spiritualism << '|'
                 << t.collectivism << '|' << t.innovation << '|' << t.centerX << '|'
                 << t.centerY << '|' << t.regionId << '|' << t.granary << '|'
                 << t.dominantReligionId << '|' << joinInts(t.memberIds) << '|'
                 << joinMap(t.relations) << '|' << joinMap(t.stances) << '|'
                 << t.festivity << '|' << t.lastFestivalDay << "\n";
        }
        file << "RELIGION_COUNT:" << civ->religions.size() << "\n";
        for (const Religion& r : civ->religions) {
            file << "REL|" << r.id << '|' << r.name << '|' << r.founderEntityId << '|'
                 << r.foundedOnDay << '|' << (int)r.moralCode << '|' << (int)r.ritual << '|'
                 << (r.isPolytheistic ? 1 : 0) << '|' << r.spiritualDemand << '|'
                 << r.holyPrinciple << '|' << joinInts(r.followerIds) << '|'
                 << r.parentReligionId << '|' << r.influence << "\n";
        }
    } else {
        file << "TRIBE_COUNT:0\nRELIGION_COUNT:0\n";
    }

    file << "ENTITY_COUNT:" << entities.size() << "\n";
    for (const Entity& entity : entities) {
        entity.saveTo(file);
    }

    saveEmergence(file, entities);   // Step 5b: versioned emergence section

    // ── V3 additions: all remaining simulation subsystems ─────────────────────
    saveEnvironment(file);
    saveLiveConfig(file);
    saveResources(file);
    saveEcosystem(file);
    saveMarket(file);
    saveSocialOrder(file);
    saveKinship(file);
    saveCivStats(file, civ);

    file.close();
    std::cout << "Game saved to " << fullPath << " (V3, " << entities.size()
              << " entities" << (civ ? ", full state" : ", entities only") << ")"
              << std::endl;
}

// ── Main load function ───────────────────────────────────────────────────────
bool loadGame(const std::string& filepath, std::vector<Entity>& entities,
              int& day, int& frameCounter, CivilizationEngine* civ) {
    // Try loading from the saves directory first, then fall back to raw path
    std::string fullPath = savePath(filepath);

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        // Fallback: try the raw path directly
        file.open(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open save file: " << fullPath
                      << " (also tried: " << filepath << ")" << std::endl;
            return false;
        }
    }

    // A save from an older build can disagree with this one about any field it
    // has since gained or reshaped, and every parse below is a std::sto* that
    // throws rather than returns on a value it cannot read. Unhandled, that
    // aborted the whole program on a bad file — the user asked to open a world
    // and got "terminate called after throwing an instance of
    // 'std::out_of_range'" and no simulation. Failing to load is an ordinary
    // outcome and belongs to the caller, which already knows how to start a
    // fresh world instead.
    try {
    std::string line;
    std::getline(file, line);
    const bool v3 = (line == SAVE_FORMAT_V3);
    const bool v2 = (line == "ASHB2_SAVE_V2");
    if (!v3 && !v2 && line != "ASHB2_SAVE") {
        std::cerr << "Invalid save file format (expected version marker)" << std::endl;
        return false;
    }

    std::getline(file, line);
    day = std::stoi(line.substr(4)); // "DAY:"
    std::getline(file, line);
    frameCounter = std::stoi(line.substr(6)); // "FRAME:"

    if (v2 || v3) {
        std::getline(file, line);                    // CLOCK_FRAME:
        g_clock.frame = std::stoull(line.substr(12));
        std::getline(file, line);                    // RNG:
        { std::stringstream rs(line.substr(4)); rs >> BetterRand::gen(); }

        std::getline(file, line);                    // ERA:.. YEAR:..  (or TRIBE_COUNT if no civ was saved)
        if (line.rfind("ERA:", 0) == 0) {
            if (civ) {
                std::stringstream es(line);
                std::string tag; int eraV, yearV;
                eraV  = std::stoi(line.substr(4, line.find(" YEAR:") - 4));
                yearV = std::stoi(line.substr(line.find(" YEAR:") + 6));
                civ->era = (CivilizationEra)eraV;
                civ->currentYear = yearV;
            }
            std::getline(file, line);                // TRIBE_COUNT:
        }
        int tribeCount = std::stoi(line.substr(12));
        if (civ) { civ->tribes.clear(); civ->religions.clear(); }
        for (int i = 0; i < tribeCount; ++i) {
            std::getline(file, line);
            if (!civ) continue;
            std::vector<std::string> f;
            { std::stringstream ss(line); std::string tok;
              while (std::getline(ss, tok, '|')) f.push_back(tok); }
            if (f.size() < 17 || f[0] != "TRIBE") continue;
            Tribe t;
            t.id = std::stoi(f[1]);        t.name = f[2];
            t.leaderId = std::stoi(f[3]);  t.foundedOnDay = std::stoi(f[4]);
            t.militarism = std::stof(f[5]);   t.spiritualism = std::stof(f[6]);
            t.collectivism = std::stof(f[7]); t.innovation = std::stof(f[8]);
            t.centerX = std::stof(f[9]);      t.centerY = std::stof(f[10]);
            t.regionId = std::stoi(f[11]);    t.granary = std::stof(f[12]);
            t.dominantReligionId = std::stoi(f[13]);
            t.memberIds = splitInts(f[14]);
            parseFloatMap(f[15], t.relations);
            { std::map<int, float> st; parseFloatMap(f[16], st);
              for (auto& [k, v] : st) t.stances[k] = (TribeStance)(int)v; }
            // AI-upgrade culture axis (presence-guarded: absent in older saves).
            if (f.size() > 18) {
                try { t.festivity       = std::stof(f[17]);
                      t.lastFestivalDay = std::stoi(f[18]); } catch (...) {}
            }
            civ->tribes.push_back(t);
        }
        std::getline(file, line);                    // RELIGION_COUNT:
        int relCount = std::stoi(line.substr(15));
        for (int i = 0; i < relCount; ++i) {
            std::getline(file, line);
            if (!civ) continue;
            std::vector<std::string> f;
            { std::stringstream ss(line); std::string tok;
              while (std::getline(ss, tok, '|')) f.push_back(tok); }
            if (f.size() < 13 || f[0] != "REL") continue;
            Religion r;
            r.id = std::stoi(f[1]);              r.name = f[2];
            r.founderEntityId = std::stoi(f[3]); r.foundedOnDay = std::stoi(f[4]);
            r.moralCode = (MoralCode)std::stoi(f[5]);
            r.ritual = (RitualType)std::stoi(f[6]);
            r.isPolytheistic = (f[7] == "1");
            r.spiritualDemand = std::stof(f[8]);
            r.holyPrinciple = f[9];
            r.followerIds = splitInts(f[10]);
            r.parentReligionId = std::stoi(f[11]);
            r.influence = std::stof(f[12]);
            civ->religions.push_back(r);
        }
    }

    std::getline(file, line);                        // ENTITY_COUNT:
    int entityCount = std::stoi(line.substr(13));

    // Read into a temporary first and only then take over the world. Clearing
    // `entities` up front meant a save that failed halfway — an older format,
    // a truncated file — destroyed the perfectly good founding population the
    // caller had already spawned, leaving nothing to fall back to and a run
    // with zero people in it.
    std::vector<Entity> loaded;
    loaded.reserve((size_t)std::max(0, entityCount));
    for (int i = 0; i < entityCount; i++) {
        Entity entity(0);
        if (!entity.loadFrom(file)) {
            std::cerr << "loadGame: save truncated at entity " << i << "/"
                      << entityCount << " — continuing with what was readable\n";
            break;
        }
        loaded.push_back(entity);
    }
    entities.swap(loaded);

    // Resolve pointers after all entities are loaded
    for (Entity& entity : entities) {
        entity.resolvePointers(entities);
    }

    loadEmergence(file, entities);   // Step 5b: tolerant of pre-upgrade saves

    // ── V3 additions: load remaining simulation subsystems ────────────────────
    if (v3) {
        loadEnvironment(file);
        loadLiveConfig(file);
        loadResources(file);
        loadEcosystem(file);
        loadMarket(file);
        loadSocialOrder(file);
        loadKinship(file);
        loadCivStats(file, civ);
    }

    file.close();
    std::cout << "Game loaded from " << fullPath
              << (v3 ? " (V3)" : v2 ? " (V2)" : " (legacy)")
              << ": " << entities.size() << " entities" << std::endl;
    return true;
    } catch (const std::exception& ex) {
        // Whatever the caller already had is left alone — if the save died
        // before the population section, that is the freshly spawned founders,
        // and the run can go on with them.
        std::cerr << "Could not read save '" << fullPath << "': " << ex.what()
                  << ".\n       It is most likely from an older build than this one.\n";
        return false;
    }
}


// ── Tick history JSON export ─────────────────────────────────────────────────
// Escapes a string for JSON format
std::string escapeJSONString(const std::string& input) {
    std::string output;
    for (char c : input) {
        switch (c) {
            case '\"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default: output += c; break;
        }
    }
    return output;
}

void exportTickHistory(const std::string& filepath, const std::vector<Entity>& entities, int day) {
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "Failed to open tick history file: " << filepath << std::endl;
        return;
    }

    // Build the JSON line manually to avoid external library dependencies
    file << "{";
    file << "\"day\":" << day << ",";
    file << "\"entityCount\":" << entities.size() << ",";
    file << "\"entities\":[";

    for (size_t i = 0; i < entities.size(); ++i) {
        const Entity& entity = entities[i];

        file << "{";
        file << "\"id\":" << entity.entityId << ",";
        file << "\"name\":\"" << escapeJSONString(entity.name) << "\",";
        file << "\"age\":" << entity.entityAge << ",";
        file << "\"health\":" << entity.entityHealth << ",";
        file << "\"happiness\":" << entity.entityHapiness << ",";
        file << "\"stress\":" << entity.entityStress << ",";
        file << "\"mentalHealth\":" << entity.entityMentalHealth << ",";
        file << "\"loneliness\":" << entity.entityLoneliness << ",";
        file << "\"boredom\":" << entity.entityBoredom << ",";
        file << "\"anger\":" << entity.entityGeneralAnger << ",";
        file << "\"hygiene\":" << entity.entityHygiene << ",";
        file << "\"wealth\":" << entity.salary.token << ",";
        file << "\"sex\":\"" << std::string(1, entity.entitySex) << "\",";
        file << "\"disease\":" << entity.entityDiseaseType << ",";
        file << "\"posX\":" << entity.posX << ",";
        file << "\"posY\":" << entity.posY << ",";

        file << "\"personality\":{";
        file << "\"e\":" << entity.personality.extraversion << ",";
        file << "\"a\":" << entity.personality.agreeableness << ",";
        file << "\"c\":" << entity.personality.conscientiousness << ",";
        file << "\"n\":" << entity.personality.neuroticism << ",";
        file << "\"o\":" << entity.personality.openness;
        file << "},";

        file << "\"valueSystem\":{";
        file << "\"family\":" << entity.ValueSystem.familyOrientation << ",";
        file << "\"achieve\":" << entity.ValueSystem.achievementDrive << ",";
        file << "\"spirit\":" << entity.ValueSystem.spiritualNeed << ",";
        file << "\"hedonism\":" << entity.ValueSystem.hedonism << ",";
        file << "\"collectivism\":" << entity.ValueSystem.collectivism;
        file << "},";

        file << "\"attachment\":" << entity.dv.attachmentStyle << ",";
        file << "\"selfEsteem\":" << entity.SelfConcept.selfEsteem << ",";
        file << "\"griefIntensity\":" << entity.getGriefIntensity() << ",";
        file << "\"currentGoal\":\"" << escapeJSONString(entity.m_goals.empty() ? "none" : entity.m_goals.front().type) << "\"";

        file << "}";
        if (i < entities.size() - 1) {
            file << ",";
        }
    }

    file << "]"
         << "}\n";
    file.close();
}
