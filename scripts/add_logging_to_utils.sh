#!/bin/bash
# Script to add comprehensive logging to all utility modules

set -e

UTILS_DIR="/home/bbrelin/nimcp/src/utils"
BACKUP_DIR="/tmp/nimcp_utils_backup_$(date +%s)"

# Create backup
mkdir -p "$BACKUP_DIR"
echo "Creating backup in $BACKUP_DIR..."

# Function to add logging to a file
add_logging() {
    local file="$1"
    local module="$2"

    # Skip if file already has LOG_MODULE
    if grep -q "#define LOG_MODULE" "$file"; then
        echo "  Skipping $file - already has logging"
        return
    fi

    # Backup original
    cp "$file" "$BACKUP_DIR/$(basename $file)"

    # Create temp file
    local tmpfile=$(mktemp)

    # Find the line after the last #include
    local include_line=$(grep -n "^#include" "$file" | tail -1 | cut -d: -f1)

    if [ -z "$include_line" ]; then
        echo "  Warning: No includes found in $file"
        return
    fi

    # Add logging header and module definition after includes
    {
        head -n "$include_line" "$file"
        echo ""
        echo "#include \"utils/logging/nimcp_logging.h\""
        echo "#define LOG_MODULE \"$module\""
        echo ""
        tail -n "+$((include_line + 1))" "$file"
    } > "$tmpfile"

    mv "$tmpfile" "$file"
    echo "  Added logging to $file"
}

# Process all .c files in utils subdirectories
process_directory() {
    local dir="$1"
    local module_prefix="$2"

    if [ ! -d "$dir" ]; then
        return
    fi

    echo "Processing $dir..."

    for file in "$dir"/*.c; do
        if [ -f "$file" ]; then
            local basename=$(basename "$file" .c)
            local module="${module_prefix}.${basename}"
            add_logging "$file" "$module"
        fi
    done
}

# Process each utils subdirectory
echo "=== Adding logging to utility modules ==="

process_directory "$UTILS_DIR/algorithms" "utils.algorithms"
process_directory "$UTILS_DIR/cache" "utils.cache"
process_directory "$UTILS_DIR/containers" "utils.containers"
process_directory "$UTILS_DIR/error" "utils.error"
process_directory "$UTILS_DIR/geometry" "utils.geometry"
process_directory "$UTILS_DIR/json" "utils.json"
process_directory "$UTILS_DIR/math" "utils.math"
process_directory "$UTILS_DIR/metrics" "utils.metrics"
process_directory "$UTILS_DIR/numerical" "utils.numerical"
process_directory "$UTILS_DIR/platform" "utils.platform"
process_directory "$UTILS_DIR/quantum" "utils.quantum"
process_directory "$UTILS_DIR/queue_manager" "utils.queue_manager"
process_directory "$UTILS_DIR/serialization" "utils.serialization"
process_directory "$UTILS_DIR/signal" "utils.signal"
process_directory "$UTILS_DIR/spatial" "utils.spatial"
process_directory "$UTILS_DIR/spectral" "utils.spectral"
process_directory "$UTILS_DIR/tensor_networks" "utils.tensor_networks"
process_directory "$UTILS_DIR/thread" "utils.thread"
process_directory "$UTILS_DIR/time" "utils.time"
process_directory "$UTILS_DIR/validation" "utils.validation"

echo ""
echo "=== Summary ==="
echo "Backup created in: $BACKUP_DIR"
echo "Done!"
