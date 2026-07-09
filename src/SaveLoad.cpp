#include "./header/SaveLoad.h"
#include "./header/Entity.h"
#include "./header/FreeWillSystem.h"
#include "./header/CivilizationEngine.h"
#include "./header/BetterRand.h"
#include "core/SimClock.h"
#include "items/ItemSystem.h"      // Step 5b: emergence save section
#include "world/PheromoneField.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

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
        file << "EM|" << e.entityId << '|'
             << e.genome.speed << ' ' << e.genome.sightRange << ' '
             << e.genome.metabolism << ' ' << e.genome.fertility << ' '
             << e.genome.resilience << '|' << (e.useNeatBrain ? 1 : 0) << '|';
        for (size_t i = 0; i < e.inventory.stacks.size(); ++i)
            file << (i ? "," : "") << e.inventory.stacks[i].defId << '='
                 << e.inventory.stacks[i].qty;
        file << '|';
        for (size_t i = 0; i < e.knownRecipeIds.size(); ++i)
            file << (i ? "," : "") << e.knownRecipeIds[i];
        file << '|';
        const auto& nodes = e.episodicMap.nodes();
        for (size_t i = 0; i < nodes.size(); ++i)
            file << (i ? "," : "") << nodes[i].x << ' ' << nodes[i].y << ' '
                 << (int)nodes[i].kind << ' ' << nodes[i].day << ' ' << nodes[i].strength;
        file << "\n";
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
        if (!std::getline(file, line) || line.rfind("EM|", 0) != 0) return;
        std::vector<std::string> f = splitOn(line, '|');
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

void saveGame(const std::string& filepath, const std::vector<Entity>& entities,
              int day, int frameCounter, const CivilizationEngine* civ) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open save file: " << filepath << std::endl;
        return;
    }

    file << "ASHB2_SAVE_V2\n";
    file << "DAY:" << day << "\n";
    file << "FRAME:" << frameCounter << "\n";
    file << "CLOCK_FRAME:" << g_clock.frame << "\n";
    file << "RNG:" << BetterRand::gen() << "\n";   // mt19937 stream state (624 words)

    if (civ) {
        file << "ERA:" << (int)civ->era << " YEAR:" << civ->currentYear << "\n";
        file << "TRIBE_COUNT:" << civ->tribes.size() << "\n";
        for (const Tribe& t : civ->tribes) {
            file << "TRIBE|" << t.id << '|' << t.name << '|' << t.leaderId << '|'
                 << t.foundedOnDay << '|' << t.militarism << '|' << t.spiritualism << '|'
                 << t.collectivism << '|' << t.innovation << '|' << t.centerX << '|'
                 << t.centerY << '|' << t.regionId << '|' << t.granary << '|'
                 << t.dominantReligionId << '|' << joinInts(t.memberIds) << '|'
                 << joinMap(t.relations) << '|' << joinMap(t.stances) << "\n";
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

    file.close();
    std::cout << "Game saved to " << filepath << " (V2, " << entities.size()
              << " entities" << (civ ? ", macro state included" : "") << ")" << std::endl;
}

bool loadGame(const std::string& filepath, std::vector<Entity>& entities,
              int& day, int& frameCounter, CivilizationEngine* civ) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open save file: " << filepath << std::endl;
        return false;
    }

    std::string line;
    std::getline(file, line);
    const bool v2 = (line == "ASHB2_SAVE_V2");
    if (!v2 && line != "ASHB2_SAVE") {
        std::cerr << "Invalid save file format" << std::endl;
        return false;
    }

    std::getline(file, line);
    day = std::stoi(line.substr(4)); // "DAY:"
    std::getline(file, line);
    frameCounter = std::stoi(line.substr(6)); // "FRAME:"

    if (v2) {
        std::getline(file, line);                    // CLOCK_FRAME:
        g_clock.frame = std::stoull(line.substr(12));
        std::getline(file, line);                    // RNG:
        { std::stringstream rs(line.substr(4)); rs >> BetterRand::gen(); }

        std::getline(file, line);                    // ERA:.. YEAR:..  (or TRIBE_COUNT if no civ was saved)
        if (line.rfind("ERA:", 0) == 0) {
            if (civ) {
                std::stringstream es(line);
                std::string tag; int eraV, yearV;
                es >> tag; // "ERA:<n>" — reparse manually
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

    entities.clear();
    for (int i = 0; i < entityCount; i++) {
        Entity entity(0);
        entity.loadFrom(file);
        entities.push_back(entity);
    }

    // Resolve pointers after all entities are loaded
    for (Entity& entity : entities) {
        entity.resolvePointers(entities);
    }

    loadEmergence(file, entities);   // Step 5b: tolerant of pre-upgrade saves

    file.close();
    std::cout << "Game loaded from " << filepath << (v2 ? " (V2)" : " (legacy)")
              << ": " << entities.size() << " entities" << std::endl;
    return true;
}

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
        // To access the const name we have to bypass the non-const getName() if one doesn't exist, but Name is public
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

        // Ensure character string uses double quotes as string
        file << "\"sex\":\"" << std::string(1, entity.entitySex) << "\",";

        file << "\"disease\":" << entity.entityDiseaseType << ",";
        file << "\"posX\":" << entity.posX << ",";
        file << "\"posY\":" << entity.posY << ",";

        // Additional info like personality can be easily extended
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

    file << "]" << "}\n";
    file.close();
}

