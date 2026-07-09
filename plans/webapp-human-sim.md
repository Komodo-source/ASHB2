# Human-in-the-Loop Web App Plan — "Release Yourself Into the World"

**Goal (MVP loop)**: a user registers, answers a psychometric questionnaire that creates a
character modeled on themselves, and releases it into the ASHB2 world. The world advances
**4 simulation days per human day** (one engine run every 6 hours). Each day the user opens
their dashboard, reads what their character did — role, tribe, relationships, health, notable
events — and submits feedback that can nudge the character before the next cycle.

**Architecture decision (made in Phase 0, do not relitigate)**: the simulation is the
EXISTING C++ engine running as a scheduled headless process against one persistent world
save. PHP never simulates anything. The two sides talk through *bridge files*:

```
MySQL ──(export_pending.php)──► bridge/inject.txt ──► app.exe --headless --load world
                                                          │ (spawns/nudges web characters,
                                                          │  runs ~4 sim-days, saves world)
MySQL ◄──(ingest_report.php)──◄ bridge/report.json ◄──────┘
```

Rejected alternative: re-implementing a mini-sim in PHP — it would duplicate the planner,
personality, tribe and society systems that already exist and that the product is *about*.

Each phase is self-contained. Execute in order.

---

## Phase 0 — Discovery output: what exists, exact locations (READ FIRST, every phase)

### Web base (`website/`) — PHP 8 + PDO + MySQL, no framework

| Piece | Where | Status |
|---|---|---|
| Config | `website/config.php` | done — env-driven; `SIM_DAYS_PER_HUMAN_DAY=4` (line 40), `SIM_TICK_INTERVAL_SECONDS=21600` (line 41) |
| DB layer | `website/db.php` | done — `Database::query/fetchOne/fetchAll/insert/raw` static helpers, PDO prepared statements |
| Auth | `website/auth.php` | done — `session_init` (32), `session_user` (43), `require_auth` (69), `register_user` (83), `login_user` (128), `logout_user` (185), password reset (209/246) |
| Characters | `website/auth.php` | done — `get_user_characters` (280), `create_character` (304), `get_latest_character_state` (377), `get_recent_actions` (391) |
| Registration + questionnaire | `website/register.php` | done — Big Five inputs, calls `create_character` at line 68 |
| Dashboard + feedback form | `website/dashboard.php` | done — feedback POST handler at lines 15-46 (`INSERT INTO user_feedback ... ON DUPLICATE KEY UPDATE`), renders state + recent actions |
| Schema | `website/sql/schema.sql` | written but **BROKEN — will not import** (see bug below); 17 tables: users, password_resets, characters, character_state_snapshots, character_actions, user_feedback, social_interactions, relationships, character_items, inventions, knowledge_diffusion, tribes, tribe_memberships, world_events, simulation_configs, character_genomes, user_session_logs |
| Static mockups | `index.html`, `chronicle.html`, `engine.html`, `experience.html` | design references ("Scientific Terminal" aesthetic) |
| Dead files | `website/login.html` (0 bytes), `website/db.sql` (0 bytes) | delete in Phase 1 |
| Existing plan | `website/PLAN.md` | concept doc — this plan supersedes its roadmap section |

**CRITICAL SCHEMA BUG**: inside CREATE TABLE bodies, section headers are written with
Unicode box-drawing dashes (`── Big Five Personality ──`, e.g. schema.sql:76, 85, 92, 99,
106, 112, 118, 142, …). These are NOT `--` SQL comments; MySQL will reject the file.
Every such line must become a real `-- ...` comment before anything else works.

### Simulation engine (C++, repo root) — relevant verified APIs

