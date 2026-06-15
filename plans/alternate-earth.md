# Plan: Alternate-Earth Simulator

> **STATUS — IMPLEMENTED (2026-06-13).** All 6 phases built and compiling clean
> (MinGW). New module `src/world/` (Noise, Planet, PlanetView, Lexicon) + `src/WorldSeed.*`.
> Deterministic core verified headless (planet hash stable per seed; per-region
> lexicons distinct & reproducible). Notes on what shifted from the original plan:
> - **Migration** (planned Phase 2) was implemented in Phase 4, where it belongs —
>   it is driven by carrying-capacity overflow.
> - **EnvironmentModel** integration is partial-but-real: its `SeasonalConfig` now
>   drives seasonal famine cycles in carrying capacity (previously 0 call-sites).
>   The full `CulturalTransmissionSystem`/`WorldEnvironment` objects remain unused —
>   cultural drift is instead delivered by the new per-region `Lexicon` + biome-driven
>   tribe-value drift, which achieves the same "similar-but-not-identical" goal with
>   far less glue and risk.
> - The world map + History/Divergence panels render in the **GLFW** ("statistics")
>   mode only; the SDL mode is pure-SDL with no ImGui surface. The world still drives
>   the simulation identically in both modes.
> - **Runtime** long-run verification (collapse/dark-age, cross-run divergence of the
>   history signature) requires running the GUI on a real desktop — it can't run in a
>   headless CI shell.
> - **Replayability is macro-level, not yet bit-exact.** All world/civilisation drivers
>   (terrain, spawn placement, tribe formation, religion/innovation rolls, names, famine)
>   now flow from the master seed. A few *cognitive* subsystems still self-seed from
>   `random_device` (`FreeWillSystem`, `PlanningSystem`, `SemanticMemory`,
>   `LearningAdaptation`). Threading the master seed through their constructors is the
>   remaining work for exact per-entity replay; the run's overall arc is already
>   reproducible.



**Goal:** Turn ASHB2 from a single-city behavioral sim into a *planet* with Earth-like
physics but human-like history that emerges **wildly differently on every run**
(different dominant religions, tech order, empires, collapses).

**Divergence target (user-chosen):** *Wildly different each run.* Same planetary
properties; radically different histories. Replayable from a master seed.

**Build context (verified):** CMake + MinGW, C++17, OpenMP optional. Add new `.cpp`
to the `add_executable(app …)` list in `CMakeLists.txt:79-115`; new source dirs need an
entry in `include_directories(...)` (`CMakeLists.txt:36-49`). **No new external deps** —
noise is hand-rolled. ImGui (GLFW path, `renderingType==1`) and SDL are the two render
backends.

**Key facts the plan relies on:**
- `EnvironmentModel.cpp` is *compiled* (`ENVIRONMENT_SOURCES`, `CMakeLists.txt:71,111`)
  but **never called** from `main.cpp` — integration = wiring, not a build fix.
- The main ImGui visual `UI::DrawGrid` (`UI.cpp:408`) is a **social graph** (circular,
  no spatial coords). Entities carry `posX/posY` used only by the `SpatialMesh` quadtree.
  → The planet map is a **new** panel.
- Spawn loop ends ~`main.cpp:1434`; `globalCivEngine = new CivilizationEngine()` at `:1440`.
- Civ engine ticks at `main.cpp:1160-1161` inside `updateSimulationStep`.
- `Entity` already has `tribeId`, `religionId`, `knownTechIds`, `specialization`, `posX/posY`.

**Guiding principle:** each phase compiles and runs on its own. Geography (Phase 1) and
wiring the dead environment system (Phase 4) are front-loaded — they unlock the most.

---

## Phase 0 — Foundations: master seed + global RNG

**Why first:** "wildly different each run, but replayable" is impossible to retrofit.
Everything random must draw from one seeded source.

