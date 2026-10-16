/**
 * @file nimcp_microglia_sleep_bridge.c
 * @brief Sleep-Microglia Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 */

#include "glial/sleep/nimcp_microglia_sleep_bridge.h"
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
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(microglia_sleep_bridge)

/**
 * WHAT: Internal bridge structure
 * WHY:  Encapsulate implementation details
 * HOW:  Store config, effects, sleep system, and synchronization
 */
struct microglia_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    microglia_sleep_config_t config;        /**< Configuration parameters */
    sleep_system_t sleep_system;            /**< Sleep system handle */
    microglia_sleep_effects_t effects;      /**< Current modulation effects */
    bool callback_registered;               /**< Track callback for cleanup */

};

/* Forward declarations */
static void microglia_on_sleep_state_change(sleep_state_t new_state, void* user_data);
static void compute_effects(microglia_sleep_bridge_t bridge, sleep_state_t state, float pressure);

/* ========================================================================
 * INTERNAL CALLBACK
 * ======================================================================== */

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update microglia parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state transitions trigger rapid microglia behavioral changes
 * - Entry to deep NREM activates glymphatic clearance within minutes
 * - Microglia extend processes as brain transitions to sleep
 * - Phagocytosis rate increases dramatically during deep sleep
 */
static void microglia_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    microglia_sleep_bridge_t bridge = (microglia_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Microglia bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current sleep pressure */
    float pressure = sleep_get_pressure(bridge->sleep_system);

    /* Recompute all effects for new state */
    compute_effects(bridge, new_state, pressure);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Microglia modulated: phago=%.2f, surveillance=%.2f, glymphatic=%s",
                        bridge->effects.phagocytosis_rate_factor,
                        bridge->effects.surveillance_activity_factor,
                        bridge->effects.glymphatic_active ? "ACTIVE" : "inactive");
}

/**
 * WHAT: Compute all modulation effects for given state and pressure
 * WHY:  Centralize effect computation logic
 * HOW:  Apply biological rules based on sleep state
 *
 * BIOLOGICAL BASIS:
 * - Deep NREM: Peak pruning, full surveillance, glymphatic clearance
 * - Light NREM: Moderate pruning, full surveillance
 * - REM: Selective pruning, maintained surveillance
 * - Drowsy: Ramping up all activities
 * - Awake: Minimal pruning, reduced surveillance
 *
 * SLEEP PRESSURE EFFECTS:
 * - High pressure (>0.7) pre-activates microglia even when awake
 * - Reduces pruning threshold (more aggressive)
 * - Models homeostatic drive for synaptic downscaling
 */
