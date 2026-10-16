# Middleware Bio-Async Integration Quick Reference

## Quick Copy-Paste Templates

### Header File Template

```c
// Add these includes at the top (after existing includes)
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Add to config struct
typedef struct {
    // ... existing fields ...
    bool enable_bio_async;      /**< Enable bio-async integration */
} your_module_config_t;
```

### Source File Template

```c
// Add after existing includes
#include "utils/logging/nimcp_logging.h"

// Define module name for logging (must be before any function definitions)
#define LOG_MODULE "your_module_name"

// Add to internal struct
struct your_module_struct {
    // ... existing fields ...

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

// In create/init function - AFTER all initialization, BEFORE returning
module->bio_ctx = NULL;
module->bio_async_enabled = false;
if (config->enable_bio_async && bio_router_is_initialized()) {
    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_XXX,  // Choose appropriate module ID
        .module_name = "your_module_name",
        .inbox_capacity = 64,  // Or 32 for lightweight modules
        .user_data = module
    };
    module->bio_ctx = bio_router_register_module(&bio_info);
    if (module->bio_ctx) {
        module->bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async integration enabled");
    } else {
        LOG_WARN(LOG_MODULE, "Bio-async registration failed");
    }
}

LOG_INFO(LOG_MODULE, "Module created (bio_async=%d, /* other params */)",
         module->bio_async_enabled);

// In destroy function - BEFORE freeing module struct
LOG_DEBUG(LOG_MODULE, "Destroying module");

if (module->bio_async_enabled && module->bio_ctx) {
    bio_router_unregister_module(module->bio_ctx);
    module->bio_ctx = NULL;
    module->bio_async_enabled = false;
    LOG_DEBUG(LOG_MODULE, "Bio-async unregistered");
}

// ... rest of cleanup ...

LOG_INFO(LOG_MODULE, "Module destroyed");

// In default_config function
config.enable_bio_async = false;  // Disabled by default for backward compat
```

## Module IDs to Use

```c
/* Middleware modules (0x0500 - 0x05FF) */
BIO_MODULE_PIPELINE = 0x0500,        // Pipeline module
BIO_MODULE_ENCODING = 0x0501,        // Encoding modules (population, temporal, rate)
BIO_MODULE_EVENT_BUS = 0x0502,       // Event bus
BIO_MODULE_SIGNAL_ROUTER = 0x0503,   // Signal routing (if needed)
BIO_MODULE_TRAINING = 0x0504,        // Training modules
```

## Logging Macros

```c
// ERROR - Fatal errors, allocation failures
LOG_ERROR(LOG_MODULE, "Failed to allocate memory: size=%zu", size);

// WARN - Non-fatal issues, degraded performance
LOG_WARN(LOG_MODULE, "Queue full, dropping oldest event");

// INFO - Lifecycle events, major state changes
LOG_INFO(LOG_MODULE, "Module initialized (config=%d)", config_value);

// DEBUG - Detailed operations, timing, flow
LOG_DEBUG(LOG_MODULE, "Processing batch of %u items", count);
```

## Logging Best Practices

```c
// Creation
LOG_INFO(LOG_MODULE, "Module created (param1=%u, param2=%d, bio_async=%d)",
         config->param1, config->param2, module->bio_async_enabled);

// Important operations
LOG_DEBUG(LOG_MODULE, "Operation started (id=%u)", operation_id);
// ... do work ...
LOG_DEBUG(LOG_MODULE, "Operation completed in %llu us", elapsed);

// Errors with context
if (!result) {
    LOG_ERROR(LOG_MODULE, "Operation failed: %s (code=%d)",
              error_message, error_code);
    return false;
}

// Warnings with details
if (queue_full) {
    LOG_WARN(LOG_MODULE, "Resource exhausted (queue_size=%u, capacity=%u)",
             queue->size, queue->capacity);
}

// Destruction
LOG_DEBUG(LOG_MODULE, "Destroying module (active_items=%u)", count);
// ... cleanup ...
LOG_INFO(LOG_MODULE, "Module destroyed");
```

## Complete Example (Encoding Module)

