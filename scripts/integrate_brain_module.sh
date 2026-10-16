#!/bin/bash
#=============================================================================
# integrate_brain_module.sh - Integrate bio-async, logging, unified memory
#=============================================================================
# WHAT: Adds bio-async, logging, and unified memory to brain modules
# WHY:  Standardize all brain modules with modern infrastructure
# HOW:  Pattern-based replacements for includes and malloc/free calls
#
# Usage: ./integrate_brain_module.sh <file_path> <log_module_name>

set -e

FILE="$1"
LOG_MODULE="$2"

if [ -z "$FILE" ] || [ -z "$LOG_MODULE" ]; then
    echo "Usage: $0 <file_path> <log_module_name>"
    exit 1
fi

if [ ! -f "$FILE" ]; then
    echo "Error: File not found: $FILE"
    exit 1
fi

echo "Integrating $FILE with LOG_MODULE=$LOG_MODULE"

# Create backup
cp "$FILE" "${FILE}.bak"

# Add bio-async, logging, unified memory includes if not present
if ! grep -q "nimcp_bio_async.h" "$FILE"; then
    # Find the first #include line and insert before it
    sed -i '0,/^#include/s|^#include|// Bio-async integration\n#include "async/nimcp_bio_async.h"\n#include "async/nimcp_bio_router.h"\n#include "async/nimcp_bio_messages.h"\n\n// Logging integration\n#include "utils/logging/nimcp_logging.h"\n\n// Unified memory integration\n#include "utils/memory/nimcp_unified_memory.h"\n\n#include|' "$FILE"
fi

# Add LOG_MODULE define after includes if not present
if ! grep -q "#define LOG_MODULE" "$FILE"; then
    # Add after the last #include line
    sed -i '/^#include/,/^[^#]/{/^[^#]/i\
\
#define LOG_MODULE "'$LOG_MODULE'"
}' "$FILE"
fi

# Replace malloc with nimcp_malloc (avoid replacing in comments)
sed -i 's/\([^a-zA-Z_]\)malloc(/\1nimcp_malloc(/g' "$FILE"
sed -i 's/^malloc(/nimcp_malloc(/g' "$FILE"

# Replace calloc with nimcp_calloc (avoid replacing in comments)
sed -i 's/\([^a-zA-Z_]\)calloc(/\1nimcp_calloc(/g' "$FILE"
sed -i 's/^calloc(/nimcp_calloc(/g' "$FILE"

# Replace free with nimcp_free (avoid replacing in comments)
sed -i 's/\([^a-zA-Z_]\)free(/\1nimcp_free(/g' "$FILE"
sed -i 's/^free(/nimcp_free(/g' "$FILE"

echo "✓ Integrated bio-async, logging, unified memory into $FILE"