static void compute_effects(microglia_sleep_bridge_t bridge,
                           sleep_state_t state,
                           float pressure)
{
    /* Update current state */
    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    /* Phagocytosis rate modulation */
    if (bridge->config.enable_phagocytosis_modulation) {
        float base_phago = microglia_sleep_phagocytosis_for_state(state);
        bridge->effects.phagocytosis_rate_factor =
            base_phago * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength) * MICROGLIA_SLEEP_PHAGO_AWAKE;

        /* Sleep pressure increases pruning drive even when awake */
        if (state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
            float pressure_boost = (pressure - 0.7f) / 0.3f;  /* 0-1 range */
            bridge->effects.phagocytosis_rate_factor += pressure_boost * 0.2f;
        }
    } else {
        bridge->effects.phagocytosis_rate_factor = MICROGLIA_SLEEP_PHAGO_AWAKE;
    }

    /* Surveillance activity modulation */
    if (bridge->config.enable_surveillance_modulation) {
        float base_surveillance = microglia_sleep_surveillance_for_state(state);
        bridge->effects.surveillance_activity_factor =
            base_surveillance * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength) * MICROGLIA_SLEEP_SURVEILLANCE_AWAKE;
    } else {
        bridge->effects.surveillance_activity_factor = MICROGLIA_SLEEP_SURVEILLANCE_AWAKE;
    }

    /* Process extension modulation */
    if (bridge->config.enable_process_modulation) {
        float base_process = microglia_sleep_process_extension_for_state(state);
        bridge->effects.process_extension_factor =
            base_process * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength) * MICROGLIA_SLEEP_PROCESS_AWAKE;
    } else {
        bridge->effects.process_extension_factor = MICROGLIA_SLEEP_PROCESS_AWAKE;
    }

    /* Glymphatic clearance (only during deep NREM) */
    if (bridge->config.enable_glymphatic_clearance &&
        state == SLEEP_STATE_DEEP_NREM) {
        bridge->effects.glymphatic_active = true;
        bridge->effects.glymphatic_clearance_factor =
            bridge->config.glymphatic_clearance_multiplier;
    } else {
        bridge->effects.glymphatic_active = false;
        bridge->effects.glymphatic_clearance_factor = 1.0f;
    }

    /* Pruning threshold adjustment (lower = more aggressive) */
    if (state == SLEEP_STATE_DEEP_NREM) {
        bridge->effects.pruning_threshold_adjustment = -0.05f;  /* 5% lower threshold */
    } else if (state == SLEEP_STATE_LIGHT_NREM) {
        bridge->effects.pruning_threshold_adjustment = -0.02f;  /* 2% lower threshold */
    } else if (pressure > 0.8f) {
        /* Very high sleep pressure lowers threshold even when awake */
        bridge->effects.pruning_threshold_adjustment = -0.03f;
    } else {
        bridge->effects.pruning_threshold_adjustment = 0.0f;
    }

    /* Enhanced mode flag (deep NREM peak activity) */
    bridge->effects.microglia_enhanced = (state == SLEEP_STATE_DEEP_NREM);
}

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

int microglia_sleep_default_config(microglia_sleep_config_t* config)
{
    /* Guard clause: Validate config pointer */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_sleep_default_config: config is NULL");
        return -1;
    }

    /* WHAT: Set default configuration values
     * WHY:  Enable all modulation features for biological realism
     * HOW:  Initialize struct with sensible defaults
     */
    config->enable_phagocytosis_modulation = true;
    config->enable_surveillance_modulation = true;
    config->enable_process_modulation = true;
    config->enable_glymphatic_clearance = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    config->glymphatic_clearance_multiplier = 15.0f;  /* 15x during deep sleep */

    return 0;
}

