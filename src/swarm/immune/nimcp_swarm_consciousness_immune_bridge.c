/**
 * @file nimcp_swarm_consciousness_immune_bridge.c
 * @brief Swarm Consciousness-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates swarm consciousness based on immune state
 * WHY:  Inflammation reduces collective phi; low consciousness triggers immune
 * HOW:  Cytokines reduce phi integration; consciousness fragmentation triggers stress
 *
 * @author NIMCP Development Team
 */

#include "swarm/immune/nimcp_swarm_consciousness_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_consciousness_immune_bridge)

struct swarm_consciousness_immune_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    swarm_consciousness_immune_config_t config;
    brain_immune_system_t* immune_system;
    void* swarm_consciousness;
    cytokine_consciousness_effects_t cytokine_effects;
    inflammation_consciousness_state_t inflammation_state;
    consciousness_immune_modulation_t modulation;
    void* bio_ctx;
    bool bio_async_connected;
};

static float get_phi_factor_for_level(brain_inflammation_level_t level)
{
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_PHI_FACTOR,
        INFLAMMATION_STORM_PHI_FACTOR);
}

int swarm_consciousness_immune_default_config(swarm_consciousness_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_cytokine_effects = true;
    config->enable_inflammation_effects = true;
    config->enable_low_phi_stress = true;
    config->enable_high_phi_boost = true;
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

swarm_consciousness_immune_bridge_t* swarm_consciousness_immune_bridge_create(
    const swarm_consciousness_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_consciousness)
{
    if (!config || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for swarm consciousness immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_immune_bridge_create: required parameter is NULL (config, immune_system)");
        return NULL;
    }

    swarm_consciousness_immune_bridge_t* bridge =
        (swarm_consciousness_immune_bridge_t*)nimcp_malloc(sizeof(swarm_consciousness_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm consciousness immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_consciousness_immune_bridge_create: allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(swarm_consciousness_immune_bridge_t));
    memcpy(&bridge->config, config, sizeof(swarm_consciousness_immune_config_t));
    bridge->immune_system = immune_system;
    bridge->swarm_consciousness = swarm_consciousness;

    if (bridge_base_init(&bridge->base, 0, "swarm_consciousness_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for swarm consciousness immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_consciousness_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->cytokine_effects.phi_suppression = 0.0f;
    bridge->cytokine_effects.integration_reduction = 0.0f;
    bridge->cytokine_effects.awareness_narrowing = 0.0f;

    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.phi_factor = 1.0f;
    bridge->inflammation_state.integration_factor = 1.0f;
    bridge->inflammation_state.awareness_fragmented = false;

    bridge->modulation.current_phi = 1.0f;
    bridge->modulation.low_phi_triggered = false;
    bridge->modulation.il10_from_phi = 0.0f;

    NIMCP_LOGGING_INFO("Swarm consciousness immune bridge created");
    return bridge;
}

void swarm_consciousness_immune_bridge_destroy(swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Swarm consciousness immune bridge destroyed");
}

int swarm_consciousness_immune_apply_cytokine_effects(swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_cytokine_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    float sensitivity = bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.phi_suppression =
        (CYTOKINE_IL1_PHI_SUPPRESSION +
         CYTOKINE_IL6_PHI_SUPPRESSION +
         CYTOKINE_TNF_PHI_SUPPRESSION +
         CYTOKINE_IL10_PHI_BOOST) * sensitivity;

    bridge->cytokine_effects.integration_reduction =
        bridge->cytokine_effects.phi_suppression * 0.8f;
    bridge->cytokine_effects.awareness_narrowing =
        bridge->cytokine_effects.phi_suppression * 0.6f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine consciousness effects: phi_suppression=%.2f",
                        bridge->cytokine_effects.phi_suppression);
    return 0;
}

int swarm_consciousness_immune_apply_inflammation_effects(swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_inflammation_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_consciousness_immune_apply_inflammation_effects: validation failed");
        return -1;
    }
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.phi_factor = get_phi_factor_for_level(level);
    bridge->inflammation_state.integration_factor =
        bridge->inflammation_state.phi_factor;
    bridge->inflammation_state.awareness_fragmented =
        (level >= INFLAMMATION_SYSTEMIC);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation consciousness effects: level=%d, phi=%.2f",
                        level, bridge->inflammation_state.phi_factor);
    return 0;
}

int swarm_consciousness_immune_trigger_from_low_phi(swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_low_phi_stress) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->modulation.current_phi < 0.3f) {
        bridge->modulation.low_phi_triggered = true;
        NIMCP_LOGGING_INFO("Low phi triggered stress response: phi=%.2f",
                          bridge->modulation.current_phi);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_consciousness_immune_boost_from_high_phi(swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_high_phi_boost) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->modulation.current_phi > 0.8f) {
        bridge->modulation.il10_from_phi += 0.1f;
        bridge->modulation.low_phi_triggered = false;
        NIMCP_LOGGING_DEBUG("High phi boosted IL-10: total=%.2f",
                           bridge->modulation.il10_from_phi);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_consciousness_immune_bridge_update(swarm_consciousness_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;

    swarm_consciousness_immune_apply_cytokine_effects(bridge);
    swarm_consciousness_immune_apply_inflammation_effects(bridge);

    return 0;
}

float swarm_consciousness_immune_get_phi_factor(const swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.phi_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

float swarm_consciousness_immune_get_integration_factor(const swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.integration_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

bool swarm_consciousness_immune_is_fragmented(const swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool fragmented = bridge->inflammation_state.awareness_fragmented;
    nimcp_mutex_unlock(bridge->base.mutex);

    return fragmented;
}

int swarm_consciousness_immune_connect_bio_async(swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect to bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->bio_async_connected) {
        NIMCP_LOGGING_INFO("Swarm consciousness-immune bridge already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SWARM_CONSCIOUSNESS,
        .module_name = "swarm_consciousness_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Swarm consciousness-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int swarm_consciousness_immune_disconnect_bio_async(swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect from bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_consciousness_immune_disconnect_bio_async: bridge is NULL");
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
    NIMCP_LOGGING_INFO("Swarm consciousness-immune bridge disconnected from bio-async router");

    return 0;
}

bool swarm_consciousness_immune_is_bio_async_connected(const swarm_consciousness_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
