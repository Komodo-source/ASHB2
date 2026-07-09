# Society Layer Plan — Roles, Voting, Leadership, Corruption

**Goal**: a complete, simple, self-reinforcing society loop — every tribe member has a role,
democratic tribes hold real elections, other governments have councils and succession,
and leaders can turn corrupt, get caught, and fall. Not an "advanced" society: every phase
reuses an existing system rather than inventing a new one.

**The loop we are building**:
roles produce wealth → wealth is taxed into the treasury → leaders control the treasury →
corrupt leaders skim it and favor their family → corruption erodes `govSatisfaction` →
low satisfaction loses elections (democracy) or triggers coups (others) → new leader → repeat.

Each phase is self-contained and executable in a fresh context. Execute in order.

---

## Phase 0 — Discovery output: Allowed APIs & Anti-patterns (READ FIRST, every phase)

### Existing systems to build on (verified, with locations)

| System | Where | Key facts |
|---|---|---|
| Tribe | `src/header/CivilizationEngine.h:107-223` | `leaderId`, `memberIds`, `taxeRate`, `Economic economy` (treasury), `granary`, `govSatisfaction` (0-100), `GovernmentType government`, `lastCoupDay`, `stances/relations` |
| Governments | `CivilizationEngine.h:86-91` | `GOV_DEMOCRACY, GOV_AUTHORITARIAN, GOV_DIVINE_MONARCHY, GOV_OLIGARCHY` |
| Civ tick | `src/CivilizationEngine.cpp:102-205` | ordered `update*` calls; once-per-civ-day block at cpp:126-132 (guarded by `lastDynastyDay`) runs `updateSocialClasses`, `updateDynasties`, `updateColonization`, `updateNarrativeChains` |
| Leader selection | `electLeader` cpp:453-472 (dominance-max); `chooseLeaderFor(tribe, gov, entities)` cpp:2372-2396 (per-government scoring); `stageCoup` cpp:2402-2447 |
| Roles today | `Entity::specialization` (std::string) + `Entity::isSpecialist` — `src/header/Entity.h:508-510`. Assigned in `updateDivisionOfLabour` cpp:1661-1780 (talent map cpp:1593-1605, promote-by-dominance cpp:1704-1721, role outputs cpp:1751-1766) |
| Taxes/treasury | `collectTaxes(Tribe&, entities)` cpp:445-451 — each member pays `salary.token * taxeRate` into `tribe.economy` |
| Wallet | `Economic` class `src/header/Economics.h:55-70` — `.token`, `earnMoney/spendMoney` |
| Personality | `Entity::personality` (Big Five floats, default 50) Entity.h:138-152; `ValueSystem` Entity.h:155-161 (`achievementDrive`, `collectivism`…) |
| Reputation | `Entity::reputationMap` (`std::map<int, PerceivedReputation>`) Entity.h:72-78, 354; write pattern `w->reputationMap[offenderId]` implem_free_will.cpp:3831-3834 |
| Punishment cascade | `FreeWillSystem::applySocialSanction(offender, victim, witnesses, lethal, simDay)` implem_free_will.cpp:3797-3900 — includes exile pattern (3871-3893) |
| Kinship/dynasty | `struct Family` `src/header/Kinship.h:15-27` — `memberIds`, `prestige`, `prominent`; `KinshipSystem::adjustReputation(familyId, delta)` Kinship.h:59; prestige accrual `updateDynasties` cpp:2116-2152 |
| Actions | `FreeWillSystem::initializeActions()` implem_free_will.cpp:708-1240; `Action` struct FreeWillSystem.h:46-60; civ side-effects in `pointedAssimilation` FreeWillSystem.h:310; existing `LeadGroup`(200) / `ChallengeLeader`(206) at implem_free_will.cpp:1046, 1100 — currently stat-only, no civ effect |
| Event log | `CivilizationEngine::logEvent(day, desc, cat, data)` cpp:3382-3395; structured `data` block e.g. coup log cpp:2442-2446 |
| LiveConfig | flat float-multiplier struct `src/header/LiveConfig.h:9-25` (`g_liveConfig`), sliders UI.cpp:1489-1505 |
| UI | CIVILIZATION panel UI.cpp:738, TRIBES section 751-780; MIND BOARD entity inspector UI.cpp:1041 |
| Save/Load | `Entity::saveTo/loadFrom` Entity.cpp:365-494 — line-based `KEY:value`, strict positional order, append-at-end with presence guard (goals pattern cpp:488-494) |
| Big-summary counters | `CivilizationEngine.h:334-346` (`totalCoups`, `totalRebellions`…) shown in `getBigSummary()` |

