/**
 * @file nimcp_biology_neuromod_bridge.c
 * @brief Biology-Neuromodulatory Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/biology_neuromod/nimcp_biology_neuromod_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(biology_neuromod_bridge)

#define LOG_MODULE "BIOLOGY_NEUROMOD_BRIDGE"


struct nimcp_biology_neuromod_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_biology_neuromod_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_biology_intra_t biology;
    nimcp_neuromod_intra_t neuromod;
    nimcp_biology_neuromod_state_t state;
    nimcp_biology_neuromod_stats_t stats;
    bool is_initialized;
};

nimcp_biology_neuromod_config_t nimcp_biology_neuromod_default_config(void) {
    nimcp_biology_neuromod_config_t config = {
        .receptor_density_coupling = 0.8f,
        .neurogenesis_integration_rate = 0.5f,
        .bdnf_expression_gain = 0.7f,
        .plasticity_gene_sensitivity = 0.6f,
        .update_interval_ms = 10,
        .enable_activity_dependent_plasticity = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_biology_neuromod_bridge_t nimcp_biology_neuromod_create(const nimcp_biology_neuromod_config_t* config) {
    nimcp_biology_neuromod_bridge_t bridge = (nimcp_biology_neuromod_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_biology_neuromod_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate biology-neuromod bridge");
    bridge->config = config ? *config : nimcp_biology_neuromod_default_config();
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.receptor_density_level = 0.5f;
    bridge->state.energy_capacity = 1.0f;
    NIMCP_LOGGING_INFO("Created %s bridge", "biology_neuromod");
    return bridge;
}

void nimcp_biology_neuromod_destroy(nimcp_biology_neuromod_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "biology_neuromod");
    if (bridge->is_initialized) nimcp_biology_neuromod_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_biology_neuromod_init(
    nimcp_biology_neuromod_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_biology_intra_t biology,
    nimcp_neuromod_intra_t neuromod
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in biology_neuromod_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in biology_neuromod_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in biology_neuromod_init");
    bridge->registry = registry;
    bridge->biology = biology;
    bridge->neuromod = neuromod;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_neuromod_shutdown(nimcp_biology_neuromod_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in biology_neuromod_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in biology_neuromod_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_neuromod_update(nimcp_biology_neuromod_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in biology_neuromod_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in biology_neuromod_update");

    /* Gene expression affects receptor density */
    bridge->state.receptor_density_level *= (1.0f - dt * 0.01f);
    bridge->state.receptor_density_level += dt * bridge->config.receptor_density_coupling * 0.01f;

    /* BDNF expression depends on activity */
    bridge->state.bdnf_expression_level *= (1.0f - dt * 0.02f);
    bridge->state.bdnf_expression_level += dt * bridge->config.bdnf_expression_gain * 0.02f;

    /* Plasticity gene activity */
    bridge->state.plasticity_gene_activity *= (1.0f - dt * 0.015f);

    /* Energy capacity decay */
    bridge->state.energy_capacity *= (1.0f - dt * 0.005f);

    /* Update averages */
    bridge->stats.avg_receptor_density = bridge->stats.avg_receptor_density * NIMCP_EMA_DECAY_DEFAULT + bridge->state.receptor_density_level * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_bdnf_level = bridge->stats.avg_bdnf_level * NIMCP_EMA_DECAY_DEFAULT + bridge->state.bdnf_expression_level * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_neuromod_transfer_bottom_up(nimcp_biology_neuromod_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in biology_neuromod_transfer_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in biology_neuromod_transfer_bottom_up");
    bridge->state.bottom_up_messages++;
    bridge->stats.receptor_density_changes++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_neuromod_transfer_top_down(nimcp_biology_neuromod_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in biology_neuromod_transfer_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in biology_neuromod_transfer_top_down");
    bridge->state.top_down_messages++;
    bridge->stats.bdnf_expressions++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_neuromod_get_state(nimcp_biology_neuromod_bridge_t bridge, nimcp_biology_neuromod_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in biology_neuromod_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in biology_neuromod_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_biology_neuromod_get_stats(nimcp_biology_neuromod_bridge_t bridge, nimcp_biology_neuromod_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in biology_neuromod_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in biology_neuromod_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_biology_neuromod_get_coherence(nimcp_biology_neuromod_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_biology_neuromod_reset_stats(nimcp_biology_neuromod_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in biology_neuromod_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
