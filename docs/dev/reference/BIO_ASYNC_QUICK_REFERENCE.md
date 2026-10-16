# Bio-Async Cognitive Integration - Quick Reference Card

**Quick lookup guide for implementing bio-async in cognitive modules**

---

## Channel Selection Guide

```
FAST QUERIES (50-100μs)     → Acetylcholine
REWARD/LEARNING (100-200μs) → Dopamine
ALERTING (100-200μs)        → Norepinephrine
DELIBERATION (1-10ms)       → Serotonin
BINDING (10-30ms)           → GAMMA oscillations
GLOBAL COORD (100-500ms)    → Glial waves
```

---

## Module Template

### 1. Add Headers
```c
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
```

### 2. Add to Structure
```c
struct your_module_struct {
    // ... existing fields ...
    bio_module_context_t bio_module_ctx;
    // ... existing fields ...
};
```

### 3. Create Message Handler
```c
static nimcp_error_t handle_your_message(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    your_module_t module = (your_module_t)user_data;
    const bio_msg_your_query_t* query = (const bio_msg_your_query_t*)msg;

    NIMCP_LOG_INFO("YourModule: Processing query from module=0x%04x",
                   query->header.source_module);

    // Build response
    bio_msg_your_response_t response = {0};
    bio_msg_init_header(&response.header,
                       BIO_MSG_YOUR_RESPONSE,
                       BIO_MODULE_YOUR_MODULE,
                       query->header.source_module,
                       sizeof(response));

    // TODO: Fill response fields

    // Complete promise
    nimcp_bio_promise_complete(response_promise, &response);

    NIMCP_LOG_INFO("YourModule: Query complete");
    return NIMCP_SUCCESS;
}
```

### 4. Register Module (in create function)
```c
    // Register with bio-async router
    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_YOUR_MODULE,
        .module_name = "your_module",
        .inbox_capacity = 100,
        .user_data = module
    };

    module->bio_module_ctx = bio_router_register_module(&module_info);
    if (module->bio_module_ctx == NULL) {
        NIMCP_LOG_ERROR("YourModule: Failed to register with router");
        // Cleanup and return NULL
    }

    // Register message handlers
    bio_router_register_handler(
        module->bio_module_ctx,
        BIO_MSG_YOUR_QUERY,
        handle_your_message
    );

    NIMCP_LOG_INFO("YourModule: Bio-async integration complete");
```

### 5. Cleanup (in destroy function)
```c
    if (module->bio_module_ctx) {
        bio_router_unregister_module(module->bio_module_ctx);
        module->bio_module_ctx = NULL;
    }
```

---

## Async Query Pattern (Replace Direct Calls)

### OLD CODE (TIGHT COUPLING):
```c
// DON'T DO THIS!
adaptive_network_t network = brain_get_network(brain);
```

### NEW CODE (ASYNC QUERY):
```c
// Create query
bio_msg_brain_state_query_t query = {0};
bio_msg_init_header(&query.header,
                   BIO_MSG_BRAIN_STATE_QUERY,
                   BIO_MODULE_YOUR_MODULE,
                   BIO_MODULE_BRAIN,
                   sizeof(query));
query.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;

// Send request and wait for response
bio_msg_brain_state_response_t response = {0};
nimcp_error_t err = bio_router_request(
    module->bio_module_ctx,
    &query, sizeof(query),
    &response, sizeof(response),
    100  // 100ms timeout
);

if (err != NIMCP_SUCCESS) {
    NIMCP_LOG_ERROR("YourModule: Query failed");
    return;
}

// Use response
uint32_t neuron_count = response.neuron_count;
```

---

## Broadcast Pattern (One-to-Many)

```c
// Create broadcast message
bio_msg_your_event_t event = {0};
bio_msg_init_header(&event.header,
                   BIO_MSG_YOUR_EVENT,
                   BIO_MODULE_YOUR_MODULE,
                   0,  // 0 = broadcast to all
                   sizeof(event));

// Fill event fields
event.event_type = YOUR_EVENT_TYPE;
event.intensity = 0.75f;

// Broadcast
bio_router_broadcast(module->bio_module_ctx, &event, sizeof(event));

NIMCP_LOG_INFO("YourModule: Event broadcast, intensity=%.2f", event.intensity);
```

---

## Glial Wave Pattern (System-Wide Coordination)

```c
// Create calcium wave
bio_msg_astrocyte_wave_t wave = {0};
bio_msg_init_header(&wave.header,
                   BIO_MSG_ASTROCYTE_CALCIUM_WAVE,
                   BIO_MODULE_YOUR_MODULE,
                   0,  // Broadcast
                   sizeof(wave));

wave.source_region = your_region_id;
wave.initial_calcium_um = 2.0f;  // High calcium = important
wave.propagation_speed_um_s = 50.0f;  // Moderate speed
wave.mode = BIO_WAVE_NETWORK;  // Along network topology

// Broadcast wave
bio_router_broadcast(module->bio_module_ctx, &wave, sizeof(wave));

NIMCP_LOG_INFO("YourModule: Glial wave initiated, calcium=%.1fμM",
               wave.initial_calcium_um);
```

---

## Phase Sync Pattern (Multi-Module Coordination)

```c
// Create phase sync group for GAMMA binding
nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

// Add modules to sync group
nimcp_phase_sync_add_module(sync, BIO_MODULE_YOUR_MODULE);
nimcp_phase_sync_add_module(sync, BIO_MODULE_OTHER_MODULE);

// Wait for coherence
bool coherent = nimcp_phase_sync_wait_coherent(
    sync,
    0.8f,     // Coherence threshold
    100       // Timeout (ms)
);

if (coherent) {
    NIMCP_LOG_INFO("YourModule: Phase sync achieved");
    // Proceed with synchronized operation
}

// Cleanup
nimcp_phase_sync_destroy(sync);
```

