# ASHB2 — The Parallel-Earth Upgrade: Master Plan

**Purpose.** Turn ASHB2 from an already-deep behavioural simulation into the most
human-resembling society simulation attainable on this engine — a world so close to ours
that a reader of its Chronicle would mistake it for a parallel Earth. This document is the
single master brief; it is self-contained and can be handed to any AI or engineer to
execute phase-by-phase.

**How to read this.** §1 diagnoses what the sim actually does today and why it falls short
of "parallel Earth" (grounded in the code and in two long run-reports). §2 fixes the rules
every phase must obey (they are the same rules that made M1–M12 succeed). §3 is the
**sociology grounding** — the real human facts and papers each feature is built on. §4–§7
are the four upgrade **Tracks**, sequenced to the owner's priority: **Inner Life →
Road to Modernity → Economy/Cities/Class → Culture/Religion/Language**. §8 is cross-cutting
instrumentation. §9 sequences everything. §10 is the acceptance test — the "parallel
universe" bar. §11 lists sources.

> Deliverable choice: **one master mega-document** (this file). Ambition: **strangler-fig**
> — every phase compiles, runs, stays deterministic, and extends an existing system rather
> than replacing it. "Make every entity meaningful" is read as **all three at once**: a
> traceable life story + visible causal legacy + a distinct interior that drives behaviour.

---

## 1. Diagnosis — where the sim stands, and the gap to "parallel Earth"

ASHB2 is already extraordinary in breadth. As-built, an agent appraises 10 discrete
emotions each tick, perceives a stress-limited subset of neighbours, pre-reads threats via
theory-of-mind, scores ~63 catalogue actions through ~16 additive/multiplicative factors,
picks stochastically, then settles the outcome into regret/relief, skill practice, injury,
gossip and intention progress (`implem_free_will.cpp:3503` `cognitiveChooseAction`;
`src/ai/MindUpgrade.{h,cpp}`). The macro layer runs tribes, four government forms with real
elections, councils, corruption, embezzlement and scandal, division of labour, religions
with doctrinal axes, a dual tech system, formal treaties, grievance-tagged wars with
conquest/vassalage machinery, a ~60-good global market with a Gini, two class systems, and
dynasties on a procedurally-generated planet with biomes, rivers, resources and an
ecosystem (`CivilizationEngine::tick` `CivilizationEngine.cpp:103-213`).

And yet the two flagship run-reports (`plans/ASHB2_SIMULATION_REPORT_2026-07-03.md`) expose
exactly where it stops being Earth-like. These are the **failures this plan targets**:

| # | Observed failure (from the run reports) | Root cause in code | Real-world truth being violated |
|---|---|---|---|
| F1 | **The stagnation trap.** Tech plateaus at ~31 and the world suffers **181 dark ages**, oscillating Medieval↔Modern forever. It literally *cannot cross into modernity.* | Fixed action/tech ceiling; dark ages *subtract* techs; knowledge is a decaying float with no imperishable store. | Cumulative culture ratchets *upward* once knowledge is recorded and populations interconnect (Henrich's collective brain). Real knowledge is hard to lose. |
| F2 | **War is performative.** 860 wars, 1,157 battles, **11 deaths, zero conquests** across 4,950 days. | Battles rarely draw blood; conquest machinery exists but thresholds/territory transfer almost never fire. | War reshapes maps, ends dynasties, moves peoples. Bloodless ritual war is the exception, not the rule. |
| F3 | **Monocausal conflict.** **98.8% of wars are holy wars.** | `updateTribeRelations` weights faith friction far above resource/border/succession causes. | Real wars are plural: land, trade, succession, ethnicity, ideology, revenge. |
| F4 | **Bonds are fragile and jealousy-dominated.** 52% of couples dissolve; **19,110 jealousy events** are 50.7% of all social events. | Relationship maintenance is thin; jealousy is the loudest social signal. | Human bonds are sticky, layered (Dunbar/Granovetter), and sustained by shared history, kids, reciprocity — not mainly by jealousy. |
| F5 | **Everyone dies stressed.** Stress ≈ 95–100 at death across *every* cause. | Stress accumulates without adequate discharge/adaptation. | Humans habituate, cope, find meaning; chronic max-stress for a whole population is unreal. |
| F6 | **Culture is a scalar.** `cultureScore` is one number; religions have near-identical creeds. | `updateCulture:2039`; content-bearing `CulturalTransmissionSystem` is dead code in `environment/EnvironmentModel.h`. | Cultures carry *content* — distinct values, art, taboos, memes — that diverge and travel (Bourdieu, Mauss). |
| F7 | **No cities.** A tribe is a single `centerX/centerY` point. | No settlement/urban system. | Population concentrates into settlements whose sizes follow Zipf's law; density changes everything (disease, innovation, class). |
| F8 | **No language effect, no symbolic speech.** `CommunicationBasis` is a broken stub; Lexicon only makes names. | `CommunicationBasis.h:7-12`; `world/Lexicon` names-only. | Language binds and divides groups, gates diplomacy and diffusion, and creolizes on contact. |
| F9 | **Inert inner content.** `coreBeliefs`, `ideologicalStances`, rich memory are stored but barely read back into choice; knowledge is 4 predicates capped at 48 facts. | `implem_free_will.cpp` scoring ignores ideology/coreBeliefs; `MindUpgrade.h:77-96`. | People act *from* their beliefs, memories and identities; those are the person. |
| F10 | **No law, institutions, property, or persistent grievance.** Corruption exists but no courts, guilds, schools, inheritance, or remembered feuds. | Grievances recomputed per tick; `InstitutionalSystem` dead code. | Institutions and heritable property are the scaffolding of complex society and of inequality's persistence. |

The through-line: **the machinery for a real world is largely present but under-wired or
scalarised.** This plan is therefore mostly *wiring, deepening, and content* — squarely
strangler-fig — not a rewrite.

---

## 2. Design rules for every phase (non-negotiable)

These are lifted from the existing plans that shipped M1–M12 and AI Phases A–E; obey them
exactly so the sim stays alive and reproducible after every commit.

1. **Determinism.** All randomness flows through the world seed via `BetterRand` /
   `makeStream(master, salt)` / the engine's `rng` (`CivilizationEngine.h:391`). Never
   `rand()`, never `random_device`, never `time()`. Deterministic iteration order always.
   New subsystems default to a **bit-exact no-op** (multiplier = 1.0 / feature-flag off) so
   the determinism pair stays green until deliberately enabled.
2. **Kill switch + tunability.** Every new subsystem gets a `g_liveConfig` multiplier
   (`LiveConfig.h`) and, where it has weights, a `bridge/priors_*.txt` entry
   (`MindUpgrade.cpp:122-148` pattern) so the director console can A/B it live and
   `butterfly.py` can measure divergence.
3. **Budget + staggering.** Each phase states a per-tick cost target and, above ~2k agents,
   hides behind the existing cohort-staggering / civ-day gating pattern
   (`CivilizationEngine.cpp:128-136`). Perception-heavy work respects the attention budget
   already in `cognitiveChooseAction:3526`.
4. **Save format.** Append-only `KEY:value` lines at the **end** of `saveTo`, read **last**
   in `loadFrom` with a presence guard (goals pattern `Entity.cpp:488-494`). Never reorder
   existing keys. Give `Tribe`/`Family`/civ systems their own chunked sections when first
   persisted (Save V2 chunk style, master plan §4B).
5. **CI gate.** `scripts/validate.sh` (build + determinism pair + realism report) stays
   green. Each phase lands **with** the realism assertion that measures it (§8).
6. **Reuse before create.** The default answer is "extend the struct / add to the switch /
   wire the dead module," not "new class." The as-built inventory in §1 and the file
   anchors throughout tell you where.

---

## 3. Sociology grounding — the human facts this plan applies

Every major feature below is anchored to an empirical or theoretical result about real
humans. This is the "interesting human facts / articles" layer the brief asked for; use it
both as design rationale and as the source of the numeric targets the realism report checks.

| Theme | Human fact / theory (source) | What it demands of the sim | Applied in |
|---|---|---|---|
| **Why history stagnates or takes off** | **Structural-Demographic Theory & secular cycles** — societies cycle through integrative/disintegrative phases driven by popular immiseration, **elite overproduction**, and state fiscal stress (Turchin). | Replace the boring Malthusian flat-line and the tech yo-yo with real secular cycles: elite numbers, inequality and instability should oscillate in anti-phase with popular well-being. | Track II P3 (Secular cycles), Track III (class) |
| **Why technology accelerates** | **Cumulative cultural evolution / the collective brain** — innovation is an emergent property of *population size × interconnection*; larger, better-connected groups hold and grow more complex tech; isolation loses it (Henrich & Muthukrishna, "Innovation in the Collective Brain," *Phil. Trans. R. Soc. B* 2016). | Tech rate = f(literate population, trade/contact connectivity, institutions that *store* knowledge). Writing makes knowledge near-imperishable → kills the dark-age ratchet. | Track II P1–P2 |
| **Social network structure** | **Dunbar's number** (tiered relationship capacity) and **Granovetter's strength of weak ties** (weak ties bridge groups and carry novel information/opportunity). | Cap strong ties; add a weak-tie layer that carries information, jobs, and diffusion across tribes. Innovation and gossip should travel on weak ties. | Track I P2, Track IV P3 |
| **Homophily & segregation** | **"Birds of a feather"** (McPherson) + **Schelling's segregation model** — mild same-kind preference produces large-scale self-sorting. | Residence, marriage, and faction choice weighted by similarity → emergent neighbourhoods, ethnic quarters, echo chambers from mild preferences. | Track III P1 (cities), Track IV P2 |
| **How norms flip** | **25% critical-mass tipping point** for social convention change (Centola et al., *Science* 2018). | A committed minority reaching ~25% of a group flips a norm/fashion/taboo abruptly — model norm/fashion/religion adoption with this threshold, not linear drift. | Track IV P1 |
| **The gift & reciprocity** | **Mauss, *The Gift*** — obligations to give/receive/repay bind people; **collective effervescence** (Durkheim) — synchronized ritual bonds groups and discharges tension. | Gifts create durable debt/bond edges (an alternative to violence for status); festivals/rituals discharge stress (fixes F5) and spike cohesion. | Track I P3, Track IV P4 |
| **Class as reproduction** | **Bourdieu** — habitus + economic/social/**cultural capital** reproduce class across generations; taste marks status. | Inheritance of wealth *and* cultural capital; taste/manners as status signals; mobility that is real but sticky. | Track III P4 |
| **Inequality's shape** | **Pareto/power laws** — ~80/20 wealth; **Zipf's law** — city sizes follow rank = 1/size. | Wealth and settlement-size distributions should *emerge* into power laws; report checks the exponent. | Track III P1, P4 |
| **Cooperation & feud** | **Axelrod**, evolution of cooperation — **tit-for-tat** (nice, retaliatory, forgiving) wins iterated games; reciprocity sustains cooperation and, when unforgiving, sustains feuds. | Inter-agent and inter-tribe relations run reciprocity with memory → durable alliances *and* multi-generation feuds (fixes F4/F9/F10). | Track I P2, Track II P4 |
| **Anomie & meaning** | **Durkheim** — normlessness raises suicide/deviance; meaning and integration protect. | Well-integrated, purpose-holding agents resist despair; social dissolution raises deviance — meaning is a real stat, not flavour. | Track I P1 (purpose), Track I P4 |
| **Demographic transition** | Real populations shift from high-fertility/high-mortality to low/low as wealth, child-survival and female status rise. | Fertility should *fall* with wealth/urbanisation/education, not stay flat — enabling modern demographic texture. | Track III P3 |

---

## 4. TRACK I — Inner Life: make every entity a meaningful person

**Goal (owner's #1 priority).** Every agent becomes a person you could write a biography
of: a **traceable life story**, a **visible causal legacy**, and a **distinct interior that
drives behaviour** — the three, integrated. Most of the raw material already exists on
`Entity` (life memories, self-concept, values, beliefs, goals) but is inert (F9). This track
*activates* it and binds it into one identity that both steers choices and leaves marks.

### I-P1. The Self that acts — identity, purpose, and narrative spine
*Reuse:* `SelfConcept` (`Entity.h:364`), `coreBeliefs` (`:520`), `ideologicalStances`
(`:556`), `ValueSystem` (`:157`), `m_goals`/`Intention` (`:320,389`), `lifeMemories`
(`:361`).

- **Make identity causal.** Add a `narrativeIdentity` summary on `Entity` (a small struct:
  dominant value, defining formative memory id, self-story tag e.g. "survivor",
  "provider", "seeker") recomputed at each life-stage transition from the *actual*
  highest-salience memories and most-acted values. Feed it into `cognitiveChooseAction` as a
  new **identity-congruence** scorer (self-consistency: people act to stay who they believe
  they are). This is the wire that makes F9's inert `coreBeliefs`/`ideologicalStances`
  finally read back into choice.
- **Purpose / meaning as a protective stat** (Durkheim anomie). Add `senseOfPurpose` derived
  from goal progress + social integration + role + faith; low purpose raises despair/deviance,
  high purpose buffers stress (a partial fix for F5). It is *earned* from doing meaningful
  things, not assigned.
- **Life-narrative spine.** Every high-salience event already writes a `LifeMemory`; add a
  compact **`lifeChapters`** log (birth, first love, first kill, bereavement, triumph,
  betrayal, migration, elevation, ruin) so each agent carries an ordered biography that the
  Interview/Chronicle can read verbatim.

*Sociology:* Durkheim (meaning/integration), narrative-identity psychology.
*Determinism/budget:* recompute on life-stage change only (cheap). Scorer behind a prior
weight defaulting to preserve current distribution, then tuned up.
*Verify:* Interview mode can answer "Who are you and what do you want?" from real fields;
despair deaths correlate with low `senseOfPurpose`; disabling the identity scorer reproduces
the old action distribution bit-for-bit.

### I-P2. Relationships with real depth and stickiness (fixes F4)
*Reuse:* the four per-target vectors (`list_entityPointedDesire/Anger/Couple/Social`,
`Entity.h:307-310`), `entityPointedCouple` commitment/satisfaction/trust/suspicion/daysTogether
(`:256`), `reputationMap` (`:365`), `MentalModelOfOther` (`:363`).

- **Tiered ties (Dunbar).** Classify each relationship into intimate / close / weak-tie /
  acquaintance bands by interaction history; cap the intimate/close tiers. Maintenance cost
  and decay differ per tier, so a few bonds are deep and durable while many stay light.
- **Reciprocity ledger (Axelrod tit-for-tat).** Each dyad accrues a memory of favours,
  slights, gifts, and betrayals; relationship deltas become **nice-retaliatory-forgiving**
  rather than jealousy-dominated. Shared positive history and children raise couple
  stickiness — dissolution should fall from 52% toward a human ~15–30%.
- **Weak-tie information layer (Granovetter).** Weak ties become the channels that carry
  gossip, job/role opportunities, and (Track II) innovation across tribe boundaries — the
  bridges homophilous strong ties can't provide.
- **Love beyond jealousy.** Add companionate vs passionate components that mature over
  `daysTogether`; grief on partner death scales with shared-history depth (ties to I-P3
  legacy). Jealousy stays but stops being the dominant social signal.

*Sociology:* Dunbar, Granovetter, Axelrod, homophily (McPherson).
*Verify:* couple-dissolution rate lands in a human band; degree distribution of the social
graph is Dunbar-capped and roughly log-normal; weak ties demonstrably carry more novel facts
than strong ties (belief-provenance check).

### I-P3. Visible causal legacy — every life leaves marks
*Reuse:* `Kinship` families/prestige, `childrenIds`, `Innovation` authorship, `reputationMap`,
`Family.prestige` (`updateDynasties:2329`), `lifeMemories`.

- **Attributable authorship.** Stamp innovations, foundings (tribes, religions, settlements),
  laws (Track III), and great works (`updateCulture`) with a `founderId`. An agent who
  invents, founds, or legislates is remembered by name in others' beliefs and in the
  Chronicle.
- **Legacy that outlives the agent.** On death, propagate: heritable property & cultural
  capital (Track III-P4), a reputation that persists in descendants' `reputationMap`, unfinished
  feuds/debts (Track II-P4 grievance ledger), and named memorials at places
  (`EpisodicMap`/place attachment) — "the ford where Kael drowned."
- **Dynastic through-lines.** Family prestige already accrues; add **named lineages** whose
  rise/fall the Chronicle tracks, so a viewer can follow a bloodline across the whole run.

*Sociology:* Bourdieu (intergenerational reproduction), Mauss (durable obligation).
*Verify:* Chronicle can render a 5-generation family saga with attributed inventions/feuds;
"notable lives" detector finds agents whose marks persist ≥2 generations after death.

### I-P4. The felt life — mood dynamics, coping, and growth (helps F5)
*Reuse:* `EmotionState` 10 emotions (`MindUpgrade.cpp:245-323`), `PADState`,
`EmotionalState` suppression debt, `DevelopmentalHistory`, grief/Kübler-Ross states.

- **Blended, decaying moods, not clamped multipliers.** Let emotions blend (mixed feelings),
  feed a slow **mood baseline** that habituates (hedonic adaptation → directly attacks the
  everyone-dies-at-stress-100 artifact F5), and drive *reappraisal* of goals, not just action
  multipliers.
- **Coping repertoire.** Add personality-skewed coping (problem-focused vs
  avoidant vs social-support-seeking); successful coping discharges suppression debt (and
  festivals in Track IV-P4 discharge it collectively — Durkheim).
- **Post-traumatic change, both directions.** Formative trauma can scar (raise neuroticism,
  seed avoidant attachment, intergenerational trauma load already exists at `Entity.h:599`)
  **or** catalyse growth (resilience, wisdom) depending on support and meaning — so
  personalities genuinely diverge over a lifetime.

*Sociology:* hedonic adaptation, stress-and-coping (Lazarus), post-traumatic growth.
*Verify:* stress distribution at death spreads out (no longer a spike at 100); two agents
with identical genomes but different life events end with measurably different personalities.

---

## 5. TRACK II — The Road to Modernity (fixes F1, F2, F3)

**Goal (owner's #2).** Break the stagnation trap so history can actually progress — and make
war and politics consequential. This is the track that most changes what the Chronicle *is*.

### II-P1. Imperishable knowledge & the collective brain (fixes F1)
*Reuse:* `Innovation` diffusion, `TechTree` (`updateTechTree:1926`), `knowledgeStock` float,
`SkillSet`, tribe `granary`, the dead `InstitutionalSystem` scaffold.

- **Writing changes the physics of knowledge.** Introduce a **record medium** unlock (oral →
  writing → printing). Before writing, tech can be lost on population crash (the current dark
  age). *After* writing, techs held by a literate institution (library/archive — see II-P2)
  are **near-imperishable**: dark ages can starve and depopulate but no longer *erase*
  recorded knowledge. This alone converts the yo-yo into a ratchet.
- **Innovation rate = collective-brain function.** Replace the flat innovation roll with
  `rate = f(literate_population_size × interconnection × institutional_support)` (Henrich).
  Bigger, better-connected, more literate societies invent faster; isolated small ones
  stagnate — and *that* becomes the interesting variable, not a hard cap.
- **Raise/remove the tech ceiling with real gates.** Replace `MAX_TECH` with prerequisite
  chains that gate a **Renaissance → Scientific Method → Industrial** sequence on
  population, literacy, urbanisation (Track III), and institutional stability — so crossing
  into modernity is *achievable but earned*.

*Sociology:* Henrich & Muthukrishna (collective brain); history of writing/printing.
*Budget:* civ-tick only. *Verify:* at least one run crosses Medieval→Modern **and stays
there**; dark-age count collapses toward single digits; disabling writing reproduces the old
regression loop (proves the mechanism).

### II-P2. Institutions that store and transmit (fixes F1, F10)
*Reuse:* wire the **dead** `InstitutionalSystem` (`environment/EnvironmentModel.h:183-224`)
with FAMILY/EDUCATION/GOVERNMENT/ECONOMY/RELIGION institutions carrying legitimacy/efficiency;
`updateDivisionOfLabour`, roles, `Family`.

- **Schools/archives** hold techs and skills independent of any single agent (the store from
  II-P1) and teach the young (accelerates skill transmission beyond the current oblique 6%).
- **Bureaucracy/state** as an institution with legitimacy and reach lets multiple tribes bind
  into a persistent **polity/empire** larger than bilateral vassalage (fixes the "no
  persistent state" gap) — administrative capacity gates how large a society can grow before
  fragmenting.
- **Guilds** (Track III) preserve craft knowledge and regulate a trade.

*Verify:* `grep` shows `InstitutionalSystem` now ticked from the civ loop (was 0 call-sites);
a knowledge shock (kill all scholars in a tribe) no longer loses techs the archive holds.

### II-P3. Secular cycles & elite overproduction (fixes F1 stagnation → replaces it with drama)
*Reuse:* `updateSocialClasses:2287`, `SocialOrder.cpp`, `auctoritas`/`dominanceRank`,
`govSatisfaction`, `updateGovernment`/`stageCoup`, `wealthGini:2019`.

- **Model the elite stratum explicitly** (count, wealth share, offices available). When elite
  numbers grow faster than offices — **elite overproduction** — intra-elite competition rises,
  factionalism and coups increase, and the state's fiscal stress climbs (Turchin).
- **Anti-phase well-being.** Popular immiseration (falling real wages as population presses on
  carrying capacity) and elite competition drive an instability index that periodically breaks
  into civil strife, then resets — a **secular cycle** replacing the featureless plateau the
  report lamented.
- These cycles interact with II-P1: a society that reaches literacy+institutions can ride the
  cycle *upward* into modernity; one that doesn't keeps cycling — exactly the divergence the
  owner wants between runs.

*Sociology:* Turchin structural-demographic theory, secular cycles, elite overproduction.
*Verify:* instability index and popular well-being oscillate in anti-phase across a long run;
inequality (Gini) peaks precede strife peaks; report plots the cycle.

### II-P4. Consequential, plural war (fixes F2, F3)
*Reuse:* `processWarTick`/`executeBattle:3177`, `conquerTribe`/`vassalizeTribe`,
`updateTribeRelations:1330-1449` and its `WarReason` tags (ETHNIC/CONQUEST/RESOURCE/TRIBUTE/
BORDER), treaties.

- **Make battles bite and maps change.** Rebalance casualty tiers and, crucially, actually
  fire **territory transfer, conquest, tribute and population displacement** on decisive
  outcomes (F2). Conquered populations migrate, assimilate or resist — feeding Track IV
  ethnogenesis and Track I displacement memories.
- **Plural casus belli (F3).** Rebalance `updateTribeRelations` so resource pressure, border
  friction, succession disputes, trade access, and revenge each *routinely* start wars — holy
  war becomes one cause among many, not 98.8%.
- **Persistent grievance ledger (fixes F10).** Replace per-tick recomputed grievances with a
  durable per-tribe ledger of massacres, stolen land, and broken treaties that **decays
  slowly across generations** (Axelrod unforgiving reciprocity) — feuds and irredentism that
  outlive their originators and periodically reignite.
- **Empires rise and fall.** Decisive conquest can assemble multi-tribe polities (II-P2)
  which then face the secular cycle (II-P3) and fracture — the grand rhythm of real history.

*Sociology:* Axelrod (reciprocity/feud), historical war-cause taxonomy.
*Verify:* nonzero conquests per long run; war-death share rises to a historical band; war
causes are spread across ≥4 categories (no single cause >~50%); a feud persists ≥2
generations in the log.

---

## 6. TRACK III — Economy, Cities & Class (fixes F7, F10, and inequality realism)

**Goal (owner's #3).** Give the world a body: people live in *places* that grow into cities,
goods actually move, wealth concentrates into power-law inequality, and class becomes a
heritable, mobility-bearing structure — not two disconnected scalars.

### III-P1. Settlements & cities (fixes F7)
*Reuse:* tribe `centerX/centerY`, `world/Planet` fertility/ore/rivers, `homeAttachment`,
`SpatialGrid`.

- **Physical settlements.** Agents claim homes; families co-reside; homes cluster into
  **settlements** sited on fertile/coastal/river/ore tiles. Settlements have population,
  density, granary, and buildings. Villages grow into towns and cities as agriculture and
  trade support surplus non-farmers.
- **Zipf-law urban hierarchy.** Migration + agglomeration economies (bigger settlements offer
  more roles, safety, mates, and — via the collective brain II-P1 — faster innovation) should
  make settlement sizes *emerge* into a rank-size (Zipf) distribution the report can check.
- **Density consequences.** Urban density feeds Track IV faith/culture mixing, raises disease
  transmission (fixes the "disease not tied to density" gap; couple `Disease.cpp` to local
  density and trade routes → real epidemics that trace caravan routes), and concentrates
  elite competition (II-P3).
- **Homophily in residence (Schelling).** Mild same-tribe/faith/kin residence preference →
  emergent quarters and ethnic neighbourhoods from weak preferences.

*Sociology:* Zipf (city sizes), Schelling (segregation), agglomeration economics.
*Verify:* settlements visible on the planet map; size distribution fits Zipf within tolerance;
a dense city suffers an epidemic a sparse one does not.

### III-P2. Regional markets & trade routes (fixes F7 economic half, and the global-market gap)
*Reuse:* `Economics.cpp` `g_market` + `wealthGini:2019`, tribe `granary`/`matStock`/
`luxuryStock`, `TREATY_TRADE`, `Diplomacy.cpp`.

- **Regionalise prices.** Replace the single global price per good with **per-settlement/region
  markets**: local scarcity raises local prices; the global market becomes the *emergent
  aggregate*, not the primitive.
- **Goods physically move.** **Merchant caravans / trade routes** carry goods from cheap
  regions to dear ones along passable tiles (respecting mountains/seas + seafaring tech),
  earning trade wealth, spreading techs and culture (collective brain again), and creating
  chokepoints worth taxing or fighting over (feeds II-P4 trade-war casus belli).
- **Money & credit.** Deepen `Economic` wallets into money with debt/credit (extend
  `SocialOrder`'s existing `Debt`), enabling merchants, moneylenders, and boom/bust.

*Sociology:* Smith/Ricardo comparative advantage (informal), economic geography.
*Verify:* price gradients exist between regions and *narrow* where a trade route runs; cutting
a route (war/mountain) re-widens them; trade wealth produces a merchant class.

### III-P3. Labour, specialisation & the demographic transition
*Reuse:* `updateDivisionOfLabour:1741`, `SkillSet`, roles, fertility in reproduction code.

- **A real labour market.** Roles are matched to skills, demand (from settlements/markets),
  and opportunity carried on weak ties (Granovetter) — not just dominance promotion.
  Apprenticeship via guilds (II-P2) locks in specialists.
- **Demographic transition.** Make fertility *fall* with wealth, urbanisation, female status,
  and child-survival — so modernising societies show the real shift from
  high-fertility/high-mortality to low/low, giving late-run demography a modern texture
  instead of flat fecundity.

*Sociology:* demographic transition theory; occupational sociology.
*Verify:* fertility negatively correlates with wealth/urbanisation late-run; specialist mix
tracks settlement size.

### III-P4. Class as heritable reproduction (fixes F10 property gap)
*Reuse:* the two class systems (`updateSocialClasses:2287` census + `SocialOrder.cpp`
Slave/Pleb/Patrician + `Clientela`/`Debt`), `Kinship`, `Family.prestige`, `auctoritas`.

- **Unify the two class systems** into one structure: economic position (wealth percentile) +
  legal status (free/bonded) + **cultural capital** (Bourdieu — taste, literacy, manners,
  lineage prestige) that *signals* and *reproduces* status.
- **Heritable property & cultural capital.** On death, wealth, land, titles, *and* cultural
  capital pass by kinship (extend the stubbed `SocialOrder::onDeath`) — so advantage
  compounds across generations and inequality becomes **heritable and sticky** (Pareto shape
  that persists, not resets).
- **Mobility that is real but hard.** Skill, office, marriage-up, and prestige provide genuine
  but sticky mobility; downward mobility on ruin/scandal/conquest. This is the substrate the
  secular cycle (II-P3) churns.

*Sociology:* Bourdieu (capital/habitus/reproduction), Pareto (wealth power law).
*Verify:* wealth distribution fits a power law; a rich lineage stays over-represented in the
elite for generations; measured mobility is nonzero but below random mixing.

---

## 7. TRACK IV — Culture, Religion & Language (fixes F6, F8, and gives F3 its texture)

**Goal (owner's #4).** Turn culture from a scalar into living **content** that diverges,
travels, tips, and gives people identity worth fighting and living for.

### IV-P1. Culture as transmissible content + the 25% tipping point (fixes F6)
*Reuse:* wire the **dead** `CulturalTransmissionSystem` + `CulturalTrait`
(`environment/EnvironmentModel.h:116-171`) with vertical (parent→child), horizontal
(peer), and oblique (elder→young) transmission and a mutation rate; `updateCulture:2039`;
tribe cultural-value drift.

- **Traits are things.** Practices, beliefs, taboos, tastes, and fashions become discrete
  transmissible traits with content and a mutation rate; tribes accumulate **distinct
  cultures** (a vector of held traits), not a scalar `cultureScore`.
- **Norms flip at critical mass (Centola 25%).** Adoption of a norm/fashion/taboo/religion is
  modelled with a tipping threshold: below ~25% committed carriers it's a minority quirk; at
  ~25% it cascades to majority. This gives cultural change its real *punctuated* dynamics.
- **Cultural capital** feeds Track III-P4 status; **cultural distance** feeds diplomacy
  tension and assimilation pressure (Track II-P4 conquest).

*Sociology:* Bourdieu, Mauss, Centola et al. 25% tipping point, cultural-evolution theory.
*Verify:* two isolated regions accumulate *different* trait sets; a seeded minority trait
either fizzles (<25%) or cascades (≥25%); disabling transmission freezes cultural drift.

### IV-P2. Religion with doctrine, ritual, schism & function
*Reuse:* `foundReligion:980`, `spreadReligions:1056`, doctrinal axes
(militarism/tolerance/asceticism/authority/afterlifeFocus), moral code, ritual,
`institutionLevel`, `checkSchisms`, `trySyncretism`.

- **Doctrine with teeth.** Make the doctrinal axes and moral code actually *constrain
  believers' behaviour* (pacifist faiths suppress the violence pipeline; ascetic ones curb
  consumption; high-authority ones boost obedience/legitimacy) — so faiths are behaviourally
  distinguishable, not near-identical creeds (the report's complaint).
- **Meaning & effervescence.** Ritual/festival attendance discharges stress and grief and
  spikes cohesion (Durkheim collective effervescence — another lever on F5), and confers
  `senseOfPurpose` (Track I-P1). Religions persist because they *do something* for their
  members, killing the 168-founded/162-extinct churn.
- **Schism & reform on real fault lines.** Doctrinal drift + inequality + charismatic founders
  (attributed, Track I-P3) trigger schisms and reform movements that split along class/region
  lines — plural religious history.

*Sociology:* Durkheim (religion, effervescence, anomie), sociology of sects/schism.
*Verify:* believers of different faiths show different action distributions; surviving
religions correlate with measurable member benefits; schisms track inequality/region.

### IV-P3. Language that matters (fixes F8)
*Reuse:* `world/Lexicon` (per-region phonotactics, drift, creolization).

- **Language gates interaction.** Mutual intelligibility (a function of shared lineage +
  contact) modulates diplomacy success, trade-route efficiency, gossip/innovation diffusion
  (collective brain — language barriers slow the collective brain), and assimilation of the
  conquered. Weak ties across a language boundary are worth more but harder to form
  (Granovetter × language).
- **Ethnolinguistic identity.** Language + culture (IV-P1) define **ethnic groups** distinct
  from tribes; contact creolizes, conquest imposes prestige languages, isolation diverges —
  ethnogenesis and language death become visible history.

*Sociology:* sociolinguistics of contact/prestige, ethnogenesis.
*Verify:* diffusion is slower across language boundaries than within; a conquered region's
lexicon blends toward the conqueror's over generations.

### IV-P4. Ritual, art, festival & the gift (fixes F5 collectively, deepens F6)
*Reuse:* existing festival/feast machinery (`updateFestivals`), `tickEmotionalSuppression`,
grief system, `updateCulture` great-works.

- **Gift economies & potlatch (Mauss).** Gifts create durable reciprocity edges (Track I-P2)
  and convert surplus into **prestige** — a non-violent status route that competes with the
  violence pipeline (an additional structural brake beyond sanctions).
- **Festivals as collective effervescence (Durkheim).** Scheduled rituals synchronise a
  settlement, discharge accumulated suppression debt and grief, and boost cohesion — a
  population-level relief valve for the chronic-stress artifact (F5).
- **Art & great works** carry cultural traits (IV-P1), mark places (Track I-P3 memorials), and
  become objects of prestige and pilgrimage.

*Sociology:* Mauss (gift), Durkheim (effervescence), sociology of ritual.
*Verify:* festival ticks show measurable stress discharge; gift-givers gain prestige without
violence; potlatch cultures show lower homicide at equal inequality.

---

## 8. Cross-cutting — instrumentation for "parallel Earth"

You cannot claim realism you don't measure. Extend the existing end-of-run **Realism Report**
(`MindUpgrade.h:125` counters; `scripts/validate.sh`) with assertions tied to the sociology
targets, each landing *with* its feature:

- **Demography:** right-skewed lifespan with infant-mortality hump; **fertility falls with
  wealth/urbanisation** late-run (demographic transition check).
- **Networks:** Dunbar-capped, ~log-normal social-degree distribution; weak ties carry more
  novel information than strong ties.
- **Inequality:** wealth fits a **power law (Pareto)**; **city sizes fit Zipf**; elite share
  and popular well-being oscillate in **anti-phase** (Turchin).
- **Conflict:** war-death share in a historical band; war causes spread across ≥4 categories;
  at least one multi-generation feud and one conquest per long run.
- **Knowledge:** tech is monotone-non-decreasing *after* writing; at least one run crosses
  into modernity and stays; dark-age count collapses.
- **Affect:** stress-at-death distribution is spread, not spiked at 100; emotion-episode
  incidence in human bands (no permanent-rage or permanent-bliss worlds).
- **Culture:** belief accuracy ~0.7–0.9 (not omniscient, not chaos); isolated regions diverge
  in trait sets; a norm cascade fires only at ≥~25% carriers.

Add to the **Chronicle / Interview / Inspector** (already present, master plan M10): render
`lifeChapters`, `narrativeIdentity`, `senseOfPurpose`, lineage sagas, grievance ledgers,
secular-cycle plots, the Zipf/Pareto curves, and belief-vs-truth overlays — so the *evidence*
of realism is legible in-app, not just in a CSV.

---

## 9. Sequencing & effort

Ordered to the owner's priority (Inner Life first) while respecting dependencies. Effort:
S ≈ ≤3 days, M ≈ 4–8 days, L ≈ 1.5–3 weeks. Every item is strangler-fig and independently
shippable.

| Order | Phase | Track | Eff | Risk | Depends on |
|---|---|---|---|---|---|
| 1 | I-P1 Self that acts (identity, purpose, narrative) | Inner | M | low | — |
| 2 | I-P2 Deep, sticky relationships | Inner | M | low | — |
| 3 | I-P4 Mood dynamics, coping, growth | Inner | M | low | I-P1 |
| 4 | I-P3 Causal legacy | Inner | M | low | I-P2, Kinship |
| 5 | II-P1 Imperishable knowledge / collective brain | Modernity | M | med | — |
| 6 | II-P2 Institutions (wire dead `InstitutionalSystem`) | Modernity | M | med | II-P1 |
| 7 | III-P1 Settlements & cities | Economy | L | med | SpatialGrid |
| 8 | III-P2 Regional markets & trade routes | Economy | L | med | III-P1 |
| 9 | II-P4 Consequential, plural war + grievance ledger | Modernity | M | med | III-P1 |
| 10 | II-P3 Secular cycles & elite overproduction | Modernity | M | med | III, II-P2 |
| 11 | III-P4 Class as heritable reproduction | Economy | M | med | III-P2, Kinship |
| 12 | III-P3 Labour market & demographic transition | Economy | M | low | III-P1/P2 |
| 13 | IV-P1 Culture as content + 25% tipping (wire `CulturalTransmissionSystem`) | Culture | M | med | — |
| 14 | IV-P2 Religion with doctrine/ritual/schism | Culture | M | low | IV-P1 |
| 15 | IV-P3 Language that matters | Culture | M | med | IV-P1 |
| 16 | IV-P4 Ritual, art, festival, gift | Culture | M | low | IV-P1, I-P2 |
| 17 | §8 Realism Report v3 assertions + Chronicle/Inspector surfacing | Cross | M | low | lands piecewise with each phase |

**Recommended first milestone (≈2–3 weeks):** items 1–4 + 5. After it, every agent has a
name-worthy interior that drives choices and a legacy, bonds are human, and knowledge has
begun to ratchet — the two most-felt realism jumps, both low-risk.

**Highest-leverage single fix:** II-P1 (imperishable knowledge) — it is what finally lets a
run *become modern*, the defining failure of the flagship report.

---

## 10. Acceptance test — the "parallel universe" bar

The plan succeeds when, on a long unattended run, **all** of the following hold, and a reader
of the generated Chronicle cannot tell it apart from a stylised human history:

1. At least one civilisation crosses into modernity and **stays** (dark ages → single digits).
2. Wars are plural in cause and consequential — maps change, empires rise and fall, and at
   least one feud burns across generations.
3. Wealth is Pareto, cities are Zipf, and inequality/well-being cycle in anti-phase.
4. You can open any agent and read a coherent life — who they are, what they wanted, whom they
   loved and wronged, what they left behind — grounded entirely in real fields.
5. Cultures, religions and languages are visibly **different between regions** and change in
   punctuated cascades, not smooth drift.
6. Nobody lives permanently pinned at stress 100; festivals, meaning and coping show in the
   affect telemetry.
7. Every result above is checked by an assertion in the Realism Report and every mechanism has
   a kill switch that reproduces the *old* behaviour bit-for-bit when disabled (determinism
   contract intact).

---

## 11. Sources

Sociology / anthropology grounding cited above:

- Peter Turchin — *Cliodynamics* & Structural-Demographic Theory / elite overproduction / secular cycles: [cliodynamics overview](https://peterturchin.com/cliodynamics-is-not-cyclical-history/), [A Structural-Demographic Analysis of American History (PDF)](https://peterturchin.com/wp-content/uploads/2013/09/SDAAS_Sep17.pdf), [Secular Cycles (P2P wiki)](https://wiki.p2pfoundation.net/Secular_Cycles), [SDT revisited, industrialized societies (PMC)](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC10621949/).
- Joseph Henrich & Michael Muthukrishna — cumulative culture / the collective brain: [Innovation in the Collective Brain (Royal Society)](https://royalsocietypublishing.org/rstb/article/371/1690/20150192/22834/Innovation-in-the-collective-brainInnovation-in), [Henrich research page](https://henrich.fas.harvard.edu/research-topics-new/cumulative-cultural-evolution), [*The Secret of Our Success* (Princeton UP)](https://press.princeton.edu/books/paperback/9780691178431/the-secret-of-our-success).
- Mark Granovetter — the strength of weak ties: [AJS abstract](https://www.journals.uchicago.edu/doi/abs/10.1086/225469), [A Network Theory Revisited (1983, PDF)](https://www.academia.edu/7684890/_GRANOVETTER_1983_The_Strength_of_Weak_Ties_A_Network_Theory_Revisited_doi_10_2307202051_). Dunbar's number: [time-resources / social structure (arXiv)](https://arxiv.org/pdf/1605.07305).
- McPherson, Smith-Lovin & Cook — homophily ("Birds of a Feather"): [ResearchGate](https://www.researchgate.net/publication/233822040_Birds_of_a_Feather_Homophily_in_Social_Networks). Thomas Schelling — segregation model: [role of homophily / structural transition (Nature Sci. Reports)](https://www.nature.com/articles/s41598-019-40990-z).
- Robert Axelrod — the evolution of cooperation / tit-for-tat: [cooperation-emergence overview (arXiv)](https://arxiv.org/pdf/2104.11455).
- Damon Centola et al. — 25% critical-mass tipping point for social change (*Science* 2018): [Penn Today](https://penntoday.upenn.edu/news/damon-centola-tipping-point-large-scale-social-change), [Scientific American](https://www.scientificamerican.com/article/the-25-revolution-how-big-does-a-minority-have-to-be-to-reshape-society/), [critical mass dynamics (Nature Comms Physics)](https://www.nature.com/articles/s42005-022-00845-y).
- Marcel Mauss — *The Gift* (obligation to give/receive/repay); Émile Durkheim — collective effervescence & anomie; Pierre Bourdieu — habitus & forms of capital: [Mauss & social solidarity](https://www.ebsco.com/research-starters/anthropology/marcel-mauss), [collective effervescence (Wikipedia)](https://en.wikipedia.org/wiki/Collective_effervescence), [Mauss, Bourdieu & the gift (Springer)](https://link.springer.com/chapter/10.1007/978-3-319-65073-9_4).
- Power-law inequality (Pareto) & Zipf's law of city sizes: [Zipf's law for cities (arXiv)](https://arxiv.org/pdf/1402.2965), [Pareto income & wealth (MIT, PDF)](https://economics.mit.edu/sites/default/files/inline-files/Lecture%208%20-%20Pareto%20Income%20and%20Wealth%20Distributuions.pdf), [Power laws, Pareto & Zipf (arXiv)](https://arxiv.org/pdf/cond-mat/0412004).

*(In-repo grounding: `plans/MASTER_PLAN.md`, `plans/ai-upgrade-2026-07.md`,
`plans/society-layer.md`, `plans/alternate-earth.md`, and the run report
`plans/ASHB2_SIMULATION_REPORT_2026-07-03.md`; code anchors are inline throughout.)*

---

## Appendix A — On delivery

This is a design master-plan, not a code change; per the plan-mode contract nothing in the
repo was modified. On approval, the recommended first action is to copy this file into the
repo's `plans/` directory (e.g. `plans/parallel-earth-upgrade.md`) alongside the sibling
plans so it versions with the code, then execute Track items in the §9 order, each as its own
commit gated by `scripts/validate.sh`.