### Anti-patterns (DO NOT)

- **Do not invent a second tribe/leader/treasury structure.** Extend `Tribe` and `Economic`.
- **Do not add a new class system.** Two already coexist (`SocialClass` enum SocialOrder.h:14-18 and the wealth-percentile census in `updateSocialClasses` cpp:2074-2109). This plan touches neither; roles ≠ classes.
- **No new per-tick pass outside the established pattern**: declare next to `updateSocialClasses` (h:435), define near `updateDynasties` (cpp:2116), call inside the once-per-civ-day block (cpp:126-132).
- **Serialization order is positional**: only append new `KEY:` lines at the END of `saveTo` and read them LAST in `loadFrom` with a presence guard. Never insert in the middle.
- **Action IDs 1-42, 50, 200-230 and 2212 are taken.** New actions use 231+ (and not 2212).
- **`Tribe`/`Family`/CivilizationEngine have NO serialization at all** (pre-existing). Do not attempt to bolt full civ save/load into these phases — it's the optional stretch phase 6 only.
- **Determinism**: use the engine's own `rng` (`std::mt19937_64`, CivilizationEngine.h:391) for all society rolls, never `rand()`.

---

## Phase 1 — Foundations: universal roles + integrity trait + persistence

### What to implement

1. **Give every tribe member an explicit role.** Keep `std::string specialization` (Entity.h:508)
   as the storage — do not migrate to an enum (too invasive). Change semantics: non-specialists
   are `"farmer"` instead of `""`.
   - In `updateDivisionOfLabour` (cpp:1661-1780): when an entity is demoted or provisioning fails
     (cpp:1722-1747), set `specialization = "farmer"` instead of clearing it. When a member joins
     a tribe (`absorbEntityIntoTribe`, h:475) default them to `"farmer"`.
   - Add role stability: new field `int roleSinceDay = -1;` next to `specialization` (copy the
     field-block style of Entity.h:501-509). In the promote/demote logic, skip reassigning anyone
     whose role changed less than ~3 civ-days ago, so careers are sticky instead of reshuffled
     every tick.
2. **Add the corruption-relevant trait**: `float integrity = 50.0f;` on Entity (same block).
   Initialize at entity creation from personality — copy wherever `dominanceRank`/`auctoritas`
   are initialized: `integrity = clamp(0.4f*conscientiousness + 0.4f*agreeableness + noise(±15), 0, 100)`.
   Children: average of parents ± noise (find the birth/inheritance site by grepping for where
   `parent1Id` is assigned on newborns).
3. **Serialize the society fields.** Append to `Entity::saveTo` (after the last existing write,
   Entity.cpp:~434) and read at the end of `loadFrom` with a presence guard (copy the goals
   pattern at Entity.cpp:488-494):
   `TRIBEID:`, `RELIGIONID:`, `FAMILYID:`, `SPECIALIZATION:`, `ISSPEC:`, `ROLESINCE:`,
   `AUCTORITAS:`, `INTEGRITY:`, `DOMRANK:`.

### Documentation references
- Field block to copy: Entity.h:496-510. Save pattern: `file << "SALARY:" << ...` Entity.cpp:422.
- Demote/provision code: CivilizationEngine.cpp:1722-1747.

### Verification checklist
- Build passes (`make` / CMake as per repo Makefile).
- Headless run: grep `civilization_log.txt` for `kind=specialist` still appearing.
- Save then load a game: entity `specialization` and `tribeId` survive; loading an OLD save
  (without the new keys) does not crash (presence guard works).
- Grep guard: `grep -n "specialization = \"\"" src/` returns nothing (no code clears a role to empty).

### Anti-pattern guards
- Do NOT reorder existing save keys. Do NOT convert specialization to an enum.

---

## Phase 2 — Complete the role economy (every role does something)

### What to implement

1. **Extend the role-output switch** (cpp:1751-1766) so all roles have a concrete effect.
   Existing: craftsman→materials, scholar→innovation, trader→earnMoney, warrior→militarism.
   Add:
   - `"farmer"`: already implicitly tithes to granary — make it explicit in the same switch
     (no behavior change, just move the tithe there if trivially possible; otherwise leave and comment).
   - `"healer"`: small per-tick reduction of tribe-mates' disease/stress — find the mortality or
     disease hook (grep `Disease.h` / `mortalityMul` read site) and apply a small tribe-level
     multiplier when healer count > 0.
   - `"priest"`: add `"priest"` to the talent map (cpp:1593-1605): highest `ValueSystem.spiritualNeed`
     → priest. Output: +spiritualism drift and +follower happiness (copy the warrior→militarism
     line style; the religion happiness hook is `institutionLevel` effects in updateReligions).
