#include <SDL.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "./header/UI.h"
#include "./header/Entity.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <time.h>
#include <random>
#include "./header/FreeWillSystem.h"
#include "./header/BetterRand.h"
#include <iostream>
#include <sstream>
#include <thread>
#include "./header/Disease.h"
#include "util/Debbug.h"
#include "./header/implot.h"
#include "./header/implot_internal.h"
#include "./util/clear.h"
#include "./header/SaveLoad.h"
#include "./header/heritage.h"
#include "./header/Logging.h"
#include "header/SDLEngine.h"
#include "header/Image.h"
#include "./header/NarrativeEngine.h"
#include "./header/CivilizationEngine.h"
#include "./header/WorldSeed.h"
#include "world/Planet.h"
#include "world/PlanetView.h"
#include "world/Lexicon.h"
#include <unordered_map>
#include <unordered_set>

using GroupEntity = std::vector<std::vector<Entity*>>;

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

void applyDisease(Entity* ent, int neighSize, int sickClose){
    //si est seul la proba de tombé malade est null
    Disease d;
    if(ent->entityDiseaseType != -1){
        //already sick we manage it
        d.manageSickness(ent);
    }
    if(neighSize >= 3){
        d.reduceAntiBody(ent);
        int disease = d.calculateDisease(neighSize, ent, sickClose);
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

int getNBSickClose(std::vector<Entity*> grp){
    int c =0;
    for(Entity* ent : grp){
        if(ent->entityDiseaseType != -1){
            c++;
        }
    }
    return c;
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
    int hour = (day % 60) * 24 / 60;
    int dayOfWeek = (day / 60) % 7;

    bool isNightTime = (hour >= 22 || hour < 6);
    bool isWeekend = (dayOfWeek >= 5);
    bool isAtWork = (!isWeekend && hour >= 9 && hour < 17);
    bool isInPublic = (numPeopleNearby > 2);

    EnvironmentalFactors env = generateEnvFactors(day);
    return ActionContext(isNightTime, isWeekend, isAtWork, isInPublic, numPeopleNearby, env);
}

// Generate random personality using Big Five distribution
Personality generateRandomPersonality() {
    std::random_device rd;
    std::mt19937 gen(rd());
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
                ent->rebuildSemanticMemory();
                mem.entityInvolvedId = dead->entityId;
                mem.emotionalIntensity = griefIntensity;
                mem.isFormative = (griefIntensity > 0.6f);
                mem.internalNarrative = narrative;
                ent->lifeMemories.push_back(mem);
            }
        }
    }


