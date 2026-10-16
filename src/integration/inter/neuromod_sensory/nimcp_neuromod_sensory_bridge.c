/**
 * @file nimcp_neuromod_sensory_bridge.c
 * @brief Neuromodulatory-Sensory Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/neuromod_sensory/nimcp_neuromod_sensory_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_sensory_bridge)

#define LOG_MODULE "NEUROMOD_SENSORY_BRIDGE"


struct nimcp_neuromod_sensory_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_neuromod_sensory_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_neuromod_intra_t neuromod;
    nimcp_sensory_intra_t sensory;
    nimcp_neuromod_sensory_state_t state;
    nimcp_neuromod_sensory_stats_t stats;
    bool is_initialized;
};

nimcp_neuromod_sensory_config_t nimcp_neuromod_sensory_default_config(void) {
    nimcp_neuromod_sensory_config_t config = {
        .ne_gain_coupling = 0.8f,
        .da_salience_coupling = 0.7f,
        .arousal_threshold_coupling = 0.6f,
        .novelty_lc_gain = 0.9f,
        .reward_vta_gain = 0.8f,
        .update_interval_ms = 10,
        .enable_gain_control = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_neuromod_sensory_bridge_t nimcp_neuromod_sensory_create(const nimcp_neuromod_sensory_config_t* config) {
    nimcp_neuromod_sensory_bridge_t bridge = (nimcp_neuromod_sensory_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_neuromod_sensory_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate neuromod-sensory bridge");
    bridge->config = config ? *config : nimcp_neuromod_sensory_default_config();
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.sensory_gain = 1.0f;
    bridge->state.threshold_level = 0.5f;
    NIMCP_LOGGING_INFO("Created %s bridge", "neuromod_sensory");
    return bridge;
}

void nimcp_neuromod_sensory_destroy(nimcp_neuromod_sensory_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromod_sensory");
    if (bridge->is_initialized) nimcp_neuromod_sensory_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_neuromod_sensory_init(
    nimcp_neuromod_sensory_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_neuromod_intra_t neuromod,
    nimcp_sensory_intra_t sensory
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_sensory_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in neuromod_sensory_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in neuromod_sensory_init");
    bridge->registry = registry;
    bridge->neuromod = neuromod;
    bridge->sensory = sensory;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_sensory_shutdown(nimcp_neuromod_sensory_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_sensory_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in neuromod_sensory_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_sensory_update(nimcp_neuromod_sensory_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_sensory_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in neuromod_sensory_update");

    /* NE levels affect sensory gain */
    bridge->state.sensory_gain *= (1.0f - dt * 0.01f);
    bridge->state.sensory_gain += dt * bridge->config.ne_gain_coupling * 0.01f;

    /* DA levels affect salience boost */
    bridge->state.salience_boost *= (1.0f - dt * 0.02f);
    bridge->state.salience_boost += dt * bridge->config.da_salience_coupling * 0.02f;

    /* Threshold adjustment based on arousal */
    bridge->state.threshold_level *= (1.0f - dt * 0.015f);

    /* Novelty signal decay */
    bridge->state.novelty_signal *= (1.0f - dt * 0.05f);

    /* Reward signal decay */
    bridge->state.reward_signal *= (1.0f - dt * 0.03f);

    /* Update averages */
    bridge->stats.avg_sensory_gain = bridge->stats.avg_sensory_gain * NIMCP_EMA_DECAY_DEFAULT + bridge->state.sensory_gain * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_salience = bridge->stats.avg_salience * NIMCP_EMA_DECAY_DEFAULT + bridge->state.salience_boost * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_sensory_transfer_bottom_up(nimcp_neuromod_sensory_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_sensory_transfer_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in neuromod_sensory_transfer_bottom_up");
    bridge->state.bottom_up_messages++;
    bridge->stats.gain_modulations++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_sensory_transfer_top_down(nimcp_neuromod_sensory_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_sensory_transfer_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in neuromod_sensory_transfer_top_down");
    bridge->state.top_down_messages++;
    bridge->stats.novel_stimuli++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_sensory_get_state(nimcp_neuromod_sensory_bridge_t bridge, nimcp_neuromod_sensory_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_sensory_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in neuromod_sensory_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_sensory_get_stats(nimcp_neuromod_sensory_bridge_t bridge, nimcp_neuromod_sensory_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_sensory_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in neuromod_sensory_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_neuromod_sensory_get_coherence(nimcp_neuromod_sensory_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_neuromod_sensory_reset_stats(nimcp_neuromod_sensory_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_sensory_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
