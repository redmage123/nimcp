# FEP Bridges Implementation Guide

## Overview
This document provides the complete implementation for 6 FEP bridge modules integrating cognitive systems with the Free Energy Principle.

## Implementation Summary

### Completed:
1. ✅ **working_memory** - Active inference maintains items via precision-weighted attention
   - Header: `/home/bbrelin/nimcp/include/cognitive/working_memory/nimcp_working_memory_fep_bridge.h`
   - Implementation: `/home/bbrelin/nimcp/src/cognitive/working_memory/nimcp_working_memory_fep_bridge.c`

### Biological Basis for Each Module:

2. **predictive** - Core FEP - hierarchical prediction and error minimization
   - FEP → Predictive: Precision tunes prediction confidence, PE drives prediction updates
   - Predictive → FEP: Predictions provide sensory expectations, errors drive belief updates
   - Bio basis: Predictive coding IS the FEP implementation

3. **wellbeing** - Allostatic inference - predicting and regulating internal states
   - FEP → Wellbeing: High free energy signals distress, low FE signals homeostasis
   - Wellbeing → FEP: Distress states modulate precision (stress reduces precision)
   - Bio basis: Interoception as hierarchical inference over internal states

4. **sleep_wake** - Offline belief consolidation and synaptic homeostasis
   - FEP → Sleep: High累積 free energy triggers sleep need
   - Sleep → FEP: Offline replay updates generative models
   - Bio basis: Sleep as offline active inference and model consolidation

5. **meta_learning** - Hierarchical precision learning - learning to learn
   - FEP → Meta-learning: Precision policies are meta-parameters
   - Meta-learning → FEP: Task similarity modulates precision priors
   - Bio basis: MAML as hierarchical FEP with precision hyperpriors

