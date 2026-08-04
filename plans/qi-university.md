# QI Plan — Intelligence, Universities, and What a Clever People Can Do

> **STATUS: implemented.** Phases 1-8 are in the tree. What follows is the plan as
> written; the deviations forced by contact with the codebase are recorded in
> **[As built](#as-built--where-the-plan-was-wrong)** at the end. Read that section
> before trusting a number in the body.


**Goal**: give every entity a QI (quotient intellectuel) that is *born, raised, and spent*, make
the **University** the building that raises it, and let a tribe's collective QI pay off in exactly
the three places asked for:

1. **Wars** — smarter peoples fight better and bleed less.
2. **Technology** — smarter peoples climb the tech tree and invent faster.
3. **Every kind of growth** — food, economy, culture, health, administration.

**The loop we are building**:
universities cost treasury + granary → they school the young → schooling lifts realized QI toward
its inherited ceiling → tribe mean/elite QI rises → research, war and growth multipliers rise →
richer tribe affords more universities → **but** educated peoples put fewer hands in the fields,
marry later and have fewer children, and a lost war or a famine burns the schools down. So the
curve bends instead of exploding.

Each phase is self-contained and executable in a fresh context. Execute in order.

---

## Phase 0 — Allowed APIs & anti-patterns (READ FIRST, every phase)

### Existing systems this plan builds on (verified)

| System | Where | Key facts |
|---|---|---|
| Building blueprints | `src/header/Building.h:33-46` | `University` = `{HOUSING_AND_CIVILIAN, health 115, price 7000, maint 1500, maxLevel 5}`, comment is literally `//see todo` |
| Building ops | `src/Building.cpp`, called from `CivilizationEngine.cpp:418-428` | monthly beat (`day % 30 == 0`): `maintenance_costs` → `upgrade` → `applyBuildingEffects` |
| Building buffs | `CivilizationEngine::applyBuildingEffects` `CivilizationEngine.cpp:803-883` | `universityEffect` currently only does `tribe.innovation += universityEffect * 5.0f` (cpp:875) |
| Tribe | `src/header/CivilizationEngine.h:119-330` | `memberIds`, `economy`, `granary`, `knowledgeStock`, `researchPoints`, `innovation`, `cultureScore`, `specialistCount`, `settlementTier`, `buildings_owned` (h:141) |
| Tech tree | `src/header/TechTree.h`, `src/TechTree.cpp:77-120` | research gain = `(pop*0.5 + scholars*2.5 + innovation*0.04) * researchMultiplier(tribe)`; unlock costs research points **and** granary food |
| Emergent innovation | `CivilizationEngine::researchClimate` cpp:164-196, call site cpp:1704-1707 | multiplier on how often anyone has an idea; already knows about scholars, literacy, `InstitutionType::EDUCATION`, Printing, Scientific Method |
| Military strength | `calculateTribeMilitaryStrength` cpp:6526-6551 | per-member combat from health/agreeableness/conscientiousness/anger, ×1.6 warrior, + military innovations, × `TechTreeSystem::militaryMultiplier`, + `weaponAttackStrength` |
| Defense strength | `calculateTribeDefenseStrength` cpp:6553-6565 | pop + collectivism + fortification innovations, × `defenseMultiplier`, + `weaponDefenseStrength` |
| War decisions / battles | war-declaration strength compare cpp:5447-5455; battle resolution cpp:5837-5850 and cpp:5974+ | both sides' strengths already computed through the two functions above |
| Specialists | `Entity::isSpecialist`, `Entity::specialization` (`Entity.h:715`) | assignment in `updateDivisionOfLabour` (`CivilizationEngine.cpp:2549-2800`), talent match at cpp:2729-2734 uses max personality trait |
| Personality | `Entity::personality` Big Five, `Entity.h:187-202` | `openness` is the closest existing proxy for intellect — QI is **new and separate** |
| Birth / inheritance | `implem_free_will.cpp:2398-2409` | midparent averaging of `openness`/`extraversion`, then `inheritEmergenceTraits(baby, pointer, pointed)` |
| Entity save/load | `Entity::saveTo`/`loadFrom`, `Entity.cpp:~365-600` | line-based `KEY:value`, **strictly positional** |
| Save (world) | `src/SaveLoad.cpp:1080-1082` | JSON snapshot writes personality as `"c"`, `"o"`… |
| DB export | `src/DbExport.cpp:250-290` | `kF(o, "openness", e.personality.openness)` field-emission style; schema `website/sql/schema_pg.sql` |
| LiveConfig | `src/header/LiveConfig.h:11-58`, registry `src/main.cpp:84-86` | every feature gets one multiplier; `X = 0.0f` **must** be bit-exact pre-feature behaviour |
| UI | `src/UI.cpp` CIVILIZATION panel / TRIBES section; MIND BOARD entity inspector | where tribe and entity stats are shown |

### Anti-patterns (DO NOT)

- **Do not accumulate QI effects into tribe fields.** `applyBuildingEffects` (cpp:803) *adds* to
  `tribe.innovation` every 30 days, so it saturates at 100 and can never fall. QI must be
  **recomputed from the members every civ-day** and consumed through **pure multiplier functions**
  (`QISystem::researchMul(tribe)`), exactly the way `TechTreeSystem::militaryMultiplier` is used.
- **Do not reuse `personality.openness` as intelligence.** It already drives variety-seeking,
  scholar selection and tribe naming; overloading it would silently change all three.
- **One read site per effect.** Every multiplier is applied at exactly one place, so
  `--set qiMul=0` reproduces today's world bit for bit.
- **Serialization is positional**: append new `KEY:` lines at the END of `saveTo`, read them LAST
  in `loadFrom` behind a presence guard. Never insert in the middle.
- **Determinism**: use the engine's own `rng` (`CivilizationEngine.h`, `std::mt19937_64`), never `rand()`.
- **No new building.** The University blueprint already exists — give it teeth, don't add "School".
- **No unbounded multipliers.** Every QI multiplier is clamped; see the table in Phase 3.

---

## Phase 1 — The stat: QI on the entity

### What to implement

1. **Three fields on `Entity`** (append next to `specialization`, `Entity.h:715`):
   ```cpp
   // ── QI (quotient intellectuel) ───────────────────────────────────────────
   // `qiPotential` is what this person could have become — set at conception from
   // the parents and never changed afterwards. `qi` is what they actually became:
   // it starts well below potential and only closes the gap if childhood feeds,
   // shelters and teaches them. Schooling is the largest single lever, which is
   // why a University is worth 7000 tokens.
   float qiPotential = 100.0f;  // genetic ceiling, ~N(100,15), clamped 55-145
   float qi          = 100.0f;  // realized, 40-160
   float schoolYears = 0.0f;    // years of formal education actually received
   ```
2. **Founding generation**: wherever the first entities are built, draw
   `qiPotential = clamp(normal(100, 15), 55, 145)` from the engine rng and set `qi = qiPotential * 0.75f`
   (an unschooled world sits below its own ceiling — that headroom is what universities later buy).
3. **Inheritance at birth** — in `implem_free_will.cpp:2398-2404`, next to the openness/extraversion
   averaging, before `inheritEmergenceTraits`:
   ```cpp
   float midparent = 0.5f * (pointer->qiPotential + pointed->qiPotential);
   baby.qiPotential = clamp(0.6f * midparent + 0.4f * 100.0f + noise(0, 8), 55, 145);
   baby.qi = 60.0f;                 // an infant has not become anything yet
   baby.schoolYears = 0.0f;
   ```
   Heritability 0.6 with regression to the population mean is what stops dynasties of geniuses.
4. **Development curve** — one new method `Entity::developQI(float nutrition01, float schoolQuality01)`
   called from the same per-day place `IncrementBDay`/ageing runs:
   - **Ages 0-16**: `qi` moves toward `qiPotential * envFactor` at ~6%/year, where
     `envFactor = 0.70 + 0.15*nutrition01 + 0.15*schoolQuality01 - 0.10*(childhoodTraumaScore/100)`
     — reuse `dv.childhoodTraumaScore` (already on Entity) and famine/health state for nutrition.
   - **Ages 16-25**: `qi` still rises with `schoolYears` (each schooled year ≈ +1.2 QI, decaying),
     capped at `qiPotential * 1.05`.
   - **Adults**: frozen except a small bonus for scholars/craftsmen actually practising.
   - **Elders**: `-0.3/year` past 60, doubled if `entityHealth < 40`.
   - Clamp to [40, 160] in the same place personality is clamped (`Entity.cpp:1387-1391`).
5. **Persistence & export**:
   - `Entity::saveTo` → append `QI:<qi>,<qiPotential>,<schoolYears>` **last**; `loadFrom` reads it
     last behind a presence guard so old saves still load (defaults stand).
   - `SaveLoad.cpp:1080` JSON snapshot → add `"qi"`, `"qip"`, `"sy"`.
   - `DbExport.cpp:268` → `kF(o, "qi", e.qi); kF(o, "qi_potential", e.qiPotential); kF(o, "school_years", e.schoolYears);`
     plus three columns in `website/sql/schema_pg.sql` (`qi real`, `qi_potential real`, `school_years real`).
6. **LiveConfig knob**: `float qiMul = 1.0f;` in `LiveConfig.h` + registry entry in `main.cpp:84`.
   Everything from Phase 3 onward is gated on it.

### Acceptance
Headless run: QI mean stays ≈100 ± 3 over 200 years with no universities built; distribution stays
roughly normal (no drift to the clamp); `--set qiMul=0` is bit-identical to `main` today.

---

## Phase 2 — The University: turning tokens into educated people

### What to implement

1. **Tribe-level derived fields** (append to `struct Tribe`, `CivilizationEngine.h`, copy the
   comment style of the resource-economy block h:190-198). All three are **recomputed, never accumulated**:
   ```cpp
   // ── QI: what this people can think with ──────────────────────────────────
   float meanQI      = 100.0f;  // living adults
   float eliteQI     = 100.0f;  // mean of the top decile (its generals and scholars)
   float schoolQuality = 0.0f;  // 0-1, what a University here is actually worth
   int   schooledCount = 0;     // members with schoolYears >= 4 (for the UI/chronicle)
   ```
2. **New pass `updateEducation(Tribe&, std::vector<Entity>&, int day)`**: declare next to
   `applyBuildingEffects` (`CivilizationEngine.h:804`), define next to it (cpp:803), call it from the
   per-tribe loop at **cpp:388-412** (every civ-day, not the monthly block — schooling is continuous).
   - **Capacity**: `seats = 10 * Σ(university.level)` across `buildings_owned`.
   - **Quality** gates on whether the tribe can actually run the place:
     ```
     schoolQuality = clamp( 0.20*Σlevel/5              // how much university there is
                          + 0.30*min(1, granary/(pop*8)) // can it feed students who don't farm
                          + 0.25*literacyShare           // tribeIsLiterate / knownTechName "Writing"
                          + 0.25*min(1, treasury/(seats*400.0)), 0, 1)
     ```
     Zero universities → `schoolQuality = 0` and this pass does nothing (the neutral path).
   - **Enrolment**: rank members aged 6-22 by (family wealth + parent `schoolYears` + a small random
     term) and admit up to `seats`. That bias is deliberate — it makes education hereditary in
     *practice*, feeding the existing class/cultural-capital system, and gives reformist governments
     something to fix later.
   - **Charge for it**: `tribe.economy.spendMoney(enrolled * 120)` per civ-day-month equivalent and
     `granary -= enrolled * 0.6f` (students eat but do not farm). If the tribe cannot pay, drop
     enrolment to what it can afford — poverty closes schools, which is the brake on runaway QI.
   - **Effect per enrolled student**: `schoolYears += 1/365 * civDaysPerTick`, and pass
     `schoolQuality` into `developQI` for that entity.
   - Students count against the labour pool: mark them `isStudent` (new `bool` on Entity) and have
     `updateDivisionOfLabour` (cpp:2549+) skip them for specialist promotion until they finish.
3. **Recompute `meanQI` / `eliteQI` / `schooledCount`** at the top of the same pass from living
   adult members (age ≥ 16). Empty tribe → leave at 100 (neutral).
4. **Scholar pipeline**: in the talent match at **cpp:2729-2734**, add QI as the dominant term for the
   scholar role — `qi + schoolYears*3` beats raw `openness` for choosing scholars. A university town
   should produce scholars, not just clever farmers.
5. **Rewrite the University branch of `applyBuildingEffects` (cpp:853-856)**: keep it modest and
   *non-accumulating* — remove the `tribe.innovation +=` line (it saturates) and instead let Phase 3-5
   read `meanQI`. The building's job is now schooling, not a stat bump.
6. **Chronicle**: `logEvent` when a tribe founds its first university, when `schooledCount` first
   passes 25% of adults ("a lettered generation"), and when war or bankruptcy closes the schools.

### Acceptance
A tribe that builds a University and can feed it shows `meanQI` climbing ~+5 to +12 over three
generations, then plateauing (potential is the ceiling). A tribe that builds one and goes bankrupt
shows `schoolQuality → 0` and `meanQI` sliding back down within two generations.

---

## Phase 3 — The multipliers (one place, clamped, killable)

New tiny module `src/QISystem.cpp` + `src/header/QISystem.h`, namespaced free functions, mirroring
`TechTreeSystem` exactly (`TechTree.h:42-67`):

```cpp
namespace QISystem {
    float researchMul(const Tribe& t);  // Phase 4
    float warMul(const Tribe& t);       // Phase 5 (offense)
    float defenseMul(const Tribe& t);   // Phase 5
    float growthMul(const Tribe& t);    // Phase 6
    std::string summary(const Tribe& t);// one line for the UI
}
```

Shape of every one of them (write it once as a helper):

```cpp
static float mul(float base, float eliteWeight, const Tribe& t, float lo, float hi) {
    if (g_liveConfig.qiMul == 0.0f) return 1.0f;          // bit-exact kill switch
    float m = (t.meanQI - 100.0f) / 100.0f * (1.0f - eliteWeight)
            + (t.eliteQI - 100.0f) / 100.0f * eliteWeight;
    return std::clamp(1.0f + base * m * g_liveConfig.qiMul, lo, hi);
}
```

| Function | base | eliteWeight | clamp | Rationale |
|---|---|---|---|---|
| `researchMul` | 2.0 | 0.5 | [0.60, 2.50] | thinking is the most QI-elastic thing a society does |
| `warMul` | 1.2 | 0.7 | [0.80, 1.45] | generalship matters more than the average soldier's wits |
| `defenseMul` | 1.0 | 0.6 | [0.85, 1.35] | siegecraft, walls, logistics |
| `growthMul` | 0.8 | 0.3 | [0.90, 1.25] | broad but shallow — this one touches everything, so it must be gentle |

**Nothing else in the codebase may read `meanQI` directly.** That is what keeps the feature auditable.

---

## Phase 4 — Faster technologies

1. **Tech tree** (`TechTree.cpp:100-102`): research gain becomes
   `gain = (pop*0.5 + scholars*2.5 + innovation*0.04) * researchMultiplier(tribe) * QISystem::researchMul(tribe)`.
2. **Unlock cost relief**: in the affordability check just below (cpp:104-120), compare against
   `n.knowledgeCost / sqrt(QISystem::researchMul(tribe))` — a clever people not only earns points
   faster, it needs fewer of them to see the point. `sqrt` so the two effects don't compound to absurdity.
3. **Emergent innovation** (`researchClimate`, cpp:176-195): add one term inside the existing
   `knowledgeMul` gate, using the *individual's* QI rather than the tribe's — this is where a single
   genius should matter:
   ```cpp
   m += std::clamp((ent.qi - 100.0f) / 100.0f, -0.4f, 0.8f) * g_liveConfig.qiMul;
   ```
4. **Diffusion**: in `spreadInnovations`, let a high-QI receiver pick up a neighbour's technique on a
   weaker contact (scale the adoption probability by `clamp(qi/100, 0.7, 1.3)`). Learning is half of intelligence.

### Acceptance
Two identical seeds, one with universities forced on: the schooled world should reach a given tech
tier noticeably earlier (target ~15-30% fewer years), not 5× earlier.

---

## Phase 5 — Help win wars

1. **Offense** (`calculateTribeMilitaryStrength`, cpp:6546, immediately after the tech multiplier):
   `strength *= QISystem::warMul(tribe);`
2. **Defense** (`calculateTribeDefenseStrength`, cpp:6561, after `defenseMultiplier`):
   `defense *= QISystem::defenseMul(tribe);`
3. **Casualties** — the payoff the idea actually asks for ("less death of wars"). Wherever battle
   losses are applied (battle resolution cpp:5837-5850 / cpp:5974+), scale each side's own losses by
   `1.0f / QISystem::defenseMul(side)` (clamped ≥ 0.7). A smarter army wins the same battles with
   fewer funerals; this interacts with `warExhaustion` so clever peoples can sustain longer wars.
4. **Not starting hopeless wars** (declaration check cpp:5447-5455): today a tribe compares raw
   strengths. Give the estimate an error term that *shrinks* with `eliteQI`:
   `perceivedFoe = foe * (1 + noise(0, 0.35 * clamp((130 - eliteQI)/30, 0.2, 1.5)))`.
   A dull people misjudges its enemy and marches to its death; a clever one sues for peace or never
   declares. This is the single most historically legible effect in the whole plan.
5. **Spoils of conquest — brain drain**: when a tribe is conquered or absorbed
   (`absorbEntityIntoTribe`, cpp:894), the victor keeps the schooled: log an event when an absorbed
   member has `schoolYears >= 4`. Conquering a university town should be worth something.

### Acceptance
In a scripted matchup of two tribes with equal population/tech, the one with `meanQI 115` should win
clearly more than half of engagements and take visibly fewer casualties — but a 20-point QI edge must
**not** beat a 2× population edge.

---

## Phase 6 — Growth on every kind of point

Apply `QISystem::growthMul(tribe)` at exactly **one** site per category (list them in the header
comment of QISystem.h so the audit is one grep):

| Point type | Site | Effect |
|---|---|---|
| Food | granary deposit in `updateDivisionOfLabour` (farmer output, cpp:2751-2766 region) | better tools, rotation, storage |
| Economy | `collectTaxes` / trade wealth accrual | administration and commerce |
| Culture | `cultureScore` accrual in the culture pass | literacy and patronage |
| Health | `tribe.childSurvival` in `applyBuildingEffects` (cpp:861) — multiply the hospital term | medicine is knowledge applied |
| Governance | `corruption` decay rate and institution `efficiency` | competent administration, not honest administration |
| Settlement | `settlementTier` growth threshold (III-P1) | cities need engineers |

Deliberately **not** boosted by QI: fertility, happiness, military manpower. Intelligence should buy
capability, not everything at once — and the fertility exclusion is what makes Phase 7's brake bite.

---

## Phase 7 — The brakes (do not skip this phase)

Without these the loop is a runaway and the world ends with one omniscient super-tribe.

1. **Demographic transition**: hook the existing `laborMul` transition — an adult with
   `schoolYears >= 6` marries later and desires fewer children (scale the conception threshold in
   `implem_free_will.cpp` by `1 + 0.04f * schoolYears`, gated on `qiMul`).
2. **Opportunity cost**: students don't farm, don't fight, and the treasury pays 1500/level upkeep
   monthly (already true via `maintenance_costs`) — verify a tribe that over-builds universities
   actually starves. That negative case must be observable.
3. **Regression to the mean** is already in Phase 1's inheritance — verify empirically that elite
   families' `qiPotential` drifts back toward 100 over ~5 generations.
4. **Dark ages**: on a lost war, a strife event (`instability` discharge) or famine, damage
   `buildings_owned` university levels and zero `schoolQuality` for a period. Knowledge that is not
   institutionally maintained is *lost* — `meanQI` must be able to fall, or this isn't a simulation.
5. **Cap the compounding**: assert in a debug build that no tribe's product of
   `researchMul * growthMul * warMul` exceeds ~4.0.

---

## Phase 8 — Surfacing & verification

1. **UI**: TRIBES section — add `QI 108 (elite 121) · 34% schooled` line via `QISystem::summary`.
   MIND BOARD entity inspector — `QI 112 / potential 118 · 7 school-years`.
2. **Web**: `website/entity.php` and the dashboard show QI next to personality; `website/world.php`
   ranks tribes by mean QI.
3. **Headless verification runs** (`scripts/run_sim_to_db.py` / the existing sweep script):
   - `--set qiMul=0` → **bit-identical** to pre-feature `main`. Non-negotiable.
   - 500-year run: report mean QI over time, universities built, years-to-tier-N tech, war
     casualty ratios by QI gap, and the count of tribes that went bankrupt on schooling.
   - A/B: `qiMul=1` vs `qiMul=0` divergence report, same seed.
4. **Docs**: extend `BUILDING_BUFFS_SUMMARY.md` with the University's real behaviour (it currently
   documents the placeholder `+innovation` bump) and add a QI section to `ARCHITECTURE.md`.

