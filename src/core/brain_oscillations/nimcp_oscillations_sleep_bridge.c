/**
 * @file nimcp_oscillations_sleep_bridge.c
 * @brief Sleep-Oscillations Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 *
 * Brain oscillations define sleep stages - this is the core interface.
 */

#include "core/brain_oscillations/nimcp_oscillations_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(oscillations_sleep_bridge)

struct oscillations_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    oscillations_sleep_config_t config;
    sleep_system_t sleep_system;
    oscillations_sleep_effects_t effects;
    bool callback_registered;  /* Track if callback is registered for cleanup */
};

/* Forward declarations */
static void oscillations_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update oscillation parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Oscillations ARE sleep states (delta=deep, theta=REM/light, alpha=drowsy)
 * - This is the most fundamental sleep integration
 * - Sleep spindles and ripples for memory consolidation
 */
static void oscillations_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    oscillations_sleep_bridge_t bridge = (oscillations_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Oscillations bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_frequency_modulation) {
        bridge->effects.dominant_frequency = oscillations_sleep_freq_for_state(new_state);
        bridge->effects.dominant_band = oscillations_sleep_band_for_state(new_state);
    }

    if (bridge->config.enable_power_modulation) {
        /* Set band power distribution based on sleep state */
        bridge->effects.delta_power = (new_state == SLEEP_STATE_DEEP_NREM) ? 0.8f :
                                      (new_state == SLEEP_STATE_LIGHT_NREM) ? 0.3f : 0.1f;
        bridge->effects.theta_power = (new_state == SLEEP_STATE_REM ||
                                       new_state == SLEEP_STATE_LIGHT_NREM) ? 0.6f :
                                      (new_state == SLEEP_STATE_DROWSY) ? 0.3f : 0.2f;
        bridge->effects.alpha_power = (new_state == SLEEP_STATE_DROWSY) ? 0.7f :
                                      (new_state == SLEEP_STATE_AWAKE) ? 0.3f : 0.1f;
        bridge->effects.beta_power = (new_state == SLEEP_STATE_AWAKE) ? 0.6f :
                                     (new_state == SLEEP_STATE_DROWSY) ? 0.3f : 0.1f;
        bridge->effects.gamma_power = (new_state == SLEEP_STATE_AWAKE) ? 0.5f : 0.1f;
    }

    if (bridge->config.enable_spindle_generation) {
        bridge->effects.spindle_activity = oscillations_sleep_spindle_for_state(new_state);
        /* Sharp wave ripples during NREM for memory replay */
        bridge->effects.ripple_activity = (new_state == SLEEP_STATE_DEEP_NREM) ? 0.7f :
                                          (new_state == SLEEP_STATE_LIGHT_NREM) ? 0.4f : 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Oscillations modulated: freq=%.1fHz, band=%d, spindle=%.2f",
                        bridge->effects.dominant_frequency,
                        bridge->effects.dominant_band,
                        bridge->effects.spindle_activity);
}

int oscillations_sleep_default_config(oscillations_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_frequency_modulation = true;
    config->enable_power_modulation = true;
    config->enable_spindle_generation = true;
    config->modulation_strength = 1.0f;
    return 0;
}

oscillations_sleep_bridge_t oscillations_sleep_bridge_create(
    const oscillations_sleep_config_t* config,
    sleep_system_t sleep)
{
    if (!sleep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;

    }

    struct oscillations_sleep_bridge_struct* bridge =
        (struct oscillations_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct oscillations_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct oscillations_sleep_bridge_struct));

    if (config) bridge->config = *config;
    else oscillations_sleep_default_config(&bridge->config);

    bridge->sleep_system = sleep;
    bridge->effects.dominant_frequency = OSC_SLEEP_FREQ_AWAKE;
    bridge->effects.dominant_band = OSC_BAND_BETA;
    bridge->effects.spindle_activity = 0.0f;
    bridge->effects.ripple_activity = 0.0f;

    if (bridge_base_init(&bridge->base, 0, "oscillations_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        oscillations_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for oscillations bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    oscillations_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Oscillations-sleep bridge created");
    return bridge;
}

