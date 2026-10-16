/**
 * @file nimcp_swarm_flocking_fep_bridge.c
 */

#include "swarm/nimcp_swarm_flocking_fep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_flocking_fep_bridge)

#define LOG_MODULE "SWARM_FLOCKING_FEP_BRIDGE"


void swarm_flocking_fep_default_config(swarm_flocking_fep_config_t* config) {
    if (!config) return;
    config->alignment_precision_weight = 0.8f;
    config->cohesion_fe_coupling = 0.6f;
    config->formation_prior_strength = 0.75f;
    config->enable_active_inference_steering = true;
}

swarm_flocking_fep_bridge_t* swarm_flocking_fep_create(const swarm_flocking_fep_config_t* config, nimcp_flocking_engine_t* flocking_engine, fep_system_t* fep_system) {
    if (!flocking_engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_flocking_fep_create: flocking_engine is NULL");
        return NULL;
    }
    if (!fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_flocking_fep_create: fep_system is NULL");
        return NULL;
    }
    swarm_flocking_fep_bridge_t* bridge = (swarm_flocking_fep_bridge_t*)nimcp_malloc(sizeof(swarm_flocking_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm_flocking_fep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_flocking_fep_create: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(swarm_flocking_fep_bridge_t));
    if (config) bridge->config = *config;
    else swarm_flocking_fep_default_config(&bridge->config);
    bridge->fep_system = fep_system;
    bridge->flocking_engine = flocking_engine;
    if (bridge_base_init(&bridge->base, 0, "swarm_flocking_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "swarm_flocking_fep");
    return bridge;
}

void swarm_flocking_fep_destroy(swarm_flocking_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "swarm_flocking_fep");
    if (bridge->base.bio_async_enabled) swarm_flocking_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int swarm_flocking_fep_update(swarm_flocking_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    // Compute precision as inverse of free energy (high FE = low precision)
    float precision = 1.0f / (1.0f + fe);
    nimcp_flocking_stats_t flock_stats;
    nimcp_flocking_get_stats(bridge->flocking_engine, &flock_stats);
    bridge->fep_effects.separation_adjustment = fe * 0.1f - 0.05f;
    bridge->fep_effects.alignment_adjustment = -fe * 0.15f;
    bridge->fep_effects.cohesion_adjustment = -fe * bridge->config.cohesion_fe_coupling;
    bridge->fep_effects.formation_tightness = fmaxf(0.3f, 1.0f - fe * 0.2f);
    bridge->flocking_effects.precision_from_alignment = 0.4f + flock_stats.alignment_metric * precision;
    bridge->flocking_effects.uncertainty_from_dispersion = (1.0f - flock_stats.cohesion_metric) * 0.5f;
    bridge->flocking_effects.formation_confidence = flock_stats.formation_quality;
    bridge->state.last_alignment_metric = flock_stats.alignment_metric;
    bridge->state.last_cohesion_metric = flock_stats.cohesion_metric;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();
    bridge->stats.total_updates++;
    bridge->stats.avg_alignment_fe = (bridge->stats.avg_alignment_fe * (bridge->stats.total_updates - 1) + fe) / bridge->stats.total_updates;
    bridge->stats.avg_formation_quality = (bridge->stats.avg_formation_quality * (bridge->stats.total_updates - 1) + flock_stats.formation_quality) / bridge->stats.total_updates;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_flocking_fep_apply_modulation(swarm_flocking_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int swarm_flocking_fep_get_effects(const swarm_flocking_fep_bridge_t* bridge, swarm_flocking_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_flocking_fep_get_flocking_effects(const swarm_flocking_fep_bridge_t* bridge, fep_swarm_flocking_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->flocking_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_flocking_fep_get_stats(const swarm_flocking_fep_bridge_t* bridge, swarm_flocking_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_flocking_fep_connect_bio_async(swarm_flocking_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_FEP_SWARM_FLOCKING, .module_name = "swarm_flocking_fep_bridge", .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL, .user_data = bridge };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int swarm_flocking_fep_disconnect_bio_async(swarm_flocking_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool swarm_flocking_fep_is_bio_async_connected(const swarm_flocking_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
