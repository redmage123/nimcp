/**
 * @file nimcp_swarm_emergence_immune_bridge.c
 * @brief Swarm Emergence-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates swarm emergence based on immune state
 * WHY:  Inflammation suppresses tier advancement; regression triggers immune
 * HOW:  Cytokines block tier advancement; tier regression triggers stress
 *
 * @author NIMCP Development Team
 */

#include "swarm/immune/nimcp_swarm_emergence_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_emergence_immune_bridge)

struct swarm_emergence_immune_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    swarm_emergence_immune_config_t config;
    brain_immune_system_t* immune_system;
    void* swarm_emergence;
    cytokine_emergence_effects_t cytokine_effects;
    inflammation_emergence_state_t inflammation_state;
    emergence_immune_modulation_t modulation;
    void* bio_ctx;
    bool bio_async_connected;
};

static float get_emergence_factor_for_level(brain_inflammation_level_t level)
{
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_EMERGENCE_FACTOR,
        INFLAMMATION_STORM_EMERGENCE_FACTOR);
}

int swarm_emergence_immune_default_config(swarm_emergence_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_cytokine_effects = true;
    config->enable_inflammation_effects = true;
    config->enable_regression_stress = true;
    config->enable_tier_boost = true;
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

swarm_emergence_immune_bridge_t* swarm_emergence_immune_bridge_create(
    const swarm_emergence_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_emergence)
{
    if (!config || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for swarm emergence immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_emergence_immune_bridge_create: required parameter is NULL (config, immune_system)");
        return NULL;
    }

    swarm_emergence_immune_bridge_t* bridge =
        (swarm_emergence_immune_bridge_t*)nimcp_malloc(sizeof(swarm_emergence_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm emergence immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_emergence_immune_bridge_create: allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(swarm_emergence_immune_bridge_t));
    memcpy(&bridge->config, config, sizeof(swarm_emergence_immune_config_t));
    bridge->immune_system = immune_system;
    bridge->swarm_emergence = swarm_emergence;

    if (bridge_base_init(&bridge->base, 0, "swarm_emergence_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for swarm emergence immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_emergence_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->cytokine_effects.tier_advancement_suppression = 0.0f;
    bridge->cytokine_effects.coherence_threshold_increase = 0.0f;
    bridge->cytokine_effects.capability_degradation = 0.0f;

    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.emergence_factor = 1.0f;
    bridge->inflammation_state.tier_penalty = 0;
    bridge->inflammation_state.advancement_blocked = false;

    bridge->modulation.current_tier = 0;
    bridge->modulation.tier_drops = 0;
    bridge->modulation.stress_triggered = false;
    bridge->modulation.il10_from_tier = 0.0f;

    NIMCP_LOGGING_INFO("Swarm emergence immune bridge created");
    return bridge;
}

void swarm_emergence_immune_bridge_destroy(swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Swarm emergence immune bridge destroyed");
}

int swarm_emergence_immune_apply_cytokine_effects(swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_emergence_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_cytokine_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    float sensitivity = bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.tier_advancement_suppression =
        (CYTOKINE_IL1_EMERGENCE_SUPPRESSION +
         CYTOKINE_IL6_EMERGENCE_SUPPRESSION +
         CYTOKINE_TNF_EMERGENCE_SUPPRESSION -
         CYTOKINE_IL10_EMERGENCE_BOOST) * sensitivity;

    bridge->cytokine_effects.coherence_threshold_increase =
        bridge->cytokine_effects.tier_advancement_suppression * 0.5f;
    bridge->cytokine_effects.capability_degradation =
        bridge->cytokine_effects.tier_advancement_suppression * 0.3f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine emergence effects: suppression=%.2f",
                        bridge->cytokine_effects.tier_advancement_suppression);
    return 0;
}

int swarm_emergence_immune_apply_inflammation_effects(swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_emergence_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_inflammation_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_emergence_immune_apply_inflammation_effects: validation failed");
        return -1;
    }
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.emergence_factor = get_emergence_factor_for_level(level);

    if (level >= INFLAMMATION_STORM) {
        bridge->inflammation_state.tier_penalty = 2;
        bridge->inflammation_state.advancement_blocked = true;
    } else if (level >= INFLAMMATION_SYSTEMIC) {
        bridge->inflammation_state.tier_penalty = 1;
        bridge->inflammation_state.advancement_blocked = true;
    } else if (level >= INFLAMMATION_REGIONAL) {
        bridge->inflammation_state.tier_penalty = 0;
        bridge->inflammation_state.advancement_blocked = true;
    } else {
        bridge->inflammation_state.tier_penalty = 0;
        bridge->inflammation_state.advancement_blocked = false;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation emergence effects: level=%d, factor=%.2f",
                        level, bridge->inflammation_state.emergence_factor);
    return 0;
}

int swarm_emergence_immune_trigger_from_regression(swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_regression_stress) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->modulation.tier_drops++;
    if (bridge->modulation.tier_drops > 2) {
        bridge->modulation.stress_triggered = true;
        NIMCP_LOGGING_INFO("Tier regression triggered stress: count=%d",
                          bridge->modulation.tier_drops);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_emergence_immune_boost_from_advancement(swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_tier_boost) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->modulation.il10_from_tier += 0.1f;
    bridge->modulation.tier_drops = 0;
    bridge->modulation.stress_triggered = false;

    NIMCP_LOGGING_DEBUG("Tier advancement boosted IL-10: total=%.2f",
                       bridge->modulation.il10_from_tier);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_emergence_immune_bridge_update(swarm_emergence_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;

    swarm_emergence_immune_apply_cytokine_effects(bridge);
    swarm_emergence_immune_apply_inflammation_effects(bridge);

    return 0;
}

float swarm_emergence_immune_get_emergence_factor(const swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.emergence_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

int swarm_emergence_immune_get_tier_penalty(const swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge) return 0;

    nimcp_mutex_lock(bridge->base.mutex);
    int penalty = bridge->inflammation_state.tier_penalty;
    nimcp_mutex_unlock(bridge->base.mutex);

    return penalty;
}

bool swarm_emergence_immune_is_advancement_blocked(const swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool blocked = bridge->inflammation_state.advancement_blocked;
    nimcp_mutex_unlock(bridge->base.mutex);

    return blocked;
}

int swarm_emergence_immune_connect_bio_async(swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect to bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_emergence_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->bio_async_connected) {
        NIMCP_LOGGING_INFO("Swarm emergence-immune bridge already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SWARM_EMERGENCE,
        .module_name = "swarm_emergence_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Swarm emergence-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int swarm_emergence_immune_disconnect_bio_async(swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect from bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_emergence_immune_disconnect_bio_async: bridge is NULL");
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
    NIMCP_LOGGING_INFO("Swarm emergence-immune bridge disconnected from bio-async router");

    return 0;
}

bool swarm_emergence_immune_is_bio_async_connected(const swarm_emergence_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
