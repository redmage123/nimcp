/**
 * @file nimcp_sensory_memory_bridge.h
 * @brief Sensory-Memory Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridges sensory processing to memory encoding
 * WHY:  Sensory experiences must be encoded for retention
 * HOW:  Routes processed sensory features to hippocampal encoding
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Sensory → Memory):
 * - Processed features → pattern encoding
 * - Multi-modal binding → episodic context
 * - Spatial information → place cell activation
 *
 * Top-Down (Memory → Sensory):
 * - Expectations → sensory prediction
 * - Memory recall → perceptual priming
 * - Context → sensory interpretation bias
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SENSORY_MEMORY_BRIDGE_H
#define NIMCP_SENSORY_MEMORY_BRIDGE_H

#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
#include "integration/intra/sensory/nimcp_sensory_intra_coordinator.h"
#include "integration/intra/memory/nimcp_memory_intra_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge message types */
#define SENS_MEM_MSG_FEATURE_ENCODE     (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0060)
#define SENS_MEM_MSG_MULTIMODAL_BIND    (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0061)
#define SENS_MEM_MSG_SPATIAL_INFO       (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0062)
#define SENS_MEM_MSG_EXPECTATION        (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0063)
#define SENS_MEM_MSG_PERCEPTUAL_PRIME   (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0064)
#define SENS_MEM_MSG_CONTEXT_BIAS       (NIMCP_LAYER_MSG_BRIDGE_BASE + 0x0065)

typedef struct nimcp_sensory_memory_bridge_struct* nimcp_sensory_memory_bridge_t;

typedef struct {
    float feature_encoding_coupling;
    float multimodal_binding_strength;
    float spatial_encoding_gain;
    float expectation_coupling;
    float priming_strength;
    float context_bias_coupling;
    uint32_t update_interval_ms;
    bool enable_predictive_coding;
    bool enable_logging;
    bool enable_metrics;
} nimcp_sensory_memory_config_t;

typedef struct {
    float feature_encoding_level;
    float multimodal_binding;
    float spatial_encoding;
    float expectation_strength;
    float priming_level;
    float context_bias;
    float bridge_coherence;
    uint64_t bottom_up_messages;
    uint64_t top_down_messages;
} nimcp_sensory_memory_state_t;

typedef struct {
    uint64_t feature_encodings;
    uint64_t multimodal_bindings;
    uint64_t spatial_encodings;
    uint64_t expectations;
    uint64_t priming_events;
    uint64_t context_biases;
    float avg_encoding_strength;
    float avg_expectation_match;
} nimcp_sensory_memory_stats_t;

NIMCP_EXPORT nimcp_sensory_memory_config_t nimcp_sensory_memory_default_config(void);
NIMCP_EXPORT nimcp_sensory_memory_bridge_t nimcp_sensory_memory_create(const nimcp_sensory_memory_config_t* config);
NIMCP_EXPORT void nimcp_sensory_memory_destroy(nimcp_sensory_memory_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_memory_init(
    nimcp_sensory_memory_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_sensory_intra_t sensory,
    nimcp_memory_intra_t memory
);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_memory_shutdown(nimcp_sensory_memory_bridge_t bridge);

NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_memory_update(nimcp_sensory_memory_bridge_t bridge, float dt);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_memory_transfer_bottom_up(nimcp_sensory_memory_bridge_t bridge, const nimcp_layer_msg_t* msg);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_memory_transfer_top_down(nimcp_sensory_memory_bridge_t bridge, const nimcp_layer_msg_t* msg);

NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_memory_get_state(nimcp_sensory_memory_bridge_t bridge, nimcp_sensory_memory_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_memory_get_stats(nimcp_sensory_memory_bridge_t bridge, nimcp_sensory_memory_stats_t* stats_out);
NIMCP_EXPORT float nimcp_sensory_memory_get_coherence(nimcp_sensory_memory_bridge_t bridge);
NIMCP_EXPORT nimcp_layer_error_t nimcp_sensory_memory_reset_stats(nimcp_sensory_memory_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SENSORY_MEMORY_BRIDGE_H */