```c
//=============================================================================
// your_encoder.h
//=============================================================================
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

typedef struct {
    // Existing config
    uint32_t sample_rate;
    float time_window_ms;

    // Bio-async
    bool enable_bio_async;
} your_encoder_config_t;

typedef struct your_encoder_struct* your_encoder_t;

your_encoder_t your_encoder_create(const your_encoder_config_t* config);
void your_encoder_destroy(your_encoder_t encoder);

//=============================================================================
// your_encoder.c
//=============================================================================
#include "your_encoder.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "your_encoder"

struct your_encoder_struct {
    your_encoder_config_t config;

    // Bio-async
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // Other state...
};

your_encoder_config_t your_encoder_default_config(void) {
    your_encoder_config_t config = {0};
    config.sample_rate = 1000;
    config.time_window_ms = 100.0f;
    config.enable_bio_async = false;
    return config;
}

your_encoder_t your_encoder_create(const your_encoder_config_t* config) {
    if (!config) {
        LOG_ERROR(LOG_MODULE, "NULL config provided");
        return NULL;
    }

    your_encoder_t encoder = nimcp_calloc(1, sizeof(struct your_encoder_struct));
    if (!encoder) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate encoder");
        return NULL;
    }

    encoder->config = *config;

    // Initialize other components...

    // Bio-async registration
    encoder->bio_ctx = NULL;
    encoder->bio_async_enabled = false;
    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_ENCODING,
            .module_name = "your_encoder",
            .inbox_capacity = 32,
            .user_data = encoder
        };
        encoder->bio_ctx = bio_router_register_module(&bio_info);
        if (encoder->bio_ctx) {
            encoder->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async integration enabled");
        } else {
            LOG_WARN(LOG_MODULE, "Bio-async registration failed");
        }
    }

    LOG_INFO(LOG_MODULE, "Encoder created (rate=%u, window=%.1fms, bio_async=%d)",
             config->sample_rate, config->time_window_ms, encoder->bio_async_enabled);

    return encoder;
}

void your_encoder_destroy(your_encoder_t encoder) {
    if (!encoder) return;

    LOG_DEBUG(LOG_MODULE, "Destroying encoder");

    // Unregister from bio-async
    if (encoder->bio_async_enabled && encoder->bio_ctx) {
        bio_router_unregister_module(encoder->bio_ctx);
        encoder->bio_ctx = NULL;
        encoder->bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered");
    }

    // Cleanup other resources...

    nimcp_free(encoder);
    LOG_INFO(LOG_MODULE, "Encoder destroyed");
}

// Example encoding function with logging
bool your_encoder_encode(your_encoder_t encoder, const float* input,
                         size_t input_size, float* output) {
    if (!encoder || !input || !output) {
        LOG_ERROR(LOG_MODULE, "Invalid parameters");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Encoding %zu samples", input_size);

    // Do encoding work...

    LOG_DEBUG(LOG_MODULE, "Encoding completed");
    return true;
}
```

## Testing Checklist

After integration:

- [ ] Module compiles without errors
- [ ] Module compiles without warnings
- [ ] Default config has `enable_bio_async = false`
- [ ] Module works with bio_async disabled (backward compat)
- [ ] Module works with bio_async enabled
- [ ] Logs appear with correct module name
- [ ] Create function logs INFO message
- [ ] Destroy function logs INFO message
- [ ] Bio-async registers successfully when enabled
- [ ] Bio-async unregisters on destroy
- [ ] No memory leaks (valgrind clean)
- [ ] No race conditions (thread sanitizer clean)

## Common Issues

### Issue: Module ID not found
```c
// Error: 'BIO_MODULE_YOUR_NAME' undeclared
// Solution: Use existing module ID or add to nimcp_bio_messages.h
.module_id = BIO_MODULE_ENCODING,  // Use existing category
```

### Issue: Logging not appearing
```c
// Problem: LOG_MODULE defined after includes
// Solution: Define BEFORE any function definitions
#define LOG_MODULE "module_name"  // Must be early in file
```

### Issue: Bio-async registration fails
```c
// Check: Is bio-router initialized?
if (!bio_router_is_initialized()) {
    LOG_WARN(LOG_MODULE, "Bio-router not initialized");
}

// Check: Valid module ID?
// Check: Router has capacity?
```

### Issue: Backward compatibility broken
```c
// Problem: Default config has bio_async enabled
// Solution: Always default to false
config.enable_bio_async = false;  // ✅ Correct
config.enable_bio_async = true;   // ❌ Wrong - breaks existing code
```

## Performance Tips

1. **Use appropriate log levels:**
   - DEBUG: Called frequently (can be compiled out)
   - INFO: Lifecycle only (create/destroy)
   - WARN: Occasional issues
   - ERROR: Real failures

2. **Minimize DEBUG logging in hot paths:**
```c
// ❌ Bad - logs every iteration
for (uint32_t i = 0; i < n; i++) {
    LOG_DEBUG(LOG_MODULE, "Processing item %u", i);
    process(items[i]);
}

// ✅ Good - logs batch operation
LOG_DEBUG(LOG_MODULE, "Processing batch of %u items", n);
for (uint32_t i = 0; i < n; i++) {
    process(items[i]);
}
```

3. **Cache bio_async_enabled:**
```c
// ✅ Good - check once
if (module->bio_async_enabled) {
    // Send bio-async message
}

// ❌ Bad - redundant checks
if (module->bio_ctx != NULL && module->bio_async_enabled) {
    // bio_async_enabled already implies bio_ctx != NULL
}
```

## File Checklist

When integrating a new module:

- [ ] `include/middleware/xxx/nimcp_xxx.h` - Add bio-async includes
- [ ] `include/middleware/xxx/nimcp_xxx.h` - Add `enable_bio_async` to config
- [ ] `src/middleware/xxx/nimcp_xxx.c` - Add `LOG_MODULE` define
- [ ] `src/middleware/xxx/nimcp_xxx.c` - Add logging include
- [ ] `src/middleware/xxx/nimcp_xxx.c` - Add bio-async fields to struct
- [ ] `src/middleware/xxx/nimcp_xxx.c` - Add bio-async registration in create
- [ ] `src/middleware/xxx/nimcp_xxx.c` - Add bio-async unregistration in destroy
- [ ] `src/middleware/xxx/nimcp_xxx.c` - Add logging throughout
- [ ] Update default config to include `enable_bio_async = false`
- [ ] Test compilation
- [ ] Test with bio_async disabled
- [ ] Test with bio_async enabled
- [ ] Update documentation

---

**Last Updated:** 2025-11-28
**Maintained By:** NIMCP Development Team