6. **consolidation** - Memory consolidation as belief compression
   - FEP → Consolidation: Free energy reduction drives consolidation
   - Consolidation → FEP: Compression updates generative model structure
   - Bio basis: Consolidation minimizes model complexity (Occam's razor)

## File Structure Pattern

For each module `<module>`:

```
include/cognitive/<module>/nimcp_<module>_fep_bridge.h
src/cognitive/<module>/nimcp_<module>_fep_bridge.c
test/unit/cognitive/<module>/test_<module>_fep_bridge.cpp
test/integration/cognitive/<module>/test_<module>_fep_bridge_integration.cpp
```

## Header Template Structure

```c
/**
 * @file nimcp_<module>_fep_bridge.h
 * @brief Free Energy Principle - <Module> Integration Bridge
 *
 * BIOLOGICAL BASIS:
 * - <FEP → Module pathway>
 * - <Module → FEP pathway>
 */

#ifndef NIMCP_<MODULE>_FEP_BRIDGE_H
#define NIMCP_<MODULE>_FEP_BRIDGE_H

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_<module>.h" // or appropriate path
#include "async/nimcp_bio_router.h"

/* Configuration struct */
typedef struct {
    /* FEP → Module */
    bool enable_<feature1>;
    float <param1>;

    /* Module → FEP */
    bool enable_<feature2>;
    float <param2>;

    /* Sensitivity */
    float precision_sensitivity;
    float <module>_sensitivity;
} <module>_fep_config_t;

/* FEP effects on module */
typedef struct {
    float precision_value;
    float prediction_error;
    float expected_free_energy;
    /* Module-specific effects */
} <module>_fep_effects_t;

/* Module effects on FEP */
typedef struct {
    float <module>_state;
    float precision_modulation;
    /* Module-specific effects */
} fep_<module>_effects_t;

/* Bridge state */
typedef struct {
    float current_precision;
    float current_<module>_state;
    /* State flags */
    bool <flag1>;
    uint64_t last_update_time;
} <module>_fep_state_t;

/* Statistics */
typedef struct {
    uint64_t <event>_count;
    float avg_precision;
    float avg_free_energy;
} <module>_fep_stats_t;

/* Bridge struct */
typedef struct <module>_fep_bridge {
    <module>_fep_config_t config;
    fep_system_t* fep_system;
    <module>_t* <module>; // or appropriate type
    <module>_fep_effects_t fep_effects;
    fep_<module>_effects_t <module>_effects;
    <module>_fep_state_t state;
    <module>_fep_stats_t stats;
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    void* mutex;
} <module>_fep_bridge_t;

/* Lifecycle */
int <module>_fep_bridge_default_config(<module>_fep_config_t* config);
<module>_fep_bridge_t* <module>_fep_bridge_create(const <module>_fep_config_t* config);
void <module>_fep_bridge_destroy(<module>_fep_bridge_t* bridge);

/* Connection */
int <module>_fep_bridge_connect_fep(<module>_fep_bridge_t* bridge, fep_system_t* fep);
int <module>_fep_bridge_connect_<module>(<module>_fep_bridge_t* bridge, <module>_t* mod);
int <module>_fep_bridge_disconnect(<module>_fep_bridge_t* bridge);

/* FEP → Module direction */
int <module>_fep_apply_<effect1>(<module>_fep_bridge_t* bridge);
int <module>_fep_apply_<effect2>(<module>_fep_bridge_t* bridge);

/* Module → FEP direction */
int <module>_fep_apply_<effect3>(<module>_fep_bridge_t* bridge);
int <module>_fep_apply_<effect4>(<module>_fep_bridge_t* bridge);

/* Update */
int <module>_fep_bridge_update(<module>_fep_bridge_t* bridge, uint64_t delta_ms);

/* State/Stats */
int <module>_fep_bridge_get_state(const <module>_fep_bridge_t* bridge, <module>_fep_state_t* state);
int <module>_fep_bridge_get_stats(const <module>_fep_bridge_t* bridge, <module>_fep_stats_t* stats);

/* Bio-Async */
int <module>_fep_bridge_connect_bio_async(<module>_fep_bridge_t* bridge);
int <module>_fep_bridge_disconnect_bio_async(<module>_fep_bridge_t* bridge);
bool <module>_fep_bridge_is_bio_async_connected(const <module>_fep_bridge_t* bridge);

#endif
```

## Implementation Template Structure

```c
/**
 * @file nimcp_<module>_fep_bridge.c
 */

#include "cognitive/<module>/nimcp_<module>_fep_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"

#include <string.h>
#include <math.h>

#define LOG_MODULE "<module>_fep_bridge"

/* Default config */
int <module>_fep_bridge_default_config(<module>_fep_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    /* Set defaults for all config fields */
    config->enable_<feature1> = true;
    config->precision_sensitivity = 1.0f;
    config-><module>_sensitivity = 1.0f;

    return 0;
}

/* Lifecycle */
<module>_fep_bridge_t* <module>_fep_bridge_create(const <module>_fep_config_t* config) {
    <module>_fep_bridge_t* bridge = nimcp_malloc(sizeof(<module>_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(<module>_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        <module>_fep_bridge_default_config(&bridge->config);
    }

    bridge->mutex = nimcp_platform_mutex_create();
    if (!bridge->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created bridge");
    return bridge;
}

void <module>_fep_bridge_destroy(<module>_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->bio_async_enabled) {
        <module>_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed bridge");
}

/* Connection */
int <module>_fep_bridge_connect_fep(<module>_fep_bridge_t* bridge, fep_system_t* fep) {
    if (!bridge || !fep) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_INFO("Connected FEP system");
    return 0;
}

int <module>_fep_bridge_connect_<module>(<module>_fep_bridge_t* bridge, <module>_t* mod) {
    if (!bridge || !mod) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->mutex);
    bridge-><module> = mod;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_INFO("Connected module");
    return 0;
}

int <module>_fep_bridge_disconnect(<module>_fep_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->mutex);
    bridge->fep_system = NULL;
    bridge-><module> = NULL;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_INFO("Disconnected all");
    return 0;
}

/* FEP → Module effects */
int <module>_fep_apply_<effect1>(<module>_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system || !bridge-><module>) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_<feature1>) {
        return 0;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Implement effect logic */

    bridge->stats.<event>_count++;

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_DEBUG("Applied effect1");
    return 0;
}

/* Module → FEP effects */
int <module>_fep_apply_<effect3>(<module>_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system || !bridge-><module>) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (!bridge->config.enable_<feature2>) {
        return 0;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Implement effect logic */

    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_DEBUG("Applied effect3");
    return 0;
}

/* Update cycle */
int <module>_fep_bridge_update(<module>_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    /* Apply all effects */
    <module>_fep_apply_<effect1>(bridge);
    <module>_fep_apply_<effect2>(bridge);
    <module>_fep_apply_<effect3>(bridge);
    <module>_fep_apply_<effect4>(bridge);

    /* Update running averages */
    nimcp_mutex_lock(bridge->mutex);

    bridge->stats.avg_precision =
        (bridge->stats.avg_precision * 0.9f) + (bridge->state.current_precision * 0.1f);

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* State/Stats */
int <module>_fep_bridge_get_state(const <module>_fep_bridge_t* bridge, <module>_fep_state_t* state) {
    if (!bridge || !state) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int <module>_fep_bridge_get_stats(const <module>_fep_bridge_t* bridge, <module>_fep_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(bridge->mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* Bio-Async */
int <module>_fep_bridge_connect_bio_async(<module>_fep_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (bridge->bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_<MODULE>_BRIDGE,
        .module_name = "<module>_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (bridge->bio_ctx) {
        bridge->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available");
    }

    return 0;
}

int <module>_fep_bridge_disconnect_bio_async(<module>_fep_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->bio_async_enabled) return 0;

    if (bridge->bio_ctx) {
        bio_router_unregister_module(bridge->bio_ctx);
        bridge->bio_ctx = NULL;
    }

    bridge->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");

    return 0;
}

bool <module>_fep_bridge_is_bio_async_connected(const <module>_fep_bridge_t* bridge) {
    return bridge ? bridge->bio_async_enabled : false;
}
```

## BIO_MODULE Definitions

Add to `/home/bbrelin/nimcp/include/async/nimcp_bio_messages.h`:

```c
/* FEP bridge modules (0x0E00 - 0x0EFF) */
BIO_MODULE_FEP_WORKING_MEMORY_BRIDGE = 0x0E00,
BIO_MODULE_FEP_PREDICTIVE_BRIDGE,
BIO_MODULE_FEP_WELLBEING_BRIDGE,
BIO_MODULE_FEP_SLEEP_WAKE_BRIDGE,
BIO_MODULE_FEP_META_LEARNING_BRIDGE,
BIO_MODULE_FEP_CONSOLIDATION_BRIDGE,
```

## CMakeLists.txt Updates

Add to `/home/bbrelin/nimcp/src/lib/CMakeLists.txt`:

```cmake
# FEP Bridge modules
set(FEP_BRIDGE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/cognitive/working_memory/nimcp_working_memory_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/predictive/nimcp_predictive_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/wellbeing/nimcp_wellbeing_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/sleep_wake/nimcp_sleep_wake_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/meta_learning/nimcp_meta_learning_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/consolidation/nimcp_consolidation_fep_bridge.c
)

target_sources(nimcp PRIVATE ${FEP_BRIDGE_SOURCES})
```

## Test Structure

Each module needs 3 test files:

### Unit Test Template (`test/unit/cognitive/<module>/test_<module>_fep_bridge.cpp`):

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "cognitive/<module>/nimcp_<module>_fep_bridge.h"
}

