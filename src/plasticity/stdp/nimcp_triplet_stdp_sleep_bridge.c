/**
 * @file nimcp_triplet_stdp_sleep_bridge.c
 * @brief Sleep-Triplet STDP Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "plasticity/stdp/nimcp_triplet_stdp_sleep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(triplet_stdp_sleep_bridge)

/* Security integration */

struct triplet_stdp_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    triplet_stdp_sleep_config_t config;
    sleep_system_t sleep_system;
    triplet_stdp_sleep_effects_t effects;
    nimcp_platform_mutex_t* mutex;
    bool callback_registered;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(triplet_stdp_sleep_bridge, struct triplet_stdp_sleep_bridge_struct)

/* Forward declarations */
static void triplet_stdp_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float triplet_stdp_sleep_get_tau_fast_factor(sleep_state_t state) {
    /* WHAT: Get fast trace modulation for sleep state
     * WHY:  Fast traces (tau_plus, tau_minus) decay differently in sleep
     * HOW:  Return constant for each sleep state
     */
    switch (state) {
        case SLEEP_STATE_AWAKE:      return TRIPLET_STDP_SLEEP_TAU_FAST_AWAKE;
        case SLEEP_STATE_DROWSY:     return TRIPLET_STDP_SLEEP_TAU_FAST_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return TRIPLET_STDP_SLEEP_TAU_FAST_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return TRIPLET_STDP_SLEEP_TAU_FAST_DEEP_NREM;
        case SLEEP_STATE_REM:        return TRIPLET_STDP_SLEEP_TAU_FAST_REM;
        default:                     return 1.0f;
    }
}

float triplet_stdp_sleep_get_tau_slow_factor(sleep_state_t state) {
    /* WHAT: Get slow trace modulation for sleep state
     * WHY:  Slow traces (tau_x, tau_y) enable triplet accumulation
     * HOW:  Return constant for each sleep state
     */
    switch (state) {
        case SLEEP_STATE_AWAKE:      return TRIPLET_STDP_SLEEP_TAU_SLOW_AWAKE;
        case SLEEP_STATE_DROWSY:     return TRIPLET_STDP_SLEEP_TAU_SLOW_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return TRIPLET_STDP_SLEEP_TAU_SLOW_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return TRIPLET_STDP_SLEEP_TAU_SLOW_DEEP_NREM;
        case SLEEP_STATE_REM:        return TRIPLET_STDP_SLEEP_TAU_SLOW_REM;
        default:                     return 1.0f;
    }
}

float triplet_stdp_sleep_get_a2_factor(sleep_state_t state) {
    /* WHAT: Get pairwise amplitude modulation
     * WHY:  Pairwise STDP component varies with sleep
     * HOW:  Return constant for each sleep state
     */
    switch (state) {
        case SLEEP_STATE_AWAKE:      return TRIPLET_STDP_SLEEP_A2_AWAKE;
        case SLEEP_STATE_DROWSY:     return TRIPLET_STDP_SLEEP_A2_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return TRIPLET_STDP_SLEEP_A2_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return TRIPLET_STDP_SLEEP_A2_DEEP_NREM;
        case SLEEP_STATE_REM:        return TRIPLET_STDP_SLEEP_A2_REM;
        default:                     return 1.0f;
    }
}

float triplet_stdp_sleep_get_a3_factor(sleep_state_t state) {
    /* WHAT: Get triplet amplitude modulation
     * WHY:  Triplet component enhanced during REM consolidation
     * HOW:  Return constant for each sleep state
     */
    switch (state) {
        case SLEEP_STATE_AWAKE:      return TRIPLET_STDP_SLEEP_A3_AWAKE;
        case SLEEP_STATE_DROWSY:     return TRIPLET_STDP_SLEEP_A3_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return TRIPLET_STDP_SLEEP_A3_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return TRIPLET_STDP_SLEEP_A3_DEEP_NREM;
        case SLEEP_STATE_REM:        return TRIPLET_STDP_SLEEP_A3_REM;
        default:                     return 1.0f;
    }
}

