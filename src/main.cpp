// HEADLESS: a build with no window at all — no GLFW, no OpenGL, no Dear ImGui,
// nothing that needs a display or a GPU driver present. That is what a Linux
// server, a CI runner and a GitHub Codespace actually are, and the simulation
// itself has never needed any of it: --headless runs the whole world and prints
// the realism report. Building the render path there meant installing a GUI
// toolchain to run a text-mode simulation, so it is compiled out instead.
#ifndef HEADLESS
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "./header/UI.h"
#endif
#include "./header/Entity.h"
#include <vector>
#include <algorithm>
#include <time.h>
#include <random>
#include "./header/FreeWillSystem.h"
#include "./header/BetterRand.h"
#include <iostream>
#include <sstream>
#include <thread>
#include "./header/Disease.h"
#include "util/Debbug.h"
#ifndef HEADLESS
#include "./header/implot.h"
#include "./header/implot_internal.h"
#endif
#include "./util/clear.h"
#include "./header/SaveLoad.h"
#include "./header/Logging.h"
#include "./header/NarrativeEngine.h"
#include "./header/CivilizationEngine.h"
#include "./header/Kinship.h"
#include "./header/WorldSeed.h"
#include "world/Planet.h"
#ifndef HEADLESS
#include "world/PlanetView.h"
#endif
#include "world/Lexicon.h"
#include <unordered_map>
#include <array>
#include <unordered_set>
#include <cstdlib>
#include <cstdint>
#include "core/SimClock.h"
#include "core/SpatialGrid.h"
#include "items/ItemSystem.h"      // Step 2: data-driven items/rules/invention
#include "world/PheromoneField.h"  // Step 4: environmental stigmergy
#include "./header/LiveConfig.h"
#include "environment/EnvironmentModel.h"
#include "world/ResourceSystem.h"
#include "world/Ecosystem.h"

// Note: SaveLoad.h is already included above (line ~26).

// M10: live config console state — all multipliers default to 1.0 (bit-exact
// no-op); the GUI console mutates them at runtime.
LiveConfig g_liveConfig;

// Name → knob, so a headless run can turn any multiplier from the command line
// (`--set cityMul=0`). The A/B and kill-switch contract in
// plans/parallel-earth-upgrade.md §2 is only meaningful if it can be exercised
// without the GUI console.
bool setLiveConfigKnob(const std::string& n, float v) {
    static const std::pair<const char*, float LiveConfig::*> kKnobs[] = {
        { "moveForceMul",     &LiveConfig::moveForceMul },
        { "mortalityMul",     &LiveConfig::mortalityMul },
        { "foodYieldMul",     &LiveConfig::foodYieldMul },
        { "aggressionMul",    &LiveConfig::aggressionMul },
        { "corruptionMul",    &LiveConfig::corruptionMul },
        { "mutationRateMul",  &LiveConfig::mutationRateMul },
        { "pheromoneDecayMul",&LiveConfig::pheromoneDecayMul },
        { "inventionRateMul", &LiveConfig::inventionRateMul },
        { "neatBrainShare",   &LiveConfig::neatBrainShare },
        { "emotionMul",       &LiveConfig::emotionMul },
        { "cultureMul",       &LiveConfig::cultureMul },
        { "identityMul",      &LiveConfig::identityMul },
        { "relationshipMul",  &LiveConfig::relationshipMul },
        { "moodMul",          &LiveConfig::moodMul },
        { "knowledgeMul",     &LiveConfig::knowledgeMul },
        { "feudMul",          &LiveConfig::feudMul },
        { "cityMul",          &LiveConfig::cityMul },
        { "institutionMul",   &LiveConfig::institutionMul },
        { "legacyMul",        &LiveConfig::legacyMul },
        { "tradeMul",         &LiveConfig::tradeMul },
        { "warMul",           &LiveConfig::warMul },
        { "cycleMul",         &LiveConfig::cycleMul },
        { "classMul",         &LiveConfig::classMul },
        { "doctrineMul",      &LiveConfig::doctrineMul },
        { "languageMul",      &LiveConfig::languageMul },
        { "giftMul",          &LiveConfig::giftMul },
        { "traitMul",         &LiveConfig::traitMul },
        { "laborMul",         &LiveConfig::laborMul },
        { "demographyMul",    &LiveConfig::demographyMul },
        { "networkMul",       &LiveConfig::networkMul },
        { "epigeneticsMul",   &LiveConfig::epigeneticsMul },
        { "pathogenMul",      &LiveConfig::pathogenMul },
        { "bioHomeostasisMul",&LiveConfig::bioHomeostasisMul },
    };
    for (const auto& k : kKnobs)
        if (n == k.first) { g_liveConfig.*(k.second) = v; return true; }
    return false;
}

using GroupEntity = std::vector<std::vector<Entity*>>;

// ── Global climate / season state ────────────────────────────────────────────
// Wires the previously-dormant EnvironmentModel into the live simulation. Seasons
// and harvest luck now modulate how much food foraging/farming yields, closing
// the loop: climate → food production → hunger → population.
static environment::EnvironmentalState g_env;
float       g_seasonalFoodModifier = 1.0f;  // read by food production in executeAction
float       g_seasonTemperature    = 60.0f; // 0..100 — cold winters cost more calories
float       g_harvestLuck          = 1.0f;  // good year vs bad year (drought / blight / bumper)
std::string g_seasonName           = "Spring";

// Advance the environment once per new simulation day and recompute modifiers.
// `day` is the SimClock day index (one per tick), not a frame count.
void updateEnvironment(int day) {
    static int lastDay = -1;
    static int lastYear = -999999;
    if (day == lastDay) return;
    lastDay = day;

    g_env.advanceTime(24.0f);                       // one in-sim day
    g_seasonName        = g_env.currentSeason.toString();
    g_seasonTemperature = g_env.currentSeason.temperature;

    // "Good year / bad year": roll harvest luck each new year. Droughts and
    // blights (bad years) slash yields and create famine pressure; bumper years
    // swell the granaries. This is the 丰收年 / 歉收年 cycle.
    // The year is the SAME year agents age by (SimClock::DAYS_PER_YEAR days);
    // it used to be a 365-frame "year" that never matched the aging cadence.
    int year = day / SimClock::DAYS_PER_YEAR;
    if (year != lastYear) {
        lastYear = year;
        int roll = BetterRand::genNrInInterval(0, 100);
        if (roll < 12)      g_harvestLuck = 0.45f;  // drought / blight — famine
        else if (roll < 22) g_harvestLuck = 0.70f;  // poor year
        else if (roll > 90) g_harvestLuck = 1.45f;  // bumper harvest
        else                g_harvestLuck = 1.00f;  // ordinary year
        if (globalLogger) {
            std::string note = (g_harvestLuck < 0.7f) ? "A year of famine begins - the crops fail."
                              : (g_harvestLuck > 1.3f) ? "A bountiful year - granaries overflow."
                                                       : "An ordinary year for the harvest.";
            globalLogger->logEvent("environment", note);
        }
    }

    g_seasonalFoodModifier = g_env.currentSeason.resourceModifier * g_harvestLuck
                             * g_liveConfig.foodYieldMul;   // M10 live console

    // Regrow / degrade per-region resource stocks for the new day. Living
    // resources track the same season + harvest luck that drive food yields.
    g_resources.update(g_seasonalFoodModifier);

    // Advance the food chain on the same season: plants grow/are grazed,
    // herbivores breed/starve/are preyed upon, predators track the game.
    g_ecosystem.update(g_seasonalFoodModifier, day);
}

/*
int main(){
    Entity entity = Entity(1, 0, 100, 50, 0, 100, "", 0, 0, 0, 100, 'A', 0, nullptr, nullptr, nullptr);
    FreeWillSystem sys;
    while(true){
        sys.updateNeeds(1.0f);
        Action* chosen =sys.chooseAction(&entity);
        sys.executeAction(&entity, chosen);
    }
    //sys.addAction()
}

*/

void applyDisease(Entity* ent, int neighSize, int sickClose, int leprosyCLose){
    //si est seul la proba de tombé malade est null
    Disease d;
    if(ent->entityDiseaseType != -1){
        //already sick we manage it
        d.manageSickness(ent);

    }
    if(neighSize >= 3){
        d.reduceAntiBody(ent);
        int disease = d.calculateDisease(neighSize, ent, sickClose, leprosyCLose);
        if(disease != -1){
            std::stringstream ss;
            ss << "Entity contaminated: " << ent->getId() << " " << ent->getName()
                      << " => " << d.getDiseaseName(disease);
            globalLogger->logCmd(ss.str());
            globalLogger->logDisease(ent->getId(), ent->getName(), d.getDiseaseName(disease));
            ent->entityDiseaseType = disease;
        }
    }
}

std::array<int, 2> getNBSickClose(std::vector<Entity*> grp){
  std::array<int, 2> disease_close;
  for(Entity* ent : grp){
        if(ent->entityDiseaseType == 6){
          disease_close[0] ++;
        }else if(ent->entityDiseaseType != -1){
            disease_close[1] ++;
        }
    }
    return disease_close;
}

// Generate simple pseudo-random EnvironmentalFactors that shift slowly over time
EnvironmentalFactors generateEnvFactors(int day) {
    // Use a slow sine-based drift so conditions change gradually
    float weather  = 50.0f + 40.0f * std::sin(day * 0.003f);
    float noise    = 20.0f + 30.0f * std::abs(std::sin(day * 0.007f + 1.0f));
    float safety   = 60.0f + 30.0f * std::cos(day * 0.005f + 2.0f);
    float crowd    = 20.0f + 40.0f * std::abs(std::sin(day * 0.004f + 0.5f));

    // Clamp to [0, 100]
    auto clamp = [](float v){ return std::max(0.0f, std::min(100.0f, v)); };
    return EnvironmentalFactors(clamp(weather), clamp(crowd), clamp(noise), clamp(safety));
}

// Generate ActionContext based on simulation time
ActionContext createContextFromTime(int day, int numPeopleNearby) {
    int hour = g_clock.hourOfDay();
    int dayOfWeek = g_clock.dayOfWeek();

    bool isNightTime = (hour >= 22 || hour < 6);
    bool isWeekend = (dayOfWeek >= 5);
    bool isAtWork = (!isWeekend && hour >= 9 && hour < 17);
    bool isInPublic = (numPeopleNearby > 2);

    EnvironmentalFactors env = generateEnvFactors(day);
    return ActionContext(isNightTime, isWeekend, isAtWork, isInPublic, numPeopleNearby, env);
}

