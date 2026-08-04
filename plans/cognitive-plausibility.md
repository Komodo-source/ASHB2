# Phase 6 — Cognitive Plausibility (M13-M16)

**Goal**: close the gap between a decision pipeline that is *mathematically* sound and one that
is *psychologically* plausible. The world already computes correct numbers; what it does not yet
do is think in the shapes a mind thinks in — hierarchically under stress, continuously across
gradients, permanently scarred by things it has forgotten, and strategically about other people.

Four milestones, ordered by dependency and impact-per-unit-effort:

| Milestone | What it kills | Effort | Kill switch |
|---|---|---|---|
| **M13** Continuous vector utility | tabular Q-learning on string keys | Medium | `utilityMul` |
| **M14** Appraisal gating + persona drift | flat utility mush, vanishing trauma | Small | `appraisalMul` |
| **M15** Simulation ToM + speech acts | "danger score", information soup | Large | `tomMul`, `speechMul` |
| **M16** Cognitive level of detail | the O(N) cost of all of the above | Medium | `lodMul` |

Each phase is self-contained and executable in a fresh context. Execute in order.

---

## Phase 0 — Verified ground truth (READ FIRST, every phase)

Everything in this table was read out of the tree before this plan was written. Where the
originating audit was wrong, the correction is marked **[CORRECTED]** — those four items are
load-bearing and a phase written against the wrong version will not work.

| System | Where | Key facts |
|---|---|---|
| RL state signature | `implem_free_will.cpp:1648-1666` | Returns a **6-character** string: loneliness L/M/H, anger `>50`, hunger `>50`, anyone nearby, fear `>50`, season initial. Total state space ≈ 3×2×2×2×2×4 = **192 states** |
| Q-table | `header/LearningAdaptation.h`, `ActionValueFunction` | `std::map<std::string, std::map<std::string,float>> qValues` + parallel `visitCounts`; `learningRate 0.1`, `discountFactor 0.9`, `explorationRate 0.1` |
| RL update site | `implem_free_will.cpp:3016`, `3181-3192` | `rlPreState` captured before the action, `rlNextState` after, then `rlSystem.processExperience(...)` |
| Decision entry | `implem_free_will.cpp:~3040` `chooseAction` | Subsumption hierarchy; the legacy ~330-line scorer was already deleted as unreachable — there is **one** pipeline, `cognitiveChooseAction` |
| Memory bias | `SemanticMemory.cpp` `calculateMemoryActionBias` | Retrieves **topK=3**; applies a flat per-event-type bias (`+0.15` bond, negative for trauma) weighted by `relevanceScore * emotionalIntensity`; returns `0.0f` when nothing is retrieved |
| Memory search | `SemanticMemory.cpp` `searchRelevantMemories` | **Linear scan of the entire `memoryDatabase`**, one `cosineSimilarity` per entry, then boosts (entity ×1.5, formative ×1.3, emotional, time decay) |
| Threat model | `ai/MindUpgrade.cpp` `mind::threatPrediction` | `clamp01(visible + believed + heard)` — body language, mental model confidence×(anger,trust), and `FACT_KILLED`/`FACT_DANGER` hearsay |
| Gossip | `ai/MindUpgrade.cpp` `mind::shareKnowledge` | Picks `knowledge.mostSalient(simDay)`, `confidence *= 0.85`, value smear, 8% truth corruption, drops below `confidence < 0.12` |
| Personality drift | `ai/MindUpgrade.cpp:274-278` | **Already exists**: sustained low mood without support moves `neuroticism` by ±0.3/pass, gated on `moodMul` |
| Determinism | verified empirically | Two parallel runs, one seed → byte-identical stdout. The contract is real and must not be broken |
| LiveConfig | `header/LiveConfig.h`, registry `main.cpp:69-108` | 35 knobs today. Every feature gets one; `X = 0.0f` **must** be bit-exact pre-feature behaviour |

### [CORRECTED] Four things the originating audit got wrong

1. **The Q-table is `std::map`, not `std::unordered_map`.** This matters more than it sounds:
   ordered iteration is part of why the determinism contract holds. Any replacement must have a
   deterministic iteration order — a `std::unordered_map` over float keys would silently break
   byte-identity across libstdc++ versions.

