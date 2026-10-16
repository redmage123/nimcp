# NIMCP Networking Modules - Complete Refactoring Guide

## Executive Summary

This document provides a comprehensive guide for refactoring all NIMCP networking modules to integrate:
1. **Async Module** - Non-blocking operations using futures/promises
2. **Unified Memory** - Replace malloc/free with nimcp_malloc/nimcp_free
3. **Logging Module** - Comprehensive logging at all levels
4. **Config Module** - Runtime-configurable hyperparameters
5. **Security Module** - Module registration and validation

## Modules to Refactor

| Module | File | Lines | Complexity | Priority |
|--------|------|-------|------------|----------|
| Distributed Cognition | `distributed/nimcp_distributed_cognition.c` | 646 | High | 1 |
| Events | `events/nimcp_events.c` | 421 | Medium | 2 |
| P2P Node | `p2p/nimcp_p2pnode.c` | 1070 | High | 3 |
| Protocol | `protocol/nimcp_protocol.c` | 1301 | Medium | 4 |
| Replication | `replication/nimcp_replication.c` | 1184 | High | 5 |

**Total**: ~3,600 lines of code to refactor

## Refactoring Checklist (Per Module)

### 1. Header Updates
- [ ] Add `#include "async/nimcp_future.h"`
- [ ] Add `#include "security/nimcp_security.h"`
- [ ] Add `#include "utils/config/nimcp_dynamic_config.h"`
- [ ] Add `#include "utils/logging/nimcp_logging.h"`
- [ ] Add `#include "utils/memory/nimcp_memory.h"` (if not already present)
- [ ] Update file documentation to mention refactoring

### 2. Module Configuration
- [ ] Define `#define MODULE_NAME "networking.xxx"`
- [ ] Define configuration key constants (e.g., `#define CFG_KEY_XXX "module.param"`)
- [ ] Create `load_configuration()` function
- [ ] Document all configuration parameters in header comment

### 3. Memory Management
- [ ] Replace ALL `malloc()` → `nimcp_malloc()`
- [ ] Replace ALL `calloc()` → `nimcp_calloc()`
- [ ] Replace ALL `free()` → `nimcp_free()`
- [ ] Replace ALL `realloc()` → `nimcp_realloc()`
- [ ] Replace ALL `strdup()` → `nimcp_strdup()`
- [ ] Search for memory leaks (ensure all allocations have matching frees)

### 4. Async Integration
- [ ] Identify blocking operations (network I/O, disk I/O, sleep)
- [ ] Create `_async()` variants returning `nimcp_future_t`
- [ ] Use `nimcp_promise_create()` → `nimcp_promise_get_future()`
- [ ] Complete promises with `nimcp_promise_complete()` or `nimcp_promise_fail()`
- [ ] Add async message queue for network sends
- [ ] Create sender thread to process queue

### 5. Logging Integration
- [ ] Add `LOG_MODULE_INFO()` at function entry points
- [ ] Add `LOG_MODULE_DEBUG()` for detailed tracing
- [ ] Add `LOG_MODULE_WARN()` for recoverable errors
- [ ] Add `LOG_MODULE_ERROR()` for critical failures
- [ ] Log all configuration values loaded
- [ ] Log all state transitions
- [ ] Log statistics periodically

### 6. Config Integration
- [ ] Create config key definitions for all hyperparameters
- [ ] Implement `load_configuration()` function
- [ ] Call `config_get_int()`, `config_get_float()`, `config_get_bool()`, `config_get_string()`
- [ ] Provide fallback defaults for all parameters
- [ ] Log each loaded config value

### 7. Security Integration
- [ ] Call `security_register_module(MODULE_NAME)` in init function
- [ ] Store returned security module ID
- [ ] Validate all network inputs
- [ ] Validate all user-provided buffers
- [ ] Enforce resource limits from config

### 8. Threading
- [ ] Replace `pthread_t` → `nimcp_thread_t`
- [ ] Replace `pthread_mutex_t` → `nimcp_mutex_t`
- [ ] Replace `pthread_rwlock_t` → `nimcp_rwlock_t`
- [ ] Replace `pthread_cond_t` → `nimcp_cond_t`
- [ ] Use `nimcp_thread_create()`, `nimcp_thread_join()`
- [ ] Use `nimcp_mutex_lock()`, `nimcp_mutex_unlock()`

### 9. Testing
- [ ] Create unit tests in `test/unit/networking/<module>/`
- [ ] Test normal operation
- [ ] Test error conditions
- [ ] Test async operations (wait for futures)
- [ ] Test config loading
- [ ] Test memory leaks (valgrind)
- [ ] Create integration tests in `test/integration/networking/<module>/`
- [ ] Test module interactions
- [ ] Test under load
- [ ] Measure performance