// Generate random personality using Big Five distribution
Personality generateRandomPersonality() {
    std::mt19937 gen(static_cast<std::mt19937::result_type>(nextDeterministicSeed(0xBAD5'EEDull)));
    std::normal_distribution<float> dist(50.0f, 20.0f);

    auto clamp = [](float val) { return std::max(0.0f, std::min(100.0f, val)); };

    return Personality(
        clamp(dist(gen)),  // extraversion
        clamp(dist(gen)),  // agreeableness
        clamp(dist(gen)),  // conscientiousness
        clamp(dist(gen)),  // neuroticism
        clamp(dist(gen))   // openness
    );
}


    void handleDeath(Entity* dead, std::vector<Entity*>& allEntities) {
        for (Entity* ent : allEntities) {
            if (ent == dead || ent->entityHealth <= 0) continue;

            bool isPartner  = ent->checkCouple(dead);
            bool isParent   = (ent == dead->parent1 || ent == dead->parent2);
            bool isChild    = (dead == ent->parent1  || dead == ent->parent2);
            bool hasSocial  = ent->searchConnSocial(dead) > 10.0f;
            bool hasDesire  = ent->searchConnDesire(dead) > 15.0f;

            float griefIntensity = 0.0f;
            std::string narrative;

            if (isPartner) {
                griefIntensity = 0.85f + BetterRand::genNrInInterval(0,15)/100.0f;
                narrative = "lost life partner";
                ent->ValueSystem.familyOrientation += 10.0f; // réalisation tardive
                ent->onMajorEventAddOrBoostGoal("loss_death");
            } else if (isChild) {
                griefIntensity = 1.0f; // perte d'un enfant = grief maximal
                narrative = "lost a child";
                ent->personality.neuroticism += 8.0f;
                ent->onMajorEventAddOrBoostGoal("loss_death");
            } else if (isParent) {
                griefIntensity = 0.7f;
                narrative = "lost a parent";
                ent->ValueSystem.spiritualNeed += 5.0f;
                ent->onMajorEventAddOrBoostGoal("loss_death");
            } else if (hasDesire) {
                griefIntensity = 0.5f;
                narrative = "lost someone desired";
                ent->onMajorEventAddOrBoostGoal("loss_death");
            } else if (hasSocial) {
                griefIntensity = 0.3f;
                narrative = "lost a social connection";
                ent->onMajorEventAddOrBoostGoal("loss_death");
            }

            if (griefIntensity > 0.0f) {
                ent->addGrief(dead->entityId, griefIntensity, true);

                // Mémoire formative
                LifeMemory mem;
                mem.eventType = "loss_death";
                mem.entityInvolvedId = dead->entityId;
                mem.emotionalIntensity = griefIntensity;
                mem.isFormative = (griefIntensity > 0.6f);
                mem.internalNarrative = narrative;
                ent->lifeMemories.push_back(mem);
                // Consolidate AFTER the memory exists (was called before push,
                // consolidating stale state once per mourner).
                ent->rebuildSemanticMemory();
            }
        }
    }


// Human-readable life-stage label for the death ledger.
static const char* lifeStageName(LifeStage s) {
    switch (s) {
        case INFANT:     return "infant";
        case CHILD:      return "child";
        case ADOLESCENT: return "adolescent";
        case ADULT:      return "adult";
        case ELDER:      return "elder";
        default:         return "unknown";
    }
}

// ── Cause-of-death attribution ───────────────────────────────────────────────
// "hardship" used to be the dumping ground for everything that wasn't disease,
// old age, or a killing — which meant the ledger could not tell starvation from
// despair from exposure. Here we name the *specific* terminal stressor by asking
// which lethal system was deepest into its kill-zone at the moment of death.
//
// Killings are attributed upstream (pendingDeathCause) and take precedence; this
// function only resolves "natural"/environmental deaths.
static std::string determineDeathCause(const Entity& e) {
    // An external agent already named the cause (murder / crime of passion).
    if (!e.pendingDeathCause.empty()) return e.pendingDeathCause;

    // Disease is unambiguous and self-naming.
    if (e.entityDiseaseType == -2) return "illness (cancer)";
    if (e.entityDiseaseType  >  0)
        return std::string("disease (") + Disease::getDiseaseName(e.entityDiseaseType) + ")";

    // Rank every active terminal stressor by how far past its lethal threshold
    // the entity is; the deepest one is what the chronicle blames.
    struct Cand { const char* cause; float severity; };
    std::vector<Cand> c;

    if (e.entityHunger      > 75.0f) c.push_back({ "starvation",            (e.entityHunger - 75.0f) / 25.0f });
    if (e.fatigueLevel      > 80.0f) c.push_back({ "exhaustion",            (e.fatigueLevel - 80.0f) / 20.0f });
    if (e.entityMentalHealth < 25.0f) c.push_back({ "despair",             (25.0f - e.entityMentalHealth) / 25.0f });
    if (e.entityHygiene     < 20.0f) c.push_back({ "sickness from squalor", (20.0f - e.entityHygiene) / 20.0f });
    if (e.entityStress      > 85.0f) c.push_back({ "chronic stress",        (e.entityStress - 85.0f) / 15.0f });
    if (e.entityLoneliness  > 85.0f) c.push_back({ "isolation",             (e.entityLoneliness - 85.0f) / 15.0f });
    if (g_seasonTemperature < 28.0f) c.push_back({ "exposure to the cold",  (28.0f - g_seasonTemperature) / 28.0f });

    // Old age competes only for those who actually reached it, scaled by how far
    // past their era's life expectancy they lived.
    int   year = globalCivEngine ? globalCivEngine->getCurrentYear() : 0;
    float lifeExp = getLifeExpectancy(year);
    if (e.entityAge >= lifeExp * 0.85f)
        c.push_back({ "old age", 0.5f + (e.entityAge - lifeExp * 0.85f) / lifeExp });

    if (c.empty()) return "hardship";
    auto best = std::max_element(c.begin(), c.end(),
        [](const Cand& a, const Cand& b) { return a.severity < b.severity; });
    return best->cause;
}

// ── M9: run ledgers for the end-of-run realism report ─────────────────────────
// Filled as the sim runs (death causes bucketed, actions tallied); consumed by
// printRealismReport() when a headless run finishes.
static std::map<std::string, int> g_deathLedger;
static std::map<std::string, int> g_actionTally;
// IV-P2: the same tally, split by the faith of whoever acted. This is what
// makes "believers of different faiths behave differently" a measurement rather
// than an assertion — if doctrine has teeth, two congregations' action mixes
// must diverge. Keyed religionId -> action -> count (-1 = the unbelieving).
static std::map<int, std::map<std::string, long>> g_faithActionTally;
// §8: demography and affect, recorded at the one moment both are final — the
// death pass. A world's realism shows in the SHAPE of these, not in their
// averages: human lifespans are right-skewed with a hump of infant deaths at
// the very bottom, and the stress people die carrying should be spread across
// the range rather than piled at 100 (the flagship run's chronic-stress
// artifact). Plain vectors: nothing reads them but the report.
static std::vector<float> g_deathAges;
static std::vector<float> g_deathStress;

// M11: true when running --headless. Presentation-only work (inner monologue,
// narrative sentences) is skipped — nobody is watching, and at 1k+ agents the
// per-entity string assembly is measurable tick time.
static bool g_headlessMode = false;

// Compact key=value snapshot of the deceased's terminal state, appended to the
// death line so the post-mortem can correlate cause with the life that ended.
static std::string deathContext(const Entity& e) {
    int livingKids = 0;
    for (int cid : e.childrenIds) (void)cid, ++livingKids; // count of recorded offspring
    bool partnered = !e.list_entityPointedCouple.empty();
    auto i = [](float v){ return std::to_string((int)(v + 0.5f)); };
    std::stringstream ss;
    ss << "stage=" << lifeStageName(e.entityLifeStage)
       << " kids=" << livingKids
       << " partnered=" << (partnered ? 1 : 0)
       << " happy=" << i(e.entityHapiness)
       << " stress=" << i(e.entityStress)
       << " hunger=" << i(e.entityHunger)
       << " mental=" << i(e.entityMentalHealth)
       << " hygiene=" << i(e.entityHygiene)
       << " fatigue=" << i(e.fatigueLevel);
    // Structured killer attribution for homicides (Plan 4.3, fixes DQ-3). Lets a
    // post-mortem join victims to killers, map inter-tribe violence, and tell
    // same-tribe from cross-tribe killings without parsing the prose cause.
    if (e.pendingKillerId >= 0) {
        ss << " killer_id=" << e.pendingKillerId
           << " killer_tribe=" << e.pendingKillerTribeId
           << " victim_tribe=" << e.tribeId
           << " same_tribe=" << ((e.pendingKillerTribeId == e.tribeId
                                  && e.tribeId >= 0) ? 1 : 0)
           << " motivation=" << (e.pendingKillMotive.empty() ? "unknown" : e.pendingKillMotive)
           << " region=" << e.originRegionId;
    }
    return ss.str();
}


// Social-graph grouping: hybrid proximity + social bonds.
// Each entity groups with bonded contacts AND anyone within 120px (chance encounters).
std::vector<std::vector<Entity*>> getSocialGroups(std::vector<Entity*>& entities) {
    if (entities.empty()) return {};

    std::vector<bool> inGroup(entities.size(), false);
    std::vector<std::vector<Entity*>> groups;

    // M4: index lookups replace the two O(n²) inner scans. The bond pass walks
    // the entity's own (Dunbar-capped) social list; the proximity pass asks the
    // spatial grid. slotOf maps a pointer back to its inGroup flag.
    std::unordered_map<Entity*, size_t> slotOf;
    slotOf.reserve(entities.size());
    for (size_t i = 0; i < entities.size(); ++i) slotOf[entities[i]] = i;
    static SpatialGrid grid;
    grid.reset(2000.0f, 2000.0f, 120.0f);
    grid.rebuild(entities);

    for (size_t i = 0; i < entities.size(); ++i) {
        if (inGroup[i] || entities[i]->entityHealth <= 0.0f) continue;

        std::vector<Entity*> group;
        group.push_back(entities[i]);
        inGroup[i] = true;

        // Add people this entity has a social bond with
        for (const auto& s : entities[i]->list_entityPointedSocial) {
            if (group.size() >= 10) break;
            Entity* other = s.pointedEntity;
            if (!other || s.social <= 5.0f || other->entityHealth <= 0.0f) continue;
            auto it = slotOf.find(other);
            if (it == slotOf.end() || inGroup[it->second]) continue;
            group.push_back(other);
            inGroup[it->second] = true;
        }

        // Add entities within 120px (proximity encounters — no bond needed)
        grid.forEachInRadius(entities[i]->posX, entities[i]->posY, 120.0f, [&](Entity* other) {
            if (other == entities[i] || other->entityHealth <= 0.0f) return;
            if (group.size() >= 12) return;
            auto it = slotOf.find(other);
            if (it == slotOf.end() || inGroup[it->second]) return;
            group.push_back(other);
            inGroup[it->second] = true;
        });

        // Add 1-2 random strangers (chance encounters)
        for (int s = 0; s < 2 && group.size() < 12; ++s) {
            int attempts = 0;
            while (attempts++ < 30) {
                int ri = BetterRand::genNrInInterval(0, (int)entities.size() - 1);
                if (!inGroup[ri] && entities[ri]->entityHealth > 0.0f) {
                    group.push_back(entities[ri]);
                    inGroup[ri] = true;
                    break;
                }
            }
        }

        groups.push_back(group);
    }
    return groups;
}

// Generates a psychologically specific first-person inner thought for this entity.
// Weighted by emotional salience so the most pressing state surfaces.
std::string generateDeepMonologue(Entity* ent) {
    struct Thought { std::string text; float weight; };
    std::vector<Thought> pool;

    // Grief (highest salience)
    for (const auto& g : ent->griefStates)
        if (g.intensity > 0.25f)
            pool.push_back({"The absence still haunts me. I keep expecting them to just... show up.", g.intensity * 3.0f});

    // Romantic bond
    if (!ent->list_entityPointedCouple.empty()) {
        Entity* partner = ent->list_entityPointedCouple[0].pointedEntity;
        if (partner) {
            float bond = ent->searchConnSocial(partner);
            if (bond > 50.0f)
                pool.push_back({"Thinking about " + partner->getName() + " makes everything feel worth it.", bond * 0.02f});
            else
                pool.push_back({"Things with " + partner->getName() + " feel off. I don't know how to fix it.", 1.8f});
        }
    } else if (ent->isGoalType("find_partner") && ent->entityLoneliness > 50.0f) {
        pool.push_back({"I wonder if I'll ever find someone who actually sees me.", 1.5f});
    }

    // Desire
    {
        float maxD = 0.0f; Entity* desired = nullptr;
        for (auto& d : ent->list_entityPointedDesire)
            if (d.desire > maxD) { maxD = d.desire; desired = d.pointedEntity; }
        if (desired && maxD > 35.0f)
            pool.push_back({"My mind keeps drifting to " + desired->getName() + ". I can't help it.", maxD * 0.025f});
    }

    // Anger at specific person
    if (ent->entityGeneralAnger > 55.0f) {
        Entity* target = ent->mostAngryConn();
        if (target)
            pool.push_back({"What " + target->getName() + " did wasn't right. I can't just let that go.", ent->entityGeneralAnger * 0.03f});
        else
            pool.push_back({"Everything is setting me off today. I need to step back before I say something I regret.", ent->entityGeneralAnger * 0.025f});
    }

    // Stress / overwhelm
    if (ent->entityStress > 75.0f)
        pool.push_back({"I feel stretched too thin. Like something inside is about to snap.", ent->entityStress * 0.025f});

    // Mental health collapse
    if (ent->entityMentalHealth < 30.0f)
        pool.push_back({"I go through the motions but nothing feels real anymore. I'm just... running out.", (100.0f - ent->entityMentalHealth) * 0.025f});

    // Loneliness with named target
    if (ent->entityLoneliness > 70.0f) {
        Entity* closest = ent->mostSocialConn();
        if (closest)
            pool.push_back({"I miss " + closest->getName() + ". I should reach out but I don't know how to start.", ent->entityLoneliness * 0.03f});
        else
            pool.push_back({"Nobody really knows me. I'm starting to think that's entirely my fault.", ent->entityLoneliness * 0.03f});
    }

    // Goal frustration
    for (const auto& g : ent->m_goals) {
        if (g.frustrationLevel > 60.0f) {
            if (g.type == "build_career")
                pool.push_back({"I put everything into this and it never seems to be enough. When does it change?", g.frustrationLevel * 0.02f});
            else if (g.type == "make_friends")
                pool.push_back({"I keep putting myself out there but real connection stays just out of reach.", g.frustrationLevel * 0.02f});
            else if (g.type == "happiness")
                pool.push_back({"I keep chasing something I can't name. Would I even recognize happiness if it arrived?", g.frustrationLevel * 0.02f});
            else if (g.type == "self")
                pool.push_back({"I don't know who I am anymore. The person I wanted to become feels like a stranger.", g.frustrationLevel * 0.02f});
            else if (g.type == "build_family")
                pool.push_back({"I thought I'd have this figured out by now. The gap between where I am and where I wanted to be is hard to look at.", g.frustrationLevel * 0.02f});
        }
    }

    // Attachment style inner voice
    switch (ent->dv.attachmentStyle) {
        case ANXIOUS:
            if (!ent->list_entityPointedSocial.empty())
                pool.push_back({"What if they're only around out of obligation? I need to stop second-guessing — but I can't.", 1.3f});
            break;
        case AVOIDANT:
            if (ent->socialDeficit > 20.0f)
                pool.push_back({"I tell myself I need people less than others do. Most days I believe it.", 1.2f});
            break;
        case DISORGANIZED:
            pool.push_back({"I want closeness and it terrifies me — both at once. I don't know how to be around people without losing myself.", 1.6f});
            break;
        default: break;
    }

    // Contentment
    if (ent->entityHapiness > 70.0f && ent->entityStress < 35.0f)
        pool.push_back({"Things genuinely feel okay right now. I'm trying to hold onto that feeling instead of waiting for it to break.", ent->entityHapiness * 0.015f});

    // Boredom
    if (ent->entityBoredom > 65.0f)
        pool.push_back({"This routine is slowly eating me alive. Something has to change.", ent->entityBoredom * 0.02f});

    // Trauma echo
    if (ent->dv.childhoodTraumaScore > 50.0f && ent->entityMentalHealth < 60.0f)
        pool.push_back({"Sometimes I react to things and then wonder where that even came from. The past is never really past.", 1.4f});

    if (pool.empty())
        pool.push_back({"Just getting through the day, one moment at a time.", 1.0f});

    // Weighted random selection (deterministic stream, not CRT rand())
    float total = 0.0f;
    for (auto& t : pool) total += t.weight;
    float roll = BetterRand::genNrInInterval(0.0f, total);
    float cum = 0.0f;
    for (auto& t : pool) {
        cum += t.weight;
        if (roll <= cum) return t.text;
    }
    return pool.back().text;
}

    Entity* weightedRandomSelect(std::vector<std::pair<Entity*, float>> scores);

    Entity* selectSocialTarget(Entity* self, const std::vector<Entity*>& neighbors,
                           Action* action) {
    std::vector<std::pair<Entity*, float>> scores;

    for (Entity* neighbor : neighbors) {
        float score = 0.0f;
        MentalModelOfOther* model = self->getModelOf(neighbor);
        // M4: a mental model only counts for as much as it's fresh — an
        // impression formed 100 days ago barely moves the needle.
        float conf = model ? model->effectiveConfidence((int)g_clock.day()) : 0.0f;

        if (action->name == "Socialize" || action->name == "GoodConnection") {
            score += self->searchConnSocial(neighbor) * 2.0f;
            score += (model ? model->trustLevel : 0) * 1.5f * conf;
            score -= self->searchConnAng(neighbor) * 3.0f;
        } else if (action->name == "Desire" || action->name == "Flirt") {
            score += self->searchConnDesire(neighbor) * 3.0f;
            score -= self->searchConnAng(neighbor) * 2.0f;
            // Attachment style affects target preference
            if (self->dv.attachmentStyle == ANXIOUS)
                score += (model ? (1.0f - model->predictability) : 0) * 2.0f * conf; // anxious drawn to unpredictable
        } else if (action->name == "AngerConnection" || action->name == "Murder") {
            score += self->searchConnAng(neighbor) * 3.0f;     // target enemies
        } else if (action->name == "HelpSupport") {
            // Blend the model's estimate with the neutral prior by confidence.
            float estHappy = model ? (model->estimatedHappiness * conf + 50.0f * (1.0f - conf)) : 50.0f;
            score += (100.0f - estHappy) * 0.02f; // help those suffering
            score += self->searchConnSocial(neighbor) * 1.0f;
        } else if (action->name == "Apologize") {
            score += self->searchConnAng(neighbor) * 2.0f;  // apologize to those angry at us
        }

        // Familiarity bonus — people you know are easier to reach
        float familiarity = self->searchConnSocial(neighbor) / 100.0f;
        score *= (0.4f + familiarity * 0.6f);

        // Proximity bonus — closer entities are more likely targets
        float dx = self->posX - neighbor->posX;
        float dy = self->posY - neighbor->posY;
        float dist2 = dx * dx + dy * dy;
        float proximityBonus = std::max(0.0f, (180.0f - std::sqrt(dist2)) / 180.0f) * 2.5f;
        score += proximityBonus;

        // M5 ostracism: a known bad reputation repels friendly approaches
        // (this is what closes the sanction loop — the shunned stay shunned)
        // while making the person a MORE likely target of hostility.
        auto repIt = self->reputationMap.find(neighbor->entityId);
        if (repIt != self->reputationMap.end() && repIt->second.negativeScore > 70.0f) {
            bool hostile = (action->name == "AngerConnection" || action->name == "Murder" ||
                            action->name == "Insult" || action->name == "Discrimination");
            score *= hostile ? 1.5f : 0.25f;
        }

        scores.push_back({neighbor, std::max(0.01f, score)});
    }

    return weightedRandomSelect(scores);
}

void implementRegion(){
    std::cout << "\n";
    std::cout << "Choose a world region (h to help): \n";
    std::cout << "1 Paris, France               | 48 51' 24'' north, 2 21' 07'' east   /// Oceanic\n";
    std::cout << "2 Philadelphia, United States | 39 57' 10'' north, 75 09' 49'' west  /// Continental\n";
    std::cout << "3 Guangzhou, China            | 23 07' 48'' north, 113 15' 36'' east /// monsoon\n";
    std::cout << "4 Addis-Abeba, Ethiopia       | 9 1' 48'' north, 38 44' 24'' east    /// arid\n";
    std::cout << ">";

    char choice;
    std::cin >> choice;
    if(choice == 'h'){
        std::cout << "This feature is principally implemented for disease spreading, it barely affects the simulation links or its global functionning of the siulation\n";
        implementRegion();
    }else{
        // Multiply by 10 so region values are 10/20/30/40 — the formula was designed
        // for single-digit multipliers, not raw ASCII values (49–52).
        Disease::region = (choice - '0') * 10;
    }
}

Entity* weightedRandomSelect(std::vector<std::pair<Entity*, float>> scores){
    if (scores.empty()) return nullptr;   // was UB: scores.back() on empty
    float total = 0.0f;
    for (auto& s : scores) total += s.second;
    if (total <= 0.0f) return scores.front().first;
    float roll = BetterRand::genNrInInterval(0.0f, total);
    float cum = 0.0f;
    for (auto& s : scores) {
        cum += s.second;
        if (roll <= cum) return s.first;
    }
    return scores.back().first;
}

std::vector<Entity> get_new_borns(){
    return FreeWillSystem::new_borns;
}


void tickRelationshipDecay(Entity* ent, float deltaTime) {
    // ── SOCIAL BOND EVOLUTION: bonds GROW from proximity AND decay from distance ──
    for (auto& social : ent->list_entityPointedSocial) {
        // Proximity growth: bonds grow naturally when entities are nearby
        if (social.pointedEntity && social.pointedEntity->entityHealth > 0.0f) {
            float dx = ent->posX - social.pointedEntity->posX;
            float dy = ent->posY - social.pointedEntity->posY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 180.0f) {
                // Closer proximity = more growth; extraverts benefit more.
                // Halved (was 0.012) so passive proximity no longer floods every
                // entity with strong friendships that crowd out desire/anger bonds.
                float proximityFactor = (180.0f - dist) / 180.0f;
                float extraversionBoost = 0.7f + (ent->personality.extraversion / 100.0f) * 0.6f;
                float growth = proximityFactor * 0.006f * extraversionBoost * deltaTime;
                social.social = std::min(100.0f, social.social + growth);
            }
        }

        // Decay: strong bonds decay very slowly, weak bonds decay faster
        float decayRate = 0.008f;
        if (social.social > 80.0f) {
            decayRate = 0.001f; // Strong bonds barely decay
        } else if (social.social > 50.0f) {
            decayRate = 0.003f; // Moderate decay for medium bonds
        } else if (social.social > 20.0f) {
            decayRate = 0.005f; // Light decay for forming bonds
        }
        social.social -= decayRate * deltaTime;
        if (social.social < 0.5f) social.social = 0.0f;
    }

    // ── DESIRE also grows from proximity ──
    for (auto& desire : ent->list_entityPointedDesire) {
        if (desire.pointedEntity && desire.pointedEntity->entityHealth > 0.0f) {
            float dx = ent->posX - desire.pointedEntity->posX;
            float dy = ent->posY - desire.pointedEntity->posY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 140.0f) {
                float proximityFactor = (140.0f - dist) / 140.0f;
                float growth = proximityFactor * 0.006f * deltaTime;
                desire.desire = std::min(100.0f, desire.desire + growth);
            }
        }

        float decayRate = 0.012f;
        if (desire.desire > 60.0f) {
            decayRate = 0.002f; // Strong attraction persists
        } else if (desire.desire > 30.0f) {
            decayRate = 0.006f;
        }
        desire.desire -= decayRate * deltaTime;
        desire.desire = std::max(0.0f, desire.desire);
    }

    // Anger fades based on agreeableness
    float forgivenessRate = 0.02f * (ent->personality.agreeableness / 100.0f) * deltaTime;
    for (auto& anger : ent->list_entityPointedAnger) {
        anger.anger -= forgivenessRate;
        anger.anger = std::max(0.0f, anger.anger);
    }

    // Clean up zeroed bonds
    ent->list_entityPointedSocial.erase(
        std::remove_if(ent->list_entityPointedSocial.begin(), ent->list_entityPointedSocial.end(),
            [](const entityPointedSocial& s){ return s.social <= 0.0f; }),
        ent->list_entityPointedSocial.end());

    // ── DUNBAR-STYLE CAP: keep social bonds in balance with desire/anger/couple ──
    // Without a cap, passive proximity + alliance bonding lets an entity rack up
    // dozens of weak social links while it only ever has a handful of desire/anger
    // bonds. Extraverts tolerate a few more connections than introverts.
    // §8: the ceiling is real, but it is not the same ceiling for everybody —
    // and a world where it is produces the flat degree distribution the report
    // caught (everyone holding six ties, nobody holding twenty). Dunbar's
    // layers scale with temperament AND with what a person's life puts in front
    // of them: a trader on the roads, a priest before the congregation and a
    // leader holding a people together meet a multiple of the people a farmer
    // does. That is what turns the graph right-skewed — most people modest, a
    // few hubs — without lifting the cognitive ceiling itself (150).
    // Kill switch: networkMul == 0 restores the old flat 5–9 cap exactly.
    const int baseCap = 5;
    int socialCap = baseCap + (int)(ent->personality.extraversion / 100.0f * 4.0f); // 5–9
    if (g_liveConfig.networkMul != 0.0f) {
        float cap = 5.0f + ent->personality.extraversion / 100.0f * 10.0f;   // 5–15
        if (ent->isSpecialist) {
            if (ent->specialization == "trader")      cap += 10.0f;
            else if (ent->specialization == "priest") cap += 8.0f;
            else if (ent->specialization == "healer") cap += 6.0f;
            else                                      cap += 3.0f;
        }
        if (globalCivEngine) {
            const Tribe* home = ent->tribeId >= 0 ? globalCivEngine->findTribe(ent->tribeId) : nullptr;
            if (home && home->leaderId == ent->entityId) cap += 12.0f;
        }
        cap *= g_liveConfig.networkMul;
        socialCap = (int)std::min(150.0f, std::max(3.0f, cap));   // Dunbar's ceiling stands
    }
    if ((int)ent->list_entityPointedSocial.size() > socialCap) {
        // Prune the weakest bonds first, preserving the strongest relationships.
        std::sort(ent->list_entityPointedSocial.begin(), ent->list_entityPointedSocial.end(),
            [](const entityPointedSocial& a, const entityPointedSocial& b){ return a.social > b.social; });
        ent->list_entityPointedSocial.resize(socialCap);
    }
}


// M11: bumped at every ent_quad rebuild (init, deaths, births, load) so
// updateMovement can cache its id/index lookup maps across the 60 frames of a
// tick instead of re-hashing the whole population every frame.
static uint64_t g_entQuadVersion = 0;

// ── Force-based movement system ──────────────────────────────────────────────
// Runs every frame (outside the UPDATE_FREQUENCY throttle).
void updateMovement(std::vector<Entity*>& entities, float worldW, float worldH, int simDay) {
    const float MAX_FORCE    = 0.9f * g_liveConfig.moveForceMul;   // M10 live console
    const float PERSONAL_R   = 38.0f;
    const float PROX_REPEL_R = 130.0f;

    // M4: spatial index + id lookup replace the three all-pairs inner scans
    // (children, sick-avoidance, personal space). Rebuilt per call — entities
    // move every frame, and a flat rebuild is O(n).
    // M11: 48px cells — sized for the hot 38px personal-space query. At 130px
    // (the old size) each dense cell held hundreds of agents and every query
    // scanned ~7× more candidates than the radius actually needed.
    static SpatialGrid grid;
    grid.reset(worldW, worldH, 48.0f);
    grid.rebuild(entities);

    // M11: byId / idxOf only change when ent_quad is rebuilt (birth, death,
    // load) — not every frame. Cache them behind the rebuild counter instead
    // of re-hashing the whole population 60× per tick.
    const int n = (int)entities.size();
    static uint64_t cachedVersion = ~0ull;
    static std::unordered_map<int, Entity*> byId;
    static std::unordered_map<Entity*, int> idxOf;
    if (cachedVersion != g_entQuadVersion) {
        byId.clear();
        byId.reserve(entities.size());
        for (Entity* e : entities) byId[e->entityId] = e;
        idxOf.clear();
        idxOf.reserve(n);
        for (int i = 0; i < n; ++i) idxOf[entities[i]] = i;
        cachedVersion = g_entQuadVersion;
    }

    // M11: sick-avoidance inverted — iterate the FEW sick agents and push
    // repulsion onto their healthy neighbors, instead of every healthy agent
    // scanning a 130px disc that is almost always empty of disease. Same
    // per-pair math; accumulated into scratch so the parallel force pass
    // below stays write-local.
    std::vector<std::pair<float, float>> sickRepel((size_t)n, {0.0f, 0.0f});
    for (Entity* sick : entities) {
        if (!sick || sick->entityDiseaseType == -1) continue;
        grid.forEachInRadius(sick->posX, sick->posY, PROX_REPEL_R, [&](Entity* h) {
            if (h == sick || h->entityDiseaseType != -1) return; // only the healthy flee
            float dx = h->posX - sick->posX;
            float dy = h->posY - sick->posY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < PROX_REPEL_R && dist > 0.01f) {
                float mag = (PROX_REPEL_R - dist) / PROX_REPEL_R * 0.6f;
                auto& acc = sickRepel[(size_t)idxOf.at(h)];
                acc.first  += (dx / dist) * mag;
                acc.second += (dy / dist) * mag;
            }
        });
    }

    // M11: synchronous update (the master plan's phase-B snapshot). Pass 1
    // computes every velocity from the CURRENT positions — nothing writes a
    // position, so the pass parallelizes with no races and no RNG, and every
    // agent reacts to the same world state instead of to whatever the agents
    // earlier in the array already did this frame. Pass 2 applies positions.
    #pragma omp parallel for schedule(static)
    for (int ei = 0; ei < n; ++ei) {
        Entity* ent = entities[ei];
        if (ent->entityHealth <= 0.0f) continue;

        float fx = sickRepel[(size_t)ei].first;
        float fy = sickRepel[(size_t)ei].second;

        // ── Couple attraction ─────────────────────────────────────────────────
        for (auto& cp : ent->list_entityPointedCouple) {
            if (!cp.pointedEntity || cp.pointedEntity->entityHealth <= 0.0f) continue;
            Entity* partner = cp.pointedEntity;
            float dx = partner->posX - ent->posX;
            float dy = partner->posY - ent->posY;
            float dist = std::sqrt(dx * dx + dy * dy);
            float target = 22.0f;
            if (dist > target && dist > 0.01f) {
                float mag = (dist - target) / dist * 0.9f;
                fx += dx * mag;
                fy += dy * mag;
            }
        }

        // ── Parent/child attraction ───────────────────────────────────────────
        auto familyPull = [&](Entity* other) {
            if (!other || other->entityHealth <= 0.0f) return;
            float dx = other->posX - ent->posX;
            float dy = other->posY - ent->posY;
            float dist = std::sqrt(dx * dx + dy * dy);
            float target = 65.0f;
            if (dist > target && dist > 0.01f) {
                float mag = (dist - target) / dist * 0.6f;
                fx += dx * mag;
                fy += dy * mag;
            }
        };
        familyPull(ent->parent1);
        familyPull(ent->parent2);
        // Pull toward children — resolved via childrenIds instead of scanning
        // every entity for a matching parent pointer.
        for (int cid : ent->childrenIds) {
            auto it = byId.find(cid);
            if (it != byId.end() && it->second != ent) familyPull(it->second);
        }

        // ── Social bonds ──────────────────────────────────────────────────────
        for (auto& s : ent->list_entityPointedSocial) {
            if (!s.pointedEntity || s.pointedEntity->entityHealth <= 0.0f) continue;
            Entity* other = s.pointedEntity;
            float dx = other->posX - ent->posX;
            float dy = other->posY - ent->posY;
            float dist = std::sqrt(dx * dx + dy * dy);
            float target = (s.social > 80.0f) ? 45.0f : 130.0f;
            float weight = std::min(0.5f, s.social / 100.0f * 0.5f);
            if (dist > target && dist > 0.01f) {
                float mag = (dist - target) / dist * weight;
                fx += dx * mag;
                fy += dy * mag;
            }
        }

        // ── Desire targets — pull toward ──────────────────────────────────────
        bool hasCouple = !ent->list_entityPointedCouple.empty();
        if (ent->entityLoneliness > 30.0f || ent->isGoalType("find_partner")) {
            for (auto& d : ent->list_entityPointedDesire) {
                if (!d.pointedEntity || d.pointedEntity->entityHealth <= 0.0f) continue;
                Entity* other = d.pointedEntity;
                float dx = other->posX - ent->posX;
                float dy = other->posY - ent->posY;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > 0.01f) {
                    float pull = (ent->entityLoneliness / 100.0f) * (d.desire / 100.0f) * 1.8f;
                    fx += (dx / dist) * pull;
                    fy += (dy / dist) * pull;
                }
            }
        }

        // ── Anger targets — flee if agreeable, approach if aggressive ─────────
        for (auto& a : ent->list_entityPointedAnger) {
            if (!a.pointedEntity || a.pointedEntity->entityHealth <= 0.0f) continue;
            Entity* other = a.pointedEntity;
            float dx = other->posX - ent->posX;
            float dy = other->posY - ent->posY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 0.01f) continue;
            bool flee     = (ent->personality.agreeableness > 40.0f);
            bool approach = (ent->entityGeneralAnger > 60.0f && ent->personality.agreeableness < 40.0f);
            float mag = (a.anger / 100.0f) * 0.5f;
            if (flee) {
                fx -= (dx / dist) * mag;
                fy -= (dy / dist) * mag;
            } else if (approach) {
                fx += (dx / dist) * mag;
                fy += (dy / dist) * mag;
            }
        }

        // (Sick-avoidance was seeded into fx/fy above via the inverted pass.)

        // ── Personal space repulsion ──────────────────────────────────────────
        grid.forEachInRadius(ent->posX, ent->posY, PERSONAL_R, [&](Entity* other) {
            if (other == ent || other->entityHealth <= 0.0f) return;
            float dx = ent->posX - other->posX;
            float dy = ent->posY - other->posY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < PERSONAL_R && dist > 0.01f) {
                float mag = (PERSONAL_R - dist) / PERSONAL_R * 1.2f;
                fx += (dx / dist) * mag;
                fy += (dy / dist) * mag;
            }
        });

        // ── Personality drift (sinusoidal wander) ────────────────────────────
        float phase  = (float)(ent->entityId) * 1.3f;
        float amp    = 0.15f * (ent->entityBoredom / 100.0f) * (ent->personality.openness / 100.0f + 0.3f);
        fx += amp * std::sin((float)simDay * 0.05f + phase);
        fy += amp * std::cos((float)simDay * 0.07f + phase + 1.0f);

        // ── Clamp total force ─────────────────────────────────────────────────
        float fmag = std::sqrt(fx * fx + fy * fy);
        if (fmag > MAX_FORCE) {
            fx = fx / fmag * MAX_FORCE;
            fy = fy / fmag * MAX_FORCE;
        }

        // ── Attachment style multiplier ───────────────────────────────────────
        switch (ent->dv.attachmentStyle) {
            case AVOIDANT:
                fx *= 0.55f; fy *= 0.55f;
                break;
            case ANXIOUS:
                fx *= 1.25f; fy *= 1.25f;
                break;
            case DISORGANIZED:
                if ((simDay / 4) % 2 == 0) { fx = -fx; fy = -fy; }
                break;
            default: break;
        }

        // ── Speed multiplier ──────────────────────────────────────────────────
        float speed = 0.55f + (ent->personality.extraversion / 100.0f) * 0.9f;
        if (ent->entityMentalHealth < 30.0f) speed *= 0.45f;
        if (ent->entityDiseaseType != -1)     speed *= 0.28f;
        if (ent->getGriefIntensity() > 0.3f)  speed *= 0.55f;

        ent->velX = fx * speed;
        ent->velY = fy * speed;
    }

    // Pass 2: apply the synchronously computed velocities.
    for (Entity* ent : entities) {
        if (ent->entityHealth <= 0.0f) continue;
        ent->posX += ent->velX;
        ent->posY += ent->velY;

        // ── Clamp to world bounds (above Mind Board) ──────────────────────────
        float minX = 40.0f, maxX = worldW - 40.0f;
        float minY = 40.0f, maxY = worldH * 0.64f - 40.0f;
        ent->posX = std::max(minX, std::min(maxX, ent->posX));
        ent->posY = std::max(minY, std::min(maxY, ent->posY));
    }
}