2. **Credit assignment is 2-step, not 1-step.** `implem_free_will.cpp:3186-3189` already
   implements "C1 eligibility-lite": the previous `(state, action)` pair is replayed with
   `reward * 0.3f`. The critique still stands — two steps does not credit a four-month farming
   project — but M13 is *extending* an eligibility trace, not inventing one.

3. **`rlNextState` is usually equal to `rlPreState`.** Both are computed from the same
   `context.numPeopleNearby` and the buckets are coarse enough that one action rarely crosses a
   threshold. The bootstrapping term `discountFactor * max Q(nextState, ·)` is therefore mostly
   bootstrapping off the state it just left. This is a second, independent reason the tabular
   learner cannot represent delayed reward.

4. **`shareKnowledge` is not purely undirected.** It already has a strategic branch: a speaker
   with `integrity < 30` and an active grudge (`anger > 40`) against the subject flips the fact
   to `FACT_DANGER` 35% of the time and *raises* its confidence ("liars sound sure"). M15
   extends this hook rather than replacing the function.

### [NEW] A bug this plan must fix first

**`prevRlState` and `prevRlAction` are shared across every entity in the world.**

`Entity` owns a `FreeWillSystem fws` (`header/Entity.h:362`), but nothing ever calls through it —
every decision in the simulation goes through the single **global** `FreeWillSystem sys`
(`main.cpp:171`, used at `main.cpp:174-175`, `1445`, `1532-1624`). `prevRlState` / `prevRlAction`
are plain members of that one object (`header/FreeWillSystem.h:231-232`).

So the eligibility-lite replay at `implem_free_will.cpp:3186` credits **whatever entity acted
immediately before this one in iteration order**. Entity B's hunting success is being used to
reinforce entity A's decision to sleep. Every agent in the world is polluting every other agent's
learning signal with a 0.3-weighted reward from a stranger.

This must be fixed as **M13-P0**, before any RL rewrite — otherwise M13 will be measured against a
baseline whose learning is noise. `Entity::fws` is dead weight and should either be deleted or
become the actual home of per-entity learner state (it is the natural place for it).

### Anti-patterns (DO NOT)

- **Do not put an LLM in the tick loop.** See §4. Post-hoc translation only.
- **Do not use `std::unordered_map` or any hash-ordered container** in a path that feeds a
  decision. Iteration order is part of the determinism contract.
- **Do not draw from `rand()` or a fresh generator.** Use the engine's seeded streams
  (`BetterRand`, or a private stream via `makeStream` as `Entity.cpp:32` does for QI).
- **Do not accumulate into personality every tick.** `MindUpgrade.cpp:274-278` already moves
  `neuroticism`; a second unbounded accumulator on the same field will saturate it at the clamp
  and never come back. See the `applyBuildingEffects` saturation failure documented in
  `qi-university.md` for what this looks like when it goes wrong.
- **One read site per effect**, so `--set <knob>=0` reproduces the old world exactly.
- **Serialization is positional**: append new `KEY:` lines at the END of `Entity::saveTo`, read
  them LAST behind a presence guard.

---

## M13 — The "anti-robot" pass: continuous vector utility

**Problem.** 192 discrete states means behaviour cannot vary smoothly. An agent at hunger 49 and
an agent at hunger 51 are different people to the learner and identical to each other at 51 and
99. Between thresholds nothing changes, so agents settle into fixed cycles; at a threshold
everything changes at once, so the transition looks like a switch being thrown.

### M13-P0 — Fix the shared eligibility trace (do this first)

Move `prevRlState` / `prevRlAction` off the global `FreeWillSystem` and onto the entity. Either
route decisions through `Entity::fws`, or (simpler, less invasive) add
`std::string prevRlState, prevRlAction;` to `Entity` and have `executeAction` read and write
`entity->prevRlState`. Verify by asserting in a debug build that the replayed state belongs to the
same entity.

**Acceptance**: a single-entity run and a 200-entity run produce the same learned policy for that
entity, given the same experiences. Today they do not.

### M13-P1 — State and action embeddings

1. **State vector** `std::array<float,16>`, built by a new
   `FreeWillSystem::rlStateVector(Entity*, int numNearby)` beside the existing signature function.
   Normalise every component to `[0,1]` so no dimension dominates the dot product:
   drives (hunger, loneliness, fatigue, stress, health), affect (fear, anger, joy, guilt),
   context (people nearby, is-night, season sin/cos, in-shelter, has-food, local danger), bias term.

   Keep `rlStateSignature` alive and unchanged during the transition — the two run side by side
   until M13-P4 retires the string path.

