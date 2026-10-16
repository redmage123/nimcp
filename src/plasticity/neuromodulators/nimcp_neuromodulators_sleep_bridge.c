/**
 * @file nimcp_neuromodulators_sleep_bridge.c
 * @brief Sleep-Neuromodulator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Implementation of bidirectional sleep-neuromodulator integration
 * WHY:  Sleep states fundamentally alter neuromodulator profiles
 * HOW:  Query sleep state, compute modulation factors, apply to neuromodulators
 *
 * @author NIMCP Development Team
 */

#include "plasticity/neuromodulators/nimcp_neuromodulators_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromodulators_sleep_bridge)

/* Security integration */

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct neuromod_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    neuromodulators_sleep_config_t config;  /**< Configuration */
    neuromodulator_system_t neuromod_system; /**< Connected neuromod system */
    sleep_system_t sleep_system;             /**< Connected sleep system */
    neuromod_sleep_effects_t effects;        /**< Current computed effects */
    nimcp_platform_mutex_t* mutex;                    /**< Thread safety */
    bool callback_registered;  /**< Track if callback is registered for cleanup */
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(neuromod_sleep_bridge, struct neuromod_sleep_bridge_struct)

/* Forward declarations */
static void neuromod_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update neuromodulator profiles for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Neuromodulators define sleep stages as much as oscillations do
 * - ACh high in waking and REM, low in NREM
 * - NE/5-HT drop during REM (locus coeruleus/raphe silent)
 * - Sleep-dependent neuromodulator shifts enable memory consolidation
 */
static void neuromod_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    neuromod_sleep_bridge_t bridge = (neuromod_sleep_bridge_t)user_data;

    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Neuromodulator bridge received sleep state: %d", new_state);

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Compute state-based modulation factors */
    if (bridge->config.enable_sleep_state_modulation) {
        float ach_base = neuromod_sleep_get_ach_factor(new_state);
        float ne_base = neuromod_sleep_get_ne_factor(new_state);
        float da_base = neuromod_sleep_get_da_factor(new_state);
        float serotonin_base = neuromod_sleep_get_serotonin_factor(new_state);

        /* Apply modulation strengths (blend toward 1.0) */
        bridge->effects.ach_factor = 1.0f + (ach_base - 1.0f) * bridge->config.ach_modulation_strength;
        bridge->effects.ne_factor = 1.0f + (ne_base - 1.0f) * bridge->config.ne_modulation_strength;
        bridge->effects.da_factor = 1.0f + (da_base - 1.0f) * bridge->config.da_modulation_strength;
        bridge->effects.serotonin_factor = 1.0f + (serotonin_base - 1.0f) * bridge->config.serotonin_modulation_strength;
    }

    /* Compute derived effects */
    bridge->effects.learning_rate_modifier =
        (bridge->effects.ach_factor * 0.4f) +
        (bridge->effects.da_factor * 0.3f) +
        (bridge->effects.ne_factor * 0.3f);

    bridge->effects.attention_modifier =
        (bridge->effects.ach_factor * 0.6f) +
        (bridge->effects.ne_factor * 0.4f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Neuromod modulated: ACh=%.2f, NE=%.2f, DA=%.2f, 5HT=%.2f",
                        bridge->effects.ach_factor,
                        bridge->effects.ne_factor,
                        bridge->effects.da_factor,
                        bridge->effects.serotonin_factor);
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * WHAT: Get default sleep-neuromodulator bridge configuration
 * WHY:  Provide sensible defaults based on biological evidence
 * HOW:  Set evidence-based parameters from sleep neuroscience research
 */
int neuromod_sleep_default_config(neuromodulators_sleep_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("neuromod_sleep_default_config: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_sleep_default_config: config is NULL");
        return -1;
    }

    config->enable_sleep_state_modulation = true;
    config->enable_pressure_effects = true;
    config->enable_neuromod_sleep_effects = true;

    config->ach_modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    config->ne_modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    config->da_modulation_strength = 0.8f;  /* DA changes less dramatic */
    config->serotonin_modulation_strength = NIMCP_SENSITIVITY_DEFAULT;

    config->pressure_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    return 0;
}

/**
 * WHAT: Create sleep-neuromodulator bridge
 * WHY:  Initialize integration between sleep and neuromodulator systems
 * HOW:  Allocate structure, store references, create mutex
 */