void applyFreeWill(std::vector<std::vector<Entity*>>& entityGroups, int currentDay, CivilizationEngine* engineCivilization){
    updateEnvironment((int)g_clock.day());   // advance season / harvest before agents act
    EnvironmentalFactors env = generateEnvFactors(currentDay);

    // M11 staggering: above 2,000 living agents, deliberation goes round-robin
    // by entityId cohort — upkeep (needs, disease, grief) still ticks everyone
    // every tick, but only 1/K of the population runs the expensive decision
    // pipeline per tick. Deterministic (id- and day-keyed), and runs below the
    // threshold are byte-identical to the unstaggered engine.
    size_t totalPop = 0;
    for (auto& g : entityGroups) totalPop += g.size();
    const int staggerCohorts = totalPop > 6000 ? 4 : (totalPop > 2000 ? 2 : 1);

    // AI upgrade: shared per-tick context for the mind upkeep pass.
    const int  hourNow  = g_clock.hourOfDay();
    const bool isNight  = (hourNow >= 22 || hourNow < 6);
    const int  civDayNow = currentDay / SimClock::FRAMES_PER_TICK;

    // ── Phase 5: pathogen vectors, once per in-world day ─────────────────────
    // Two strictly separate passes, same idiom as the M11 movement pass below
    // ("Pass 1 computes... nothing writes... Pass 2 applies" — every agent
    // reacts to the same settled world state instead of whatever earlier
    // agents in this same frame already changed). Exposure-check reads
    // OTHER entities' isContagious flag; if it ran interleaved with
    // tickPathogens (which can flip that flag) inside the single per-entity
    // loop below, entity order within a group would silently change who
    // catches what on a given day — chaos-amplified, that desynced the
    // determinism pair at scale. Splitting into two full passes removes the
    // ordering dependency: pass 1 only ever sees yesterday's settled
    // contagion state, and pass 2's own mutations (fresh exposures start
    // non-contagious) can't feed back into pass 1 since it already finished.
    if (currentDay % SimClock::FRAMES_PER_TICK == 0) {
#if 0
        for (auto& group : entityGroups) {
            for (Entity* entity : group) {
                if (entity->entityHealth <= 0.0f) continue;
                for (Entity* other : group) {
                    if (other == entity || other->entityHealth <= 0.0f) continue;
                    bool exposed = false;
                    for (const auto& p : other->pathogenExposures) {
                        if (p.isContagious) { entity->exposeToPathogen(p.pathogenId, civDayNow); exposed = true; break; }
                    }
                    if (exposed) break;
                }
            }
        }
        for (auto& group : entityGroups) {
            for (Entity* entity : group) {
                if (entity->entityHealth <= 0.0f) continue;
                entity->tickPathogens(civDayNow);
                entity->resolveBiologicalHomeostasis();
            }
        }
#endif
    }

    // Process each group of close entities
    for(auto& group : entityGroups){
        for(Entity* entity : group){
            // The dead don't act. Grief propagation happens exactly once, in
            // updateSimulationStep's removal pass, where the whole population
            // is in scope (a group-only pass missed partners in other groups
            // and double-counted murder grief).
            if(entity->entityHealth <= 0.0f) continue;

            //on applique aussi les paramètres de maladies
            std::array<int, 2> peopleCloseDisease = getNBSickClose(group);
            applyDisease(entity, group.size(), peopleCloseDisease[0], peopleCloseDisease[1]);

            // Tick grief recovery
            entity->tickGrief(1.0f);

            // ── AI upgrade upkeep (B1 emotions, D2 sleep, D5 homesickness):
            // appraisal + decay runs for EVERYONE every tick, independent of
            // the deliberation cohort stagger — feelings don't take turns.
            mind::upkeep(entity, group, isNight, civDayNow);
            // B4: adopt/abandon the season's pursuit — id-staggered weekly so
            // the whole world doesn't re-plan on the same day.
            if ((civDayNow + entity->entityId) % 7 == 0)
                mind::updateIntention(entity, civDayNow);

            //lower pheromones
            if(!entity->pheromone.type.empty()){
                entity->pheromone.releasing_level -= BetterRand::genNrInInterval(3.0,6.0);
            }

            // handle old people
            if (entity->entityAge > 100){
              entity->entityHealth -= BetterRand::genNrInInterval(0, 8);
              Disease d;
              d.checkInfamousDisease(entity);
            }
            if (entity->entityAge > 100){
              // some entities lives up to 140 years need to refresh generation
            entity->entityAge -= BetterRand::genNrInInterval(0, 3);
            }



            FreeWillSystem& sys = entity->getFreeWill();

            // FIX: Remove duplicate social decay - now handled properly in tickRelationshipDecay
            // This was causing double decay which prevented relationships from forming
            // float deltaTime = 1.0f;
            // for (auto &link : entity->list_entityPointedSocial) {
            //     link.social = std::max(0.0f, link.social - (0.005f * deltaTime));
            // }
            float deltaTime = 1.0f;

            // NOTE: loneliness & boredom growth now lives solely in the
            // personality block below. The previous extra `+= 0.05` / `+= 0.04`
            // here double-counted it, so both stats pinned to 100 and went stale
            // ("abandoned"). Removing it gives them real dynamic range again.

            // Apply direct environmental stat effects
            sys.applyEnvironmentalEffects(entity, env);


            {
                const Personality& p = entity->personality;
                auto pdclamp = [](float v, float lo, float hi){ return std::max(lo, std::min(hi, v)); };

                // Hygiene degrades very slowly — noticeable over many ticks, not per tick
                float hygieneDecay = 0.08f + (1.0f - p.conscientiousness / 100.0f) * 0.10f;
                entity->entityHygiene = pdclamp(entity->entityHygiene - hygieneDecay, 0.0f, 100.0f);

                // Stress builds; neurotic entities accumulate it faster
                float stressGrowth = 0.06f + (p.neuroticism / 100.0f) * 0.12f;
                if (entity->entityLoneliness > 60.0f) stressGrowth += 0.04f;
                entity->entityStress = pdclamp(entity->entityStress + stressGrowth, 0.0f, 100.0f);

                // Boredom builds gently; curious (open) minds tire of routine faster
                float boredomGrowth = 0.10f + (p.openness / 100.0f) * 0.10f;
                entity->entityBoredom = pdclamp(entity->entityBoredom + boredomGrowth, 0.0f, 100.0f);

                // Anger decays naturally; agreeable people let it go faster
                float angerDecay = 0.2f + (p.agreeableness / 100.0f) * 0.4f;
                entity->entityGeneralAnger = pdclamp(entity->entityGeneralAnger - angerDecay, 0.0f, 100.0f);

                // Loneliness builds slowly always; extraverts feel it faster.
                // Rate trimmed (was 0.25-0.50) now that the duplicate growth above
                // is gone, so social actions can actually claw it back down.
                float lonelinessGrowth = 0.15f + (p.extraversion / 100.0f) * 0.20f;
                entity->entityLoneliness = pdclamp(entity->entityLoneliness + lonelinessGrowth, 0.0f, 100.0f);

                // Happiness drifts toward a personality-based setpoint
                float happinessSetpoint = 40.0f + (p.agreeableness / 100.0f) * 15.0f
                                                 - (p.neuroticism   / 100.0f) * 20.0f
                                                 + (p.extraversion  / 100.0f) * 10.0f;
                float happinessDrift = (happinessSetpoint - entity->entityHapiness) * 0.02f;
                entity->entityHapiness = pdclamp(entity->entityHapiness + happinessDrift, 0.0f, 100.0f);

                // Mental health degrades under chronic stress, recovers in calm
                if (entity->entityStress > 70.0f)
                    entity->entityMentalHealth = pdclamp(entity->entityMentalHealth - 0.10f, 0.0f, 100.0f);
                else if (entity->entityStress < 30.0f)
                    entity->entityMentalHealth = pdclamp(entity->entityMentalHealth + 0.08f, 0.0f, 100.0f);
                else
                    entity->entityMentalHealth = pdclamp(entity->entityMentalHealth + 0.02f, 0.0f, 100.0f);  // small passive recovery

                // Health decays very slowly; high stress accelerates, conscientiousness slows it
                float healthDecay = 0.002f + (entity->entityStress / 100.0f) * 0.008f
                                          - (p.conscientiousness  / 100.0f) * 0.003f;
                entity->entityHealth = pdclamp(entity->entityHealth - healthDecay, 0.0f, 100.0f);

                // ── Metabolism: burn stored food, or starve ──────────────────
                // Each tick the body consumes rations. While the store holds out
                // the entity stays fed; once it runs dry, hunger climbs and —
                // past a threshold — eats away health. This is what makes food a
                // requirement for life rather than a market curiosity.
                // Cold seasons raise the calorie cost of staying alive.
                float coldFactor = (g_seasonTemperature < 45.0f)
                                 ? 1.0f + (45.0f - g_seasonTemperature) / 100.0f : 1.0f;
                // Genome read site (Step 4): fast metabolisms burn rations quicker.
                float burn = 0.08f * deltaTime * coldFactor * entity->genome.metabolism;
                if (entity->foodStore > burn) {
                    entity->foodStore -= burn;
                    entity->entityHunger = pdclamp(entity->entityHunger - 0.8f, 0.0f, 100.0f);
                } else {
                    entity->foodStore = 0.0f;
                    entity->entityHunger = pdclamp(entity->entityHunger + 0.45f * coldFactor, 0.0f, 100.0f);
                }
                if (entity->entityHunger > 80.0f) {
                    // Real hunger still kills, but more slowly — it should grind a
                    // neglected agent down over many ticks, not wipe out anyone who
                    // misses a couple of meals.
                    float starve = (entity->entityHunger - 80.0f) / 20.0f; // 0..1
                    entity->entityHealth   = pdclamp(entity->entityHealth   - starve * 0.18f, 0.0f, 100.0f);
                    entity->entityHapiness = pdclamp(entity->entityHapiness - starve * 0.4f, 0.0f, 100.0f);
                    entity->entityStress   = pdclamp(entity->entityStress   + starve * 0.3f, 0.0f, 100.0f);
                }

                // ── Fatigue: drifts up with wakefulness, relieved by rest/sleep.
                // Exhaustion frays the mind and body (the doc's second named need).
                entity->fatigueLevel = pdclamp(entity->fatigueLevel + 0.18f * deltaTime, 0.0f, 100.0f);
                if (entity->fatigueLevel > 80.0f) {
                    // Exhaustion mostly frays the mind and raises stress; it only
                    // lightly chips health, since people rarely die of tiredness
                    // alone (it makes them vulnerable, it doesn't kill outright).
                    float tired = (entity->fatigueLevel - 80.0f) / 20.0f; // 0..1
                    entity->entityStress      = pdclamp(entity->entityStress      + tired * 0.4f, 0.0f, 100.0f);
                    entity->entityMentalHealth= pdclamp(entity->entityMentalHealth- tired * 0.2f, 0.0f, 100.0f);
                    entity->entityHealth      = pdclamp(entity->entityHealth      - tired * 0.04f, 0.0f, 100.0f);
                }

                // Low hygiene cascades into stress and happiness
                if (entity->entityHygiene < 25.0f) {
                    entity->entityStress   = pdclamp(entity->entityStress   + 0.4f, 0.0f, 100.0f);
                    entity->entityHapiness = pdclamp(entity->entityHapiness - 0.3f, 0.0f, 100.0f);
                }

                // Extreme loneliness erodes mental health
                if (entity->entityLoneliness > 80.0f)
                    entity->entityMentalHealth = pdclamp(entity->entityMentalHealth - 0.1f, 0.0f, 100.0f);

                // Chronic boredom saps happiness and, when severe, mental health —
                // an under-stimulated life slowly grinds someone down.
                if (entity->entityBoredom > 60.0f)
                    entity->entityHapiness = pdclamp(entity->entityHapiness - 0.12f, 0.0f, 100.0f);
                if (entity->entityBoredom > 85.0f)
                    entity->entityMentalHealth = pdclamp(entity->entityMentalHealth - 0.06f, 0.0f, 100.0f);

                // ── Bipolar drives (0 -> setpoint <- 1) ──────────────────────
                // The block above is the legacy *unipolar* drift: every stat only
                // climbs and only the high pole bites. The drive layer adds the
                // missing half — a lethal FLOOR, chronic allostatic load, and
                // dopamine habituation — without disturbing that tuned drift.
                //
                // The legacy floats only ever span "deprivation -> fine", so they
                // feed the LOWER half of each axis (deprivation pole -> setpoint);
                // the ceiling is only reached once an action over-satisfies via
                // Drive::satisfy(). Happiness is genuinely bidirectional, so it
                // maps to the FULL pleasure axis — making the joy-overload and
                // down-regulation loop live immediately.
                {
                    DriveSet& dr = entity->drives;
                    if (dr.has("pleasure")) {   // guard: drives are initialized
                        // Lazy psychology init for entities that bypassed spawn
                        // (births, loaded saves). Safe: the bridge re-syncs drive
                        // values from the legacy floats just below, so this only
                        // (re)shapes setpoints/bands from the function stack.
                        if (!entity->cognition.built) entity->initPsychology();

                        auto half = [](Drive& d, float legacy01){
                            // legacy 0 (fine) -> setpoint, legacy 1 (deprived) -> deprivation pole
                            d.value = Drive::clamp01(d.setpoint * (1.0f - legacy01));
                        };
                        auto highHalf = [](Drive& d, float legacy01){
                            // legacy 0 (sated) -> setpoint, legacy 1 (starving) -> high lethal pole
                            d.value = Drive::clamp01(d.setpoint + (1.0f - d.setpoint) * legacy01);
                        };
                        highHalf(dr["hunger"],      entity->entityHunger     / 100.0f);
                        highHalf(dr["fatigue"],     entity->fatigueLevel     / 100.0f);
                        half    (dr["stimulation"], entity->entityBoredom    / 100.0f);
                        half    (dr["social"],      entity->entityLoneliness / 100.0f);
                        dr["pleasure"].value = Drive::clamp01(entity->entityHapiness / 100.0f);

                        // Bridge mode: we move the values ourselves above, so the
                        // drive only accrues load/tolerance and reports damage.
                        float dmg = dr.tick(deltaTime, /*applyEntropy=*/false);
                        entity->entityHealth = pdclamp(entity->entityHealth - dmg, 0.0f, 100.0f);

                        // Mind-axis wear frays the mind: isolation and an under-
                        // stimulated life grind down mental health past what the
                        // legacy thresholds caught.
                        float mindLoad = dr["stimulation"].load + dr["social"].load
                                       + dr["pleasure"].load;
                        entity->entityMentalHealth = pdclamp(
                            entity->entityMentalHealth - mindLoad * 0.05f, 0.0f, 100.0f);

                        // Hedonic treadmill: as pleasure habituates, the same input
                        // feels flatter — the felt baseline is dragged down, so
                        // nothing natural quite satisfies anymore.
                        entity->entityHapiness = pdclamp(
                            entity->entityHapiness - dr["pleasure"].tolerance * 0.15f,
                            0.0f, 100.0f);

                        // ── Beebe grip / shadow ─────────────────────────────
                        // Psychic load (worst drive wear + stress) decides who is
                        // driving. Below threshold the Hero leads; above it the
                        // inferior erupts, and at extreme load a context-specific
                        // shadow archetype takes over and acts out.
                        float worstLoad = 0.0f;
                        for (auto& kv : dr.axes)
                            worstLoad = std::max(worstLoad, kv.second.load);
                        float psychicLoad = Drive::clamp01(
                            0.6f * worstLoad + 0.4f * (entity->entityStress / 100.0f));

                        unsigned flags = GT_NONE;
                        if (entity->entityHealth < 25.0f ||
                            entity->getGriefIntensity() > 0.6f)         flags |= GT_THREAT;
                        if (entity->entityGeneralAnger > 60.0f)         flags |= GT_CHALLENGED;
                        if (entity->dominanceRank > 60.0f)              flags |= GT_AUTHORITY;
                        for (auto& c : entity->list_entityPointedCouple)
                            if (c.suspicion > 50.0f) { flags |= GT_DOUBLEBIND; break; }
                        if (entity->entityLoneliness > 60.0f &&
                            !entity->list_entityPointedCouple.empty())  flags |= GT_DOUBLEBIND;

                        Archetype prev = entity->cognition.active;
                        Archetype now  = entity->cognition.resolveActive(psychicLoad, flags);
                        if (now != Archetype::Hero) {
                            float in = entity->cognition.gripIntensity(psychicLoad);
                            // The grip is itself stressful; the destructive shadows
                            // (Demon/Trickster) also spill into anger.
                            float stressKick = (now == Archetype::Demon) ? 1.2f : 0.5f;
                            entity->entityStress   = pdclamp(entity->entityStress   + in * stressKick, 0.0f, 100.0f);
                            entity->entityHapiness = pdclamp(entity->entityHapiness - in * 0.4f,        0.0f, 100.0f);
                            if (now == Archetype::Demon || now == Archetype::Trickster)
                                entity->entityGeneralAnger = pdclamp(entity->entityGeneralAnger + in * 0.8f, 0.0f, 100.0f);
                            // Narrate only on transition, so it reads as an event.
                            if (now != prev)
                                entity->innerMonologue = entity->cognition.gripNarrative();
                        }
                    }
                }
            }

            // Passive recovery: not sick = slow heal (recovery no longer blocked by
            // moderate stress). Slightly stronger than the chronic-need drains so a
            // fed, rested agent reliably mends instead of slowly bleeding to death.
            if (entity->entityDiseaseType == -1 && entity->entityHealth < 97.0f)
                entity->entityHealth = std::min(100.0f, entity->entityHealth + 0.06f);

            //apply tick relationship
            tickRelationshipDecay(entity, 1.0f);

            std::vector<Entity*> neighbors;
            for(Entity* potential_neighbor : group){
                if(potential_neighbor != entity && potential_neighbor->entityHealth > 0.0f){
                    neighbors.push_back(potential_neighbor);
                }
            }

            // Create context based on current simulation time (includes env and norms)
            ActionContext context = createContextFromTime(currentDay, neighbors.size());

            // ── Situation-aware AI: detect nearby relationships and set hint ──
            {
                bool hasCouple = false;
                // couple_nearby: partner within 70px
                for (auto& cp : entity->list_entityPointedCouple) {
                    if (!cp.pointedEntity || cp.pointedEntity->entityHealth <= 0.0f) continue;
                    float dx = entity->posX - cp.pointedEntity->posX;
                    float dy = entity->posY - cp.pointedEntity->posY;
                    if (dx*dx + dy*dy <= 70.0f * 70.0f) {
                        context.situationHint = "couple_nearby";
                        hasCouple = true;
                        break;
                    }
                }
                // enemy_nearby: anger target (anger>40) within 90px
                if (context.situationHint.empty()) {
                    for (auto& a : entity->list_entityPointedAnger) {
                        if (!a.pointedEntity || a.pointedEntity->entityHealth <= 0.0f) continue;
                        if (a.anger <= 40.0f) continue;
                        float dx = entity->posX - a.pointedEntity->posX;
                        float dy = entity->posY - a.pointedEntity->posY;
                        if (dx*dx + dy*dy <= 90.0f * 90.0f) {
                            context.situationHint = "enemy_nearby";
                            break;
                        }
                    }
                }
                // family_nearby: parent within 90px
                if (context.situationHint.empty()) {
                    auto checkParent = [&](Entity* parent) {
                        if (!parent || parent->entityHealth <= 0.0f) return;
                        float dx = entity->posX - parent->posX;
                        float dy = entity->posY - parent->posY;
                        if (dx*dx + dy*dy <= 90.0f * 90.0f)
                            context.situationHint = "family_nearby";
                    };
                    checkParent(entity->parent1);
                    if (context.situationHint.empty()) checkParent(entity->parent2);
                }
                // desire_nearby: desire target within 80px (only if no couple)
                if (context.situationHint.empty() && !hasCouple) {
                    for (auto& d : entity->list_entityPointedDesire) {
                        if (!d.pointedEntity || d.pointedEntity->entityHealth <= 0.0f) continue;
                        float dx = entity->posX - d.pointedEntity->posX;
                        float dy = entity->posY - d.pointedEntity->posY;
                        if (dx*dx + dy*dy <= 80.0f * 80.0f) {
                            context.situationHint = "desire_nearby";
                            break;
                        }
                    }
                }
            }

            // M11: not this entity's turn to deliberate — upkeep already ran.
            if (staggerCohorts > 1 &&
                (entity->entityId % staggerCohorts) != ((int)g_clock.day() % staggerCohorts))
                continue;

            // Choose action based on needs, social environment, context, personality, grief, and env
            Action* chosenAction = sys.chooseAction(entity, neighbors, context);

            // Tick the Tree of Thoughts planning system
            entity->planner.tick(entity, neighbors, 1.0f);

            //social deficit
            if (chosenAction && chosenAction->needCategory == "social" &&
                chosenAction->name != "Murder" && chosenAction->name != "Betray" &&
                chosenAction->name != "Discrimination") {
                // Only real social fulfillment clears deficit
                entity->socialDeficit = std::max(0.0f, entity->socialDeficit - 8.0f);
                entity->dayWithoutSocialAction = 0;
            } else {
                if (entity->entityLoneliness > 10.0f) {
                    entity->socialDeficit += 2.0f;
                }
                entity->dayWithoutSocialAction++;
            }
            entity->socialDeficit = std::min(50.0f, entity->socialDeficit);


            //Update Hierachical need
            sys.updateHieratchicalNeed(entity, *chosenAction);
            sys.updateNeeds(currentDay, entity);

            if (neighbors.size() > 0) {
                float socialDrain = 0.0f;
                for (Entity* n : neighbors) {
                    float bond = entity->searchConnSocial(n);
                    if (bond > 10.0f) socialDrain += 0.15f;
                    else socialDrain += 0.05f;
                }
                entity->entityLoneliness = std::max(0.0f, entity->entityLoneliness - socialDrain);
            } else {
                entity->entityLoneliness += 2.5f;
            }
            entity->entityLoneliness = std::min(100.0f, entity->entityLoneliness);

            //chaque 10 ticks on applique le développement
            // et on update les goals
            if(currentDay % 10 == 0){
                sys.tickChildDevelopment(entity, 1.0f);
                entity->recalculatePriority();
            }

            // Determine if this is a pointed action (requires a target)
            bool isPointedAction = (chosenAction->name == "Socialize" ||
                                   chosenAction->name == "Desire" ||
                                   chosenAction->name == "GoodConnection" ||
                                   chosenAction->name == "AngerConnection" ||
                                   chosenAction->name == "Murder" ||
                                   chosenAction->name == "Discrimination" ||
                                   chosenAction->name == "breeding" ||
                                   chosenAction->name == "couple" ||
                                   chosenAction->name == "Gossip" ||
                                   chosenAction->name == "Apologize" ||
                                   chosenAction->name == "HelpSupport" ||
                                   chosenAction->name == "GiveGift" ||
                                   chosenAction->name == "IgnoreAvoid" ||
                                   chosenAction->name == "Insult" ||
                                   chosenAction->name == "Manipulate" ||
                                   chosenAction->name == "Jealousy" ||
                                   chosenAction->name == "Betray" ||
                                   chosenAction->name == "Flirt" ||
                                   chosenAction->name == "Date" ||
                                   chosenAction->name == "BreakUp" ||
                                   chosenAction->name == "Reconcile" ||
                                   chosenAction->name == "SetBoundaries" ||
                                   // Civilization actions (pointed)
                                   chosenAction->name == "Preach" ||
                                   chosenAction->name == "TeachSkill" ||
                                   chosenAction->name == "ChallengeLeader" ||
                                   chosenAction->name == "DeclareWar" ||
                                   chosenAction->name == "Negotiate" ||
                                   chosenAction->name == "Trade" ||
                                   chosenAction->name == "Marry" ||
                                   chosenAction->name == "Duel" ||
                                   chosenAction->name == "Raid");

            Entity* target = nullptr;
            if(isPointedAction && !neighbors.empty()){
                target = selectSocialTarget(entity, neighbors, chosenAction);
            }
            if(target != nullptr){
                bool targetWasAlive = (target->entityHealth > 0.0f);

                // Execute the action with the target
                sys.executeAction(entity, chosenAction, context, target);

                //choosing side social action
                // if(entity->dayWithoutSocialAction >= 2){
                Action* side_social_act = sys.ChooseSpecificSocialAction(entity);
                if (side_social_act) {
                    sys.executeAction(entity, side_social_act, context, target);
                }
                //saving data
                entity->saveEntityStats(chosenAction);
                g_actionTally[chosenAction->name]++;
                g_faithActionTally[entity->religionId][chosenAction->name]++;
                globalLogger->logAction(entity->entityId, entity->name, chosenAction->name, target->name, "targeted action");

                // ── Romantic side-drive ───────────────────────────────────────
                // Fires periodically and, crucially, ASSIMILATES on a fitting mate
                // (most-desired / most-attractive nearby), so desire and couples
                // actually build instead of only platonic social bonds. The couple/
                // breeding branches self-throttle via their desire/familiarity gates.
                if (BetterRand::genNrInInterval(0, 100) < 45) {
                    Action* romantic = sys.TriggerDesireLinkedAction();
                    Entity* mate = sys.selectSocialTarget(entity, neighbors, romantic);
                    if (romantic && mate) {
                        sys.executeAction(entity, romantic, context, mate);
                        const std::string& rn = romantic->name;
                        if (rn == "Desire" || rn == "Flirt" || rn == "Date" ||
                            rn == "couple" || rn == "breeding" || rn == "Reconcile") {
                            sys.pointedAssimilation(entity, mate, romantic, engineCivilization);
                        }
                    }
                }

                // ── Hostile side-drive ────────────────────────────────────────
                // Only when there is a genuine grievance, so the world is neither
                // uniformly friendly nor uniformly hostile. Murder is never driven
                // from here (it would cause carnage) — only resentment links build.
                bool hasGrievance = (entity->entityGeneralAnger > 35.0f) ||
                                    (!entity->list_entityPointedAnger.empty()) ||
                                    (entity->personality.agreeableness < 35.0f &&
                                     BetterRand::genNrInInterval(0, 100) < 30);
                if (hasGrievance && BetterRand::genNrInInterval(0, 100) < 40) {
                    Action* hostile = sys.TriggerHatredLinkedAction();
                    Entity* foe = sys.selectSocialTarget(entity, neighbors, hostile);
                    if (hostile && foe) {
                        sys.executeAction(entity, hostile, context, foe);
                        const std::string& hn = hostile->name;
                        if (hn == "AngerConnection" || hn == "Discrimination") {
                            sys.pointedAssimilation(entity, foe, hostile, engineCivilization);
                        }
                    }
                }

                // ── Narrative + inner monologue (GUI-only presentation) ──────
                if (!g_headlessMode) {
                    int hour = g_clock.hourOfDay();
                    entity->lastActionName = chosenAction->name;
                    entity->lastNarrative  = NarrativeEngine::actionToSentence(
                        entity, chosenAction->name, target, hour, "");
                    entity->innerMonologue = generateDeepMonologue(entity);
                    std::string entry = "[" + NarrativeEngine::formatHour(hour) + "] " + entity->lastNarrative;
                    globalNarrativeLog.push_back(entry);
                    if (globalNarrativeLog.size() > 200) globalNarrativeLog.pop_front();
                }

                // Update relationship based on action
                sys.pointedAssimilation(entity, target, chosenAction, engineCivilization);


                // Detect murder: if target just died, trigger grief in all connected entities
                if(targetWasAlive && target->entityHealth <= 0.0f){
                    std::stringstream ss;
                    ss << "DEATH EVENT: " << target->name << " was killed. Propagating grief.";
                    globalLogger->logCmd(ss.str());
                    // Attribute the cause now; the central death-handler is the
                    // sole site that writes the death line, so the ledger no
                    // longer double-counts this body as both "murder" and
                    // "hardship".
                    target->pendingDeathCause = "murder by " + entity->name;
                    // Structured attribution (Plan 4.3): killer identity & motive.
                    target->pendingKillerId      = entity->entityId;
                    target->pendingKillerTribeId = entity->tribeId;
                    target->pendingKillMotive    = "aggression";
                    // E3: the chronicle names the feeling behind the blow.
                    { std::string emo = entity->emotions.dominant();
                      if (!emo.empty()) target->pendingKillMotive += " (" + emo + ")"; }
                    // Grief propagation happens once, in the central removal
                    // pass (handleDeath) — no per-group duplicate here.
                    // M5: the group saw it happen — sanctions land immediately.
                    sys.applySocialSanction(entity, target, neighbors, true, currentDay);
                }
            } else {
                // Execute self-directed action
                sys.executeAction(entity, chosenAction, context);
                entity->saveEntityStats(chosenAction);
                g_actionTally[chosenAction->name]++;
                g_faithActionTally[entity->religionId][chosenAction->name]++;
                globalLogger->logAction(entity->entityId, entity->name, chosenAction->name, "", "self-directed action");

                // ── Narrative + inner monologue (GUI-only presentation) ──────
                if (!g_headlessMode) {
                    int hour = g_clock.hourOfDay();
                    entity->lastActionName = chosenAction->name;
                    entity->lastNarrative  = NarrativeEngine::actionToSentence(
                        entity, chosenAction->name, nullptr, hour, "");
                    entity->innerMonologue = generateDeepMonologue(entity);
                    std::string entry = "[" + NarrativeEngine::formatHour(hour) + "] " + entity->lastNarrative;
                    globalNarrativeLog.push_back(entry);
                    if (globalNarrativeLog.size() > 200) globalNarrativeLog.pop_front();
                }
            }

            // ── M6: social learning by observation ────────────────────────────
            // Everyone in the group watched this action land (or flop). Notably
            // good or bad outcomes nudge each observer's own value estimates —
            // this is how a productive foraging technique or a disastrous
            // brawl spreads through a community without anyone teaching it.
            {
                float observed = chosenAction->outcomeSuccess;
                if (std::abs(observed - 0.5f) > 0.15f) {
                    for (Entity* watcher : neighbors) {
                        if (!watcher || watcher == entity || watcher->entityHealth <= 0.0f) continue;
                        watcher->getFreeWill().learnByObservation(
                            watcher, entity, chosenAction->name,
                            observed, (int)neighbors.size());
                    }
                }
            }

            // ── Social fallout: jealousy, rivalry, infidelity, crimes of passion ──
            // Reads this entity's couple/desire configuration against the group and
            // produces emergent consequences (resentment, violence, breakups).
            sys.processSocialConsequences(entity, group, currentDay);

            sys.applyEmotionalContagion(entity, group);
        }
    }
}