2. **Action embedding** `std::array<float,16>` per action, stored in the action catalogue.
   Initialise from a fixed hand-authored table (not random) so a fresh world starts deterministic
   and identical across machines.

3. **Utility** `U(s,a) = wᵀ · (s ⊙ a) + b_a`, i.e. the elementwise product of state and action
   embedding, projected through a learned 16-vector. One `w` per entity (16 floats), plus a scalar
   bias per action. This is a **linear bandit**, not a neural net: it is 16 multiply-adds, fully
   deterministic, and trivially serialisable.

   > The originating plan proposed a 16×16 matrix `W`. That is 256 floats per agent and buys
   > cross-dimension interaction the state vector already encodes through `s ⊙ a`. Start with the
   > 16-vector; promote to a matrix only if validation shows it is needed.

4. **Learning** — gradient step on prediction error, no matrix inverse:
   ```
   δ  = reward + γ · maxₐ' U(s', a') − U(s, a)
   w += α · δ · (s ⊙ a)
   b_a += α · δ
   ```
   with `α = 0.1`, `γ = 0.9` (carry the existing constants across). Clamp `w` to `[-4, 4]` so a
   single freak reward cannot blow up the policy.

5. **Eligibility trace** — replace the 2-step hack with a real decaying trace of depth 8:
   keep a per-entity ring of the last 8 `(s ⊙ a)` vectors; on reward, apply
   `w += α · δ · λᵏ · eₖ` with `λ = 0.7`. This is what finally lets a four-month farming project
   credit the days of labour that preceded the harvest.

6. **Exploration** — keep ε-greedy at 0.1, drawing from the existing `BetterRand` stream so
   determinism holds.

### M13-P2 — Kill switch

`float utilityMul = 1.0f;` in `LiveConfig.h` + registry entry. At `0.0f` the vector path is
skipped entirely and the string Q-table drives decisions exactly as today. This is what makes M13
verifiable: `--set utilityMul=0` must be **bit-identical** to pre-M13 main.

### Acceptance

- No entity repeats a cycle of fewer than 4 distinct actions for more than 20 consecutive
  decisions during a stable (non-crisis) period. Instrument this as a realism report line.
- Behaviour varies continuously: sweeping hunger 40→60 in a scripted probe changes the chosen
  action's utility monotonically, with no cliff at 50.
- `--set utilityMul=0` is bit-identical to main.
- Memory: per-entity RL state is a fixed 16 floats + 8×16 trace + per-action bias, replacing an
  unbounded `map<string, map<string,float>>`.

---

## M14 — The "tunnel vision" pass: appraisal gating and persona drift

**Dependency**: M13, so the new learning signal is trained against masked utilities rather than
learning a policy the mask will later forbid.

### M14-P1 — Appraisal gates (Maslow, hard-masked)

Insert an `AppraisalGates` struct computed **once** at the top of `cognitiveChooseAction`, before
the factor sum, and apply it as a multiplicative mask on the final per-action utility:

| Condition | Effect |
|---|---|
| `hunger > 80` | `mask[social] = 0`, `mask[leisure] = 0` — a starving agent does not flirt |
| `fear > 60` | risk aversion ×3 on every deliberation; `mask[explore] = 0` |
| `anger > 70` | discount factor `γ` → 0.5 (impulsive: the future stops mattering) |
| `fatigue > 85` | `mask[complex_work] = 0` |
| any gate active | narrow the candidate list to the top 3 by utility ("tunnel vision") |

Gates are **hard zeros, not penalties** — that is the entire point. A linear penalty is what
produces the current mush, where a starving agent still socialises because twelve small positive
terms outvoted one large negative one.

**Anti-pattern**: do not apply the mask before the RL update, or the learner will be trained on
utilities it never gets to act on. Mask, choose, then learn on the masked utility.

### M14-P2 — Sleep-time persona drift (fixes vanishing trauma)

**Careful**: `MindUpgrade.cpp:274-278` already drifts `neuroticism` from sustained mood. Do not
add a second unbounded accumulator on the same field — extend the existing pass.

In `consolidateMemories`, during deep sleep only:

1. Aggregate the emotional valence of memories being **pruned or decayed below retrieval
   threshold** this pass — precisely the ones about to stop influencing behaviour.
