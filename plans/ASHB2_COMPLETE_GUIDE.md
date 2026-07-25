# ASHB2 — The Complete Guide

*Everything the simulation is made of. Written 2026-07-26 by reading the source.*

ASHB2 is a society in a box. Psychologically detailed individual agents live, form
relationships, reproduce and die on a procedurally generated planet — and their
moment-to-moment decisions emergently produce tribes, religions, cities, economies, wars,
classes, languages and history. Nothing at the civilizational scale is scripted; all of it is
the aggregate of what individual people chose to do.

This document is the canonical map of the project. Every mechanism below exists in the code,
with the file that owns it named. For a deep dive on the most recent work specifically, see
`plans/WHAT_IS_IMPLEMENTED.md`; for the design intent behind it, `plans/parallel-earth-upgrade.md`.

---

## Contents

- [0. At a glance](#0-at-a-glance)
- [I. The shape of the thing](#i-the-shape-of-the-thing)
- [II. The person](#ii-the-person)
- [III. The mind that decides](#iii-the-mind-that-decides)
- [IV. Between people](#iv-between-people)
- [V. Society](#v-society)
- [VI. The world](#vi-the-world)
- [VII. What the world actually does](#vii-what-the-world-actually-does)
- [VIII. How it got here — the plans that built it](#viii-how-it-got-here--the-plans-that-built-it)
- [IX. Instrumentation, tools and verification](#ix-instrumentation-tools-and-verification)
- [X. Honest status](#x-honest-status)
- [Appendix — source map](#appendix--source-map)

---

## 0. At a glance

| | |
|---|---|
| Language / build | C++17, CMake ≥ 3.10 + MinGW-w64, `-O2` Release by default, OpenMP optional |
| Size | ~53,000 lines across `src/` |
| Layers | Micro (`Entity` + `FreeWillSystem`) → Macro (`CivilizationEngine`) → World (`world/`) |
| Render | GLFW + OpenGL + Dear ImGui + ImPlot (no SDL) |
| Action catalogue | **71** actions |
| Drive axes | **5** bipolar, each with two lethal poles |
| Emotions | **10** discrete (OCC-style) + PAD + body language |
| Skills | **8**, with practice curves |
| Personality | Big Five + attachment style + Jungian 8-function stack in Beebe's 8 archetypes |
| Genome | **5** heritable multipliers + epigenetic markers |
| Diseases | **7** named, 4 climate regions, density-driven urban epidemics |
| Innovations | **55** in 6 categories (emergent catalogue) |
| Tech tree | **14** prerequisite-gated nodes (deliberate research) |
| Market goods | **55** in 4 categories |
| Cultural traits | **50** in 5 categories, with mutual-exclusivity families |
| Eras | **10**, Stone Age → Modern |
| Biomes | **9** |
| Government forms | **4** |
| Treaty types | **4** |
| War causes | **5** |
| Settlement tiers | **5**: camp → village → town → city → great city |
| Live kill switches | **31** named multipliers, all defaulting to an exact ×1.0 |
| RNG streams | **8** named salts off one master seed |
| GUI panels | **16** |
| Acceptance tests | **25** red/green assertions per headless run |

---

## I. The shape of the thing

### The three layers

**Micro** — `Entity` (`src/header/Entity.h`, `src/Entity.cpp`) is a person: a body, a
personality, needs, memories, relationships, beliefs, an identity. `FreeWillSystem`
(`src/implem_free_will.cpp`, ~257k) decides what that person does each tick.

**Macro** — `CivilizationEngine` (`src/CivilizationEngine.cpp`, ~349k) aggregates people into
tribes, faiths, settlements, markets, classes, polities and wars. It never *commands* an
agent; it reads what they have done and writes back conditions (famine, war, doctrine,
price) that change what they will do next.

**World** — `src/world/` holds the planet, its biomes and regions, resources, an ecosystem,
per-region languages, and a pheromone field.

### Time

`SimClock` (`src/core/SimClock.h`) is the single time base. **One day = 60 frames.** Movement
and per-tick upkeep run every frame; decisions run per day; the *civ tick* gates the
once-per-day macro passes; some passes (dynasties, classes, settlements, trade, the secular
cycle) are further gated to once per civ-day by a `lastDynastyDay` guard.

Above 2,000 living agents, deliberation staggers into round-robin cohorts (2, then 4 above
6,000) so decision cost stays bounded. Upkeep still runs for everyone, every tick.

### The determinism contract

**Same seed ⇒ byte-identical history.** All randomness flows from one 64-bit master seed
through `BetterRand` and `makeStream(master, salt)`. There is no `rand()`, no
`random_device`, no `time()` anywhere in simulation logic, and iteration order is always
deterministic.

The seed is either a literal number or an FNV-1a hash of a text phrase (`WorldSeed.cpp`), and
it forks into **8 named streams** so that adding a die-roll in one subsystem cannot shift
another's sequence:

`STREAM_TERRAIN`, `STREAM_CULTURE`, `STREAM_INNOV`, `STREAM_DISEASE`, `STREAM_NAMES`,
`STREAM_SPAWN`, `STREAM_MIGRATION`, `STREAM_SOCIAL`.

The build additionally compiles with `-ftrivial-auto-var-init=zero`. This is not paranoia:
determinism was silently broken more than once by an automatic variable read before it was
written, so each run inherited whatever the stack happened to hold. Zero-initialising
untouched locals closes the whole class structurally — an undefined read becomes a defined 0
instead of a per-run lottery — at a fraction of a percent of throughput. It does *not* excuse
uninitialised fields; new POD members still get default member initialisers.

### The kill-switch contract

Every feature has an off switch that restores the *old world exactly*. Each subsystem reads a
`g_liveConfig` multiplier (`src/header/LiveConfig.h`, 31 knobs) and returns **before any RNG
draw** when it is 0. `×1.0f` is exact in IEEE float, so leaving the console untouched is
bit-identical to not having it.

This is stricter than it sounds and has caught real bugs. A feature that merely *scores* a new
action at zero rather than excluding it from the candidate list still consumes different RNG
and shifts the softmax — so the off-state diverges. Hence guards like:

```cpp
if (act.name == "GiveGift" && g_liveConfig.giftMul == 0.0f) continue;
```

`--set <knob>=<value>` sets any knob from the command line, which is what makes the switches
testable headlessly and what `scripts/butterfly.py` uses to measure a feature's divergence.

---

## II. The person

### Body and drives (`src/header/Drive.h`, `src/Drive.cpp`)

Five bipolar axes — **hunger, fatigue, stimulation, social, pleasure** — and this is the part
most simulations get wrong. Each axis has a lethal floor *and* a lethal ceiling:

```
     0 ──────▶  setpoint  ◀────── 1
   death                        death
```

Too little kills; too much also kills. Between them sits a comfort band where wellbeing lives,
and the agent's behavioural pressure is the signed error pushing it back.

Two flavours:

- **Homeostatic (body)** — entropy pushes the value toward one pole. Hunger rises, fatigue
  rises, warmth bleeds away. The agent acts to pull it back. Stability is the goal.
- **Novelty (mind)** — entropy lets the value *decay toward deprivation* (boredom, anhedonia).
  The agent acts to push it up — but pushing it up breeds tolerance, so the bar keeps rising
  and the same input stops reaching comfort. Stimulation is the goal; **dependency is the
  trap.**

Two slow accumulators give the "short-term fine, sustained kills" texture a bare clamp cannot:

- **`load`** — allostatic wear from time spent in the danger zone between the comfort edge and
  the lethal edge. Chronic anger and chronic hunger damage you *before* the hard threshold,
  and the wear lingers after the value returns to comfort.
- **`tolerance`** — habituation on novelty axes. Rises while held high, decays during
  abstinence, and drags the effective setpoint toward the high pole: you need more just to
  feel normal, and further satisfaction pays less.

Axis shape is personality-derived: openness widens the mind's appetite for stimulation,
extraversion raises the social setpoint, neuroticism narrows comfort bands, conscientiousness
slightly slows bodily entropy.

### Personality

**Big Five** (openness, conscientiousness, extraversion, agreeableness, neuroticism) plus an
**attachment style** — secure, anxious, avoidant, disorganized — driving both behaviour and
physical movement.

**The Jungian layer** (`src/header/JungianType.h`, `src/JungianType.cpp`) is the unusual one.
Jung's 8 cognitive functions (Se, Si, Ne, Ni, Te, Ti, Fe, Fi) are ordered into John Beebe's
eight-archetype stack:

| Conscious | Shadow |
|---|---|
| 1 Hero | 5 Opposing |
| 2 Parent | 6 Senex |
| 3 Child | 7 Trickster |
| 4 Anima/Animus | 8 Demon |

Positions 5–8 are the *same* function as their conscious partner with the opposite attitude.
The behavioural payoff is **the grip**: under psychic load the ego falls out of the Hero and
into the inferior function; under extreme, context-specific load a shadow archetype erupts.
Four grip triggers select which one — `GT_CHALLENGED` (contradiction/competition → Opposing),
`GT_AUTHORITY` (judging/controlling → Senex), `GT_DOUBLEBIND` (conflicting bonds → Trickster),
`GT_THREAT` (existential → Demon). It hangs directly off the Drive `load` accumulator, so
**chronic tension literally changes who is driving.**

### Genome and epigenetics

`Genome` (`src/header/Genome.h`) carries five heritable multipliers around 1.0, each applied
at exactly one read site so an all-1.0 genome is bit-identical to the pre-genetics sim:

| Trait | Read site |
|---|---|
| `speed` | movement force |
| `sightRange` | perception radius |
| `metabolism` | hunger growth rate |
| `fertility` | conception odds |
| `resilience` | disease/health damage |

Inheritance is uniform crossover (each gene from a random parent) plus Gaussian mutation
whose sigma scales with `mutationRateMul`, clamped to a viable band so mutation load cannot
produce absurd phenotypes.

On top of that sit **epigenetic markers** (`Entity.h:684`): a trauma source, a methylation
level, a generation offset, an expression level. Markers are acquired from war, famine and
loss, their expression modulated by environmental support, and they are **inherited by
children** — intergenerational trauma as a real mechanism rather than a metaphor.

### Health and disease (`src/Disease.cpp`)

Seven named diseases — Plague, Fever, Malaria, Typhus, Cancer, Leprosy, Smallpox — with four
climate regions setting base incidence. On top of the legacy model sits a `PathogenExposure`
track per agent: pathogen id, exposure day, viral load, and a per-pathogen immunity level, so
survivors carry acquired immunity and reinfection with the same pathogen is blocked while it
is still active. Contagion spreads from sick neighbours; **settlement density seeds outbreaks**
(see §VII).

Bio-homeostasis (`bioHomeostasisMul`) penalises both starvation and overeating.

### Emotions and affect

**Ten discrete emotions** (`EmotionState`, `MindUpgrade.h`), OCC-style, each with an action
tendency:

| Emotion | Appraisal | Tendency |
|---|---|---|
| fear | undesirable + uncontrollable | flee, appease, ally |
| joy | goal progress | share, create, bond |
| sadness | loss (non-grief) | withdraw, seek meaning |
| shame | self-blame, witnessed | withdraw, conform |
| guilt | self-blame, private | repair toward the victim |
| envy | a rival's desirable outcome | status-seek, sabotage |
| gratitude | other-credit for own gain | reciprocate |
| pride | self-credit for own gain | display, persist |
| hope | anticipated good outcome | explore, invest |
| regret | counterfactual "should have" | repair, avoid repeat |

(Anger deliberately stays on the legacy `entityGeneralAnger` stat, which dozens of existing
sites read.) All decay every tick and blend rather than clamp, so mixed feelings are possible.

Alongside them: **PAD state** (pleasure/arousal/dominance) driving visible **body language
cues**, an **emotional suppression debt** that accumulates when feeling is held in,
**grief in Kübler-Ross stages**, and — from I-P4 — **hedonic adaptation**: a slowly-drifting
personal stress baseline so populations habituate instead of pinning at 100, plus a
personality-skewed **coping repertoire** and **post-traumatic change in both directions**
(scarring *or* growth, depending on support and meaning).

### Memory — five distinct kinds

1. **Working memory** — 5 slots (`PersonaSystem.h`).
2. **Episodic life memories** — decay on an `exp(-age/300)` curve and are pruned once they
   stop influencing behaviour.
3. **Semantic memory** (`SemanticMemory.cpp`) — an embedding-vector index over experience
   with similarity queries; repeated experience consolidates into **core beliefs**.
4. **Episodic map** (`src/ai/EpisodicMap.h`) — *spatial* event memory, capped and
   exponentially decayed: food found here, danger seen there, a resource over that ridge.
   This is what "walk back to the berry patch" and "don't cross the battlefield" read.
5. **Propositional knowledge** (`KnowledgeStore`, 48 facts max) — four predicates:
   `FACT_KILLED`, `FACT_FOOD_AT`, `FACT_DANGER`, `FACT_GENEROUS`. Each fact carries a
   confidence that **decays with retelling**, a source (−1 = witnessed it myself), a day, and
   a ground-truth `isTrue` tag purely so the report can measure how accurate the population's
   beliefs are. Rumour distorts. Lies are possible and are counted.

### Skills (`SkillSet`)

Eight: hunt, gather, farm, craft, heal, fight, oratory, lore. Everyone starts at 5/100.
Practice is log-shaped — fast early gains, slow mastery. Teaching transfers a fraction of the
gap from teacher to student (oblique transmission). Forgetting is very slow.

### Theory of mind, and lying

Agents build a `MentalModelOfOther` for each person they interact with, its accuracy gated by
the observer's **empathy**. Models **go stale** — an impression from 100 days ago counts for
little — and can simply be **wrong**, especially when acquired secondhand through gossip.
`threatPrediction()` reads only the model plus visible body language, so agents act on what
they *believe* about you, not on your actual stats.

**Active deception** (`mind::attemptDeception`) is gated to low-integrity agents holding a
grudge against a specific target — the high-stakes cue. During a positive social interaction
they fake warmth, inflating the target's trust beyond what the interaction earned, at the cost
of the target's true read on them. Reveal produces a prediction-error snap. Attempts and
detections are both counted for assertion 9.

### Identity, purpose and biography

`NarrativeIdentity` (`Entity.h:107`) — a dominant value, a defining formative memory, and a
self-story tag ("survivor", "provider", "seeker") — recomputed at each life-stage transition
from the memories the agent actually holds and the values they actually act on. It feeds a
scoring factor, so people act to stay who they believe they are.

`senseOfPurpose` is *earned* from goal progress, social integration, role and faith. High
purpose buffers stress and mental health; low purpose feeds anomie and despair (Durkheim).

`LifeChapter` log — birth, first love, first kill, bereavement, triumph, betrayal, migration,
elevation, ruin — an ordered biography the Interview panel and the Chronicle read verbatim.

### Life course

Five life stages (infant, child, adolescent, adult, elder) with a `DevelopmentalHistory`
recording childhood nurturing, secure attachment, and formative events. Old-age mortality is
**Gompertz**, anchored on a modal adult death age. Infant and child mortality
(`demographyMul`) put the hump at the bottom of the lifespan curve that real demography has.

---

## III. The mind that decides

### The pipeline (`FreeWillSystem::chooseAction`, `implem_free_will.cpp:1655`)

A subsumption hierarchy — the first tier that fires wins:

| | Tier | What it is |
|---|---|---|
| 0 | **Possession** | if the director has taken this agent over, the queued command overrides everything |
| 1 | **Reflex** | hard survival thresholds veto all else |
| 2 | *(value↔goal drift tick)* | goals and values pull on each other |
| 3 | **Habit** | calm, familiar contexts run on cached policy — reward-modulated, with an entropy floor against rut lock-in |
| 4 | **Deliberation** | the cognitive scorer |

There used to be *two* near-duplicate scorers; the ~330-line legacy one was unreachable (the
cognitive path pads its candidate list and never returns null) and the pair only drifted apart
silently. It was deleted in M3.

### Deliberation: how a choice is actually made

Perception comes first, and it is **budgeted**: stress narrows how many nearby people an agent
registers, threats first and then strong bonds. A panicked agent genuinely acts on less
information.

Then each candidate action is scored. Five additive factors:

`requirement fitness · need satisfaction · memory bias · variety · social influence`

then a chain of multiplicative ones:

`contextual weight · personality · values · grief · pheromones · environment · norms ·
emotion tendency · active intention · identity congruence · doctrine · gift driver ·
NEAT instinct · planned-action bias (×1.5) · social rarity`

The result is picked **stochastically**, not argmax — which is why the world does not collapse
into behavioural monoculture (assertion 5 checks the top action stays under 25%).

Afterwards the outcome is settled: expected-vs-actual comparison producing **regret or
relief**, skill practice, injury, gossip, intention progress, habit reinforcement, and a
recorded **Chain-of-Thought** the UI can display, plus a **hesitation state** for morally
heavy choices.

### The tuning surface

Every weight above lives in `ScoringPriors` (`MindUpgrade.h`), loadable at boot from
`bridge/priors_active.txt` as `KEY:value` lines. Defaults reproduce the hardcoded scorer
exactly, so an absent file is the engine's classic behaviour and a `priors_vN` file retunes
the whole mind **without a recompile**. Multiplicative factors are reweighted through
`mind::adjustFactor()`, guarded so weight 1.0 is bit-identical.

This is the human-in-the-loop tuning loop's landing surface: rate a run, fit new priors
offline in Python, drop the file in, re-run.

### Planning — three planners, three jobs

- **Intentions** (`Intention`, B4) — a persistent multi-day pursuit of one life goal, selected
  weekly, abandoned on frustration, biasing every aligned action while active.
- **Tree-of-Thoughts daily planner** (`PlanningSystem.cpp`) — generates alternative day plans,
  evaluates them, commits to one, and biases the matching action by ×1.5.
- **GOAP** (`src/ai/GoapPlanner.cpp`) — genuine A* over action rules. The agent states a goal
  as desired facts (`"stat:hunger ≤ 30"`), and the planner searches world states reachable
  through its *known* `ActionRule`s plus a synthetic Gather step, returning the cheapest
  sequence. Facts are numeric: `tag:<t>` = how many items with that tag I hold, `stat:<s>` =
  an agent stat. Forward-chaining with the admissible heuristic "remaining deficit ÷ best
  single-action improvement", bounded to depth 6 and 256 nodes so a 2,000-agent world can
  afford per-day replans. Deterministic: rules expand in id order, ties broken by id.

### Learning — three channels

- **Q-learning** (`LearningAdaptationSystem`, `rlSystem`) — per-agent action values updated
  from real outcomes, read back as a scoring input.
- **Vicarious learning** — watching a neighbour's action visibly succeed or fail nudges the
  *observer's* value estimates, weighted by the model's prestige and the observer's openness.
  This is cultural transmission at the level of individual policy.
- **Reward-modulated habits** — with satiation and an entropy floor, so agents form routines
  without locking into a single rut.

### NEAT brains (`src/ai/neat/Neat.h`)

An optional evolved neural controller after Stanley & Miikkulainen (2002): genomes are
connection genes with historical innovation numbers, networks grow by structural mutation
(add-connection, add-node), offspring are built by innovation-aligned crossover.

**There is no backpropagation.** Selection pressure is survival and reproduction: agents that
starve never reproduce, so their brains leave the gene pool. Feed-forwardness is guaranteed
structurally — every node carries a `layer` coordinate and connections only go from lower to
strictly higher layer.

Fixed sensor/actuator widths keep genomes crossover-compatible:

- **inputs**: hunger, health, fatigue, food-pheromone gradient x, food-pheromone gradient y,
  danger pheromone, holds-food flag, loneliness, bias
- **outputs**: moveX (tanh), moveY (tanh), forage urge (σ), eat urge (σ)

`neatBrainShare` controls what fraction of newborns are wired with one; it defaults to **0**,
so NEAT is opt-in.

### Narration

`NarrativeEngine` turns each action into a readable English sentence with time and place, and
generates a first-person **inner monologue** line from the dominant emotional state. This is
what the log panel and the Chronicle read.

---

## IV. Between people

### The relationship substrate

Four per-target vectors on every agent — **social**, **desire**, **anger**, **couple** — grown
from proximity and interaction, decayed over time, and Dunbar-capped with personal layers
scaled by role and temperament (`networkMul`). Couples carry commitment, satisfaction, trust,
suspicion and days together.

Ties are **tiered** (intimate / close / weak) with different maintenance costs and decay
rates, so a few bonds are deep and durable while many stay light. A **reciprocity ledger**
per dyad accrues favours, slights, gifts and betrayals, making relationship deltas
nice-retaliatory-forgiving (Axelrod) rather than jealousy-dominated. **Weak ties** carry
gossip, role opportunities and innovation across group boundaries — the bridges homophilous
strong ties cannot provide (Granovetter).

### Love, and why it lasts

Couples have companionate and passionate components that mature over `daysTogether`. Children
raise commitment (+6) and satisfaction (+4) at conception. Dissolution resilience is:

```
resilience = commitment/100 × 0.6  +  min(1, daysTogether/200) × 0.4
```

A long, committed bond with children tolerates a bad day; a shallow or poisoned one still
breaks. Grief on a partner's death scales with the depth of shared history.

### Justice without a state

Violence has an **audience**. Witnesses form vendettas, reputations collapse, friendships are
cut, a same-tribe killing can end in exile, and bystanders deter crimes before they happen.
Reputation drives ostracism. This is `applySocialSanction` (M5), and it is what took homicide
from 83% of deaths down to 11–14%.

Gossip propagates *beliefs*, not facts — a rumour arrives with reduced confidence and a
source, and can be false.

### The gift (`implem_free_will.cpp:854`)

An agent with a surplus who wants standing has two routes to it. One is violence. The other is
`GiveGift`, which creates a durable reciprocity edge and converts surplus into **prestige**.

Generosity here is modelled as a **strategy, not a mood**: weighted by what the agent can
spare (`min(2.5, wallet/250)`) times how much they care about rank (achievement drive 0.45,
extraversion 0.25, agreeableness 0.30). This mattered — prosocial giving has no need that
presses for it the way hunger or loneliness does, so without an explicit driver the action sat
in the catalogue and was **never once chosen** in a full run. With it, the peaceful status
route is genuinely competitive with the violent one: a structural brake on homicide operating
through incentive rather than sanction.

### Kinship and dynasty (`src/Kinship.cpp`)

An **ID-based** registry — not pointers — so it survives the entity vector reallocating.
Families, lineage depth counted from the founder, incest avoidance, and family prestige that
accrues over generations. On death an estate passes by kinship, and a child receives its
house's **cultural capital before it earns any**. Named lineages give the Chronicle the spine
of a multi-generation saga.

---

## V. Society

`CivilizationEngine::tick` runs, every tick: dominance ranks → tribes → religions →
innovations → tribe relations → diplomacy → government → war → carrying capacity → climate →
era → effects on entities → division of labour → tech tree → economy → culture → festivals →
tech diffusion.

Once per civ-day: social classes → settlements → institutions → trade → secular cycle → class
reproduction → languages → cultural traits → dynasties → elections → corruption →
colonisation → narrative chains.

### Tribes

Cultural values, a leader, a granary, material and luxury stocks, a territory centre, a
festivity level, a religion, a language, a settlement tier. Newborns are enrolled in their
parents' tribe, which is what makes tribes persist across generational turnover. Small tribes
dissolve; large ones split.

### Government and politics

Four forms, each crowning a different virtue:

| Form | Leader selection | Character |
|---|---|---|
| Democracy | broad approval | low unrest, coups are peaceful votes |
| Authoritarian | fear | heavy taxes tolerated, coups bloody |
| Divine monarchy | devotion, legitimated by the dominant faith | spiritualism stabilises |
| Oligarchy | wealth | wealth buys the throne, poverty breeds revolt |

With: real **elections** with ballots and councils, taxation, **corruption** (graft,
embezzlement, scandal, downfall), legitimacy, coups, vassalage and rebellion.

### Class — three legs, not two

Two class systems were unified. A person's position is now:

- **economic** — wealth percentile;
- **legal** — `CLASS_SLAVE` / `CLASS_PLEBEIAN` / `CLASS_PATRICIAN`, a Roman-esque ladder where
  the slide into debt-bondage is far easier than the climb out;
- **cultural capital** (Bourdieu) — lore skill × 0.35, a school in the tribe +12, specialist
  +8, esteem above average, family prestige × 0.20, minus chronic stress. It moves at 2% of
  the gap per civ-day: a lifetime's cultivation, not a season's.

Plus **Clientela**, the patron–client bond: a directed obligation where the patron extends
protection, coin and food and the client returns labour, military support and political
backing. And **Debt**, whose cascade is what produces bondage.

Inheritance passes wealth, land, titles *and* cultural capital, so advantage compounds and
inequality is heritable and sticky — Pareto shape that persists rather than resetting each
generation.

### Labour and specialisation

Surplus creates specialists; famine sends them back to the fields. Roles match skills and
settlement demand, with opportunity carried on weak ties, and guild apprenticeship locking in
craft. Specialist mix tracks settlement size.

### Institutions (`src/environment/EnvironmentModel.h:265`)

FAMILY / EDUCATION / GOVERNMENT / ECONOMY / RELIGION, each carrying **legitimacy** and
**efficiency**. Schools and archives hold techniques independently of any living skull and
teach the young. Guilds preserve craft knowledge. A bureaucracy is administrative capacity —
it governs strangers the way acquaintance cannot, and gates how large a society grows before
it fissions.

### The secular cycle (Turchin)

Three measured quantities per people, per civ-day:

- **Popular wellbeing** — food per head, the health and stress of the *poorer half* against
  the sorted median, minus Malthusian overshoot (regional population ÷ carrying capacity).
- **Elite overproduction** — aspirants ÷ offices. Offices are deliberately scarce and do not
  scale one-for-one with population: `1 + pop/12`, plus one for a standing bureaucracy.
  Aspirants are the top wealth quintile who are also specialists or hold authority above 60.
- **Political stress** — `2.2 × elite surplus + 1.9 × immiseration + 1.2 × excess Gini`,
  bleeding off at 0.55/civ-day when none of them bite.

Past instability 72, with a 150-day cooldown, it **discharges as strife**: surplus elites lose
45% of their fortune, 18 authority and 12 esteem; the confiscated share is spread across the
commons; 15% of the purge is lethal. Instability drops 45, inequality falls with it, and the
next generation starts from a flatter base. Fields go unworked during the strife, so ordinary
households are poorer afterwards too.

A society that has reached literacy and institutions can ride the cycle *upward* into
modernity. One that has not just keeps cycling.

### Religion

Founded by prophets **in spiritual vacuums** (founding is saturation-gated, which stopped the
168-founded/162-extinct churn), spread by conversion, and genuinely **extinct** when the
congregation scatters. Doctrinal axes: militarism, tolerance, asceticism, authority,
afterlife-focus, plus a moral code and a ritual.

Doctrine has **teeth**: each believer carries a cached `Creed`, and `doctrineModifier()` is a
scoring factor scaled by personal devotion — a lukewarm member is barely constrained, a zealot
heavily so. Pacifist faiths suppress the violence pipeline; ascetic ones curb consumption;
high-authority ones boost obedience and legitimacy. Assertion 17 measures the action-mix
distance between congregations, because a faith that does not produce different behaviour is
not a different faith.

Schisms fire on doctrinal drift, inequality and region, with attributed founders. Syncretism
blends faiths on contact.

### Culture as content

50 named traits in 5 categories, bounded at 64 so a person's whole culture is one machine
word:

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

Rates are set by category **on purpose**. Taboos transmit at 0.08–0.13 and hold on once in —
a people does not casually stop burying its dead. Fashions catch and drop at 0.32–0.36, which
is what makes them the things that visibly cascade. Some traits are `prestigious` (what a
household shows off with — these become cultural capital). Some share a **mutual-exclusivity
family**: Bride-Price and Dowry are rival answers to the same question, as are Cremation and
Sky Burial, so a people picks a way rather than accumulating everything — which is what
structurally prevents monoculture convergence.

Transmission is **vertical** (parent → child, the strongest channel), **horizontal** (peers)
and **oblique** (elders → young), with a mutation rate. Adoption uses Centola's **25%
critical-mass threshold**: below it a trait is a minority quirk; at it, it cascades to
majority. Punctuated change, not smooth drift.

### Language (`src/world/Lexicon.cpp`)

Per-region tongues with real phonotactics, generated at world-gen, drifting every 40 days and
creolising on contact. `mutualIntelligibility()` gates four things:

- **diffusion** — techniques and practices travel on understanding, so a language barrier
  slows the collective brain exactly where real ones do;
- **trade** — a caravan whose crew cannot haggle carries less;
- **diplomacy** — peoples who cannot talk warm to each other more slowly;
- **assimilation** — a conquered people sharing its conqueror's tongue is absorbed; one that
  does not stays a distinct nation under new masters.

Contact works both ways: trading and allied speakers creolise toward each other while
isolation lets drift pull them apart, so **language boundaries are themselves an outcome of
history**, not a fixed backdrop. Ethnogenesis makes ethnic groups distinct from tribes.

### Settlements

A tribe's home hardens into a place with a size: **camp → village → town → city → great city**.
A tier must be earned on three axes at once — bodies, food surplus per head, built fabric per
head — plus, for the upper tiers, the era's techniques. City requires pop ≥ 20, surplus ≥ 1.4,
fabric ≥ 0.9, Early Agriculture or later.

Places grow one step at a time and **shrink reluctantly** (25% per civ-day): stone and streets
outlive the harvest that paid for them, so a town stays a town through a lean year before it
empties. Because peoples clear those bars at different times and lose them again in famine and
war, sizes spread into a heavy-tailed rank-size hierarchy (assertion 21 fits the Zipf slope).

Size pays back both ways — agglomeration (research, culture and administrative reach rise with
tier) and crowding (epidemics, stress, urban anomie).

### Economy

A supply-and-demand market of **55 goods** in four categories (FOOD, OBJECT, DEF_OBJECT,
ATK_OBJECT), each with a base price, a war-ration flag and a farmed flag (needs agriculture).
Price drifts up when demand exceeds supply and down when shelves are full; wars distort it.

But prices are **local**, not global. Each people prices food and goods from stores per head
against household need — a glut halves, a dearth doubles — easing 15% per civ-day so markets
have memory. The global market is the *emergent aggregate*, not the primitive.

**Trade routes** are physically constrained: `pathBetween()` samples the straight line across
the planet, a mountain or ice wall stops a caravan dead, and open water stops one whose people
have never built a boat (`Sailing` required). Goods move along live routes, prices converge,
and cutting a route re-widens the gap. Trade wealth produces a merchant class. Money, debt and
credit ride on `SocialOrder`'s `Debt`.

`wealthGini` is computed continuously and feeds the secular cycle.

### Knowledge — two tech systems and one ratchet

**The emergent catalogue** — 55 innovations discovered by *individuals*, prerequisite-chained,
in 6 categories: agriculture (5), medicine (9), military (8), social (14), spiritual (6),
tool (13). From Seed Selection and Fire Making up through Metal Working, Quarantine, Writing,
Philosophy, Mathematics, the Scientific Method and Steam Power.

**The deliberate tree** — 14 prerequisite-gated nodes researched by *peoples*: Toolmaking,
Foraging Lore, Fire Mastery, Agriculture, Animal Husbandry, Pottery, Masonry, Bronze Working,
Writing, Irrigation, Iron Working, Mathematics, Currency, Fortification. Unlocking costs both
research points and stockpiled food, so the economy gates advancement.

The two grew up separately and named the same human achievements differently — the tree's
"Iron Working" is the catalogue's "Iron Smelting", its "Pottery" is "Clay Shaping" — and
nothing translated between them. The consequence was severe and invisible: the catalogue's
whole upper half is chained off Writing and Mathematics, both of which most peoples reach
through the *tree*, so a society that had researched writing and geometry the hard way still
counted as having neither. Philosophy, and therefore the Scientific Method, and therefore
Steam Power, were unreachable in practice no matter how learned a society became. An
equivalence table now translates: smelting iron is smelting iron however you came to it.

**Writing is the hinge.** Before it, standing on the shoulders of giants means having
personally met the giant — a chain like Oral Record → Writing → Philosophy → Mathematics →
Scientific Method → Steam Power has to fit inside one skull and one lifetime, which is why it
never did. After it, the people's own knowledge and its school's archive are consultable by
anyone, long after the knowers are dead, and the chain can be assembled across generations.

And the *rate* is not a constant. `researchClimate()` implements Henrich's collective brain:
a scholar's occupation **+1.6**, literacy **+0.45**, a school **+0.8 × legitimacy ×
efficiency**, the printing press **+0.5**, the scientific method **+1.2**. Each rung makes the
next cheaper — a flat per-capita rate becomes the accelerating curve real history shows.

Innovations diffuse tribe→tribe, gated by contact, trade and language.

### War

Five causes — `WAR_ETHNIC` (clashing faiths and ancient hatreds), `WAR_CONQUEST` (a strong
militarist simply takes what a weak neighbour has), `WAR_RESOURCE` (a starving tribe fights a
fat-granaried one), `WAR_TRIBUTE` (a vassal throws off its overlord, or an overlord punishes a
defaulter), `WAR_BORDER` (ordinary friction between mismatched cultures). Assertion 22
requires at least four kinds to actually occur.

Battles draw blood; decisive outcomes fire territory transfer, conquest, vassalage, tribute
and population displacement. Conquered populations migrate, assimilate or resist. Empires
assemble by conquest (≥3 vassals) and are lost to overstretch.

The **grievance ledger** is the durable half. Ordinary `relations` warm back toward zero once
fighting stops, so atrocities were forgotten within a generation. Grievance is a separate map
of concrete harms — battle dead, seized land, subjugation — decaying only very slowly, dragging
relations down for generations, and supplying a revenge casus belli that outlives everyone who
first drew blood.

### Diplomacy

Four formal treaty types distinct from the emergent stance drift: `TREATY_PEACE` (ends a war
and bars its rekindling for a term), `TREATY_ALLIANCE` (mutual defence), `TREATY_TRADE`
(ongoing exchange growing both granaries and relations), `TREATY_TRIBUTE` (food paid to avoid
being raided). Plus stances, ethnic-war markers, and colonisation into empty land.

---

## VI. The world

### The planet (`src/world/Planet.cpp`, `Noise.cpp`)

Noise-generated tiles, each carrying elevation (−1..1, below 0 is ocean), temperature (poles →
equator), moisture, **biome** (ocean, coast, ice, tundra, desert, grassland, forest, jungle,
mountain), fertility, ore richness, a river flag, and a contiguous **region id**. Regions are
the unit that carrying capacity, migration, language and disease climate all key off.

The map is drawn in the World Map panel with a History & Report panel beside it.

### Ecosystem and resources

A predator–prey `Ecosystem` deliberately **damped** to avoid violent boom/bust, so the food
chain is a steady backdrop the economy can lean on rather than a source of noise. A
`ResourceSystem` for regional extractables. An `EnvironmentModel` driving seasons, temperature
and **harvest luck** (good year vs bad year: drought, blight, bumper crop) — climate genuinely
gates survival.

`updateClimate` produces droughts, floods, quakes and eruptions. `updateCarryingCapacity`
produces famine, migration and dark ages.

### Stigmergy (`src/world/PheromoneField.h`)

A coarse three-channel scalar field over the world — **FOOD**, **DANGER**, **SOCIAL**. Agents
*deposit* when something notable happens where they stand; the field *decays* every simulation
day; other agents *sample the gradient* and let it bias movement. Unlike the legacy
agent-borne pheromone (which vanished with its carrier), field deposits persist in the
environment after the agent leaves — **true stigmergy**. Deterministic: plain arrays mutated in
caller order, no RNG.

The NEAT sensor vector reads this field directly, which is how evolved brains learn to follow
trails.

### Items and invention (`src/items/ItemSystem.h`)

No item classes, no hardcoded verbs. An `ItemDef` is a bag of **tags** plus a bag of numeric
**properties**. An `ActionRule` is **preconditions** (tags consumed or required) plus
**effects** (items produced, stats changed). Everything an agent can hold, eat, craft or
invent flows through those two tables.

- Seeded defs and rules import the existing market catalogue, so the old economy and the item
  layer share one vocabulary.
- **Invented** defs are generated at runtime by combining two inventory items — tag union,
  property blend — with success gated by the same `(18/complexity)²` law the innovation
  catalogue uses, so invention pacing matches civilizational discovery pacing.
- Item **value is never stored**. `subjectiveValue()` computes it per-agent, per-moment from
  need-response curves: a starving agent literally prices "food" tags exponentially higher.

---

## VII. What the world actually does

These are the characteristic behaviours the parts produce together. Each is a real chain in
the code.

### The post-war baby boom

Peace fires `endWarFor()`, opening a **90-day fertility window** — about a season and a half —
and recording how the war went (+1 win / 0 draw / −1 loss). During it, `postWarBirthBoost()`
returns **1.8× for victors and 1.5× for the defeated**: victors celebrate hardest, but even a
beaten people rushes to replace the fallen. And it does not merely raise the odds — it *lowers
the bar*: the desire gate becomes `35.0 / boomBoost`, so couples conceive on a weaker spark
than they normally would. Three paths open the window: a stalemate (both sides get a draw), a
decisive conquest (the victor gets a win), and a negotiated peace. The memory of the outcome
fades when the window closes. It is the returning-soldier effect, and it is why the birth
curve spikes a season after the death curve does.

### The epidemic only cities get

Below tier 2, nothing — a village is not yet crowded. At town and above, crowding is
`(tier − 1) × sanitation`, where sanitation starts at 1.0 and is bought down by technique:
Pottery −0.20 (clean water storage), Irrigation −0.25 (waste carried away), Masonry −0.15
(stone underfoot), floored at 0.35. Each civ-day, `0.004 × crowd` odds seed a pathogen into up
to three inhabitants and the Chronicle logs *"Sickness breaks out in the crowded city of the
…"*. Real towns only became survivable once they were engineered; here a great city without
drainage pays for its size in outbreaks. The same crowd figure adds daily stress and
loneliness — **urban anomie**, the particular loneliness of being unknown among many, which
`senseOfPurpose` is precisely what buffers.

### The secular cycle

A society grows. Population presses on carrying capacity, so ordinary life gets worse.
Meanwhile the wealthy and the credentialed multiply faster than the offices that exist to
absorb them, and the surplus does not disperse — it competes. Political stress accumulates
from all three, elite competition weighted heaviest (states break from above more often than
from below). Past the threshold it discharges: the surplus elite is ruined, its fortunes
confiscated and spread across the commons, a sixth of it killed. Instability falls, inequality
falls with it, and the cycle restarts from a flatter base — unless the society has crossed
into literacy and institutions, in which case it rides the cycle upward instead.

### The knowledge ratchet

Before writing, a population crash erases what its dead knew: the dark age. After writing,
techniques held by a literate institution survive the people who knew them — a knowledge shock
that kills every scholar in a tribe no longer loses what the archive holds. And because
`researchClimate()` compounds, the societies that get there first pull away. That is the
difference between a world that oscillates Medieval↔Modern forever and one that crosses over
and stays.

The engine samples the world's total knowledge every 25 days as the **union** of what all
peoples hold, not the sum — a tribe dying out while its neighbours still know the same
technique is not the world forgetting anything, and counting it as a loss made the ratchet
look broken every time a band starved.

### The feud that outlives everyone who started it

Two peoples fight. Relations crater, then warm back toward neutral — the ordinary channel, and
it forgets. But the battle dead, the seized land and the subjugation are also written to the
grievance ledger, which decays only very slowly. Two generations later nobody alive remembers
the war, relations have long since normalised, and the grievance is still there dragging them
down and supplying a revenge casus belli. The Chronicle can show a feud reigniting between
grandchildren.

### The trait that cascades — or fizzles

A fashion appears in one household. It transmits at 0.34 per contact, mutates at 0.018, and
spreads on peer contact and from elders. Below 25% of the people it stays a minority quirk and
eventually dies. Above 25% it cascades to majority within days. The taboo next to it transmits
at a third the rate — but once in, it does not leave. And because Bride-Price and Dowry occupy
the same exclusivity family, a people that adopts one drops the other: two valleys that never
talk end up with genuinely different cultures rather than both accumulating everything.

### The price gradient a caravan closes

Two peoples, one with full granaries and one short. Local prices diverge. If the ground between
them is passable — no mountain wall, no sea crossing without `Sailing` — and their tongues are
mutually intelligible enough to haggle, a route opens, goods move, and the gap narrows. Cut
the route with a war or a closed pass and it widens again. Assertion 12 measures exactly this.

### The gift instead of the knife

An agent with a surplus who wants standing can take it by violence or buy it with generosity.
Because giving is weighted by spare wealth × desire for rank rather than by mood, the peaceful
route is genuinely competitive — a brake on homicide that works through incentive rather than
sanction. At the tribe level, potlatch does the same with the collective surplus.

### The conquered nation that stays a nation

A people is conquered. If it shares its conqueror's tongue it assimilates. If not, it stays a
distinct nation under new masters — lexicon slowly blending toward the conqueror's, grievance
ledger full, traits intact, language still gating how fast anything diffuses across the new
internal border. Empires assemble by conquest, face the secular cycle, and fracture along
exactly these seams.

### The feast where a people breathes out

Festivals fire every ~60 days in a festive people, ~130 in a dour one, never during famine.
Each celebrant gets suppression debt halved, joy +18, gratitude +6, stress −8, loneliness −10.
Then **collective effervescence** scaled by the crowd (`0.4 + celebrants/25 + tier × 0.15`,
capped 1.6): a further stress −7 × crowd, suppression debt −8 × crowd, active grief −8% ×
crowd, and purpose **+1.6 × crowd** — because belonging to something larger is meaning and not
merely relief. A feast in a full city does more than the same feast among a handful. Synchrony
is the mechanism, and this is the population-scale answer to a world where everyone used to
die at stress 100.

### The grip

An agent under sustained load — chronic hunger, chronic anger, a bond in double-bind — watches
its Drive `load` accumulator climb. Past a threshold the ego falls out of its Hero function
into its inferior one, and under the right trigger a shadow archetype takes over: the
challenged agent turns Opposing, the threatened one turns Demon. The same person, stressed
long enough, becomes a different actor. It is one of the few places in the sim where
personality is not a constant.

### The drive that kills from both ends

An agent starved of stimulation dies of it. An agent flooded with pleasure builds tolerance,
needs more to feel normal, pushes past the comfort edge, accumulates allostatic load, and dies
of *that*. Between the two deaths is a band, and the band moves as tolerance rises. It is the
cleanest piece of psychological modelling in the codebase.

---

## VIII. How it got here — the plans that built it

Seven plan documents in `plans/`, executed roughly in this order. Each is still on disk.

### `MASTER_PLAN.md` — the forensic audit and M1–M12 rebuild (all done)

| | Milestone | Delivered |
|---|---|---|
| M1 | Stop the bleeding | use-after-free fixes, single grief pass, pointer repair, −4k lines of dead code; 600 ticks crash-free |
| M2 | Core engine | `SimClock`, the CLI, determinism root-caused to uninitialised fields |
| M3 | One decision core | legacy scorer deleted (−310 lines), unified subsumption pipeline, static action catalogue |
| M4 | Perception & memory | `SpatialGrid` (O(n²) scans killed), percept budget, episodic decay + pruning, mental-model staleness; `-O0`→`-O2` and buffered I/O made it **18× faster** |
| M5 | Social layer | `applySocialSanction` (vendetta/reputation/exile), gossip→belief propagation, ostracism, bystander deterrence, Gompertz mortality; **homicide 83% → 11–14% of deaths** |
| M6 | Learning | vicarious Q-updates with prestige bias, reward-modulated habits, entropy floor + satiation |
| M7 | Macro rebalance | grievance-driven wars, religion vacuum-gating and extinction, tribe persistence fixed |
| M8 | Persistence | Save V2 (clock + RNG + tribes + religions + era), `--load/--save-at/--save-file` |
| M9 | Observability | the end-of-run realism report (5 assertions at the time) |
| M10 | Interactivity | God Console, Possess, Interview, live Config Console, `--scenario` presets, `butterfly.py` |
| M11 | Scale | OpenMP snapshot-parallel movement, staggered cohorts, 48px grid, cached lookups. 1k/5k/10k agents: 240/830/1500 ms per tick, down from 423/2405/4588 |
| M12 | Hardening & docs | `scripts/validate.sh`, GitHub Actions CI, README and `ARCHITECTURE.md` |

### Emergence upgrade, Steps 2–5

Data-driven items and action rules (`ItemSystem`), GOAP planning, environmental stigmergy
(`PheromoneField`), heritable `Genome`, and NEAT brains.

### `ai-upgrade-2026-07.md` — Phases A–E

- **A** — reclaim dormant intelligence: seed/RNG unification, utility-AI wiring, cultural
  transmission live, orphan triage.
- **B** — deeper minds: discrete emotions with action tendencies (B1), prospection and regret
  (B2), active theory of mind (B3), hierarchical intentions (B4), metacognitive effort control
  (B5).
- **C** — learning, culture, communication: generalising RL (C1), skills with practice curves
  (C2), propositional communication and the knowledge economy (C3), NEAT expansion (C4).
- **D** — sensory and embodied realism: sensory channels, sleep/circadian, place attachment.
- **E** — data-driven priors, run counters, motive naming.

### `society-layer.md`

Universal roles and an integrity trait, a complete role economy, voting/elections/councils,
and power — real challenges, succession and the perks of office (which is where corruption,
graft and scandal came from).

### `alternate-earth.md`

The procedural planet, per-region `Lexicon` tongues, biome-driven tribal value drift, and the
World Map / History & Divergence panels.

### `human-in-the-loop-upgrade.md` + `webapp-human-sim.md`

A four-layer loop letting a real person be injected into the world and rate what happens:

- **User-input layer** — personality → character.
- **Simulation-run layer** — the `bridge/` file channel (`world.txt` the one persistent save,
  `inject.txt` PHP→engine for new characters and nudges, `report.json` engine→PHP for the
  daily report, `scheduler.log`).
- **Rating layer** — coarse per-day and granular per-action feedback.
- **AI upgrade loop** — offline Python fits a new `priors_vN.txt` (action-utility deltas,
  drive setpoint deltas, Big-Five × action multiplicative biases) which the engine loads at
  boot. A/B tasting without breaking determinism.

The web side is PHP 8 + PDO + MySQL, no framework, driven by a 6-hour scheduler heartbeat.

### `parallel-earth-upgrade.md` — the last plan, 17 phases

Diagnosed ten failures (F1–F10) from two long flagship runs and fixed each with a phase.
Highlights: the flagship run had **181 dark ages** and could not cross into modernity; **860
wars produced 11 deaths and zero conquests**; **98.8% of wars were holy wars**; 52% of couples
dissolved; everyone died at stress 95–100; culture was one number; there were no cities; the
Lexicon did nothing; beliefs never fed back into choice; there was no persistent grievance.

All 17 phases (I-P1…I-P4, II-P1…II-P4, III-P1…III-P4, IV-P1…IV-P4, plus §8 instrumentation)
are in the tree, each with a kill switch, a realism assertion and phase tags in the source
(`grep -rn "II-P1" src/`). See `plans/WHAT_IS_IMPLEMENTED.md` for the phase-by-phase detail.

---

## IX. Instrumentation, tools and verification

### The realism report — 25 assertions

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
| 10 | Institutions store knowledge no skull holds |
| 11 | Lives leave marks that outlast them |
| 12 | Trade narrows price gaps (linked < unlinked) |
| 13 | War is consequential (deaths > 0, conquests > 0, share ≤ 30%) |
| 14 | Secular cycle runs (wellbeing vs instability anti-phase) |
| 15 | Class is heritable and sticky (top decile > 25%, mobility 0–50%) |
| 16 | Culture diverges and tips (peoples differ, traits rise and die) |
| 17 | Faiths differ in conduct (action-mix distance > 0.05) |
| 18 | Language gates diffusion (same-tongue rate > cross-tongue) |
| 19 | Lifespans have a human shape (infant hump 1–45%, elders > 5%) |
| 20 | Social circles are Dunbar-capped and uneven (max ≤ 150) |
| 21 | Settlement sizes fit a rank-size law (R² ≥ 0.75) |
| 22 | Wars have plural causes (≥ 4 kinds) |
| 23 | Knowledge ratchets, and nobody dies pinned at stress 100 |
| 24 | Fertility falls with wealth; specialists track settlement size |
| 25 | Ritual and gift work (stress discharged > 0, gifts > 0) |

Assertions carry soft/hard verdicts — some only bind past a minimum run length, because a
400-tick world has not had time to produce a conquest or a demographic transition.

### The CI gate

```bash
scripts/validate.sh 400 ci   # build + two identical-seed runs diffed + realism report
```

Runs on every push via `.github/workflows/ci.yml` (msys2 / MinGW64 runner). Exit-code gated.

### Running it

```bash
cmake -G "MinGW Makefiles" . && mingw32-make -j8
app.exe                                                        # GUI
app.exe --headless 600 --seed 42 --entities 40 --region 1 --chaos 1.3
```

| Flag | Meaning |
|---|---|
| `--headless <ticks>` | run N ticks without a window, then exit |
| `--seed <text\|num>` | world seed — same seed ⇒ byte-identical history |
| `--entities <n>` | founding population (default 40) |
| `--region <1-4>` | disease-climate region |
| `--chaos <0.3-2.5>` | divergence level (the butterfly knob) |
| `--load` / `--save-at` / `--save-file` | resume, checkpoint, checkpoint target |
| `--save-every <n>` / `--save-on-exit` | periodic checkpointing, and save on shutdown |
| `--scenario <name>` | `eden` (gentle, 150 souls), `crucible` (harsh, 60), `babel` (crowded, 400), `dish` (petri dish, 12) |
| `--set <knob>=<v>` | set any `LiveConfig` multiplier (kill switches) |
| `--inject <file>` | apply a web-bridge injection file |

### What a run writes

Nine event logs under `src/data/` — `cmd`, `deaths`, `diseases`, `actions`, `relationships`,
`movements`, `births`, `events`, `civilization` — plus a unified `complete_logs.txt`,
per-entity stat CSVs, and `tick_history.jsonl` (population state sampled every 5th tick; a
per-tick export ran to ~27 MB per 500 ticks and dominated I/O).

Every 25 days the engine also writes a **history fingerprint** — proof that two seeds produce
genuinely different histories — and a knowledge sample.

### The GUI — 16 panels

Stats + Save · System Statistics · Entity Statistics · Action Statistics · CIVILIZATION ·
MARKET · MIND BOARD · Player Statistics · **God Console** · **Possess** · **Interview** ·
**Config Console** · Emergence · World Map · History & Report · ImPlot Metrics.

- **God Console** — smite, bless, torment, feast, famine, meteor, great calm. Every act is
  written into the Chronicle.
- **Possess** — take over the selected entity; your command overrides its reflexes, habits and
  deliberation until released.
- **Interview** — six templated questions answered from the agent's real feelings, memories,
  beliefs, goals, narrative identity, life chapters and opinions of others.
- **Config Console** — the live multipliers, all ×1.0 by default so determinism holds until
  you touch something.

### Analysis scripts

| Script | Purpose |
|---|---|
| `scripts/validate.sh` | the CI gate: build + determinism pair + realism report |
| `scripts/butterfly.py` | A/B divergence table over two `tick_history.jsonl` runs — the butterfly effect, measured |
| `scripts/ashb2_report.py` | Markdown post-mortem: population, mortality with killer attribution, war (casus belli + battle tiers), religion, economy, disasters, era trajectory |
| `scripts/ashb2_sweep.py` | parameter-sweep harness across seeds → one CSV row per run |
| `scripts/ashb2_multiverse.py` | side-by-side run comparison, ranked by advancement, with correlation flags |
| `scripts/ashb2_inspect.py` | run inspector |
| `scripts/ashb2_dashboard.html` | browser dashboard |
| `postmortem_generate.py` | narrative post-mortem generator |

### Saves

`ASHB2_SAVE_V2` carries the sim clock, the shared RNG stream, tribes, religions and era.
New keys are appended at the *end* of `saveTo` and read *last* in `loadFrom` behind a presence
guard, so the format is forward-compatible and existing keys are never reordered.

---

## X. Honest status

### Performance

Laptop, Release `-O2`, 8 threads, one tick = 60 frames:

| Agents | Sim | Movement | Total | 1,000-day headless run |
|---|---|---|---|---|
| 1,000 | 220 ms | 20 ms | ~240 ms | ~4 min |
| 5,000 | 597 ms | 229 ms | ~830 ms | ~14 min |
| 10,000 | 746 ms | 758 ms | ~1,500 ms | ~25 min |

The M11 target was 10k agents × 1,000 days in under 10 minutes. **Not met** — it is ~25 min,
down from ~76 min before the scale pass. The remaining cost is the per-frame force loop
itself (10k × 60 frames × neighbour queries); closing the gap needs an SoA hot-data split
and/or movement level-of-detail, both of which risk behavioural drift and deserve their own
pass. Determinism was verified byte-identical after every optimisation.

### Known dead code

About **4,450 lines compile but are never called**:

| Module | Status |
|---|---|
| `src/validation/ValidationFramework.cpp` | compiled, zero call sites |
| `src/scalability/Scalability.cpp` | compiled, zero call sites |
| `src/observability/Observability.cpp` | compiled, zero call sites |
| `src/modules/BehavioralModule.cpp` | compiled, zero call sites |
| `src/CognitiveArchitecture.cpp` | compiled, never instantiated |
| `src/EmotionalComplexity.cpp` | compiled, never instantiated |
| `src/EnvironmentalInteraction.cpp` | compiled, never instantiated |
| `src/SocialDynamics.cpp` | compiled, never instantiated |
| `src/LifeCourse.cpp` | compiled, never instantiated |

Not in the build at all: `SpatialMesh.cpp`, `movement.cpp`, `WorldMap.cpp`. Superseded:
`Heritage` (by `Kinship`), `Graph.cpp` (dangling-pointer design). Stub: `CommunicationBasis`
(one line).

Note that `LearningAdaptation.cpp` looks similar but is **live** — it is the Q-learning
system, reached through `FreeWillSystem::rlSystem`.

### Other caveats

- A resumed save is a *plausible continuation*, not bit-identical to the unbroken run: some
  subsystems re-derive their private RNGs from the world seed at construction. Two loads of
  the same save are identical to each other.
- The 25 assertions are printed but this document does not report the results of a fresh long
  acceptance run.
- SDL is fully removed (2026-07-25). The only render path is GLFW + OpenGL + ImGui + ImPlot.

---

## Appendix — source map

| Concern | Where |
|---|---|
| Decision pipeline, relationships, reproduction | `src/implem_free_will.cpp` |
| Emotions, identity, purpose, skills, facts, doctrine, priors | `src/ai/MindUpgrade.{h,cpp}` |
| The person | `src/header/Entity.h`, `src/Entity.cpp` |
| Drives (bipolar, two lethal poles) | `src/header/Drive.h`, `src/Drive.cpp` |
| Jungian stack, archetypes, the grip | `src/header/JungianType.h`, `src/JungianType.cpp` |
| Genome, epigenetics | `src/header/Genome.h`, `src/Entity.cpp` |
| Everything civilizational | `src/CivilizationEngine.cpp`, `src/header/CivilizationEngine.h` |
| Cultural traits, institutions | `src/environment/EnvironmentModel.{h,cpp}` |
| Class, clientela, debt, estates | `src/SocialOrder.cpp` |
| Lineage, inheritance, vertical transmission | `src/Kinship.cpp` |
| Market and goods | `src/Economics.cpp`, `src/header/Economics.h` |
| Tech tree | `src/TechTree.cpp` |
| Treaties and stances | `src/Diplomacy.cpp` |
| Disease and pathogens | `src/Disease.cpp` |
| Q-learning, vicarious learning | `src/LearningAdaptation.cpp` |
| Tree-of-Thoughts day planner | `src/PlanningSystem.cpp` |
| Semantic memory / embeddings | `src/SemanticMemory.cpp` |
| PAD, body language, CoT, hesitation | `src/PersonaSystem.cpp`, `src/header/PersonaSystem.h` |
| Prose and inner monologue | `src/NarrativeEngine.cpp` |
| GOAP | `src/ai/GoapPlanner.{h,cpp}` |
| NEAT | `src/ai/neat/Neat.{h,cpp}` |
| Spatial event memory | `src/ai/EpisodicMap.h` |
| Items, action rules, invention | `src/items/ItemSystem.{h,cpp}` |
| Stigmergy field | `src/world/PheromoneField.{h,cpp}` |
| Planet, noise, biomes, map view | `src/world/Planet.cpp`, `Noise.cpp`, `PlanetView.cpp` |
| Languages | `src/world/Lexicon.{h,cpp}` |
| Ecosystem, resources | `src/world/Ecosystem.cpp`, `ResourceSystem.cpp` |
| Clock, spatial grid | `src/core/SimClock.h`, `src/core/SpatialGrid.h` |
| Seed and RNG streams | `src/WorldSeed.cpp`, `src/header/WorldSeed.h`, `BetterRand.h` |
| Kill switches | `src/header/LiveConfig.h` |
| Save format | `src/SaveLoad.cpp` |
| GUI | `src/UI.cpp`, `src/world/PlanetView.cpp` |
| Realism report, CLI, main loop | `src/main.cpp` |
| Web bridge | `bridge/README.md`, `website/` |
| CI gate | `scripts/validate.sh`, `.github/workflows/ci.yml` |
| Plans | `plans/MASTER_PLAN.md` and siblings |
