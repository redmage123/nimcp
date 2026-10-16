# NIMCP Middleware Refactoring Guide

## Overview

This guide documents the systematic refactoring of all middleware modules to meet the following requirements:

1. **Async Communication**: Replace tight coupling with async futures/promises
2. **Unified Memory**: Replace malloc/free with nimcp_malloc/nimcp_free
3. **Comprehensive Logging**: Add LOG_MODULE_* throughout
4. **Configurable Parameters**: Make hyperparameters configurable via config module
5. **Security Registration**: Register each module with security system

## Refactoring Pattern

### 1. Header Additions

Add these includes to every .c file:

```c
#include "utils/logging/nimcp_logging.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "security/nimcp_security.h"
#include "async/nimcp_future.h"
```

### 2. Module Globals

Add module-level state tracking:

```c
// Module name for logging and security registration
#define MODULE_NAME "your_module_name"

// Security module ID (set during registration)
static uint32_t s_security_module_id = 0;
static bool s_module_initialized = false;
```

### 3. Module Init/Shutdown Functions

Add initialization and shutdown functions:

```c
/**
 * @brief Initialize module
 *
 * WHAT: Register module with security system and load configuration
 * WHY:  Enable security monitoring and configurable parameters
 * HOW:  Call security_register_module() and load config defaults
 *
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t <module>_init(void) {
    if (s_module_initialized) {
        LOG_MODULE_WARN(MODULE_NAME, "Module already initialized");
        return NIMCP_SUCCESS;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Initializing %s module", MODULE_NAME);

    // Register with security system
    s_security_module_id = security_register_module(MODULE_NAME, SECURITY_LEVEL_MEDIUM);
    if (s_security_module_id == 0) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to register with security system");
        return NIMCP_ERROR_SECURITY_REGISTRATION_FAILED;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Registered with security system (ID: %u)",
                    s_security_module_id);

    // Load configuration defaults
    // Example: s_default_timeout = config_get_int("module.timeout_ms", 1000);

    s_module_initialized = true;
    LOG_MODULE_INFO(MODULE_NAME, "Module initialization complete");

    return NIMCP_SUCCESS;
}

/**
 * @brief Shutdown module
 */
void <module>_shutdown(void) {
    if (!s_module_initialized) {
        return;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Shutting down %s module", MODULE_NAME);

    // Module cleanup
    s_security_module_id = 0;
    s_module_initialized = false;

    LOG_MODULE_INFO(MODULE_NAME, "Module shutdown complete");
}
```

### 4. Memory Allocation Replacements

Replace all memory allocations:

**OLD:**
```c
ptr = malloc(size);
ptr = calloc(count, size);
ptr = realloc(ptr, new_size);
str = strdup(source);
free(ptr);
```

**NEW:**
```c
ptr = nimcp_malloc(size);
ptr = nimcp_calloc(count, size);
ptr = nimcp_realloc(ptr, new_size);
str = nimcp_strdup(source);
nimcp_free(ptr);
```

### 5. Add Logging

Add logging at key points:

**Function Entry (Debug):**
```c
LOG_MODULE_DEBUG(MODULE_NAME, "function_name called with param=%d", param);
```

**Successful Operations (Info):**
```c
LOG_MODULE_INFO(MODULE_NAME, "Operation completed successfully");
```

**Warnings:**
```c
LOG_MODULE_WARN(MODULE_NAME, "Buffer near capacity: %zu/%zu", used, total);
```

**Errors:**
```c
LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate memory (size=%zu)", size);
```

### 6. Make Parameters Configurable

Replace hardcoded constants with config lookups:

**OLD:**
```c
#define MAX_CAPACITY 1024
#define TIMEOUT_MS 1000
#define THRESHOLD 0.95f
```

**NEW:**
```c
size_t max_capacity = config_get_int("module.max_capacity", 1024);
int timeout_ms = config_get_int("module.timeout_ms", 1000);
float threshold = config_get_float("module.threshold", 0.95f);
bool enable_feature = config_get_bool("module.enable_feature", true);
const char* mode = config_get_string("module.mode", "default");
```

### 7. Async Communication (Where Applicable)

Replace tight coupling with async events:

**OLD (Direct Call):**
```c
other_module_process(data);
```

**NEW (Async Event):**
```c
// Create promise for async result
nimcp_promise_t promise = nimcp_promise_create(sizeof(result_t));
nimcp_future_t future = nimcp_promise_get_future(promise);

// Publish event with promise
event_t event = {
    .type = EVENT_TYPE_PROCESS_REQUEST,
    .data = data,
    .promise = promise
};
event_bus_publish(bus, &event);

// Optionally wait for result
if (nimcp_future_wait_timeout(future, 1000)) {
    result_t result;
    nimcp_future_get(future, &result);
}

nimcp_future_destroy(future);
nimcp_promise_destroy(promise);
```

### 8. Update Create Functions

All create functions should:
1. Auto-initialize module if needed
2. Validate inputs with logging
3. Check config-based limits
4. Log creation

**Template:**
```c
module_t* module_create(params) {
    // Auto-initialize module
    if (!s_module_initialized) {
        if (module_init() != NIMCP_SUCCESS) {
            LOG_MODULE_ERROR(MODULE_NAME, "Module initialization failed");
            return NULL;
        }
    }

    // Validate inputs
    if (invalid_params) {
        LOG_MODULE_ERROR(MODULE_NAME, "Invalid parameters: param=%d", param);
        return NULL;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Creating module instance with param=%d", param);

    // Check configurable limits
    size_t max_size = config_get_int("module.max_size", DEFAULT_MAX);
    if (size > max_size) {
        LOG_MODULE_ERROR(MODULE_NAME, "Size %zu exceeds maximum %zu", size, max_size);
        return NULL;
    }

    // Allocate with unified memory
    module_t* mod = nimcp_calloc(1, sizeof(module_t));
    if (!mod) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate module structure");
        return NULL;
    }

    // Initialize...

    LOG_MODULE_INFO(MODULE_NAME, "Created module instance: param=%d", param);
    return mod;
}
```

