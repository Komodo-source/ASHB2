#include "BehavioralModule.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace modules {

// ============= Utility Functions =============

namespace utils {

float sigmoid(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

float softmax(const std::vector<float>& values, size_t index) {
    if (index >= values.size()) return 0.0f;
    
    float maxVal = *std::max_element(values.begin(), values.end());
    float sum = 0.0f;
    
    for (float v : values) {
        sum += std::exp(v - maxVal);
    }
    
    return std::exp(values[index] - maxVal) / sum;
}

float clamp(float value, float min, float max) {
    return std::max(min, std::min(max, value));
}

} // namespace utils

// ============= RationalChoiceModel Implementation =============

Action* RationalChoiceModel::decideAction(Entity* entity,
                                          const std::vector<Entity*>& neighbors,
                                          const ActionContext& context) {
    // Placeholder: would implement expected utility calculation
    // For now, return nullptr to indicate no decision made
    return nullptr;
}

float RationalChoiceModel::calculateExpectedUtility(Entity* entity, 
                                                     const Action& action,
                                                     const ActionContext& context) {
    // Expected utility = probability of success * value of outcome - cost
    float probability = getParameter("success_probability");
    float value = getParameter("outcome_value");
    float cost = getParameter("action_cost");
    
    return probability * value - cost;
}

void RationalChoiceModel::saveState(std::ostream& out) const {
    out << "RationalChoiceModel\n";
    for (const auto& [key, value] : parameters) {
        out << key << "=" << value << "\n";
    }
}

void RationalChoiceModel::loadState(std::istream& in) {
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line == "RationalChoiceModel") continue;
        
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            double value = std::stod(line.substr(eq + 1));
            parameters[key] = value;
        }
    }
}

// ============= EmotionalDecisionModel Implementation =============

Action* EmotionalDecisionModel::decideAction(Entity* entity,
                                             const std::vector<Entity*>& neighbors,
                                             const ActionContext& context) {
    // Placeholder: would implement emotion-based decision making
    return nullptr;
}

float EmotionalDecisionModel::calculateEmotionalImpulse(Entity* entity, 
                                                         const Action& action) {
    // Impulse strength based on emotional state
    float anger = getParameter("anger_level");
    float fear = getParameter("fear_level");
    float joy = getParameter("joy_level");
    
    // Different actions triggered by different emotions
    return anger + fear + joy;
}

void EmotionalDecisionModel::saveState(std::ostream& out) const {
    out << "EmotionalDecisionModel\n";
    for (const auto& [key, value] : parameters) {
        out << key << "=" << value << "\n";
    }
}

void EmotionalDecisionModel::loadState(std::istream& in) {
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line == "EmotionalDecisionModel") continue;
        
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            double value = std::stod(line.substr(eq + 1));
            parameters[key] = value;
        }
    }
}

// ============= SocialLearningModel Implementation =============

Action* SocialLearningModel::decideAction(Entity* entity,
                                          const std::vector<Entity*>& neighbors,
                                          const ActionContext& context) {
    observeNeighbors(entity, neighbors);
    
    // Placeholder: would return action based on social learning
    return nullptr;
}

void SocialLearningModel::observeNeighbors(Entity* entity,
                                           const std::vector<Entity*>& neighbors) {
    // Observe and learn from neighbor behaviors
    for (auto* neighbor : neighbors) {
        // Would track what actions neighbors are taking
        // and their outcomes
    }
}

float SocialLearningModel::calculateImitationProbability(Entity* entity, 
                                                          Entity* model) {
    // Probability of imitating based on model's success and similarity
    float similarity = getParameter("similarity_to_model");
    float modelSuccess = getParameter("model_success_rate");
    float prestige = getParameter("model_prestige");
    
    return utils::sigmoid(similarity * 0.5 + modelSuccess * 0.3 + prestige * 0.2);
}

void SocialLearningModel::saveState(std::ostream& out) const {
    out << "SocialLearningModel\n";
    for (const auto& [key, value] : parameters) {
        out << key << "=" << value << "\n";
    }
}

