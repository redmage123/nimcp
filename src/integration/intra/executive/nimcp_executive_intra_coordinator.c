/**
 * @file nimcp_executive_intra_coordinator.c
 * @brief Executive Layer Intra-Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/intra/executive/nimcp_executive_intra_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(executive_intra_coordinator)

typedef struct {
    void* module;
    nimcp_module_interface_t interface;
    bool connected;
} module_slot_t;

struct nimcp_executive_intra_struct {
    nimcp_executive_intra_config_t config;
    nimcp_layer_registry_t registry;
    module_slot_t ofc;
    module_slot_t retrosplenial;
    module_slot_t pfc;
    nimcp_executive_intra_state_t state;
    nimcp_executive_intra_stats_t stats;
    bool is_initialized;
};

nimcp_executive_intra_config_t nimcp_executive_intra_default_config(void) {
    nimcp_executive_intra_config_t config = {
        .enable_ofc = true,
        .enable_retrosplenial = true,
        .enable_pfc = true,
        .ofc_retro_coupling = 0.4f,
        .ofc_pfc_coupling = 0.6f,
        .retro_pfc_coupling = 0.5f,
        .sync_interval_ms = 10,
        .coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD,
        .enable_goal_maintenance = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_executive_intra_t nimcp_executive_intra_create(const nimcp_executive_intra_config_t* config) {
    nimcp_executive_intra_t coord = (nimcp_executive_intra_t)nimcp_calloc(1, sizeof(struct nimcp_executive_intra_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate executive intra coordinator");
    coord->config = config ? *config : nimcp_executive_intra_default_config();
    return coord;
}

void nimcp_executive_intra_destroy(nimcp_executive_intra_t coord) {
    if (!coord) return;
    if (coord->is_initialized) nimcp_executive_intra_shutdown(coord);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_executive_intra_init(nimcp_executive_intra_t coord, nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL");
    if (coord->is_initialized) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;
    coord->registry = registry;
    coord->is_initialized = true;
    coord->state.layer_coherence = 1.0f;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_shutdown(nimcp_executive_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_connect_ofc(nimcp_executive_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->ofc.module = module;
    coord->ofc.interface = *interface;
    coord->ofc.connected = true;
    coord->state.ofc_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_connect_retrosplenial(nimcp_executive_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->retrosplenial.module = module;
    coord->retrosplenial.interface = *interface;
    coord->retrosplenial.connected = true;
    coord->state.retrosplenial_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_connect_pfc(nimcp_executive_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->pfc.module = module;
    coord->pfc.interface = *interface;
    coord->pfc.connected = true;
    coord->state.pfc_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_update(nimcp_executive_intra_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Goal maintenance - goals decay without reinforcement */
    if (coord->config.enable_goal_maintenance) {
        coord->state.goal_strength *= (1.0f - dt * 0.01f);
    }

    /* Inhibition decays */
    coord->state.inhibition_level *= (1.0f - dt * 0.1f);

    /* Planning progress toward completion */
    if (coord->state.planning_progress > 0.0f && coord->state.planning_progress < 1.0f) {
        coord->state.planning_progress += dt * 0.1f;
        if (coord->state.planning_progress > 1.0f) coord->state.planning_progress = 1.0f;
    }

    /* Update stats */
    coord->stats.avg_coherence = coord->stats.avg_coherence * NIMCP_EMA_DECAY_DEFAULT + coord->state.layer_coherence * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_sync(nimcp_executive_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_send(nimcp_executive_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)target_module;
    coord->stats.messages_sent++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_broadcast(nimcp_executive_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)source_module;
    coord->stats.messages_sent += EXECUTIVE_MODULE_COUNT;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_get_state(nimcp_executive_intra_t coord, nimcp_executive_intra_state_t* state_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL");
    *state_out = coord->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_executive_intra_get_stats(nimcp_executive_intra_t coord, nimcp_executive_intra_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_executive_intra_get_coherence(nimcp_executive_intra_t coord) {
    return coord ? coord->state.layer_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_executive_intra_reset_stats(nimcp_executive_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    memset(&coord->stats, 0, sizeof(coord->stats));
    return NIMCP_LAYER_OK;
}
