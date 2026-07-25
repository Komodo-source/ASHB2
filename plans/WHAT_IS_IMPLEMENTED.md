# ASHB2 — What Is Actually Implemented

*A state-of-the-world document: everything the simulation is made of, and the last plan's
contribution to it. Written 2026-07-26 from the source, not from the plans.*

This is not a roadmap. Every mechanism described below exists in the code, with the file and
function that owns it named so you can go read it. Where a number is quoted it is the number
in the source.

---

## 0. At a glance

| | |
|---|---|
| Language / build | C++17, CMake ≥ 3.10 + MinGW-w64, `-O2` Release by default |
| Size | ~53,000 lines across `src/` |
| Layers | Micro (`Entity` + `FreeWillSystem`) → Macro (`CivilizationEngine`) → World (`world/`) |
| Action catalogue | 71 actions |
| Eras | 10, Stone Age → Modern |
| Biomes | 9, on a noise-generated planet with rivers, fertility, ore |
| Cultural traits | 50 named traits in 5 categories |
| War causes | 5 |
| Settlement tiers | 5: camp → village → town → city → great city |
| Live kill switches | 31 named multipliers, all defaulting to an exact ×1.0 |
| Acceptance tests | 25 red/green assertions printed at the end of every headless run |
| Determinism | Same seed ⇒ byte-identical logs; enforced by `scripts/validate.sh` in CI |

The two design contracts everything obeys:

1. **Determinism.** All randomness flows from the world seed through `BetterRand` /
   `makeStream(master, salt)`. No `rand()`, no `random_device`, no `time()` in sim logic.
   Iteration order is always deterministic. CI runs two identical-seed worlds and diffs the
   logs.
2. **Every feature has an off switch that restores the old world exactly.** Each subsystem
   reads a `g_liveConfig` multiplier and returns *before any RNG draw* when it is 0
   (`LiveConfig.h`). Setting a knob to 0 must reproduce the pre-feature run bit-for-bit —
   which is why kill-switch guards sit at the top of functions and, in the action scorer,
   exclude the action from the candidate list entirely rather than scoring it at zero
   (scoring it would consume different RNG and shift the softmax).

---

# Part I — The last plan: the Parallel-Earth Upgrade

`plans/parallel-earth-upgrade.md` was written against two long flagship run reports that
exposed ten specific ways the world did not resemble Earth. Its thesis was that the machinery
was mostly present but **under-wired or scalarised** — so the work was wiring, deepening and
content, not a rewrite ("strangler fig": every phase compiles, runs, stays deterministic, and
extends an existing system).

## The ten failures it targeted

| | Failure observed in the flagship runs | Status |
|---|---|---|
| F1 | **Stagnation trap** — tech plateaued at ~31, **181 dark ages**, oscillating Medieval↔Modern forever, unable to cross into modernity | II-P1, II-P2, II-P3 |
| F2 | **War was performative** — 860 wars, 1,157 battles, **11 deaths, zero conquests** in 4,950 days | II-P4 |
| F3 | **Monocausal conflict** — **98.8% of wars were holy wars** | II-P4 |
| F4 | **Fragile, jealousy-dominated bonds** — 52% of couples dissolved; jealousy was 50.7% of all social events | I-P2 |
| F5 | **Everyone died stressed** — stress ≈ 95–100 at death from *every* cause | I-P4, IV-P4 |
| F6 | **Culture was one number** (`cultureScore`) | IV-P1 |
| F7 | **No cities** — a tribe was a single `centerX/centerY` point | III-P1, III-P2 |
| F8 | **Language did nothing** — the Lexicon made names and nothing else | IV-P3 |
| F9 | **Inert inner content** — beliefs, ideology and memory were stored but never read back into choice | I-P1, I-P3 |
| F10 | **No law, property, institutions or persistent grievance** | II-P2, II-P4, III-P4 |

## What landed, phase by phase

All 17 phases are in the tree, each with its own kill switch, its own realism assertion, and
its phase tag in the source (`grep -rn "II-P1" src/`).

### Track I — Inner life

**I-P1 · The self that acts** (`MindUpgrade.cpp:283`, `Entity.h:101`, knob `identityMul`)
Each agent carries a `NarrativeIdentity` — dominant value, defining formative memory, a
self-story tag ("survivor", "provider", "seeker") — recomputed at life-stage transitions from
the memories they actually hold and the values they actually act on. It feeds
`identityCongruenceModifier()`, a scoring factor that nudges people toward acting in
character. `senseOfPurpose` is earned from goal progress, social integration, role and faith;
high purpose buffers stress, low purpose feeds anomie. A `LifeChapter` log records birth,
first love, first kill, bereavement, triumph, betrayal, migration, elevation, ruin — an
ordered biography the Interview panel and Chronicle read verbatim.

