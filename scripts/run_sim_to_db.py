#!/usr/bin/env python3
"""
ASHB2: run a simulation locally and land it in PostgreSQL — one command.

    python scripts/run_sim_to_db.py --days 500

Starts the engine with --db-export, runs the spool loader alongside it, and
does not return until the engine has exited AND every chunk it produced has
been committed. That last part is the whole reason this script exists: the
engine and the loader are deliberately decoupled (see bridge/README.md), so
running them by hand means the run "finishes" while the tail of the world is
still sitting in the spool directory unread.

Both halves are the real thing, not reimplementations — the engine binary and
scripts/db_spool_loader.py. This only sequences them:

  1. preflight — binary present, database reachable, sim.* schema applied
  2. start the engine
  3. drain the spool on an interval while it runs
  4. when it exits, drain once more (the shutdown flush publishes the tail)
  5. report what actually landed in the database

Exit status is the engine's, unless ingest failed — a run whose data never
reached PostgreSQL is a failed run even when the simulation itself was fine.
"""

import argparse
import os
import pathlib
import subprocess
import sys
import time

REPO = pathlib.Path(__file__).resolve().parent.parent
LOADER = REPO / "scripts" / "db_spool_loader.py"

sys.path.insert(0, str(REPO / "scripts"))
try:
    import psycopg2                     # noqa: F401  (db_spool_loader needs it)
    from db_spool_loader import connect, describe
except ImportError as exc:
    sys.exit(f"{exc}\npsycopg2 is required:  pip install psycopg2-binary")


def engine_binary(explicit):
    if explicit:
        p = pathlib.Path(explicit)
        if not p.is_file():
            sys.exit(f"engine not found: {p}")
        return p
    # app.exe on Windows, app elsewhere; the CMake target is `app` either way.
    for name in ("app.exe", "app"):
        p = REPO / name
        if p.is_file():
            return p
    sys.exit("engine binary not found — build it first:  cmake --build .")


def drain(spool, dsn, once=True):
    """Run the loader for one pass. Returns True if it exited cleanly.

    A subprocess rather than an in-process call so the loader stays the single
    implementation of ingest, with its own transaction handling and its own
    .failed quarantine rules.
    """
    cmd = [sys.executable, str(LOADER), "--spool", str(spool)]
    if once:
        cmd.append("--once")
    if dsn:
        cmd += ["--dsn", dsn]
    return subprocess.run(cmd, cwd=str(REPO)).returncode == 0


def report(conn, world_id):
    with conn.cursor() as cur:
        cur.execute(
            "SELECT last_day, "
            "  (SELECT count(*) FROM sim.entity e WHERE e.world_id=w.world_id), "
            "  (SELECT count(*) FROM sim.entity e WHERE e.world_id=w.world_id AND e.alive) "
            "FROM sim.world w WHERE w.world_id=%s",
            (world_id,),
        )
        row = cur.fetchone()
        if not row:
            print(f"world {world_id}: nothing landed in the database", file=sys.stderr)
            return False
        day, total, alive = row
        print(f"world {world_id}: day {day}, {alive} alive of {total} ever recorded")

        cur.execute(
            "SELECT 'desire', count(*) FROM sim.entity_desire WHERE world_id=%s "
            "UNION ALL SELECT 'anger', count(*) FROM sim.entity_anger WHERE world_id=%s "
            "UNION ALL SELECT 'social', count(*) FROM sim.entity_social WHERE world_id=%s "
            "UNION ALL SELECT 'couple', count(*) FROM sim.entity_couple WHERE world_id=%s "
            "UNION ALL SELECT 'mental_model', count(*) FROM sim.entity_mental_model WHERE world_id=%s",
            (world_id,) * 5,
        )
        edges = ", ".join(f"{k}={v}" for k, v in cur.fetchall())
        print(f"  edges: {edges}")
    return True


