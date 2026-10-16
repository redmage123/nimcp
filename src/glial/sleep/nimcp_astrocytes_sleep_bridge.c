/**
 * @file nimcp_astrocytes_sleep_bridge.c
 * @brief Sleep-Astrocyte Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and astrocytes
 * WHY:  Astrocytes are critical for adenosine accumulation, glymphatic clearance,
 *       and synaptic renormalization during sleep
 * HOW:  Sleep state modulates astrocyte calcium signaling, gliotransmitter release,
 *       and metabolic support
 */

#include "glial/sleep/nimcp_astrocytes_sleep_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(astrocytes_sleep_bridge)

/**
 * WHAT: Internal bridge structure
 * WHY:  Encapsulate implementation details
 * HOW:  Store config, effects, sleep system, and synchronization
 */
struct astro_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    astro_sleep_config_t config;           /**< Configuration parameters */
    sleep_system_t sleep_system;           /**< Sleep system handle */
    astro_sleep_effects_t effects;         /**< Current modulation effects */
    bool callback_registered;              /**< Track callback for cleanup */

    /* Adenosine accumulation state */
    float adenosine_accumulation_rate;     /**< Current accumulation rate */
    float last_activity_level;             /**< Last neural activity level */

};

/* Forward declarations */
static void astro_on_sleep_state_change(sleep_state_t new_state, void* user_data);
static void compute_effects(astro_sleep_bridge_t bridge, sleep_state_t state, float pressure);
static float get_adenosine_factor_for_state(sleep_state_t state);
static float get_glymphatic_factor_for_state(sleep_state_t state);
static float get_calcium_factor_for_state(sleep_state_t state);
static float get_coupling_factor_for_state(sleep_state_t state);
static float get_downscale_factor_for_state(sleep_state_t state);

/* ========================================================================
 * INTERNAL CALLBACK
 * ======================================================================== */

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update astrocyte parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state transitions trigger rapid astrocyte behavioral changes
 * - Entry to deep NREM activates peak glymphatic clearance (10-20x)
 * - Calcium wave dynamics change with sleep oscillations
 * - Gap junction coupling increases during NREM for network coordination
 */
