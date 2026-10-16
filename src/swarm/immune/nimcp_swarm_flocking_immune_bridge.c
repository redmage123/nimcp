/**
 * @file nimcp_swarm_flocking_immune_bridge.c
 * @brief Swarm Flocking-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates swarm flocking behavior based on immune state
 * WHY:  Inflammation affects flocking behavior; fragmentation triggers immune
 * HOW:  Cytokines reduce alignment/cohesion; fragmentation triggers stress
 *
 * @author NIMCP Development Team
 */

#include "swarm/immune/nimcp_swarm_flocking_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_flocking_immune_bridge)

struct swarm_flocking_immune_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    swarm_flocking_immune_config_t config;
    brain_immune_system_t* immune_system;
    void* swarm_flocking;
    cytokine_flocking_effects_t cytokine_effects;
    inflammation_flocking_state_t inflammation_state;
    flocking_immune_modulation_t modulation;
    void* bio_ctx;
    bool bio_async_connected;
};

static float get_flocking_factor_for_level(brain_inflammation_level_t level)
{
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_FLOCKING_FACTOR,
        INFLAMMATION_STORM_FLOCKING_FACTOR);
}

int swarm_flocking_immune_default_config(swarm_flocking_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_cytokine_effects = true;
    config->enable_inflammation_effects = true;
    config->enable_fragmentation_stress = true;
    config->enable_formation_boost = true;
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

swarm_flocking_immune_bridge_t* swarm_flocking_immune_bridge_create(
    const swarm_flocking_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_flocking)
{
    if (!config || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for swarm flocking immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_flocking_immune_bridge_create: required parameter is NULL (config, immune_system)");
        return NULL;
    }

    swarm_flocking_immune_bridge_t* bridge =
        (swarm_flocking_immune_bridge_t*)nimcp_malloc(sizeof(swarm_flocking_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm flocking immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_flocking_immune_bridge_create: allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(swarm_flocking_immune_bridge_t));
    memcpy(&bridge->config, config, sizeof(swarm_flocking_immune_config_t));
    bridge->immune_system = immune_system;
    bridge->swarm_flocking = swarm_flocking;

    if (bridge_base_init(&bridge->base, 0, "swarm_flocking_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for swarm flocking immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_flocking_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->cytokine_effects.alignment_reduction = 0.0f;
    bridge->cytokine_effects.cohesion_reduction = 0.0f;
    bridge->cytokine_effects.separation_increase = 0.0f;
    bridge->cytokine_effects.formation_quality_impact = 0.0f;

    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.flocking_factor = 1.0f;
    bridge->inflammation_state.fragmentation_risk = 0.0f;
    bridge->inflammation_state.formation_degraded = false;

    bridge->modulation.formation_quality = 1.0f;
    bridge->modulation.fragmentation_events = 0;
    bridge->modulation.stress_triggered = false;
    bridge->modulation.il10_from_formation = 0.0f;

    NIMCP_LOGGING_INFO("Swarm flocking immune bridge created");
    return bridge;
}

void swarm_flocking_immune_bridge_destroy(swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Swarm flocking immune bridge destroyed");
}

int swarm_flocking_immune_apply_cytokine_effects(swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_flocking_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_cytokine_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    float sensitivity = bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.alignment_reduction =
        (CYTOKINE_IL1_ALIGNMENT_IMPACT +
         CYTOKINE_IL6_ALIGNMENT_IMPACT +
         CYTOKINE_TNF_ALIGNMENT_IMPACT) * sensitivity;

    bridge->cytokine_effects.cohesion_reduction =
        bridge->cytokine_effects.alignment_reduction + CYTOKINE_IL10_COHESION_BOOST * sensitivity;

    bridge->cytokine_effects.separation_increase =
        (CYTOKINE_IL1_SEPARATION_INCREASE + CYTOKINE_TNF_SEPARATION_INCREASE) * sensitivity;

    bridge->cytokine_effects.formation_quality_impact =
        bridge->cytokine_effects.alignment_reduction;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine flocking effects: align=%.2f, sep=%.2f",
                        bridge->cytokine_effects.alignment_reduction,
                        bridge->cytokine_effects.separation_increase);
    return 0;
}

int swarm_flocking_immune_apply_inflammation_effects(swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_flocking_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_inflammation_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_flocking_immune_apply_inflammation_effects: validation failed");
        return -1;
    }
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.flocking_factor = get_flocking_factor_for_level(level);
    bridge->inflammation_state.fragmentation_risk =
        (level >= INFLAMMATION_SYSTEMIC) ? 0.5f : 0.0f;
    bridge->inflammation_state.formation_degraded =
        (level >= INFLAMMATION_REGIONAL);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation flocking effects: level=%d, factor=%.2f",
                        level, bridge->inflammation_state.flocking_factor);
    return 0;
}

int swarm_flocking_immune_trigger_from_fragmentation(swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_fragmentation_stress) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->modulation.fragmentation_events++;
    if (bridge->modulation.fragmentation_events > 3) {
        bridge->modulation.stress_triggered = true;
        NIMCP_LOGGING_INFO("Flocking fragmentation triggered stress: count=%d",
                          bridge->modulation.fragmentation_events);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_flocking_immune_boost_from_formation(swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_formation_boost) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->modulation.il10_from_formation += 0.05f;
    bridge->modulation.fragmentation_events = 0;
    bridge->modulation.stress_triggered = false;

    NIMCP_LOGGING_DEBUG("Good formation boosted IL-10: total=%.2f",
                       bridge->modulation.il10_from_formation);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_flocking_immune_bridge_update(swarm_flocking_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;

    swarm_flocking_immune_apply_cytokine_effects(bridge);
    swarm_flocking_immune_apply_inflammation_effects(bridge);

    return 0;
}

float swarm_flocking_immune_get_alignment_factor(const swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.flocking_factor +
                   bridge->cytokine_effects.alignment_reduction;
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

float swarm_flocking_immune_get_cohesion_factor(const swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.flocking_factor +
                   bridge->cytokine_effects.cohesion_reduction;
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

float swarm_flocking_immune_get_separation_factor(const swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = 1.0f + bridge->cytokine_effects.separation_increase;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

bool swarm_flocking_immune_is_fragmented(const swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool fragmented = bridge->inflammation_state.fragmentation_risk > 0.3f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return fragmented;
}

int swarm_flocking_immune_connect_bio_async(swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect to bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_flocking_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->bio_async_connected) {
        NIMCP_LOGGING_INFO("Swarm flocking-immune bridge already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SWARM_FLOCKING,
        .module_name = "swarm_flocking_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Swarm flocking-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int swarm_flocking_immune_disconnect_bio_async(swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect from bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_flocking_immune_disconnect_bio_async: bridge is NULL");
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
    NIMCP_LOGGING_INFO("Swarm flocking-immune bridge disconnected from bio-async router");

    return 0;
}

bool swarm_flocking_immune_is_bio_async_connected(const swarm_flocking_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
