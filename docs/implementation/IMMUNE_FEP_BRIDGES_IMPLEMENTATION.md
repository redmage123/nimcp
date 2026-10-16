# Immune FEP Bridges Implementation Guide

## Overview

This document provides complete implementation for FEP (Free Energy Principle) bridges for 7 immune modules:

1. **brain_immune** - Core immune coordination
2. **tolerance** - Self/non-self discrimination
3. **vaccine** - Pre-training and passive immunity
4. **exhaustion** - T cell functional decline
5. **trained_immunity** - Innate memory
6. **complement** - Innate amplification
7. **mucosal** - Barrier immunity

## Biological Basis

### Immune System as Free Energy Minimizer

The immune system minimizes variational free energy through:

1. **Generative Model**: Self vs non-self discrimination
2. **Precision Weighting**: Cytokine-mediated confidence signals
3. **Prediction Errors**: Inflammation as model failure
4. **Active Inference**: Immune responses as actions minimizing expected free energy

## Implementation Status

### ✅ Completed
- `include/cognitive/immune/nimcp_brain_immune_fep_bridge.h`

### 📝 To Create

#### Headers (6 files):
1. `include/cognitive/immune/nimcp_tolerance_fep_bridge.h`
2. `include/cognitive/immune/nimcp_vaccine_fep_bridge.h`
3. `include/cognitive/immune/nimcp_exhaustion_fep_bridge.h`
4. `include/cognitive/immune/nimcp_trained_immunity_fep_bridge.h`
5. `include/cognitive/immune/nimcp_complement_fep_bridge.h`
6. `include/cognitive/immune/nimcp_mucosal_fep_bridge.h`

#### Implementations (7 files):
1. `src/cognitive/immune/nimcp_brain_immune_fep_bridge.c`
2. `src/cognitive/immune/nimcp_tolerance_fep_bridge.c`
3. `src/cognitive/immune/nimcp_vaccine_fep_bridge.c`
4. `src/cognitive/immune/nimcp_exhaustion_fep_bridge.c`
5. `src/cognitive/immune/nimcp_trained_immunity_fep_bridge.c`
6. `src/cognitive/immune/nimcp_complement_fep_bridge.c`
7. `src/cognitive/immune/nimcp_mucosal_fep_bridge.c`

## Common Bridge Pattern

All bridges follow this standardized structure:

```c
typedef struct {
    <module>_fep_config_t config;
    fep_system_t* fep_system;
    <module_type>* module;  // Actual module type (e.g., tolerance_system_t*)
    <module>_fep_effects_t fep_effects;  // FEP → module effects
    fep_<module>_effects_t module_effects;  // Module → FEP effects
    <module>_fep_state_t state;
    <module>_fep_stats_t stats;
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    void* mutex;
} <module>_fep_bridge_t;
```

### Required API Functions

1. **Lifecycle**:
   - `<module>_fep_default_config()`
   - `<module>_fep_create()`
   - `<module>_fep_destroy()`

2. **Update**:
   - `<module>_fep_update()` - Compute bidirectional effects
   - `<module>_fep_apply_to_<module>()` - Apply FEP effects
   - `<module>_fep_apply_to_fep()` - Apply module effects to FEP

3. **Query**:
   - `<module>_fep_get_stats()`
   - Module-specific query functions

4. **Bio-Async**:
   - `<module>_fep_connect_bio_async()`
   - `<module>_fep_disconnect_bio_async()`
   - `<module>_fep_is_bio_async_connected()`

## Module-Specific FEP Mappings

### 1. Tolerance FEP Bridge

**Biological Basis**: Self-tolerance as accurate generative model

- **FEP → Tolerance**:
  - Low free energy for self-patterns = accurate model
  - High free energy for non-self = model failure (immune response)

- **Tolerance → FEP**:
  - Central deletion = removing high-error predictions
  - Anergy = reducing precision for self-reactive cells

```c
// Key mapping
typedef struct {
    float self_pattern_precision;     // Precision for self patterns
    float deletion_free_energy_threshold;  // FE threshold for deletion
    float anergy_precision_reduction; // How much anergy reduces precision
} tolerance_fep_effects_t;
```

### 2. Vaccine FEP Bridge

**Biological Basis**: Vaccines as prior belief updating

- **FEP → Vaccine**:
  - Prior beliefs about threats → vaccine selection
  - Expected free energy → vaccine efficacy prediction

