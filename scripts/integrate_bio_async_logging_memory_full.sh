#!/bin/bash
#==============================================================================
# integrate_bio_async_logging_memory_full.sh
# Integrate bio-async, logging, and unified memory into all middleware and IO
#==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
TOTAL_FILES=0
PROCESSED_FILES=0
SKIPPED_FILES=0
FAILED_FILES=0

# Module ID mapping (0x0510-0x052F range)
declare -A MODULE_IDS=(
    # Buffering modules (0x0510-0x0514)
    ["nimcp_circular_buffer"]=0x0510
    ["nimcp_integration_buffer"]=0x0511
    ["nimcp_phase_coded_buffer"]=0x0512
    ["nimcp_sliding_window"]=0x0513
    ["nimcp_temporal_accumulator"]=0x0514

    # Cognitive modules (0x0515-0x0516)
    ["nimcp_cognitive_adapters"]=0x0515
    ["nimcp_working_memory_adapter"]=0x0516

    # Encoding modules (0x0517-0x0519)
    ["nimcp_population_coding"]=0x0517
    ["nimcp_rate_coding"]=0x0518
    ["nimcp_temporal_coding"]=0x0519

    # Feature modules (0x051A)
    ["nimcp_feature_extractor"]=0x051A

    # Integration modules (0x051B-0x051F)
    ["nimcp_executive_middleware_adapter"]=0x051B
    ["nimcp_flow_tracker"]=0x051C
    ["nimcp_middleware_controller"]=0x051D
    ["nimcp_quantum_command_propagator"]=0x051E
    ["nimcp_shannon_monitor"]=0x051F

    # Normalization modules (0x0520-0x0523)
    ["nimcp_adaptive_normalizer"]=0x0520
    ["nimcp_homeostatic_normalizer"]=0x0521
    ["nimcp_min_max_normalizer"]=0x0522
    ["nimcp_zscore_normalizer"]=0x0523

    # Pattern modules (0x0524-0x0528)
    ["nimcp_oscillation_detector"]=0x0524
    ["nimcp_pattern_cow"]=0x0525
    ["nimcp_pattern_library"]=0x0526
    ["nimcp_sequence_detector"]=0x0527
    ["nimcp_synchrony_detector"]=0x0528

    # Routing modules (0x0529-0x052C)
    ["nimcp_attention_gate"]=0x0529
    ["nimcp_routing_table"]=0x052A
    ["nimcp_signal_wrapper"]=0x052B
    ["nimcp_thalamic_router"]=0x052C

    # IO modules (0x052D-0x052F)
    ["nimcp_dataio"]=0x052D
    ["nimcp_serialization"]=0x052E
    ["nimcp_stream"]=0x052F
)

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

# Get module ID from filename
get_module_id() {
    local filename="$1"
    local basename=$(basename "$filename" .c)

    # Check if we have a mapping
    if [[ -n "${MODULE_IDS[$basename]}" ]]; then
        printf "0x%04X" "${MODULE_IDS[$basename]}"
    else
        # Default fallback
        echo "0x0510"
    fi
}

# Get module name from filename
get_module_name() {
    local filename="$1"
    local basename=$(basename "$filename" .c)
    echo "$basename"
}

# Check if file already has bio-async integration
check_existing_integration() {
    local file="$1"

    if grep -q "nimcp_bio_async.h" "$file" 2>/dev/null && \
       grep -q "nimcp_unified_memory.h" "$file" 2>/dev/null && \
       grep -q "LOG_MODULE" "$file" 2>/dev/null; then
        return 0  # Already integrated
    fi
    return 1  # Not integrated
}

# Backup file
backup_file() {
    local file="$1"
    cp "$file" "${file}.backup"
}

