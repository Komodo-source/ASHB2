#ifndef SAVELOAD_H
#define SAVELOAD_H

#include <string>
#include <vector>

class Entity;
class CivilizationEngine;

// ── M8: versioned full-state save ─────────────────────────────────────────────
// Format "ASHB2_SAVE_V2": header (day, frame, sim-clock frame, shared RNG
// stream state, era/year) + macro state (tribes, religions) + all entities.
// Legacy "ASHB2_SAVE" files still load (entities only, no macro state).
//
// Honest limitation (documented, not hidden): the shared BetterRand stream is
// restored exactly, but subsystems that own self-seeded RNGs (per-entity
// planners, cognitive modules) re-derive theirs from the world seed at
// construction — so a resumed run continues *plausibly*, not bit-identically.
// Bit-exact reproducibility remains guaranteed via same-seed replay from tick 0.
void saveGame(const std::string& filepath, const std::vector<Entity>& entities,
              int day, int frameCounter, const CivilizationEngine* civ = nullptr);
bool loadGame(const std::string& filepath, std::vector<Entity>& entities,
              int& day, int& frameCounter, CivilizationEngine* civ = nullptr);
void exportTickHistory(const std::string& filepath, const std::vector<Entity>& entities, int day);

#endif // SAVELOAD_H
