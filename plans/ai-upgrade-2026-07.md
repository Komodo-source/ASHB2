# ASHB2 — AI Intelligence & Realism Upgrade Plan

**Date**: 2026-07-11
**Status**: IMPLEMENTED (first pass, same day) — see "Implementation status" below
**Companion plans**: `plans/human-in-the-loop-upgrade.md` (feedback→priors loop),
`plans/society-layer.md`, `plans/webapp-human-sim.md`
**Scope**: the minds (decision, learning, emotion, perception) and the realism
loop around them. Does NOT cover the web bridge or governance — those have
their own plans.

---

## 1. Where the AI stands today (audit summary)

### What is genuinely strong

| System | Location | State |
|---|---|---|
| Subsumption decision core (possess → reflex → habit → deliberation) | `implem_free_will.cpp` | Live, profiled |
| 12-factor candidate scoring + two-pass memory bias + temperature noise | `implem_free_will.cpp` | Live |
| Episodic memory w/ salience decay, formative events, consolidation → core beliefs | `Entity.cpp`, `SemanticMemory.cpp` | Live |
| Theory of mind w/ confidence staleness + gossip | `MentalModelOfOther` | Live |
| Q-learning + vicarious (prestige-gated) learning + outcome-gated habits | `LearningAdaptation.cpp` | Live |
| PAD emotion, body language, inner monologue, chain-of-thought trace | `PersonaSystem.cpp` | Live |
| GOAP A* over ActionRules + item/tag system | `ai/GoapPlanner.*`, `items/` | Live (survival goals only) |
| NEAT evolved brains (opt-in per entity) | `ai/neat/` | Live (movement/forage only) |
| Heritable genome, bipolar drives, Jungian stack | `Genome.h`, `Drive.h`, `JungianType.h` | Live |
| Macro layer: tribes, religions, wars, chronicle, social classes, dynasties | `CivilizationEngine.cpp` | Live |
| Determinism contract + realism report + CI | `scripts/validate.sh` | Live |

### The gaps this plan targets

1. **Dormant code (~5,500 lines)**. `WorldEnvironment`, `CulturalTransmissionSystem`,
   `InstitutionalSystem` (`environment/EnvironmentModel.cpp`), `CognitiveArchitecture.cpp`,
   `LifeCourse.cpp`, `EmotionalComplexity.cpp`, and `UtilityCurves.h` are compiled but
   never invoked from the tick loop. They are either free capability or dead weight.
2. **Perception is distance-only.** The percept budget picks salient *nearby* agents from
   the spatial grid, but there are no sensory channels: no sight lines, no hearing of
   distant events, no darkness/weather effects on what an agent can know. `noiseLevel`
   and `weatherQuality` exist in `EnvironmentalFactors` but barely gate anything.
3. **Emotion is a modifier, not a driver.** The OCC-style `Appraisal` struct exists
   (`FreeWillSystem.h:113`) but never produces *discrete* emotions (fear ≠ anger ≠ shame ≠
   envy) with distinct action tendencies. PAD is updated but mostly read for display.
4. **Planning is shallow.** `PlanningSystem` generates daily template steps from hardcoded
   goal types; GOAP only chases `stat:hunger`. No multi-day intentions, no social goals
   ("befriend X", "avenge Y", "win the election"), no plan sharing between agents.
5. **No prospection.** Agents never *imagine* outcomes before acting (beyond Q-values) and
   never feel regret/relief after — a large realism lever psychology says humans run
   constantly.
6. **Theory of mind is passive.** Beliefs about others bias trust/scoring but are never
   used to *predict others' actions* — so no anticipation, deception, gift strategy, or
   coalition foresight.
7. **Communication carries almost no information.** `world/Lexicon` names things, gossip
   moves reputation scalars, but agents can't tell each other *facts* ("there's food at
   the river", "the chief lied"), so no rumor distortion, no lies, no knowledge economy.
