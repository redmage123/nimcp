/**
 * @file nimcp_sensory_executive_bridge.c
 * @brief Sensory-Executive Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/sensory_executive/nimcp_sensory_executive_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sensory_executive_bridge)

#define LOG_MODULE "SENSORY_EXECUTIVE_BRIDGE"


struct nimcp_sensory_executive_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_sensory_executive_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_sensory_intra_t sensory;
    nimcp_executive_intra_t executive;
    nimcp_sensory_executive_state_t state;
    nimcp_sensory_executive_stats_t stats;
    bool is_initialized;
};

nimcp_sensory_executive_config_t nimcp_sensory_executive_default_config(void) {
    nimcp_sensory_executive_config_t config = {
        .attention_capture_threshold = 0.6f,
        .decision_input_coupling = 0.75f,
        .conflict_detection_gain = 0.7f,
        .feature_enhancement_strength = 0.8f,
        .filter_coupling = 0.65f,
        .predictive_enhancement_gain = 0.7f,
        .update_interval_ms = 10,
        .enable_attention_control = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_sensory_executive_bridge_t nimcp_sensory_executive_create(const nimcp_sensory_executive_config_t* config) {
    nimcp_sensory_executive_bridge_t bridge = (nimcp_sensory_executive_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_sensory_executive_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate sensory-executive bridge");
    bridge->config = config ? *config : nimcp_sensory_executive_default_config();
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.attention_capture_level = 0.5f;
    bridge->state.filter_strength = 0.5f;
    NIMCP_LOGGING_INFO("Created %s bridge", "sensory_executive");
    return bridge;
}

void nimcp_sensory_executive_destroy(nimcp_sensory_executive_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "sensory_executive");
    if (bridge->is_initialized) nimcp_sensory_executive_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_sensory_executive_init(
    nimcp_sensory_executive_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_sensory_intra_t sensory,
    nimcp_executive_intra_t executive
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_executive_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in sensory_executive_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in sensory_executive_init");
    bridge->registry = registry;
    bridge->sensory = sensory;
    bridge->executive = executive;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_executive_shutdown(nimcp_sensory_executive_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_executive_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in sensory_executive_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_executive_update(nimcp_sensory_executive_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_executive_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in sensory_executive_update");

    /* Salient stimuli capture attention */
    bridge->state.attention_capture_level *= (1.0f - dt * 0.01f);

    /* Decision input strength */
    bridge->state.decision_input_strength *= (1.0f - dt * 0.02f);
    bridge->state.decision_input_strength += dt * bridge->config.decision_input_coupling * 0.02f;

    /* Conflict level decay */
    bridge->state.conflict_level *= (1.0f - dt * 0.03f);

    /* Feature enhancement from attention */
    bridge->state.feature_enhancement *= (1.0f - dt * 0.02f);
    bridge->state.feature_enhancement += dt * bridge->config.feature_enhancement_strength * 0.02f;

    /* Filter strength adjustment */
    bridge->state.filter_strength *= (1.0f - dt * 0.015f);

    /* Predictive enhancement decay */
    bridge->state.predictive_enhancement *= (1.0f - dt * 0.025f);

    /* Update averages */
    bridge->stats.avg_attention_capture = bridge->stats.avg_attention_capture * NIMCP_EMA_DECAY_DEFAULT + bridge->state.attention_capture_level * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_feature_enhancement = bridge->stats.avg_feature_enhancement * NIMCP_EMA_DECAY_DEFAULT + bridge->state.feature_enhancement * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_executive_transfer_bottom_up(nimcp_sensory_executive_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_executive_transfer_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in sensory_executive_transfer_bottom_up");
    bridge->state.bottom_up_messages++;
    bridge->stats.attention_captures++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_executive_transfer_top_down(nimcp_sensory_executive_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_executive_transfer_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in sensory_executive_transfer_top_down");
    bridge->state.top_down_messages++;
    bridge->stats.feature_enhancements++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_executive_get_state(nimcp_sensory_executive_bridge_t bridge, nimcp_sensory_executive_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_executive_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in sensory_executive_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_executive_get_stats(nimcp_sensory_executive_bridge_t bridge, nimcp_sensory_executive_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_executive_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in sensory_executive_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_sensory_executive_get_coherence(nimcp_sensory_executive_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_sensory_executive_reset_stats(nimcp_sensory_executive_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in sensory_executive_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