**Implement**
- New `src/header/WorldSeed.h` + `src/WorldSeed.cpp`:
  - `struct WorldSeed { uint64_t master; ... };`
  - A global `extern std::mt19937_64 g_rng;` seeded once from `master`.
  - Helper sub-streams so systems don't fight over one generator:
    `std::mt19937_64 makeStream(uint64_t master, uint64_t salt)` (mix with splitmix64).
    Streams: `STREAM_TERRAIN`, `STREAM_CULTURE`, `STREAM_INNOV`, `STREAM_DISEASE`, `STREAM_NAMES`.
  - A **divergence knob** struct (drives "how wild"): `struct DivergenceConfig {
    float butterfly = 1.0f; /*0=tame,2=chaotic*/ float innovationLuck = 1.0f;
    float catastropheRate = 1.0f; float migrationPressure = 1.0f; };`
- Replace ad-hoc `std::random_device{}()` seeding. The civ engine currently seeds from
  `std::random_device` (`CivilizationEngine.h:142`) — change its ctor to accept a seed and
  pull from `makeStream(master, STREAM_INNOV)`.
- Surface `master` + `DivergenceConfig` in a tiny ImGui "World Setup" panel shown at
  startup (text input for seed, sliders for the knobs, "Regenerate" button).

**Where**
- `main.cpp` before the entity spawn loop (`~:1380`) and before `:1440`.

**Verify**
- Same seed + same knobs → identical terrain hash + identical first 10 civ events
  (log them). Different seed → different. `grep -rn "random_device" src/` returns only
  intentional uses (ideally zero in sim logic).

**Anti-patterns**
- Do **not** keep per-system `random_device` seeds — that destroys replayability.
- Do not seed mt19937 from `time(0)`.

---

## Phase 1 — Procedural planet (the core unlock)

**Why:** Real history diverged because separated regions had different climate, resources,
and barriers. Without terrain every run converges. This is the single highest-impact phase.

**Implement** — new module `src/world/` (add dir to `include_directories`):
- `src/world/Noise.h/.cpp` — hand-rolled 2D value/simplex noise + fBm (fractal Brownian
  motion: sum 5–6 octaves). Seeded from `makeStream(master, STREAM_TERRAIN)`. ~120 LOC,
  no deps.
- `src/world/Planet.h/.cpp`:
  ```
  struct Tile {
    float elevation;     // -1..1  (<0 = ocean)
    float temperature;   // derived from latitude + elevation
    float moisture;      // fBm + distance-to-water
    Biome biome;         // enum: OCEAN, COAST, DESERT, GRASSLAND, FOREST,
                         //       JUNGLE, TUNDRA, MOUNTAIN, ICE
    float fertility;     // food potential by biome
    float oreRichness;   // metals (mountains/hills high)
    int   regionId;      // flood-fill landmass / basin id
    bool  river;
  };
  class Planet {
    int W, H;                 // e.g. 256x160 grid
    std::vector<Tile> tiles;
    void generate(const WorldSeed&, const DivergenceConfig&);
    const Tile& at(int x,int y) const;
    // world(grid) <-> existing entity pixel space (posX/posY) mapping
    void gridToWorld(int gx,int gy, float& wx,float& wy) const;
    void worldToGrid(float wx,float wy, int& gx,int& gy) const;
  };
  ```
- Generation steps (classic, well-trodden): elevation fBm → sea level cut → temperature
  from |latitude| with elevation lapse → moisture fBm + ocean proximity → biome lookup
  table (temp×moisture) → rivers by tracing downhill from high-moisture peaks → fertility
  & oreRichness per biome.
- **Regions** via flood-fill on land tiles separated by ocean/mountain → `regionId`. These
  become the isolated cradles of Phase 2.
- Global `extern Planet* g_planet;` created in `main.cpp` before `:1440`.

**Render** — new `src/world/PlanetView.cpp` (ImGui panel "World Map"):
- Draw the tile grid into an `ImDrawList` (use `GetBackgroundDrawList` like
  `UI.cpp:410`), one filled rect per tile, color by biome. Overlay rivers, region borders,
  and entity dots (map `posX/posY` → screen via `worldToGrid`/pan-zoom). This is **new UI**,
  parallel to the social `DrawGrid` — do not modify the graph.

**Verify**
- Panel shows continents/oceans/mountains/rivers; visibly different per seed.
- `regionId` count ≥ 3 separated landmasses on a typical seed (log it).
- Same seed → pixel-identical map.