| Capability | Where | Notes |
|---|---|---|
| Headless run | `src/main.cpp:1958` `--headless <ticks>` | exits after N ticks, prints realism report |
| Resume world | `src/main.cpp:1963` `--load <file>` | loads save instead of spawning founders |
| Checkpoint | `--save-at <tick>`, `--save-file <file>` (main.cpp:1964-1965) | default `src/data/saves/headless_save.txt` |
| Determinism | `--seed <text>` (main.cpp:1959) | same seed = same history |
| Entity serialization | `Entity::saveTo/loadFrom` `src/Entity.cpp:365+` | line-based `KEY:value`, append-only with presence guards (goals pattern cpp:488-494) |
| Personality | `Entity::personality` Big Five 0-100 (`src/header/Entity.h:138-152`), `ValueSystem` (155-161) | web scale is 0.0-1.0 → multiply by 100 |
| Society state per entity | `tribeId` (Entity.h:497), `specialization` role (508), `integrity` (507), `socialClass` (505), `familyId` (335), `salary.token` wealth (362) | all present since society-layer work |
| Tribe/government | `Tribe` struct `src/header/CivilizationEngine.h:107+` — name, leaderId, government, councilIds, corruption, govSatisfaction | |
| Structured event log | `CivilizationEngine::logEvent(day, desc, cat, data)` with `kind=...` blocks | elections, scandals, successions, wars… |
| Per-entity action CSVs | `src/data/act_<entityId>.csv` | action history per entity |
| Sim clock | `src/core/SimClock.h` + `g_clock.isCivTick()` (main.cpp:1724) | ticks ≠ civ-days; **calibrate in Phase 2** (an 800-tick run reached ~day 500-800) |

### Anti-patterns (DO NOT)

- **Do not parse `src/data/complete_logs.txt` / `tick_history.jsonl` from PHP** — they are
  500-700 MB append files. The bridge report (Phase 3) is the only ingestion source.
- **Do not add a JSON library to the C++ build.** The bridge is asymmetric by design:
  C++ *reads* the same line-based `KEY:value` format it already uses for saves
  (inject.txt), and *writes* JSON by hand with a std::ofstream (report.json) — emitting
  JSON is trivial, parsing it is not. PHP does the opposite (`json_decode` the report,
  write plain lines for inject).
- **Do not invent new auth/DB helpers in PHP** — every page uses `require_auth()` +
  `Database::` helpers exactly as `dashboard.php` does.
- **Do not overwrite state rows** — snapshots/actions tables are append-only (schema
  philosophy, schema.sql header). Only `characters.current_day` and lifecycle flags update.
- **Do not let two engine runs overlap** — the scheduler must take a lock
  (`simulation_configs.is_running`) before launching app.exe.
- **Entity save-format changes**: only append new `KEY:` lines at the END of
  `Entity::saveTo` with presence-guarded reads (established rule from the society plan).

---

## Phase 1 — Make the existing web base actually run (schema fix + smoke test)

### What to implement
1. **Fix `website/sql/schema.sql`**: convert every in-table `── ... ──` Unicode header
   line into a proper `-- comment` (keep the text). Verify with a real import.
2. **Delete dead files** `website/login.html`, `website/db.sql` (both 0 bytes).
3. **Local dev environment**: document + script it in `website/README.md` —
   MySQL/MariaDB with database `ashb2` (config.php:24 defaults), then
   `php -S localhost:8080` from `website/` (APP_URL config.php:31 already expects 8080).
4. **Seeder for development**: `website/bin/seed_demo.php` (CLI-only guard:
   `if (php_sapi_name() !== 'cli') exit;`) that creates a demo user, a character, and
   ~8 fake days of `character_state_snapshots` + `character_actions` rows so the
   dashboard renders fully without the engine. Use `Database::insert` helpers.
5. **Walk the flow and fix what breaks**: register → auto-created character
   (register.php:68) → dashboard shows state/actions from seeder → submit feedback →
   row lands in `user_feedback`. Fix any column-name drift between PHP queries and the
   schema (likely, since the schema never imported anywhere).

### Documentation references
- Query idioms to copy: `website/dashboard.php:15-46` (feedback insert), `auth.php:280-410`.
- Schema columns: `website/sql/schema.sql` tables 3, 4, 5, 6.

### Verification checklist
- `mysql ashb2 < website/sql/schema.sql` completes with zero errors; `SHOW TABLES;` lists 17.
- Full manual flow works end-to-end on localhost:8080 (register, login, dashboard, feedback).
- `SELECT COUNT(*) FROM user_feedback;` increments after submitting the form.

### Anti-pattern guards
- No framework, no composer packages. No schema redesign — comment fix + minimal column
  corrections only.

---

## Phase 2 — C++ web bridge, part 1: identity tag + character injection

