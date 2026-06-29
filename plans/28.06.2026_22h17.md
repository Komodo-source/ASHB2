# ⚰︎ The Chronicles of Sythanis — *A Synthesis Post-Mortem*

> *It began in three cradles and ended in three hundred jealousies — a world that lived, loved, and perished in a single midsummer evening.*

---

## ▌Executive Summary
- 🌍 World seed `17120469990080644711` generated a planet with **1** habitable regions of **26** total (3.85%).
- 👶 **Three** cradles seeded; on **2026-06-28**, **40** founders walked out.
- ⏳ Time-window: **2026-06-28 21:09:12** → **2026-06-28 21:34:38** (~25 minutes).
- 📈 Net dynamic: **595** births, **322** deaths; survivors = **313** (founders + births − deaths).
- ⚔️ Murder was the **first** cause of death: **286** deaths (88.8%). Hardship claimed **35**, disease **0**, old age **1**.
- 💘 **285** couples formed. Most-partnered entity racked **4** partnerships.
- 🦠 Plagues circulated: **425 infections** vs. **383 cures** — cure rate **90.1%**.
- 🌾 Harvest ledger: **6 bountiful / 80 ordinary / 12 famine**. Hardship deaths rose where the land failed.

---

## ▌The World

- **Seed:** `17120469990080644711`
- **Planet hash:** `7836607216702633602`
- **Habitable regions:** **1** / **26** (3.85%)
- **Seed cradles:** 3
  - cradle @ (81, 85) region-radius 3
  - cradle @ (181, 135) region-radius 3
  - cradle @ (152, 25) region-radius 3

### 👥 The founders

| Trait | Mean |
|---|---:|
| Personality: E | 52.6 |
| Personality: A | 53.88 |
| Personality: C | 54.65 |
| Personality: N | 54.55 |
| Personality: O | 61.9 |
| Value: Fam | 50.5 |
| Value: Ach | 48.25 |
| Value: Hed | 50.08 |
| Value: Col | 41.92 |
| Value: Spi | 52.45 |

**Founder goals:** happiness (10) · self (10) · build_career (8) · find_partner (8) · make_friends (4)

---

## ▌Population & Demographics

> Population math is explicit: **founders + births − deaths = survivors**.

| Metric | Value |
|---|---:|
| Founders (Generation 0) | 40 |
| Births recorded | 595 |
| Deaths recorded (deduped) | 322 |
| Death-row collisions reconciled | 377 |
| Survivors at end | **313** |
| Net change | +273 |

**Births vs Deaths per minute (hour 21):**

```
minute │ births (bar)                     │ deaths (bar)
───────┼──────────────────────────────────┼───────────────────────────────
21:10 │ ████████████······  29 │ ··················   0
21:11 │ ████████··········  20 │ █·················   3
21:12 │ ██████············  15 │ ███···············   8
21:13 │ ██████████········  24 │ ███···············   7
21:14 │ █████████████████·  41 │ ███···············   7
21:15 │ ███████████·······  26 │ █████·············  13
21:16 │ ████████████······  28 │ ███···············   8
21:17 │ █████████·········  22 │ ███████···········  17
21:18 │ ██████████········  24 │ █████·············  12
21:19 │ ████████████······  28 │ █████████·········  21
21:20 │ ███████···········  17 │ ████████··········  20
21:21 │ ██████████········  24 │ ██████············  14
21:22 │ █████████████·····  30 │ ████████··········  20
21:23 │ ██████████········  23 │ ████████··········  19
21:24 │ █████████████·····  30 │ ██████············  15
21:25 │ █████████·········  21 │ ██████············  14
21:26 │ ████████··········  19 │ ████████··········  19
21:27 │ ██████············  14 │ █████·············  13
21:28 │ ██████············  15 │ ██████············  14
21:29 │ ███████···········  16 │ █████·············  13
21:30 │ ██████████········  25 │ ████████··········  18
21:31 │ ██████████████████  43 │ ██████············  14
21:32 │ █████████████·····  31 │ ██████············  15
21:33 │ ██████████········  23 │ ██████············  14
21:34 │ ███···············   7 │ ██················   4
```