**Anti-patterns**
- Don't pull in a noise library; keep it in-repo and seeded.
- Don't store terrain in globals scattered across files — one `Planet` owns it.

---

## Phase 2 — Multiple isolated populations + migration/contact

**Why:** Geographic isolation → independent language/religion/tech evolution → divergence,
then migration & trade re-mix them. This is *the* Earth divergence mechanism.

**Implement**
- At spawn (`main.cpp:~1380-1434`): instead of one blob, place N starting bands, one per
  habitable `regionId`, each near fertile/coastal tiles. Stamp `Entity` with a new field
  `int originRegionId`.
- Extend `CivilizationEngine`:
  - Tribes already have `centerX/centerY` (`CivilizationEngine.h:65-66`). Make
    `updateTribeCenter` pull toward high-`fertility`/`oreRichness` tiles and away from rival
    tribe centers (query `g_planet`).
  - **Migration**: when local carrying capacity is exceeded (Phase 4) or after war, split a
    band and walk it along passable tiles (avoid OCEAN unless "seafaring" innovation known;
    avoid high MOUNTAIN) toward open fertile land.
  - **Contact/trade**: when two tribes' centers come within range, exchange a fraction of
    `knownTechIds` and cultural traits (Phase 4) and adjust `relations`. Distance + barriers
    gate the exchange rate → far regions stay alien longer.
- Biome modifies tribe values: e.g. harsh tundra/desert → higher `militarism`; rich river
  valley → faster `innovation`. (Tribe value drift already exists in `updateTribeValues`.)

**Verify**
- Log per-region: dominant religion id, top-3 techs, tribe count over time. After ~N ticks,
  regions show **different** dominant religions/tech sets (the divergence signal).
- A seafaring innovation is required before any cross-ocean settlement appears in the log.

**Anti-patterns**
- Don't let tribes teleport across oceans/mountains — movement must respect `Tile` passability.

---

## Phase 3 — Region-specific procedural language & names that drift

**Why:** Cheap, huge payoff for the "alien but familiar" feel. Replaces shared global pools.

**Implement**
- New `src/world/Lexicon.h/.cpp`: per-region phonotactics (syllable inventory chosen from
  `STREAM_NAMES` at region creation) → `genName(regionId, sex)`, `genTribeName`,
  `genReligionName`, `genPlaceName`.
- Replace the global `male_name`/`female_name` lists (`Entity.h:19-44`) usage at naming
  sites with `Lexicon::genName(ent.originRegionId, ent.entitySex)`. (Keep the old arrays only
  as a fallback if `originRegionId < 0`.)
- **Drift**: every M ticks, perturb a region's syllable weights slightly (mutation), and on
  tribe schism/conquest, blend lexicons → languages diverge and creolize over time.

**Verify**
- Names from region A are phonetically distinguishable from region B (eyeball the log).
- Same seed → same names. Over a long run, a conquered region's names show blending.

**Anti-patterns**
- Don't hardcode new fixed name lists — generation must be seeded & per-region.

---

## Phase 4 — Wire up the dead EnvironmentModel + Malthusian feedback

**Why:** `CulturalTransmissionSystem` (with `mutationRate`) is literally the engine for
"similar but not identical to humanity"; `ResourceManager` + feedback give collapse/dark
ages — the non-linear shape of real history. It's **already written**, just never called.

**Implement** (integration, minimal new logic)
- Instantiate one `environment::WorldEnvironment` (or its parts) globally; populate
  `ResourceManager` from `g_planet` tiles (FOOD∝fertility, MATERIALS∝oreRichness,
  WATER∝rivers/coast). Seed resource positions from tile centers.
- Tick it inside `updateSimulationStep` near the civ tick (`main.cpp:1160`): call
  `WorldEnvironment::tick` / `advanceDay`, and the feedback loops
  (`setupPopulationPressureLoop`, `setupResourceDepletionLoop`, `setupClimateFeedbackLoop`,
  all defined in `EnvironmentModel.h:251-253`).
