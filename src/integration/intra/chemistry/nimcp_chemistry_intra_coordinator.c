/**
 * @file nimcp_chemistry_intra_coordinator.c
 * @brief Chemistry Layer Intra-Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/intra/chemistry/nimcp_chemistry_intra_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(chemistry_intra_coordinator)

typedef struct {
    void* module;
    nimcp_module_interface_t interface;
    bool connected;
} module_slot_t;

struct nimcp_chemistry_intra_struct {
    nimcp_chemistry_intra_config_t config;
    nimcp_layer_registry_t registry;
    module_slot_t ph;
    module_slot_t no_signaling;
    module_slot_t neurovascular;
    nimcp_chemistry_intra_state_t state;
    nimcp_chemistry_intra_stats_t stats;
    bool is_initialized;
};

nimcp_chemistry_intra_config_t nimcp_chemistry_intra_default_config(void) {
    nimcp_chemistry_intra_config_t config = {
        .enable_ph = true,
        .enable_no_signaling = true,
        .enable_neurovascular = true,
        .ph_no_coupling = 0.5f,
        .ph_vascular_coupling = 0.6f,
        .no_vascular_coupling = 0.7f,
        .sync_interval_ms = 10,
        .coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD,
        .baseline_ph = 7.4f,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_chemistry_intra_t nimcp_chemistry_intra_create(const nimcp_chemistry_intra_config_t* config) {
    nimcp_chemistry_intra_t coord = (nimcp_chemistry_intra_t)nimcp_calloc(1, sizeof(struct nimcp_chemistry_intra_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate chemistry intra coordinator");
    coord->config = config ? *config : nimcp_chemistry_intra_default_config();
    coord->state.current_ph = coord->config.baseline_ph;
    return coord;
}

void nimcp_chemistry_intra_destroy(nimcp_chemistry_intra_t coord) {
    if (!coord) return;
    if (coord->is_initialized) nimcp_chemistry_intra_shutdown(coord);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_chemistry_intra_init(nimcp_chemistry_intra_t coord, nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL");
    if (coord->is_initialized) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;
    coord->registry = registry;
    coord->is_initialized = true;
    coord->state.layer_coherence = 1.0f;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_shutdown(nimcp_chemistry_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_connect_ph(nimcp_chemistry_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->ph.module = module;
    coord->ph.interface = *interface;
    coord->ph.connected = true;
    coord->state.ph_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_connect_no_signaling(nimcp_chemistry_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->no_signaling.module = module;
    coord->no_signaling.interface = *interface;
    coord->no_signaling.connected = true;
    coord->state.no_signaling_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_connect_neurovascular(nimcp_chemistry_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->neurovascular.module = module;
    coord->neurovascular.interface = *interface;
    coord->neurovascular.connected = true;
    coord->state.neurovascular_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_update(nimcp_chemistry_intra_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* pH homeostasis */
    coord->state.current_ph += (coord->config.baseline_ph - coord->state.current_ph) * dt * 0.1f;
    coord->stats.avg_ph = coord->stats.avg_ph * NIMCP_EMA_DECAY_DEFAULT + coord->state.current_ph * NIMCP_LEARNING_RATE_DEFAULT;
    coord->stats.avg_no_concentration = coord->stats.avg_no_concentration * NIMCP_EMA_DECAY_DEFAULT + coord->state.no_concentration * NIMCP_LEARNING_RATE_DEFAULT;
    coord->stats.avg_coherence = coord->stats.avg_coherence * NIMCP_EMA_DECAY_DEFAULT + coord->state.layer_coherence * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_sync(nimcp_chemistry_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_send(nimcp_chemistry_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)target_module;
    coord->stats.messages_sent++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_broadcast(nimcp_chemistry_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)source_module;
    coord->stats.messages_sent += CHEMISTRY_MODULE_COUNT;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_get_state(nimcp_chemistry_intra_t coord, nimcp_chemistry_intra_state_t* state_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL");
    *state_out = coord->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_chemistry_intra_get_stats(nimcp_chemistry_intra_t coord, nimcp_chemistry_intra_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_chemistry_intra_get_coherence(nimcp_chemistry_intra_t coord) {
    return coord ? coord->state.layer_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_chemistry_intra_reset_stats(nimcp_chemistry_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    memset(&coord->stats, 0, sizeof(coord->stats));
    return NIMCP_LAYER_OK;
}
