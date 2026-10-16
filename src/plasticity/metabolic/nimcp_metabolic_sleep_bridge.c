/**
 * @file nimcp_metabolic_sleep_bridge.c
 * @brief Sleep-Metabolic Plasticity Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "plasticity/metabolic/nimcp_metabolic_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metabolic_sleep_bridge)

/* Security integration */
/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct metabolic_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    metabolic_sleep_config_t config;
    sleep_system_t sleep_system;
    metabolic_plasticity_t* metabolic;
    metabolic_sleep_effects_t effects;
    nimcp_platform_mutex_t* mutex;
    bool callback_registered;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(metabolic_sleep_bridge, struct metabolic_sleep_bridge_struct)

/* ============================================================================
 * Sleep State Callback
 * ============================================================================ */

/**
 * @brief Callback invoked when sleep state changes
 *
 * WHAT: Update metabolic recovery rate for new sleep state
 * WHY:  Sleep states have different ATP restoration rates
 * HOW:  Called by sleep system via observer pattern
 */
static void metabolic_on_sleep_state_change(sleep_state_t new_state, void* user_data) {
    metabolic_sleep_bridge_t bridge = (metabolic_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Metabolic bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update recovery rate modulation */
    if (bridge->config.enable_recovery_modulation) {
        float recovery_base = metabolic_sleep_get_recovery_factor(new_state);
        bridge->effects.recovery_rate_factor =
            1.0f + (recovery_base - 1.0f) * bridge->config.modulation_strength;
    }

    /* Update energy cost modulation */
    if (bridge->config.enable_cost_modulation) {
        float cost_base = metabolic_sleep_get_energy_cost_factor(new_state);
        bridge->effects.energy_cost_factor =
            1.0f + (cost_base - 1.0f) * bridge->config.modulation_strength;
    }

    /* Check for deep restoration mode */
    bridge->effects.deep_restoration_active = (new_state == SLEEP_STATE_DEEP_NREM);

    /* Apply new recovery rate to metabolic system */
    if (bridge->metabolic && bridge->config.enable_recovery_modulation) {
        float base_rate = metabolic_plasticity_get_recovery_rate(bridge->metabolic);
        /* Reverse previous modulation to get true base */
        if (bridge->effects.recovery_rate_factor > 0.0f) {
            base_rate = base_rate / bridge->effects.recovery_rate_factor;
        }
        /* Apply new modulation */
        float new_rate = base_rate * bridge->effects.recovery_rate_factor;
        bridge->effects.effective_recovery_rate = new_rate;
        metabolic_plasticity_set_recovery_rate(bridge->metabolic, new_rate);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Metabolic modulated: recovery=%.2fx, cost=%.2fx",
                        bridge->effects.recovery_rate_factor,
                        bridge->effects.energy_cost_factor);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int metabolic_sleep_default_config(metabolic_sleep_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_recovery_modulation = true;
    config->enable_cost_modulation = true;
    config->enable_sleep_pressure_feedback = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

metabolic_sleep_bridge_t metabolic_sleep_bridge_create(
    const metabolic_sleep_config_t* config,
    sleep_system_t sleep_system,
    metabolic_plasticity_t* metabolic
) {
    /* Guard: require sleep system and metabolic system */
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("metabolic_sleep_bridge_create: NULL sleep_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_sleep_bridge_create: sleep_system is NULL");
        return NULL;
    }
    if (!metabolic) {
        NIMCP_LOGGING_ERROR("metabolic_sleep_bridge_create: NULL metabolic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_sleep_bridge_create: metabolic is NULL");
        return NULL;
    }

    /* Allocate bridge */
    struct metabolic_sleep_bridge_struct* bridge =
        (struct metabolic_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct metabolic_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate metabolic sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(struct metabolic_sleep_bridge_struct));

    /* Link systems */
    bridge->sleep_system = sleep_system;
    bridge->metabolic = metabolic;

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(metabolic_sleep_config_t));
    } else {
        metabolic_sleep_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "metabolic_sleep") != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metabolic_sleep_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_LOGGING_ERROR("Failed to create metabolic sleep bridge mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metabolic_sleep_bridge_create: failed to create mutex");
        return NULL;
    }

    /* Initialize effects to awake state */
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.recovery_rate_factor = METABOLIC_SLEEP_RECOVERY_AWAKE;
    bridge->effects.energy_cost_factor = METABOLIC_SLEEP_COST_AWAKE;

    /* Register callback with sleep system */
    if (sleep_register_state_callback(sleep_system, metabolic_on_sleep_state_change, bridge) == 0) {
        bridge->callback_registered = true;
        NIMCP_LOGGING_DEBUG("Metabolic sleep bridge callback registered");
    }

    /* Perform initial update */
    metabolic_sleep_update(bridge);

    NIMCP_LOGGING_INFO("Metabolic sleep bridge created");
    return bridge;
}