- **Cultural transmission**: register each tribe as a `CulturalGroup`; on birth call
  `transmitVertically(parent,child)`; on interaction `transmitHorizontally`; elders →
  `transmitObliquely`. Mutation already in the trait model → drift for free. Feed adopted
  traits back into tribe values / religion doctrine.
- **Carrying capacity / Malthus**: per region, `K = Σ fertility (× tech multiplier)`.
  Population > K → rising starvation: lower `entityHealth`, raise `entityStress`, raise death
  & migration & war probability. Population crash → tech/innovation can be *lost* (drop low-
  knowerCount innovations) → **dark age**, then recovery. Scale severity by
  `DivergenceConfig.catastropheRate`.
- Optional flavor: route the existing `Disease` system through region density + climate so
  plagues hit dense regions (ties to the May-26 disease-region note).

**Verify**
- A run produces at least one visible **collapse then recovery** in the population/tech log.
- Disable transmission → cultures stop drifting (proves it's wired).
- Two isolated regions accumulate **different** `CulturalTrait` sets.

**Anti-patterns**
- Don't rewrite `EnvironmentModel.*` — call the existing API. If a method is missing, add a
  thin adapter, don't fork the system.
- Don't double-own resources: `ResourceManager` is the source of truth, not new globals.

---

## Phase 5 — Divergence amplifiers + observability

**Why:** Make the wildness legible and tunable, and prove runs really diverge.

**Implement**
- Wire `DivergenceConfig` knobs into the probabilities they name: `innovationLuck` →
  discovery roll in `CivilizationEngine::updateInnovations`; `catastropheRate` → plague/famine/
  collapse odds; `migrationPressure` → split/migrate thresholds; `butterfly` → magnitude of
  all per-tick random perturbations.
- **History panel** (new ImGui window): timeline of `CivEvent`s per region, current era,
  dominant religion/tech, population & a "tech level" line per region (reuse `implot`).
- **Run fingerprint**: at fixed checkpoints, hash (dominant religion ids, era, top techs,
  population per region) → a short "history signature" string. Two seeds → different
  signatures = objective proof of divergence. Log to `data/`.

**Verify**
- Sweep 5 seeds, same knobs: 5 distinct history signatures, distinct dominant religions/
  tech orders.
- Raising `butterfly`/`catastropheRate` visibly increases run-to-run variance.

---

## Final Phase — Verification & integration sweep

1. **Replayability:** same seed+knobs twice → identical terrain hash, identical history
   signature. (Core contract.)
2. **Divergence:** 5 seeds → 5 distinct signatures; eyeball maps + dominant religions differ.
3. **No dead wiring:** `grep -rn "WorldEnvironment\|CulturalTransmission\|ResourceManager\|g_planet\|environment::" src/main.cpp` now returns matches (was 0).
4. **Seeding hygiene:** `grep -rn "random_device\|time(0)\|time(NULL)" src/` — none in sim logic.
5. **Build:** clean CMake reconfigure + build on MinGW; run both render paths (ImGui & SDL)
   without crash; load/save round-trips with new fields (`Entity::saveTo/loadFrom`,
   `SaveLoad.cpp`).
6. **Performance:** 256×160 planet + N regions ticks at acceptable FPS (quadtree already
   handles entity proximity; terrain is static after gen).

---

## Suggested order & checkpoints
`Phase 0 → 1` first (you can *see* a planet and prove replay). Then `4` (collapse/drift
make it feel alive) → `2` (regions truly diverge) → `3` (names sell it) → `5` (tune the
wildness). Each phase is a safe commit point.

## Files touched (summary)
- **New:** `src/header/WorldSeed.h`, `src/WorldSeed.cpp`, `src/world/{Noise,Planet,PlanetView,Lexicon}.{h,cpp}`.
- **Edited:** `CMakeLists.txt` (sources + include dir), `main.cpp` (seed panel, world gen,
  region spawn, env tick), `CivilizationEngine.{h,cpp}` (seeded RNG, terrain-aware tribes,
  migration, capacity), `Entity.h` (`originRegionId`), `UI.cpp` (+History panel),
  `SaveLoad.cpp` (new fields). **Wired, not rewritten:** `environment/EnvironmentModel.*`.
