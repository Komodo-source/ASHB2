#include "Entity.h"

struct SocialNorm {
    std::string actionName;
    float prevalence  = 0.0f;
    float normPressure = 0.0f;
};

class SocialNormSystem {
public:
    std::map<std::string, SocialNorm> norms;

    void update(const std::vector<Entity*>& allEntities) {
        std::map<std::string, int> actionCounts;
        int totalActions = 0;

        for (Entity* ent : allEntities) {
            if (ent->fws.getActionHistory().empty()) continue;
            const std::string& lastAction = ent->fws.getActionHistory().front().actionName;
            actionCounts[lastAction]++;
            totalActions++;
        }

        if (totalActions == 0) return;

        for (auto& [name, count] : actionCounts) {
            float prevalence = (float)count / totalActions;
            auto& norm = norms[name];
            norm.actionName = name;
            norm.prevalence = norm.prevalence * 0.95f + prevalence * 0.05f;
            norm.normPressure = std::abs(norm.prevalence - 0.5f) * 2.0f;
        }
    }

    std::map<std::string, SocialNorm> getNormsSnapshot() const {
        return norms;
    }
};
