# Swarm Module Bio-Async Integration

## Summary

Added bio-async messaging support to swarm modules for inter-module communication and coordination.

## Completed Work

### 1. Added Missing Bio-Async Module IDs

**File:** `/home/bbrelin/nimcp/include/async/nimcp_bio_messages.h`

Added 15 new swarm module IDs to the `bio_module_id_t` enum (lines 625-639):

```c
/* Swarm modules (0x0B00 - 0x0BFF) */
BIO_MODULE_SWARM_ENERGY_GOSSIP = 0x0B00,
BIO_MODULE_SWARM_CASCADE,
BIO_MODULE_SWARM_PROPRIOCEPTION,
BIO_MODULE_SWARM_MEMORY,
BIO_MODULE_SWARM_QUORUM,
BIO_MODULE_SWARM_FLOCKING,
BIO_MODULE_SWARM_IMMUNE,
BIO_MODULE_SWARM_MORPHOGENESIS,
BIO_MODULE_SWARM_MULTI,
BIO_MODULE_SWARM_PHEROMONE,
BIO_MODULE_SWARM_SIGNAL,                      // NEW
BIO_MODULE_SWARM_CONSENSUS,                   // NEW
BIO_MODULE_SWARM_EMERGENCE,                   // NEW
BIO_MODULE_SWARM_BRAIN,                       // NEW
BIO_MODULE_SWARM_BRAIN_LOCAL,                 // NEW
BIO_MODULE_SWARM_CONSCIOUSNESS,               // NEW
BIO_MODULE_SWARM_CONSCIOUSNESS_ENHANCED,      // NEW
BIO_MODULE_SWARM_CONFLICT,                    // NEW
BIO_MODULE_SWARM_GATEWAY,                     // NEW
BIO_MODULE_SWARM_LOGIC_BRIDGE,                // NEW
BIO_MODULE_SWARM_NARRATIVE,                   // NEW
BIO_MODULE_SWARM_PROTOCOL,                    // NEW
BIO_MODULE_COLLECTIVE_WORKSPACE,              // NEW
BIO_MODULE_EMOTIONAL_CONTAGION,               // NEW
BIO_MODULE_GOSSIP_BELIEFS,                    // NEW
```

### 2. Fully Integrated Modules

#### swarm_consensus
- **Header:** `/home/bbrelin/nimcp/include/swarm/nimcp_swarm_consensus.h`
- **Implementation:** `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_consensus.c`
- **Module ID:** `BIO_MODULE_SWARM_CONSENSUS`
- **API:**
  - `nimcp_error_t swarm_consensus_connect_bio_async(swarm_consensus_t ctx)`
  - `nimcp_error_t swarm_consensus_disconnect_bio_async(swarm_consensus_t ctx)`
  - `bool swarm_consensus_is_bio_async_connected(const swarm_consensus_t ctx)`

#### swarm_signal
- **Header:** `/home/bbrelin/nimcp/include/swarm/nimcp_swarm_signal.h`
- **Implementation:** `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_signal.c`
- **Module ID:** `BIO_MODULE_SWARM_SIGNAL`
- **API:**
  - `bool swarm_signal_connect_bio_async(nimcp_swarm_signal_adapter_t* adapter)`
  - `bool swarm_signal_disconnect_bio_async(nimcp_swarm_signal_adapter_t* adapter)`
  - `bool swarm_signal_is_bio_async_connected(const nimcp_swarm_signal_adapter_t* adapter)`

## Integration Pattern

### Standard Bio-Async Integration Template

For each swarm module, follow this pattern:

#### 1. Header File Changes

Add to the end of the public API section (before `#ifdef __cplusplus`):

