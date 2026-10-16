#!/bin/bash
# Script to add health agent integration boilerplate to .c files
# Usage: ./add_health_integration.sh <directory>
#
# Adds:
# 1. Health agent forward declarations
# 2. Global health agent pointer
# 3. Setter function
# 4. Heartbeat helper function

DIR="${1:-.}"
COUNT=0
SKIPPED=0

process_file() {
    local file="$1"
    local basename=$(basename "$file" .c)

    # Skip files that already have health integration
    if grep -q 'health_agent_heartbeat\|g_.*_health_agent\|_set_health_agent' "$file" 2>/dev/null; then
        SKIPPED=$((SKIPPED + 1))
        return
    fi

    # Skip test files
    if echo "$file" | grep -q '/test/'; then
        return
    fi

    # Derive module prefix from filename
    # Remove nimcp_ prefix and trailing .c
    local prefix=$(echo "$basename" | sed 's/^nimcp_//')

    # Create shorter prefix for variable names (max ~20 chars)
    local short_prefix="$prefix"

    # Create the health integration block
    local BLOCK="
//=============================================================================
// Health Agent Integration (Phase 8: System-Wide Health Integration)
//=============================================================================
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
extern void nimcp_health_agent_heartbeat_ex(nimcp_health_agent_t* agent,
                                             const char* operation,
                                             float progress);

/** Global health agent for ${prefix} module */
static nimcp_health_agent_t* g_${short_prefix}_health_agent = NULL;

/**
 * @brief Set health agent for ${prefix} heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
void ${short_prefix}_set_health_agent(nimcp_health_agent_t* agent) {
    g_${short_prefix}_health_agent = agent;
}

/** @brief Send heartbeat from ${prefix} module */
static inline void ${short_prefix}_heartbeat(const char* operation, float progress) {
    if (g_${short_prefix}_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_${short_prefix}_health_agent, operation, progress);
    }
}
"

    # Find insertion point: after last #include or #define LOG_MODULE line
    # Try to find LOG_MODULE first
    local log_module_line=$(grep -n '#define LOG_MODULE' "$file" | tail -1 | cut -d: -f1)

    if [ -n "$log_module_line" ]; then
        # Insert after LOG_MODULE line
        local insert_line=$log_module_line
    else
        # Find last #include line
        local last_include=$(grep -n '^#include' "$file" | tail -1 | cut -d: -f1)
        if [ -n "$last_include" ]; then
            local insert_line=$last_include
        else
            # Find last #define line in first 50 lines
            local last_define=$(head -50 "$file" | grep -n '^#define' | tail -1 | cut -d: -f1)
            if [ -n "$last_define" ]; then
                local insert_line=$last_define
            else
                echo "  WARN: No suitable insertion point found in $file"
                return
            fi
        fi
    fi

    # Insert the block after the insertion line
    # Use a temp file to avoid issues
    local tmpfile=$(mktemp)
    head -n "$insert_line" "$file" > "$tmpfile"
    echo "$BLOCK" >> "$tmpfile"
    tail -n +$((insert_line + 1)) "$file" >> "$tmpfile"
    mv "$tmpfile" "$file"

    COUNT=$((COUNT + 1))
    echo "  Added health integration to: $file (after line $insert_line)"
}

echo "Adding health agent integration to .c files in: $DIR"
echo "=============================================="

# Find all .c files and process them
while IFS= read -r -d '' file; do
    process_file "$file"
done < <(find "$DIR" -name '*.c' -type f -print0 | sort -z)

echo ""
echo "=============================================="
echo "Processed: $COUNT files"
echo "Skipped (already integrated): $SKIPPED files"
