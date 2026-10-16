# Swarm Bio-Async Integration - Quick Reference

## Quick Checklist for Each Module

### Files to Modify
- [ ] `include/swarm/nimcp_<module>.h`
- [ ] `src/swarm/nimcp_<module>.c`

### Header Changes (`include/swarm/nimcp_<module>.h`)

**Location:** Add before final `#endif`

```c
//=============================================================================
// Bio-async Integration API
//=============================================================================

<return_type> <module>_connect_bio_async(<type>* obj);
<return_type> <module>_disconnect_bio_async(<type>* obj);
bool <module>_is_bio_async_connected(const <type>* obj);
```

### Implementation Changes (`src/swarm/nimcp_<module>.c`)

#### 1. Add includes (top of file):
```c
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
```

#### 2. Add to struct:
```c
bio_module_context_t bio_ctx;
bool bio_async_enabled;
```

#### 3. In create function:
```c
obj->bio_ctx = NULL;
obj->bio_async_enabled = false;
```

#### 4. In destroy function (BEFORE other cleanup):
```c
if (obj->bio_async_enabled) {
    <module>_disconnect_bio_async(obj);
}
```

#### 5. Add functions (end of file):
```c
//=============================================================================
// Bio-async Integration API
//=============================================================================

<return_type> <module>_connect_bio_async(<type>* obj)
{
    if (!obj) return <error>;
    if (obj->bio_async_enabled) return <success>;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_<NAME>,
        .module_name = "<name>",
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
    return <success>;
}

<return_type> <module>_disconnect_bio_async(<type>* obj)
{
    if (!obj) return <error>;
    if (!obj->bio_async_enabled) return <success>;

    if (obj->bio_ctx) {
        bio_router_deregister_module(obj->bio_ctx);
        obj->bio_ctx = NULL;
    }
    obj->bio_async_enabled = false;
    LOG_INFO("Disconnected from bio-async router");
    return <success>;
}

bool <module>_is_bio_async_connected(const <type>* obj)
{
    if (!obj) return false;
    return obj->bio_async_enabled;
}
```

## Module-Specific Details

### Return Types
- **nimcp_error_t modules:** Return `NIMCP_ERROR_NULL_POINTER` on error, `NIMCP_SUCCESS` on success
- **bool modules:** Return `false` on error, `true` on success

### Module ID Mapping

| Module | BIO_MODULE_* ID |
|--------|-----------------|
| swarm_emergence | BIO_MODULE_SWARM_EMERGENCE |
| swarm_brain | BIO_MODULE_SWARM_BRAIN |
| swarm_brain_local | BIO_MODULE_SWARM_BRAIN_LOCAL |
| swarm_consciousness | BIO_MODULE_SWARM_CONSCIOUSNESS |
| swarm_consciousness_enhanced | BIO_MODULE_SWARM_CONSCIOUSNESS_ENHANCED |
| swarm_conflict | BIO_MODULE_SWARM_CONFLICT |
| swarm_gateway | BIO_MODULE_SWARM_GATEWAY |
| swarm_logic_bridge | BIO_MODULE_SWARM_LOGIC_BRIDGE |
| swarm_narrative | BIO_MODULE_SWARM_NARRATIVE |
| swarm_protocol | BIO_MODULE_SWARM_PROTOCOL |
| collective_workspace | BIO_MODULE_COLLECTIVE_WORKSPACE |
| emotional_contagion | BIO_MODULE_EMOTIONAL_CONTAGION |
| gossip_beliefs | BIO_MODULE_GOSSIP_BELIEFS |
| swarm_cascade | BIO_MODULE_SWARM_CASCADE |

### Common Struct Types

| Module | Struct Name | Type Name |
|--------|-------------|-----------|
| swarm_emergence | swarm_emergence_ctx | swarm_emergence_ctx_t* |
| swarm_brain | swarm_brain | swarm_brain_t* |
| swarm_consciousness | swarm_consciousness | swarm_consciousness_t* |
| collective_workspace | collective_workspace | collective_workspace_t* |
| emotional_contagion | emotional_contagion | emotional_contagion_t* |

### Find Struct Definition
```bash
grep "struct <module_name>" src/swarm/nimcp_<module>.c -A 20
```