2. If the pruned cluster is dominated by `trauma` / `loss_death` / `betrayal`, apply a permanent
   micro-shift: `neuroticism += 0.5`, `baselineTrust -= 0.5`, `agreeableness -= 0.2`.
3. **Cap the lifetime drift** at ±15 points from the entity's birth personality, stored as a
   separate `personalityDrift` accumulator so the cap is enforceable and the drift is auditable
   in the inspector.
4. Positive clusters drift the other way, so a well-loved life makes a measurably calmer person.

The specific memory is forgotten; the *disposition* it created remains. This is the mechanism that
turns a bad decade into a personality instead of a temporary debuff.

### M14-P3 — Kill switch

`float appraisalMul = 1.0f;` — at `0.0f`, no masks are applied and no drift is written.

### Acceptance

- Agents with `hunger > 80` show **0%** socialisation over a 500-tick window (currently non-zero).
- An entity subjected to a scripted decade of loss shows a measurable `neuroticism` increase that
  **persists after** the originating memories are pruned. Verify by dumping personality at tick
  1000 and 2500 with the memory database cleared in between.
- Lifetime drift never exceeds the ±15 cap on any entity across a 2500-tick run.
- `--set appraisalMul=0` is bit-identical to post-M13 main.

---

## M15 — The "politics" pass: simulation ToM and speech acts

**Dependency**: M14 — a ghost simulation is only as good as the appraisal model it runs, so the
gates must exist before the ghost can predict behaviour under stress.

### M15-P1 — 1-step lookahead theory of mind

Replace the additive danger score with a prediction of what the other agent will *do*.

1. `struct GhostAgent` — a POD holding the observer's **beliefs** about the target: believed
   hunger, believed wealth, believed anger, believed goals. Populate from
   `MentalModelOfOther`, degraded by `effectiveConfidence(simDay)`. It must not alias the real
   `Entity`.
2. `ToMSystem::simulateNextAction(const Entity& self, const GhostAgent& g)` — run a stripped
   deterministic `reflexLayer` plus a single-action `cognitiveChooseAction` over the ghost.
3. Compare the predicted action against the observer's assets:
   predicted `StealFood` + observer has surplus → threat high; predicted `Ally` → opportunity.
4. **Decoupling requirement**: `cognitiveChooseAction` currently takes `Entity*` and mutates it.
   It must be split into a pure scoring core taking a state vector (which M13 already
   introduces — this is why M15 depends on M13) and a thin mutating wrapper. Budget most of the
   effort here; the ghost logic itself is small.

**Cost control — non-negotiable.** Run the lookahead **only** for the observer's Dunbar network
(cap 150). For everyone else use the existing `threatPrediction` as the fast path. This is
`O(N × 150)`, not `O(N²)`. At 10k agents the naive version is 10⁸ micro-deliberations per tick and
will destroy the performance target.

**Determinism**: the ghost must draw from a stream derived from the *observer's* id
(`makeStream(g_worldSeed.master, observerId)`), never from the shared `rng`, or ToM will reorder
every random draw in the world.

### M15-P2 — Speech acts

Add `conversationalGoal` to `Entity` (`None`, `Alliance`, `SmearRival`, `ExtractInfo`,
`Reassure`), chosen from the speaker's current social standing and grudges.

Rewrite `mind::shareKnowledge` to **select** a fact that advances the goal instead of always
taking `mostSalient`:

| Goal | Selection rule |
|---|---|
| `Alliance` | positive facts about people the listener likes |
| `SmearRival` | negative facts about the listener's rivals — extends the existing grudge branch |
| `ExtractInfo` | offer a cheap true fact to prompt reciprocation |
| `Reassure` | low-threat facts; suppress `FACT_DANGER` |

Keep the whole telephone-game degradation (`0.85`, value smear, 8% corruption, `0.12` floor)
exactly as it is — that machinery is good and orthogonal to selection.

### M15-P3 — Kill switches

`tomMul` and `speechMul`, separately, so the two halves can be bisected independently.

### Acceptance

- Coordinated smear campaigns are observable: ≥3 agents independently propagating the same false
  `FACT_DANGER` about one target within 60 days, and that target's average trust measurably falling.
- Pre-emptive defence: an agent that predicts `StealFood` from a neighbour takes a protective
  action (guard, hide food, pre-emptive confrontation) before the theft occurs.
