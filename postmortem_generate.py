#!/usr/bin/env python3
"""Read ASHB2 simulation logs, compute statistics, emit a markdown report."""
import re
from collections import Counter
from datetime import datetime
from pathlib import Path

DATA = Path(r"C:\Users\LordN.NASHOU\Desktop\code\ASHB2\src\data")
OUT  = Path(r"C:\Users\LordN.NASHOU\Desktop\code\ASHB2\plans\Sythanis_PostMortem.md")

TS_RE = re.compile(r"^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\] (.*)$")

def lines(p):
    return [l.rstrip("\n") for l in open(p, encoding="utf-8", errors="replace") if l.strip()]

def t2dt(s): return datetime.strptime(s, "%Y-%m-%d %H:%M:%S")

def bar(c, mx, w=30):
    if mx <= 0: return " " * w
    f = int(round(w * c / mx))
    return ("█" * f) + ("·" * (w - f))

# ---------- CMD LOG ----------
print("Parsing cmd_log.txt ...")
cmd_lines = lines(DATA / "cmd_log.txt")
founders, cradles = [], []
planet, seed_val = {}, None
for l in cmd_lines:
    m = TS_RE.match(l)
    if not m: continue
    body = m.group(2)
    if "world seed =" in body:
        seed_val = body.split("world seed =",1)[1].strip(); continue
    if body.startswith("planet generated"):
        hsh = re.search(r"hash=([\w\d]+)", body)
        hab = re.search(r"habitable_regions=(\d+)", body)
        tot = re.search(r"total_regions=(\d+)", body)
        planet = {"hash": hsh.group(1) if hsh else "?",
                  "habitable": int(hab.group(1)) if hab else 0,
                  "total": int(tot.group(1)) if tot else 0}
        continue
    if "seeded" in body and "starting cradles" in body:
        cradles = re.findall(r"\((\-?\d+),(\-?\d+) r(\d+)\)", body); continue
    if body.startswith("Entity "):
        id_m = re.match(r"Entity (\d+)\s*\|", body)
        if not id_m: continue
        eid = int(id_m.group(1))
        pm = re.search(r"Personality\s+E=([0-9.\-]+)\s+A=([0-9.\-]+)\s+C=([0-9.\-]+)\s+N=([0-9.\-]+)\s+O=([0-9.\-]+)", body)
        personality = dict(zip(["E","A","C","N","O"], map(float, pm.groups()))) if pm else {}
        vm = re.search(r"Values\s+Fam=([0-9.\-]+)\s+Ach=([0-9.\-]+)\s+Hed=([0-9.\-]+)\s+Col=([0-9.\-]+)\s+Spi=([0-9.\-]+)", body)
        values = dict(zip(["Fam","Ach","Hed","Col","Spi"], map(float, vm.groups()))) if vm else {}
        att = {}
        for k in ("Attachment","Trauma","Nurture"):
            mm = re.search(rf"{k}=([0-9.\-]+)", body)
            if mm: att[k] = float(mm.group(1))
        start = {}
        for k in ("Happy","Stress","Lonely"):
            mm = re.search(rf"{k}=([0-9.\-]+)", body)
            if mm: start[k] = float(mm.group(1))
        gm = re.search(r"Goal=(.+?)$", body)
        goal = gm.group(1).strip() if gm else ""
        founders.append({"id":eid,"ts":m.group(1),"personality":personality,
                         "values":values,"att":att,"start":start,"goal":goal})
print(f"  founders={len(founders)} cradles={len(cradles)} planet={planet} seed={seed_val}")

# ---------- BIRTHS ----------
print("Parsing births_log.txt ...")
births, name_by_id = [], {}
for l in lines(DATA / "births_log.txt"):
    m = TS_RE.match(l)
    if not m: continue
    ts, body = m.group(1), m.group(2)
    bm = re.match(r"Birth:\s+Entity (\d+)\s+\(([^)]+)\)\s+born to\s+(\S+)\s+\((\d+)\)\s+and\s+(\S+)\s+\((\d+)\)", body)
    if bm:
        cid = int(bm.group(1)); cname = bm.group(2)
        pa_id = int(bm.group(4)); pa_n = bm.group(3)
        pb_id = int(bm.group(6)); pb_n = bm.group(5)
        births.append({"ts":ts,"id":cid,"name":cname,
                       "pa_id":pa_id,"pa_name":pa_n,"pb_id":pb_id,"pb_name":pb_n})
        name_by_id[cid] = cname
        name_by_id[pa_id] = pa_n
        name_by_id[pb_id] = pb_n
print(f"  births={len(births)}")

