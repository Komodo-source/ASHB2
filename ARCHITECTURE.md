# ASHB2 Architecture

ASHB2 is an agent-based human civilization simulator: a few dozen to ten
thousand agents with individual minds (perception, memory, emotion, learning,
theory of mind) whose interactions produce macro history — tribes, religions,
wars, famines, lineages — recorded as a readable chronicle. This document
describes how the system is put together; `plans/MASTER_PLAN.md` records the
forensic audit and the 12-milestone redesign that produced this shape.

## The two-clock loop

Everything hangs off one loop in `src/main.cpp`, shared verbatim by the GUI
and `--headless` modes:

- **Frame** (60× per tick): `updateMovement` — force-based motion (couples,
  family, social bonds, desire/anger targets, personal space, sick-avoidance).
- **Tick = one simulation day** (every `UPDATE_FREQUENCY`=60 frames):
  `updateSimulationStep` → environment advance, `applyFreeWill` (every agent's
  upkeep + decision), births/deaths with pointer repair, civilization update,
  logging/export.

`SimClock` (`src/core/SimClock.h`) owns the day count and the day→year
mapping; agents age by the same year the harvest rolls on.

## The decision pipeline (one mind, one choice)

`FreeWillSystem::chooseAction` (`src/implem_free_will.cpp`) is the single
decision core — a subsumption hierarchy where higher urgency preempts lower:

1. **Possession override** (M10) — if the player has possessed this entity,
   the queued command wins outright.
2. **Reflex layer** — hard survival thresholds (starvation, mortal danger)
   veto everything.
3. **Goal↔value alignment tick** — life goals drift toward lived values.
4. **Habit layer** — calm, familiar contexts fire cached policy
   (reward-modulated strength with an entropy floor, so failing habits decay
   and no agent locks into one action forever).
5. **Deliberation** (`cognitiveChooseAction`) —
   - *Perception*: a percept budget selects the few most salient neighbors
     (`attended`) from the spatial grid; agents act on what they notice, not
     on global truth.
   - *Candidate scoring*: every catalog action is scored by 12 named factors
     (requirements, needs, memory bias, variety, social influence, context,
     persona, pheromones, values, grief, environment, norms). Aggregates that
     don't vary per action (social mood, pheromone fields) are hoisted to
     per-decision sums (M11).
   - *Two-pass memory bias*: the top 14 candidates get episodic + semantic
     memory multipliers, then the top 7 go to weighted-random selection —
     personality-temperature noise keeps minds non-greedy.
   - *Chain-of-thought + hesitation*: the deliberation trace is stored on the
     entity for the inspector UI.

Above 2,000 living agents deliberation staggers into round-robin cohorts
(2, then 4 above 6,000); upkeep still runs for everyone every tick.

## Minds: memory, beliefs, learning

- **Episodic memory** (`LifeMemory`): salience-decayed, pruned with a grace
  period and hard cap; formative memories permanently shift personality.
- **Semantic memory** (`src/SemanticMemory.cpp`): generalized associations
  that bias scoring.
- **Theory of mind** (`MentalModelOfOther`): per-acquaintance beliefs about
  personality, mood and trustworthiness, with confidence that grows on
  observation and decays with staleness — beliefs can be stale and wrong.
  Gossip propagates second-hand beliefs and reputations.
- **Learning** (`src/LearningAdaptation.cpp`): per-agent Q-tables updated by
  direct reward and by *vicarious* experience (watching prestigious others,
  gated by openness), plus habit reinforcement from outcomes.
- **Persona** (`src/PersonaSystem.cpp`): PAD emotional state, body language,
  core beliefs, working memory, inner monologue.

## Social and macro layers

- **Sanctions** (`applySocialSanction`): a witnessed crime triggers vendetta
  anger, reputation collapse, trust craters, bond cuts, tribal exile, and
  blood grievance between tribes — the micro→macro war escalation path.