## Configuration Parameters Reference

### Distributed Cognition
```c
// Boolean parameters
distrib_cog.enable_neuromod_sync = true
distrib_cog.enable_glial_sync = true
distrib_cog.enable_region_sync = true

// Integer parameters (ms)
distrib_cog.neuromod_broadcast_interval_ms = 100
distrib_cog.glial_sync_interval_ms = 500
distrib_cog.region_sync_interval_ms = 200
distrib_cog.max_message_queue = 1000
distrib_cog.retry_attempts = 3
distrib_cog.retry_delay_ms = 100

// Float parameters
distrib_cog.neuromod_diffusion_rate = 0.1  // 0.0-1.0
```

### Events
```c
events.max_queue_size = 10000
events.process_batch_size = 100
events.flush_interval_ms = 50
events.enable_filtering = true
events.enable_rate_limiting = true
events.max_events_per_sec = 100000
```

### P2P Node
```c
p2p.listen_port = 7777
p2p.max_peers = 32
p2p.heartbeat_interval_ms = 5000
p2p.peer_timeout_ms = 15000
p2p.reconnect_attempts = 5
p2p.reconnect_delay_ms = 1000
p2p.max_send_queue = 1000
p2p.enable_encryption = true
```

### Protocol
```c
protocol.max_payload_size = 1048576  // 1MB
protocol.protocol_version = 2
protocol.enable_compression = false
protocol.compression_threshold = 4096
protocol.checksum_algorithm = "crc32"
```

### Replication
```c
replication.backend = "filesystem"  // "filesystem", "redis", "postgres"
replication.sync_interval_ms = 5000
replication.heartbeat_interval_ms = 10000
replication.node_timeout_ms = 30000
replication.enable_vector_clock = false
replication.enable_crdt = false
replication.max_retry_attempts = 3
```

## Async Operation Patterns

### Pattern 1: Simple Async Function
```c
// OLD (blocking):
bool send_message(module_t mod, const uint8_t* data, size_t size);

// NEW (async):
nimcp_future_t send_message_async(module_t mod, const uint8_t* data, size_t size)
{
    LOG_MODULE_DEBUG(MODULE_NAME, "Sending message async (size=%zu)", size);

    // Create promise
    nimcp_promise_t promise = nimcp_promise_create(sizeof(bool));
    nimcp_future_t future = nimcp_promise_get_future(promise);

    // Enqueue operation
    enqueue_send_operation(mod, data, size, promise);

    return future;
}
```

### Pattern 2: Async with Callback
```c
nimcp_future_t operation_async(module_t mod, ...)
{
    nimcp_future_t future = perform_async_operation(mod, ...);

    // Chain callback
    nimcp_future_then(future, on_complete_callback, user_data);

    return future;
}

static void on_complete_callback(const void* result, nimcp_error_t error, void* user_data)
{
    if (error == NIMCP_SUCCESS) {
        LOG_MODULE_INFO(MODULE_NAME, "Operation completed successfully");
    } else {
        LOG_MODULE_ERROR(MODULE_NAME, "Operation failed: %d", error);
    }
}
```

### Pattern 3: Wait for Multiple Async Operations
```c
void perform_batch_operations(module_t mod)
{
    nimcp_future_t futures[10];

    // Start 10 async operations
    for (int i = 0; i < 10; i++) {
        futures[i] = operation_async(mod, ...);
    }

    // Wait for all to complete
    nimcp_future_t all = nimcp_future_all(futures, 10);
    nimcp_future_wait(all);

    // Check results
    bool results[10];
    nimcp_future_get(all, results);

    nimcp_future_destroy(all);
    for (int i = 0; i < 10; i++) {
        nimcp_future_destroy(futures[i]);
    }
}
```

## Logging Best Practices

### Log Levels
- **DEBUG**: Detailed tracing (function entry/exit, variable values, state changes)
- **INFO**: Important events (module init/destroy, connection established, significant operations)
- **WARN**: Recoverable errors (retry attempts, degraded performance, config fallbacks)
- **ERROR**: Critical failures (allocation failures, invalid inputs, unrecoverable errors)

### Examples
```c
// Function entry (INFO for public API, DEBUG for internal)
LOG_MODULE_INFO(MODULE_NAME, "Creating module (param1=%d, param2=%s)", p1, p2);

// Configuration loaded (DEBUG)
LOG_MODULE_DEBUG(MODULE_NAME, "Config loaded: max_queue=%d", max_queue);

// State transition (DEBUG)
LOG_MODULE_DEBUG(MODULE_NAME, "State transition: %s -> %s", old_state, new_state);

// Recoverable error (WARN)
LOG_MODULE_WARN(MODULE_NAME, "Send failed, retry %d/%d", retry, max_retries);

// Critical error (ERROR)
LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate buffer (size=%zu)", size);
```

