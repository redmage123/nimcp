# Portia-Training Integration Guide

## Table of Contents
1. [Introduction](#introduction)
2. [Quick Start](#quick-start)
3. [Configuration](#configuration)
4. [Event Handling](#event-handling)
5. [Resource Adaptation](#resource-adaptation)
6. [Bio-Async Integration](#bio-async-integration)
7. [Best Practices](#best-practices)
8. [Troubleshooting](#troubleshooting)

## Introduction

The Portia-Training integration enables NIMCP's training pipeline to automatically adapt to platform resource constraints. Named after the Portia fimbriata spider, which demonstrates remarkable cognitive flexibility despite limited resources, this system allows training to gracefully degrade when resources are constrained and recover when resources become available.

### Key Benefits

- **Automatic Adaptation**: Training parameters adjust based on platform tier
- **Graceful Degradation**: Reduces compute load before OOM kills
- **Resource Awareness**: Prevents system overload during constrained scenarios
- **Seamless Recovery**: Returns to full performance when resources available
- **Zero Configuration**: Works out-of-box with sensible defaults

## Quick Start

### Basic Integration (5 lines of code)

```c
#include "middleware/training/nimcp_brain_training_integration.h"
#include "portia/nimcp_portia.h"

/* 1. Create training context with Portia enabled */
nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
config.enable_portia_integration = true;

nimcp_brain_training_ctx_t* ctx = nimcp_brain_training_create(&config);
nimcp_brain_training_init(ctx, NULL, NULL);

/* 2. Initialize Portia */
portia_init(NULL);  /* NULL = use defaults */

/* 3. Connect Portia to training */
nimcp_brain_training_connect_portia(ctx, portia_get_context());

/* 4. Use adjusted parameters in training loop */
size_t batch_size = nimcp_brain_training_get_adjusted_batch_size(ctx, 128);
float learning_rate = nimcp_brain_training_get_adjusted_lr(ctx, 0.01f);

/* 5. Train normally - adaptation happens automatically! */
train_model(batch_size, learning_rate);
```

### Full Example

```c
#include "middleware/training/nimcp_brain_training_integration.h"
#include "portia/nimcp_portia.h"
#include "utils/logging/nimcp_logging.h"

int main(void) {
    /* Initialize bio-async router (required for event messaging) */
    bio_router_config_t router_config = {0};
    router_config.max_modules = 32;
    router_config.enable_priority_channels = true;

    bio_router_context_t* router = bio_router_init(&router_config);
    if (!router) {
        LOG_ERROR("Failed to initialize bio-router");
        return -1;
    }

    /* Initialize Portia system */
    portia_config_t portia_config = portia_get_default_config();
    portia_config.enable_bio_async = true;
    portia_config.tier_config.enable_auto_switching = true;

    if (portia_init(&portia_config) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize Portia");
        return -1;
    }

    /* Create training context with Portia integration */
    nimcp_brain_training_config_t train_config =
        nimcp_brain_training_default_config();
    train_config.enable_portia_integration = true;
    train_config.min_batch_size_ratio = 0.25f;      /* Min 25% of base batch */
    train_config.allow_training_pause = true;       /* Pause in EMERGENCY */
    train_config.adapt_to_tier_changes = true;      /* Auto-adapt */

    nimcp_brain_training_ctx_t* train_ctx =
        nimcp_brain_training_create(&train_config);
    if (!train_ctx) {
        LOG_ERROR("Failed to create training context");
        return -1;
    }

    nimcp_brain_training_init(train_ctx, NULL, NULL);

    /* Connect Portia to training */
    nimcp_brain_training_connect_portia(train_ctx, portia_get_context());

    LOG_INFO("Resource-aware training initialized");

    /* Training loop with automatic adaptation */
    const int num_epochs = 100;
    const size_t base_batch_size = 128;
    const float base_learning_rate = 0.01f;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        /* Check if training is paused (EMERGENCY mode) */
        if (nimcp_brain_training_is_paused(train_ctx)) {
            LOG_WARNING("Epoch %d skipped: training paused due to resource constraints",
                        epoch);

            /* Wait for resources to become available */
            sleep(1);
            continue;
        }

        /* Get resource-adjusted parameters */
        size_t actual_batch = nimcp_brain_training_get_adjusted_batch_size(
            train_ctx,
            base_batch_size
        );

        float actual_lr = nimcp_brain_training_get_adjusted_lr(
            train_ctx,
            base_learning_rate
        );

        LOG_INFO("Epoch %d: batch=%zu (%.0f%%), lr=%.6f (%.0f%%)",
                 epoch,
                 actual_batch,
                 (actual_batch * 100.0f) / base_batch_size,
                 actual_lr,
                 (actual_lr * 100.0f) / base_learning_rate);

        /* Optionally request resources from Portia */
        nimcp_brain_training_request_resources(
            train_ctx,
            actual_batch,
            50000  /* param_count */
        );

        /* Perform training step with adjusted parameters */
        float loss = train_step(actual_batch, actual_lr);

        LOG_INFO("Epoch %d complete: loss=%.6f", epoch, loss);
    }

    /* Cleanup */
    nimcp_brain_training_destroy(train_ctx);
    portia_destroy();
    bio_router_shutdown(router);

    LOG_INFO("Training complete");
    return 0;
}
```

## Configuration

### Training Configuration Options

```c
nimcp_brain_training_config_t config = nimcp_brain_training_default_config();

/* Enable Portia integration */
config.enable_portia_integration = true;

/* Minimum batch size as ratio of base (0.0 - 1.0) */
config.min_batch_size_ratio = 0.25f;  /* Never go below 25% of base batch */

/* Allow training to pause in EMERGENCY mode */
config.allow_training_pause = true;

/* Automatically adapt to tier changes */
config.adapt_to_tier_changes = true;
```

### Portia Configuration for Training

```c
portia_config_t portia_config = portia_get_default_config();

/* Enable automatic tier switching */
portia_config.tier_config.enable_auto_switching = true;

/* Set tier switch thresholds */
portia_config.tier_config.memory_high_threshold = 85.0f;  /* % RAM */
portia_config.tier_config.memory_low_threshold = 60.0f;

/* Enable graceful degradation */
portia_config.degradation_config.enable_graceful_degradation = true;
portia_config.degradation_config.max_degradation = PORTIA_DEGRADATION_SEVERE;

/* Enable bio-async for event messaging */
portia_config.enable_bio_async = true;
```

## Event Handling

### Manual Tier Change Handling

```c
/* Manually handle tier change (e.g., user request or external signal) */
void on_tier_change_detected(platform_tier_t new_tier) {
    nimcp_result_t result = nimcp_brain_training_on_tier_change(
        train_ctx,
        new_tier
    );

    if (result != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to handle tier change: %d", result);
        return;
    }

    /* Log the change */
    LOG_INFO("Training adapted to tier: %s", platform_tier_get_name(new_tier));

    /* Check if training was paused */
    if (nimcp_brain_training_is_paused(train_ctx)) {
        LOG_WARNING("Training paused due to EMERGENCY tier");

        /* Optionally save checkpoint here */
        save_checkpoint(train_ctx);
    }
}
```

### Degradation Event Handling

```c
/* Handle degradation event from Portia */
void on_degradation_event(uint32_t degradation_level) {
    nimcp_result_t result = nimcp_brain_training_on_degradation_event(
        train_ctx,
        degradation_level
    );

    if (result != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to handle degradation event: %d", result);
        return;
    }

    const char* level_names[] = {
        "NONE", "MINOR", "MODERATE", "SEVERE", "CRITICAL"
    };

    LOG_INFO("Training adapted to degradation level: %s",
             level_names[degradation_level]);

    /* Check if training was paused */
    if (nimcp_brain_training_is_paused(train_ctx)) {
        LOG_WARNING("Training paused due to CRITICAL degradation");
    }
}
```

### Bio-Async Event Subscription

```c
/* Subscribe to Portia events via bio-async */
void setup_portia_event_handlers(nimcp_brain_training_ctx_t* ctx) {
    /* Register handler for tier change events */
    bio_router_subscribe(
        "training",
        BIO_MSG_PORTIA_TIER_CHANGE,
        BIO_CHANNEL_SEROTONIN,
        tier_change_handler,
        ctx
    );

    /* Register handler for degradation events */
    bio_router_subscribe(
        "training",
        BIO_MSG_PORTIA_DEGRADATION_EVENT,
        BIO_CHANNEL_SEROTONIN,
        degradation_event_handler,
        ctx
    );
}

/* Tier change event handler callback */
void tier_change_handler(
    const bio_message_t* msg,
    void* user_data)
{
    nimcp_brain_training_ctx_t* ctx = (nimcp_brain_training_ctx_t*)user_data;

    /* Extract new tier from message payload */
    platform_tier_t new_tier = *(platform_tier_t*)msg->payload;

    /* Handle tier change */
    nimcp_brain_training_on_tier_change(ctx, new_tier);
}

/* Degradation event handler callback */
void degradation_event_handler(
    const bio_message_t* msg,
    void* user_data)
{
    nimcp_brain_training_ctx_t* ctx = (nimcp_brain_training_ctx_t*)user_data;

    /* Extract degradation level from message payload */
    uint32_t level = *(uint32_t*)msg->payload;

    /* Handle degradation event */
    nimcp_brain_training_on_degradation_event(ctx, level);
}
```

## Resource Adaptation

### Tier-Based Adaptation Table

| Tier         | Batch Size | Learning Rate | Features Enabled          |
|--------------|------------|---------------|---------------------------|
| FULL         | 100%       | 100%          | All features              |
| MEDIUM       | 75%        | 90%           | Core features             |
| CONSTRAINED  | 50%        | 75%           | Essential only            |
| MINIMAL      | PAUSED     | PAUSED        | Training paused           |

### Checking Current Adaptation State

```c
/* Query current resource state */
void print_training_state(nimcp_brain_training_ctx_t* ctx) {
    /* Check if paused */
    bool paused = nimcp_brain_training_is_paused(ctx);

    if (paused) {
        LOG_INFO("Training Status: PAUSED (waiting for resources)");
        return;
    }

    /* Get current adjustments */
    size_t base_batch = 128;
    float base_lr = 0.01f;

    size_t actual_batch = nimcp_brain_training_get_adjusted_batch_size(
        ctx,
        base_batch
    );

    float actual_lr = nimcp_brain_training_get_adjusted_lr(
        ctx,
        base_lr
    );

    float batch_ratio = (float)actual_batch / base_batch;
    float lr_ratio = actual_lr / base_lr;

    LOG_INFO("Training Status: ACTIVE");
    LOG_INFO("  Batch Size: %zu / %zu (%.0f%%)",
             actual_batch, base_batch, batch_ratio * 100.0f);
    LOG_INFO("  Learning Rate: %.6f / %.6f (%.0f%%)",
             actual_lr, base_lr, lr_ratio * 100.0f);
}
```

### Responding to Resource Changes

```c
/* Adaptive training loop that responds to resource changes */
void adaptive_training_loop(
    nimcp_brain_training_ctx_t* ctx,
    int num_steps)
{
    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    size_t prev_batch = base_batch;
    float prev_lr = base_lr;

    for (int step = 0; step < num_steps; step++) {
        /* Check if paused */
        if (nimcp_brain_training_is_paused(ctx)) {
            LOG_WARNING("Step %d: Training paused, waiting...", step);
            sleep(1);
            step--;  /* Retry this step */
            continue;
        }

        /* Get current parameters */
        size_t batch = nimcp_brain_training_get_adjusted_batch_size(
            ctx,
            base_batch
        );

        float lr = nimcp_brain_training_get_adjusted_lr(ctx, base_lr);

        /* Detect parameter changes */
        if (batch != prev_batch || lr != prev_lr) {
            LOG_INFO("Step %d: Parameters adjusted - batch %zu->%zu, lr %.6f->%.6f",
                     step, prev_batch, batch, prev_lr, lr);

            prev_batch = batch;
            prev_lr = lr;

            /* Optionally adjust optimizer state here */
            adjust_optimizer_for_new_parameters(batch, lr);
        }

        /* Perform training step */
        float loss = train_step(batch, lr);

        if (step % 10 == 0) {
            LOG_INFO("Step %d: loss=%.6f (batch=%zu, lr=%.6f)",
                     step, loss, batch, lr);
        }
    }
}
```

## Bio-Async Integration

### Sending Resource Requests to Portia

```c
/* Request resources from Portia before expensive operation */
void request_training_resources(
    nimcp_brain_training_ctx_t* ctx,
    size_t planned_batch_size,
    size_t model_parameters)
{
    nimcp_result_t result = nimcp_brain_training_request_resources(
        ctx,
        planned_batch_size,
        model_parameters
    );

    if (result == NIMCP_SUCCESS) {
        LOG_INFO("Resource request sent to Portia: batch=%zu, params=%zu",
                 planned_batch_size, model_parameters);
    } else if (result == NIMCP_ERROR_NOT_INITIALIZED) {
        LOG_DEBUG("Bio-async not initialized, resource request skipped");
    } else {
        LOG_WARNING("Failed to send resource request: %d", result);
    }
}
```

### Complete Bio-Async Integration Example

```c
/* Initialize bio-async messaging between Training and Portia */
int setup_training_portia_messaging(void) {
    /* 1. Initialize bio-router */
    bio_router_config_t router_config = {0};
    router_config.max_modules = 32;
    router_config.default_inbox_capacity = 128;
    router_config.enable_priority_channels = true;
    router_config.worker_thread_count = 4;

    bio_router_context_t* router = bio_router_init(&router_config);
    if (!router) {
        return -1;
    }

    /* 2. Register Training module */
    bio_router_register_module(
        "training",
        BIO_MODULE_TRAINING,
        NULL,  /* No custom handler */
        NULL   /* No user data */
    );

    /* 3. Register Portia module */
    bio_router_register_module(
        "portia",
        BIO_MODULE_PORTIA,
        NULL,
        NULL
    );

    /* 4. Subscribe Training to Portia events */
    bio_router_subscribe(
        "training",
        BIO_MSG_PORTIA_TIER_CHANGE,
        BIO_CHANNEL_SEROTONIN,
        on_tier_change_message,
        NULL
    );

    bio_router_subscribe(
        "training",
        BIO_MSG_PORTIA_DEGRADATION_EVENT,
        BIO_CHANNEL_SEROTONIN,
        on_degradation_message,
        NULL
    );

    LOG_INFO("Training-Portia bio-async messaging configured");
    return 0;
}
```

## Best Practices

### 1. Always Check for Pause State

```c
/* ✓ GOOD: Check pause state before training */
if (!nimcp_brain_training_is_paused(ctx)) {
    train_step(batch_size, learning_rate);
} else {
    LOG_WARNING("Training paused, skipping step");
}

/* ✗ BAD: Don't assume training is always active */
train_step(batch_size, learning_rate);  /* May fail if paused! */
```

### 2. Use Adjusted Parameters Consistently

```c
/* ✓ GOOD: Get adjusted parameters from training context */
size_t batch = nimcp_brain_training_get_adjusted_batch_size(ctx, 128);
float lr = nimcp_brain_training_get_adjusted_lr(ctx, 0.01f);
train_step(batch, lr);

/* ✗ BAD: Don't use base parameters directly */
train_step(128, 0.01f);  /* Ignores resource constraints! */
```

### 3. Handle Tier Changes Gracefully

```c
/* ✓ GOOD: Adapt to tier changes */
void on_tier_change(platform_tier_t new_tier) {
    nimcp_brain_training_on_tier_change(ctx, new_tier);

    if (nimcp_brain_training_is_paused(ctx)) {
        save_checkpoint();
        LOG_INFO("Checkpoint saved, training paused");
    }
}

/* ✗ BAD: Ignore tier changes */
void on_tier_change(platform_tier_t new_tier) {
    /* Do nothing - training will be unaware of resource constraints */
}
```

### 4. Request Resources for Large Operations

```c
/* ✓ GOOD: Inform Portia of resource needs */
void train_large_model(void) {
    nimcp_brain_training_request_resources(ctx, 256, 1000000);
    /* Portia can now plan accordingly */
    train_with_large_batch(256);
}

/* ✗ BAD: Don't surprise Portia with large allocations */
void train_large_model(void) {
    train_with_large_batch(256);  /* Portia not informed! */
}
```

### 5. Enable Bio-Async for Automatic Adaptation

```c
/* ✓ GOOD: Enable bio-async for automatic tier adaptation */
portia_config.enable_bio_async = true;
train_config.enable_portia_integration = true;
/* Training now adapts automatically to Portia events */

/* ✗ BAD: Manual polling is inefficient */
while (training) {
    platform_tier_t tier = portia_get_current_tier();
    nimcp_brain_training_on_tier_change(ctx, tier);  /* Wasteful! */
}
```

## Troubleshooting

### Issue: Training not adapting to tier changes

**Symptoms**: Batch size and learning rate remain constant despite tier changes

**Solution**:
```c
/* Check if Portia integration is enabled */
nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
if (!config.enable_portia_integration) {
    LOG_ERROR("Portia integration is disabled!");
    config.enable_portia_integration = true;
}

/* Ensure Portia is connected */
if (ctx->portia_context == NULL) {
    LOG_ERROR("Portia not connected!");
    nimcp_brain_training_connect_portia(ctx, portia_context);
}

/* Verify tier change handler is called */
nimcp_result_t res = nimcp_brain_training_on_tier_change(ctx, new_tier);
if (res != NIMCP_SUCCESS) {
    LOG_ERROR("Tier change failed: %d", res);
}
```

### Issue: Training pauses unexpectedly

**Symptoms**: Training stops without clear reason

**Solution**:
```c
/* Check current tier */
platform_tier_t tier = portia_get_current_tier();
LOG_INFO("Current tier: %s", platform_tier_get_name(tier));

/* Check degradation level */
portia_status_t status;
portia_get_status(&status);
LOG_INFO("Degradation level: %d", status.degradation_level);

/* Manually resume if desired */
if (nimcp_brain_training_is_paused(ctx)) {
    nimcp_brain_training_resume(ctx);
    LOG_INFO("Training manually resumed");
}
```

### Issue: Batch size becomes zero

**Symptoms**: `get_adjusted_batch_size()` returns 0

**Solution**:
```c
size_t batch = nimcp_brain_training_get_adjusted_batch_size(ctx, base_batch);

if (batch == 0) {
    /* Check if training is paused */
    if (nimcp_brain_training_is_paused(ctx)) {
        LOG_INFO("Batch size is 0 because training is paused");
        /* Wait for tier upgrade or manually resume */
    } else {
        /* Check min_batch_size_ratio */
        if (ctx->config.min_batch_size_ratio == 0.0f) {
            LOG_ERROR("min_batch_size_ratio is 0! Set to >= 0.01");
        }
    }
}
```

### Issue: Bio-async messages not received

**Symptoms**: Tier changes not triggering automatically

**Solution**:
```c
/* Verify bio-router is initialized */
if (!bio_router_is_initialized()) {
    LOG_ERROR("Bio-router not initialized!");
    bio_router_init(&router_config);
}

/* Check module registration */
if (!bio_router_is_module_registered(BIO_MODULE_TRAINING)) {
    LOG_ERROR("Training module not registered!");
}

/* Verify subscriptions */
LOG_INFO("Checking subscriptions:");
bio_router_list_subscriptions("training");

/* Enable bio-async in Portia config */
portia_config.enable_bio_async = true;
```

### Issue: Memory usage not decreasing with tier downgrades

**Symptoms**: Memory consumption stays high despite CONSTRAINED tier

**Explanation**: Batch size reduction affects memory *per batch*, not total memory.
Existing allocations (model weights, optimizer state) remain unchanged.

**Solution**:
```c
/* Reducing batch size helps per-iteration memory */
size_t batch = nimcp_brain_training_get_adjusted_batch_size(ctx, 128);
/* batch=64 in CONSTRAINED tier */

/* For deeper memory reduction, consider: */
if (tier <= PLATFORM_TIER_CONSTRAINED) {
    /* 1. Free gradient accumulation buffers */
    clear_gradient_buffers();

    /* 2. Reduce precision (FP32 -> FP16) */
    convert_to_half_precision();

    /* 3. Offload optimizer state to disk */
    offload_optimizer_state();
}
```

## Advanced Usage

### Custom Tier Multipliers

If you need different batch size / LR multipliers than the defaults:

```c
/* Modify the calculate_tier_multipliers() function in */
/* src/middleware/training/nimcp_brain_training_integration.c */

/* Example: More aggressive batch size reduction */
static void calculate_tier_multipliers_custom(
    platform_tier_t tier,
    float* batch_multiplier,
    float* lr_multiplier)
{
    switch (tier) {
        case PLATFORM_TIER_FULL:
            *batch_multiplier = 1.0f;
            *lr_multiplier = 1.0f;
            break;
        case PLATFORM_TIER_MEDIUM:
            *batch_multiplier = 0.5f;   /* 50% instead of 75% */
            *lr_multiplier = 0.85f;     /* 85% instead of 90% */
            break;
        case PLATFORM_TIER_CONSTRAINED:
            *batch_multiplier = 0.25f;  /* 25% instead of 50% */
            *lr_multiplier = 0.5f;      /* 50% instead of 75% */
            break;
        default:
            *batch_multiplier = 1.0f;
            *lr_multiplier = 1.0f;
            break;
    }
}
```

### Checkpoint on Pause

```c
void on_tier_change_with_checkpoint(platform_tier_t new_tier) {
    platform_tier_t old_tier = portia_get_current_tier();

    nimcp_brain_training_on_tier_change(ctx, new_tier);

    /* Save checkpoint when downgrading to MINIMAL */
    if (new_tier == PLATFORM_TIER_MINIMAL &&
        old_tier != PLATFORM_TIER_MINIMAL) {

        LOG_INFO("Saving checkpoint before pause");
        save_training_checkpoint(ctx, "emergency_checkpoint.bin");
    }

    /* Load checkpoint when upgrading from MINIMAL */
    if (old_tier == PLATFORM_TIER_MINIMAL &&
        new_tier != PLATFORM_TIER_MINIMAL) {

        LOG_INFO("Loading checkpoint after resume");
        load_training_checkpoint(ctx, "emergency_checkpoint.bin");
    }
}
```

## Summary

The Portia-Training integration provides automatic, transparent resource-aware training for NIMCP. Key takeaways:

1. **Enable with one line**: `config.enable_portia_integration = true;`
2. **Query adjusted parameters**: Use `get_adjusted_batch_size()` and `get_adjusted_lr()`
3. **Check pause state**: Always check `is_paused()` before training
4. **Handle events**: Subscribe to tier change and degradation events
5. **Request resources**: Inform Portia of upcoming large operations
6. **Graceful degradation**: Training automatically adapts to constraints
7. **Automatic recovery**: Training resumes when resources available

For more information, see:
- API Reference: `include/middleware/training/nimcp_brain_training_integration.h`
- Implementation: `src/middleware/training/nimcp_brain_training_integration.c`
- Unit Tests: `test/unit/middleware/training/test_portia_training_integration.cpp`
- Integration Tests: `test/integration/middleware/training/test_portia_training_integration.cpp`
- Regression Tests: `test/regression/middleware/training/test_portia_training_regression.cpp`

---

**Document Version**: 1.0
**Last Updated**: 2025-12-09
**Maintained By**: NIMCP Development Team
