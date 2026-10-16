/**
 * @file nimcp_superhuman_neuromod_bridge.c
 * @brief Superhuman-Neuromodulatory Inter-Layer Bridge (Feedback Loop) Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/superhuman_neuromod/nimcp_superhuman_neuromod_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(superhuman_neuromod_bridge)

#define LOG_MODULE "SUPERHUMAN_NEUROMOD_BRIDGE"


struct nimcp_superhuman_neuromod_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_superhuman_neuromod_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_superhuman_intra_t superhuman;
    nimcp_neuromod_intra_t neuromod;
    nimcp_superhuman_neuromod_state_t state;
    nimcp_superhuman_neuromod_stats_t stats;
    bool is_initialized;
};

nimcp_superhuman_neuromod_config_t nimcp_superhuman_neuromod_default_config(void) {
    nimcp_superhuman_neuromod_config_t config = {
        .novel_enhance_lc_gain = 0.9f,
        .threat_stress_gain = 0.8f,
        .time_arousal_coupling = 0.7f,
        .enhance_reward_da_gain = 0.75f,
        .ne_sensitivity_coupling = 0.7f,
        .da_cap_learning_coupling = 0.65f,
        .stress_dilation_threshold = 0.8f,
        .update_interval_ms = 10,
        .enable_feedback_loop = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_superhuman_neuromod_bridge_t nimcp_superhuman_neuromod_create(const nimcp_superhuman_neuromod_config_t* config) {
    nimcp_superhuman_neuromod_bridge_t bridge = (nimcp_superhuman_neuromod_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_superhuman_neuromod_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate superhuman-neuromod bridge");
    bridge->config = config ? *config : nimcp_superhuman_neuromod_default_config();
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.sensitivity_level = 0.5f;
    bridge->state.stress_dilation_trigger = 0.0f;
    NIMCP_LOGGING_INFO("Created %s bridge", "superhuman_neuromod");
    return bridge;
}

void nimcp_superhuman_neuromod_destroy(nimcp_superhuman_neuromod_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "superhuman_neuromod");
    if (bridge->is_initialized) nimcp_superhuman_neuromod_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_superhuman_neuromod_init(
    nimcp_superhuman_neuromod_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_superhuman_intra_t superhuman,
    nimcp_neuromod_intra_t neuromod
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in superhuman_neuromod_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in superhuman_neuromod_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in superhuman_neuromod_init");
    bridge->registry = registry;
    bridge->superhuman = superhuman;
    bridge->neuromod = neuromod;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_neuromod_shutdown(nimcp_superhuman_neuromod_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in superhuman_neuromod_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in superhuman_neuromod_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_neuromod_update(nimcp_superhuman_neuromod_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in superhuman_neuromod_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in superhuman_neuromod_update");

    /* Novel enhanced percepts trigger LC (NE) */
    bridge->state.novel_enhancement_signal *= (1.0f - dt * 0.03f);

    /* Threat detection triggers stress response */
    bridge->state.threat_detection_level *= (1.0f - dt * 0.02f);

    /* Time arousal adjustment */
    bridge->state.time_arousal_adjustment *= (1.0f - dt * 0.025f);

    /* Enhancement reward signal (DA) */
    bridge->state.enhancement_reward *= (1.0f - dt * 0.03f);

    /* NE modulates sensitivity */
    bridge->state.sensitivity_level *= (1.0f - dt * 0.01f);
    bridge->state.sensitivity_level += dt * bridge->config.ne_sensitivity_coupling * 0.01f;

    /* DA modulates capability learning */
    bridge->state.capability_learning_signal *= (1.0f - dt * 0.02f);

    /* Stress triggers time dilation */
    if (bridge->state.threat_detection_level > bridge->config.stress_dilation_threshold) {
        bridge->state.stress_dilation_trigger = 1.0f;
    } else {
        bridge->state.stress_dilation_trigger *= (1.0f - dt * 0.05f);
    }

    /* Update averages */
    bridge->stats.avg_enhancement_signal = bridge->stats.avg_enhancement_signal * NIMCP_EMA_DECAY_DEFAULT + bridge->state.novel_enhancement_signal * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_sensitivity = bridge->stats.avg_sensitivity * NIMCP_EMA_DECAY_DEFAULT + bridge->state.sensitivity_level * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_neuromod_transfer_feedback(nimcp_superhuman_neuromod_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in superhuman_neuromod_transfer_feedback");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in superhuman_neuromod_transfer_feedback");
    bridge->state.feedback_messages++;
    bridge->stats.novel_enhancements++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_neuromod_transfer_modulation(nimcp_superhuman_neuromod_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in superhuman_neuromod_transfer_modulation");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in superhuman_neuromod_transfer_modulation");
    bridge->state.modulation_messages++;
    bridge->stats.sensitivity_updates++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_neuromod_get_state(nimcp_superhuman_neuromod_bridge_t bridge, nimcp_superhuman_neuromod_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in superhuman_neuromod_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in superhuman_neuromod_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_neuromod_get_stats(nimcp_superhuman_neuromod_bridge_t bridge, nimcp_superhuman_neuromod_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in superhuman_neuromod_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in superhuman_neuromod_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_superhuman_neuromod_get_coherence(nimcp_superhuman_neuromod_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_superhuman_neuromod_reset_stats(nimcp_superhuman_neuromod_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in superhuman_neuromod_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
