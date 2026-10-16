/**
 * @file nimcp_swarm_quorum_immune_bridge.c
 * @brief Swarm Quorum-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates swarm quorum decisions based on immune state
 * WHY:  Inflammation affects quorum thresholds; failed quorums trigger immune
 * HOW:  Cytokines increase thresholds; split decisions trigger stress
 *
 * @author NIMCP Development Team
 */

#include "swarm/immune/nimcp_swarm_quorum_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_quorum_immune_bridge)

struct swarm_quorum_immune_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    swarm_quorum_immune_config_t config;
    brain_immune_system_t* immune_system;
    void* swarm_quorum;
    cytokine_quorum_effects_t cytokine_effects;
    inflammation_quorum_state_t inflammation_state;
    quorum_immune_modulation_t modulation;
    void* bio_ctx;
    bool bio_async_connected;
};

static float get_quorum_factor_for_level(brain_inflammation_level_t level)
{
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_QUORUM_FACTOR;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_QUORUM_FACTOR;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_QUORUM_FACTOR;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_QUORUM_FACTOR;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_QUORUM_FACTOR;
        default:                          return INFLAMMATION_NONE_QUORUM_FACTOR;
    }
}

int swarm_quorum_immune_default_config(swarm_quorum_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_cytokine_effects = true;
    config->enable_inflammation_effects = true;
    config->enable_failure_stress = true;
    config->enable_success_boost = true;
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

swarm_quorum_immune_bridge_t* swarm_quorum_immune_bridge_create(
    const swarm_quorum_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_quorum)
{
    if (!config || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for swarm quorum immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_quorum_immune_bridge_create: required parameter is NULL (config, immune_system)");
        return NULL;
    }

    swarm_quorum_immune_bridge_t* bridge =
        (swarm_quorum_immune_bridge_t*)nimcp_malloc(sizeof(swarm_quorum_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm quorum immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_quorum_immune_bridge_create: allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(swarm_quorum_immune_bridge_t));
    memcpy(&bridge->config, config, sizeof(swarm_quorum_immune_config_t));
    bridge->immune_system = immune_system;
    bridge->swarm_quorum = swarm_quorum;

    if (bridge_base_init(&bridge->base, 0, "swarm_quorum_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for swarm quorum immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_quorum_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->cytokine_effects.threshold_increase = 0.0f;
    bridge->cytokine_effects.signal_integration_reduction = 0.0f;
    bridge->cytokine_effects.commitment_rate_reduction = 0.0f;

    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.quorum_factor = 1.0f;
    bridge->inflammation_state.integration_efficiency = 1.0f;
    bridge->inflammation_state.quorum_impaired = false;

    bridge->modulation.failed_quorums = 0;
    bridge->modulation.split_decisions = 0;
    bridge->modulation.stress_triggered = false;
    bridge->modulation.il10_from_success = 0.0f;

    NIMCP_LOGGING_INFO("Swarm quorum immune bridge created");
    return bridge;
}

void swarm_quorum_immune_bridge_destroy(swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Swarm quorum immune bridge destroyed");
}

int swarm_quorum_immune_apply_cytokine_effects(swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_quorum_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_cytokine_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    float sensitivity = bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.threshold_increase =
        (CYTOKINE_IL1_THRESHOLD_INCREASE +
         CYTOKINE_IL6_THRESHOLD_INCREASE +
         CYTOKINE_TNF_THRESHOLD_INCREASE -
         CYTOKINE_IL10_THRESHOLD_REDUCTION) * sensitivity;

    bridge->cytokine_effects.signal_integration_reduction =
        bridge->cytokine_effects.threshold_increase * 0.5f;
    bridge->cytokine_effects.commitment_rate_reduction =
        bridge->cytokine_effects.threshold_increase * 0.3f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine quorum effects: threshold_increase=%.2f",
                        bridge->cytokine_effects.threshold_increase);
    return 0;
}

int swarm_quorum_immune_apply_inflammation_effects(swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_quorum_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_inflammation_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_quorum_immune_apply_inflammation_effects: validation failed");
        return -1;
    }
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.quorum_factor = get_quorum_factor_for_level(level);
    bridge->inflammation_state.integration_efficiency =
        1.0f / bridge->inflammation_state.quorum_factor;
    bridge->inflammation_state.quorum_impaired =
        (level >= INFLAMMATION_REGIONAL);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation quorum effects: level=%d, factor=%.2f",
                        level, bridge->inflammation_state.quorum_factor);
    return 0;
}

int swarm_quorum_immune_trigger_from_failure(swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_failure_stress) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->modulation.failed_quorums++;
    if (bridge->modulation.failed_quorums > 5) {
        bridge->modulation.stress_triggered = true;
        NIMCP_LOGGING_INFO("Quorum failures triggered stress: count=%d",
                          bridge->modulation.failed_quorums);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_quorum_immune_boost_from_success(swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_success_boost) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->modulation.il10_from_success += 0.05f;
    bridge->modulation.failed_quorums = 0;
    bridge->modulation.stress_triggered = false;

    NIMCP_LOGGING_DEBUG("Quorum success boosted IL-10: total=%.2f",
                       bridge->modulation.il10_from_success);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_quorum_immune_bridge_update(swarm_quorum_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;

    swarm_quorum_immune_apply_cytokine_effects(bridge);
    swarm_quorum_immune_apply_inflammation_effects(bridge);

    return 0;
}

float swarm_quorum_immune_get_threshold_factor(const swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.quorum_factor *
                   (1.0f + bridge->cytokine_effects.threshold_increase);
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

float swarm_quorum_immune_get_integration_factor(const swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.integration_efficiency;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

bool swarm_quorum_immune_is_impaired(const swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool impaired = bridge->inflammation_state.quorum_impaired;
    nimcp_mutex_unlock(bridge->base.mutex);

    return impaired;
}

int swarm_quorum_immune_connect_bio_async(swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect to bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_quorum_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->bio_async_connected) {
        NIMCP_LOGGING_INFO("Swarm quorum-immune bridge already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SWARM_QUORUM,
        .module_name = "swarm_quorum_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Swarm quorum-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int swarm_quorum_immune_disconnect_bio_async(swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect from bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_quorum_immune_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    if (!bridge->bio_async_connected) {
        return 0;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->bio_async_connected = false;
    NIMCP_LOGGING_INFO("Swarm quorum-immune bridge disconnected from bio-async router");

    return 0;
}

bool swarm_quorum_immune_is_bio_async_connected(const swarm_quorum_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
