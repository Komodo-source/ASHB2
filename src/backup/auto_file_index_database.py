#!/usr/bin/env python3
"""
ASHB2 backup indexer + crash supervisor.

The mirror image of scripts/db_spool_loader.py. There the engine spools NDJSON
rows and this side COPYs them into PostgreSQL; here the engine checkpoints whole
world saves into backup_log/ and this side indexes them in SQLite -- and, if the
run dies, starts it again from the newest good one.

    ./app --headless 20000 --seed mars --backup-every 300      # producer
    python src/backup/auto_file_index_database.py              # consumer

Three kinds of file live in backup_log/ (see src/header/BackupExport.h):

    <sig>_bk_<run>_<nnnnnn>_d<day>.txt        a full ASHB2_SAVE_V3 world
    <sig>_bk_<run>_<nnnnnn>_d<day>.meta.json  what it is and how to resume it
    run_<run>.alive                           the heartbeat, rewritten ~1/s

The handoff is the engine's atomic rename: a save is written under `.part` and
only renamed to `.txt` once it is closed, so the glob below never matches a
half-written file. Every indexed save is checked anyway -- the format marker on
the first line, `CS_END` on the last -- because a file torn by the very crash we
are recovering from must never be handed back to the engine as a restore point.

Crash detection is the heartbeat. While the engine lives it rewrites the file
with status "running"; a clean exit rewrites it as "finished" (or "extinct")
through an atexit hook. A segfault, an OOM kill, or a power cut runs no atexit
handler, so the file is left saying "running" with a pid that no longer exists.
Stale heartbeat + dead pid = crash, and the newest indexed checkpoint for that
run gets relaunched with the argv the heartbeat recorded.

Usage:
    python src/backup/auto_file_index_database.py              # watch forever
    python src/backup/auto_file_index_database.py --once       # index, check, exit
    python src/backup/auto_file_index_database.py --index-only # never relaunch
    python src/backup/auto_file_index_database.py --list       # what is indexed
    python src/backup/auto_file_index_database.py --restore-now # resume the newest
                                                                # crashed run by hand
"""

import argparse
import datetime
import json
import os
import pathlib
import platform
import re
import sqlite3
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent          # src/backup
REPO = HERE.parent.parent                               # repo root
BACKUP_DIR = HERE / "backup_log"
DB_PATH = HERE / "schema_local_backup.db"

SAVE_GLOB = "*.txt"
BEAT_GLOB = "run_*.alive"

# A save is complete when it opens with the format marker and ends with the last
# thing saveGame writes (saveCivStats' terminator). Cheap, and it is the only
# thing standing between a torn file and the engine being asked to load it.
SAVE_MARKER = "ASHB2_SAVE"
SAVE_TERMINATOR = "CS_END"

# bk_<run>_<counter>_d<day>.txt, with the history signature saveGame prepends.
NAME_RE = re.compile(r"^(?P<sig>\d+)_bk_(?P<run>\d+_\d+)_(?P<seq>\d+)_d(?P<day>\d+)\.txt$")

# simulation_id for saves that did not come from the backup channel.
MANUAL = "manual"


# ── database ────────────────────────────────────────────────────────────────
# The BACKUP table is the one that was already here (backup_path, simulation_id,
# date_backup); everything the supervisor needs on top is added by ALTER, so an
# existing schema_local_backup.db keeps its rows.

EXTRA_COLUMNS = [
    ("world_id",     "INTEGER"),
    ("day",          "INTEGER"),   # the save's own DAY: — frames, ordering key
    ("civ_day",      "INTEGER"),   # the in-world day the civ log speaks in
    ("ticks_done",   "INTEGER"),
    ("target_ticks", "INTEGER"),
    ("entities",     "INTEGER"),
    ("seed",         "TEXT"),
    ("label",        "TEXT"),
    ("signature",    "TEXT"),
    ("bytes",        "INTEGER"),
    ("created_at",   "INTEGER"),   # unix seconds
    ("host",         "TEXT"),
    ("pid",          "INTEGER"),
    ("cwd",          "TEXT"),
    ("argv",         "TEXT"),      # JSON array, verbatim from the run
    ("complete",     "INTEGER"),   # passed the marker/terminator check
    ("present",      "INTEGER"),   # file still on disk
]


