# NIMCP Core Modules Refactoring Guide

## Overview

This document provides a systematic approach for refactoring all 60+ NIMCP core modules to integrate:

1. **Async/Future Communication** - Replace direct function calls with async futures
2. **Enhanced Logging** - Add comprehensive logging with module names
3. **Config Integration** - Make all hyperparameters configurable
4. **Security Registration** - Register each module with security system
5. **Unified Memory** - Ensure all allocations use `nimcp_malloc`/`nimcp_free`

## Scope

- **Total Files**: 60+ C source files
- **Total Lines**: 57,646 lines of code
- **Estimated Effort**: 40-60 hours for complete refactoring
- **Modules**: axon, brain, dendrite, neuron_models, synapse_compute, topology, and more

## Refactoring Pattern

### 1. Module-Level Changes

Every module must implement these module-level functions:

```c
//=============================================================================
// MODULE CONSTANTS
//=============================================================================

#define MODULE_NAME "module_name"  // e.g., "axon", "dendrite"
#define MODULE_VERSION "2.0.0"

//=============================================================================
// MODULE STATE
//=============================================================================

typedef struct {
    bool initialized;
    uint32_t security_module_id;
    nimcp_mutex_t module_lock;
    // Module-specific statistics
} module_state_t;

static module_state_t g_module = {
    .initialized = false,
    .security_module_id = 0
};

//=============================================================================
// MODULE INITIALIZATION
//=============================================================================

/**
 * @brief Initialize module
 *
 * WHAT: Initialize module state, register with security
 * WHY:  Enable security monitoring and resource management
 * HOW:  One-time init with security registration
 */
nimcp_result_t module_init(void) {
    if (g_module.initialized) {
        LOG_MODULE_WARN(MODULE_NAME, "Already initialized");
        return NIMCP_SUCCESS;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Initializing %s module v%s",
                    MODULE_NAME, MODULE_VERSION);

    // Initialize module lock
    nimcp_mutex_init(&g_module.module_lock, NULL);

    // Register with security system
    // g_module.security_module_id = security_register_module(MODULE_NAME, MODULE_VERSION);

    g_module.initialized = true;

    LOG_MODULE_INFO(MODULE_NAME, "Module initialized successfully");
    return NIMCP_SUCCESS;
}

/**
 * @brief Shutdown module
 */
void module_shutdown(void) {
    if (!g_module.initialized) {
        return;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Shutting down module");

    // Unregister from security
    // security_unregister_module(g_module.security_module_id);

    nimcp_mutex_destroy(&g_module.module_lock);

    g_module.initialized = false;
    LOG_MODULE_INFO(MODULE_NAME, "Shutdown complete");
}
```

### 2. Configuration Integration

Replace ALL hardcoded constants with config lookups:

**Before:**
```c
static const float LEARNING_RATE = 0.01f;
static const int MAX_NEURONS = 1000;
```

**After:**
```c
static inline float get_learning_rate(void) {
    return (float)config_get_float("module.learning_rate", 0.01);
}

static inline int get_max_neurons(void) {
    return (int)config_get_int("module.max_neurons", 1000);
}
```

### 3. Logging Integration

Add logging at ALL key points:

**Function Entry:**
```c
LOG_MODULE_DEBUG(MODULE_NAME, "Creating object id=%u with params x=%f, y=%f",
                 id, x, y);
```

**Success:**
```c
LOG_MODULE_INFO(MODULE_NAME, "Successfully created object id=%u", id);
```

**Warnings:**
```c
LOG_MODULE_WARN(MODULE_NAME, "Object %u: Parameter x=%f out of normal range", id, x);
```

**Errors:**
```c
LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate memory for object id=%u", id);
```

**Tracing (Detailed):**
```c
LOG_MODULE_TRACE(MODULE_NAME, "Object %u: Computing velocity with diameter=%f myelination=%f",
                 id, diameter, myelination);
```

### 4. Async/Future Integration

Replace synchronous operations with async where appropriate:

