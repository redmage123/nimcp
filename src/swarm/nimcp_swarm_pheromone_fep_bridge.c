/**
 * @file nimcp_swarm_pheromone_fep_bridge.c
 */

#include "swarm/nimcp_swarm_pheromone_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_pheromone_fep_bridge)

#define LOG_MODULE "SWARM_PHEROMONE_FEP_BRIDGE"


void swarm_pheromone_fep_default_config(swarm_pheromone_fep_config_t* config) {
    if (!config) return;
    config->gradient_precision_weight = 0.75f;
    config->evaporation_fe_coupling = 0.2f;
    config->trail_strength_gain = 1.1f;
    config->enable_fe_gradient_descent = true;
}

swarm_pheromone_fep_bridge_t* swarm_pheromone_fep_create(const swarm_pheromone_fep_config_t* config, void* pheromone_ctx, fep_system_t* fep_system) {
    if (!pheromone_ctx || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_pheromone_fep_create: required parameter is NULL (pheromone_ctx, fep_system)");
        return NULL;
    }
    swarm_pheromone_fep_bridge_t* bridge = (swarm_pheromone_fep_bridge_t*)nimcp_malloc(sizeof(swarm_pheromone_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_pheromone_fep_create: allocation failed");

        return NULL;

    }
    memset(bridge, 0, sizeof(swarm_pheromone_fep_bridge_t));
    if (config) bridge->config = *config;
    else swarm_pheromone_fep_default_config(&bridge->config);
    bridge->fep_system = fep_system;
    bridge->pheromone_ctx = pheromone_ctx;
    if (bridge_base_init(&bridge->base, 0, "swarm_pheromone_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "swarm_pheromone_fep");
    return bridge;
}

void swarm_pheromone_fep_destroy(swarm_pheromone_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "swarm_pheromone_fep");
    if (bridge->base.bio_async_enabled) swarm_pheromone_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int swarm_pheromone_fep_update(swarm_pheromone_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    // Compute precision as inverse of free energy (high FE = low precision)
    float precision = 1.0f / (1.0f + fe);
    bridge->fep_effects.deposition_rate = precision * bridge->config.trail_strength_gain;
    bridge->fep_effects.evaporation_adjustment = 1.0f + fe * bridge->config.evaporation_fe_coupling;
    bridge->fep_effects.gradient_following_bias = fmaxf(0.5f, 1.0f - fe * 0.2f);
    float gradient_mag = 0.8f;
    bridge->pheromone_effects.precision_from_gradient = 0.4f + gradient_mag * bridge->config.gradient_precision_weight;
    bridge->pheromone_effects.action_bias_from_trail = bridge->fep_effects.gradient_following_bias;
    bridge->pheromone_effects.uncertainty_from_evaporation = fe * 0.3f;
    bridge->state.last_gradient_magnitude = gradient_mag;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();
    bridge->stats.total_updates++;
    bridge->stats.avg_gradient_fe = (bridge->stats.avg_gradient_fe * (bridge->stats.total_updates - 1) + fe) / bridge->stats.total_updates;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_pheromone_fep_apply_modulation(swarm_pheromone_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int swarm_pheromone_fep_get_effects(const swarm_pheromone_fep_bridge_t* bridge, swarm_pheromone_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_pheromone_fep_get_pheromone_effects(const swarm_pheromone_fep_bridge_t* bridge, fep_swarm_pheromone_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->pheromone_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_pheromone_fep_get_stats(const swarm_pheromone_fep_bridge_t* bridge, swarm_pheromone_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_pheromone_fep_connect_bio_async(swarm_pheromone_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_FEP_SWARM_PHEROMONE, .module_name = "swarm_pheromone_fep_bridge", .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL, .user_data = bridge };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int swarm_pheromone_fep_disconnect_bio_async(swarm_pheromone_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool swarm_pheromone_fep_is_bio_async_connected(const swarm_pheromone_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
