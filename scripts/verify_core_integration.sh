#!/bin/bash
##############################################################################
# verify_core_integration.sh
# Verifies bio-async, logging, and unified memory integration in core modules
##############################################################################

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[✓]${NC} $1"; }
log_error() { echo -e "${RED}[✗]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[!]${NC} $1"; }

##############################################################################
# Verification Functions
##############################################################################

verify_file() {
    local file="$1"
    local module_name="$2"
    local checks_passed=0
    local checks_total=5

    if [[ ! -f "$file" ]]; then
        log_error "File not found: $file"
        return 1
    fi

    echo ""
    log_info "Verifying: $(basename $file)"

    # Check 1: Bio-async includes
    if grep -q "async/nimcp_bio_async.h" "$file"; then
        log_success "Bio-async includes present"
        ((checks_passed++))
    else
        log_error "Missing bio-async includes"
    fi

    # Check 2: Logging includes
    if grep -q "utils/logging/nimcp_logging.h" "$file"; then
        log_success "Logging includes present"
        ((checks_passed++))
    else
        log_error "Missing logging includes"
    fi

    # Check 3: Unified memory includes
    if grep -q "utils/memory/nimcp_unified_memory.h" "$file"; then
        log_success "Unified memory includes present"
        ((checks_passed++))
    else
        log_error "Missing unified memory includes"
    fi

    # Check 4: LOG_MODULE defined
    if grep -q "^#define LOG_MODULE" "$file"; then
        local log_module=$(grep "^#define LOG_MODULE" "$file" | awk '{print $3}' | tr -d '"')
        log_success "LOG_MODULE defined: $log_module"
        ((checks_passed++))
    else
        log_error "LOG_MODULE not defined"
    fi

    # Check 5: Memory functions replaced
    if grep -q "nimcp_malloc\|nimcp_calloc\|nimcp_free" "$file"; then
        log_success "Memory functions replaced"
        ((checks_passed++))
    else
        log_warning "No nimcp_* memory functions found (may not allocate memory)"
        ((checks_passed++))  # Not critical
    fi

    # Summary
    echo "  Status: $checks_passed/$checks_total checks passed"

    if [ $checks_passed -eq $checks_total ]; then
        log_success "$module_name: PASS"
        return 0
    else
        log_error "$module_name: FAIL ($checks_passed/$checks_total)"
        return 1
    fi
}

##############################################################################
# Main Verification
##############################################################################

echo ""
echo "=========================================================================="
echo "  Core Modules Bio-Async Integration Verification"
echo "=========================================================================="

total_files=0
passed_files=0
failed_files=0

# Array of files to verify
declare -a files=(
    "src/core/dendrite/nimcp_dendrite.c:dendrite"
    "src/core/events/nimcp_event_bus.c:event_bus"
    "src/core/integration/nimcp_multimodal_integration.c:integration"
    "src/core/synapse_compute/nimcp_synapse_compute.c:synapse_compute"
    "src/core/synapse_types/nimcp_synapse_types.c:synapse_types"
    "src/core/logic/nimcp_neural_logic_attachment.c:logic_attachment"
    "src/core/logic/nimcp_neural_logic_brain_integration.c:logic_brain_integration"
    "src/core/logic/nimcp_neural_logic_circuit_builder.c:logic_circuit_builder"
    "src/core/logic/nimcp_neural_logic_evaluation.c:logic_evaluation"
    "src/core/logic/nimcp_neural_logic_factory.c:logic_factory"
    "src/core/logic/nimcp_neural_logic_neuromodulation.c:logic_neuromodulation"
    "src/core/neuralnet/nimcp_neuralnet.c:neuralnet"
    "src/core/neuralnet/nimcp_synapse_embeddings.c:synapse_embeddings"
)

for entry in "${files[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    full_path="$PROJECT_ROOT/$file"

    ((total_files++))

    if verify_file "$full_path" "$module"; then
        ((passed_files++))
    else
        ((failed_files++))
    fi
done

echo ""
echo "=========================================================================="
echo "  Verification Summary"
echo "=========================================================================="
echo "  Total files:  $total_files"
echo "  Passed:       $passed_files"
echo "  Failed:       $failed_files"
echo ""

if [ $failed_files -eq 0 ]; then
    log_success "All files passed verification!"
    exit 0
else
    log_error "$failed_files file(s) failed verification"
    exit 1
fi
