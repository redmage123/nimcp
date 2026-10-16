/**
 * @file nimcp_collective_workspace_fep_bridge.c
 */

#include "swarm/nimcp_collective_workspace_fep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(collective_workspace_fep_bridge)

#define LOG_MODULE "COLLECTIVE_WORKSPACE_FEP_BRIDGE"


void collective_workspace_fep_default_config(collective_workspace_fep_config_t* config) {
    if (!config) return;
    config->salience_precision_weight = 0.85f;
    config->coherence_fe_coupling = 0.7f;
    config->broadcast_threshold_adaptation = 0.1f;
    config->enable_fe_pruning = true;
}

collective_workspace_fep_bridge_t* collective_workspace_fep_create(const collective_workspace_fep_config_t* config, collective_workspace_t* workspace, fep_system_t* fep_system) {
    if (!workspace || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_workspace_fep_create: required parameter is NULL (workspace, fep_system)");
        return NULL;
    }
    collective_workspace_fep_bridge_t* bridge = (collective_workspace_fep_bridge_t*)nimcp_malloc(sizeof(collective_workspace_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "collective_workspace_fep_create: allocation failed");

        return NULL;

    }
    memset(bridge, 0, sizeof(collective_workspace_fep_bridge_t));
    if (config) bridge->config = *config;
    else collective_workspace_fep_default_config(&bridge->config);
    bridge->fep_system = fep_system;
    bridge->workspace = workspace;
    if (bridge_base_init(&bridge->base, 0, "collective_workspace_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "collective_workspace_fep");
    return bridge;
}

void collective_workspace_fep_destroy(collective_workspace_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "collective_workspace_fep");
    if (bridge->base.bio_async_enabled) collective_workspace_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int collective_workspace_fep_update(collective_workspace_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    // Compute precision as inverse of free energy (high FE = low precision)
    float precision = 1.0f / (1.0f + fe);
    float coherence = collective_workspace_get_coherence(bridge->workspace);
    uint32_t item_count = collective_workspace_get_item_count(bridge->workspace);
    float avg_salience = (item_count > 0) ? 0.7f : 0.0f;
    bridge->fep_effects.salience_modulation = precision * bridge->config.salience_precision_weight - 0.5f;
    bridge->fep_effects.broadcast_urgency = fminf(1.0f, fe * 0.4f);
    bridge->fep_effects.pruning_threshold_adjustment = (fe > 0.5f) ? 0.1f : -0.05f;
    bridge->workspace_effects.precision_from_coherence = 0.5f + coherence * bridge->config.coherence_fe_coupling;
    bridge->workspace_effects.attention_from_salience = avg_salience;
    bridge->workspace_effects.collective_focus_strength = coherence * avg_salience;
    bridge->state.last_coherence = coherence;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();
    bridge->stats.total_updates++;
    bridge->stats.avg_workspace_fe = (bridge->stats.avg_workspace_fe * (bridge->stats.total_updates - 1) + fe) / bridge->stats.total_updates;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int collective_workspace_fep_apply_modulation(collective_workspace_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int collective_workspace_fep_get_effects(const collective_workspace_fep_bridge_t* bridge, collective_workspace_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int collective_workspace_fep_get_workspace_effects(const collective_workspace_fep_bridge_t* bridge, fep_collective_workspace_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->workspace_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int collective_workspace_fep_get_stats(const collective_workspace_fep_bridge_t* bridge, collective_workspace_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int collective_workspace_fep_connect_bio_async(collective_workspace_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_FEP_COLLECTIVE_WORKSPACE, .module_name = "collective_workspace_fep_bridge", .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL, .user_data = bridge };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int collective_workspace_fep_disconnect_bio_async(collective_workspace_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool collective_workspace_fep_is_bio_async_connected(const collective_workspace_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