/* ============================================================================
 * Sleep State Change Callback
 * ============================================================================ */

static void triplet_stdp_on_sleep_state_change(sleep_state_t new_state, void* user_data) {
    /* WHAT: Callback invoked when sleep state changes
     * WHY:  Immediately update triplet STDP parameters
     * HOW:  Compute new modulation factors for all parameters
     */
    triplet_stdp_sleep_bridge_t bridge = (triplet_stdp_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Triplet STDP bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Compute modulation factors */
    if (bridge->config.enable_tau_fast_modulation) {
        float tau_fast = triplet_stdp_sleep_get_tau_fast_factor(new_state);
        bridge->effects.tau_fast_factor =
            1.0f + (tau_fast - 1.0f) * bridge->config.modulation_strength;
    } else {
        bridge->effects.tau_fast_factor = 1.0f;
    }

    if (bridge->config.enable_tau_slow_modulation) {
        float tau_slow = triplet_stdp_sleep_get_tau_slow_factor(new_state);
        bridge->effects.tau_slow_factor =
            1.0f + (tau_slow - 1.0f) * bridge->config.modulation_strength;
    } else {
        bridge->effects.tau_slow_factor = 1.0f;
    }

    if (bridge->config.enable_a2_modulation) {
        float a2 = triplet_stdp_sleep_get_a2_factor(new_state);
        bridge->effects.a2_factor =
            1.0f + (a2 - 1.0f) * bridge->config.modulation_strength;
    } else {
        bridge->effects.a2_factor = 1.0f;
    }

    if (bridge->config.enable_a3_modulation) {
        float a3 = triplet_stdp_sleep_get_a3_factor(new_state);
        bridge->effects.a3_factor =
            1.0f + (a3 - 1.0f) * bridge->config.modulation_strength;
    } else {
        bridge->effects.a3_factor = 1.0f;
    }

    /* Plasticity disabled during deep sleep with very low activity */
    bridge->effects.plasticity_enabled =
        (new_state != SLEEP_STATE_DEEP_NREM) ||
        (bridge->effects.a2_factor > 0.1f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Triplet STDP modulated: tau_fast=%.2f, tau_slow=%.2f, a2=%.2f, a3=%.2f",
                        bridge->effects.tau_fast_factor,
                        bridge->effects.tau_slow_factor,
                        bridge->effects.a2_factor,
                        bridge->effects.a3_factor);
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int triplet_stdp_sleep_default_config(triplet_stdp_sleep_config_t* config) {
    /* WHAT: Return default configuration
     * WHY:  All modulations enabled for realistic sleep effects
     * HOW:  Set all flags to true, strength to 1.0
     */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config in default_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_sleep_default_config: config is NULL");
        return -1;
    }

    config->enable_tau_fast_modulation = true;
    config->enable_tau_slow_modulation = true;
    config->enable_a2_modulation = true;
    config->enable_a3_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

triplet_stdp_sleep_bridge_t triplet_stdp_sleep_bridge_create(
    const triplet_stdp_sleep_config_t* config,
    sleep_system_t sleep_system
) {
    /* WHAT: Create sleep-triplet STDP bridge
     * WHY:  Initialize integration between sleep and triplet STDP
     * HOW:  Allocate structure, register callback, get initial state
     */
    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("NULL sleep_system in bridge create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_system is NULL");

        return NULL;
    }

    struct triplet_stdp_sleep_bridge_struct* bridge =
        (struct triplet_stdp_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct triplet_stdp_sleep_bridge_struct)
        );

    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate triplet STDP sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;
    }

    memset(bridge, 0, sizeof(struct triplet_stdp_sleep_bridge_struct));

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        triplet_stdp_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep_system;

    /* Initialize effects to baseline */
    bridge->effects.tau_fast_factor = 1.0f;
    bridge->effects.tau_slow_factor = 1.0f;
    bridge->effects.a2_factor = 1.0f;
    bridge->effects.a3_factor = 1.0f;
    bridge->effects.plasticity_enabled = true;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.sleep_pressure = 0.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "triplet_stdp_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for sleep bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "triplet_stdp_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        triplet_stdp_on_sleep_state_change,
        bridge
    );

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for triplet STDP bridge");
    }

    /* Get initial state */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    triplet_stdp_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Triplet STDP sleep bridge created");
    return bridge;
}