---

## As built — where the plan was wrong

Six things the plan got wrong, found by building it. Each one is load-bearing.

1. **`Building::build` had no callers.** Nothing in the codebase ever constructed a
   building, so `buildings_owned` was permanently empty and *every* building effect —
   including the eleven buffs implemented in the previous session — was unreachable
   dead code. A university plan on top of that would have been inert. Added
   `CivilizationEngine::considerConstruction()` (monthly, need-scored, keeps two
   months of upkeep in reserve) behind its own kill switch `buildMul`. This is a
   separate feature from QI and is why the neutrality check needs **both**
   `--set qiMul=0 --set buildMul=0` to reproduce the pre-change world.

2. **An unschooled people does not sit at QI 100 — it sits at ~80.** The development
   curve floors an untaught childhood at ~0.79 of its inherited ceiling, which is the
   point of the mechanic. Centring the multipliers on 100 (as the plan said) would
   have handed every pre-modern tribe a permanent penalty on *food*, which feeds
   nutrition, which feeds QI: a death spiral. Measured empirically over an unschooled
   900-tick world and centred there instead.

3. **Mean and elite need different baselines.** The top decile of any distribution
   sits well above its mean — measured, 91.5 against 80.5. Scoring both against one
   constant gave every unschooled tribe a standing ~7% war bonus it had not earned.
   `kUnschooledMean` and `kUnschooledElite` are now separate.

