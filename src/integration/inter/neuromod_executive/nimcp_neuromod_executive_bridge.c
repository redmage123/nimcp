/**
 * @file nimcp_neuromod_executive_bridge.c
 * @brief Neuromodulatory-Executive Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/neuromod_executive/nimcp_neuromod_executive_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_executive_bridge)

#define LOG_MODULE "NEUROMOD_EXECUTIVE_BRIDGE"


struct nimcp_neuromod_executive_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_neuromod_executive_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_neuromod_intra_t neuromod;
    nimcp_executive_intra_t executive;
    nimcp_neuromod_executive_state_t state;
    nimcp_neuromod_executive_stats_t stats;
    bool is_initialized;
};

nimcp_neuromod_executive_config_t nimcp_neuromod_executive_default_config(void) {
    nimcp_neuromod_executive_config_t config = {
        .da_motivation_coupling = 0.85f,
        .ne_flexibility_coupling = 0.7f,
        .serotonin_impulse_coupling = 0.75f,
        .goal_reward_gain = 0.8f,
        .effort_ne_gain = 0.6f,
        .conflict_serotonin_gain = 0.65f,
        .update_interval_ms = 10,
        .enable_optimal_arousal = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_neuromod_executive_bridge_t nimcp_neuromod_executive_create(const nimcp_neuromod_executive_config_t* config) {
    nimcp_neuromod_executive_bridge_t bridge = (nimcp_neuromod_executive_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_neuromod_executive_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate neuromod-executive bridge");
    bridge->config = config ? *config : nimcp_neuromod_executive_default_config();
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.motivation_level = 0.5f;
    bridge->state.cognitive_flexibility = 0.7f;
    bridge->state.impulse_control = 0.6f;
    NIMCP_LOGGING_INFO("Created %s bridge", "neuromod_executive");
    return bridge;
}

void nimcp_neuromod_executive_destroy(nimcp_neuromod_executive_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "neuromod_executive");
    if (bridge->is_initialized) nimcp_neuromod_executive_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_neuromod_executive_init(
    nimcp_neuromod_executive_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_neuromod_intra_t neuromod,
    nimcp_executive_intra_t executive
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_executive_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in neuromod_executive_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in neuromod_executive_init");
    bridge->registry = registry;
    bridge->neuromod = neuromod;
    bridge->executive = executive;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_executive_shutdown(nimcp_neuromod_executive_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_executive_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in neuromod_executive_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_executive_update(nimcp_neuromod_executive_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_executive_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in neuromod_executive_update");

    /* DA levels affect motivation */
    bridge->state.motivation_level *= (1.0f - dt * 0.01f);
    bridge->state.motivation_level += dt * bridge->config.da_motivation_coupling * 0.01f;

    /* NE levels affect cognitive flexibility */
    bridge->state.cognitive_flexibility *= (1.0f - dt * 0.02f);
    bridge->state.cognitive_flexibility += dt * bridge->config.ne_flexibility_coupling * 0.02f;

    /* Serotonin affects impulse control */
    bridge->state.impulse_control *= (1.0f - dt * 0.015f);
    bridge->state.impulse_control += dt * bridge->config.serotonin_impulse_coupling * 0.015f;

    /* Goal reward signal decay */
    bridge->state.goal_reward_signal *= (1.0f - dt * 0.03f);

    /* Effort signal decay */
    bridge->state.effort_signal *= (1.0f - dt * 0.02f);

    /* Conflict level decay */
    bridge->state.conflict_level *= (1.0f - dt * 0.04f);

    /* Update averages */
    bridge->stats.avg_motivation = bridge->stats.avg_motivation * NIMCP_EMA_DECAY_DEFAULT + bridge->state.motivation_level * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_flexibility = bridge->stats.avg_flexibility * NIMCP_EMA_DECAY_DEFAULT + bridge->state.cognitive_flexibility * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_executive_transfer_bottom_up(nimcp_neuromod_executive_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_executive_transfer_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in neuromod_executive_transfer_bottom_up");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in neuromod_executive_transfer_bottom_up");
    bridge->state.bottom_up_messages++;
    bridge->stats.motivation_updates++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_executive_transfer_top_down(nimcp_neuromod_executive_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_executive_transfer_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in neuromod_executive_transfer_top_down");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in neuromod_executive_transfer_top_down");
    bridge->state.top_down_messages++;
    bridge->stats.goal_rewards++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_executive_get_state(nimcp_neuromod_executive_bridge_t bridge, nimcp_neuromod_executive_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_executive_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in neuromod_executive_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_executive_get_stats(nimcp_neuromod_executive_bridge_t bridge, nimcp_neuromod_executive_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_executive_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in neuromod_executive_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_neuromod_executive_get_coherence(nimcp_neuromod_executive_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_neuromod_executive_reset_stats(nimcp_neuromod_executive_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in neuromod_executive_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
