#pragma once

enum NeedLevel { PHYSIOLOGICAL, SAFETY, BELONGING, ESTEEM, SELF_ACTUALIZATION };

struct HierarchicalNeed {
    NeedLevel level;
    std::string name;
    float urgency;          // 0-100
    float decayRate;    // how fast this need grows naturally
    float satisfactionThreshold; // 0-100, how "met" this need currently is (inverse of urgency)

    HierarchicalNeed() : name(""), level(BELONGING), urgency(50.0f), decayRate(0.1f), satisfactionThreshold(50.0f) {}
    HierarchicalNeed(std::string n, NeedLevel lvl, float decay = 0.1f)
        : name(n), level(lvl), urgency(50.0f), decayRate(decay), satisfactionThreshold(50.0f) {}
};