## Memory Management Best Practices

### Rules
1. **Always use unified memory** - `nimcp_malloc/calloc/free`
2. **NULL-check all allocations** - Handle allocation failures gracefully
3. **Free in reverse order** - Last allocated, first freed
4. **Set pointers to NULL after free** - Prevent double-free
5. **Use valgrind** - Check for leaks and memory errors

### Example
```c
typedef struct {
    char* name;
    uint8_t* buffer;
    size_t size;
} resource_t;

resource_t* create_resource(const char* name, size_t size)
{
    LOG_MODULE_DEBUG(MODULE_NAME, "Creating resource (name=%s, size=%zu)", name, size);

    // Allocate structure
    resource_t* res = nimcp_calloc(1, sizeof(resource_t));
    if (!res) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate resource structure");
        return NULL;
    }

    // Allocate name
    res->name = nimcp_strdup(name);
    if (!res->name) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate resource name");
        nimcp_free(res);
        return NULL;
    }

    // Allocate buffer
    res->buffer = nimcp_malloc(size);
    if (!res->buffer) {
        LOG_MODULE_ERROR(MODULE_NAME, "Failed to allocate resource buffer");
        nimcp_free(res->name);
        nimcp_free(res);
        return NULL;
    }

    res->size = size;
    LOG_MODULE_DEBUG(MODULE_NAME, "Resource created successfully");
    return res;
}

void destroy_resource(resource_t* res)
{
    if (!res) {
        LOG_MODULE_DEBUG(MODULE_NAME, "NULL resource (safe no-op)");
        return;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Destroying resource (name=%s)", res->name);

    // Free in reverse order
    nimcp_free(res->buffer);
    nimcp_free(res->name);
    nimcp_free(res);
}
```

## Security Integration

### Module Registration
```c
static uint32_t g_security_module_id = 0;

module_t module_create(...)
{
    // ... allocation ...

    // Register with security system
    g_security_module_id = security_register_module(MODULE_NAME);
    if (g_security_module_id > 0) {
        LOG_MODULE_INFO(MODULE_NAME, "Registered with security (ID=%u)", g_security_module_id);
    } else {
        LOG_MODULE_WARN(MODULE_NAME, "Security registration failed");
    }

    return module;
}
```

### Input Validation
```c
bool process_network_data(module_t mod, const uint8_t* data, size_t size)
{
    // Validate inputs
    if (!mod || !data || size == 0) {
        LOG_MODULE_ERROR(MODULE_NAME, "Invalid input parameters");
        return false;
    }

    // Check size limits (from config)
    size_t max_size = get_max_packet_size(mod);
    if (size > max_size) {
        LOG_MODULE_ERROR(MODULE_NAME, "Packet too large: %zu > %zu", size, max_size);
        return false;
    }

    // Validate data contents
    if (!validate_packet_structure(data, size)) {
        LOG_MODULE_ERROR(MODULE_NAME, "Invalid packet structure");
        return false;
    }

    // Process data
    LOG_MODULE_DEBUG(MODULE_NAME, "Processing packet (size=%zu)", size);
    return true;
}
```

## Testing Strategy

### Unit Test Structure
```cpp
// test/unit/networking/distributed/test_distributed_cognition.cpp

#include <gtest/gtest.h>
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "async/nimcp_future.h"

class DistributedCognitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize modules
        nimcp_future_init(nullptr, nullptr);

        // Create mock P2P node
        p2p_node = create_mock_p2p_node();

        // Create coordinator
        dc = distrib_cognition_create(nullptr, p2p_node);
        ASSERT_NE(dc, nullptr);
    }

    void TearDown() override {
        distrib_cognition_destroy(dc);
        destroy_mock_p2p_node(p2p_node);
        nimcp_future_shutdown();
    }

    distrib_cognition_t dc;
    p2p_node_t p2p_node;
};

TEST_F(DistributedCognitionTest, CreateDestroy) {
    // Already tested in SetUp/TearDown
    SUCCEED();
}

TEST_F(DistributedCognitionTest, BroadcastNeuromodAsync) {
    // Broadcast neuromodulator
    nimcp_future_t future = distrib_cognition_broadcast_neuromod_async(
        dc, NEUROMOD_DOPAMINE, 0.5f);

    ASSERT_NE(future, nullptr);

    // Wait for completion
    bool success = nimcp_future_wait_timeout(future, 1000);
    ASSERT_TRUE(success);

    // Get result
    bool result = false;
    nimcp_error_t err = nimcp_future_get(future, &result);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    ASSERT_TRUE(result);

    nimcp_future_destroy(future);
}

TEST_F(DistributedCognitionTest, ConfigLoading) {
    // Set config values
    config_set_int("distrib_cog.neuromod_broadcast_interval_ms", 200);
    config_set_float("distrib_cog.neuromod_diffusion_rate", 0.25f);

    // Create new coordinator (loads config)
    distrib_cognition_t dc2 = distrib_cognition_create(nullptr, p2p_node);
    ASSERT_NE(dc2, nullptr);

    // Verify config was loaded
    // (Would need accessor functions or test hooks)

    distrib_cognition_destroy(dc2);
}

TEST_F(DistributedCognitionTest, MemoryLeakTest) {
    // Create and destroy many times
    for (int i = 0; i < 1000; i++) {
        distrib_cognition_t temp = distrib_cognition_create(nullptr, p2p_node);
        ASSERT_NE(temp, nullptr);
        distrib_cognition_destroy(temp);
    }

    // Run with valgrind to detect leaks
}
```