### What to implement
1. **Web identity on Entity**: add `int webCharId = -1;` to `Entity`
   (`src/header/Entity.h`, next to the society block ~501-511) — the MySQL
   `characters.id` this entity embodies, -1 for pure AI entities. Serialize it:
   append `WEBCHARID:` at the END of `Entity::saveTo` and presence-guarded read in
   `loadFrom` (copy the Phase-1 society keys added at Entity.cpp:435-443).
2. **`--inject <file>` CLI flag**: add to `CliOptions` + `parseCli`
   (`src/main.cpp:1938-1999`, copy the `--load` pattern at 1963). After world
   load/creation and before the tick loop, parse the file — same line-based format as
   saves, one block per character:
   ```
   CHAR:42                      ← characters.id
   NAME:Naelle
   OPENNESS:62                  ← web 0-1 scale × 100
   CONSCIENTIOUSNESS:55
   EXTRAVERSION:71
   AGREEABLENESS:48
   NEUROTICISM:33
   NUDGE:0                     ← 0 = new spawn, 1 = personality correction for existing
   END
   ```
   For `NUDGE:0`: spawn a new adult entity near a mid-sized tribe's center (copy however
   founders/colonists are constructed — the colonization spawn at
   `CivilizationEngine.cpp:2199+` and the founder spawn in main.cpp are the references),
   set personality from the block, `webCharId = CHAR value`, log
   `kind=web_spawn charId=... entityId=...`.
   For `NUDGE:1`: find the living entity with that `webCharId` and add the personality
   deltas (clamped ±10 per cycle) — this is how user feedback reaches the sim.
3. **Calibrate ticks per civ-day**: instrument or empirically measure how many headless
   ticks advance one civ-day (log `day` at tick 0 and tick N; see `g_clock.isCivTick()`
   usage at main.cpp:1724 and `SimClock.h`). Record the number in the bridge README —
   Phase 4's scheduler must pass `--headless <ticksPerCivDay × 1>` per 6-hour run.

### Documentation references
- CLI pattern: main.cpp:1951-1999. Save-key pattern: Entity.cpp:365-443 + 488-494.
- Personality struct: Entity.h:138-152. Spawn references: CivilizationEngine.cpp:2199+
  (colonists), main.cpp founder creation (search `--entities` handling).

### Verification checklist
- Build clean. Write a 2-character inject.txt by hand; run
  `app.exe --headless 50 --inject bridge/inject.txt --save-at 50 --save-file bridge/world.txt`;
  grep the civ log for two `kind=web_spawn` lines.
- Reload: `app.exe --headless 10 --load bridge/world.txt` — grep the save file for
  `WEBCHARID:42` (tag survives the save/load round trip).
- Old saves without `WEBCHARID:` still load.

### Anti-pattern guards
- No JSON parsing in C++. No new library. Injection must be a no-op when the file is
  absent or empty (the flag is optional).

---

## Phase 3 — C++ web bridge, part 2: the daily report

### What to implement
1. **`--report <file>` CLI flag** (same parseCli pattern). At the END of the headless run
   (where the realism report prints, search `HEADLESS: done` in main.cpp), if the flag is
   set, emit hand-written JSON to the file for **every entity with `webCharId >= 0`**
   (alive or newly dead):
   ```json
   {"day": 812, "characters": [
     {"webCharId": 42, "entityId": 20011, "alive": true,
      "day": 812, "health": 84.2, "hunger": 31.0, "stress": 40.1, "happiness": 66.3,
      "posX": 712.4, "posY": 530.9,
      "tribeId": 3, "tribeName": "Kind Gleeneend", "government": "Democracy",
      "role": "healer", "isSpecialist": true, "socialClass": "PLEBEIAN",
      "wealth": 118.5, "integrity": 61.0, "auctoritas": 24.0,
      "bonds": 4, "enemies": 1,
      "family": "House Tarn", "childrenCount": 1,
      "events": ["Joined the Kind Gleeneend", "Elected to the council"],
      "actions": [{"day": 810, "name": "Forage", "detail": "..."}]
     }],
    "world": [{"day": 811, "category": "tribe", "description": "SCANDAL in the ..."}]}
   ```
   Sources, all already in memory at end of run: entity fields (Entity.h:256-536), tribe
   via `globalCivEngine->findTribe(e.tribeId)`, role/class/wealth per the Phase-0 table,
   bonds from `list_entityPointedSocial`, family via `globalKinship->findFamily`,
   per-character events by filtering the engine `eventLog` deque (CivilizationEngine.h)
   for the entity's name/id, recent actions from the entity's action memory (or the last
   ~20 lines of its `src/data/act_<entityId>.csv`).
   Escape strings minimally (quotes, backslashes, control chars) — write one tiny
   `jsonEscape` helper.
