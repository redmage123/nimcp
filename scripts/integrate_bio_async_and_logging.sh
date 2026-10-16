#!/bin/bash

# Bio-async and logging integration script
# Integrates bio-async communication and comprehensive logging into glial and security modules

set -e

echo "===================================================================="
echo "Bio-Async and Logging Integration Script"
echo "===================================================================="

# Array of glial modules to integrate (excluding already integrated ones)
GLIAL_MODULES=(
    "glial/myelin_sheath/nimcp_myelin_sheath"
    "glial/integration/nimcp_glial_integration"
    "glial/astrocyte_types/nimcp_astrocyte_types"
    "glial/astrocytes/nimcp_astrocyte_calcium"
    "glial/myelin_sheath/nimcp_myelin_math"
)

# Array of security modules to integrate
SECURITY_MODULES=(
    "security/nimcp_security"
    "security/nimcp_capability"
    "security/nimcp_cfi"
    "security/nimcp_security_audit"
    "security/nimcp_continuous_monitor"
    "security/nimcp_shadow_stack"
    "security/nimcp_security_coverage"
    "security/nimcp_security_fractal"
    "security/nimcp_security_integration"
    "security/nimcp_security_math"
    "security/nimcp_security_recovery_bridge"
    "security/nimcp_blood_brain_barrier"
    "security/nimcp_bbb_access_control"
    "security/nimcp_bbb_code_signing"
    "security/nimcp_bbb_input_gate"
    "security/nimcp_bbb_memory_boundary"
)