4. **The war-aversion rule abolished war.** Setting the stance back to `TS_RIVAL`
   without changing relations meant the same pair re-averted the same war every
   civ-day forever: 164 "averted wars" that were one war refused 164 times, and
   `conquests` fell 36 → 4 with zero war deaths in the whole run. Backing down now
   costs the yielding tribe 10% of its treasury and buys +12 relations, so the pair
   drifts back above the war line and the concession has a price.

5. **The fertility brake was an extinction event.** At 0.04 per school-year, stacked
   on the wealth/status/urbanisation terms already in `fertilityModifier`, a schooled
   world died out inside three centuries. Now 0.013 with a 0.80 floor (~20% fewer
   children for a fully schooled couple), and `schoolYears` is capped at 16 — the
   uncapped version banked 20 years on anyone who sat in a school from 6 to 26.

6. **Schooling must be paid out of surplus, not out of the granary.** At 0.6 food per
   seat per civ-day charged against the bare granary, universities starved their own
   tribe to death. Now 0.15 per seat and only what is left above `pop * 2`.

## Verification results (Phase 8.3)

Run 2026-08-04 against the tree at that date. Binary rebuilt first: `CivilizationEngine.cpp`
had been edited *after* the previous session's last build, so every `on2_*` log from
that session was produced by a stale binary.

