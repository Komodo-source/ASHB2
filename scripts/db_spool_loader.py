#!/usr/bin/env python3
"""
ASHB2 spool -> PostgreSQL loader.

Watches the directory the engine spools NDJSON chunks into (--db-export), and
for each completed chunk: COPYs it into unlogged temp tables, upserts into the
sim schema in one transaction, then deletes the chunk.

The engine and this script never touch each other. The handoff is the atomic
rename the engine does from `.ndjson.tmp` to `.ndjson`: this script's glob only
matches the finished form, so a half-written chunk is invisible. Either side can
be down for hours without the other noticing -- chunks pile up and get caught up
on, which is the whole reason for doing it this way rather than piping.

Ingest is idempotent. A crash between COPY and unlink leaves the chunk on disk;
the next pass replays it, and because the whole ingest is one transaction --
including the world's high-water mark -- the retry sees exactly the state the
first attempt did and lands the same result.

Re-feeding chunks that were already consumed (copying old ones back in by hand)
is the one weaker case: their days sit behind the recorded high-water mark, so
the prune correctly refuses to act on stale input and edges retired since then
reappear. The next real push clears them, since the prune drops anything left
below the mark.

Usage:
    python scripts/db_spool_loader.py                      # watch forever
    python scripts/db_spool_loader.py --once               # drain and exit
    python scripts/db_spool_loader.py --spool bridge/spool --dsn postgresql://...

Credentials, in precedence order: --dsn, then $ASHB2_DSN / $PG_DSN, then the
discrete PG_* fields, checking the real environment before website/.env at each
step. Same resolution website/config.php uses, so the loader and the web app
cannot end up pointed at different databases. DB_* is MySQL (the accounts) and
is never read here.
"""

import argparse
import csv
import io
import json
import os
import pathlib
import sys
import time

try:
    import psycopg2
except ImportError:
    sys.exit("psycopg2 is required:  pip install psycopg2-binary")

REPO = pathlib.Path(__file__).resolve().parent.parent

# ── column lists ────────────────────────────────────────────────────────────
# Explicit rather than derived from the chunk, so a key the engine adds before
# the schema has caught up is ignored instead of blowing up the transaction,
# and a key it stops emitting lands as NULL instead of shifting every column.

ENTITY_COLS = [
    "world_id", "sim_id", "last_seen_day",
    "name", "sex", "birth_day", "birth_year", "web_char_id",
    "age", "life_stage", "health", "hunger", "food_store", "hygiene",
    "fatigue_level", "injury_level", "sleep_pressure", "sleep_quality",
    "antibody", "disease_type", "base_immunity",
    "happiness", "stress", "stress_baseline", "mental_health", "loneliness",
    "boredom", "general_anger", "esteem", "sense_of_purpose", "grief_intensity",
    "social_deficit", "days_without_social_action", "meeting_count",
    "openness", "conscientiousness", "extraversion", "agreeableness", "neuroticism",
    "value_family", "value_achievement", "value_spiritual", "value_hedonism",
    "value_collectivism",
    "self_perceived_extraversion", "self_perceived_agreeableness",
    "self_perceived_neuroticism", "self_primary_identity", "self_esteem",
    "self_efficacy", "self_calibration",
    "had_secure_attachment", "childhood_trauma_score", "childhood_nurturing_score",
    "attachment_style",
    "raw_anger", "expressed_anger", "suppression_debt",
    "self_story", "dominant_value", "narrative_coherence",
    "creed_held", "creed_militarism", "creed_tolerance", "creed_asceticism",
    "creed_authority", "creed_afterlife_focus", "creed_devotion",
    "nutritional_status", "hydration_level", "energy_level",
    "parent1_id", "parent2_id", "family_id", "lineage_depth",
    "tribe_id", "religion_id", "origin_region_id", "dominance_rank",
    "social_class", "auctoritas", "integrity", "specialization", "is_specialist",
    "role_since_day", "cultural_capital", "culture_traits", "committed_traits",
    "home_attachment",
    "wealth", "pos_x", "pos_y",
    "current_goal", "last_action", "inner_monologue",
]

EDGE_BASE = ["world_id", "from_id", "to_id", "last_seen_day"]