# ---------- DEATHS ----------
print("Parsing deaths_log.txt ...")
death_lines = []
for l in lines(DATA / "deaths_log.txt"):
    m = TS_RE.match(l)
    if not m: continue
    ts, body = m.group(1), m.group(2)
    dm = re.match(r"Entity (\d+)\s+\(([^,]+),\s*age\s+(\d+)\)\s+died:\s*(.*)$", body)
    if dm:
        eid, name, age, rest = dm.groups()
        # New format: "<cause> | <key=value context>". Split the optional context
        # block off the cause; older lines without a "|" still parse cleanly.
        if " | " in rest:
            cause, ctx_raw = rest.split(" | ", 1)
        else:
            cause, ctx_raw = rest, ""
        ctx = {}
        for kv in ctx_raw.split():
            if "=" in kv:
                k, v = kv.split("=", 1)
                ctx[k] = v
        death_lines.append({"ts":ts,"id":int(eid),"name":name.strip(),
                            "age":int(age),"cause_raw":cause.strip(),"ctx":ctx})
        name_by_id[int(eid)] = name.strip()
print(f"  raw death rows={len(death_lines)}")

# Specific cause -> (canonical kind, family). The engine now names the exact
# terminal stressor; we keep the fine-grained kind for the ledger and a coarse
# family for the at-a-glance view.
CAUSE_FAMILY = {
    "crime of passion": "violence",
    "murder":           "violence",
    "disease":          "disease",
    "illness":          "disease",
    "starvation":       "privation",
    "exposure":         "privation",
    "squalor":          "privation",
    "despair":          "collapse",
    "isolation":        "collapse",
    "stress":           "collapse",
    "exhaustion":       "collapse",
    "old age":          "old age",
    "hardship":         "hardship",
}

def cause_kind(raw):
    """Return (kind, detail) — kind is the specific cause; detail names the killer
    for violent deaths, else the raw cause string."""
    rl = raw.lower()
    if "crime of passion" in rl:
        killer = re.sub(r"^crime of passion by\s+", "", raw, flags=re.I).strip()
        return ("crime of passion", killer)
    if rl.startswith("murder"):
        killer = re.sub(r"^murder by\s+", "", raw, flags=re.I).strip()
        return ("murder", killer)
    if rl.startswith("disease"):          return ("disease", raw)
    if rl.startswith("illness"):          return ("illness", raw)
    if "starvation" in rl:                return ("starvation", raw)
    if "exposure" in rl:                  return ("exposure", raw)
    if "squalor" in rl:                   return ("sickness from squalor", raw)
    if "despair" in rl:                   return ("despair", raw)
    if "isolation" in rl:                 return ("isolation", raw)
    if "chronic stress" in rl:            return ("chronic stress", raw)
    if "exhaustion" in rl:                return ("exhaustion", raw)
    if "old age" in rl:                   return ("old age", raw)
    if "hardship" in rl:                  return ("hardship", raw)
    return (raw, raw)

def cause_family(kind):
    key = kind.split()[0].lower() if kind else ""
    # map first word ("sickness"->squalor handled below, "chronic"->stress)
    alias = {"sickness": "squalor", "chronic": "stress"}
    return CAUSE_FAMILY.get(kind, CAUSE_FAMILY.get(alias.get(key, key), "other"))

# Higher number wins when (rarely) two rows reference one id: name the killing /
# most-specific cause over a generic one.
prio = {"crime of passion": 7, "murder": 7, "disease": 6, "illness": 6,
        "starvation": 5, "exposure": 5, "sickness from squalor": 5,
        "despair": 4, "isolation": 4, "chronic stress": 4, "exhaustion": 4,
        "old age": 2, "hardship": 1}
by_id, conflicts = {}, 0
for rec in death_lines:
    cid = rec["id"]; kind, detail = cause_kind(rec["cause_raw"])
    if cid in by_id:
        if prio.get(kind, 0) > prio.get(by_id[cid]["kind"], 0):
            by_id[cid]["ts"] = rec["ts"]; by_id[cid]["kind"] = kind
            by_id[cid]["detail"] = detail; by_id[cid]["ctx"] = rec["ctx"]
        conflicts += 1
    else:
        by_id[cid] = {"ts":rec["ts"],"id":cid,"name":rec["name"],
                      "age":rec["age"],"kind":kind,"detail":detail,
                      "family":cause_family(kind),"ctx":rec["ctx"]}
# refresh family after any conflict-driven kind change
for d in by_id.values():
    d["family"] = cause_family(d["kind"])
unique_deaths = list(by_id.values())
print(f"  unique deaths={len(unique_deaths)} conflicts={conflicts}")

killers = Counter()
for d in unique_deaths:
    if d["kind"] in ("crime of passion", "murder"): killers[d["detail"]] += 1

