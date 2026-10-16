/**
 * @file nimcp_chemistry_biology_bridge.c
 * @brief Chemistry-Biology Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/chemistry_biology/nimcp_chemistry_biology_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(chemistry_biology_bridge)

#define LOG_MODULE "CHEMISTRY_BIOLOGY_BRIDGE"


struct nimcp_chemistry_biology_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_chemistry_biology_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_chemistry_intra_t chemistry;
    nimcp_biology_intra_t biology;
    nimcp_chemistry_biology_state_t state;
    nimcp_chemistry_biology_stats_t stats;
    bool is_initialized;
};

nimcp_chemistry_biology_config_t nimcp_chemistry_biology_default_config(void) {
    nimcp_chemistry_biology_config_t config = {
        .receptor_coupling_strength = 0.8f,
        .ph_sensitivity = 0.9f,
        .no_signaling_gain = 0.7f,
        .protein_synthesis_rate = 0.5f,
        .update_interval_ms = 10,
        .enable_receptor_dynamics = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_chemistry_biology_bridge_t nimcp_chemistry_biology_create(const nimcp_chemistry_biology_config_t* config) {
    nimcp_chemistry_biology_bridge_t bridge = (nimcp_chemistry_biology_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_chemistry_biology_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate chemistry-biology bridge");
    bridge->config = config ? *config : nimcp_chemistry_biology_default_config();
    bridge->state.bridge_coherence = 1.0f;
    NIMCP_LOGGING_INFO("Created %s bridge", "chemistry_biology");
    return bridge;
}

void nimcp_chemistry_biology_destroy(nimcp_chemistry_biology_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "chemistry_biology");
    if (bridge->is_initialized) nimcp_chemistry_biology_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_chemistry_biology_init(
    nimcp_chemistry_biology_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_chemistry_intra_t chemistry,
    nimcp_biology_intra_t biology
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in chemistry_biology_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in chemistry_biology_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in chemistry_biology_init");
    bridge->registry = registry;
    bridge->chemistry = chemistry;
    bridge->biology = biology;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_biology_shutdown(nimcp_chemistry_biology_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in chemistry_biology_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in chemistry_biology_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_biology_update(nimcp_chemistry_biology_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in chemistry_biology_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in chemistry_biology_update");

    /* Receptor activation decays */
    bridge->state.receptor_activation_level *= (1.0f - dt * 0.1f);

    /* pH effects on protein function */
    bridge->state.ph_effect_magnitude *= (1.0f - dt * 0.05f);

    /* NO signal diffuses and decays */
    bridge->state.no_signal_strength *= (1.0f - dt * 0.2f);

    /* Update stats */
    bridge->stats.avg_receptor_activation = bridge->stats.avg_receptor_activation * NIMCP_EMA_DECAY_DEFAULT + bridge->state.receptor_activation_level * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_synthesis_rate = bridge->stats.avg_synthesis_rate * NIMCP_EMA_DECAY_DEFAULT + bridge->state.protein_synthesis_load * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_biology_transfer_bottom_up(nimcp_chemistry_biology_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in chemistry_biology_transfer_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in chemistry_biology_transfer_bottom_up");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in chemistry_biology_transfer_bottom_up");
    bridge->state.bottom_up_messages++;
    bridge->stats.receptor_activations++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_biology_transfer_top_down(nimcp_chemistry_biology_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in chemistry_biology_transfer_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in chemistry_biology_transfer_top_down");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in chemistry_biology_transfer_top_down");
    bridge->state.top_down_messages++;
    bridge->stats.protein_requests++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_biology_get_state(nimcp_chemistry_biology_bridge_t bridge, nimcp_chemistry_biology_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in chemistry_biology_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in chemistry_biology_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_biology_get_stats(nimcp_chemistry_biology_bridge_t bridge, nimcp_chemistry_biology_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in chemistry_biology_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in chemistry_biology_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_chemistry_biology_get_coherence(nimcp_chemistry_biology_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_chemistry_biology_reset_stats(nimcp_chemistry_biology_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in chemistry_biology_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
