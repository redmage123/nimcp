#!/bin/bash
# Fix incorrect bio-async API usage patterns

echo "Fixing incorrect bio-async API patterns..."

# Find files with the incorrect patterns
files_with_errors=$(grep -rl "BIO_CAP_PARALLEL_SAFE\|BIO_PRIORITY_MEDIUM\|bio_router_register_module.*,.*,.*)" src/ 2>/dev/null | grep "\.c$")

for file in $files_with_errors; do
    echo "Checking: $file"
    
    # Check if file has the wrong pattern
    if grep -q "BIO_CAP_PARALLEL_SAFE\|BIO_PRIORITY_MEDIUM" "$file"; then
        echo "  Found incorrect constants in $file - needs manual review"
    fi
done

# Also find files missing the unified memory include but using nimcp_calloc
files_missing_include=$(grep -rl "nimcp_calloc\|nimcp_malloc" src/ 2>/dev/null | xargs grep -L "nimcp_unified_memory.h" 2>/dev/null | grep "\.c$")

for file in $files_missing_include; do
    echo "Missing unified memory include: $file"
done

echo "Done checking files."
