#!/usr/bin/env bash
# Regression gate — run before deploying any change.
#
# Runs in order of increasing cost; fails fast.
#   1. Build check (compile + link)
#   2. Python import check
#   3. Smoke tests (fast, deterministic)
#   4. Baseline-compare (if baseline exists)
#
# Usage:
#   tests/regression/run_regression.sh              # standard run
#   tests/regression/run_regression.sh --skip-build # skip rebuild
#   tests/regression/run_regression.sh --capture    # update baseline
set -e

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO"

SKIP_BUILD=0
CAPTURE=0
for arg in "$@"; do
    [ "$arg" = "--skip-build" ] && SKIP_BUILD=1
    [ "$arg" = "--capture" ] && CAPTURE=1
done

echo "═══════════════════════════════════════════════════════════"
echo "  NIMCP Regression Test Gate"
echo "═══════════════════════════════════════════════════════════"
echo ""

# --- 1. Build ---
if [ "$SKIP_BUILD" = "0" ]; then
    echo "[1/4] Build check"
    cd build
    if ! make nimcp nimcp_python -j4 2>&1 | tail -3; then
        echo "  FAIL: build failed"
        exit 1
    fi
    cd ..
    echo "  PASS: build clean"
    echo ""
else
    echo "[1/4] Build check  SKIPPED"
    echo ""
fi

# --- 2. Python import ---
echo "[2/4] Python import check"
if ! python3 -c "import nimcp; print('nimcp version:', getattr(nimcp, '__version__', '?'))" 2>&1 | grep -E "^nimcp version"; then
    echo "  FAIL: nimcp cannot be imported"
    exit 1
fi
echo "  PASS: import OK"
echo ""

# --- 3. Smoke tests ---
echo "[3/4] Smoke test suite"
FAILED_TESTS=()
for test in tests/smoke/test_*.py; do
    name=$(basename "$test" .py)
    if ! python3 "$test" 2>&1 | grep -qE "^All .* smoke tests passed"; then
        FAILED_TESTS+=("$name")
    fi
done
if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
    echo "  FAIL: ${#FAILED_TESTS[@]} smoke tests failed: ${FAILED_TESTS[*]}"
    exit 1
fi
echo "  PASS: all smoke tests passed"
echo ""

# --- 4. Baseline comparison ---
echo "[4/4] Baseline comparison"
BASELINE_FILE="tests/baseline/battery_baseline.json"

if [ "$CAPTURE" = "1" ]; then
    echo "  Capturing new baseline to $BASELINE_FILE..."
    python3 tests/regression/capture_baseline.py "$BASELINE_FILE"
    echo "  Baseline captured."
elif [ ! -f "$BASELINE_FILE" ]; then
    echo "  SKIP: no baseline at $BASELINE_FILE (run with --capture first)"
else
    if ! python3 tests/regression/compare_baseline.py "$BASELINE_FILE"; then
        echo "  FAIL: baseline regression detected"
        exit 1
    fi
    echo "  PASS: within baseline tolerance"
fi

echo ""
echo "═══════════════════════════════════════════════════════════"
echo "  All regression checks passed"
echo "═══════════════════════════════════════════════════════════"
