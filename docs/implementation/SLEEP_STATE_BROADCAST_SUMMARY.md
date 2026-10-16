# Sleep-Wake State Broadcast Implementation Summary

## Overview
Implemented callback/observer mechanism in Sleep-Wake coordinator to broadcast state changes to all integrated modules (attention, working memory, executive, plasticity, etc.).

## Problem Solved
Previously, sleep bridge modules had to poll the sleep system via `sleep_get_current_state()` to detect state changes. This required:
- Manual calls to `*_sleep_update()` functions
- Potential delays in detecting state changes
- Inefficient polling pattern

## Solution
Added observer pattern with callbacks:
- Sleep system maintains a linked list of registered callbacks
- When state changes, all callbacks are invoked automatically
- Modules receive immediate notifications of state changes
- Thread-safe implementation with deadlock prevention

## Files Modified

### 1. `/home/bbrelin/nimcp/include/cognitive/nimcp_sleep_wake.h`
**Added:**
- `sleep_state_callback_t` - Callback function pointer typedef
- `sleep_register_state_callback()` - Register observer
- `sleep_unregister_state_callback()` - Unregister observer

```c
typedef void (*sleep_state_callback_t)(sleep_state_t new_state, void* user_data);

bool sleep_register_state_callback(sleep_system_t sleep,
                                    sleep_state_callback_t callback,
                                    void* user_data);

bool sleep_unregister_state_callback(sleep_system_t sleep,
                                      sleep_state_callback_t callback,
                                      void* user_data);
```

### 2. `/home/bbrelin/nimcp/src/cognitive/sleep_wake/nimcp_sleep_wake.c`
**Added:**
- `sleep_callback_entry_t` - Linked list node structure
- `callbacks` field in `sleep_system_struct`
- `sleep_notify_state_change()` - Helper to invoke all callbacks
- Callback registration/unregistration implementation

**Modified:**
- `sleep_system_create()` - Initialize callbacks to NULL
- `sleep_system_destroy()` - Free callback linked list
- `sleep_enter_state()` - Call `sleep_notify_state_change()`
- `sleep_wake_up()` - Call `sleep_notify_state_change()`

## Key Design Decisions

### 1. Linked List vs Array
**Chose:** Linked list
**Why:**
- Unknown number of modules at compile time
- O(1) insertion at head
- Dynamic growth without reallocation
- Simple cleanup

### 2. Callback Invocation Strategy
**Chose:** Copy callbacks to stack, invoke without lock
**Why:**
- Prevents deadlock if callbacks call back into sleep system
- Thread-safe
- Performance: callbacks invoked concurrently
- Tradeoff: Max 32 callbacks (easily increased)

### 3. Optional Integration
**Chose:** NULL checks, registration can fail gracefully
**Why:**
- Backwards compatible
- Modules without sleep support don't break
- Fallback to polling if callback fails

### 4. Matching on Both Callback + User Data
**Chose:** Unregister requires both function pointer and user_data
**Why:**
- Same callback function might be registered multiple times with different contexts
- Precise cleanup during module destruction

## Thread Safety

### Lock Strategy
1. **Registration/Unregistration**: Hold lock during list modification
2. **Notification**: Copy callbacks to stack WITH lock, invoke WITHOUT lock
3. **State Changes**: Brief lock during state update, callbacks invoked after unlock

### Deadlock Prevention
Callbacks are invoked WITHOUT holding the sleep system lock. This prevents:
- Module callback trying to call `sleep_get_current_state()` → deadlock
- Module callback trying to register another callback → deadlock
- Module callback holding its own lock and accessing sleep → deadlock cycle

## Usage Pattern for Sleep Bridges

### Step 1: Define Callback
```c
static void module_on_sleep_state_change(sleep_state_t new_state, void* user_data)
{
    module_sleep_bridge_t bridge = (module_sleep_bridge_t)user_data;

    nimcp_mutex_lock(bridge->mutex);
    bridge->effects.current_state = new_state;
    // Update module parameters based on new state
    nimcp_mutex_unlock(bridge->mutex);
}
```

