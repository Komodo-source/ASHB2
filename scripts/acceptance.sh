#!/usr/bin/env bash
# ASHB2 §10 acceptance run — the "parallel universe" bar from
# plans/parallel-earth-upgrade.md. This is NOT the CI gate (that is
# scripts/validate.sh, which runs short and fast); this is the long unattended
# run the plan is actually judged on, and it takes ~15 minutes.
#
#   scripts/acceptance.sh [ticks] [seed] [entities]
#
# Exit code 0 = every hard assertion passed. Tests that print [warn] are soft:
# the run was too short or too small for that measurement to mean anything,
# which is a "not measured", not a "failed".
set -u
TICKS="${1:-3000}"
SEED="${2:-acc1}"
ENTS="${3:-150}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

GEN="${ASHB_CMAKE_GEN:-MinGW Makefiles}"
MAKE="${ASHB_MAKE:-mingw32-make}"
cmake -G "$GEN" . >/dev/null 2>&1
if ! "$MAKE" -j8 2>&1 | grep -qE "Built target app"; then
    echo "BUILD: FAIL"; exit 1
fi

LOG="${TMPDIR:-/tmp}/ashb2_acceptance_$$.log"
echo "== running $TICKS ticks, seed '$SEED', $ENTS entities (this takes a while) =="
./app.exe --headless "$TICKS" --seed "$SEED" --entities "$ENTS" --region 1 --chaos 1.3 \
    > "$LOG" 2>&1 || { echo "RUN: crashed"; exit 1; }

grep -E "^[0-9]{1,2}\. " "$LOG"

fails=$(grep -cE "^[0-9]{1,2}\..*\[FAIL\]" "$LOG" || true)
warns=$(grep -cE "^[0-9]{1,2}\..*\[warn\]" "$LOG" || true)
echo
echo "== $fails failed, $warns unmeasured (log: $LOG) =="
[ "$fails" -eq 0 ]