void SocialLearningModel::loadState(std::istream& in) {
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line == "SocialLearningModel") continue;
        
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            double value = std::stod(line.substr(eq + 1));
            parameters[key] = value;
        }
    }
}

// ============= HybridDecisionModel Implementation =============

void HybridDecisionModel::addSubModel(std::shared_ptr<BehavioralPlugin> model, 
                                       double weight) {
    subModels.push_back(model);
    modelWeights[model->getName()] = weight;
}

Action* HybridDecisionModel::decideAction(Entity* entity,
                                          const std::vector<Entity*>& neighbors,
                                          const ActionContext& context) {
    if (subModels.empty()) return nullptr;
    
    // Each model proposes an action with a confidence score
    std::vector<float> scores;
    std::vector<Action*> actions;
    
    for (const auto& model : subModels) {
        Action* action = model->decideAction(entity, neighbors, context);
        actions.push_back(action);
        
        double weight = modelWeights.count(model->getName()) ? 
                       modelWeights[model->getName()] : 1.0;
        scores.push_back(static_cast<float>(weight));
    }
    
    // Select action using weighted voting
    if (actions.empty()) return nullptr;
    
    size_t bestIdx = 0;
    float bestScore = scores[0];
    
    for (size_t i = 1; i < scores.size(); ++i) {
        if (scores[i] > bestScore) {
            bestScore = scores[i];
            bestIdx = i;
        }
    }
    
    return actions[bestIdx];
}

void HybridDecisionModel::saveState(std::ostream& out) const {
    out << "HybridDecisionModel\n";
    out << "num_models=" << subModels.size() << "\n";
    for (const auto& [key, value] : modelWeights) {
        out << "weight_" << key << "=" << value << "\n";
    }
}

void HybridDecisionModel::loadState(std::istream& in) {
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line == "HybridDecisionModel") continue;
        
        if (line.find("num_models=") == 0) continue;
        
        if (line.find("weight_") == 0) {
            size_t eq = line.find('=');
            std::string key = line.substr(7, eq - 7); // Skip "weight_"
            double value = std::stod(line.substr(eq + 1));
            modelWeights[key] = value;
        }
    }
}

// ============= PluginManager Implementation =============

PluginManager& PluginManager::getInstance() {
    static PluginManager instance;
    return instance;
}

void PluginManager::registerPlugin(const std::string& name,
                                    std::shared_ptr<BehavioralPlugin> plugin) {
    plugins[name] = plugin;
    if (!activePlugin) {
        activePlugin = plugin;
    }
}

void PluginManager::setActivePlugin(const std::string& name) {
    auto it = plugins.find(name);
    if (it != plugins.end()) {
        activePlugin = it->second;
    }
}

std::shared_ptr<BehavioralPlugin> PluginManager::getActivePlugin() const {
    return activePlugin;
}

std::vector<std::string> PluginManager::getAvailablePlugins() const {
    std::vector<std::string> names;
    for (const auto& [name, plugin] : plugins) {
        names.push_back(name);
    }
    return names;
}

Action* PluginManager::executeDecision(Entity* entity,
                                        const std::vector<Entity*>& neighbors,
                                        const ActionContext& context) {
    if (!activePlugin) return nullptr;
    return activePlugin->decideAction(entity, neighbors, context);
}

// ============= DecisionTreeExecutor Implementation =============

void DecisionTreeExecutor::setRoot(std::shared_ptr<DecisionNode> node) {
    root = node;
}

std::string DecisionTreeExecutor::evaluate(Entity* entity, 
                                            const ActionContext& context) {
    if (!root) return "";
    
    auto current = root;
    while (current && !current->isLeaf()) {
        if (current->predicate && current->predicate(entity, context)) {
            current = current->trueBranch;
        } else {
            current = current->falseBranch;
        }
    }
    
    return current ? current->actionName : "";
}

void DecisionTreeExecutor::buildFromConfig(const std::string& configJson) {
    // Placeholder: would parse JSON configuration and build tree
    // For now, create a simple dummy tree
    root = std::make_shared<DecisionNode>();
    root->condition = "default";
    root->actionName = "DefaultAction";
}

} // namespace modules