## Module-Specific Refactoring Checklist

### Buffering Modules (5 files)
- [x] nimcp_circular_buffer.c - COMPLETED (template)
- [ ] nimcp_integration_buffer.c
- [ ] nimcp_phase_coded_buffer.c
- [ ] nimcp_sliding_window.c
- [ ] nimcp_temporal_accumulator.c

### Cognitive Modules (2 files)
- [ ] nimcp_cognitive_adapters.c
- [ ] nimcp_working_memory_adapter.c

### Encoding Modules (3 files)
- [ ] nimcp_population_coding.c
- [ ] nimcp_rate_coding.c
- [ ] nimcp_temporal_coding.c

### Events Modules (4 files)
- [ ] nimcp_event_bus.c - Already has some async, needs config/logging/security
- [ ] nimcp_event_queue.c
- [ ] nimcp_event_subscriber.c
- [ ] nimcp_event_types.c

### Features Module (1 file)
- [ ] nimcp_feature_extractor.c

### Integration Modules (6 files)
- [ ] nimcp_executive_middleware_adapter.c
- [ ] nimcp_flow_tracker.c
- [ ] nimcp_middleware_controller.c
- [ ] nimcp_quantum_command_propagator.c
- [ ] nimcp_shannon_monitor.c

### Normalization Modules (4 files)
- [ ] nimcp_adaptive_normalizer.c
- [ ] nimcp_homeostatic_normalizer.c
- [ ] nimcp_min_max_normalizer.c
- [ ] nimcp_zscore_normalizer.c

### Patterns Modules (5 files)
- [ ] nimcp_oscillation_detector.c
- [ ] nimcp_pattern_cow.c
- [ ] nimcp_pattern_library.c
- [ ] nimcp_sequence_detector.c
- [ ] nimcp_synchrony_detector.c

### Pipeline Modules (2 files)
- [ ] nimcp_middleware_context.c
- [ ] nimcp_middleware_pipeline.c

### Routing Modules (4 files)
- [ ] nimcp_attention_gate.c
- [ ] nimcp_routing_table.c
- [ ] nimcp_signal_wrapper.c
- [ ] nimcp_thalamic_router.c

### Training Modules (11 files)
- [ ] nimcp_brain_training_integration.c
- [ ] nimcp_event_driven_plasticity.c
- [ ] nimcp_gradient_manager.c
- [ ] nimcp_loss_functions.c
- [ ] nimcp_lr_scheduler.c
- [ ] nimcp_optimizers.c
- [ ] nimcp_regularization.c
- [ ] nimcp_training_adapters.c
- [ ] nimcp_training_callbacks.c
- [ ] nimcp_training_module.c
- [ ] nimcp_training_plasticity_bridge.c

## Configuration Keys Convention

Use hierarchical naming:
- `<module>.<parameter>` - Basic parameter
- `<module>.<category>.<parameter>` - Categorized parameter

Examples:
- `circular_buffer.max_capacity`
- `event_bus.queue_size`
- `training.learning_rate.initial`
- `training.optimizer.adam.beta1`

## Testing Requirements

### Unit Tests
Each refactored module needs unit tests for:
1. Module init/shutdown
2. Security registration
3. Config parameter loading
4. Memory allocation tracking
5. Async event handling (where applicable)
6. All existing functionality

### Integration Tests
Test interactions between modules:
1. Event bus message passing
2. Async promise completion
3. Config changes affecting behavior
4. Security audit trails

## Common Pitfalls

1. **Don't forget module init**: Always call or auto-initialize before use
2. **Match allocation/free**: Use nimcp_malloc with nimcp_free, not mixed
3. **Config defaults**: Always provide sensible defaults
4. **Log levels**: Use DEBUG for trace, INFO for events, WARN for issues, ERROR for failures
5. **Null checks**: Log errors before returning NULL
6. **Security levels**: Choose appropriate level (LOW/MEDIUM/HIGH/CRITICAL)

## Automation Script

For bulk refactoring, use this sed/awk pattern:

```bash
#!/bin/bash
# refactor_module.sh - Apply refactoring pattern to a module

FILE=$1
MODULE_NAME=$2

# Add includes after existing includes
sed -i '/#include.*\.h"/a\
#include "utils/logging/nimcp_logging.h"\
#include "utils/config/nimcp_dynamic_config.h"\
#include "security/nimcp_security.h"\
#include "async/nimcp_future.h"' "$FILE"

# Replace malloc/calloc/realloc/free
sed -i 's/\bmalloc\(/nimcp_malloc(/g' "$FILE"
sed -i 's/\bcalloc\(/nimcp_calloc(/g' "$FILE"
sed -i 's/\brealloc\(/nimcp_realloc(/g' "$FILE"
sed -i 's/\bfree\(/nimcp_free(/g' "$FILE"
sed -i 's/\bstrdup\(/nimcp_strdup(/g' "$FILE"

echo "Refactored $FILE for module $MODULE_NAME"
echo "Manual steps:"
echo "1. Add MODULE_NAME define"
echo "2. Add static module state variables"
echo "3. Add init/shutdown functions"
echo "4. Add logging calls"
echo "5. Add config lookups"
echo "6. Add async events (if needed)"
```

## Progress Tracking

Total modules: 46
Completed: 1 (circular_buffer)
Remaining: 45

Estimated effort: 30-60 minutes per module × 45 = 22-45 hours total
