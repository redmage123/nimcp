#!/usr/bin/env bash
# Run all smoke tests. Fails fast on first error.
set -e
cd "$(dirname "$0")/../.."

echo "=== tests/smoke/ — run_all.sh ==="
echo ""

FAILED=0
for test in tests/smoke/test_*.py; do
    name=$(basename "$test" .py)
    echo "--- $name ---"
    if ! python3 "$test" 2>&1 | grep -vE "^\[.*\] \[INFO" | grep -vE "^\[.*bio_async"; then
        FAILED=$((FAILED + 1))
        echo "[$name FAILED]"
    fi
    echo ""
done

if [ "$FAILED" -gt 0 ]; then
    echo "=== $FAILED smoke test(s) failed ==="
    exit 1
fi

echo "=== All smoke tests passed ==="