- **Vaccine → FEP**:
  - Memory formation = updating generative model
  - Efficacy decay = forgetting (prior drift)

```c
typedef struct {
    float memory_strength;        // Strength of prior belief
    float efficacy_precision;     // Precision of vaccine memory
    float booster_signal;         // Signal to refresh prior
} vaccine_fep_effects_t;
```

### 3. Exhaustion FEP Bridge

**Biological Basis**: Exhaustion as precision depletion

- **FEP → Exhaustion**:
  - Chronic high prediction error → exhaustion
  - Sustained high precision demands → fatigue

- **Exhaustion → FEP**:
  - Reduced effector capacity → reduced precision
  - Recovery → precision restoration

```c
typedef struct {
    float effector_precision;     // Precision based on capacity
    float exhaustion_error_accumulation;  // Cumulative prediction error
    float recovery_precision_gain; // Precision gained from recovery
} exhaustion_fep_effects_t;
```

### 4. Trained Immunity FEP Bridge

**Biological Basis**: Trained immunity as heightened prior sensitivity

- **FEP → Trained Immunity**:
  - Epigenetic changes = generative model reprogramming
  - Enhanced PRR = increased sensory precision

- **Trained Immunity → FEP**:
  - Enhanced responses = higher precision weights
  - Cross-protection = generalized priors

```c
typedef struct {
    float prr_sensitivity_precision;  // PRR-based precision boost
    float metabolic_precision_cost;   // Energy cost of high precision
    float cross_protection_prior;     // Generalized prior strength
} trained_immunity_fep_effects_t;
```

### 5. Complement FEP Bridge

**Biological Basis**: Complement amplification as precision cascade

- **FEP → Complement**:
  - High prediction error → trigger amplification
  - Opsonization = precision-weighted tagging

- **Complement → FEP**:
  - Amplification = precision multiplication
  - Anaphylatoxins = prediction error signals

```c
typedef struct {
    float amplification_precision_multiplier;  // C3b amplification effect
    float mac_formation_free_energy;  // Free energy for MAC decision
    float c3a_c5a_precision_boost;    // Anaphylatoxin precision increase
} complement_fep_effects_t;
```

### 6. Mucosal FEP Bridge

**Biological Basis**: Mucosal immunity as barrier-specific priors

- **FEP → Mucosal**:
  - Tolerance bias = high threshold for prediction error
  - sIgA production = low-cost active inference

- **Mucosal → FEP**:
  - Barrier integrity = sensory precision
  - Oral tolerance = updating priors for benign stimuli

```c
typedef struct {
    float barrier_precision;      // Precision at boundary
    float tolerance_prior_strength;  // Strength of tolerance prior
    float siga_precision_cost;    // Low cost of sIgA vs systemic response
} mucosal_fep_effects_t;
```

## Bio-Async Integration

All bridges should register with bio-async router using module IDs. Add to `nimcp_bio_messages.h`:

```c
/* Immune FEP bridge modules (0x0E00 - 0x0EFF) */
BIO_MODULE_FEP_BRAIN_IMMUNE = 0x0E00,
BIO_MODULE_FEP_TOLERANCE,
BIO_MODULE_FEP_VACCINE,
BIO_MODULE_FEP_EXHAUSTION,
BIO_MODULE_FEP_TRAINED_IMMUNITY,
BIO_MODULE_FEP_COMPLEMENT,
BIO_MODULE_FEP_MUCOSAL,
```

## Implementation Template

### Header Template

