/**
 * @file nimcp_hippocampus_adapter.h
 * @brief Hippocampus Adapter for Memory Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts hippocampal episodic memory for Memory layer
 * WHY:  Hippocampus is critical for episodic memory and spatial navigation
 * HOW:  Implements pattern separation, completion, and replay
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HIPPOCAMPUS_ADAPTER_H
#define NIMCP_HIPPOCAMPUS_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nimcp_hippocampus_adapter_struct* nimcp_hippocampus_adapter_t;

typedef struct {
    uint32_t ca3_size;                  /**< CA3 pattern storage capacity */
    uint32_t ca1_size;                  /**< CA1 output size */
    uint32_t dg_size;                   /**< Dentate gyrus size */
    float pattern_separation_strength;  /**< DG orthogonalization */
    float pattern_completion_threshold; /**< CA3 completion threshold */
    bool enable_replay;                 /**< Enable memory replay */
    bool enable_logging;
} nimcp_hippocampus_config_t;

typedef struct {
    float pattern_separation_index;
    float pattern_completion_rate;
    uint32_t stored_patterns;
    uint32_t replay_events;
    bool is_active;
} nimcp_hippocampus_state_t;

typedef struct {
    uint64_t updates_processed;
    uint64_t messages_handled;
    uint64_t patterns_encoded;
    uint64_t patterns_retrieved;
    uint64_t replay_cycles;
} nimcp_hippocampus_stats_t;

NIMCP_EXPORT nimcp_hippocampus_config_t nimcp_hippocampus_adapter_default_config(void);
NIMCP_EXPORT nimcp_hippocampus_adapter_t nimcp_hippocampus_adapter_create(const nimcp_hippocampus_config_t* config);
NIMCP_EXPORT void nimcp_hippocampus_adapter_destroy(nimcp_hippocampus_adapter_t adapter);
NIMCP_EXPORT nimcp_module_interface_t* nimcp_hippocampus_adapter_get_interface(nimcp_hippocampus_adapter_t adapter);
NIMCP_EXPORT nimcp_layer_error_t nimcp_hippocampus_adapter_encode_episode(nimcp_hippocampus_adapter_t adapter, const float* pattern, uint32_t size);
NIMCP_EXPORT nimcp_layer_error_t nimcp_hippocampus_adapter_retrieve(nimcp_hippocampus_adapter_t adapter, const float* cue, uint32_t cue_size, float* pattern_out, uint32_t max_size);
NIMCP_EXPORT nimcp_layer_error_t nimcp_hippocampus_adapter_trigger_replay(nimcp_hippocampus_adapter_t adapter);
NIMCP_EXPORT nimcp_layer_error_t nimcp_hippocampus_adapter_get_state(nimcp_hippocampus_adapter_t adapter, nimcp_hippocampus_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_hippocampus_adapter_get_stats(nimcp_hippocampus_adapter_t adapter, nimcp_hippocampus_stats_t* stats_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_hippocampus_adapter_reset_stats(nimcp_hippocampus_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIPPOCAMPUS_ADAPTER_H */