- **CivilizationEngine** (`src/CivilizationEngine.cpp`): tribes (formation,
  splits, absorption, dangling-member healing), religions (founding gated by
  spiritual vacuum, extinction when followings die), grievance-driven
  diplomacy and wars, innovations, and the event log ("the Chronicle").
- **Environment** (`src/environment/`, `src/world/`): seasons, harvest luck,
  regional resources and a plant/herbivore/predator food chain feed
  `g_seasonalFoodModifier`, closing the loop climate → food → hunger →
  migration/conflict → population.
- **Demography**: fertility desire and conception, ID-based kinship
  (`src/Kinship.cpp`), childhood development, and Gompertz old-age mortality
  anchored on modal adult death age.

## Determinism contract

Same seed ⇒ byte-identical logs. Everything stochastic draws from
`BetterRand` (mt19937 seeded from the world seed); iteration orders are
deterministic (row-major grid walks, index loops); the OpenMP movement pass is
write-local with no RNG (velocities computed from a position snapshot, applied
serially). Wall-clock timestamps in log lines are the one permitted
difference — `scripts/validate.sh` strips them before comparing runs. The M10
live-config multipliers default to exactly 1.0, which is a bit-exact no-op.

## Performance architecture (M11)

- `SpatialGridT` (`src/core/SpatialGrid.h`): uniform hash grid, 48 px cells,
  rebuilt per frame (positions change every frame); id/index lookup maps are
  cached behind `g_entQuadVersion`, bumped only when the entity set changes.
- Movement is OpenMP-parallel (snapshot semantics); sick-avoidance is
  inverted (few sick agents push repulsion outward).
- Hot-loop hygiene: no per-line log flushes, buffered per-entity CSVs,
  hoisted per-decision aggregates, `-O2` release builds.
- `ASHB_PROFILE=1` enables the hierarchical profilers (tick phases → decision
  layers → cognitive segments → factor costs).
- Current benchmarks and the honest gap to the 10k real-time target are in
  `README.md` → Performance notes.

## Persistence & observability

- **Save V2** (`src/SaveLoad.cpp`): versioned full-state save — clock, RNG
  stream, tribes, religions, era, entities — with legacy-format loading.
  Resume is a plausible continuation, not bit-exact vs an unbroken run
  (some subsystems self-seed).
- **tick_history.jsonl**: sampled per-entity state for the HTML viewer and
  `scripts/butterfly.py` (A/B divergence measurement).
- **Realism report** (M9): five red/green assertions printed after every
  headless run (violence share, demographic sustainability, war rate,
  tribe/faith stability, behavioral monoculture).
- **validate.sh / CI**: build + determinism pair + realism report, exit-coded;
  `.github/workflows/ci.yml` runs it on every push.

## UI (GUI mode)

ImGui/GLFW: social-graph world view, mind board, entity inspector (stats,
chain-of-thought, memories), civilization panel, market panel, save/load —
plus the M10 director's tools: God Console (smite/bless/feast/famine/meteor/
calm), Possess mode (override an agent's will), Interview mode (templated
Q&A answered from the agent's real state), and the live Config Console
(`src/header/LiveConfig.h` multipliers).

## Source map

```
src/main.cpp               loop, movement, applyFreeWill, CLI, headless mode
src/implem_free_will.cpp   decision pipeline, sanctions, learning hooks
src/Entity.cpp|.h          agent state, memory pruning, aging/mortality
src/CivilizationEngine.*   tribes, religions, wars, chronicle
src/core/                  SimClock, SpatialGrid
src/environment/, world/   climate, resources, ecosystem, planet, lexicon
src/SaveLoad.cpp           save V2, tick history export
src/UI.cpp                 all ImGui panels incl. god/possess/interview/config
scripts/validate.sh        the CI gate;  scripts/butterfly.py  A/B divergence
plans/MASTER_PLAN.md       audit, redesign, milestone status table
```
