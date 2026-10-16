#!/bin/bash
# Quick coverage analysis script

cd /home/bbrelin/nimcp/build/test

echo "Analyzing coverage from .gcda files..."
echo "========================================"

total_files=0
covered_files=0
total_lines=0
covered_lines=0

# Sample 50 gcda files for quick analysis
for gcda in $(find . -name "*.gcda" | head -50); do
    output=$(gcov -o "$(dirname "$gcda")" "$gcda" 2>/dev/null)

    # Extract lines executed percentage
    lines=$(echo "$output" | grep "Lines executed" | head -1 | awk '{print $3}' | tr -d '%')

    if [[ -n "$lines" ]]; then
        total_files=$((total_files + 1))
        covered_lines=$(echo "$covered_lines + $lines" | bc 2>/dev/null || echo "$covered_lines")
    fi
done

if [[ $total_files -gt 0 ]]; then
    avg_coverage=$(echo "scale=2; $covered_lines / $total_files" | bc)
    echo "Sample Analysis (50 files):"
    echo "  Files analyzed: $total_files"
    echo "  Average coverage: ${avg_coverage}%"
else
    echo "No coverage data found"
fi

echo ""
echo "Total .gcda files: $(find . -name "*.gcda" | wc -l)"
echo "Total .gcno files: $(find . -name "*.gcno" | wc -l)"
