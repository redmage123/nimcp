/**
 * @file nimcp_eligibility_sleep_bridge.c
 * @brief Sleep-Eligibility Trace Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "plasticity/eligibility/nimcp_eligibility_sleep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(eligibility_sleep_bridge)

/* Security integration */

struct eligibility_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    eligibility_sleep_config_t config;
    sleep_system_t sleep_system;
    eligibility_sleep_effects_t effects;
    nimcp_platform_mutex_t* mutex;
    bool callback_registered;  /* Track if callback is registered for cleanup */
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(eligibility_sleep_bridge, struct eligibility_sleep_bridge_struct)

/* Forward declarations */
static void eligibility_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update eligibility trace parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Eligibility traces act as synaptic tags during waking
 * - Sleep consolidation captures tagged synapses
 * - Trace decay varies with sleep state to preserve tags during consolidation
 * - Deep NREM provides optimal window for synaptic capture
 */
static void eligibility_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    eligibility_sleep_bridge_t bridge = (eligibility_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Eligibility bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_lr_modulation) {
        float lr_base = eligibility_sleep_get_lr_factor(new_state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_decay_modulation) {
        float decay_base = eligibility_sleep_get_decay_factor(new_state);
        bridge->effects.decay_factor =
            1.0f + (decay_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_consolidation) {
        bridge->effects.consolidation_active = (new_state == SLEEP_STATE_DEEP_NREM);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Eligibility modulated: lr=%.2f, decay=%.2f, consolidation=%d",
                        bridge->effects.learning_rate_factor,
                        bridge->effects.decay_factor,
                        bridge->effects.consolidation_active);
}

int eligibility_sleep_default_config(eligibility_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_lr_modulation = true;
    config->enable_decay_modulation = true;
    config->enable_consolidation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

eligibility_sleep_bridge_t eligibility_sleep_bridge_create(
    const eligibility_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_sleep_bridge_create: sleep_system is NULL");
        NIMCP_LOGGING_ERROR("eligibility_sleep_bridge_create: NULL sleep_system");
        return NULL;
    }

    struct eligibility_sleep_bridge_struct* bridge =
        (struct eligibility_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct eligibility_sleep_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    memset(bridge, 0, sizeof(struct eligibility_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        eligibility_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.learning_rate_factor = 1.0f;
    bridge->effects.decay_factor = 1.0f;
    bridge->effects.consolidation_active = false;

    if (bridge_base_init(&bridge->base, 0, "eligibility_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        eligibility_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for eligibility bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    eligibility_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Eligibility-sleep bridge created");
    return bridge;
}

void eligibility_sleep_bridge_destroy(eligibility_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_sleep_bridge_destroy: bridge is NULL");
        return;
    }

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            eligibility_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for eligibility bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int eligibility_sleep_update(eligibility_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_lr_modulation) {
        float lr_base = eligibility_sleep_get_lr_factor(state);
        bridge->effects.learning_rate_factor =
            1.0f + (lr_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_decay_modulation) {
        float decay_base = eligibility_sleep_get_decay_factor(state);
        bridge->effects.decay_factor =
            1.0f + (decay_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_consolidation) {
        bridge->effects.consolidation_active = (state == SLEEP_STATE_DEEP_NREM);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int eligibility_sleep_get_effects(const eligibility_sleep_bridge_t bridge,
                                   eligibility_sleep_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_sleep_get_effects: effects is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float eligibility_sleep_get_learning_rate(const eligibility_sleep_bridge_t bridge,
                                           float base_lr) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_sleep_get_learning_rate: bridge is NULL");
        return base_lr;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = base_lr * bridge->effects.learning_rate_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float eligibility_sleep_get_decay_lambda(const eligibility_sleep_bridge_t bridge,
                                          float base_lambda) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_sleep_get_decay_lambda: bridge is NULL");
        return base_lambda;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* Apply decay factor - higher factor = slower decay (higher lambda) */
    float result = base_lambda * bridge->effects.decay_factor;
    /* Clamp to valid range */
    if (result > 0.999f) result = 0.999f;
    if (result < 0.5f) result = 0.5f;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float eligibility_sleep_get_lr_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ELIGIBILITY_SLEEP_LR_AWAKE;
        case SLEEP_STATE_DROWSY:     return ELIGIBILITY_SLEEP_LR_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ELIGIBILITY_SLEEP_LR_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ELIGIBILITY_SLEEP_LR_DEEP_NREM;
        case SLEEP_STATE_REM:        return ELIGIBILITY_SLEEP_LR_REM;
        default:                     return ELIGIBILITY_SLEEP_LR_AWAKE;
    }
}

float eligibility_sleep_get_decay_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ELIGIBILITY_SLEEP_DECAY_AWAKE;
        case SLEEP_STATE_DROWSY:     return ELIGIBILITY_SLEEP_DECAY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ELIGIBILITY_SLEEP_DECAY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ELIGIBILITY_SLEEP_DECAY_DEEP_NREM;
        case SLEEP_STATE_REM:        return ELIGIBILITY_SLEEP_DECAY_REM;
        default:                     return ELIGIBILITY_SLEEP_DECAY_AWAKE;
    }
}
