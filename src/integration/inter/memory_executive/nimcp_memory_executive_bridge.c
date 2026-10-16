/**
 * @file nimcp_memory_executive_bridge.c
 * @brief Memory-Executive Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/memory_executive/nimcp_memory_executive_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(memory_executive_bridge)

#define LOG_MODULE "MEMORY_EXECUTIVE_BRIDGE"


struct nimcp_memory_executive_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_memory_executive_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_memory_intra_t memory;
    nimcp_executive_intra_t executive;
    nimcp_memory_executive_state_t state;
    nimcp_memory_executive_stats_t stats;
    bool is_initialized;
};

nimcp_memory_executive_config_t nimcp_memory_executive_default_config(void) {
    nimcp_memory_executive_config_t config = {
        .context_retrieval_coupling = 0.8f,
        .value_estimation_gain = 0.75f,
        .rule_application_coupling = 0.7f,
        .wm_rehearsal_strength = 0.65f,
        .retrieval_cue_coupling = 0.7f,
        .learning_signal_gain = 0.8f,
        .update_interval_ms = 10,
        .enable_prospective_memory = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_memory_executive_bridge_t nimcp_memory_executive_create(const nimcp_memory_executive_config_t* config) {
    nimcp_memory_executive_bridge_t bridge = (nimcp_memory_executive_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_memory_executive_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate memory-executive bridge");
    bridge->config = config ? *config : nimcp_memory_executive_default_config();
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.decision_context_strength = 0.5f;
    bridge->state.wm_rehearsal_level = 0.5f;
    NIMCP_LOGGING_INFO("Created %s bridge", "memory_executive");
    return bridge;
}

void nimcp_memory_executive_destroy(nimcp_memory_executive_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "memory_executive");
    if (bridge->is_initialized) nimcp_memory_executive_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_memory_executive_init(
    nimcp_memory_executive_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_memory_intra_t memory,
    nimcp_executive_intra_t executive
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_executive_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in memory_executive_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in memory_executive_init");
    bridge->registry = registry;
    bridge->memory = memory;
    bridge->executive = executive;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_executive_shutdown(nimcp_memory_executive_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_executive_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in memory_executive_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_executive_update(nimcp_memory_executive_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_executive_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in memory_executive_update");

    /* Decision context from memory retrieval */
    bridge->state.decision_context_strength *= (1.0f - dt * 0.01f);
    bridge->state.decision_context_strength += dt * bridge->config.context_retrieval_coupling * 0.01f;

    /* Value estimation from episodic recall */
    bridge->state.value_estimate *= (1.0f - dt * 0.02f);
    bridge->state.value_estimate += dt * bridge->config.value_estimation_gain * 0.02f;

    /* Rule activation from semantic knowledge */
    bridge->state.rule_activation *= (1.0f - dt * 0.015f);

    /* Working memory rehearsal */
    bridge->state.wm_rehearsal_level *= (1.0f - dt * 0.03f);
    bridge->state.wm_rehearsal_level += dt * bridge->config.wm_rehearsal_strength * 0.03f;

    /* Retrieval cue strength decay */
    bridge->state.retrieval_cue_strength *= (1.0f - dt * 0.02f);

    /* Learning signal decay */
    bridge->state.learning_signal *= (1.0f - dt * 0.04f);

    /* Update averages */
    bridge->stats.avg_context_strength = bridge->stats.avg_context_strength * NIMCP_EMA_DECAY_DEFAULT + bridge->state.decision_context_strength * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_value_accuracy = bridge->stats.avg_value_accuracy * NIMCP_EMA_DECAY_DEFAULT + bridge->state.value_estimate * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_executive_transfer_bottom_up(nimcp_memory_executive_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_executive_transfer_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in memory_executive_transfer_bottom_up");
    bridge->state.bottom_up_messages++;
    bridge->stats.context_retrievals++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_executive_transfer_top_down(nimcp_memory_executive_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_executive_transfer_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in memory_executive_transfer_top_down");
    bridge->state.top_down_messages++;
    bridge->stats.wm_rehearsals++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_executive_get_state(nimcp_memory_executive_bridge_t bridge, nimcp_memory_executive_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_executive_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in memory_executive_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_executive_get_stats(nimcp_memory_executive_bridge_t bridge, nimcp_memory_executive_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_executive_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in memory_executive_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_memory_executive_get_coherence(nimcp_memory_executive_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_memory_executive_reset_stats(nimcp_memory_executive_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_executive_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
