/**
 * @file nimcp_swarm_brain_immune_bridge.c
 * @brief Swarm Brain-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates swarm brain coordination based on immune state
 * WHY:  Inflammation affects swarm coordination; swarm stress triggers immune
 * HOW:  Cytokines reduce coordination; low coherence triggers inflammation
 *
 * @author NIMCP Development Team
 */

#include "swarm/immune/nimcp_swarm_brain_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_brain_immune_bridge)

struct swarm_brain_immune_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    swarm_brain_immune_config_t config;
    brain_immune_system_t* immune_system;
    void* swarm_brain;
    cytokine_swarm_effects_t cytokine_effects;
    inflammation_swarm_state_t inflammation_state;
    swarm_immune_modulation_t modulation;
    void* bio_ctx;
    bool bio_async_connected;
};

static float get_coherence_factor_for_level(brain_inflammation_level_t level)
{
    switch (level) {
        case INFLAMMATION_NONE:     return INFLAMMATION_NONE_COHERENCE_FACTOR;
        case INFLAMMATION_LOCAL:    return INFLAMMATION_LOCAL_COHERENCE_FACTOR;
        case INFLAMMATION_REGIONAL: return INFLAMMATION_REGIONAL_COHERENCE_FACTOR;
        case INFLAMMATION_SYSTEMIC: return INFLAMMATION_SYSTEMIC_COHERENCE_FACTOR;
        case INFLAMMATION_STORM:    return INFLAMMATION_STORM_COHERENCE_FACTOR;
        default:                    return INFLAMMATION_NONE_COHERENCE_FACTOR;
    }
}

int swarm_brain_immune_default_config(swarm_brain_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_cytokine_impairment = true;
    config->enable_inflammation_effects = true;
    config->enable_stress_trigger = true;
    config->enable_cohesion_boost = true;
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

swarm_brain_immune_bridge_t* swarm_brain_immune_bridge_create(
    const swarm_brain_immune_config_t* config,
    brain_immune_system_t* immune_system,
    void* swarm_brain)
{
    if (!config || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for swarm brain immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_immune_bridge_create: required parameter is NULL (config, immune_system)");
        return NULL;
    }

    swarm_brain_immune_bridge_t* bridge =
        (swarm_brain_immune_bridge_t*)nimcp_malloc(sizeof(swarm_brain_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm brain immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_immune_bridge_create: allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(swarm_brain_immune_bridge_t));
    memcpy(&bridge->config, config, sizeof(swarm_brain_immune_config_t));
    bridge->immune_system = immune_system;
    bridge->swarm_brain = swarm_brain;

    if (bridge_base_init(&bridge->base, 0, "swarm_brain_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for swarm brain immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_brain_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->inflammation_state.current_level = INFLAMMATION_NONE;
    bridge->inflammation_state.coherence_factor = INFLAMMATION_NONE_COHERENCE_FACTOR;
    bridge->inflammation_state.fragmentation_risk = 0.0f;
    bridge->inflammation_state.consensus_impairment = 0.0f;

    bridge->modulation.coherence_level = 1.0f;
    bridge->modulation.peer_count = 0;
    bridge->modulation.immune_triggered = false;
    bridge->modulation.il10_release = 0.0f;

    NIMCP_LOGGING_INFO("Swarm brain immune bridge created");
    return bridge;
}

void swarm_brain_immune_bridge_destroy(swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Swarm brain immune bridge destroyed");
}

int swarm_brain_immune_apply_cytokine_effects(swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_cytokine_impairment) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    float sensitivity = bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.il1_deficit = CYTOKINE_IL1_COORDINATION_IMPACT * sensitivity;
    bridge->cytokine_effects.il6_deficit = CYTOKINE_IL6_COORDINATION_IMPACT * sensitivity;
    bridge->cytokine_effects.tnf_deficit = CYTOKINE_TNF_COORDINATION_IMPACT * sensitivity;
    bridge->cytokine_effects.il10_recovery = CYTOKINE_IL10_COORDINATION_IMPACT * sensitivity;

    bridge->cytokine_effects.total_coherence_reduction =
        bridge->cytokine_effects.il1_deficit +
        bridge->cytokine_effects.il6_deficit +
        bridge->cytokine_effects.tnf_deficit +
        bridge->cytokine_effects.il10_recovery;

    bridge->cytokine_effects.consensus_delay_factor =
        1.0f - bridge->cytokine_effects.total_coherence_reduction;

    if (bridge->cytokine_effects.consensus_delay_factor < 0.3f) {
        bridge->cytokine_effects.consensus_delay_factor = 0.3f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine effects: coherence reduction=%.2f",
                        bridge->cytokine_effects.total_coherence_reduction);
    return 0;
}

int swarm_brain_immune_apply_inflammation_effects(swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_inflammation_effects) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->immune_system, &stats) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_brain_immune_apply_inflammation_effects: validation failed");
        return -1;
    }
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;
    bridge->inflammation_state.coherence_factor =
        get_coherence_factor_for_level(level) * bridge->config.inflammation_sensitivity;

    bridge->inflammation_state.fragmentation_risk =
        (level >= INFLAMMATION_SYSTEMIC) ? 0.5f : 0.0f;
    bridge->inflammation_state.consensus_impairment =
        1.0f - bridge->inflammation_state.coherence_factor;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation effects: level=%d, coherence=%.2f",
                        level, bridge->inflammation_state.coherence_factor);
    return 0;
}

