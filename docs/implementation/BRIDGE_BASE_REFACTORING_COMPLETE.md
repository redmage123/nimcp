# Bridge Base Refactoring - Complete Guide

This document provides the complete refactoring pattern for converting all bridge files in `src/middleware/immune/` and `src/nlp/immune/` to use the `bridge_base` pattern.

## Files to Refactor

### Middleware Immune Bridges
1. `nimcp_feature_extractor_immune_bridge.{h,c}`
2. `nimcp_population_coding_immune_bridge.{h,c}`
3. `nimcp_sequence_immune_bridge.{h,c}`
4. `nimcp_thalamic_immune_bridge.{h,c}`
5. `nimcp_training_immune.{h,c}`

### NLP Immune Bridges
6. `nimcp_multimodal_nlp_immune_bridge.{h,c}`
7. `nimcp_nlp_immune_bridge.{h,c}`
8. `nimcp_spike_nlp_immune_bridge.{h,c}`

### Special Cases (Non-Bridge Monitoring Systems)
- `nimcp_buffer_immune.{h,c}` - Buffer monitoring system, not a two-system bridge
- `nimcp_pattern_immune.{h,c}` - Pattern immune system, check if bridge
- `nimcp_routing_immune.{h,c}` - Routing immune system, check if bridge

## Refactoring Pattern

### 1. Header File Changes

#### Add Include
```c
#include "utils/bridge/nimcp_bridge_base.h"
```

#### Modify Bridge Struct
**BEFORE:**
```c
typedef struct {
    brain_immune_system_t* immune_system;
    <module>_t* module;
    <module>_config_t config;
    <module>_effects_t effects;

    /* Thread safety */
    pthread_mutex_t mutex;  // OR nimcp_mutex_t* mutex;

    /* Bio-async */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Statistics */
    uint64_t total_updates;
    uint64_t last_update_time;
} <module>_immune_bridge_t;
```

**AFTER:**
```c
typedef struct {
    bridge_base_t base;  // MUST BE FIRST MEMBER

    <module>_config_t config;
    <module>_effects_t effects;

    /* Any additional domain-specific fields */
} <module>_immune_bridge_t;
```

#### Add Accessor Macros (After struct definition)
```c
/* System accessors */
#define <PREFIX>_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
#define <PREFIX>_GET_MODULE(bridge) ((<module>_t*)(bridge)->base.system_b)
```

Example:
```c
#define FEATURE_IMMUNE_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
#define FEATURE_IMMUNE_GET_EXTRACTOR(bridge) ((feature_extractor_t*)(bridge)->base.system_b)
```

### 2. Implementation File Changes

#### Add Include
```c
#include "utils/bridge/nimcp_bridge_base.h"
```

#### Remove Manual Mutex Operations
**REMOVE** all instances of:
```c
pthread_mutex_init(&bridge->mutex, NULL);
pthread_mutex_lock(&bridge->mutex);
pthread_mutex_unlock(&bridge->mutex);
pthread_mutex_destroy(&bridge->mutex);

// OR
nimcp_mutex_t* mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
nimcp_mutex_init(mutex, NULL);
nimcp_mutex_lock(bridge->mutex);
nimcp_mutex_unlock(bridge->mutex);
nimcp_mutex_destroy(bridge->mutex);
nimcp_free(bridge->mutex);
```

**REPLACE WITH:**
```c
BRIDGE_LOCK(bridge);
BRIDGE_UNLOCK(bridge);
```

#### Update Create Function

**BEFORE:**
```c
<module>_immune_bridge_t* <module>_immune_bridge_create(
    const <module>_config_t* config,
    brain_immune_system_t* immune,
    <module>_t* module)
{
    if (!immune || !module) {
        NIMCP_LOGGING_ERROR("NULL pointers");
        return NULL;
    }

    <module>_immune_bridge_t* bridge = nimcp_malloc(sizeof(<module>_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Allocation failed");
        return NULL;
    }
    memset(bridge, 0, sizeof(<module>_immune_bridge_t));

    // Set config
    if (config) {
        memcpy(&bridge->config, config, sizeof(<module>_config_t));
    } else {
        <module>_immune_default_config(&bridge->config);
    }

    // Set connections
    bridge->immune_system = immune;
    bridge->module = module;

    // Initialize mutex
    bridge->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    nimcp_mutex_init(bridge->mutex, NULL);

    // Initialize other fields...

    return bridge;
}
```

