#ifndef BACKUPEXPORT_H
#define BACKUPEXPORT_H

// ─── crash-recovery backup channel → src/backup/auto_file_index_database.py ───
//
// Same shape as the PostgreSQL bridge in DbExport.h, different artifact and
// different consumer. There the engine spools NDJSON rows a loader COPYs into
// Postgres; here it spools whole world saves an indexer records in SQLite and
// — if the run dies — resumes from.
//
// Two kinds of file land in SAVES_DIR (src/backup/backup_log/):
//
//   <sig>_bk_<run>_<nnnnnn>_d<day>.txt        a full ASHB2_SAVE_V3 world
//   <sig>_bk_<run>_<nnnnnn>_d<day>.meta.json  what it is and how to resume it
//   run_<run>.alive                           the heartbeat, rewritten ~1/s
//
// The handoff is the same atomic rename DbExport uses: saveGame writes under a
// `.part` suffix, and only once the file is closed is it renamed to `.txt`.
// The indexer's glob only matches the published form, so a save being written
// (or one torn by the very crash we are guarding against) is never indexed as
// a restore candidate.
//
// The heartbeat is the crash signal. While the process lives it is rewritten
// with status "running"; a clean exit rewrites it as "finished" through the
// atexit hook. A segfault or a kill -9 runs no atexit handler, so the file is
// left saying "running" with a pid that no longer exists — which is exactly
// how the supervisor tells a crash from a completed run. It carries the argv
// and cwd of the run, so the supervisor can relaunch it verbatim with
// `--load <newest backup>` and the remaining tick count.
//
// Cadence is wall-clock, not ticks: tick cost swings with population, so "every
// 500 ticks" is minutes early in a run and an hour late once the world is
// crowded. Seconds keep the worst case bounded at what you actually care about
// — how much simulated history a crash can cost you.
//
// Disabled unless --backup-every was passed; every call below is then a no-op.

#include <string>
#include <vector>

class Entity;
class CivilizationEngine;

namespace bkexport {

// Turn the channel on and write the first heartbeat. `intervalSeconds` is the
// minimum wall-clock gap between checkpoints. argc/argv are recorded verbatim
// so a supervisor can reconstruct the command line. Call once at startup.
void configure(int worldId, const std::string& seed, const std::string& label,
               int intervalSeconds, int argc, char** argv);

bool enabled();

// Call once per tick. Refreshes the heartbeat (at most once a second) and
// writes a checkpoint when `intervalSeconds` has elapsed. `ticksDone` and
// `targetTicks` are what a resumed run needs to know how much is left; pass
// -1 for both from the windowed loop, which has no tick target.
void tick(const std::vector<Entity>& entities, int day, int frameCounter,
          const CivilizationEngine* civ, int ticksDone, int targetTicks);

// Write a checkpoint now, whatever the clock says.
void checkpoint(const std::vector<Entity>& entities, int day, int frameCounter,
                const CivilizationEngine* civ, int ticksDone, int targetTicks);

// Final heartbeat: "finished" (ran to completion), "extinct" (population hit
// zero), "stopped" (asked to quit). Anything that leaves the heartbeat saying
// "running" is read by the supervisor as a crash.
void shutdown(const char* status);

// std::atexit takes no arguments. Registering this marks the run finished on
// every ordinary exit path, and — because atexit handlers do not run on abort
// or a fatal signal — leaves the crash signal intact when it matters.
void atExit();

} // namespace bkexport

#endif // BACKUPEXPORT_H
