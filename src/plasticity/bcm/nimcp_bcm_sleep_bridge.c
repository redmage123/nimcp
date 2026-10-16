/**
 * @file nimcp_bcm_sleep_bridge.c
 * @brief Sleep-BCM Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "plasticity/bcm/nimcp_bcm_sleep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bcm_sleep_bridge)

/* Security integration */
struct bcm_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    bcm_sleep_config_t config;
    sleep_system_t sleep_system;
    bcm_sleep_effects_t effects;
    nimcp_platform_mutex_t* mutex;
    bool callback_registered;  /* Track if callback is registered for cleanup */
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(bcm_sleep_bridge, struct bcm_sleep_bridge_struct)

/* Forward declarations */
static void bcm_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update BCM sliding threshold for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - BCM theta threshold modulates LTP/LTD balance
 * - During sleep, elevated theta favors LTD (synaptic downscaling)
 * - Deep NREM is critical for synaptic homeostasis
 */
static void bcm_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    bcm_sleep_bridge_t bridge = (bcm_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_on_sleep_state_change: bridge is NULL");
        return;
    }

    NIMCP_LOGGING_DEBUG("BCM bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_theta_modulation) {
        float theta_base = bcm_sleep_theta_for_state(new_state);
        bridge->effects.theta_factor =
            1.0f + (theta_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_lr_modulation) {
        float lr_base = bcm_sleep_lr_for_state(new_state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    /* Elevated theta favors LTD (activity must exceed higher threshold for LTP) */
    bridge->effects.favors_ltd = (bridge->effects.theta_factor > 1.0f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("BCM modulated: theta=%.2f, lr=%.2f, favors_ltd=%d",
                        bridge->effects.theta_factor,
                        bridge->effects.learning_rate_factor,
                        bridge->effects.favors_ltd);
}

int bcm_sleep_default_config(bcm_sleep_config_t* config) {
    NIMCP_API_CHECK_NULL(config, -1, "BCM-sleep config is NULL");
    config->enable_theta_modulation = true;
    config->enable_lr_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

bcm_sleep_bridge_t bcm_sleep_bridge_create(
    const bcm_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    NIMCP_API_CHECK_NULL_RET_NULL(sleep_system, "Sleep system is NULL");

    struct bcm_sleep_bridge_struct* bridge =
        (struct bcm_sleep_bridge_struct*)nimcp_malloc(sizeof(struct bcm_sleep_bridge_struct));
    NIMCP_API_CHECK_ALLOC(bridge, "BCM-sleep bridge allocation failed");

    memset(bridge, 0, sizeof(struct bcm_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        bcm_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.theta_factor = 1.0f;
    bridge->effects.learning_rate_factor = 1.0f;
    bridge->effects.favors_ltd = false;

    if (bridge_base_init(&bridge->base, 0, "bcm_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        LOG_ERROR("BCM-sleep bridge mutex creation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "BCM-sleep bridge mutex creation failed");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        bcm_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for BCM bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    bcm_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("BCM-sleep bridge created");
    return bridge;
}

void bcm_sleep_bridge_destroy(bcm_sleep_bridge_t bridge) {
    if (!bridge) {
        return;  /* destroy(NULL) is idempotent - standard pattern */
    }

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            bcm_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for BCM bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int bcm_sleep_update(bcm_sleep_bridge_t bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM-sleep bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "bcm_sleep_update");
    BRIDGE_LGSS_GATE(bridge, "bcm_sleep_update");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_theta_modulation) {
        float theta_base = bcm_sleep_theta_for_state(state);
        bridge->effects.theta_factor =
            1.0f + (theta_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_lr_modulation) {
        float lr_base = bcm_sleep_lr_for_state(state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    /* Elevated theta favors LTD (activity must exceed higher threshold for LTP) */
    bridge->effects.favors_ltd = (bridge->effects.theta_factor > 1.0f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int bcm_sleep_get_effects(const bcm_sleep_bridge_t bridge, bcm_sleep_effects_t* effects) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM-sleep bridge is NULL");
    NIMCP_API_CHECK_NULL(effects, -1, "Effects output pointer is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float bcm_sleep_get_theta_factor(const bcm_sleep_bridge_t bridge) {
    if (!bridge) {
        return 1.0f;  /* NULL bridge returns default factor - normal for getters */
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.theta_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float bcm_sleep_get_lr_factor(const bcm_sleep_bridge_t bridge) {
    if (!bridge) {
        return 1.0f;  /* NULL bridge returns default factor - normal for getters */
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.learning_rate_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float bcm_sleep_theta_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return BCM_SLEEP_THETA_AWAKE;
        case SLEEP_STATE_DROWSY:     return BCM_SLEEP_THETA_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return BCM_SLEEP_THETA_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return BCM_SLEEP_THETA_DEEP_NREM;
        case SLEEP_STATE_REM:        return BCM_SLEEP_THETA_REM;
        default:                     return BCM_SLEEP_THETA_AWAKE;
    }
}

float bcm_sleep_lr_for_state(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return BCM_SLEEP_LR_AWAKE;
        case SLEEP_STATE_DROWSY:     return BCM_SLEEP_LR_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return BCM_SLEEP_LR_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return BCM_SLEEP_LR_DEEP_NREM;
        case SLEEP_STATE_REM:        return BCM_SLEEP_LR_REM;
        default:                     return BCM_SLEEP_LR_AWAKE;
    }
}