2. **Pay wages by role**: in the same pass, `salary.earnMoney(...)` small role-dependent income
   (farmer lowest, trader/scholar highest) so wealth, classes, and taxes all flow from work.
   Keep magnitudes tiny — verify Gini (`wealthGini`, h:451) doesn't explode.
3. **UI**: in the CIVILIZATION panel's TRIBES section (UI.cpp:751-780), add one line per tribe:
   role breakdown (`farmers/craftsmen/traders/scholars/healers/warriors/priests`) computed by
   counting `memberIds`' specializations. In MIND BOARD (UI.cpp:1041) show
   `Role: <specialization> (since day N) | Integrity: X`.

### Documentation references
- Role output switch: CivilizationEngine.cpp:1751-1766. Talent map: cpp:1593-1605.
- Wallet API: `Economic::earnMoney` Economics.h:55-70. UI value-bar style: UI.cpp:751-780.

### Verification checklist
- Headless 2000+ tick run: role breakdown in logs shows all 7 roles occupied in a large tribe.
- `getBigSummary()` still prints; Gini stays < ~0.8.
- MIND BOARD shows role + integrity for a selected entity.

### Anti-pattern guards
- No new stockpile fields; reuse `granary`, `matStock`, existing stockpiles (h:166-169).

---

## Phase 3 — Voting, elections, and councils (greenfield — confirmed nothing exists)

### What to implement

1. **New pass `void updateElections(std::vector<Entity>& entities, int day);`**
   Declare at CivilizationEngine.h next to `updateSocialClasses` (h:435); define following the
   `updateDynasties` style (cpp:2116); call inside the once-per-civ-day block (cpp:126-132),
   AFTER `updateSocialClasses` so class/wealth data is fresh.
2. **New Tribe fields** (append inside `struct Tribe`, copy style of the government block h:119-127):
   ```cpp
   // ── Elections & council ──
   int  nextElectionDay = -1;   // democracy: when the next vote happens
   int  termLengthDays  = 40;   // civ-days per term
   std::vector<int> councilIds; // advisors (all gov forms), size 3-5
   float lastElectionMargin = 0.f; // winner share 0-1, for legitimacy
   ```
3. **Democratic elections** (only `GOV_DEMOCRACY` tribes): when `day >= nextElectionDay`
   (initialize on first pass), run a real ballot:
   - Candidates: top 4 members by `auctoritas` (+ incumbent always).
   - Each living member casts one vote, scoring each candidate by:
     personal opinion (`reputationMap[cand].positiveScore - negativeScore`, guard missing key),
     candidate `auctoritas`, same-family bonus (`familyId` match), candidate family `prestige`
     (via `KinshipSystem` lookup), and a legitimacy penalty on the incumbent scaled by
     `(60 - govSatisfaction)` so unpopular rulers actually lose.
   - Winner → `tribe.leaderId`; `lastElectionMargin` = winner share; winning boosts
     `govSatisfaction += 10 * margin`; log
     `logEvent(day, "<Tribe>: <Name> wins the election (X% of Y votes)", "tribe", "kind=election tribe=... winner=... margin=... turnout=...")`
     — copy the coup log format cpp:2442-2446. Increment new counter `totalElections`.
   - Set `nextElectionDay = day + termLengthDays`.
4. **Councils for every government** (rebuilt each pass, cheap):
   - Democracy: top 3 vote-getters. Oligarchy: 3 wealthiest (`salary.token`).
   - Divine monarchy: 3 highest `spiritualNeed` (priests). Authoritarian: 3 highest `dominanceRank`.
   - Council effect (keep simple): council average opinion of the leader (same reputationMap
     scoring) nudges `govSatisfaction` ±2 per pass — a hostile council destabilizes a ruler.
5. **Council sets the tax rate**: replace wherever `taxeRate` is statically set (grep for
   `taxeRate =`) with a per-pass adjustment: council nudges `taxeRate` toward a gov-form target
   (democracy ~0.05, oligarchy ~0.15, authoritarian ~0.2, divine ~0.1), ±0.01 per pass, clamped 0-0.3.
6. **Counters + summary**: add `int totalElections = 0;` next to `totalCoups` (h:344) and print
   it in `getBigSummary()`.

### Documentation references
- Once-per-day pattern & guard: cpp:126-132 and `updateGovernment`'s `lastGovDay` self-gate cpp:2452-2454.
- Candidate scoring ingredients: `chooseLeaderFor` cpp:2372-2396 (reuse its trait-scoring idiom).
- RNG: member iteration order is deterministic; use engine `rng` (h:391) for tie-breaks.