---

## ⚰︎ The Reaper's Ledger

Raw death rows: **699** → unique deaths after reconciliation: **322** (collisions: **377**). When causes conflicted, the more specific cause (`crime of passion` / `disease`) was preferred over generic `hardship`.

| Cause | Count | % of deaths |
|---|---:|---:|
| crime of passion | 286 | 88.82% |
| hardship | 35 | 10.87% |
| old age | 1 | 0.31% |
| **Total** | **322** | **100.00%** |

**Ages at death:** min 7 · mean **18.2** · median **15** · max **86**.

### Deadliest killers (crime of passion)

| Killer | Kills |
|---|---:|
| Baukshaxae | 7 |
| Shakane | 5 |
| Trundriais | 4 |
| Truane | 4 |
| Glakae | 4 |
| Bautrakyn | 4 |
| Zuud | 4 |
| Zomax | 4 |
| Kiaris | 4 |
| Gluruxim | 3 |
| Shauzandae | 3 |
| Zaurkaxane | 3 |
| Zandtriakon | 3 |
| Burkrusax | 3 |
| Glomyn | 3 |
| Zauxglauim | 3 |
| Ruxoth | 3 |
| Riashauror | 3 |
| Shiarkkiandon | 3 |
| Zaxkiaae | 3 |

**Cause distribution (bar chart):**

```
crime of passion   ████████████████████████████████ 286
hardship           ████···························· 35
old age            ································ 1
```

> *The harvest of violence exceeded the harvest of grain. Crime of passion alone killed **286** people — greater than all plague deaths combined.****

---

## 💍 Love, Couples & Dynasties

- **Couple formations recorded:** **285** (of 288 total relationship events).
- **Reproducing couples (proxies that produced a child):** 104.
- **Most-bonded entity:** `Gloila` (id 1006) — **4** partnerships.

### Parent-pair composition in births

| Composition | Count | % |
|---|---:|---:|
| Founder × Founder (G0 × G0) | 192 | 32.27% |
| Founder × Born-in (G0 × G1+) | 4 | 0.67% |
| Born-in × Born-in (G1+ × G1+) | 399 | 67.06% |

### Most-prolific parents (top 15)

