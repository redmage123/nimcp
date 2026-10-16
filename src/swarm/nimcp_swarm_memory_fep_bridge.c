/**
 * @file nimcp_swarm_memory_fep_bridge.c
 */

#include "swarm/nimcp_swarm_memory_fep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_memory_fep_bridge)

#define LOG_MODULE "SWARM_MEMORY_FEP_BRIDGE"


void swarm_memory_fep_default_config(swarm_memory_fep_config_t* config) {
    if (!config) return;
    config->recall_precision_weight = 0.9f;
    config->consolidation_fe_threshold = 0.5f;
    config->pattern_completion_gain = 1.2f;
    config->enable_predictive_recall = true;
}

swarm_memory_fep_bridge_t* swarm_memory_fep_create(const swarm_memory_fep_config_t* config, void* memory_ctx, fep_system_t* fep_system) {
    if (!memory_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_memory_fep_create: memory_ctx is NULL");
        return NULL;
    }
    if (!fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_memory_fep_create: fep_system is NULL");
        return NULL;
    }
    swarm_memory_fep_bridge_t* bridge = (swarm_memory_fep_bridge_t*)nimcp_malloc(sizeof(swarm_memory_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm_memory_fep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_memory_fep_create: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(swarm_memory_fep_bridge_t));
    if (config) bridge->config = *config;
    else swarm_memory_fep_default_config(&bridge->config);
    bridge->fep_system = fep_system;
    bridge->memory_ctx = memory_ctx;
    if (bridge_base_init(&bridge->base, 0, "swarm_memory_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "swarm_memory_fep");
    return bridge;
}

void swarm_memory_fep_destroy(swarm_memory_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "swarm_memory_fep");
    if (bridge->base.bio_async_enabled) swarm_memory_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int swarm_memory_fep_update(swarm_memory_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    bridge->fep_effects.recall_confidence = 1.0f - fe * 0.2f;
    bridge->fep_effects.consolidation_rate = (fe < bridge->config.consolidation_fe_threshold) ? 1.2f : 0.8f;
    bridge->fep_effects.pattern_strength = 1.0f - fe * 0.15f;
    bridge->memory_effects.precision_from_recall = 0.6f + bridge->fep_effects.recall_confidence * 0.8f;
    bridge->memory_effects.prior_strength_from_memory = 0.7f;
    bridge->state.last_recall_fe = fe;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();
    bridge->stats.total_updates++;
    bridge->stats.avg_recall_fe = (bridge->stats.avg_recall_fe * (bridge->stats.total_updates - 1) + fe) / bridge->stats.total_updates;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_memory_fep_apply_modulation(swarm_memory_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int swarm_memory_fep_get_effects(const swarm_memory_fep_bridge_t* bridge, swarm_memory_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_memory_fep_get_memory_effects(const swarm_memory_fep_bridge_t* bridge, fep_swarm_memory_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->memory_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_memory_fep_get_stats(const swarm_memory_fep_bridge_t* bridge, swarm_memory_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_memory_fep_connect_bio_async(swarm_memory_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_FEP_SWARM_MEMORY, .module_name = "swarm_memory_fep_bridge", .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL, .user_data = bridge };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int swarm_memory_fep_disconnect_bio_async(swarm_memory_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool swarm_memory_fep_is_bio_async_connected(const swarm_memory_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
