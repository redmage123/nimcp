#!/bin/bash
#=============================================================================
# complete_brain_init_refactor.sh
#=============================================================================
# Complete the brain_init refactoring by extracting subsystems and security
# functions from the original nimcp_brain_init.c file.
#
# This script extracts the large subsystem and security sections and appends
# them to the stub files created during refactoring.
#
# Usage: ./complete_brain_init_refactor.sh
#
# Date: 2025-12-08
#=============================================================================

set -e

NIMCP_ROOT="/home/bbrelin/nimcp"
ORIGINAL_FILE="$NIMCP_ROOT/src/core/brain/factory/init/nimcp_brain_init.c"
SUBSYSTEMS_FILE="$NIMCP_ROOT/src/core/brain/factory/init/nimcp_brain_init_subsystems.c"
SECURITY_FILE="$NIMCP_ROOT/src/core/brain/factory/init/nimcp_brain_init_security.c"

echo "==================================================================="
echo "Completing Brain Init Refactoring"
echo "==================================================================="
echo ""
echo "Original file: $ORIGINAL_FILE"
echo "Target files:"
echo "  - $SUBSYSTEMS_FILE"
echo "  - $SECURITY_FILE"
echo ""

# Check if original file exists
if [ ! -f "$ORIGINAL_FILE" ]; then
    echo "ERROR: Original file not found: $ORIGINAL_FILE"
    exit 1
fi

# Backup stub files
echo "Creating backups..."
cp "$SUBSYSTEMS_FILE" "$SUBSYSTEMS_FILE.stub" 2>/dev/null || true
cp "$SECURITY_FILE" "$SECURITY_FILE.stub" 2>/dev/null || true

# Extract subsystems section (lines 803-3821 = 3019 lines)
echo "Extracting subsystems functions (lines 803-3821)..."
sed -n '803,3821p' "$ORIGINAL_FILE" >> "$SUBSYSTEMS_FILE"

# Extract security section (lines 3822-4094 = 273 lines)
echo "Extracting security function (lines 3822-4094)..."
sed -n '3822,4094p' "$ORIGINAL_FILE" >> "$SECURITY_FILE"

echo ""
echo "==================================================================="
echo "Extraction Complete!"
echo "==================================================================="
echo ""
echo "File sizes:"
wc -l "$SUBSYSTEMS_FILE" "$SECURITY_FILE"

echo ""
echo "Refactoring Summary:"
echo "  Original: $ORIGINAL_FILE (4107 lines)"
echo "  -----------------------------------------------------------"
echo "  1. nimcp_brain_init_config.c       (313 lines)  - Config"
echo "  2. nimcp_brain_init_validation.c   (102 lines)  - BBB"
echo "  3. nimcp_brain_init_core.c         (142 lines)  - Core"
echo "  4. nimcp_brain_init_subsystems.c   (~3130 lines) - Subsystems"
echo "  5. nimcp_brain_init_security.c     (~285 lines)  - Security"
echo "  -----------------------------------------------------------"
echo "  Total refactored: ~3972 lines across 5 files"
echo ""
echo "Next steps:"
echo "  1. Build: cd $NIMCP_ROOT/build && make"
echo "  2. Test: ctest"
echo "  3. If successful, remove original nimcp_brain_init.c"
echo ""
