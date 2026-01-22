#ifndef MOVEMENT_H
#define MOVEMENT_H

class Movement{
  public:
  void MovementTowardsPoint(Entity* ent, Entity* target, int force);
  void applyMovement(Entity* ent, int closeSickEnt);
  void MovementInversePoint(Entity* ent, Entity* target, int force);
  void MoveTowardsRandom(Entity* ent, int force);
};


#endif // MOVEMENT_H

