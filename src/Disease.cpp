
#include <vector>
#include "./header/Entity.h"
#include "./header/BetterRand.h"
#include "./header/Disease.h"


const int Disease::DISEASE_1 = 1;
const char* Disease::DISEASE_1_NAME = "Plague";
const int Disease::DISEASE_2 = 2;
const char* Disease::DISEASE_2_NAME = "Fever";
const int Disease::DISEASE_3 = 3;
const char* Disease::DISEASE_3_NAME = "Malaria";
const int Disease::DISEASE_4 = 4;
const char* Disease::DISEASE_4_NAME = "Typhus";

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

  bool Disease::calculateDisease(int neighboorsSize, Entity* ent){
    int ranchoice = BetterRand::genNrInInterval(0,15);
    int hygiene = ent->entityHygiene;
    int diseasePicked = pickDisease();
    if(ent->entityDiseaseTypeAntiBody == diseasePicked){
      //a peut etre des anticorps pour cette maladie
      if(hygiene - ranchoice - neighboorsSize + (1.3 * ent->entityAntiBody) < 0){
        return diseasePicked;
      }else{
        return -1;
      }
    }
    return hygiene - ranchoice - neighboorsSize < 0 ? diseasePicked : -1;
  }
