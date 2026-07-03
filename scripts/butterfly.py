#!/usr/bin/env python3
"""M10: A/B butterfly viewer.

Compare two tick_history.jsonl exports (same seed + one perturbation, e.g. a
God Console smite in world B) and chart how far the histories diverge over
time — the butterfly effect, measured.

Usage:
    python scripts/butterfly.py runA/tick_history.jsonl runB/tick_history.jsonl

Typical workflow:
    ./app.exe --headless 300 --seed twin            # world A
    cp src/data/tick_history.jsonl /tmp/a.jsonl
    ./app.exe --headless 300 --seed twin --load perturbed.txt   # world B
    python scripts/butterfly.py /tmp/a.jsonl src/data/tick_history.jsonl

Divergence metrics per sampled day:
  pop_delta   absolute population difference
  survivors   entity ids alive in A but not B (or vice versa), as a fraction
  drift       mean position distance of entities alive in both worlds
  mood        mean |happiness_A - happiness_B| over shared entities
"""
import json
import math
import sys


def load(path):
    """day -> {id -> entity record}"""
    days = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                rec = json.loads(line)
            except json.JSONDecodeError:
                continue  # truncated tail line from an interrupted run
            days[rec["day"]] = {e["id"]: e for e in rec.get("entities", [])}
    return days


def bar(frac, width=40):
    n = max(0, min(width, int(round(frac * width))))
    return "#" * n + "." * (width - n)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)

    a_days, b_days = load(sys.argv[1]), load(sys.argv[2])
    shared_days = sorted(set(a_days) & set(b_days))
    if not shared_days:
        print("No overlapping sampled days between the two runs.")
        sys.exit(1)

    print(f"{'day':>6} | {'popA':>5} {'popB':>5} | {'onlyIn1':>7} | "
          f"{'drift':>8} | {'mood':>6} | divergence")
    print("-" * 100)

    world_w = 1280.0  # matches the sim window; only used to normalise drift
    for day in shared_days:
        a, b = a_days[day], b_days[day]
        ids_a, ids_b = set(a), set(b)
        shared = ids_a & ids_b
        only = len(ids_a ^ ids_b)
        union = len(ids_a | ids_b)

        drift = mood = 0.0
        if shared:
            for i in shared:
                dx = a[i]["posX"] - b[i]["posX"]
                dy = a[i]["posY"] - b[i]["posY"]
                drift += math.sqrt(dx * dx + dy * dy)
                mood += abs(a[i]["happiness"] - b[i]["happiness"])
            drift /= len(shared)
            mood /= len(shared)

        # 0..1 composite: population mismatch + normalised drift + mood gap
        div = 0.0
        if union:
            div += (only / union) * 0.5
        div += min(1.0, drift / world_w) * 0.3
        div += min(1.0, mood / 50.0) * 0.2

        print(f"{day:>6} | {len(ids_a):>5} {len(ids_b):>5} | {only:>7} | "
              f"{drift:>8.1f} | {mood:>6.1f} | {bar(div)} {div * 100:.0f}%")

    print("\ndivergence = 0.5*(ids alive in only one world / union) "
          "+ 0.3*(mean pos drift / world width) + 0.2*(mean |happiness gap| / 50)")


if __name__ == "__main__":
    main()