### Integration Test Structure
```cpp
// test/integration/networking/test_distributed_cognition_integration.cpp

TEST(DistributedCognitionIntegration, MultiNodeSync) {
    // Create 3 nodes
    p2p_node_t nodes[3];
    distrib_cognition_t coordinators[3];

    for (int i = 0; i < 3; i++) {
        nodes[i] = p2p_node_create(...);
        coordinators[i] = distrib_cognition_create(nullptr, nodes[i]);
    }

    // Connect nodes
    // ... peer discovery ...

    // Broadcast from node 0
    nimcp_future_t future = distrib_cognition_broadcast_neuromod_async(
        coordinators[0], NEUROMOD_DOPAMINE, 0.8f);

    nimcp_future_wait(future);

    // Wait for propagation
    sleep(1);

    // Verify node 1 and 2 received update
    // (Would need test hooks to check)

    // Cleanup
    for (int i = 0; i < 3; i++) {
        distrib_cognition_destroy(coordinators[i]);
        p2p_node_destroy(nodes[i]);
    }
}
```

## Migration Path

### Phase 1: Foundation (Week 1)
1. Refactor `nimcp_protocol.c` (already uses good patterns)
2. Refactor `nimcp_events.c` (smallest module)
3. Create test infrastructure

### Phase 2: Core Networking (Week 2)
4. Refactor `nimcp_p2pnode.c` (critical path)
5. Refactor `nimcp_replication.c` (uses p2pnode)
6. Integration testing

### Phase 3: High-Level (Week 3)
7. Refactor `nimcp_distributed_cognition.c` (uses all modules)
8. Full system integration testing
9. Performance testing and optimization

## Performance Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| Future create+complete | < 1μs | Microbenchmark |
| Config lookup | < 100ns | Cached after first load |
| Log entry (DEBUG disabled) | < 50ns | Should compile out |
| Log entry (DEBUG enabled) | < 5μs | Buffered async write |
| Async send latency | < 100μs | Enqueue only |
| Memory overhead per future | < 128 bytes | Profile with massif |

## Validation Checklist

Before considering a module "done":

- [ ] All malloc/free replaced with unified memory
- [ ] All blocking operations have async variants
- [ ] All config parameters documented and loaded
- [ ] Security module registered
- [ ] Comprehensive logging at all levels
- [ ] Unit tests achieve >90% code coverage
- [ ] Integration tests pass
- [ ] No memory leaks (valgrind clean)
- [ ] Performance targets met
- [ ] Code review completed
- [ ] Documentation updated

## Common Pitfalls

1. **Forgetting to destroy futures** - Always call `nimcp_future_destroy()`
2. **Not checking allocation failures** - Every malloc can fail
3. **Logging in hot paths** - Use DEBUG level, will be compiled out
4. **Not loading config** - Must call `load_configuration()`
5. **Double-free errors** - Set pointers to NULL after free
6. **Thread safety** - Use proper locking for shared state
7. **Blocking in async functions** - Defeats the purpose
8. **Not handling promise completion** - Complete or fail, never leave pending

## Reference Implementation

See `nimcp_distributed_cognition_refactored.c` for a complete example showing:
- Async operations with futures
- Config module integration
- Logging throughout
- Security registration
- Unified memory usage
- Proper error handling

Apply this pattern to all remaining modules.

## Questions/Issues

Contact: NIMCP Development Team
Documentation: https://github.com/nimcp/nimcp/wiki/Networking-Refactoring