**Before:**
```c
object_t* object_create(uint32_t id, float param) {
    object_t* obj = (object_t*)nimcp_calloc(1, sizeof(object_t));
    if (!obj) return NULL;
    // ... initialization ...
    return obj;
}
```

**After:**
```c
// Async version
nimcp_future_t object_create_async(uint32_t id, float param) {
    nimcp_promise_t promise = nimcp_promise_create(sizeof(object_t*));
    if (!promise) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to create promise");
        return NULL;
    }

    object_t* obj = (object_t*)nimcp_calloc(1, sizeof(object_t));
    if (!obj) {
        nimcp_promise_fail(promise, NIMCP_ERROR_NO_MEMORY);
        nimcp_future_t future = nimcp_promise_get_future(promise);
        nimcp_promise_destroy(promise);
        return future;
    }

    // ... initialization ...

    nimcp_promise_complete(promise, &obj);
    nimcp_future_t future = nimcp_promise_get_future(promise);
    nimcp_promise_destroy(promise);
    return future;
}

// Synchronous wrapper for backward compatibility
object_t* object_create(uint32_t id, float param) {
    nimcp_future_t future = object_create_async(id, param);
    if (!future) return NULL;

    uint32_t timeout_ms = (uint32_t)config_get_int("module.creation_timeout_ms", 5000);
    if (!nimcp_future_wait_timeout(future, timeout_ms)) {
        nimcp_future_destroy(future);
        return NULL;
    }

    object_t* obj = NULL;
    nimcp_future_get(future, &obj);
    nimcp_future_destroy(future);
    return obj;
}
```

### 5. Memory Allocation

Ensure ALL allocations use unified memory:

**Required:**
- `nimcp_malloc()` instead of `malloc()`
- `nimcp_calloc()` instead of `calloc()`
- `nimcp_realloc()` instead of `realloc()`
- `nimcp_free()` instead of `free()`
- `nimcp_strdup()` instead of `strdup()`

**Search & Replace:**
```bash
# Find all malloc/free calls
grep -rn "malloc\|calloc\|realloc\|free\|strdup" src/core/*.c

# Verify all use nimcp_ prefix
grep -rn "(?<!nimcp_)(malloc|calloc|realloc|free|strdup)" src/core/*.c
```

### 6. Error Handling

Consistent error handling pattern:

```c
// Guard clauses at function start
if (!ptr) {
    LOG_MODULE_ERROR(MODULE_NAME, "NULL pointer parameter");
    return NULL;  // or appropriate error code
}

if (value < MIN || value > MAX) {
    LOG_MODULE_ERROR(MODULE_NAME, "Value %f out of range [%f, %f]",
                     value, MIN, MAX);
    return NIMCP_ERROR_INVALID_PARAMETER;
}

// Resource allocation with cleanup
object_t* obj = nimcp_calloc(1, sizeof(object_t));
if (!obj) {
    LOG_MODULE_ERROR(MODULE_NAME, "Memory allocation failed");
    return NULL;
}

// ... use obj ...

// Cleanup on error
if (error_condition) {
    LOG_MODULE_ERROR(MODULE_NAME, "Error condition detected");
    nimcp_free(obj);
    return NULL;
}
```

## File-by-File Checklist

For each file, complete this checklist:

- [ ] Add MODULE_NAME and MODULE_VERSION constants
- [ ] Add module state structure
- [ ] Implement `module_init()` and `module_shutdown()`
- [ ] Register with security system in init
- [ ] Replace all hardcoded constants with config_get_*()
- [ ] Add LOG_MODULE_* calls at key points:
  - [ ] Function entry (DEBUG level)
  - [ ] Success paths (INFO level)
  - [ ] Error paths (ERROR level)
  - [ ] Warnings (WARN level)
  - [ ] Detailed tracing (TRACE level)
- [ ] Add async versions of creation/heavy operations
- [ ] Verify all malloc/calloc/free use nimcp_ prefix
- [ ] Add comprehensive error handling
- [ ] Update function documentation with WHAT/WHY/HOW
- [ ] Write unit tests
- [ ] Write integration tests

## Module Priority Order

Refactor in this order to minimize dependencies:

