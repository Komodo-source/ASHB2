
#include <vector>
#include "./header/Entity.h"
#include "./header/BetterRand.h"
#include "./header/Disease.h"
#include "iostream"
#include "./header/Logging.h"

extern Logger* globalLogger;

const int Disease::DISEASE_1 = 1;
const char* Disease::DISEASE_1_NAME = "Plague";
const int Disease::DISEASE_2 = 2;
const char* Disease::DISEASE_2_NAME = "Fever";
const int Disease::DISEASE_3 = 3;
const char* Disease::DISEASE_3_NAME = "Malaria";
const int Disease::DISEASE_4 = 4;
const char* Disease::DISEASE_4_NAME = "Typhus";
int Disease::region;

  int Disease::pickDisease(){
    return BetterRand::genNrInInterval(1,4);
  }

    const char* Disease::getDiseaseName(int pick){
      switch(pick){
          case DISEASE_1: return DISEASE_1_NAME;
          case DISEASE_2: return DISEASE_2_NAME;
          case DISEASE_3: return DISEASE_3_NAME;
          case DISEASE_4: return DISEASE_4_NAME;
          default: return "Unknown Disease";
      }
  }


  void Disease::reduceAntiBody(Entity* ent){
    // Estimate that antibody disapear in 50 days -> reduce by 2 every day after a disease
    // where antibody = 100
    if(ent->entityAntiBody -2 >= 0 ){
      ent->entityAntiBody -= 2;
    }else{
      ent->entityAntiBody = 0;
    }
  }

  int Disease::calculateDisease(int neighboorsSize, Entity* ent, int nbSickClose){
    int ranchoice = BetterRand::genNrInInterval(0,4);
    int hygiene = ent->entityHygiene;
    int diseasePicked = pickDisease();
      if (ent->entityAntiBody < 70 || ent->entityDiseaseType != -1){ //a déja une maladie
        if(hygiene - (ranchoice * region) - neighboorsSize - (1.15 * nbSickClose) + (2.2 * ent->entityAntiBody) < 0){
          return diseasePicked;
        }else{
          return -1;
        }
      }
      return -1; // Default return to prevent undefined behavior crash
  }

  void Disease::manageSickness(Entity* ent){
    // guerison
    ent->entityHealth -= ent->entityDiseaseType * 2;
    ent->entityHygiene -= ent->entityDiseaseType * 2;
    ent->entityAntiBody += BetterRand::genNrInInterval(4, 15);
    if(ent->entityAntiBody + BetterRand::genNrInInterval(0, 20) > 100){
      std::string dName = Disease::getDiseaseName(ent->entityDiseaseType);
      std::cout << ent->getName() + " was cured from " + dName << std::endl;
      ent->entityDiseaseType = -1;
      if(globalLogger) globalLogger->logDisease(ent->getId(), ent->getName(), dName, true);
    }
  }