def main():
    ap = argparse.ArgumentParser(
        description="Run the simulation and load it into PostgreSQL.")
    ap.add_argument("--days", type=int, default=500,
                    help="civ-days to simulate (one headless tick = one day)")
    ap.add_argument("--world-id", type=int, default=1,
                    help="sim.world row this run writes to")
    ap.add_argument("--seed", default=None, help="world seed")
    ap.add_argument("--label", default=None, help="label stored on sim.world")
    ap.add_argument("--spool", default=str(REPO / "bridge" / "spool"))
    ap.add_argument("--dsn", default=None, help="postgresql://... (overrides .env)")
    ap.add_argument("--engine", default=None, help="path to the engine binary")
    ap.add_argument("--load", default=None, help="resume from a save file")
    ap.add_argument("--save-file", default=None, help="write the world here when done")
    ap.add_argument("--drain-interval", type=float, default=5.0,
                    help="seconds between ingest passes while the engine runs")
    ap.add_argument("--keep-spool", action="store_true",
                    help="do not clear leftover chunks before starting")
    args = ap.parse_args()

    engine = engine_binary(args.engine)
    spool = pathlib.Path(args.spool)
    spool.mkdir(parents=True, exist_ok=True)

    # ── 1. preflight ────────────────────────────────────────────────────────
    # Check the database BEFORE burning minutes of simulation. The failure this
    # prevents is the expensive one: a long run that completes perfectly and
    # then has nowhere to go.
    try:
        conn = connect(args.dsn)
    except Exception as exc:
        sys.exit(f"cannot connect to the simulation database: {exc}\n"
                 f"Set PG_DSN in website/.env, or pass --dsn.")
    with conn.cursor() as cur:
        cur.execute("SELECT to_regclass('sim.entity')")
        if cur.fetchone()[0] is None:
            sys.exit("schema sim.* is missing — create it with:\n"
                     "    php website/bin/pg_check.php --import")
    conn.rollback()
    print(f"database: {describe(conn)}")

    # Chunks from an earlier run are still valid input and would be ingested
    # into this world, silently mixing two runs. Clear them unless asked not to.
    stale = sorted(spool.glob("*.ndjson"))
    if stale and not args.keep_spool:
        for f in stale:
            f.unlink()
        print(f"cleared {len(stale)} leftover chunk(s) from {spool}")

    # ── 2. the engine ───────────────────────────────────────────────────────
    cmd = [str(engine), "--headless", str(args.days),
           "--db-export", str(spool), "--world-id", str(args.world_id)]
    if args.seed:      cmd += ["--seed", args.seed]
    if args.load:      cmd += ["--load", args.load]
    if args.save_file: cmd += ["--save-file", args.save_file]
    if args.label:
        # --label is not an engine flag; the exporter reads it from the
        # environment so the world row can be named without touching argv.
        os.environ["ASHB2_LABEL"] = args.label

    print(f"engine: {' '.join(cmd)}")
    started = time.time()
    proc = subprocess.Popen(cmd, cwd=str(REPO))

    # ── 3. drain while it runs ──────────────────────────────────────────────
    ingest_failed = False
    try:
        while proc.poll() is None:
            time.sleep(args.drain_interval)
            if not drain(spool, args.dsn):
                ingest_failed = True
        # ── 4. the tail ─────────────────────────────────────────────────────
        # dbexport::shutdown() publishes the final partial chunk as the process
        # exits, so the last pass has to happen AFTER the engine is gone.
        if not drain(spool, args.dsn):
            ingest_failed = True
    except KeyboardInterrupt:
        print("\ninterrupted — stopping the engine and draining what exists",
              file=sys.stderr)
        proc.terminate()
        proc.wait()
        drain(spool, args.dsn)

    elapsed = time.time() - started
    rc = proc.returncode

    # ── 5. what actually landed ─────────────────────────────────────────────
    print(f"\nengine exited {rc} after {elapsed:.0f}s")
    if not report(conn, args.world_id):
        ingest_failed = True
    conn.close()

    left = sorted(spool.glob("*.ndjson"))
    failed = sorted(spool.glob("*.failed"))
    if left:
        print(f"WARNING: {len(left)} chunk(s) still unread in {spool}", file=sys.stderr)
        ingest_failed = True
    if failed:
        print(f"WARNING: {len(failed)} chunk(s) quarantined as .failed in {spool}",
              file=sys.stderr)
        ingest_failed = True

    sys.exit(rc or (1 if ingest_failed else 0))


if __name__ == "__main__":
    main()