neuromod_sleep_bridge_t neuromod_sleep_bridge_create(
    const neuromodulators_sleep_config_t* config,
    neuromodulator_system_t neuromod_system,
    sleep_system_t sleep_system)
{
    if (!neuromod_system) {
        NIMCP_LOGGING_ERROR("neuromod_sleep_bridge_create: NULL neuromod_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_system is NULL");

        return NULL;
    }

    if (!sleep_system) {
        NIMCP_LOGGING_ERROR("neuromod_sleep_bridge_create: NULL sleep_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_system is NULL");


        return NULL;
    }

    struct neuromod_sleep_bridge_struct* bridge =
        (struct neuromod_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct neuromod_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("neuromod_sleep_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(struct neuromod_sleep_bridge_struct));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        neuromod_sleep_default_config(&bridge->config);
    }

    bridge->neuromod_system = neuromod_system;
    bridge->sleep_system = sleep_system;

    /* Initialize effects to awake baseline */
    bridge->effects.ach_factor = 1.0f;
    bridge->effects.ne_factor = 1.0f;
    bridge->effects.da_factor = 1.0f;
    bridge->effects.serotonin_factor = 1.0f;
    bridge->effects.ne_release_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->effects.ach_release_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.sleep_pressure = 0.0f;
    bridge->effects.learning_rate_modifier = 1.0f;
    bridge->effects.attention_modifier = 1.0f;
    bridge->effects.sleep_inhibited = false;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "neuromodulators_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("neuromod_sleep_bridge_create: mutex creation failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep_system,
        neuromod_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for neuromodulator bridge");
    }

    /* Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep_system);
    neuromod_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Sleep-neuromodulator bridge created successfully");
    return bridge;
}

/**
 * WHAT: Destroy sleep-neuromodulator bridge
 * WHY:  Clean up resources, prevent memory leaks
 * HOW:  Free mutex, free structure (doesn't destroy connected systems)
 */
void neuromod_sleep_bridge_destroy(neuromod_sleep_bridge_t bridge) {
    if (!bridge) {
        return;
    }

    /* Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            neuromod_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for neuromodulator bridge");
        }
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Sleep-neuromodulator bridge destroyed");
}

/* ============================================================================
 * Update Functions (SLEEP → NEUROMODULATORS)
 * ============================================================================ */

/**
 * WHAT: Update neuromodulator effects from sleep system state
 * WHY:  Compute how current sleep state affects neuromodulator profiles
 * HOW:  Query sleep state and pressure, compute modulation factors
 */
int neuromod_sleep_update(neuromod_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("neuromod_sleep_update: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_sleep_update: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get sleep state */
    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    /* Compute state-based modulation factors */
    if (bridge->config.enable_sleep_state_modulation) {
        float ach_base = neuromod_sleep_get_ach_factor(state);
        float ne_base = neuromod_sleep_get_ne_factor(state);
        float da_base = neuromod_sleep_get_da_factor(state);
        float serotonin_base = neuromod_sleep_get_serotonin_factor(state);

        /* Apply modulation strengths (blend toward 1.0) */
        bridge->effects.ach_factor = 1.0f + (ach_base - 1.0f) * bridge->config.ach_modulation_strength;
        bridge->effects.ne_factor = 1.0f + (ne_base - 1.0f) * bridge->config.ne_modulation_strength;
        bridge->effects.da_factor = 1.0f + (da_base - 1.0f) * bridge->config.da_modulation_strength;
        bridge->effects.serotonin_factor = 1.0f + (serotonin_base - 1.0f) * bridge->config.serotonin_modulation_strength;
    }

    /* Compute release sensitivity from sleep pressure */
    if (bridge->config.enable_pressure_effects) {
        bridge->effects.ne_release_sensitivity = neuromod_sleep_compute_release_sensitivity(
            pressure, SLEEP_PRESSURE_THRESHOLD, SLEEP_PRESSURE_NE_SUPPRESSION);
        bridge->effects.ach_release_sensitivity = neuromod_sleep_compute_release_sensitivity(
            pressure, SLEEP_PRESSURE_THRESHOLD, SLEEP_PRESSURE_ACH_SUPPRESSION);
    }

    /* Check for sleep inhibition from high neuromodulators */
    if (bridge->config.enable_neuromod_sleep_effects) {
        float ne_level = neuromodulator_get_level(bridge->neuromod_system, NEUROMOD_NOREPINEPHRINE);
        float ach_level = neuromodulator_get_level(bridge->neuromod_system, NEUROMOD_ACETYLCHOLINE);
        bridge->effects.sleep_inhibited = (ne_level > NEUROMOD_NE_SLEEP_INHIBIT) ||
                                          (ach_level > NEUROMOD_ACH_REM_TRIGGER && state != SLEEP_STATE_AWAKE);
    }

    /* Compute derived effects */
    /* Learning rate: low during sleep (except encoding in REM) */
    bridge->effects.learning_rate_modifier =
        (bridge->effects.ach_factor * 0.4f) +
        (bridge->effects.da_factor * 0.3f) +
        (bridge->effects.ne_factor * 0.3f);

    /* Attention: mainly ACh and NE */
    bridge->effects.attention_modifier =
        (bridge->effects.ach_factor * 0.6f) +
        (bridge->effects.ne_factor * 0.4f);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Neuromod sleep effects updated: state=%d, pressure=%.2f, "
                       "ACh=%.2f, NE=%.2f, DA=%.2f, 5HT=%.2f",
                       state, pressure,
                       bridge->effects.ach_factor,
                       bridge->effects.ne_factor,
                       bridge->effects.da_factor,
                       bridge->effects.serotonin_factor);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/**
 * WHAT: Apply sleep-modulated neuromodulator levels
 * WHY:  Actually modify neuromodulator system based on sleep state
 * HOW:  Set neuromodulator levels using computed factors
 */
int neuromod_sleep_apply_modulation(neuromod_sleep_bridge_t bridge) {
    if (!bridge) {
        NIMCP_LOGGING_ERROR("neuromod_sleep_apply_modulation: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_sleep_apply_modulation: bridge is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current baseline levels */
    neuromodulator_pool_t pool = neuromodulator_pool_create();
    if (!neuromodulator_get_levels(bridge->neuromod_system, &pool)) {
        neuromodulator_pool_destroy(&pool);
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_WARN("neuromod_sleep_apply_modulation: failed to get levels");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuromod_sleep_apply_modulation: neuromodulator_get_levels is NULL");
        return -1;
    }

    /* Apply modulation factors to each neuromodulator */
    /* Note: We modulate the levels, not override them completely */
    /* This preserves event-driven changes while applying sleep modulation */
    float modulated_ach = neuromodulator_pool_get_acetylcholine(&pool) * bridge->effects.ach_factor;
    float modulated_ne = neuromodulator_pool_get_norepinephrine(&pool) * bridge->effects.ne_factor;
    float modulated_da = neuromodulator_pool_get_dopamine(&pool) * bridge->effects.da_factor;
    float modulated_5ht = neuromodulator_pool_get_serotonin(&pool) * bridge->effects.serotonin_factor;

    neuromodulator_pool_destroy(&pool);

    /* Clamp to valid range */
    modulated_ach = fminf(fmaxf(modulated_ach, 0.0f), 1.0f);
    modulated_ne = fminf(fmaxf(modulated_ne, 0.0f), 1.0f);
    modulated_da = fminf(fmaxf(modulated_da, 0.0f), 1.0f);
    modulated_5ht = fminf(fmaxf(modulated_5ht, 0.0f), 1.0f);

    /* Set modulated levels */
    neuromodulator_set_level(bridge->neuromod_system, NEUROMOD_ACETYLCHOLINE, modulated_ach);
    neuromodulator_set_level(bridge->neuromod_system, NEUROMOD_NOREPINEPHRINE, modulated_ne);
    neuromodulator_set_level(bridge->neuromod_system, NEUROMOD_DOPAMINE, modulated_da);
    neuromodulator_set_level(bridge->neuromod_system, NEUROMOD_SEROTONIN, modulated_5ht);

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * WHAT: Get current sleep effects on neuromodulators
 * WHY:  Query integrated sleep-neuromodulator state
 * HOW:  Copy neuromod_sleep_effects_t from bridge
 */
int neuromod_sleep_get_effects(
    const neuromod_sleep_bridge_t bridge,
    neuromod_sleep_effects_t* effects)
{
    if (!bridge) {
        NIMCP_LOGGING_ERROR("neuromod_sleep_get_effects: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_sleep_get_effects: bridge is NULL");
        return -1;
    }

    if (!effects) {
        NIMCP_LOGGING_ERROR("neuromod_sleep_get_effects: NULL effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_sleep_get_effects: effects is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get modulation factor for specific neuromodulator
 * WHY:  Query single neuromodulator's sleep modulation
 * HOW:  Look up from current effects structure
 */
float neuromod_sleep_get_factor(
    const neuromod_sleep_bridge_t bridge,
    neuromodulator_type_t type)
{
    if (!bridge) {
        return 1.0f;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float factor = 1.0f;

    switch (type) {
        case NEUROMOD_ACETYLCHOLINE:
            factor = bridge->effects.ach_factor;
            break;
        case NEUROMOD_NOREPINEPHRINE:
            factor = bridge->effects.ne_factor;
            break;
        case NEUROMOD_DOPAMINE:
            factor = bridge->effects.da_factor;
            break;
        case NEUROMOD_SEROTONIN:
            factor = bridge->effects.serotonin_factor;
            break;
        default:
            factor = 1.0f;  /* No sleep modulation for GABA/GLU */
            break;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return factor;
}

/**
 * WHAT: Check if high neuromodulators are inhibiting sleep
 * WHY:  Detect stress-induced insomnia conditions
 * HOW:  Return cached inhibition flag
 */
bool neuromod_sleep_is_inhibited(const neuromod_sleep_bridge_t bridge) {
    if (!bridge) {
        return false;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bool inhibited = bridge->effects.sleep_inhibited;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return inhibited;
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get acetylcholine factor for sleep state
 * WHY:  ACh varies dramatically across sleep stages
 * HOW:  Return predefined factor for each state
 */
float neuromod_sleep_get_ach_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return SLEEP_NEUROMOD_ACH_AWAKE;
        case SLEEP_STATE_DROWSY:
            return SLEEP_NEUROMOD_ACH_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return SLEEP_NEUROMOD_ACH_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return SLEEP_NEUROMOD_ACH_DEEP_NREM;
        case SLEEP_STATE_REM:
            return SLEEP_NEUROMOD_ACH_REM;
        default:
            return SLEEP_NEUROMOD_ACH_AWAKE;
    }
}

/**
 * WHAT: Get norepinephrine factor for sleep state
 * WHY:  NE drops dramatically during sleep, especially REM
 * HOW:  Return predefined factor for each state
 */
float neuromod_sleep_get_ne_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return SLEEP_NEUROMOD_NE_AWAKE;
        case SLEEP_STATE_DROWSY:
            return SLEEP_NEUROMOD_NE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return SLEEP_NEUROMOD_NE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return SLEEP_NEUROMOD_NE_DEEP_NREM;
        case SLEEP_STATE_REM:
            return SLEEP_NEUROMOD_NE_REM;
        default:
            return SLEEP_NEUROMOD_NE_AWAKE;
    }
}

/**
 * WHAT: Get dopamine factor for sleep state
 * WHY:  DA shows moderate changes during sleep
 * HOW:  Return predefined factor for each state
 */
float neuromod_sleep_get_da_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return SLEEP_NEUROMOD_DA_AWAKE;
        case SLEEP_STATE_DROWSY:
            return SLEEP_NEUROMOD_DA_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return SLEEP_NEUROMOD_DA_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return SLEEP_NEUROMOD_DA_DEEP_NREM;
        case SLEEP_STATE_REM:
            return SLEEP_NEUROMOD_DA_REM;
        default:
            return SLEEP_NEUROMOD_DA_AWAKE;
    }
}

/**
 * WHAT: Get serotonin factor for sleep state
 * WHY:  5-HT drops during REM (raphe silent)
 * HOW:  Return predefined factor for each state
 */
float neuromod_sleep_get_serotonin_factor(sleep_state_t state) {
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return SLEEP_NEUROMOD_5HT_AWAKE;
        case SLEEP_STATE_DROWSY:
            return SLEEP_NEUROMOD_5HT_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return SLEEP_NEUROMOD_5HT_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return SLEEP_NEUROMOD_5HT_DEEP_NREM;
        case SLEEP_STATE_REM:
            return SLEEP_NEUROMOD_5HT_REM;
        default:
            return SLEEP_NEUROMOD_5HT_AWAKE;
    }
}

/**
 * WHAT: Compute release sensitivity modifier from sleep pressure
 * WHY:  High sleep pressure suppresses arousal neuromodulator responses
 * HOW:  Reduce sensitivity when pressure > threshold
 */
float neuromod_sleep_compute_release_sensitivity(
    float pressure,
    float threshold,
    float suppression)
{
    if (pressure < threshold) {
        return 1.0f;
    }

    /* Linear reduction from 1.0 to (1.0 - suppression) */
    float excess = (pressure - threshold) / (1.0f - threshold + 1e-6f);
    float sensitivity = 1.0f - (suppression * excess);

    return fmaxf(sensitivity, 1.0f - suppression);
}
