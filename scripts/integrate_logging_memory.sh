#!/bin/bash

# Script to integrate logging and unified memory into utility modules
# Usage: ./integrate_logging_memory.sh <source_file> <module_name>

SOURCE_FILE="$1"
MODULE_NAME="$2"

if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: File $SOURCE_FILE not found"
    exit 1
fi

# Create backup
cp "$SOURCE_FILE" "${SOURCE_FILE}.backup"

# Check if already integrated
if grep -q "nimcp_unified_memory.h" "$SOURCE_FILE" && grep -q "nimcp_logging.h" "$SOURCE_FILE"; then
    echo "Already integrated: $SOURCE_FILE"
    exit 0
fi

# Create temporary file for processing
TEMP_FILE=$(mktemp)

# Process the file
awk -v module="$MODULE_NAME" '
BEGIN {
    has_logging = 0
    has_unified = 0
    has_log_module = 0
    in_includes = 0
    includes_done = 0
}

# Detect if already has the includes
/nimcp_logging\.h/ { has_logging = 1 }
/nimcp_unified_memory\.h/ { has_unified = 1 }
/#define LOG_MODULE/ { has_log_module = 1 }

# Track include section
/^#include/ {
    in_includes = 1
    print
    next
}

# When we exit includes section, add our includes if needed
in_includes && !/^#include/ && !includes_done {
    if (!has_logging) {
        print "#include \"utils/logging/nimcp_logging.h\""
    }
    if (!has_unified) {
        print "#include \"utils/memory/nimcp_unified_memory.h\""
    }
    print ""
    if (!has_log_module) {
        print "#define LOG_MODULE \"" module "\""
        print ""
    }
    includes_done = 1
    in_includes = 0
}

# Replace memory allocation functions
{
    line = $0

    # Replace malloc
    gsub(/([^_a-zA-Z])malloc\(/, "\\1nimcp_malloc(", line)

    # Replace calloc
    gsub(/([^_a-zA-Z])calloc\(/, "\\1nimcp_calloc(", line)

    # Replace realloc
    gsub(/([^_a-zA-Z])realloc\(/, "\\1nimcp_realloc(", line)

    # Replace free
    gsub(/([^_a-zA-Z])free\(/, "\\1nimcp_free(", line)

    print line
}
' "$SOURCE_FILE" > "$TEMP_FILE"

# Replace original file
mv "$TEMP_FILE" "$SOURCE_FILE"

echo "Processed: $SOURCE_FILE"
