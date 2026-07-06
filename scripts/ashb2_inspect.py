#!/usr/bin/env python3
"""ASHB2 save inspector (Improvement Plan item 17.2).

Parses an ASHB2_SAVE_V2 text save and prints formatted summaries: population,
age distribution, sex ratio, disease load, region spread, and the healthiest /
wealthiest individuals — without launching the simulation.

    python scripts/ashb2_inspect.py save.txt --population --ages --regions --diseases
    python scripts/ashb2_inspect.py save.txt --top 10
"""
import argparse, sys
from collections import Counter
from pathlib import Path

try:                       # Windows consoles default to cp1252; bar glyphs need UTF-8
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

SCALAR = {"ID": int, "NAME": str, "AGE": float, "HEALTH": float, "HAPPINESS": float,
          "STRESS": float, "MENTAL_HEALTH": float, "SEX": str, "ANTIBODY": float,
          "DISEASE": int, "POSX": float, "POSY": float, "ORIGIN_REGION": int}


def parse(path: Path):
    header = {}
    entities = []
    cur = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if line.startswith("--- ENTITY"):
                if cur is not None:
                    entities.append(cur)
                cur = {}
                continue
            if ":" not in line:
                continue
            key, _, val = line.partition(":")
            if cur is None:  # header region
                if key in ("DAY", "FRAME", "ENTITY_COUNT", "CLOCK_FRAME"):
                    header[key] = val.strip()
                continue
            if key in SCALAR:
                try:
                    cur[key] = SCALAR[key](val.strip())
                except (ValueError, TypeError):
                    cur[key] = val.strip()
    if cur:
        entities.append(cur)
    return header, entities


def main():
    ap = argparse.ArgumentParser(description="ASHB2 save inspector")
    ap.add_argument("save")
    ap.add_argument("--population", action="store_true")
    ap.add_argument("--ages", action="store_true")
    ap.add_argument("--regions", action="store_true")
    ap.add_argument("--diseases", action="store_true")
    ap.add_argument("--top", type=int, default=0, help="show N healthiest individuals")
    a = ap.parse_args()

    header, ents = parse(Path(a.save))
    alive = [e for e in ents if e.get("HEALTH", 0) > 0]
    show_all = not any([a.population, a.ages, a.regions, a.diseases, a.top])

    print(f"Save: {a.save}")
    print(f"  DAY={header.get('DAY')} FRAME={header.get('FRAME')} "
          f"declared ENTITY_COUNT={header.get('ENTITY_COUNT')}")
    print(f"  parsed entities={len(ents)}  living={len(alive)}")

    if a.population or show_all:
        sex = Counter(e.get("SEX", "?") for e in alive)
        avg = lambda k: (sum(e.get(k, 0) for e in alive) / len(alive)) if alive else 0
        print("\n[Population]")
        print(f"  sex ratio: " + ", ".join(f"{k}={v}" for k, v in sex.items()))
        print(f"  mean health {avg('HEALTH'):.1f}, happiness {avg('HAPPINESS'):.1f}, "
              f"stress {avg('STRESS'):.1f}, mental {avg('MENTAL_HEALTH'):.1f}")

    if a.ages or show_all:
        buckets = Counter()
        for e in alive:
            try:
                age = float(e.get("AGE", 0))
            except (ValueError, TypeError):
                age = 0.0
            b = int(age // 10) * 10
            buckets[b] += 1
        mx = max(buckets.values()) if buckets else 1
        print("\n[Age distribution]")
        for b in sorted(buckets):
            bar = "█" * int(30 * buckets[b] / mx)
            print(f"  {b:3d}-{b+9:<3d} {buckets[b]:5d} {bar}")

    if a.regions or show_all:
        reg = Counter(e.get("ORIGIN_REGION", -1) for e in alive)
        print("\n[Region spread]")
        for r, c in reg.most_common():
            print(f"  region {r}: {c}")

    if a.diseases or show_all:
        dis = Counter(e.get("DISEASE", -1) for e in alive)
        sick = sum(c for d, c in dis.items() if isinstance(d, int) and d >= 0)
        print("\n[Disease]")
        print(f"  infected {sick} / {len(alive)} "
              f"({100*sick/max(1,len(alive)):.1f}%)")

    if a.top:
        print(f"\n[Top {a.top} by health]")
        for e in sorted(alive, key=lambda x: x.get("HEALTH", 0), reverse=True)[:a.top]:
            print(f"  #{e.get('ID')} {e.get('NAME','?'):16s} "
                  f"age {e.get('AGE',0):5.0f}  health {e.get('HEALTH',0):5.1f}  "
                  f"region {e.get('ORIGIN_REGION','?')}")


if __name__ == "__main__":
    main()