---

## Logging Levels

```c
NIMCP_LOG_ERROR("Critical error: %s", error_msg);   // Always logged
NIMCP_LOG_WARN("Warning: %s", warning_msg);         // Usually logged
NIMCP_LOG_INFO("Operation complete");               // Important events
NIMCP_LOG_DEBUG("Value=%d", val);                   // Verbose debugging
```

---

## Common Message Types

| Message Type | Channel | Use Case |
|-------------|---------|----------|
| BIO_MSG_BRAIN_STATE_QUERY | ACh | Query brain state |
| BIO_MSG_INTROSPECTION_QUERY | ACh | Fast self-queries |
| BIO_MSG_ETHICS_EVALUATION_REQUEST | 5-HT | Ethical reasoning |
| BIO_MSG_SALIENCE_QUERY | NE | "Is this important?" |
| BIO_MSG_MIRROR_NEURON_ACTIVATION | DA | Imitation learning |
| BIO_MSG_ASTROCYTE_CALCIUM_WAVE | Glial | System-wide events |

---

## Error Handling

```c
nimcp_error_t err = bio_router_request(...);

switch (err) {
    case NIMCP_SUCCESS:
        // Success
        break;

    case NIMCP_ERROR_TIMEOUT:
        NIMCP_LOG_WARN("YourModule: Query timeout");
        // Use default/fallback
        break;

    case NIMCP_ERROR_NOT_FOUND:
        NIMCP_LOG_ERROR("YourModule: Module not found");
        // Fatal error
        return false;

    default:
        NIMCP_LOG_ERROR("YourModule: Unknown error %d", err);
        return false;
}
```

---

## Memory Management

**Use unified memory everywhere:**
```c
// DON'T use malloc/free
void* bad = malloc(size);  // ❌ WRONG

// DO use unified memory
void* good = nimcp_unified_alloc(size, NIMCP_MEM_COGNITIVE);  // ✅ CORRECT
nimcp_unified_free(good);
```

**Use NIMCP threading:**
```c
// DON'T use pthread directly
pthread_mutex_t bad_lock;  // ❌ WRONG

// DO use NIMCP platform abstraction
nimcp_mutex_t good_lock;   // ✅ CORRECT
nimcp_mutex_init(&good_lock, NULL);
nimcp_mutex_lock(&good_lock);
nimcp_mutex_unlock(&good_lock);
nimcp_mutex_destroy(&good_lock);
```

---

## CMakeLists.txt Template

```cmake
target_link_libraries(your_module
    PRIVATE
        nimcp_bio_async
        nimcp_bio_messages
        nimcp_bio_router
        nimcp_unified_memory
        nimcp_logging
        nimcp_platform_mutex
)
```

---

## Testing Template

```cpp
#include <gtest/gtest.h>
#include "async/nimcp_bio_router.h"
#include "cognitive/your_module/nimcp_your_module.h"

class YourModuleBioAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-router
        bio_router_init();

        // Create your module
        module = your_module_create(&config);
        ASSERT_NE(module, nullptr);
    }

    void TearDown() override {
        your_module_destroy(module);
        bio_router_shutdown();
    }

    your_module_t module;
    your_module_config_t config = your_module_default_config();
};

TEST_F(YourModuleBioAsyncTest, QueryResponse) {
    // Send query
    bio_msg_your_query_t query = {0};
    // ... fill query ...

    bio_msg_your_response_t response = {0};
    nimcp_error_t err = bio_router_request(
        module->bio_module_ctx,
        &query, sizeof(query),
        &response, sizeof(response),
        1000
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Validate response fields
}
```

---

## Performance Guidelines

| Operation | Target Latency | Notes |
|-----------|---------------|-------|
| ACh query | <100 μs | Fast attention |
| DA broadcast | <200 μs | Reward signal |
| NE alert | <200 μs | Urgency signal |
| 5-HT query | <10 ms | Deliberation |
| GAMMA sync | <30 ms | Binding |
| Glial wave | <500 ms | Global coord |

---

## Common Pitfalls

1. ❌ **Using malloc/free** → ✅ Use `nimcp_unified_alloc/free`
2. ❌ **Using pthread directly** → ✅ Use `nimcp_mutex_t`
3. ❌ **No logging** → ✅ Add comprehensive logging
4. ❌ **Direct brain access** → ✅ Use async queries
5. ❌ **No error handling** → ✅ Check all return codes
6. ❌ **Wrong channel choice** → ✅ Match channel to semantics

---

## Channel Semantics Cheat Sheet

```
ACETYLCHOLINE (ACh)
└── Fast attention queries
└── "What is X?" "Where is Y?"
└── 50-100 μs latency

DOPAMINE (DA)
└── Reward signals, learning events
└── "This was good!" "Imitation success"
└── 100-200 μs latency

NOREPINEPHRINE (NE)
└── Alerting, priority signaling
└── "This is important!" "Pay attention!"
└── 100-200 μs latency

SEROTONIN (5-HT)
└── Slow deliberation, mood
└── "Should I do this?" "Is this ethical?"
└── 1-10 ms latency

GAMMA (30-100 Hz)
└── Fast binding, consciousness
└── Multi-module synchronization
└── 10-30 ms latency

GLIAL WAVES
└── System-wide coordination
└── Sleep, consolidation, global events
└── 100-500 ms latency
```

---

**End of Quick Reference**

For detailed implementation, see: `BIO_ASYNC_COGNITIVE_INTEGRATION_SUMMARY.md`
