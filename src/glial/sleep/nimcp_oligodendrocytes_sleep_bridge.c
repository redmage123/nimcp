/**
 * @file nimcp_oligodendrocytes_sleep_bridge.c
 * @brief Sleep-Oligodendrocyte Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and oligodendrocytes
 * WHY:  Oligodendrocytes maintain myelin during sleep, repair damage, and modulate
 *       conduction velocity based on activity patterns
 * HOW:  Sleep state modulates OPC differentiation, myelin synthesis, and repair
 */

#include "glial/sleep/nimcp_oligodendrocytes_sleep_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(oligodendrocytes_sleep_bridge)

/**
 * WHAT: Internal bridge structure
 * WHY:  Encapsulate implementation details
 * HOW:  Store config, effects, sleep system, and synchronization
 */
struct oligo_sleep_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    oligo_sleep_config_t config;           /**< Configuration parameters */
    sleep_system_t sleep_system;           /**< Sleep system handle */
    oligo_sleep_effects_t effects;         /**< Current modulation effects */
    bool callback_registered;              /**< Track callback for cleanup */

    /* Activity tracking for activity-dependent myelination */
    float accumulated_activity;            /**< Accumulated axon activity */
    float last_activity_update;            /**< Last activity timestamp */

};

/* Forward declarations */
static void oligo_on_sleep_state_change(sleep_state_t new_state, void* user_data);
static void compute_effects(oligo_sleep_bridge_t bridge, sleep_state_t state, float pressure);
static float get_synthesis_factor_for_state(sleep_state_t state);
static float get_opc_factor_for_state(sleep_state_t state);
static float get_repair_factor_for_state(sleep_state_t state);
static float get_metabolic_factor_for_state(sleep_state_t state);

/* ========================================================================
 * INTERNAL CALLBACK
 * ======================================================================== */

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update oligodendrocyte parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 *
 * BIOLOGICAL BASIS:
 * - Sleep state transitions trigger changes in oligodendrocyte activity
 * - Entry to deep NREM activates peak myelin synthesis
 * - OPC differentiation rate increases during sleep
 * - Myelin repair is accelerated during deep sleep phases
 */
static void oligo_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    oligo_sleep_bridge_t bridge = (oligo_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Oligodendrocyte bridge received sleep state: %d", new_state);

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get current sleep pressure */
    float pressure = sleep_get_pressure(bridge->sleep_system);

    /* Recompute all effects for new state */
    compute_effects(bridge, new_state, pressure);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Oligodendrocyte modulated: synthesis=%.2f, opc=%.2f, repair=%.2f",
                        bridge->effects.myelin_synthesis_factor,
                        bridge->effects.opc_differentiation_factor,
                        bridge->effects.myelin_repair_factor);
}

/**
 * WHAT: Compute all modulation effects for given state and pressure
 * WHY:  Centralize effect computation logic
 * HOW:  Apply biological rules based on sleep state
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Active myelin maintenance, minimal new synthesis
 * - DROWSY: Reduced activity, preparing for repair phase
 * - LIGHT_NREM: Beginning myelin synthesis, OPC activation
 * - DEEP_NREM: Peak myelin repair and synthesis, OPC differentiation
 * - REM: Moderate activity, selective maintenance
 */