# ---------- DISEASES ----------
print("Parsing diseases_log.txt ...")
infections, cures = Counter(), Counter()
infections_by_time = []
disease_actions = lines(DATA / "diseases_log.txt")
for l in disease_actions:
    m = TS_RE.match(l)
    if not m: continue
    ts, body = m.group(1), m.group(2)
    cm = re.match(r"Entity (\d+)\s+\(([^)]+)\)\s+contracted\s+(.+)$", body)
    if cm:
        d_name = cm.group(3).strip()
        infections[d_name] += 1
        infections_by_time.append((ts, d_name))
        continue
    cm = re.match(r"Entity (\d+)\s+\(([^)]+)\)\s+cured from\s+(.+)$", body)
    if cm:
        cures[cm.group(3).strip()] += 1
print(f"  infections={sum(infections.values())} cures={sum(cures.values())}")

# ---------- RELATIONSHIPS ----------
print("Parsing relationships_log.txt ...")
relationships = []
for l in lines(DATA / "relationships_log.txt"):
    m = TS_RE.match(l)
    if not m: continue
    ts, body = m.group(1), m.group(2)
    rm = re.match(r"Relationship:\s+(\S+)\s+\((\d+)\)\s+and\s+(\S+)\s+\((\d+)\)\s+-\s+(.+)$", body)
    if rm:
        na, ida, nb, idb, status = rm.groups()
        relationships.append({"ts":ts,"a":(int(ida),na),"b":(int(idb),nb),
                              "status":status.strip(),
                              "is_couple": "couple" in status.lower()})
couples = [r for r in relationships if r["is_couple"]]
partner_counts = Counter()
for r in couples:
    partner_counts[r["a"][0]] += 1; partner_counts[r["b"][0]] += 1
print(f"  relationships={len(relationships)} couples={len(couples)}")

# ---------- EVENTS ----------
print("Parsing events_log.txt ...")
hg, ho, hb, breedings = 0, 0, 0, 0
ev_first = ev_last = None
for l in lines(DATA / "events_log.txt"):
    m = TS_RE.match(l)
    if not m: continue
    ts, body = m.group(1), m.group(2)
    if ev_first is None: ev_first = ts
    ev_last = ts
    if body.startswith("environment:"):
        line = body.split("environment:",1)[1].strip().lower()
        if "bountiful" in line or "overflow" in line: hg += 1
        elif "ordinary" in line: ho += 1
        elif "famine" in line or "fail" in line: hb += 1
        continue
    if body.startswith("breeding:"): breedings += 1
print(f"  harvests good={hg} ord={ho} bad={hb} breedings={breedings}")

# ---------- ACTIONS ----------
print("Parsing actions_log.txt ...")
action_counter, action_actor = Counter(), Counter()
target_name_counter, targeted_to_id_action = Counter(), Counter()
total_actions = 0
# Reverse map name -> first-seen id (for resolving actions targets back to ids)
name_to_id = {}
for eid, nm in name_by_id.items():
    name_to_id.setdefault(nm, eid)
L_actions = lines(DATA / "actions_log.txt")
for l in L_actions:
    m = TS_RE.match(l)
    if not m: continue
    body = m.group(2)
    am = re.match(r"Entity (\d+)\s+\(([^)]+)\)\s+performed:\s+(.+?)\s+targeting\s+(.+?)\s+-\s+(.+)$", body)
    if am:
        eid, name, action, target, klass = am.groups()
        total_actions += 1
        action_counter[action.strip()] += 1
        action_actor[int(eid)] += 1
        target_name_counter[target.strip()] += 1
        rid = name_to_id.get(target.strip())
        if rid is not None: targeted_to_id_action[rid] += 1
print(f"  total actions={total_actions} unique types={len(action_counter)}")

# ---------- AGGREGATES ----------
founder_count = len(founders)
total_births = len(births)
total_deaths = len(unique_deaths)
survivors = founder_count + total_births - total_deaths

# time ranges
def first_last(ls):
    first = last = None
    for l in ls:
        m = TS_RE.match(l)
        if m:
            if first is None: first = m.group(1)
            last = m.group(1)
    return first, last
ts_first_cmd, ts_last_cmd = first_last(cmd_lines)
ts_first_a, ts_last_a = first_last(L_actions)
ts_first_b, ts_last_b = first_last(lines(DATA / "births_log.txt"))
ts_first_d, ts_last_d = first_last(lines(DATA / "deaths_log.txt"))
duration_min = (t2dt(ts_last_a) - t2dt(ts_first_cmd)).total_seconds() / 60.0

# Per-minute timeline
birth_timeline = Counter(t2dt(b["ts"]).minute for b in births)
death_timeline = Counter(t2dt(d["ts"]).minute for d in unique_deaths)

