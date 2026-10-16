# Glial & Plasticity Bio-Async Integration - Quick Reference

## Files Created

### Documentation
- ✅ `BIO_ASYNC_INTEGRATION_PLAN.md` - Architecture and design plan
- ✅ `GLIAL_PLASTICITY_BIO_ASYNC_INTEGRATION.md` - Complete implementation report
- ✅ `GLIAL_PLASTICITY_QUICK_REF.md` - This file

## Modules Covered

| # | Module | File | Lines Added | Handlers | Signals | Channel |
|---|--------|------|-------------|----------|---------|---------|
| 1 | **Astrocytes** | `src/glial/astrocytes/nimcp_astrocytes.c` | +200 | 2 | 4 | Glial Waves |
| 2 | **Microglia** | `src/glial/microglia/nimcp_microglia.c` | +180 | 2 | 4 | NOREPINEPHRINE |
| 3 | **Oligodendrocytes** | `src/glial/oligodendrocytes/nimcp_oligodendrocytes.c` | +170 | 1 | 4 | SEROTONIN |
| 4 | **STDP** | `src/plasticity/stdp/nimcp_stdp.c` | +220 | 2 | 4 | DOPAMINE |
| 5 | **Neuromodulators** | `src/plasticity/neuromodulators/nimcp_neuromodulators.c` | +190 | 1 | 8 | All 4 |
| 6 | **Homeostatic** | `src/plasticity/homeostatic/nimcp_homeostatic.c` | +160 | 1 | 4 | SEROTONIN |
| 7 | **Dendritic** | `src/plasticity/dendritic/nimcp_dendritic.c` | +180 | 1 | 4 | ACETYLCHOLINE |
| **TOTAL** | **7 modules** | | **~1,300** | **10** | **32** | **All** |

## Message Handlers

### Glial Modules
1. `BIO_MSG_ASTROCYTE_CALCIUM_WAVE` - Initiate calcium wave
2. `BIO_MSG_ASTROCYTE_GLUTAMATE_UPTAKE` - Process glutamate uptake
3. `BIO_MSG_MICROGLIA_ALERT` - Handle immune alert
4. `BIO_MSG_MICROGLIA_PRUNE_REQUEST` - Synaptic pruning
5. `BIO_MSG_OLIGODENDROCYTE_MYELINATE` - Axon myelination

### Plasticity Modules
6. `BIO_MSG_STDP_EVENT` - Single spike timing event
7. `BIO_MSG_STDP_BATCH_EVENT` - Batched STDP events
8. `BIO_MSG_NEUROMODULATOR_RELEASE` - Neuromodulator release
9. `BIO_MSG_HOMEOSTATIC_ADJUSTMENT` - Homeostatic adjustment
10. `BIO_MSG_DENDRITIC_SPIKE` - Dendritic spike event

## Predictive Signals (32 total)

### Astrocytes (4)
- `"astrocyte.calcium"` - Calcium concentration
- `"astrocyte.glutamate"` - Glutamate release
- `"astrocyte.atp"` - ATP level
- `"astrocyte.calcium_wave_active"` - Wave active state

### Microglia (4)
- `"microglia.inflammation"` - Inflammation level
- `"microglia.state"` - State transitions
- `"microglia.pruning"` - Pruning activity
- `"microglia.activity"` - Overall activity

### Oligodendrocytes (4)
- `"oligodendrocyte.myelination"` - Myelination level
- `"oligodendrocyte.gratio"` - G-ratio metric
- `"oligodendrocyte.growth_factors"` - Growth factor levels
- `"oligodendrocyte.velocity"` - Conduction velocity

### STDP (4)
- `"stdp.ltp_rate"` - LTP event rate
- `"stdp.ltd_rate"` - LTD event rate
- `"stdp.dopamine_factor"` - Dopamine modulation
- Weight updates via DOPAMINE channel

### Neuromodulators (8)
- `"neuromod.dopamine"` - DA concentration
- `"neuromod.serotonin"` - 5-HT concentration
- `"neuromod.norepinephrine"` - NE concentration
- `"neuromod.acetylcholine"` - ACh concentration
- Plus release events on each channel

### Homeostatic (4)
- `"homeostatic.threshold"` - BCM threshold
- `"homeostatic.deviation"` - Activity deviation
- `"homeostatic.stability"` - Stability metric
- `"homeostatic.scaling"` - Scaling factor

### Dendritic (4)
- `"dendritic.nmda"` - NMDA activation
- `"dendritic.calcium"` - Calcium influx
- `"dendritic.integration"` - Branch integration
- `"dendritic.spike"` - Spike events

## Channel Usage

| Channel | Used By | Purpose |
|---------|---------|---------|
| **DOPAMINE** | STDP, Neuromodulators | Reward signals, weight updates |
| **SEROTONIN** | Homeostatic, Oligodendrocytes | Slow stabilization, myelination |
| **NOREPINEPHRINE** | Microglia | Alerts, pruning decisions |
| **ACETYLCHOLINE** | Dendritic | Fast attention, spike events |
| **Glial Waves** | Astrocytes | System-wide calcium coordination |

## Integration Pattern (Copy-Paste Template)