### The method was wrong before the results were

`main.cpp:3311-3325` and `src/util/clear.h` delete and rewrite `./src/data/` **relative to
the process CWD** at startup. The previous session ran `on2_s1`, `on2_s2` and `on2_s3`
concurrently in one directory, so each run wiped and interleaved the others' logs. That —
not a regression — is why `on2_s1` (pop 46) contradicted `on_s1` (pop 128) on an identical
seed, and why `on2_s3` appeared to die *earlier* than `on_s3` after the clamp fix that was
supposed to save it. **Those logs are void.** Every run below was given a private working
directory; two parallel runs on one seed were confirmed byte-identical first, so the sim is
deterministic and the isolation holds.

### A/B, 2500 ticks, three seeds — `on` = defaults, `off` = `--set qiMul=0 --set buildMul=0`

| seed | pop off → on | techs off → on | realism PASS off → on | universities built |
|---|---|---|---|---|
| s1 | 8 → **151** | 30 → **64** | 17 → **25**/27 | 23 (2 ruined) |
| s2 | 8 → **0** (extinct t2385) | 24 → **9** | 12 → **10**/27 | 1 |
| s3 | 65 → **40** | 50 → 48 | 19 → 18/27 | 15 |

**QI is not yet a clean win.** It is strongly positive on s1, fatal on s2, and slightly
negative on s3. Three seeds cannot separate that from noise — s2 and s3's `off` arms are
themselves near-dead worlds (pop 8 and 65), so the extinction may be seed sickness rather
than QI. More seeds are needed before the feature can be called good.