void metabolic_sleep_bridge_destroy(metabolic_sleep_bridge_t bridge) {
    if (!bridge) return;

    /* Unregister callback */
    if (bridge->callback_registered && bridge->sleep_system) {
        sleep_unregister_state_callback(bridge->sleep_system, metabolic_on_sleep_state_change, bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free structure (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Metabolic sleep bridge destroyed");
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int metabolic_sleep_update(metabolic_sleep_bridge_t bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->sleep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "metabolic_sleep_update: sleep_system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Query current sleep state */
    sleep_state_t current_state = sleep_get_current_state(bridge->sleep_system);
    bridge->effects.current_state = current_state;

    /* Query sleep pressure */
    bridge->effects.sleep_pressure = sleep_get_pressure(bridge->sleep_system);

    /* Update modulation factors */
    if (bridge->config.enable_recovery_modulation) {
        float recovery_base = metabolic_sleep_get_recovery_factor(current_state);
        bridge->effects.recovery_rate_factor =
            1.0f + (recovery_base - 1.0f) * bridge->config.modulation_strength;
    }

    if (bridge->config.enable_cost_modulation) {
        float cost_base = metabolic_sleep_get_energy_cost_factor(current_state);
        bridge->effects.energy_cost_factor =
            1.0f + (cost_base - 1.0f) * bridge->config.modulation_strength;
    }

    bridge->effects.deep_restoration_active = (current_state == SLEEP_STATE_DEEP_NREM);

    /* Apply recovery rate to metabolic system */
    if (bridge->metabolic && bridge->config.enable_recovery_modulation) {
        float base_rate = METABOLIC_RECOVERY_RATE_BASE +
                         METABOLIC_RECOVERY_RATE_GLYCOLYSIS +
                         METABOLIC_RECOVERY_RATE_ASTROCYTE;
        float effective_rate = base_rate * bridge->effects.recovery_rate_factor;
        bridge->effects.effective_recovery_rate = effective_rate;
        metabolic_plasticity_set_recovery_rate(bridge->metabolic, effective_rate);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int metabolic_sleep_get_effects(const metabolic_sleep_bridge_t bridge,
                                 metabolic_sleep_effects_t* effects) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_sleep_get_effects: effects is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->effects, sizeof(metabolic_sleep_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

float metabolic_sleep_get_recovery_rate(const metabolic_sleep_bridge_t bridge,
                                         float base_rate) {
    if (!bridge) return base_rate;
    if (!bridge->config.enable_recovery_modulation) return base_rate;

    return base_rate * bridge->effects.recovery_rate_factor;
}

float metabolic_sleep_get_cost_factor(const metabolic_sleep_bridge_t bridge) {
    if (!bridge) return 1.0f;
    if (!bridge->config.enable_cost_modulation) return 1.0f;

    return bridge->effects.energy_cost_factor;
}

bool metabolic_sleep_is_deep_restoration(const metabolic_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->effects.deep_restoration_active;
}

/* ============================================================================
 * Feedback Implementation (Metabolic → Sleep)
 * ============================================================================ */

float metabolic_sleep_get_sleep_pressure(const metabolic_sleep_bridge_t bridge) {
    if (!bridge) return 0.0f;
    if (!bridge->config.enable_sleep_pressure_feedback) return 0.0f;
    if (!bridge->metabolic) return 0.0f;

    /* Get current ATP level */
    float atp_level = metabolic_plasticity_get_atp_level(bridge->metabolic);

    /* Convert to sleep pressure */
    return metabolic_sleep_atp_to_pressure(atp_level);
}

bool metabolic_sleep_is_critical_need(const metabolic_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    if (!bridge->metabolic) {
        return false;
    }

    float atp_level = metabolic_plasticity_get_atp_level(bridge->metabolic);
    return (atp_level < METABOLIC_SLEEP_PRESSURE_ATP_CRITICAL);
}

/* ============================================================================
 * Helper Function Implementation
 * ============================================================================ */

float metabolic_sleep_get_recovery_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return METABOLIC_SLEEP_RECOVERY_AWAKE;
        case SLEEP_STATE_DROWSY:     return METABOLIC_SLEEP_RECOVERY_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return METABOLIC_SLEEP_RECOVERY_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return METABOLIC_SLEEP_RECOVERY_DEEP_NREM;
        case SLEEP_STATE_REM:        return METABOLIC_SLEEP_RECOVERY_REM;
        default:                     return METABOLIC_SLEEP_RECOVERY_AWAKE;
    }
}

float metabolic_sleep_get_energy_cost_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:      return METABOLIC_SLEEP_COST_AWAKE;
        case SLEEP_STATE_DROWSY:     return METABOLIC_SLEEP_COST_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return METABOLIC_SLEEP_COST_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return METABOLIC_SLEEP_COST_DEEP_NREM;
        case SLEEP_STATE_REM:        return METABOLIC_SLEEP_COST_REM;
        default:                     return METABOLIC_SLEEP_COST_AWAKE;
    }
}

float metabolic_sleep_atp_to_pressure(float atp_level) {
    /* Inverse relationship: low ATP = high sleep pressure */

    if (atp_level >= METABOLIC_ATP_FULL_CAPACITY) {
        return 0.0f;  /* No sleep pressure when fully charged */
    }

    if (atp_level <= METABOLIC_SLEEP_PRESSURE_ATP_CRITICAL) {
        return 1.0f;  /* Maximum sleep pressure when critical */
    }

    /* Linear mapping from ATP to pressure */
    /* ATP 100 → pressure 0.0 */
    /* ATP 20  → pressure 1.0 */
    float pressure = (METABOLIC_ATP_FULL_CAPACITY - atp_level) /
                    (METABOLIC_ATP_FULL_CAPACITY - METABOLIC_SLEEP_PRESSURE_ATP_CRITICAL);

    /* Clamp to [0, 1] */
    if (pressure < 0.0f) pressure = 0.0f;
    if (pressure > 1.0f) pressure = 1.0f;

    return pressure;
}