void triplet_stdp_sleep_bridge_destroy(triplet_stdp_sleep_bridge_t bridge) {
    /* WHAT: Destroy sleep bridge
     * WHY:  Clean up resources
     * HOW:  Unregister callback, destroy mutex, free structure
     */
    if (!bridge) return;

    /* Unregister callback */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            triplet_stdp_on_sleep_state_change,
            bridge
        );

        if (!unregistered) {
            NIMCP_LOGGING_WARN("Failed to unregister sleep state callback");
        }
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
        bridge->base.mutex = NULL;
    }

    /* Free structure */
    nimcp_free(bridge);

    NIMCP_LOGGING_DEBUG("Destroyed triplet STDP sleep bridge");
}

/* ============================================================================
 * Update Functions
 * ============================================================================ */

int triplet_stdp_sleep_update(triplet_stdp_sleep_bridge_t bridge) {
    /* WHAT: Update effects from current sleep state (polling mode)
     * WHY:  For systems without callback support
     * HOW:  Query sleep system, recompute factors
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep_update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_sleep_update: bridge is NULL");
        return -1;
    }
    if (!bridge->sleep_system) {
        NIMCP_LOGGING_ERROR("NULL sleep_system in sleep_update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_sleep_update: sleep_system is NULL");
        return -1;
    }

    sleep_state_t current_state = sleep_get_current_state(bridge->sleep_system);
    triplet_stdp_on_sleep_state_change(current_state, bridge);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int triplet_stdp_sleep_get_effects(
    const triplet_stdp_sleep_bridge_t bridge,
    triplet_stdp_sleep_effects_t* effects
) {
    /* WHAT: Get computed sleep effects
     * WHY:  Apply to synapses
     * HOW:  Copy effects structure
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in get_effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_LOGGING_ERROR("NULL effects in get_effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_sleep_get_effects: effects is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

float triplet_stdp_sleep_get_tau_plus(
    const triplet_stdp_sleep_bridge_t bridge,
    float base_tau_plus
) {
    if (!bridge) return base_tau_plus;
    return base_tau_plus * bridge->effects.tau_fast_factor;
}

float triplet_stdp_sleep_get_tau_minus(
    const triplet_stdp_sleep_bridge_t bridge,
    float base_tau_minus
) {
    if (!bridge) return base_tau_minus;
    return base_tau_minus * bridge->effects.tau_fast_factor;
}

float triplet_stdp_sleep_get_tau_x(
    const triplet_stdp_sleep_bridge_t bridge,
    float base_tau_x
) {
    if (!bridge) return base_tau_x;
    return base_tau_x * bridge->effects.tau_slow_factor;
}

float triplet_stdp_sleep_get_tau_y(
    const triplet_stdp_sleep_bridge_t bridge,
    float base_tau_y
) {
    if (!bridge) return base_tau_y;
    return base_tau_y * bridge->effects.tau_slow_factor;
}

int triplet_stdp_sleep_apply_modulation(
    triplet_stdp_sleep_bridge_t bridge,
    triplet_stdp_synapse_t* synapse
) {
    /* WHAT: Apply sleep modulation to synapse parameters
     * WHY:  Realize sleep effects on plasticity
     * HOW:  Update synapse time constants and amplitudes (future implementation)
     */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in apply_modulation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_sleep_apply_modulation: bridge is NULL");
        return -1;
    }
    if (!synapse) {
        NIMCP_LOGGING_ERROR("NULL synapse in apply_modulation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "triplet_stdp_sleep_apply_modulation: synapse is NULL");
        return -1;
    }

    /* This would modify synapse parameters based on sleep state */
    /* Implementation depends on synapse structure having modifiable params */

    NIMCP_LOGGING_DEBUG("Applied sleep modulation to triplet STDP synapse");
    return 0;
}