### The `off` arm is much sicker than pre-feature `main`, and QI is not the cause

Running HEAD `c6ced5e` on the same three seeds finished at populations **151 / 391 / 166**.
The current tree's *neutral* arm — QI and construction both switched off — finished at
**8 / 8 / 65**.

| seed | HEAD `c6ced5e` | current `off` | current `on` |
|---|---|---|---|
| s1 | 151 | 8 | 151 |
| s2 | 391 | 8 | 0 |
| s3 | 166 | 65 | 40 |

These are different planets (see below), so this is not a controlled comparison and no single
number should be read literally. But the direction is the same on all three seeds and the
magnitude is large, and the `off` arm has **every** QI and building code path disabled — so
whatever is depressing it lives in the *other* uncommitted work, not in this feature. The
`src/world/Planet.cpp` worldgen change is the prime suspect (it roughly doubles the number of
habitable regions and pushes far more terrain above water, scattering a fixed starting
population across more, smaller basins); the ungated grief/anger/fertility edits in
`implem_free_will.cpp` are the second.

This reframes the A/B table above: on s1, QI-on does not merely beat QI-off, it restores the
world to exactly the population pre-feature `main` reached. Before QI is judged on these
numbers, the non-QI regression underneath it needs to be found and fixed — otherwise the
feature is being scored on how well it compensates for someone else's bug.

