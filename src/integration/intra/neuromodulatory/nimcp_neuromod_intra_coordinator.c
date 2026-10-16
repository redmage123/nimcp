/**
 * @file nimcp_neuromod_intra_coordinator.c
 * @brief Neuromodulatory Layer Intra-Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/intra/neuromodulatory/nimcp_neuromod_intra_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_intra_coordinator)

typedef struct {
    void* module;
    nimcp_module_interface_t interface;
    bool connected;
} module_slot_t;

struct nimcp_neuromod_intra_struct {
    nimcp_neuromod_intra_config_t config;
    nimcp_layer_registry_t registry;
    module_slot_t lc;
    module_slot_t vta;
    module_slot_t raphe;
    module_slot_t habenula;
    nimcp_neuromod_intra_state_t state;
    nimcp_neuromod_intra_stats_t stats;
    bool is_initialized;
};

nimcp_neuromod_intra_config_t nimcp_neuromod_intra_default_config(void) {
    nimcp_neuromod_intra_config_t config = {
        .enable_lc = true,
        .enable_vta = true,
        .enable_raphe = true,
        .enable_habenula = true,
        .lc_vta_coupling = 0.5f,
        .lc_raphe_coupling = 0.4f,
        .lc_habenula_coupling = 0.3f,
        .vta_raphe_coupling = 0.4f,
        .vta_habenula_coupling = 0.5f,
        .raphe_habenula_coupling = 0.3f,
        .sync_interval_ms = 10,
        .coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_neuromod_intra_t nimcp_neuromod_intra_create(const nimcp_neuromod_intra_config_t* config) {
    nimcp_neuromod_intra_t coord = (nimcp_neuromod_intra_t)nimcp_calloc(1, sizeof(struct nimcp_neuromod_intra_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate neuromod intra coordinator");
    coord->config = config ? *config : nimcp_neuromod_intra_default_config();
    coord->state.norepinephrine_level = NIMCP_NOREPINEPHRINE_BASELINE;
    coord->state.dopamine_level = NIMCP_DOPAMINE_BASELINE;
    coord->state.serotonin_level = NIMCP_SEROTONIN_BASELINE;
    return coord;
}

void nimcp_neuromod_intra_destroy(nimcp_neuromod_intra_t coord) {
    if (!coord) return;
    if (coord->is_initialized) nimcp_neuromod_intra_shutdown(coord);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_neuromod_intra_init(nimcp_neuromod_intra_t coord, nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL");
    if (coord->is_initialized) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;
    coord->registry = registry;
    coord->is_initialized = true;
    coord->state.layer_coherence = 1.0f;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_shutdown(nimcp_neuromod_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_connect_lc(nimcp_neuromod_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->lc.module = module;
    coord->lc.interface = *interface;
    coord->lc.connected = true;
    coord->state.lc_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_connect_vta(nimcp_neuromod_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->vta.module = module;
    coord->vta.interface = *interface;
    coord->vta.connected = true;
    coord->state.vta_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_connect_raphe(nimcp_neuromod_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->raphe.module = module;
    coord->raphe.interface = *interface;
    coord->raphe.connected = true;
    coord->state.raphe_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_connect_habenula(nimcp_neuromod_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->habenula.module = module;
    coord->habenula.interface = *interface;
    coord->habenula.connected = true;
    coord->state.habenula_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_update(nimcp_neuromod_intra_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Homeostatic regulation of neurotransmitter levels */
    coord->state.norepinephrine_level += (0.5f - coord->state.norepinephrine_level) * dt * 0.05f;
    coord->state.dopamine_level += (0.5f - coord->state.dopamine_level) * dt * 0.05f;
    coord->state.serotonin_level += (0.5f - coord->state.serotonin_level) * dt * 0.05f;

    /* Update stats */
    coord->stats.avg_arousal = coord->stats.avg_arousal * NIMCP_EMA_DECAY_DEFAULT + coord->state.arousal_level * NIMCP_LEARNING_RATE_DEFAULT;
    coord->stats.avg_dopamine = coord->stats.avg_dopamine * NIMCP_EMA_DECAY_DEFAULT + coord->state.dopamine_level * NIMCP_LEARNING_RATE_DEFAULT;
    coord->stats.avg_coherence = coord->stats.avg_coherence * NIMCP_EMA_DECAY_DEFAULT + coord->state.layer_coherence * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_sync(nimcp_neuromod_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_send(nimcp_neuromod_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)target_module;
    coord->stats.messages_sent++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_broadcast(nimcp_neuromod_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)source_module;
    coord->stats.messages_sent += NEUROMOD_MODULE_COUNT;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_get_state(nimcp_neuromod_intra_t coord, nimcp_neuromod_intra_state_t* state_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL");
    *state_out = coord->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_neuromod_intra_get_stats(nimcp_neuromod_intra_t coord, nimcp_neuromod_intra_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_neuromod_intra_get_coherence(nimcp_neuromod_intra_t coord) {
    return coord ? coord->state.layer_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_neuromod_intra_reset_stats(nimcp_neuromod_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    memset(&coord->stats, 0, sizeof(coord->stats));
    return NIMCP_LAYER_OK;
}