static void compute_effects(oligo_sleep_bridge_t bridge,
                           sleep_state_t state,
                           float pressure)
{
    /* Update current state */
    bridge->effects.current_state = state;
    bridge->effects.sleep_pressure = pressure;

    /* Myelin synthesis modulation */
    if (bridge->config.enable_synthesis_modulation) {
        float base_synthesis = get_synthesis_factor_for_state(state);
        bridge->effects.myelin_synthesis_factor =
            base_synthesis * bridge->config.modulation_strength *
            bridge->config.synthesis_boost_multiplier;

        /* Synthesis is active during deep NREM */
        bridge->effects.synthesis_active = (state == SLEEP_STATE_DEEP_NREM &&
                                            base_synthesis > 0.8f);
    } else {
        bridge->effects.myelin_synthesis_factor = 1.0f;
        bridge->effects.synthesis_active = false;
    }

    /* OPC differentiation modulation */
    if (bridge->config.enable_opc_modulation) {
        float base_opc = get_opc_factor_for_state(state);
        bridge->effects.opc_differentiation_factor =
            base_opc * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength) * OLIGO_SLEEP_OPC_AWAKE;
    } else {
        bridge->effects.opc_differentiation_factor = OLIGO_SLEEP_OPC_AWAKE;
    }

    /* Myelin repair modulation */
    if (bridge->config.enable_repair_modulation) {
        float base_repair = get_repair_factor_for_state(state);
        bridge->effects.myelin_repair_factor =
            base_repair * bridge->config.modulation_strength *
            bridge->config.repair_boost_multiplier;

        /* Repair is active during NREM sleep */
        bridge->effects.repair_active = (state == SLEEP_STATE_DEEP_NREM ||
                                         state == SLEEP_STATE_LIGHT_NREM) &&
                                        base_repair > 0.5f;
    } else {
        bridge->effects.myelin_repair_factor = 1.0f;
        bridge->effects.repair_active = false;
    }

    /* Metabolic support modulation */
    if (bridge->config.enable_metabolic_modulation) {
        float base_metabolic = get_metabolic_factor_for_state(state);
        bridge->effects.metabolic_support_factor =
            base_metabolic * bridge->config.modulation_strength +
            (1.0f - bridge->config.modulation_strength) * OLIGO_SLEEP_METABOLIC_AWAKE;
    } else {
        bridge->effects.metabolic_support_factor = OLIGO_SLEEP_METABOLIC_AWAKE;
    }

    /* Activity-dependent myelination factor */
    if (bridge->config.enable_activity_myelination) {
        /* During sleep, accumulated activity from waking hours guides myelination */
        if (state == SLEEP_STATE_DEEP_NREM || state == SLEEP_STATE_LIGHT_NREM) {
            /* Higher activity during wake → more myelination during sleep */
            float activity_factor = fminf(bridge->accumulated_activity / 100.0f, 2.0f);
            bridge->effects.activity_myelination_factor = 1.0f + activity_factor;
        } else {
            bridge->effects.activity_myelination_factor = 1.0f;
        }
    } else {
        bridge->effects.activity_myelination_factor = 1.0f;
    }
}

/* ========================================================================
 * STATE FACTOR LOOKUP FUNCTIONS
 * ======================================================================== */

static float get_synthesis_factor_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return OLIGO_SLEEP_SYNTHESIS_AWAKE;
        case SLEEP_STATE_DROWSY:     return OLIGO_SLEEP_SYNTHESIS_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return OLIGO_SLEEP_SYNTHESIS_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return OLIGO_SLEEP_SYNTHESIS_DEEP_NREM;
        case SLEEP_STATE_REM:        return OLIGO_SLEEP_SYNTHESIS_REM;
        default:                     return OLIGO_SLEEP_SYNTHESIS_AWAKE;
    }
}

static float get_opc_factor_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return OLIGO_SLEEP_OPC_AWAKE;
        case SLEEP_STATE_DROWSY:     return OLIGO_SLEEP_OPC_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return OLIGO_SLEEP_OPC_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return OLIGO_SLEEP_OPC_DEEP_NREM;
        case SLEEP_STATE_REM:        return OLIGO_SLEEP_OPC_REM;
        default:                     return OLIGO_SLEEP_OPC_AWAKE;
    }
}

static float get_repair_factor_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return OLIGO_SLEEP_REPAIR_AWAKE;
        case SLEEP_STATE_DROWSY:     return OLIGO_SLEEP_REPAIR_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return OLIGO_SLEEP_REPAIR_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return OLIGO_SLEEP_REPAIR_DEEP_NREM;
        case SLEEP_STATE_REM:        return OLIGO_SLEEP_REPAIR_REM;
        default:                     return OLIGO_SLEEP_REPAIR_AWAKE;
    }
}

static float get_metabolic_factor_for_state(sleep_state_t state)
{
    switch (state) {
        case SLEEP_STATE_AWAKE:      return OLIGO_SLEEP_METABOLIC_AWAKE;
        case SLEEP_STATE_DROWSY:     return OLIGO_SLEEP_METABOLIC_DROWSY;
        case SLEEP_STATE_LIGHT_NREM: return OLIGO_SLEEP_METABOLIC_LIGHT_NREM;
        case SLEEP_STATE_DEEP_NREM:  return OLIGO_SLEEP_METABOLIC_DEEP_NREM;
        case SLEEP_STATE_REM:        return OLIGO_SLEEP_METABOLIC_REM;
        default:                     return OLIGO_SLEEP_METABOLIC_AWAKE;
    }
}

/* ========================================================================
 * LIFECYCLE FUNCTIONS
 * ======================================================================== */

int oligo_sleep_default_config(oligo_sleep_config_t* config)
{
    /* Guard clause: Validate config pointer */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligo_sleep_default_config: config is NULL");
        return -1;
    }

    /* WHAT: Set default configuration values
     * WHY:  Enable all modulation features for biological realism
     * HOW:  Initialize struct with sensible defaults
     */
    config->enable_synthesis_modulation = true;
    config->enable_opc_modulation = true;
    config->enable_repair_modulation = true;
    config->enable_metabolic_modulation = true;
    config->enable_activity_myelination = true;
    config->modulation_strength = NIMCP_SENSITIVITY_DEFAULT;
    config->repair_boost_multiplier = 3.0f;    /* 3x during deep sleep */
    config->synthesis_boost_multiplier = 5.0f; /* 5x during deep sleep */

    return 0;
}

