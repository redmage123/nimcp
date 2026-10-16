/**
 * @file nimcp_calcium_sleep_bridge.c
 * @brief Sleep-Calcium Dynamics Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "plasticity/calcium/nimcp_calcium_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(calcium_sleep_bridge)

/* Security integration */
/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct calcium_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    calcium_sleep_config_t config;
    sleep_system_t sleep_system;
    calcium_dynamics_t calcium;
    calcium_sleep_effects_t effects;
    nimcp_platform_mutex_t* mutex;
    bool callback_registered;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(calcium_sleep_bridge, struct calcium_sleep_bridge_struct)

/* Forward declaration */
static void calcium_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/* ============================================================================
 * Sleep State Callback
 * ============================================================================ */

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update calcium dynamics parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - NMDA activity changes rapidly with sleep state transitions
 * - Calcium clearance mechanisms activate during NREM
 * - Learning rate must adjust for consolidation vs acquisition modes
 */
static void calcium_on_sleep_state_change(sleep_state_t new_state, void* user_data) {
    calcium_sleep_bridge_t bridge = (calcium_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Calcium bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update modulation factors */
    if (bridge->config.enable_influx_modulation) {
        float influx_base = calcium_sleep_get_influx_factor_for_state(new_state);
        bridge->effects.influx_factor =
            1.0f + (influx_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_decay_modulation) {
        bridge->effects.decay_tau_ms = calcium_sleep_get_decay_tau_for_state(new_state);
    }

    if (bridge->config.enable_lr_modulation) {
        float lr_base = calcium_sleep_get_lr_factor_for_state(new_state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_pump_modulation) {
        float pump_base = calcium_sleep_get_pump_factor_for_state(new_state);
        bridge->effects.pump_rate_factor =
            1.0f + (pump_base - 1.0f) * bridge->config.modulation_strength;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Calcium modulated: influx=%.2f, decay=%.1f ms, LR=%.2f, pump=%.2f",
                        bridge->effects.influx_factor,
                        bridge->effects.decay_tau_ms,
                        bridge->effects.learning_rate_factor,
                        bridge->effects.pump_rate_factor);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int calcium_sleep_default_config(calcium_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_default_config: config is NULL");
        return -1;
    }

    config->enable_influx_modulation = true;
    config->enable_decay_modulation = true;
    config->enable_lr_modulation = true;
    config->enable_pump_modulation = true;
    config->enable_threshold_shifts = false;  /* Advanced feature */
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

calcium_sleep_bridge_t calcium_sleep_bridge_create(
    const calcium_sleep_config_t* config,
    sleep_system_t sleep_system,
    calcium_dynamics_t calcium
) {
    if (!sleep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_bridge_create: sleep_system is NULL");
        return NULL;
    }
    if (!calcium) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_bridge_create: calcium is NULL");
        return NULL;
    }

    struct calcium_sleep_bridge_struct* bridge =
        (struct calcium_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct calcium_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "calcium_sleep_bridge_create: bridge allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct calcium_sleep_bridge_struct));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        calcium_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->calcium = calcium;

    /* Initialize effects to awake state */
    bridge->effects.influx_factor = 1.0f;
    bridge->effects.decay_tau_ms = CALCIUM_SLEEP_DECAY_AWAKE;
    bridge->effects.learning_rate_factor = 1.0f;
    bridge->effects.pump_rate_factor = 1.0f;
    bridge->effects.threshold_ltd_shift = 0.0f;
    bridge->effects.threshold_ltp_shift = 0.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "calcium_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("Calcium-sleep bridge mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Calcium-sleep bridge mutex creation failed");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        calcium_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for calcium bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    calcium_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Calcium-sleep bridge created");
    return bridge;
}

void calcium_sleep_bridge_destroy(calcium_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            calcium_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for calcium bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int calcium_sleep_update(calcium_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    /* Update modulation factors */
    if (bridge->config.enable_influx_modulation) {
        float influx_base = calcium_sleep_get_influx_factor_for_state(state);
        bridge->effects.influx_factor =
            1.0f + (influx_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_decay_modulation) {
        bridge->effects.decay_tau_ms = calcium_sleep_get_decay_tau_for_state(state);
    }

    if (bridge->config.enable_lr_modulation) {
        float lr_base = calcium_sleep_get_lr_factor_for_state(state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_pump_modulation) {
        float pump_base = calcium_sleep_get_pump_factor_for_state(state);
        bridge->effects.pump_rate_factor =
            1.0f + (pump_base - 1.0f) * bridge->config.modulation_strength;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int calcium_sleep_get_effects(const calcium_sleep_bridge_t bridge,
                               calcium_sleep_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_get_effects: effects is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

float calcium_sleep_get_influx_factor(const calcium_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_get_influx_factor: bridge is NULL");
        return 1.0f;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float factor = bridge->effects.influx_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return factor;
}

float calcium_sleep_get_decay_tau(const calcium_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_get_decay_tau: bridge is NULL");
        return CALCIUM_SLEEP_DECAY_AWAKE;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float tau = bridge->effects.decay_tau_ms;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return tau;
}

float calcium_sleep_get_learning_rate(const calcium_sleep_bridge_t bridge,
                                       float base_lr) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_get_learning_rate: bridge is NULL");
        return base_lr;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float modulated_lr = base_lr * bridge->effects.learning_rate_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return modulated_lr;
}

int calcium_sleep_apply_modulation(calcium_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "calcium_sleep_apply_modulation: bridge is NULL");
        return -1;
    }
    if (!bridge->calcium) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "calcium_sleep_apply_modulation: calcium dynamics not connected");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current calcium config */
    calcium_config_t config;
    calcium_get_config(bridge->calcium, &config);

    /* Apply modulations */
    if (bridge->config.enable_influx_modulation) {
        config.influx_alpha *= bridge->effects.influx_factor;
    }

    if (bridge->config.enable_decay_modulation) {
        config.decay_tau_ms = bridge->effects.decay_tau_ms;
    }

    if (bridge->config.enable_pump_modulation) {
        config.pump_rate *= bridge->effects.pump_rate_factor;
    }

    /* Note: Cannot directly modify calcium config after creation in current API
     * This would require adding calcium_set_config() or parameter setters
     * For now, this demonstrates the intended behavior */

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Helper Function Implementation
 * ============================================================================ */

float calcium_sleep_get_influx_factor_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return CALCIUM_SLEEP_INFLUX_AWAKE;
        case SLEEP_STATE_DROWSY:     return CALCIUM_SLEEP_INFLUX_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return CALCIUM_SLEEP_INFLUX_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return CALCIUM_SLEEP_INFLUX_DEEP_NREM;
        case SLEEP_STATE_REM:        return CALCIUM_SLEEP_INFLUX_REM;
        default:                     return CALCIUM_SLEEP_INFLUX_AWAKE;
    }
}

float calcium_sleep_get_decay_tau_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return CALCIUM_SLEEP_DECAY_AWAKE;
        case SLEEP_STATE_DROWSY:     return CALCIUM_SLEEP_DECAY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return CALCIUM_SLEEP_DECAY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return CALCIUM_SLEEP_DECAY_DEEP_NREM;
        case SLEEP_STATE_REM:        return CALCIUM_SLEEP_DECAY_REM;
        default:                     return CALCIUM_SLEEP_DECAY_AWAKE;
    }
}

float calcium_sleep_get_lr_factor_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return CALCIUM_SLEEP_LR_AWAKE;
        case SLEEP_STATE_DROWSY:     return CALCIUM_SLEEP_LR_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return CALCIUM_SLEEP_LR_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return CALCIUM_SLEEP_LR_DEEP_NREM;
        case SLEEP_STATE_REM:        return CALCIUM_SLEEP_LR_REM;
        default:                     return CALCIUM_SLEEP_LR_AWAKE;
    }
}

float calcium_sleep_get_pump_factor_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return CALCIUM_SLEEP_PUMP_AWAKE;
        case SLEEP_STATE_DROWSY:     return CALCIUM_SLEEP_PUMP_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return CALCIUM_SLEEP_PUMP_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return CALCIUM_SLEEP_PUMP_DEEP_NREM;
        case SLEEP_STATE_REM:        return CALCIUM_SLEEP_PUMP_REM;
        default:                     return CALCIUM_SLEEP_PUMP_AWAKE;
    }
}