float swarm_brain_immune_compute_coherence(const swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float base_coherence = bridge->modulation.coherence_level;
    float cytokine_factor = 1.0f + bridge->cytokine_effects.total_coherence_reduction;
    float inflammation_factor = bridge->inflammation_state.coherence_factor;
    float coherence = base_coherence * cytokine_factor * inflammation_factor;
    if (coherence < 0.0f) coherence = 0.0f;
    if (coherence > 1.0f) coherence = 1.0f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return coherence;
}

int swarm_brain_immune_trigger_from_stress(swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_immune_trigger_from_stress: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_stress_trigger) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->modulation.coherence_level < SWARM_STRESS_TRIGGER_THRESHOLD) {
        bridge->modulation.immune_triggered = true;
        NIMCP_LOGGING_INFO("Swarm stress triggered immune response: coherence=%.2f",
                          bridge->modulation.coherence_level);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_brain_immune_boost_from_cohesion(swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_immune_boost_from_cohesion: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }
    if (!bridge->config.enable_cohesion_boost) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->modulation.coherence_level > SWARM_COHERENCE_BOOST_THRESHOLD) {
        bridge->modulation.il10_release += 0.1f;
        NIMCP_LOGGING_DEBUG("High cohesion released IL-10: total=%.2f",
                           bridge->modulation.il10_release);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_brain_immune_bridge_update(swarm_brain_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;

    swarm_brain_immune_apply_cytokine_effects(bridge);
    swarm_brain_immune_apply_inflammation_effects(bridge);

    return 0;
}

int swarm_brain_immune_get_cytokine_effects(const swarm_brain_immune_bridge_t* bridge,
                                             cytokine_swarm_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_swarm_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int swarm_brain_immune_get_inflammation_state(const swarm_brain_immune_bridge_t* bridge,
                                               inflammation_swarm_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_swarm_state_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool swarm_brain_immune_is_fragmented(const swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool fragmented = bridge->inflammation_state.fragmentation_risk > 0.3f;
    nimcp_mutex_unlock(bridge->base.mutex);

    return fragmented;
}

float swarm_brain_immune_get_coherence_factor(const swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->inflammation_state.coherence_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

int swarm_brain_immune_connect_bio_async(swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect to bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->bio_async_connected) {
        NIMCP_LOGGING_INFO("Swarm brain-immune bridge already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SWARM_BRAIN,
        .module_name = "swarm_brain_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Swarm brain-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int swarm_brain_immune_disconnect_bio_async(swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect from bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_immune_disconnect_bio_async: bridge is NULL");
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
    NIMCP_LOGGING_INFO("Swarm brain-immune bridge disconnected from bio-async router");

    return 0;
}

bool swarm_brain_immune_is_bio_async_connected(const swarm_brain_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