# Module IDs mapping
declare -A MODULE_IDS
MODULE_IDS["glial/myelin_sheath/nimcp_myelin_sheath"]="BIO_MODULE_MYELIN"
MODULE_IDS["glial/integration/nimcp_glial_integration"]="BIO_MODULE_GLIAL_INTEGRATION"
MODULE_IDS["glial/astrocyte_types/nimcp_astrocyte_types"]="BIO_MODULE_ASTROCYTE"
MODULE_IDS["glial/astrocytes/nimcp_astrocyte_calcium"]="BIO_MODULE_ASTROCYTE"
MODULE_IDS["glial/myelin_sheath/nimcp_myelin_math"]="BIO_MODULE_MYELIN"
MODULE_IDS["security/nimcp_security"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_capability"]="BIO_MODULE_CAPABILITY"
MODULE_IDS["security/nimcp_cfi"]="BIO_MODULE_CFI"
MODULE_IDS["security/nimcp_security_audit"]="BIO_MODULE_SECURITY_AUDIT"
MODULE_IDS["security/nimcp_continuous_monitor"]="BIO_MODULE_CONTINUOUS_MONITOR"
MODULE_IDS["security/nimcp_shadow_stack"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_security_coverage"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_security_fractal"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_security_integration"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_security_math"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_security_recovery_bridge"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_blood_brain_barrier"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_bbb_access_control"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_bbb_code_signing"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_bbb_input_gate"]="BIO_MODULE_SECURITY"
MODULE_IDS["security/nimcp_bbb_memory_boundary"]="BIO_MODULE_SECURITY"

# Log modules mapping
declare -A LOG_MODULES
LOG_MODULES["glial/myelin_sheath/nimcp_myelin_sheath"]="MYELIN"
LOG_MODULES["glial/integration/nimcp_glial_integration"]="GLIAL_INTEGRATION"
LOG_MODULES["glial/astrocyte_types/nimcp_astrocyte_types"]="ASTROCYTE_TYPES"
LOG_MODULES["glial/astrocytes/nimcp_astrocyte_calcium"]="ASTROCYTE_CALCIUM"
LOG_MODULES["glial/myelin_sheath/nimcp_myelin_math"]="MYELIN_MATH"
LOG_MODULES["security/nimcp_security"]="SECURITY"
LOG_MODULES["security/nimcp_capability"]="CAPABILITY"
LOG_MODULES["security/nimcp_cfi"]="CFI"
LOG_MODULES["security/nimcp_security_audit"]="SECURITY_AUDIT"
LOG_MODULES["security/nimcp_continuous_monitor"]="CONTINUOUS_MONITOR"
LOG_MODULES["security/nimcp_shadow_stack"]="SHADOW_STACK"
LOG_MODULES["security/nimcp_security_coverage"]="SECURITY_COVERAGE"
LOG_MODULES["security/nimcp_security_fractal"]="SECURITY_FRACTAL"
LOG_MODULES["security/nimcp_security_integration"]="SECURITY_INTEGRATION"
LOG_MODULES["security/nimcp_security_math"]="SECURITY_MATH"
LOG_MODULES["security/nimcp_security_recovery_bridge"]="SECURITY_RECOVERY_BRIDGE"
LOG_MODULES["security/nimcp_blood_brain_barrier"]="BLOOD_BRAIN_BARRIER"
LOG_MODULES["security/nimcp_bbb_access_control"]="BBB_ACCESS_CONTROL"
LOG_MODULES["security/nimcp_bbb_code_signing"]="BBB_CODE_SIGNING"
LOG_MODULES["security/nimcp_bbb_input_gate"]="BBB_INPUT_GATE"
LOG_MODULES["security/nimcp_bbb_memory_boundary"]="BBB_MEMORY_BOUNDARY"

# Counter for summary
TOTAL_MODIFIED=0
ALREADY_INTEGRATED=0
FAILED=0

# List of modified files
MODIFIED_FILES=()

check_bio_async_integration() {
    local src_file="$1"

    # Check if file already has bio-async includes
    if grep -q "bio_router.h" "$src_file" 2>/dev/null; then
        return 0  # Already integrated
    fi
    return 1  # Not integrated
}

echo ""
echo "Processing GLIAL modules..."
echo "--------------------------------------------------------------------"

for module in "${GLIAL_MODULES[@]}"; do
    SRC_FILE="src/${module}.c"
    HEADER_FILE="include/${module}.h"
    MODULE_ID="${MODULE_IDS[$module]}"
    LOG_MODULE_NAME="${LOG_MODULES[$module]}"

    echo "Processing: $module"

    if [ ! -f "$SRC_FILE" ]; then
        echo "  [SKIP] Source file not found: $SRC_FILE"
        continue
    fi

    if check_bio_async_integration "$SRC_FILE"; then
        echo "  [OK] Already has bio-async integration"
        ((ALREADY_INTEGRATED++))
        continue
    fi

    echo "  [INFO] Would integrate bio-async and logging"
    echo "         Module ID: $MODULE_ID"
    echo "         Log module: $LOG_MODULE_NAME"
    MODIFIED_FILES+=("$SRC_FILE")
    ((TOTAL_MODIFIED++))
done

echo ""
echo "Processing SECURITY modules..."
echo "--------------------------------------------------------------------"

for module in "${SECURITY_MODULES[@]}"; do
    SRC_FILE="src/${module}.c"
    HEADER_FILE="include/${module}.h"
    MODULE_ID="${MODULE_IDS[$module]}"
    LOG_MODULE_NAME="${LOG_MODULES[$module]}"

    echo "Processing: $module"

    if [ ! -f "$SRC_FILE" ]; then
        echo "  [SKIP] Source file not found: $SRC_FILE"
        continue
    fi

    if check_bio_async_integration "$SRC_FILE"; then
        echo "  [OK] Already has bio-async integration"
        ((ALREADY_INTEGRATED++))
        continue
    fi

    echo "  [INFO] Would integrate bio-async and logging"
    echo "         Module ID: $MODULE_ID"
    echo "         Log module: $LOG_MODULE_NAME"
    MODIFIED_FILES+=("$SRC_FILE")
    ((TOTAL_MODIFIED++))
done

echo ""
echo "===================================================================="
echo "INTEGRATION SUMMARY"
echo "===================================================================="
echo "Total modules to modify: $TOTAL_MODIFIED"
echo "Already integrated: $ALREADY_INTEGRATED"
echo "Failed: $FAILED"
echo ""
echo "Files that need integration:"
for file in "${MODIFIED_FILES[@]}"; do
    echo "  - $file"
done
echo ""
echo "===================================================================="
