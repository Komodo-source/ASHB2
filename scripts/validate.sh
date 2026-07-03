#!/usr/bin/env bash
# ASHB2 validation suite (M12) — build, determinism, realism.
# Usage:  scripts/validate.sh [ticks] [seed]
# Exit code 0 = everything passed; non-zero = something regressed.
set -u
TICKS="${1:-400}"
SEED="${2:-ci}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="${TMPDIR:-/tmp}/ashb2_validate_$$"
mkdir -p "$TMP/r1" "$TMP/r2"
cd "$ROOT"

fail=0

echo "== 1/3 build =="
# CI (MSYS2 shell) overrides these: the "MinGW Makefiles" generator refuses to
# configure when sh.exe is on PATH, so GitHub Actions uses "MSYS Makefiles"+make.
GEN="${ASHB_CMAKE_GEN:-MinGW Makefiles}"
MAKE="${ASHB_MAKE:-mingw32-make}"
cmake -G "$GEN" . >/dev/null 2>&1
if ! "$MAKE" -j8 2>&1 | grep -qE "Built target app"; then
    echo "BUILD: FAIL"; exit 1
fi
echo "BUILD: PASS"

clean_logs() { rm -f src/data/*_log.txt src/data/complete_logs.txt src/data/*.jsonl src/data/*.json src/data/*.csv; }

echo "== 2/3 determinism (two identical-seed runs) =="
for R in r1 r2; do
    clean_logs
    ./app.exe --headless "$TICKS" --seed "$SEED" --entities 40 --region 1 --chaos 1.3 \
        > "$TMP/$R/stdout.log" 2>&1 || { echo "RUN $R: crashed"; exit 1; }
    cp src/data/actions_log.txt src/data/deaths_log.txt src/data/births_log.txt \
       src/data/civilization_log.txt "$TMP/$R/" 2>/dev/null
done
for f in actions_log.txt deaths_log.txt births_log.txt civilization_log.txt; do
    # wall-clock timestamps differ between runs by construction; strip them
    sed 's/^\[[0-9: -]*\] //' "$TMP/r1/$f" > "$TMP/a.tmp"
    sed 's/^\[[0-9: -]*\] //' "$TMP/r2/$f" > "$TMP/b.tmp"
    if cmp -s "$TMP/a.tmp" "$TMP/b.tmp"; then
        echo "DETERMINISM $f: PASS"
    else
        echo "DETERMINISM $f: FAIL"; fail=1
    fi
done

echo "== 3/3 realism report =="
grep -E "^[1-5]\." "$TMP/r1/stdout.log"
if grep -qE "^[1-5]\..*FAIL" "$TMP/r1/stdout.log"; then
    echo "REALISM: FAIL"; fail=1
else
    echo "REALISM: PASS"
fi

rm -rf "$TMP"
exit $fail