- ToM cost stays under 15% of tick time at 2k agents; the Dunbar cap is never exceeded.
- `--set tomMul=0 --set speechMul=0` is bit-identical to post-M14 main.

---

## M16 — Cognitive level of detail: scale

**Dependency**: M15 — the deep-mind logic must be stable before it is suspended and resumed.

### M16-P1 — Semantic memory search (do this first; it is the cheapest win)

`searchRelevantMemories` linearly scans the whole `memoryDatabase` computing a cosine similarity
per entry, and `calculateMemoryActionBias` calls it on **every action scored, every decision**.
This is the largest hidden cost in the pipeline and it grows with every memory an agent ever forms.

Two fixes, in order:
1. **Working-memory buffer**: restrict search to the top 32 most-active embeddings, refreshed on
   consolidation. Search becomes `O(32)` regardless of life experience.
2. **Cache per decision**: `calculateMemoryActionBias` is called once per candidate action with the
   same query. Retrieve once, score all actions off that one result. This alone removes a factor
   equal to the candidate-list length.

### M16-P2 — Dormant agents

Agents far from any high-stakes event or observer focus collapse to a **macro-state Markov chain**:
their `AgentMind` is unloaded, and behaviour is drawn from a coarse transition table calibrated
against their last active policy. Re-inflate on approach, on becoming a gossip subject, or on any
event targeting them.

**Determinism requirement**: the dormant/active decision must be a pure function of world state,
never of wall-clock time, frame rate, or camera position in a GUI build — otherwise a headless run
and a GUI run diverge, and the byte-identity contract dies. Drive it from simulation state only.

### Acceptance

- 10k agents in < 10 minutes; 50k agents in < 30 minutes.
- A dormant→active→dormant round trip leaves an agent bit-identical to one that never slept.
- Headless and GUI builds produce identical logs for the same seed with LOD enabled.

---

## §4 — Critical trade-offs

### A. The ToM bottleneck
Enforce the Dunbar cap (150) absolutely. `O(N²)` micro-deliberation is not survivable at any
agent count this project targets. Strangers get the cheap heuristic, always.

### B. Determinism
Every mechanism proposed here is deterministic float math — dot products, threshold conditionals,
table lookups. The two real hazards are **(i)** hash-ordered container iteration and **(ii)** any
new RNG draw on the shared stream. Both are avoidable: ordered containers, and private streams
seeded off the world seed (the pattern `Entity.cpp:32` already establishes for QI).

Verify the contract the way it was verified for QI: two runs, one seed, byte-compare stdout. Give
each run its **own working directory** — the app clears `./src/data/` relative to CWD at startup
(`main.cpp:3311-3325`), so concurrent runs in one directory silently corrupt each other's logs.
That mistake has already invalidated one round of verification in this repo.

### C. The LLM question — do NOT put an LLM in the tick loop
Injecting an LLM into `chooseAction` destroys both the performance target and byte-identity
(temperature, sampling, provider drift, network non-determinism). There is no configuration of an
LLM call that is safe inside a deterministic simulation loop.

**Use LLMs strictly as a post-hoc translator.** The simulation runs 100% deterministically and
emits a JSONL of internal state — retrieved memories, emotion vector, appraisal gates that fired,
ToM prediction, chosen action and its utility margin. A separate offline pass (or the Chronicle /
Interview UI) reads that JSONL and generates prose for `internalNarrative`. The simulation stays
fast and reproducible; only the presentation layer is generative. If the prose must be
reproducible too, pin it by caching output keyed on a hash of the JSONL row.

### D. What this plan deliberately does not do
- No new neural architecture. The linear bandit is chosen precisely because it is inspectable,
  serialisable and deterministic.
- No change to the emotion model (OCC), the norm system, or the civilisation layer.
- No fertility, mortality or economy tuning — this phase is about *how minds decide*, not about
  what the world does to them.

---

## Suggested execution order for a single sitting

M13-P0 (the shared-trace bug — small, and everything downstream is measured against it) →
M13-P1/P2 → validate the loop metric → M14-P1 → M14-P2 → validate tunnel vision and drift →
M16-P1 (the memory-search win is independent and cheap; take it early if profiling demands) →
M15-P1 → M15-P2 → M16-P2.

M14-P2 (persona drift) and M16-P1 (memory search) are independent of everything else and can be
split across contexts once M13 lands.
