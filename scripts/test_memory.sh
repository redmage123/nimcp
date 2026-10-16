#!/bin/bash
export ASAN_OPTIONS=detect_leaks=0:detect_odr_violation=0
export LD_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/13/libasan.so

cd /home/bbrelin/nimcp/test

echo "Testing all modules for memory corruption..."
echo "=============================================="
echo ""

total=0
clean=0

for test in unit_* integration_* regression_*; do
    # Skip non-executables
    [[ ! -x "$test" ]] && continue
    [[ "$test" == *.txt ]] && continue
    [[ "$test" == *CMakeLists* ]] && continue

    total=$((total + 1))
    warnings=$(timeout 10 ./$test 2>&1 | grep "\[MEMORY\]" | wc -l)

    if [[ $warnings -eq 0 ]]; then
        clean=$((clean + 1))
        echo "✓ $test: 0 warnings"
    else
        echo "✗ $test: $warnings warnings"
    fi
done

echo ""
echo "=============================================="
echo "Summary:"
echo "  Total tests:    $total"
echo "  Clean:          $clean"  
echo "  With warnings:  $((total - clean))"
echo "=============================================="