### What passed

- **Phase 2 acceptance — met.** `meanQI` climbs 80 → 92.7 (s1) and 81 → 91.0 (s3), i.e.
  +11 to +12 over the run, then plateaus against the inherited ceiling. Exactly the
  "+5 to +12, then plateau" the phase asked for.
- **Realism line 27 (schooling makes minds)** — schooled/unschooled QI gap of **14** (s1)
  and **10** (s3); 91/145 and 32/40 adults schooled.
- **Phase 7.4 dark ages fire.** Two `university_ruined` events on s1: schools can be lost.
- **"As built" #4 (war-aversion) is genuinely fixed.** 185 aversions on s1 spread across
  **75 distinct tribe pairs** (max 7 for any one pair) — not the old 164-refusals-of-one-war.
  War still happens around it: 25.8 wars/1500d, 8 conquests, 15 war dead.
- **Kill-switch gating audit — clean.** Every `QISystem::*Mul()` returns exactly `1.0f`
  under `qiMul=0` (a bit-exact multiplicative no-op); `misjudgement()`'s whole block is
  gated at cpp:2384; the two direct `qi` reads are gated at cpp:2173 and scaled by
  `qiMul` at cpp:201/3052; `updateEducation` and `developQI` early-return; `considerConstruction`
  early-returns on `buildMul=0`. `rollQIPotential` is called unconditionally but draws from
  the private `qiRng()` stream (`Entity.cpp:32`), so it cannot shift the shared `rng`.
  Empirically, the `off` arm builds nothing and schools nobody.

