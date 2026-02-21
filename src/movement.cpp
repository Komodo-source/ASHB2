#include "./header/Movement.h"
#include "./header/Entity.h"
#include "./header/BetterRand.h"
#include "./header/UI.h"
#include <iostream>

void Movement::MoveTowardsRandom(Entity* ent, int force){
  int x = BetterRand::genNrInInterval(1, 6);
  if(x == 1){
    ent->posX += force;
    ent->posY += force;
  }else if(x == 2){
    ent->posX += force;
    ent->posY -= force;
  }else if(x == 3){
    ent->posX -= force;
    ent->posY += force;
  }else if(x == 4){
    ent->posX -= force;
    ent->posY -= force;
  }

}

void Movement::MovementTowardsPoint(Entity* ent, Entity* target, int force){
  int x = ent->posX; int y = ent->posY;
  int x_target = target->posX; int y_target = target->posY;
  int pos[2] = {0, 0};
  if(std::max(x, x_target) == x_target){
    pos[0] = x + force;
  }else{
    pos[0] = x - force;
  }

  if(std::max(y, x_target) == x_target){

    pos[1] = y + force;
  }else{
    pos[1] = y - force;
  }
  ent->posX = pos[0];
  ent->posY = pos[1];
}

 void Movement::MovementInversePoint(Entity* ent, Entity* target, int force){
  int x = ent->posX; int y = ent->posY;
  int x_target = target->posX; int y_target = target->posY;
  int pos[2] = {0, 0};
  if(std::max(x, x_target) == x_target){
    pos[0] = x - force;
  }else{
    pos[0] = x + force;
  }

  if(std::max(y, x_target) == x_target){
    pos[1] = y - force;
  }else{
    pos[1] = y + force;
  }
  ent->posX = pos[0];
  ent->posY = pos[1];
}


void Movement::clampToWindow(Entity* ent, float windowWidth, float windowHeight){
  const float margin = 10.0f;
  if(ent->posX < margin) ent->posX = margin;
  if(ent->posY < margin) ent->posY = margin;
  if(ent->posX > windowWidth - margin) ent->posX = windowWidth - margin;
  if(ent->posY > windowHeight - margin) ent->posY = windowHeight - margin;
}

void Movement::applyMovement(Entity* ent, int closeSickEnt,
                              int crowdSize,
                              const EnvironmentalFactors& env,
                              float windowWidth, float windowHeight){
  int pointMovement = BetterRand::genNrInInterval(1, 6) + (ent->entityHealth / 40);

  // --- Low-safety flee behaviour ---
  // Entities in dangerous areas move away randomly with an urgency boost
  if (env.safetyLevel < 40.0f) {
    float dangerFactor = (40.0f - env.safetyLevel) / 40.0f; // 0-1
    int fleeBoost = static_cast<int>(dangerFactor * 6.0f);
    pointMovement += fleeBoost + BetterRand::genNrInInterval(1, 4);
    MoveTowardsRandom(ent, pointMovement);
    clampToWindow(ent, windowWidth, windowHeight);
    std::cout << "== Movement (Safety-Flee) == \n " << ent->name
              << " flees danger. safetyLevel=" << env.safetyLevel
              << " pos->(" << ent->posX << ";" << ent->posY << ")\n";
    return;
  }

  // --- Introvert crowd-avoidance ---
  // Introverts (low extraversion) flee when crowdSize is high
  bool isCrowded = (crowdSize > 3 || env.crowdDensity > 60.0f);
  bool isIntrovert = (ent->personality.extraversion < 40.0f);
  if (isCrowded && isIntrovert) {
    int avoidBoost = BetterRand::genNrInInterval(1, 4);
    pointMovement += avoidBoost;
    MoveTowardsRandom(ent, pointMovement);
    clampToWindow(ent, windowWidth, windowHeight);
    std::cout << "== Movement (Introvert Crowd-Avoid) == \n " << ent->name
              << " avoids crowd (extraversion=" << ent->personality.extraversion
              << ", crowd=" << crowdSize << ")"
              << " pos->(" << ent->posX << ";" << ent->posY << ")\n";
    return;
  }

  if(closeSickEnt < 2){
    Entity* mostHated = ent->mostAngryConn();
    Entity* mostDesire = ent->mostDesireConn();
    Entity* mostSocial = ent->mostSocialConn();

    if(mostHated != nullptr){
      MovementInversePoint(ent, mostHated, floor(pointMovement * 1.3));
    }else if(mostDesire != nullptr){
      MovementTowardsPoint(ent, mostDesire, ceil(pointMovement * 1.5));
    }else if(mostSocial != nullptr){
      MovementTowardsPoint(ent, mostSocial, floor(pointMovement * 1.16));
    }
  }else{
    pointMovement += BetterRand::genNrInInterval(1,6);
    MoveTowardsRandom(ent, pointMovement);
  }
  clampToWindow(ent, windowWidth, windowHeight);
  std::cout << "== Movement Applied == \n " << ent->name << " new pos->(" << ent->posX << ";" << ent->posY << ")\n";
}