oligo_sleep_bridge_t oligo_sleep_create(
    const oligo_sleep_config_t* config,
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
    struct oligo_sleep_bridge_struct* bridge =
        (struct oligo_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct oligo_sleep_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate oligodendrocyte-sleep bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    memset(bridge, 0, sizeof(struct oligo_sleep_bridge_struct));

    /* WHAT: Initialize configuration
     * WHY:  Use provided config or defaults
     * HOW:  Copy or call default_config
     */
    if (config) {
        bridge->config = *config;
    } else {
        oligo_sleep_default_config(&bridge->config);
    }

    bridge->sleep_system = sleep;

    /* WHAT: Initialize effects to awake defaults
     * WHY:  Start with sensible baseline values
     * HOW:  Set factors to awake-state values
     */
    bridge->effects.myelin_synthesis_factor = OLIGO_SLEEP_SYNTHESIS_AWAKE;
    bridge->effects.opc_differentiation_factor = OLIGO_SLEEP_OPC_AWAKE;
    bridge->effects.myelin_repair_factor = OLIGO_SLEEP_REPAIR_AWAKE;
    bridge->effects.metabolic_support_factor = OLIGO_SLEEP_METABOLIC_AWAKE;
    bridge->effects.activity_myelination_factor = 1.0f;
    bridge->effects.current_state = SLEEP_STATE_AWAKE;
    bridge->effects.sleep_pressure = 0.0f;
    bridge->effects.synthesis_active = false;
    bridge->effects.repair_active = false;

    /* Initialize activity tracking */
    bridge->accumulated_activity = 0.0f;
    bridge->last_activity_update = 0.0f;

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
    if (bridge_base_init(&bridge->base, 0, "oligodendrocytes_sleep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for oligodendrocyte-sleep bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "oligo_sleep_create: bridge->base is NULL");
        return NULL;
    }

    /* WHAT: Register callback for automatic state updates
     * WHY:  Get immediate notifications on sleep state changes
     * HOW:  Observer pattern via sleep system callback API
     */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        oligo_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_DEBUG("Registered sleep state callback for oligodendrocyte bridge");
    }

    /* WHAT: Get initial state immediately
     * WHY:  Ensure effects match current sleep state
     * HOW:  Query current state and trigger callback manually
     */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    oligo_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Oligodendrocyte-sleep bridge created");
    return bridge;
}

void oligo_sleep_destroy(oligo_sleep_bridge_t bridge)
{
    /* Guard clause: NULL safe */
    if (!bridge) return;

    /* WHAT: Disconnect bio-async if connected
     * WHY:  Clean shutdown of messaging
     * HOW:  Call disconnect function
     */
    if (bridge->base.bio_async_enabled) {
        oligo_sleep_disconnect_bio_async(bridge);
    }

    /* WHAT: Unregister callback if it was registered
     * WHY:  Clean shutdown, prevent dangling callback
     * HOW:  Call sleep system unregister API
     */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            oligo_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback for oligodendrocyte bridge");
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

    NIMCP_LOGGING_INFO("Oligodendrocyte-sleep bridge destroyed");
}

/* ========================================================================
 * MYELIN SYNTHESIS FUNCTIONS
 * ======================================================================== */

float oligo_sleep_enable_synthesis(oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) return 1.0f;

    /* WHAT: Enable myelin synthesis mode
     * WHY:  Activate synthesis during sleep
     * HOW:  Set synthesis factor based on sleep depth
     *
     * BIOLOGICAL BASIS:
     * - Myelin synthesis is energy-intensive
     * - Sleep provides optimal metabolic conditions
     * - Deep NREM enables peak synthesis rates
     * - New myelin wraps around active axons
     */
    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = bridge->effects.current_state;
    float synthesis = get_synthesis_factor_for_state(state) *
                      bridge->config.synthesis_boost_multiplier;

    bridge->effects.myelin_synthesis_factor = synthesis;
    bridge->effects.synthesis_active = (synthesis > 2.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return synthesis;
}

float oligo_sleep_get_synthesis_rate(const oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.myelin_synthesis_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

bool oligo_sleep_is_synthesis_active(const oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Return false if NULL */
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.synthesis_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ========================================================================
 * OPC DIFFERENTIATION FUNCTIONS
 * ======================================================================== */

float oligo_sleep_get_opc_factor(const oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return OLIGO_SLEEP_OPC_AWAKE;

    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.opc_differentiation_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

float oligo_sleep_initiate_opc_differentiation(oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) return OLIGO_SLEEP_OPC_AWAKE;

    /* WHAT: Initiate OPC differentiation
     * WHY:  Activate progenitor maturation during sleep
     * HOW:  Set differentiation factor based on sleep depth
     *
     * BIOLOGICAL BASIS:
     * - OPCs (Oligodendrocyte Progenitor Cells) are stem-like precursors
     * - Sleep promotes OPC differentiation into mature oligodendrocytes
     * - Deep NREM provides optimal conditions for maturation
     * - Newly differentiated oligodendrocytes myelinate active axons
     */
    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = bridge->effects.current_state;
    float factor = get_opc_factor_for_state(state) *
                   bridge->config.modulation_strength;

    bridge->effects.opc_differentiation_factor = factor;

    nimcp_mutex_unlock(bridge->base.mutex);

    return factor;
}

/* ========================================================================
 * MYELIN REPAIR FUNCTIONS
 * ======================================================================== */

float oligo_sleep_enable_repair(oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) return 1.0f;

    /* WHAT: Enable myelin repair mode
     * WHY:  Activate damage repair during sleep
     * HOW:  Set repair factor based on sleep depth
     *
     * BIOLOGICAL BASIS:
     * - Myelin is damaged by oxidative stress during waking activity
     * - Sleep enables repair of damaged myelin sheaths
     * - Deep NREM provides optimal conditions for repair
     * - Repair involves both patching and complete remyelination
     */
    nimcp_mutex_lock(bridge->base.mutex);

    sleep_state_t state = bridge->effects.current_state;
    float repair = get_repair_factor_for_state(state) *
                   bridge->config.repair_boost_multiplier;

    bridge->effects.myelin_repair_factor = repair;
    bridge->effects.repair_active = (repair > 1.5f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return repair;
}

float oligo_sleep_get_repair_rate(const oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return 1.0f;

    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.myelin_repair_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

bool oligo_sleep_is_repair_active(const oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Return false if NULL */
    if (!bridge) {
        return false;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool result = bridge->effects.repair_active;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ========================================================================
 * STATE QUERY FUNCTIONS
 * ======================================================================== */

int oligo_sleep_get_effects(
    const oligo_sleep_bridge_t bridge,
    oligo_sleep_effects_t* effects)
{
    /* Guard clauses: Validate inputs */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in get_effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligo_sleep_get_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_LOGGING_ERROR("NULL effects pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligo_sleep_get_effects: effects is NULL");
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

int oligo_sleep_update(oligo_sleep_bridge_t bridge, float dt_ms)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in update");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligo_sleep_update: bridge is NULL");
        return -1;
    }

    /* WHAT: Update bridge state for new timestep
     * WHY:  Evolve synthesis, repair, and modulations
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

    /* Decay accumulated activity during sleep (activity-dependent myelination) */
    if (state != SLEEP_STATE_AWAKE && bridge->config.enable_activity_myelination) {
        float decay_rate = 0.01f;  /* 1% per update during sleep */
        bridge->accumulated_activity *= (1.0f - decay_rate);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float oligo_sleep_get_metabolic_factor(const oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Return baseline if NULL */
    if (!bridge) return OLIGO_SLEEP_METABOLIC_AWAKE;

    nimcp_mutex_lock(bridge->base.mutex);
    float result = bridge->effects.metabolic_support_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return result;
}

/* ========================================================================
 * BIO-ASYNC INTEGRATION
 * ======================================================================== */

int oligo_sleep_connect_bio_async(oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in connect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligo_sleep_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard clause: Already connected */
    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("Bio-async already connected for oligodendrocyte-sleep bridge");
        return 0;
    }

    /* WHAT: Register with bio-async router
     * WHY:  Enable inter-module messaging for oligodendrocyte signals
     * HOW:  Create module info and register
     */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_OLIGO_SLEEP,
        .module_name = "oligodendrocyte_sleep_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Oligodendrocyte-sleep bridge connected to bio-async router");
        return 0;
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "oligo_sleep_connect_bio_async: validation failed");
        return -1;
    }
}

int oligo_sleep_disconnect_bio_async(oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in disconnect_bio_async");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "oligo_sleep_disconnect_bio_async: bridge is NULL");
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
    NIMCP_LOGGING_INFO("Oligodendrocyte-sleep bridge disconnected from bio-async router");

    return 0;
}

bool oligo_sleep_is_bio_async_connected(const oligo_sleep_bridge_t bridge)
{
    /* Guard clause: Return false if NULL */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
