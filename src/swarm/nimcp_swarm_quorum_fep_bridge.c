/**
 * @file nimcp_swarm_quorum_fep_bridge.c
 */

#include "swarm/nimcp_swarm_quorum_fep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_quorum_fep_bridge)

#define LOG_MODULE "SWARM_QUORUM_FEP_BRIDGE"


void swarm_quorum_fep_default_config(swarm_quorum_fep_config_t* config) {
    if (!config) return;
    config->signal_evidence_weight = 0.8f;
    config->threshold_certainty_level = 0.7f;
    config->cascade_precision_gain = 1.4f;
    config->enable_fe_threshold_adaptation = true;
}

swarm_quorum_fep_bridge_t* swarm_quorum_fep_create(const swarm_quorum_fep_config_t* config, nimcp_swarm_quorum_t* quorum_system, fep_system_t* fep_system) {
    if (!quorum_system || !fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_quorum_fep_create: required parameter is NULL (quorum_system, fep_system)");
        return NULL;
    }
    swarm_quorum_fep_bridge_t* bridge = (swarm_quorum_fep_bridge_t*)nimcp_malloc(sizeof(swarm_quorum_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_quorum_fep_create: allocation failed");

        return NULL;

    }
    memset(bridge, 0, sizeof(swarm_quorum_fep_bridge_t));
    if (config) bridge->config = *config;
    else swarm_quorum_fep_default_config(&bridge->config);
    bridge->fep_system = fep_system;
    bridge->quorum_system = quorum_system;
    if (bridge_base_init(&bridge->base, 0, "swarm_quorum_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "swarm_quorum_fep");
    return bridge;
}

void swarm_quorum_fep_destroy(swarm_quorum_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "swarm_quorum_fep");
    if (bridge->base.bio_async_enabled) swarm_quorum_fep_disconnect_bio_async(bridge);
    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int swarm_quorum_fep_update(swarm_quorum_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float fe = fep_get_free_energy(bridge->fep_system);
    // Compute precision as inverse of free energy (high FE = low precision)
    float precision = 1.0f / (1.0f + fe);
    float consensus_strength = nimcp_quorum_get_consensus_strength(bridge->quorum_system);
    bridge->fep_effects.signal_amplification = precision * bridge->config.signal_evidence_weight;
    bridge->fep_effects.threshold_adjustment = (fe > bridge->config.threshold_certainty_level) ? 0.1f : -0.05f;
    bridge->fep_effects.cascade_trigger_bias = fmaxf(0.5f, 1.0f - fe * 0.3f);
    bridge->quorum_effects.precision_from_consensus = 0.5f + consensus_strength * 1.2f;
    bridge->quorum_effects.belief_strength_from_commitment = consensus_strength;
    bridge->quorum_effects.quorum_state = (consensus_strength > 0.7f) ? 1 : 0;
    bridge->state.last_quorum_fe = fe;
    bridge->state.last_update_time = nimcp_platform_time_monotonic_ms();
    bridge->stats.total_updates++;
    bridge->stats.avg_quorum_fe = (bridge->stats.avg_quorum_fe * (bridge->stats.total_updates - 1) + fe) / bridge->stats.total_updates;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_quorum_fep_apply_modulation(swarm_quorum_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    return 0;
}

int swarm_quorum_fep_get_effects(const swarm_quorum_fep_bridge_t* bridge, swarm_quorum_fep_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->fep_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_quorum_fep_get_quorum_effects(const swarm_quorum_fep_bridge_t* bridge, fep_swarm_quorum_effects_t* effects) {
    if (!bridge || !effects) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->quorum_effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_quorum_fep_get_stats(const swarm_quorum_fep_bridge_t* bridge, swarm_quorum_fep_stats_t* stats) {
    if (!bridge || !stats) return -1;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_quorum_fep_connect_bio_async(swarm_quorum_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = { .module_id = BIO_MODULE_FEP_SWARM_QUORUM, .module_name = "swarm_quorum_fep_bridge", .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL, .user_data = bridge };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int swarm_quorum_fep_disconnect_bio_async(swarm_quorum_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool swarm_quorum_fep_is_bio_async_connected(const swarm_quorum_fep_bridge_t* bridge) {
    return bridge && bridge->base.bio_async_enabled;
}