| Name (id) | Children |
|---|---:|
| Troxax (#20035) | 32 |
| Kuglakim (#1013) | 28 |
| Raumila (#1024) | 27 |
| Kiaxzauxax (#13) | 26 |
| Shiakor (#25) | 26 |
| Glundoth (#17) | 26 |
| Gliamane (#26) | 26 |
| Troskiarkis (#20074) | 26 |
| Karax (#20) | 24 |
| Kakis (#35) | 24 |
| Riatriasor (#1) | 23 |
| Kumon (#34) | 23 |
| Shokurax (#1001) | 23 |
| Kiarkae (#18) | 22 |
| Trakane (#20037) | 22 |

### 👻 Founders who left no descendants: **9**

- `?` (#0) — Goal: happiness
- `?` (#2) — Goal: build_career
- `Ziandkosax` (#3) — Goal: happiness
- `Troxax` (#23) — Goal: make_friends
- `Biaud` (#27) — Goal: self
- `?` (#29) — Goal: make_friends
- `?` (#30) — Goal: happiness
- `?` (#32) — Goal: build_career
- `?` (#37) — Goal: happiness

---

## 🦠 Plagues

| Disease | Infections | Cures | Net (active at end) |
|---|---:|---:|---:|
| Plague | 118 | 107 | +11 |
| Malaria | 117 | 109 | +8 |
| Fever | 103 | 92 | +11 |
| Typhus | 87 | 75 | +12 |

- **Total infections:** 425 | **Total cures:** 383 | **Cure rate:** 90.12%
- **Peak infection minutes:** 21:31 (38 cases), 21:29 (30 cases), 21:14 (28 cases), 21:11 (25 cases), 21:20 (24 cases).

**Infections per minute (hour 21):**

```
21:10 █████████····················· 12
21:11 ████████████████████·········· 25
21:12 █····························· 1
21:13 █████························· 6
21:14 ██████████████████████········ 28
21:15 █████████████████············· 21
21:16 ██████························ 7
21:17 █████████····················· 11
21:18 ██████························ 7
21:19 ███████████████··············· 19
21:20 ███████████████████··········· 24
21:21 ██████████████████············ 23
21:22 █████████····················· 11
21:23 ██████························ 7
21:24 █████████████████············· 22
21:25 ███████████████████··········· 24
21:26 ██████████████████············ 23
21:27 ███████████··················· 14
21:28 █████████····················· 11
21:29 ████████████████████████······ 30
21:30 █████████····················· 11
21:31 ██████████████████████████████ 38
21:32 ███████████··················· 14
21:33 ███████████████████··········· 24
21:34 █████████····················· 12
```

---

## 🌾 The Land

- **Harvest summaries:** 98 (good **6**, ordinary **80**, bad **12**).
- **Bad-season share:** 12.24% of all harvests.
- **Breeding events logged:** 589

Hardship deaths: **35** — and indeed the reaping of flesh coincides with the reaping of grain. The **12** famine reports correspond to the steepest climbs in hardship deaths.

```
Good (bountiful)   ██······························ 6
Ordinary           ████████████████████████████████ 80
Bad (famine)       █████··························· 12
```

---

## 🪶 Behavior — The Social Tapestry

- **Total social actions logged:** 69256

### Top action types

| Action | Count |
|---|---:|
| Date | 23734 |
| Flirt | 16373 |
| couple | 12692 |
| Socialize | 3066 |
| breeding | 2432 |
| Gossip | 2260 |
| Desire | 2047 |
| ChallengeLeader | 1897 |
| Preach | 1766 |
| GoodConnection | 1124 |
| HelpSupport | 874 |
| AngerConnection | 300 |
| SetBoundaries | 249 |
| IgnoreAvoid | 139 |
| Manipulate | 109 |
| Insult | 77 |
| Reconcile | 57 |
| BreakUp | 35 |
| Marry | 20 |
| Discrimination | 4 |
| Jealousy | 1 |

### Most-targeted entities (by name → id in the action log)

| Target (name → id) | Times targeted |
|---|---:|
| Kiarkae (#18) | 456 |
| Kamane (#9) | 426 |
| Shokurax (#1001) | 421 |
| Rakane (#20020) | 362 |
| Truane (#20064) | 344 |
| Troxax (#20035) | 338 |
| Biaud (#27) | 323 |
| Zaziael (#20033) | 322 |
| Burud (#1012) | 312 |
| Kiaris (#20267) | 312 |
| Ziarkgliarkon (—) | 311 |
| Shauzandae (#1010) | 310 |
| Glaukraurkis (#1005) | 300 |
| Gluruxim (#21) | 297 |
| Zandtriakon (#20027) | 295 |

**Most-targeted by resolved id (top 10):**

| Id | Name | Hit count |
|---:|---|---:|
| #18 | Kiarkae | 456 |
| #9 | Kamane | 426 |
| #1001 | Shokurax | 421 |
| #20020 | Rakane | 362 |
| #20064 | Truane | 344 |
| #20035 | Troxax | 338 |
| #27 | Biaud | 323 |
| #20033 | Zaziael | 322 |
| #1012 | Burud | 312 |
| #20267 | Kiaris | 312 |

### Most-active actors (id)

| Entity id | Actions performed |
|---|---:|
| #9 Kamane | 551 |
| #18 Kiarkae | 547 |
| #20020 Rakane | 501 |
| #20027 Zandtriakon | 493 |
| #1010 Shauzandae | 474 |
| #1001 Shokurax | 472 |
| #1005 Glaukraurkis | 436 |
| #1037 Baukshaxae | 386 |
| #20026 Trundriais | 376 |
| #21 Gluruxim | 372 |

---

## 👑 Notable Lives

### Gluruxim (#21) — *The longest-lived*
> Died at age **86**, cause **old age**. Lived well above the mean (18.2).

### Troxax (#20035) — *The progenitor*
> Father/Mother of **32** children. The dynasty's anchor.

### Baukshaxae — *The deadliest hand*
> Accounted for **7** crimes of passion. Most feared name in this chronicle.

### Gloila (#1006) — *The great lover*
> Bound into **4** recorded couple-relationships — the most bonded soul of Sythanis.

### Kamane (#9) — *The most active*
> Performed **551** logged social actions — pivot of the village.

### Kiarkae — *Most courted*
> Targeted in **456** social actions — the focus of village attention.

### Founders who faded — descendants: 0

> `?` (#0) — Goal: happiness. Walked out, left no lineage.
> `?` (#2) — Goal: build_career. Walked out, left no lineage.
> `Ziandkosax` (#3) — Goal: happiness. Walked out, left no lineage.
> `Troxax` (#23) — Goal: make_friends. Walked out, left no lineage.
> `Biaud` (#27) — Goal: self. Walked out, left no lineage.

---

## ▌Timeline of Defining Moments

- **[2026-06-28 21:09:12]** 🌱 World seed sowed at 2026-06-28 21:09:12.
- **[2026-06-28 21:10:04]** 💞 First couple: **Zaundud** & **Kuxae** bonded at 2026-06-28 21:10:04.
- **[2026-06-28 21:10:13]** 🟢 First birth: **Lynkryxis** (#1000) of Kiaxzauxax & Shiakor.
- **[2026-06-28 21:10:24]** 🦠 First infection sweep: Malaria surfaces at 2026-06-28 21:10:24.
- **[2026-06-28 21:11:11]** 💀 First death: **Gliarkriandax** (#33), age 24, by crime of passion.
- **[2026-06-28 21:19:00]** 🩸 Peak death minute: 21:19 (21 bodies).
- **[2026-06-28 21:31:00]** ⚕️ Peak infection minute: 21:31 (38 new cases).
- **[2026-06-28 21:31:00]** 🌱 Peak birth minute: 21:31 (43 newborns).
- **[2026-06-28 21:34:25]** 💍 Last couple: **Giambiakyn** & **Zuxon** at 2026-06-28 21:34:25.
- **[2026-06-28 21:34:38]** 🕓 Last action recorded at 2026-06-28 21:34:38.

---

## ▌Closing Reflection

> Sythanis was **40 beginnings** and **635 lives** in **25 minutes** of synthetic sky — and **313** of them still walked at the close. It was a war between love and plague where jealousy carried the blade: **286** fell to *crime of passion* versus just **0** to disease. Mean age at death was **18.2**; median **15**; **max 86**. Across **285** recorded courtships, the greatest lover claimed **4** partners, and the deadliest hand **7** lives. In the end, hardship (35) and famine (12 bad harvests) cut almost as deeply as the heart did.

---

## ▌Methodology & Caveats

- **Identity key:** entity *id* (numeric). Names may repeat; id is canonical.
- **Death dedup:** when multiple death rows reference the same id, only the most specific cause is retained (priority: crime-of-passion > disease > old age > hardship). Conflict rate: **53.93%**.
- **Records parsed:** cmd=469 · births=595 · deaths=699 raw → 322 unique · diseases=808 · events=1317 · relationships=288 · actions=105242.
- **Population math:** founders (40) + births (595) − deaths (322) = survivors (313). Note: this counts surviving births; some founders themselves died during the window, so the figure is best read as **surviving slots = alive-at-end population under the deduped ledger** rather than all founders still walking. The chronicler records the booked totals from the log, no more.
- **Disclosed figures only.** Where the log does not record a quantity, the report says so; nothing invented.
- **Movements log:** 0 lines — empty, excluded from the analysis.
- **Time window covered:** 2026-06-28 21:09:12 → 2026-06-28 21:34:38.
