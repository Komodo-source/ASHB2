#ifndef BEHAVIORAL_MODULE_H
#define BEHAVIORAL_MODULE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <any>

// Forward declarations
class Entity;
struct Action;
struct ActionContext;

namespace modules {

// Plugin interface for behavioral models
class BehavioralPlugin {
public:
    virtual ~BehavioralPlugin() = default;
    
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual void initialize() = 0;
    virtual void shutdown() = 0;
    
    // Core decision logic
    virtual Action* decideAction(Entity* entity, 
                                 const std::vector<Entity*>& neighbors,
                                 const ActionContext& context) = 0;
    
    // Configuration
    virtual void setParameter(const std::string& name, double value) = 0;
    virtual double getParameter(const std::string& name) const = 0;
    
    // Serialization
    virtual void saveState(std::ostream& out) const = 0;
    virtual void loadState(std::istream& in) = 0;
};

// Base class for specific behavioral models
class BehavioralModelBase : public BehavioralPlugin {
protected:
    std::map<std::string, double> parameters;
    bool initialized = false;
    
public:
    void setParameter(const std::string& name, double value) override {
        parameters[name] = value;
    }
    
    double getParameter(const std::string& name) const override {
        auto it = parameters.find(name);
        return (it != parameters.end()) ? it->second : 0.0;
    }
    
    void initialize() override {
        if (!initialized) {
            onInitialize();
            initialized = true;
        }
    }
    
    void shutdown() override {
        if (initialized) {
            onShutdown();
            initialized = false;
        }
    }
    
protected:
    virtual void onInitialize() {}
    virtual void onShutdown() {}
};

// Rational choice model
class RationalChoiceModel : public BehavioralModelBase {
public:
    std::string getName() const override { return "RationalChoice"; }
    std::string getVersion() const override { return "1.0.0"; }
    
    Action* decideAction(Entity* entity,
                        const std::vector<Entity*>& neighbors,
                        const ActionContext& context) override;
    
    void saveState(std::ostream& out) const override;
    void loadState(std::istream& in) override;
    
private:
    float calculateExpectedUtility(Entity* entity, const Action& action,
                                   const ActionContext& context);
};

// Emotional/impulsive model
class EmotionalDecisionModel : public BehavioralModelBase {
public:
    std::string getName() const override { return "EmotionalDecision"; }
    std::string getVersion() const override { return "1.0.0"; }
    
    Action* decideAction(Entity* entity,
                        const std::vector<Entity*>& neighbors,
                        const ActionContext& context) override;
    
    void saveState(std::ostream& out) const override;
    void loadState(std::istream& in) override;
    
private:
    float calculateEmotionalImpulse(Entity* entity, const Action& action);
};

// Social learning model
class SocialLearningModel : public BehavioralModelBase {
public:
    std::string getName() const override { return "SocialLearning"; }
    std::string getVersion() const override { return "1.0.0"; }
    
    Action* decideAction(Entity* entity,
                        const std::vector<Entity*>& neighbors,
                        const ActionContext& context) override;
    
    void saveState(std::ostream& out) const override;
    void loadState(std::istream& in) override;
    
private:
    void observeNeighbors(Entity* entity, 
                         const std::vector<Entity*>& neighbors);
    float calculateImitationProbability(Entity* entity, Entity* model);
};

// Hybrid model combining multiple approaches
class HybridDecisionModel : public BehavioralModelBase {
private:
    std::vector<std::shared_ptr<BehavioralPlugin>> subModels;
    std::map<std::string, double> modelWeights;
    
public:
    std::string getName() const override { return "Hybrid"; }
    std::string getVersion() const override { return "1.0.0"; }
    
    void addSubModel(std::shared_ptr<BehavioralPlugin> model, double weight);
    
    Action* decideAction(Entity* entity,
                        const std::vector<Entity*>& neighbors,
                        const ActionContext& context) override;
    
    void saveState(std::ostream& out) const override;
    void loadState(std::istream& in) override;
};

// Plugin manager
class PluginManager {
private:
    std::map<std::string, std::shared_ptr<BehavioralPlugin>> plugins;
    std::shared_ptr<BehavioralPlugin> activePlugin;
    
public:
    static PluginManager& getInstance();
    
    void registerPlugin(const std::string& name, 
                       std::shared_ptr<BehavioralPlugin> plugin);
    
    void setActivePlugin(const std::string& name);
    std::shared_ptr<BehavioralPlugin> getActivePlugin() const;
    
    std::vector<std::string> getAvailablePlugins() const;
    
    Action* executeDecision(Entity* entity,
                           const std::vector<Entity*>& neighbors,
                           const ActionContext& context);
};

// Decision tree node
struct DecisionNode {
    std::string condition;
    std::function<bool(Entity*, const ActionContext&)> predicate;
    std::shared_ptr<DecisionNode> trueBranch;
    std::shared_ptr<DecisionNode> falseBranch;
    std::string actionName;
    
    bool isLeaf() const { return !trueBranch && !falseBranch; }
};

// Decision tree executor
class DecisionTreeExecutor {
private:
    std::shared_ptr<DecisionNode> root;
    
public:
    void setRoot(std::shared_ptr<DecisionNode> node);
    std::string evaluate(Entity* entity, const ActionContext& context);
    
    // Build tree from configuration
    void buildFromConfig(const std::string& configJson);
};

// Utility function wrappers
namespace utils {
    float sigmoid(float x);
    float softmax(const std::vector<float>& values, size_t index);
    float clamp(float value, float min, float max);
}

} // namespace modules

#endif // BEHAVIORAL_MODULE_H