microglia_sleep_bridge_t microglia_sleep_bridge_create(
    const microglia_sleep_config_t* config,
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
    struct microglia_sleep_bridge_struct* bridge =
        (struct microglia_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct microglia_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate microglia-sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(struct microglia_sleep_bridge_struct));

    /* WHAT: Initialize configuration
     * WHY:  Use provided config or defaults
     * HOW:  Copy or call default_config
     */
    if (config) {
        bridge->config = *config;
    } else {
        microglia_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep;

    /* WHAT: Initialize effects to awake defaults
     * WHY:  Start with sensible baseline values
     * HOW:  Set factors to awake-state values
     */
    bridge->effects.phagocytosis_rate_factor = MICROGLIA_SLEEP_PHAGO_AWAKE;
    bridge->effects.surveillance_activity_factor = MICROGLIA_SLEEP_SURVEILLANCE_AWAKE;
    bridge->effects.process_extension_factor = MICROGLIA_SLEEP_PROCESS_AWAKE;
    bridge->effects.glymphatic_clearance_factor = 1.0f;
    bridge->effects.pruning_threshold_adjustment = 0.0f;
    bridge->effects.microglia_enhanced = false;
    bridge->effects.glymphatic_active = false;

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
    if (bridge_base_init(&bridge->base, 0, "microglia_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for microglia-sleep bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "microglia_sleep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* WHAT: Register callback for automatic state updates
     * WHY:  Get immediate notifications on sleep state changes
     * HOW:  Observer pattern via sleep system callback API
     */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        microglia_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for microglia bridge");
    }

    /* WHAT: Get initial state immediately
     * WHY:  Ensure effects match current sleep state
     * HOW:  Query current state and trigger callback manually
     */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    microglia_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Microglia-sleep bridge created");
    return bridge;
}

void microglia_sleep_bridge_destroy(microglia_sleep_bridge_t bridge)
{
    /* Guard clause: NULL safe */
    if (!bridge) return;

    /* WHAT: Disconnect bio-async if connected
     * WHY:  Clean shutdown of messaging
     * HOW:  Call disconnect function
     */
    if (bridge->base.bio_async_enabled) {
        microglia_sleep_disconnect_bio_async(bridge);
    }

    /* WHAT: Unregister callback if it was registered
     * WHY:  Clean shutdown, prevent dangling callback
     * HOW:  Call sleep system unregister API
     */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            microglia_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for microglia bridge");
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

    NIMCP_LOGGING_INFO("Microglia-sleep bridge destroyed");
}

/* ========================================================================
 * UPDATE FUNCTIONS
 * ======================================================================== */

int microglia_sleep_update(microglia_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_sleep_update: bridge is NULL");
        return -1;
    }

    /* WHAT: Manual polling update (fallback if callback not registered)
     * WHY:  Provide alternative update path
     * HOW:  Query sleep system and recompute effects
     */
    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = sleep_get_current_state(bridge->sleep_system);
    float pressure = sleep_get_pressure(bridge->sleep_system);

    /* Recompute all effects */
    compute_effects(bridge, state, pressure);

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int microglia_sleep_get_effects(const microglia_sleep_bridge_t bridge,
                                 microglia_sleep_effects_t* effects)
{
    /* Guard clauses: Validate inputs */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in get_effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_LOGGING_ERROR("NULL effects pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_sleep_get_effects: effects is NULL");
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

/* ========================================================================
 * QUERY FUNCTIONS
 * ======================================================================== */

float microglia_sleep_get_phagocytosis_rate(const microglia_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return MICROGLIA_SLEEP_PHAGO_AWAKE;

    /* WHAT: Thread-safe read of phagocytosis factor
     * WHY:  Quick access to pruning modulation
     * HOW:  Lock, read, unlock
     */
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.phagocytosis_rate_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

float microglia_sleep_get_surveillance_activity(const microglia_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return MICROGLIA_SLEEP_SURVEILLANCE_AWAKE;

    /* WHAT: Thread-safe read of surveillance factor
     * WHY:  Quick access to surveillance modulation
     * HOW:  Lock, read, unlock
     */
    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.surveillance_activity_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

bool microglia_sleep_is_glymphatic_active(const microglia_sleep_bridge_t bridge)
{
    /* Guard clause: Return false if NULL */
    if (!bridge) {
        return false;
    }

    /* WHAT: Thread-safe read of glymphatic status
     * WHY:  Determine if waste clearance is active
     * HOW:  Lock, read, unlock
     */
    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.glymphatic_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

bool microglia_sleep_is_enhanced(const microglia_sleep_bridge_t bridge)
{
    /* Guard clause: Return false if NULL */
    if (!bridge) {
        return false;
    }

    /* WHAT: Thread-safe read of enhanced mode status
     * WHY:  Determine if microglia are in peak activity mode
     * HOW:  Lock, read, unlock
     */
    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.microglia_enhanced;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

float microglia_sleep_phagocytosis_for_state(sleep_state_t state)
{
    /* WHAT: Static lookup of phagocytosis factor by sleep state
     * WHY:  Provide state-specific baseline values
     * HOW:  Switch on state enum
     *
     * BIOLOGICAL BASIS:
     * - DEEP_NREM: 1.0 (peak pruning, synaptic downscaling)
     * - LIGHT_NREM: 0.6 (active assessment and tagging)
     * - REM: 0.5 (selective pruning, preserve dream circuits)
     * - DROWSY: 0.3 (ramping up activity)
     * - AWAKE: 0.1 (minimal pruning, synapses active)
     */
    switch (state) {
        case SLEEP_STATE_AWAKE:      return MICROGLIA_SLEEP_PHAGO_AWAKE;
        case SLEEP_STATE_DROWSY:     return MICROGLIA_SLEEP_PHAGO_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return MICROGLIA_SLEEP_PHAGO_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return MICROGLIA_SLEEP_PHAGO_DEEP_NREM;
        case SLEEP_STATE_REM:        return MICROGLIA_SLEEP_PHAGO_REM;
        default:                     return MICROGLIA_SLEEP_PHAGO_AWAKE;
    }
}

float microglia_sleep_surveillance_for_state(sleep_state_t state)
{
    /* WHAT: Static lookup of surveillance factor by sleep state
     * WHY:  Provide state-specific baseline values
     * HOW:  Switch on state enum
     *
     * BIOLOGICAL BASIS:
     * - DEEP_NREM: 1.2 (enhanced surveillance, expanded coverage)
     * - LIGHT_NREM: 1.0 (full surveillance, process extension)
     * - REM: 0.9 (maintained but slightly reduced)
     * - DROWSY: 0.8 (ramping up)
     * - AWAKE: 0.5 (reduced during neural activity)
     */
    switch (state) {
        case SLEEP_STATE_AWAKE:      return MICROGLIA_SLEEP_SURVEILLANCE_AWAKE;
        case SLEEP_STATE_DROWSY:     return MICROGLIA_SLEEP_SURVEILLANCE_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return MICROGLIA_SLEEP_SURVEILLANCE_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return MICROGLIA_SLEEP_SURVEILLANCE_DEEP_NREM;
        case SLEEP_STATE_REM:        return MICROGLIA_SLEEP_SURVEILLANCE_REM;
        default:                     return MICROGLIA_SLEEP_SURVEILLANCE_AWAKE;
    }
}

float microglia_sleep_process_extension_for_state(sleep_state_t state)
{
    /* WHAT: Static lookup of process extension factor by sleep state
     * WHY:  Provide state-specific baseline values
     * HOW:  Switch on state enum
     *
     * BIOLOGICAL BASIS:
     * - DEEP_NREM: 1.1 (maximum process extension)
     * - LIGHT_NREM: 1.0 (fully extended processes)
     * - REM: 1.0 (maintained extension)
     * - DROWSY: 0.9 (extending)
     * - AWAKE: 0.7 (retracted during activity)
     */
    switch (state) {
        case SLEEP_STATE_AWAKE:      return MICROGLIA_SLEEP_PROCESS_AWAKE;
        case SLEEP_STATE_DROWSY:     return MICROGLIA_SLEEP_PROCESS_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return MICROGLIA_SLEEP_PROCESS_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return MICROGLIA_SLEEP_PROCESS_DEEP_NREM;
        case SLEEP_STATE_REM:        return MICROGLIA_SLEEP_PROCESS_REM;
        default:                     return MICROGLIA_SLEEP_PROCESS_AWAKE;
    }
}

/* ========================================================================
 * BIO-ASYNC INTEGRATION FUNCTIONS
 * ======================================================================== */

int microglia_sleep_connect_bio_async(microglia_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in connect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_sleep_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard clause: Already connected */
    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("Bio-async already connected for microglia-sleep bridge");
        return 0;
    }

    /* WHAT: Register with bio-async router
     * WHY:  Enable inter-module messaging for microglia signals
     * HOW:  Create module info and register
     */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_MICROGLIA_SLEEP,
        .module_name = "microglia_sleep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Microglia-sleep bridge connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "microglia_sleep_connect_bio_async: validation failed");
        return -1;
    }
}

int microglia_sleep_disconnect_bio_async(microglia_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in disconnect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "microglia_sleep_disconnect_bio_async: bridge is NULL");
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
    NIMCP_LOGGING_INFO("Microglia-sleep bridge disconnected from bio-async router");

    return 0;
}

bool microglia_sleep_is_bio_async_connected(const microglia_sleep_bridge_t bridge)
{
    /* Guard clause: Return false if NULL */
    if (!bridge) {
        return false;
    }

    /* WHAT: Return bio-async connection status
     * WHY:  Allow conditional bio-async usage
     * HOW:  Return bio_async_enabled flag
     */
    return bridge->base.bio_async_enabled;
}