// Social-graph grouping: hybrid proximity + social bonds.
// Each entity groups with bonded contacts AND anyone within 120px (chance encounters).
std::vector<std::vector<Entity*>> getSocialGroups(std::vector<Entity*>& entities) {
    if (entities.empty()) return {};

    std::vector<bool> inGroup(entities.size(), false);
    std::vector<std::vector<Entity*>> groups;

    for (size_t i = 0; i < entities.size(); ++i) {
        if (inGroup[i] || entities[i]->entityHealth <= 0.0f) continue;

        std::vector<Entity*> group;
        group.push_back(entities[i]);
        inGroup[i] = true;

        // Add people this entity has a social bond with
        for (size_t j = 0; j < entities.size(); ++j) {
            if (j == i || inGroup[j] || entities[j]->entityHealth <= 0.0f) continue;
            if (entities[i]->searchConnSocial(entities[j]) > 5.0f && group.size() < 10) {
                group.push_back(entities[j]);
                inGroup[j] = true;
            }
        }

        // Add entities within 120px (proximity encounters — no bond needed)
        for (size_t j = 0; j < entities.size(); ++j) {
            if (j == i || inGroup[j] || entities[j]->entityHealth <= 0.0f) continue;
            if (group.size() >= 12) break;
            float dx = entities[i]->posX - entities[j]->posX;
            float dy = entities[i]->posY - entities[j]->posY;
            float dist2 = dx * dx + dy * dy;
            if (dist2 <= 120.0f * 120.0f) {
                group.push_back(entities[j]);
                inGroup[j] = true;
            }
        }

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

    // Weighted random selection
    float total = 0.0f;
    for (auto& t : pool) total += t.weight;
    float roll = (float)rand() / (float)RAND_MAX * total;
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

        if (action->name == "Socialize" || action->name == "GoodConnection") {
            score += self->searchConnSocial(neighbor) * 2.0f;
            score += (model ? model->trustLevel : 0) * 1.5f;
            score -= self->searchConnAng(neighbor) * 3.0f;
        } else if (action->name == "Desire" || action->name == "Flirt") {
            score += self->searchConnDesire(neighbor) * 3.0f;
            score -= self->searchConnAng(neighbor) * 2.0f;
            // Attachment style affects target preference
            if (self->dv.attachmentStyle == ANXIOUS)
                score += (model ? (1.0f - model->predictability) : 0) * 2.0f; // anxious drawn to unpredictable
        } else if (action->name == "AngerConnection" || action->name == "Murder") {
            score += self->searchConnAng(neighbor) * 3.0f;     // target enemies
        } else if (action->name == "HelpSupport") {
            score += (model ? (100.0f - model->estimatedHappiness) : 50) * 0.02f; // help those suffering
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

        scores.push_back({neighbor, std::max(0.01f, score)});
    }

    return weightedRandomSelect(scores);
}

void implementRegion(){
    std::cout << "\n";
    std::cout << "Choose a world region (h to help): \n";
    std::cout << "1 /// Paris, France | 48 51' 24'' north, 2 21' 07'' east /// Oceanic\n";
    std::cout << "2 /// Philadelphia, United States | 39 57' 10'' north, 75 09' 49'' west /// Continental\n";
    std::cout << "3 /// Guangzhou, China | 23 07' 48'' north, 113 15' 36'' east /// monsoon\n";
    std::cout << "4 /// Addis-Abeba, Ethiopia | 9 1' 48'' north, 38 44' 24'' east /// arid\n";
    std::cout << ">";

    char choice;
    std::cin >> choice;
    if(choice == 'h'){
        std::cout << "This feature is principally implemented for disease spreading, it barely affects the simulation links or its global functionning\n";
        implementRegion();
    }else{
        // Multiply by 10 so region values are 10/20/30/40 — the formula was designed
        // for single-digit multipliers, not raw ASCII values (49–52).
        Disease::region = (choice - '0') * 10;
    }
}

Entity* weightedRandomSelect(std::vector<std::pair<Entity*, float>> scores){
    float total = 0.0f;
    for (auto& s : scores) total += s.second;
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
    const int baseCap = 5;
    int socialCap = baseCap + (int)(ent->personality.extraversion / 100.0f * 4.0f); // 5–9
    if ((int)ent->list_entityPointedSocial.size() > socialCap) {
        // Prune the weakest bonds first, preserving the strongest relationships.
        std::sort(ent->list_entityPointedSocial.begin(), ent->list_entityPointedSocial.end(),
            [](const entityPointedSocial& a, const entityPointedSocial& b){ return a.social > b.social; });
        ent->list_entityPointedSocial.resize(socialCap);
    }
}


// ── Force-based movement system ──────────────────────────────────────────────
// Runs every frame (outside the UPDATE_FREQUENCY throttle).
void updateMovement(std::vector<Entity*>& entities, float worldW, float worldH, int simDay) {
    const float MAX_FORCE    = 0.9f;
    const float PERSONAL_R   = 38.0f;
    const float PROX_REPEL_R = 130.0f;

    for (Entity* ent : entities) {
        if (ent->entityHealth <= 0.0f) continue;

        float fx = 0.0f, fy = 0.0f;

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
        // Pull toward children (entities whose parent1 or parent2 is this entity)
        for (Entity* other : entities) {
            if (other == ent || other->entityHealth <= 0.0f) continue;
            if (other->parent1 == ent || other->parent2 == ent) {
                familyPull(other);
            }
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

        // ── Sick entity avoidance ─────────────────────────────────────────────
        if (ent->entityDiseaseType == -1) { // only healthy entities flee sick ones
            for (Entity* other : entities) {
                if (other == ent || other->entityDiseaseType == -1) continue;
                float dx = ent->posX - other->posX;
                float dy = ent->posY - other->posY;
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < PROX_REPEL_R && dist > 0.01f) {
                    float mag = (PROX_REPEL_R - dist) / PROX_REPEL_R * 0.6f;
                    fx += (dx / dist) * mag;
                    fy += (dy / dist) * mag;
                }
            }
        }

        // ── Personal space repulsion ──────────────────────────────────────────
        for (Entity* other : entities) {
            if (other == ent || other->entityHealth <= 0.0f) continue;
            float dx = ent->posX - other->posX;
            float dy = ent->posY - other->posY;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < PERSONAL_R && dist > 0.01f) {
                float mag = (PERSONAL_R - dist) / PERSONAL_R * 1.2f;
                fx += (dx / dist) * mag;
                fy += (dy / dist) * mag;
            }
        }

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

        ent->posX += ent->velX;
        ent->posY += ent->velY;

        // ── Clamp to world bounds (above Mind Board) ──────────────────────────
        float minX = 40.0f, maxX = worldW - 40.0f;
        float minY = 40.0f, maxY = worldH * 0.64f - 40.0f;
        ent->posX = std::max(minX, std::min(maxX, ent->posX));
        ent->posY = std::max(minY, std::min(maxY, ent->posY));
    }
}