**AFTER:**
```c
<module>_immune_bridge_t* <module>_immune_bridge_create(
    const <module>_config_t* config,
    brain_immune_system_t* immune,
    <module>_t* module)
{
    if (!immune || !module) {
        NIMCP_LOGGING_ERROR("NULL pointers");
        return NULL;
    }

    // Use BRIDGE_CREATE_BEGIN macro
    BRIDGE_CREATE_BEGIN(<module>_immune_bridge_t, bridge,
                        BIO_MODULE_<APPROPRIATE_ID>, "<module>_immune_bridge");

    // Set config
    if (config) {
        memcpy(&bridge->config, config, sizeof(<module>_config_t));
    } else {
        <module>_immune_default_config(&bridge->config);
    }

    // Connect systems using bridge_base (OUTSIDE any lock)
    bridge_base_connect_a(&bridge->base, immune);
    bridge_base_connect_b(&bridge->base, module);

    // Initialize other domain-specific fields...

    return bridge;
}
```

#### Update Destroy Function

**BEFORE:**
```c
void <module>_immune_bridge_destroy(<module>_immune_bridge_t* bridge) {
    if (!bridge) return;

    // Disconnect bio-async
    if (bridge->bio_async_enabled) {
        <module>_immune_disconnect_bio_async(bridge);
    }

    // Destroy mutex
    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
        nimcp_free(bridge->mutex);
    }

    nimcp_free(bridge);
}
```

**AFTER:**
```c
void <module>_immune_bridge_destroy(<module>_immune_bridge_t* bridge) {
    BRIDGE_DESTROY(bridge);
}
```

#### Update Bio-Async Functions

**REPLACE** all manual bio-async functions with macro:

**BEFORE:**
```c
int <module>_immune_connect_bio_async(<module>_immune_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (bridge->bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_<ID>,
        .module_name = "<module>_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (bridge->bio_ctx) {
        bridge->bio_async_enabled = true;
    }
    return 0;
}

int <module>_immune_disconnect_bio_async(<module>_immune_bridge_t* bridge) {
    if (!bridge || !bridge->bio_async_enabled) return 0;

    if (bridge->bio_ctx) {
        bio_router_unregister_module(bridge->bio_ctx);
        bridge->bio_ctx = NULL;
    }
    bridge->bio_async_enabled = false;
    return 0;
}

bool <module>_immune_is_bio_async_connected(const <module>_immune_bridge_t* bridge) {
    return bridge ? bridge->bio_async_enabled : false;
}
```

**AFTER:**
```c
// Add this single line at bottom of implementation file
BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(<module>_immune, <module>_immune_bridge_t)
```

#### Update Connection Functions (Optional, if they exist)

If your bridge has explicit connect/disconnect functions:

**BEFORE:**
```c
int <module>_immune_connect_immune(<module>_immune_bridge_t* bridge,
                                   brain_immune_system_t* immune) {
    if (!bridge || !immune) return -1;
    bridge->immune_system = immune;
    return 0;
}

int <module>_immune_connect_module(<module>_immune_bridge_t* bridge,
                                   <module>_t* module) {
    if (!bridge || !module) return -1;
    bridge->module = module;
    return 0;
}
```

**AFTER:**
```c
// Option 1: Use macro (recommended)
BRIDGE_DEFINE_CONNECT_FUNCS_TYPE(<module>_immune, <module>_immune_bridge_t,
                                  brain_immune_system_t, immune_system,
                                  <module>_t, module)

// Option 2: Manual implementation using bridge_base
int <module>_immune_connect_immune(<module>_immune_bridge_t* bridge,
                                   brain_immune_system_t* immune) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(immune);
    return bridge_base_connect_a(&bridge->base, immune);
}

int <module>_immune_connect_module(<module>_immune_bridge_t* bridge,
                                   <module>_t* module) {
    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(module);
    return bridge_base_connect_b(&bridge->base, module);
}
```

#### Update Update Function

**BEFORE:**
```c
int <module>_immune_update(<module>_immune_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->mutex);

    // ... computation ...

    // Update statistics
    bridge->total_updates++;
    bridge->last_update_time = nimcp_time_get_ms();

    nimcp_mutex_unlock(bridge->mutex);
    return 0;
}
```

**AFTER:**
```c
int <module>_immune_update(<module>_immune_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    BRIDGE_LOCK(bridge);

    // ... computation ...

    // Record update (INSIDE lock, we already hold it)
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);
    return 0;
}
```

