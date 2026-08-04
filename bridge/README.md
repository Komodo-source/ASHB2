# bridge/ — the C++ engine ↔ web app file bridge

This directory is the only channel between the PHP web app (`website/`) and the
C++ simulation engine (`app.exe`). Everything in here except this README is a
runtime artifact and is gitignored (`bridge/*.txt`, `bridge/*.json`,
`bridge/*.log` in the repo-root `.gitignore`).

| File | Direction | Purpose |
|---|---|---|
| `world.txt` | engine ↔ engine | the ONE persistent world save; every scheduled run does `--load bridge/world.txt ... --save-file bridge/world.txt` (used from Phase 4 on) |
| `inject.txt` | PHP → engine | new web characters + feedback nudges, applied via `--inject` |
| `report.json` | engine → PHP | per-web-character daily report (Phase 3) |
| `scheduler.log` | scheduler | stdout of scheduled engine runs (Phase 4) |
| `spool/` | engine → PostgreSQL | NDJSON chunks for `scripts/db_spool_loader.py` |

## spool/ — the PostgreSQL channel

One command, when you just want a run to end up in the database:

```
python scripts/run_sim_to_db.py --days 500 --seed mars --label "first world"
```

It checks the database is reachable and `sim.*` applied **before** starting the
engine, runs producer and consumer together, and — the part that is easy to get
wrong by hand — drains the spool once more *after* the engine exits, because
`dbexport::shutdown()` publishes the tail chunk on the way out. It then prints
what actually landed and fails if anything is still unread. Credentials come
from `PG_DSN` in `website/.env`, same as the web app; `--dsn` overrides.

The two halves separately, which is what the scheduler uses:

```
./app --headless 250 --db-export bridge/spool --world-id 1     # producer
python scripts/db_spool_loader.py                               # consumer
```

First-time setup:

```
# 1. put PG_DSN=postgresql://... in website/.env
php website/bin/pg_check.php --import      # create sim.*
php website/bin/pg_check.php               # verify, then run the sim
```

The engine appends rows to `<epoch>_<pid>_<counter>.ndjson.tmp` and every 5 000
rows closes it and renames it to `.ndjson`. **That rename is the whole protocol.**
While the name ends in `.tmp` the loader's glob ignores it; the moment it is
renamed the chunk is complete and on disk. Rename inside one directory is atomic
on NTFS and ext4, so a half-written chunk is never visible.

The loader ingests chunks oldest-first (the names sort chronologically), COPYs
each into `sim.*` in a single transaction, then deletes it. Disk stays bounded at
about one chunk no matter how long the world runs — unlike `tick_history.jsonl`,
which nothing consumes and which reached 188 MB.

Neither side can block or crash the other. The loader can be down for hours and
catch up; the engine can be between scheduled runs and the loader just sleeps.
A chunk that fails to load three times is renamed `.failed` rather than retried
forever or deleted, so it can be inspected without wedging the queue.

Schema: `website/sql/schema_pg.sql` (an entity's own attributes, plus one table
per pointed association — desire, anger, social, couple, mental model).
Deaths arrive as absence: the tick's batch erase removes entities with
health <= 0 before the export runs, so someone who stops being pushed has died
and the loader flips them to `alive = false` without deleting the row.

Everything under `spool/` is runtime data and is gitignored.

## src/backup/ — the crash-recovery channel

Not under `bridge/`, but the same producer/consumer shape as `spool/`, so it is
documented next to it. There the artifact is a row and the consumer is
PostgreSQL; here the artifact is a whole world save and the consumer is a SQLite
index that can put the run back on its feet.

```
./app --headless 20000 --seed mars --backup-every 300     # producer (seconds!)
python src/backup/auto_file_index_database.py             # consumer
```

Three kinds of file land in `src/backup/backup_log/`:

| File | What it is |
|---|---|
| `<sig>_bk_<run>_<nnnnnn>_d<day>.txt` | a full `ASHB2_SAVE_V3` world |
| `<sig>_bk_<run>_<nnnnnn>_d<day>.meta.json` | what it is and how to resume it (day, ticks done/target, seed, cwd, argv) |
| `run_<run>.alive` | the heartbeat, rewritten about once a second |

**Cadence is wall-clock, not ticks.** A tick costs more as the population grows,
so "every 500 ticks" is minutes early in a run and an hour late once the world
is crowded. Seconds bound the thing you actually care about: how much history a
crash can cost you.

**Publication is the same atomic rename as the spool.** `saveGame` writes under
`.part`; only once the file is closed is it renamed to `.txt`. The indexer's
glob only matches the published form, so a save being written — or one torn by
the very crash we are recovering from — is never picked as a restore point. Each
indexed file is checked anyway: format marker on the first line, `CS_END` on the
last.

**The heartbeat is the crash signal.** While the engine lives it says
`"running"`; a clean exit rewrites it as `"finished"`, and a world that dies out
as `"extinct"`, through an `atexit` hook. A segfault, an OOM kill or a power cut
runs no `atexit` handler — so the file is left saying `"running"` with a pid that
no longer exists. Stale heartbeat **and** dead pid means a crash; the supervisor
then relaunches the recorded argv with `--load <newest checkpoint>` and the
tick count that is *left*, not the one originally asked for. Stale but pid still
alive is reported and left alone: a second engine on the same world would
corrupt exactly what is being protected. `--extinct` and `--finished` runs are
never resumed.