### Step 2: Register During Creation
```c
module_sleep_bridge_t module_sleep_bridge_create(config, sleep)
{
    // ... create bridge ...

    bridge->callback_registered = sleep_register_state_callback(
        sleep, module_on_sleep_state_change, bridge);

    // Get initial state
    sleep_state_t initial_state = sleep_get_current_state(sleep);
    module_on_sleep_state_change(initial_state, bridge);

    return bridge;
}
```

### Step 3: Unregister During Destruction
```c
void module_sleep_bridge_destroy(module_sleep_bridge_t bridge)
{
    if (bridge->callback_registered) {
        sleep_unregister_state_callback(
            bridge->sleep_system,
            module_on_sleep_state_change,
            bridge);
    }
    // ... cleanup ...
}
```

## Benefits

### 1. Performance
- No polling overhead
- Immediate state updates
- Reduced function call overhead

### 2. Consistency
- All modules notified simultaneously
- No race conditions between modules
- Single source of truth for state

### 3. Maintainability
- Clear observer pattern
- Self-documenting code
- Easy to add new modules

### 4. Flexibility
- Optional integration (backwards compatible)
- Each module controls its own callback logic
- Graceful degradation if registration fails

## Modules That Can Use This

All 8+ sleep bridge modules can register callbacks:

1. **Cognitive Bridges:**
   - `nimcp_attention_sleep_bridge`
   - `nimcp_working_memory_sleep_bridge`
   - `nimcp_executive_sleep_bridge`
   - `nimcp_wellbeing_sleep_bridge`

2. **Plasticity Bridges:**
   - `nimcp_stdp_sleep_bridge`
   - `nimcp_bcm_sleep_bridge`
   - `nimcp_homeostatic_sleep_bridge`
   - `nimcp_neuromodulators_sleep_bridge`

3. **Core Bridges:**
   - `nimcp_oscillations_sleep_bridge`

## Testing Recommendations

### Unit Tests
1. Register single callback → verify invocation
2. Register multiple callbacks → verify all invoked
3. Unregister callback → verify not invoked
4. Unregister non-existent → returns false
5. Register with NULL callback → returns false
6. State transitions → verify callbacks called

### Integration Tests
1. Multiple bridges register callbacks
2. State cycle progression
3. Concurrent state changes
4. Module destruction cleanup

### Thread Safety Tests
1. Concurrent registrations
2. Concurrent state changes
3. Unregister during notification
4. Callback calls back into sleep system

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Register callback | O(1) | Prepend to linked list |
| Unregister callback | O(n) | Search linked list, n = callbacks |
| Notify callbacks | O(n) | Invoke each callback once |
| State change | O(n) | Notification dominates |

## Backward Compatibility

✅ **Fully backward compatible**
- Existing code continues to work
- `*_sleep_update()` functions still work
- Polling pattern still supported
- No breaking changes

## Next Steps (Optional)

1. **Update all 8+ sleep bridges** to use callbacks
2. **Add unit tests** for callback mechanism
3. **Add integration tests** for multi-module scenarios
4. **Document migration path** in each bridge
5. **Performance benchmarks** comparing polling vs callbacks
6. **Consider bio-async integration** for cross-module messaging

## Code Quality

✅ All functions documented with WHAT/WHY/HOW comments
✅ Guard clauses for NULL validation
✅ Thread-safe with mutex protection
✅ Proper memory management (malloc/free)
✅ Logging at appropriate levels
✅ Single responsibility functions
✅ Biological basis documented (observer pattern for neural coordination)

## Files for Reference

1. `SLEEP_STATE_BROADCAST_USAGE.md` - Detailed usage guide
2. `SLEEP_BRIDGE_CALLBACK_EXAMPLE.c` - Complete example for attention bridge
3. `include/cognitive/nimcp_sleep_wake.h` - Updated header
4. `src/cognitive/sleep_wake/nimcp_sleep_wake.c` - Implementation
