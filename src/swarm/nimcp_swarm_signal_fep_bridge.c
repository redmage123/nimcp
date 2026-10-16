/**
 * @file nimcp_swarm_signal_fep_bridge.c
 */

#include "swarm/nimcp_swarm_signal_fep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_signal_fep_bridge)

#define LOG_MODULE "SWARM_SIGNAL_FEP_BRIDGE"


void swarm_signal_fep_default_config(swarm_signal_fep_config_t* config) {
    if (!config) return;
    config->signal_precision_weight = 0.85f;
    config->routing_fe_threshold = 0.6f;
    config->propagation_decay_rate = 0.1f;
    config->enable_precision_routing = true;
}

swarm_signal_fep_bridge_t* swarm_signal_fep_create(const swarm_signal_fep_config_t* config, void* signal_ctx, fep_system_t* fep_system) {
    if (!signal_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_fep_create: signal_ctx is NULL");
        return NULL;
    }
    if (!fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_signal_fep_create: fep_system is NULL");
        return NULL;
    }
    swarm_signal_fep_bridge_t* bridge = (swarm_signal_fep_bridge_t*)nimcp_malloc(sizeof(swarm_signal_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm_signal_fep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_signal_fep_create: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(swarm_signal_fep_bridge_t));
    if (config) bridge->config = *config;
    else swarm_signal_fep_default_config(&bridge->config);
    bridge->fep_system = fep_system;
    bridge->signal_ctx = signal_ctx;
    if (bridge_base_init(&bridge->base, 0, "swarm_signal_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "swarm_signal_fep");
    return bridge;
}

void swarm_signal_fep_destroy(swarm_signal_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "swarm_signal_fep");
    if (bridge->base.bio_async_enabled) swarm_signal_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int swarm_signal_fep_update(swarm_signal_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    // Compute precision as inverse of free energy (high FE = low precision)
    float precision = 1.0f / (1.0f + fe);
    bridge->fep_effects.signal_amplification = precision * bridge->config.signal_precision_weight;
    bridge->fep_effects.routing_confidence = fmaxf(0.3f, 1.0f - fe * 0.3f);
    bridge->fep_effects.propagation_rate = fmaxf(0.5f, 1.0f - fe * bridge->config.propagation_decay_rate);
    bridge->signal_effects.precision_from_signal = 0.5f + bridge->fep_effects.signal_amplification * 0.5f;
    bridge->signal_effects.belief_update_from_signal = bridge->fep_effects.routing_confidence;
    bridge->state.last_signal_fe = fe;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();
    bridge->stats.total_updates++;
    bridge->stats.avg_signal_fe = (bridge->stats.avg_signal_fe * (bridge->stats.total_updates - 1) + fe) / bridge->stats.total_updates;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_signal_fep_apply_modulation(swarm_signal_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int swarm_signal_fep_get_effects(const swarm_signal_fep_bridge_t* bridge, swarm_signal_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_signal_fep_get_signal_effects(const swarm_signal_fep_bridge_t* bridge, fep_swarm_signal_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->signal_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_signal_fep_get_stats(const swarm_signal_fep_bridge_t* bridge, swarm_signal_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_signal_fep_connect_bio_async(swarm_signal_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_FEP_SWARM_SIGNAL, .module_name = "swarm_signal_fep_bridge", .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL, .user_data = bridge };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int swarm_signal_fep_disconnect_bio_async(swarm_signal_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool swarm_signal_fep_is_bio_async_connected(const swarm_signal_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