2. **Death reporting**: a tagged entity with `entityHealth <= 0` appears once with
   `"alive": false` and a `"deathCause"` if the death-attribution field exists (search
   `deathCause`/death log emission in Entity.cpp — structured death logs were added
   Jul 4). Phase 4's ingest flips `characters.is_alive`.

### Documentation references
- End-of-run hook: main.cpp `HEADLESS: done` block. Event deque: CivilizationEngine.h
  `eventLog`. Kinship: `src/header/Kinship.h:15-57`.

### Verification checklist
- Run inject (Phase 2) + `--report bridge/report.json` for ~1 civ-day of ticks; open the
  file: valid JSON (`php -r "var_dump(json_decode(file_get_contents('bridge/report.json')));"`
  returns an object, not NULL), both characters present with tribe/role fields.
- Kill a tagged character via a short brutal run (`--scenario crucible`) or manual edit;
  report shows `"alive": false`.

### Anti-pattern guards
- Do not write the report incrementally during the run (one shot at the end).
- Do not include non-web entities (report stays small; the DB is not an archive of the
  whole world — the save file is).

---

## Phase 4 — PHP scheduler: the 6-hour heartbeat

### What to implement — all CLI-only scripts in `website/bin/`
1. **`export_pending.php`**: SELECT characters where `released_at IS NULL AND is_active=1`
   → write `NUDGE:0` blocks; SELECT yesterday's `user_feedback` rows with
   `personality_corrections IS NOT NULL` for living characters → `NUDGE:1` blocks with
   deltas (×100, clamped ±0.1 web-scale). Output `bridge/inject.txt`. Mark exported
   characters' `released_at = NOW()` only AFTER Phase-4 step 3 confirms the run succeeded
   (write ids to `bridge/pending_release.txt` for step 3 to confirm).
2. **`ingest_report.php <report.json>`**: `json_decode`, then per character:
   - INSERT a `character_state_snapshots` row (map report fields → columns; scale
     0-100 floats to the DECIMAL columns as-is; mood_happiness = happiness/100).
   - INSERT `character_actions` rows for new actions (dedupe on character_id +
     simulation_day + action_type using `INSERT IGNORE` or a lookup).
   - Upsert `tribes` row by sim tribeId kept in a new small mapping table
     `sim_tribe_map (sim_tribe_id, tribe_id)` or simply store the sim id in
     `tribes.id`-adjacent column — pick ONE and document it; update `tribe_memberships`
     when the character's tribe changed (close old row with `left_day`, open new).
   - UPDATE `characters` SET `current_day`, `is_alive`, `died_at`/`death_cause` when dead.
   - INSERT `world_events` rows from the `world` array (dedupe on day+description hash).
3. **`run_tick.php`** — the orchestrator, run every 6 hours by Windows Task Scheduler
   (document the `schtasks` one-liner in the README; cron line for Linux deploys):
   - Take the lock: `UPDATE simulation_configs SET is_running=1 WHERE id=1 AND is_running=0`
     — zero affected rows = previous run still going, exit.
   - Call export_pending; run
     `app.exe --headless <ticksPerCivDay> --load bridge/world.txt --inject bridge/inject.txt --save-at <ticksPerCivDay> --save-file bridge/world.txt --report bridge/report.json`
     (first ever run: `--seed` + `--entities` instead of `--load`, seed from
     `simulation_configs.seed`); check exit code.
   - On success: ingest_report, confirm `released_at` for pending ids, increment
     `simulation_configs.tick_count`, release lock. On failure: release lock, DO NOT
     mark released, log to `bridge/scheduler.log`.

### Documentation references
- DB helpers: `website/db.php:58-89`. Feedback columns: schema.sql table 6.
  Lock table: schema.sql table 15 (`simulation_configs.is_running`, seeded row id 1).

