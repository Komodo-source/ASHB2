#ifndef DBEXPORT_H
#define DBEXPORT_H

// ─── bridge/spool → PostgreSQL ──────────────────────────────────────────────
//
// The engine never talks to a database. It appends NDJSON rows to a chunk file
// in a spool directory and, every so often, closes the chunk and renames it
// from `.ndjson.tmp` to `.ndjson`. That rename is the whole handoff protocol:
// while the extension says .tmp the file is being written and the loader's glob
// ignores it; the instant it is renamed the chunk is complete and durable.
// Rename within a directory is atomic on NTFS and ext4 alike, so the loader can
// never read a half-written chunk.
//
// scripts/db_spool_loader.py consumes chunks oldest-first, COPYs each into
// PostgreSQL in one transaction, then deletes it. Neither process can block or
// crash the other, and disk stays bounded at roughly one chunk — which is the
// point, since the old tick_history export grew to 188 MB with nothing ever
// consuming it.
//
// Disabled unless --db-export was passed; every call below is then a cheap no-op.

#include <string>
#include <vector>

class Entity;

namespace dbexport {

// Turn the exporter on. `spoolDir` is created if missing. Call once at startup.
void configure(const std::string& spoolDir, int worldId,
               const std::string& label, const std::string& seed);

bool enabled();

// Emit the world header row. Cheap; call alongside each entity push.
void pushWorld(int day);

// Emit one `entity` row per living entity plus every pointed association it
// carries (desire / anger / social / couple / mental model). Call on civ ticks —
// the loader upserts, so pushing the same day twice is harmless.
void pushEntities(const std::vector<Entity>& entities, int day);

// Close the current chunk and publish it (the .tmp → .ndjson rename). Called
// automatically once a chunk fills; call it directly to publish early.
void flush();

// Publish whatever is buffered and stop. Call before the process exits, or the
// tail of the run stays stuck in a .tmp file the loader will never pick up.
void shutdown();

} // namespace dbexport

#endif // DBEXPORT_H
