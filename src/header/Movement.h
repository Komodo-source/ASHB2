#ifndef MOVEMENT_H
#define MOVEMENT_H
#include "./Entity.h"
#include "./BetterRand.h"
#include "./UI.h"
#include "./FreeWillSystem.h"
#include <iostream>


class Movement{
  public:
  void MovementTowardsPoint(Entity* ent, Entity* target, int force);
  void applyMovement(Entity* ent, int closeSickEnt,
                     int crowdSize = 0,
                     const EnvironmentalFactors& env = EnvironmentalFactors(),
                     float windowWidth = 1600, float windowHeight = 1050);
  void MovementInversePoint(Entity* ent, Entity* target, int force);
  void MoveTowardsRandom(Entity* ent, int force);
  void clampToWindow(Entity* ent, float windowWidth, float windowHeight);
};


#endif // MOVEMENT_H

