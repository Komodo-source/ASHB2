/*
class Action {
public:
    std::string name;
    std::string description;
    float baseAppeal;
    float duration;

    float happinessChange;
    float stressChange;
    float lonelinessChange;
    float boredomChange;
    float hygieneChange;
    float healthChange;
    float mentalHealthChange;
    float angerChange;

    std::map<std::string, float> personalityModifiers;
    std::function<bool(const Entity&)> prerequisite;

    bool requiresOtherEntity;
    bool improvesRelationship;

    Action(const std::string& n, const std::string& desc, float appeal);

    Action& setDuration(float d);
    Action& setHappinessChange(float c);
    Action& setStressChange(float c);
    Action& setLonelinessChange(float c);
    Action& setBoredomChange(float c);
    Action& setHygieneChange(float c);
    Action& setHealthChange(float c);
    Action& setMentalHealthChange(float c);
    Action& setAngerChange(float c);
    Action& setRequiresOther(bool req);
    Action& setImprovesRelationship(bool imp);
    Action& addPersonalityModifier(const std::string& trait, float modifier);
    Action& setPrerequisite(std::function<bool(const Entity&)> func);
};

// Helper functions
std::vector<std::shared_ptr<Action>> createLifeActions();
void runSimulation();
*/