**I-P2 · Sticky relationships** (`implem_free_will.cpp:4706`, knob `relationshipMul`)
Bonds are tiered and Dunbar-capped, maintained by a reciprocity ledger rather than dominated
by jealousy. Couple resilience is `commitment × 0.6 + min(1, daysTogether/200) × 0.4` — a
long, committed bond with children tolerates a bad day; a shallow or poisoned one still
breaks. Children raise commitment at conception (`+6`) and satisfaction (`+4`).

**I-P3 · Visible causal legacy** (`CivilizationEngine.cpp:4526 recordLegacy`, knob `legacyMul`)
Innovations, tribe and religion foundings, and great works are stamped with a `founderId` and
the founder is remembered by name for as long as the thing lasts. On death, `recordLegacy()`
closes the book: standing passes to descendants, reputation persists in others'
`reputationMap`, unfinished feuds and debts survive, lineage depth is counted from the
founder. Assertion 11 counts *notable* lives — agents whose marks outlast them.

**I-P4 · The felt life** (`MindUpgrade.cpp:249, 874`, knob `moodMul`)
Hedonic adaptation gives every agent a slowly-drifting personal stress baseline, so
populations habituate instead of pinning at 100. A personality-skewed coping repertoire
discharges suppression debt. Formative trauma can scar *or* catalyse growth depending on
support and meaning — two agents with identical genomes and different lives end up
measurably different people.

### Track II — The road to modernity

