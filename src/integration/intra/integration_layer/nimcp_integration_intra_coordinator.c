/**
 * @file nimcp_integration_intra_coordinator.c
 * @brief Integration Layer Intra-Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/intra/integration_layer/nimcp_integration_intra_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_neural_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(integration_intra_coordinator)

typedef struct {
    void* module;
    nimcp_module_interface_t interface;
    bool connected;
} module_slot_t;

struct nimcp_integration_intra_struct {
    nimcp_integration_intra_config_t config;
    nimcp_layer_registry_t registry;
    module_slot_t claustrum;
    module_slot_t pag;
    module_slot_t red_nucleus;
    module_slot_t reticular;
    nimcp_integration_intra_state_t state;
    nimcp_integration_intra_stats_t stats;
    bool is_initialized;
};

nimcp_integration_intra_config_t nimcp_integration_intra_default_config(void) {
    nimcp_integration_intra_config_t config = {
        .enable_claustrum = true,
        .enable_pag = true,
        .enable_red_nucleus = true,
        .enable_reticular = true,
        .claustrum_pag_coupling = 0.5f,
        .claustrum_red_coupling = 0.3f,
        .claustrum_reticular_coupling = 0.6f,
        .pag_red_coupling = 0.4f,
        .pag_reticular_coupling = 0.5f,
        .red_reticular_coupling = 0.4f,
        .sync_interval_ms = 10,
        .coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD,
        .enable_global_binding = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_integration_intra_t nimcp_integration_intra_create(const nimcp_integration_intra_config_t* config) {
    nimcp_integration_intra_t coord = (nimcp_integration_intra_t)nimcp_calloc(1, sizeof(struct nimcp_integration_intra_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate integration intra coordinator");
    coord->config = config ? *config : nimcp_integration_intra_default_config();
    coord->state.arousal_level = 0.5f;  /* Baseline arousal */
    return coord;
}

void nimcp_integration_intra_destroy(nimcp_integration_intra_t coord) {
    if (!coord) return;
    if (coord->is_initialized) nimcp_integration_intra_shutdown(coord);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_integration_intra_init(nimcp_integration_intra_t coord, nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL");
    if (coord->is_initialized) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;
    coord->registry = registry;
    coord->is_initialized = true;
    coord->state.layer_coherence = 1.0f;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_shutdown(nimcp_integration_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_connect_claustrum(nimcp_integration_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->claustrum.module = module;
    coord->claustrum.interface = *interface;
    coord->claustrum.connected = true;
    coord->state.claustrum_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_connect_pag(nimcp_integration_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->pag.module = module;
    coord->pag.interface = *interface;
    coord->pag.connected = true;
    coord->state.pag_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_connect_red_nucleus(nimcp_integration_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->red_nucleus.module = module;
    coord->red_nucleus.interface = *interface;
    coord->red_nucleus.connected = true;
    coord->state.red_nucleus_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_connect_reticular(nimcp_integration_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->reticular.module = module;
    coord->reticular.interface = *interface;
    coord->reticular.connected = true;
    coord->state.reticular_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_update(nimcp_integration_intra_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Arousal tends toward baseline */
    coord->state.arousal_level += (0.5f - coord->state.arousal_level) * dt * 0.1f;

    /* Global binding strength from claustrum */
    if (coord->state.claustrum_active) {
        coord->state.binding_strength = 0.8f * coord->state.global_coherence;
    }

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_sync(nimcp_integration_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_send(nimcp_integration_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    coord->stats.messages_sent++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_broadcast(nimcp_integration_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    coord->stats.messages_sent += 4;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_get_state(nimcp_integration_intra_t coord, nimcp_integration_intra_state_t* state_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL");
    *state_out = coord->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_integration_intra_get_stats(nimcp_integration_intra_t coord, nimcp_integration_intra_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_integration_intra_get_coherence(nimcp_integration_intra_t coord) {
    return coord ? coord->state.layer_coherence : 0.0f;
}

nimcp_layer_error_t nimcp_integration_intra_reset_stats(nimcp_integration_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    memset(&coord->stats, 0, sizeof(coord->stats));
    return NIMCP_LAYER_OK;
}
