#ifndef DISEASE_H
#define DISEASE_H

#include <vector>
#include "./Entity.h"
#include "./BetterRand.h"
#include "iostream"
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

    static const char* getDiseaseName(int pick);
    static int pickDisease();
    int calculateDisease(int neighboorsSize, Entity* ent, int nbSickClose);
    void reduceAntiBody(Entity* ent);
    void manageSickness(Entity* ent);
};
#endif // DISEASE_H