### Phase 1: Foundation (Week 1)
1. **axon** - Signal propagation (COMPLETE - see example)
2. **dendrite** - Signal reception
3. **neuron_models** - Neuron dynamics
4. **synapse_types** - Synaptic connections

### Phase 2: Core Brain (Week 2)
5. **brain** - Main brain structure
6. **brain/factory** - Brain creation
7. **brain/persistence** - Save/load
8. **neuralnet** - Network management

### Phase 3: Advanced (Week 3)
9. **cortical_columns** - Cortical organization
10. **brain_oscillations** - Network rhythms
11. **brain_regions** - Regional specialization
12. **topology** - Network topology

### Phase 4: Integration (Week 4)
13. **integration** - Multimodal integration
14. **synapse_compute** - Synaptic computation
15. Remaining modules
16. **Comprehensive testing**

## Testing Strategy

### Unit Tests

Create unit tests for each module in `/home/bbrelin/nimcp/test/unit/core/<module>/`:

```c
// test_<module>_refactored.cpp

TEST(ModuleRefactored, Initialization) {
    // Test module init
    EXPECT_EQ(module_init(), NIMCP_SUCCESS);
    EXPECT_TRUE(module_is_initialized());

    // Test double init is safe
    EXPECT_EQ(module_init(), NIMCP_SUCCESS);

    module_shutdown();
}

TEST(ModuleRefactored, ConfigIntegration) {
    module_init();

    // Set config value
    config_set_float("module.param", 3.14f);

    // Create object and verify it uses config
    object_t* obj = object_create(1, 0);
    EXPECT_NE(obj, nullptr);
    // Verify object used config value
    EXPECT_FLOAT_EQ(obj->param, 3.14f);

    object_destroy(obj);
    module_shutdown();
}

TEST(ModuleRefactored, AsyncCreation) {
    module_init();

    // Test async creation
    nimcp_future_t future = object_create_async(1, 2.5f);
    EXPECT_NE(future, nullptr);

    // Wait with timeout
    EXPECT_TRUE(nimcp_future_wait_timeout(future, 1000));

    // Get result
    object_t* obj = nullptr;
    EXPECT_EQ(nimcp_future_get(future, &obj), NIMCP_SUCCESS);
    EXPECT_NE(obj, nullptr);

    nimcp_future_destroy(future);
    object_destroy(obj);
    module_shutdown();
}

TEST(ModuleRefactored, Logging) {
    // Capture log output
    // Create object and verify logs were generated
    // Check for expected log messages
}

TEST(ModuleRefactored, SecurityRegistration) {
    module_init();

    // Verify module registered with security
    // uint32_t sec_id = module_get_security_id();
    // EXPECT_NE(sec_id, 0);

    module_shutdown();
}
```

### Integration Tests

Create integration tests in `/home/bbrelin/nimcp/test/integration/core/<module>/`:

```c
// test_<module>_integration.cpp

TEST(ModuleIntegration, EndToEndWorkflow) {
    // Initialize multiple modules
    module1_init();
    module2_init();

    // Create objects
    obj1_t* obj1 = obj1_create(1, 1.0f);
    obj2_t* obj2 = obj2_create(2, 2.0f);

    // Test interaction
    obj1_connect_to_obj2(obj1, obj2);

    // Verify async communication
    nimcp_future_t future = obj1_send_signal_async(obj1, 5.0f);
    EXPECT_TRUE(nimcp_future_wait_timeout(future, 1000));

    // Cleanup
    obj1_destroy(obj1);
    obj2_destroy(obj2);
    module2_shutdown();
    module1_shutdown();
}
```

## Configuration File Template

Create `/home/bbrelin/nimcp/config/core_modules.ini`:

```ini
[axon]
velocity_coeff_unmyelinated = 1.0
velocity_coeff_myelinated = 6.0
activity_decay_factor = 0.99
min_velocity = 0.1
refractory_period_ms = 1.0
atp_consumption_per_spike = 0.01
atp_regeneration_rate = 0.001
creation_timeout_ms = 5000
use_segment_pool = false
default_g_ratio = 0.77
initial_atp_level = 1.0
min_atp_for_spike = 0.1

[dendrite]
default_diameter = 2.0
default_length = 100.0
...

[neuron_models]
...

# Add sections for all modules
```