```c
//=============================================================================
// Bio-async Integration API
//=============================================================================

/**
 * @brief Connect <module> to bio-async router
 *
 * WHAT: Register module with bio-async messaging system
 * WHY:  Enable inter-module messaging for coordination
 * HOW:  Register with BIO_MODULE_<MODULE_NAME> ID
 *
 * @param ctx/adapter Module context
 * @return NIMCP_SUCCESS or error code (OR bool for non-error modules)
 */
<return_type> <module>_connect_bio_async(<module_type>* obj);

/**
 * @brief Disconnect <module> from bio-async router
 *
 * WHAT: Unregister from bio-async messaging system
 * WHY:  Clean shutdown of messaging
 * HOW:  Deregister module and cleanup
 *
 * @param ctx/adapter Module context
 * @return NIMCP_SUCCESS or error code (OR bool)
 */
<return_type> <module>_disconnect_bio_async(<module_type>* obj);

/**
 * @brief Check if <module> is connected to bio-async
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging availability
 * HOW:  Check bio_async_enabled flag
 *
 * @param ctx/adapter Module context
 * @return true if connected, false otherwise
 */
bool <module>_is_bio_async_connected(const <module_type>* obj);
```

#### 2. Implementation File Changes

**A. Add includes:**

```c
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
```

**B. Add struct fields:**

```c
struct <module_struct_name> {
    // ... existing fields ...

    // Bio-async integration
    bio_module_context_t bio_ctx;      /**< Bio-async module context */
    bool bio_async_enabled;            /**< Whether bio-async is active */
};
```

**C. Initialize in create function:**

```c
// Initialize bio-async fields
obj->bio_ctx = NULL;
obj->bio_async_enabled = false;
```

**D. Cleanup in destroy function:**

```c
/* Disconnect bio-async if connected */
if (obj->bio_async_enabled) {
    <module>_disconnect_bio_async(obj);
}
```

**E. Implement bio-async functions:**

```c
//=============================================================================
// Bio-async Integration API
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
<return_type> <module>_connect_bio_async(<module_type>* obj)
{
    if (!obj) {
        return <error_value>;  // NIMCP_ERROR_NULL_POINTER or false
    }

    if (obj->bio_async_enabled) {
        return <success_value>;  // Already connected
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_<MODULE_NAME>,
        .module_name = "<module_name>",
        .inbox_capacity = 32,
        .user_data = obj
    };

    obj->bio_ctx = bio_router_register_module(&info);
    if (obj->bio_ctx) {
        obj->bio_async_enabled = true;
        LOG_INFO("Connected to bio-async router");
    } else {
        LOG_INFO("Bio-async router not available, skipping registration");
    }

    return <success_value>;
}

/**
 * @brief Disconnect from bio-async router
 */
<return_type> <module>_disconnect_bio_async(<module_type>* obj)
{
    if (!obj) {
        return <error_value>;
    }

    if (!obj->bio_async_enabled) {
        return <success_value>;  // Not connected
    }

    if (obj->bio_ctx) {
        bio_router_deregister_module(obj->bio_ctx);
        obj->bio_ctx = NULL;
    }

    obj->bio_async_enabled = false;
    LOG_INFO("Disconnected from bio-async router");

    return <success_value>;
}

/**
 * @brief Check if connected to bio-async
 */
bool <module>_is_bio_async_connected(const <module_type>* obj)
{
    if (!obj) {
        return false;
    }

    return obj->bio_async_enabled;
}
```

## Remaining Swarm Modules to Integrate

### High Priority (Core Functionality)
1. **swarm_emergence** - BIO_MODULE_SWARM_EMERGENCE
2. **swarm_brain** - BIO_MODULE_SWARM_BRAIN
3. **swarm_brain_local** - BIO_MODULE_SWARM_BRAIN_LOCAL
4. **collective_workspace** - BIO_MODULE_COLLECTIVE_WORKSPACE
5. **emotional_contagion** - BIO_MODULE_EMOTIONAL_CONTAGION

### Medium Priority (Enhanced Features)
6. **swarm_consciousness** - BIO_MODULE_SWARM_CONSCIOUSNESS
7. **swarm_consciousness_enhanced** - BIO_MODULE_SWARM_CONSCIOUSNESS_ENHANCED
8. **swarm_conflict** - BIO_MODULE_SWARM_CONFLICT
9. **swarm_gateway** - BIO_MODULE_SWARM_GATEWAY
10. **gossip_beliefs** - BIO_MODULE_GOSSIP_BELIEFS