void updateSimulationStep(std::vector<Entity>& entities, std::vector<Entity*>& ent_quad, std::vector<std::vector<Entity*>>& close_entity_together, int& day, int& frameCounter, const int UPDATE_FREQUENCY, bool isPaused, int width, int height, int& selectedEntityIndex, bool& showEntityWindow, CivilizationEngine* engineCivilization) {
    // Keep the global clock in sync with the frame counter ("day" is,
    // historically, a frame count — SimClock derives tick/day/year from it).
    g_clock.frame = (uint64_t)day;
    // ── Safe dead-entity removal ──────────────────────────────────────────────
    // Raw Entity* pointers become dangling after erase() shifts the vector.
    // Strategy: snapshot old addresses, null pointers to dead entities, batch
    // erase, rebuild ent_quad, then repair surviving pointers via id lookup.
    {
        // 1. Snapshot: old address → entity ID (all pointers still valid here)
        std::unordered_map<const Entity*, int> ptrToId;
        ptrToId.reserve(entities.size());
        for (const Entity& e : entities) ptrToId[&e] = e.entityId;

        // 2. Collect dead IDs and log
        std::unordered_set<int> deadIds;
        int selectedId = (selectedEntityIndex >= 0 && selectedEntityIndex < (int)entities.size())
                         ? entities[selectedEntityIndex].entityId : -1;
        for (Entity& e : entities) {
            if (e.entityHealth <= 0.0f) {
                std::cout << "Entity " << e.entityId << " has died.\n";
                // Single, population-wide grief pass (partners/kin in other
                // groups included). Runs while all pointers are still valid.
                handleDeath(&e, ent_quad);
                // Single, authoritative attribution: killings carry an explicit
                // cause (pendingDeathCause); everything else is resolved from the
                // deceased's terminal state into a specific named cause rather
                // than the old catch-all "hardship".
                std::string cause = determineDeathCause(e);
                // M9: in-memory ledger feeding the end-of-run realism report.
                {
                    std::string bucket = cause;
                    if (cause.find("crime of passion") != std::string::npos ||
                        cause.rfind("murder", 0) == 0)
                        bucket = "homicide";
                    g_deathLedger[bucket]++;
                    // §8: the two distributions the report checks the shape of.
                    g_deathAges.push_back(e.entityAge);
                    g_deathStress.push_back(e.entityStress);
                }
                if (globalLogger) {
                    globalLogger->logDeath(e.entityId, e.name, (int)e.entityAge,
                                           cause, deathContext(e));
                }
                // Record every death in the civilisation's running history & tally.
                if (globalCivEngine) {
                    globalCivEngine->totalDeaths++;
                    // I-P3: close the book on this life — gather what it changed,
                    // pass its standing to its children, mark the ground. Runs
                    // HERE, inside the death pass, while every pointer into
                    // `entities` is still valid (the same reason handleDeath is
                    // called from this block and not after the erase).
                    globalCivEngine->recordLegacy(entities, e, day / 60);
                    globalCivEngine->logEvent(day / 60, e.name + " died of " + cause
                        + " at age " + std::to_string((int)e.entityAge), "death");
                }
                // Pass the estate of obligations (debts, clients, creditors) to the
                // eldest living child; with no heir, those bonds simply dissolve.
                if (globalSocialOrder) {
                    int heir = -1;
                    for (int cid : e.childrenIds) {
                        for (const Entity& cand : entities)
                            if (cand.entityId == cid && cand.entityHealth > 0.0f) { heir = cid; break; }
                        if (heir != -1) break;
                    }
                    globalSocialOrder->onDeath(entities, e.entityId, heir,
                        globalCivEngine ? globalCivEngine->getCurrentYear() : 0);
                }
                deadIds.insert(e.entityId);
            }
        }

        if (!deadIds.empty()) {
            // 3. Null out pointers to dead entities (pointers still valid)
            for (Entity& e : entities) {
                auto nullIfDead = [&](Entity*& p) {
                    if (!p) return;
                    auto it = ptrToId.find(p);
                    if (it != ptrToId.end() && deadIds.count(it->second)) p = nullptr;
                };
                for (auto& d : e.list_entityPointedDesire)  nullIfDead(d.pointedEntity);
                for (auto& a : e.list_entityPointedAnger)   nullIfDead(a.pointedEntity);
                for (auto& s : e.list_entityPointedSocial)  nullIfDead(s.pointedEntity);
                for (auto& c : e.list_entityPointedCouple)  nullIfDead(c.pointedEntity);
                nullIfDead(e.parent1);
                nullIfDead(e.parent2);
                for (auto* m : e.list_MentalModelOfOther)
                    if (m) nullIfDead(m->entityPointed);
                // Free models of the deceased — you stop modeling the dead,
                // and stale null entries would clog the 16-slot ToM capacity.
                // A dying entity's whole list goes too: the vector holds raw
                // owning pointers, so erasing the Entity would leak them.
                bool dying = deadIds.count(e.entityId) != 0;
                if (dying) e.flushEntityStats();   // final CSV rows before erase
                e.list_MentalModelOfOther.erase(
                    std::remove_if(e.list_MentalModelOfOther.begin(), e.list_MentalModelOfOther.end(),
                        [&](MentalModelOfOther* m) {
                            if (!m) return true;
                            if (dying || m->entityPointed == nullptr) {
                                delete m;
                                return true;
                            }
                            return false;
                        }),
                    e.list_MentalModelOfOther.end());
            }

            // 4. Batch erase — shifts surviving elements; pointers now stale
            entities.erase(
                std::remove_if(entities.begin(), entities.end(),
                    [](const Entity& e){ return e.entityHealth <= 0.0f; }),
                entities.end());

            // 5. Rebuild ent_quad
            ent_quad.clear();
            for (Entity& e : entities) ent_quad.push_back(&e);
            ++g_entQuadVersion;   // invalidate movement lookup caches

            // 6. Repair surviving pointedEntity pointers (old addr → id → new addr)
            std::unordered_map<int, Entity*> idToNewPtr;
            for (Entity* e : ent_quad) idToNewPtr[e->entityId] = e;
            for (Entity& e : entities) {
                auto repairPtr = [&](Entity*& p) {
                    if (!p) return;
                    auto oldIt = ptrToId.find(p);
                    if (oldIt == ptrToId.end()) { p = nullptr; return; }
                    auto newIt = idToNewPtr.find(oldIt->second);
                    p = (newIt != idToNewPtr.end()) ? newIt->second : nullptr;
                };
                for (auto& d : e.list_entityPointedDesire)  repairPtr(d.pointedEntity);
                for (auto& a : e.list_entityPointedAnger)   repairPtr(a.pointedEntity);
                for (auto& s : e.list_entityPointedSocial)  repairPtr(s.pointedEntity);
                for (auto& c : e.list_entityPointedCouple)  repairPtr(c.pointedEntity);
                repairPtr(e.parent1);
                repairPtr(e.parent2);
                for (auto* m : e.list_MentalModelOfOther)
                    if (m) repairPtr(m->entityPointed);
            }

            // 7. Fix selected entity index
            if (deadIds.count(selectedId)) {
                showEntityWindow = false;
                selectedEntityIndex = -1;
            } else if (selectedId >= 0) {
                selectedEntityIndex = -1;
                for (int j = 0; j < (int)entities.size(); ++j)
                    if (entities[j].entityId == selectedId) { selectedEntityIndex = j; break; }
            }
        }
    }

    // Update free will system periodically (only when not paused)
    if (!isPaused) {
        frameCounter++;
        if(frameCounter >= UPDATE_FREQUENCY){
            frameCounter = 0;

            // Birthday: one year = SimClock::DAYS_PER_YEAR sim-ticks. Fires
            // once per tick (this block runs once per UPDATE_FREQUENCY frames).
            if(g_clock.isYearStartTick() && day > 0){
                for(Entity& ent : entities){
                    ent.IncrementBDay();
                }
            }

            // M11: sub-phase profiler (ASHB_PROFILE=1) — prints a breakdown
            // every 10 ticks so hotspots inside the tick pass are attributable.
            static const bool s_profile = std::getenv("ASHB_PROFILE") != nullptr;
            static double s_msGroups = 0, s_msWill = 0, s_msPersona = 0, s_msCiv = 0;
            static int    s_profTicks = 0;
            auto pf_now = std::chrono::steady_clock::now;
            auto pfA = pf_now();

            // Recalculate entity groups based on current positions
            close_entity_together = getSocialGroups(ent_quad);
            auto pfB = pf_now();

            // Apply free will to all entity groups with current day for context
            applyFreeWill(close_entity_together, day, engineCivilization);
            auto pfC = pf_now();

            // PersonaSystem: update self-grounding every tick; consolidate memories every 10 ticks
            for (Entity& ent : entities) {
                if (ent.entityHealth <= 0.0f) continue;
                ent.updateSelfGrounding(day);
                if (day % 10 == 0) {
                    // Consolidate BEFORE pruning: an episode must get its shot
                    // at becoming a core belief before it can be forgotten.
                    ent.consolidateMemories(day);
                    if (ent.pruneLifeMemories(day))
                        ent.semanticMemory.rebuildFromLifeMemories(&ent);
                    // Phase 6: re-resolve epigenetic expression against how
                    // supportive the entity's current life is (happiness as
                    // the cheap proxy) and re-apply its personality/mood pull.
                    // Same 10-tick cadence as consolidation — this is a slow
                    // drift, not a per-tick force.
                    if (!ent.epigeneticMarkers.empty()) {
                        ent.updateEpigeneticExpression(ent.entityHapiness);
                        ent.applyEpigeneticEffects();
                    }
                }
            }
            auto pfD = pf_now();

            // ── Emergence layer (Upgrade Plan Steps 2-5): item foraging and
            // eating, combinatorial invention, recipe diffusion, episodic-
            // memory- and pheromone-guided displacement, NEAT brains. Runs
            // once per sim-day, before the civ tick so freshly invented
            // recipes are visible to the same day's diffusion/economy passes.
            if (!g_pheromoneField.ready())
                g_pheromoneField.reset((float)width, (float)height);
            if (!g_itemManager.seeded()) g_itemManager.seed();
            g_itemManager.tickAgents(entities, day / UPDATE_FREQUENCY,
                                     (float)width, (float)height,
                                     &g_pheromoneField, globalCivEngine);
            g_pheromoneField.decay(g_liveConfig.pheromoneDecayMul);

            // ── Civilization tick (every TICKS_PER_CIV_TICK days) ──
            if (globalCivEngine && g_clock.isCivTick()) {
                globalCivEngine->tick(entities, day / UPDATE_FREQUENCY);
                // Social order rides the same cadence: refresh classes, clientela
                // and the debt cascade once per civ-day.
                if (globalSocialOrder)
                    globalSocialOrder->tick(entities, globalCivEngine->getCurrentYear());
            }
            if (s_profile) {
                auto pfE = pf_now();
                auto ms = [](auto a, auto b) {
                    return std::chrono::duration<double, std::milli>(b - a).count();
                };
                s_msGroups  += ms(pfA, pfB);
                s_msWill    += ms(pfB, pfC);
                s_msPersona += ms(pfC, pfD);
                s_msCiv     += ms(pfD, pfE);
                if (++s_profTicks % 10 == 0) {
                    std::cout << "PROFILE-TICK avg over " << s_profTicks << ": groups="
                              << s_msGroups / s_profTicks << " will=" << s_msWill / s_profTicks
                              << " persona=" << s_msPersona / s_profTicks
                              << " civ=" << s_msCiv / s_profTicks << " ms\n";
                }
            }

            // ── Economy tick: supply, demand & prices for the whole market ─────
            {
                float warIntensity = 0.0f;
                bool  hasAgriculture = false;
                if (globalCivEngine) {
                    int atWarPop = 0, living = 0;
                    for (const auto& t : globalCivEngine->tribes) {
                        bool atWar = false;
                        for (const auto& s : t.stances)
                            if (s.second == TS_AT_WAR) { atWar = true; break; }
                        if (atWar) atWarPop += t.population();
                    }
                    for (const auto& inv : globalCivEngine->innovations)
                        if (inv.category == "agriculture") { hasAgriculture = true; break; }
                    for (const Entity& e : entities)
                        if (e.entityHealth > 0.0f) living++;
                    if (living > 0)
                        warIntensity = std::min(1.0f, (float)atWarPop / (float)living);
                } else {
                    hasAgriculture = true; // no civ engine -> treat farming as known
                }
                g_market.update(entities, warIntensity, hasAgriculture);
            }

            std::vector<Entity> new_borns = get_new_borns();
            if (!new_borns.empty()) {
                // 1. Snapshot old address → id while every pointer is still
                //    valid. (The old code read entityId THROUGH stale pointers
                //    after reallocation — undefined behaviour — and never
                //    repaired parent or mental-model pointers at all.)
                std::unordered_map<const Entity*, int> ptrToId;
                ptrToId.reserve(entities.size());
                for (const Entity& e : entities) ptrToId[&e] = e.entityId;

                // 2. Grow geometrically so reallocation is rare; when it does
                //    happen, step 4 repairs everything.
                if (entities.capacity() < entities.size() + new_borns.size())
                    entities.reserve(std::max(entities.size() * 2,
                                              entities.size() + new_borns.size()));
                for (Entity& ent : new_borns) entities.push_back(ent);

                // 3. Rebuild ent_quad and id → new-address map.
                ent_quad.clear();
                std::unordered_map<int, Entity*> idToNewPtr;
                idToNewPtr.reserve(entities.size());
                for (Entity& e : entities) {
                    ent_quad.push_back(&e);
                    idToNewPtr[e.entityId] = &e;
                }
                ++g_entQuadVersion;   // invalidate movement lookup caches

                // 4. Repair every Entity* field: old addr → id → new addr.
                auto repairPtr = [&](Entity*& p) {
                    if (!p) return;
                    auto oldIt = ptrToId.find(p);
                    if (oldIt == ptrToId.end()) { p = nullptr; return; }
                    auto newIt = idToNewPtr.find(oldIt->second);
                    p = (newIt != idToNewPtr.end()) ? newIt->second : nullptr;
                };
                for (Entity& e : entities) {
                    for (auto& d : e.list_entityPointedDesire)  repairPtr(d.pointedEntity);
                    for (auto& a : e.list_entityPointedAnger)   repairPtr(a.pointedEntity);
                    for (auto& s : e.list_entityPointedSocial)  repairPtr(s.pointedEntity);
                    for (auto& c : e.list_entityPointedCouple)  repairPtr(c.pointedEntity);
                    repairPtr(e.parent1);
                    repairPtr(e.parent2);
                    for (auto* m : e.list_MentalModelOfOther)
                        if (m) repairPtr(m->entityPointed);
                }
            }
            FreeWillSystem::clear_new_borns();

            // Export state to JSON lines for the HTML viewer. Sampled every
            // 5th tick: per-tick export of the whole population produced
            // ~27 MB per 500 ticks and dominated I/O.
            if (g_clock.isCivTick())
                exportTickHistory("./src/data/tick_history.json", entities, day);
        }
        day++;
    }
}

