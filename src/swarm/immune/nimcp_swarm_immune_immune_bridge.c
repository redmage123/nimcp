/**
 * @file nimcp_swarm_immune_immune_bridge.c
 * @brief Swarm Immune-Brain Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Bidirectional integration between brain and swarm immune systems
 * WHY:  Coordinate immune responses across swarm and brain layers
 * HOW:  Brain inflammation affects swarm threat detection; swarm threats trigger brain immune
 *
 * @author NIMCP Development Team
 */

#include "swarm/immune/nimcp_swarm_immune_immune_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_immune_immune_bridge)

struct swarm_immune_immune_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    swarm_immune_immune_config_t config;
    brain_immune_system_t* brain_immune;
    NimcpSwarmImmuneSystem* swarm_immune;
    brain_to_swarm_immune_effects_t brain_effects;
    swarm_to_brain_immune_effects_t swarm_effects;
    immune_coordination_state_t coordination;
    void* bio_ctx;
    bool bio_async_connected;
};

static float get_detection_factor_for_level(brain_inflammation_level_t level)
{
    switch (level) {
        case INFLAMMATION_NONE:     return BRAIN_INFLAMMATION_NONE_DETECTION;
        case INFLAMMATION_LOCAL:    return BRAIN_INFLAMMATION_LOCAL_DETECTION;
        case INFLAMMATION_REGIONAL: return BRAIN_INFLAMMATION_REGIONAL_DETECTION;
        case INFLAMMATION_SYSTEMIC: return BRAIN_INFLAMMATION_SYSTEMIC_DETECTION;
        case INFLAMMATION_STORM:    return BRAIN_INFLAMMATION_STORM_DETECTION;
        default:                    return BRAIN_INFLAMMATION_NONE_DETECTION;
    }
}

int swarm_immune_immune_default_config(swarm_immune_immune_config_t* config)
{
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_brain_to_swarm = true;
    config->enable_swarm_to_brain = true;
    config->enable_cross_immunity = true;
    config->enable_coordinated_response = true;
    config->coordination_strength = 1.0f;

    return 0;
}

swarm_immune_immune_bridge_t* swarm_immune_immune_bridge_create(
    const swarm_immune_immune_config_t* config,
    brain_immune_system_t* brain_immune,
    NimcpSwarmImmuneSystem* swarm_immune)
{
    if (!config || !brain_immune) {
        NIMCP_LOGGING_ERROR("Invalid parameters for swarm immune-immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_immune_immune_bridge_create: required parameter is NULL (config, brain_immune)");
        return NULL;
    }

    swarm_immune_immune_bridge_t* bridge =
        (swarm_immune_immune_bridge_t*)nimcp_malloc(sizeof(swarm_immune_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate swarm immune-immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_immune_immune_bridge_create: allocation failed");

        return NULL;
    }

    memset(bridge, 0, sizeof(swarm_immune_immune_bridge_t));
    memcpy(&bridge->config, config, sizeof(swarm_immune_immune_config_t));
    bridge->brain_immune = brain_immune;
    bridge->swarm_immune = swarm_immune;

    if (bridge_base_init(&bridge->base, 0, "swarm_immune_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for swarm immune-immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_immune_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->brain_effects.detection_sensitivity_factor = 1.0f;
    bridge->brain_effects.response_intensity_factor = 1.0f;
    bridge->brain_effects.memory_consolidation_factor = 1.0f;

    bridge->swarm_effects.threat_presentation_rate = 0.0f;
    bridge->swarm_effects.antigen_severity = 0.0f;
    bridge->swarm_effects.cross_reactive_matches = 0;

    bridge->coordination.brain_level = INFLAMMATION_NONE;
    bridge->coordination.swarm_threat_level = 0.0f;
    bridge->coordination.coordinated_response_active = false;
    bridge->coordination.cross_immunity_strength = 0.0f;

    NIMCP_LOGGING_INFO("Swarm immune-immune bridge created");
    return bridge;
}

void swarm_immune_immune_bridge_destroy(swarm_immune_immune_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Swarm immune-immune bridge destroyed");
}

int swarm_immune_immune_apply_brain_effects(swarm_immune_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->brain_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_immune_immune_apply_brain_effects: required parameter is NULL (bridge, bridge->brain_immune)");
        return -1;
    }
    if (!bridge->config.enable_brain_to_swarm) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    brain_immune_stats_t stats;
    if (brain_immune_get_stats(bridge->brain_immune, &stats) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_immune_immune_apply_brain_effects: validation failed");
        return -1;
    }
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->coordination.brain_level = level;

    float factor = get_detection_factor_for_level(level);
    bridge->brain_effects.detection_sensitivity_factor =
        1.0f + (factor - 1.0f) * bridge->config.coordination_strength;
    bridge->brain_effects.response_intensity_factor =
        bridge->brain_effects.detection_sensitivity_factor;
    bridge->brain_effects.memory_consolidation_factor =
        (level >= INFLAMMATION_SYSTEMIC) ? 1.5f : 1.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied brain-to-swarm effects: detection=%.2f",
                        bridge->brain_effects.detection_sensitivity_factor);
    return 0;
}

int swarm_immune_immune_apply_swarm_effects(swarm_immune_immune_bridge_t* bridge)
{
    if (!bridge || !bridge->brain_immune) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_immune_immune_apply_swarm_effects: required parameter is NULL (bridge, bridge->brain_immune)");
        return -1;
    }
    if (!bridge->config.enable_swarm_to_brain) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->swarm_effects.threat_presentation_rate =
        bridge->coordination.swarm_threat_level * bridge->config.coordination_strength;

    if (bridge->coordination.swarm_threat_level > SWARM_THREAT_CRITICAL_SEVERITY) {
        bridge->swarm_effects.antigen_severity = 1.0f;
    } else if (bridge->coordination.swarm_threat_level > SWARM_THREAT_SEVERE_SEVERITY) {
        bridge->swarm_effects.antigen_severity = 0.8f;
    } else if (bridge->coordination.swarm_threat_level > SWARM_THREAT_MODERATE_SEVERITY) {
        bridge->swarm_effects.antigen_severity = 0.5f;
    } else if (bridge->coordination.swarm_threat_level > SWARM_THREAT_MINOR_SEVERITY) {
        bridge->swarm_effects.antigen_severity = 0.3f;
    } else {
        bridge->swarm_effects.antigen_severity = 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied swarm-to-brain effects: severity=%.2f",
                        bridge->swarm_effects.antigen_severity);
    return 0;
}