### Lower Priority (Support Modules)
11. **swarm_cascade** - BIO_MODULE_SWARM_CASCADE (may already have bio-async)
12. **swarm_protocol** - BIO_MODULE_SWARM_PROTOCOL
13. **swarm_narrative** - BIO_MODULE_SWARM_NARRATIVE
14. **swarm_logic_bridge** - BIO_MODULE_SWARM_LOGIC_BRIDGE

### Note: Already Integrated Modules
These modules already have bio-async integration:
- swarm_energy_gossip (BIO_MODULE_SWARM_ENERGY_GOSSIP)
- swarm_proprioception (BIO_MODULE_SWARM_PROPRIOCEPTION)
- swarm_memory (BIO_MODULE_SWARM_MEMORY)
- swarm_quorum (BIO_MODULE_SWARM_QUORUM)
- swarm_flocking (BIO_MODULE_SWARM_FLOCKING)
- swarm_immune (BIO_MODULE_SWARM_IMMUNE)
- swarm_morphogenesis (BIO_MODULE_SWARM_MORPHOGENESIS)
- swarm_multi (BIO_MODULE_SWARM_MULTI)
- swarm_pheromone (BIO_MODULE_SWARM_PHEROMONE)

## State-Change Callback Mechanisms

### Purpose
Enable modules to notify other systems when significant state changes occur (e.g., tier transitions in emergence, consensus decisions, conflict resolution).

### Pattern

#### 1. Define callback type in header:

```c
/**
 * @brief State change callback
 *
 * @param old_state Previous state
 * @param new_state New state
 * @param user_ctx User-provided context
 */
typedef void (*<module>_state_change_callback_t)(
    const <state_type>* old_state,
    const <state_type>* new_state,
    void* user_ctx
);
```

#### 2. Add to module struct:

```c
<module>_state_change_callback_t state_change_callback;
void* state_change_callback_ctx;
```

#### 3. Add registration function:

```c
/**
 * @brief Register state change callback
 *
 * @param obj Module object
 * @param callback Callback function
 * @param user_ctx User context for callback
 * @return Success/error code
 */
<return_type> <module>_register_state_change_callback(
    <module_type>* obj,
    <module>_state_change_callback_t callback,
    void* user_ctx
);
```

#### 4. Invoke when state changes:

```c
// In state transition code
if (obj->state_change_callback) {
    obj->state_change_callback(&old_state, &new_state, obj->state_change_callback_ctx);
}
```

### Recommended Modules for State-Change Callbacks

1. **swarm_emergence** - Tier transitions
2. **swarm_consensus** - Vote completion
3. **swarm_conflict** - Conflict resolution
4. **swarm_brain** - Decision changes
5. **collective_workspace** - Workspace updates
6. **emotional_contagion** - Emotion spread events
7. **swarm_consciousness** - Consciousness level changes

## Testing Recommendations

For each integrated module, add tests for:

1. **Connection lifecycle:**
   - Connect/disconnect
   - Double connect (idempotent)
   - Double disconnect (safe)
   - Destroy while connected

2. **Status queries:**
   - Connection status after connect
   - Connection status after disconnect
   - Connection status with NULL pointer

3. **Bio-async messaging:**
   - Send messages to other modules
   - Receive messages
   - Message ordering

4. **Integration tests:**
   - Multi-module coordination
   - State-change callback triggers
   - Bio-async + callback interaction

## Benefits

1. **Inter-module Communication:** Swarm modules can coordinate via bio-async messaging
2. **Event Broadcasting:** State changes can be broadcast to interested modules
3. **Decoupled Architecture:** Modules don't need direct references to each other
4. **Scalability:** Bio-async router handles message routing efficiently
5. **Monitoring:** All communication goes through router for observability

## Next Steps

1. Complete bio-async integration for remaining 15 swarm modules
2. Add state-change callbacks to 7 high-priority modules
3. Create integration tests for swarm module coordination
4. Document swarm messaging protocols (message types, routing patterns)
5. Add bio-async orchestrator support for swarm module lifecycle