void applyFreeWill(std::vector<std::vector<Entity*>>& entityGroups, int currentDay){
    EnvironmentalFactors env = generateEnvFactors(currentDay);


    // Process each group of close entities
    for(auto& group : entityGroups){

        std::map<std::string, int> actionCounts;
        int totalRecentActions = 0;

        for(Entity* groupMember : group) {
            if(groupMember->entityHealth <= 0.0f) continue;

            const auto& history = groupMember->getFreeWill().getActionHistory();
            int actionsToCheck = std::min(10, (int)history.size());
            for(int i = 0; i < actionsToCheck; ++i) {
                actionCounts[history[i].actionName]++;
                totalRecentActions++;
            }
        }

        // Calculate prevalence and define norms
        if(totalRecentActions > 0) {
            for(const auto& pair : actionCounts) {
                float prevalence = (float)pair.second / totalRecentActions;
                // Only consider it a norm if it's somewhat common (> 10%)
                if(prevalence > 0.10f) {
                    // Norm pressure scales with prevalence.
                    //currentNorms[pair.first] = SocialNorm(pair.first, prevalence, prevalence);
                    ;
                }
            }
        }

        for(Entity* entity : group){
            //on applique aussi les paramètres de maladies
            applyDisease(entity, group.size(), getNBSickClose(group));

            // Tick grief recovery
            entity->tickGrief(1.0f);

            //lower pheromones
            if(!entity->pheromone.type.empty()){
                entity->pheromone.releasing_level -= BetterRand::genNrInInterval(3.0,6.0);
            }


            if(entity->entityHealth <= 0.0f){
                handleDeath(entity, group);
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
            }

            // Passive recovery: not sick = slow heal (recovery no longer blocked by moderate stress)
            if (entity->entityDiseaseType == -1 && entity->entityHealth < 97.0f)
                entity->entityHealth = std::min(100.0f, entity->entityHealth + 0.04f);

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

                //better neightboor selection
                //int targetIndex = BetterRand::genNrInInterval(0, (int) neighbors.size() - 1);
                //target = neighbors[targetIndex];
                target = selectSocialTarget(entity, neighbors, chosenAction);

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
                            sys.pointedAssimilation(entity, mate, romantic);
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
                            sys.pointedAssimilation(entity, foe, hostile);
                        }
                    }
                }

                // ── Narrative + inner monologue ───────────────────────────────
                {
                    int hour = (currentDay % 60) * 24 / 60;
                    entity->lastActionName = chosenAction->name;
                    entity->lastNarrative  = NarrativeEngine::actionToSentence(
                        entity, chosenAction->name, target, hour, "");
                    entity->innerMonologue = generateDeepMonologue(entity);
                    std::string entry = "[" + NarrativeEngine::formatHour(hour) + "] " + entity->lastNarrative;
                    globalNarrativeLog.push_back(entry);
                    if (globalNarrativeLog.size() > 200) globalNarrativeLog.pop_front();
                }

                // Update relationship based on action
                sys.pointedAssimilation(entity, target, chosenAction);


                // Detect murder: if target just died, trigger grief in all connected entities
                if(targetWasAlive && target->entityHealth <= 0.0f){
                    std::stringstream ss;
                    ss << "DEATH EVENT: " << target->name << " was killed. Propagating grief.";
                    globalLogger->logCmd(ss.str());
                    globalLogger->logDeath(target->entityId, target->name, target->entityAge, "murder by " + entity->name);
                    for(Entity* other : group){
                        if(other == target || other->entityHealth <= 0.0f) continue;
                        bool hadBond = false;
                        for(const auto& s : other->list_entityPointedSocial)
                            if(s.pointedEntity == target && s.social > 8.0f){ hadBond = true; break; }
                        if(!hadBond) for(const auto& d : other->list_entityPointedDesire)
                            if(d.pointedEntity == target && d.desire > 8.0f){ hadBond = true; break; }
                        if(!hadBond) for(const auto& c : other->list_entityPointedCouple)
                            if(c.pointedEntity == target){ hadBond = true; break; }
                        if(hadBond){
                            float intensity = 0.65f + BetterRand::genNrInInterval(0, 25) / 100.0f;
                            other->addGrief(target->entityId, intensity, true);
                        }
                    }
                }
            } else {
                // Execute self-directed action
                sys.executeAction(entity, chosenAction, context);
                entity->saveEntityStats(chosenAction);
                globalLogger->logAction(entity->entityId, entity->name, chosenAction->name, "", "self-directed action");

                // ── Narrative + inner monologue ───────────────────────────────
                {
                    int hour = (currentDay % 60) * 24 / 60;
                    entity->lastActionName = chosenAction->name;
                    entity->lastNarrative  = NarrativeEngine::actionToSentence(
                        entity, chosenAction->name, nullptr, hour, "");
                    entity->innerMonologue = generateDeepMonologue(entity);
                    std::string entry = "[" + NarrativeEngine::formatHour(hour) + "] " + entity->lastNarrative;
                    globalNarrativeLog.push_back(entry);
                    if (globalNarrativeLog.size() > 200) globalNarrativeLog.pop_front();
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



void sync_clock_stats(Entity* ent, int neighboors){
    FreeWillSystem fs;
    fs.chooseAction(ent);
}


void updateSimulationStep(std::vector<Entity>& entities, std::vector<Entity*>& ent_quad, std::vector<std::vector<Entity*>>& close_entity_together, int& day, int& frameCounter, const int UPDATE_FREQUENCY, bool isPaused, int width, int height, int& selectedEntityIndex, bool& showEntityWindow) {
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
                if (globalLogger) {
                    std::string cause = (e.entityDiseaseType != -1) ? "disease"
                                       : (e.entityAge > 60.0f)       ? "old age"
                                                                     : "hardship";
                    globalLogger->logDeath(e.entityId, e.name, (int)e.entityAge, cause);
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
            }

            // 4. Batch erase — shifts surviving elements; pointers now stale
            entities.erase(
                std::remove_if(entities.begin(), entities.end(),
                    [](const Entity& e){ return e.entityHealth <= 0.0f; }),
                entities.end());

            // 5. Rebuild ent_quad
            ent_quad.clear();
            for (Entity& e : entities) ent_quad.push_back(&e);

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

            // Birthday: one year = 8 sim-ticks. Gated to fire exactly once per
            // tick (this block runs once per UPDATE_FREQUENCY frames); otherwise
            // it ran every frame and aged everyone decades per second, draining
            // health past life expectancy and killing the whole population on entry.
            if((day / UPDATE_FREQUENCY) % 8 == 0 && day > 0){
                for(Entity& ent : entities){
                    ent.IncrementBDay();
                }
            }

            // Recalculate entity groups based on current positions
            close_entity_together = getSocialGroups(ent_quad);

            // Apply free will to all entity groups with current day for context
            applyFreeWill(close_entity_together, day);

            // PersonaSystem: update self-grounding every tick; consolidate memories every 10 ticks
            for (Entity& ent : entities) {
                if (ent.entityHealth <= 0.0f) continue;
                ent.updateSelfGrounding(day);
                if (day % 10 == 0)
                    ent.consolidateMemories(day);
            }

            // ── Civilization tick (every 5 FreeWill updates = ~1 in-game day) ──
            if (globalCivEngine && (day / UPDATE_FREQUENCY) % 5 == 0)
                globalCivEngine->tick(entities, day / UPDATE_FREQUENCY);

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
                // Reserve before push_back so the vector never reallocates —
                // existing Entity* pointedEntity pointers stay valid.
                entities.reserve(entities.size() + new_borns.size());
                for (Entity& ent : new_borns) entities.push_back(ent);
            }
            FreeWillSystem::clear_new_borns();

            // Rebuild ent_quad so new entities appear on map and pointers are fresh
            if (!new_borns.empty()) {
                ent_quad.clear();
                for(int j = 0; j < (int)entities.size(); j++){
                    ent_quad.push_back(&entities[j]);
                }
                // Repair all pointedEntity pointers
                for(Entity& e : entities){
                    for(auto& d : e.list_entityPointedDesire){
                        if(d.pointedEntity){
                            int id = d.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ d.pointedEntity = &other; break; }
                        }
                    }
                    for(auto& a : e.list_entityPointedAnger){
                        if(a.pointedEntity){
                            int id = a.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ a.pointedEntity = &other; break; }
                        }
                    }
                    for(auto& s : e.list_entityPointedSocial){
                        if(s.pointedEntity){
                            int id = s.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ s.pointedEntity = &other; break; }
                        }
                    }
                    for(auto& c : e.list_entityPointedCouple){
                        if(c.pointedEntity){
                            int id = c.pointedEntity->entityId;
                            for(Entity& other : entities)
                                if(other.entityId == id){ c.pointedEntity = &other; break; }
                        }
                    }
                }
            }

            // Export current state to JSON lines for HTML viewer
            exportTickHistory("./src/data/tick_history.jsonl", entities, day);
        }
        day++;
    }
}