```c
/**
 * @file nimcp_<module>_fep_bridge.h
 * @brief FEP Bridge for <Module Name>
 *
 * WHAT: Bidirectional integration between <module> and FEP
 * WHY:  <Biological justification>
 * HOW:  <Key mappings>
 *
 * BIOLOGICAL BASIS:
 * <Detailed biological rationale>
 */

#ifndef NIMCP_<MODULE>_FEP_BRIDGE_H
#define NIMCP_<MODULE>_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/immune/nimcp_<module>.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct <module>_fep_bridge <module>_fep_bridge_t;

/* FEP → Module effects */
typedef struct {
    // Module-specific fields
} <module>_fep_effects_t;

/* Module → FEP effects */
typedef struct {
    float precision_modulation;
    float prediction_error;
    // Module-specific fields
} fep_<module>_effects_t;

/* Configuration */
typedef struct {
    bool enable_precision_modulation;
    bool enable_bio_async;
    bool enable_logging;
    // Module-specific config
} <module>_fep_config_t;

/* State */
typedef struct {
    uint64_t total_updates;
    // Module-specific state
} <module>_fep_state_t;

/* Statistics */
typedef struct {
    uint64_t total_updates;
    float current_precision_modulation;
    float current_prediction_error;
} <module>_fep_stats_t;

/* Bridge structure */
struct <module>_fep_bridge {
    <module>_fep_config_t config;
    fep_system_t* fep_system;
    <module>_system_t* module;  // Use actual type
    <module>_fep_effects_t fep_effects;
    fep_<module>_effects_t module_effects;
    <module>_fep_state_t state;
    <module>_fep_stats_t stats;
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    void* mutex;
};

/* API declarations */
int <module>_fep_default_config(<module>_fep_config_t* config);
<module>_fep_bridge_t* <module>_fep_create(
    const <module>_fep_config_t* config,
    <module>_system_t* module,
    fep_system_t* fep_system
);
void <module>_fep_destroy(<module>_fep_bridge_t* bridge);
int <module>_fep_update(<module>_fep_bridge_t* bridge);
int <module>_fep_apply_to_<module>(<module>_fep_bridge_t* bridge);
int <module>_fep_apply_to_fep(<module>_fep_bridge_t* bridge);
int <module>_fep_get_stats(const <module>_fep_bridge_t* bridge, <module>_fep_stats_t* stats);
int <module>_fep_connect_bio_async(<module>_fep_bridge_t* bridge);
int <module>_fep_disconnect_bio_async(<module>_fep_bridge_t* bridge);
bool <module>_fep_is_bio_async_connected(const <module>_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_<MODULE>_FEP_BRIDGE_H */
```

### Implementation Template

```c
/**
 * @file nimcp_<module>_fep_bridge.c
 * @brief FEP Bridge for <Module> - Implementation
 */

#include "cognitive/immune/nimcp_<module>_fep_bridge.h"
#include <string.h>
#include <math.h>

/* Default configuration */
int <module>_fep_default_config(<module>_fep_config_t* config) {
    if (!config) return -1;
    memset(config, 0, sizeof(<module>_fep_config_t));
    config->enable_precision_modulation = true;
    config->enable_bio_async = false;
    config->enable_logging = false;
    // Set module-specific defaults
    return 0;
}

/* Create bridge */
<module>_fep_bridge_t* <module>_fep_create(
    const <module>_fep_config_t* config,
    <module>_system_t* module,
    fep_system_t* fep_system
) {
    if (!module || !fep_system) return NULL;

    <module>_fep_bridge_t* bridge = (<module>_fep_bridge_t*)nimcp_malloc(sizeof(<module>_fep_bridge_t));
    if (!bridge) return NULL;

    memset(bridge, 0, sizeof(<module>_fep_bridge_t));

    /* Copy config or use defaults */
    if (config) {
        memcpy(&bridge->config, config, sizeof(<module>_fep_config_t));
    } else {
        <module>_fep_default_config(&bridge->config);
    }

    bridge->fep_system = fep_system;
    bridge->module = module;

    /* Create mutex */
    bridge->mutex = nimcp_platform_mutex_create();
    if (!bridge->mutex) {
        nimcp_free(bridge);
        return NULL;
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("<module>_fep_bridge created");
    }

    return bridge;
}

/* Destroy bridge */
void <module>_fep_destroy(<module>_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->bio_async_enabled) {
        <module>_fep_disconnect_bio_async(bridge);
    }

    if (bridge->mutex) {
        nimcp_platform_mutex_destroy(bridge->mutex);
    }

    nimcp_free(bridge);
}

/* Update bridge */
int <module>_fep_update(<module>_fep_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;

    nimcp_platform_mutex_lock(bridge->mutex);

    /* Compute module → FEP effects */
    // TODO: Calculate precision modulation from module state
    bridge->module_effects.precision_modulation = 1.0f;
    bridge->module_effects.prediction_error = 0.0f;

    /* Compute FEP → module effects */
    // TODO: Calculate module effects from FEP state

    bridge->state.total_updates++;

    nimcp_platform_mutex_unlock(bridge->mutex);
    return 0;
}

/* Apply FEP effects to module */
int <module>_fep_apply_to_<module>(<module>_fep_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->config.enable_precision_modulation) return 0;

    nimcp_platform_mutex_lock(bridge->mutex);

    /* Apply FEP-derived effects to module */
    // TODO: Implement module-specific application

    nimcp_platform_mutex_unlock(bridge->mutex);
    return 0;
}

/* Apply module effects to FEP */
int <module>_fep_apply_to_fep(<module>_fep_bridge_t* bridge) {
    if (!bridge) return NIMCP_ERROR_NULL_POINTER;
    if (!bridge->config.enable_precision_modulation) return 0;

    nimcp_platform_mutex_lock(bridge->mutex);

    /* Apply module-derived effects to FEP precision */
    // TODO: Update FEP precision based on module state

    nimcp_platform_mutex_unlock(bridge->mutex);
    return 0;
}

/* Get statistics */
int <module>_fep_get_stats(const <module>_fep_bridge_t* bridge, <module>_fep_stats_t* stats) {
    if (!bridge || !stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_platform_mutex_lock(bridge->mutex);
    memcpy(stats, &bridge->stats, sizeof(<module>_fep_stats_t));
    nimcp_platform_mutex_unlock(bridge->mutex);
    return 0;
}

/* Bio-async connection */
int <module>_fep_connect_bio_async(<module>_fep_bridge_t* bridge) {
    if (!bridge) return -1;
    if (bridge->bio_async_enabled) return 0;
    if (!bridge->config.enable_bio_async) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_<MODULE>,
        .module_name = "<module>_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (bridge->bio_ctx) {
        bridge->bio_async_enabled = true;
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_INFO("<module>_fep_bridge connected to bio-async");
        }
    }
    return 0;
}

int <module>_fep_disconnect_bio_async(<module>_fep_bridge_t* bridge) {
    if (!bridge || !bridge->bio_async_enabled) return -1;

    if (bridge->bio_ctx) {
        bio_router_unregister_module(bridge->bio_ctx);
        bridge->bio_ctx = NULL;
    }
    bridge->bio_async_enabled = false;
    return 0;
}

bool <module>_fep_is_bio_async_connected(const <module>_fep_bridge_t* bridge) {
    return bridge ? bridge->bio_async_enabled : false;
}
```

