#include "./header/Movement.h"
#include "./header/Entity.h"
#include "./header/BetterRand.h"
#include "./header/UI.h"
#include <iostream>

void Movement::MoveTowardsRandom(Entity* ent, int force){
  int x = BetterRand::genNrInInterval(0, 4);
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

  if(std::max(y, y_target) == x_target){
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

  if(std::max(y, y_target) == x_target){
    pos[1] = y - force;
  }else{
    pos[1] = y + force;
  }
  ent->posX = pos[0];
  ent->posY = pos[1];
}


void Movement::applyMovement(Entity* ent, int closeSickEnt=0){
  // on ne peut (et veut) pas envoyer l'entite vers son couple
  int pointMovement = BetterRand::genNrInInterval(0, 7) + (ent->entityHealth / 40);
  if(closeSickEnt < 2){

    Entity* mostHated = ent->mostAngryConn(); //2e 2/6 = 1/3 = 0.3
    Entity* mostDesire = ent->mostDesireConn(); //lien le plus fort 3/6 = 1/2
    Entity* mostSocial = ent->mostSocialConn(); //4e 1/6 = 0.16

    if(mostHated != nullptr){
      MovementInversePoint(ent, mostHated, floor(pointMovement * 0.3));
    }else if(mostDesire != nullptr){
      MovementTowardsPoint(ent, mostDesire, ceil(pointMovement * 0.5));
    }else if(mostSocial != nullptr){
      MovementTowardsPoint(ent, mostSocial, floor(pointMovement * 0.16));
    }
  }else{
    pointMovement += BetterRand::genNrInInterval(1,5); //on donne un 'boost' pour qu'il puisse s'échapper
    MoveTowardsRandom(ent, pointMovement);
  }
    std::cout << "== Movement Applied == \n " << ent->name << "new pos->(" << ent->posX << ";" << ent->posY << ")\n";
}
