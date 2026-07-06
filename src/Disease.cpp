
#include <vector>
#include "./header/Entity.h"
#include "./header/BetterRand.h"
#include "./header/Disease.h"
#include "iostream"
#include "./header/Logging.h"
#include <set>

extern Logger* globalLogger;

const int Disease::DISEASE_1 = 1;
const char* Disease::DISEASE_1_NAME = "Plague";
const int Disease::DISEASE_2 = 2;
const char* Disease::DISEASE_2_NAME = "Fever";
const int Disease::DISEASE_3 = 3;
const char* Disease::DISEASE_3_NAME = "Malaria";
const int Disease::DISEASE_4 = 4;
const char* Disease::DISEASE_4_NAME = "Typhus";

const int Disease::DISEASE_6 = 6;
const char* Disease::DISEASE_6_NAME = "Leprosy";
const int Disease::DISEASE_7 = 7;
const char* Disease::DISEASE_7_NAME = "Smallpox";
int Disease::region;

//infamous disease
const int Disease::DISEASE_5 = 5;
const char* Disease::DISEASE_5_NAME = "Cancer";


  int Disease::pickDisease(){
    return BetterRand::genNrInInterval(1,4);
  }

    const char* Disease::getDiseaseName(int pick){
      switch(pick){
          case DISEASE_1: return DISEASE_1_NAME;
          case DISEASE_2: return DISEASE_2_NAME;
          case DISEASE_3: return DISEASE_3_NAME;
          case DISEASE_4: return DISEASE_4_NAME;
          case DISEASE_5: return DISEASE_5_NAME;
          case DISEASE_6: return DISEASE_6_NAME;
          default: return "Unknown Disease";
      }
  }


  void Disease::reduceAntiBody(Entity* ent){
    // Slow decay — antibodies should persist for many in-game days, not vanish in seconds
    int reducedAntiBody = BetterRand::genNrInInterval(0,1);
    if(ent->entityAntiBody - reducedAntiBody >= 0 ){
      ent->entityAntiBody -= reducedAntiBody;
    }else{
      ent->entityAntiBody = 0;
    }
  }

  void Disease::checkInfamousDisease(Entity* ent) {
    if(ent->entityDiseaseType == -2){
      if(ent->entityAntiBody > 95){
        ent->entityDiseaseType = 0;
        std::cout << ent->name + " was cured from cancer\n";
      }else{
        ent->entityAntiBody += BetterRand::genNrInInterval(0,3);
        ent->entityAntiBody -= BetterRand::genNrInInterval(0,3);
      }
    }else{
      if(ent->entityAge > 40){
        if(BetterRand::genNrInInterval(0, 100) > 5){
          std::cout << ent->name + " has now cancer\n";
          ent->entityDiseaseType = -2;
        }
      }
    }
  }

  int Disease::calculateDisease(int neighboorsSize, Entity* ent, int nbSickClose, int leprosyClose){
    if (ent->entityDiseaseType != -2){

    if(leprosyClose > nbSickClose){
      //check for leprosy
      int hygiene = ent->entityHygiene;
      int diseasePicked = DISEASE_6;
      if (ent->entityAntiBody < 60 || ent->entityDiseaseType != -1){ //a déja une maladie
        if(hygiene - region - neighboorsSize - (1.2 * leprosyClose) + (2.5 * ent->entityAntiBody) < 0){
          return diseasePicked;
        }else{
          return -1;
        }
      }
    }else{


    int ranchoice = BetterRand::genNrInInterval(0,2);
    int hygiene = ent->entityHygiene;
    int diseasePicked = pickDisease();
      if (ent->entityAntiBody < 60 || ent->entityDiseaseType != -1){ //a déja une maladie
        if(hygiene - (ranchoice * region) - neighboorsSize - (1.09 * nbSickClose) + (2.5 * ent->entityAntiBody) < 0){
          return diseasePicked;
        }else{
          return -1;
        }
      }}
    }
      return -1; // Default return to prevent undefined behavior crash
  }

  void Disease::manageSickness(Entity* ent){
    if (ent->entityDiseaseType != -2){

    if(ent->entityDiseaseType == DISEASE_7){
      //smallpox
      ent->entityHealth -= ent->entityDiseaseType * 0.30;
      ent->entityHygiene -= ent->entityDiseaseType * 0.25;
      ent->entityAntiBody += BetterRand::genNrInInterval(3, 9);
    }else if(ent->entityDiseaseType == DISEASE_6){
      ent->entityHealth -= ent->entityDiseaseType * 0.07;
      ent->entityHygiene -= ent->entityDiseaseType * 0.35;
      ent->entityAntiBody += BetterRand::genNrInInterval(3, 5);
    }
    else{

    // guerison
    // Diseases drain health slowly — should be survivable for days
    ent->entityHealth -= ent->entityDiseaseType * 0.10;
    ent->entityHygiene -= ent->entityDiseaseType * 0.15;
    ent->entityAntiBody += BetterRand::genNrInInterval(3, 14);
    }

    int medication_power =0;
    for (Tribe t : globalCivEngine->tribes) {
      std::set<std::string>::iterator itr;
      for (itr = t.knownTechName.begin(); itr != t.knownTechName.end(); itr++) {
        if(*itr == "Vaccine/Medication"){
          medication_power = BetterRand::genNrInInterval(3,9);
          if(ent->entityHealth + 3 < 100){
            //taking medicine is a mix between placebo, well being -> so at the end it add a bit of health
            BetterRand::genNrInInterval(0, 3);

          }
        }
      }
    }

    if(ent->entityAntiBody + BetterRand::genNrInInterval(0, 6) + medication_power > 90){
      std::string dName = Disease::getDiseaseName(ent->entityDiseaseType);
      std::cout << "## " <<ent->getName() + " was cured from " + dName << " ##"<< std::endl;
      //we give a solid base of antibody to make him avoid getting sick again
      ent->entityAntiBody = 80;
      ent->entityDiseaseType = -1;
      if(globalLogger) globalLogger->logDisease(ent->getId(), ent->getName(), dName, true);
      }
    }else{
      for (Tribe t : globalCivEngine->tribes) {
        std::set<std::string>::iterator itr;
        for (itr = t.knownTechName.begin(); itr != t.knownTechName.end(); itr++) {
          if(*itr == "Autophagy"){
            if(BetterRand::genNrInInterval(0,100) < 7){
              std::cout << "$$ " <<ent->getName() + " was cured from from cancer  $$"<< std::endl;
              if(globalLogger) globalLogger->logDisease(ent->getId(), ent->getName(), "cancer", true);
              ent->entityDiseaseType = -1;
            }
          }
        }
      }
    }
  }
