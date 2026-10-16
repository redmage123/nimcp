/**
 * @file nimcp_stdp_sleep_bridge.c
 * @brief Sleep-STDP Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "plasticity/stdp/nimcp_stdp_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stdp_sleep_bridge)

/* Security integration */

struct stdp_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    stdp_sleep_config_t config;
    sleep_system_t sleep_system;
    stdp_sleep_effects_t effects;
    nimcp_platform_mutex_t* mutex;
    bool callback_registered;  /* Track if callback is registered for cleanup */
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(stdp_sleep_bridge, struct stdp_sleep_bridge_struct)

/* Forward declarations */
static void stdp_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update STDP parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - STDP learning rates vary dramatically with sleep state
 * - During deep NREM, consolidation occurs via replay
 * - REM enables creative learning with increased LTP bias
 */
static void stdp_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    stdp_sleep_bridge_t bridge = (stdp_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_on_sleep_state_change: bridge is NULL");
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("STDP bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_lr_modulation) {
        float lr_base = stdp_sleep_get_lr_factor(new_state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_ratio_modulation) {
        float ratio_base = stdp_sleep_get_ratio_factor(new_state);
        bridge->effects.ltp_ltd_ratio =
            1.0f + (ratio_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_window_modulation) {
        float tau_base = stdp_sleep_get_tau_factor(new_state);
        bridge->effects.tau_factor =
            1.0f + (tau_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.plasticity_enabled = (new_state != SLEEP_STATE_DEEP_NREM) ||
                                          bridge->effects.learning_rate_factor > 0.1f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("STDP modulated: lr=%.2f, ratio=%.2f, tau=%.2f",
                        bridge->effects.learning_rate_factor,
                        bridge->effects.ltp_ltd_ratio,
                        bridge->effects.tau_factor);
}

int stdp_sleep_default_config(stdp_sleep_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "STDP-sleep config is NULL");
    config->enable_lr_modulation = true;
    config->enable_ratio_modulation = true;
    config->enable_window_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

stdp_sleep_bridge_t stdp_sleep_bridge_create(
    const stdp_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    NIMCP_API_CHECK_NULL_RET_NULL(sleep_system, "Sleep system is NULL");

    struct stdp_sleep_bridge_struct* bridge =
        (struct stdp_sleep_bridge_struct*)nimcp_malloc(sizeof(struct stdp_sleep_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "STDP-sleep bridge allocation failed");

    memset(bridge, 0, sizeof(struct stdp_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        stdp_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.learning_rate_factor = 1.0f;
    bridge->effects.ltp_ltd_ratio = 1.0f;
    bridge->effects.tau_factor = 1.0f;
    bridge->effects.plasticity_enabled = true;

    if (bridge_base_init(&bridge->base, 0, "stdp_sleep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_sleep_bridge_create: failed to init bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("STDP-sleep bridge mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_sleep_bridge_create: mutex creation failed");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        stdp_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_sleep_bridge_create: failed to register callback");
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for STDP bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    stdp_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("STDP-sleep bridge created");
    return bridge;
}

void stdp_sleep_bridge_destroy(stdp_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            stdp_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for STDP bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int stdp_sleep_update(stdp_sleep_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-sleep bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "stdp_sleep_update");
    BRIDGE_LGSS_GATE(bridge, "stdp_sleep_update");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_lr_modulation) {
        float lr_base = stdp_sleep_get_lr_factor(state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_ratio_modulation) {
        float ratio_base = stdp_sleep_get_ratio_factor(state);
        bridge->effects.ltp_ltd_ratio =
            1.0f + (ratio_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_window_modulation) {
        float tau_base = stdp_sleep_get_tau_factor(state);
        bridge->effects.tau_factor =
            1.0f + (tau_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.plasticity_enabled = (state != SLEEP_STATE_DEEP_NREM) ||
                                          bridge->effects.learning_rate_factor > 0.1f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int stdp_sleep_get_effects(const stdp_sleep_bridge_t bridge, stdp_sleep_effects_t* effects) {
    NIMCP_API_CHECK_NULL(bridge, -1, "STDP-sleep bridge is NULL");
    NIMCP_API_CHECK_NULL(effects, -1, "Effects output pointer is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float stdp_sleep_get_learning_rate(const stdp_sleep_bridge_t bridge, float base_lr) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_sleep_get_learning_rate: bridge is NULL");
        return base_lr;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = base_lr * bridge->effects.learning_rate_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float stdp_sleep_get_a_plus(const stdp_sleep_bridge_t bridge, float base_a_plus) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_sleep_get_a_plus: bridge is NULL");
        return base_a_plus;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = base_a_plus * bridge->effects.ltp_ltd_ratio;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float stdp_sleep_get_a_minus(const stdp_sleep_bridge_t bridge, float base_a_minus) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_sleep_get_a_minus: bridge is NULL");
        return base_a_minus;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* A- gets inverse ratio adjustment to shift LTP/LTD balance */
    float result = base_a_minus / bridge->effects.ltp_ltd_ratio;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float stdp_sleep_get_lr_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return STDP_SLEEP_LR_AWAKE;
        case SLEEP_STATE_DROWSY:     return STDP_SLEEP_LR_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return STDP_SLEEP_LR_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return STDP_SLEEP_LR_DEEP_NREM;
        case SLEEP_STATE_REM:        return STDP_SLEEP_LR_REM;
        default:                     return STDP_SLEEP_LR_AWAKE;
    }
}

float stdp_sleep_get_ratio_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return STDP_SLEEP_RATIO_AWAKE;
        case SLEEP_STATE_DROWSY:     return STDP_SLEEP_RATIO_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return STDP_SLEEP_RATIO_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return STDP_SLEEP_RATIO_DEEP_NREM;
        case SLEEP_STATE_REM:        return STDP_SLEEP_RATIO_REM;
        default:                     return STDP_SLEEP_RATIO_AWAKE;
    }
}

float stdp_sleep_get_tau_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return STDP_SLEEP_TAU_AWAKE;
        case SLEEP_STATE_DROWSY:     return STDP_SLEEP_TAU_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return STDP_SLEEP_TAU_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return STDP_SLEEP_TAU_DEEP_NREM;
        case SLEEP_STATE_REM:        return STDP_SLEEP_TAU_REM;
        default:                     return STDP_SLEEP_TAU_AWAKE;
    }
}
