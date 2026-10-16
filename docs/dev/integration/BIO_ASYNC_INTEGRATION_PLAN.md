# Bio-Async Integration Implementation Plan

## Overview
Integrating bio-async messaging into NIMCP glial and plasticity modules for event-driven, biologically-inspired inter-module communication.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Bio-Async Router                           │
│  (Central message dispatch with neuromodulator channels)       │
└────────────┬────────────────────────────────────┬──────────────┘
             │                                    │
    ┌────────▼─────────┐              ┌──────────▼──────────┐
    │ GLIAL MODULES    │              │ PLASTICITY MODULES  │
    ├──────────────────┤              ├─────────────────────┤
    │ • Astrocytes     │              │ • STDP              │
    │ • Microglia      │              │ • Neuromodulators   │
    │ • Oligodendrocyt │              │ • Homeostatic       │
    └──────────────────┘              │ • Dendritic         │
                                      └─────────────────────┘
```

## Module-by-Module Integration

### 1. ASTROCYTES (`src/glial/astrocytes/nimcp_astrocytes.c`)

**Messages to Handle:**
- `BIO_MSG_ASTROCYTE_CALCIUM_WAVE` - Initiate calcium wave via glial wave API
- `BIO_MSG_ASTROCYTE_GLUTAMATE_UPTAKE` - Process glutamate uptake request

**Messages to Publish:**
- Calcium wave initiation → Glial wave channel (slow coordination)
- Glutamate release events → Predictive signal
- ATP level changes → Metabolic demand messages

**Implementation:**
1. Add bio-async includes
2. Add module registration in initialization
3. Add message handler for calcium wave requests
4. Replace direct calcium wave propagation with `nimcp_glial_wave_initiate()`
5. Publish glutamate release via predictive signals
6. Add logging throughout

### 2. MICROGLIA (`src/glial/microglia/nimcp_microglia.c`)

**Messages to Handle:**
- `BIO_MSG_MICROGLIA_ALERT` - Handle alert via NOREPINEPHRINE (alerting)
- `BIO_MSG_MICROGLIA_PRUNE_REQUEST` - Process pruning request

**Messages to Publish:**
- Pruning decisions → NOREPINEPHRINE channel (alerting)
- State transitions → Predictive signal
- Inflammation level changes → Predictive signal

**Implementation:**
1. Add bio-async includes
2. Add module context registration
3. Add alert message handler
4. Add pruning request handler
5. Publish pruning events asynchronously
6. Add logging

### 3. OLIGODENDROCYTES (`src/glial/oligodendrocytes/nimcp_oligodendrocytes.c`)

**Messages to Handle:**
- `BIO_MSG_OLIGODENDROCYTE_MYELINATE` - Process myelination request

**Messages to Publish:**
- Myelination progress → Predictive signal (SEROTONIN - slow, stabilizing)
- Growth factor level changes → Predictive signal

**Implementation:**
1. Add bio-async includes
2. Add module registration
3. Add myelination message handler
4. Publish myelination progress updates
5. Add logging

### 4. STDP (`src/plasticity/stdp/nimcp_stdp.c`)

**Messages to Handle:**
- `BIO_MSG_STDP_EVENT` - Single STDP spike timing event
- `BIO_MSG_STDP_BATCH_EVENT` - Batched STDP events for efficiency

**Messages to Publish:**
- Weight changes → DOPAMINE channel (reward signal)
- `BIO_MSG_WEIGHT_UPDATE_RESPONSE` - Response to weight update requests

**Implementation:**
1. Add bio-async includes (already has some logging/security)
2. Add bio-async module registration
3. Add STDP event handler
4. Add batch event handler
5. Publish weight updates via DOPAMINE channel
6. Send weight update responses

### 5. NEUROMODULATORS (`src/plasticity/neuromodulators/nimcp_neuromodulators.c`)

**Messages to Handle:**
- `BIO_MSG_NEUROMODULATOR_RELEASE` - Process release request

**Messages to Publish:**
- Concentration changes → Predictive signals
- Release events → Appropriate neuromodulator channels

**Implementation:**
1. Add bio-async includes
2. Add module registration
3. Add release message handler
4. Publish concentration changes via predictive coding
5. Use bio-async neuromodulator channels for signaling

### 6. HOMEOSTATIC (`src/plasticity/homeostatic/nimcp_homeostatic.c`)

**Messages to Handle:**
- `BIO_MSG_HOMEOSTATIC_ADJUSTMENT` - Process adjustment request

**Messages to Publish:**
- Scaling factor changes → SEROTONIN (slow, stabilizing)
- Threshold updates → Predictive signal

**Implementation:**
1. Add bio-async includes
2. Add module registration
3. Add adjustment message handler
4. Publish adjustments via SEROTONIN channel
5. Add logging

### 7. DENDRITIC (`src/plasticity/dendritic/nimcp_dendritic.c`)

**Messages to Handle:**
- `BIO_MSG_DENDRITIC_SPIKE` - Process dendritic spike event

**Messages to Publish:**
- Dendritic spikes → ACETYLCHOLINE (fast attention)
- NMDA activation → Predictive signal
- Calcium influx → Predictive signal

**Implementation:**
1. Add bio-async includes
2. Add module registration
3. Add dendritic spike handler
4. Publish spike events via ACETYLCHOLINE channel
5. Add logging

## Common Patterns

### Module Initialization
```c
// Add to each module's init function
static bio_module_context_t g_module_ctx = NULL;