### Verification checklist
- Manual end-to-end: register a fresh user on localhost → run `php bin/run_tick.php`
  4 times → dashboard shows 4 new sim-days of snapshots/actions, character has a tribe
  and role, `characters.current_day` advanced by 4, world events visible.
- Run `run_tick.php` twice concurrently (two terminals): second exits immediately on lock.
- Kill app.exe mid-run: lock is released by the failure path, `released_at` unmarked,
  next run retries the inject.

### Anti-pattern guards
- Scripts refuse non-CLI execution. No PHP `exec` of anything except the one app.exe
  command line (built with escapeshellarg). The engine's stdout goes to
  `bridge/scheduler.log`, not the browser.

---

## Phase 5 — Dashboard: the daily chronicle experience

### What to implement (dashboard.php + one new page; copy the mockups' aesthetic)
1. **Day header**: "Human day N — simulation day M of your character's life"
   (N = days since `released_at`, M = `characters.current_day`), plus alive/dead state
   with cause. A "next world tick in ~Hh" hint from `SIM_TICK_INTERVAL_SECONDS`.
2. **Today's chronicle**: the last 4 sim-days of `character_actions` +
   per-character events, rendered as a narrative timeline (the `chronicle.html` mockup
   is the design reference — reuse its CSS classes from `style.css`).
3. **Vitals panel**: latest snapshot (health/hunger/stress/happiness bars), role, tribe
   + government, wealth, bonds/enemies counts; small sparkline of health/happiness over
   the last ~28 snapshots (inline SVG, no chart library).
4. **Society context**: tribe block — name, government, leader, your character's role and
   class; recent `world_events` involving the character's tribe.
5. **Feedback loop surfacing**: the existing form (dashboard.php:212+) gains the
   structured `personality_corrections` inputs (five ±0.1 sliders); after submission
   show "will influence your character at the next world tick"; when a NUDGE was applied
   (ingest can mark feedback rows `applied_at` — add the column in Phase 1's schema fix),
   show "✓ applied on day M".
6. **Character death**: dead characters get a memorial view (life summary: days lived,
   tribe history, children, notable events) + a "create a new character" path (reuse the
   register.php questionnaire extracted into a shared include).

### Documentation references
- Existing render + query patterns: dashboard.php whole file. Aesthetic: chronicle.html,
  experience.html. Data: tables 4, 5, 6, 12, 13, 14.

### Verification checklist
- With 4+ ingested cycles: dashboard shows chronicle, vitals with sparkline, tribe block.
- Submit feedback with a -0.1 neuroticism correction → next `run_tick.php` → inject.txt
  contains the NUDGE block → after the run, feedback row has `applied_at`, and the
  character's stored personality in the save reflects the nudge (spot-check via report).
- A dead character renders the memorial, not a broken dashboard.

### Anti-pattern guards
- No JS framework, no chart library — inline SVG and vanilla JS only, matching the
  existing static pages. No queries outside `Database::` helpers.

---

## Phase 6 — End-to-end verification & hardening

1. **The 48-hour dry run**: schedule the task at a compressed interval (every 10 min) with
   3 test users; confirm 8+ cycles: no lock deadlocks, no duplicate snapshots
   (`SELECT character_id, simulation_day, COUNT(*) ... HAVING COUNT(*)>1` is empty),
   `current_day` strictly increasing, feedback nudges land.
2. **Anti-pattern greps**: no `json` includes in C++ diff; no PHP reads of
   `src/data/complete_logs.txt|tick_history.jsonl`; all new PHP entry points call
   `require_auth()` or the CLI guard; inject/report paths never web-accessible (bridge/
   lives OUTSIDE the web root or is .htaccess-denied — verify with a curl 403/404).
3. **Failure drills**: corrupt report.json → ingest logs and skips, lock released;
   missing world save → run_tick falls back to genesis seed run and logs loudly.
4. **Docs**: `website/README.md` — setup, the schtasks/cron line, bridge file formats,
   ticks-per-civ-day calibration value, and "how a feedback nudge flows" diagram.

---

## Explicitly deferred (the "maybe more features later" bucket)
World map page (planet render), spectating other characters, notifications/email, live
SSE updates, multiple characters per user simultaneously, the ML export pipeline
(PLAN.md §V), admin panel. The schema already anticipates most of these — nothing in the
MVP blocks them.
