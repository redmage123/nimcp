/**
 * @file nimcp_memory_intra_coordinator.c
 * @brief Memory Layer Intra-Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/intra/memory/nimcp_memory_intra_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(memory_intra_coordinator)

typedef struct {
    void* module;
    nimcp_module_interface_t interface;
    bool connected;
} module_slot_t;

struct nimcp_memory_intra_struct {
    nimcp_memory_intra_config_t config;
    nimcp_layer_registry_t registry;
    module_slot_t entorhinal;
    module_slot_t perirhinal;
    module_slot_t parahippocampal;
    module_slot_t mammillary;
    nimcp_memory_intra_state_t state;
    nimcp_memory_intra_stats_t stats;
    bool is_initialized;
};

nimcp_memory_intra_config_t nimcp_memory_intra_default_config(void) {
    nimcp_memory_intra_config_t config = {
        .enable_entorhinal = true,
        .enable_perirhinal = true,
        .enable_parahippocampal = true,
        .enable_mammillary = true,
        .ec_prc_coupling = 0.6f,
        .ec_phc_coupling = 0.6f,
        .ec_mb_coupling = 0.4f,
        .prc_phc_coupling = 0.5f,
        .prc_mb_coupling = 0.3f,
        .phc_mb_coupling = 0.4f,
        .sync_interval_ms = 10,
        .coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD,
        .enable_theta_gating = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_memory_intra_t nimcp_memory_intra_create(const nimcp_memory_intra_config_t* config) {
    nimcp_memory_intra_t coord = (nimcp_memory_intra_t)nimcp_calloc(1, sizeof(struct nimcp_memory_intra_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate memory intra coordinator");
    coord->config = config ? *config : nimcp_memory_intra_default_config();
    return coord;
}

void nimcp_memory_intra_destroy(nimcp_memory_intra_t coord) {
    if (!coord) return;
    if (coord->is_initialized) nimcp_memory_intra_shutdown(coord);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_memory_intra_init(nimcp_memory_intra_t coord, nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL");
    if (coord->is_initialized) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;
    coord->registry = registry;
    coord->is_initialized = true;
    coord->state.layer_coherence = 1.0f;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_shutdown(nimcp_memory_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_connect_entorhinal(nimcp_memory_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->entorhinal.module = module;
    coord->entorhinal.interface = *interface;
    coord->entorhinal.connected = true;
    coord->state.entorhinal_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_connect_perirhinal(nimcp_memory_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->perirhinal.module = module;
    coord->perirhinal.interface = *interface;
    coord->perirhinal.connected = true;
    coord->state.perirhinal_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_connect_parahippocampal(nimcp_memory_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->parahippocampal.module = module;
    coord->parahippocampal.interface = *interface;
    coord->parahippocampal.connected = true;
    coord->state.parahippocampal_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_connect_mammillary(nimcp_memory_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL");
    coord->mammillary.module = module;
    coord->mammillary.interface = *interface;
    coord->mammillary.connected = true;
    coord->state.mammillary_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_update(nimcp_memory_intra_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Update theta oscillation (4-8 Hz) */
    if (coord->config.enable_theta_gating) {
        coord->state.theta_phase += dt * 6.0f * NIMCP_TWO_PI_F; /* ~6 Hz theta */
        if (coord->state.theta_phase > NIMCP_TWO_PI_F) {
            coord->state.theta_phase -= NIMCP_TWO_PI_F;
            coord->stats.theta_cycles++;
        }
    }

    /* Encoding vs retrieval depends on theta phase */
    float theta_sin = sinf(coord->state.theta_phase);
    coord->state.encoding_strength = (theta_sin > 0) ? theta_sin : 0.0f;
    coord->state.retrieval_strength = (theta_sin < 0) ? -theta_sin : 0.0f;

    /* Update stats */
    coord->stats.avg_encoding_strength = coord->stats.avg_encoding_strength * NIMCP_EMA_DECAY_DEFAULT + coord->state.encoding_strength * NIMCP_LEARNING_RATE_DEFAULT;
    coord->stats.avg_coherence = coord->stats.avg_coherence * NIMCP_EMA_DECAY_DEFAULT + coord->state.layer_coherence * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_sync(nimcp_memory_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_send(nimcp_memory_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)target_module;
    coord->stats.messages_sent++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_broadcast(nimcp_memory_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL");
    (void)source_module;
    coord->stats.messages_sent += MEMORY_MODULE_COUNT;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_get_state(nimcp_memory_intra_t coord, nimcp_memory_intra_state_t* state_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL");
    *state_out = coord->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_memory_intra_get_stats(nimcp_memory_intra_t coord, nimcp_memory_intra_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_memory_intra_get_coherence(nimcp_memory_intra_t coord) {
    return coord ? coord->state.layer_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_memory_intra_reset_stats(nimcp_memory_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL");
    memset(&coord->stats, 0, sizeof(coord->stats));
    return NIMCP_LAYER_OK;
}