bool module_init(void) {
    // ... existing init ...

    // Register with bio-async router
    bio_module_info_t info = {
        .module_id = BIO_MODULE_[NAME],
        .module_name = "module_name",
        .inbox_capacity = 256,
        .user_data = NULL
    };
    g_module_ctx = bio_router_register_module(&info);
    if (!g_module_ctx) {
        LOG_MODULE_ERROR("MODULE", "Failed to register with bio-async router");
        return false;
    }

    // Register message handlers
    bio_router_register_handler(g_module_ctx, BIO_MSG_TYPE, handler_function);

    LOG_MODULE_INFO("MODULE", "Bio-async integration initialized");
    return true;
}
```

### Message Handler Pattern
```c
static nimcp_error_t handle_message(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    const bio_msg_type_t* typed_msg = (const bio_msg_type_t*)msg;

    LOG_MODULE_DEBUG("MODULE", "Received message type=%d", typed_msg->header.type);

    // Process message
    // ...

    // Complete response promise if provided
    if (response_promise) {
        result_type_t result = { /* ... */ };
        nimcp_bio_promise_complete(response_promise, &result);
    }

    return NIMCP_SUCCESS;
}
```

### Publishing Pattern
```c
// Publish predictive signal
bio_router_publish_signal(g_module_ctx, "signal_name", value);

// Send async message
bio_msg_type_t msg;
bio_msg_init_header(&msg.header, BIO_MSG_TYPE, BIO_MODULE_[SOURCE],
                     BIO_MODULE_[TARGET], sizeof(msg));
msg.field = value;
bio_router_send_async(g_module_ctx, &msg, sizeof(msg), BIO_CHANNEL_DOPAMINE);
```

## Testing Strategy

### Unit Tests (test/unit/)
- Test message handler registration
- Test message sending/receiving
- Test predictive signal publishing
- Test module context management

### Integration Tests (test/integration/)
- Test cross-module communication
- Test bio-async router integration
- Test neuromodulator channel usage
- Test glial wave propagation

### Regression Tests (test/regression/)
- Ensure existing functionality preserved
- Test performance under async messaging
- Test message delivery guarantees
- Test cleanup and shutdown

## Logging Strategy

All modules will use NIMCP logging with:
- `LOG_MODULE_INFO` - Module initialization, major events
- `LOG_MODULE_DEBUG` - Message receipt, processing details
- `LOG_MODULE_WARN` - Non-critical issues
- `LOG_MODULE_ERROR` - Critical failures

## Memory Strategy

All modules will use unified memory (`nimcp_unified_memory.h`):
- NO malloc/free
- Use `unified_mem_create()`, `unified_mem_acquire()`, etc.
- Automatic CoW strategy selection

## Thread Safety

All modules use NIMCP platform threading:
- `nimcp_platform_mutex.h` for mutexes
- `nimcp_platform_rwlock.h` for read-write locks
- `nimcp_platform_thread.h` for threads
- NO raw pthread calls

## Files to Modify

1. `src/glial/astrocytes/nimcp_astrocytes.c`
2. `src/glial/microglia/nimcp_microglia.c`
3. `src/glial/oligodendrocytes/nimcp_oligodendrocytes.c`
4. `src/plasticity/stdp/nimcp_stdp.c`
5. `src/plasticity/neuromodulators/nimcp_neuromodulators.c`
6. `src/plasticity/homeostatic/nimcp_homeostatic.c`
7. `src/plasticity/dendritic/nimcp_dendritic.c`

## Files to Create

### Tests
1. `test/unit/glial/astrocytes/test_astrocytes_bio_async.cpp`
2. `test/unit/glial/microglia/test_microglia_bio_async.cpp`
3. `test/unit/glial/oligodendrocytes/test_oligodendrocytes_bio_async.cpp`
4. `test/unit/plasticity/stdp/test_stdp_bio_async.cpp`
5. `test/unit/plasticity/neuromodulators/test_neuromodulators_bio_async.cpp`
6. `test/unit/plasticity/homeostatic/test_homeostatic_bio_async.cpp`
7. `test/unit/plasticity/dendritic/test_dendritic_bio_async.cpp`
8. `test/integration/bio_async/test_glial_plasticity_integration.cpp`

## Success Criteria

1. All modules register with bio-async router successfully
2. All message handlers invoked correctly
3. All async messages delivered reliably
4. All predictive signals published correctly
5. All tests pass (unit, integration, regression)
6. No memory leaks (valgrind clean)
7. No thread safety issues (helgrind clean)
8. Performance acceptable (< 10% overhead)

## Timeline

Given the comprehensive nature of this integration:
- Implementation: Complete in this session
- All 7 modules updated with bio-async
- All logging added
- All tests created
- Summary report generated

