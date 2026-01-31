#include <string>
#include <vector>
#include <map>
#include <random>
#include <cmath>
#include <algorithm>
#include <memory>
#include <deque>
#include "./header/Entity.h"
#include "./header/FreeWillSystem.h"
#include <iostream>
#include "./header/BetterRand.h"

//class Entity;

    // Calculate memory weight (more recent = more weight)
    float FreeWillSystem::getMemoryWeight(int memoryAge) {
        float decay = std::exp(-memoryAge / 20.0f); // Exponential decay
        return decay;
    }

    // Calculate how well entity meets action requirements
    float FreeWillSystem::calculateRequirementFitness(Entity* entity, const Action& action) {
        float totalFitness = 0.0f;
        float totalWeight = 0.0f;

        for (const auto& req : action.requirements) {
             float currentValue = getEntityStat(entity, req.statName);
            float difference = currentValue - req.requiredValue;

            float fitness;
            if (difference >= 0) {
                fitness = 1.0f; // Full fitness if requirement met
            } else {
                // Partial fitness based on how close we are
                fitness = std::max(0.0f, 1.0f + (difference / req.requiredValue));
            }

            totalFitness += fitness * req.weight;
            totalWeight += req.weight;
        }
        return totalWeight > 0 ? totalFitness / totalWeight : 1.0f;
    }

    // Calculate how much this action addresses current needs
    float FreeWillSystem::calculateNeedSatisfaction(const Action& action) {
        float satisfaction = 0.0f;

        auto needIt = needs.find(action.needCategory);
        if (needIt != needs.end()) {
            // Higher urgency = more satisfaction from doing this action
            satisfaction += needIt->second.urgency * 0.01f * action.baseSatisfaction;
        }

        return satisfaction;
    }

    // Calculate bias from memory (learn from past experiences)
    float FreeWillSystem::calculateMemoryBias(int actionId) {
        float bias = 0.0f;
        float totalWeight = 0.0f;
        int memoryIndex = 0;

        for (auto it = actionHistory.rbegin(); it != actionHistory.rend(); ++it) {
            if (it->actionId == actionId) {
                float weight = getMemoryWeight(memoryIndex);
                bias += it->outcomeSuccess * weight;
                totalWeight += weight;
            }
            memoryIndex++;
        }

        return totalWeight > 0 ? bias / totalWeight : 0.5f; // Default neutral bias
    }

    // Calculate variety bonus (avoid repetition)
    float FreeWillSystem::calculateVarietyBonus(int actionId) {
        int recentCount = 0;
        int checkDepth = std::min(10, (int)actionHistory.size());

        for (int i = 0; i < checkDepth; i++) {
            if (actionHistory[i].actionId == actionId) {
                recentCount++;
            }
        }

        // Penalty for repeated actions
        return std::max(0.0f, 1.0f - (recentCount * 0.2f));
    }


    FreeWillSystem::FreeWillSystem() : currentTime(0), rng(std::random_device{}()) {
        initializeNeeds();
        initializeActions();
    }

    void FreeWillSystem::initializeNeeds() {
        needs["social"] = Need("social", 0.15f);
        needs["social"].satisfyingCategories = {"social", "entertainment"};

        needs["health"] = Need("health", 0.08f);
        needs["health"].satisfyingCategories = {"health", "food", "sleep"};

        needs["hygiene"] = Need("hygiene", 0.12f);
        needs["hygiene"].satisfyingCategories = {"hygiene"};

        needs["safety"] = Need("safety", 0.05f);
        needs["safety"].satisfyingCategories = {"safety", "health"};

        needs["happiness"] = Need("happiness", 0.1f);
        needs["happiness"].satisfyingCategories = {"entertainment", "social", "achievement"};
    }

    //implementation of a list of action
    void FreeWillSystem::initializeActions() {
        //
        //POINTED actions (require a target entity)
        Action socialize("Socialize", 1, "social");
        socialize.requirements = {
            {"loneliness", 30.0f, 0.8f},
            {"stress", 30.0f, 0.3f}
        };
        socialize.statChanges = {
            {"loneliness", -7.0f},
            {"happiness", 6.0f},
            {"stress", -4.0f},
            {"boredom", -9.0f}
        };
        socialize.baseSatisfaction = 20.0f;
        availableActions.push_back(socialize);

        Action desire("Desire", 2, "social");
        desire.requirements = {
            {"loneliness", 40.0f, 0.9f},
            {"happiness", 40.0f, 0.6f}
        };
        desire.statChanges = {
            {"loneliness", -12.0f},
            {"happiness", 13.0f},
            {"stress", -6.0f}
        };
        desire.baseSatisfaction = 30.0f;
        availableActions.push_back(desire);

        Action goodconn("GoodConnection", 3, "social");
        goodconn.requirements = {
            {"happiness", 50.0f, 0.7f},
            {"loneliness", 30.0f, 0.8f}
        };
        goodconn.statChanges = {
            {"happiness", 13.0f},
            {"loneliness", -14.0f},
            {"mentalHealth", 9.0f},
            {"stress", -4.0f}
        };
        goodconn.baseSatisfaction = 35.0f;
        availableActions.push_back(goodconn);

        Action angconn("AngerConnection", 4, "social");
        angconn.requirements = {
            {"anger", 60.0f, 0.9f},
            {"stress", 50.0f, 0.7f}
        };
        angconn.statChanges = {
            {"anger", 13.0f},
            {"stress", 17.0f},
            {"happiness", -12.0f},
            {"mentalHealth", -7.0f}
        };
        angconn.baseSatisfaction = 15.0f;
        availableActions.push_back(angconn);

        // Extreme actions requiring high negative stats
        Action murder("Murder", 5, "safety");
        murder.requirements = {
            {"anger", 80.0f, 1.0f},
            {"mentalHealth", 20.0f, 0.9f},
            {"stress", 70.0f, 0.8f}
        };
        murder.statChanges = {
            {"anger", -33.0f},
            {"mentalHealth", -23.0f},
            {"stress", 17.0f},
            {"happiness", 3.0f}
        };
        murder.baseSatisfaction = 10.0f;
        availableActions.push_back(murder);

        Action discrimination("Discrimination", 6, "social");
        discrimination.requirements = {
            {"anger", 30.0f, 0.8f},
            {"mentalHealth", 20.0f, 0.6f},
            {"stress", 30.0f, 0.5f}
        };
        discrimination.statChanges = {
            {"anger", -14.0f},
            {"mentalHealth", -12.0f},
            {"stress", 5.0f},
            {"happiness", -1.0f}
        };
        discrimination.baseSatisfaction = 5.0f;
        availableActions.push_back(discrimination);

        // Self-harm actions
        Action suicide("Suicide", 7, "safety");
        suicide.requirements = {
            {"mentalHealth", 10.0f, 1.0f},
            {"stress", 90.0f, 1.0f},
            {"happiness", 5.0f, 0.9f}
        };
        suicide.statChanges = {
            {"health", -100.0f},
            {"mentalHealth", -100.0f}
        };
        suicide.baseSatisfaction = 0.0f;
        availableActions.push_back(suicide);

        Action anxiety("Anxiety", 8, "health");
        anxiety.requirements = {
            {"stress", 60.0f, 0.9f},
            {"mentalHealth", 40.0f, 0.7f}
        };
        anxiety.statChanges = {
            {"stress", 17.0f},
            {"mentalHealth", -10.0f},
            {"health", -7.0f},
            {"happiness", -10.0f}
        };
        anxiety.baseSatisfaction = 5.0f;
        availableActions.push_back(anxiety);

        // Positive social action
        Action breeding("Breeding", 9, "social");
        breeding.requirements = {
            {"happiness", 60.0f, 0.8f},
            {"health", 60.0f, 0.7f},
            {"stress", 40.0f, 0.5f}
        };
        breeding.statChanges = {
            {"happiness", 35.0f},
            {"stress", -15.0f},
            {"loneliness", -20.0f},
            {"health", -10.0f}
        };
        breeding.baseSatisfaction = 40.0f;
        availableActions.push_back(breeding);

        // Health actions
        Action exercise("Exercise", 10, "health");
        exercise.requirements = {
            {"health", 30.0f, 0.7f},
            {"stress", 60.0f, 0.4f}
        };
        exercise.statChanges = {
            {"health", 10.0f},
            {"stress", -10.0f},
            {"happiness", 10.0f},
            {"boredom", -10.0f}
        };
        exercise.baseSatisfaction = 20.0f;
        availableActions.push_back(exercise);

        // Hygiene actions
        Action shower("Take Shower", 11, "hygiene");
        shower.requirements = {
            {"hygiene", 50.0f, 0.9f}
        };
        shower.statChanges = {
            {"hygiene", 30.0f},
            {"happiness", 5.0f},
            {"stress", -5.0f}
        };
        shower.baseSatisfaction = 15.0f;
        availableActions.push_back(shower);

        // Rest action
        Action rest("Rest", 12, "health");
        rest.requirements = {
            {"stress", 60.0f, 0.7f},
            {"health", 40.0f, 0.5f}
        };
        rest.statChanges = {
            {"stress", -20.0f},
            {"health", 15.0f},
            {"mentalHealth", 10.0f},
            {"boredom", 10.0f}
        };
        rest.baseSatisfaction = 20.0f;
        availableActions.push_back(rest);

        // Work/Achievement action
        Action work("Work on Project", 13, "achievement");
        work.requirements = {
            {"stress", 40.0f, 0.5f},
            {"health", 50.0f, 0.4f}
        };
        work.statChanges = {
            {"stress", 17.0f},
            {"happiness", 15.0f},
            {"boredom", -20.0f},
            {"loneliness", 10.0f}
        };
        work.baseSatisfaction = 25.0f;
        availableActions.push_back(work);
    }

    // Calculate social influence from neighbors
    float FreeWillSystem::calculateSocialInfluence(Entity* entity, const std::vector<Entity*>& neighbors, const Action& action) {
        if (neighbors.empty()) return 0.5f; // Neutral if no neighbors

        float influence = 0.0f;
        float totalWeight = 0.0f;

        for (Entity* neighbor : neighbors) {
            // Check existing relationships
            auto desireList = entity->getListDesire();
            auto angerList = entity->getListAnger();
            auto socialList = entity->getListSocial();

            float relationshipWeight = 0.0f;

            // Positive relationships increase weight for positive actions
            for (const auto& social : socialList) {
                if (social.pointedEntity == neighbor) {
                    relationshipWeight += social.social * 0.1f;
                }
            }

            for (const auto& desire : desireList) {
                if (desire.pointedEntity == neighbor) {
                    relationshipWeight += desire.desire * 0.15f;
                }
            }

            // Negative relationships increase weight for negative actions
            for (const auto& anger : angerList) {
                if (anger.pointedEntity == neighbor) {
                    relationshipWeight -= anger.anger * 0.17f;
                }
            }

            // Neighbors with similar stats influence action choices
            float neighborMentalHealth = neighbor->entityMentalHealth;
            float neighborAnger = neighbor->entityGeneralAnger;
            float neighborHappiness = neighbor->entityHapiness;

            // If neighbors are angry/stressed, negative actions become more likely
            if (action.name == "Murder" || action.name == "Discrimination" || action.name == "AngerConnection") {
                influence += (neighborAnger / 100.0f) * 0.3f;
            }
            // If neighbors are happy/healthy, positive actions become more likely
            else if (action.name == "Socialize" || action.name == "GoodConnection" || action.name == "Breeding") {
                influence += (neighborHappiness / 100.0f) * 0.3f;
            }

            totalWeight += 1.0f + relationshipWeight;
        }

        return totalWeight > 0 ? (influence / neighbors.size()) : 0.5f;
    }

        // Main decision-making function
        // main entry = entree principale de fichier
    Action* FreeWillSystem::chooseAction(Entity* entity, const std::vector<Entity*>& neighbors) {
            std::cout << "\n=== Choosing Action for Entity " << entity->getId() << " ===\n";
            std::cout << "Number of neighbors: " << neighbors.size() << "\n";

            std::vector<std::pair<Action*, float>> actionWeights;

            for (auto& action : availableActions) {
                float requirementFitness = calculateRequirementFitness(entity, action);
                float needSatisfaction = calculateNeedSatisfaction(action);
                float memoryBias = calculateMemoryBias(action.actionId);
                float varietyBonus = calculateVarietyBonus(action.actionId);
                float socialInfluence = calculateSocialInfluence(entity, neighbors, action);

                std::cout << "\nAction: " << action.name << "\n";
                std::cout << "  RequirementFitness: " << requirementFitness << "\n";
                std::cout << "  NeedSatisfaction:   " << needSatisfaction << "\n";
                std::cout << "  MemoryBias:         " << memoryBias << "\n";
                std::cout << "  VarietyBonus:       " << varietyBonus << "\n";
                std::cout << "  SocialInfluence:    " << socialInfluence << "\n";

                float weight =
                    requirementFitness * 0.25f +
                    needSatisfaction * 0.35f +
                    memoryBias * 0.15f +
                    varietyBonus * 0.1f +
                    socialInfluence * 0.15f;

                // Apply rarity multipliers for extreme actions
                float rarityMultiplier = 1.0f;
                if (action.name == "Murder") {
                    rarityMultiplier = 0.05f; // 5% probability weight
                } else if (action.name == "Suicide") {
                    rarityMultiplier = 0.02f; // 2% probability weight
                } else if (action.name == "Discrimination") {
                    rarityMultiplier = 0.3f; // 30% probability weight
                } else if (action.name == "Anxiety") {
                    rarityMultiplier = 0.4f; // 40% probability weight
                }

                weight *= rarityMultiplier;

                std::uniform_real_distribution<float> dist(0.8f, 1.2f);
                float randomFactor = dist(rng);
                weight *= randomFactor;

                std::cout << "  RarityMultiplier:   " << rarityMultiplier << "\n";
                std::cout << "  Combined Weight (pre-sort): " << weight
                        << " (RandomFactor: " << randomFactor << ")\n";

                actionWeights.push_back({&action, weight});
            }

            std::sort(actionWeights.begin(), actionWeights.end(),
                    [](const auto& a, const auto& b) { return a.second > b.second; });

            std::cout << "\n-- Sorted Action Weights --\n";
            for (auto& aw : actionWeights) {
                std::cout << "  " << aw.first->name << ": " << aw.second << "\n";
            }

            float totalWeight = 0.0f;
            for (const auto& aw : actionWeights) totalWeight += aw.second;

            std::uniform_real_distribution<float> selectDist(0.0f, totalWeight);
            float selection = selectDist(rng);
            std::cout << "\nTotalWeight: " << totalWeight
                    << " | SelectionPoint: " << selection << "\n";

            float cumulative = 0.0f;
            for (const auto& aw : actionWeights) {
                cumulative += aw.second;
                if (selection <= cumulative) {
                    std::cout << ">>> Chosen Action: " << aw.first->name << " <<<\n";
                    return aw.first;
                }
            }

            std::cout << ">>> Default fallback to: " << availableActions[0].name << " <<<\n";
            return &availableActions[0];
        }

    //ici on assimile l'action pointé vers sur celui qui est pointé
    //par le pointeur
    void FreeWillSystem::pointedAssimilation(Entity* pointer, Entity* pointed, Action* action){
        if(action->name == "Desire"){
            // Natural attraction checks - cannot be attracted if target has poor hygiene or is unhappy
            if(pointed->entityHygiene < 40){
                std::cout << "Desire bloqué: " << pointed->getName() << " a une hygiène trop basse (" << pointed->entityHygiene << ")\n";
                return;
            }
            if(pointed->entityHapiness < 30){
                std::cout << "Desire bloqué: " << pointed->getName() << " est trop malheureux (" << pointed->entityHapiness << ")\n";
                return;
            }
            if(pointed->entityHealth < 30){
                std::cout << "Desire bloqué: " << pointed->getName() << " est en mauvaise santé (" << pointed->entityHealth << ")\n";
                return;
            }
            // Check if pointer has anger toward pointed - anger blocks desire
            int anger_index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
            if(anger_index != -1 && pointer->list_entityPointedAnger[anger_index].anger > 15){
                std::cout << "Desire bloqué: " << pointer->getName() << " a trop de colère envers " << pointed->getName() << "\n";
                return;
            }

            int index = pointer->contains(pointer->list_entityPointedDesire, pointed, 1);
            if(index == -1){ // n'as pas de desire, créer un nouveau
                // Desire starts lower, based on target's attractiveness factors
                float attractiveness = (pointed->entityHygiene / 100.0f) * 0.3f +
                                       (pointed->entityHapiness / 100.0f) * 0.4f +
                                       (pointed->entityHealth / 100.0f) * 0.3f;
                float desire = static_cast<float>(BetterRand::genNrInInterval(1,3)) * attractiveness;
                std::cout << "Nouveau lien de desire ajouté entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " " << desire << std::endl;
                pointer->addDesire({1, pointed, desire});
            }else{ //le désire existe déjà
                int index_social = pointer->contains(pointer->list_entityPointedSocial, pointed, 1);
                int borne_haut = 3;
                if(index_social != -1){ //si a des liens social augmenté le désir
                    borne_haut += (pointer->list_entityPointedSocial[index_social].social / 15);
                }
                float increment = static_cast<float>(BetterRand::genNrInInterval(1, borne_haut));
                // Reduce increment if target's attractiveness is low
                float attractiveness = (pointed->entityHygiene / 100.0f + pointed->entityHapiness / 100.0f) / 2.0f;
                increment *= attractiveness;
                pointer->list_entityPointedDesire[index].desire += increment;
                std::cout << "Desire renforcé entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " +" << increment << std::endl;
            }
        }else if(action->name == "AngerConnection"){
            int index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
            if(index == -1){
                float anger = static_cast<float>(BetterRand::genNrInInterval(1,5));
                std::cout << "Nouveau lien anger ajouté entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " " << anger << std::endl;
                pointer->addAnger({1, pointed, anger});
            }else{
                float increment = static_cast<float>(BetterRand::genNrInInterval(1,5));
                if(pointed->entityDiseaseType != -1){
                    //on ajoute un poids mauvais si une personne est malade
                    pointer->list_entityPointedAnger[index].anger += increment + BetterRand::genNrInInterval(1,4);
                }else{
                    pointer->list_entityPointedAnger[index].anger += increment;
                }
                pointer->list_entityPointedAnger[index].anger += increment;
                std::cout << "Anger renforcé entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " +" << increment << std::endl;
            }
        }else if(action->name == "Socialize"){
            int index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
            if(index == -1){
                float social = static_cast<float>(BetterRand::genNrInInterval(1,2));
                std::cout << "Nouveau lien social ajouté entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " " <<social << std::endl;
                pointer->addSocial({1, pointed, social});
            }else{
                //Ici on choisit le lien social mais lorsqu'il sociabilise il se rend compte qu'il est malade donc on le
                //met de poid plus fort
                float increment = static_cast<float>(BetterRand::genNrInInterval(1,2));
                if(pointed->entityDiseaseType != -1){
                    pointer->list_entityPointedSocial[index].social += increment - BetterRand::genNrInInterval(2,6);
                }else{
                    pointer->list_entityPointedSocial[index].social += increment;
                }
                std::cout << "Social renforcé entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " +" << increment << std::endl;
            }
        }else if(action->name == "GoodConnection"){
            int index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
            if(index == -1){
                float social = static_cast<float>(BetterRand::genNrInInterval(2,4));
                std::cout << "Nouveau lien social (good) ajouté entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " " <<social << std::endl;
                pointer->addSocial({1, pointed, social});
            }else{
                float increment = static_cast<float>(BetterRand::genNrInInterval(2,4));
                pointer->list_entityPointedSocial[index].social += increment;
                std::cout << "Social (good) renforcé entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " +" << increment << std::endl;
            }
        }else if(action->name == "Breeding"){
            // Check if pointer has high enough desire for pointed (minimum 25)
            int desire_index = pointer->contains(pointer->list_entityPointedDesire, pointed, 1);
            if(desire_index == -1 || pointer->list_entityPointedDesire[desire_index].desire < 25){
                float current_desire = (desire_index == -1) ? 0 : pointer->list_entityPointedDesire[desire_index].desire;
                std::cout << "Couple bloqué: " << pointer->getName() << " n'a pas assez de désir pour " << pointed->getName() << " (" << current_desire << " < 25)\n";
                return;
            }
            // Check if pointed also has desire for pointer (mutual attraction - minimum 20)
            int pointed_desire_index = pointed->contains(pointed->list_entityPointedDesire, pointer, 1);
            if(pointed_desire_index == -1 || pointed->list_entityPointedDesire[pointed_desire_index].desire < 20){
                float pointed_desire = (pointed_desire_index == -1) ? 0 : pointed->list_entityPointedDesire[pointed_desire_index].desire;
                std::cout << "Couple bloqué: " << pointed->getName() << " n'a pas assez de désir pour " << pointer->getName() << " (" << pointed_desire << " < 20)\n";
                return;
            }
            // Check that neither has too much anger toward the other
            int anger_index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
            if(anger_index != -1 && pointer->list_entityPointedAnger[anger_index].anger > 10){
                std::cout << "Couple bloqué: " << pointer->getName() << " a trop de colère envers " << pointed->getName() << "\n";
                return;
            }
            int pointed_anger_index = pointed->contains(pointed->list_entityPointedAnger, pointer, 2);
            if(pointed_anger_index != -1 && pointed->list_entityPointedAnger[pointed_anger_index].anger > 10){
                std::cout << "Couple bloqué: " << pointed->getName() << " a trop de colère envers " << pointer->getName() << "\n";
                return;
            }
            // Check both have good social link (minimum 15)
            int social_index = pointer->contains(pointer->list_entityPointedSocial, pointed, 4);
            if(social_index == -1 || pointer->list_entityPointedSocial[social_index].social < 15){
                float current_social = (social_index == -1) ? 0 : pointer->list_entityPointedSocial[social_index].social;
                std::cout << "Couple bloqué: lien social insuffisant entre " << pointer->getName() << " et " << pointed->getName() << " (" << current_social << " < 15)\n";
                return;
            }

            int index = pointer->contains(pointer->list_entityPointedCouple, pointed, 3);
            if(index == -1){
                std::cout << "Nouveau couple ajouté entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << std::endl;
                pointer->addCouple({1, pointed});
                // Also add the reverse couple link for the pointed entity
                pointed->addCouple({1, pointer});
            }else{
                std::cout << "INFO: Couple existe déjà, renforcement du lien\n" ;
            }
        }else if(action->name == "Murder"){
            std::cout << "MURDER: (" << pointer->getId() << ")" << pointer->getName()<< " a tué (" << pointed->getId() << ")" << pointed->getName() << std::endl;
            // Mark pointed entity as dead (health = 0)
            pointed->entityHealth = 0.0f;
        }else if(action->name == "Discrimination"){
            int index = pointer->contains(pointer->list_entityPointedAnger, pointed, 2);
            if(index == -1){
                float anger = static_cast<float>(BetterRand::genNrInInterval(3,8));
                std::cout << "Discrimination: lien anger ajouté entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " " << anger << std::endl;
                pointer->addAnger({1, pointed, anger});
            }else{
                float increment = static_cast<float>(BetterRand::genNrInInterval(2,6));
                pointer->list_entityPointedAnger[index].anger += increment;
                std::cout << "Discrimination renforcée entre: (" << pointer->getId() << ")" << pointer->getName()<< " -> (" << pointed->getId() << ")" << pointed->getName() << " +" << increment << std::endl;
            }
        }
    }

        // Execute chosen action
    void FreeWillSystem::executeAction(Entity* entity, Action* &action, Entity* pointed) {
        std::cout << "\n=== Executing Action: " << action->name << " ===\n";

        std::map<std::string, float> statsBefore = captureEntityStats(entity);
        std::cout << "Stats Before:\n";
        for (auto& [k, v] : statsBefore) std::cout << "  " << k << ": " << v << "\n";

        for (const auto& change : action->statChanges) {
            float currentValue = getEntityStat(entity, change.statName);
            float newValue = currentValue + BetterRand::genNrInInterval(change.changeValue -4, change.changeValue+4); // nouvelle valeur appliqué
            //on fait une variation de 4
            setEntityStat(entity, change.statName, newValue);
            std::cout << "  Changed " << change.statName << ": " << currentValue
                    << " -> " << newValue << "\n";
        }

        auto needIt = needs.find(action->needCategory);
        if (needIt != needs.end()) {
            std::cout << "Satisfying need category: " << action->needCategory << "\n";
            needIt->second.satisfy(action->baseSatisfaction);
        }

        std::map<std::string, float> statsAfter = captureEntityStats(entity);
        std::cout << "Stats After:\n";
        for (auto& [k, v] : statsAfter) std::cout << "  " << k << ": " << v << "\n";

        float outcomeSuccess = calculateOutcomeSuccess(statsBefore, statsAfter);
        action->outcomeSuccess = outcomeSuccess;
        std::cout << "Outcome Success: " << outcomeSuccess << "\n";

        ActionMemory memory;
        memory.actionId = action->actionId;
        memory.actionName = action->name;
        memory.timestamp = currentTime;
        memory.outcomeSuccess = outcomeSuccess;
        memory.statsBefore = statsBefore;
        memory.statsAfter = statsAfter;

        actionHistory.push_front(memory);
        if (actionHistory.size() > MAX_MEMORY) actionHistory.pop_back();

        currentTime++;
        std::cout << "Action completed. Memory recorded. Time now: " << currentTime << "\n";
    }

    // Update needs over time
    void FreeWillSystem::updateNeeds(float deltaTime) {
        for (auto& needPair : needs) {
            needPair.second.update(deltaTime);
        }
    }

    // Add custom action
    void FreeWillSystem::addAction(const Action& action) {
        availableActions.push_back(action);
    }

    // Get action history
    const std::deque<ActionMemory>& FreeWillSystem::getActionHistory() const {
        return actionHistory;
    }

    // Get current needs
    const std::map<std::string, Need>& FreeWillSystem::getNeeds() const {
        return needs;
    }

    std::map<std::string, float> FreeWillSystem::captureEntityStats(Entity* entity) {
        return {
            {"health", entity->entityHealth},
            {"happiness", entity->entityHapiness},
            {"stress", entity->entityStress},
            {"mentalHealth", entity->entityMentalHealth},
            {"loneliness", entity->entityLoneliness},
            {"boredom", entity->entityBoredom},
            {"anger", entity->entityGeneralAnger},
            {"hygiene", (float)entity->entityHygiene}
        };
    }

    float FreeWillSystem::calculateOutcomeSuccess(const std::map<std::string, float>& before,
                                   const std::map<std::string, float>& after) {
        // Compare positive stats (higher is better)
        float success = 0.0f;
        int count = 0;

        std::vector<std::string> positiveStats = {"health", "happiness", "mentalHealth", "hygiene"};
        for (const auto& stat : positiveStats) {
            if (after.at(stat) > before.at(stat)) success += 1.0f;
            count++;
        }

        std::vector<std::string> negativeStats = {"stress", "loneliness", "boredom", "anger"};
        for (const auto& stat : negativeStats) {
            if (after.at(stat) < before.at(stat)) success += 1.0f;
            count++;
        }

        return count > 0 ? success / count : 0.5f;
    }

// Implementation of stat getAter/setter
float FreeWillSystem::getEntityStat(Entity* entity, const std::string& statName) {
    if (statName == "health") return entity->entityHealth;
    if (statName == "happiness") return entity->entityHapiness;
    if (statName == "stress") return entity->entityStress;
    if (statName == "mentalHealth") return entity->entityMentalHealth;
    if (statName == "loneliness") return entity->entityLoneliness;
    if (statName == "boredom") return entity->entityBoredom;
    if (statName == "anger") return entity->entityGeneralAnger;
    if (statName == "hygiene") return (float)entity->entityHygiene;
    return 0.0f;
}

void FreeWillSystem::setEntityStat(Entity* entity, const std::string& statName, float value) {
    if (statName == "health") entity->entityHealth = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "happiness") entity->entityHapiness = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "stress") entity->entityStress = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "mentalHealth") entity->entityMentalHealth = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "loneliness") entity->entityLoneliness = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "boredom") entity->entityBoredom = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "anger") entity->entityGeneralAnger = std::max(0.0f, std::min(100.0f, value));
    else if (statName == "hygiene") entity->entityHygiene = (int)std::max(0.0f, std::min(100.0f, value));
}
