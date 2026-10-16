#!/bin/bash
# Add unified memory include to files that use nimcp_malloc/calloc/free

files=$(grep -rl "nimcp_calloc\|nimcp_malloc\|nimcp_free" src/ 2>/dev/null | xargs grep -L "nimcp_unified_memory.h" 2>/dev/null | grep "\.c$")

for file in $files; do
    # Check if file already has the include
    if ! grep -q "nimcp_unified_memory.h" "$file"; then
        echo "Adding unified memory include to: $file"
        # Add include after first #include line
        sed -i '0,/^#include/s/^#include.*$/&\n#include "utils\/memory\/nimcp_unified_memory.h"/' "$file"
    fi
done

echo "Done adding includes."
