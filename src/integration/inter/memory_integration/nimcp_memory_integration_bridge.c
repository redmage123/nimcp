/**
 * @file nimcp_memory_integration_bridge.c
 * @brief Memory-Integration Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/memory_integration/nimcp_memory_integration_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(memory_integration_bridge)

#define LOG_MODULE "MEMORY_INTEGRATION_BRIDGE"


struct nimcp_memory_integration_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_memory_integration_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_memory_intra_t memory;
    nimcp_integration_intra_t integration;
    nimcp_memory_integration_state_t state;
    nimcp_memory_integration_stats_t stats;
    bool is_initialized;
};

nimcp_memory_integration_config_t nimcp_memory_integration_default_config(void) {
    nimcp_memory_integration_config_t config = {
        .gw_entry_threshold = 0.6f,
        .conscious_recall_coupling = 0.8f,
        .arousal_modulation_gain = 0.7f,
        .consolidation_trigger_threshold = 0.7f,
        .encoding_gate_coupling = 0.75f,
        .retrieval_priority_coupling = 0.65f,
        .update_interval_ms = 10,
        .enable_conscious_access = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_memory_integration_bridge_t nimcp_memory_integration_create(const nimcp_memory_integration_config_t* config) {
    nimcp_memory_integration_bridge_t bridge = (nimcp_memory_integration_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_memory_integration_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate memory-integration bridge");
    bridge->config = config ? *config : nimcp_memory_integration_default_config();
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.gw_entry_level = 0.5f;
    bridge->state.encoding_gate_level = 0.5f;
    NIMCP_LOGGING_INFO("Created %s bridge", "memory_integration");
    return bridge;
}

void nimcp_memory_integration_destroy(nimcp_memory_integration_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "memory_integration");
    if (bridge->is_initialized) nimcp_memory_integration_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_memory_integration_init(
    nimcp_memory_integration_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_memory_intra_t memory,
    nimcp_integration_intra_t integration
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_integration_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in memory_integration_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in memory_integration_init");
    bridge->registry = registry;
    bridge->memory = memory;
    bridge->integration = integration;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_integration_shutdown(nimcp_memory_integration_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_integration_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in memory_integration_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_integration_update(nimcp_memory_integration_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_integration_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in memory_integration_update");

    /* Memory retrieval entry to global workspace */
    bridge->state.gw_entry_level *= (1.0f - dt * 0.01f);

    /* Conscious recall strength */
    bridge->state.conscious_recall_strength *= (1.0f - dt * 0.02f);
    bridge->state.conscious_recall_strength += dt * bridge->config.conscious_recall_coupling * 0.02f;

    /* Arousal modulation */
    bridge->state.arousal_modulation *= (1.0f - dt * 0.015f);

    /* Consolidation trigger */
    bridge->state.consolidation_trigger *= (1.0f - dt * 0.03f);

    /* Encoding gate level */
    bridge->state.encoding_gate_level *= (1.0f - dt * 0.02f);
    bridge->state.encoding_gate_level += dt * bridge->config.encoding_gate_coupling * 0.02f;

    /* Retrieval priority decay */
    bridge->state.retrieval_priority *= (1.0f - dt * 0.025f);

    /* Update averages */
    bridge->stats.avg_conscious_access = bridge->stats.avg_conscious_access * NIMCP_EMA_DECAY_DEFAULT + bridge->state.conscious_recall_strength * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_consolidation_rate = bridge->stats.avg_consolidation_rate * NIMCP_EMA_DECAY_DEFAULT + bridge->state.consolidation_trigger * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_integration_transfer_bottom_up(nimcp_memory_integration_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_integration_transfer_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in memory_integration_transfer_bottom_up");
    bridge->state.bottom_up_messages++;
    bridge->stats.gw_entries++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_integration_transfer_top_down(nimcp_memory_integration_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_integration_transfer_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in memory_integration_transfer_top_down");
    bridge->state.top_down_messages++;
    bridge->stats.consolidation_triggers++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_integration_get_state(nimcp_memory_integration_bridge_t bridge, nimcp_memory_integration_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_integration_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in memory_integration_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_integration_get_stats(nimcp_memory_integration_bridge_t bridge, nimcp_memory_integration_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_integration_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in memory_integration_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_memory_integration_get_coherence(nimcp_memory_integration_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_memory_integration_reset_stats(nimcp_memory_integration_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in memory_integration_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