### Find Create/Destroy Functions
```bash
grep "<module>_create\|<module>_destroy" src/swarm/nimcp_<module>.c -A 15
```

## State-Change Callback Pattern

### For modules with important state transitions:

#### 1. Add to header:
```c
typedef void (*<module>_state_change_callback_t)(
    <old_state_type> old_state,
    <new_state_type> new_state,
    void* user_ctx
);

<return_type> <module>_register_state_change_callback(
    <type>* obj,
    <module>_state_change_callback_t callback,
    void* user_ctx
);
```

#### 2. Add to struct:
```c
<module>_state_change_callback_t state_change_callback;
void* state_change_callback_ctx;
```

#### 3. Initialize in create:
```c
obj->state_change_callback = NULL;
obj->state_change_callback_ctx = NULL;
```

#### 4. Implement registration:
```c
<return_type> <module>_register_state_change_callback(
    <type>* obj,
    <module>_state_change_callback_t callback,
    void* user_ctx
)
{
    if (!obj) return <error>;
    obj->state_change_callback = callback;
    obj->state_change_callback_ctx = user_ctx;
    return <success>;
}
```

#### 5. Invoke on state change:
```c
if (obj->state_change_callback) {
    obj->state_change_callback(old_value, new_value, obj->state_change_callback_ctx);
}
```

## Validation Commands

### Check compilation:
```bash
cd /home/bbrelin/nimcp/build
make nimcp -j4
```

### Check for bio-async integration:
```bash
grep -l "bio_async_enabled" src/swarm/*.c
```

### Verify module IDs:
```bash
grep "BIO_MODULE_SWARM" include/async/nimcp_bio_messages.h
```

## Common Pitfalls

1. **Missing includes** - Always add both bio_router.h and bio_messages.h
2. **Wrong module ID** - Use exact ID from nimcp_bio_messages.h
3. **Forgot cleanup** - Call disconnect in destroy BEFORE freeing memory
4. **Wrong return type** - Match existing module's return convention
5. **NULL checks** - Always check pointer before accessing
6. **BBB modules** - Some use bbb_check_pointer() for validation

## Example: swarm_emergence (Complete)

### Header addition:
```c
// At end of nimcp_swarm_emergence.h, before #endif
int swarm_emergence_connect_bio_async(swarm_emergence_ctx_t* ctx);
int swarm_emergence_disconnect_bio_async(swarm_emergence_ctx_t* ctx);
bool swarm_emergence_is_bio_async_connected(const swarm_emergence_ctx_t* ctx);
```

### Implementation changes:
```c
// Includes
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Struct
struct swarm_emergence_ctx {
    // ... existing fields ...
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

// Create function
obj->bio_ctx = NULL;
obj->bio_async_enabled = false;

// Destroy function (FIRST thing)
if (ctx->bio_async_enabled) {
    swarm_emergence_disconnect_bio_async(ctx);
}

// New functions at end
int swarm_emergence_connect_bio_async(swarm_emergence_ctx_t* ctx)
{
    if (!ctx) return -1;
    if (ctx->bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SWARM_EMERGENCE,
        .module_name = "swarm_emergence",
        .inbox_capacity = 32,
        .user_data = ctx
    };

    ctx->bio_ctx = bio_router_register_module(&info);
    if (ctx->bio_ctx) {
        ctx->bio_async_enabled = true;
        LOG_INFO("Connected to bio-async router");
    } else {
        LOG_INFO("Bio-async router not available");
    }
    return 0;
}

int swarm_emergence_disconnect_bio_async(swarm_emergence_ctx_t* ctx)
{
    if (!ctx) return -1;
    if (!ctx->bio_async_enabled) return 0;

    if (ctx->bio_ctx) {
        bio_router_deregister_module(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
    }
    ctx->bio_async_enabled = false;
    LOG_INFO("Disconnected from bio-async router");
    return 0;
}

bool swarm_emergence_is_bio_async_connected(const swarm_emergence_ctx_t* ctx)
{
    if (!ctx) return false;
    return ctx->bio_async_enabled;
}
```

## Time Estimates

Per module (experienced developer):
- Header changes: 2-3 minutes
- Implementation changes: 5-7 minutes
- Testing: 3-5 minutes
- **Total: ~10-15 minutes per module**

For all 15 remaining modules: **~2.5-4 hours**
