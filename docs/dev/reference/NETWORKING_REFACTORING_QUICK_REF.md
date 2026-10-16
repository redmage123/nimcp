# NIMCP Networking Refactoring - Quick Reference Card

## 1. Required Headers (Add to ALL modules)

```c
#include "async/nimcp_future.h"
#include "security/nimcp_security.h"
#include "utils/config/nimcp_dynamic_config.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
```

## 2. Module Setup (Add to ALL modules)

```c
#define MODULE_NAME "networking.module_name"
static uint32_t g_security_module_id = 0;
```

## 3. Memory Replacements (DO everywhere)

```c
// OLD                    // NEW
malloc(size)         →   nimcp_malloc(size)
calloc(n, size)      →   nimcp_calloc(n, size)
free(ptr)            →   nimcp_free(ptr)
realloc(ptr, size)   →   nimcp_realloc(ptr, size)
strdup(str)          →   nimcp_strdup(str)
```

## 4. Logging Template (Use throughout)

```c
// Function entry (public API)
LOG_MODULE_INFO(MODULE_NAME, "Function called (param=%d)", param);

// Detailed tracing
LOG_MODULE_DEBUG(MODULE_NAME, "State changed: %s->%s", old, new);

// Recoverable error
LOG_MODULE_WARN(MODULE_NAME, "Retry %d/%d failed", retry, max);

// Critical error
LOG_MODULE_ERROR(MODULE_NAME, "Operation failed: %s", error);
```

## 5. Configuration Loading Pattern

```c
#define CFG_PARAM_NAME "module.param_name"

static void load_configuration(config_t* config) {
    LOG_MODULE_INFO(MODULE_NAME, "Loading configuration");

    // Set defaults
    *config = DEFAULT_CONFIG;

    // Load integer parameter
    int param_value;
    if (config_get_int(CFG_PARAM_NAME, &param_value) && param_value > 0) {
        config->param = param_value;
        LOG_MODULE_DEBUG(MODULE_NAME, "Config: param=%d", param_value);
    } else {
        config->param = DEFAULT_VALUE;
        LOG_MODULE_DEBUG(MODULE_NAME, "Config default: param=%d", DEFAULT_VALUE);
    }

    // Load float parameter
    float float_value;
    if (config_get_float(CFG_FLOAT, &float_value) &&
        float_value >= MIN && float_value <= MAX) {
        config->float_param = float_value;
    }

    // Load boolean parameter
    bool bool_value;
    if (config_get_bool(CFG_BOOL, &bool_value)) {
        config->bool_param = bool_value;
    }

    // Load string parameter
    const char* str_value;
    if (config_get_string(CFG_STRING, &str_value)) {
        strncpy(config->str_param, str_value, sizeof(config->str_param) - 1);
    }
}
```

## 6. Async Operation Pattern

```c
// OLD (blocking):
bool operation(module_t mod, params) {
    // ... do work ...
    return success;
}

// NEW (async):
nimcp_future_t operation_async(module_t mod, params) {
    LOG_MODULE_DEBUG(MODULE_NAME, "Starting async operation");

    // Create promise/future pair
    nimcp_promise_t promise = nimcp_promise_create(sizeof(result_t));
    if (!promise) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to create promise");
        return NULL;
    }

    nimcp_future_t future = nimcp_promise_get_future(promise);
    if (!future) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to get future");
        nimcp_promise_destroy(promise);
        return NULL;
    }

    // Enqueue work
    enqueue_work(mod, params, promise);

    return future;
}

// Worker thread completes promise:
static void worker_thread_fn(void* arg) {
    while (running) {
        work_item_t* work = dequeue_work();

        result_t result = do_work(work);

        if (success) {
            nimcp_promise_complete(work->promise, &result);
        } else {
            nimcp_promise_fail(work->promise, NIMCP_ERROR_XXX);
        }

        nimcp_promise_destroy(work->promise);
    }
}
```

## 7. Security Registration

```c
// In module create function:
module_t module_create(...) {
    module_t mod = nimcp_calloc(1, sizeof(struct module_struct));
    if (!mod) {
        LOG_MODULE_ERROR(MODULE_NAME, "Allocation failed");
        return NULL;
    }

    // ... initialization ...

    // Register with security
    g_security_module_id = security_register_module(MODULE_NAME);
    if (g_security_module_id > 0) {
        LOG_MODULE_INFO(MODULE_NAME, "Security registered (ID=%u)",
                       g_security_module_id);
    } else {
        LOG_MODULE_WARN(MODULE_NAME, "Security registration failed");
    }

    return mod;
}
```

## 8. Input Validation Template

```c
bool function(module_t mod, const uint8_t* data, size_t size) {
    // Validate module handle
    if (!mod) {
        LOG_MODULE_ERROR(MODULE_NAME, "NULL module handle");
        return false;
    }

    // Validate data pointer
    if (!data) {
        LOG_MODULE_ERROR(MODULE_NAME, "NULL data pointer");
        return false;
    }

    // Validate size
    if (size == 0 || size > MAX_SIZE) {
        LOG_MODULE_ERROR(MODULE_NAME, "Invalid size: %zu (max: %zu)",
                        size, MAX_SIZE);
        return false;
    }

    // Process
    LOG_MODULE_DEBUG(MODULE_NAME, "Processing data (size=%zu)", size);
    return true;
}
```

## 9. Error Handling Pattern

