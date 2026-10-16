/**
 * @file nimcp_sensory_memory_bridge.c
 * @brief Sensory-Memory Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/sensory_memory/nimcp_sensory_memory_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sensory_memory_bridge)

#define LOG_MODULE "SENSORY_MEMORY_BRIDGE"


struct nimcp_sensory_memory_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_sensory_memory_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_sensory_intra_t sensory;
    nimcp_memory_intra_t memory;
    nimcp_sensory_memory_state_t state;
    nimcp_sensory_memory_stats_t stats;
    bool is_initialized;
};

nimcp_sensory_memory_config_t nimcp_sensory_memory_default_config(void) {
    nimcp_sensory_memory_config_t config = {
        .feature_encoding_coupling = 0.8f,
        .multimodal_binding_strength = 0.75f,
        .spatial_encoding_gain = 0.7f,
        .expectation_coupling = 0.65f,
        .priming_strength = 0.6f,
        .context_bias_coupling = 0.55f,
        .update_interval_ms = 10,
        .enable_predictive_coding = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_sensory_memory_bridge_t nimcp_sensory_memory_create(const nimcp_sensory_memory_config_t* config) {
    nimcp_sensory_memory_bridge_t bridge = (nimcp_sensory_memory_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_sensory_memory_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate sensory-memory bridge");
    bridge->config = config ? *config : nimcp_sensory_memory_default_config();
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.feature_encoding_level = 0.5f;
    bridge->state.multimodal_binding = 0.5f;
    NIMCP_LOGGING_INFO("Created %s bridge", "sensory_memory");
    return bridge;
}

void nimcp_sensory_memory_destroy(nimcp_sensory_memory_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "sensory_memory");
    if (bridge->is_initialized) nimcp_sensory_memory_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_sensory_memory_init(
    nimcp_sensory_memory_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_sensory_intra_t sensory,
    nimcp_memory_intra_t memory
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_memory_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in sensory_memory_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in sensory_memory_init");
    bridge->registry = registry;
    bridge->sensory = sensory;
    bridge->memory = memory;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_memory_shutdown(nimcp_sensory_memory_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_memory_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in sensory_memory_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_memory_update(nimcp_sensory_memory_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_memory_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in sensory_memory_update");

    /* Feature encoding to hippocampus */
    bridge->state.feature_encoding_level *= (1.0f - dt * 0.01f);
    bridge->state.feature_encoding_level += dt * bridge->config.feature_encoding_coupling * 0.01f;

    /* Multi-modal binding for episodic context */
    bridge->state.multimodal_binding *= (1.0f - dt * 0.02f);
    bridge->state.multimodal_binding += dt * bridge->config.multimodal_binding_strength * 0.02f;

    /* Spatial encoding for place cells */
    bridge->state.spatial_encoding *= (1.0f - dt * 0.015f);

    /* Expectation strength decay */
    bridge->state.expectation_strength *= (1.0f - dt * 0.03f);

    /* Priming level decay */
    bridge->state.priming_level *= (1.0f - dt * 0.04f);

    /* Context bias decay */
    bridge->state.context_bias *= (1.0f - dt * 0.025f);

    /* Update averages */
    bridge->stats.avg_encoding_strength = bridge->stats.avg_encoding_strength * NIMCP_EMA_DECAY_DEFAULT + bridge->state.feature_encoding_level * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_expectation_match = bridge->stats.avg_expectation_match * NIMCP_EMA_DECAY_DEFAULT + bridge->state.expectation_strength * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_memory_transfer_bottom_up(nimcp_sensory_memory_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_memory_transfer_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in sensory_memory_transfer_bottom_up");
    bridge->state.bottom_up_messages++;
    bridge->stats.feature_encodings++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_memory_transfer_top_down(nimcp_sensory_memory_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_memory_transfer_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in sensory_memory_transfer_top_down");
    bridge->state.top_down_messages++;
    bridge->stats.expectations++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_memory_get_state(nimcp_sensory_memory_bridge_t bridge, nimcp_sensory_memory_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_memory_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in sensory_memory_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_memory_get_stats(nimcp_sensory_memory_bridge_t bridge, nimcp_sensory_memory_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_memory_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in sensory_memory_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_sensory_memory_get_coherence(nimcp_sensory_memory_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_sensory_memory_reset_stats(nimcp_sensory_memory_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_memory_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
