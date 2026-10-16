/**
 * @file nimcp_physics_intra_coordinator.c
 * @brief Physics Layer Intra-Layer Coordinator Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "integration/intra/physics/nimcp_physics_intra_coordinator.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_intra_coordinator)

typedef struct {
    void* module;
    nimcp_module_interface_t interface;
    bool connected;
} module_slot_t;

struct nimcp_physics_intra_struct {
    nimcp_physics_intra_config_t config;
    nimcp_layer_registry_t registry;
    module_slot_t ephaptic;
    module_slot_t info_geometry;
    module_slot_t hh_dynamics;
    module_slot_t thermodynamics;
    nimcp_physics_intra_state_t state;
    nimcp_physics_intra_stats_t stats;
    bool is_initialized;
};

nimcp_physics_intra_config_t nimcp_physics_intra_default_config(void) {
    nimcp_physics_intra_config_t config = {
        .enable_ephaptic = true,
        .enable_info_geometry = true,
        .enable_hh_dynamics = true,
        .enable_thermodynamics = true,
        .ephaptic_hh_coupling = 0.5f,
        .ephaptic_geometry_coupling = 0.3f,
        .hh_thermo_coupling = 0.4f,
        .geometry_thermo_coupling = 0.3f,
        .sync_interval_ms = 10,
        .coherence_threshold = NIMCP_PHASE_COHERENCE_THRESHOLD,
        .enforce_energy_conservation = true,
        .enforce_entropy_increase = true,
        .temperature_kelvin = 310.0f,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_physics_intra_t nimcp_physics_intra_create(const nimcp_physics_intra_config_t* config) {
    nimcp_physics_intra_t coord = (nimcp_physics_intra_t)nimcp_calloc(1, sizeof(struct nimcp_physics_intra_struct));
    NIMCP_API_CHECK_ALLOC(coord, "Failed to allocate physics intra coordinator");
    coord->config = config ? *config : nimcp_physics_intra_default_config();
    coord->state.temperature = coord->config.temperature_kelvin;
    return coord;
}

void nimcp_physics_intra_destroy(nimcp_physics_intra_t coord) {
    if (!coord) return;
    if (coord->is_initialized) nimcp_physics_intra_shutdown(coord);
    nimcp_free(coord);
}

nimcp_layer_error_t nimcp_physics_intra_init(nimcp_physics_intra_t coord, nimcp_layer_registry_t registry) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in physics_intra_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in physics_intra_init");
    if (coord->is_initialized) return NIMCP_LAYER_ERR_ALREADY_REGISTERED;
    coord->registry = registry;
    coord->is_initialized = true;
    coord->state.layer_coherence = 1.0f;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_shutdown(nimcp_physics_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in physics_intra_shutdown");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_connect_ephaptic(nimcp_physics_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in connect_ephaptic");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL in connect_ephaptic");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL in connect_ephaptic");
    coord->ephaptic.module = module;
    coord->ephaptic.interface = *interface;
    coord->ephaptic.connected = true;
    coord->state.ephaptic_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_connect_info_geometry(nimcp_physics_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in connect_info_geometry");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL in connect_info_geometry");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL in connect_info_geometry");
    coord->info_geometry.module = module;
    coord->info_geometry.interface = *interface;
    coord->info_geometry.connected = true;
    coord->state.info_geometry_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_connect_hh_dynamics(nimcp_physics_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in connect_hh_dynamics");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL in connect_hh_dynamics");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL in connect_hh_dynamics");
    coord->hh_dynamics.module = module;
    coord->hh_dynamics.interface = *interface;
    coord->hh_dynamics.connected = true;
    coord->state.hh_dynamics_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_connect_thermodynamics(nimcp_physics_intra_t coord, void* module, nimcp_module_interface_t* interface) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in connect_thermodynamics");
    NIMCP_API_CHECK_NULL(module, NIMCP_LAYER_ERR_NULL_PTR, "Module is NULL in connect_thermodynamics");
    NIMCP_API_CHECK_NULL(interface, NIMCP_LAYER_ERR_NULL_PTR, "Interface is NULL in connect_thermodynamics");
    coord->thermodynamics.module = module;
    coord->thermodynamics.interface = *interface;
    coord->thermodynamics.connected = true;
    coord->state.thermodynamics_active = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_update(nimcp_physics_intra_t coord, float dt) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in physics_intra_update");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;

    /* Accumulate energy/entropy stats */
    coord->stats.avg_energy = coord->stats.avg_energy * NIMCP_EMA_DECAY_DEFAULT + coord->state.total_energy * NIMCP_LEARNING_RATE_DEFAULT;
    coord->stats.avg_entropy = coord->stats.avg_entropy * NIMCP_EMA_DECAY_DEFAULT + coord->state.entropy * NIMCP_LEARNING_RATE_DEFAULT;
    coord->stats.avg_field_magnitude = coord->stats.avg_field_magnitude * NIMCP_EMA_DECAY_DEFAULT + coord->state.field_magnitude * NIMCP_LEARNING_RATE_DEFAULT;

    /* Update coherences */
    float sum = 0.0f;
    int count = 0;
    for (int i = 0; i < PHYSICS_MODULE_COUNT; i++) {
        sum += coord->state.module_coherences[i];
        count++;
    }
    coord->state.layer_coherence = count > 0 ? sum / count : 1.0f;
    coord->stats.avg_coherence = coord->stats.avg_coherence * NIMCP_EMA_DECAY_DEFAULT + coord->state.layer_coherence * NIMCP_LEARNING_RATE_DEFAULT;

    (void)dt;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_sync(nimcp_physics_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in physics_intra_sync");
    if (!coord->is_initialized) return NIMCP_LAYER_ERR_NOT_INITIALIZED;
    coord->stats.sync_events++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_send(nimcp_physics_intra_t coord, uint32_t target_module, nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in physics_intra_send");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in physics_intra_send");
    (void)target_module;
    coord->stats.messages_sent++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_broadcast(nimcp_physics_intra_t coord, uint32_t source_module, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in physics_intra_broadcast");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in physics_intra_broadcast");
    (void)source_module;
    coord->stats.messages_sent += PHYSICS_MODULE_COUNT;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_update_energy(nimcp_physics_intra_t coord, float delta_energy) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in update_energy");

    if (coord->config.enforce_energy_conservation) {
        /* In closed system, energy should be conserved - track violations */
        if (delta_energy != 0.0f) {
            coord->stats.constraint_violations++;
        }
    }

    coord->state.total_energy += delta_energy;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_update_entropy(nimcp_physics_intra_t coord, float delta_entropy) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in update_entropy");

    if (coord->config.enforce_entropy_increase && delta_entropy < 0.0f) {
        coord->stats.constraint_violations++;
    }

    coord->state.entropy += delta_entropy;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_check_constraints(nimcp_physics_intra_t coord, uint32_t* violations_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in check_constraints");
    NIMCP_API_CHECK_NULL(violations_out, NIMCP_LAYER_ERR_NULL_PTR, "violations_out is NULL in check_constraints");
    *violations_out = (uint32_t)coord->stats.constraint_violations;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_get_state(nimcp_physics_intra_t coord, nimcp_physics_intra_state_t* state_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in get_state");
    *state_out = coord->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_intra_get_stats(nimcp_physics_intra_t coord, nimcp_physics_intra_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in get_stats");
    *stats_out = coord->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_physics_intra_get_coherence(nimcp_physics_intra_t coord) {
    return coord ? coord->state.layer_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_physics_intra_reset_stats(nimcp_physics_intra_t coord) {
    NIMCP_API_CHECK_NULL(coord, NIMCP_LAYER_ERR_NULL_PTR, "Coordinator is NULL in reset_stats");
    memset(&coord->stats, 0, sizeof(coord->stats));
    return NIMCP_LAYER_OK;
}