static void astro_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    astro_sleep_bridge_t bridge = (astro_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    /* Guard clause: Validate sleep system */
    if (!bridge->sleep_system) {
        NIMCP_LOGGING_ERROR("NULL sleep system in bridge during state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Astrocyte bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current sleep pressure */
    float pressure = sleep_get_pressure(bridge->sleep_system);

    /* Recompute all effects for new state */
    compute_effects(bridge, new_state, pressure);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Astrocyte modulated: adenosine=%.2f, glymphatic=%.2f, calcium=%.2f",
                        bridge->effects.adenosine_level,
                        bridge->effects.glymphatic_clearance_factor,
                        bridge->effects.calcium_wave_factor);
}

/**
 * WHAT: Compute all modulation effects for given state and pressure
 * WHY:  Centralize effect computation logic
 * HOW:  Apply biological rules based on sleep state
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Active glutamate uptake, high metabolic demand, lactate shuttle active
 * - DROWSY: Adenosine accumulation increases, sleep pressure builds
 * - LIGHT_NREM: Calcium wave frequency increases, gap junction coupling increases
 * - DEEP_NREM: Peak glymphatic clearance (10-20x), synaptic downscaling
 * - REM: Moderate activity, selective metabolic support
 */
static void compute_effects(astro_sleep_bridge_t bridge,
                           sleep_state_t state,
                           float pressure)
{
    /* Update current state */
    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    /* Adenosine modulation */
    if (bridge->config.enable_adenosine_modulation) {
        float base_adenosine = get_adenosine_factor_for_state(state);
        /* Adenosine level is accumulated, not reset - but rate changes with state */
        bridge->adenosine_accumulation_rate = base_adenosine;
    }

    /* Glymphatic clearance modulation */
    if (bridge->config.enable_glymphatic_modulation) {
        float base_glymphatic = get_glymphatic_factor_for_state(state);
        bridge->effects.glymphatic_clearance_factor =
            base_glymphatic * bridge->config.modulation_strength *
            bridge->config.glymphatic_clearance_multiplier;

        /* Glymphatic system is active during NREM sleep */
        bridge->effects.glymphatic_active = (state == SLEEP_STATE_DEEP_NREM ||
                                             state == SLEEP_STATE_LIGHT_NREM);
    } else {
        bridge->effects.glymphatic_clearance_factor = 1.0f;
        bridge->effects.glymphatic_active = false;
    }

    /* Calcium wave modulation */
    if (bridge->config.enable_calcium_modulation) {
        float base_calcium = get_calcium_factor_for_state(state);
        bridge->effects.calcium_wave_factor =
            base_calcium * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength) * ASTRO_SLEEP_CALCIUM_AWAKE;
    } else {
        bridge->effects.calcium_wave_factor = ASTRO_SLEEP_CALCIUM_AWAKE;
    }

    /* Gap junction coupling modulation */
    if (bridge->config.enable_coupling_modulation) {
        float base_coupling = get_coupling_factor_for_state(state);
        bridge->effects.gap_junction_coupling_factor =
            base_coupling * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength) * ASTRO_SLEEP_COUPLING_AWAKE;
    } else {
        bridge->effects.gap_junction_coupling_factor = ASTRO_SLEEP_COUPLING_AWAKE;
    }

    /* Synaptic renormalization/downscaling modulation */
    if (bridge->config.enable_downscaling_modulation) {
        float base_downscale = get_downscale_factor_for_state(state);
        bridge->effects.synaptic_renormalization_factor =
            base_downscale * bridge->config.modulation_strength;

        /* Downscaling is active during deep NREM */
        bridge->effects.downscaling_active = (state == SLEEP_STATE_DEEP_NREM &&
                                              base_downscale > 0.5f);
    } else {
        bridge->effects.synaptic_renormalization_factor = 0.0f;
        bridge->effects.downscaling_active = false;
    }

    /* Lactate shuttle modulation */
    if (bridge->config.enable_lactate_modulation) {
        /* Lactate shuttle is most active during wakefulness, reduced during sleep */
        switch (state) {
            case SLEEP_STATE_AWAKE:
                bridge->effects.lactate_shuttle_factor = 1.0f;
                break;
            case SLEEP_STATE_DROWSY:
                bridge->effects.lactate_shuttle_factor = 0.8f;
                break;
            case SLEEP_STATE_LIGHT_NREM:
                bridge->effects.lactate_shuttle_factor = 0.5f;
                break;
            case SLEEP_STATE_DEEP_NREM:
                bridge->effects.lactate_shuttle_factor = 0.3f;
                break;
            case SLEEP_STATE_REM:
                bridge->effects.lactate_shuttle_factor = 0.6f;
                break;
            default:
                bridge->effects.lactate_shuttle_factor = 1.0f;
        }
    } else {
        bridge->effects.lactate_shuttle_factor = 1.0f;
    }
}

/* ========================================================================
 * STATE FACTOR LOOKUP FUNCTIONS
 * ======================================================================== */

static float get_adenosine_factor_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ASTRO_SLEEP_ADENOSINE_AWAKE;
        case SLEEP_STATE_DROWSY:     return ASTRO_SLEEP_ADENOSINE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ASTRO_SLEEP_ADENOSINE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ASTRO_SLEEP_ADENOSINE_DEEP_NREM;
        case SLEEP_STATE_REM:        return ASTRO_SLEEP_ADENOSINE_REM;
        default:                     return ASTRO_SLEEP_ADENOSINE_AWAKE;
    }
}