def open_db(path):
    conn = sqlite3.connect(str(path))
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")     # the engine never touches this
    conn.execute(
        """CREATE TABLE IF NOT EXISTS BACKUP (
               id_backup     INTEGER PRIMARY KEY AUTOINCREMENT,
               backup_path   TEXT NOT NULL,
               simulation_id TEXT NOT NULL,
               date_backup   DATE NOT NULL
           )"""
    )
    have = {r["name"] for r in conn.execute("PRAGMA table_info(BACKUP)")}
    for name, decl in EXTRA_COLUMNS:
        if name not in have:
            conn.execute(f"ALTER TABLE BACKUP ADD COLUMN {name} {decl}")

    # Indexing has to be repeatable -- the watcher rescans the directory every
    # few seconds -- so the path is the key and re-seeing a file is a no-op.
    # Older rows may predate that rule, so collapse duplicates before the index
    # is created or CREATE UNIQUE INDEX would fail on them.
    conn.execute(
        "DELETE FROM BACKUP WHERE id_backup NOT IN "
        "  (SELECT MIN(id_backup) FROM BACKUP GROUP BY backup_path)"
    )
    conn.execute(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_backup_path ON BACKUP(backup_path)"
    )

    conn.execute(
        """CREATE TABLE IF NOT EXISTS RESTART (
               id_restart    INTEGER PRIMARY KEY AUTOINCREMENT,
               simulation_id TEXT NOT NULL,
               restored_from TEXT,
               reason        TEXT,
               command       TEXT,
               cwd           TEXT,
               new_pid       INTEGER,
               started_at    INTEGER
           )"""
    )
    conn.commit()
    return conn


# ── reading what the engine left ────────────────────────────────────────────

