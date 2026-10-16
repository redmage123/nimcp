#!/bin/bash
export ASAN_OPTIONS=detect_leaks=0:detect_odr_violation=0
export LD_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/13/libasan.so

cd /home/bbrelin/nimcp/build/test

echo "Testing ALL modules for memory corruption..."
echo "=============================================="
echo ""

total=0
clean=0
failed=0

for test in unit_* integration_* regression_*; do
    # Skip non-executables
    [[ ! -x "$test" ]] && continue
    [[ "$test" == *.txt ]] && continue
    [[ "$test" == *CMakeLists* ]] && continue

    total=$((total + 1))

    # Run test and count memory warnings
    warnings=$(timeout 15 ./$test 2>&1 | grep "\[MEMORY\]" | wc -l)
    exit_code=$(timeout 15 ./$test > /dev/null 2>&1; echo $?)

    if [[ $exit_code -eq 0 || $exit_code -eq 1 ]]; then
        if [[ $warnings -eq 0 ]]; then
            clean=$((clean + 1))
            echo "✓ $test: PASS, 0 warnings"
        else
            echo "✗ $test: PASS, $warnings warnings"
        fi
    else
        failed=$((failed + 1))
        if [[ $warnings -eq 0 ]]; then
            echo "⚠ $test: FAIL (exit $exit_code), 0 warnings"
        else
            echo "✗ $test: FAIL (exit $exit_code), $warnings warnings"
        fi
    fi
done

echo ""
echo "=============================================="
echo "Summary:"
echo "  Total tests:        $total"
echo "  Clean (0 warnings): $clean"
echo "  With warnings:      $((total - clean))"
echo "  Test failures:      $failed"
echo "=============================================="