```c
result_t* function(...) {
    LOG_MODULE_DEBUG(MODULE_NAME, "Function entry");

    // Allocate result
    result_t* result = nimcp_calloc(1, sizeof(result_t));
    if (!result) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate result");
        return NULL;
    }

    // Allocate buffer
    result->buffer = nimcp_malloc(BUFFER_SIZE);
    if (!result->buffer) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate buffer");
        nimcp_free(result);  // Clean up previously allocated
        return NULL;
    }

    // Process
    if (!process_data(result)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Processing failed");
        nimcp_free(result->buffer);
        nimcp_free(result);
        return NULL;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Function success");
    return result;
}
```

## 10. Cleanup Pattern

```c
void module_destroy(module_t mod) {
    if (!mod) {
        LOG_MODULE_DEBUG(MODULE_NAME, "NULL handle (safe no-op)");
        return;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Destroying module");

    // Signal shutdown to threads
    mod->shutdown_requested = true;
    nimcp_cond_broadcast(&mod->cond);

    // Wait for threads
    if (mod->worker_thread) {
        nimcp_thread_join(mod->worker_thread, NULL);
        LOG_MODULE_DEBUG(MODULE_NAME, "Worker thread joined");
    }

    // Clean up queue
    while (mod->queue_head) {
        queue_entry_t* entry = mod->queue_head;
        mod->queue_head = entry->next;
        nimcp_promise_fail(entry->promise, NIMCP_ERROR_CANCELLED);
        nimcp_free(entry->data);
        nimcp_promise_destroy(entry->promise);
        nimcp_free(entry);
    }

    // Destroy synchronization primitives
    nimcp_cond_destroy(&mod->cond);
    nimcp_mutex_destroy(&mod->mutex);

    // Log final stats
    LOG_MODULE_INFO(MODULE_NAME, "Final stats: processed=%u, errors=%u",
                   mod->stats.processed, mod->stats.errors);

    // Free structure
    nimcp_free(mod);

    LOG_MODULE_INFO(MODULE_NAME, "Module destroyed");
}
```

## 11. Unit Test Template

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "networking/module/module.h"
#include "async/nimcp_future.h"
}

class ModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_future_init(nullptr, nullptr);
        config_init(nullptr);
        // Set test config values
        config_set_int("module.param", 100);

        module = module_create(...);
        ASSERT_NE(module, nullptr);
    }

    void TearDown() override {
        module_destroy(module);
        config_shutdown();
        nimcp_future_shutdown();
    }

    module_t module;
};

TEST_F(ModuleTest, AsyncOperation) {
    nimcp_future_t future = module_operation_async(module, params);
    ASSERT_NE(future, nullptr);

    bool success = nimcp_future_wait_timeout(future, 1000);
    ASSERT_TRUE(success);

    result_t result;
    nimcp_error_t err = nimcp_future_get(future, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    nimcp_future_destroy(future);
}
```

## 12. Configuration Parameters Format

```ini
# Module configuration file format
# Boolean
module.enable_feature = true

# Integer (ms for timeouts)
module.timeout_ms = 1000
module.max_queue_size = 100

# Float (0.0-1.0 for rates)
module.retry_rate = 0.1

# String
module.backend = "filesystem"
```

## 13. Common Config Keys by Module

### Distributed Cognition
```
distrib_cog.enable_neuromod_sync
distrib_cog.neuromod_broadcast_interval_ms
distrib_cog.neuromod_diffusion_rate
distrib_cog.enable_glial_sync
distrib_cog.glial_sync_interval_ms
```

### Events
```
events.max_queue_size
events.process_batch_size
events.flush_interval_ms
events.enable_rate_limiting
```

### P2P
```
p2p.listen_port
p2p.max_peers
p2p.heartbeat_interval_ms
p2p.reconnect_attempts
```

### Replication
```
replication.backend
replication.sync_interval_ms
replication.heartbeat_interval_ms
replication.max_retry_attempts
```

## 14. Checklist (Print and Keep Handy)

**Per Function:**
- [ ] Input validation with NULL checks
- [ ] LOG_MODULE_DEBUG at entry
- [ ] LOG_MODULE_ERROR on failures
- [ ] Use nimcp_malloc/free
- [ ] Return async variant if blocking
- [ ] Document in header

**Per Module:**
- [ ] Headers added
- [ ] MODULE_NAME defined
- [ ] load_configuration() implemented
- [ ] Security registration in create()
- [ ] All malloc→nimcp_malloc
- [ ] Logging throughout
- [ ] Tests written
- [ ] Valgrind clean

## 15. Performance Rules

1. **Async First**: Any operation >1ms should be async
2. **Config Once**: Load config at startup, cache values
3. **Log Levels**: DEBUG compiles out in release builds
4. **Memory**: Pool allocations when possible
5. **Validate Early**: Check inputs before heavy work

## 16. Common Mistakes to Avoid

❌ `malloc(size)` → ✅ `nimcp_malloc(size)`
❌ Blocking in `_async()` function
❌ Not destroying futures
❌ Not checking allocation failures
❌ Logging in tight loops
❌ Not completing promises
❌ Double-free (set ptr to NULL after free)
❌ Missing config fallbacks

## 17. Help and Resources

- **Template**: `src/networking/distributed/nimcp_distributed_cognition_refactored.c`
- **Guide**: `NETWORKING_REFACTORING_GUIDE.md`
- **Report**: `NETWORKING_REFACTORING_REPORT.md`
- **Tests**: `test/unit/networking/distributed/test_distributed_cognition_refactored.cpp`

---

**Keep this card visible while coding!**
