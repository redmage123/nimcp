#!/bin/bash
# Script to integrate bio-async, logging, and unified memory into cognitive modules
# Generated: 2025-11-28

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NIMCP_ROOT="${SCRIPT_DIR}/.."
SRC_DIR="${NIMCP_ROOT}/src/cognitive"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counter for stats
total_files=0
success_files=0
failed_files=0

echo "=================================================="
echo "NIMCP Cognitive Bio-Async Integration Script"
echo "=================================================="
echo ""

# Function to add includes to a file
add_bioasync_includes() {
    local file="$1"
    local module_name="$2"
    local module_id="$3"

    echo -e "${YELLOW}Processing: ${file}${NC}"

    # Check if already integrated
    if grep -q "async/nimcp_bio_async.h" "$file" 2>/dev/null; then
        echo -e "${GREEN}  ✓ Already integrated${NC}"
        return 0
    fi

    # Create backup
    cp "$file" "${file}.bak"

    # Find the first #include line
    local first_include_line=$(grep -n "^#include" "$file" | head -1 | cut -d: -f1)

    if [ -z "$first_include_line" ]; then
        echo -e "${RED}  ✗ No include statements found${NC}"
        rm "${file}.bak"
        return 1
    fi

    # Insert bio-async includes after existing includes
    local insert_line=$((first_include_line))

    # Create temp file with new includes
    head -n $insert_line "$file" > "${file}.tmp"
    cat >> "${file}.tmp" <<EOF
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"

#define LOG_MODULE "${module_name}"
#define BIO_MODULE_${module_name^^} ${module_id}

EOF
    tail -n +$((insert_line + 1)) "$file" >> "${file}.tmp"

    mv "${file}.tmp" "$file"
    echo -e "${GREEN}  ✓ Includes added${NC}"
    return 0
}

# Function to replace malloc/calloc/realloc/free
replace_memory_calls() {
    local file="$1"

    # Replace malloc calls
    sed -i 's/malloc(/nimcp_malloc(/g' "$file"

    # Replace calloc calls (careful not to replace nimcp_calloc)
    sed -i 's/\([^_]\)calloc(/\1nimcp_calloc(/g' "$file"

    # Replace realloc calls
    sed -i 's/realloc(/nimcp_realloc(/g' "$file"

    # Replace free calls (careful not to replace nimcp_free)
    sed -i 's/\([^_]\)free(/\1nimcp_free(/g' "$file"

    echo -e "${GREEN}  ✓ Memory calls replaced${NC}"
}

# Function to add basic logging
add_basic_logging() {
    local file="$1"

    # Add LOG_INFO to creation functions
    sed -i 's/\(.*_create.*{\)/\1\n    LOG_INFO("Creating module");/g' "$file"

    # Add LOG_DEBUG to major operations
    # This is a simple pattern - manual refinement recommended

    echo -e "${GREEN}  ✓ Basic logging added (manual review recommended)${NC}"
}

# Process each target module
process_module() {
    local file="$1"
    local module_name="$2"
    local module_id="$3"

    total_files=$((total_files + 1))

    echo ""
    echo "[$total_files] Processing $module_name..."

    if [ ! -f "$file" ]; then
        echo -e "${RED}  ✗ File not found: $file${NC}"
        failed_files=$((failed_files + 1))
        return 1
    fi

    # Add includes
    if ! add_bioasync_includes "$file" "$module_name" "$module_id"; then
        failed_files=$((failed_files + 1))
        return 1
    fi

    # Replace memory calls
    replace_memory_calls "$file"

    # Add logging
    add_basic_logging "$file"

    success_files=$((success_files + 1))
    echo -e "${GREEN}  ✓ Integration complete${NC}"
    return 0
}