```
python src/backup/auto_file_index_database.py --list        # what is indexed
python src/backup/auto_file_index_database.py --index-only  # never relaunch
python src/backup/auto_file_index_database.py --dry-run     # print the command only
python src/backup/auto_file_index_database.py --restore-now # resume by hand, one pass
```

Restarts are budgeted (`--max-restarts 3` per `--restart-window 3600`s): a world
that crashes deterministically is a bug to look at, not something to relaunch all
night. Old checkpoints are pruned to `--keep 20` per run — never the newest, and
never a save that did not come from this channel. Everything in `backup_log/`
plus the SQLite index is runtime data and gitignored.

## TICKS_PER_CIV_DAY = 1

**One headless tick advances exactly one civ-log day.**

How it was measured (2026-07-10): built at commit-of-Phase-2, then

```
app.exe --headless 200 --seed 777
```

- `src/data/civilization_log.txt` `kind=snapshot` lines showed
  `day=0, 25, 50, 75, 100, 125, 150, 175` (snapshot cadence is every 25 days),
  and the maximum `day=` value anywhere in the log was **199** — i.e. a
  200-tick run covers civ days 0–199.
- This matches the clock model in `src/core/SimClock.h`: 1 headless tick =
  `FRAMES_PER_TICK` (60) frames = 1 in-world day (`tick() = frame/60`,
  `day() = tick()`). Note the civilization ENGINE only executes every
  `TICKS_PER_CIV_TICK = 5` days, but the day counter itself advances 1 per tick.

So the Phase-4 scheduler, which must advance **one civ-day per 6-hour run**,
passes `--headless 1` per sim-day (in practice: `--headless <N>` advances
exactly N civ-days).

## inject.txt block format

Parsed by `applyWebInjectFile` in `src/main.cpp` — same line-based `KEY:value`
idiom as the save files. One block per character, terminated by `END`.
Unknown lines are ignored; malformed blocks are skipped with a stderr note
(never fatal); a missing/empty file is a no-op.

```
CHAR:42               MySQL characters.id
NAME:Naelle
OPENNESS:62           NUDGE:0 → absolute Big Five trait, 0-100
CONSCIENTIOUSNESS:55  NUDGE:1 → signed delta, clamped to [-10,+10] per cycle
EXTRAVERSION:71
AGREEABLENESS:48
NEUROTICISM:33
NUDGE:0               0 = spawn a new entity, 1 = adjust the living entity
END                       whose webCharId == CHAR
```

Behavior:

- **NUDGE:0 (spawn)**: a new adult (~20) entity is constructed exactly like a
  founder (same constructor/derivations: values, integrity, attachment,
  psychology, starting stats, goals), then the block's name/personality and
  `webCharId = CHAR` are applied. It is placed at the centre of the mid-sized
  tribe closest to the median tribe population (world centre if no tribes);
  tribe membership is NOT forced — absorption logic recruits naturally.
  Logged as `kind=web_spawn charId=... entityId=...` in the civ log.
  A `CHAR` id already present in the world skips the spawn
  (`kind=web_spawn_duplicate`).
- **NUDGE:1 (feedback)**: each trait line is a signed delta clamped to
  [-10,+10], applied to the living entity with that `webCharId`, traits then
  clamped to [0,100]. Logged as `kind=web_nudge ... dO=.. dC=..`; if no living
  entity matches, `kind=web_nudge_missing`.

The `webCharId` tag is serialized as the `WEBCHARID:` key — the LAST key of
each entity block in the save file (append-only format; old saves without it
still load).

## priors_active.txt — the AI-upgrade tuning surface (A2/E1)

Loaded once at engine boot by `mind::loadPriors("bridge/priors_active.txt")`
(`src/ai/MindUpgrade.cpp`). **A missing file is a silent no-op**: the compiled
defaults reproduce the classic scorer bit-for-bit. This file is what the
human-in-the-loop upgrade loop (`plans/human-in-the-loop-upgrade.md`) writes as
`priors_vN` and symlinks/copies to `priors_active.txt` when a new AI version
ships. Same `KEY:value` idiom, malformed lines keep their default:

```
VERSION:1
W_REQUIRE:0.20     additive blend weights of the 5 base scoring terms
W_NEED:0.25        (requirement fitness, need satisfaction, memory bias,
W_MEMBIAS:0.10      variety, social influence)
W_VARIETY:0.10
W_SOCIAL:0.19
M_CONTEXT:1.0      multiplicative factor intensities: 1.0 = engine default,
M_PERSONA:1.0      0.0 = factor contributes nothing, >1 = amplified.
M_VALUE:1.0        Applied as  1 + (factor - 1) * M  (guarded: M==1.0 leaves
M_GRIEF:1.0        the factor bit-identical).
M_PHEROM:1.0
M_ENV:1.0
M_NORM:1.0
M_EMOTION:1.0      B1 discrete-emotion action tendencies
M_INTENT:1.0       B4 intention pull
```

Engine stdout confirms a load with `PRIORS: loaded vN from ...`.
