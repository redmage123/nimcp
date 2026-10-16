#!/bin/bash
#
# integrate_bio_async_all.sh - Comprehensive integration of bio-async, logging, and unified memory
#
# This script integrates bio-async messaging, comprehensive logging, and unified memory
# into all remaining NIMCP modules.
#
# Usage: ./integrate_bio_async_all.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Color output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to add includes if not present
add_includes() {
    local file="$1"
    local module_name="$2"

    # Check if bio-async includes already present
    if grep -q "async/nimcp_bio_async.h" "$file"; then
        log_info "$file already has bio-async includes"
        return 0
    fi

    log_info "Adding includes to $file"

    # Create temporary file with new includes
    local temp_file="${file}.tmp"

    # Find first #include line and insert our includes before it
    awk -v module="$module_name" '
    BEGIN { inserted = 0 }
    /^#include/ && !inserted {
        print "#include \"async/nimcp_bio_async.h\""
        print "#include \"async/nimcp_bio_router.h\""
        print "#include \"async/nimcp_bio_messages.h\""
        print "#include \"utils/logging/nimcp_logging.h\""
        print "#include \"utils/memory/nimcp_unified_memory.h\""
        print ""
        print "#define LOG_MODULE \"" toupper(module) "\""
        print ""
        inserted = 1
    }
    { print }
    ' "$file" > "$temp_file"

    mv "$temp_file" "$file"
}

# Function to replace malloc/calloc/free with unified memory
replace_memory_calls() {
    local file="$1"

    log_info "Replacing memory calls in $file"

    # Replace malloc
    sed -i 's/\bmalloc(/nimcp_malloc(/g' "$file"

    # Replace calloc
    sed -i 's/\bcalloc(/nimcp_calloc(/g' "$file"

    # Replace free (but not nimcp_free)
    sed -i 's/\bfree(/nimcp_free(/g' "$file"
    sed -i 's/nimcp_nimcp_free(/nimcp_free(/g' "$file"
}

# Function to add logging statements
add_logging() {
    local file="$1"
    local module_name="$2"

    log_info "Adding logging to $file (manual review recommended)"

    # This is a placeholder - actual logging requires understanding
    # the function context, so we'll just flag it for manual review
    # In production, you'd want to add LOG_DEBUG/INFO/WARN/ERROR at key points
}

# Function to process a single file
process_file() {
    local file="$1"
    local module_name="$2"

    log_info "Processing $file (module: $module_name)"

    # Backup original file
    cp "$file" "${file}.bak"

    # Add includes
    add_includes "$file" "$module_name"

    # Replace memory calls
    replace_memory_calls "$file"

    # Add logging (placeholder)
    add_logging "$file" "$module_name"

    log_info "Completed $file"
}

# Main processing
main() {
    log_info "Starting comprehensive bio-async integration"
    log_info "Project root: $PROJECT_ROOT"

    # Process glial files (excluding already-integrated files)
    log_info "=== Processing GLIAL modules ==="
    for file in \
        "$PROJECT_ROOT/src/glial/astrocytes/nimcp_astrocyte_calcium.c" \
        "$PROJECT_ROOT/src/glial/myelin_sheath/nimcp_myelin_math.c" \
        "$PROJECT_ROOT/src/glial/astrocytes/nimcp_astrocytes_refactored.c"
    do
        if [ -f "$file" ]; then
            module_name=$(basename "$(dirname "$file")")
            process_file "$file" "$module_name"
        else
            log_warn "File not found: $file"
        fi
    done

    # Process security files
    log_info "=== Processing SECURITY modules ==="
    for file in "$PROJECT_ROOT"/src/security/*.c; do
        if [ -f "$file" ]; then
            process_file "$file" "security"
        fi
    done

    # Process lib files
    log_info "=== Processing LIB modules ==="
    for file in \
        "$PROJECT_ROOT/src/lib/nimcp_distributed_cognition_impl.c" \
        "$PROJECT_ROOT/src/lib/cognitive/nimcp_hierarchical.c" \
        "$PROJECT_ROOT/src/lib/perception/nimcp_retina.c"
    do
        if [ -f "$file" ]; then
            module_name=$(basename "$(dirname "$file")" | sed 's/nimcp_//')
            process_file "$file" "$module_name"
        fi
    done

    # Process information files
    log_info "=== Processing INFORMATION modules ==="
    for file in "$PROJECT_ROOT"/src/information/*.c; do
        if [ -f "$file" ]; then
            process_file "$file" "information"
        fi
    done

    # Process optimization files
    log_info "=== Processing OPTIMIZATION modules ==="
    for file in "$PROJECT_ROOT"/src/optimization/**/*.c; do
        if [ -f "$file" ]; then
            process_file "$file" "optimization"
        fi
    done

    # Process API files
    log_info "=== Processing API module ==="
    if [ -f "$PROJECT_ROOT/src/api/nimcp.c" ]; then
        process_file "$PROJECT_ROOT/src/api/nimcp.c" "api"
    fi

    # Process networking files (excluding already-integrated)
    log_info "=== Processing NETWORKING modules ==="
    for file in \
        "$PROJECT_ROOT/src/networking/protocol/nimcp_protocol.c" \
        "$PROJECT_ROOT/src/networking/events/nimcp_events.c" \
        "$PROJECT_ROOT/src/networking/distributed/nimcp_distributed_cognition_refactored.c"
    do
        if [ -f "$file" ]; then
            if ! grep -q "bio_router" "$file"; then
                process_file "$file" "networking"
            else
                log_info "Skipping $file (already has bio-async)"
            fi
        fi
    done

    log_info "=== Integration complete ==="
    log_info "Backup files created with .bak extension"
    log_warn "Manual review recommended for:"
    log_warn "  1. Adding context-specific LOG_DEBUG/INFO/WARN/ERROR statements"
    log_warn "  2. Adding bio-async message handlers where appropriate"
    log_warn "  3. Verifying unified memory replacements don't break code"
}

main "$@"