# Integrate bio-async, logging, and unified memory into a file
integrate_file() {
    local file="$1"

    log_info "Processing: $file"

    # Check if already integrated
    if check_existing_integration "$file"; then
        log_warning "Already integrated, skipping: $file"
        ((SKIPPED_FILES++))
        return 0
    fi

    # Backup original
    backup_file "$file"

    # Get module info
    local module_name=$(get_module_name "$file")
    local module_id=$(get_module_id "$file")

    # Create temporary file
    local temp_file="${file}.tmp"

    # Process file
    python3 - "$file" "$temp_file" "$module_name" "$module_id" <<'PYTHON_SCRIPT'
import sys
import re

def integrate_file(input_file, output_file, module_name, module_id):
    with open(input_file, 'r') as f:
        content = f.read()

    # Check if already has bio-async includes
    if 'nimcp_bio_async.h' in content and 'nimcp_unified_memory.h' in content:
        print(f"Already integrated: {input_file}")
        return False

    # Find the first #include line
    lines = content.split('\n')
    include_section_end = 0
    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            include_section_end = i

    # Find where to insert (after last include before first blank line or code)
    insert_pos = include_section_end + 1
    for i in range(include_section_end + 1, len(lines)):
        if lines[i].strip().startswith('#include'):
            insert_pos = i + 1
        elif lines[i].strip() == '':
            break
        elif not lines[i].strip().startswith('#'):
            break

    # Build new includes section
    new_includes = [
        '#include "async/nimcp_bio_async.h"',
        '#include "async/nimcp_bio_router.h"',
        '#include "async/nimcp_bio_messages.h"',
        '#include "utils/logging/nimcp_logging.h"',
        '#include "utils/memory/nimcp_unified_memory.h"'
    ]

    # Check which includes are missing
    includes_to_add = []
    for inc in new_includes:
        if inc not in content:
            includes_to_add.append(inc)

    if not includes_to_add:
        print(f"All includes already present: {input_file}")
        return False

    # Insert includes
    lines = lines[:insert_pos] + includes_to_add + lines[insert_pos:]

    # Add LOG_MODULE define if not present
    has_log_module = False
    for line in lines:
        if 'LOG_MODULE' in line or 'MODULE_NAME' in line:
            has_log_module = True
            break

    if not has_log_module:
        # Find position after includes
        for i, line in enumerate(lines):
            if line.strip() and not line.strip().startswith('#include') and not line.strip().startswith('//'):
                # Insert LOG_MODULE before first non-include, non-comment line
                lines.insert(i, f'\n#define LOG_MODULE "{module_name}"\n#define LOG_MODULE_ID {module_id}\n')
                break

    # Replace malloc/calloc/free with unified memory equivalents
    content = '\n'.join(lines)

    # Replace memory functions (be careful with function-like macros)
    replacements = [
        (r'\bmalloc\s*\(', 'nimcp_malloc('),
        (r'\bcalloc\s*\(', 'nimcp_calloc('),
        (r'\bfree\s*\(', 'nimcp_free('),
        (r'\brealloc\s*\(', 'nimcp_realloc('),
    ]

    for pattern, replacement in replacements:
        content = re.sub(pattern, replacement, content)

    # Write output
    with open(output_file, 'w') as f:
        f.write(content)

    return True

if __name__ == '__main__':
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    module_name = sys.argv[3]
    module_id = sys.argv[4]

    success = integrate_file(input_file, output_file, module_name, module_id)
    sys.exit(0 if success else 1)
PYTHON_SCRIPT

    # Check if Python script succeeded
    if [ $? -eq 0 ] && [ -f "$temp_file" ]; then
        mv "$temp_file" "$file"
        log_success "Integrated: $file"
        ((PROCESSED_FILES++))
        return 0
    else
        log_error "Failed to integrate: $file"
        rm -f "$temp_file"
        # Restore backup
        mv "${file}.backup" "$file"
        ((FAILED_FILES++))
        return 1
    fi
}

# Main execution
main() {
    log_info "Starting bio-async, logging, and unified memory integration"
    log_info "Project root: $PROJECT_ROOT"

    # Find all middleware C files
    log_info "Processing middleware modules..."
    MIDDLEWARE_DIRS=(
        "src/middleware/buffering"
        "src/middleware/cognitive"
        "src/middleware/encoding"
        "src/middleware/features"
        "src/middleware/integration"
        "src/middleware/normalization"
        "src/middleware/patterns"
        "src/middleware/routing"
    )

    for dir in "${MIDDLEWARE_DIRS[@]}"; do
        full_dir="$PROJECT_ROOT/$dir"
        if [ -d "$full_dir" ]; then
            log_info "Processing directory: $dir"
            for file in "$full_dir"/*.c; do
                if [ -f "$file" ] && [[ ! "$file" =~ CMake ]]; then
                    ((TOTAL_FILES++))
                    integrate_file "$file"
                fi
            done
        fi
    done

    # Find all IO C files
    log_info "Processing IO modules..."
    IO_DIRS=(
        "src/io/dataio"
        "src/io/serialization"
        "src/io/stream"
    )

    for dir in "${IO_DIRS[@]}"; do
        full_dir="$PROJECT_ROOT/$dir"
        if [ -d "$full_dir" ]; then
            log_info "Processing directory: $dir"
            for file in "$full_dir"/*.c; do
                if [ -f "$file" ] && [[ ! "$file" =~ CMake ]]; then
                    ((TOTAL_FILES++))
                    integrate_file "$file"
                fi
            done
        fi
    done

    # Summary
    echo ""
    log_info "========================================"
    log_info "Integration Summary"
    log_info "========================================"
    log_info "Total files found:    $TOTAL_FILES"
    log_success "Successfully processed: $PROCESSED_FILES"
    log_warning "Skipped (already done): $SKIPPED_FILES"
    log_error "Failed:                 $FAILED_FILES"
    log_info "========================================"

    if [ $FAILED_FILES -gt 0 ]; then
        log_error "Some files failed to integrate. Check errors above."
        exit 1
    fi

    log_success "Integration complete!"
}

main "$@"
