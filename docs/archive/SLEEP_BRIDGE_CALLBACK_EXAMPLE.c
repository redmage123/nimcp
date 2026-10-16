/**
 * @file SLEEP_BRIDGE_CALLBACK_EXAMPLE.c
 * @brief Example of how to update a sleep bridge to use state change callbacks
 *
 * This shows how the attention sleep bridge can be modified to use the new
 * callback mechanism instead of polling.
 */

#include "cognitive/attention/nimcp_attention_sleep_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/logging/nimcp_logging.h"

/* ========================================================================
 * EXAMPLE: Updated Attention Sleep Bridge Structure
 * ======================================================================== */

struct attention_sleep_bridge_struct {
    attention_sleep_config_t config;
    sleep_system_t sleep_system;
    attention_sleep_effects_t effects;
    nimcp_mutex_t* mutex;

    // NEW: Track if callback is registered for cleanup
    bool callback_registered;
};

/* ========================================================================
 * EXAMPLE: Callback Function
 * ======================================================================== */

/**
 * WHAT: Callback invoked when sleep state changes
 * WHY:  Immediately update attention parameters for new sleep state
 * HOW:  Called by sleep system via observer pattern
 */
static void attention_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    /* WHAT: Cast user_data back to bridge */
    attention_sleep_bridge_t bridge = (attention_sleep_bridge_t)user_data;

    /* Guard clause: Validate bridge */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge in sleep state callback");
        return;
    }

    NIMCP_LOGGING_DEBUG("Attention bridge received sleep state: %d", new_state);

    /* WHAT: Update effects based on new state */
    nimcp_mutex_lock(bridge->mutex);

    bridge->effects.current_state = new_state;

    /* Update capacity factor */
    if (bridge->config.enable_capacity_modulation) {
        float cap_base = attention_sleep_capacity_for_state(new_state);
        bridge->effects.capacity_factor = cap_base * bridge->config.modulation_strength +
                                          (1.0f - bridge->config.modulation_strength);
    }

    /* Update vigilance factor */
    if (bridge->config.enable_vigilance_modulation) {
        bridge->effects.vigilance_factor = attention_sleep_vigilance_for_state(new_state);
    }

    /* Update spotlight size */
    bridge->effects.spotlight_size_factor = (new_state == SLEEP_STATE_DROWSY) ? 0.7f :
                                            (new_state == SLEEP_STATE_REM) ? 1.3f : 1.0f;

    /* Update offline status */
    bridge->effects.attention_offline = (new_state == SLEEP_STATE_DEEP_NREM ||
                                         new_state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_INFO("Attention modulated: capacity=%.2f, vigilance=%.2f, offline=%d",
                       bridge->effects.capacity_factor,
                       bridge->effects.vigilance_factor,
                       bridge->effects.attention_offline);
}

/* ========================================================================
 * EXAMPLE: Updated Creation Function
 * ======================================================================== */

attention_sleep_bridge_t attention_sleep_bridge_create(
    const attention_sleep_config_t* config,
    sleep_system_t sleep)
{
    if (!sleep) return NULL;

    /* Allocate and initialize bridge */
    struct attention_sleep_bridge_struct* bridge =
        (struct attention_sleep_bridge_struct*)nimcp_malloc(
            sizeof(struct attention_sleep_bridge_struct));
    if (!bridge) return NULL;

    memset(bridge, 0, sizeof(struct attention_sleep_bridge_struct));

    /* Set configuration */
    if (config) bridge->config = *config;
    else attention_sleep_default_config(&bridge->config);

    bridge->sleep_system = sleep;
    bridge->effects.capacity_factor = 1.0f;
    bridge->effects.vigilance_factor = 1.0f;
    bridge->effects.spotlight_size_factor = 1.0f;
    bridge->effects.attention_offline = false;

    /* Create mutex */
    bridge->mutex = nimcp_platform_mutex_create();
    if (!bridge->mutex) {
        nimcp_free(bridge);
        return NULL;
    }

    /* NEW: Register callback for automatic state updates */
    bridge->callback_registered = sleep_register_state_callback(
        sleep,
        attention_on_sleep_state_change,
        bridge);

    if (!bridge->callback_registered) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback - will use polling");
    } else {
        NIMCP_LOGGING_INFO("Registered sleep state callback for attention bridge");
    }

    /* NEW: Get initial state immediately */
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    attention_on_sleep_state_change(initial_state, bridge);

    NIMCP_LOGGING_INFO("Attention-sleep bridge created");
    return bridge;
}

/* ========================================================================
 * EXAMPLE: Updated Destruction Function
 * ======================================================================== */

void attention_sleep_bridge_destroy(attention_sleep_bridge_t bridge)
{
    if (!bridge) return;

    /* NEW: Unregister callback if it was registered */
    if (bridge->callback_registered && bridge->sleep_system) {
        bool unregistered = sleep_unregister_state_callback(
            bridge->sleep_system,
            attention_on_sleep_state_change,
            bridge);

        if (unregistered) {
            NIMCP_LOGGING_DEBUG("Unregistered sleep state callback");
        } else {
            NIMCP_LOGGING_WARN("Failed to unregister sleep state callback");
        }
    }

    /* Cleanup */
    if (bridge->mutex) nimcp_mutex_destroy(bridge->mutex);
    nimcp_free(bridge);
}

/* ========================================================================
 * EXAMPLE: Update Function (Now Optional)
 * ======================================================================== */

/**
 * WHAT: Manual update function (now optional)
 * WHY:  Backwards compatibility, or for polling if callback fails
 * HOW:  Same as before, but callback makes this mostly unnecessary
 *
 * NOTE: With callbacks registered, this function is only needed for:
 * 1. Backwards compatibility
 * 2. Fallback if callback registration failed
 * 3. Manual pressure-based updates (pressure changes don't trigger callback)
 */
int attention_sleep_update(attention_sleep_bridge_t bridge)
{
    if (!bridge) return -1;

    /* If callback is registered, this is redundant for state changes */
    /* But we can still update based on sleep pressure */

    nimcp_mutex_lock(bridge->mutex);

    float pressure = sleep_get_pressure(bridge->sleep_system);
    bridge->effects.sleep_pressure = pressure;

    /* Apply pressure-based capacity reduction (only when awake) */
    if (bridge->effects.current_state == SLEEP_STATE_AWAKE && pressure > 0.7f) {
        bridge->effects.capacity_factor *= (1.0f - (pressure - 0.7f));
    }

    nimcp_mutex_unlock(bridge->mutex);
    return 0;
}

/* ========================================================================
 * EXAMPLE: Migration Path
 * ======================================================================== */

/**
 * MIGRATION NOTES:
 *
 * 1. BACKWARDS COMPATIBLE:
 *    - Existing code still works (callback is optional)
 *    - Can still call attention_sleep_update() manually
 *    - If callback fails, falls back to polling
 *
 * 2. RECOMMENDED USAGE:
 *    - Register callback during creation
 *    - Don't call attention_sleep_update() for state changes
 *    - Only call attention_sleep_update() if you need pressure updates
 *
 * 3. TRANSITION STEPS:
 *    a) Add callback_registered field to struct
 *    b) Add callback function (attention_on_sleep_state_change)
 *    c) Register callback in _create()
 *    d) Unregister callback in _destroy()
 *    e) Call callback manually with initial state
 *    f) Optional: Keep _update() for backwards compatibility
 *
 * 4. TESTING:
 *    - Test with callback registered (normal case)
 *    - Test with callback registration failure (fallback)
 *    - Test unregistration during destruction
 *    - Test state transitions trigger callback
 *    - Test NULL safety
 */
