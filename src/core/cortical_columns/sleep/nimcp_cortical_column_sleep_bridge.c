/**
 * @file nimcp_cortical_column_sleep_bridge.c
 * @brief Sleep-Cortical Column Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-17
 */

#include "core/cortical_columns/sleep/nimcp_cortical_column_sleep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_column_sleep_bridge)

/* ========================================================================
 * INTERNAL STRUCTURES
 * ======================================================================== */

struct cortical_column_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    cortical_column_sleep_config_t config;
    hypercolumn_t* hypercolumn;
    sleep_system_t sleep_system;
    cortical_column_sleep_effects_t effects;
    bool callback_registered;
};

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

static void cortical_column_on_sleep_state_change(sleep_state_t new_state, void* user_data);

/* ========================================================================
 * CALLBACK IMPLEMENTATION
 * ======================================================================== */

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update cortical column parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state transitions affect column responsiveness immediately
 * - Neuromodulators change lateral inhibition strength
 * - Competition dynamics shift with arousal level
 */
static void cortical_column_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    cortical_column_sleep_bridge_t bridge = (cortical_column_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Cortical column bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->effects.current_state = new_state;

    /* Update receptive field gain */
    if (bridge->config.enable_receptive_field_modulation) {
        float rf_base = cortical_column_sleep_rf_gain_for_state(new_state);
        bridge->effects.receptive_field_gain =
            rf_base * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength);
    }

    /* Update lateral inhibition strength */
    if (bridge->config.enable_inhibition_modulation) {
        bridge->effects.lateral_inhibition_strength =
            cortical_column_sleep_inhibition_for_state(new_state);
    }

    /* Update competition parameters */
    if (bridge->config.enable_competition_modulation) {
        bridge->effects.competition_temperature =
            cortical_column_sleep_temperature_for_state(new_state);
        bridge->effects.competition_mode =
            cortical_column_sleep_competition_mode_for_state(new_state);
    }

    /* Update activation threshold */
    if (bridge->config.enable_threshold_modulation) {
        switch (new_state) {
            case SLEEP_STATE_AWAKE:
                bridge->effects.activation_threshold_factor = COLUMN_SLEEP_THRESH_AWAKE;
                break;
            case SLEEP_STATE_DROWSY:
                bridge->effects.activation_threshold_factor = COLUMN_SLEEP_THRESH_DROWSY;
                break;
            case SLEEP_STATE_LIGHT_NREM:
                bridge->effects.activation_threshold_factor = COLUMN_SLEEP_THRESH_LIGHT_NREM;
                break;
            case SLEEP_STATE_DEEP_NREM:
                bridge->effects.activation_threshold_factor = COLUMN_SLEEP_THRESH_DEEP_NREM;
                break;
            case SLEEP_STATE_REM:
                bridge->effects.activation_threshold_factor = COLUMN_SLEEP_THRESH_REM;
                break;
            default:
                bridge->effects.activation_threshold_factor = COLUMN_SLEEP_THRESH_AWAKE;
                break;
        }
    }

    /* Update offline status */
    bridge->effects.columns_offline = (new_state == SLEEP_STATE_DEEP_NREM);

    /* Spindle detection (light NREM) */
    bridge->effects.in_spindle = (new_state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Cortical column modulated: RF=%.2f, inhib=%.2f, temp=%.2f, offline=%d",
                        bridge->effects.receptive_field_gain,
                        bridge->effects.lateral_inhibition_strength,
                        bridge->effects.competition_temperature,
                        bridge->effects.columns_offline);
}

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

int cortical_column_sleep_default_config(cortical_column_sleep_config_t* config)
{
    /* Guard clause: Validate config */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    config->enable_receptive_field_modulation = true;
    config->enable_inhibition_modulation = true;
    config->enable_competition_modulation = true;
    config->enable_threshold_modulation = true;
    config->modulation_strength = 1.0f;

    return 0;
}

cortical_column_sleep_bridge_t cortical_column_sleep_bridge_create(
    const cortical_column_sleep_config_t* config,
    hypercolumn_t* hypercolumn,
    sleep_system_t sleep)
{
    /* Guard clauses: Validate required parameters */
    if (!hypercolumn || !sleep) {
        NIMCP_LOGGING_ERROR("NULL hypercolumn or sleep system in bridge create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_column_sleep_bridge_create: required parameter is NULL (hypercolumn, sleep)");
        return NULL;
    }

    /* Allocate bridge */
    struct cortical_column_sleep_bridge_struct* bridge =
        (struct cortical_column_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct cortical_column_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate cortical column sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_column_sleep_bridge_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(struct cortical_column_sleep_bridge_struct));

    /* Initialize configuration */
    if (config) {
        bridge->config = *config;
    } else {
        cortical_column_sleep_default_config(&bridge->config);
    }

    /* Store references */
    bridge->hypercolumn = hypercolumn;
    bridge->sleep_system = sleep;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "cortical_column_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for cortical column sleep bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_column_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize effects */
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.receptive_field_gain = 1.0f;
    bridge->effects.lateral_inhibition_strength = 1.0f;
    bridge->effects.competition_temperature = NIMCP_TEMPERATURE_DEFAULT;
    bridge->effects.activation_threshold_factor = 1.0f;
    bridge->effects.competition_mode = CC_COMPETITION_WINNER_TAKE_ALL;
    bridge->effects.columns_offline = false;
    bridge->effects.in_spindle = false;
    bridge->effects.sleep_pressure = 0.0f;

    /* Register callback with sleep system */
    bool registered = sleep_register_state_callback(
        sleep,
        cortical_column_on_sleep_state_change,
        bridge);

    if (!registered) {
        NIMCP_LOGGING_WARN("Failed to register cortical column sleep callback");
        /* Continue anyway - bridge still functional, just won't get automatic updates */
    } else {
        bridge->callback_registered = true;
        NIMCP_LOGGING_INFO("Cortical column sleep bridge created and callback registered");
    }

    return bridge;
}

