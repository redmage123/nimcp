/**
 * @file nimcp_dendritic_sleep_bridge.c
 * @brief Sleep-Dendritic Plasticity Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "plasticity/dendritic/nimcp_dendritic_sleep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dendritic_sleep_bridge)

/* Security integration */

struct dendritic_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    dendritic_sleep_config_t config;
    sleep_system_t sleep_system;
    dendritic_sleep_effects_t effects;
    nimcp_platform_mutex_t* mutex;
    bool callback_registered;  /* Track if callback is registered for cleanup */
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(dendritic_sleep_bridge, struct dendritic_sleep_bridge_struct)

/* Forward declarations */
static void dendritic_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update dendritic computation parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - NMDA receptors show enhanced activity during NREM sleep
 * - Dendritic spike thresholds are reduced during consolidation
 * - Ca2+ dynamics are prolonged during sleep to enhance integration
 * - Deep NREM provides optimal conditions for dendritic computation
 */
static void dendritic_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    dendritic_sleep_bridge_t bridge = (dendritic_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Dendritic bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    if (bridge->config.enable_nmda_modulation) {
        float nmda_base = dendritic_sleep_get_nmda_factor(new_state);
        bridge->effects.nmda_conductance_factor =
            1.0f + (nmda_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_threshold_modulation) {
        float threshold_base = dendritic_sleep_get_threshold_factor(new_state);
        bridge->effects.spike_threshold_factor =
            1.0f + (threshold_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_calcium_modulation) {
        float calcium_base = dendritic_sleep_get_calcium_factor(new_state);
        bridge->effects.calcium_decay_factor =
            1.0f + (calcium_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.enhanced_integration =
        (new_state == SLEEP_STATE_LIGHT_NREM || new_state == SLEEP_STATE_DEEP_NREM);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Dendritic modulated: nmda=%.2f, threshold=%.2f, ca_decay=%.2f",
                        bridge->effects.nmda_conductance_factor,
                        bridge->effects.spike_threshold_factor,
                        bridge->effects.calcium_decay_factor);
}

int dendritic_sleep_default_config(dendritic_sleep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_sleep_default_config: config is NULL");
        return -1;
    }
    config->enable_nmda_modulation = true;
    config->enable_threshold_modulation = true;
    config->enable_calcium_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    return 0;
}

dendritic_sleep_bridge_t dendritic_sleep_bridge_create(
    const dendritic_sleep_config_t* config,
    sleep_system_t sleep_system)
{
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("dendritic_sleep_bridge_create: NULL sleep_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_sleep_bridge_create: sleep_system is NULL");
        return NULL;
    }

    struct dendritic_sleep_bridge_struct* bridge =
        (struct dendritic_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct dendritic_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_sleep_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct dendritic_sleep_bridge_struct));

    if (config) {
        bridge->config = *config;
    } else {
        dendritic_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;
    bridge->effects.nmda_conductance_factor = 1.0f;
    bridge->effects.spike_threshold_factor = 1.0f;
    bridge->effects.calcium_decay_factor = 1.0f;
    bridge->effects.enhanced_integration = false;

    if (bridge_base_init(&bridge->base, 0, "dendritic_sleep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "dendritic_sleep_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_sleep_bridge_create: mutex allocation failed");
        nimcp_free(bridge);
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        dendritic_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for dendritic bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    dendritic_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Dendritic-sleep bridge created");
    return bridge;
}

void dendritic_sleep_bridge_destroy(dendritic_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            dendritic_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for dendritic bridge");
        }
    }

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dendritic_sleep_update(dendritic_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    if (bridge->config.enable_nmda_modulation) {
        float nmda_base = dendritic_sleep_get_nmda_factor(state);
        bridge->effects.nmda_conductance_factor =
            1.0f + (nmda_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_threshold_modulation) {
        float threshold_base = dendritic_sleep_get_threshold_factor(state);
        bridge->effects.spike_threshold_factor =
            1.0f + (threshold_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_calcium_modulation) {
        float calcium_base = dendritic_sleep_get_calcium_factor(state);
        bridge->effects.calcium_decay_factor =
            1.0f + (calcium_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.enhanced_integration =
        (state == SLEEP_STATE_LIGHT_NREM || state == SLEEP_STATE_DEEP_NREM);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int dendritic_sleep_get_effects(const dendritic_sleep_bridge_t bridge,
                                 dendritic_sleep_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_sleep_get_effects: effects is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float dendritic_sleep_get_nmda_conductance(const dendritic_sleep_bridge_t bridge,
                                            float base_conductance) {
    if (!bridge) return base_conductance;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = base_conductance * bridge->effects.nmda_conductance_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float dendritic_sleep_get_spike_threshold(const dendritic_sleep_bridge_t bridge,
                                           float base_threshold) {
    if (!bridge) return base_threshold;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    float result = base_threshold * bridge->effects.spike_threshold_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float dendritic_sleep_get_calcium_decay(const dendritic_sleep_bridge_t bridge,
                                         float base_tau) {
    if (!bridge) return base_tau;
    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* Lower factor = slower decay (longer tau) */
    float result = base_tau / bridge->effects.calcium_decay_factor;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

float dendritic_sleep_get_nmda_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return DENDRITIC_SLEEP_NMDA_AWAKE;
        case SLEEP_STATE_DROWSY:     return DENDRITIC_SLEEP_NMDA_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return DENDRITIC_SLEEP_NMDA_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return DENDRITIC_SLEEP_NMDA_DEEP_NREM;
        case SLEEP_STATE_REM:        return DENDRITIC_SLEEP_NMDA_REM;
        default:                     return DENDRITIC_SLEEP_NMDA_AWAKE;
    }
}

float dendritic_sleep_get_threshold_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return DENDRITIC_SLEEP_THRESHOLD_AWAKE;
        case SLEEP_STATE_DROWSY:     return DENDRITIC_SLEEP_THRESHOLD_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return DENDRITIC_SLEEP_THRESHOLD_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return DENDRITIC_SLEEP_THRESHOLD_DEEP_NREM;
        case SLEEP_STATE_REM:        return DENDRITIC_SLEEP_THRESHOLD_REM;
        default:                     return DENDRITIC_SLEEP_THRESHOLD_AWAKE;
    }
}

float dendritic_sleep_get_calcium_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return DENDRITIC_SLEEP_CA_DECAY_AWAKE;
        case SLEEP_STATE_DROWSY:     return DENDRITIC_SLEEP_CA_DECAY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return DENDRITIC_SLEEP_CA_DECAY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return DENDRITIC_SLEEP_CA_DECAY_DEEP_NREM;
        case SLEEP_STATE_REM:        return DENDRITIC_SLEEP_CA_DECAY_REM;
        default:                     return DENDRITIC_SLEEP_CA_DECAY_AWAKE;
    }
}
