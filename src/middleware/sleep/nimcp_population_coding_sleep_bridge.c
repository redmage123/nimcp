/**
 * @file nimcp_population_coding_sleep_bridge.c
 * @brief Sleep-Population Coding Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "middleware/sleep/nimcp_population_coding_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(population_coding_sleep_bridge)

struct population_coding_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    population_coding_sleep_config_t config;
    sleep_system_t sleep_system;
    population_coding_sleep_effects_t effects;
    bool callback_registered;
};

/* Forward declarations */
static void population_coding_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update population coding parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Encoding precision degrades during sleep
 * - Population synchrony increases in NREM (slow waves)
 * - Sparsity increases as sleep deepens
 * - REM enables creative but lower-precision coding
 */
static void population_coding_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    population_coding_sleep_bridge_t bridge = (population_coding_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Population coding bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_precision_modulation) {
        float precision_base = population_coding_sleep_get_precision_factor(new_state);
        bridge->effects.encoding_precision_factor =
            1.0f + (precision_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_synchrony_modulation) {
        bridge->effects.synchrony_threshold =
            population_coding_sleep_get_synchrony_factor(new_state);
    }

    if (bridge->config.enable_sparsity_modulation) {
        bridge->effects.sparsity_target =
            population_coding_sleep_get_sparsity_factor(new_state);
    }

    bridge->effects.encoding_enabled = (new_state != SLEEP_STATE_DEEP_NREM) ||
                                        bridge->effects.encoding_precision_factor > 0.25f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Population coding modulated: precision=%.2f, synchrony=%.2f, sparsity=%.2f",
                        bridge->effects.encoding_precision_factor,
                        bridge->effects.synchrony_threshold,
                        bridge->effects.sparsity_target);
}

int population_coding_sleep_default_config(population_coding_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_precision_modulation = true;
    config->enable_synchrony_modulation = true;
    config->enable_sparsity_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

population_coding_sleep_bridge_t population_coding_sleep_bridge_create(
    const population_coding_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("population_coding_sleep_bridge_create: NULL sleep_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "population_coding_sleep_bridge_create: NULL sleep_system");
        return NULL;
    }

    struct population_coding_sleep_bridge_struct* bridge =
        (struct population_coding_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct population_coding_sleep_bridge_struct));
    NIMCP_API_CHECK_ALLOC_SIZE(bridge, sizeof(struct population_coding_sleep_bridge_struct),
        "population_coding_sleep_bridge_create: failed to allocate bridge");

    memset(bridge, 0, sizeof(struct population_coding_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        population_coding_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.encoding_precision_factor = 1.0f;
    bridge->effects.synchrony_threshold = 0.5f;
    bridge->effects.sparsity_target = 0.1f;
    bridge->effects.encoding_enabled = true;

    if (bridge_base_init(&bridge->base, 0, "population_coding_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(nimcp_platform_mutex_t),
            "population_coding_sleep_bridge_create: failed to allocate mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        population_coding_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for population coding bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    population_coding_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Population coding-sleep bridge created");
    return bridge;
}

void population_coding_sleep_bridge_destroy(population_coding_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            population_coding_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for population coding bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int population_coding_sleep_update(population_coding_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_precision_modulation) {
        float precision_base = population_coding_sleep_get_precision_factor(state);
        bridge->effects.encoding_precision_factor =
            1.0f + (precision_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_synchrony_modulation) {
        bridge->effects.synchrony_threshold =
            population_coding_sleep_get_synchrony_factor(state);
    }

    if (bridge->config.enable_sparsity_modulation) {
        bridge->effects.sparsity_target =
            population_coding_sleep_get_sparsity_factor(state);
    }

    bridge->effects.encoding_enabled = (state != SLEEP_STATE_DEEP_NREM) ||
                                        bridge->effects.encoding_precision_factor > 0.25f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int population_coding_sleep_get_effects(
    const population_coding_sleep_bridge_t bridge,
    population_coding_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "population_coding_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float population_coding_sleep_get_precision(
    const population_coding_sleep_bridge_t bridge)
{
    if (!bridge) return 1.0f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.encoding_precision_factor;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float population_coding_sleep_get_synchrony_threshold(
    const population_coding_sleep_bridge_t bridge)
{
    if (!bridge) return 0.5f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.synchrony_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float population_coding_sleep_get_sparsity_target(
    const population_coding_sleep_bridge_t bridge)
{
    if (!bridge) return 0.1f;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.sparsity_target;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float population_coding_sleep_get_precision_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return POPULATION_CODING_SLEEP_PRECISION_AWAKE;
        case SLEEP_STATE_DROWSY:     return POPULATION_CODING_SLEEP_PRECISION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return POPULATION_CODING_SLEEP_PRECISION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return POPULATION_CODING_SLEEP_PRECISION_DEEP_NREM;
        case SLEEP_STATE_REM:        return POPULATION_CODING_SLEEP_PRECISION_REM;
        default:                     return POPULATION_CODING_SLEEP_PRECISION_AWAKE;
    }
}

float population_coding_sleep_get_synchrony_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return POPULATION_CODING_SLEEP_SYNCHRONY_AWAKE;
        case SLEEP_STATE_DROWSY:     return POPULATION_CODING_SLEEP_SYNCHRONY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return POPULATION_CODING_SLEEP_SYNCHRONY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return POPULATION_CODING_SLEEP_SYNCHRONY_DEEP_NREM;
        case SLEEP_STATE_REM:        return POPULATION_CODING_SLEEP_SYNCHRONY_REM;
        default:                     return POPULATION_CODING_SLEEP_SYNCHRONY_AWAKE;
    }
}

float population_coding_sleep_get_sparsity_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return POPULATION_CODING_SLEEP_SPARSITY_AWAKE;
        case SLEEP_STATE_DROWSY:     return POPULATION_CODING_SLEEP_SPARSITY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return POPULATION_CODING_SLEEP_SPARSITY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return POPULATION_CODING_SLEEP_SPARSITY_DEEP_NREM;
        case SLEEP_STATE_REM:        return POPULATION_CODING_SLEEP_SPARSITY_REM;
        default:                     return POPULATION_CODING_SLEEP_SPARSITY_AWAKE;
    }
}