# tag in the NDJSON  ->  (target table, columns, primary key)
TABLES = {
    "entity": ("sim.entity", ENTITY_COLS, ["world_id", "sim_id"]),
    "desire": ("sim.entity_desire", EDGE_BASE + ["desire"], ["world_id", "from_id", "to_id"]),
    "anger": ("sim.entity_anger", EDGE_BASE + ["anger"], ["world_id", "from_id", "to_id"]),
    "social": ("sim.entity_social", EDGE_BASE + ["social"], ["world_id", "from_id", "to_id"]),
    "couple": ("sim.entity_couple",
               EDGE_BASE + ["commitment", "satisfaction", "trust", "suspicion", "days_together"],
               ["world_id", "from_id", "to_id"]),
    "mental_model": ("sim.entity_mental_model",
                     EDGE_BASE + ["perceived_extraversion", "perceived_agreeableness",
                                  "perceived_neuroticism", "estimated_happiness",
                                  "estimated_anger", "estimated_stress", "trust_level",
                                  "predictability", "perceived_intentionality",
                                  "confidence", "last_observed_day",
                                  "last_interaction_day", "last_interaction_outcome",
                                  "faked_by_them", "fake_trust_gain"],
                     ["world_id", "from_id", "to_id"]),
}

# Parents before children: sim.entity's FK to sim.world, and every edge table's
# FK to sim.entity, both have to already resolve when their rows land.
INGEST_ORDER = ["entity", "desire", "anger", "social", "couple", "mental_model"]

EDGE_TABLES = ["sim.entity_desire", "sim.entity_anger", "sim.entity_social",
               "sim.entity_couple", "sim.entity_mental_model"]


# ── connection ──────────────────────────────────────────────────────────────

def load_dotenv(path):
    """Minimal .env reader. Real environment variables win, as the file says."""
    values = {}
    if not path.exists():
        return values
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        values[k.strip()] = v.strip().strip('"').strip("'")
    return values


def unbracket(host):
    """Strip the brackets from an IPv6 literal.

    A URL must bracket an IPv6 address so its colons cannot be mistaken for the
    port separator, and people carry that habit into the bare host setting.
    libpq's `host` keyword wants the address without them — `[2001:db8::1]` is
    read as a hostname containing bracket characters and resolves to nothing.
    Hostnames and IPv4 literals are returned unchanged.
    """
    if host and len(host) > 1 and host[0] == "[" and host[-1] == "]":
        return host[1:-1]
    return host


def connect(dsn):
    """Open the simulation database.

    Resolution order matches website/config.php, which is the other reader of
    the same .env: an explicit URL wins, then the discrete PG_* fields, then
    the local defaults.

    DB_* is deliberately NOT consulted. In this repo those are the MySQL
    accounts credentials (website/.env says so in as many words), and an
    earlier version of this function listed them as fallbacks for PGHOST /
    PGDATABASE / PGUSER — so with no PG_* set the loader would dial the MySQL
    host as though it spoke the Postgres wire protocol. The failure was a
    connection timeout against someone else's server, which reads as "the
    database is down" rather than "you are pointed at the wrong database".
    """
    env = load_dotenv(REPO / "website" / ".env")

    def pick(*names, default=None):
        """Real environment first, then .env — the precedence .env documents."""
        for n in names:
            if os.environ.get(n):
                return os.environ[n]
        for n in names:
            if env.get(n):
                return env[n]
        return default

    # PG_POOLER_URL last, and on Supabase it is the only one that connects from
    # a host without IPv6: the direct name (db.<ref>.supabase.co) publishes an
    # AAAA record and no A record, so it fails to resolve rather than failing to
    # connect. The pooler answers over IPv4.
    url = dsn or pick("ASHB2_DSN", "PG_DSN", "PG_POOLER_URL")
    if url:
        # Passed through verbatim: libpq parses the URI itself, including a
        # bracketed IPv6 literal, and does it more correctly than we would.
        return psycopg2.connect(url)

    # sslmode is passed only when set: libpq's own default ("prefer") is the
    # right thing locally, and forcing an empty string is a parse error.
    params = dict(
        host=unbracket(pick("PGHOST", "PG_HOST", default="127.0.0.1")),
        port=pick("PGPORT", "PG_PORT", default="5432"),
        dbname=pick("PGDATABASE", "PG_NAME", default="ashb2"),
        user=pick("PGUSER", "PG_USER", default="postgres"),
        password=pick("PGPASSWORD", "PG_PASS", default=""),
    )
    sslmode = pick("PGSSLMODE", "PG_SSLMODE")
    if sslmode:
        params["sslmode"] = sslmode
    return psycopg2.connect(**params)


def describe(conn):
    """user@host:port/dbname — for the startup line. Never the password."""
    p = conn.get_dsn_parameters()
    return (f"{p.get('user','?')}@{p.get('host','?')}:{p.get('port','?')}"
            f"/{p.get('dbname','?')}")


# ── ingest ──────────────────────────────────────────────────────────────────

