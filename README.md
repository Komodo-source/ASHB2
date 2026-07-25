# ASHB2 — an agent-based civilization & life simulator

ASHB2 is a "society in a box" written in C++17, rendered with GLFW + Dear ImGui + ImPlot. Psychologically rich individual agents live, form relationships, reproduce, and die — and their moment-to-moment decisions emergently produce tribes, religions, economies, wars, and history.

## Building

Requires CMake ≥ 3.10 and MinGW-w64 (g++ with C++17). Bundled third-party libs live in `libs/` (GLFW, ImGui).

```bash
cmake -G "MinGW Makefiles" .
mingw32-make -j8
```

The build defaults to `-O2` (Release). Use `cmake -DCMAKE_BUILD_TYPE=Debug .` when you need a debugger — but expect roughly an order of magnitude slower ticks.

## Running

```bash
app.exe                              # GUI (interactive prompts for seed/region/…)
app.exe --headless 600 --seed 42 --entities 40 --region 1 --chaos 1.3
```

| Flag | Meaning |
|---|---|
| `--headless <ticks>` | run N ticks without a window, then exit |
| `--seed <text\|num>` | world seed — **same seed ⇒ byte-identical history** |
| `--entities <n>` | founding population (default 40) |
| `--region <1-4>` | disease-climate region |
| `--chaos <0.3-2.5>` | divergence level (butterfly knob) |
| `--load <file>` | resume from a save instead of spawning founders |
| `--save-at <tick>` | write a checkpoint at this headless tick |
| `--save-file <file>` | where `--save-at` writes |
| `--scenario <name>` | preset bundle: `eden` (gentle, 150 souls), `crucible` (harsh, 60), `babel` (crowded, 400), `dish` (petri dish, 12) — flags placed after it override |

Every headless run ends with a **realism report** — five red/green assertions (homicide share of deaths, demographic sustainability, war rate, tribe/faith stability, behavioral monoculture). Grep `REALISM` / `FAIL` in CI to catch social-dynamics regressions.

### Director's tools (GUI)

