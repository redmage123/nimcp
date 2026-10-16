/**
 * @file nimcp_rate_limiter_sleep_bridge.c
 * @brief Sleep-Rate Limiter Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * WHAT: Modulates rate limiting based on sleep state
 * WHY:  Rate limits can be relaxed during low-activity sleep states
 * HOW:  Sleep state callbacks dynamically adjust rate limit parameters
 *
 * @author NIMCP Development Team
 */

#include "security/sleep/nimcp_rate_limiter_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rate_limiter_sleep_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


struct rate_limiter_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    rate_limiter_sleep_config_t config;
    sleep_system_t sleep_system;
    rate_limiter_sleep_effects_t effects;
    bool callback_registered;
};

float rate_limiter_sleep_get_rate_factor(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return RATE_LIMITER_SLEEP_RATE_AWAKE;
        case SLEEP_STATE_DROWSY:     return RATE_LIMITER_SLEEP_RATE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return RATE_LIMITER_SLEEP_RATE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return RATE_LIMITER_SLEEP_RATE_DEEP_NREM;
        case SLEEP_STATE_REM:        return RATE_LIMITER_SLEEP_RATE_REM;
        default:                     return RATE_LIMITER_SLEEP_RATE_AWAKE;
    }
}

float rate_limiter_sleep_get_burst_factor(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return RATE_LIMITER_SLEEP_BURST_AWAKE;
        case SLEEP_STATE_DROWSY:     return RATE_LIMITER_SLEEP_BURST_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return RATE_LIMITER_SLEEP_BURST_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return RATE_LIMITER_SLEEP_BURST_DEEP_NREM;
        case SLEEP_STATE_REM:        return RATE_LIMITER_SLEEP_BURST_REM;
        default:                     return RATE_LIMITER_SLEEP_BURST_AWAKE;
    }
}

static void rate_limiter_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    rate_limiter_sleep_bridge_t bridge = (rate_limiter_sleep_bridge_t)user_data;
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;
    bridge->effects.rate_limit_factor = rate_limiter_sleep_get_rate_factor(new_state);
    bridge->effects.burst_capacity_factor = rate_limiter_sleep_get_burst_factor(new_state);
    bridge->effects.penalty_threshold_factor = bridge->effects.rate_limit_factor;
    bridge->effects.limits_relaxed = (new_state != SLEEP_STATE_AWAKE);

    NIMCP_LOGGING_DEBUG("Rate limiter sleep state changed to %d, rate=%.2f, burst=%.2f",
                        new_state, bridge->effects.rate_limit_factor,
                        bridge->effects.burst_capacity_factor);

    nimcp_mutex_unlock(bridge->base.mutex);
}

int rate_limiter_sleep_default_config(rate_limiter_sleep_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_limiter_sleep_default_config: config is NULL");
        return -1;
    }

    config->enable_rate_modulation = true;
    config->enable_burst_modulation = true;
    config->modulation_strength = 1.0f;

    return 0;
}

rate_limiter_sleep_bridge_t rate_limiter_sleep_bridge_create(
    const rate_limiter_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!config || !sleep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_limiter_sleep_bridge_create: config or sleep_system is NULL");
        NIMCP_LOGGING_ERROR("Invalid parameters for rate limiter sleep bridge creation");
        return NULL;
    }

    rate_limiter_sleep_bridge_t bridge =
        (rate_limiter_sleep_bridge_t)nimcp_malloc(sizeof(struct rate_limiter_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rate_limiter_sleep_bridge_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate rate limiter sleep bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct rate_limiter_sleep_bridge_struct));
    memcpy(&bridge->config, config, sizeof(rate_limiter_sleep_config_t));
    bridge->sleep_system = sleep_system;

    if (bridge_base_init(&bridge->base, 0, "rate_limiter_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "rate_limiter_sleep_bridge_create: mutex creation failed");
        NIMCP_LOGGING_ERROR("Failed to create mutex for rate limiter sleep bridge");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->effects.rate_limit_factor = RATE_LIMITER_SLEEP_RATE_AWAKE;
    bridge->effects.burst_capacity_factor = RATE_LIMITER_SLEEP_BURST_AWAKE;
    bridge->effects.penalty_threshold_factor = 1.0f;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.sleep_pressure = 0.0f;
    bridge->effects.limits_relaxed = false;

    bridge->callback_registered = sleep_register_state_callback(
        sleep_system, rate_limiter_on_sleep_state_change, bridge);

    if (bridge->callback_registered) {
        sleep_state_t initial = sleep_get_current_state(sleep_system);
        rate_limiter_on_sleep_state_change(initial, bridge);
    }

    NIMCP_LOGGING_INFO("Rate limiter sleep bridge created");
    return bridge;
}

void rate_limiter_sleep_bridge_destroy(rate_limiter_sleep_bridge_t bridge)
{
    if (!bridge) return;

    if (bridge->callback_registered && bridge->sleep_system) {
        sleep_unregister_state_callback(bridge->sleep_system,
                                        rate_limiter_on_sleep_state_change, bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Rate limiter sleep bridge destroyed");
}

int rate_limiter_sleep_update(rate_limiter_sleep_bridge_t bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_limiter_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.sleep_pressure = sleep_get_pressure(bridge->sleep_system);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rate_limiter_sleep_get_effects(const rate_limiter_sleep_bridge_t bridge,
                                    rate_limiter_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rate_limiter_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->effects, sizeof(rate_limiter_sleep_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float rate_limiter_sleep_get_effective_rate(const rate_limiter_sleep_bridge_t bridge, float base)
{
    if (!bridge) return base;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->config.enable_rate_modulation ?
        bridge->effects.rate_limit_factor : 1.0f;
    float modulated = 1.0f + (factor - 1.0f) * bridge->config.modulation_strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return base * modulated;
}

float rate_limiter_sleep_get_burst_capacity(const rate_limiter_sleep_bridge_t bridge, float base)
{
    if (!bridge) return base;

    nimcp_mutex_lock(bridge->base.mutex);
    float factor = bridge->config.enable_burst_modulation ?
        bridge->effects.burst_capacity_factor : 1.0f;
    float modulated = 1.0f + (factor - 1.0f) * bridge->config.modulation_strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return base * modulated;
}

bool rate_limiter_sleep_is_relaxed(const rate_limiter_sleep_bridge_t bridge)
{
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool relaxed = bridge->effects.limits_relaxed;
    nimcp_mutex_unlock(bridge->base.mutex);

    return relaxed;
}