def copy_into_temp(cur, tmp, target, cols, rows):
    """CTAS the shape of the target, then stream the rows in with COPY.

    `CREATE TABLE AS ... WITH NO DATA` rather than `LIKE` on purpose: LIKE
    carries NOT NULL across, and columns the engine legitimately omits (alive,
    which the upsert sets) would fail on arrival.
    """
    cur.execute(
        f"CREATE TEMP TABLE {tmp} ON COMMIT DROP AS "
        f"SELECT {','.join(cols)} FROM {target} WITH NO DATA"
    )
    buf = io.StringIO()
    writer = csv.writer(buf, lineterminator="\n")
    for row in rows:
        writer.writerow(["" if row.get(c) is None else row.get(c) for c in cols])
    buf.seek(0)
    cur.copy_expert(
        f"COPY {tmp} ({','.join(cols)}) FROM STDIN WITH (FORMAT csv, NULL '')",
        buf,
    )


def upsert(cur, tag, rows):
    target, cols, key = TABLES[tag]
    tmp = "tmp_" + tag
    copy_into_temp(cur, tmp, target, cols, rows)

    updatable = [c for c in cols if c not in key]
    # sim.entity carries one column the engine never emits. The tick's batch
    # erase removes everything with health <= 0 before the export runs, so an
    # entity that shows up in a push is alive by construction -- including one
    # previously reaped that came back because the world was reloaded from an
    # older save.
    insert_cols = cols + (["alive"] if tag == "entity" else [])
    select_cols = cols + (["true"] if tag == "entity" else [])
    sets = [f"{c}=EXCLUDED.{c}" for c in updatable]
    if tag == "entity":
        sets.append("alive=true")

    # DISTINCT ON is not optional: a chunk holds several pushes, so the same
    # (world, entity) turns up once per day it spans. Postgres refuses to let
    # ON CONFLICT touch a row twice in one command ("cannot affect row a second
    # time"), which would abort the chunk. Keeping the highest last_seen_day per
    # key is also just correct -- these are current-state tables, and only the
    # newest push in the chunk describes the present.
    cur.execute(
        f"INSERT INTO {target} ({','.join(insert_cols)}) "
        f"SELECT DISTINCT ON ({','.join(key)}) {','.join(select_cols)} FROM {tmp} "
        f"ORDER BY {','.join(key)}, last_seen_day DESC "
        f"ON CONFLICT ({','.join(key)}) DO UPDATE SET {', '.join(sets)}"
    )
    return len(rows)


def ensure_worlds(cur, world_rows, world_ids):
    """Every world_id in the chunk must have a sim.world row before entities land.

    A chunk that begins mid-push has entity rows but no world header, so the
    ids are taken from the entity rows too rather than trusting the header to
    be present.
    """
    labelled = {r["world_id"]: r for r in world_rows}
    for wid in sorted(world_ids):
        r = labelled.get(wid, {})
        cur.execute(
            "INSERT INTO sim.world (world_id, label, seed) VALUES (%s,%s,%s) "
            "ON CONFLICT (world_id) DO UPDATE SET "
            "  label = COALESCE(EXCLUDED.label, sim.world.label), "
            "  seed  = COALESCE(EXCLUDED.seed,  sim.world.seed)",
            (wid, r.get("label"), r.get("seed")),
        )


def reap(cur, world_id, prev_day, new_day):
    """Retire whatever the engine stopped emitting.

    Absence is the death signal, and the engine sends no tombstones: the tick's
    batch erase drops every entity with health <= 0 before the export runs, so
    a person who stops appearing has died. The same rule retires a broken couple
    or a forgotten mental model, which likewise just stop being emitted.

    A push can be split across two chunks, so a day is only known to be COMPLETE
    once a strictly later day shows up. Hence the threshold is the previous high
    water mark, not the current one: rows still sitting below `prev_day` were
    missing from a push we now know finished. Using `new_day` here would delete
    half the population's edges every time a chunk boundary landed mid-push.
    """
    if new_day <= prev_day:
        return 0, 0
    dropped = 0
    for table in EDGE_TABLES:
        cur.execute(
            f"DELETE FROM {table} WHERE world_id = %s AND last_seen_day < %s",
            (world_id, prev_day),
        )
        dropped += cur.rowcount
    cur.execute(
        "UPDATE sim.entity SET alive = false "
        "WHERE world_id = %s AND last_seen_day < %s AND alive",
        (world_id, prev_day),
    )
    died = cur.rowcount
    cur.execute(
        "UPDATE sim.world SET last_day = %s, last_push_at = now() WHERE world_id = %s",
        (new_day, world_id),
    )
    return died, dropped