// Five falsifiable assertions about whether the run looks like a human world.
// Printed after every headless run so a regression in social dynamics is
// caught by eye (or by grepping "REALISM.*FAIL" in CI) instead of going
// unnoticed for months the way the tribes-collapse bug did.
static void printRealismReport(const std::vector<Entity>& entities,
                               const CivilizationEngine* civ, int ticksRun) {
    int totalDeaths = 0, homicides = 0;
    for (const auto& [cause, n] : g_deathLedger) {
        totalDeaths += n;
        if (cause == "homicide") homicides = n;
    }
    long long totalActions = 0; int topCount = 0; std::string topAction = "-";
    for (const auto& [name, n] : g_actionTally) {
        totalActions += n;
        if (n > topCount) { topCount = n; topAction = name; }
    }
    int births = civ ? civ->totalBirths : 0;
    int tribesAlive = civ ? (int)civ->tribes.size() : 0;
    int viableReligions = 0;
    if (civ)
        for (const auto& r : civ->religions)
            if ((int)r.followerIds.size() >= 3) ++viableReligions;
    float homicideShare = totalDeaths > 0 ? (float)homicides / totalDeaths : 0.0f;
    float topShare      = totalActions > 0 ? (float)topCount / (float)totalActions : 0.0f;
    // Wars scaled to the plan's target window of 1-5 per 1500 days.
    float warsPer1500 = civ && ticksRun > 0
                        ? (float)civ->totalWarsDeclared * 1500.0f / (float)ticksRun : 0.0f;

    auto verdict = [](bool ok) { return ok ? "PASS" : "FAIL"; };
    std::cout << "\n===== REALISM REPORT (" << ticksRun << " ticks) =====\n";
    // Share-of-deaths misleads in young booming populations (nobody is old
    // enough to die naturally, so the denominator is tiny). A world is
    // non-violent if EITHER the share is low or killings are rare per capita
    // (proxied per birth, which tracks population-years lived).
    float homicidePerBirth = births > 0 ? (float)homicides / (float)births : 0.0f;
    bool  homicideOk = homicideShare < 0.15f || homicidePerBirth < 0.04f;
    std::cout << "1. Violence rare (share<15% or /birth<4%): " << (int)(homicideShare * 100)
              << "% share, " << (int)(homicidePerBirth * 100)
              << "%/birth  [" << verdict(homicideOk) << "]\n";
    std::cout << "2. Population sustains (births>=deaths): " << births << " births / "
              << totalDeaths << " deaths  [" << verdict(births >= totalDeaths) << "]\n";
    // Rebalanced alongside the raised war line: with ~150 micro-tribes a war
    // somewhere every few weeks is the intent, so the ceiling now guards only
    // against the old 860-war endemic (~260/1500d), not against war happening.
    // It's still a CROSS-SEED statistic; a single short run legitimately sees
    // zero, so only demand w>0 once the run is long enough that silence would
    // itself be suspicious.
    bool warsOk = (warsPer1500 <= 90.0f) && (ticksRun < 1000 || warsPer1500 > 0.0f);
    std::cout << "3. Wars occur, world survives (w/1500d<=90"
              << (ticksRun >= 1000 ? ", >0" : "") << "): " << warsPer1500
              << "  [" << verdict(warsOk) << "]\n";
    // Faith count scales with how many people there are to convert: 8 is the
    // floor of the allowance, one more faith allowed per 60 living souls.
    int faithAllowance = std::max(8, (int)entities.size() / 60 + 7);
    std::cout << "4. Tribes persist, faiths stabilise (t>=2, 1<=r<=" << faithAllowance
              << "): tribes=" << tribesAlive << " religions=" << viableReligions
              << "  [" << verdict(tribesAlive >= 2 && viableReligions >= 1
                                  && viableReligions <= faithAllowance) << "]\n";
    std::cout << "5. No behavioral monoculture (top action < 25%): " << topAction << " "
              << (int)(topShare * 100) << "%  [" << verdict(topShare < 0.25f) << "]\n";

    // ── E2: realism metrics v2 (AI upgrade). The three new lines assert only
    // on runs long enough for the phenomena to develop; below that they are
    // informational. `warn` is lowercase on purpose — CI greps "REALISM.*FAIL".
    auto softVerdict = [&](bool ok, bool assertable) {
        return assertable ? (ok ? "PASS" : "FAIL") : (ok ? "PASS" : "warn");
    };
    // 6. Emotional lives: fear, joy AND regret should all have occurred — a
    // world with no felt emotion episodes is running on autopilot.
    bool emoOk = g_mindStats.fearEpisodes > 0 && g_mindStats.joyEpisodes > 0 &&
                 g_mindStats.regrets > 0;
    std::cout << "6. Emotional lives (fear/joy/regret all occur): fear="
              << g_mindStats.fearEpisodes << " joy=" << g_mindStats.joyEpisodes
              << " regret=" << g_mindStats.regrets << " relief=" << g_mindStats.reliefs
              << " guilt=" << g_mindStats.guiltEpisodes
              << "  [" << softVerdict(emoOk, ticksRun >= 200) << "]\n";
    // 7. Belief ecology: people should know things, mostly-but-not-perfectly
    // true (1.0 = omniscient parrots, <0.5 = a world run on lies).
    float acc = mind::populationBeliefAccuracy(entities);
    bool beliefOk = acc < 0.0f || (acc >= 0.5f && acc <= 0.995f);
    std::cout << "7. Belief ecology (0.5<=accuracy<=0.995): accuracy="
              << (acc < 0.0f ? std::string("no facts yet") : std::to_string(acc))
              << " shared=" << g_mindStats.factsShared << " lies=" << g_mindStats.liesTold
              << "  [" << softVerdict(beliefOk, ticksRun >= 300) << "]\n";
    // 8. Skill differentiation: practice curves should spread the population
    // (Gini > 0.03) — everyone identical means learning-by-doing is dead.
    float gini = mind::skillGini(entities);
    std::cout << "8. Skills differentiate (best-skill Gini > 0.03): gini=" << gini
              << " deepSleeps=" << g_mindStats.deepSleeps
              << "  [" << softVerdict(gini > 0.03f, ticksRun >= 300) << "]\n";
    // 9. Deception (B3): rare and optional by design (low-integrity + grudge
    // gated) — never a CI failure, purely informational on how often faked
    // warmth gets staged and later detected.
    std::cout << "9. Deception (informational): attempted="
              << g_mindStats.deceptionsAttempted << " detected="
              << g_mindStats.deceptionsDetected
              << "  [" << (g_mindStats.deceptionsAttempted > 0 ? "PASS" : "warn") << "]\n";
    // 10. II-P2: institutions should exist and hold knowledge nobody's skull
    // does. `archived` counts techniques written down; `saves` counts times an
    // archive carried one through a collapse that would otherwise have erased
    // it — the ratchet doing its job. Informational below 400 ticks: a people
    // needs writing and a scholar before it can found a school at all.
    if (civ) {
        size_t archived = 0;
        for (const auto& inst : civ->institutions.all()) archived += inst->archive.size();
        std::cout << "10. Institutions store knowledge (informational): live="
                  << civ->institutions.getInstitutionCount()
                  << " founded=" << civ->totalInstitutions
                  << " archived=" << archived
                  << " saves=" << civ->totalArchiveSaves
                  << "  [" << softVerdict(civ->totalInstitutions > 0, ticksRun >= 400) << "]\n";
        // 11. I-P3: do lives leave marks that outlast them? `notable` counts
        // lives the world still carries a mark of; `persisting` is the plan's
        // acceptance bar — marks still standing at least two generations after
        // the person died (a founder whose house is still running, a maker
        // whose work is still named). `deepest` is the longest bloodline.
        int deepest = 0, persisting = 0;
        if (globalKinship) {
            for (const auto& fam : globalKinship->families) {
                deepest = std::max(deepest, fam.generation);
                if (fam.founderId < 0 || fam.generation < 3) continue;
                for (const auto& l : civ->legacies)
                    if (l.entityId == fam.founderId && !l.marks.empty()) { ++persisting; break; }
            }
        }
        // 12. III-P2's acceptance test, measured rather than asserted: if trade
        // does what trade does, two peoples joined by a road should disagree
        // LESS about what food is worth than two who are not. `linked` and
        // `unlinked` are mean absolute price-index gaps; linked < unlinked is
        // arbitrage working. Needs at least one route and one unlinked pair to
        // mean anything.
        {
            float linkSum = 0.0f, freeSum = 0.0f;
            int   linkN = 0, freeN = 0;
            const auto& tr = civ->tribes;
            for (size_t i = 0; i < tr.size(); ++i)
                for (size_t j = i + 1; j < tr.size(); ++j) {
                    bool linked = false;
                    for (const auto& r : civ->tradeRoutes)
                        if (r.active && r.a == std::min(tr[i].id, tr[j].id)
                                     && r.b == std::max(tr[i].id, tr[j].id)) { linked = true; break; }
                    float gap = std::abs(tr[i].priceFood - tr[j].priceFood);
                    if (linked) { linkSum += gap; ++linkN; } else { freeSum += gap; ++freeN; }
                }
            float linkAvg = linkN ? linkSum / linkN : -1.0f;
            float freeAvg = freeN ? freeSum / freeN : -1.0f;
            // `<=` with a hair of tolerance, because the success case saturates:
            // a world criss-crossed by 2,500 caravan trips has arbitraged every
            // gap it can reach down to zero, and zero is not less than zero.
            bool ok = (linkN > 0 && freeN > 0 && linkAvg <= freeAvg + 1e-6f);
            std::cout << "11. Lives leave marks (informational): notable="
                      << civ->totalNotableLives
                      << " works=" << civ->greatWorks.size()
                      << " memorials=" << civ->totalMemorials
                      << " deepestLine=" << deepest
                      << " persisting>=2gen=" << persisting
                      << "  [" << softVerdict(civ->totalNotableLives > 0, ticksRun >= 400) << "]\n";
            std::cout << "12. Trade narrows price gaps (linked < unlinked): linked="
                      << (linkN ? std::to_string(linkAvg) : std::string("n/a"))
                      << " unlinked=" << (freeN ? std::to_string(freeAvg) : std::string("n/a"))
                      << " routes=" << civ->totalRoutesOpened
                      << " cut=" << civ->totalRoutesCut
                      << " caravans=" << civ->totalCaravans
                      // Both halves of the comparison have to exist. A world
                      // whose surviving peoples are ALL joined by roads has no
                      // unlinked pair left to compare against — that is trade
                      // having won, not arbitrage having failed.
                      << "  [" << softVerdict(ok, ticksRun >= 400 && linkN > 0 && freeN > 0) << "]\n";
        }
        // 13. II-P4: is war consequential and plural? Four things at once —
        // war actually kills (deaths as a share of all deaths, in a rough
        // historical band rather than the bloodless ritual of the flagship
        // run), maps change hands (conquests), polities assemble and come
        // apart (empires), and the deepest standing feud shows blood debt
        // outliving the people who first drew it.
        {
            float warShare = totalDeaths > 0
                           ? (float)civ->totalWarDeaths / (float)totalDeaths : 0.0f;
            float deepestFeud = 0.0f;
            for (const auto& t : civ->tribes)
                for (const auto& g : t.grievance)
                    deepestFeud = std::max(deepestFeud, g.second);
            // Largest polity actually held: how many peoples the biggest
            // overlord rules right now. This is what "empire" is measured on.
            int maxVassals = 0;
            for (const auto& o : civ->tribes) {
                int held = 0;
                for (const auto& v : civ->tribes) if (v.overlordTribeId == o.id) ++held;
                maxVassals = std::max(maxVassals, held);
            }
            // The band has a ceiling as well as a floor. §8 asks for a
            // HISTORICAL share, and the first cut of II-P4 only checked that
            // war killed anybody at all — under which a world where war
            // accounts for half of all deaths and grinds the population from
            // 217 down to 19 reads as a pass. Even the most warlike societies
            // ethnographers have measured lose well under a third of their dead
            // to violence; above that the sim is not modelling history, it is
            // modelling an extinction event.
            bool ok = civ->totalWarDeaths > 0 && civ->totalConquests > 0
                      && (totalDeaths < 40 || warShare <= 0.30f);
            std::cout << "13. War is consequential (deaths>0, conquests>0, share<=30%): warDead="
                      << civ->totalWarDeaths
                      << " share=" << (int)(warShare * 100) << "%"
                      << " conquests=" << civ->totalConquests
                      << " empires=" << civ->totalEmpires
                      << "/" << civ->totalEmpiresFallen << " fell"
                      << " maxVassals=" << maxVassals
                      << " deepestFeud=" << (int)deepestFeud
                      << "  [" << softVerdict(ok, ticksRun >= 400) << "]\n";
        }
        // 14. II-P3's claim, measured: political stress and how well ordinary
        // people are living should move in ANTI-PHASE across a long run — when
        // life at the bottom degrades and elites pile up, stress rises; strife
        // discharges it and the cycle turns. A negative correlation is the
        // signature; a flat or positive one means the cycle is not running.
        {
            // Measured on the DETRENDED series, which is what "oscillate in
            // anti-phase" means and what the raw correlation cannot see. Over a
            // long run both quantities carry a trend — a world that grows
            // richer AND more crowded raises average wellbeing and average
            // political stress together — and that shared drift swamps the
            // cycle riding on top of it, reporting a positive correlation for a
            // world whose cycles are textbook. Subtract a moving average from
            // each and correlate what is left: the swings, which are the claim.
            const auto& h = civ->cycleHistory;
            float corr = 0.0f;
            if (h.size() >= 20) {
                const int W = 15;                       // half-window, in samples
                std::vector<double> rw, ri;
                for (size_t i = 0; i < h.size(); ++i) {
                    const size_t lo = (i > (size_t)W) ? i - W : 0;
                    const size_t hi = std::min(h.size() - 1, i + W);
                    double sw = 0, si = 0;
                    for (size_t k = lo; k <= hi; ++k) { sw += h[k].wellbeing; si += h[k].instability; }
                    const double n = (double)(hi - lo + 1);
                    rw.push_back(h[i].wellbeing   - sw / n);
                    ri.push_back(h[i].instability - si / n);
                }
                double mw = 0, mi = 0;
                for (size_t i = 0; i < rw.size(); ++i) { mw += rw[i]; mi += ri[i]; }
                mw /= rw.size(); mi /= ri.size();
                double cov = 0, vw = 0, vi = 0;
                for (size_t i = 0; i < rw.size(); ++i) {
                    double dw = rw[i] - mw, di = ri[i] - mi;
                    cov += dw * di; vw += dw * dw; vi += di * di;
                }
                if (vw > 1e-9 && vi > 1e-9) corr = (float)(cov / std::sqrt(vw * vi));
            }
            bool ok = (h.size() >= 20 && corr < 0.0f && civ->totalStrifes > 0);
            float mWell = 0, mInst = 0, mElite = 0, peakInst = 0, mOver = 0;
            for (const auto& s : h) {
                mWell += s.wellbeing; mInst += s.instability; mElite += s.elites; mOver += s.overshoot;
                peakInst = std::max(peakInst, s.instability);
            }
            if (!h.empty()) { mWell /= h.size(); mInst /= h.size(); mElite /= h.size(); mOver /= h.size(); }
            std::cout << "14. Secular cycle runs (wellbeing vs instability anti-phase): corr="
                      << (h.size() >= 20 ? std::to_string(corr) : std::string("n/a"))
                      << " strifes=" << civ->totalStrifes
                      << " meanWellbeing=" << (int)mWell
                      << " meanInstability=" << (int)mInst
                      << " peakInstability=" << (int)peakInst
                      << " eliteRatio=" << mElite
                      << " overshoot=" << mOver
                      << " samples=" << h.size()
                      << "  [" << softVerdict(ok, ticksRun >= 800) << "]\n";
        }
        // 15. III-P4's acceptance test. Three things at once:
        //  • top10Share — what fraction of all wealth the richest tenth hold.
        //    A Pareto-shaped world sits near 40-70%; an even split is ~10%.
        //  • eliteLineages — how concentrated the elite is by family: the share
        //    of patricians drawn from the most over-represented houses. If
        //    advantage is heritable, elite families recur.
        //  • mobility — the share of living adults whose class differs from the
        //    parent they inherited from. The plan's bar is that this is
        //    NONZERO (rank is not caste) but well BELOW random mixing.
        {
            std::vector<float> w;
            for (const auto& e : entities)
                if (e.entityHealth > 0.0f) w.push_back(std::max(0.0f, e.salary.token));
            std::sort(w.begin(), w.end());
            float total = 0.0f, top = 0.0f;
            for (size_t i = 0; i < w.size(); ++i) {
                total += w[i];
                if (i >= w.size() - w.size() / 10) top += w[i];
            }
            float topShare = (total > 0.0f) ? top / total : 0.0f;

            std::map<int, int> eliteByFamily;
            int elites = 0, moved = 0, comparable = 0;
            for (const auto& e : entities) {
                if (e.entityHealth <= 0.0f) continue;
                if (e.socialClass == CLASS_PATRICIAN) { ++elites; ++eliteByFamily[e.familyId]; }
                if (e.entityAge < 18.0f) continue;
                // Compare against the parent we can still find.
                for (int pid : { e.parent1Id, e.parent2Id }) {
                    if (pid < 0) continue;
                    for (const auto& p : entities)
                        if (p.entityId == pid) {
                            ++comparable;
                            if (p.socialClass != e.socialClass) ++moved;
                            break;
                        }
                    break;
                }
            }
            int biggest = 0;
            for (const auto& kv : eliteByFamily) biggest = std::max(biggest, kv.second);
            float eliteConc = elites > 0 ? (float)biggest / (float)elites : 0.0f;
            float mobility  = comparable > 0 ? (float)moved / (float)comparable : -1.0f;
            bool ok = (topShare > 0.25f) && (mobility >= 0.0f && mobility < 0.5f);
            std::cout << "15. Class is heritable & sticky (top10>25%, 0<=mobility<50%): top10Share="
                      << (int)(topShare * 100) << "%"
                      << " elites=" << elites
                      << " largestEliteHouse=" << (int)(eliteConc * 100) << "%"
                      << " mobility=" << (mobility < 0 ? std::string("n/a")
                                                       : std::to_string((int)(mobility * 100)) + "%")
                      << " (" << comparable << " child/parent pairs)"
                      // A mobility RATE needs a denominator worth dividing by.
                      // It is measured over adults with a parent still living,
                      // which in a world of a couple of hundred souls can be a
                      // dozen pairs — and one patrician with four grown
                      // children then reads as "class is not heritable". Below
                      // thirty pairs the number is reported, not asserted on.
                      << "  [" << softVerdict(ok, ticksRun >= 600 && comparable >= 30) << "]\n";
        }
        // 16. IV-P1's acceptance test: is culture content that diverges and
        // tips? Two measurements, neither of which a scalar `cultureScore` can
        // produce at all:
        //  • meanDistance — how unlike each other the world's peoples actually
        //    are, averaged over every pair (0 = one global monoculture, 1 = no
        //    two peoples share a single practice). This is the plan's "distinct
        //    trait sets" bar: transmission that ran everywhere at once would
        //    drive this to 0, and traits that never travelled would pin it at 1.
        //    `distanceCorr` (map distance vs cultural distance) and `far`/`near`
        //    (across landmasses vs within one) report the geographic texture,
        //    but are informational: tribes migrate, split and colonise, so on a
        //    single crowded cradle they are dominated by descent rather than by
        //    who lives where.
        //  • cascades — how often a trait crossed Centola's 25% critical mass.
        //    Cascades alongside fizzles is the punctuated dynamic: novelties
        //    mostly die quietly, and the survivors take a whole people quickly.
        {
            double farSum = 0.0, nearSum = 0.0;
            int    farN = 0, nearN = 0;
            std::vector<std::pair<double, double>> pairs;   // (map distance, cultural distance)
            unsigned long long worldTraits = 0ull;
            const auto& tr = civ->tribes;
            for (size_t i = 0; i < tr.size(); ++i) {
                worldTraits |= tr[i].knownTraits;
                for (size_t j = i + 1; j < tr.size(); ++j) {
                    if (tr[i].cultureTraits == 0ull || tr[j].cultureTraits == 0ull) continue;
                    double d = environment::CulturalTransmissionSystem::distance(
                                   tr[i].cultureTraits, tr[j].cultureTraits);
                    if (tr[i].regionId == tr[j].regionId) { nearSum += d; ++nearN; }
                    else                                  { farSum  += d; ++farN;  }
                    double dx = tr[i].centerX - tr[j].centerX, dy = tr[i].centerY - tr[j].centerY;
                    pairs.push_back({ std::sqrt(dx * dx + dy * dy), d });
                }
            }
            double corr = 0.0;
            if (pairs.size() >= 10) {
                double mg = 0, mc = 0;
                for (const auto& p : pairs) { mg += p.first; mc += p.second; }
                mg /= pairs.size(); mc /= pairs.size();
                double cov = 0, vg = 0, vc = 0;
                for (const auto& p : pairs) {
                    double dg = p.first - mg, dc = p.second - mc;
                    cov += dg * dc; vg += dg * dg; vc += dc * dc;
                }
                if (vg > 1e-9 && vc > 1e-9) corr = cov / std::sqrt(vg * vc);
            }
            float farAvg  = farN  ? (float)(farSum  / farN)  : -1.0f;
            float nearAvg = nearN ? (float)(nearSum / nearN) : -1.0f;
            double meanDist = 0.0;
            for (const auto& p : pairs) meanDist += p.second;
            if (!pairs.empty()) meanDist /= pairs.size();
            // One people cannot be culturally unlike anybody; there the line
            // reports and warns rather than failing on a world too small to
            // have a cultural geography at all.
            const bool measurable = pairs.size() >= 3;
            // Traits dying out is the other half of the punctuated dynamic, but
            // a short run legitimately hasn't buried one yet — only demand it
            // once the world has had time to forget something.
            bool ok = meanDist > 0.15 && civ->totalCascades > 0
                      && (ticksRun < 600 || civ->totalFizzles > 0);
            std::cout << "16. Culture diverges & tips (peoples differ, traits rise & die): meanDistance="
                      << (measurable ? std::to_string(meanDist) : std::string("n/a"))
                      << " distanceCorr="
                      << (pairs.size() >= 10 ? std::to_string(corr) : std::string("n/a"))
                      << " pairs=" << pairs.size()
                      << " far=" << (farN  ? std::to_string(farAvg)  : std::string("n/a"))
                      << " near=" << (nearN ? std::to_string(nearAvg) : std::string("n/a"))
                      << " traitsAlive="
                      << environment::CulturalTransmissionSystem::count(worldTraits)
                      << " invented=" << civ->totalTraitsInvented
                      << " cascades=" << civ->totalCascades
                      << " fizzles="  << civ->totalFizzles
                      << " diffused=" << civ->totalTraitsDiffused
                      << "  [" << softVerdict(ok, ticksRun >= 400 && measurable) << "]\n";
        }
            // 17. IV-P2's acceptance test: do congregations actually behave
            // differently? A first cut compared the share of *aggressive* acts
            // between the two largest faiths, which measured almost nothing —
            // violence is 0.04% of all action in this world, so the comparison
            // was pure noise. The honest measurement is the whole action mix:
            // total-variation distance between the two congregations' action
            // distributions (0 = they do exactly the same things in the same
            // proportions, 1 = no overlap at all), plus the devotional share,
            // which is what the afterlife/asceticism axes bear on directly and
            // is common enough to read.
            {
                std::vector<std::pair<int, long>> byFaith;
                for (const auto& kv : g_faithActionTally) {
                    if (kv.first < 0) continue;              // skip the unbelieving
                    long n = 0;
                    for (const auto& a : kv.second) n += a.second;
                    if (n >= 200) byFaith.push_back({ kv.first, n });   // enough to compare
                }
                std::sort(byFaith.begin(), byFaith.end(),
                          [](const auto& a, const auto& b){ return a.second > b.second; });
                auto devotionalShare = [&](int rid) -> float {
                    const auto& t = g_faithActionTally[rid];
                    long tot = 0, dev = 0;
                    for (const auto& a : t) {
                        tot += a.second;
                        if (a.first == "Prayer" || a.first == "Preach" || a.first == "Meditate")
                            dev += a.second;
                    }
                    return tot > 0 ? (float)dev / (float)tot : 0.0f;
                };
                float tv = -1.0f, dA = 0.0f, dB = 0.0f;
                if (byFaith.size() > 1) {
                    const auto& A = g_faithActionTally[byFaith[0].first];
                    const auto& B = g_faithActionTally[byFaith[1].first];
                    long nA = byFaith[0].second, nB = byFaith[1].second;
                    std::set<std::string> names;
                    for (const auto& a : A) names.insert(a.first);
                    for (const auto& b : B) names.insert(b.first);
                    double sum = 0.0;
                    for (const auto& n : names) {
                        double pa = A.count(n) ? (double)A.at(n) / nA : 0.0;
                        double pb = B.count(n) ? (double)B.at(n) / nB : 0.0;
                        sum += std::abs(pa - pb);
                    }
                    tv = (float)(sum * 0.5);
                    dA = devotionalShare(byFaith[0].first);
                    dB = devotionalShare(byFaith[1].first);
                }
                bool ok = (tv > 0.05f) && civ->totalRites > 0;
                std::cout << "17. Faiths differ in conduct (action-mix distance > 0.05): congregations="
                          << byFaith.size()
                          << " actionMixDistance=" << (tv < 0 ? std::string("n/a") : std::to_string(tv))
                          << " devotionalA=" << (dA * 100) << "%"
                          << " devotionalB=" << (dB * 100) << "%"
                          << " rites=" << civ->totalRites
                          << " schisms=" << civ->totalSchisms
                          << "  [" << softVerdict(ok, ticksRun >= 600) << "]\n";
            }
            // 18. IV-P3's acceptance test: does a language boundary actually
            // slow things down? Diffusion RATE (transfers per contact
            // opportunity) inside a shared tongue against the rate across an
            // opaque one. Comparing rates and not raw counts matters — most
            // pairs share a tongue, so counts alone would say nothing. Also
            // reports the mean intelligibility across the world's peoples and
            // how many pairs have creolised into mutual understanding.
            {
                double rSame  = civ->diffusionOpportunitySame > 0
                    ? (double)civ->diffusionSameTongue / civ->diffusionOpportunitySame : -1.0;
                double rCross = civ->diffusionOpportunityCross > 0
                    ? (double)civ->diffusionCrossTongue / civ->diffusionOpportunityCross : -1.0;
                float meanIntel = -1.0f;
                {
                    double sum = 0.0; int n = 0;
                    const auto& tr = civ->tribes;
                    for (size_t i = 0; i < tr.size(); ++i)
                        for (size_t j = i + 1; j < tr.size(); ++j) {
                            sum += civ->mutualIntelligibility(tr[i], tr[j]); ++n;
                        }
                    if (n) meanIntel = (float)(sum / n);
                }
                bool ok = (rSame >= 0.0 && rCross >= 0.0 && rSame > rCross);
                std::cout << "18. Language gates diffusion (same-tongue rate > cross-tongue): same="
                          << (rSame  < 0 ? std::string("n/a") : std::to_string(rSame))
                          << " cross=" << (rCross < 0 ? std::string("n/a") : std::to_string(rCross))
                          << " meanIntelligibility="
                          << (meanIntel < 0 ? std::string("n/a") : std::to_string(meanIntel))
                          << " creolisations=" << civ->totalCreolisations
                          << "  [" << softVerdict(ok, ticksRun >= 600) << "]\n";
            }

        // ── §8: Realism Report v3 — the cross-cutting assertions ─────────────
        // Everything above measures one phase's own claim. These five measure
        // the SHAPES the plan says a world resembling Earth has to have,
        // regardless of which phase produced them: how people die, how many
        // people they know, how their settlements are sized, what they fight
        // over, and whether what they know goes backwards.
        // See plans/parallel-earth-upgrade.md §8.

        // 19. Demography: a human lifespan distribution is not a bell curve. It
        // has a hump of deaths at the very start of life and a long right tail
        // of people who got through it — infant mortality plus survivorship. A
        // world where nobody dies young, or where nobody reaches old age, has
        // the wrong shape whatever its mean says.
        {
            std::vector<float> ages = g_deathAges;
            std::sort(ages.begin(), ages.end());
            float infantShare = 0.0f, elderShare = 0.0f, median = -1.0f, mean = 0.0f;
            if (!ages.empty()) {
                int young = 0, old = 0;
                for (float a : ages) { if (a < 5.0f) ++young; if (a >= 60.0f) ++old; mean += a; }
                infantShare = (float)young / ages.size();
                elderShare  = (float)old   / ages.size();
                mean       /= ages.size();
                median      = ages[ages.size() / 2];
            }
            // Deaths in childhood happen but do not dominate, and a meaningful
            // share of people live long enough to grow old.
            bool ok = ages.size() >= 20 && infantShare > 0.01f && infantShare < 0.45f
                      && elderShare > 0.05f;
            std::cout << "19. Lifespans have a human shape (infant hump 1-45%, elders >5%): infant<5="
                      << (int)(infantShare * 100) << "%"
                      << " elders60+=" << (int)(elderShare * 100) << "%"
                      << " median=" << (median < 0 ? std::string("n/a") : std::to_string((int)median))
                      << " mean=" << (int)mean
                      << " deaths=" << ages.size()
                      << "  [" << softVerdict(ok, ticksRun >= 600 && ages.size() >= 20) << "]\n";
        }

        // 20. Networks: Dunbar's ceiling is a fact about human cognition, and a
        // social graph that ignores it is a graph of address books, not
        // friendships. Degrees should also be *unequal* — most people hold a
        // modest circle, a few are hubs — which is what a log-normal-ish spread
        // looks like from the outside.
        {
            std::vector<int> degree;
            for (const auto& e : entities) {
                if (e.entityHealth <= 0.0f) continue;
                int d = 0;
                for (const auto& s : e.list_entityPointedSocial)
                    if (s.social > 5.0f) ++d;
                degree.push_back(d);
            }
            std::sort(degree.begin(), degree.end());
            int   maxDeg = degree.empty() ? 0 : degree.back();
            float medDeg = degree.empty() ? 0.0f : (float)degree[degree.size() / 2];
            float meanDeg = 0.0f;
            for (int d : degree) meanDeg += d;
            if (!degree.empty()) meanDeg /= degree.size();
            double var = 0.0;
            for (int d : degree) var += (d - meanDeg) * (d - meanDeg);
            float sd = degree.empty() ? 0.0f : (float)std::sqrt(var / degree.size());
            bool ok = !degree.empty() && maxDeg <= 150 && sd > 1.0f;
            std::cout << "20. Social circles are Dunbar-capped & uneven (max<=150, spread>1): max="
                      << maxDeg << " median=" << (int)medDeg << " mean=" << meanDeg
                      << " sd=" << sd << " people=" << degree.size()
                      << "  [" << softVerdict(ok, ticksRun >= 300) << "]\n";
        }

        // 21. Zipf: settlement sizes in every studied human society fall on a
        // rank-size line — the second city about half the first, the third a
        // third of it. Fitting log(size) against log(rank) and reading off the
        // slope and how tightly the points sit on it is the standard test, and
        // it is emergent here: nothing assigns sizes, migration and surplus do.
        {
            std::vector<int> sizes;
            for (const auto& t : civ->tribes)
                if (t.population() > 0) sizes.push_back(t.population());
            std::sort(sizes.begin(), sizes.end(), std::greater<int>());
            double slope = 0.0, r2 = -1.0;
            if (sizes.size() >= 5) {
                double mx = 0, my = 0;
                const int n = (int)sizes.size();
                std::vector<double> X(n), Y(n);
                for (int i = 0; i < n; ++i) {
                    X[i] = std::log((double)(i + 1));
                    Y[i] = std::log((double)sizes[i]);
                    mx += X[i]; my += Y[i];
                }
                mx /= n; my /= n;
                double sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < n; ++i) {
                    sxy += (X[i] - mx) * (Y[i] - my);
                    sxx += (X[i] - mx) * (X[i] - mx);
                    syy += (Y[i] - my) * (Y[i] - my);
                }
                if (sxx > 1e-9 && syy > 1e-9) {
                    slope = sxy / sxx;
                    r2    = (sxy * sxy) / (sxx * syy);
                }
            }
            bool ok = r2 >= 0.75 && slope < -0.2 && slope > -2.5;
            std::cout << "21. Settlement sizes fit a rank-size law (R^2>=0.75, slope -0.2..-2.5): slope="
                      << (r2 < 0 ? std::string("n/a") : std::to_string(slope))
                      << " R^2=" << (r2 < 0 ? std::string("n/a") : std::to_string(r2))
                      << " settlements=" << sizes.size()
                      << " largest=" << (sizes.empty() ? 0 : sizes[0])
                      << "  [" << softVerdict(ok, ticksRun >= 600 && sizes.size() >= 5) << "]\n";
        }

        // 22. Conflict is plural: real wars are fought over faith, land, food,
        // tribute and borders, and a world that only ever fights one of those
        // has a single mechanism wearing five names.
        {
            int kinds = 0, total = 0;
            for (int i = 0; i < 5; ++i) {
                total += civ->warsByReason[i];
                if (civ->warsByReason[i] > 0) ++kinds;
            }
            bool ok = kinds >= 4;
            std::cout << "22. Wars have plural causes (>=4 kinds): kinds=" << kinds
                      << " ethnic="   << civ->warsByReason[WAR_ETHNIC]
                      << " conquest=" << civ->warsByReason[WAR_CONQUEST]
                      << " resource=" << civ->warsByReason[WAR_RESOURCE]
                      << " tribute="  << civ->warsByReason[WAR_TRIBUTE]
                      << " border="   << civ->warsByReason[WAR_BORDER]
                      << " declared=" << total
                      << "  [" << softVerdict(ok, ticksRun >= 800 && total >= 8) << "]\n";
        }

        // 23. The knowledge ratchet (II-P1's central claim, measured over the
        // whole run rather than at the end): before writing, a collapse can take
        // techniques back out of the world; after it, the archive holds and what
        // is known must never fall. `dropsAfterWriting` is the number of times
        // it did anyway — the one number that has to be zero. Affect rides along
        // here too: the stress people die carrying should be spread, not piled
        // at the ceiling.
        {
            // Two kinds of loss, and only one of them is a broken ratchet. A
            // fall that lands in the same window as a COLLAPSE is the dark age
            // eating the library — exactly what writing and its archives are
            // supposed to prevent, so after literacy it must never happen. A
            // fall with no collapse behind it is a people dying out and taking
            // a craft nobody else practised with it: a real way to lose
            // knowledge, and not a failure of the ratchet.
            int dropsBefore = 0, dropsAfter = 0, attrition = 0, peak = 0;
            bool seenLiterate = false;
            const auto& h = civ->knowledgeHistory;
            for (size_t i = 1; i < h.size(); ++i) {
                if (h[i - 1].literate) seenLiterate = true;
                if (h[i].techCount < h[i - 1].techCount) {
                    const bool collapsed = h[i].darkAges > h[i - 1].darkAges;
                    if (!collapsed)        ++attrition;
                    else if (seenLiterate) ++dropsAfter;
                    else                   ++dropsBefore;
                }
                peak = std::max(peak, h[i].techCount);
            }
            float pinned = 0.0f, sdStress = 0.0f;
            if (!g_deathStress.empty()) {
                int atCeiling = 0; double m = 0;
                for (float s : g_deathStress) { if (s >= 95.0f) ++atCeiling; m += s; }
                m /= g_deathStress.size();
                double v = 0;
                for (float s : g_deathStress) v += (s - m) * (s - m);
                sdStress = (float)std::sqrt(v / g_deathStress.size());
                pinned   = (float)atCeiling / g_deathStress.size();
            }
            bool ok = dropsAfter == 0 && pinned < 0.5f
                      && (g_deathStress.size() < 20 || sdStress > 5.0f);
            std::cout << "23. Knowledge ratchets & nobody dies pinned (collapse-drops-after-writing=0, pinned<50%): drops="
                      << dropsAfter << " (before writing " << dropsBefore
                      << ", lost with their last keepers " << attrition << ")"
                      << " peakTech=" << peak
                      << " darkAges=" << civ->darkAgeCount
                      << " diedAtStress100=" << (int)(pinned * 100) << "%"
                      << " stressSd=" << sdStress
                      << "  [" << softVerdict(ok, ticksRun >= 600) << "]\n";
        }

        // 24. III-P3's acceptance test, both halves of it:
        //  • The demographic transition — birth RATES (births per thousand
        //    population-days of fertile adults, so a city is not "more fertile"
        //    merely for being bigger) must fall with wealth and with
        //    urbanisation. This is the check that separates a modernising
        //    society from an early one, and it is the reason late-run
        //    demography stops looking like the opening centuries.
        //  • The labour market — the specialist share must rise with settlement
        //    size (a camp cannot feed a scholar; a city cannot function without
        //    one), and posts should be filled through weak ties and taught
        //    through guilds rather than handed out by rank.
        {
            auto rate = [](long births, double popDays) {
                return popDays > 0.0 ? (double)births * 1000.0 / popDays : -1.0;
            };
            double richRate = rate(civ->birthsRich, civ->popDaysRich);
            double poorRate = rate(civ->birthsPoor, civ->popDaysPoor);
            long   urbanB = 0, ruralB = 0;
            double urbanD = 0.0, ruralD = 0.0;
            for (int t = 0; t < 5; ++t) {
                if (t >= 2) { urbanB += civ->birthsByTier[t]; urbanD += civ->popDaysByTier[t]; }
                else        { ruralB += civ->birthsByTier[t]; ruralD += civ->popDaysByTier[t]; }
            }
            double urbanRate = rate(urbanB, urbanD), ruralRate = rate(ruralB, ruralD);
            auto share = [&](int t) {
                return civ->specialistSamplesByTier[t] > 0
                     ? civ->specialistShareByTier[t] / (double)civ->specialistSamplesByTier[t]
                     : -1.0;
            };
            double campShare = share(0), townShare = -1.0;
            {   // the biggest settlement class this world actually built
                double sum = 0.0; long n = 0;
                for (int t = 2; t < 5; ++t)
                    if (civ->specialistSamplesByTier[t] > 0) {
                        sum += civ->specialistShareByTier[t];
                        n   += civ->specialistSamplesByTier[t];
                    }
                if (n > 0) townShare = sum / (double)n;
            }
            // What is asserted and what is merely reported, and why. The
            // wealth gradient is measured WITHIN each people (every parent
            // against their own tribe's mean), so prosperity, food, safety and
            // partner availability are held constant and what remains is the
            // transition itself — that one is the test. The urban/rural
            // comparison is across peoples, where a town is a town precisely
            // BECAUSE it is thriving, so its higher birth rate is that
            // prosperity showing through rather than the transition failing;
            // it is printed for the reader and not asserted on. And the
            // gradient is a late-run claim by construction: a world too poor to
            // have produced a wealth spread has nothing to show yet.
            const bool wealthOk = (richRate >= 0.0 && poorRate > 0.0 && richRate < poorRate);
            const bool mixOk    = (townShare < 0.0 || campShare < 0.0 || townShare > campShare);
            const bool measurable = (civ->popDaysRich > 500.0 && civ->popDaysPoor > 500.0);
            bool ok = wealthOk && mixOk;
            std::cout << "24. Fertility falls with wealth (within a people), specialists track size: rich="
                      << (richRate < 0 ? std::string("n/a") : std::to_string(richRate))
                      << " poor=" << (poorRate < 0 ? std::string("n/a") : std::to_string(poorRate))
                      << " urban=" << (urbanRate < 0 ? std::string("n/a") : std::to_string(urbanRate))
                      << " rural=" << (ruralRate < 0 ? std::string("n/a") : std::to_string(ruralRate))
                      << " specialistShare camp=" << (campShare < 0 ? std::string("n/a")
                                                      : std::to_string(campShare))
                      << " town+=" << (townShare < 0 ? std::string("n/a") : std::to_string(townShare))
                      << " hires=" << civ->hiresTotal
                      << " viaWeakTie=" << civ->hiresViaWeakTie
                      << " apprenticed=" << civ->apprenticeships
                      << "  [" << softVerdict(ok, ticksRun >= 800 && measurable) << "]\n";
        }
            // 25. IV-P4's acceptance test, three claims in one line:
            //  • festivals and rites measurably discharge stress (the F5 relief
            //    valve) — total stress points removed by collective gathering;
            //  • the gift is a REAL status route: gifts given, potlatches held,
            //    and the resulting non-violent prestige;
            //  • art outlives its makers: great works standing as memorials.
            // The comparison that matters is gifts against killings — if the
            // peaceful route to standing is used more than the violent one,
            // the structural brake Mauss describes is actually working.
            {
                long killings = 0;
                for (const auto& kv : g_actionTally)
                    if (kv.first == "Murder" || kv.first == "Duel" || kv.first == "Raid")
                        killings += kv.second;
                bool ok = g_mindStats.festivalStressDischarged > 0.0
                          && g_mindStats.giftsGiven > 0;
                std::cout << "25. Ritual & gift work (discharge>0, gifts>0): stressDischarged="
                          << (long)g_mindStats.festivalStressDischarged
                          << " festivals=" << g_mindStats.festivalsHeld
                          << " rites=" << civ->totalRites
                          << " gifts=" << g_mindStats.giftsGiven
                          << " potlatches=" << civ->totalPotlatches
                          << " vsKillings=" << killings
                          << " greatWorks=" << civ->greatWorks.size()
                          << "  [" << softVerdict(ok, ticksRun >= 400) << "]\n";
            }

            // 26. §10.1 — the plan's headline claim, and the one the flagship
            // run report said was impossible: a civilisation crosses into
            // modernity AND STAYS. Both halves are asserted, because the old
            // world could touch an era and lose it again the same century —
            // 181 dark ages of Medieval↔Modern yo-yo. "Stays" is read off the
            // last fifth of the run: having got there, the world is still there
            // at the end, and it did not spend that stretch flickering.
            {
                const auto& h = civ->knowledgeHistory;
                int  peakEra = 0, endEra = 0, heldSamples = 0, tailSamples = 0;
                const size_t tailFrom = h.size() - h.size() / 5;
                for (size_t i = 0; i < h.size(); ++i) {
                    peakEra = std::max(peakEra, h[i].era);
                    if (i >= tailFrom) {
                        ++tailSamples;
                        if (h[i].era >= (int)ERA_RENNAISSANCE) ++heldSamples;
                    }
                }
                if (!h.empty()) endEra = h.back().era;
                const float held = tailSamples > 0 ? (float)heldSamples / tailSamples : 0.0f;
                // Crossing over is reaching the early modern rung; holding is
                // spending the closing stretch at renaissance or above (a
                // one-rung slip is survivable history, a fall to the bronze age
                // is the failure this plan exists to fix).
                bool ok = peakEra >= (int)ERA_EARLY_MODERN
                          && endEra >= (int)ERA_RENNAISSANCE
                          && held >= 0.8f;
                std::cout << "26. A civilisation crosses into modernity and stays (peak>=EarlyModern, held>=80%): peakEra="
                          << peakEra << " endEra=" << endEra
                          << " heldTail=" << (int)(held * 100) << "%"
                          << " techs=" << (h.empty() ? 0 : h.back().techCount)
                          << " darkAges=" << civ->darkAgeCount
                          << "  [" << softVerdict(ok, ticksRun >= 2000) << "]\n";
            }
    }
    std::cout << "==========================================\n\n";
}