def read_meta(save):
    """The sidecar for a save, or {} if it never landed.

    The sidecar is written after the save is published, so a crash in between
    leaves a save with no metadata. That save is still perfectly loadable, so
    the caller falls back to the filename and the save's own header rather than
    throwing it away.
    """
    meta = save.with_name(save.name[:-len(".txt")] + ".meta.json")
    try:
        return json.loads(meta.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def inspect_save(save):
    """(complete, day, entity_count) read from the save file itself.

    Only the head and the tail are read: these files run to megabytes and the
    two things worth knowing are at the ends.
    """
    day, entities, complete = None, None, False
    try:
        with save.open("r", encoding="utf-8", errors="replace") as fh:
            first = fh.readline().strip()
            if not first.startswith(SAVE_MARKER):
                return False, None, None
            for _ in range(64):                      # DAY:/ENTITY_COUNT: are near the top
                line = fh.readline()
                if not line:
                    break
                if line.startswith("DAY:"):
                    day = int(line[4:].strip() or 0)
                elif line.startswith("ENTITY_COUNT:"):
                    entities = int(line[13:].strip() or 0)
                    break
        with save.open("rb") as fh:                  # tail: is the file finished?
            fh.seek(0, os.SEEK_END)
            fh.seek(max(0, fh.tell() - 4096))
            complete = SAVE_TERMINATOR.encode() in fh.read()
    except (OSError, ValueError):
        return False, day, entities
    return complete, day, entities


def read_heartbeats(directory):
    beats = []
    for path in sorted(directory.glob(BEAT_GLOB)):
        try:
            hb = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            # The engine replaces this file atomically, so a partial read means
            # a genuinely corrupt file rather than a race. Skip it; the next
            # heartbeat overwrites it anyway.
            continue
        hb["_path"] = path
        beats.append(hb)
    return beats


# ── indexing ────────────────────────────────────────────────────────────────

def index_backups(conn, directory, verbose):
    """Record every published save that is not in the table yet."""
    known = {r["backup_path"] for r in conn.execute("SELECT backup_path FROM BACKUP")}
    added = 0

    for save in sorted(directory.glob(SAVE_GLOB)):
        try:
            rel = str(save.relative_to(REPO)).replace("\\", "/")
        except ValueError:
            rel = str(save)              # --dir pointed outside the repo
        if rel in known:
            continue

        meta = read_meta(save)
        name = NAME_RE.match(save.name)
        complete, hdr_day, hdr_entities = inspect_save(save)
        if not complete:
            # Never index an unfinished save as a restore candidate. A `.part`
            # left by a crash is invisible to the glob; this catches the rarer
            # case of a file truncated in place (a full disk, a copy that died).
            if verbose:
                print(f"{save.name}: incomplete — not indexed", file=sys.stderr, flush=True)
            continue

        try:
            stat = save.stat()
        except OSError:
            continue

        # Saves that did not come from the backup channel -- a --save-on-exit
        # file, something copied in by hand -- are indexed too (this table is
        # the record of what backups exist), but they are filed under "manual":
        # they are never a restore candidate for a run, and pruning leaves them
        # alone. Deleting a file a person put here is not this script's job.
        run_id = meta.get("run_id") or (name.group("run") if name else MANUAL)
        day = meta.get("day")
        if day is None:
            day = hdr_day if hdr_day is not None else (int(name.group("day")) if name else None)
        created = int(meta.get("created_at") or stat.st_mtime)

        conn.execute(
            "INSERT OR IGNORE INTO BACKUP ("
            "  backup_path, simulation_id, date_backup, world_id, day, civ_day,"
            "  ticks_done, target_ticks, entities, seed, label, signature, bytes,"
            "  created_at, host, pid, cwd, argv, complete, present"
            ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (
                rel,
                run_id,
                datetime.datetime.fromtimestamp(created).isoformat(timespec="seconds"),
                meta.get("world_id"),
                day,
                meta.get("civ_day"),
                meta.get("ticks_done"),
                meta.get("target_ticks"),
                meta.get("entities", hdr_entities),
                meta.get("seed"),
                meta.get("label"),
                meta.get("signature") or (name.group("sig") if name else None),
                stat.st_size,
                created,
                meta.get("host"),
                meta.get("pid"),
                meta.get("cwd"),
                json.dumps(meta["argv"]) if meta.get("argv") else None,
                1,
                1,
            ),
        )
        added += 1
        if verbose:
            shown = meta.get("civ_day", day)
            print(f"indexed {save.name}  run={run_id} day={shown} "
                  f"entities={meta.get('entities', hdr_entities)}", flush=True)

    # A row whose file is gone is history, not a restore candidate: keep the
    # record, drop the claim that it is still there.
    for row in conn.execute("SELECT id_backup, backup_path FROM BACKUP WHERE present IS NOT 0"):
        if not (REPO / row["backup_path"]).exists():
            conn.execute("UPDATE BACKUP SET present = 0 WHERE id_backup = ?",
                         (row["id_backup"],))
    conn.commit()
    return added


def prune(conn, keep, verbose):
    """Keep the newest `keep` checkpoints per run; delete the rest from disk.

    Only indexed, superseded backups are touched -- never the newest for a run,
    which is the one a restore would use, and never a file this script has not
    recorded.
    """
    if keep <= 0:
        return 0
    removed = 0
    runs = [r["simulation_id"] for r in conn.execute(
        "SELECT DISTINCT simulation_id FROM BACKUP WHERE present = 1 AND simulation_id != ?",
        (MANUAL,))]
    for run in runs:
        rows = conn.execute(
            "SELECT id_backup, backup_path FROM BACKUP "
            "WHERE simulation_id = ? AND present = 1 "
            "ORDER BY day DESC, created_at DESC, id_backup DESC",
            (run,),
        ).fetchall()
        for row in rows[keep:]:
            path = REPO / row["backup_path"]
            try:
                path.unlink()
                meta = path.with_name(path.name[:-len(".txt")] + ".meta.json")
                if meta.exists():
                    meta.unlink()
            except OSError as exc:
                print(f"prune {path.name}: {exc}", file=sys.stderr, flush=True)
                continue
            conn.execute("UPDATE BACKUP SET present = 0 WHERE id_backup = ?",
                         (row["id_backup"],))
            removed += 1
            if verbose:
                print(f"pruned {path.name}", flush=True)
    conn.commit()
    return removed


# ── crash detection ─────────────────────────────────────────────────────────

def pid_alive(pid):
    """Is this pid a live process on this machine?

    Deliberately conservative: anything that is not a definite "no" counts as
    alive, because the cost of being wrong is starting a second engine on top of
    a running one.
    """
    if not pid or pid <= 0:
        return True
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes

        PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
        STILL_ACTIVE = 259
        ERROR_INVALID_PARAMETER = 87            # no such process
        k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        handle = k32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
        if not handle:
            return ctypes.get_last_error() != ERROR_INVALID_PARAMETER
        try:
            code = wintypes.DWORD()
            if not k32.GetExitCodeProcess(handle, ctypes.byref(code)):
                return True
            return code.value == STILL_ACTIVE
        finally:
            k32.CloseHandle(handle)
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True                             # someone else's process: it exists
    except OSError:
        return True
    return True


def classify(hb, stale_after):
    """None | ("crashed", why) | ("hung", why) — what happened to this run.

    Order matters. Staleness comes first because a pid check on a live process
    can be wrong (pids are reused, and permissions vary), while a heartbeat that
    stopped is unambiguous evidence the tick loop is not turning.
    """
    if hb.get("status") != "running":
        return None                             # finished / extinct / stopped
    age = time.time() - (hb.get("updated_at") or 0)
    if age < stale_after:
        return None
    if hb.get("host") and not same_host(hb["host"]):
        # Written on another machine; the pid here means nothing. Staleness is
        # all there is to go on, and starting that world here is not our call.
        return ("hung", f"heartbeat {int(age)}s old on host {hb['host']}")
    if pid_alive(hb.get("pid")):
        # Alive but silent: a very long tick, a debugger, a stopped process.
        # Resuming would put two engines on the same world.
        return ("hung", f"pid {hb.get('pid')} alive but silent for {int(age)}s")
    return ("crashed", f"pid {hb.get('pid')} gone, heartbeat {int(age)}s old")


def same_host(recorded):
    """Did this heartbeat come from the machine we are running on?

    Compared case-insensitively: hostnames are, and Windows hands the engine
    COMPUTERNAME ("PIERRE") while it hands Python the same name in the case the
    machine was registered with ("Pierre"). Getting this wrong is not cosmetic —
    a mismatch makes every local crash look like another machine's problem and
    silently disables recovery.
    """
    return platform.node().strip().casefold() == str(recorded).strip().casefold()


# ── restore ─────────────────────────────────────────────────────────────────

def latest_backup(conn, run_id):
    return conn.execute(
        "SELECT * FROM BACKUP WHERE simulation_id = ? AND present = 1 AND complete = 1 "
        "ORDER BY day DESC, created_at DESC, id_backup DESC LIMIT 1",
        (run_id,),
    ).fetchone()


def build_command(argv, save_name, ticks_left, seed):
    """The original command line, aimed at a checkpoint.

    Three edits and nothing else, so anything the run was doing -- --db-export,
    --inject, --set knobs, a scenario -- carries over untouched:

      * --load points at the checkpoint. A bare filename is what the engine
        wants: loadGame resolves it inside the saves directory itself.
      * --headless becomes what is LEFT of the run, not what it originally was,
        or a resumed 20 000-tick world would run 20 000 more.
      * --seed is pinned when there is no checkpoint, so a from-scratch restart
        rebuilds the same world instead of a new one. (A pure-digit seed is used
        as the literal master, see WorldSeed::fromString.)
    """
    out, skip = [], False
    for arg in argv:
        if skip:
            skip = False
            continue
        if arg == "--load":
            skip = True                                  # replaced below
            continue
        if arg == "--headless" and ticks_left is not None:
            out += ["--headless", str(max(1, ticks_left))]
            skip = True
            continue
        if arg == "--seed" and save_name is None and seed:
            out += ["--seed", str(seed)]
            skip = True
            continue
        out.append(arg)

    if save_name:
        out += ["--load", save_name]
    elif seed and "--seed" not in out:
        out += ["--seed", str(seed)]
    return out


def spawn(cmd, cwd, run_id, directory):
    """Start the engine so that it outlives this script.

    Two things the default Popen gets wrong for a supervisor:

      * The child inherits our stdout. A simulation prints constantly, so the
        supervisor's console fills with someone else's log — and if nothing is
        draining that pipe the engine eventually blocks on a write and stops
        simulating. Its output goes to a file next to the backups instead.
      * The child inherits our process group, so Ctrl-C in the supervisor's
        terminal (or the terminal simply closing) takes the resumed world down
        with it — which is exactly the thing we just spent a restore fixing.
        A new session/process group cuts that tie.
    """
    log = directory / f"restart_{run_id}.log"
    handle = log.open("a", encoding="utf-8", errors="replace")
    handle.write(f"\n=== {datetime.datetime.now():%Y-%m-%d %H:%M:%S}  {' '.join(cmd)}\n")
    handle.flush()

    kwargs = {"cwd": cwd, "stdout": handle, "stderr": subprocess.STDOUT,
              "stdin": subprocess.DEVNULL}
    if os.name == "nt":
        kwargs["creationflags"] = (subprocess.CREATE_NEW_PROCESS_GROUP |
                                   getattr(subprocess, "DETACHED_PROCESS", 0x00000008))
    else:
        kwargs["start_new_session"] = True

    try:
        return subprocess.Popen(cmd, **kwargs)
    finally:
        handle.close()          # the child holds its own duplicate of the fd


def restore(conn, hb, reason, dry_run, directory):
    """Start the world again from its newest complete checkpoint."""
    run_id = hb.get("run_id") or "unknown"
    row = latest_backup(conn, run_id)

    argv = hb.get("argv") or (json.loads(row["argv"]) if row and row["argv"] else None)
    if not argv:
        print(f"{run_id}: crashed ({reason}) but no command line was recorded — "
              f"cannot resume", file=sys.stderr, flush=True)
        return None

    cwd = hb.get("cwd") or (row["cwd"] if row else None) or str(REPO)
    if row:
        save_name = pathlib.PurePath(row["backup_path"]).name
        # How far the CHECKPOINT had got, never how far the run had got: the
        # crash happened after the checkpoint, and counting those lost ticks as
        # done would cut them off the end of the resumed run. If the sidecar
        # never landed this stays None and the run keeps its original tick
        # count — simulating too much is recoverable, stopping early is not.
        done = row["ticks_done"]
        target = row["target_ticks"] if row["target_ticks"] is not None \
            else hb.get("target_ticks")
    else:
        # Crashed before the first checkpoint. There is no state to resume, but
        # the seed is recorded, so the same world can be built again from tick
        # zero -- which is strictly better than losing the run.
        save_name = None
        done = 0
        target = hb.get("target_ticks")

    left = None
    if target is not None and target > 0 and done is not None and done >= 0:
        left = target - done
        if left <= 0:
            print(f"{run_id}: crashed at the finish line ({done}/{target} ticks) — "
                  f"nothing left to run", flush=True)
            retire(hb, "crashed-complete")
            return None

    cmd = build_command(argv, save_name, left, hb.get("seed") or (row["seed"] if row else None))
    printable = " ".join(cmd)
    origin = save_name or f"scratch (seed {hb.get('seed')})"
    print(f"{run_id}: {reason} — resuming from {origin}\n  $ {printable}\n  (cwd {cwd})",
          flush=True)

    if dry_run:
        return None

    try:
        proc = spawn(cmd, cwd, run_id, directory)
    except OSError as exc:
        print(f"{run_id}: relaunch failed: {exc}", file=sys.stderr, flush=True)
        return None

    conn.execute(
        "INSERT INTO RESTART (simulation_id, restored_from, reason, command, cwd,"
        "                     new_pid, started_at) VALUES (?,?,?,?,?,?,?)",
        (run_id, row["backup_path"] if row else None, reason, printable, cwd,
         proc.pid, int(time.time())),
    )
    conn.commit()
    retire(hb, "restored")
    print(f"{run_id}: restarted as pid {proc.pid} "
          f"(output: backup_log/restart_{run_id}.log)", flush=True)
    return proc


def retire(hb, why):
    """Take a dead heartbeat out of circulation.

    Renaming rather than deleting: the file is the post-mortem, and leaving it
    as `.alive` would have every pass re-detect the same crash forever.
    """
    path = hb.get("_path")
    if not path:
        return
    try:
        path.rename(path.with_suffix(f".{why}"))
    except OSError as exc:
        print(f"{path.name}: {exc}", file=sys.stderr, flush=True)


def recent_restarts(conn, window):
    return conn.execute(
        "SELECT COUNT(*) AS n FROM RESTART WHERE started_at > ?",
        (int(time.time()) - window,),
    ).fetchone()["n"]


# ── reporting ───────────────────────────────────────────────────────────────

def show(conn, limit):
    rows = conn.execute(
        "SELECT * FROM BACKUP ORDER BY created_at DESC, id_backup DESC LIMIT ?",
        (limit,),
    ).fetchall()
    if not rows:
        print("no backups indexed yet")
        return
    print(f"{'run':>18}  {'civ day':>7}  {'pop':>5}  {'size':>9}  when                 file")
    for r in rows:
        mark = "" if r["present"] else "  (gone)"
        size = f"{(r['bytes'] or 0) / 1024:.0f}K"
        day = r["civ_day"] if r["civ_day"] is not None else r["day"]
        print(f"{r['simulation_id']:>18}  {day if day is not None else '?':>7}  "
              f"{r['entities'] if r['entities'] is not None else '?':>5}  {size:>9}  "
              f"{r['date_backup']}  {pathlib.PurePath(r['backup_path']).name}{mark}")

    restarts = conn.execute(
        "SELECT * FROM RESTART ORDER BY started_at DESC LIMIT 5").fetchall()
    if restarts:
        print("\nrecent restarts:")
        for r in restarts:
            when = datetime.datetime.fromtimestamp(r["started_at"] or 0)
            print(f"  {when:%Y-%m-%d %H:%M:%S}  {r['simulation_id']}  "
                  f"pid {r['new_pid']}  <- {pathlib.PurePath(r['restored_from'] or '-').name}"
                  f"  ({r['reason']})")


# ── main loop ───────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(
        description="Index ASHB2 world backups and resume the simulation after a crash.")
    ap.add_argument("--dir", default=str(BACKUP_DIR),
                    help="directory the engine checkpoints into")
    ap.add_argument("--db", default=str(DB_PATH), help="SQLite index")
    ap.add_argument("--interval", type=float, default=30.0,
                    help="seconds between passes (default 30)")
    ap.add_argument("--stale", type=float, default=120.0,
                    help="heartbeat age that counts as not-answering (default 120s)")
    ap.add_argument("--keep", type=int, default=20,
                    help="checkpoints to keep per run, 0 = keep everything")
    ap.add_argument("--max-restarts", type=int, default=3,
                    help="restarts allowed inside --restart-window")
    ap.add_argument("--restart-window", type=int, default=3600,
                    help="seconds the restart budget is measured over")
    ap.add_argument("--index-only", action="store_true",
                    help="index and report crashes, never relaunch")
    ap.add_argument("--restore-now", action="store_true",
                    help="resume every crashed run found in this pass, then exit")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the command a restore would run, do not run it")
    ap.add_argument("--once", action="store_true", help="one pass, then exit")
    ap.add_argument("--list", action="store_true", help="show the index and exit")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    directory = pathlib.Path(args.dir)
    directory.mkdir(parents=True, exist_ok=True)
    conn = open_db(pathlib.Path(args.db))
    verbose = not args.quiet

    if args.list:
        show(conn, 25)
        conn.close()
        return

    if verbose:
        print(f"watching {directory}\nindex     {args.db}", flush=True)

    try:
        while True:
            index_backups(conn, directory, verbose)
            prune(conn, args.keep, verbose)

            for hb in read_heartbeats(directory):
                verdict = classify(hb, args.stale)
                if not verdict:
                    continue
                state, why = verdict
                if state == "hung":
                    # Reported, never acted on: a second engine writing the same
                    # world would corrupt exactly what we are protecting.
                    print(f"{hb.get('run_id')}: {why} — left alone", flush=True)
                    continue
                if args.index_only:
                    print(f"{hb.get('run_id')}: crashed ({why}) — "
                          f"--index-only, not resuming", flush=True)
                    retire(hb, "crashed")
                    continue
                # --restore-now is a person asking for this one, so the budget
                # (which exists to stop an unattended watcher from relaunching a
                # deterministically-crashing world all night) does not apply.
                if not args.restore_now and \
                        recent_restarts(conn, args.restart_window) >= args.max_restarts:
                    print(f"{hb.get('run_id')}: crashed ({why}) but "
                          f"{args.max_restarts} restarts already in the last "
                          f"{args.restart_window}s — something is wrong with the run "
                          f"itself, not with the machine. Not resuming.",
                          file=sys.stderr, flush=True)
                    retire(hb, "crashloop")
                    continue
                restore(conn, hb, why, args.dry_run, directory)

            if args.once or args.restore_now:
                break
            time.sleep(args.interval)
    except KeyboardInterrupt:
        pass
    finally:
        conn.close()


if __name__ == "__main__":
    main()
