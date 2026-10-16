#!/bin/bash
##############################################################################
# integrate_core_modules_bio_async.sh
# Integrates bio-async, comprehensive logging, and unified memory into
# all remaining core modules
##############################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Module IDs for core modules (0x0130-0x013F range)
declare -A MODULE_IDS
MODULE_IDS["dendrite"]="0x0130"
MODULE_IDS["events"]="0x0131"
MODULE_IDS["integration"]="0x0132"
MODULE_IDS["synapse_compute"]="0x0133"
MODULE_IDS["synapse_types"]="0x0134"
MODULE_IDS["logic_attachment"]="0x0135"
MODULE_IDS["logic_brain_integration"]="0x0136"
MODULE_IDS["logic_circuit_builder"]="0x0137"
MODULE_IDS["logic_evaluation"]="0x0138"
MODULE_IDS["logic_factory"]="0x0139"
MODULE_IDS["logic_neuromodulation"]="0x013A"
MODULE_IDS["neuralnet"]="0x013B"
MODULE_IDS["synapse_embeddings"]="0x013C"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

##############################################################################
# Function to add bio-async, logging, and unified memory to a C file
##############################################################################
add_integrations() {
    local file="$1"
    local module_name="$2"
    local module_id="$3"

    if [[ ! -f "$file" ]]; then
        log_error "File not found: $file"
        return 1
    fi

    log_info "Processing: $file (module: $module_name, ID: $module_id)"

    # Create backup
    cp "$file" "${file}.bak"

    # Check if already integrated
    if grep -q "async/nimcp_bio_async.h" "$file" && \
       grep -q "utils/logging/nimcp_logging.h" "$file" && \
       grep -q "utils/memory/nimcp_unified_memory.h" "$file"; then
        log_warning "File already integrated: $file"
        rm "${file}.bak"
        return 0
    fi

    # Use awk to add includes after existing includes section
    awk -v module_id="$module_id" -v module_name="$module_name" '
    BEGIN {
        includes_added = 0
        define_added = 0
        in_includes = 0
    }

    # Detect include section
    /^#include/ {
        in_includes = 1
        print
        next
    }

    # After includes section ends, add our includes
    in_includes && !/^#include/ && !includes_added {
        print ""
        print "// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ==="
        print "#include \"async/nimcp_bio_async.h\""
        print "#include \"async/nimcp_bio_router.h\""
        print "#include \"async/nimcp_bio_messages.h\""
        print "#include \"utils/logging/nimcp_logging.h\""
        print "#include \"utils/memory/nimcp_unified_memory.h\""
        print ""
        print "#define LOG_MODULE \"" module_name "\""
        print "#define BIO_MODULE_ID " module_id
        print ""
        includes_added = 1
        in_includes = 0
    }

    # Print all other lines
    {
        if (!in_includes || /^#include/) {
            print
        }
    }
    ' "${file}.bak" > "$file"

    # Replace malloc/calloc/realloc/free with unified memory
    sed -i 's/\bmalloc(/nimcp_malloc(/g' "$file"
    sed -i 's/\bcalloc(/nimcp_calloc(/g' "$file"
    sed -i 's/\brealloc(/nimcp_realloc(/g' "$file"
    sed -i 's/\bfree(/nimcp_free(/g' "$file"

    # Add logging to key functions (simple pattern matching)
    # This is a basic implementation - full logging would require more sophisticated parsing

    log_success "Integrated: $file"
    rm "${file}.bak"
    return 0
}

##############################################################################
# Process all files
##############################################################################

log_info "Starting bio-async + logging + unified memory integration for core modules"
log_info "=========================================================================="

# Dendrite
add_integrations \
    "$PROJECT_ROOT/src/core/dendrite/nimcp_dendrite.c" \
    "dendrite" \
    "${MODULE_IDS[dendrite]}"

# Events
add_integrations \
    "$PROJECT_ROOT/src/core/events/nimcp_event_bus.c" \
    "event_bus" \
    "${MODULE_IDS[events]}"

# Integration
add_integrations \
    "$PROJECT_ROOT/src/core/integration/nimcp_multimodal_integration.c" \
    "multimodal_integration" \
    "${MODULE_IDS[integration]}"

# Synapse compute
add_integrations \
    "$PROJECT_ROOT/src/core/synapse_compute/nimcp_synapse_compute.c" \
    "synapse_compute" \
    "${MODULE_IDS[synapse_compute]}"

# Synapse types
add_integrations \
    "$PROJECT_ROOT/src/core/synapse_types/nimcp_synapse_types.c" \
    "synapse_types" \
    "${MODULE_IDS[synapse_types]}"

# Logic modules
add_integrations \
    "$PROJECT_ROOT/src/core/logic/nimcp_neural_logic_attachment.c" \
    "neural_logic_attachment" \
    "${MODULE_IDS[logic_attachment]}"

add_integrations \
    "$PROJECT_ROOT/src/core/logic/nimcp_neural_logic_brain_integration.c" \
    "neural_logic_brain_integration" \
    "${MODULE_IDS[logic_brain_integration]}"

add_integrations \
    "$PROJECT_ROOT/src/core/logic/nimcp_neural_logic_circuit_builder.c" \
    "neural_logic_circuit_builder" \
    "${MODULE_IDS[logic_circuit_builder]}"

add_integrations \
    "$PROJECT_ROOT/src/core/logic/nimcp_neural_logic_evaluation.c" \
    "neural_logic_evaluation" \
    "${MODULE_IDS[logic_evaluation]}"

add_integrations \
    "$PROJECT_ROOT/src/core/logic/nimcp_neural_logic_factory.c" \
    "neural_logic_factory" \
    "${MODULE_IDS[logic_factory]}"

add_integrations \
    "$PROJECT_ROOT/src/core/logic/nimcp_neural_logic_neuromodulation.c" \
    "neural_logic_neuromodulation" \
    "${MODULE_IDS[logic_neuromodulation]}"

# Neuralnet modules
add_integrations \
    "$PROJECT_ROOT/src/core/neuralnet/nimcp_neuralnet.c" \
    "neuralnet" \
    "${MODULE_IDS[neuralnet]}"

add_integrations \
    "$PROJECT_ROOT/src/core/neuralnet/nimcp_synapse_embeddings.c" \
    "synapse_embeddings" \
    "${MODULE_IDS[synapse_embeddings]}"

log_info "=========================================================================="
log_success "All core modules integrated successfully!"
log_info ""
log_info "Summary:"
log_info "  - Added bio-async includes (bio_async.h, bio_router.h, bio_messages.h)"
log_info "  - Added logging includes (nimcp_logging.h) with LOG_MODULE defines"
log_info "  - Added unified memory (nimcp_unified_memory.h)"
log_info "  - Replaced malloc/calloc/realloc/free with nimcp_* equivalents"
log_info "  - Assigned module IDs (0x0130-0x013C)"
log_info ""
log_info "Next steps:"
log_info "  1. Review changes in each file"
log_info "  2. Add comprehensive logging to key functions"
log_info "  3. Add bio-async message handlers where appropriate"
log_info "  4. Test compilation"
log_info "  5. Run tests to verify functionality"

exit 0
