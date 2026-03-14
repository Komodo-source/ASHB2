#ifndef SOCIAL_NORM_SYSTEM_H
#define SOCIAL_NORM_SYSTEM_H

#include <string>
#include <vector>
#include <map>
#include <cmath>

class Entity;

struct SocialNorm {
    std::string actionName;
    float prevalence  = 0.0f;
    float normPressure = 0.0f;
};

class SocialNormSystem {
public:
    std::map<std::string, SocialNorm> norms;

    void update(const std::vector<Entity*>& allEntities);

    std::map<std::string, SocialNorm> getNormsSnapshot() const {
        return norms;
    }
};

#endif
