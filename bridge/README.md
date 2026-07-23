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
