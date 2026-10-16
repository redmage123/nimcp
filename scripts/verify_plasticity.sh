#!/bin/bash
echo "=== FINAL VERIFICATION ==="
echo

echo "Test 1: No raw malloc/free in predictive_coding.c"
if ! grep -q "\\bmalloc(\\|\\bcalloc(\\|\\bfree(" src/plasticity/predictive/nimcp_predictive_coding.c 2>/dev/null; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi
echo

echo "Test 2: No raw malloc/free in spatial_neuromod.c"
if ! grep -q "\\bmalloc(\\|\\bcalloc(\\|\\bfree(" src/plasticity/neuromodulators/nimcp_spatial_neuromod.c 2>/dev/null; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi
echo

echo "Test 3: Bio-async integrated in 10+ modules"
COUNT=$(grep -l "nimcp_bio_router" src/plasticity/*/*.c 2>/dev/null | wc -l)
echo "Count: $COUNT"
if [ $COUNT -ge 10 ]; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi
echo

echo "Test 4: Logging in 14+ modules"
COUNT=$(grep -l "#define LOG_MODULE" src/plasticity/*/*.c 2>/dev/null | wc -l)
echo "Count: $COUNT"
if [ $COUNT -ge 14 ]; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi
echo

echo "Test 5: Zero pthread violations"
COUNT=$(grep -c "pthread_" src/plasticity/*/*.c 2>/dev/null | awk '{s+=$1} END {print s}')
echo "Count: $COUNT"
if [ "$COUNT" -eq 0 ]; then
    echo "✓ PASS"
else
    echo "✗ FAIL"
fi
echo

echo "=== ALL TESTS COMPLETE ==="
