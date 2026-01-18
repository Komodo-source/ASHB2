#ifndef DISEASE_H
#define DISEASE_H

#include <vector>
#include "./header/Entity.h"
#include "./header/BetterRand.h"
#pragma once


class Disease{
  public:
    static const int DISEASE_1;
    static const char* DISEASE_1_NAME;
    static const int DISEASE_2;
    static const char* DISEASE_2_NAME;
    static const int DISEASE_3;
    static const char* DISEASE_3_NAME;
    static const int DISEASE_4;
    static const char* DISEASE_4_NAME;
    
    const char* getDiseaseName(int pick);
    static int pickDisease();
    bool calculateDisease(int neighboorsSize, Entity* ent);
    void reduceAntiBody(Entity* ent);
};
#endif // DISEASE_H