void initialiseSDL(std::vector<Entity>& entities, std::vector<Entity*>& ent_quad, std::vector<std::vector<Entity*>>& close_entity_together, int& day, int& frameCounter, const int UPDATE_FREQUENCY, int width, int height, int& selectedEntityIndex, bool& showEntityWindow){
    SDLEngine SDLEngine("ASHB2 DEBUG");
    Image obj(SDLEngine, "assets/background.jpg");

    bool running = true;
    SDL_Event event;
    while (running)
    {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Apply free will and simulation updates
        updateSimulationStep(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, false, width, height, selectedEntityIndex, showEntityWindow);

        SDLEngine.initialiserRendu();
        obj.dessiner(0,0);

        // SDL mode: no spatial positions — entity count indicator only
        SDL_SetRenderDrawColor(SDLEngine.getRenderer(), 255, 255, 255, 255);

        SDLEngine.finaliserRendu();
    }
}


int getRenderingChoice(){
    std::cout << "Choose your rendering method(1 or 2)\n  1-Simple Dots representing entities (=more statistics, less beautiful) \n  2-Graphic rendering with character moving(=less statistics, more beautiful)\n>";
    std::string input;
    std::cin >> input;
    if(input == "1"){
        return 1;
    }else{
        return 2;
    }
}




