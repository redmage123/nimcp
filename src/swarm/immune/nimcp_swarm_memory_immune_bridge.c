/**
 * @file nimcp_swarm_memory_immune_bridge.c
 * @brief Swarm Memory-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates swarm memory based on immune state
 * WHY:  Inflammation impairs memory consolidation; overload triggers immune
 * HOW:  Cytokines reduce consolidation; high load triggers stress response
 *
 * @author NIMCP Development Team
 */

#include "swarm/immune/nimcp_swarm_memory_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_memory_immune_bridge)

struct swarm_memory_immune_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    swarm_memory_immune_config_t config;
    brain_immune_system_t* immune_system;
    void* swarm_memory;
    cytokine_memory_effects_t cytokine_effects;
    inflammation_memory_state_t inflammation_state;
    memory_immune_modulation_t modulation;
    void* bio_ctx;
    bool bio_async_connected;
};

static float get_capacity_factor_for_level(brain_inflammation_level_t level)
{
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_MEMORY_CAPACITY,
        INFLAMMATION_STORM_MEMORY_CAPACITY);
}

static float get_forgetting_mult_for_level(brain_inflammation_level_t level)
{
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_FORGETTING_MULT,
        INFLAMMATION_STORM_FORGETTING_MULT);
}

int swarm_memory_immune_default_config(swarm_memory_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_cytokine_effects = true;
    config->enable_inflammation_effects = true;
    config->enable_load_stress = true;
    config->enable_corruption_cleanup = true;
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

swarm_memory_immune_bridge_t* swarm_memory_immune_create(
    const swarm_memory_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_memory)
{
    if (!config || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for swarm memory immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_memory_immune_create: required parameter is NULL (config, immune_system)");
        return NULL;
    }

    swarm_memory_immune_bridge_t* bridge =
        (swarm_memory_immune_bridge_t*)nimcp_malloc(sizeof(swarm_memory_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm memory immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_memory_immune_bridge_create: allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(swarm_memory_immune_bridge_t));
    memcpy(&bridge->config, config, sizeof(swarm_memory_immune_config_t));
    bridge->immune_system = immune_system;
    bridge->swarm_memory = swarm_memory;

    if (bridge_base_init(&bridge->base, 0, "swarm_memory_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for swarm memory immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_memory_immune_create: bridge->base is NULL");
        return NULL;
    }

    bridge->cytokine_effects.consolidation_deficit = 0.0f;
    bridge->cytokine_effects.forgetting_rate_multiplier = 1.0f;
    bridge->cytokine_effects.replay_strength_reduction = 0.0f;

    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.capacity_factor = 1.0f;
    bridge->inflammation_state.consolidation_efficiency = 1.0f;
    bridge->inflammation_state.forgetting_multiplier = 1.0f;

    bridge->modulation.memory_load = 0.0f;
    bridge->modulation.corruptions = 0;
    bridge->modulation.stress_triggered = false;
    bridge->modulation.cleanup_signal = 0.0f;

    NIMCP_LOGGING_INFO("Swarm memory immune bridge created");
    return bridge;
}

void swarm_memory_immune_destroy(swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Swarm memory immune bridge destroyed");
}

int swarm_memory_immune_apply_cytokine_effects(swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_memory_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_cytokine_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    float sensitivity = bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.consolidation_deficit =
        (CYTOKINE_IL1_CONSOLIDATION_IMPACT +
         CYTOKINE_IL6_CONSOLIDATION_IMPACT +
         CYTOKINE_TNF_CONSOLIDATION_IMPACT +
         CYTOKINE_IL10_CONSOLIDATION_IMPACT) * sensitivity;

    bridge->cytokine_effects.forgetting_rate_multiplier =
        1.0f - bridge->cytokine_effects.consolidation_deficit;
    bridge->cytokine_effects.replay_strength_reduction =
        -bridge->cytokine_effects.consolidation_deficit * 0.5f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine memory effects: deficit=%.2f",
                        bridge->cytokine_effects.consolidation_deficit);
    return 0;
}

int swarm_memory_immune_apply_inflammation_effects(swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_memory_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_inflammation_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_memory_immune_apply_inflammation_effects: validation failed");
        return -1;
    }
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.capacity_factor = get_capacity_factor_for_level(level);
    bridge->inflammation_state.forgetting_multiplier = get_forgetting_mult_for_level(level);
    bridge->inflammation_state.consolidation_efficiency =
        bridge->inflammation_state.capacity_factor;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation memory effects: level=%d, capacity=%.2f",
                        level, bridge->inflammation_state.capacity_factor);
    return 0;
}

int swarm_memory_immune_trigger_stress_from_load(swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_load_stress) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->modulation.memory_load > 0.9f) {
        bridge->modulation.stress_triggered = true;
        NIMCP_LOGGING_INFO("Memory load triggered stress response: load=%.2f",
                          bridge->modulation.memory_load);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_memory_immune_activate_cleanup(swarm_memory_immune_bridge_t* bridge, uint32_t corruptions)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_corruption_cleanup) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->modulation.corruptions += corruptions;
    if (bridge->modulation.corruptions > 5) {
        bridge->modulation.cleanup_signal = 1.0f;
        NIMCP_LOGGING_INFO("Memory corruption activated cleanup: count=%d",
                          bridge->modulation.corruptions);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_memory_immune_update(swarm_memory_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;

    swarm_memory_immune_apply_cytokine_effects(bridge);
    swarm_memory_immune_apply_inflammation_effects(bridge);

    return 0;
}

float swarm_memory_immune_get_capacity_factor(const swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.capacity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

float swarm_memory_immune_get_consolidation_efficiency(const swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float efficiency = bridge->inflammation_state.consolidation_efficiency;
    nimcp_mutex_unlock(bridge->base.mutex);

    return efficiency;
}

float swarm_memory_immune_compute_forgetting_multiplier(const swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float mult = bridge->inflammation_state.forgetting_multiplier *
                 bridge->cytokine_effects.forgetting_rate_multiplier;
    nimcp_mutex_unlock(bridge->base.mutex);

    return mult;
}

bool swarm_memory_immune_has_memory_impairment(const swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool impaired = (bridge->inflammation_state.current_level >= INFLAMMATION_REGIONAL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return impaired;
}

int swarm_memory_immune_connect_bio_async(swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect to bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_memory_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->bio_async_connected) {
        NIMCP_LOGGING_INFO("Swarm memory-immune bridge already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SWARM_MEMORY,
        .module_name = "swarm_memory_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Swarm memory-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int swarm_memory_immune_disconnect_bio_async(swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect from bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_memory_immune_disconnect_bio_async: bridge is NULL");
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
    NIMCP_LOGGING_INFO("Swarm memory-immune bridge disconnected from bio-async router");

    return 0;
}

bool swarm_memory_immune_is_bio_async_connected(const swarm_memory_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
