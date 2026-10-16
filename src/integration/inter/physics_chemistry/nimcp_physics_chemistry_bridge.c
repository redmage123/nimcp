/**
 * @file nimcp_physics_chemistry_bridge.c
 * @brief Physics-Chemistry Inter-Layer Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-10
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "integration/inter/physics_chemistry/nimcp_physics_chemistry_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <string.h>
#include <stdlib.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_chemistry_bridge)

#define LOG_MODULE "PHYSICS_CHEMISTRY_BRIDGE"


struct nimcp_physics_chemistry_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_physics_chemistry_config_t config;
    nimcp_layer_registry_t registry;
    nimcp_physics_intra_t physics;
    nimcp_chemistry_intra_t chemistry;
    nimcp_physics_chemistry_state_t state;
    nimcp_physics_chemistry_stats_t stats;
    bool is_initialized;
};

nimcp_physics_chemistry_config_t nimcp_physics_chemistry_default_config(void) {
    nimcp_physics_chemistry_config_t config = {
        .energy_coupling_strength = 0.8f,
        .thermal_coupling_strength = 0.9f,
        .diffusion_coupling_strength = 0.7f,
        .update_interval_ms = 10,
        .enable_thermodynamic_constraints = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

nimcp_physics_chemistry_bridge_t nimcp_physics_chemistry_create(const nimcp_physics_chemistry_config_t* config) {
    nimcp_physics_chemistry_bridge_t bridge = (nimcp_physics_chemistry_bridge_t)nimcp_calloc(1, sizeof(struct nimcp_physics_chemistry_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate physics-chemistry bridge");
    bridge->config = config ? *config : nimcp_physics_chemistry_default_config();
    bridge->state.bridge_coherence = 1.0f;
    bridge->state.current_temperature = 310.0f;
    NIMCP_LOGGING_INFO("Created %s bridge", "physics_chemistry");
    return bridge;
}

void nimcp_physics_chemistry_destroy(nimcp_physics_chemistry_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "physics_chemistry");
    if (bridge->is_initialized) nimcp_physics_chemistry_shutdown(bridge);
    nimcp_free(bridge);
}

nimcp_layer_error_t nimcp_physics_chemistry_init(
    nimcp_physics_chemistry_bridge_t bridge,
    nimcp_layer_registry_t registry,
    nimcp_physics_intra_t physics,
    nimcp_chemistry_intra_t chemistry
) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in physics_chemistry_init");
    NIMCP_API_CHECK_NULL(registry, NIMCP_LAYER_ERR_NULL_PTR, "Registry is NULL in physics_chemistry_init");
    NIMCP_API_CHECK(!bridge->is_initialized, NIMCP_LAYER_ERR_ALREADY_REGISTERED, "Bridge already initialized in physics_chemistry_init");
    bridge->registry = registry;
    bridge->physics = physics;
    bridge->chemistry = chemistry;
    bridge->is_initialized = true;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_chemistry_shutdown(nimcp_physics_chemistry_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in physics_chemistry_shutdown");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in physics_chemistry_shutdown");
    bridge->is_initialized = false;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_chemistry_update(nimcp_physics_chemistry_bridge_t bridge, float dt) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in physics_chemistry_update");
    NIMCP_API_CHECK(bridge->is_initialized, NIMCP_LAYER_ERR_NOT_INITIALIZED, "Bridge not initialized in physics_chemistry_update");

    /* Temperature affects reaction rates via Arrhenius relationship */
    float temp_factor = bridge->state.current_temperature / 310.0f;
    bridge->state.reaction_rate_modifier = temp_factor * bridge->config.thermal_coupling_strength;

    /* Energy availability for reactions */
    bridge->state.energy_available *= (1.0f - dt * 0.01f);

    /* Diffusion rate depends on temperature */
    bridge->state.diffusion_rate = temp_factor * bridge->config.diffusion_coupling_strength;

    /* Update average stats */
    bridge->stats.avg_energy_transfer = bridge->stats.avg_energy_transfer * NIMCP_EMA_DECAY_DEFAULT + bridge->state.energy_available * NIMCP_LEARNING_RATE_DEFAULT;
    bridge->stats.avg_thermal_coupling = bridge->stats.avg_thermal_coupling * NIMCP_EMA_DECAY_DEFAULT + bridge->state.reaction_rate_modifier * NIMCP_LEARNING_RATE_DEFAULT;

    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_chemistry_transfer_bottom_up(nimcp_physics_chemistry_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in physics_chemistry_transfer_bottom_up");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in physics_chemistry_transfer_bottom_up");
    bridge->state.bottom_up_messages++;
    bridge->stats.energy_state_transfers++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_chemistry_transfer_top_down(nimcp_physics_chemistry_bridge_t bridge, const nimcp_layer_msg_t* msg) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in physics_chemistry_transfer_top_down");
    NIMCP_API_CHECK_NULL(msg, NIMCP_LAYER_ERR_NULL_PTR, "Message is NULL in physics_chemistry_transfer_top_down");
    bridge->state.top_down_messages++;
    bridge->stats.reaction_heat_events++;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_chemistry_get_state(nimcp_physics_chemistry_bridge_t bridge, nimcp_physics_chemistry_state_t* state_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in physics_chemistry_get_state");
    NIMCP_API_CHECK_NULL(state_out, NIMCP_LAYER_ERR_NULL_PTR, "state_out is NULL in physics_chemistry_get_state");
    *state_out = bridge->state;
    return NIMCP_LAYER_OK;
}

nimcp_layer_error_t nimcp_physics_chemistry_get_stats(nimcp_physics_chemistry_bridge_t bridge, nimcp_physics_chemistry_stats_t* stats_out) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in physics_chemistry_get_stats");
    NIMCP_API_CHECK_NULL(stats_out, NIMCP_LAYER_ERR_NULL_PTR, "stats_out is NULL in physics_chemistry_get_stats");
    *stats_out = bridge->stats;
    return NIMCP_LAYER_OK;
}

float nimcp_physics_chemistry_get_coherence(nimcp_physics_chemistry_bridge_t bridge) {
    return bridge ? bridge->state.bridge_coherence : -1.0f;
}

nimcp_layer_error_t nimcp_physics_chemistry_reset_stats(nimcp_physics_chemistry_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, NIMCP_LAYER_ERR_NULL_PTR, "Bridge is NULL in physics_chemistry_reset_stats");
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return NIMCP_LAYER_OK;
}