### Verification checklist
- Headless run with ≥1 democracy tribe (they emerge from coups; or temporarily set default gov):
  `grep "kind=election" src/data/civilization_log.txt` shows elections with plausible margins.
- Incumbents with `govSatisfaction < 40` lose more often than they win (spot-check 5 elections).
- `getBigSummary()` prints a nonzero election count on a long run.
- No election events from non-democracy tribes.

### Anti-pattern guards
- Do NOT create an Election/Ballot class or per-vote log lines (one logEvent per election).
- Do NOT let elections run every tick — once per civ-day pass, date-gated per tribe.

---

## Phase 4 — Power: real challenges, succession, and the perks of office

### What to implement

1. **Make `ChallengeLeader` (action 206) real.** In `pointedAssimilation`
   (FreeWillSystem.h:310, definition in implem_free_will.cpp — find the switch on action id/name):
   when the action fires and the actor's tribe is NOT a democracy, roll
   challenger `dominanceRank + auctoritas` vs leader's, weighted by `govSatisfaction`
   (unpopular leaders are easier to topple). Success → `tribe.leaderId = challenger`,
   `govSatisfaction -= 10`, log `kind=leader_challenge`. Failure → challenger loses auctoritas
   and gains anger. In a democracy the action instead boosts the challenger's `auctoritas`
   (campaigning) — no violent takeover.
2. **Hereditary succession.** On leader death (leaders currently vanish until
   `electLeader`/coup replaces them — grep `removeDeadFromTribes` cpp and where `leaderId`
   becomes stale): for `GOV_DIVINE_MONARCHY` and `GOV_OLIGARCHY`, first look for the dead
   leader's family via `familyId` + `Family::memberIds` (Kinship.h:15-27) and crown the
   adult kin with highest `auctoritas`; boost that family's `prestige` (+2, via the
   `updateDynasties` accrual idiom cpp:2116-2152). If no kin: fall back to existing
   `chooseLeaderFor`. Democracy: schedule a snap election (`nextElectionDay = day`).
   Log `kind=succession heir=... family=...`.
3. **Perks of office (the honest kind — corruption comes next).** In `collectTaxes`
   (cpp:445-451) pay the leader a legal stipend: 5% of collected taxes into
   `leader->salary.token` via `earnMoney`. Leaders also gain +0.2 `auctoritas`/pass
   (cap 100). This creates the wealth-and-status prize that makes office worth
   seeking, holding, and abusing.
4. **Counter**: `int totalSuccessions = 0;` + summary line.

### Documentation references
- Kin lookup: `Entity::familyId` Entity.h:335, `Family` Kinship.h:15-27.
- Stipend wallet ops: Economics.h:55-70. Challenge action def: implem_free_will.cpp:1100-1105.

### Verification checklist
- Long headless run: `grep -c "kind=succession"` > 0 and `grep -c "kind=leader_challenge"` > 0.
- A tribe whose leader dies has a new valid `leaderId` (not -1, not a dead entity) next civ-day.
- Leaders are measurably richer than the tribe median late in a run (inspect save or logs).

### Anti-pattern guards
- Do NOT bypass `pointedAssimilation` with a direct engine call from Entity code — follow the
  existing action→civ-effect route.
- Succession must never crown a dead or non-member entity (check `isMember` + alive).

---

## Phase 5 — Corruption: skimming, favoritism, scandal, and downfall

### What to implement

1. **Embezzlement in `collectTaxes`** (cpp:445-451). After taxes land in `tribe.economy`,
   the leader skims: probability and size scale with `(100 - leader->integrity)`,
   `achievementDrive`, and LOW oversight. Oversight = government transparency
   (democracy 0.8, oligarchy 0.4, divine 0.3, authoritarian 0.2) × (1 + hostile council bonus).
   Skimmed amount: up to `corruptionRate * taxTake` moved `tribe.economy.spendMoney` →
   `leader->salary.earnMoney`. Track it: new Tribe field
   `float corruption = 0.f; // 0-100 accumulated graft` — rises with each skim, decays 0.5/pass
   when clean. Multiply all odds by new `g_liveConfig.corruptionMul` (add field LiveConfig.h:13
   style + slider UI.cpp:1496 style).
2. **Favoritism.** In `updateDivisionOfLabour`'s promote-by-dominance sort (cpp:1704-1721):
   a corrupt leader (integrity < 40) bumps same-`familyId` kin to the front of the promotion
   queue regardless of dominance, and their family gains prestige while OTHER prominent
   families lose relation to the leader (use `KinshipSystem::adjustReputation`, Kinship.h:59).
