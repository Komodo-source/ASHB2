#ifndef SAVELOAD_H
#define SAVELOAD_H

#include <string>
#include <vector>
#include <ctime>

class Entity;
class CivilizationEngine;
class SocialOrderSystem;
class KinshipSystem;

// ── Backup save directory ────────────────────────────────────────────────────
// All saves go into a `saves/` subdirectory relative to the working directory.
// The directory is auto-created on first save. Manual saves use the user's
// chosen filename; autobackup saves use timestamped names.
extern const char* SAVES_DIR;

// Return the full path for a save filename inside the saves/ directory.
// If `filename` is a bare name (no path separator), prepends SAVES_DIR + "/".
// If it already contains a path separator, returns it as-is (absolute or
// relative path that bypasses the backup folder, for headless --save-file).
std::string savePath(const std::string& filename);

// Generate a timestamped autobackup filename: "autosave_YYYY-MM-DD_HHMMSS.txt"
std::string autobackupFilename();

// Create the saves/ directory if it doesn't exist. Returns true if it exists
// or was created successfully.
bool ensureSavesDir();

// ── Format markers ───────────────────────────────────────────────────────────
// Current: "ASHB2_SAVE_V3" — includes all subsystems (env, resources, ecosystem,
// market, social order, kinship, live config, civ stats, death ledger).
// Legacy "ASHB2_SAVE_V2" and "ASHB2_SAVE" still load (with partial recovery).
extern const char* SAVE_FORMAT_V3;

// ── M8: versioned full-state save ─────────────────────────────────────────────
// Format "ASHB2_SAVE_V3": V2 format plus environment, resources, ecosystem,
// market, social order, kinship, live config, death ledger, civ stats.
// V2: header (day, frame, sim-clock frame, shared RNG stream state, era/year)
//     + macro state (tribes, religions) + all entities + emergence section.
// V1: legacy "ASHB2_SAVE" files still load (entities only, no macro state).
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