int main(int argc, char* argv[]) {
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

    // Clear tick history file
    std::ofstream tick_history("./src/data/tick_history.json", std::ios::trunc);
    tick_history.close();

    globalLogger->logCmd("done");
    int entity_num;
    std::cout << "Welcome to Artificial Simulation of Human Behavior (ASHB)\n";
    std::cout << "complete simulation can be found at /data/complete_logs.txt\n";
    std::cout << "you can save and load simulation at any moment\n";
    std::cout << "@author: Komodo \n";
    implementRegion();
    std::cout << "enter entity number (default: 40): ";
    std::cin >> entity_num;
    if (!entity_num){
        entity_num = 40;
    }
    int renderingType = getRenderingChoice();

    // ── World seed (determines the whole planet & history; same seed = same run) ──
    {
        std::string seedInput;
        std::cout << "enter world seed (text or number, blank = random): ";
        std::cin.ignore();
        std::getline(std::cin, seedInput);
        if (seedInput.empty()) {
            g_worldSeed.master = std::random_device{}() ^
                (static_cast<uint64_t>(std::random_device{}()) << 32);
        } else {
            g_worldSeed = WorldSeed::fromString(seedInput);
        }
        std::cout << "world seed = " << g_worldSeed.master << "\n";
        globalLogger->logCmd("world seed = " + std::to_string(g_worldSeed.master));

        // Divergence / chaos level: how wildly history varies between runs.
        std::cout << "chaos level 0.5 (tame) .. 2.0 (wild) [default 1.3]: ";
        std::string chaosInput;
        std::getline(std::cin, chaosInput);
        float chaos = 1.3f;
        if (!chaosInput.empty()) { try { chaos = std::stof(chaosInput); } catch (...) {} }
        chaos = std::max(0.3f, std::min(2.5f, chaos));
        g_worldSeed.divergence.butterfly       = chaos;
        g_worldSeed.divergence.innovationLuck  = 0.7f + chaos * 0.4f;
        g_worldSeed.divergence.catastropheRate = chaos;
        g_worldSeed.divergence.migrationPressure = chaos;
        std::cout << "chaos = " << chaos << "\n";

        // Seed every randomness source deterministically from the master seed.
        BetterRand::reseed(splitmix64(g_worldSeed.master ^ STREAM_SPAWN));
    }

    srand((unsigned)splitmix64(g_worldSeed.master));
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
                int birthYear = -5000 - BetterRand::genNrInInterval(15, 30); // born in stone age
                Entity entity = Entity(
                    count, 15.0f, BetterRand::genNrInInterval(80.0f, 100.0f), BetterRand::genNrInInterval(30.0f, 70.0f), BetterRand::genNrInInterval(0.0f, 50.0f)
                    , BetterRand::genNrInInterval(80.0f, 100.0f), "", BetterRand::genNrInInterval(0.0f, 20.0f), BetterRand::genNrInInterval(0.0f, 20.0f),
                    BetterRand::genNrInInterval(0.0f, 40.0f), BetterRand::genNrInInterval(60.0f, 100.0f), 'A', 0, BetterRand::genNrInInterval(0.0f, 50.0f), -1, nullptr, nullptr, nullptr, nullptr, "happiness", birthYear);

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
                Heritage::UnlinkedNode(&entity);
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

        bool showEntityWindow = false;
        int selectedEntityIndex = -1;
        std::vector<Entity*> ent_quad;
        for(int i=0; i<entities.size(); i++){
            ent_quad.push_back(&entities[i]);
        }

        std::vector<std::vector<Entity*>> close_entity_together = getSocialGroups(ent_quad);

        // ici on applique l'algorithme pour modification stats
        // Note: For now, we'll run this in the main loop instead of a separate thread
        // to avoid threading complexity with the UI
        // std::thread statistics(applyFreeWill, std::ref(close_entity_together));

        int frameCounter = 0;
        int day = FreeWillSystem::day;

        const int UPDATE_FREQUENCY = 60; // Update free will every 60 frames
    if(renderingType == 1){
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

            updateSimulationStep(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, instanceUI.isSimulationPaused(), width, height, selectedEntityIndex, showEntityWindow);

            std::string saveFilename;
            int saveLoadAction = instanceUI.showSaveLoadButtons(saveFilename, day / 60 , entities.size(), UPDATE_FREQUENCY, {});
            if (saveLoadAction == 1) {
                saveGame(saveFilename, entities, day, frameCounter);
            } else if (saveLoadAction == 2) {
                if (loadGame(saveFilename, entities, day, frameCounter)) {
                    // Rebuild pointer vectors after loading
                    ent_quad.clear();
                    for (int j = 0; j < (int)entities.size(); j++) {
                        ent_quad.push_back(&entities[j]);
                    }
                    close_entity_together = getSocialGroups(ent_quad);
                    showEntityWindow = false;
                    selectedEntityIndex = -1;
                }
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
            instanceUI.ShowCivilizationPanel(day / 60);

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
        std::cout << "s";

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwTerminate();
    }else{// sdl rendering
        initialiseSDL(entities, ent_quad, close_entity_together, day, frameCounter, UPDATE_FREQUENCY, width, height, selectedEntityIndex, showEntityWindow);
    }
    return 0;
}
