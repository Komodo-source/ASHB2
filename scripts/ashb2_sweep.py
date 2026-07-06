#!/usr/bin/env python3
"""ASHB2 parameter-sweep harness (Improvement Plan item 21).

Runs the headless simulation across a grid of seeds (and any other CLI knobs the
binary supports), scrapes the final history snapshot from civilization_log.txt
after each run, and writes one CSV row per run for cross-seed analysis.

The engine exposes `--headless <ticks>` and `--seed <text|num>`, so this harness
needs no special build. Example:

    python scripts/ashb2_sweep.py --ticks 4000 --seeds 1 2 3 4 5 --out sweeps/seeds.csv
"""
import argparse, csv, re, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SNAP_RE = re.compile(r"kind=snapshot .*")
FIELDS = ["population", "peakPopulation", "tribes", "religions", "innovations",
          "darkAges", "warsDeclared", "ethnicWars", "battles", "warDeaths",
          "conquests", "treatiesSigned", "wealthGini", "avgCulture", "culturalWorks",
          "avgFortification", "disasters", "civilWars", "colonies", "greatFamilies",
          "sagas", "techSpreads", "meanOpenness", "meanNeuroticism"]


def kv(body, key):
    m = re.search(rf'{key}=(?:"([^"]*)"|(\S+))', body)
    if not m:
        return ""
    return m.group(1) if m.group(1) is not None else m.group(2)


def final_snapshot(civ_log: Path):
    last = None
    if not civ_log.exists():
        return None
    with open(civ_log, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "kind=snapshot" in line:
                last = line
    if not last:
        return None
    row = {"era": kv(last, "era"), "year": kv(last, "year")}
    for fld in FIELDS:
        row[fld] = kv(last, fld)
    return row


def main():
    ap = argparse.ArgumentParser(description="ASHB2 parameter sweep")
    ap.add_argument("--exe", default=str(ROOT / "build" / "app.exe"))
    ap.add_argument("--data", default=str(ROOT / "src" / "data"))
    ap.add_argument("--ticks", type=int, default=3000)
    ap.add_argument("--seeds", nargs="+", default=["1", "2", "3"])
    ap.add_argument("--extra", nargs=argparse.REMAINDER, default=[],
                    help="extra args passed verbatim to every run (after --extra)")
    ap.add_argument("--out", default=str(ROOT / "sweeps" / "sweep.csv"))
    ap.add_argument("--timeout", type=int, default=3600)
    a = ap.parse_args()

    exe = Path(a.exe)
    if not exe.exists():
        sys.exit(f"executable not found: {exe} (build it first)")
    civ_log = Path(a.data) / "civilization_log.txt"
    out = Path(a.out); out.parent.mkdir(parents=True, exist_ok=True)

    rows = []
    for seed in a.seeds:
        cmd = [str(exe), "--headless", str(a.ticks), "--seed", str(seed)] + a.extra
        print(f"[sweep] seed={seed} ticks={a.ticks} → running ...", flush=True)
        t0 = time.time()
        try:
            subprocess.run(cmd, cwd=str(ROOT), timeout=a.timeout,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except subprocess.TimeoutExpired:
            print(f"[sweep] seed={seed} TIMED OUT after {a.timeout}s")
        dt = time.time() - t0
        snap = final_snapshot(civ_log)
        if snap is None:
            print(f"[sweep] seed={seed}: no snapshot found (run may have failed)")
            continue
        snap = {"seed": seed, "wall_seconds": round(dt, 1), **snap}
        rows.append(snap)
        print(f"[sweep] seed={seed}: era={snap['era']} pop={snap['population']} "
              f"techs={snap['innovations']} darkAges={snap['darkAges']} "
              f"warDeaths={snap['warDeaths']} ({dt:.0f}s)")

    if not rows:
        sys.exit("no successful runs — nothing written")
    cols = ["seed", "wall_seconds", "era", "year"] + FIELDS
    with open(out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)
    print(f"[sweep] wrote {len(rows)} rows → {out}")


if __name__ == "__main__":
    main()
