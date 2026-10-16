#!/bin/bash
export ASAN_OPTIONS=detect_leaks=0:detect_odr_violation=0
export LD_PRELOAD=/usr/lib/gcc/x86_64-linux-gnu/13/libasan.so

cd /home/bbrelin/nimcp/build/examples

echo "Testing PRODUCTION CODE (examples/demos) for memory corruption..."
echo "==================================================================="
echo ""

total=0
clean=0
failed=0

for demo in brain_demo simple_demo integrated_demo ethics_demo event_demo infant_demo neuromodulation_integration_example izhikevich_demo; do
    # Skip non-executables
    [[ ! -x "$demo" ]] && continue

    total=$((total + 1))
    echo -n "Testing $demo... "

    # Run demo and count memory warnings
    warnings=$(timeout 5 ./$demo 2>&1 | grep "\[MEMORY\]" | wc -l)
    exit_code=$(timeout 5 ./$demo > /dev/null 2>&1; echo $?)

    if [[ $exit_code -eq 0 || $exit_code -eq 124 ]]; then
        if [[ $warnings -eq 0 ]]; then
            clean=$((clean + 1))
            echo "✓ PASS, 0 warnings"
        else
            echo "✗ PASS, $warnings warnings"
        fi
    else
        failed=$((failed + 1))
        if [[ $warnings -eq 0 ]]; then
            echo "⚠ FAIL (exit $exit_code), 0 warnings"
        else
            echo "✗ FAIL (exit $exit_code), $warnings warnings"
        fi
    fi
done

echo ""
echo "==================================================================="
echo "Production Code Summary:"
echo "  Total programs:     $total"
echo "  Clean (0 warnings): $clean"
echo "  With warnings:      $((total - clean))"
echo "  Runtime failures:   $failed"
echo "==================================================================="
