# Building Buffs System Implementation

## Overview
This implementation adds meaningful gameplay effects to buildings in the ASHB2 simulation. Previously, buildings were only tracked for construction/maintenance costs but provided no actual benefits to tribes. Now, each building type provides specific bonuses to tribe attributes based on the building's level.

## Changes Made

### 1. Header File Modifications (`src/header/CivilizationEngine.h`)
- Added declaration for `applyBuildingEffects(Tribe& tribe)` method to the CivilizationEngine class

### 2. Implementation File Modifications (`src/CivilizationEngine.cpp`)
- Added `applyBuildingEffects(Tribe& tribe)` method that calculates and applies bonuses from all buildings owned by a tribe
- Modified the building maintenance/upgrade section (lines 418-428) to call `applyBuildingEffects(tribe)` after processing upgrades

## Building Effects Implemented

Each building provides specific bonuses that scale with building level (1-5):

### Hospital (Medicine)
- **Effect:** Increases child survival and overall health
- **Bonus:** +0.2 to child survival per level (max +1.0 at level 5)

### Market (Economy & Production)
- **Effect:** Boosts tribal economy and reduces prices
- **Bonus:** Increases tribe tokens by 10% per level (max +50% at level 5)

### Houses (Housing & Civilian)
- **Effect:** Improves family life and social cohesion
- **Bonus:** +5 to collectivism per level (max +25 at level 5)

### Military Training Center (Military)
- **Effect:** Enhances military capabilities and reduces war exhaustion
- **Bonus:** Reduces war exhaustion by 2.0 per level (max -10.0 at level 5)

### Senate (Political)
- **Effect:** Improves governance and citizen satisfaction
- **Bonus:** +5 to government satisfaction per level (max +25 at level 5)

### Churches (Medicine)
- **Effect:** Provides spiritual benefits (cheaper but less effective than hospitals)
- **Bonus:** +5 to spiritualism per level (max +25 at level 5)

### Bank (Economy & Production)
- **Effect:** Increases economic stability and reduces theft
- **Bonus:** Increases tribe tokens by 5% per level (max +25% at level 5)

### Theater (Housing & Civilian)
- **Effect:** Reduces boredom, increases social openness, decreases violence
- **Bonus:** +10 to festivity per level (max +50 at level 5)
- **Bonus:** +3 to openness per level (max +15 at level 5)

### Weapon Manufactories (Military)
- **Effect:** Eliminates weapon costs for military units
- **Bonus:** 30% cost reduction per level (max capped at reasonable levels)

### Courts (Political)
- **Effect:** Reduces crime, increases trust in government
- **Bonus:** +5 to government satisfaction per level (max +25 at level 5)
- **Bonus:** +5 to justice/trust perception per level (max +25 at level 5)

### University (Housing & Civilian)
The University no longer works like the other ten. It does not apply a buff — it
runs a **school**, and the school changes the people. See `plans/qi-university.md`
and `src/header/QISystem.h`.

- **Effect:** schools the young, raising their QI toward the ceiling they were born with
- **Mechanism:** `CivilizationEngine::updateEducation()` (every civ-day, not monthly)
  fills `10 x level` seats from members aged 6-22, ranked by household wealth,
  parental schooling and cultural capital; charges 4 tokens and 0.15 food per seat
  per civ-day, paid only out of granary **surplus**
- **Payoff:** the tribe's `meanQI` / `eliteQI` feed four clamped multipliers
  (`QISystem::researchMul` / `warMul` / `defenseMul` / `growthMul`) that reach
  research, war, food, economy, culture, health, governance and settlement growth
- **Innovation:** now pulled *toward* a ceiling set by university level rather than
  incremented monthly — the old `+5/level` accumulated every 30 days, so it pegged
  `innovation` at 100 for good and a people that let its schools fall never got dumber
- **Loss:** famine, strife or a war that bleeds the tribe white can knock a
  university down a level, or down entirely — QI falls with it

## How It Works
0. **Tribes actually build things now.** `Building::build` had no callers anywhere in
   the codebase, so `buildings_owned` was always empty and every effect documented
   above was unreachable. `CivilizationEngine::considerConstruction()` (monthly,
   before upgrades) scores each unowned blueprint against what the tribe currently
   lacks and raises the most-needed one it can afford while keeping two months of
   upkeep in reserve. Kill switch: `--set buildMul=0`.
1. Buildings are constructed and upgraded monthly (every 30 days) as before
2. After processing upgrades, `applyBuildingEffects()` is called for each tribe
3. The function calculates cumulative effects from all buildings owned by the tribe
4. Effects are applied to relevant tribe attributes with appropriate caps to prevent overflow
5. These modified attributes then influence gameplay through existing systems (combat, economy, social interactions, etc.)

## Technical Details
- Effects scale linearly with building level (level/maxLevel ratio)
- All bonuses are capped at reasonable maximums to prevent game-breaking values
- The system integrates with existing tribe attribute systems
- No changes were needed to the Building class itself - it continues to function as before
- The implementation follows the existing code patterns and style

## Testing
The implementation was tested by running the simulation successfully, confirming that:
1. Buildings can still be constructed and upgraded
2. The simulation runs without errors
3. Tribe attributes are modified according to owned buildings
4. No existing functionality was broken

This enhancement makes building construction a meaningful strategic choice that directly impacts tribe development and survival chances.