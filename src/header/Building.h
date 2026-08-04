#ifndef BUILDING_H
#define BUILDING_H

#include <string>
#include <vector>

// Tribe is only ever named through a reference here, so a forward declaration
// is enough — and is required: CivilizationEngine.h includes THIS header to
// declare Tribe::buildings_owned, so including it back would be a cycle.
struct Tribe;

//hospital reduce infant mortality

enum class BuildingType { MEDECINE, ECONOMY_AND_PRODUCTION, HOUSING_AND_CIVILIAN, MILITARY, POLITICAL };


struct buildingStructure{
  std::string name;
  BuildingType category;
  int baseHealth;   // health at level 1
  int basePrice;    // price at level 1
  int baseMaintenance; // maintenance at level 1
  int level = 1;
  int maxLevel = 5;

  // Effective values for current level
  int getHealth() const { return baseHealth * level; }
  int getPrice() const { return basePrice * level; }
  int getMaintenance() const { return baseMaintenance * level; }
};

//divide and multiply by inflation
static const std::vector<buildingStructure> buildings = {
 {"Hospital", BuildingType::MEDECINE, 100, 15000, 3000, 1, 5}, // tribes live longer over time + less infant death
 {"Market", BuildingType::ECONOMY_AND_PRODUCTION, 65, 8500, 100, 1, 5}, // price goes a bit lower, economy of tribe higher + social goes up
 {"Houses", BuildingType::HOUSING_AND_CIVILIAN, 75, 5000, 600, 1, 5}, // In a family, people lives longer, more familial links, more centered about families and nation,
 {"Military training center", BuildingType::MILITARY, 200, 20000, 1000, 1, 5}, // Better defenses, Better attack less death of wars
 {"Senate", BuildingType::POLITICAL, 100, 17000, 300, 1, 5}, // Unlock tribes decision (=> see todo)

 {"Churches", BuildingType::MEDECINE, 100, 7500, 400, 1, 5}, // Hospital but cheaper, and less efficient but bring spiritual
 {"Bank", BuildingType::ECONOMY_AND_PRODUCTION, 65, 9000, 1200, 1, 5}, // reduce number of stealing bring stability in economy
 {"Theater", BuildingType::HOUSING_AND_CIVILIAN, 45, 5000, 800, 1, 5}, // reduces boredom, can higher dates/desire, openess and tribes openess over other tribes, also catharsis => less violent actions (suicide, murder, death)
 {"Weapon manufactures", BuildingType::MILITARY, 115, 25000, 5000, 1, 5}, // No more weapon costs
 {"Court", BuildingType::POLITICAL, 100, 17000, 0, 1, 5}, // less robbery more punished drastic actions, more trust in governement unless corrosion
 {"University", BuildingType::HOUSING_AND_CIVILIAN, 115, 7000, 1500, 1, 5}, //see todo
};

// Stateless: every operation reads the blueprint table and writes through the
// Tribe it is handed, so these are static — there is nothing for an instance to
// hold, and updateTribes() calls them as Building::upgrade(...).
class Building{
  public:
    static void build(std::string building_name, Tribe& tribe);
    static void upgrade(std::string building_name, Tribe& tribe);
    static void maintenance_costs(Tribe& tribe);
    static const std::vector<buildingStructure>& getBuildings() { return buildings; }
};


#endif //BUILDING_H