void cortical_column_sleep_bridge_destroy(cortical_column_sleep_bridge_t bridge)
{
    /* Guard clause: NULL safe */
    if (!bridge) {
        return;
    }

    /* Unregister callback */
    if (bridge->callback_registered) {
        sleep_unregister_state_callback(
            bridge->sleep_system,
            cortical_column_on_sleep_state_change,
            bridge);
        NIMCP_LOGGING_DEBUG("Cortical column sleep callback unregistered");
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_DEBUG("Cortical column sleep bridge destroyed");
}

/* ========================================================================
 * STATE UPDATE FUNCTIONS
 * ======================================================================== */

int cortical_column_sleep_update(cortical_column_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Effects are updated automatically via callback */
    /* This function exists for manual refresh if needed */

    return 0;
}

int cortical_column_sleep_apply_modulation(cortical_column_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Copy state under lock, then call external functions outside lock
     * to avoid potential deadlock if external code acquires other locks */
    nimcp_mutex_lock(bridge->base.mutex);

    hypercolumn_t* hcol = bridge->hypercolumn;
    bool do_competition = bridge->config.enable_competition_modulation;
    cc_competition_mode_t comp_mode = bridge->effects.competition_mode;
    float comp_temp = bridge->effects.competition_temperature;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply competition mode and temperature (outside lock) */
    if (do_competition) {
        hypercolumn_run_competition(hcol, comp_mode, comp_temp);
    }

    return 0;
}

/* ========================================================================
 * STATE ACCESS FUNCTIONS
 * ======================================================================== */

int cortical_column_sleep_get_effects(
    const cortical_column_sleep_bridge_t bridge,
    cortical_column_sleep_effects_t* effects)
{
    /* Guard clauses: Validate parameters */
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortical_column_sleep_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float cortical_column_sleep_get_rf_gain(const cortical_column_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        return -1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float gain = bridge->effects.receptive_field_gain;
    nimcp_mutex_unlock(bridge->base.mutex);

    return gain;
}

float cortical_column_sleep_get_inhibition(const cortical_column_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        return -1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float inhib = bridge->effects.lateral_inhibition_strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return inhib;
}

bool cortical_column_sleep_is_offline(const cortical_column_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool offline = bridge->effects.columns_offline;
    nimcp_mutex_unlock(bridge->base.mutex);

    return offline;
}

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

float cortical_column_sleep_rf_gain_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return COLUMN_SLEEP_RF_GAIN_AWAKE;
        case SLEEP_STATE_DROWSY:
            return COLUMN_SLEEP_RF_GAIN_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
            return COLUMN_SLEEP_RF_GAIN_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:
            return COLUMN_SLEEP_RF_GAIN_DEEP_NREM;
        case SLEEP_STATE_REM:
            return COLUMN_SLEEP_RF_GAIN_REM;
        default:
            return COLUMN_SLEEP_RF_GAIN_AWAKE;
    }
}

float cortical_column_sleep_inhibition_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return COLUMN_SLEEP_INHIB_AWAKE;
        case SLEEP_STATE_DROWSY:
            return COLUMN_SLEEP_INHIB_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:
            return COLUMN_SLEEP_INHIB_NREM;
        case SLEEP_STATE_REM:
            return COLUMN_SLEEP_INHIB_REM;
        default:
            return COLUMN_SLEEP_INHIB_AWAKE;
    }
}

float cortical_column_sleep_temperature_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return COLUMN_SLEEP_TEMP_AWAKE;
        case SLEEP_STATE_DROWSY:
            return COLUMN_SLEEP_TEMP_DROWSY;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:
            return COLUMN_SLEEP_TEMP_NREM;
        case SLEEP_STATE_REM:
            return COLUMN_SLEEP_TEMP_REM;
        default:
            return COLUMN_SLEEP_TEMP_AWAKE;
    }
}

cc_competition_mode_t cortical_column_sleep_competition_mode_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:
            return CC_COMPETITION_WINNER_TAKE_ALL;
        case SLEEP_STATE_DROWSY:
            return CC_COMPETITION_SOFTMAX;
        case SLEEP_STATE_LIGHT_NREM:
        case SLEEP_STATE_DEEP_NREM:
            return CC_COMPETITION_NONE; /* No competition during sleep */
        case SLEEP_STATE_REM:
            return CC_COMPETITION_SOFTMAX; /* Soft competition in REM */
        default:
            return CC_COMPETITION_WINNER_TAKE_ALL;
    }
}