**II-P1 · Imperishable knowledge & the collective brain** (`CivilizationEngine.cpp:98-190`, knob `knowledgeMul`)
Writing is the hinge. `tribeIsLiterate()` asks *both* tech systems — the emergent innovation
catalogue and the deliberate research tree — because most peoples reach Writing through the
tree, and the first cut of this phase, which only asked the catalogue, left the archive
permanently shut. A `tribeTreeHolds()` equivalence table translates between the two systems'
names for the same achievement ("Iron Working" ↔ "Iron Smelting", "Pottery" ↔ "Clay
Shaping"), which had silently made Philosophy → Scientific Method → Steam Power unreachable
however learned a society became.

`researchClimate()` replaces the flat per-capita invention roll with Henrich's collective
brain: a scholar's occupation `+1.6`, literacy `+0.45`, a school
`+0.8 × legitimacy × efficiency`, the printing press `+0.5`, the scientific method `+1.2`.
Each rung makes the next cheaper — a flat rate becomes the accelerating curve real history
shows.

**II-P2 · Institutions that store and transmit** (`EnvironmentModel.h:265`, `CivilizationEngine.cpp:4681`, knob `institutionMul`)
The dead `InstitutionalSystem` is wired and ticked every civ-day. Schools and archives hold
techniques independently of any living skull and teach the young. Guilds preserve craft
knowledge. A bureaucracy is administrative capacity — it governs strangers the way
acquaintance cannot, and gates how large a society grows before it fissions.

**II-P3 · Secular cycles & elite overproduction** (`CivilizationEngine.cpp:4053`, knob `cycleMul`)
Turchin's structural-demographic theory, made mechanical. Three measured quantities per
people:

- **Popular wellbeing** — food per head, the health and stress of the *poorer half* measured
  against the sorted median, minus Malthusian overshoot (regional population ÷ carrying
  capacity).
- **Elite overproduction** — aspirants ÷ offices. Offices are deliberately scarce and do not
  grow one-for-one with population: `1 + pop/12`, plus one for a standing bureaucracy. (A
  first pass counted the leader, every council seat and every institution — six offices for
  two aspirants in a fifteen-person band, a ratio of 0.31, and a cycle that could never
  start.) Aspirants are the top wealth quintile who are also specialists or hold authority
  above 60.
- **Political stress** — `2.2 × elite surplus + 1.9 × immiseration + 1.2 × excess Gini`,
  bleeding off at 0.55/civ-day when none of them bite. Immiseration is referenced at
  wellbeing 70, not 45: this world's ordinary wellbeing sits near 60, so at the lower
  reference the term was dead almost all the time and the anti-phase never appeared in the
  telemetry.

Past instability 72, with a 150-day cooldown, the stress **discharges as strife**: surplus
elites lose 45% of their fortune, 18 authority and 12 esteem; the confiscated share is
spread across the commons (the levelling that makes a cycle's trough less unequal); 15% of
the purge is lethal. Instability drops 45 and the cycle restarts from a flatter base.

**II-P4 · Consequential, plural war** (knobs `warMul`, `feudMul`)
Five war causes — `WAR_ETHNIC`, `WAR_CONQUEST`, `WAR_RESOURCE`, `WAR_TRIBUTE`, `WAR_BORDER` —
rebalanced so holy war is one among several rather than 98.8%. Casualties actually bite;
territory transfer, conquest, vassalage and tribute fire on decisive outcomes; empires
assemble by conquest and lose to overstretch.

The **grievance ledger** (`Tribe::grievance`) is the durable half. Ordinary `relations` warm
back toward zero once fighting stops, so atrocities were forgotten within a generation.
Grievance is a separate map of concrete harms — battle dead, seized land, subjugation — that
decays only very slowly, drags relations down for generations, and gives war a revenge cause
that outlives everyone who first drew blood (Axelrod's unforgiving tit-for-tat).

### Track III — Economy, cities, class

**III-P1 · Settlements & cities** (`CivilizationEngine.cpp:3470`, knob `cityMul`)
A tribe's home hardens into a *place with a size*. A tier must be earned on three axes at
once — bodies, food surplus per head, built fabric per head — plus, for the upper tiers, the
era's techniques. A town becomes a city at pop ≥ 20, surplus ≥ 1.4, fabric ≥ 0.9, Early
Agriculture or later. Places grow one step at a time and **shrink reluctantly** (25% chance
per civ-day): stone and streets outlive the harvest that paid for them, so a town stays a
town through a lean year before it empties. Because peoples clear those bars at different
times and lose them again in famine and war, sizes spread into a heavy-tailed rank-size
hierarchy rather than converging (assertion 21 fits the Zipf slope).

Size pays back in both directions, exactly as in real urban history — see §III below.

**III-P2 · Regional markets & trade routes** (`CivilizationEngine.cpp:4272`, knob `tradeMul`)
Prices are **local**, not global: stores per head against household need, a glut halving and
a dearth doubling, easing 15% per civ-day so markets have memory. The global market becomes
the emergent aggregate rather than the primitive. Routes are physically constrained —
`pathBetween()` samples the straight line across the planet and a mountain or ice wall stops
a caravan dead, open water stops one whose people have never built a boat (`Sailing`
required). Goods move, prices converge along a live route and re-diverge when war or terrain
cuts it (assertion 12 measures exactly that), and trade wealth produces a merchant class.

**III-P3 · Labour market & demographic transition** (`implem_free_will.cpp:4874`, knob `laborMul`)
Roles match skills and settlement demand rather than dominance alone; guild apprenticeship
locks in specialists. And `fertilityModifier()` implements the single most robust regularity
in modern demography: a prosperous, urban, literate household with children who survive has
markedly fewer of them than a poor rural one that expects to bury some. Late-run societies
stop looking like early ones. (Age still dominates at the individual level — a couple whose
younger partner is under 30 is 1.6× as fertile, under 40 1.25×, so the pyramid refills from
the bottom instead of greying out.)

**III-P4 · Class as heritable reproduction** (`CivilizationEngine.cpp:3627`, `SocialOrder.cpp:129`, knob `classMul`)
Class rests on three legs, not two (Bourdieu): economic position, legal status, and
**cultural capital** — lore skill × 0.35, a school in the tribe +12, specialist +8, esteem
above average, family prestige × 0.20, minus chronic stress. It moves at 2% of the gap per
civ-day: a lifetime's cultivation, not a season's. On death an estate passes by kinship, and
`Kinship.cpp:93` gives the child its house's cultural capital *before it earns any*.
Assertion 15 checks the result is both concentrated (top decile > 25%) and mobile but sticky
(0 < mobility < 50%).

### Track IV — Culture, religion, language

**IV-P1 · Culture as content, with a 25% tipping point** (`EnvironmentModel.cpp:265`, `CivilizationEngine.cpp:3692`, knob `traitMul`)
`cultureScore` was a number that went up. A number cannot diverge between valleys, cannot be
carried home by a caravan, and cannot flip. So culture became a catalogue of **50 named
traits**, bounded at 64 so a person's whole culture is one machine word:

- **16 practices** — Ancestor Veneration, Communal Feasting, Bride-Price, Dowry, Cremation of
  the Dead, Sky Burial, Grave Goods, Age-Set Initiation, Guest-Right, Blood Oath, Ritual
  Bathing, Seasonal Pilgrimage, Storytelling Nights, Gift Exchange, Duelling, Council of Elders
- **10 beliefs** — Sun Worship, River Spirits, The Wheel of Rebirth, Fate-Written Lives, Dream
  Omens, The Evil Eye, Sacred Mountains, Ancestral Judgement, Star-Reading, The Unmade Name
- **8 taboos** — Meat Taboo, Blood Taboo, Silence at Dawn, Forbidden Naming, Exogamy Rule,
  Left-Hand Prohibition, Sacred Grove Ban, Fasting Season
- **8 tastes** — Polyphonic Singing, Geometric Ornament, Spiced Cooking, Fermented Drink,
  Verse Recital, Carved Beadwork, Drum Circles, Letters and Numbers
- **8 fashions** — Face Tattoos, Braided Hair, Dyed Cloth, Bone Piercings, Long Beards,
  Shaven Heads, Copper Bangles, Feather Headdress

The transmission and mutation rates are set by category on purpose. Taboos travel slowly
(0.08–0.13) and hold on once in — a people does not casually stop burying its dead. Fashions
catch and drop fast (0.32–0.36), which is what makes them the things that visibly cascade.
Some traits are `prestigious` (what a household shows off with — these become III-P4's
cultural capital), and some share a **mutual-exclusivity family**: Bride-Price and Dowry are
rival answers to the same question, as are Cremation and Sky Burial, so a people picks a way
rather than accumulating all of them, and monoculture convergence is structurally prevented.

Transmission runs vertically (parent → child, `Kinship.cpp:104` — the strongest channel
culture has), horizontally between peers, and obliquely from elders. Adoption uses Centola's
**25% critical-mass threshold**: below it a trait is a minority quirk, at it the trait
cascades to majority — punctuated change, not smooth drift.

**IV-P2 · Religion with teeth** (`CivilizationEngine.cpp:1153`, `MindUpgrade.cpp:428`, knob `doctrineMul`)
Faiths carry a `Creed` cached on each believer, and `doctrineModifier()` is a scoring factor:
what your faith forbids, demands and blesses actually changes what you do, scaled by personal
devotion — a lukewarm member is barely constrained, a zealot heavily so. Pacifist doctrines
suppress the violence pipeline; ascetic ones curb consumption; high-authority ones boost
obedience. Schisms split along doctrinal drift, inequality and region, with attributed
founders. Assertion 17 measures the action-mix distance between congregations — faiths that
do not produce different behaviour do not count as different faiths.

**IV-P3 · Language that matters** (`CivilizationEngine.cpp:1081`, `world/Lexicon.cpp:136`, knob `languageMul`)
The Lexicon has generated and drifted per-region tongues since world-gen, but nothing ever
*asked* whether two peoples could understand each other, so a technique crossed an
unintelligible border exactly as fast as it passed between neighbours. `mutualIntelligibility()`
now gates the four things language really gates: **diffusion** (a barrier slows the collective
brain where real ones do), **trade** (a caravan whose crew cannot haggle carries less),
**diplomacy** (peoples who cannot talk warm to each other more slowly), and **assimilation**
(a conquered people sharing its conqueror's tongue is absorbed; one that does not stays a
distinct nation under new masters). Contact works both ways: trading and allied speakers
creolise toward each other while isolation lets drift pull them apart, so language boundaries
are themselves an outcome of history. Ethnogenesis makes ethnic groups distinct from tribes.

**IV-P4 · Ritual, art, festival & the gift** (`CivilizationEngine.cpp:3158`, `implem_free_will.cpp:854`, knob `giftMul`)
Festivals fire every ~60 days in a festive people, ~130 in a dour one, and never during
famine — the granary must carry a surplus, and the feast consumes 0.15 per head. What each
celebrant gets: suppression debt halved, joy +18, gratitude +6, stress −8, loneliness −10,
home attachment +3. Then **collective effervescence** on top, scaled by the crowd
(`0.4 + celebrants/25 + tier × 0.15`, capped 1.6): a further stress −7 × crowd, suppression
debt −8 × crowd, active grief −8% × crowd, and `senseOfPurpose` +1.6 × crowd, because
belonging to something larger is meaning and not merely relief. A feast in a full city does
more than the same feast among a handful — synchrony is the mechanism.

`GiveGift` needed its own driver, because prosocial giving has no need that presses for it
the way hunger or loneliness does, and the action sat in the catalogue never once chosen.
Generosity is modelled as a strategy, not a mood: weighted by what the agent can spare
(`min(2.5, wallet/250)`) times how much they care about rank (achievement drive 0.45,
extraversion 0.25, agreeableness 0.30). Gifts create durable reciprocity edges and convert
surplus into prestige — a **non-violent status route competing with the violence pipeline**.
Potlatch burns surplus for standing at the tribe level.

---

# Part II — The whole simulation, layer by layer

## 1. Time and the tick

`SimClock` (`src/core/`) is the unified time base. One day = 60 frames. A *civ tick* gates
the once-per-day macro passes. Above 2,000 living agents, deliberation staggers into
round-robin cohorts (2 cohorts, then 4 above 6,000); per-tick upkeep still runs for everyone.

## 2. Micro — the person (`Entity`, `Entity.h`)

- **Big Five personality** + attachment style (secure / anxious / avoidant / disorganized),
  driving both behaviour and physical movement.
- **Needs and stats** ticking continuously: hunger, fatigue, hygiene, stress, loneliness,
  boredom, mental health, happiness, health, plus a food store with metabolism.
- **Ten discrete emotions** (`EmotionState`, OCC-style): fear, joy, sadness, shame, guilt,
  envy, gratitude, pride, hope, regret — each with an action tendency (fear flees, guilt
  repairs toward the victim, envy status-seeks, gratitude reciprocates). Plus PAD state,
  body language, suppression debt, and grief in Kübler-Ross stages.
- **Eight skills** (`SkillSet`): hunt, gather, farm, craft, heal, fight, oratory, lore — with
  log-shaped practice curves (fast early gains, slow mastery), oblique transmission from
  teachers, and very slow forgetting.
- **Propositional knowledge** (`KnowledgeStore`, 48 facts max): four predicates — someone
  killed, a region has food, something is dangerous, someone is generous — each carrying a
  confidence that decays with retelling, a source (−1 = witnessed myself), and a ground-truth
  tag so the report can measure belief accuracy. Rumour distorts; lies are possible.
- **Memory that lives and dies**: 5 working-memory slots; episodic life memories decaying on
  an `exp(-age/300)` curve and pruned once they stop influencing behaviour; consolidation of
  repeated experience into core beliefs; a semantic index.
- **Theory of mind**: `MentalModelOfOther` per acquaintance, accuracy gated by the observer's
  empathy. Models go stale — an impression from 100 days ago counts for little — and can be
  simply *wrong*, especially when acquired secondhand through gossip. `threatPrediction()`
  reads only the model plus visible body language, so agents act on what they believe.
- **Active deception**: low-integrity agents holding a grudge fake warmth during a positive
  interaction, inflating the target's trust beyond what the interaction earned — at the cost
  of the target's true read on them. Reveal produces a prediction-error snap.
- **Learning**: per-agent Q-learning from outcomes, plus vicarious learning — watching a
  neighbour visibly succeed or fail nudges the observer's own value estimates,
  prestige- and openness-gated.
- **Perception with an attention budget**: stress narrows how many nearby people an agent
  registers (threats first, then strong bonds). A panicked agent genuinely acts on less
  information.

### The decision pipeline (`FreeWillSystem::chooseAction`, `implem_free_will.cpp:1655`)

A subsumption hierarchy — the first tier that fires wins:

1. **Possession** — if the director has taken this agent over, the queued command overrides
   everything.
2. **Reflex** — hard survival thresholds veto all else.
3. **Value/goal alignment** tick.
4. **Habit** — calm, familiar contexts run on cached policy, reward-modulated, with an
   entropy floor against rut lock-in.
5. **Deliberation** — `cognitiveChooseAction` scores the candidate set.

Deliberation blends five additive factors (requirement fitness, need satisfaction, memory
bias, variety, social influence) then applies multiplicative ones: contextual weight,
personality, values, grief, pheromones, environment, norms, emotion tendency, active
intention, **identity congruence (I-P1)**, **doctrine (IV-P2)**, and the **gift driver
(IV-P4)**, plus evolved NEAT instinct on subsistence choices and a ×1.5 bias toward the
day's planned action. The result is picked stochastically. Every weight lives in
`ScoringPriors`, loadable at boot from `bridge/priors_active.txt` — the mind can be retuned
without a recompile, and an absent file is bit-identical to the engine defaults.

Afterwards the outcome is settled into regret/relief (expected vs actual), skill practice,
injury, gossip, intention progress, and a recorded Chain-of-Thought the UI can display.

## 3. Meso — relationships and society

Four per-target relationship vectors (social, desire, anger, couple) grown from proximity and
interaction and decayed over time, Dunbar-capped with personal layers scaled by role and
temperament. Couples carry commitment, satisfaction, trust, suspicion and days together.
Reputation maps, gossip, vendettas.

**Justice is social, not systemic**: violence has an audience. Witnesses form vendettas,
reputations collapse, friendships are cut, a same-tribe killing can end in exile, and
bystanders deter crimes in the first place.

## 4. Macro — the civilization (`CivilizationEngine::tick`, `CivilizationEngine.cpp:193`)

Every tick: dominance ranks, tribes, religions, innovations, tribe relations, diplomacy,
government, war, carrying capacity, climate, era, effects on entities, division of labour,
tech tree, economy resources, culture, festivals, tech diffusion.

Once per civ-day: social classes, **settlements**, **institutions**, **trade**, **secular
cycle**, **class reproduction**, **languages**, **cultural traits**, dynasties, elections,
corruption, colonisation, narrative chains.

Also present: four government forms with real elections, councils, corruption, embezzlement
and scandal; formal treaties; vassalage and rebellion; colonisation into empty land; a ~60-good
market with a Gini; dynasties and family prestige; an ID-based kinship registry with incest
avoidance; patron-client social order with `Clientela` and `Debt`.

Every 25 days the engine writes a **history fingerprint** and a `KnowledgeSample` — the
series assertion 23 uses to prove the knowledge ratchet. It records the *union* of what the
world knows, not the sum over peoples: a tribe dying out while its neighbours hold the same
technique is not the world forgetting anything, and counting it as a loss made the ratchet
look broken every time a band starved.

## 5. World & environment

A noise-generated planet of tiles carrying elevation, temperature, moisture, biome (ocean,
coast, ice, tundra, desert, grassland, forest, jungle, mountain), fertility, ore richness,
river flag and contiguous region id. On top of it: a `ResourceSystem`, a predator-prey
`Ecosystem` damped to avoid violent boom/bust so the economy has a steady backdrop to lean
on, an `EnvironmentModel` driving seasons and harvest luck, a `PheromoneField` for stigmergy,
per-region `Lexicon` tongues, and a `Disease` model with four climate regions.

---

# Part III — What the world actually *does*

These are the characteristic behaviours the parts produce together. Each is a real chain in
the code, not an illustration.

### The post-war baby boom

The one that started this document. Peace fires `endWarFor()`
(`CivilizationEngine.cpp:5410`), which opens a **90-day fertility window** — about a
season-and-a-half — and records how the war went (+1 win / 0 draw / −1 loss). During that
window `postWarBirthBoost()` returns **1.8× for victors and 1.5× for the defeated**: victors
celebrate hardest, but even a beaten people rushes to replace the fallen. The reproduction
code does not just multiply the odds — it *lowers the bar*: the desire gate is
`35.0 / boomBoost`, so during a boom couples conceive on a weaker spark than they normally
would. Three separate paths open the window: a war ending in stalemate (both sides get a
draw), a decisive conquest (the victor gets a win at `conquerTribe`), and a negotiated peace.
The memory of the outcome fades when the window closes. It is the returning-soldier effect,
and it is why the graph of births spikes a season after the graph of deaths does.

### The epidemic that only cities get

Below tier 2 nothing happens — a village is not yet crowded. At town and above, crowding is
`(tier − 1) × sanitation`, where sanitation starts at 1.0 and is bought back down by
technique: Pottery −0.20 (clean water storage), Irrigation −0.25 (waste carried away),
Masonry −0.15 (stone underfoot), floored at 0.35. Each civ-day, `0.004 × crowd` odds seed a
pathogen into up to three inhabitants and the Chronicle logs *"Sickness breaks out in the
crowded city of the …"*. Real towns only became survivable once they were engineered, and
here a great city without drainage pays for its size in outbreaks. The same crowd figure
adds daily stress and loneliness — **urban anomie**, the particular loneliness of being
unknown among many, which `senseOfPurpose` is what buffers.

### The secular cycle

A society grows. Population presses on carrying capacity, so ordinary life gets worse.
Meanwhile the wealthy and the credentialed multiply faster than the offices that exist to
absorb them, and the surplus does not disperse — it competes. Political stress accumulates
from all three (elite competition weighted heaviest: states break from above more often than
from below). Past 72 it discharges: the surplus elite is ruined, its fortunes confiscated and
spread across the commons, a sixth of it killed. Instability falls 45, inequality falls with
it, and the next generation starts from a flatter base. Fields go unworked during the strife,
so the people are poorer afterwards too. Then it begins again — unless the society has
crossed into literacy and institutions, in which case it can ride the cycle *upward* instead.
Assertion 14 checks that wellbeing and instability run in anti-phase.

### The knowledge ratchet

Before writing, a population crash erases what its dead knew — the dark age. After writing,
techniques held by a literate institution survive the people who knew them: a knowledge shock
that kills every scholar in a tribe no longer loses what the archive holds. And the rate of
new knowledge is not a constant but a function of the society's shape — scholars, literacy,
schools, the press, the method — each rung making the next cheaper. That is the difference
between a world that oscillates Medieval↔Modern forever and one that crosses over and stays.

### The feud that outlives everyone who started it

Two peoples fight. Relations crater, then warm back toward neutral once the fighting stops —
that is the ordinary channel, and it forgets. But the battle dead, the seized land and the
subjugation are also written into the grievance ledger, which decays only very slowly. Two
generations later nobody alive remembers the war, relations have long since normalised, and
the grievance is still there dragging them down and supplying a revenge casus belli. The
Chronicle can show a feud reigniting between grandchildren.

### The trait that cascades — or fizzles

A fashion appears in one household. It transmits at 0.34 per contact, mutates at 0.018, and
spreads on peer contact and from elders. If it stalls below 25% of the people, it stays a
minority quirk and eventually dies. If it crosses 25%, it cascades to majority within a
handful of civ-days. Meanwhile the taboo next to it transmits at a third the rate — but once
it is in, it does not leave. And because Bride-Price and Dowry occupy the same exclusivity
family, a people that adopts one drops the other: two valleys that never talk end up with
genuinely different cultures rather than both accumulating everything.

### The price gradient a caravan closes

Two peoples, one with full granaries and one short. Local prices diverge — a glut halves,
a dearth doubles. If the ground between them is passable (no mountain wall, no sea crossing
without `Sailing`) and their tongues are mutually intelligible enough to haggle, a route
opens, goods move, and the gap narrows. Cut the route — a war, a mountain pass closed — and
it widens again. Assertion 12 measures exactly this: linked pairs must show smaller price
gaps than unlinked ones.

### The gift instead of the knife

An agent with a surplus who wants standing has two routes to it. Violence is one. The other
is `GiveGift`: spend the surplus, create a durable reciprocity edge, and gain prestige
without blood. Because generosity here is a strategy weighted by spare wealth × desire for
rank rather than a mood, the peaceful route is genuinely competitive with the violent one —
a structural brake on homicide that operates through incentive rather than sanction. At the
tribe level, potlatch does the same thing with the collective surplus.

### The conquered nation that stays a nation

A people is conquered. If it shares its conqueror's tongue it assimilates. If it does not, it
stays a distinct nation under new masters — its lexicon slowly blending toward the
conqueror's over generations, its grievance ledger full, its traits intact, its language
still gating how fast anything diffuses across the new internal border. Empires assemble by
conquest and then face the secular cycle, and fracture along exactly these seams.

---

# Part IV — How you verify any of it

## The realism report — 25 assertions

Printed at the end of every headless run (`src/main.cpp:2016-2732`). Grep `REALISM` or `FAIL`.

| # | Claim |
|---|---|
| 1 | Violence is rare (homicide share < 15%, or < 4% per birth) |
| 2 | Population sustains (births ≥ deaths) |
| 3 | Wars occur and the world survives |
| 4 | Tribes persist, faiths stabilise |
| 5 | No behavioural monoculture (top action < 25%) |
| 6 | Emotional lives (fear, joy, regret all occur) |
| 7 | Belief ecology (accuracy 0.5–0.995 — not omniscient, not chaos) |
| 8 | Skills differentiate (best-skill Gini > 0.03) |
| 9 | Deception occurs and is sometimes caught |
| 10 | **II-P2** Institutions store knowledge no skull holds |
| 11 | **I-P3** Lives leave marks that outlast them |
| 12 | **III-P2** Trade narrows price gaps (linked < unlinked) |
| 13 | **II-P4** War is consequential (deaths > 0, conquests > 0, share ≤ 30%) |
| 14 | **II-P3** Secular cycle runs (wellbeing vs instability anti-phase) |
| 15 | **III-P4** Class is heritable and sticky (top decile > 25%, mobility 0–50%) |
| 16 | **IV-P1** Culture diverges and tips (peoples differ, traits rise and die) |
| 17 | **IV-P2** Faiths differ in conduct (action-mix distance > 0.05) |
| 18 | **IV-P3** Language gates diffusion (same-tongue rate > cross-tongue) |
| 19 | Lifespans have a human shape (infant hump 1–45%, elders > 5%) |
| 20 | Social circles are Dunbar-capped and uneven (max ≤ 150) |
| 21 | **III-P1** Settlement sizes fit a rank-size law (R² ≥ 0.75) |
| 22 | **II-P4** Wars have plural causes (≥ 4 kinds) |
| 23 | **II-P1** Knowledge ratchets, and nobody dies pinned at stress 100 |
| 24 | **III-P3** Fertility falls with wealth; specialists track settlement size |
| 25 | **IV-P4** Ritual and gift work (stress discharged > 0, gifts > 0) |

Assertions carry soft/hard verdicts — some only bind past a minimum run length, because a
400-tick world has not had time to produce a conquest or a demographic transition.

## The determinism and kill-switch gates

```bash
scripts/validate.sh 400 ci      # build + two identical-seed runs diffed + realism report
```

CI runs this on every push. Separately, every phase must satisfy the kill-switch condition:
`--set <knob>=0` reproduces the pre-feature world **bit-for-bit**. This is stricter than it
looks and has caught real bugs — a feature that merely *scores* a new action at zero rather
than excluding it from the candidate list still consumes different RNG and shifts the
softmax, so the off-state diverges.

## Running it

```bash
cmake -G "MinGW Makefiles" . && mingw32-make -j8
app.exe                                                        # GUI
app.exe --headless 600 --seed 42 --entities 40 --region 1 --chaos 1.3
```

Scenarios: `eden` (gentle, 150 souls), `crucible` (harsh, 60), `babel` (crowded, 400),
`dish` (petri dish, 12). Flags placed after `--scenario` override it.

## Watching it

The GUI ships four intervention panels: **God Console** (smite, bless, torment, feast,
famine, meteor, great calm — every act written into the Chronicle), **Possess** (your command
overrides an agent's reflexes, habits and deliberation until released), **Interview** (six
questions answered from the agent's real feelings, memories, beliefs, goals, narrative
identity, life chapters and opinions of others), and the **Config Console** (the live
multipliers, all defaulting to an exact ×1.0 no-op so determinism holds until you touch
something).

For A/B experiments, run two headless worlds and feed both `tick_history.jsonl` files to
`python scripts/butterfly.py` for a per-day divergence table — the butterfly effect,
measured.

---

# Part V — Honest status

- **All 17 Parallel-Earth phases are in the tree**, each with its kill switch, its realism
  assertion and its phase tags. This document was written by reading the source; it does not
  report the results of a fresh long acceptance run.
- **Known dead code**, carried from the master plan audit and still true: `validation/`,
  `scalability/`, `observability/` and `modules/` compile but are never referenced;
  `SpatialMesh.cpp` and `movement.cpp` are not in the build; `Graph.cpp` has a
  dangling-pointer design and is unused; `Heritage` is superseded by `Kinship`;
  `CommunicationBasis` is one line.
- **Performance** (laptop, Release `-O2`, 8 threads, one tick = 60 frames): 1,000 agents
  ≈ 240 ms/tick; 5,000 ≈ 830 ms; 10,000 ≈ 1,500 ms, i.e. ~25 min for a 1,000-day headless
  run at 10k. The original M11 target was < 10 min at that scale — not met; it was ~76 min
  before the scale pass. The remaining cost is the per-frame force loop; closing it needs an
  SoA hot-data split and/or reduced movement frequency at scale, both of which risk
  behavioural drift and deserve their own pass.
- **Saves** (`ASHB2_SAVE_V2`) carry the sim clock, shared RNG stream, tribes, religions and
  era. Two loads of the same save are identical to each other, but a resumed run is *not*
  bit-identical to the unbroken run, because some subsystems re-derive their private RNGs
  from the world seed at construction.
- **The GUI needs no SDL.** The only render path is GLFW + OpenGL + Dear ImGui + ImPlot;
  the vendored SDL2 tree and the dead SDL render branch were removed on 2026-07-25.

---

## Source map

| Concern | Where |
|---|---|
| Decision pipeline, relationships, reproduction | `src/implem_free_will.cpp` |
| Inner life: emotions, identity, purpose, skills, knowledge, doctrine | `src/ai/MindUpgrade.{h,cpp}` |
| The person | `src/header/Entity.h`, `src/Entity.cpp` |
| Everything civilizational | `src/CivilizationEngine.cpp`, `src/header/CivilizationEngine.h` |
| Cultural traits, institutions | `src/environment/EnvironmentModel.{h,cpp}` |
| Class, estates, patron-client | `src/SocialOrder.cpp` |
| Lineage, inheritance, vertical transmission | `src/Kinship.cpp` |
| Language, planet, ecosystem, resources | `src/world/` |
| Kill switches | `src/header/LiveConfig.h` |
| Realism report, CLI, main loop | `src/main.cpp` |
| CI gate | `scripts/validate.sh`, `.github/workflows/ci.yml` |
| The plan this implements | `plans/parallel-earth-upgrade.md` |