static float get_glymphatic_factor_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ASTRO_SLEEP_GLYMPHATIC_AWAKE;
        case SLEEP_STATE_DROWSY:     return ASTRO_SLEEP_GLYMPHATIC_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ASTRO_SLEEP_GLYMPHATIC_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ASTRO_SLEEP_GLYMPHATIC_DEEP_NREM;
        case SLEEP_STATE_REM:        return ASTRO_SLEEP_GLYMPHATIC_REM;
        default:                     return ASTRO_SLEEP_GLYMPHATIC_AWAKE;
    }
}

static float get_calcium_factor_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ASTRO_SLEEP_CALCIUM_AWAKE;
        case SLEEP_STATE_DROWSY:     return ASTRO_SLEEP_CALCIUM_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ASTRO_SLEEP_CALCIUM_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ASTRO_SLEEP_CALCIUM_DEEP_NREM;
        case SLEEP_STATE_REM:        return ASTRO_SLEEP_CALCIUM_REM;
        default:                     return ASTRO_SLEEP_CALCIUM_AWAKE;
    }
}

static float get_coupling_factor_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ASTRO_SLEEP_COUPLING_AWAKE;
        case SLEEP_STATE_DROWSY:     return ASTRO_SLEEP_COUPLING_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ASTRO_SLEEP_COUPLING_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ASTRO_SLEEP_COUPLING_DEEP_NREM;
        case SLEEP_STATE_REM:        return ASTRO_SLEEP_COUPLING_REM;
        default:                     return ASTRO_SLEEP_COUPLING_AWAKE;
    }
}

static float get_downscale_factor_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return ASTRO_SLEEP_DOWNSCALE_AWAKE;
        case SLEEP_STATE_DROWSY:     return ASTRO_SLEEP_DOWNSCALE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return ASTRO_SLEEP_DOWNSCALE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return ASTRO_SLEEP_DOWNSCALE_DEEP_NREM;
        case SLEEP_STATE_REM:        return ASTRO_SLEEP_DOWNSCALE_REM;
        default:                     return ASTRO_SLEEP_DOWNSCALE_AWAKE;
    }
}

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

int astro_sleep_default_config(astro_sleep_config_t* config)
{
    /* Guard clause: Validate config pointer */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_sleep_default_config: config is NULL");
        return -1;
    }

    /* WHAT: Set default configuration values
     * WHY:  Enable all modulation features for biological realism
     * HOW:  Initialize struct with sensible defaults
     */
    config->enable_adenosine_modulation = true;
    config->enable_glymphatic_modulation = true;
    config->enable_calcium_modulation = true;
    config->enable_coupling_modulation = true;
    config->enable_downscaling_modulation = true;
    config->enable_lactate_modulation = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    config->glymphatic_clearance_multiplier = 15.0f;  /* 15x during deep sleep */
    config->adenosine_decay_rate = 0.05f;  /* Per minute during sleep */

    return 0;
}