void oscillations_sleep_bridge_destroy(oscillations_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            oscillations_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for oscillations bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int oscillations_sleep_update(oscillations_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_frequency_modulation) {
        bridge->effects.dominant_frequency = oscillations_sleep_freq_for_state(state);
        bridge->effects.dominant_band = oscillations_sleep_band_for_state(state);
    }

    if (bridge->config.enable_power_modulation) {
        /* Set band power distribution based on sleep state */
        bridge->effects.delta_power = (state == SLEEP_STATE_DEEP_NREM) ? 0.8f :
                                      (state == SLEEP_STATE_LIGHT_NREM) ? 0.3f : 0.1f;
        bridge->effects.theta_power = (state == SLEEP_STATE_REM ||
                                       state == SLEEP_STATE_LIGHT_NREM) ? 0.6f :
                                      (state == SLEEP_STATE_DROWSY) ? 0.3f : 0.2f;
        bridge->effects.alpha_power = (state == SLEEP_STATE_DROWSY) ? 0.7f :
                                      (state == SLEEP_STATE_AWAKE) ? 0.3f : 0.1f;
        bridge->effects.beta_power = (state == SLEEP_STATE_AWAKE) ? 0.6f :
                                     (state == SLEEP_STATE_DROWSY) ? 0.3f : 0.1f;
        bridge->effects.gamma_power = (state == SLEEP_STATE_AWAKE) ? 0.5f : 0.1f;
    }

    if (bridge->config.enable_spindle_generation) {
        bridge->effects.spindle_activity = oscillations_sleep_spindle_for_state(state);
        /* Sharp wave ripples during NREM for memory replay */
        bridge->effects.ripple_activity = (state == SLEEP_STATE_DEEP_NREM) ? 0.7f :
                                          (state == SLEEP_STATE_LIGHT_NREM) ? 0.4f : 0.0f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int oscillations_sleep_get_effects(
    const oscillations_sleep_bridge_t bridge,
    oscillations_sleep_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oscillations_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float oscillations_sleep_get_frequency(const oscillations_sleep_bridge_t bridge) {
    if (!bridge) return OSC_SLEEP_FREQ_AWAKE;
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.dominant_frequency;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

oscillation_band_t oscillations_sleep_get_band(const oscillations_sleep_bridge_t bridge) {
    if (!bridge) return OSC_BAND_BETA;
    nimcp_mutex_lock(bridge->base.mutex);
    oscillation_band_t result = bridge->effects.dominant_band;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float oscillations_sleep_freq_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return OSC_SLEEP_FREQ_AWAKE;
        case SLEEP_STATE_DROWSY:     return OSC_SLEEP_FREQ_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return OSC_SLEEP_FREQ_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return OSC_SLEEP_FREQ_DEEP_NREM;
        case SLEEP_STATE_REM:        return OSC_SLEEP_FREQ_REM;
        default:                     return OSC_SLEEP_FREQ_AWAKE;
    }
}

oscillation_band_t oscillations_sleep_band_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return OSC_BAND_BETA;
        case SLEEP_STATE_DROWSY:     return OSC_BAND_ALPHA;
        case SLEEP_STATE_LIGHT_NREM: return OSC_BAND_THETA;
        case SLEEP_STATE_DEEP_NREM:  return OSC_BAND_DELTA;
        case SLEEP_STATE_REM:        return OSC_BAND_THETA;
        default:                     return OSC_BAND_BETA;
    }
}

float oscillations_sleep_spindle_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return OSC_SLEEP_SPINDLE_AWAKE;
        case SLEEP_STATE_DROWSY:     return OSC_SLEEP_SPINDLE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return OSC_SLEEP_SPINDLE_LIGHT;
        case SLEEP_STATE_DEEP_NREM:  return OSC_SLEEP_SPINDLE_DEEP;
        case SLEEP_STATE_REM:        return OSC_SLEEP_SPINDLE_REM;
        default:                     return OSC_SLEEP_SPINDLE_AWAKE;
    }
}
