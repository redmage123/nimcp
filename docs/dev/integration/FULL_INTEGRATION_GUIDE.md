# Full Bio-Async + Logging + Unified Memory Integration Guide

**Complete Implementation Pattern with Code Examples**

---

## Table of Contents
1. [Quick Reference](#quick-reference)
2. [Step-by-Step Integration](#step-by-step-integration)
3. [Code Examples by Pattern](#code-examples-by-pattern)
4. [Module-Specific Changes](#module-specific-changes)
5. [Testing & Verification](#testing--verification)

---

## Quick Reference

### Essential Includes (Add to EVERY file)
```c
#define LOG_MODULE "MODULE_NAME"         // e.g., "GPU_MULTIGPU", "PLASTICITY_ADAPTIVE"
#define LOG_MODULE_ID 0xXXXX             // Unique hex ID (see ID table below)

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
```

### Module ID Table
```c
// GPU Modules
#define BIO_MODULE_ID_GPU_MULTIGPU      0x0901
#define BIO_MODULE_ID_GPU_SPIKE_EVENT   0x0902
#define BIO_MODULE_ID_GPU_NEURON        0x0903
#define BIO_MODULE_ID_GPU_EXECUTION     0x0904

// Plasticity Modules
#define BIO_MODULE_ID_PLASTICITY_ADAPTIVE    0x0A01
#define BIO_MODULE_ID_PLASTICITY_ATTENTION   0x0A02
#define BIO_MODULE_ID_PLASTICITY_DENDRITIC   0x0A03
#define BIO_MODULE_ID_PLASTICITY_ELIGIBILITY 0x0A04
#define BIO_MODULE_ID_NEUROMOD_RECEPTOR      0x0A05
#define BIO_MODULE_ID_NEUROMOD_VESICLE       0x0A06
#define BIO_MODULE_ID_NEUROMOD_METABOLIC     0x0A07
#define BIO_MODULE_ID_NEUROMOD_PHASIC        0x0A08
#define BIO_MODULE_ID_NEUROMOD_SPATIAL       0x0A09
#define BIO_MODULE_ID_NEUROMOD_MAIN          0x0A0A
#define BIO_MODULE_ID_PLASTICITY_NOISE       0x0A0B
#define BIO_MODULE_ID_PLASTICITY_PREDICTIVE  0x0A0C
```

---

## Step-by-Step Integration

### Step 1: Add Header Includes

**Location:** Top of file, after file header comment

```c
// BEFORE (Original)
#include "module/module.h"
#include <stdlib.h>
#include <string.h>

// AFTER (Integrated)
#define LOG_MODULE "MODULE_NAME"
#define LOG_MODULE_ID 0xXXXX

#include "module/module.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include <stdlib.h>
#include <string.h>
```

### Step 2: Replace Memory Allocations

**Find and replace ALL instances:**

```bash
# Pattern 1: malloc()
FIND:    malloc(
REPLACE: nimcp_malloc(

# Pattern 2: calloc()
FIND:    calloc(
REPLACE: nimcp_calloc(

# Pattern 3: free()
FIND:    free(
REPLACE: nimcp_free(

# Pattern 4: aligned_alloc() (if present)
FIND:    aligned_alloc(
REPLACE: nimcp_aligned_alloc(
```

**Example transformation:**
```c
// BEFORE
void* buffer = malloc(1024);
float* data = calloc(n, sizeof(float));
free(buffer);

// AFTER
void* buffer = nimcp_malloc(1024);
float* data = nimcp_calloc(n, sizeof(float));
nimcp_free(buffer);
```

### Step 3: Add bio_module_context_t to Structures

**For modules with persistent context:**

```c
// BEFORE
struct module_context_struct {
    config_t config;
    state_t state;
    void* data;
};

// AFTER
struct module_context_struct {
    config_t config;
    state_t state;
    void* data;

    // Bio-async integration
    bio_module_context_t bio_ctx;
};
```

### Step 4: Initialize Bio-Async in Create Function

```c
// BEFORE
module_context_t* module_create(const config_t* config) {
    module_context_t* ctx = nimcp_calloc(1, sizeof(module_context_t));
    if (!ctx) return NULL;

    ctx->config = *config;
    // ... more initialization ...

    return ctx;
}

// AFTER
module_context_t* module_create(const config_t* config) {
    // Guard: NULL check
    if (!config) {
        LOG_ERROR("NULL config in module_create");
        return NULL;
    }

    LOG_DEBUG("Creating module context");

    module_context_t* ctx = nimcp_calloc(1, sizeof(module_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate module context");
        return NULL;
    }

    ctx->config = *config;

    // Initialize bio-async
    ctx->bio_ctx = bio_module_context_create("MODULE_NAME", BIO_MODULE_ID_XXX);
    if (!ctx->bio_ctx) {
        LOG_WARN("Bio-async initialization failed, continuing without");
    }

    // ... more initialization ...

    LOG_INFO("Module created: param1=%u, param2=%f",
             config->param1, config->param2);

    // Publish creation event
    if (ctx->bio_ctx) {
        bio_message_t msg = bio_message_create_event(
            BIO_EVENT_MODULE_INIT, NULL, 0
        );
        bio_module_send(ctx->bio_ctx, msg);
    }

    return ctx;
}
```

### Step 5: Cleanup Bio-Async in Destroy Function

```c
// BEFORE
void module_destroy(module_context_t* ctx) {
    if (!ctx) return;

    nimcp_free(ctx->data);
    nimcp_free(ctx);
}

// AFTER
void module_destroy(module_context_t* ctx) {
    if (!ctx) {
        LOG_WARN("Attempted to destroy NULL context");
        return;
    }

    LOG_DEBUG("Destroying module context");

    // Publish destruction event
    if (ctx->bio_ctx) {
        bio_message_t msg = bio_message_create_event(
            BIO_EVENT_MODULE_DESTROY, NULL, 0
        );
        bio_module_send(ctx->bio_ctx, msg);

        // Cleanup bio-async
        bio_module_context_destroy(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
    }

    // Cleanup resources
    nimcp_free(ctx->data);
    nimcp_free(ctx);

    LOG_INFO("Module destroyed");
}
```

### Step 6: Add Comprehensive Logging

**Logging levels and when to use them:**

| Level | Use Case | Example |
|-------|----------|---------|
| `LOG_ERROR` | Allocation failures, invalid params | `LOG_ERROR("Failed to allocate buffer: size=%zu", size)` |
| `LOG_WARN` | Recoverable issues, unexpected states | `LOG_WARN("Parameter out of range: value=%f, clamping to %f", val, max)` |
| `LOG_INFO` | Key milestones, state changes | `LOG_INFO("Network created: neurons=%u, layers=%u", n, l)` |
| `LOG_DEBUG` | Function entry/exit, intermediate values | `LOG_DEBUG("Processing batch: batch_id=%u", id)` |
| `LOG_TRACE` | Detailed iteration info, hot paths | `LOG_TRACE("Iteration %llu: loss=%.6f", iter, loss)` |

**Example: Comprehensive logging in update function**
```c
// BEFORE
void module_update(module_context_t* ctx, float dt) {
    if (!ctx) return;

    for (uint32_t i = 0; i < ctx->count; i++) {
        ctx->values[i] += dt;
    }
}

// AFTER
void module_update(module_context_t* ctx, float dt) {
    if (!ctx) {
        LOG_ERROR("NULL context in module_update");
        return;
    }

    if (dt <= 0.0f) {
        LOG_WARN("Invalid dt=%f, skipping update", dt);
        return;
    }

    LOG_DEBUG("Updating module: count=%u, dt=%f", ctx->count, dt);

    uint32_t processed = 0;
    for (uint32_t i = 0; i < ctx->count; i++) {
        ctx->values[i] += dt;
        processed++;

        // Trace logging every 100 iterations (avoid spam)
        if (processed % 100 == 0) {
            LOG_TRACE("Processed %u/%u values", processed, ctx->count);
        }
    }

    LOG_DEBUG("Module update complete: processed=%u", processed);

    // Publish update event
    if (ctx->bio_ctx) {
        bio_message_t msg = bio_message_create_custom(
            BIO_MSG_MODULE_UPDATE,
            &processed, sizeof(processed)
        );
        bio_module_send(ctx->bio_ctx, msg);
    }
}
```

### Step 7: Replace Existing Logging (if present)

**Many plasticity modules use `NIMCP_LOGGING_*` macros - standardize to `LOG_*`:**

```c
// BEFORE
NIMCP_LOGGING_ERROR("Failed to allocate: %zu", size);
NIMCP_LOGGING_INFO("Created network: %u neurons", n);
NIMCP_LOGGING_DEBUG("Processing...");

// AFTER
LOG_ERROR("Failed to allocate: size=%zu", size);
LOG_INFO("Created network: neurons=%u", n);
LOG_DEBUG("Processing...");
```

---

## Code Examples by Pattern

### Pattern 1: GPU Modules (State Machine + Performance Events)

**Example: nimcp_multigpu.c**

```c
#define LOG_MODULE "GPU_MULTIGPU"
#define LOG_MODULE_ID 0x0901

// In multigpu_context_struct:
bio_module_context_t bio_ctx;

// Device enumeration with logging:
bool multigpu_enumerate_devices(...) {
    LOG_DEBUG("Enumerating GPU devices: max_devices=%u", max_devices);

    #if NIMCP_MULTIGPU_CUDA_AVAILABLE
        int device_count = 0;
        cudaError_t err = cudaGetDeviceCount(&device_count);

        if (err != cudaSuccess) {
            LOG_ERROR("CUDA enumeration failed: %s", cudaGetErrorString(err));
            *count = 0;
            return false;
        }

        LOG_INFO("Found %d CUDA devices", device_count);
    #else
        LOG_INFO("Using mock GPU backend (4 simulated devices)");
        uint32_t mock_count = 4;
    #endif

    *count = actual_count;

    // Publish GPU enumeration event
    if (ctx->bio_ctx) {
        gpu_device_info_t info = { .count = actual_count };
        bio_message_t msg = bio_message_create_custom(
            BIO_MSG_GPU_DEVICES_ENUMERATED,
            &info, sizeof(info)
        );
        bio_module_send(ctx->bio_ctx, msg);
    }

    LOG_INFO("GPU enumeration complete: count=%u", *count);
    return true;
}

// Partition network with bio-async events:
bool multigpu_partition_network(multigpu_context_t ctx, ...) {
    LOG_INFO("Partitioning network: num_layers=%u, num_devices=%u",
             num_layers, ctx->num_devices);

    // ... partitioning logic ...

    if (success && ctx->bio_ctx) {
        partition_info_t info = {
            .num_layers = num_layers,
            .num_devices = ctx->num_devices,
            .strategy = ctx->config.partition_strategy
        };
        bio_message_t msg = bio_message_create_custom(
            BIO_MSG_GPU_PARTITION_COMPLETE,
            &info, sizeof(info)
        );
        bio_module_send(ctx->bio_ctx, msg);
    }

    LOG_INFO("Network partitioning %s", success ? "succeeded" : "failed");
    return success;
}
```

### Pattern 2: Plasticity Modules (Learning Events)

**Example: nimcp_adaptive.c**

```c
#define LOG_MODULE "PLASTICITY_ADAPTIVE"
#define LOG_MODULE_ID 0x0A01

// Network training with comprehensive logging and events:
float adaptive_network_train(adaptive_network_t* net, ...) {
    LOG_DEBUG("Training network: epoch=%u, learning_rate=%f",
              epoch, net->config.learning_rate);

    float total_loss = 0.0f;
    uint32_t num_samples = 0;

    for (uint32_t i = 0; i < batch_size; i++) {
        // Forward pass
        adaptive_network_forward(net, inputs[i], outputs);

        // Compute loss
        float loss = compute_loss(outputs, targets[i]);
        total_loss += loss;
        num_samples++;

        LOG_TRACE("Sample %u: loss=%.6f", i, loss);

        // Backward pass
        adaptive_network_backward(net, targets[i]);

        // Publish learning event every 10 samples
        if (num_samples % 10 == 0 && net->bio_ctx) {
            learning_event_t event = {
                .epoch = epoch,
                .sample = i,
                .loss = loss,
                .learning_rate = net->config.learning_rate
            };
            bio_message_t msg = bio_message_create_learning_update(&event);
            bio_module_send(net->bio_ctx, msg);
        }
    }

    float avg_loss = total_loss / (float)num_samples;

    LOG_INFO("Training epoch complete: epoch=%u, avg_loss=%.6f, samples=%u",
             epoch, avg_loss, num_samples);

    // Publish epoch completion
    if (net->bio_ctx) {
        epoch_complete_t event = {
            .epoch = epoch,
            .avg_loss = avg_loss,
            .num_samples = num_samples
        };
        bio_message_t msg = bio_message_create_custom(
            BIO_MSG_TRAINING_EPOCH_COMPLETE,
            &event, sizeof(event)
        );
        bio_module_send(net->bio_ctx, msg);
    }

    return avg_loss;
}
```

### Pattern 3: Neuromodulator Modules (Release Events)

**Example: nimcp_neuromodulators.c**

```c
#define LOG_MODULE "NEUROMOD_MAIN"
#define LOG_MODULE_ID 0x0A0A

// Dopamine release with bio-async notification:
void neuromod_release_dopamine(neuromod_context_t* ctx,
                                float concentration,
                                location_t location) {
    if (!ctx) {
        LOG_ERROR("NULL context in neuromod_release_dopamine");
        return;
    }

    if (concentration < 0.0f || concentration > 1.0f) {
        LOG_WARN("Dopamine concentration out of range: %f, clamping", concentration);
        concentration = fmaxf(0.0f, fminf(1.0f, concentration));
    }

    LOG_DEBUG("Releasing dopamine: concentration=%f, location=%u",
              concentration, location);

    // Update internal state
    ctx->dopamine_level += concentration;
    ctx->last_release_time = current_time();

    LOG_TRACE("Updated dopamine level: %f", ctx->dopamine_level);

    // Publish neuromodulator release event
    if (ctx->bio_ctx) {
        neuromod_release_t release = {
            .type = NEUROMOD_DOPAMINE,
            .concentration = concentration,
            .location = location,
            .timestamp = ctx->last_release_time
        };
        bio_message_t msg = bio_message_create_neuromod_release(&release);
        bio_module_send(ctx->bio_ctx, msg);

        LOG_DEBUG("Published dopamine release event");
    }

    LOG_INFO("Dopamine released: concentration=%f, total_level=%f",
             concentration, ctx->dopamine_level);
}
```

---

## Module-Specific Changes

### GPU Module: nimcp_spike_event.c

**Current status:** Unified memory ✅, Logging ❌, Bio-async ❌

**Changes needed:**
```c
#define LOG_MODULE "GPU_SPIKE_EVENT"
#define LOG_MODULE_ID 0x0902

// Add logging to spike train creation:
spike_train_t* spike_train_create(uint32_t capacity) {
    if (capacity == 0 || capacity > 1000000) {
        LOG_ERROR("Invalid capacity in spike_train_create: %u", capacity);
        return NULL;
    }

    LOG_DEBUG("Creating spike train: capacity=%u", capacity);

    spike_train_t* train = nimcp_calloc(1, sizeof(spike_train_t));
    if (!train) {
        LOG_ERROR("Failed to allocate spike train");
        return NULL;
    }

    train->events = nimcp_calloc(capacity, sizeof(spike_event_t));
    if (!train->events) {
        LOG_ERROR("Failed to allocate spike events: capacity=%u", capacity);
        nimcp_free(train);
        return NULL;
    }

    train->capacity = capacity;
    LOG_INFO("Created spike train: capacity=%u", capacity);

    return train;
}

// Add logging to spike addition:
bool spike_train_add(spike_train_t* train, uint64_t timestamp, float amplitude) {
    if (!train) {
        LOG_ERROR("NULL train in spike_train_add");
        return false;
    }

    LOG_TRACE("Adding spike: timestamp=%llu, amplitude=%f", timestamp, amplitude);

    // ... existing logic ...

    LOG_DEBUG("Spike added: count=%u/%u", train->count, train->capacity);
    return true;
}
```

### Plasticity Module: nimcp_eligibility_trace.c

**Current status:** No allocations (stack-only), Logging ❌, Bio-async ❌

**Changes needed:**
```c
#define LOG_MODULE "PLASTICITY_ELIGIBILITY"
#define LOG_MODULE_ID 0x0A04

// NOTE: This module has no malloc/calloc (stack-only), so unified memory not applicable

// Add logging to trace updates:
void eligibility_trace_update(eligibility_trace_t* trace, ...) {
    if (!trace || !config) {
        LOG_ERROR("NULL parameters in eligibility_trace_update");
        return;
    }

    uint64_t delta_t = current_time - trace->last_update;
    LOG_TRACE("Updating trace: delta_t=%llu, spike_contribution=%f",
              delta_t, spike_contribution);

    // ... existing logic ...

    LOG_DEBUG("Trace updated: value=%f", trace->trace);
}

// Add logging to consolidation:
float eligibility_consolidate_on_burst(...) {
    if (!synapse || !trace || !config) {
        LOG_ERROR("NULL parameters in eligibility_consolidate_on_burst");
        return 0.0f;
    }

    if (trace->trace < config->trace_threshold) {
        LOG_TRACE("Skipping negligible trace: %f < %f",
                  trace->trace, config->trace_threshold);
        return 0.0f;
    }

    bool in_burst = eligibility_is_in_burst(phasic_tonic, config);
    LOG_DEBUG("Consolidation: in_burst=%d, trace=%f, reward=%f",
              in_burst, trace->trace, reward);

    // ... existing logic ...

    if (config->burst_triggered_mode && in_burst) {
        LOG_INFO("Burst-triggered consolidation: delta_w=%f", delta_w);
    }

    return delta_w;
}
```

---

## Testing & Verification

### 1. Compile Test
```bash
cd /home/bbrelin/nimcp/build
cmake ..
make -j$(nproc)
```

### 2. Check for Logging Output
```bash
# Run any test that uses integrated modules
./test/unit_module_test

# Verify LOG_ output appears:
# [INFO] [GPU_MULTIGPU] Module created: num_devices=4
# [DEBUG] [PLASTICITY_ADAPTIVE] Training network: epoch=1
# [ERROR] [GPU_SPIKE_EVENT] NULL parameters in spike_train_add
```

### 3. Verify Bio-Async Messages
```bash
# Add debug output to bio_module_send:
LOG_DEBUG("Bio-async message sent: type=%u, size=%zu", msg->type, msg->size);

# Run test and verify messages are published
```

### 4. Memory Leak Check
```bash
valgrind --leak-check=full ./test/unit_module_test

# Verify no leaks from nimcp_malloc/calloc/free
```

---

## Summary Checklist

For each of the 12 modules, verify:

- [ ] `#define LOG_MODULE` and `LOG_MODULE_ID` added
- [ ] Bio-async includes added
- [ ] Unified memory includes added
- [ ] All `malloc/calloc/free` replaced with `nimcp_*` versions
- [ ] `bio_module_context_t` added to context structures
- [ ] Bio-async initialized in create functions
- [ ] Bio-async destroyed in destroy functions
- [ ] `LOG_ERROR` added for all error conditions
- [ ] `LOG_WARN` added for warnings
- [ ] `LOG_INFO` added for major milestones
- [ ] `LOG_DEBUG` added for function entry/state
- [ ] `LOG_TRACE` added for detailed iterations (optional)
- [ ] Bio-async events published at key operations
- [ ] Module compiles without errors
- [ ] Logging output verified
- [ ] Bio-async messages verified
- [ ] No memory leaks detected

---

## Estimated Effort Per Module

| Module | Size | Allocations | Logging Sites | Events | Time Est. |
|--------|------|-------------|---------------|--------|-----------|
| nimcp_multigpu.c | Large | ~20 | ~30 | ~10 | 90 min |
| nimcp_spike_event.c | Small | ~10 | ~15 | ~5 | 30 min |
| nimcp_gpu_neuron.c | Medium | ~15 | ~20 | ~8 | 45 min |
| nimcp_execution_mode.c | ✅ DONE | - | - | - | 0 min |
| nimcp_adaptive.c | X-Large | ~50 | ~80 | ~20 | 3 hours |
| nimcp_attention.c | Medium | ~20 | ~25 | ~10 | 60 min |
| nimcp_dendritic.c | Medium | ~15 | ~20 | ~8 | 45 min |
| nimcp_eligibility_trace.c | Small | 0 | ~15 | ~5 | 30 min |
| neuromod modules (6) | Medium ea. | ~10 ea. | ~20 ea. | ~8 ea. | 45 min ea. |

**Total Estimated Time:** ~10-12 hours for all 12 modules
