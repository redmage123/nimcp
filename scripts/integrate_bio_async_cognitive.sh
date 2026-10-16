#!/bin/bash
#==============================================================================
# integrate_bio_async_cognitive.sh
# Automates bio-async and logging integration into cognitive modules
#==============================================================================

set -e

# Module configuration: module_name:module_id_suffix:header_dir:source_file
declare -a MODULES=(
    "knowledge:KNOWLEDGE:knowledge:src/cognitive/knowledge/nimcp_knowledge.c"
    "wellbeing:WELLBEING:wellbeing:src/cognitive/wellbeing/nimcp_wellbeing.c"
    "global_workspace:GLOBAL_WORKSPACE:global_workspace:src/cognitive/global_workspace/nimcp_global_workspace.c"
    "mirror_neurons:MIRROR_NEURONS:mirror_neurons:src/cognitive/mirror_neurons/nimcp_mirror_neurons.c"
    "consolidation:CONSOLIDATION:consolidation:src/cognitive/consolidation/nimcp_consolidation.c"
    "epistemic:EPISTEMIC:epistemic:src/cognitive/epistemic/nimcp_epistemic_filter.c"
    "network_analysis:NETWORK_ANALYSIS:analysis:src/cognitive/analysis/nimcp_network_analysis.c"
)

echo "Starting bio-async integration for cognitive modules..."
echo "================================================================"

for module_spec in "${MODULES[@]}"; do
    IFS=':' read -r module_name module_id header_dir source_file <<< "$module_spec"

    echo ""
    echo "Processing: $module_name"
    echo "----------------------------------------"

    # Check if source file exists
    if [ ! -f "$source_file" ]; then
        echo "  ✗ Source file not found: $source_file"
        continue
    fi

    # Check if already has bio-async
    if grep -q "bio_module_context_t" "$source_file" 2>/dev/null; then
        echo "  ✓ Already has bio-async integration"
        continue
    fi

    # Find the header file
    header_file=$(find include/cognitive -name "nimcp_${module_name}.h" -o -name "nimcp_${header_dir}.h" -o -name "nimcp_network_analysis.h" 2>/dev/null | head -1)

    if [ -z "$header_file" ]; then
        echo "  ✗ Header file not found for $module_name"
        continue
    fi

    echo "  Header: $header_file"
    echo "  Source: $source_file"
    echo "  Module ID: BIO_MODULE_$module_id"

    # Check current status
    has_log_module=$(grep -q "#define LOG_MODULE" "$source_file" && echo "yes" || echo "no")
    has_bio_includes=$(grep -q "bio_async.h" "$source_file" && echo "yes" || echo "no")

    echo "  Status: LOG_MODULE=$has_log_module, bio_includes=$has_bio_includes"

done

echo ""
echo "================================================================"
echo "Integration scan complete"
echo ""
echo "Manual integration required for modules without bio-async."
echo "Use the following pattern:"
echo ""
echo "1. Add to header includes:"
echo '   #include "async/nimcp_bio_async.h"'
echo '   #include "async/nimcp_bio_router.h"'
echo '   #include "async/nimcp_bio_messages.h"'
echo ""
echo "2. Add to config struct: bool enable_bio_async;"
echo ""
echo "3. Add to main struct:"
echo "   bio_module_context_t bio_ctx;"
echo "   bool bio_async_enabled;"
echo ""
echo "4. Add to source file after includes:"
echo '   #include "utils/logging/nimcp_logging.h"'
echo '   #define LOG_MODULE "module_name"'
echo ""
echo "5. Add to create/init function:"
echo "   ctx->bio_ctx = NULL;"
echo "   ctx->bio_async_enabled = false;"
echo "   if (bio_router_is_initialized()) {"
echo "       bio_module_info_t bio_info = {"
echo "           .module_id = BIO_MODULE_XXX,"
echo "           .module_name = \"module_name\","
echo "           .inbox_capacity = 64,"
echo "           .user_data = ctx"
echo "       };"
echo "       ctx->bio_ctx = bio_router_register_module(&bio_info);"
echo "       if (ctx->bio_ctx) {"
echo "           ctx->bio_async_enabled = true;"
echo '           LOG_INFO(LOG_MODULE, "Bio-async registered");'
echo "       }"
echo "   }"
echo ""
echo "6. Add to destroy function:"
echo "   if (ctx->bio_async_enabled && ctx->bio_ctx) {"
echo "       bio_router_unregister_module(ctx->bio_ctx);"
echo "       ctx->bio_ctx = NULL;"
echo "       ctx->bio_async_enabled = false;"
echo "   }"