3. **Detection & scandal** (inside the same `updateElections`/governance pass or a small
   `updateCorruption` helper called next to it): each civ-day, chance of exposure =
   `corruption/100 × oversight × scholarCount-bonus`. On exposure:
   - Log `kind=scandal tribe=... leader=... graft=...` (copy coup log format cpp:2442-2446).
   - Reputation collapse: for up to ~20 tribe members, write
     `member->reputationMap[leaderId]` negativeScore += 30, trustworthiness -= 30
     (copy the witness loop implem_free_will.cpp:3831-3834).
   - `govSatisfaction -= 15 + corruption/5`. `corruption` halves (the books are opened).
   - Consequence by government: democracy → snap election (`nextElectionDay = day`);
     others → if `govSatisfaction` now below the existing coup floor, let the EXISTING
     `updateGovernment`/`stageCoup` machinery (cpp:2402-2447) do the deposing — do not
     duplicate coup logic. Severe graft (corruption > 70) in any government → exile the
     leader using the exile pattern (implem_free_will.cpp:3871-3893: remove from
     `memberIds`, `tribeId = -1`, boost goal "exiled", log `kind=exile`).
4. **Background pressure**: each governance pass, `govSatisfaction -= corruption/50`
   even without a scandal — people feel the missing granary money. This closes the loop:
   corruption → unrest → election loss/coup → new (maybe honest) leader.
5. **Optional possess-mode action** `"Embezzle"` id 231 (category "leadership"): copy the
   `ChallengeLeader` block (implem_free_will.cpp:1100-1105), require being a leader, and route
   its civ effect through `pointedAssimilation` to a one-shot skim. Add to the main.cpp:1372
   routing list only if trivial.
6. **Counters**: `int totalScandals = 0; int totalDepositions = 0;` + `getBigSummary()` lines.

### Documentation references
- Treasury ops: cpp:445-451 + Economics.h:55-70. Exile: implem_free_will.cpp:3871-3893.
- Reputation write: implem_free_will.cpp:3831-3834. Coup floor logic: `updateGovernment` cpp:2452+.

### Verification checklist
- Long headless run: `grep -c "kind=scandal"` > 0; scandals are followed (same tribe, later day)
  by elections/coups in the log more often than not.
- Democracies average LOWER tribe `corruption` than authoritarian tribes over a run
  (print per-gov averages once in the end-of-run summary to check).
- `corruptionMul = 0` slider → zero scandals in a fresh run (clean kill-switch).
- Build + old saves still load.

### Anti-pattern guards
- Do NOT write a parallel "police/court" system — detection is a probability roll, punishment
  reuses scandal→satisfaction→existing coup/election/exile machinery.
- Never let skimming drive `tribe.economy.token` negative; clamp.

---

## Phase 6 — Surface it + final verification (and optional persistence stretch)

### What to implement

1. **CIVILIZATION panel** (UI.cpp:751-780), per tribe add:
   government name (`governmentName(g)`, h:92), leader name + integrity, council member names,
   treasury (`economy.token`), tax rate, `govSatisfaction` bar, `corruption` bar (red),
   "next election in N days" for democracies, role breakdown from Phase 2.
2. **Big summary** (`getBigSummary`, cpp — grep it): one governance block —
   elections held, successions, challenges, scandals, depositions, average corruption by
   government form.
3. **(Stretch, only if time allows)** Tribe persistence: add `saveTo/loadFrom` on `Tribe`
   mirroring the Entity text pattern (Entity.cpp:365-494), writing
   id/name/leaderId/government/govSatisfaction/taxeRate/economy.token/corruption/
   nextElectionDay/councilIds/memberIds, called from wherever entities are saved
   (SaveLoad.cpp). This fixes a pre-existing gap (tribes were never saved), so treat it as
   its own commit and don't block the society features on it.

### Final verification (whole plan)
1. Clean build, zero new warnings in changed files.
2. Headless run ≥ 5000 ticks; then:
   - `grep -c "kind=election\|kind=scandal\|kind=succession\|kind=leader_challenge" src/data/civilization_log.txt` — all four present.
   - `getBigSummary()` governance block shows nonzero counts.
3. Anti-pattern greps:
   - `grep -rn "rand()" src/CivilizationEngine.cpp` — no new hits (determinism).
   - `grep -n "specialization = \"\"" src/` — empty.
   - No new action reuses ids ≤ 230 or 2212.
4. GUI smoke test: CIVILIZATION panel shows government/leader/corruption; MIND BOARD shows role.
5. Save → load → run 100 ticks without crash; old saves still load.