def ingest(conn, chunk, verbose):
    grouped = {tag: [] for tag in TABLES}
    worlds = []
    bad = 0

    with chunk.open("r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                # One torn line should cost one row, not the chunk. The only way
                # to get here is a chunk the engine never finished renaming.
                bad += 1
                continue
            tag = row.get("t")
            if tag == "world":
                worlds.append(row)
            elif tag in grouped:
                grouped[tag].append(row)
            else:
                bad += 1

    world_ids = {r["world_id"] for r in worlds}
    world_ids |= {r["world_id"] for r in grouped["entity"]}
    max_day = {}
    for r in grouped["entity"]:
        wid = r["world_id"]
        max_day[wid] = max(max_day.get(wid, -1), r["last_seen_day"])

    counts = {}
    with conn:                       # one transaction; rollback on any failure
        with conn.cursor() as cur:
            prev = {}
            for wid in world_ids:
                cur.execute("SELECT last_day FROM sim.world WHERE world_id = %s", (wid,))
                got = cur.fetchone()
                prev[wid] = got[0] if got else -1

            ensure_worlds(cur, worlds, world_ids)

            for tag in INGEST_ORDER:
                if grouped[tag]:
                    counts[tag] = upsert(cur, tag, grouped[tag])

            reaped = {}
            for wid, day in max_day.items():
                reaped[wid] = reap(cur, wid, prev.get(wid, -1), day)

    if verbose:
        summary = " ".join(f"{k}={v}" for k, v in counts.items()) or "empty"
        days = " ".join(f"w{w}@day{d}" for w, d in sorted(max_day.items()))
        extra = ""
        for wid, (died, dropped) in reaped.items():
            if died or dropped:
                extra += f" reaped(w{wid}): {died} died, {dropped} edges"
        warn = f" [{bad} unparseable]" if bad else ""
        print(f"{chunk.name}: {summary} {days}{extra}{warn}", flush=True)


# ── main loop ───────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description="Load ASHB2 spool chunks into PostgreSQL.")
    ap.add_argument("--spool", default=str(REPO / "bridge" / "spool"),
                    help="directory the engine writes chunks to")
    ap.add_argument("--dsn", default=None, help="postgresql://... (overrides .env)")
    ap.add_argument("--once", action="store_true", help="drain what is there and exit")
    ap.add_argument("--interval", type=float, default=1.0,
                    help="seconds to sleep when the spool is empty")
    ap.add_argument("--max-retries", type=int, default=3,
                    help="attempts before a chunk is set aside as .failed")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    spool = pathlib.Path(args.spool)
    spool.mkdir(parents=True, exist_ok=True)

    try:
        conn = connect(args.dsn)
    except psycopg2.Error as exc:
        sys.exit(f"cannot connect: {exc}\n"
                 f"Set PG_DSN in website/.env (or pass --dsn).")

    # Fail here rather than on the first chunk. Without this the loader starts
    # cleanly, waits for data, and only reports "relation sim.entity does not
    # exist" once a chunk arrives — by which time the message looks like an
    # engine problem instead of a database that was never set up.
    with conn.cursor() as cur:
        cur.execute("SELECT to_regclass('sim.entity')")
        if cur.fetchone()[0] is None:
            conn.close()
            sys.exit("schema sim.* is missing — create it with:\n"
                     "    php website/bin/pg_check.php --import")
    conn.rollback()

    if not args.quiet:
        print(f"connected to {describe(conn)}", flush=True)
        print(f"watching {spool}", flush=True)

    attempts = {}
    try:
        while True:
            # Sorted by name, which the engine makes chronological: a
            # fixed-width epoch, then pid, then a per-run counter. Ordering
            # matters because a later chunk's reap depends on earlier days
            # already being in.
            chunks = sorted(spool.glob("*.ndjson"))
            if not chunks:
                if args.once:
                    break
                time.sleep(args.interval)
                continue

            for chunk in chunks:
                try:
                    ingest(conn, chunk, not args.quiet)
                except psycopg2.Error as exc:
                    # Nothing was committed, so a retry starts from the same
                    # clean state. But a chunk that fails deterministically
                    # (schema drift, a value the column can't hold) must not
                    # wedge the queue behind it: after a few tries it is set
                    # aside for inspection rather than retried forever or --
                    # worse -- deleted. Before this, a permanent failure made
                    # even --once spin.
                    conn.rollback()
                    attempts[chunk.name] = attempts.get(chunk.name, 0) + 1
                    print(f"{chunk.name}: {exc}", file=sys.stderr, flush=True)
                    if attempts[chunk.name] >= args.max_retries:
                        chunk.rename(chunk.with_suffix(".ndjson.failed"))
                        print(f"{chunk.name}: set aside after "
                              f"{attempts[chunk.name]} attempts",
                              file=sys.stderr, flush=True)
                    else:
                        time.sleep(min(30.0, args.interval * 10))
                    # Stop the pass rather than skipping ahead: chunks must be
                    # applied in order, because reaping a day depends on every
                    # earlier day already being in.
                    break
                chunk.unlink()       # only after the transaction committed

            # One pass is the contract for --once, whatever the outcome.
            if args.once:
                break
    except KeyboardInterrupt:
        pass
    finally:
        conn.close()


if __name__ == "__main__":
    main()
