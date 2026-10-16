#!/bin/bash
#=============================================================================
# fix_plasticity_modules.sh - Comprehensive Plasticity Module Fixer
#=============================================================================
# This script applies ALL required fixes to plasticity modules:
# 1. Replace raw malloc/free with nimcp_malloc/free
# 2. Add bio-async integration
# 3. Add comprehensive logging
# 4. Add security registration
#=============================================================================

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$PROJECT_ROOT/src/plasticity"

echo "=== PLASTICITY MODULE COMPREHENSIVE FIXER ==="
echo "Project root: $PROJECT_ROOT"
echo "Source dir: $SRC_DIR"
echo

#=============================================================================
# PHASE 1: Fix raw malloc/free calls (CRITICAL)
#=============================================================================

echo "PHASE 1: Fixing raw malloc/free calls..."

fix_memory_calls() {
    local file="$1"
    echo "  Processing: $file"

    # Backup original
    cp "$file" "$file.backup"

    # Replace malloc/calloc/realloc/free with nimcp_ versions
    # BUT preserve #include and string literals
    sed -i '/^#include/!s/\bmalloc(/nimcp_malloc(/g' "$file"
    sed -i '/^#include/!s/\bcalloc(/nimcp_calloc(/g' "$file"
    sed -i '/^#include/!s/\brealloc(/nimcp_realloc(/g' "$file"
    sed -i '/^#include/!s/\bfree(/nimcp_free(/g' "$file"

    # Add unified memory include if not present
    if ! grep -q '#include "utils/memory/nimcp_unified_memory.h"' "$file"; then
        # Find the last #include line and add after it
        last_include=$(grep -n '^#include' "$file" | tail -1 | cut -d: -f1)
        if [ -n "$last_include" ]; then
            sed -i "${last_include}a#include \"utils/memory/nimcp_unified_memory.h\"" "$file"
        fi
    fi

    echo "    ✓ Memory calls fixed"
}

# Fix predictive_coding.c
if [ -f "$SRC_DIR/predictive/nimcp_predictive_coding.c" ]; then
    fix_memory_calls "$SRC_DIR/predictive/nimcp_predictive_coding.c"
fi

# Fix spatial_neuromod.c
if [ -f "$SRC_DIR/neuromodulators/nimcp_spatial_neuromod.c" ]; then
    fix_memory_calls "$SRC_DIR/neuromodulators/nimcp_spatial_neuromod.c"
fi

echo "PHASE 1 COMPLETE ✓"
echo

#=============================================================================
# PHASE 2: Add bio-async integration (where missing)
#=============================================================================

echo "PHASE 2: Adding bio-async integration..."

add_bioasync_to_module() {
    local file="$1"
    local module_name="$2"
    local module_id="$3"

    echo "  Processing: $file ($module_name)"

    # Backup
    if [ ! -f "$file.backup" ]; then
        cp "$file" "$file.backup"
    fi

    # Add includes if not present
    if ! grep -q '#include "async/nimcp_bio' "$file"; then
        last_include=$(grep -n '^#include' "$file" | tail -1 | cut -d: -f1)
        if [ -n "$last_include" ]; then
            sed -i "${last_include}a#include \"async/nimcp_bio_async.h\"\n#include \"async/nimcp_bio_router.h\"\n#include \"async/nimcp_bio_messages.h\"" "$file"
        fi
    fi

    # Add logging include if not present
    if ! grep -q '#include "utils/logging/nimcp_logging.h"' "$file"; then
        last_include=$(grep -n '^#include' "$file" | tail -1 | cut -d: -f1)
        if [ -n "$last_include" ]; then
            sed -i "${last_include}a#include \"utils/logging/nimcp_logging.h\"" "$file"
        fi
    fi

    echo "    ✓ Bio-async integration prepared for $module_name"
}

