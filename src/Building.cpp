#include "header/Building.h"
#include "header/BetterRand.h"
#include "header/CivilizationEngine.h"
#include "header/Logging.h"
#include <vector>

void Building::build(std::string building_name, Tribe& tribe){
  for(const buildingStructure& blueprint : buildings){
    if(blueprint.name == building_name && tribe.economy.token >= blueprint.getPrice()){
      tribe.economy.token -= blueprint.getPrice();
      tribe.buildings_owned.push_back(blueprint); // copy the blueprint (level=1)
      globalLogger->logEvent("Building", tribe.name + " built a " + building_name);
      return;   // one build per call; without this the loop keeps scanning
    }
  }
}

void Building::upgrade(std::string building_name, Tribe& tribe){
  for(auto& building : tribe.buildings_owned){
    if(building.name == building_name && building.level < building.maxLevel){
      int upgradeCost = building.basePrice; // cost to upgrade one level is the base price
      if(tribe.economy.token >= upgradeCost){
        tribe.economy.token -= upgradeCost;
        building.level++;
        globalLogger->logEvent("Building", tribe.name + " upgraded " + building_name + " to level " + std::to_string(building.level));
      }
    }
  }
}

//to call maybe once in a week -> if too much call once in a month
void Building::maintenance_costs(Tribe& tribe){
  for(auto& building : tribe.buildings_owned){
    int maintenanceCost = building.getMaintenance();
    if(tribe.economy.token >= maintenanceCost){
      tribe.economy.token -= maintenanceCost;
    }
    // If unable to pay maintenance, building becomes inactive (no effect) but not removed
    // Could add deterioration over time, but for simplicity we just skip payment
  }
}