### What could not be verified, and why

- **The neutrality contract cannot be tested against `main` as written.** `--set qiMul=0
  --set buildMul=0` is *not* bit-identical to HEAD `c6ced5e` — but the divergence is at
  **worldgen, before any entity acts**: same seed, `hash=16015618117808332344
  habitable_regions=2` on HEAD against `hash=2610816390138884558 habitable_regions=5` on the
  current tree. The cause is the uncommitted `src/world/Planet.cpp` change (continent scale
  2.6-4.4 → 4.0-6.0, sea level -0.10 → -0.20, habitability threshold /700 → /2800), which is
  gated by no knob at all. While unrelated uncommitted work sits in the tree, this test can
  only ever fail, and its failure says nothing about QI. It needs a baseline commit that
  contains everything *except* the QI/building work.
- **Phase 7.5's compounding assert has never executed.** Release builds set `-O3 -DNDEBUG`
  (`CMakeCache.txt:57`), so the `#ifndef NDEBUG` guard at cpp:1015 is compiled out.
  Recomputed offline over all 1180 samples the max observed
  `researchMul * warMul * growthMul` is **2.065** — comfortably safe. But the clamps permit
  `2.50 × 1.45 × 1.25 = 4.53`, so the ≤4.0 invariant is **not structurally guaranteed**: it
  trips once `meanQI` reaches ~142 (`qi` is clamped at 160, so that is reachable). Either
  lower a clamp or raise the assert's threshold to 4.6 and stop calling it a cap.