// ── Web bridge: --inject <file> ──────────────────────────────────────────────
// Applies a web-bridge injection file. Runs once, AFTER the world has been
// created/loaded and BEFORE the tick loop — crucially before ent_quad takes
// raw pointers into `entities`, so push_back (which may reallocate) is safe.
//
// File format: same line-based KEY:value idiom as the save files, one block
// per web character, terminated by END. Unknown lines are ignored.
//
//   CHAR:42               MySQL characters.id
//   NAME:Naelle
//   OPENNESS:62           NUDGE:0 → absolute Big Five trait, 0-100
//   CONSCIENTIOUSNESS:55  NUDGE:1 → signed delta, clamped to [-10,+10]
//   EXTRAVERSION:71
//   AGREEABLENESS:48
//   NEUROTICISM:33
//   NUDGE:0               0 = spawn a new entity; 1 = adjust the living
//   END                       entity whose webCharId == CHAR
//
// A missing/empty file or a malformed block is skipped with a stderr note —
// never fatal (the flag is optional and a no-op when absent).
static void applyWebInjectFile(const std::string& path,
                               std::vector<Entity>& entities,
                               int width, int height) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "INJECT: cannot open '" << path << "' — skipping\n";
        return;
    }

    const int civDay = FreeWillSystem::day / SimClock::FRAMES_PER_TICK;
    auto vc = [](float v){ return std::max(0.0f, std::min(100.0f, v)); };

    // Big Five block, index order O C E A N.
    struct Block {
        int         charId = -1;
        std::string name;
        bool        has[5] = { false, false, false, false, false };
        float       v[5]   = { 0, 0, 0, 0, 0 };
        int         nudge  = -1;
    };

    auto processBlock = [&](const Block& b) {
        if (b.charId < 0 || (b.nudge != 0 && b.nudge != 1)) {
            std::cerr << "INJECT: malformed block (CHAR/NUDGE missing or bad) — skipped\n";
            return;
        }

        if (b.nudge == 1) {
            // ── Feedback nudge: shift an existing web character's traits ─────
            Entity* target = nullptr;
            for (Entity& e : entities)
                if (e.webCharId == b.charId && e.entityHealth > 0) { target = &e; break; }
            if (!target) {
                std::cerr << "INJECT: no living entity with webCharId " << b.charId << "\n";
                if (globalCivEngine)
                    globalCivEngine->logEvent(civDay, "A whisper from beyond finds no one",
                        "tribe", "kind=web_nudge_missing charId=" + std::to_string(b.charId));
                return;
            }
            float* traits[5] = { &target->personality.openness,
                                 &target->personality.conscientiousness,
                                 &target->personality.extraversion,
                                 &target->personality.agreeableness,
                                 &target->personality.neuroticism };
            const char* tag[5] = { "dO", "dC", "dE", "dA", "dN" };
            std::string deltas;
            for (int i = 0; i < 5; ++i) {
                if (!b.has[i]) continue;
                float d = std::max(-10.0f, std::min(10.0f, b.v[i]));   // per-cycle cap
                *traits[i] = vc(*traits[i] + d);
                deltas += " " + std::string(tag[i]) + "=" + std::to_string((int)d);
            }
            if (globalCivEngine)
                globalCivEngine->logEvent(civDay,
                    target->name + " feels a subtle shift of temperament", "tribe",
                    "kind=web_nudge charId=" + std::to_string(b.charId)
                    + " entityId=" + std::to_string(target->entityId) + deltas);
            return;
        }

        // ── NUDGE:0 — spawn a new web-controlled entity ──────────────────────
        for (const Entity& e : entities) {
            if (e.webCharId == b.charId) {
                std::cerr << "INJECT: webCharId " << b.charId << " already in world — spawn skipped\n";
                if (globalCivEngine)
                    globalCivEngine->logEvent(civDay, "An echo of a soul already here fades",
                        "tribe", "kind=web_spawn_duplicate charId=" + std::to_string(b.charId)
                        + " entityId=" + std::to_string(e.entityId));
                return;
            }
        }

        int newId = 0;
        for (const Entity& e : entities) newId = std::max(newId, e.entityId + 1);
        const int   year     = globalCivEngine ? globalCivEngine->getCurrentYear() : -5000;
        const float adultAge = 20.0f;   // web characters arrive as young adults
        const int   birthYear = year - (int)adultAge;

        // Same constructor + stat ranges as the founding population (main():
        // founder spawn loop), so every subsystem starts initialised the same way.
        Entity entity = Entity(
            newId, adultAge, BetterRand::genNrInInterval(80.0f, 100.0f),
            BetterRand::genNrInInterval(30.0f, 70.0f), BetterRand::genNrInInterval(0.0f, 50.0f),
            BetterRand::genNrInInterval(80.0f, 100.0f), "", BetterRand::genNrInInterval(0.0f, 20.0f),
            BetterRand::genNrInInterval(0.0f, 20.0f), BetterRand::genNrInInterval(0.0f, 40.0f),
            BetterRand::genNrInInterval(60.0f, 100.0f), 'A', 0,
            BetterRand::genNrInInterval(0.0f, 50.0f), -1, "happiness", birthYear);
        entity.selected = false;
        entity.salary   = 200;

        // Position: centre of the mid-sized tribe closest to the median
        // population, so newcomers land where society already is; membership is
        // NOT forced — the tribe absorption logic recruits them naturally.
        float px = width * 0.5f, py = height * 0.5f;
        if (globalCivEngine && !globalCivEngine->tribes.empty()) {
            std::vector<int> pops;
            for (const Tribe& t : globalCivEngine->tribes)
                if (t.population() > 0) pops.push_back(t.population());
            if (!pops.empty()) {
                std::sort(pops.begin(), pops.end());
                int median = pops[pops.size() / 2];
                const Tribe* home = nullptr;
                for (const Tribe& t : globalCivEngine->tribes) {
                    if (t.population() <= 0) continue;
                    if (!home || std::abs(t.population() - median) < std::abs(home->population() - median))
                        home = &t;
                }
                if (home) {
                    px = home->centerX; py = home->centerY;
                    entity.originRegionId = home->regionId;
                }
            }
        }
        // Small jitter so multiple injected souls don't stack, snapped to land.
        float jx = std::max(10.0f, std::min((float)width  - 10.0f, px + BetterRand::genNrInInterval(-15.0f, 15.0f)));
        float jy = std::max(10.0f, std::min((float)height - 10.0f, py + BetterRand::genNrInInterval(-15.0f, 15.0f)));
        if (g_planet) {
            const Tile* t = g_planet->tileAtWorld(jx, jy);
            if (t && !t->isPassable()) { jx = px; jy = py; }
        }
        entity.posX = jx;
        entity.posY = jy;

        // Personality: questionnaire traits are ABSOLUTE 0-100 values; traits
        // absent from the block fall back to a random draw. Unlike founders,
        // the childhood shift below does NOT touch personality — the web user's
        // measured traits must survive exactly.
        entity.personality = generateRandomPersonality();
        if (b.has[0]) entity.personality.openness          = vc(b.v[0]);
        if (b.has[1]) entity.personality.conscientiousness = vc(b.v[1]);
        if (b.has[2]) entity.personality.extraversion      = vc(b.v[2]);
        if (b.has[3]) entity.personality.agreeableness     = vc(b.v[3]);
        if (b.has[4]) entity.personality.neuroticism       = vc(b.v[4]);

        // ValueSystem/integrity derived from personality — same formulas and
        // seeding scheme as the founder loop.
        std::mt19937 rng_spawn((unsigned)splitmix64(g_worldSeed.master ^ (0x51ED2C17ull * (newId + 1))));
        std::normal_distribution<float> vd(50.0f, 18.0f);
        entity.ValueSystem.familyOrientation = vc(vd(rng_spawn) + (entity.personality.agreeableness - 50.0f) * 0.3f);
        entity.ValueSystem.achievementDrive  = vc(vd(rng_spawn) + (entity.personality.conscientiousness - 50.0f) * 0.4f);
        entity.ValueSystem.spiritualNeed     = vc(vd(rng_spawn) - (entity.personality.openness - 50.0f) * 0.2f);
        entity.ValueSystem.hedonism          = vc(vd(rng_spawn) + (entity.personality.extraversion - 50.0f) * 0.3f - (entity.personality.conscientiousness - 50.0f) * 0.2f);
        entity.ValueSystem.collectivism      = vc(vd(rng_spawn) + (entity.personality.agreeableness - 50.0f) * 0.35f - (entity.personality.openness - 50.0f) * 0.1f);
        entity.integrity = vc(0.4f * entity.personality.conscientiousness
                            + 0.4f * entity.personality.agreeableness
                            + static_cast<float>(BetterRand::genNrInInterval(-15.0f, 15.0f)));

        // Developmental history (attachment/trauma), founder-style — minus the
        // personality shift (see above).
        float traumaRoll  = vc(static_cast<float>(BetterRand::genNrInInterval(0, 50)));
        float nurtureRoll = vc(100.0f - traumaRoll + static_cast<float>(BetterRand::genNrInInterval(-20, 20)));
        entity.dv.childhoodTraumaScore    = traumaRoll;
        entity.dv.childhoodNurturingScore = nurtureRoll;
        entity.dv.hadSecureAttachment     = (traumaRoll < 20.0f && nurtureRoll > 55.0f);
        if (traumaRoll < 20.0f && nurtureRoll > 60.0f)      entity.dv.attachmentStyle = SECURE;
        else if (traumaRoll > 55.0f)                        entity.dv.attachmentStyle = (nurtureRoll < 30.0f) ? DISORGANIZED : ANXIOUS;
        else if (traumaRoll > 30.0f && nurtureRoll < 40.0f) entity.dv.attachmentStyle = AVOIDANT;
        else                                                entity.dv.attachmentStyle = ANXIOUS;

        // Personality is final — build drives + Jungian stack (as founders do).
        entity.initPsychology();

        // Starting emotional stats: founder formulas, driven by the web traits.
        entity.entityStress       = vc(static_cast<float>(BetterRand::genNrInInterval(5, 30))
                                    + (entity.personality.neuroticism - 50.0f) * 0.25f + traumaRoll * 0.15f);
        entity.entityHygiene      = vc(static_cast<float>(BetterRand::genNrInInterval(55, 95))
                                    + (entity.personality.conscientiousness - 50.0f) * 0.20f);
        entity.entityLoneliness   = vc(static_cast<float>(BetterRand::genNrInInterval(5, 45))
                                    - (entity.personality.extraversion - 50.0f) * 0.25f + traumaRoll * 0.10f);
        entity.entityBoredom      = vc(static_cast<float>(BetterRand::genNrInInterval(10, 50))
                                    + (entity.personality.openness - 50.0f) * 0.15f);
        entity.entityGeneralAnger = vc(static_cast<float>(BetterRand::genNrInInterval(0, 25))
                                    + traumaRoll * 0.12f - (entity.personality.agreeableness - 50.0f) * 0.20f);
        entity.entityHapiness     = vc(static_cast<float>(BetterRand::genNrInInterval(30, 72))
                                    - traumaRoll * 0.15f + nurtureRoll * 0.10f
                                    + (entity.personality.extraversion - 50.0f) * 0.10f);
        entity.entityMentalHealth = vc(static_cast<float>(BetterRand::genNrInInterval(55, 95))
                                    - traumaRoll * 0.20f + nurtureRoll * 0.08f);
        entity.entityHealth       = vc(static_cast<float>(BetterRand::genNrInInterval(70, 100)));

        // Life goals seeded from values (founder logic verbatim).
        entity.m_goals.clear();
        struct GoalSeed { std::string type; float weight; };
        std::vector<GoalSeed> seeds = {
            {"find_partner", entity.ValueSystem.familyOrientation},
            {"build_career", entity.ValueSystem.achievementDrive},
            {"make_friends", entity.ValueSystem.collectivism},
            {"happiness",    entity.ValueSystem.hedonism},
            {"self",         entity.ValueSystem.spiritualNeed}
        };
        auto best = std::max_element(seeds.begin(), seeds.end(),
            [](const GoalSeed& a, const GoalSeed& g){ return a.weight < g.weight; });
        LifeGoal primary; primary.type = best->type; primary.priority = 100.0f;
        primary.progressToward = 0.0f; primary.frustrationLevel = 0.0f; primary.ticksSinceProgress = 0;
        entity.m_goals.push_back(primary);
        for (auto& s : seeds) {
            if (s.type != best->type && s.weight > 35.0f && entity.m_goals.size() < 3) {
                LifeGoal sec; sec.type = s.type; sec.priority = s.weight * 0.6f;
                sec.progressToward = 0.0f; sec.frustrationLevel = 0.0f; sec.ticksSinceProgress = 0;
                entity.m_goals.push_back(sec);
            }
        }

        // Web identity + name (fallback name if the block omitted NAME).
        entity.webCharId = b.charId;
        entity.name = !b.name.empty() ? b.name : ("Newcomer" + std::to_string(b.charId));

        if (globalKinship)
            globalKinship->ensureFounderFamily(entity, year);

        entities.push_back(entity);
        std::cout << "INJECT: spawned '" << entity.name << "' entityId=" << newId
                  << " webCharId=" << b.charId << " at (" << (int)entity.posX
                  << "," << (int)entity.posY << ")\n";
        if (globalCivEngine)
            globalCivEngine->logEvent(civDay,
                entity.name + " (a newcomer) wanders into the world", "tribe",
                "kind=web_spawn charId=" + std::to_string(b.charId)
                + " entityId=" + std::to_string(newId));
    };

    // ── Line parser: same forgiving KEY:value scanning as the save loader ────
    Block b;
    bool inBlock = false;
    std::string line;
    auto key = [&](const char* k){ return line.rfind(k, 0) == 0; };
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        if (line.empty()) continue;
        try {
            if (key("CHAR:")) {
                if (inBlock)
                    std::cerr << "INJECT: block for charId " << b.charId << " missing END — dropped\n";
                b = Block();
                b.charId = std::stoi(line.substr(5));
                inBlock = true;
            }
            else if (!inBlock) continue;   // stray line outside any block
            else if (key("NAME:"))              { b.name = line.substr(5); }
            else if (key("OPENNESS:"))          { b.v[0] = std::stof(line.substr(9));  b.has[0] = true; }
            else if (key("CONSCIENTIOUSNESS:")) { b.v[1] = std::stof(line.substr(18)); b.has[1] = true; }
            else if (key("EXTRAVERSION:"))      { b.v[2] = std::stof(line.substr(13)); b.has[2] = true; }
            else if (key("AGREEABLENESS:"))     { b.v[3] = std::stof(line.substr(14)); b.has[3] = true; }
            else if (key("NEUROTICISM:"))       { b.v[4] = std::stof(line.substr(12)); b.has[4] = true; }
            else if (key("NUDGE:"))             { b.nudge = std::stoi(line.substr(6)); }
            else if (line == "END")             { processBlock(b); b = Block(); inBlock = false; }
            // anything else: ignored (forward compatibility)
        } catch (const std::exception& ex) {
            std::cerr << "INJECT: bad line '" << line << "' (" << ex.what() << ") — block dropped\n";
            b = Block();
            inBlock = false;
        }
    }
    if (inBlock)
        std::cerr << "INJECT: trailing block for charId " << b.charId << " missing END — dropped\n";
}