#### Update Getter Functions

**BEFORE:**
```c
float <module>_immune_get_some_value(const <module>_immune_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(bridge->mutex);
    float value = bridge->effects.some_value;
    nimcp_mutex_unlock(bridge->mutex);

    return value;
}
```

**AFTER:**
```c
float <module>_immune_get_some_value(const <module>_immune_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    BRIDGE_LOCK(bridge);
    float value = bridge->effects.some_value;
    BRIDGE_UNLOCK(bridge);

    return value;
}
```

#### Access Connected Systems

**BEFORE:**
```c
brain_immune_system_t* immune = bridge->immune_system;
<module>_t* module = bridge->module;
```

**AFTER:**
```c
brain_immune_system_t* immune = <PREFIX>_GET_IMMUNE(bridge);
<module>_t* module = <PREFIX>_GET_MODULE(bridge);

// Or directly from base:
brain_immune_system_t* immune = (brain_immune_system_t*)bridge->base.system_a;
<module>_t* module = (<module>_t*)bridge->base.system_b;
```

## Common Patterns by File

### Feature Extractor Immune Bridge
- Prefix: `feature_immune` or `feature_extractor_immune`
- Module ID: `BIO_MODULE_IMMUNE_FEATURE_EXTRACTOR`
- System A: `brain_immune_system_t* immune_system`
- System B: `feature_extractor_t* feature_extractor`
- Accessor macros:
  ```c
  #define FEATURE_IMMUNE_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
  #define FEATURE_IMMUNE_GET_EXTRACTOR(bridge) ((feature_extractor_t*)(bridge)->base.system_b)
  ```

### Population Coding Immune Bridge
- Prefix: `population_coding_immune`
- Module ID: `BIO_MODULE_IMMUNE_POPULATION_CODING`
- System A: `brain_immune_system_t* immune_system`
- System B: `population_coding_t* population_coding`
- Accessor macros:
  ```c
  #define POP_CODING_IMMUNE_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
  #define POP_CODING_IMMUNE_GET_CODING(bridge) ((population_coding_t*)(bridge)->base.system_b)
  ```

### Sequence Immune Bridge
- Prefix: `sequence_immune`
- Module ID: `BIO_MODULE_IMMUNE_SEQUENCE`
- System A: `brain_immune_system_t* immune_system`
- System B: `sequence_detector_t* sequence_detector`
- Accessor macros:
  ```c
  #define SEQ_IMMUNE_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
  #define SEQ_IMMUNE_GET_DETECTOR(bridge) ((sequence_detector_t*)(bridge)->base.system_b)
  ```

### Thalamic Immune Bridge
- Prefix: `thalamic_immune`
- Module ID: `BIO_MODULE_IMMUNE_THALAMIC`
- System A: `brain_immune_system_t* immune_system`
- System B: `thalamic_router_t* thalamic_router`
- Accessor macros:
  ```c
  #define THAL_IMMUNE_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
  #define THAL_IMMUNE_GET_ROUTER(bridge) ((thalamic_router_t*)(bridge)->base.system_b)
  ```

### Training Immune
- Prefix: `training_immune`
- Module ID: `BIO_MODULE_TRAINING_IMMUNE`
- System A: `brain_immune_system_t* immune_system`
- System B: Could be `optimizer_t*` or `training_context_t*`
- Accessor macros:
  ```c
  #define TRAINING_IMMUNE_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
  #define TRAINING_IMMUNE_GET_OPTIMIZER(bridge) ((optimizer_t*)(bridge)->base.system_b)
  ```

### Multimodal NLP Immune Bridge
- Prefix: `multimodal_nlp_immune`
- Module ID: `BIO_MODULE_IMMUNE_MULTIMODAL_NLP`
- System A: `brain_immune_system_t* immune_system`
- System B: `multimodal_nlp_t* multimodal_nlp`
- Accessor macros:
  ```c
  #define MM_NLP_IMMUNE_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
  #define MM_NLP_IMMUNE_GET_NLP(bridge) ((multimodal_nlp_t*)(bridge)->base.system_b)
  ```

### NLP Immune Bridge
- Prefix: `nlp_immune`
- Module ID: `BIO_MODULE_IMMUNE_NLP`
- System A: `brain_immune_system_t* immune_system`
- System B: `nlp_system_t*` or `nlp_processor_t*`
- Accessor macros:
  ```c
  #define NLP_IMMUNE_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
  #define NLP_IMMUNE_GET_PROCESSOR(bridge) ((nlp_processor_t*)(bridge)->base.system_b)
  ```