- **Phase 4's "15-30% fewer years to a tech tier" is unconfirmed.** s1 more than doubles the
  tech count (30 → 64) while s3 is flat (50 → 48). Population differs by 19× on s1, so tech
  and QI are hopelessly confounded; this needs the scripted equal-population matchup the
  phase actually specifies.
- **Phase 5's war acceptance is unconfirmed.** Two of three `on` runs recorded zero war
  deaths, so casualty-ratio-by-QI-gap has no data. Needs the scripted matchup.

### The finding that matters most: the schools are nearly empty

Across all three `on` runs the universities run at a **~6% seat-fill rate**, with 43-58% of
sampled civ-days at *zero* enrolment, while the tribe pays the full 1500/level monthly upkeep:

| run | mean enrolled | median | seat-fill | days at zero |
|---|---|---|---|---|
| on_s1 | 2.25 | 1 | 6.0% | 43% |
| on_s2 | 1.03 | 0 | 4.0% | 58% |
| on_s3 | 1.82 | 1 | 6.1% | 46% |

**The binding constraint is not the one the plan designed.** Phase 2 intended poverty to be
the brake ("if the tribe cannot pay, drop enrolment"), but `schoolQuality` averages 0.797
(median 0.831), which requires both `fed` and `funded` to be near maximum — so `byCoin` and
`byFood` are *not* binding. What binds is `candidates`: members aged 6-22. These worlds are
geriatric — 73-79% of the population is 60+ and only 10-13% is under 5, in the `off` arm as
much as the `on` arm. There are simply almost no children to school.

So the QI loop is currently gated by a pre-existing demographic pathology rather than by its
own economics, and a University is a 7000-token building that teaches one or two children.
The brake Phase 7 designed has never actually been the thing slowing QI down.

## Suggested execution order for a single sitting

Phase 1 → 2 (compile & run headless, eyeball meanQI) → 3 → 4 (verify tech acceleration) →
5 (verify war) → 6 → 7 (verify the brakes bite) → 8. Phases 4, 5 and 6 are independent of each
other and can be split across contexts once Phase 3 lands.