astro_sleep_bridge_t astro_sleep_create(
    const astro_sleep_config_t* config,
    sleep_system_t sleep)
{
    /* Guard clause: Validate sleep system */
    if (!sleep) {
        NIMCP_LOGGING_ERROR("NULL sleep system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep is NULL");

        return NULL;
    }

    /* WHAT: Allocate bridge structure
     * WHY:  Create instance for this integration
     * HOW:  Allocate and zero-initialize
     */
    struct astro_sleep_bridge_struct* bridge =
        (struct astro_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct astro_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate astrocyte-sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(struct astro_sleep_bridge_struct));

    /* WHAT: Initialize configuration
     * WHY:  Use provided config or defaults
     * HOW:  Copy or call default_config
     */
    if (config) {
        bridge->config = *config;
    } else {
        astro_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep;

    /* WHAT: Initialize effects to awake defaults
     * WHY:  Start with sensible baseline values
     * HOW:  Set factors to awake-state values
     */
    bridge->effects.adenosine_level = 0.3f;  /* Baseline adenosine */
    bridge->effects.glymphatic_clearance_factor = 1.0f;
    bridge->effects.calcium_wave_factor = ASTRO_SLEEP_CALCIUM_AWAKE;
    bridge->effects.gap_junction_coupling_factor = ASTRO_SLEEP_COUPLING_AWAKE;
    bridge->effects.synaptic_renormalization_factor = 0.0f;
    bridge->effects.lactate_shuttle_factor = 1.0f;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.sleep_pressure = 0.0f;
    bridge->effects.glymphatic_active = false;
    bridge->effects.downscaling_active = false;

    /* Initialize adenosine accumulation state */
    bridge->adenosine_accumulation_rate = ASTRO_SLEEP_ADENOSINE_AWAKE;
    bridge->last_activity_level = 0.0f;

    /* WHAT: Initialize bio-async fields
     * WHY:  Start with bio-async disabled
     * HOW:  Set fields to NULL/false
     */
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    /* WHAT: Create thread safety mutex
     * WHY:  Protect concurrent access to effects
     * HOW:  Platform-agnostic mutex creation
     */
    if (bridge_base_init(&bridge->base, 0, "astrocytes_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for astrocyte-sleep bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "astro_sleep_create: bridge->base is NULL");
        return NULL;
    }

    /* WHAT: Register callback for automatic state updates
     * WHY:  Get immediate notifications on sleep state changes
     * HOW:  Observer pattern via sleep system callback API
     */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        astro_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for astrocyte bridge");
    }

    /* WHAT: Get initial state immediately
     * WHY:  Ensure effects match current sleep state
     * HOW:  Query current state and trigger callback manually
     */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    astro_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Astrocyte-sleep bridge created");
    return bridge;
}

void astro_sleep_destroy(astro_sleep_bridge_t bridge)
{
    /* Guard clause: NULL safe */
    if (!bridge) return;

    /* WHAT: Disconnect bio-async if connected
     * WHY:  Clean shutdown of messaging
     * HOW:  Call disconnect function
     */
    if (bridge->base.bio_async_enabled) {
        astro_sleep_disconnect_bio_async(bridge);
    }

    /* WHAT: Unregister callback if it was registered
     * WHY:  Clean shutdown, prevent dangling callback
     * HOW:  Call sleep system unregister API
     */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            astro_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for astrocyte bridge");
        }
    }

    /* WHAT: Destroy mutex
     * WHY:  Free synchronization resources
     * HOW:  Platform-agnostic mutex destruction
     */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* WHAT: Free bridge structure
     * WHY:  Prevent memory leak
     * HOW:  Single free call
     */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Astrocyte-sleep bridge destroyed");
}

/* ========================================================================
 * ADENOSINE FUNCTIONS
 * ======================================================================== */