int swarm_immune_immune_present_swarm_threat(swarm_immune_immune_bridge_t* bridge,
                                              const uint8_t* pattern, size_t len, float severity)
{
    if (!bridge || !pattern || len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_immune_immune_present_swarm_threat: required parameter is NULL (bridge, pattern)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->coordination.swarm_threat_level = severity;

    if (bridge->config.enable_swarm_to_brain && severity > SWARM_THREAT_MODERATE_SEVERITY) {
        NIMCP_LOGGING_INFO("Presenting swarm threat to brain immune: severity=%.2f", severity);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_immune_immune_coordinate_response(swarm_immune_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_coordinated_response) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bool brain_active = (bridge->coordination.brain_level >= INFLAMMATION_REGIONAL);
    bool swarm_active = (bridge->coordination.swarm_threat_level > SWARM_THREAT_MODERATE_SEVERITY);

    bridge->coordination.coordinated_response_active = brain_active || swarm_active;

    if (bridge->coordination.coordinated_response_active) {
        NIMCP_LOGGING_INFO("Coordinated immune response active: brain=%d, swarm=%.2f",
                          bridge->coordination.brain_level,
                          bridge->coordination.swarm_threat_level);
    }

    if (bridge->config.enable_cross_immunity) {
        bridge->coordination.cross_immunity_strength =
            (brain_active && swarm_active) ? 1.0f : 0.5f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int swarm_immune_immune_bridge_update(swarm_immune_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    (void)delta_ms;

    swarm_immune_immune_apply_brain_effects(bridge);
    swarm_immune_immune_apply_swarm_effects(bridge);
    swarm_immune_immune_coordinate_response(bridge);

    return 0;
}

float swarm_immune_immune_get_detection_factor(const swarm_immune_immune_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->brain_effects.detection_sensitivity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

bool swarm_immune_immune_is_coordinated(const swarm_immune_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool coordinated = bridge->coordination.coordinated_response_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return coordinated;
}

int swarm_immune_immune_connect_bio_async(swarm_immune_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect to bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_immune_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    if (bridge->bio_async_connected) {
        NIMCP_LOGGING_INFO("Swarm immune-immune bridge already connected to bio-async");
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SWARM_IMMUNE,
        .module_name = "swarm_immune_immune_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Swarm immune-immune bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

int swarm_immune_immune_disconnect_bio_async(swarm_immune_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect from bio-async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_immune_immune_disconnect_bio_async: bridge is NULL");
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
    NIMCP_LOGGING_INFO("Swarm immune-immune bridge disconnected from bio-async router");

    return 0;
}

bool swarm_immune_immune_is_bio_async_connected(const swarm_immune_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}