// ── Command-line options ─────────────────────────────────────────────────────
// Every startup question can be answered on the command line so runs are
// scriptable/reproducible; anything not provided falls back to the interactive
// prompt (or to a sane default in --headless mode, which must never block on
// stdin).
struct CliOptions {
    int         headlessTicks = -1;   // >0 = run without a window for N ticks
    std::string seedText;             // "" = ask (GUI) / random (headless)
    bool        seedGiven = false;
    int         entities  = -1;
    int         region    = -1;       // 1..4
    float       chaos     = -1.0f;
    // M8: persistence — resume a world, or checkpoint one mid-run.
    std::string loadFile;             // load this save instead of spawning founders
    std::string saveFile = "headless_autosave.txt"; // --save-at target (goes to saves/ dir)
    int         saveAtTick = -1;      // >0 = write saveFile at this headless tick
    int         saveEvery  = -1;      // >0 = rolling autosave every N ticks
    bool        saveOnExit = false;   // write the world once the run finishes
    // True when the terminal run was CHOSEN at the prompt rather than passed on
    // the command line. A scripted headless run must never block on stdin; a
    // person who just picked "no window" is sitting right there and should
    // still be asked the same world questions the windowed path asks.
    bool        interactiveTerminal = false;
    // Web bridge: spawn/nudge web-controlled characters from a block file
    // before the tick loop starts (see applyWebInjectFile).
    std::string injectFile;
};

static CliOptions parseCli(int argc, char* argv[]) {
    CliOptions o;
    auto next = [&](int& i) -> const char* {
        return (i + 1 < argc) ? argv[++i] : "";
    };
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--headless")      { o.headlessTicks = std::atoi(next(i)); if (o.headlessTicks <= 0) o.headlessTicks = 500; }
        else if (a == "--seed")     { o.seedText = next(i); o.seedGiven = true; }
        else if (a == "--entities") { o.entities = std::atoi(next(i)); }
        else if (a == "--region")   { o.region = std::atoi(next(i)); }
        else if (a == "--chaos")    { o.chaos = (float)std::atof(next(i)); }
        else if (a == "--load")     { o.loadFile = next(i); }
        else if (a == "--inject")   { o.injectFile = next(i); }
        else if (a == "--save-at")  { o.saveAtTick = std::atoi(next(i)); }
        else if (a == "--save-file"){ o.saveFile = next(i); }
        else if (a == "--save-every"){ o.saveEvery = std::atoi(next(i)); }
        else if (a == "--save-on-exit") { o.saveOnExit = true; }
        // Turn any live-config knob from the command line: `--set cityMul=0`.
        // Repeatable. This is how a kill switch gets verified headlessly — run
        // the pair with and without the knob and diff the logs.
        else if (a == "--set") {
            std::string kv = next(i);
            size_t eq = kv.find('=');
            if (eq == std::string::npos || eq == 0) {
                std::cerr << "--set expects KNOB=VALUE (got '" << kv << "') — ignored\n";
            } else if (!setLiveConfigKnob(kv.substr(0, eq), (float)std::atof(kv.c_str() + eq + 1))) {
                std::cerr << "--set: unknown knob '" << kv.substr(0, eq) << "' — ignored\n";
            }
        }
        // M10: scenario presets — curated parameter bundles. Applied in
        // parse order, so any explicit flag AFTER --scenario overrides it.
        else if (a == "--scenario") {
            std::string s = next(i);
            if (s == "eden") {          // gentle world: mild region, low chaos
                o.entities = 150; o.region = 1; o.chaos = 0.5f;
            } else if (s == "crucible") { // harsh world: disease region, high chaos
                o.entities = 60;  o.region = 4; o.chaos = 2.2f;
            } else if (s == "babel") {   // crowded world: many founders, factions
                o.entities = 400; o.region = 2; o.chaos = 1.3f;
            } else if (s == "dish") {    // petri dish: a handful of souls to watch
                o.entities = 12;  o.region = 1; o.chaos = 1.0f;
            } else {
                std::cerr << "Unknown scenario '" << s
                          << "' (eden|crucible|babel|dish) — ignored\n";
            }
        }
        else if (a == "--help" || a == "-h") {
            std::cout << "ASHB2 options:\n"
                         "  --headless <ticks>   run N ticks without a window, then exit\n"
                         "  --seed <text|num>    world seed (same seed = same history)\n"
                         "  --entities <n>       founding population (default 40)\n"
                         "  --region <1-4>       disease-climate region (default 1)\n"
                         "  --chaos <0.3-2.5>    divergence level (default 1.3)\n"                          "  --load <file>        resume from a save (looks in saves/ dir first, then raw path)\n"
                          "  --inject <file>      web bridge: spawn/nudge web characters from a block file\n"
                          "  --save-at <tick>     write a save at this headless tick\n"
                          "  --save-file <file>   where saves are written (default saves/headless_autosave.txt)\n"
                          "  --save-every <n>     rolling autosave every N ticks (overwrites --save-file)\n"
                          "  --save-on-exit       write the world once the run finishes\n"
                          "  --set <knob>=<val>   set a live-config multiplier (repeatable), e.g. --set cityMul=0\n"
                         "  --scenario <name>    preset bundle: eden (gentle, 150 souls), crucible (harsh, 60),\n"
                         "                       babel (crowded, 400), dish (petri dish, 12). Later flags override.\n";
            std::exit(0);
        }
    }
    return o;
}