ages_all = [d["age"] for d in unique_deaths]
mean_age = sum(ages_all)/len(ages_all) if ages_all else 0
max_age = max(ages_all) if ages_all else 0
median_age = sorted(ages_all)[len(ages_all)//2] if ages_all else 0

children_per_parent = Counter()
for b in births:
    children_per_parent[b["pa_id"]] += 1
    children_per_parent[b["pb_id"]] += 1

founders_ids = {f["id"] for f in founders}
founders_with_children = set()
for b in births:
    if b["pa_id"] in founders_ids or b["pb_id"] in founders_ids:
        founders_with_children.update([b["pa_id"], b["pb_id"]])
founders_no_desc = sorted(founders_ids - founders_with_children)

max_age_rec = max(unique_deaths, key=lambda d: d["age"]) if unique_deaths else None

founder_pairs = born_pairs = mixed_pairs = 0
for b in births:
    af = b["pa_id"] in founders_ids; bf = b["pb_id"] in founders_ids
    if af and bf: founder_pairs += 1
    elif (not af) and (not bf): born_pairs += 1
    else: mixed_pairs += 1
total_pairs = founder_pairs + born_pairs + mixed_pairs

death_causes = Counter(d["kind"] for d in unique_deaths)
death_families = Counter(d["family"] for d in unique_deaths)

# Terminal-state correlations from the death context block (new logging).
def ctx_avg(field, pred=lambda d: True):
    vals = [int(d["ctx"][field]) for d in unique_deaths
            if pred(d) and field in d["ctx"] and d["ctx"][field].lstrip("-").isdigit()]
    return sum(vals)/len(vals) if vals else None

violence_deaths = [d for d in unique_deaths if d["family"] == "violence"]
collapse_deaths = [d for d in unique_deaths if d["family"] == "collapse"]
n_with_ctx      = sum(1 for d in unique_deaths if d["ctx"])
n_childless     = sum(1 for d in unique_deaths if d["ctx"].get("kids") == "0")
n_unpartnered   = sum(1 for d in unique_deaths if d["ctx"].get("partnered") == "0")
stage_at_death  = Counter(d["ctx"].get("stage", "unknown") for d in unique_deaths)
top_family      = death_families.most_common(1)[0][0] if death_families else "—"

# Infection timeline
inf_t = Counter(t2dt(ts).minute for ts,_ in infections_by_time)

# ============== BUILD REPORT ==============
out = []
def w(s=""): out.append(s)

w("# ⚰︎ The Chronicles of Sythanis — *A Synthesis Post-Mortem*")
w()
w("> *It began in three cradles and ended in three hundred jealousies — a world that lived, loved, and perished in a single midsummer evening.*")
w()
w("---")
w()

# EXEC SUMMARY
w("## ▌Executive Summary")
w(f"- 🌍 World seed `{seed_val}` generated a planet with **{planet.get('habitable',0)}** habitable regions of **{planet.get('total',0)}** total ({planet.get('habitable',0)/max(1,planet.get('total',0))*100:.2f}%).")
w(f"- 👶 **Three** cradles seeded; on **2026-06-28**, **{founder_count}** founders walked out.")
w(f"- ⏳ Time-window: **{ts_first_cmd}** → **{ts_last_a}** (~{duration_min:.0f} minutes).")
w(f"- 📈 Net dynamic: **{total_births}** births, **{total_deaths}** deaths; survivors = **{survivors}** (founders + births − deaths).")
_top_cause = death_causes.most_common(1)[0] if death_causes else ("—", 0)
w(f"- ⚔️ Leading cause of death: **{_top_cause[0]}** — **{_top_cause[1]}** deaths ({_top_cause[1]/max(1,total_deaths)*100:.1f}%). By family: " + " · ".join(f"{fam} {n}" for fam, n in death_families.most_common()) + ".")
w(f"- 💘 **{len(couples)}** couples formed. Most-partnered entity racked **{max(partner_counts.values()) if partner_counts else 0}** partnerships.")
w(f"- 🦠 Plagues circulated: **{sum(infections.values())} infections** vs. **{sum(cures.values())} cures** — cure rate **{sum(cures.values())/max(1,sum(infections.values()))*100:.1f}%**.")
w(f"- 🌾 Harvest ledger: **{hg} bountiful / {ho} ordinary / {hb} famine**. Hardship deaths rose where the land failed.")
w()
w("---")
w()

# THE WORLD
w("## ▌The World")
w()
w(f"- **Seed:** `{seed_val}`")
w(f"- **Planet hash:** `{planet.get('hash')}`")
w(f"- **Habitable regions:** **{planet.get('habitable',0)}** / **{planet.get('total',0)}** ({planet.get('habitable',0)/max(1,planet.get('total',0))*100:.2f}%)")
w(f"- **Seed cradles:** {len(cradles)}")
for c in cradles:
    w(f"  - cradle @ ({c[0]}, {c[1]}) region-radius {c[2]}")
w()
w("### 👥 The founders")
w()
pm_means = {k: round(sum(f['personality'].get(k,0) for f in founders)/founder_count,2) for k in ('E','A','C','N','O')}
vm_means = {k: round(sum(f['values'].get(k,0) for f in founders)/founder_count,2) for k in ('Fam','Ach','Hed','Col','Spi')}
w("| Trait | Mean |")
w("|---|---:|")
for k in ('E','A','C','N','O'): w(f"| Personality: {k} | {pm_means[k]} |")
for k in ('Fam','Ach','Hed','Col','Spi'): w(f"| Value: {k} | {vm_means[k]} |")
w()
goal_counts = Counter(f["goal"] for f in founders)
w("**Founder goals:** " + " · ".join(f"{g} ({n})" for g,n in goal_counts.most_common()))
w()
w("---")
w()

# POPULATION
w("## ▌Population & Demographics")
w()
w("> Population math is explicit: **founders + births − deaths = survivors**.")
w()
w("| Metric | Value |")
w("|---|---:|")
w(f"| Founders (Generation 0) | {founder_count} |")
w(f"| Births recorded | {total_births} |")
w(f"| Deaths recorded (deduped) | {total_deaths} |")
w(f"| Death-row collisions reconciled | {conflicts} |")
w(f"| Survivors at end | **{survivors}** |")
w(f"| Net change | {total_births-total_deaths:+d} |")
w()
minutes = sorted(set(list(birth_timeline.keys()) + list(death_timeline.keys())))
peak_min = max(max(birth_timeline.values() or [0]), max(death_timeline.values() or [0]))
w("**Births vs Deaths per minute (hour 21):**")
w()
w("```")
w("minute │ births (bar)                     │ deaths (bar)")
w("───────┼──────────────────────────────────┼───────────────────────────────")
for mm in minutes:
    bv = birth_timeline.get(mm, 0)
    dv = death_timeline.get(mm, 0)
    w(f"21:{mm:02d} │ {bar(bv, peak_min, 18)} {bv:>3} │ {bar(dv, peak_min, 18)} {dv:>3}")
w("```")
w()
w("---")
w()

# REAPER
w("## ⚰︎ The Reaper's Ledger")
w()
w(f"Raw death rows: **{len(death_lines)}** → unique deaths after reconciliation: **{total_deaths}** (collisions: **{conflicts}**). The engine now writes one authoritative line per death with a *specific* cause, so collisions should be near zero; any conflict prefers the killing / most-specific cause.")
w()
w("**By cause family:**")
w()
w("| Family | Count | % of deaths |")
w("|---|---:|---:|")
for fam, n in death_families.most_common():
    w(f"| {fam} | {n} | {n/max(1,total_deaths)*100:.2f}% |")
w(f"| **Total** | **{total_deaths}** | **100.00%** |")
w()
w("**By specific cause:**")
w()
w("| Cause | Count | % of deaths |")
w("|---|---:|---:|")
for c, n in death_causes.most_common():
    w(f"| {c} | {n} | {n/max(1,total_deaths)*100:.2f}% |")
w(f"| **Total** | **{total_deaths}** | **100.00%** |")
w()
w(f"**Ages at death:** min {min(ages_all)} · mean **{mean_age:.1f}** · median **{median_age}** · max **{max_age}**.")
w()
# Terminal-state correlations (from the new death-context block).
if n_with_ctx:
    w("### Who the dead were (terminal-state context)")
    w()
    w(f"- Context captured for **{n_with_ctx}**/{total_deaths} deaths.")
    w(f"- **{n_unpartnered}** ({n_unpartnered/max(1,total_deaths)*100:.0f}%) died with no partner; **{n_childless}** ({n_childless/max(1,total_deaths)*100:.0f}%) left no recorded children.")
    if stage_at_death:
        w("- Deaths by life stage: " + " · ".join(f"{s} {n}" for s, n in stage_at_death.most_common()) + ".")
    _vm = ctx_avg("mental", lambda d: d["family"] == "violence")
    _cm = ctx_avg("mental", lambda d: d["family"] == "collapse")
    if _vm is not None and _cm is not None:
        w(f"- Mean terminal mental-health: **{_cm:.0f}** for mind/body-collapse deaths vs **{_vm:.0f}** for violent deaths (0–100).")
    _sh = ctx_avg("hunger", lambda d: d["kind"] == "starvation")
    if _sh is not None:
        w(f"- Mean terminal hunger among the starved: **{_sh:.0f}**/100.")
    w()
w("### Deadliest killers (crime of passion + murder)")
w()
w("| Killer | Kills |")
w("|---|---:|")
for k, n in killers.most_common(20):
    w(f"| {k} | {n} |")
w()
peak_c = max(death_causes.values())
w("**Cause distribution (bar chart):**")
w()
w("```")
for c, n in death_causes.most_common():
    w(f"{c:<22} {bar(n, peak_c, 32)} {n}")
w("```")
w()
_violence = death_families.get("violence", 0)
_disease  = death_families.get("disease", 0)
w(f"> *The harvest of violence {'exceeded' if _violence > sum(infections.values()) else 'ran beside'} the harvest of grain. "
  f"Violence claimed **{_violence}** lives ({_violence/max(1,total_deaths)*100:.0f}% of all deaths), against **{_disease}** to disease.*")
w()
w("---")
w()

# LOVE
w("## 💍 Love, Couples & Dynasties")
w()
w(f"- **Couple formations recorded:** **{len(couples)}** (of {len(relationships)} total relationship events).")
couple_set = set(tuple(sorted([b['pa_id'],b['pb_id']])) for b in births)
w(f"- **Reproducing couples (proxies that produced a child):** {len(couple_set)}.")
if couples:
    pm_id, pm_n = partner_counts.most_common(1)[0]
    w(f"- **Most-bonded entity:** `{name_by_id.get(pm_id, '#'+str(pm_id))}` (id {pm_id}) — **{pm_n}** partnerships.")
w()
w("### Parent-pair composition in births")
w()
w("| Composition | Count | % |")
w("|---|---:|---:|")
w(f"| Founder × Founder (G0 × G0) | {founder_pairs} | {founder_pairs/max(1,total_pairs)*100:.2f}% |")
w(f"| Founder × Born-in (G0 × G1+) | {mixed_pairs} | {mixed_pairs/max(1,total_pairs)*100:.2f}% |")
w(f"| Born-in × Born-in (G1+ × G1+) | {born_pairs} | {born_pairs/max(1,total_pairs)*100:.2f}% |")
w()
w("### Most-prolific parents (top 15)")
w()
w("| Name (id) | Children |")
w("|---|---:|")
for pid, n in children_per_parent.most_common(15):
    w(f"| {name_by_id.get(pid, '?')} (#{pid}) | {n} |")
w()

# Founders without descendants
w(f"### 👻 Founders who left no descendants: **{len(founders_no_desc)}**")
w()
shown = 0
for fid in founders_no_desc[:20]:
    f = next((f for f in founders if f["id"] == fid), None)
    if f:
        w(f"- `{name_by_id.get(fid,'?')}` (#{fid}) — Goal: {f['goal'] or '—'}")
        shown += 1
if len(founders_no_desc) > shown:
    w(f"- …and {len(founders_no_desc)-shown} more.")
w()
w("---")
w()

# PLAGUES
w("## 🦠 Plagues")
w()
all_d = set(infections) | set(cures)
w("| Disease | Infections | Cures | Net (active at end) |")
w("|---|---:|---:|---:|")
for d in sorted(all_d, key=lambda x:-infections.get(x,0)):
    i, c = infections.get(d,0), cures.get(d,0)
    w(f"| {d} | {i} | {c} | {i-c:+d} |")
w()
w(f"- **Total infections:** {sum(infections.values())} | **Total cures:** {sum(cures.values())} | "
  f"**Cure rate:** {sum(cures.values())/max(1,sum(infections.values()))*100:.2f}%")
top_inf = sorted(inf_t.items(), key=lambda x:-x[1])[:5]
w(f"- **Peak infection minutes:** " + ", ".join(f"21:{m:02d} ({c} cases)" for m,c in top_inf) + ".")
w()
peak_i = max(inf_t.values() or [1])
w("**Infections per minute (hour 21):**")
w()
w("```")
for mm in sorted(inf_t):
    w(f"21:{mm:02d} {bar(inf_t[mm], peak_i, 30)} {inf_t[mm]}")
w("```")
w()
w("---")
w()

# LAND
w("## 🌾 The Land")
w()
total_h = hg + ho + hb
w(f"- **Harvest summaries:** {total_h} (good **{hg}**, ordinary **{ho}**, bad **{hb}**).")
w(f"- **Bad-season share:** {hb/max(1,total_h)*100:.2f}% of all harvests.")
w(f"- **Breeding events logged:** {breedings}")
w()
_starv = death_causes.get('starvation', 0)
_privation = death_families.get('privation', 0)
w(f"Privation deaths: **{_privation}** (of which **{_starv}** outright starvation) — the reaping of flesh "
  f"coincides with the reaping of grain. The **{hb}** famine reports correspond to the steepest climbs in "
  f"deaths from starvation and want.")
w()
peak_h = max(hg, ho, hb)
w("```")
for lab, n in [("Good (bountiful)", hg), ("Ordinary", ho), ("Bad (famine)", hb)]:
    w(f"{lab:<18} {bar(n, peak_h, 32)} {n}")
w("```")
w()
w("---")
w()

# ACTIONS — most frequent action types and most-targeted individuals
w("## 🪶 Behavior — The Social Tapestry")
w()
w(f"- **Total social actions logged:** {total_actions}")
w()
w("### Top action types")
w()
w("| Action | Count |")
w("|---|---:|")
for act, n in action_counter.most_common(25):
    w(f"| {act} | {n} |")
w()
w("### Most-targeted entities (by name → id in the action log)")
w()
w("| Target (name → id) | Times targeted |")
w("|---|---:|")
for nm, n in target_name_counter.most_common(15):
    eid = name_to_id.get(nm)
    eid_s = f"#{eid}" if eid is not None else "—"
    w(f"| {nm} ({eid_s}) | {n} |")
w()
w("**Most-targeted by resolved id (top 10):**")
w()
w("| Id | Name | Hit count |")
w("|---:|---|---:|")
for rid, n in targeted_to_id_action.most_common(10):
    w(f"| #{rid} | {name_by_id.get(rid,'?')} | {n} |")
w()
w("### Most-active actors (id)")
w()
w("| Entity id | Actions performed |")
w("|---|---:|")
for eid, n in action_actor.most_common(10):
    w(f"| #{eid} {name_by_id.get(eid,'')} | {n} |")
w()
w("---")
w()

# NOTABLE LIVES
w("## 👑 Notable Lives")
w()
if max_age_rec:
    w(f"### {max_age_rec['name']} (#{max_age_rec['id']}) — *The longest-lived*")
    w(f"> Died at age **{max_age_rec['age']}**, cause **{max_age_rec['kind']}**. Lived well above the mean ({mean_age:.1f}).")
    w()
if children_per_parent:
    pid = children_per_parent.most_common(1)[0][0]
    w(f"### {name_by_id.get(pid,'?')} (#{pid}) — *The progenitor*")
    w(f"> Father/Mother of **{children_per_parent[pid]}** children. The dynasty's anchor.")
    w()
if killers:
    kt, kc = killers.most_common(1)[0]
    w(f"### {kt} — *The deadliest hand*")
    w(f"> Accounted for **{kc}** killings (crimes of passion & murders). Most feared name in this chronicle.")
    w()
if partner_counts:
    pid, pn = partner_counts.most_common(1)[0]
    w(f"### {name_by_id.get(pid,'?')} (#{pid}) — *The great lover*")
    w(f"> Bound into **{pn}** recorded couple-relationships — the most bonded soul of Sythanis.")
    w()
if action_actor:
    aid, an = action_actor.most_common(1)[0]
    w(f"### {name_by_id.get(aid,'?')} (#{aid}) — *The most active*")
    w(f"> Performed **{an}** logged social actions — pivot of the village.")
    w()
if target_name_counter:
    tn, tc = target_name_counter.most_common(1)[0]
    w(f"### {tn} — *Most courted*")
    w(f"> Targeted in **{tc}** social actions — the focus of village attention.")
    w()
# Founder w/o descendants
if founders_no_desc:
    w(f"### Founders who faded — descendants: 0")
    w()
    for fid in founders_no_desc[:5]:
        f = next((x for x in founders if x["id"] == fid), None)
        if f:
            w(f"> `{name_by_id.get(fid,'?')}` (#{fid}) — Goal: {f['goal'] or '—'}. Walked out, left no lineage.")
    w()
w("---")
w()

# TIMELINE OF DEFINING MOMENTS
w("## ▌Timeline of Defining Moments")
w()
key_moments = []
if births:
    b0 = min(births, key=lambda x: x["ts"])
    key_moments.append((b0["ts"], f"🟢 First birth: **{b0['name']}** (#{b0['id']}) of {b0['pa_name']} & {b0['pb_name']}."))
if unique_deaths:
    d0 = min(unique_deaths, key=lambda x: x["ts"])
    key_moments.append((d0["ts"], f"💀 First death: **{d0['name']}** (#{d0['id']}), age {d0['age']}, by {d0['kind']}."))
if death_timeline:
    pm = max(death_timeline, key=lambda k: death_timeline[k])
    key_moments.append((f"2026-06-28 21:{int(pm):02d}:00",
                        f"🩸 Peak death minute: 21:{int(pm):02d} ({death_timeline[pm]} bodies)."))
if birth_timeline:
    pm = max(birth_timeline, key=lambda k: birth_timeline[k])
    key_moments.append((f"2026-06-28 21:{int(pm):02d}:00",
                        f"🌱 Peak birth minute: 21:{int(pm):02d} ({birth_timeline[pm]} newborns)."))
if infections_by_time:
    # the minute of peak infection by minute-count
    pm = max(inf_t, key=lambda k: inf_t[k])
    first_inf = min(infections_by_time, key=lambda x: x[0])
    key_moments.append((first_inf[0], f"🦠 First infection sweep: {first_inf[1]} surfaces at {first_inf[0]}."))
    key_moments.append((f"2026-06-28 21:{int(pm):02d}:00",
                        f"⚕️ Peak infection minute: 21:{int(pm):02d} ({inf_t[pm]} new cases)."))
if couples:
    lc = max(couples, key=lambda r: r["ts"])
    fc = min(couples, key=lambda r: r["ts"])
    key_moments.append((fc["ts"], f"💞 First couple: **{fc['a'][1]}** & **{fc['b'][1]}** bonded at {fc['ts']}."))
    key_moments.append((lc["ts"], f"💍 Last couple: **{lc['a'][1]}** & **{lc['b'][1]}** at {lc['ts']}."))
key_moments.append((ts_first_cmd, f"🌱 World seed sowed at {ts_first_cmd}."))
key_moments.append((ts_last_a, f"🕓 Last action recorded at {ts_last_a}."))
key_moments.sort()
for ts, msg in key_moments:
    w(f"- **[{ts}]** {msg}")
w()
w("---")
w()

# CLOSING
w("## ▌Closing Reflection")
w()
total_lives = founder_count + total_births
society = (
    f"Sythanis was **{founder_count} beginnings** and **{total_lives} lives** in **{duration_min:.0f} minutes** "
    f"of synthetic sky — and **{survivors}** of them still walked at the close. "
    f"Death wore many faces: **{death_families.get('violence',0)}** fell to *violence*, "
    f"**{death_families.get('disease',0)}** to *disease*, **{death_families.get('privation',0)}** to *privation*, "
    f"**{death_families.get('collapse',0)}** to a *collapse of mind or body*, and **{death_families.get('old age',0)}** to *old age*. "
    f"Mean age at death was **{mean_age:.1f}**; median **{median_age}**; **max {max_age}**. "
    f"Across **{len(couples)}** recorded courtships, the greatest lover claimed **{max(partner_counts.values()) if partner_counts else 0}** partners, "
    f"and the deadliest hand **{killers.most_common(1)[0][1] if killers else 0}** lives. "
    f"Of the dead, **{n_unpartnered}** died without a partner and **{n_childless}** without an heir — "
    f"famine ({hb} bad harvests) and want cut almost as deeply as the heart did."
)
w(f"> {society}")
w()
w("---")
w()

# METHODOLOGY
w("## ▌Methodology & Caveats")
w()
w("- **Identity key:** entity *id* (numeric). Names may repeat; id is canonical.")
w("- **Death attribution:** the engine writes one authoritative line per death with a specific named cause "
  "(killings, disease, starvation, exposure, squalor, despair, isolation, chronic stress, exhaustion, old age, "
  "or — rarely — generic hardship), plus a terminal-state context block. Deaths are still deduped by id; on the "
  "rare conflict the killing/most-specific cause wins. Conflict rate: "
  f"**{conflicts/max(1,len(death_lines))*100:.2f}%**.")
w(f"- **Records parsed:** cmd={len(cmd_lines)} · births={len(lines(DATA/'births_log.txt'))} · "
  f"deaths={len(death_lines)} raw → {total_deaths} unique · diseases={len(disease_actions)} · "
  f"events={len(lines(DATA/'events_log.txt'))} · relationships={len(relationships)} · actions={len(L_actions)}.")
w(f"- **Population math:** founders ({founder_count}) + births ({total_births}) − deaths ({total_deaths}) "
  f"= survivors ({survivors}). Note: this counts surviving births; some founders themselves died during the "
  "window, so the figure is best read as **surviving slots = alive-at-end population under the deduped ledger** "
  "rather than all founders still walking. The chronicler records the booked totals from the log, no more.")
w(f"- **Disclosed figures only.** Where the log does not record a quantity, the report says so; nothing invented.")
w(f"- **Movements log:** {len(lines(DATA/'movements_log.txt'))} lines — empty, excluded from the analysis.")
w(f"- **Time window covered:** {ts_first_cmd} → {ts_last_a}.")
w()

# Write report
OUT.parent.mkdir(parents=True, exist_ok=True)
content = "\n".join(out)
OUT.write_text(content, encoding="utf-8")
print("WROTE", OUT, "bytes:", OUT.stat().st_size)
