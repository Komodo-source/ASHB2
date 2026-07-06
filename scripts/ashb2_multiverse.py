#!/usr/bin/env python3
"""ASHB2 multiverse comparison (Improvement Plan item 22).

Compares several runs side by side. Input is either a sweep CSV (from
ashb2_sweep.py) or a list of civilization_log.txt files. Prints a comparison
table, ranks runs by "advancement", and flags simple correlations.

    python scripts/ashb2_multiverse.py --csv sweeps/seeds.csv
    python scripts/ashb2_multiverse.py --logs runA/civilization_log.txt runB/civilization_log.txt
"""
import argparse, csv, re, sys
from pathlib import Path

ERA_ORDER = ["Stone Age", "Tribal Age", "Early Agriculture", "Bronze Age", "Iron Age",
             "Classical Era", "Medieval Era", "Renaissance", "Early Modern", "Modern Era"]
NUM = ["population", "peakPopulation", "innovations", "darkAges", "warsDeclared",
       "ethnicWars", "warDeaths", "conquests", "wealthGini", "avgCulture", "disasters"]


def era_rank(e):
    try:
        return ERA_ORDER.index(e)
    except ValueError:
        return -1


def kv(body, key):
    m = re.search(rf'{key}=(?:"([^"]*)"|(\S+))', body)
    return (m.group(1) if m.group(1) is not None else m.group(2)) if m else ""


def from_log(path):
    last = None
    for line in open(path, encoding="utf-8", errors="replace"):
        if "kind=snapshot" in line:
            last = line
    if not last:
        return None
    row = {"run": Path(path).parent.name or Path(path).stem, "era": kv(last, "era")}
    for k in NUM:
        row[k] = kv(last, k)
    return row


def num(v):
    try:
        return float(v)
    except (ValueError, TypeError):
        return 0.0


def main():
    ap = argparse.ArgumentParser(description="ASHB2 multiverse comparison")
    ap.add_argument("--csv")
    ap.add_argument("--logs", nargs="+")
    a = ap.parse_args()

    rows = []
    if a.csv:
        with open(a.csv, encoding="utf-8") as f:
            for r in csv.DictReader(f):
                r["run"] = r.get("seed", r.get("run", "?"))
                rows.append(r)
    elif a.logs:
        for p in a.logs:
            r = from_log(p)
            if r:
                rows.append(r)
    else:
        sys.exit("provide --csv or --logs")
    if not rows:
        sys.exit("no runs parsed")

    # Advancement score: era + tech + population, penalised by dark ages.
    def score(r):
        return era_rank(r.get("era", "")) * 100 + num(r.get("innovations")) * 3 \
            + num(r.get("population")) * 0.02 - num(r.get("darkAges")) * 2
    rows.sort(key=score, reverse=True)

    cols = ["run", "era", "innovations", "population", "peakPopulation",
            "darkAges", "warsDeclared", "warDeaths", "conquests", "wealthGini"]
    widths = {c: max(len(c), *(len(str(r.get(c, ""))) for r in rows)) for c in cols}
    print("  ".join(c.ljust(widths[c]) for c in cols))
    print("  ".join("-" * widths[c] for c in cols))
    for r in rows:
        print("  ".join(str(r.get(c, "")).ljust(widths[c]) for c in cols))

    best, worst = rows[0], rows[-1]
    print(f"\nMost advanced: run {best['run']} ({best.get('era')}, "
          f"{best.get('innovations')} techs)")
    print(f"Least advanced: run {worst['run']} ({worst.get('era')}, "
          f"{worst.get('innovations')} techs)")

    # Simple correlation: dark ages vs. tech count (Pearson).
    xs = [num(r.get("darkAges")) for r in rows]
    ys = [num(r.get("innovations")) for r in rows]
    if len(xs) >= 3:
        n = len(xs); mx = sum(xs) / n; my = sum(ys) / n
        cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
        vx = sum((x - mx) ** 2 for x in xs) ** 0.5
        vy = sum((y - my) ** 2 for y in ys) ** 0.5
        if vx > 0 and vy > 0:
            print(f"\nCorrelation(darkAges, techCount) = {cov/(vx*vy):+.2f} "
                  f"(negative = dark ages suppress tech, as expected)")


if __name__ == "__main__":
    main()