int main(int argc, char* argv[]) {
    CliOptions cli = parseCli(argc, argv);
    // Environment variable kept as an alternative to --headless.
    if (cli.headlessTicks <= 0) {
        if (const char* hl = std::getenv("ASHB_HEADLESS")) {
            cli.headlessTicks = std::atoi(hl);
            if (cli.headlessTicks <= 0) cli.headlessTicks = 500;
        }
    }
    // ── How do you want to watch this world? ─────────────────────────────────
    // Running without a window used to mean remembering `--headless <ticks>`
    // before the program started, which is fine for scripts and useless at a
    // prompt. Asked here instead, so the terminal run is a first-class way to
    // start the simulation rather than a flag you had to know about. Skipped
    // entirely when the command line already decided (--headless, ASHB_HEADLESS
    // or --load), because a scripted run must never block on stdin.
    if (cli.headlessTicks <= 0 && !std::getenv("ASHB_HEADLESS")) {
        std::cout << "\nHow do you want to run this world?\n"
                     "  1  With a window   — watch it live (graphs, inspector, save/load buttons)\n"
                     "  2  In the terminal — no window: run N ticks, print the realism report\n"
                     ">";
        std::string modeInput;
        std::getline(std::cin, modeInput);
        // Tolerate a stray newline left in the buffer by an earlier read.
        if (modeInput.empty()) std::getline(std::cin, modeInput);
        if (!modeInput.empty() && modeInput[0] == '2') {
            cli.interactiveTerminal = true;
            std::cout << "how many ticks (blank = 1000): ";
            std::string t; std::getline(std::cin, t);
            cli.headlessTicks = t.empty() ? 1000 : std::atoi(t.c_str());
            if (cli.headlessTicks <= 0) cli.headlessTicks = 1000;

            // Saving, from the terminal. The windowed build has had save/load
            // buttons all along; a terminal run could only checkpoint at one
            // hard-coded tick given on the command line. Both questions are
            // asked here so a long unattended run can be resumed rather than
            // repeated — the same saves/ directory and the same format the
            // window writes, so a world saved here opens there.
            std::cout << "save the world when it finishes? (filename, blank = no): ";
            std::string sf; std::getline(std::cin, sf);
            if (!sf.empty()) { cli.saveFile = sf; cli.saveOnExit = true; }
            std::cout << "autosave every how many ticks? (blank/0 = never): ";
            std::string se; std::getline(std::cin, se);
            const int every = se.empty() ? 0 : std::atoi(se.c_str());
            if (every > 0) {
                cli.saveEvery = every;
                if (!cli.saveOnExit && sf.empty()) {
                    std::cout << "  autosaving to '" << cli.saveFile << "'\n";
                }
            }
        }
    }

    const bool headless = (cli.headlessTicks > 0);
    g_headlessMode = headless;
    std::cout << "Client version 2.64.0\n";
    std::cout << " \"I was meant to be perfect, ";
    std::cout << "I was meant to be beautiful\" \n\n";
   //// Initialize logger (this redirects std::cout to cmd_log.txt)
   Logger logger;
   globalLogger = &logger;

    globalLogger->logCmd("clearing files...");
    try{

        rm_data_file();
        rm_data_act_file();
        rm_log_files();
        globalLogger->clearAllLogs();
      std::cout << "file clearing done!\n";
    }catch(...){
        std::cout << "error with file clearing! if it persist just reclone the repo!\n";
    }

    // Clear tick history file. NOTE: must match the exporter's filename
    // (exportTickHistory appends to tick_history.jsonl); the old ".json"
    // truncation cleared a file nobody wrote, so the real one grew forever
    // across runs (observed at 188 MB).
    std::ofstream tick_history("./src/data/tick_history.jsonl", std::ios::trunc);
    tick_history.close();

    globalLogger->logCmd("done");
    std::cout << "Welcome to Artificial Simulation of Human Behavior 2 (ASHB2)\n";
    std::cout << "Complete simulation can be found at /data/complete_logs.txt\n";
    std::cout << "@author: Komodo \n";

    // Region (disease climate)
    // A terminal run chosen at the prompt still gets asked the world questions;
    // only a scripted one (--headless / ASHB_HEADLESS) takes the defaults.
    const bool askWorldQuestions = (!headless || cli.interactiveTerminal);

    if (cli.region >= 1 && cli.region <= 4) {
        Disease::region = cli.region * 10;
    } else if (!askWorldQuestions) {
        Disease::region = 10;                 // default: Paris/Oceanic
    } else {
        implementRegion();
    }

    // Founding population
    int entity_num = 40;
    if (cli.entities > 0) {
        entity_num = cli.entities;
    } else if (askWorldQuestions) {
        std::cout << "enter entity number (default: 40): ";
        std::cin >> entity_num;
        if (!entity_num) entity_num = 40;
    }

    // ── A2/E1: scoring priors — the human-in-the-loop tuning surface. Absent
    // file = engine defaults (bit-identical classic scorer); a priors_vN file
    // produced by the feedback loop retunes the mind without a recompile.
    mind::loadPriors("bridge/priors_active.txt");

    // ── World seed (determines the whole planet & history; same seed = same run) ──
    {
        std::string seedInput = cli.seedText;
        if (!cli.seedGiven && askWorldQuestions) {
            std::cout << "enter world seed (text or number, blank = random): ";
            std::cin.ignore();
            std::getline(std::cin, seedInput);
        }
        if (seedInput.empty()) {
            g_worldSeed.master = std::random_device{}() ^
                (static_cast<uint64_t>(std::random_device{}()) << 32);
        } else {
            g_worldSeed = WorldSeed::fromString(seedInput);
        }
        std::cout << "world seed = " << g_worldSeed.master << "\n";
        globalLogger->logCmd("world seed = " + std::to_string(g_worldSeed.master));

        // Divergence / chaos level: how wildly history varies between runs.
        float chaos = 1.3f;
        if (cli.chaos > 0.0f) {
            chaos = cli.chaos;
        } else if (askWorldQuestions) {
            std::cout << "chaos level 0.5 (tame) .. 2.0 (wild) [default 1.3]: ";
            std::string chaosInput;
            std::getline(std::cin, chaosInput);
            if (!chaosInput.empty()) { try { chaos = std::stof(chaosInput); } catch (...) {} }
        }
        chaos = std::max(0.3f, std::min(2.5f, chaos));
        g_worldSeed.divergence.butterfly       = chaos;
        g_worldSeed.divergence.innovationLuck  = 0.7f + chaos * 0.4f;
        g_worldSeed.divergence.catastropheRate = chaos;
        g_worldSeed.divergence.migrationPressure = chaos;
        std::cout << "chaos = " << chaos << "\n";

        // Seed every randomness source deterministically from the master seed.
        BetterRand::reseed(splitmix64(g_worldSeed.master ^ STREAM_SPAWN));
    }

    const int height = 1050;
    const int width = 1400;

    // ── Procedural planet ────────────────────────────────────────────────────
    g_planet = new Planet();
    g_planet->generate(g_worldSeed, 200, 150, (float)width, (float)height);
    {
        std::stringstream ps;
        ps << "planet generated: hash=" << g_planet->hash()
           << " habitable_regions=" << g_planet->habitableRegionCount()
           << " total_regions=" << g_planet->regions.size();
        std::cout << ps.str() << "\n";
        globalLogger->logCmd(ps.str());
    }

    // ── Per-region resource pools (food/wood/stone/metal/water/herbs) ─────────
    // Geography sets each region's resource ceilings; the simulation then draws
    // them down and lets them regrow, so settlement location genuinely matters.
    g_resources.init(*g_planet);

    // ── Living food chain layered on the resource pools ──────────────────────
    // Plants, herbivores (game) and predators per region; agents hunt and forage
    // it, drought starves it, overhunting collapses it. Seed it from the pools.
    g_ecosystem.init();

    // ── Per-region procedural languages ──────────────────────────────────────
    g_lexicon = new Lexicon();
    g_lexicon->initRegions((int)g_planet->regions.size(), g_worldSeed.master);

        // ── Starting cradles: well-separated fertile homelands ───────────────────
        std::mt19937_64 cradleRng = makeStream(g_worldSeed.master, STREAM_SPAWN, 1);
        int cradleCount = std::min(5, std::max(2, entity_num / 12));
        std::vector<Planet::Cradle> cradles = g_planet->pickCradlePoints(cradleCount, cradleRng);
        {
            std::stringstream cs;
            cs << "seeded " << cradles.size() << " starting cradles at:";
            for (auto& c : cradles) cs << " (" << c.gx << "," << c.gy << " r" << c.regionId << ")";
            std::cout << cs.str() << "\n";
            globalLogger->logCmd(cs.str());
        }

        std::vector<Entity> entities;
        entities.reserve(2048);
        int count = 0;
        for (int y = 0; y < 1; ++y){
            for (int x = 0; x < entity_num; ++x){
                // M7: stagger founder ages (16-42). A uniform-age cohort hits
                // the old-age hazard in the same handful of years — the whole
                // founding generation died at once around year 50, hollowing
                // every tribe below its dissolution threshold simultaneously.
                int founderAge = BetterRand::genNrInInterval(16, 42);
                int birthYear = -5000 - founderAge; // born in stone age
                Entity entity = Entity(
                    count, (float)founderAge, BetterRand::genNrInInterval(80.0f, 100.0f), BetterRand::genNrInInterval(30.0f, 70.0f), BetterRand::genNrInInterval(0.0f, 50.0f)
                    , BetterRand::genNrInInterval(80.0f, 100.0f), "", BetterRand::genNrInInterval(0.0f, 20.0f), BetterRand::genNrInInterval(0.0f, 20.0f),
                    BetterRand::genNrInInterval(0.0f, 40.0f), BetterRand::genNrInInterval(60.0f, 100.0f), 'A', 0, BetterRand::genNrInInterval(0.0f, 50.0f), -1, "happiness", birthYear);

                entity.selected = false;
                // Place into a cradle: each starting band clusters around its homeland,
                // so populations begin geographically isolated and diverge over time.
                if (!cradles.empty()) {
                    const Planet::Cradle& home = cradles[x % cradles.size()];
                    entity.originRegionId = home.regionId;
                    float hwx, hwy; g_planet->gridToWorld(home.gx, home.gy, hwx, hwy);
                    // jitter around the homeland, then snap onto passable land
                    float jx = hwx + BetterRand::genNrInInterval(-60.0f, 60.0f);
                    float jy = hwy + BetterRand::genNrInInterval(-60.0f, 60.0f);
                    jx = std::max(10.0f, std::min((float)width - 10.0f, jx));
                    jy = std::max(10.0f, std::min((float)height - 10.0f, jy));
                    const Tile* t = g_planet->tileAtWorld(jx, jy);
                    if (t && !t->isPassable()) { jx = hwx; jy = hwy; } // fall back to centre
                    entity.posX = jx;
                    entity.posY = jy;
                    // Re-name in the homeland's language now that its region is known.
                    if (g_lexicon) entity.name = g_lexicon->genName(entity.originRegionId, entity.entitySex);
                } else {
                    entity.posX = BetterRand::genNrInInterval(80.0f, (float)(width - 80));
                    entity.posY = BetterRand::genNrInInterval(60.0f, (float)(height) * 0.60f - 60.0f);
                }
                entity.salary = 200;

                // --- Personality (Big Five, already randomized) ---
                entity.personality = generateRandomPersonality();

                // --- ValueSystem: the "soul" — what this person cares about ---
                // Each value is correlated with personality for realism
                std::mt19937 rng_spawn((unsigned)splitmix64(g_worldSeed.master ^ (0x51ED2C17ull * (count + 1))));
                std::normal_distribution<float> vd(50.0f, 18.0f);
                auto vc = [](float v){ return std::max(0.0f, std::min(100.0f, v)); };

                entity.ValueSystem.familyOrientation  = vc(vd(rng_spawn) + (entity.personality.agreeableness - 50.0f) * 0.3f);
                entity.ValueSystem.achievementDrive   = vc(vd(rng_spawn) + (entity.personality.conscientiousness - 50.0f) * 0.4f);
                entity.ValueSystem.spiritualNeed      = vc(vd(rng_spawn) - (entity.personality.openness - 50.0f) * 0.2f);
                entity.ValueSystem.hedonism           = vc(vd(rng_spawn) + (entity.personality.extraversion - 50.0f) * 0.3f - (entity.personality.conscientiousness - 50.0f) * 0.2f);
                entity.ValueSystem.collectivism       = vc(vd(rng_spawn) + (entity.personality.agreeableness - 50.0f) * 0.35f - (entity.personality.openness - 50.0f) * 0.1f);

                // Integrity: honesty grows from conscientiousness + agreeableness
                entity.integrity = vc(0.4f * entity.personality.conscientiousness
                                    + 0.4f * entity.personality.agreeableness
                                    + static_cast<float>(BetterRand::genNrInInterval(-15.0f, 15.0f)));

                // --- Developmental History: childhood shapes the adult ---
                float traumaRoll   = vc(static_cast<float>(BetterRand::genNrInInterval(0, 50)));
                float nurtureRoll  = vc(100.0f - traumaRoll + static_cast<float>(BetterRand::genNrInInterval(-20, 20)));
                entity.dv.childhoodTraumaScore    = traumaRoll;
                entity.dv.childhoodNurturingScore = nurtureRoll;
                entity.dv.hadSecureAttachment     = (traumaRoll < 20.0f && nurtureRoll > 55.0f);

                // Attachment style derived from childhood
                if (traumaRoll < 20.0f && nurtureRoll > 60.0f)
                    entity.dv.attachmentStyle = SECURE;
                else if (traumaRoll > 55.0f)
                    entity.dv.attachmentStyle = (nurtureRoll < 30.0f) ? DISORGANIZED : ANXIOUS;
                else if (traumaRoll > 30.0f && nurtureRoll < 40.0f)
                    entity.dv.attachmentStyle = AVOIDANT;
                else
                    entity.dv.attachmentStyle = ANXIOUS;

                // Childhood permanently shifts personality at spawn (replicate finalizeChildhood effect)
                entity.personality.neuroticism    = vc(entity.personality.neuroticism    + traumaRoll  * 0.25f);
                entity.personality.agreeableness  = vc(entity.personality.agreeableness  - traumaRoll  * 0.15f + nurtureRoll * 0.12f);
                entity.personality.extraversion   = vc(entity.personality.extraversion   - traumaRoll  * 0.10f + nurtureRoll * 0.10f);
                entity.personality.openness       = vc(entity.personality.openness       + nurtureRoll * 0.15f);

                // Personality is now finalized — build drives AND the Jungian
                // stack so setpoints/type reflect this individual (the ctor ran
                // with default midline traits before personality was assigned).
                entity.initPsychology();

                // --- Starting emotional stats: no two people start at zero ---
                // Neuroticism → more stress; conscientiousness → better hygiene; extraversion → less loneliness
                entity.entityStress       = vc(static_cast<float>(BetterRand::genNrInInterval(5, 30))
                                            + (entity.personality.neuroticism - 50.0f) * 0.25f
                                            + traumaRoll * 0.15f);
                entity.entityHygiene      = vc(static_cast<float>(BetterRand::genNrInInterval(55, 95))
                                            + (entity.personality.conscientiousness - 50.0f) * 0.20f);
                entity.entityLoneliness   = vc(static_cast<float>(BetterRand::genNrInInterval(5, 45))
                                            - (entity.personality.extraversion - 50.0f) * 0.25f
                                            + traumaRoll * 0.10f);
                entity.entityBoredom      = vc(static_cast<float>(BetterRand::genNrInInterval(10, 50))
                                            + (entity.personality.openness - 50.0f) * 0.15f);
                entity.entityGeneralAnger = vc(static_cast<float>(BetterRand::genNrInInterval(0, 25))
                                            + traumaRoll * 0.12f
                                            - (entity.personality.agreeableness - 50.0f) * 0.20f);
                entity.entityHapiness     = vc(static_cast<float>(BetterRand::genNrInInterval(30, 72))
                                            - traumaRoll * 0.15f
                                            + nurtureRoll * 0.10f
                                            + (entity.personality.extraversion - 50.0f) * 0.10f);
                entity.entityMentalHealth = vc(static_cast<float>(BetterRand::genNrInInterval(55, 95))
                                            - traumaRoll * 0.20f
                                            + nurtureRoll * 0.08f);
                entity.entityHealth       = vc(static_cast<float>(BetterRand::genNrInInterval(70, 100)));

                // --- Life goals: seeded by values and personality ---
                {
                    // Clear the default goal and assign based on dominant value
                    entity.m_goals.clear();
                    struct GoalSeed { std::string type; float weight; };
                    std::vector<GoalSeed> seeds = {
                        {"find_partner",  entity.ValueSystem.familyOrientation},
                        {"build_career",  entity.ValueSystem.achievementDrive},
                        {"make_friends",  entity.ValueSystem.collectivism},
                        {"happiness",     entity.ValueSystem.hedonism},
                        {"self",          entity.ValueSystem.spiritualNeed}
                    };
                    // Primary goal = highest value
                    auto best = std::max_element(seeds.begin(), seeds.end(),
                        [](const GoalSeed& a, const GoalSeed& b){ return a.weight < b.weight; });
                    LifeGoal primary;
                    primary.type = best->type;
                    primary.priority = 100.0f;
                    primary.progressToward = 0.0f;
                    primary.frustrationLevel = 0.0f;
                    primary.ticksSinceProgress = 0;
                    entity.m_goals.push_back(primary);

                    // Secondary goal: random from remaining if value > 35
                    for (auto& s : seeds) {
                        if (s.type != best->type && s.weight > 35.0f && entity.m_goals.size() < 3) {
                            LifeGoal sec;
                            sec.type = s.type;
                            sec.priority = s.weight * 0.6f;
                            sec.progressToward = 0.0f;
                            sec.frustrationLevel = 0.0f;
                            sec.ticksSinceProgress = 0;
                            entity.m_goals.push_back(sec);
                        }
                    }
                }

                std::stringstream ss;
                ss << "Entity " << count
                << " | Personality E=" << (int)entity.personality.extraversion
                << " A=" << (int)entity.personality.agreeableness
                << " C=" << (int)entity.personality.conscientiousness
                << " N=" << (int)entity.personality.neuroticism
                << " O=" << (int)entity.personality.openness
                << " | Values Fam=" << (int)entity.ValueSystem.familyOrientation
                << " Ach=" << (int)entity.ValueSystem.achievementDrive
                << " Hed=" << (int)entity.ValueSystem.hedonism
                << " Col=" << (int)entity.ValueSystem.collectivism
                << " Spi=" << (int)entity.ValueSystem.spiritualNeed
                << " | Attachment=" << entity.dv.attachmentStyle
                << " Trauma=" << (int)entity.dv.childhoodTraumaScore
                << " Nurture=" << (int)entity.dv.childhoodNurturingScore
                << " | Start: Happy=" << (int)entity.entityHapiness
                << " Stress=" << (int)entity.entityStress
                << " Lonely=" << (int)entity.entityLoneliness
                << " Goal=" << entity.m_goals[0].type;
                globalLogger->logCmd(ss.str());
                entities.push_back(entity);
                count++;
            }
        }

        // ── CivilizationEngine initialisation ────────────────────────────────
        globalCivEngine = new CivilizationEngine();

        // ── Kinship initialisation ───────────────────────────────────────────
        // Every seeded entity founds its own family/clan; their descendants then
        // inherit it. Done up front so the very first births already have a line.
        globalKinship = new KinshipSystem();
        for (Entity& e : entities)
            globalKinship->ensureFounderFamily(e, globalCivEngine->getCurrentYear());

        // ── Social order (classes & clientela) ───────────────────────────────
        // Everyone starts a free plebeian; wealth, standing and debt then sort
        // society into slaves, plebeians and patricians over the generations.
        globalSocialOrder = new SocialOrderSystem();

        // ── M8: resume a saved world ─────────────────────────────────────────
        // Replaces the freshly spawned founders with the saved population and
        // macro state (tribes, religions, era, sim clock, shared RNG stream).
        int loadedFrame = -1;
        if (!cli.loadFile.empty()) {
            int ld = 0, lf = 0;
            if (loadGame(cli.loadFile, entities, ld, lf, globalCivEngine)) {
                FreeWillSystem::day = ld;
                loadedFrame = lf;
            } else {
                std::cerr << "LOAD FAILED (" << cli.loadFile << "): continuing with fresh founders\n";
            }
        }

        // ── Web bridge: inject web-controlled characters ─────────────────────
        // Must run AFTER the world exists (fresh founders or a loaded save) and
        // BEFORE ent_quad takes raw pointers into `entities` below — push_back
        // may reallocate the vector.
        if (!cli.injectFile.empty())
            applyWebInjectFile(cli.injectFile, entities, width, height);

        bool showEntityWindow = false;
        int selectedEntityIndex = -1;
        std::vector<Entity*> ent_quad;
        for(int i=0; i<entities.size(); i++){
            ent_quad.push_back(&entities[i]);
        }
        ++g_entQuadVersion;   // initial build → invalidate movement lookup caches

        std::vector<std::vector<Entity*>> close_entity_together = getSocialGroups(ent_quad);

        // ici on applique l'algorithme pour modification stats
        // Note: For now, we'll run this in the main loop instead of a separate thread
        // to avoid threading complexity with the UI
        // std::thread statistics(applyFreeWill, std::ref(close_entity_together));

        int frameCounter = 0;
        int day = FreeWillSystem::day;
        if (loadedFrame >= 0) frameCounter = loadedFrame;   // M8: resume mid-tick

        const int UPDATE_FREQUENCY = 60; // Update free will every 60 frames

    // ── Headless mode ────────────────────────────────────────────────────────
    // --headless <ticks> (or ASHB_HEADLESS=<ticks>) runs the full simulation
    // loop without any window, so automated tests / experiments can exercise
    // the engine (the GUI needs an interactive GL context CI doesn't have).
    // ASHB_NEAT_SHARE=<0..1>: fraction of newborns wired with a NEAT brain.
    // Lets headless runs bootstrap a neuroevolution population without the
    // GUI slider (the config console is unreachable in automated runs).
    if (const char* ns = std::getenv("ASHB_NEAT_SHARE"))
        g_liveConfig.neatBrainShare = std::min(1.0f, std::max(0.0f, (float)atof(ns)));

    if (headless) {
        int targetTicks = cli.headlessTicks;
        std::cout << "HEADLESS: running " << targetTicks << " ticks ("
                  << targetTicks * UPDATE_FREQUENCY << " frames)\n";
        int ticksDone = 0;
        // M11: coarse phase profiler (ASHB_PROFILE=1) — where does the tick go?
        const bool profile = std::getenv("ASHB_PROFILE") != nullptr;
        double msSim = 0.0, msMove = 0.0;
        while (ticksDone < targetTicks && !entities.empty()) {
            auto t0 = std::chrono::steady_clock::now();
            updateSimulationStep(entities, ent_quad, close_entity_together, day,
                                 frameCounter, UPDATE_FREQUENCY, false, width, height,
                                 selectedEntityIndex, showEntityWindow, globalCivEngine);
            auto t1 = std::chrono::steady_clock::now();
            updateMovement(ent_quad, (float)width, (float)height, day);
            auto t2 = std::chrono::steady_clock::now();
            if (profile) {
                msSim  += std::chrono::duration<double, std::milli>(t1 - t0).count();
                msMove += std::chrono::duration<double, std::milli>(t2 - t1).count();
            }
            if (frameCounter == 0) {              // frameCounter wraps once per tick
                ticksDone++;
                // M8: checkpoint mid-run so long worlds survive a crash and
                // the save→load path gets exercised by automated tests.
                if (cli.saveAtTick > 0 && ticksDone == cli.saveAtTick)
                    saveGame(cli.saveFile, entities, day, frameCounter, globalCivEngine);
                // Rolling autosave: overwrite the same file, so a run that dies
                // at hour three is resumable from minutes ago rather than lost.
                if (cli.saveEvery > 0 && ticksDone % cli.saveEvery == 0) {
                    saveGame(cli.saveFile, entities, day, frameCounter, globalCivEngine);
                    std::cout << "AUTOSAVE: tick " << ticksDone
                              << " -> saves/" << cli.saveFile << "\n";
                }
            }
        }
        if (cli.saveOnExit) {
            saveGame(cli.saveFile, entities, day, frameCounter, globalCivEngine);
            std::cout << "SAVED: saves/" << cli.saveFile
                      << "  (resume with --load " << cli.saveFile << ")\n";
        }
        if (profile && ticksDone > 0)
            std::cout << "PROFILE: sim=" << (msSim / ticksDone) << " ms/tick, movement="
                      << (msMove / ticksDone) << " ms/tick (x" << UPDATE_FREQUENCY << " frames)\n";
        for (Entity& e : entities) e.flushEntityStats();  // drain buffered CSVs
        std::cout << "HEADLESS: done. ticks=" << ticksDone
                  << " population=" << entities.size() << "\n";
        printRealismReport(entities, globalCivEngine, ticksDone);   // M9
        return 0;
    }

#ifdef HEADLESS
    // No render path was compiled in. Reaching here means the person asked for
    // a window from a binary that has none — say so plainly and tell them the
    // one flag that gets them a run, rather than exiting silently.
    std::cout << "\nThis is a headless build — it has no window.\n"
                 "Run the simulation in the terminal instead, e.g.\n"
                 "  ./app --headless 1000 --seed mars --entities 60\n"
                 "(or rebuild with ./build_linux.sh --gui for the graphical version)\n";
    return 0;
#else
    // Interactive mode: GLFW + OpenGL + Dear ImGui is the only render path.
    {
        if (!glfwInit()) return -1;

        GLFWwindow* window = glfwCreateWindow(width, height, "ASHB", NULL, NULL);
        if (!window) {
            glfwTerminate();
            return -1;
        }
        glfwMakeContextCurrent(window);

        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        glfwSwapInterval(1);

        UI instanceUI;


        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            updateSimulationStep(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, instanceUI.isSimulationPaused(), width, height, selectedEntityIndex, showEntityWindow, globalCivEngine);

            // Force-based movement runs every frame (was orphaned: defined but
            // never called, so all proximity mechanics ran on frozen positions).
            if (!instanceUI.isSimulationPaused())
                updateMovement(ent_quad, (float)width, (float)height, day);

            std::string saveFilename;
            int saveLoadAction = instanceUI.showSaveLoadButtons(saveFilename, day / 60 , entities.size(), UPDATE_FREQUENCY, {});
            if (saveLoadAction == 1) {
                saveGame(saveFilename, entities, day, frameCounter, globalCivEngine);
            } else if (saveLoadAction == 2) {
                if (loadGame(saveFilename, entities, day, frameCounter, globalCivEngine)) {
                    // Rebuild pointer vectors after loading
                    ent_quad.clear();
                    for (int j = 0; j < (int)entities.size(); j++) {
                        ent_quad.push_back(&entities[j]);
                    }
                    ++g_entQuadVersion;   // invalidate movement lookup caches
                    close_entity_together = getSocialGroups(ent_quad);
                    showEntityWindow = false;
                    selectedEntityIndex = -1;
                }
            }

            // M10: divine interventions (smite/bless/feast/famine/meteor/calm),
            // possession, interviews, live world tunables.
            {
                Entity* chosen = (selectedEntityIndex >= 0 &&
                                  selectedEntityIndex < (int)entities.size())
                                 ? &entities[selectedEntityIndex] : nullptr;
                instanceUI.ShowGodConsole(ent_quad, chosen, day / 60);
                instanceUI.ShowPossessWindow(chosen, day / 60);
                instanceUI.ShowInterviewWindow(chosen, ent_quad, day / 60);
                instanceUI.ShowConfigConsole();
                instanceUI.ShowEmergencePanel(ent_quad, day / UPDATE_FREQUENCY);
            }

            // Entity selection: click graph node OR mind board card
            int graph_sel = instanceUI.HandlePointMovement(ent_quad);
            int board_sel = instanceUI.ShowMindBoard(ent_quad);
            int sel = (board_sel != -1) ? board_sel : graph_sel;
            if (sel != -1) {
                selectedEntityIndex = sel;
                showEntityWindow = true;
            }

            if (showEntityWindow && selectedEntityIndex >= 0 && selectedEntityIndex < entities.size()) {
                instanceUI.ShowEntityWindow(&entities.at(selectedEntityIndex), &showEntityWindow, ent_quad);
            }

            // ── Civilization panel ────────────────────────────────────────────
            instanceUI.ShowCivilizationPanel(day / 60, ent_quad);

            // ── Market panel (supply & demand) ────────────────────────────────
            instanceUI.ShowMarketPanel();

            // ── World map + history panels ────────────────────────────────────
            DrawPlanetWindow(g_planet, ent_quad);
            DrawHistoryWindow();

            instanceUI.DrawGrid(ent_quad);

            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        glfwTerminate();
    }
    return 0;
#endif   // HEADLESS
}
