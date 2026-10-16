/**
 * @file nimcp_sensory_intra_coordinator.c
 * @brief Sensory Layer Intra-Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/intra/sensory/nimcp_sensory_intra_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sensory_intra_coordinator)

typedef struct {
    void* module;
    nimcp_module_interface_t interface;
    bool connected;
} module_slot_t;

struct nimcp_sensory_intra_struct {
    nimcp_sensory_intra_config_t config;
    nimcp_layer_registry_t registry;
    module_slot_t somatosensory;
    module_slot_t olfactory;
    module_slot_t gustatory;
    nimcp_sensory_intra_state_t state;
    nimcp_sensory_intra_stats_t stats;
    bool is_initialized;
};

nimcp_sensory_intra_config_t nimcp_sensory_intra_default_config(void) {
    nimcp_sensory_intra_config_t config = {
        .enable_somatosensory = true,
        .enable_olfactory = true,
        .enable_gustatory = true,
        .somato_olfactory_coupling = 0.3f,
        .somato_gustatory_coupling = 0.4f,
        .olfactory_gustatory_coupling = 0.7f,
        .sync_interval_ms = 10,
        .coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD,
        .enable_crossmodal_binding = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_sensory_intra_t nimcp_sensory_intra_create(const nimcp_sensory_intra_config_t* config) {
    nimcp_sensory_intra_t coord = (nimcp_sensory_intra_t)nimcp_calloc(1, sizeof(struct nimcp_sensory_intra_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate sensory intra coordinator");
    coord->config = config ? *config : nimcp_sensory_intra_default_config();
    return coord;
}

void nimcp_sensory_intra_destroy(nimcp_sensory_intra_t coord) {
    if (!coord) return;
    if (coord->is_initialized) nimcp_sensory_intra_shutdown(coord);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_sensory_intra_init(nimcp_sensory_intra_t coord, nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL");
    if (coord->is_initialized) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;
    coord->registry = registry;
    coord->is_initialized = true;
    coord->state.layer_coherence = 1.0f;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_shutdown(nimcp_sensory_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_connect_somatosensory(nimcp_sensory_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->somatosensory.module = module;
    coord->somatosensory.interface = *interface;
    coord->somatosensory.connected = true;
    coord->state.somatosensory_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_connect_olfactory(nimcp_sensory_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->olfactory.module = module;
    coord->olfactory.interface = *interface;
    coord->olfactory.connected = true;
    coord->state.olfactory_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_connect_gustatory(nimcp_sensory_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->gustatory.module = module;
    coord->gustatory.interface = *interface;
    coord->gustatory.connected = true;
    coord->state.gustatory_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_update(nimcp_sensory_intra_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Sensory decay */
    coord->state.touch_intensity *= (1.0f - dt * 0.1f);
    coord->state.smell_intensity *= (1.0f - dt * 0.05f);
    coord->state.taste_intensity *= (1.0f - dt * 0.08f);

    /* Update crossmodal binding if enabled */
    if (coord->config.enable_crossmodal_binding) {
        float avg_intensity = (coord->state.touch_intensity + coord->state.smell_intensity + coord->state.taste_intensity) / 3.0f;
        coord->state.crossmodal_binding_strength = avg_intensity * coord->state.layer_coherence;
    }

    coord->stats.avg_coherence = coord->stats.avg_coherence * NIMCP_EMA_DECAY_DEFAULT + coord->state.layer_coherence * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_sync(nimcp_sensory_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_send(nimcp_sensory_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)target_module;
    coord->stats.messages_sent++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_broadcast(nimcp_sensory_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)source_module;
    coord->stats.messages_sent += SENSORY_MODULE_COUNT;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_get_state(nimcp_sensory_intra_t coord, nimcp_sensory_intra_state_t* state_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL");
    *state_out = coord->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_sensory_intra_get_stats(nimcp_sensory_intra_t coord, nimcp_sensory_intra_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_sensory_intra_get_coherence(nimcp_sensory_intra_t coord) {
    return coord ? coord->state.layer_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_sensory_intra_reset_stats(nimcp_sensory_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    memset(&coord->stats, 0, sizeof(coord->stats));
    return NIMCP_LAYER_OK;
}