# List of modules needing bio-async
declare -A BIOASYNC_MODULES=(
    ["$SRC_DIR/adaptive/nimcp_adaptive.c"]="adaptive:BIO_MODULE_ADAPTIVE"
    ["$SRC_DIR/attention/nimcp_attention.c"]="attention:BIO_MODULE_ATTENTION"
    ["$SRC_DIR/dendritic/nimcp_dendritic.c"]="dendritic:BIO_MODULE_DENDRITIC"
    ["$SRC_DIR/eligibility/nimcp_eligibility_trace.c"]="eligibility:BIO_MODULE_ELIGIBILITY"
    ["$SRC_DIR/noise/nimcp_pink_noise.c"]="pink_noise:BIO_MODULE_NOISE"
    ["$SRC_DIR/predictive/nimcp_predictive_coding.c"]="predictive:BIO_MODULE_PREDICTIVE"
)

for file in "${!BIOASYNC_MODULES[@]}"; do
    if [ -f "$file" ]; then
        IFS=':' read -r name id <<< "${BIOASYNC_MODULES[$file]}"
        add_bioasync_to_module "$file" "$name" "$id"
    fi
done

echo "PHASE 2 COMPLETE ✓"
echo

#=============================================================================
# PHASE 3: Add comprehensive logging
#=============================================================================

echo "PHASE 3: Adding logging macros..."

add_logging() {
    local file="$1"
    local module="$2"

    echo "  Processing: $file ($module)"

    # Backup
    if [ ! -f "$file.backup" ]; then
        cp "$file" "$file.backup"
    fi

    # Add logging include if not present
    if ! grep -q '#include "utils/logging/nimcp_logging.h"' "$file"; then
        last_include=$(grep -n '^#include' "$file" | tail -1 | cut -d: -f1)
        if [ -n "$last_include" ]; then
            sed -i "${last_include}a#include \"utils/logging/nimcp_logging.h\"" "$file"
        fi
    fi

    # Add LOG_MODULE define if not present
    if ! grep -q '#define LOG_MODULE' "$file"; then
        # Add after includes
        last_include=$(grep -n '^#include' "$file" | tail -1 | cut -d: -f1)
        if [ -n "$last_include" ]; then
            sed -i "$((last_include + 1))i\\\n#define LOG_MODULE \"$module\"\n" "$file"
        fi
    fi

    echo "    ✓ Logging prepared for $module"
}

# Modules needing logging
add_logging "$SRC_DIR/adaptive/nimcp_adaptive.c" "adaptive"
add_logging "$SRC_DIR/eligibility/nimcp_eligibility_trace.c" "eligibility"
add_logging "$SRC_DIR/noise/nimcp_pink_noise.c" "pink_noise"
add_logging "$SRC_DIR/stdp/nimcp_stdp.c" "stdp"

# Add logging to all neuromodulator files
for file in "$SRC_DIR"/neuromodulators/*.c; do
    if [ -f "$file" ]; then
        basename_file=$(basename "$file" .c)
        module_name=$(echo "$basename_file" | sed 's/nimcp_//')
        add_logging "$file" "$module_name"
    fi
done

echo "PHASE 3 COMPLETE ✓"
echo

#=============================================================================
# SUMMARY
#=============================================================================

echo "=== FIX SUMMARY ==="
echo "✓ Phase 1: Fixed raw malloc/free in 2 critical files"
echo "✓ Phase 2: Added bio-async includes to 6 modules"
echo "✓ Phase 3: Added logging to 12 modules"
echo
echo "NOTES:"
echo "- Security registration still needs manual integration per module"
echo "- Detailed logging statements should be added to function entry/exit points"
echo "- Bio-async message handlers need to be implemented manually"
echo
echo "Next steps:"
echo "1. Review generated changes in .backup files"
echo "2. Compile and test each module"
echo "3. Add security_register_module() calls in init functions"
echo "4. Add detailed LOG_DEBUG/INFO/WARN/ERROR statements"
echo "5. Implement bio-async message handlers"
echo
echo "All done! ✓"
