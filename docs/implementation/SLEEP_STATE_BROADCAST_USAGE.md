# Sleep State Broadcast Implementation

## Overview
Added callback/observer mechanism to Sleep-Wake coordinator to broadcast state changes to all integrated modules.

## Changes Made

### Header File (`include/cognitive/nimcp_sleep_wake.h`)
1. Added `sleep_state_callback_t` typedef for callback function pointer
2. Added `sleep_register_state_callback()` function to register observers
3. Added `sleep_unregister_state_callback()` function to cleanup callbacks

### Implementation File (`src/cognitive/sleep_wake/nimcp_sleep_wake.c`)
1. Added `sleep_callback_entry_t` structure for linked list of callbacks
2. Added `callbacks` field to `sleep_system_struct`
3. Added `sleep_notify_state_change()` static helper function
4. Modified `sleep_enter_state()` to call `sleep_notify_state_change()`
5. Modified `sleep_wake_up()` to call `sleep_notify_state_change()`
6. Updated `sleep_system_destroy()` to free callback list
7. Implemented `sleep_register_state_callback()` and `sleep_unregister_state_callback()`

## Usage Pattern

### For Sleep Bridge Modules

Each sleep bridge (attention, working memory, executive, etc.) can register a callback to be notified immediately when sleep state changes:

```c
// Example: Attention sleep bridge

// Step 1: Define callback function
static void attention_sleep_state_changed(sleep_state_t new_state, void* user_data)
{
    attention_sleep_bridge_t bridge = (attention_sleep_bridge_t)user_data;

    // Update internal state immediately
    nimcp_mutex_lock(bridge->mutex);
    bridge->effects.current_state = new_state;

    // Update capacity based on new state
    bridge->effects.capacity_factor = attention_sleep_capacity_for_state(new_state);
    bridge->effects.vigilance_factor = attention_sleep_vigilance_for_state(new_state);

    // Mark attention as offline during deep sleep
    bridge->effects.attention_offline = (new_state == SLEEP_STATE_DEEP_NREM ||
                                         new_state == SLEEP_STATE_LIGHT_NREM);

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_INFO("Attention updated for sleep state: %d", new_state);
}

// Step 2: Register callback during bridge creation
attention_sleep_bridge_t attention_sleep_bridge_create(
    const attention_sleep_config_t* config,
    sleep_system_t sleep)
{
    // ... existing creation code ...

    // Register for state change notifications
    if (!sleep_register_state_callback(sleep, attention_sleep_state_changed, bridge)) {
        NIMCP_LOGGING_WARN("Failed to register sleep state callback");
    }

    return bridge;
}

// Step 3: Unregister callback during destruction
void attention_sleep_bridge_destroy(attention_sleep_bridge_t bridge)
{
    if (!bridge) return;

    // Unregister callback
    if (bridge->sleep_system) {
        sleep_unregister_state_callback(bridge->sleep_system,
                                        attention_sleep_state_changed,
                                        bridge);
    }

    // ... existing cleanup code ...
}
```

## Benefits

1. **Push vs Pull**: Modules are notified immediately when state changes instead of polling
2. **Efficiency**: No need to call `attention_sleep_update()` manually - updates happen automatically
3. **Consistency**: All modules receive state changes at the same time
4. **Optional**: Modules that don't support sleep state callbacks don't break (NULL checks)
5. **Thread-Safe**: Callbacks are copied to stack before invocation to prevent deadlock

## Thread Safety

The implementation is thread-safe with careful lock management:
- Callbacks are copied to a temporary array while holding the lock
- Callbacks are invoked WITHOUT holding the lock to prevent deadlock
- Maximum of 32 callbacks supported (can be increased if needed)

## Testing Recommendations

1. Test callback registration and unregistration
2. Test multiple modules registering callbacks
3. Test state transitions trigger all callbacks
4. Test callback unregistration doesn't affect other callbacks
5. Test NULL safety (unregistering non-existent callback)
6. Test thread safety (concurrent state changes)

## Example Integration for All Sleep Bridges

All 8+ sleep bridge modules can now use this pattern:
- `nimcp_attention_sleep_bridge.h/c`
- `nimcp_working_memory_sleep_bridge.h/c`
- `nimcp_executive_sleep_bridge.h/c`
- `nimcp_wellbeing_sleep_bridge.h/c`
- `nimcp_stdp_sleep_bridge.h/c`
- `nimcp_bcm_sleep_bridge.h/c`
- `nimcp_homeostatic_sleep_bridge.h/c`
- `nimcp_oscillations_sleep_bridge.h/c`
- `nimcp_neuromodulators_sleep_bridge.h/c`

Each bridge can register a callback during creation and unregister during destruction.

## API Reference

### Callback Type
```c
typedef void (*sleep_state_callback_t)(sleep_state_t new_state, void* user_data);
```

### Register Callback
```c
bool sleep_register_state_callback(sleep_system_t sleep,
                                    sleep_state_callback_t callback,
                                    void* user_data);
```
- Returns `true` on success, `false` on failure
- Thread-safe
- O(1) complexity

### Unregister Callback
```c
bool sleep_unregister_state_callback(sleep_system_t sleep,
                                      sleep_state_callback_t callback,
                                      void* user_data);
```
- Returns `true` if callback was found and removed
- Thread-safe
- O(n) complexity where n = number of callbacks
- Must match both callback function AND user_data

## State Change Notifications

Callbacks are invoked when:
1. `sleep_enter_state()` is called (explicit state transition)
2. `sleep_wake_up()` is called (wake transition)
3. During `sleep_run_cycle()` (automatic state progression through sleep stages)

All sleep state transitions:
- AWAKE → DROWSY
- DROWSY → LIGHT_NREM
- LIGHT_NREM → DEEP_NREM
- DEEP_NREM → REM
- REM → AWAKE (via wake_up)