## Progress Tracking

Use todo list to track completion:

```bash
# Mark axon as complete
# Mark dendrite as in_progress
# etc.
```

## Validation

After refactoring each module:

1. **Compile**: `cd build && make`
2. **Run unit tests**: `ctest -R test_<module>`
3. **Run integration tests**: `ctest -R integration_<module>`
4. **Check logging**: Verify log output includes module name
5. **Check config**: Verify config values are used
6. **Memory check**: Run with valgrind to verify no leaks
7. **Coverage**: Aim for >90% code coverage

## Common Pitfalls

### 1. Forgetting Module Init

**Problem**: Using module before calling `module_init()`

**Solution**: Add check in all public functions:
```c
if (!g_module.initialized) {
    LOG_MODULE_ERROR(MODULE_NAME, "Module not initialized");
    return NULL;
}
```

### 2. Config Key Typos

**Problem**: Typo in config key leads to always using default

**Solution**: Define config keys as constants:
```c
#define CONFIG_KEY_LEARNING_RATE "module.learning_rate"

float lr = config_get_float(CONFIG_KEY_LEARNING_RATE, 0.01);
```

### 3. Memory Leaks in Async Paths

**Problem**: Forgetting to destroy futures/promises

**Solution**: Always destroy in cleanup:
```c
nimcp_future_t future = create_async();
// ... use future ...
nimcp_future_destroy(future);  // Always!
```

### 4. Log Flooding

**Problem**: Too many TRACE logs slow down execution

**Solution**: Use appropriate log levels:
- TRACE: Very detailed, disabled in production
- DEBUG: Detailed but important info
- INFO: Normal operations
- WARN: Potential issues
- ERROR: Actual errors

## Tools

### Search & Replace Script

```bash
#!/bin/bash
# replace_hardcoded_constants.sh

FILE=$1

# Replace malloc with nimcp_malloc
sed -i 's/\bmalloc(/nimcp_malloc(/g' "$FILE"
sed -i 's/\bcalloc(/nimcp_calloc(/g' "$FILE"
sed -i 's/\brealloc(/nimcp_realloc(/g' "$FILE"
sed -i 's/\bfree(/nimcp_free(/g' "$FILE"
sed -i 's/\bstrdup(/nimcp_strdup(/g' "$FILE"

echo "Replaced memory functions in $FILE"
```

### Logging Insertion Script

```bash
#!/bin/bash
# add_logging.sh

# Add LOG_MODULE_DEBUG at function entry
# Add LOG_MODULE_ERROR before error returns
# etc.
```

## Estimated Timeline

- **Phase 1 (Foundation)**: 10-15 hours (4 modules)
- **Phase 2 (Core Brain)**: 15-20 hours (4 modules)
- **Phase 3 (Advanced)**: 15-20 hours (4 modules)
- **Phase 4 (Integration)**: 10-15 hours (remaining + tests)

**Total**: 50-70 hours of development time

## Next Steps

1. Complete axon module refactoring (use provided example as template)
2. Apply same pattern to dendrite module
3. Continue with neuron_models
4. ...progressively refactor all 60+ files

## Example Files

- **Complete example**: `/home/bbrelin/nimcp/src/core/axon/nimcp_axon_refactored_example.c`
- **This guide**: `/home/bbrelin/nimcp/docs/CORE_REFACTORING_GUIDE.md`

## Questions?

Contact NIMCP development team or refer to:
- Async API: `/home/bbrelin/nimcp/include/async/nimcp_future.h`
- Logging API: `/home/bbrelin/nimcp/include/utils/logging/nimcp_logging.h`
- Config API: `/home/bbrelin/nimcp/include/utils/config/nimcp_dynamic_config.h`
- Security API: `/home/bbrelin/nimcp/include/security/nimcp_security.h`
- Memory API: `/home/bbrelin/nimcp/include/utils/memory/nimcp_memory.h`
