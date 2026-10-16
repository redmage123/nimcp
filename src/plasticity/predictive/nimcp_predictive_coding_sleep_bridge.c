/**
 * @file nimcp_predictive_coding_sleep_bridge.c
 * @brief Sleep-Predictive Coding Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "plasticity/predictive/nimcp_predictive_coding_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_coding_sleep_bridge)

/* Security integration */

struct predictive_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    predictive_sleep_config_t config;
    sleep_system_t sleep_system;
    predictive_sleep_effects_t effects;
    nimcp_platform_mutex_t* mutex;
    bool callback_registered;  /* Track if callback is registered for cleanup */
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(predictive_sleep_bridge, struct predictive_sleep_bridge_struct)

/* Forward declarations */
static void predictive_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update predictive coding parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Predictive coding shifts from sensory-driven to internally-driven during sleep
 * - Precision on sensory errors decreases during NREM (offline processing)
 * - Top-down predictions strengthen during consolidation
 * - REM enables creative prediction exploration with reduced constraints
 */
static void predictive_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    predictive_sleep_bridge_t bridge = (predictive_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Predictive coding bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_prediction_modulation) {
        float pred_base = predictive_sleep_get_pred_factor(new_state);
        bridge->effects.prediction_strength_factor =
            1.0f + (pred_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_precision_modulation) {
        float precision_base = predictive_sleep_get_precision_factor(new_state);
        bridge->effects.precision_factor =
            1.0f + (precision_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_error_lr_modulation) {
        float lr_base = predictive_sleep_get_error_lr_factor(new_state);
        bridge->effects.error_learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.offline_consolidation = (new_state == SLEEP_STATE_DEEP_NREM);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Predictive coding modulated: pred=%.2f, precision=%.2f, error_lr=%.2f",
                        bridge->effects.prediction_strength_factor,
                        bridge->effects.precision_factor,
                        bridge->effects.error_learning_rate_factor);
}

int predictive_sleep_default_config(predictive_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_prediction_modulation = true;
    config->enable_precision_modulation = true;
    config->enable_error_lr_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

predictive_sleep_bridge_t predictive_sleep_bridge_create(
    const predictive_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_sleep_bridge_create: sleep_system is NULL");
        NIMCP_LOGGING_ERROR("predictive_sleep_bridge_create: NULL sleep_system");
        return NULL;
    }

    struct predictive_sleep_bridge_struct* bridge =
        (struct predictive_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct predictive_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_sleep_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct predictive_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        predictive_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.prediction_strength_factor = 1.0f;
    bridge->effects.precision_factor = 1.0f;
    bridge->effects.error_learning_rate_factor = 1.0f;
    bridge->effects.offline_consolidation = false;

    if (bridge_base_init(&bridge->base, 0, "predictive_coding_sleep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_sleep_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "predictive_sleep_bridge_create: mutex allocation failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        predictive_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for predictive coding bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    predictive_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Predictive coding-sleep bridge created");
    return bridge;
}

void predictive_sleep_bridge_destroy(predictive_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            predictive_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for predictive coding bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int predictive_sleep_update(predictive_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_prediction_modulation) {
        float pred_base = predictive_sleep_get_pred_factor(state);
        bridge->effects.prediction_strength_factor =
            1.0f + (pred_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_precision_modulation) {
        float precision_base = predictive_sleep_get_precision_factor(state);
        bridge->effects.precision_factor =
            1.0f + (precision_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_error_lr_modulation) {
        float lr_base = predictive_sleep_get_error_lr_factor(state);
        bridge->effects.error_learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.offline_consolidation = (state == SLEEP_STATE_DEEP_NREM);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int predictive_sleep_get_effects(const predictive_sleep_bridge_t bridge,
                                  predictive_sleep_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "predictive_sleep_get_effects: effects is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float predictive_sleep_get_prediction_strength(const predictive_sleep_bridge_t bridge,
                                                float base_strength) {
    if (!bridge) return base_strength;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = base_strength * bridge->effects.prediction_strength_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float predictive_sleep_get_precision(const predictive_sleep_bridge_t bridge,
                                      float base_precision) {
    if (!bridge) return base_precision;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = base_precision * bridge->effects.precision_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float predictive_sleep_get_error_learning_rate(const predictive_sleep_bridge_t bridge,
                                                float base_lr) {
    if (!bridge) return base_lr;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = base_lr * bridge->effects.error_learning_rate_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float predictive_sleep_get_pred_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return PREDICTIVE_SLEEP_PRED_AWAKE;
        case SLEEP_STATE_DROWSY:     return PREDICTIVE_SLEEP_PRED_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return PREDICTIVE_SLEEP_PRED_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return PREDICTIVE_SLEEP_PRED_DEEP_NREM;
        case SLEEP_STATE_REM:        return PREDICTIVE_SLEEP_PRED_REM;
        default:                     return PREDICTIVE_SLEEP_PRED_AWAKE;
    }
}

float predictive_sleep_get_precision_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return PREDICTIVE_SLEEP_PRECISION_AWAKE;
        case SLEEP_STATE_DROWSY:     return PREDICTIVE_SLEEP_PRECISION_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return PREDICTIVE_SLEEP_PRECISION_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return PREDICTIVE_SLEEP_PRECISION_DEEP_NREM;
        case SLEEP_STATE_REM:        return PREDICTIVE_SLEEP_PRECISION_REM;
        default:                     return PREDICTIVE_SLEEP_PRECISION_AWAKE;
    }
}

float predictive_sleep_get_error_lr_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return PREDICTIVE_SLEEP_ERROR_LR_AWAKE;
        case SLEEP_STATE_DROWSY:     return PREDICTIVE_SLEEP_ERROR_LR_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return PREDICTIVE_SLEEP_ERROR_LR_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return PREDICTIVE_SLEEP_ERROR_LR_DEEP_NREM;
        case SLEEP_STATE_REM:        return PREDICTIVE_SLEEP_ERROR_LR_REM;
        default:                     return PREDICTIVE_SLEEP_ERROR_LR_AWAKE;
    }
}