8. **Determinism risk already on record**: several subsystem RNGs (e.g. `PlanningSystem`'s
   own `mt19937`) are not seeded from the world seed (memory obs #747). Any new AI work
   multiplies exposure — fix first.
9. **Performance ceiling**: 10k agents ≈ 1.5 s/tick; every feature below must state its
   budget and hide behind the existing cohort-staggering + LiveConfig-multiplier pattern.

---

## 2. Design rules for every item below

- **Determinism**: all randomness through `BetterRand`/world-seeded streams; deterministic
  iteration order; feature multipliers default to a bit-exact no-op (M10 pattern).
- **Budget**: each phase declares a per-tick cost target and staggers above 2k agents.
- **Save format**: append-only `KEY:value` blocks with presence-guarded reads.
- **CI gate**: `validate.sh` (determinism pair + realism report) must stay green after
  every phase; new realism assertions are added *with* the features they measure.
- **Kill switch**: every new subsystem gets a LiveConfig toggle so the director console
  can A/B it live and `butterfly.py` can measure its divergence.

---

## 3. Phase A — Reclaim the dormant intelligence (1–2 weeks, low risk, high leverage)

### A1. Seed-audit and RNG unification *(do first, blocks everything)*
Sweep every `std::mt19937` outside `BetterRand` (`PlanningSystem`, `FreeWillSystem::rng`,
cognitive subsystems per obs #747) and re-seed them from the world seed + entity id.
Add a CI assertion: grep for unseeded `mt19937` construction.
**Impact**: protects the determinism contract that the whole human-in-the-loop plan
depends on. **Cost**: ~0 runtime.

### A2. Utility-AI unification (wire `UtilityCurves.h`)
Port the 12 scoring factors one at a time to `UtilityTerm` weighted sums. Scorers become
*data*: a table of `{curve shape, k, m, weight}` per factor per action category, loadable
from `priors_vN.txt` — which is exactly the tuning surface the human-feedback loop
(`human-in-the-loop-upgrade.md`) needs to retune. Keep old code path behind a LiveConfig
flag until `butterfly.py` shows acceptable divergence.
**Impact**: makes the mind tunable without recompiles; unblocks the RLHF loop.
**Cost**: neutral (same math, cleaner).

### A3. Cultural transmission goes live
Wire `CulturalTransmissionSystem` (vertical parent→child at `finalizeChildhood`,
horizontal peer during social actions, oblique elder→young during teaching) into the
civ tick. Traits = practices/beliefs/taboos with a mutation rate; tribes accumulate
distinct cultures; cultural distance feeds diplomacy tension and assimilation on
absorption. Delete `InstitutionalSystem` (redundant with the society layer) and
`WorldEnvironment` wrapper (redundant with `environment/`) rather than wiring them —
less dead weight.
**Impact**: tribes stop being interchangeable; wars/splits get cultural texture in the
Chronicle. **Cost**: civ-tick only (daily), negligible.

### A4. Triage the other orphans
`CognitiveArchitecture.cpp`, `LifeCourse.cpp`, `EmotionalComplexity.cpp`: extract what
Phase B needs (life-stage transition events, emotion taxonomy), then delete the rest.
A module that never runs is a maintenance tax, not an asset.

---

## 4. Phase B — Deeper minds: emotion, prospection, theory of mind (2–4 weeks)

### B1. Appraisal → discrete emotions with action tendencies
Map the existing `Appraisal` fields through OCC rules to ~10 discrete emotions, each an
intensity with decay stored on the entity:

| Appraisal pattern | Emotion | Action tendency (scoring bias) |
|---|---|---|
| undesirable + other-blame | anger | approach/aggress toward source |
| undesirable + uncontrollable | fear | flee, seek allies, freeze |
| undesirable + self-blame + witnessed | shame | withdraw, appease, norm-comply |
| undesirable + self-blame + private | guilt | repair action toward victim |
| desirable for rival | envy | status-seeking, sabotage |
| desirable + other-credit | gratitude | reciprocate, bond boost |
| goal progress | joy/pride | variety, social sharing |
| loss (exists) | grief | (keep current system) |

Emotions plug into candidate scoring as one new factor (replacing the blunt
`entityGeneralAnger` reads scattered around), into body language, and into memory
encoding (emotional intensity already tags `LifeMemory` — now it gets the *right* label).
**Impact**: the single biggest believability lever — an agent that acts from shame reads
as human in the inspector. **Cost**: appraisal already computed; mapping is O(1)/decision.

### B2. Prospection: imagine → act → compare → regret
Before finalizing among the top-7 candidates, each gets a one-step *mental simulation*:
expected stat delta from the action definition × success prior from semantic memory +
Q-value = expected outcome vector; store it in the chain-of-thought. After execution,
compare expected vs actual:
- big negative surprise → **regret** episode (boosts learning rate on that Q-entry,
  writes a `LifeMemory` "I should not have…") — this is counterfactual learning;
- big positive surprise → **relief/delight**, habit reinforcement bonus.
Also add *anticipatory* emotion: dread/hope from the imagined best/worst candidate feeds
PAD before the act. All from data already computed — no search, so cost is a few adds per
candidate.
**Impact**: agents learn faster from mistakes, inner monologue gets "I regret…" texture,
and hesitation (`HesitationState`, currently declared but unused — obs #1030) finally has
a driver: hesitate when imagined outcomes are close or variance is high.

### B3. Active theory of mind: predict, then exploit or protect
New `predictNextAction(other)`: run a *cheap* utility score (top-3 factors only) using the
observer's *believed* model of the other (their `MentalModelOfOther` personality +
estimated mood), not ground truth. Used by:
- **Anticipation**: if a hostile is predicted to attack, pre-emptive flee/ally/strike.
- **Deception** (gated by low integrity + high stakes): act to *shape* others' models —
  fake friendliness, concealed food. Detection = prediction error on the deceiver
  accumulating in `predictability`, which craters trust when it snaps.
- **Strategic gifting/courtship**: choose the target whose model says the gift moves
  trust most.
Budget: only for the ≤3 most salient attended neighbors, deliberating cohort only.
**Impact**: manipulation, betrayal, and paranoia become emergent — Chronicle-grade
stories. **Cost**: 3 × mini-score per deliberation, measurable but bounded; profile it.

### B4. Hierarchical intentions: GOAP grows social arms
- Extend GOAP facts with `rel:trust:<id>`, `rep:self`, `status:auctoritas` and give
  actions declared social effects.
- A weekly **intention** layer above daily plans: pick 1 multi-day intention from top
  LifeGoal ("court X", "become elder", "avenge brother", "stockpile winter food"),
  compile it to GOAP subgoals, persist across days, abandon on repeated frustration
  (frustration machinery already exists).
- Plans become *narratable*: intention + progress renders in interview mode ("What are
  you working toward?").
**Impact**: agents stop living day-to-day; revenge arcs and courtships span seasons.
**Cost**: replan weekly or on frustration, staggered.

### B5. Metacognitive effort control
Stakes × uncertainty decide how hard to think: raise/lower percept budget, candidate
count (14/7 → 6/3 for calm routine, 20/10 for life-or-death), and whether B2/B3 run at
all. System-1/System-2 flag already exists — this makes it real and *pays for* B2/B3 by
cutting routine-decision cost.
**Impact**: smarter where it matters, cheaper overall — likely net perf win.

---

## 5. Phase C — Learning, culture, communication (3–5 weeks)

### C1. Generalizing RL
Replace the string state-signature with a small feature vector (hunger bucket, threat,
social context, season, role) and linear function approximation per action (~10 floats ×
catalog size per agent). Add eligibility traces so multi-step payoffs (hunt → carry →
eat) credit the whole chain.
**Impact**: transfer between similar situations instead of cold-starting each bucket;
noticeably less dithering in young agents. **Cost**: same order as Q-table.

### C2. Skills with practice curves
Per-agent skill map (hunt, farm, craft, heal, oratory, combat) with log-shaped
learning-by-doing, slow decay, and **teaching** (elder/parent transfers fraction of skill
during teach actions — plugs into C3 and the specialization roles from the society
layer). Success probabilities of actions read the skill.
**Impact**: individual life stories ("the best healer in the valley"), realistic role
lock-in, apprenticeship dynamics. **Cost**: O(1) lookups.

### C3. Propositional communication & the knowledge economy
A tiny fact store per agent: `(subject, predicate, value, confidence, sourceId, day)` —
e.g. "berries @ (x,y)", "Kael killed Amari", "chief hoards grain". Facts propagate
during social actions with distortion = f(speaker accuracy, relationship, telephone-game
noise); lies (B3) inject false facts. `EpisodicMap` becomes the spatial slice of this
store. Agents act on believed facts (forage where told), so **false belief → visible
mistake** — the most human failure mode there is. Rumors about leaders feed the
society layer's legitimacy/corruption mechanics.
**Impact**: information becomes a resource; secrets, rumors, and scandals emerge.
**Cost**: cap facts/agent (~64, salience-pruned, same pattern as LifeMemory).

### C4. NEAT expansion (kept honest)
Widen the sensor/actuator contract (add threat, social density, season; outputs for
social/flee urges) behind a genome version tag; hybrid arbitration — the evolved brain
emits *biases into* utility scoring rather than overriding it, so evolved instinct and
learned deliberation blend instead of switching.
**Impact**: multi-generation behavioral evolution you can watch in the diversity
telemetry. **Cost**: unchanged evaluation, slightly wider nets.

---

## 6. Phase D — Sensory, embodied, immersive realism (3–5 weeks, parallelizable with C)

### D1. Sensory channels (see / hear / smell)
- **Vision**: cone in facing direction + range modulated by daylight (`daylightHours`
  exists), weather, and genome sight; terrain/structures occlude (coarse grid raycast,
  cached). Outside the cone → not in `attended` → genuinely unseen crimes.
- **Hearing**: loud events (fight, scream, celebration) emit a decaying sound field on
  the existing pheromone-grid infrastructure; agents perceive *that* something happened
  without *what* — investigate-or-flee choice by personality (fear vs curiosity).
- **Smell** stays on the pheromone field (already live) + food-spoilage scent.
Misperception now creates false beliefs *naturally*, feeding ToM staleness and C3 facts.
**Impact**: night ambushes, unwitnessed murders, mysterious noises — perception gap is
where drama lives. **Cost**: the one physically risky item; prototype the raycast on the
48px grid, budget ≤10% of movement pass, else ship cone+range without occlusion.

### D2. Circadian rhythm & sleep
Sleep pressure accumulates (fatigue exists but has no cycle); agents sleep at night
(personality-skewed chronotypes), sleep quality = f(safety, shelter, stress).
**Memory consolidation moves into sleep**: `consolidateMemories()` runs on waking,
strength scaled by sleep quality; sleep-deprived agents get System-1-only decisions
(B5 hook), irritability (B1 anger bias), and hallucinated-fact risk (C3 hook, rare).
Optional flavor: dreams = recombined high-salience memories written to inner monologue.
**Impact**: day/night texture for the viewer; insomnia-from-grief is deeply human.
**Cost**: sleeping agents skip deliberation → net perf *gain* overnight.

### D3. Embodiment & visible condition
Injury state (wounded → limp → recover/infect) from fights and hunts, visible in body
language and movement speed; illness symptoms readable by others (fever behavior —
sick-avoidance already exists, now it has visible cause); pregnancy trimester effects;
age-graded gait. All existing stats, surfaced behaviorally.
**Impact**: the world *shows* rather than tells state — big inspector/viewer win.

### D4. Rituals, mourning, festivals
From C3 culture traits: tribes evolve rituals (harvest feast, funeral rites, coming-of-age)
executed as synchronized group actions at Chronicle-worthy moments. Funerals hook the
existing grief system (attendance modulates grief recovery); festivals discharge
suppression debt (`tickEmotionalSuppression` exists) and boost cohesion.
**Impact**: the Chronicle gains rhythm and meaning; group scenes for the viewer.
**Cost**: civ-tick scheduled, cheap.

### D5. Place attachment & home
Agents accrue attachment to regions/dwellings (uses `EpisodicMap` density); leaving home
costs happiness (homesickness), returning heals; exile (already a sanction) becomes
psychologically expensive; migration decisions weigh attachment vs hunger.
**Impact**: migration waves feel reluctant and tragic, as they should.

---

## 7. Phase E — Close the loop: measurement & human feedback (ongoing)

### E1. Execute `plans/human-in-the-loop-upgrade.md`
A2's data-driven scorers are the missing tuning surface. Resolve its 3 open questions and
ship the priors_vN loop — user alignment ratings become the ground truth for whether
Phases B–D actually read as "more human".

### E2. Realism metrics v2 (extend the M9 report)
- **Time-use distribution** per capita vs hunter-gatherer ethnography bands (work/social/
  rest/ritual shares).
- **Behavioral entropy** per agent (already have monoculture check — make it per-cohort).
- **Emotion episode statistics** (fear/anger/joy incidence — no permanent-rage worlds).
- **Belief accuracy** (C3): fraction of held facts that are true — should sit ~0.7-0.9,
  not 1.0 (omniscience) or <0.5 (chaos).
- **Sleep debt distribution**, **skill Gini**, **rumor half-life**.
Each new phase lands with its assertion, keeping CI as the realism regressor.

### E3. Storyteller pass on the Chronicle
NarrativeEngine already runs; feed it B4 intentions, B1 emotions, and D4 rituals so
chronicle entries carry motive ("Kael, shamed at the feast, left the village at dawn")
— pure logging change, disproportionate immersion payoff.

---

## 8. Sequencing & effort summary

| Order | Item | Effort | Risk | Realism impact |
|---|---|---|---|---|
| 1 | A1 RNG audit | S | none | (protects everything) |
| 2 | A2 utility unification | M | low | enables tuning + RLHF |
| 3 | B1 discrete emotions | M | low | ★★★★★ |
| 4 | B2 prospection/regret | S–M | low | ★★★★ |
| 5 | A3 culture live | M | low | ★★★★ |
| 6 | B5 metacognition | S | low | perf win |
| 7 | B3 active ToM/deception | M | med | ★★★★★ |
| 8 | B4 intentions/social GOAP | M–L | med | ★★★★ |
| 9 | D2 sleep/circadian | M | low | ★★★★ |
| 10 | C3 facts/rumors/lies | L | med | ★★★★★ |
| 11 | C1 RL generalization | M | med | ★★★ |
| 12 | C2 skills | M | low | ★★★★ |
| 13 | D1 senses | L | high | ★★★★★ |
| 14 | D3 embodiment | M | low | ★★★ |
| 15 | D4 rituals | M | low | ★★★★ |
| 16 | D5 place attachment | S | low | ★★★ |
| 17 | C4 NEAT v2 | M | med | ★★★ |
| 18 | E1–E3 loop closure | M | low | compounding |

S ≈ ≤2 days, M ≈ 3–7 days, L ≈ 1–2 weeks.

**Recommended first milestone (2 weeks)**: A1 + A2 + B1 + B2 — after which every agent
feels emotions with names, imagines before acting, regrets afterward, and the whole mind
is tunable from data files. That is the highest believability-per-effort slice available,
and it unblocks the human-feedback loop that makes all later tuning empirical.

---

## 9. Implementation status (2026-07-11)

Core module: `src/ai/MindUpgrade.h/.cpp`; integration edits in
`implem_free_will.cpp`, `main.cpp`, `Entity.h/.cpp`, `CivilizationEngine.h/.cpp`,
`SaveLoad.cpp`, `FreeWillSystem.h`, `LiveConfig.h`. Verified: clean build,
300-tick headless run with all realism checks 1-8 PASS, determinism pair PASS,
save/load round-trip PASS.

Latent bug fixed along the way: `PlanningSystem::saveTo/loadFrom` wrote a raw
binary blob through the text-mode save stream — any float byte equal to 0x1A
read back as DOS EOF and truncated the save mid-entity (probabilistic load
crashes). The planner now serializes as `PLAN_V2:`/`PSTEP:`/`PSTR:`/`PGOAL:`
text lines like every other section; `Entity::loadFrom` skips unknown keys and
resyncs on the END ENTITY marker, and returns false on a truncated block so
`loadGame` recovers what was readable from old (pre-V2-planner) saves instead
of crashing.

| Item | Status | Notes |
|---|---|---|
| A1 RNG audit | ✅ (was already done) | all engine RNGs flow from `g_worldSeed` via `nextDeterministicSeed`/`makeStream`; only dormant modules (Scalability/Validation/WorldMap) hold unseeded RNGs |
| A2 utility unification | ✅ (as data-driven weights) | `ScoringPriors` + `bridge/priors_active.txt` loader; guarded so defaults are bit-identical. Full UtilityCurve port deferred |
| A3 culture live | ✅ (lean) | festivity axis on Tribe, vertical value+skill transmission at `finalizeChildhood`, allied-culture convergence; cultural distance→diplomacy already existed (`valDiff`) |
| A4 orphan triage | ⏸ deferred | dormant modules left in place (deletion is a separate, destructive pass) |
| B1 discrete emotions | ✅ | 10 emotions, appraisal in `mind::upkeep`, 13th scoring factor, body feedback |
| B2 prospection/regret | ✅ | history-centered expectations, asymmetric thresholds (outcome quantization), counterfactual RL bonus, hesitation driver |
| B3 active ToM | ✅ (anticipation + reputational + deception) | `threatPrediction` via mental model + body language + heard facts; deception now live — `attemptDeception`/`detectDeception` (2026-07-23): low-integrity+grudge agents fake warmth to inflate a target's trust beyond what the interaction earned, and a later Betray/Manipulate against them snaps trust/predictability harder as a "prediction error" than the graded update, with a distinct shock memory when the target was truly fooled |
| B4 intentions | ✅ (bias form) | weekly adoption from top LifeGoal, progress/abandonment, scoring pull; GOAP social-fact compilation deferred |
| B5 metacognition | ✅ | stakes-driven candidate widths 8/4 vs 14/7, night-narrowed percept budget |
| C1 RL generalization | ✅ (lite) | +fear/season state features, eligibility-lite previous-pair credit; function approximation deferred |
| C2 skills | ✅ | 8 skills, log practice curves, parent/teacher transfer, Hunt/Gather/Farm yield effects, Gini metric |
| C3 knowledge economy | ✅ | capped fact store, gossip propagation w/ distortion + 8% content corruption + grudge lies, crime witnessing, food-region facts, belief-accuracy metric |
| C4 NEAT v2 | ✅ (hybrid arbitration) | brain outputs bias Gather/Hunt/EatMeal scores instead of overriding; sensor widening deferred (save compat) |
| D1 senses | ✅ (lite) | night halves percept budget, commotion hearing events, dark-alone fear; occlusion raycasts deferred |
| D2 sleep/circadian | ✅ | sleep pressure/quality, deprivation effects, consolidation moved into good sleep |
| D3 embodiment | ✅ | injury from duels/raids/hunts, healing w/ genome resilience, withdrawn body language, health drain |
| D4 rituals/festivals | ✅ | granary-gated feasts (suppression discharge, joy, cohesion, chronicle entry), cadence from festivity |
| D5 place attachment | ✅ | homeAttachment growth among familiar faces, homesickness window after tribe switch |
| E1 priors loop | ✅ (engine side) | loader + format documented in `bridge/README.md`; the offline Python rating→priors pipeline remains (see human-in-the-loop plan) |
| E2 metrics v2 | ✅ | report lines 6-8 (emotion episodes, belief accuracy, skill Gini); CI still gates on 1-5 only, new lines use `warn` not FAIL on short runs |
| E3 storyteller | ✅ (first pass) | kill motives carry the dominant emotion, CoT names the feeling, festival chronicle entries |