# Module definitions: file path, module name, module ID
declare -a MODULES=(
    "${SRC_DIR}/bias/nimcp_bias_detection.c|cognitive.bias_detection|0x0340"
    "${SRC_DIR}/executive/nimcp_executive.c|cognitive.executive|0x0341"
    "${SRC_DIR}/explanations/nimcp_explanations.c|cognitive.explanations|0x0342"
    "${SRC_DIR}/logic/nimcp_symbolic_logic.c|cognitive.logic|0x0343"
    "${SRC_DIR}/reasoning/nimcp_backward_chaining.c|cognitive.reasoning.backward|0x0344"
    "${SRC_DIR}/reasoning/nimcp_forward_chaining.c|cognitive.reasoning.forward|0x0345"
    "${SRC_DIR}/reasoning/nimcp_knowledge_base_interface.c|cognitive.reasoning.kb|0x0346"
    "${SRC_DIR}/reasoning/nimcp_reasoning_factory.c|cognitive.reasoning.factory|0x0347"
    "${SRC_DIR}/reasoning/nimcp_reasoning_integration.c|cognitive.reasoning.integration|0x0348"
    "${SRC_DIR}/reasoning/nimcp_symbolic_logic_attachment.c|cognitive.reasoning.attachment|0x0349"
    "${SRC_DIR}/reasoning/nimcp_symbolic_logic_brain_integration.c|cognitive.reasoning.brain_integration|0x034A"
    "${SRC_DIR}/reasoning/nimcp_unification_engine.c|cognitive.reasoning.unification|0x034B"
    "${SRC_DIR}/reasoning/integration/nimcp_reasoning_attention.c|cognitive.reasoning.attention|0x034C"
    "${SRC_DIR}/reasoning/integration/nimcp_reasoning_curiosity.c|cognitive.reasoning.curiosity|0x034D"
    "${SRC_DIR}/theory_of_mind/nimcp_theory_of_mind.c|cognitive.theory_of_mind|0x034E"
    "${SRC_DIR}/social/nimcp_love_loyalty_friendship.c|cognitive.social|0x034F"
    "${SRC_DIR}/self_awareness/nimcp_self_awareness_extended.c|cognitive.self_awareness|0x0350"
    "${SRC_DIR}/self_model/nimcp_self_model.c|cognitive.self_model|0x0351"
    "${SRC_DIR}/personality/nimcp_personality.c|cognitive.personality|0x0352"
    "${SRC_DIR}/shadow/nimcp_shadow_emotions.c|cognitive.shadow|0x0353"
    "${SRC_DIR}/sleep_wake/nimcp_sleep_wake.c|cognitive.sleep_wake|0x0354"
    "${SRC_DIR}/fault_tolerance/nimcp_emotional_tagging.c|cognitive.fault.emotional_tag|0x0355"
    "${SRC_DIR}/fault_tolerance/nimcp_failure_prediction.c|cognitive.fault.prediction|0x0356"
    "${SRC_DIR}/fault_tolerance/nimcp_fault_attention.c|cognitive.fault.attention|0x0357"
    "${SRC_DIR}/fault_tolerance/nimcp_fault_working_memory.c|cognitive.fault.working_memory|0x0358"
    "${SRC_DIR}/fault_tolerance/nimcp_metacognition.c|cognitive.fault.metacognition|0x0359"
    "${SRC_DIR}/fault_tolerance/nimcp_recovery_consolidation.c|cognitive.fault.recovery_consolidation|0x035A"
    "${SRC_DIR}/fault_tolerance/nimcp_recovery_episodic_memory.c|cognitive.fault.recovery_episodic|0x035B"
    "${SRC_DIR}/fault_tolerance/nimcp_recovery_executive.c|cognitive.fault.recovery_executive|0x035C"
    "${SRC_DIR}/mental_health/disorder_detectors.c|cognitive.mental_health.detectors|0x035D"
    "${SRC_DIR}/mental_health/interventions.c|cognitive.mental_health.interventions|0x035E"
    "${SRC_DIR}/mental_health/nimcp_mental_health.c|cognitive.mental_health.core|0x035F"
)

# Process all modules
echo "Found ${#MODULES[@]} modules to process"
echo ""

for module_spec in "${MODULES[@]}"; do
    IFS='|' read -r file module_name module_id <<< "$module_spec"
    process_module "$file" "$module_name" "$module_id" || true
done

# Print summary
echo ""
echo "=================================================="
echo "Integration Summary"
echo "=================================================="
echo -e "Total files:    ${total_files}"
echo -e "${GREEN}Successful:     ${success_files}${NC}"
echo -e "${RED}Failed:         ${failed_files}${NC}"
echo ""

if [ $failed_files -eq 0 ]; then
    echo -e "${GREEN}✓ All files integrated successfully!${NC}"
    exit 0
else
    echo -e "${YELLOW}⚠ Some files failed - please review manually${NC}"
    exit 1
fi
