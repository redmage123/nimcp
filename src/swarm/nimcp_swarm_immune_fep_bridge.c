/**
 * @file nimcp_swarm_immune_fep_bridge.c
 */

#include "swarm/nimcp_swarm_immune_fep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_immune_fep_bridge)

#define LOG_MODULE "SWARM_IMMUNE_FEP_BRIDGE"


void swarm_immune_fep_default_config(swarm_immune_fep_config_t* config) {
    if (!config) return;
    config->threat_pe_weight = 0.9f;
    config->response_action_gain = 1.3f;
    config->inflammation_precision_mod = 0.7f;
    config->enable_adaptive_response = true;
}

swarm_immune_fep_bridge_t* swarm_immune_fep_create(const swarm_immune_fep_config_t* config, void* immune_system, fep_system_t* fep_system) {
    if (!immune_system || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_immune_fep_create: required parameter is NULL (immune_system, fep_system)");
        return NULL;
    }
    swarm_immune_fep_bridge_t* bridge = (swarm_immune_fep_bridge_t*)nimcp_malloc(sizeof(swarm_immune_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_immune_fep_create: allocation failed");

        return NULL;

    }
    memset(bridge, 0, sizeof(swarm_immune_fep_bridge_t));
    if (config) bridge->config = *config;
    else swarm_immune_fep_default_config(&bridge->config);
    bridge->fep_system = fep_system;
    bridge->immune_system = immune_system;
    if (bridge_base_init(&bridge->base, 0, "swarm_immune_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "swarm_immune_fep");
    return bridge;
}

void swarm_immune_fep_destroy(swarm_immune_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "swarm_immune_fep");
    if (bridge->base.bio_async_enabled) swarm_immune_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int swarm_immune_fep_update(swarm_immune_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    bridge->fep_effects.threat_sensitivity = 0.5f + fe * 0.5f;
    bridge->fep_effects.response_urgency = fminf(1.0f, fe * bridge->config.response_action_gain);
    bridge->fep_effects.inflammation_level = fe * bridge->config.inflammation_precision_mod;
    bridge->immune_effects.precision_from_threat = 0.3f + bridge->fep_effects.threat_sensitivity * 0.7f;
    bridge->immune_effects.action_bias_from_immune = bridge->fep_effects.response_urgency;
    bridge->immune_effects.learning_suppression = bridge->fep_effects.inflammation_level;
    bridge->state.last_threat_fe = fe;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();
    bridge->stats.total_updates++;
    bridge->stats.avg_threat_fe = (bridge->stats.avg_threat_fe * (bridge->stats.total_updates - 1) + fe) / bridge->stats.total_updates;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_immune_fep_apply_modulation(swarm_immune_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int swarm_immune_fep_get_effects(const swarm_immune_fep_bridge_t* bridge, swarm_immune_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_immune_fep_get_immune_effects(const swarm_immune_fep_bridge_t* bridge, fep_swarm_immune_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->immune_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_immune_fep_get_stats(const swarm_immune_fep_bridge_t* bridge, swarm_immune_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_immune_fep_connect_bio_async(swarm_immune_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_FEP_SWARM_IMMUNE, .module_name = "swarm_immune_fep_bridge", .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL, .user_data = bridge };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int swarm_immune_fep_disconnect_bio_async(swarm_immune_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool swarm_immune_fep_is_bio_async_connected(const swarm_immune_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
