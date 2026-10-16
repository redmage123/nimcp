#!/bin/bash
#=============================================================================
# fix_malloc_violations.sh - Automated NIMCP Memory Policy Enforcement
#=============================================================================

set -e

echo "==============================================================================="
echo "NIMCP Memory Policy Enforcement - Automated Fix"
echo "==============================================================================="

# Files to fix (exclude bindings, memory system itself, and test files)
FILES=(
    "src/cognitive/bias/nimcp_bias_detection.c"
    "src/cognitive/curiosity/nimcp_curiosity_fractal.c"
    "src/cognitive/curiosity/nimcp_curiosity_hyperbolic.c"
    "src/cognitive/emotions/nimcp_emotional_system.c"
    "src/cognitive/epistemic/nimcp_epistemic_filter.c"
    "src/cognitive/ethics/nimcp_ethics_hyperbolic.c"
    "src/cognitive/fault_tolerance/nimcp_fault_attention.c"
    "src/cognitive/global_workspace/nimcp_global_workspace.c"
    "src/cognitive/grief/nimcp_grief_and_loss.c"
    "src/cognitive/joy/nimcp_joy_euphoria.c"
    "src/cognitive/memory/nimcp_engram.c"
    "src/cognitive/memory/nimcp_semantic_memory.c"
    "src/cognitive/memory/nimcp_wm_transfer.c"
    "src/cognitive/mental_health_monitor.c"
    "src/cognitive/reasoning/integration/nimcp_reasoning_attention.c"
    "src/cognitive/reasoning/integration/nimcp_reasoning_curiosity.c"
    "src/cognitive/remorse/nimcp_remorse_regret.c"
    "src/cognitive/shadow/nimcp_shadow_emotions.c"
    "src/cognitive/working_memory/nimcp_working_memory.c"
    "src/core/brain/learning/nimcp_association_learning.c"
    "src/core/brain/learning/nimcp_circuit_compilation.c"
    "src/core/brain/learning/nimcp_rule_learning.c"
    "src/core/brain/nimcp_brain.c"
    "src/core/brain/nimcp_pretrained.c"
    "src/core/events/nimcp_event_bus.c"
    "src/core/synapse_compute/nimcp_synapse_compute.c"
    "src/core/topology/nimcp_community_detection.c"
    "src/core/topology/nimcp_fractal_topology.c"
    "src/core/topology/nimcp_network_builder.c"
    "src/gpu/nimcp_multigpu.c"
    "src/information/nimcp_cross_modal.c"
    "src/information/nimcp_shannon.c"
    "src/middleware/events/nimcp_event_queue.c"
    "src/middleware/integration/nimcp_quantum_command_propagator.c"
    "src/nlp/nimcp_nlp.c"
    "src/plasticity/adaptive/nimcp_adaptive.c"
    "src/plasticity/neuromodulators/nimcp_neuromod_pink_noise.c"
    "src/plasticity/noise/nimcp_pink_noise.c"
    "src/utils/config/nimcp_dynamic_config.c"
    "src/utils/error/nimcp_error_codes.c"
    "src/utils/fault_tolerance/nimcp_brain_recovery_integration.c"
    "src/utils/fault_tolerance/nimcp_diagnostics.c"
    "src/utils/fault_tolerance/nimcp_fault_event_bus.c"
    "src/utils/fault_tolerance/nimcp_fault_state_machine.c"
    "src/utils/fault_tolerance/nimcp_health_monitor.c"
    "src/utils/fault_tolerance/nimcp_lockfree_metrics.c"
    "src/utils/fault_tolerance/nimcp_metrics_aggregator.c"
    "src/utils/fault_tolerance/nimcp_recovery.c"
    "src/utils/fault_tolerance/nimcp_recovery_cache.c"
    "src/utils/fault_tolerance/nimcp_runtime_adaptation.c"
    "src/utils/numerical/nimcp_integration.c"
    "src/utils/platform/nimcp_platform_thread.c"
    "src/utils/queue_manager/nimcp_queue_manager.c"
)

FIXED_COUNT=0
TOTAL_FILES=${#FILES[@]}

for FILE in "${FILES[@]}"; do
    if [ ! -f "$FILE" ]; then
        echo "  [SKIP] $FILE (not found)"
        continue
    fi

    echo "  [FIX] $FILE"

    # Check if file already includes nimcp_memory.h
    if ! grep -q 'nimcp_memory.h' "$FILE"; then
        # Add include after the first include line
        sed -i '0,/^#include/{s|^\(#include.*\)$|\1\n#include "utils/memory/nimcp_memory.h"|}' "$FILE"
    fi

    # Replace malloc/calloc/realloc/free with nimcp_ versions
    sed -i 's/\bmalloc(/nimcp_malloc(/g' "$FILE"
    sed -i 's/\bcalloc(/nimcp_calloc(/g' "$FILE"
    sed -i 's/\brealloc(/nimcp_realloc(/g' "$FILE"
    sed -i 's/\bfree(/nimcp_free(/g' "$FILE"

    ((FIXED_COUNT++))
done

echo ""
echo "==============================================================================="
echo "Summary: Fixed $FIXED_COUNT / $TOTAL_FILES files"
echo "==============================================================================="