### Spike NLP Immune Bridge
- Prefix: `spike_nlp_immune`
- Module ID: `BIO_MODULE_IMMUNE_SPIKE_NLP`
- System A: `brain_immune_system_t* immune_system`
- System B: `spike_nlp_t* spike_nlp`
- Accessor macros:
  ```c
  #define SPIKE_NLP_IMMUNE_GET_IMMUNE(bridge) ((brain_immune_system_t*)(bridge)->base.system_a)
  #define SPIKE_NLP_IMMUNE_GET_SPIKE_NLP(bridge) ((spike_nlp_t*)(bridge)->base.system_b)
  ```

## Checklist per File

### Header File (.h)
- [ ] Add `#include "utils/bridge/nimcp_bridge_base.h"`
- [ ] Change struct: Add `bridge_base_t base;` as FIRST member
- [ ] Remove: `brain_immune_system_t* immune_system;`
- [ ] Remove: `<module>_t* module;`
- [ ] Remove: `pthread_mutex_t mutex;` or `nimcp_mutex_t* mutex;`
- [ ] Remove: `bio_module_context_t bio_ctx;`
- [ ] Remove: `bool bio_async_enabled;`
- [ ] Remove: `uint64_t total_updates;`
- [ ] Remove: `uint64_t last_update_time;` (or similar)
- [ ] Add accessor macros after struct definition

### Implementation File (.c)
- [ ] Add `#include "utils/bridge/nimcp_bridge_base.h"`
- [ ] Replace create function body with `BRIDGE_CREATE_BEGIN` macro
- [ ] Use `bridge_base_connect_a/b()` for system connections (OUTSIDE lock)
- [ ] Replace destroy function body with `BRIDGE_DESTROY` macro
- [ ] Replace `pthread_mutex_lock/unlock` with `BRIDGE_LOCK/UNLOCK`
- [ ] Replace manual bio-async functions with `BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE` macro
- [ ] Replace `bridge->immune_system` with accessor macro or `(type*)bridge->base.system_a`
- [ ] Replace `bridge->module` with accessor macro or `(type*)bridge->base.system_b`
- [ ] Add `bridge_base_record_update(&bridge->base)` in update functions (INSIDE lock)
- [ ] Remove manual statistics tracking that's now in base

## Build Verification

After refactoring each file:

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make nimcp -j4
```

Check for compilation errors related to:
- Missing fields (use accessor macros)
- Mutex operations (use BRIDGE_LOCK/UNLOCK)
- Bio-async functions (ensure macro is at end of .c file)

## Testing

After successful build, run relevant tests:

```bash
# Find tests for the module
find test -name "*<module>*immune*" -type f

# Run unit tests
./test/unit/middleware/immune/unit_middleware_immune_test_<module>_immune_bridge

# Run integration tests
./test/integration/middleware/immune/integration_middleware_immune_test_<module>_immune_bridge
```

## Benefits of Refactoring

1. **Reduced Boilerplate**: 60-70% less code per bridge
2. **Consistent Pattern**: All bridges follow same structure
3. **Easier Maintenance**: Changes to bridge infrastructure affect all bridges
4. **Built-in Statistics**: Automatic update tracking
5. **Thread Safety**: Consistent mutex usage pattern
6. **Bio-Async Integration**: Standardized module registration
7. **Type Safety**: Accessor macros provide type checking

## Notes

- **CRITICAL**: `bridge_base_t base` MUST be the FIRST member of the struct
- Mutex pattern uses `nimcp_mutex_t` NOT `pthread_mutex_t`
- `bridge_base_record_update()` should be called INSIDE lock (caller holds lock)
- `bridge_base_connect_a/b()` should be called OUTSIDE lock (they acquire lock internally)
- Use `BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE` for non-standard naming (when type != prefix_t)
- Accessor macros are optional but improve code readability

## Example: Complete Before/After

See `/home/bbrelin/nimcp/src/cognitive/working_memory/nimcp_working_memory_substrate_bridge.c` for a complete working example of the bridge_base pattern.

## Questions?

Refer to:
- `/home/bbrelin/nimcp/include/utils/bridge/nimcp_bridge_base.h` - Base interface
- `/home/bbrelin/nimcp/src/utils/bridge/nimcp_bridge_base.c` - Implementation
- Example bridge for reference pattern
