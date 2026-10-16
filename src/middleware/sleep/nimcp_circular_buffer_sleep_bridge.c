/**
 * @file nimcp_circular_buffer_sleep_bridge.c
 * @brief Sleep-Circular Buffer Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "middleware/sleep/nimcp_circular_buffer_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(circular_buffer_sleep_bridge)

struct circular_buffer_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    circular_buffer_sleep_config_t config;
    sleep_system_t sleep_system;
    circular_buffer_sleep_effects_t effects;
    bool callback_registered;
};

/* Forward declarations */
static void circular_buffer_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update circular buffer parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Working memory capacity degrades with sleep deprivation
 * - NREM sleep reduces online buffering, focuses on consolidation
 * - Deep sleep minimizes new encoding, maximizes replay
 * - REM enables creative buffering with emotional bias
 */
static void circular_buffer_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    circular_buffer_sleep_bridge_t bridge = (circular_buffer_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Circular buffer bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_capacity_modulation) {
        float capacity_base = circular_buffer_sleep_get_capacity_factor(new_state);
        bridge->effects.capacity_factor =
            1.0f + (capacity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_retention_modulation) {
        float retention_base = circular_buffer_sleep_get_retention_factor(new_state);
        bridge->effects.retention_duration_factor =
            1.0f + (retention_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_overflow_modulation) {
        bridge->effects.overflow_strategy =
            circular_buffer_sleep_get_overflow_strategy(new_state);
    }

    bridge->effects.buffering_enabled = (new_state != SLEEP_STATE_DEEP_NREM) ||
                                         bridge->effects.capacity_factor > 0.15f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Circular buffer modulated: capacity=%.2f, retention=%.2f",
                        bridge->effects.capacity_factor,
                        bridge->effects.retention_duration_factor);
}

int circular_buffer_sleep_default_config(circular_buffer_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_capacity_modulation = true;
    config->enable_retention_modulation = true;
    config->enable_overflow_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

circular_buffer_sleep_bridge_t circular_buffer_sleep_bridge_create(
    const circular_buffer_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("circular_buffer_sleep_bridge_create: NULL sleep_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "circular_buffer_sleep_bridge_create: NULL sleep_system");
        return NULL;
    }

    struct circular_buffer_sleep_bridge_struct* bridge =
        (struct circular_buffer_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct circular_buffer_sleep_bridge_struct));
    NIMCP_API_CHECK_ALLOC_SIZE(bridge, sizeof(struct circular_buffer_sleep_bridge_struct),
        "circular_buffer_sleep_bridge_create: failed to allocate bridge");

    memset(bridge, 0, sizeof(struct circular_buffer_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        circular_buffer_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.capacity_factor = 1.0f;
    bridge->effects.retention_duration_factor = 1.0f;
    bridge->effects.overflow_strategy = OVERFLOW_OVERWRITE;
    bridge->effects.buffering_enabled = true;

    if (bridge_base_init(&bridge->base, 0, "circular_buffer_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(nimcp_platform_mutex_t),
            "circular_buffer_sleep_bridge_create: failed to allocate mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        circular_buffer_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for circular buffer bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    circular_buffer_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Circular buffer-sleep bridge created");
    return bridge;
}

void circular_buffer_sleep_bridge_destroy(circular_buffer_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            circular_buffer_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for circular buffer bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int circular_buffer_sleep_update(circular_buffer_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_capacity_modulation) {
        float capacity_base = circular_buffer_sleep_get_capacity_factor(state);
        bridge->effects.capacity_factor =
            1.0f + (capacity_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_retention_modulation) {
        float retention_base = circular_buffer_sleep_get_retention_factor(state);
        bridge->effects.retention_duration_factor =
            1.0f + (retention_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_overflow_modulation) {
        bridge->effects.overflow_strategy =
            circular_buffer_sleep_get_overflow_strategy(state);
    }

    bridge->effects.buffering_enabled = (state != SLEEP_STATE_DEEP_NREM) ||
                                         bridge->effects.capacity_factor > 0.15f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int circular_buffer_sleep_get_effects(
    const circular_buffer_sleep_bridge_t bridge,
    circular_buffer_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "circular_buffer_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float circular_buffer_sleep_get_capacity(
    const circular_buffer_sleep_bridge_t bridge)
{
    if (!bridge) return 1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.capacity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float circular_buffer_sleep_get_retention_duration(
    const circular_buffer_sleep_bridge_t bridge)
{
    if (!bridge) return 1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.retention_duration_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float circular_buffer_sleep_get_capacity_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return CIRCULAR_BUFFER_SLEEP_CAPACITY_AWAKE;
        case SLEEP_STATE_DROWSY:     return CIRCULAR_BUFFER_SLEEP_CAPACITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return CIRCULAR_BUFFER_SLEEP_CAPACITY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return CIRCULAR_BUFFER_SLEEP_CAPACITY_DEEP_NREM;
        case SLEEP_STATE_REM:        return CIRCULAR_BUFFER_SLEEP_CAPACITY_REM;
        default:                     return CIRCULAR_BUFFER_SLEEP_CAPACITY_AWAKE;
    }
}

float circular_buffer_sleep_get_retention_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return CIRCULAR_BUFFER_SLEEP_RETENTION_AWAKE;
        case SLEEP_STATE_DROWSY:     return CIRCULAR_BUFFER_SLEEP_RETENTION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return CIRCULAR_BUFFER_SLEEP_RETENTION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return CIRCULAR_BUFFER_SLEEP_RETENTION_DEEP_NREM;
        case SLEEP_STATE_REM:        return CIRCULAR_BUFFER_SLEEP_RETENTION_REM;
        default:                     return CIRCULAR_BUFFER_SLEEP_RETENTION_AWAKE;
    }
}

overflow_strategy_t circular_buffer_sleep_get_overflow_strategy(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return OVERFLOW_OVERWRITE;
        case SLEEP_STATE_DROWSY:     return OVERFLOW_OVERWRITE;
        case SLEEP_STATE_LIGHT_NREM: return OVERFLOW_BLOCK;
        case SLEEP_STATE_DEEP_NREM:  return OVERFLOW_BLOCK;
        case SLEEP_STATE_REM:        return OVERFLOW_OVERWRITE;
        default:                     return OVERFLOW_OVERWRITE;
    }
}