The GUI ships four intervention panels: the **God Console** (smite, bless, torment, feast, famine, meteor, great calm — every act written into the Chronicle), **Possess** (take over the selected entity: your command overrides its reflexes, habits and deliberation until released), **Interview** (six templated questions answered from the entity's real feelings, memories, beliefs, goals and opinions of others), and the **Config Console** (live world multipliers — movement force, old-age mortality, food yield, aggression; all default to an exact ×1.0 no-op, so determinism holds until you touch them). For A/B experiments, run two headless worlds and feed both `tick_history.jsonl` files to `python scripts/butterfly.py` to get a per-day divergence table (the butterfly effect, measured).

### Determinism

Two runs with the same seed and parameters produce identical event logs (verified continuously during development). Saves (format `ASHB2_SAVE_V2`) carry the sim clock, shared RNG stream, tribes, religions, and era; a resumed run continues *plausibly and reproducibly* (two loads of the same save are identical), though not bit-identically to the unbroken run, because some subsystems re-derive their private RNGs from the world seed at construction.

## Architecture (two layers)

### Micro — the individual (`Entity`)
- **Big Five personality** + attachment style (secure / anxious / avoidant / disorganized) driving behavior and movement.
- **Needs & stats** ticking continuously: hunger, fatigue, hygiene, stress, loneliness, boredom, mental health, happiness, health, plus a food store with metabolism.
- **A unified decision pipeline** (`FreeWillSystem`, subsumption-style): reflex layer → value/goal alignment → habits (reward-modulated, with an entropy floor against rut lock-in) → cognitive deliberation. Deliberation scores candidates through need satisfaction, memory biases (episodic + semantic), social influence, norms, grief, environment, personality — then records a Chain-of-Thought the UI can display, and a hesitation state for morally heavy choices.
- **Perception with an attention budget**: stress narrows how many nearby people an agent actually registers (threats first, then strong bonds) — a panicked agent genuinely acts on less information.
- **Memory that lives and dies**: working memory (5 slots), episodic life memories that decay on an `exp(-age/300)` curve and are pruned once they stop influencing behavior, consolidation of repeated experiences into core beliefs, and a semantic memory index.
- **Theory of mind**: agents build mental models of the people they interact with (accuracy gated by the observer's empathy). Models go stale — an impression from 100 days ago counts for little — and can be *wrong*, especially when acquired secondhand through gossip.
- **Learning**: per-agent Q-learning from outcomes, plus vicarious learning — watching a neighbor's action visibly succeed or fail nudges the observer's own value estimates (prestige- and openness-gated cultural transmission).
- **Relationships**: social bonds, desire, anger, couples — grown from proximity and interaction, decayed over time, Dunbar-capped.
- **Inner life**: grief (Kübler-Ross stages), goals, self-concept, PAD emotional state with body language, a Tree-of-Thoughts daily planner, generated first-person inner monologue.

### Macro — the civilization (`CivilizationEngine`)
- **Tribes** with cultural values, leaders, granaries and division of labour; newborns are enrolled in their parents' tribe, so tribes persist across generational turnover.
- **Religions** founded by prophets in spiritual vacuums (founding is gated by saturation), spread by conversion, and *go extinct* when their congregation scatters.
- **Wars with causes**: relations between tribes erode from concrete grievances — clashing faiths, hunger envy between poor and rich granaries, and blood grievances when a member of one tribe kills a member of another (the micro→macro escalation channel).
- **Justice**: violence has an audience. Witnesses form vendettas, reputations collapse, friendships are cut, and a same-tribe killing can end in exile. Bystanders deter crimes; reputation drives ostracism.
- **Demography**: staggered founder ages and Gompertz old-age mortality (anchored on modal adult death age) produce a realistic death ledger — old age leads, homicide stays under 15%.
- Innovation diffusion + a prerequisite-gated TechTree; diplomacy (stances, treaties, ethnic wars); ID-based kinship registry with incest avoidance; patron-client social order with classes and inheritance.

### World & environment
Procedural planet (noise-based) with biomes, a ResourceSystem, a predator-prey Ecosystem, and an EnvironmentModel driving seasons and harvest luck — climate genuinely gates survival.

## Repository layout

```
src/               engine + simulation sources
src/core/          SimClock (unified time base), SpatialGrid (uniform hash grid)
src/header/        public headers
src/world/         planet, biomes, resources, ecosystem, lexicon
src/validation/    statistical validation framework
scripts/           validate.sh (CI gate), butterfly.py (A/B divergence viewer)
.github/workflows/ CI: build + determinism pair + realism report on every push
ARCHITECTURE.md    system overview (loop, decision pipeline, determinism contract)
plans/MASTER_PLAN.md   the forensic audit + 12-milestone redesign plan (with status)
```

## Performance notes

Neighbor queries run through a spatial hash grid (48 px cells, no O(n²) all-pairs scans); per-entity stat CSVs are buffered in memory and flushed on read/death/exit; log streams don't flush per line. Movement force computation is OpenMP-parallel with snapshot semantics (velocities from current positions, applied in a second serial pass — deterministic because the pass has no RNG and no shared writes). Above 2,000 living agents, deliberation staggers into round-robin cohorts (2, then 4 above 6,000); upkeep still runs every tick for everyone. Sick-avoidance is inverted (few sick agents push repulsion onto neighbors) and the movement id/index lookup maps are cached across frames, invalidated only when the entity set changes.

Measured on a laptop (Release `-O2`, 8 threads, `ASHB_PROFILE=1`, one tick = 60 frames):

| Agents | Sim (decisions+civ) | Movement | Total ms/tick | 1,000-day headless run |
|---|---|---|---|---|
| 1,000 | 220 ms | 20 ms | ~240 ms | ~4 min |
| 5,000 | 597 ms | 229 ms | ~830 ms | ~14 min |
| 10,000 | 746 ms | 758 ms | ~1,500 ms | ~25 min |

The original M11 target (10k agents, 1,000 days, < 10 min) is **not yet met** — honest status: ~25 min, down from ~76 min before the scale pass. The remaining cost is the per-frame force loop itself (10k × 60 frames × neighbor queries); closing the gap would need an SoA hot-data split and/or reduced movement frequency at scale, both of which risk behavioral drift and deserve their own pass. Determinism is verified intact at every step (identical-seed runs produce byte-identical logs after timestamp stripping).
