#!/bin/bash

# Process all utility C files with logging and unified memory integration

cd /home/bbrelin/nimcp

echo "========================================="
echo "Utils Integration - Logging & Unified Memory"
echo "========================================="
echo ""

# Counter
total=0
processed=0
skipped=0
errors=0

# Process each file
find /home/bbrelin/nimcp/src/utils -name "*.c" -type f | sort | while read file; do
    total=$((total + 1))

    # Get module name from path
    module=$(echo "$file" | sed 's|.*/src/utils/\([^/]*\)/.*|\1|')

    # Run integrator
    result=$(python3 scripts/integrate_all_utils.py "$file" 2>&1)
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        if echo "$result" | grep -q "SKIP"; then
            skipped=$((skipped + 1))
        else
            processed=$((processed + 1))
        fi
        echo "$result"
    else
        errors=$((errors + 1))
        echo "ERROR: $file"
        echo "$result"
    fi
done

echo ""
echo "========================================="
echo "Summary"
echo "========================================="
echo "Total files: $total"
echo "Processed: $processed"
echo "Skipped: $skipped"
echo "Errors: $errors"
