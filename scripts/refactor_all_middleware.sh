#!/bin/bash

#=============================================================================
# refactor_all_middleware.sh - Batch Refactor All Middleware Modules
#=============================================================================
#
# USAGE: ./refactor_all_middleware.sh
#
# WHAT: Applies refactoring to all middleware modules in batch
# WHY:  Automate refactoring of 46 middleware modules
# HOW:  Calls refactor_middleware_module.sh for each module
#
#=============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NIMCP_ROOT="$(dirname "$SCRIPT_DIR")"
REFACTOR_SCRIPT="$SCRIPT_DIR/refactor_middleware_module.sh"

# Change to NIMCP root
cd "$NIMCP_ROOT"

echo "======================================================================"
echo "NIMCP Middleware Batch Refactoring"
echo "======================================================================"
echo ""
echo "This will refactor all middleware modules in src/middleware/"
echo "Backups will be created for all files."
echo ""
read -p "Continue? (y/n) " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 1
fi

# Counter for progress
TOTAL=0
PROCESSED=0

#=============================================================================
# Buffering Modules (5 files)
#=============================================================================

echo ""
echo "=== Buffering Modules ==="
echo ""

# Note: circular_buffer already refactored manually, skip it
declare -a BUFFERING_MODULES=(
    "src/middleware/buffering/nimcp_integration_buffer.c:integration_buffer"
    "src/middleware/buffering/nimcp_phase_coded_buffer.c:phase_coded_buffer"
    "src/middleware/buffering/nimcp_sliding_window.c:sliding_window"
    "src/middleware/buffering/nimcp_temporal_accumulator.c:temporal_accumulator"
)

for entry in "${BUFFERING_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Cognitive Modules (2 files)
#=============================================================================

echo ""
echo "=== Cognitive Modules ==="
echo ""

declare -a COGNITIVE_MODULES=(
    "src/middleware/cognitive/nimcp_cognitive_adapters.c:cognitive_adapters"
    "src/middleware/cognitive/nimcp_working_memory_adapter.c:working_memory_adapter"
)

for entry in "${COGNITIVE_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Encoding Modules (3 files)
#=============================================================================

echo ""
echo "=== Encoding Modules ==="
echo ""

declare -a ENCODING_MODULES=(
    "src/middleware/encoding/nimcp_population_coding.c:population_coding"
    "src/middleware/encoding/nimcp_rate_coding.c:rate_coding"
    "src/middleware/encoding/nimcp_temporal_coding.c:temporal_coding"
)

for entry in "${ENCODING_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Events Modules (4 files)
#=============================================================================

echo ""
echo "=== Events Modules ==="
echo ""

declare -a EVENTS_MODULES=(
    "src/middleware/events/nimcp_event_bus.c:event_bus"
    "src/middleware/events/nimcp_event_queue.c:event_queue"
    "src/middleware/events/nimcp_event_subscriber.c:event_subscriber"
    "src/middleware/events/nimcp_event_types.c:event_types"
)

for entry in "${EVENTS_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Features Module (1 file)
#=============================================================================

echo ""
echo "=== Features Module ==="
echo ""

declare -a FEATURES_MODULES=(
    "src/middleware/features/nimcp_feature_extractor.c:feature_extractor"
)

for entry in "${FEATURES_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Integration Modules (6 files)
#=============================================================================

echo ""
echo "=== Integration Modules ==="
echo ""

declare -a INTEGRATION_MODULES=(
    "src/middleware/brain_integration.c:brain_integration"
    "src/middleware/integration/nimcp_executive_middleware_adapter.c:executive_middleware_adapter"
    "src/middleware/integration/nimcp_flow_tracker.c:flow_tracker"
    "src/middleware/integration/nimcp_middleware_controller.c:middleware_controller"
    "src/middleware/integration/nimcp_quantum_command_propagator.c:quantum_command_propagator"
    "src/middleware/integration/nimcp_shannon_monitor.c:shannon_monitor"
)

for entry in "${INTEGRATION_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Normalization Modules (4 files)
#=============================================================================

echo ""
echo "=== Normalization Modules ==="
echo ""

