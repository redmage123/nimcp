#!/bin/bash
# Script to integrate bio-async, logging, and unified memory into emotion modules

set -e

echo "=== Integrating Bio-Async, Logging, and Unified Memory into Emotion Modules ==="

# Module list with IDs
declare -A MODULES=(
    ["src/cognitive/grief/nimcp_grief_and_loss.c"]="GRIEF:0x0323"
    ["src/cognitive/joy/nimcp_joy_euphoria.c"]="JOY:0x0324"
    ["src/cognitive/remorse/nimcp_remorse_regret.c"]="REMORSE:0x0325"
    ["src/cognitive/empathetic_response/nimcp_empathetic_response.c"]="EMPATHY:0x0322"
    ["src/cognitive/emotional_tagging/nimcp_emotional_tagging.c"]="EMOTIONAL_TAGGING:0x0326"
    ["src/cognitive/emotion_recognition/nimcp_emotion_recognition_simple.c"]="EMOTION_RECOGNITION:0x0321"
)

for file in "${!MODULES[@]}"; do
    IFS=':' read -r module_name module_id <<< "${MODULES[$file]}"

    echo "Processing $file (MODULE=$module_name, ID=$module_id)..."

    # Check if file exists
    if [ ! -f "$file" ]; then
        echo "  WARNING: File not found: $file"
        continue
    fi

    # Add defines if not present
    if ! grep -q "define LOG_MODULE" "$file"; then
        echo "  Adding LOG_MODULE and BIO_MODULE defines..."
        # Insert after first #include block
        sed -i '/#include.*\.h"$/a \\n#define LOG_MODULE "'"$module_name"'"\n#define BIO_MODULE_'"$module_name"' '"$module_id" "$file"
    fi

    # Replace memory functions
    echo "  Replacing memory functions..."
    sed -i 's/\bmalloc(/nimcp_malloc(/g' "$file"
    sed -i 's/\bcalloc(/nimcp_calloc(/g' "$file"
    sed -i 's/\brealloc(/nimcp_realloc(/g' "$file"
    sed -i 's/\bfree(/nimcp_free(/g' "$file"
    sed -i 's/\bstrdup(/nimcp_strdup(/g' "$file"

    # Update includes
    echo "  Updating includes..."
    if ! grep -q "nimcp_unified_memory.h" "$file"; then
        sed -i 's|#include "utils/memory/nimcp_memory.h"|#include "utils/memory/nimcp_unified_memory.h"\n#include "utils/logging/nimcp_logging.h"\n#include "async/nimcp_bio_async.h"\n#include "async/nimcp_bio_router.h"\n#include "async/nimcp_bio_messages.h"|' "$file"
    fi

    echo "  ✓ Done with $file"
done

echo ""
echo "=== Summary ==="
echo "Modified ${#MODULES[@]} files"
echo "To complete integration, manually add:"
echo "  1. bio_module_context_t bio_ctx to structs"
echo "  2. bio_router_register_module() calls in create functions"
echo "  3. bio_router_unregister_module() calls in destroy functions"
echo "  4. LOG_*() calls throughout major functions"
echo ""
echo "See EMOTION_MODULES_BIO_ASYNC_INTEGRATION.md for complete pattern."