class <Module>FEPBridgeTest : public ::testing::Test {
protected:
    <module>_fep_bridge_t* bridge;

    void SetUp() override {
        bridge = <module>_fep_bridge_create(NULL);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        <module>_fep_bridge_destroy(bridge);
    }
};

TEST_F(<Module>FEPBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(<Module>FEPBridgeTest, DefaultConfig) {
    <module>_fep_config_t config;
    int ret = <module>_fep_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_<feature1>);
}

TEST_F(<Module>FEPBridgeTest, ConnectFEP) {
    fep_config_t fep_cfg;
    fep_default_config(&fep_cfg);
    fep_system_t* fep = fep_create(&fep_cfg, 64, 8);
    ASSERT_NE(fep, nullptr);

    int ret = <module>_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

// Add more tests for each function
```

## Next Steps

1. Generate remaining bridge files using templates above
2. Implement module-specific biological pathways
3. Create comprehensive unit tests
4. Add integration tests
5. Update bio_messages.h with module IDs
6. Update CMakeLists.txt
7. Build and validate all bridges

## Module-Specific Implementation Notes

### Predictive FEP Bridge
- Direct mapping: predictive coding IS FEP
- Key pathway: Prediction errors drive belief updates
- Special: Hierarchical level matching

### Wellbeing FEP Bridge
- Free energy = distress signal
- High FE → triggers wellbeing intervention
- Wellbeing state → modulates precision (stress reduces precision)

### Sleep/Wake FEP Bridge
- Sleep pressure = accumulated free energy
- Sleep = offline model optimization
- Wake → active inference, Sleep → belief consolidation

### Meta-Learning FEP Bridge
- Task adaptation = precision policy learning
- Inner loop = belief update, Outer loop = precision hyperprior update
- Transfer learning via precision similarity

### Consolidation FEP Bridge
- Consolidation = model compression (minimize complexity)
- Replay = belief strengthening
- Pruning = reduce model complexity term in free energy