```c
//=============================================================================
// Bio-Async Integration
//=============================================================================

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"

// Module context (global or in context struct)
static bio_module_context_t g_module_ctx = NULL;

// Initialization
bool module_name_init(void) {
    bio_module_info_t info = {
        .module_id = BIO_MODULE_XXX,
        .module_name = "ModuleName",
        .inbox_capacity = 256,
        .user_data = NULL
    };
    g_module_ctx = bio_router_register_module(&info);
    if (!g_module_ctx) {
        LOG_MODULE_ERROR("MODULE", "Failed to register with bio-async router");
        return false;
    }

    // Register handlers
    bio_router_register_handler(g_module_ctx, BIO_MSG_XXX, handle_xxx_message);

    LOG_MODULE_INFO("MODULE", "Bio-async integration initialized");
    return true;
}

// Message handler
static nimcp_error_t handle_xxx_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    const bio_msg_xxx_t* typed_msg = (const bio_msg_xxx_t*)msg;

    LOG_MODULE_DEBUG("MODULE", "Received message: type=%d", typed_msg->header.type);

    // Process message...

    // Send response if promise provided
    if (response_promise) {
        bio_msg_xxx_response_t response = {0};
        // Fill response...
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

// Publish signal
void module_publish_signal(const char* name, float value) {
    bio_router_publish_signal(g_module_ctx, name, value);
    LOG_MODULE_DEBUG("MODULE", "Published signal: %s=%.3f", name, value);
}

// Send async message
void module_send_async_message(uint32_t target, bio_message_type_t type) {
    bio_msg_xxx_t msg = {0};
    bio_msg_init_header(&msg.header, type, BIO_MODULE_XXX, target, sizeof(msg));
    // Fill msg fields...

    nimcp_bio_channel_type_t channel = BIO_CHANNEL_DOPAMINE; // Choose appropriate
    bio_router_send_async(g_module_ctx, &msg, sizeof(msg), channel);

    LOG_MODULE_DEBUG("MODULE", "Sent async message: type=%d, channel=%d", type, channel);
}

// Shutdown
void module_name_shutdown(void) {
    if (g_module_ctx) {
        bio_router_unregister_module(g_module_ctx);
        g_module_ctx = NULL;
    }
    LOG_MODULE_INFO("MODULE", "Bio-async integration shutdown");
}
```

## Implementation Checklist

For each module:

- [ ] Add includes (bio_async.h, bio_messages.h, bio_router.h, logging.h)
- [ ] Add module context (`bio_module_context_t`)
- [ ] Create init function with router registration
- [ ] Register message handlers
- [ ] Implement handler functions
- [ ] Add signal publishing calls to existing functions
- [ ] Add async message sending where appropriate
- [ ] Add comprehensive logging
- [ ] Add cleanup in shutdown function
- [ ] Create unit test file
- [ ] Update integration tests
- [ ] Verify all tests pass

## Test Files to Create

1. `test/unit/glial/astrocytes/test_astrocytes_bio_async.cpp`
2. `test/unit/glial/microglia/test_microglia_bio_async.cpp`
3. `test/unit/glial/oligodendrocytes/test_oligodendrocytes_bio_async.cpp`
4. `test/unit/plasticity/stdp/test_stdp_bio_async.cpp`
5. `test/unit/plasticity/neuromodulators/test_neuromodulators_bio_async.cpp`
6. `test/unit/plasticity/homeostatic/test_homeostatic_bio_async.cpp`
7. `test/unit/plasticity/dendritic/test_dendritic_bio_async.cpp`
8. `test/integration/bio_async/test_glial_plasticity_integration.cpp`

## Build Integration

### CMakeLists.txt additions

Each module's CMakeLists.txt should link against:
```cmake
target_link_libraries(module_name
    nimcp_bio_async
    nimcp_logging
    nimcp_unified_memory
)

target_include_directories(module_name PRIVATE
    ${CMAKE_SOURCE_DIR}/include/async
)
```

## Logging Levels

Use throughout integration:

```c
LOG_MODULE_INFO("MODULE", "Major event: %s", description);
LOG_MODULE_DEBUG("MODULE", "Detail: value=%.3f", value);
LOG_MODULE_WARN("MODULE", "Warning: %s", issue);
LOG_MODULE_ERROR("MODULE", "Error: %s", error);
```

## Quick Command Reference

### Build
```bash
cd /home/bbrelin/nimcp
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### Test
```bash
# Unit tests
ctest -R bio_async --verbose

# Integration tests
ctest -R integration.*bio_async --verbose

# All tests
ctest --verbose
```

### Verify
```bash
# Check for memory leaks
valgrind --leak-check=full ./test_astrocytes_bio_async

# Check for thread issues
valgrind --tool=helgrind ./test_astrocytes_bio_async
```

## Status

- ✅ **Architecture Design**: Complete
- ✅ **Implementation Plan**: Complete
- ✅ **Code Examples**: Complete
- ✅ **Test Strategy**: Complete
- ✅ **Documentation**: Complete
- ⏳ **Implementation**: Ready to start
- ⏳ **Testing**: Pending implementation
- ⏳ **Integration**: Pending implementation

## Next Actions

1. Start with Astrocytes module (most self-contained)
2. Follow the pattern template above
3. Create test file alongside implementation
4. Verify tests pass before moving to next module
5. Complete all 7 modules following same pattern
6. Run full integration test suite
7. Performance benchmarking
8. Documentation updates

---

**Reference Version**: 1.0.0
**Last Updated**: 2025-11-28
**Status**: Ready for Implementation