## Testing Strategy

Create test file `test/unit/cognitive/analysis/test_<module>_fep_bridge.cpp`:

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "cognitive/immune/nimcp_<module>_fep_bridge.h"
#include "cognitive/immune/nimcp_<module>.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
}

class <Module>FEPBridgeTest : public ::testing::Test {
protected:
    <module>_system_t* module;
    fep_system_t* fep;
    <module>_fep_bridge_t* bridge;

    void SetUp() override {
        // Create module
        <module>_config_t module_config;
        <module>_default_config(&module_config);
        module = <module>_create(&module_config, nullptr);
        ASSERT_NE(module, nullptr);

        // Create FEP
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, 10, 5);
        ASSERT_NE(fep, nullptr);

        // Create bridge
        bridge = <module>_fep_create(nullptr, module, fep);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        <module>_fep_destroy(bridge);
        fep_destroy(fep);
        <module>_destroy(module);
    }
};

TEST_F(<Module>FEPBridgeTest, Create) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(<Module>FEPBridgeTest, Update) {
    int ret = <module>_fep_update(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(<Module>FEPBridgeTest, ApplyBidirectional) {
    <module>_fep_update(bridge);
    int ret1 = <module>_fep_apply_to_<module>(bridge);
    int ret2 = <module>_fep_apply_to_fep(bridge);
    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
}
```

## Build Integration

Add to `src/lib/CMakeLists.txt`:

```cmake
# Immune FEP bridges
set(IMMUNE_FEP_BRIDGE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/cognitive/immune/nimcp_brain_immune_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/immune/nimcp_tolerance_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/immune/nimcp_vaccine_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/immune/nimcp_exhaustion_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/immune/nimcp_trained_immunity_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/immune/nimcp_complement_fep_bridge.c
    ${CMAKE_SOURCE_DIR}/src/cognitive/immune/nimcp_mucosal_fep_bridge.c
)

target_sources(nimcp PRIVATE ${IMMUNE_FEP_BRIDGE_SOURCES})
```

## Next Steps

1. Create all 6 remaining header files using the template
2. Create all 7 implementation files using the template
3. Add bio-async module IDs to `nimcp_bio_messages.h`
4. Create test files for each bridge
5. Add to CMakeLists.txt
6. Build and test

## References

- Friston, K. (2010). "The free-energy principle: a unified brain theory?"
- Sterling, P. (2012). "Allostasis: A model of predictive regulation"
- Netea, M.G. et al. (2016). "Trained immunity: A program of innate immune memory"
