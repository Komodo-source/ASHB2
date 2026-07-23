# ASHB2 — Human-in-the-Loop Feedback & AI Upgrade Loop

**Author**: Buffy (architecture proposal)
**Date**: 2026-07-10
**Status**: Proposed (awaiting sign-off on 3 open questions before implementation begins)
**Supersedes**: nothing — this is additive to `plans/society-layer.md` and `plans/webapp-human-sim.md` (Phases 1-6)
**Companion plans**:
  - `plans/webapp-human-sim.md` — Phases 0-6 of the web app (does **not** cover the AI upgrade loop, only the bridge & dashboard)
  - `plans/society-layer.md` — Phases 0-6 of government/roles/corruption in the C++ engine

---

## Table of contents

1. [Goal](#1-goal)
2. [Status: what already exists](#2-status-what-already-exists)
3. [Top-level architecture](#3-top-level-architecture)
4. [The four layers in detail](#4-the-four-layers-in-detail)
   - [4.1 User-input layer (personality → character)](#41-user-input-layer-personality--character)
   - [4.2 Simulation-run layer (bridge files)](#42-simulation-run-layer-bridge-files)
   - [4.3 Rating layer (coarse day + granular action)](#43-rating-layer-coarse-day--granular-action)
   - [4.4 AI upgrade loop (offline Python)](#44-ai-upgrade-loop-offline-python)
5. [Schema additions](#5-schema-additions)
6. [The `priors_vN.txt` file format](#6-the-priors_vntxt-file-format)
7. [A/B tasting without breaking determinism](#7-ab-tasting-without-breaking-determinism)
8. [Anti-patterns (what we explicitly skip)](#8-anti-patterns-what-we-explicitly-skip)
9. [Decisions already locked in](#9-decisions-already-locked-in)
10. [Tightened data flow](#10-tightened-data-flow)
11. [MVP build sequence (4 weeks)](#11-mvp-build-sequence-4-weeks)
12. [Open questions (must precede any code)](#12-open-questions-must-precede-any-code)
13. [Reference: existing files & locations](#13-reference-existing-files--locations)

---

## 1. Goal

> A user registers, fills a Big-Five psychometric questionnaire, and releases their digital twin into the ASHB2 world. Each day the user reads what their character did and rates **how aligned with their own personality/temperament the simulation felt**. The rating — plus granular corrections when the score is low — become a training signal that **retunes the simulation's planning weights**. A subsequent cohort of twins runs against a new "AI version" and a head-to-head alignment comparison tells us whether the upgrade actually helped.

**Hard constraints inherited from existing plans**

- Engine **must** stay deterministic (`--seed` reproducibility from `plans/webapp-human-sim.md`).
- Engine **must not** parse JSON (`bridge/README.md`: "asymmetric bridge — C++ writes JSON, reads KEY:value").
- All `KEY:value` line formats are append-only with presence-guarded reads (`Entity::saveTo/loadFrom` pattern).
- Web app **does not simulate** anything (PHP only translates DB ↔ bridge files).
- DB tables for state are **append-only** (snapshots, actions, events are never OVERWRITTEN).

---

## 2. Status: what already exists

(Listed by component, file locations, status. ~80% of the scaffolding exists; the upgrade layer is the missing piece.)

| Component | Where | Status |
|---|---|---|
| Web auth, register, dashboard, password reset | `website/auth.php`, `register.php`, `dashboard.php`, `login.php` | ✅ |
| 17-table schema (users, characters, snapshots, actions, feedback, events, tribes, …) | `website/sql/schema.sql` | ✅ (Phase 1 fix needed: in-table Unicode box-drawing lines, ~17 occurrences) |
| Big-Five + attachment + drives on `characters` | `schema.sql:96-129`, `register.php:68` → `create_character` | ✅ |
| Daily `user_feedback` with `alignment_score 1-5`, `would_change_action`, `personality_corrections JSON` | `schema.sql:271-310`, `dashboard.php:23-46` | ✅ |
| Per-action rows with `utility_score` **and** `alternative_actions_json` | `schema.sql:213-238` | ✅ (critical — this is the preference-pair substrate) |
| `Entity.webCharId` ↔ `MySQL.characters.id` round-tripped via save/load | `Entity.h:512`, `Entity.cpp:445-614` | ✅ |
| `--inject <file>` CLI parses `CHAR:BLOCK:END` from `bridge/inject.txt`, supports spawn (NUDGE:0) and trait nudge (NUDGE:1, ±10/cycle, clamped) | `src/main.cpp:1953-2172`, `bridge/README.md` | ✅ |
| `--report <file>` CLI emits hand-written JSON for every tagged entity at end of headless run | `bridge/README.md` (planned) | 🟡 stale document — implementation status re-verify |
| 6-hour scheduler (`website/bin/run_tick.php`) — tries to claim `simulation_configs.is_running`, runs engine, ingests report | `plans/webapp-human-sim.md` Phase 4 | 🟡 not implemented |
| Personality derived entity fields at spawn (Personality, ValueSystem, Jungian, Drive, VSM) | `Entity::initPsychology` | ✅ |
| Structured civilization event log (elections, scandals, successions, wars, web_spawn, web_nudge) | `CivilizationEngine.logEvent`, `kind=...` blocks | ✅ |
| C++ observability hooks (EventStreamManager, StateObserver, AnalyticsEngine) | `src/observability/Observability.h` | ✅ (present but unused by bridge) |

**Missing for the upgrade loop** (the gap this plan fills):

- ❌ Action-level preference capture (we have `alternative_actions_json` per row, but no UI/schema/flow to capture the user's "I would have chosen ___").
- ❌ Universal planner weight + drive setpoint retuner (a complete toolchain layer).
- ❌ Bridge-side priors file (C++ can read injection but has no `--priors <file>` flag).
- ❌ A/B cohort assignment at character creation.
- ❌ Trailing-median comparison tile on the dashboard.

---

## 3. Top-level architecture

```
                   ┌──────────────────────────────────────────┐
                   │  Web app  (PHP + MySQL, website/)         │
                   │  • auth.php, register.php, dashboard.php │
                   │  • schema.sql (17 tables)                │
                   └────────────────┬─────────────────────────┘
                                    │  export_pending.php
                                    ▼
                        bridge/inject.txt
                                    │  --inject  (CLI flag)
                                    ▼
   ┌──────────────────────────────────────────────────────────────┐
   │  app.exe  --headless N --load bridge/world.txt                │
   │            --inject bridge/inject.txt                        │
   │            --priors  bridge/priors_vN.txt     (NEW)          │
   │            --save-at N --save-file bridge/world.txt          │
   │            --report bridge/report.json                       │
   │  • Entity.webCharId embedded, NUDGE:0 spawns, NUDGE:1 nudges │
   │  • PlanningSystem uses priors_vN to modulate action utilities│
   │  • DriveSystem uses priors_vN to nudge setpoints             │
   └──────────────────────────────────────────────────────────────┘
                                    │  end-of-run, --report flag
                                    ▼
                        bridge/report.json
                                    │  ingest_report.php
                                    ▼
                   ┌──────────────────────────────────────────┐
                   │  MySQL  character_state_snapshots,        │
                   │          character_actions, user_feedback,│
                   │          action_preferences  (NEW),       │
                   │          ai_models           (NEW),       │
                   │          retune_runs         (NEW)        │
                   └──────────────────────────────────────────┘
                                    │
                            Dashboard renders
                                    │
                            User rates (NEW: course-correction panel)
                                    │
                            preference pairs fed to retuner
                                    │
                                    ▼
                       ┌──────────────────────────────┐
                       │  retune_weights.py (weekly)  │
                       │  L2-regressed per action-type│
                       │  + per Big Five trait deltas │
                       └────────────────┬─────────────┘
                                    │
                                    ▼
                          bridge/priors_v(N+1).txt  ─► next cohort
```

---

## 4. The four layers in detail

### 4.1 User-input layer (personality → character)

The user fills in a 5-10 minute questionnaire on `register.php`:

- Big Five (OCEAN, web scale 0.0–1.0) — primary axis.
- Attachment style (secure/anxious/avoidant/disorganized/fearful) — from cluster or override.
- Five drives (exploration, social, safety, dominance, achievement) — sliders.
- Memory parameters (decay rate, trauma retention, capacity).
- Optional initial knowledge (recipes/tools JSON).

Stored in `characters` (`schema.sql:96-129`). On the first scheduler tick after registration:

- `export_pending.php` writes a `NUDGE:0` block to `bridge/inject.txt`.
- Engine spawns a new adult entity near the median-sized tribe's center (per `bridge/README.md`).
- Tribe membership is **not forced** — absorption logic recruits naturally.

The personality, drives, and the entity's `webCharId = characters.id` survive the save/load round trip via the `WEBCHARID:`, `PERSONALITY:`, `DRIVES:`, `VALSYS:` keys appended at the END of `Entity::saveTo` (presence-guarded — see `Entity.cpp:445-614` for the established rule).

### 4.2 Simulation-run layer (bridge files)

Every ~6 hours (configurable in `website/config.php:41` as `SIM_TICK_INTERVAL_SECONDS`), `run_tick.php`:

1. Tries to claim `simulation_configs.is_running=1` on row 1 — zero affected rows = refuse (lock honored).
2. Calls `export_pending.php` — emits `bridge/inject.txt` (NUDGE:0 for new chars, NUDGE:1 from persona_corrections submitted yesterday).
3. Runs (Windows-side, with `schtasks`; Linux-side, cron):
   ```
   app.exe --headless T --load bridge/world.txt \
           --inject bridge/inject.txt \
           --priors  bridge/priors_vN.txt \
           --seed 12345 --entities 60 \
           --save-at T --save-file bridge/world.txt \
           --report bridge/report.json
   ```
   T = ticks per civ-day ≈ 1 (empirically, per `bridge/README.md` "TICKS_PER_CIV_DAY = 1" measurement on 2026-07-10).
4. Calls `ingest_report.php` — parses the per-entity JSON report → fills `character_state_snapshots`, `character_actions`, `world_events`, etc.
5. Releases the lock (`is_running=0`).
6. Marks new characters' `released_at = NOW()` so they don't double-spawn on the next tick.

`bridge/report.json` shape (per `plans/webapp-human-sim.md` Phase 3):

```json
{
  "day": 812,
  "characters": [
    { "webCharId": 42, "entityId": 20011, "alive": true,
      "day": 812, "health": 84.2, "hunger": 31.0, "stress": 40.1, "happiness": 66.3,
      "posX": 712.4, "posY": 530.9,
      "tribeId": 3, "tribeName": "Kind Gleeneend", "government": "Democracy",
      "role": "healer", "isSpecialist": true, "socialClass": "PLEBEIAN",
      "wealth": 118.5, "integrity": 61.0, "auctoritas": 24.0,
      "bonds": 4, "enemies": 1,
      "family": "House Tarn", "childrenCount": 1,
      "events": ["Joined the Kind Gleeneend", "Elected to the council"],
      "actions": [{"day": 810, "name": "Forage", "detail": "...", "utility": 6.2, "alternatives": [{"name": "Socialize", "utility": 4.9}]}]
    }],
  "world": [{"day": 811, "category": "tribe", "description": "SCANDAL in the ..."}]
}
```

The `actions[].alternatives` field is the substrate for the action-preference signal in 4.3.

### 4.3 Rating layer (coarse day + granular action)

Two capture moments per dashboard visit:

| Capture | Field | Mandatory? | When captured |
|---|---|---|---|
| **Coarse** | `user_feedback.alignment_score 1-5` | Yes | Every dashboard visit |
| **Coarse** | `user_feedback.would_change_action` (free text) | No | Same form |
| **Coarse** | `user_feedback.personality_corrections JSON` | No | Same form |
| **Granular** | NEW `action_preferences` row, one per (chosen, preferred) pair | No | Only when alignment_score ≤ 3 |

UI flow in `dashboard.php` (modification plan):

1. Show the day's snapshot, top 4 actions, recent events (unchanged).
2. Show the `alignment_score` 1-5 radio (unchanged, mandatory).
3. Show free-text fields (unchanged).
4. **NEW**: if `alignment_score <= 3`, expand a "Course Correction" block:
   - Pull the top 2 actions of the day by `utility_score` from `character_actions`.
   - Render each as: `Action taken: [Forage for Food] (utility 6.20). Alternatives: [Rest 4.10, Socialize 3.50]`.
   - For each: `I'd have chosen: [dropdown from alternatives]`.
   - Optional "Why?" text box.
   - Submit appends one `action_preferences` row per pair.

The submitted CSV-equivalent per pairing:

```
(action_id=812023, snapshot_id=42001, chosen=Forage(6.20), preferred=Socialize(3.50),
 traits_at_snapshot={O:0.62, C:0.55, E:0.71, A:0.48, N:0.33}, confidence=4, free_text=...)
```

The `traits_at_snapshot` are joined from `character_state_snapshots` (the entity's `O/C/E/A/N` at decision time). These are the only psychology fields we need for retuning — the user specifies Big Five at registration, the engine mirrors it into `Entity::personality`, snapshots can recover it.

### 4.4 AI upgrade loop (offline Python)

Once per week (cron / Task Scheduler), a thin Python script:

```
retune_weights.py  (numpy + scipy, < 200 LOC)
   │
   │  input:  pre-joined CSV from MySQL —
   │          (snapshot_id, big_five_5tuple,
   │           chosen_action_type, preferred_action_type,
   │           chosen_utility, preferred_utility,
   │           feedback_confidence, ai_model_id)
   │  filter: only rows where confidence >= 3
   │          AND ai_model_id = the current "control"
   │
   │  model:  log_odds(preferred) - log_odds(chosen)
   │          regressed separately per action_type column
   │          traits enter as additive modifiers (L2 regularised)
   │
   │  output: bridge/priors_v(N+1).txt
   │          plus a metrics JSON in retune_runs table
```

Concretely, for each `(chosen, preferred)` pair, the algorithm increments a count vector:

```
delta[O][action_type] += sign(util(preferred) - util(chosen)) * confidence
delta[E][action_type] += …
…
```

After all rows are processed, deltas are L2-normalised to keep changes small (≤ ±5 per dimension per version — see §9.2). The output `priors_vN.txt` is line-based KEY:value, same idiom as `inject.txt`.

The engine loads this file once at boot, BEFORE planning starts, into an in-memory `std::map<std::string, float> g_priorsAbility` and `std::map<std::tuple<DriveAxis,string>, float> g_priorsDrive`. Both are referenced in `PlanningSystem::scoreAction` and `Drive::recomputeSetpoint`.

---

## 5. Schema additions

| Table | Purpose | Act | Key columns | Index / aggregation |
|---|---|---|---|---|
| `ai_models` | Track each retuned `priors_v*.txt` | NEW | `id`, `version_tag` (e.g. `v2_2026-07-10`), `deployed_at`, `retired_at`, `active` (boolean — one row is `1` at any time), `notes`, `git_commit` (FK to source version) | `idx(active)`, `idx(deployed_at)` |
| `action_preferences` | Bradley-Terry preference-pair signal $U(\text{pref}) > U(\text{chosen})$ | NEW | `id`, `user_feedback_id`, `character_id`, `snapshot_id`, `chosen_action_id`, `preferred_action_type` (varchar), `chosen_utility DECIMAL(6,4)`, `preferred_utility DECIMAL(6,4)`, `confidence TINYINT`, `free_text TEXT`, `ai_model_id` (FK), `created_at` | `idx(char_day)`, `idx(snapshot)`, `idx(ai_model_id, created_at)`, `UNIQUE (user_feedback_id, chosen_action_id)` |
| `retune_runs` | Auditability of every weight update | NEW | `id`, `ai_model_id` (FK, the model that was produced), `started_at`, `finished_at`, `samples_used`, `l2_penalty`, `median_alignment_before`, `cohort_size`, `metrics_json`, `script_path` | `idx(started_at)` |
| `cohort_stats` | Daily aggregate metrics for A/B comparison | NEW | `id`, `window_start`, `window_end`, `n_users`, `median_alignment`, `p25_alignment`, `p75_alignment`, `n_preference_pairs`, `ai_model_id` | `idx(ai_model_id, window_end)` |
| `characters` | Assign character to A/B cohort | ALTER | ADD `ai_model_id INT UNSIGNED NOT NULL DEFAULT 1` | `idx(ai_model_id)` |
| `character_actions` | Tag action with model id at decision time | ALTER | ADD `ai_model_id INT UNSIGNED NOT NULL DEFAULT 1` (denormalised — set at insert time by ingest_report.php) | `idx(action_type, ai_model_id)` |
| `user_feedback` | Track granular-fallback trigger | ALTER | no column change needed — `alignment_score` already there; granular block is gated by the value at render time | — |

All `ALTER`s are backward-compatible (default `1` for existing rows). The migration is safe to apply while the system is running.

---

## 6. The `priors_vN.txt` file format

Line-based KEY:value, same idiom as `inject.txt` and entity saves. Append-only-friendly — unknown lines are ignored, missing lines fall back to baseline 0.0.

```
# priors_v2.txt  (generated 2026-07-10, samples=122, l2=0.05)
# Format: PRIOR:<CATEGORY>:<KEY>:<SIGN>:<FLOAT>
# Categories: UTIL  (PlanningSystem action-utility deltas)
#             DRIVE (Drive::recomputeSetpoint deltas)
#             BIAS  (per-Big-Five trait multiplier on action_type)

VERSION:2
GENERATED:2026-07-10T19:00Z
SAMPLES_USED:122
L2_PENALTY:0.05
COHORT_AI_MODEL_ID:2

# ─ action utility deltas, base-utility units (0-10 scale) ───────────────
UTIL:forage:O:+0.42
UTIL:forage:C:+0.18
UTIL:forage:E:-0.33
UTIL:forage:A:-0.05
UTIL:forage:N:-0.61

UTIL:socialize:E:+0.77
UTIL:socialize:A:+0.31
UTIL:socialize:N:-0.45
UTIL:socialize:O:+0.10

UTIL:rest:N:+0.55
UTIL:rest:C:-0.12

UTIL:craft:C:+0.40
UTIL:craft:O:+0.29

# ─ drive setpoint deltas (0-100 bipolar homeostatic scale) ──────────────
DRIVE:safety:N:+2.20
DRIVE:exploration:O:+1.80
DRIVE:social:E:+3.10
DRIVE:achievement:C:+2.50
DRIVE:dominance:E:+1.60

# ─ Big-Five trait × action multiplicative bias on utility ───────────────
BIAS:high_O:exploration:1.18
BIAS:high_N:safety:1.22
BIAS:low_C:forage:0.84
```

**Parser in C++** at world load (mirrors the `applyWebInjectFile` idiom from `src/main.cpp:1953`):

- Unknown keys ignored with a stderr note.
- Malformed line → skip + log.
- Missing `VERSION:` → refuse to load (forces explicit deploy).
- Reject if max `|value|` exceeds safety clamp (default 5.0 for utility, 5.0 for drive, 2.0 for bias — see §9.2).

---

## 7. A/B tasting without breaking determinism

Same world seed. Same tribes. Same initial events. Only the per-character planner/drive weighting differs.

```
                  The World (one shared seed)
   ┌─────────────────────────────┬─────────────────────────────┐
   │  characters.ai_model_id = 1 │  characters.ai_model_id = 2 │
   │  (control: priors v1 / base)│  (treatment: priors v2)     │
   │  ~50% of new spawns         │  ~50% of new spawns         │
   └──────────────┬──────────────┴──────────────┬──────────────┘
                  │                             │
                  └──────────────┬──────────────┘
                                 ▼
            Backfill `sim_tribe_map` keeps tribe ids stable
            so `social_interactions` and `world_events`
            span both cohorts cleanly.
                                 │
                  every 24 h, `bin/aggregate_cohorts.php` writes
                  cohort_stats rows per ai_model_id
                                 │
                  dashboard shows 7-day trailing-median tile:
                  ──────────────────────────────────────────────
                  Cohort A (v1, N=23 chars)  ★★★☆☆  3.20 ▒▒▒
                  Cohort B (v2, N=21 chars)  ★★★½☆  3.55 ▒▒▒▒
                  ──────────────────────────────────────────────
```

Promotion gate (§12): ship v3 ONLY if both cohorts have N ≥ 5 chars AND `B.median ≥ A.median + 0.3` over the trailing 7 days AND median time-on-feedback has not spiked (engagement signal).

---

## 8. Anti-patterns (what we explicitly skip)

| Anti-pattern | Why skipped | What we do instead |
|---|---|---|
| Live gradient descent in C++ | Destroys determinism (the `--seed` reproducibility is the project's bedrock) and is engineering-heavy | Weekly offline Python retune |
| JSON parser in C++ | Forbidden by `plans/webapp-human-sim.md` Phase 0 ("asymmetric bridge — C++ writes JSON, reads KEY:value"); would also drag a dep into the engine | `KEY:value` line format with presence guards |
| LLM-as-judge evaluating decisions | Compounds biases (position, verbosity, self-enhancement — MT-Bench / Chatbot Arena findings); adds an opaque layer | Human pairwise comparison at low-UI cost |
| Tune `Drive.setpoint` directly from feedback | Users are unreliable judges of long-term homeostasis; bad drives compound slowly and are hard to A/B-spot | Tune `Drive.setpoint` ONLY via clues from `user_feedback.personality_corrections`, never from preference pairs |
| Multiple parallel worlds for A/B | Butterfly-effect divergence rules out clean comparison | One world, per-character `ai_model_id` |
| Per-user / per-profile weight retune | Cold-start problem for new users; needs ~10× more data | One universal baseline + Big-Five trait modifiers (still global, not per-user) |
| Mid-run switching of `ai_model_id` per character | Perma-contaminates the snapshot/action data with mismatched weights | `ai_model_id` set at character creation, immutable for the character's life — A/B compares like-for-like |
| Auto-pushing a retuned model without a promotion gate | Headless regression failures would be invisible | Promotion gate (§7, §12) |

---

## 9. Decisions already locked in

Locked in during proposal discussion (2026-07-10):

### 9.1 Granularity — day + per-action when low

- Coarse `user_feedback.alignment_score` keeps its mandatory 1-click position.
- The granular course-correction panel only expands when `alignment_score ≤ 3`.
- The 20-30% of days where users rate low will produce the high-signal pairs.
- Net per-user annotation burden stays in the ≤ 30 s/day range.

### 9.2 Tuning power — moderate (planner utility + drive setpoints)

- The retuner can learn deltas for **both** `Planning::scoreAction` modifiers and `Drive::recomputeSetpoint` deltas.
- **Safety clamps** on every output value:
  - `UTIL:*` Δ ≤ ±5 in base-utility units.
  - `DRIVE:*` Δ ≤ ±5 in setpoint units (0-100 bipolar).
  - `BIAS:*` multiplier in [0.5, 2.0].
- Big drives changes are revisited over multiple weekly versions before promotion.

### 9.3 Scope — universal baseline

- One weight set, applied to all characters regardless of profile.
- Big Five are **not** the input — they enter as additive trait modifiers on top of universal weights.
- Cold-start is fast: new users' characters start on the universal baseline.
- Promotion path: every globally-helpful correction improves everyone's twin.

---

## 10. Tightened data flow

```
DAY feedback  (every visit)        ACTION feedback  (only when day ≤ 3)
─────────────────────────────      ─────────────────────────────────────
alignment_score 1-5  ──┐           snapshot_id
would_change_action   │             chosen_action_type   (from character_actions)
narrative_feedback    │             preferred_action_type (from dropdown)
                       │             chosen_utility       (already in row)
personality_corrections│            preferred_utility     (scored from JSON alts)
                       │             confidence (1-5)
                       ▼             free_text
                  user_feedback                         │
                       │                                ▼
                       └────────┬───────────────────────┘
                                ▼
                       action_preferences
                                │  (when ingest_report.php lands,
                                │   join with character_state_snapshots
                                │   for Big Five at decision time)
                                ▼

           retune_weights.py (weekly, offline)
                ──────────────────────────
                FEATURE    TARGET         LAMBDA     CLAMP
                ──────────────────────────
                O          util_forage    0.05       ±5.0
                E          util_social    0.05       ±5.0
                N          setpoint_safe  0.05       ±5.0
                A          util_bond      0.05       ±5.0
                C          setpoint_ach   0.05       ±5.0
                ──────────────────────────
                                │
                                ▼
                    bridge/priors_vN.txt   (+ safety-clamp final pass)
                                │
                                ▼
              C++ --priors loader (reads at world startup)
              PlanningSystem.scoreAction:    util += UTIL:<action>:<trait>[:trait...]
              DriveSystem.recomputeSetpoint: set += DRIVE:<axis>:<trait>
                                │
                                ▼
              Next cohort of characters uses vN
                                │
                                ▼
              bin/aggregate_cohorts.php  → cohort_stats
              dashboard tile shows trailing-7-day median per cohort
```

---

## 11. MVP build sequence (4 weeks)

Each step is small and self-contained. Each step lands behind a single commit and includes its own verification.

### Week 1 — Schema + UI capture

1. **Day 1**: schema migration — add `ai_models`, `action_preferences`, `retune_runs`, `cohort_stats`; alter `characters.ai_model_id` and `character_actions.ai_model_id` (default `1`).
2. **Day 2-3**: `dashboard.php` course-correction expansion — render top-2 actions and dropdown only when `alignment_score ≤ 3`. Submit appends one `action_preferences` row per pairing.
3. **Day 4**: aggregate_cohorts.php CLI stub — reads `user_feedback` and `action_preferences` for a window, writes `cohort_stats`, exits non-zero on missing columns.
4. **Day 5**: dashboard cohort tile — vanilla PHP table, one row per `ai_model_id`, no JS library.

**Verification**: register a test user, run scheduler 4 simulated ticks with synthetic events, submit course-correction feedback, confirm `action_preferences` rows + `cohort_stats` roll up.

### Week 2 — C++ priors reader

5. **Day 6-7**: `--priors <file>` CLI flag in `src/main.cpp` (mirror `applyWebInjectFile` parse-cli idiom).
6. **Day 8-9**: line-based `loadPriorsFile` populated into `g_priorsUtil` and `g_priorsDrive` maps. Parsed once at world load, before planning begins.
7. **Day 10**: `PlanningSystem::scoreAction` consults the map — adds the deltas to base utility before action selection. Negative deltas allowed (skip actions).
8. **Day 11**: `Drive::recomputeSetpoint` appends `DRIVE:<axis>:<trait>` deltas. Cap-aware — out-of-bounds ignored with stderr note.

**Verification**: emit a hand-written `priors_v1.txt` with one tweak, run a headless 50-tick session, confirm the affected action is chosen more / less often.

### Week 3 — Offline retuner

9. **Day 12-14**: `retune_weights.py` — joins `user_feedback + action_preferences + character_state_snapshots`, builds a `(action_type, trait, util_delta)` matrix, runs L2-regularized regression per action column, applies safety clamps.
10. **Day 15**: CLI handler — takes `[--from AI_MODEL_ID]`, writes `--out bridge/priors_vN.txt`, INSERTS a `retune_runs` row.
11. **Day 16**: schedule a weekly cron / Task Scheduler job + README section.

**Verification**: feed the script ~100 synthetic preference pairs, confirm the output file matches expected format and clamp limits.

### Week 4 — A/B tasting + transparency

12. **Day 17-18**: at user registration / new character creation, 50/50 split `ai_model_id` (constant-hash on `characters.id` so the answer is reproducible per user — never changes mid-life).
13. **Day 19**: dashboard transparency — show "Your twin's decisions are tuned by model *v2*, trained on 142 corrections from 18 participants" for the active character.
14. **Day 20**: promotion-gate logic, comment-only — `ai_models.active` is a single column, flipping it manually is the entire rollback path for now. Add a brief runbook in `website/README.md`.

**Verification**: end-to-end manual run from "fresh user → 4 ticks → coarse + granular feedback → cohort stats tile updates".

---

## 12. Open questions (must precede any code)

These need an answer before implementation starts. None blocks the write-up here, but each one prevents rewrites during Week 2-4.

### Q1 — Sample-size threshold to ship a new `priors_vN`

**Recommendation**: ≥ 30 preference pairs AND ≥ 10 distinct users AND both cohorts have ≥ 5 live characters after deploy AND at least 7 days have elapsed with vN running.
**Why**: avoids a single opinionated user dominating the regression.

### Q2 — Revert path on a bad retune

**Recommendation**: `ai_models.active` is a single boolean column in MySQL; flipping it is the entire rollback. Engine reads `active=1` per world boot, never mid-run. Old world saves keep working because `priors_vN.txt` is loaded fresh each run.
**Why**: zero rollback ceremony, every change is auditable from `retune_runs`.

### Q3 — User-facing transparency about model version

**Recommendation**: SHOW it on the dashboard. Bragging on improvements is good marketing; whinging about regressions is good quality control.
**Why**: transparency builds trust, and the complaint metric is itself a signal (a v2 with better median but worse free-text has overfit the easy cases).

### Q4 (new) — Character-level vs cohort-level cohort assignment

**Recommendation**: cohort on **character** (each character gets a stable `ai_model_id` at creation, never changes mid-life). Cohort on user would re-twin the same user into different worlds.
**Why**: per-character A/B keeps the world semantically the same, allows within-user comparisons across different twins.

### Q5 (new) — Anti-gaming / sock-puppets

**Recommendation**: one-account-one-active-character for the first 30 days after registration (or until first death). Multiple characters per user remain allowed but only one is in cohort_stats. Free tier → automatic A/B; paid tier → opt out (assign to v_latest always).
**Why**: prevents a user with 100 sock puppets from spamming the preference signal.

---

## 13. Reference: existing files & locations

Anchor points for any implementation work landing against this plan.

### Web app (`website/`)

- `website/config.php` — env-driven; `SIM_DAYS_PER_HUMAN_DAY=4` (line 40), `SIM_TICK_INTERVAL_SECONDS=21600` (line 41).
- `website/db.php` — `Database::query/fetchOne/fetchAll/insert/raw` static helpers, PDO prepared statements.
- `website/auth.php` — `session_init`, `session_user`, `require_auth`, `register_user`, `login_user`, `create_character`, `get_user_characters`, `get_latest_character_state`, `get_recent_actions`.
- `website/register.php:68` — Big Five inputs → `create_character`.
- `website/dashboard.php:23-46` — feedback POST handler; course-correction expansion will follow this pattern.
- `website/sql/schema.sql` — 17 tables; column drift vs PHP queries likely here since schema didn't import anywhere yet.

### Bridge (`bridge/`)

- `bridge/README.md` — file-format spec for `world.txt`, `inject.txt`, `report.json`, `scheduler.log`; tic/civ-day calibration value (1 headless tick = 1 civ-day).
- `bridge/inject.txt` — NUDGE:0 (spawn) + NUDGE:1 (±10 trait deltas, clamped) + structured `kind=web_*` log entries.
- `bridge/report.json` — per tagged entity day report (planned in `webapp-human-sim.md` Phase 3, status to re-verify).
- NEW (this plan): `bridge/priors_vN.txt` — KEY:value lines consumed by `--priors` flag.

### C++ engine (`src/`)

- `src/main.cpp:1953-2172` — `applyWebInjectFile`, `parseCli`, `--inject` flag.
- `src/main.cpp:2225-2234` — `CliOptions` struct (mirror for new `--priors` field).
- `src/Entity.cpp:445-614` — `saveTo` (append at the END, presence-guarded), `loadFrom` (presence-guarded reads at the END).
- `src/header/Entity.h:138-152` — `Personality` struct (Big Five, 0-100).
- `src/header/Entity.h:512` — `int webCharId = -1`.
- `src/header/CivilizationEngine.h:107-223` — `Tribe` struct (`government`, `corruption`, `govSatisfaction`, `councilIds`).
- `src/header/Drive.h` — `DriveSet` + `homeostatic_setpoint` (the pre-existing bipolar drive model).
- `src/CivilizationEngine.cpp:445-451` — `collectTaxes` (governance economy).
- `src/CivilizationEngine.cpp:3382-3395` — `logEvent(day, desc, cat, data)` structured `kind=...` blocks.

### Observability

- `src/observability/Observability.h` — present and unused by the bridge; can be wired later as an OPTIONAL `--profile` flag to dump an event stream alongside `report.json`.

### Plans referenced

- `plans/MASTER_PLAN.md` — overall product roadmap.
- `plans/society-layer.md` Phases 0-6 — government/roles/corruption already implemented (this plan builds on top).
- `plans/webapp-human-sim.md` Phases 0-6 — web app scaffolding; this plan **extends** with new phases 7-10.
- `plans/ASHB2_SIMULATION_REPORT_2026-07-03.md` — recent run outcomes.

---

## Appendix A — Index of new files this plan will introduce

| Path | Purpose | Approx. size |
|---|---|---|
| `plans/human-in-the-loop-upgrade.md` | this document | — |
| `website/bin/aggregate_cohorts.php` | daily CLI: build `cohort_stats` | ~80 LOC |
| `website/bin/retune_export.php` | weekly CLI: emit preference-pair CSV from MySQL | ~120 LOC |
| `tools/retune_weights.py` | weekly offline Python retuner | < 200 LOC |
| `tools/README.md` | how to run + library deps (numpy/scipy) | ~40 LOC |
| `bridge/priors_vN.txt` | generated artifact (gitignored; tracked only in `retune_runs.git_commit` reference) | ~50 lines per version |
| Migration: `website/sql/migration-001-hil-feedback.sql` | adds `ai_models`, `action_preferences`, `retune_runs`, `cohort_stats`, alters `characters` + `character_actions` | ~120 LOC |
| Modified: `website/dashboard.php` | adds course-correction expansion + cohort tile | +~150 LOC |
| Modified: `src/main.cpp` | adds `--priors` CLI flag + `loadPriorsFile` parse + apply in `PlanningSystem` & `Drive` | +~80 LOC |
| Modified: `src/header/PlanningSystem.h` | adds `g_priorsUtil` (`std::map<std::string, float>`) | +~10 LOC |
| Modified: `src/header/Drive.h` | adds `g_priorsDrive` (`std::map<DriveAxis,float>`) | +~10 LOC |
| Modified: `website/register.php` | cohort assignment at character creation | +~10 LOC |

---

*End of plan.*
