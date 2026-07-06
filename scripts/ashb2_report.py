#!/usr/bin/env python3
"""ASHB2 automated report generator (Improvement Plan item 20).

Parses the event logs a run writes to src/data/ and emits a Markdown post-mortem
covering population, mortality (with killer attribution), war (casus belli + battle
tiers), religion, economy/culture, disasters and the era/tech trajectory — including
all the fields added by Improvement-Plan items 1-14.

Usage:
    python scripts/ashb2_report.py [--data src/data] [--out plans/ASHB2_AutoReport.md]
"""
import argparse, re, json
from collections import Counter, defaultdict
from datetime import datetime
from pathlib import Path

TS = re.compile(r"^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\] (.*)$")


def read(p):
    out = []
    if not p.exists():
        return out
    with open(p, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line.strip():
                continue
            m = TS.match(line)
            if m:
                out.append((m.group(1), m.group(2)))
    return out


def kv(body, key, cast=str, default=None):
    m = re.search(rf'{key}=(?:"([^"]*)"|(\S+))', body)
    if not m:
        return default
    val = m.group(1) if m.group(1) is not None else m.group(2)
    try:
        return cast(val)
    except (ValueError, TypeError):
        return default


def analyze(data: Path):
    S = {}

    # ── founders / world (cmd_log) ───────────────────────────────────────────
    cmd = read(data / "cmd_log.txt")
    S["seed"] = next((b.split("world seed =", 1)[1].strip()
                      for _, b in cmd if "world seed =" in b), "?")
    planet = next((b for _, b in cmd if b.startswith("planet generated")), "")
    S["habitable"] = kv(planet, "habitable_regions", int, 0)
    S["planet_hash"] = kv(planet, "hash", str, "?")

    # ── births ───────────────────────────────────────────────────────────────
    births = read(data / "births_log.txt")
    parent_kids = Counter()
    bre = re.compile(r"born to (\S+) \((\d+)\) and (\S+) \((\d+)\)")
    for _, b in births:
        m = bre.search(b)
        if m:
            parent_kids[(m.group(1), int(m.group(2)))] += 1
            parent_kids[(m.group(3), int(m.group(4)))] += 1
    S["total_births"] = len(births)
    S["top_parents"] = [(n, c) for (n, _i), c in parent_kids.most_common(10)]

    # ── deaths (with killer attribution, Plan 4.3) ───────────────────────────
    deaths = read(data / "deaths_log.txt")
    cause = Counter(); ages = []; killers = Counter(); cross_tribe = 0; same_tribe = 0
    dre = re.compile(r"age (\d+)\) died: ([^|]+)\|?(.*)$")
    for _, b in deaths:
        m = dre.search(b)
        if not m:
            continue
        ages.append(int(m.group(1)))
        c = m.group(1) and m.group(2).strip()
        cl = c.lower()
        if "passion" in cl or "murder" in cl or "battle" in cl or "kill" in cl:
            fam = "violence"
        elif "old age" in cl:
            fam = "old age"
        elif "exhaust" in cl:
            fam = "exhaustion"
        elif any(d in cl for d in ("drought", "flood", "earthquake", "volcan", "meteor")):
            fam = "disaster"
        elif "disease" in cl or "illness" in cl:
            fam = "illness"
        else:
            fam = cl.split(" by ")[0].strip()
        cause[fam] += 1
        rest = m.group(3)
        kid = kv(rest, "killer_id", int)
        if kid is not None and kid >= 0:
            killers[kid] += 1
            st = kv(rest, "same_tribe", int, -1)
            if st == 1:
                same_tribe += 1
            elif st == 0:
                cross_tribe += 1
    S["total_deaths"] = len(deaths)
    S["death_cause"] = cause.most_common()
    S["mean_age_death"] = round(sum(ages) / len(ages), 1) if ages else None
    S["max_age_death"] = max(ages) if ages else None
    S["distinct_killers"] = len(killers)
    S["deadliest"] = killers.most_common(5)
    S["killings_same_tribe"] = same_tribe
    S["killings_cross_tribe"] = cross_tribe

    # ── war (casus belli + battle tiers) ─────────────────────────────────────
    civ = read(data / "civilization_log.txt")
    war_reason = Counter(); battle_tier = Counter(); kinds = Counter()
    disasters = Counter(); cultural_works = 0; snapshots = []
    for _, b in civ:
        k = kv(b, "kind")
        if k:
            kinds[k] += 1
        if k == "war_declared":
            war_reason[kv(b, "reason", str, "?")] += 1
        elif k == "battle":
            battle_tier[kv(b, "tier", str, "?")] += 1
        elif k == "natural_disaster":
            disasters[kv(b, "type", str, "?")] += 1
        elif k == "cultural_achievement":
            cultural_works += 1
        elif k == "snapshot":
            snapshots.append(b)
    S["war_reasons"] = war_reason.most_common()
    S["battle_tiers"] = battle_tier.most_common()
    S["kind_counts"] = kinds.most_common(25)
    S["disaster_types"] = disasters.most_common()
    S["cultural_works"] = cultural_works

    def snap(b):
        f = {}
        for key, cast in (("day", int), ("era", str), ("population", int), ("peakPopulation", int),
                          ("tribes", int), ("religions", int), ("innovations", int), ("darkAges", int),
                          ("warsDeclared", int), ("ethnicWars", int), ("battles", int), ("warDeaths", int),
                          ("conquests", int), ("wealthGini", float), ("avgCulture", int),
                          ("culturalWorks", int), ("avgFortification", int), ("disasters", int),
                          ("civilWars", int), ("meanOpenness", int), ("meanNeuroticism", int)):
            f[key] = kv(b, key, cast)
        return f

    S["first"] = snap(snapshots[0]) if snapshots else {}
    S["final"] = snap(snapshots[-1]) if snapshots else {}
    S["eras"] = list(dict.fromkeys(kv(b, "era", str) for b in snapshots if kv(b, "era", str)))

    # ── religion ──────────────────────────────────────────────────────────────
    rel = read(data / "civilization_log.txt")
    S["religions_founded"] = sum(1 for _, b in rel if re.search(r'religion: \w+ founded "', b))
    S["religions_extinct"] = kinds.get("religion_extinct", 0)
    S["syncretisms"] = kinds.get("religion_syncretism", 0)
    S["institutions_built"] = kinds.get("religion_institution", 0)

    # ── disease ───────────────────────────────────────────────────────────────
    dis = read(data / "diseases_log.txt")
    S["disease_contracted"] = sum(1 for _, b in dis if "contracted" in b)
    S["disease_cured"] = sum(1 for _, b in dis if "cured" in b)
    return S


def bar(c, mx, w=42):
    return "█" * int(round(w * c / mx)) if mx > 0 else ""


def render(S: dict) -> str:
    f, fi = S.get("final", {}), S.get("first", {})
    L = []
    L.append("# ASHB2 Automated Report\n")
    L.append(f"*Generated {datetime.now():%Y-%m-%d %H:%M} · seed `{S['seed']}` · "
             f"planet `{S['planet_hash']}` · {S['habitable']} habitable regions*\n")

    L.append("## Headline\n")
    L.append(f"- Final era **{f.get('era')}** (visited: {', '.join(S['eras'])})")
    L.append(f"- Population **{f.get('population')}** (peak {f.get('peakPopulation')}), "
             f"tribes {f.get('tribes')}, religions {f.get('religions')}, techs {f.get('innovations')}")
    L.append(f"- Dark ages **{f.get('darkAges')}**, disasters {f.get('disasters')}, "
             f"civil wars {f.get('civilWars')}")
    L.append(f"- Wars declared **{f.get('warsDeclared')}** "
             f"(ethnic/holy {f.get('ethnicWars')}), battles {f.get('battles')}, "
             f"war deaths {f.get('warDeaths')}, conquests {f.get('conquests')}")
    L.append(f"- Wealth Gini **{f.get('wealthGini')}**, avg culture {f.get('avgCulture')}, "
             f"great works {f.get('culturalWorks')}, avg fortification {f.get('avgFortification')}\n")

    L.append("## Mortality\n")
    L.append(f"Total deaths **{S['total_deaths']}**, mean age **{S['mean_age_death']}** "
             f"(max {S['max_age_death']}).\n")
    tot = max(1, sum(c for _, c in S["death_cause"]))
    L.append("| Cause | Count | % |\n|---|---|---|")
    for name, c in S["death_cause"]:
        L.append(f"| {name} | {c} | {100*c/tot:.1f}% |")
    L.append(f"\nKillers identified: **{S['distinct_killers']}** · same-tribe killings "
             f"{S['killings_same_tribe']} · cross-tribe {S['killings_cross_tribe']}")
    if S["deadliest"]:
        L.append("Deadliest individuals (by id): " +
                 ", ".join(f"#{i}×{c}" for i, c in S["deadliest"]))
    L.append("")

    L.append("## War\n")
    if S["war_reasons"]:
        tw = max(1, sum(c for _, c in S["war_reasons"]))
        L.append("**Casus belli distribution:**\n")
        L.append("| Reason | Count | % |\n|---|---|---|")
        for r, c in S["war_reasons"]:
            L.append(f"| {r} | {c} | {100*c/tw:.1f}% |")
        L.append("")
    if S["battle_tiers"]:
        tb = max(1, sum(c for _, c in S["battle_tiers"]))
        L.append("**Battle bloodiness:**\n")
        L.append("| Tier | Count | % |\n|---|---|---|")
        for t, c in S["battle_tiers"]:
            L.append(f"| {t} | {c} | {100*c/tb:.1f}% |")
        L.append("")

    L.append("## Religion\n")
    L.append(f"- Founded {S['religions_founded']}, extinct {S['religions_extinct']}, "
             f"**syncretisms {S['syncretisms']}**, institutions built {S['institutions_built']}\n")

    L.append("## Economy, Culture & Environment\n")
    L.append(f"- Wealth Gini {f.get('wealthGini')} · mean personality drift: "
             f"openness {fi.get('meanOpenness')}→{f.get('meanOpenness')}, "
             f"neuroticism {fi.get('meanNeuroticism')}→{f.get('meanNeuroticism')}")
    L.append(f"- Great works of culture: {S['cultural_works']}")
    if S["disaster_types"]:
        L.append("- Natural disasters: " +
                 ", ".join(f"{t} ×{c}" for t, c in S["disaster_types"]))
    L.append(f"- Disease: {S['disease_contracted']} infections, {S['disease_cured']} cured "
             f"({100*S['disease_cured']/max(1,S['disease_contracted']):.1f}% recovery)\n")

    L.append("## Fertility\n")
    if S["top_parents"]:
        L.append("Most prolific parents: " +
                 ", ".join(f"{n} ({c})" for n, c in S["top_parents"][:6]))
    L.append(f"\nTotal births {S['total_births']}.\n")
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser(description="ASHB2 automated report generator")
    root = Path(__file__).resolve().parent.parent
    ap.add_argument("--data", default=str(root / "src" / "data"))
    ap.add_argument("--out", default=str(root / "plans" / "ASHB2_AutoReport.md"))
    ap.add_argument("--json", action="store_true", help="also dump raw stats as JSON")
    a = ap.parse_args()

    data = Path(a.data)
    print(f"Parsing logs in {data} ...")
    S = analyze(data)
    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render(S), encoding="utf-8")
    print(f"Wrote {out}")
    if a.json:
        j = out.with_suffix(".json")
        j.write_text(json.dumps(S, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"Wrote {j}")


if __name__ == "__main__":
    main()
