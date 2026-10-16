/**
 * @file nimcp_superhuman_intra_coordinator.c
 * @brief Superhuman Layer Intra-Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/intra/superhuman/nimcp_superhuman_intra_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_neural_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(superhuman_intra_coordinator)

typedef struct {
    void* module;
    nimcp_module_interface_t interface;
    bool connected;
} module_slot_t;

struct nimcp_superhuman_intra_struct {
    nimcp_superhuman_intra_config_t config;
    nimcp_layer_registry_t registry;
    module_slot_t eagle_vision;
    module_slot_t echolocation;
    module_slot_t time_dilation;
    module_slot_t magnetoreception;
    module_slot_t electroreception;
    module_slot_t infrared;
    module_slot_t ultraviolet;
    nimcp_superhuman_intra_state_t state;
    nimcp_superhuman_intra_stats_t stats;
    bool is_initialized;
};

nimcp_superhuman_intra_config_t nimcp_superhuman_intra_default_config(void) {
    nimcp_superhuman_intra_config_t config = {
        .enable_eagle_vision = true,
        .enable_echolocation = true,
        .enable_time_dilation = true,
        .enable_magnetoreception = true,
        .enable_electroreception = true,
        .enable_infrared = true,
        .enable_ultraviolet = true,
        .vision_echo_coupling = 0.5f,
        .vision_infrared_coupling = 0.7f,
        .vision_uv_coupling = 0.6f,
        .echo_electroreception_coupling = 0.8f,
        .time_all_coupling = 1.0f,  /* Time affects everything */
        .sync_interval_ms = 10,
        .coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD,
        .enable_capability_blending = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_superhuman_intra_t nimcp_superhuman_intra_create(const nimcp_superhuman_intra_config_t* config) {
    nimcp_superhuman_intra_t coord = (nimcp_superhuman_intra_t)nimcp_calloc(1, sizeof(struct nimcp_superhuman_intra_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate superhuman intra coordinator");
    coord->config = config ? *config : nimcp_superhuman_intra_default_config();
    coord->state.time_scale_factor = 1.0f;  /* Normal time */
    return coord;
}

void nimcp_superhuman_intra_destroy(nimcp_superhuman_intra_t coord) {
    if (!coord) return;
    if (coord->is_initialized) nimcp_superhuman_intra_shutdown(coord);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_superhuman_intra_init(nimcp_superhuman_intra_t coord, nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL");
    if (coord->is_initialized) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;
    coord->registry = registry;
    coord->is_initialized = true;
    coord->state.layer_coherence = 1.0f;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_shutdown(nimcp_superhuman_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_connect_eagle_vision(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->eagle_vision.module = module;
    coord->eagle_vision.interface = *interface;
    coord->eagle_vision.connected = true;
    coord->state.eagle_vision_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_connect_echolocation(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->echolocation.module = module;
    coord->echolocation.interface = *interface;
    coord->echolocation.connected = true;
    coord->state.echolocation_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_connect_time_dilation(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->time_dilation.module = module;
    coord->time_dilation.interface = *interface;
    coord->time_dilation.connected = true;
    coord->state.time_dilation_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_connect_magnetoreception(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->magnetoreception.module = module;
    coord->magnetoreception.interface = *interface;
    coord->magnetoreception.connected = true;
    coord->state.magnetoreception_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_connect_electroreception(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->electroreception.module = module;
    coord->electroreception.interface = *interface;
    coord->electroreception.connected = true;
    coord->state.electroreception_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_connect_infrared(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->infrared.module = module;
    coord->infrared.interface = *interface;
    coord->infrared.connected = true;
    coord->state.infrared_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_connect_ultraviolet(nimcp_superhuman_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->ultraviolet.module = module;
    coord->ultraviolet.interface = *interface;
    coord->ultraviolet.connected = true;
    coord->state.ultraviolet_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_update(nimcp_superhuman_intra_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Time dilation returns to normal */
    if (coord->state.time_dilation_active) {
        coord->state.time_scale_factor += (1.0f - coord->state.time_scale_factor) * dt * 0.5f;
    }

    /* Capability blending */
    if (coord->config.enable_capability_blending) {
        int active_count = 0;
        if (coord->state.eagle_vision_active) active_count++;
        if (coord->state.echolocation_active) active_count++;
        if (coord->state.infrared_active) active_count++;
        if (coord->state.ultraviolet_active) active_count++;
        if (coord->state.magnetoreception_active) active_count++;
        if (coord->state.electroreception_active) active_count++;
        coord->state.capability_blend_strength = active_count > 1 ? (float)active_count / 7.0f : 0.0f;
    }

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_sync(nimcp_superhuman_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_send(nimcp_superhuman_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    coord->stats.messages_sent++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_broadcast(nimcp_superhuman_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    coord->stats.messages_sent += 7;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_get_state(nimcp_superhuman_intra_t coord, nimcp_superhuman_intra_state_t* state_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL");
    *state_out = coord->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_superhuman_intra_get_stats(nimcp_superhuman_intra_t coord, nimcp_superhuman_intra_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_superhuman_intra_get_coherence(nimcp_superhuman_intra_t coord) {
    return coord ? coord->state.layer_coherence : 0.0f;
}

nimcp_layer_error_t nimcp_superhuman_intra_reset_stats(nimcp_superhuman_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    memset(&coord->stats, 0, sizeof(coord->stats));
    return NIMCP_LAYER_OK;
}