float astro_sleep_accumulate_adenosine(
    astro_sleep_bridge_t bridge,
    float activity_level)
{
    /* Guard clause: Validate bridge */
    if (!bridge) return 0.0f;

    /* WHAT: Accumulate adenosine based on neural activity
     * WHY:  Sleep pressure builds from metabolic activity
     * HOW:  Increase adenosine proportional to activity
     *
     * BIOLOGICAL BASIS:
     * - ATP breakdown during neural activity produces adenosine
     * - Adenosine accumulates in extracellular space
     * - High adenosine activates A1 receptors → sleep pressure
     * - Accumulation rate modulated by current sleep state
     */
    nimcp_mutex_lock(bridge->base.mutex);

    float accumulation = activity_level * bridge->adenosine_accumulation_rate * 0.01f;
    bridge->effects.adenosine_level += accumulation;

    /* Clamp to [0, 1] range */
    if (bridge->effects.adenosine_level > 1.0f) {
        bridge->effects.adenosine_level = 1.0f;
    }

    bridge->last_activity_level = activity_level;

    float result = bridge->effects.adenosine_level;

    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

float astro_sleep_clear_adenosine(
    astro_sleep_bridge_t bridge,
    float dt_ms)
{
    /* Guard clause: Validate bridge */
    if (!bridge) return 0.0f;

    /* WHAT: Clear adenosine during sleep
     * WHY:  Sleep reduces adenosine, restoring wakefulness capacity
     * HOW:  Exponential decay based on sleep depth
     *
     * BIOLOGICAL BASIS:
     * - Adenosine is cleared via adenosine kinase and deaminase
     * - Clearance rate increases during sleep
     * - Deep sleep has fastest clearance
     * - Caffeine blocks A1 receptors, masking adenosine effects
     */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Calculate decay rate based on sleep state */
    float decay_rate = bridge->config.adenosine_decay_rate;
    sleep_state_t state = bridge->effects.current_state;

    /* Modulate decay by sleep state */
    switch (state) {
        case SLEEP_STATE_DEEP_NREM:
            decay_rate *= 3.0f;  /* 3x faster clearance in deep sleep */
            break;
        case SLEEP_STATE_LIGHT_NREM:
            decay_rate *= 2.0f;  /* 2x faster in light sleep */
            break;
        case SLEEP_STATE_REM:
            decay_rate *= 1.5f;  /* 1.5x in REM */
            break;
        case SLEEP_STATE_DROWSY:
            decay_rate *= 1.0f;  /* Normal rate when drowsy */
            break;
        case SLEEP_STATE_AWAKE:
            decay_rate *= 0.5f;  /* Slower clearance when awake */
            break;
        default:
            break;
    }

    /* Apply exponential decay */
    float dt_minutes = dt_ms / 60000.0f;
    float decay_factor = expf(-decay_rate * dt_minutes);
    bridge->effects.adenosine_level *= decay_factor;

    /* Ensure minimum baseline */
    if (bridge->effects.adenosine_level < 0.05f) {
        bridge->effects.adenosine_level = 0.05f;
    }

    float result = bridge->effects.adenosine_level;

    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

float astro_sleep_get_adenosine_level(const astro_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return 0.3f;

    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.adenosine_level;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ========================================================================
 * GLYMPHATIC FUNCTIONS
 * ======================================================================== */

float astro_sleep_enable_glymphatic(astro_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) return 1.0f;

    /* WHAT: Enable glymphatic clearance mode
     * WHY:  Activate waste removal during sleep
     * HOW:  Set glymphatic factor based on sleep depth
     *
     * BIOLOGICAL BASIS:
     * - Glymphatic system clears metabolic waste during sleep
     * - Interstitial space expands ~60% during sleep
     * - CSF flow through perivascular channels increases
     * - Peak clearance during deep NREM (10-20x waking levels)
     */
    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = bridge->effects.current_state;
    float clearance = get_glymphatic_factor_for_state(state) *
                      bridge->config.glymphatic_clearance_multiplier;

    bridge->effects.glymphatic_clearance_factor = clearance;
    bridge->effects.glymphatic_active = (clearance > 1.5f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return clearance;
}

float astro_sleep_get_clearance_rate(const astro_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.glymphatic_clearance_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

bool astro_sleep_is_glymphatic_active(const astro_sleep_bridge_t bridge)
{
    /* Guard clause: Return false if NULL */
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.glymphatic_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ========================================================================
 * SYNAPTIC DOWNSCALING FUNCTIONS
 * ======================================================================== */

float astro_sleep_initiate_downscaling(astro_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) return 0.0f;

    /* WHAT: Initiate synaptic downscaling
     * WHY:  Coordinate homeostatic scaling during sleep
     * HOW:  Set downscaling factor based on sleep depth
     *
     * BIOLOGICAL BASIS:
     * - Synaptic potentiation during waking increases total synaptic weight
     * - Sleep enables global synaptic renormalization
     * - Astrocytes coordinate this via gliotransmitter release
     * - Deep NREM provides optimal conditions for downscaling
     * - Preserves relative synaptic strengths while reducing total
     */
    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = bridge->effects.current_state;
    float factor = get_downscale_factor_for_state(state) *
                   bridge->config.modulation_strength;

    bridge->effects.synaptic_renormalization_factor = factor;
    bridge->effects.downscaling_active = (factor > 0.5f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

float astro_sleep_get_renormalization_factor(const astro_sleep_bridge_t bridge)
{
    /* Guard clause: Return 0 if NULL */
    if (!bridge) return 0.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.synaptic_renormalization_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

bool astro_sleep_is_downscaling_active(const astro_sleep_bridge_t bridge)
{
    /* Guard clause: Return false if NULL */
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.downscaling_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ========================================================================
 * CALCIUM AND COUPLING FUNCTIONS
 * ======================================================================== */

float astro_sleep_get_calcium_factor(const astro_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return ASTRO_SLEEP_CALCIUM_AWAKE;

    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.calcium_wave_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

float astro_sleep_get_coupling_factor(const astro_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return ASTRO_SLEEP_COUPLING_AWAKE;

    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.gap_junction_coupling_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ========================================================================
 * STATE QUERY FUNCTIONS
 * ======================================================================== */

int astro_sleep_get_effects(
    const astro_sleep_bridge_t bridge,
    astro_sleep_effects_t* effects)
{
    /* Guard clauses: Validate inputs */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in get_effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_LOGGING_ERROR("NULL effects pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_sleep_get_effects: effects is NULL");
        return -1;
    }

    /* WHAT: Thread-safe copy of effects structure
     * WHY:  Allow caller to use effects without holding lock
     * HOW:  Lock, copy, unlock
     */
    nimcp_mutex_lock(bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int astro_sleep_update(astro_sleep_bridge_t bridge, float dt_ms)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_sleep_update: bridge is NULL");
        return -1;
    }

    /* WHAT: Update bridge state for new timestep
     * WHY:  Evolve adenosine, clearance, and modulations
     * HOW:  Called each simulation step
     */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Query current sleep state */
    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    /* Update effects if state changed */
    if (state != bridge->effects.current_state) {
        compute_effects(bridge, state, pressure);
    }

    /* Clear adenosine based on current sleep state */
    if (state != SLEEP_STATE_AWAKE) {
        /* Apply exponential decay */
        float decay_rate = bridge->config.adenosine_decay_rate;
        float dt_minutes = dt_ms / 60000.0f;

        /* Modulate decay by sleep state */
        switch (state) {
            case SLEEP_STATE_DEEP_NREM:
                decay_rate *= 3.0f;
                break;
            case SLEEP_STATE_LIGHT_NREM:
                decay_rate *= 2.0f;
                break;
            case SLEEP_STATE_REM:
                decay_rate *= 1.5f;
                break;
            default:
                break;
        }

        float decay_factor = expf(-decay_rate * dt_minutes);
        bridge->effects.adenosine_level *= decay_factor;

        if (bridge->effects.adenosine_level < 0.05f) {
            bridge->effects.adenosine_level = 0.05f;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ========================================================================
 * BIO-ASYNC INTEGRATION
 * ======================================================================== */

int astro_sleep_connect_bio_async(astro_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in connect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_sleep_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard clause: Already connected */
    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("Bio-async already connected for astrocyte-sleep bridge");
        return 0;
    }

    /* WHAT: Register with bio-async router
     * WHY:  Enable inter-module messaging for astrocyte signals
     * HOW:  Create module info and register
     */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_ASTROCYTE_SLEEP,
        .module_name = "astrocyte_sleep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Astrocyte-sleep bridge connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "astro_sleep_connect_bio_async: validation failed");
        return -1;
    }
}

int astro_sleep_disconnect_bio_async(astro_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in disconnect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "astro_sleep_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard clause: Not connected */
    if (!bridge->base.bio_async_enabled) {
        return 0;
    }

    /* WHAT: Unregister from bio-async router
     * WHY:  Clean shutdown of messaging
     * HOW:  Unregister and clear context
     */
    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Astrocyte-sleep bridge disconnected from bio-async router");

    return 0;
}

bool astro_sleep_is_bio_async_connected(const astro_sleep_bridge_t bridge)
{
    /* Guard clause: Return false if NULL */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
