/**
 * @file nimcp_swarm_emergence_fep_bridge.c
 */

#include "swarm/nimcp_swarm_emergence_fep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_emergence_fep_bridge)

#define LOG_MODULE "SWARM_EMERGENCE_FEP_BRIDGE"


void swarm_emergence_fep_default_config(swarm_emergence_fep_config_t* config) {
    if (!config) return;
    config->tier_fe_threshold = 0.5f;
    config->coherence_precision_gain = 1.2f;
    config->hierarchy_depth_scaling = 1.0f;
    config->enable_emergence_detection = true;
}

swarm_emergence_fep_bridge_t* swarm_emergence_fep_create(const swarm_emergence_fep_config_t* config, swarm_emergence_ctx_t* emergence_ctx, fep_system_t* fep_system) {
    if (!emergence_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_emergence_fep_create: emergence_ctx is NULL");
        return NULL;
    }
    if (!fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_emergence_fep_create: fep_system is NULL");
        return NULL;
    }
    swarm_emergence_fep_bridge_t* bridge = (swarm_emergence_fep_bridge_t*)nimcp_malloc(sizeof(swarm_emergence_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm_emergence_fep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_emergence_fep_create: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(swarm_emergence_fep_bridge_t));
    if (config) bridge->config = *config;
    else swarm_emergence_fep_default_config(&bridge->config);
    bridge->fep_system = fep_system;
    bridge->emergence_ctx = emergence_ctx;
    if (bridge_base_init(&bridge->base, 0, "swarm_emergence_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "swarm_emergence_fep");
    return bridge;
}

void swarm_emergence_fep_destroy(swarm_emergence_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "swarm_emergence_fep");
    if (bridge->base.bio_async_enabled) swarm_emergence_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int swarm_emergence_fep_update(swarm_emergence_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    swarm_emergence_tier_t tier = swarm_emergence_get_tier(bridge->emergence_ctx);
    float coherence = swarm_emergence_get_coherence(bridge->emergence_ctx);
    bridge->fep_effects.tier_advancement_bias = (fe < bridge->config.tier_fe_threshold) ? 0.2f : -0.1f;
    bridge->fep_effects.coherence_boost = fmaxf(0.0f, 0.3f - fe * 0.2f);
    bridge->fep_effects.capability_activation_threshold = 0.6f + fe * 0.1f;
    bridge->emergence_effects.precision_from_tier = 0.5f + ((float)tier / (float)SWARM_TIER_COUNT) * 1.0f;
    bridge->emergence_effects.hierarchy_depth = (uint32_t)tier + 1;
    bridge->emergence_effects.model_complexity = (float)tier / (float)SWARM_TIER_COUNT;
    if (bridge->state.last_tier != tier) {
        bridge->state.tier_transitions++;
        bridge->state.last_tier = tier;
        bridge->stats.emergence_events++;
    }
    bridge->state.last_coherence = coherence;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();
    bridge->stats.total_updates++;
    bridge->stats.avg_tier_fe = (bridge->stats.avg_tier_fe * (bridge->stats.total_updates - 1) + fe) / bridge->stats.total_updates;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_emergence_fep_apply_modulation(swarm_emergence_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int swarm_emergence_fep_get_effects(const swarm_emergence_fep_bridge_t* bridge, swarm_emergence_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_emergence_fep_get_emergence_effects(const swarm_emergence_fep_bridge_t* bridge, fep_swarm_emergence_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->emergence_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_emergence_fep_get_stats(const swarm_emergence_fep_bridge_t* bridge, swarm_emergence_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_emergence_fep_connect_bio_async(swarm_emergence_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_FEP_SWARM_EMERGENCE, .module_name = "swarm_emergence_fep_bridge", .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL, .user_data = bridge };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int swarm_emergence_fep_disconnect_bio_async(swarm_emergence_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool swarm_emergence_fep_is_bio_async_connected(const swarm_emergence_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
