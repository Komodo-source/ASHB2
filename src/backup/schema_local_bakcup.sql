-- Local backup index — the SQLite side of the crash-recovery channel.
--
-- auto_file_index_database.py creates and migrates this itself (CREATE TABLE IF
-- NOT EXISTS, then ALTER TABLE for whatever is missing), so this file is the
-- reference copy rather than something that has to be run by hand. An existing
-- schema_local_backup.db from before the extra columns keeps its rows.
--
-- Producer: the engine's --backup-every channel (src/header/BackupExport.h).
-- Consumer: src/backup/auto_file_index_database.py.

CREATE TABLE IF NOT EXISTS BACKUP (
  id_backup     INTEGER PRIMARY KEY AUTOINCREMENT,
  backup_path   TEXT NOT NULL,   -- repo-relative path to the save
  simulation_id TEXT NOT NULL,   -- run id "<epoch>_<pid>", or 'manual'
  date_backup   DATE NOT NULL,   -- ISO timestamp the checkpoint was written

  world_id      INTEGER,
  day           INTEGER,         -- the save's own DAY: (frames) — ordering key
  civ_day       INTEGER,         -- the in-world day the civ log speaks in
  ticks_done    INTEGER,         -- how far the run had got
  target_ticks  INTEGER,         -- what --headless asked for
  entities      INTEGER,
  seed          TEXT,            -- resolved master seed; replays the same world
  label         TEXT,
  signature     TEXT,            -- CivilizationEngine::historySignature()
  bytes         INTEGER,
  created_at    INTEGER,         -- unix seconds
  host          TEXT,
  pid           INTEGER,
  cwd           TEXT,            -- where the engine ran
  argv          TEXT,            -- JSON array: the command line, verbatim
  complete      INTEGER,         -- passed the marker/terminator check
  present       INTEGER          -- file still on disk (0 once pruned)
);

-- Indexing re-scans the directory every pass, so the same path is seen many
-- times. This index is what makes re-seeing a file a no-op rather than a
-- duplicate row.
CREATE UNIQUE INDEX IF NOT EXISTS idx_backup_path ON BACKUP(backup_path);

-- Every time the supervisor brought a crashed world back.
CREATE TABLE IF NOT EXISTS RESTART (
  id_restart    INTEGER PRIMARY KEY AUTOINCREMENT,
  simulation_id TEXT NOT NULL,   -- the run that died
  restored_from TEXT,            -- BACKUP.backup_path it resumed from
  reason        TEXT,            -- how the crash was detected
  command       TEXT,            -- what was actually run
  cwd           TEXT,
  new_pid       INTEGER,
  started_at    INTEGER
);