declare -a NORMALIZATION_MODULES=(
    "src/middleware/normalization/nimcp_adaptive_normalizer.c:adaptive_normalizer"
    "src/middleware/normalization/nimcp_homeostatic_normalizer.c:homeostatic_normalizer"
    "src/middleware/normalization/nimcp_min_max_normalizer.c:min_max_normalizer"
    "src/middleware/normalization/nimcp_zscore_normalizer.c:zscore_normalizer"
)

for entry in "${NORMALIZATION_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Patterns Modules (5 files)
#=============================================================================

echo ""
echo "=== Patterns Modules ==="
echo ""

declare -a PATTERNS_MODULES=(
    "src/middleware/patterns/nimcp_oscillation_detector.c:oscillation_detector"
    "src/middleware/patterns/nimcp_pattern_cow.c:pattern_cow"
    "src/middleware/patterns/nimcp_pattern_library.c:pattern_library"
    "src/middleware/patterns/nimcp_sequence_detector.c:sequence_detector"
    "src/middleware/patterns/nimcp_synchrony_detector.c:synchrony_detector"
)

for entry in "${PATTERNS_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Pipeline Modules (2 files)
#=============================================================================

echo ""
echo "=== Pipeline Modules ==="
echo ""

declare -a PIPELINE_MODULES=(
    "src/middleware/pipeline/nimcp_middleware_context.c:middleware_context"
    "src/middleware/pipeline/nimcp_middleware_pipeline.c:middleware_pipeline"
)

for entry in "${PIPELINE_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Routing Modules (4 files)
#=============================================================================

echo ""
echo "=== Routing Modules ==="
echo ""

declare -a ROUTING_MODULES=(
    "src/middleware/routing/nimcp_attention_gate.c:attention_gate"
    "src/middleware/routing/nimcp_routing_table.c:routing_table"
    "src/middleware/routing/nimcp_signal_wrapper.c:signal_wrapper"
    "src/middleware/routing/nimcp_thalamic_router.c:thalamic_router"
)

for entry in "${ROUTING_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Training Modules (11 files)
#=============================================================================

echo ""
echo "=== Training Modules ==="
echo ""

declare -a TRAINING_MODULES=(
    "src/middleware/training/nimcp_brain_training_integration.c:brain_training_integration"
    "src/middleware/training/nimcp_event_driven_plasticity.c:event_driven_plasticity"
    "src/middleware/training/nimcp_gradient_manager.c:gradient_manager"
    "src/middleware/training/nimcp_loss_functions.c:loss_functions"
    "src/middleware/training/nimcp_lr_scheduler.c:lr_scheduler"
    "src/middleware/training/nimcp_optimizers.c:optimizers"
    "src/middleware/training/nimcp_regularization.c:regularization"
    "src/middleware/training/nimcp_training_adapters.c:training_adapters"
    "src/middleware/training/nimcp_training_callbacks.c:training_callbacks"
    "src/middleware/training/nimcp_training_module.c:training_module"
    "src/middleware/training/nimcp_training_plasticity_bridge.c:training_plasticity_bridge"
)

for entry in "${TRAINING_MODULES[@]}"; do
    IFS=':' read -r file module <<< "$entry"
    if [ -f "$file" ]; then
        echo "Processing: $file"
        bash "$REFACTOR_SCRIPT" "$file" "$module"
        ((PROCESSED++))
    fi
    ((TOTAL++))
done

#=============================================================================
# Summary
#=============================================================================

echo ""
echo "======================================================================"
echo "Batch Refactoring Complete"
echo "======================================================================"
echo ""
echo "Processed: $PROCESSED / $TOTAL modules"
echo ""
echo "Next steps:"
echo "  1. Review all .bak files to verify changes"
echo "  2. For each module:"
echo "     - Copy init/shutdown from .init_template files"
echo "     - Add logging using .logging_guide files"
echo "     - Add config using .config_keys files"
echo "  3. Build and test"
echo "  4. Run: find src/middleware -name '*.bak' -delete  # Remove backups after verification"
echo ""
echo "Generated files pattern:"
echo "  - *.bak            - Original file backups"
echo "  - *.init_template  - Init/shutdown functions to copy"
echo "  - *.logging_guide  - Logging guidelines"
echo "  - *.config_keys    - Configuration key suggestions"
echo ""
