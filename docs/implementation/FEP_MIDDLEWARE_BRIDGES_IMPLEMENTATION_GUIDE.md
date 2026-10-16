# FEP Middleware Bridges Implementation Guide

## Overview
This guide documents the implementation of FEP (Free Energy Principle) bridges for 8 middleware modules in NIMCP. Each bridge enables bidirectional integration between FEP and middleware components.

## Status Summary

| Module | Header | Implementation | Tests | Bio Module ID | Status |
|--------|--------|---------------|-------|---------------|---------|
| buffering | ✅ Complete | ✅ Complete | ⏳ Pending | TBD | DONE |
| encoding | ⏳ Template | ⏳ Template | ⏳ Pending | TBD | IN PROGRESS |
| events | ⏳ Template | ⏳ Template | ⏳ Pending | TBD | PENDING |
| integration | ⏳ Template | ⏳ Template | ⏳ Pending | TBD | PENDING |
| memory | ⏳ Template | ⏳ Template | ⏳ Pending | TBD | PENDING |
| normalization | ⏳ Template | ⏳ Template | ⏳ Pending | TBD | PENDING |
| pipeline | ⏳ Template | ⏳ Template | ⏳ Pending | TBD | PENDING |
| training | ⏳ Template | ⏳ Template | ⏳ Pending | TBD | PENDING |

## Module Mappings

### 1. Buffering (Circular Buffer)
**Module**: `nimcp_circular_buffer.h`
**Biological Basis**: Temporal buffering as prediction horizon management

**FEP → Buffering**:
- Prediction horizon → buffer capacity/depth
- Precision → attention window size
- Expected sequences → buffer priming

**Buffering → FEP**:
- Buffer utilization → capacity constraint/uncertainty
- Overflows → surprise signals
- Buffer patterns → temporal observations

**Files Created**:
- ✅ `/home/bbrelin/nimcp/include/middleware/buffering/nimcp_circular_buffer_fep_bridge.h`
- ✅ `/home/bbrelin/nimcp/src/middleware/buffering/nimcp_circular_buffer_fep_bridge.c`

### 2. Encoding (Population Coding)
**Module**: `nimcp_population_coding.h`
**Biological Basis**: Neural encoding as likelihood mapping

**FEP → Encoding**:
- Precision → encoding granularity (sparse vs. dense)
- Hierarchy level → encoding type (rate/temporal/population)
- Expected states → encoding priors

**Encoding → FEP**:
- Encoded representations → observations
- Sparse code overlap → observation similarity
- Population synchrony → precision estimate

**Key Functions**:
```c
int population_coding_fep_set_precision(bridge, precision);
int population_coding_fep_select_encoding_type(bridge, hierarchy_level);
int population_coding_fep_report_synchrony(bridge, synchrony_result);
```

### 3. Events (Event Bus)
**Module**: `nimcp_event_bus.h`
**Biological Basis**: Event detection as significant prediction error thresholding

**FEP → Events**:
- Prediction error magnitude → event detection threshold
- Precision → event priority
- Surprise → event generation trigger

**Events → FEP**:
- Event occurrences → observations
- Event frequency → temporal precision
- Dropped events → capacity constraints

**Key Functions**:
```c
int event_bus_fep_set_detection_threshold(bridge, pe_magnitude);
int event_bus_fep_report_event(bridge, event_type, magnitude);
int event_bus_fep_report_drops(bridge, drop_count);
```

### 4. Integration (Multimodal Integration)
**Module**: `nimcp_multimodal_integration.h`
**Biological Basis**: Multi-modal integration as hierarchical inference

**FEP → Integration**:
- Hierarchy level → which modalities to integrate
- Precision weights → modality attention
- Cross-modal predictions → expected integration patterns

**Integration → FEP**:
- Integrated features → multi-modal observations
- Modality conflicts → prediction errors
- Attention weights → learned precision

**Key Functions**:
```c
int multimodal_integration_fep_set_precision_weights(bridge, weights);
int multimodal_integration_fep_report_conflict(bridge, conflict_magnitude);
int multimodal_integration_fep_report_integration(bridge, integrated_features);
```

### 5. Memory (Gradient Manager)
**Module**: `nimcp_gradient_manager.h`
**Biological Basis**: Memory operations as belief storage and retrieval

**FEP → Memory**:
- Prediction errors → gradient signals
- Precision → learning rate modulation
- Free energy → consolidation trigger

**Memory → FEP**:
- Gradient statistics → belief update metrics
- NaN/Inf detection → instability signals
- Accumulation state → temporal integration

**Key Functions**:
```c
int gradient_manager_fep_set_learning_rate(bridge, lr);
int gradient_manager_fep_report_gradients(bridge, grad_stats);
int gradient_manager_fep_report_instability(bridge, has_nan, has_inf);
```

### 6. Normalization (Z-Score Normalizer)
**Module**: `nimcp_zscore_normalizer.h`
**Biological Basis**: Divisive normalization as precision normalization

**FEP → Normalization**:
- Precision → normalization target (variance)
- Expected statistics → normalization priors
- Belief confidence → adaptation rate

**Normalization → FEP**:
- Normalized observations → precision-weighted inputs
- Running statistics → observation reliability
- Outliers → surprise signals

**Key Functions**:
```c
int zscore_normalizer_fep_set_target_variance(bridge, precision);
int zscore_normalizer_fep_report_statistics(bridge, mean, variance);
int zscore_normalizer_fep_report_outliers(bridge, outlier_count);
```

### 7. Pipeline (Middleware Pipeline)
**Module**: `nimcp_middleware_pipeline.h`
**Biological Basis**: Processing pipeline as hierarchical message passing

**FEP → Pipeline**:
- Hierarchy level → active pipeline stages
- Precision → stage execution thresholds
- Predictions → expected stage outputs

**Pipeline → FEP**:
- Stage outputs → hierarchical observations
- Execution failures → prediction errors
- Processing time → computational cost

**Key Functions**:
```c
int pipeline_fep_enable_stages(bridge, hierarchy_level);
int pipeline_fep_set_stage_thresholds(bridge, precision);
int pipeline_fep_report_stage_output(bridge, stage_id, output);
int pipeline_fep_report_failure(bridge, stage_id, error);
```

### 8. Training (Brain Training Integration)
**Module**: `nimcp_brain_training_integration.h`
**Biological Basis**: Training as generative model optimization

**FEP → Training**:
- Free energy → loss function
- Prediction errors → gradients
- Precision → learning rate
- EFE → policy selection

**Training → FEP**:
- Loss values → free energy estimates
- Gradients → belief update signals
- Convergence → belief stability
- Divergence → surprise

**Key Functions**:
```c
int training_fep_set_loss_from_fe(bridge, free_energy);
int training_fep_set_lr_from_precision(bridge, precision);
int training_fep_report_loss(bridge, loss_value);
int training_fep_report_gradients(bridge, grad_norm);
int training_fep_report_convergence(bridge, converged);
```

## Implementation Pattern

All bridges follow this standard structure:

### Header File Structure
```c
// 1. Constants
#define FEP_MODULE_CONSTANT 1.0f

// 2. Config struct
typedef struct {
    bool enable_feature_1;
    bool enable_feature_2;
    float sensitivity_1;
    float sensitivity_2;
} module_fep_config_t;

// 3. Effects structs (FEP → Module direction)
typedef struct {
    float modulation_factor;
    uint32_t target_parameter;
} module_fep_effects_t;

// 4. State struct
typedef struct {
    float current_precision;
    float module_state;
} module_fep_state_t;

// 5. Stats struct
typedef struct {
    uint64_t update_count;
    float avg_metric;
} module_fep_stats_t;

// 6. Bridge struct
struct module_fep_bridge {
    module_fep_config_t config;
    module_t* module;
    fep_system_t* fep_system;
    module_fep_effects_t effects;
    module_fep_state_t state;
    module_fep_stats_t stats;
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    void* mutex;
};

// 7. API functions
int module_fep_bridge_default_config(module_fep_config_t* config);
module_fep_bridge_t* module_fep_bridge_create(const module_fep_config_t* config);
void module_fep_bridge_destroy(module_fep_bridge_t* bridge);
int module_fep_bridge_connect_module(module_fep_bridge_t* bridge, module_t* module);
int module_fep_bridge_connect_fep(module_fep_bridge_t* bridge, fep_system_t* fep);
int module_fep_bridge_disconnect(module_fep_bridge_t* bridge);
int module_fep_bridge_update(module_fep_bridge_t* bridge, uint64_t delta_ms);
int module_fep_bridge_get_state(const module_fep_bridge_t* bridge, module_fep_state_t* state);
int module_fep_bridge_get_stats(const module_fep_bridge_t* bridge, module_fep_stats_t* stats);
int module_fep_bridge_connect_bio_async(module_fep_bridge_t* bridge);
int module_fep_bridge_disconnect_bio_async(module_fep_bridge_t* bridge);
bool module_fep_bridge_is_bio_async_connected(const module_fep_bridge_t* bridge);
```

### Implementation File Structure
```c
// 1. Lifecycle functions
int module_fep_bridge_default_config(...) { /* Set defaults */ }
module_fep_bridge_t* module_fep_bridge_create(...) { /* Allocate, init */ }
void module_fep_bridge_destroy(...) { /* Cleanup */ }

// 2. Connection functions
int module_fep_bridge_connect_module(...) { /* Store pointer */ }
int module_fep_bridge_connect_fep(...) { /* Store pointer */ }
int module_fep_bridge_disconnect(...) { /* Clear pointers */ }

// 3. FEP → Module direction functions
int module_fep_apply_precision(...) { /* Modulate module */ }
int module_fep_set_expected_state(...) { /* Prime module */ }

// 4. Module → FEP direction functions
int module_fep_report_observation(...) { /* Send to FEP */ }
int module_fep_report_error(...) { /* Generate PE */ }

// 5. Update cycle
int module_fep_bridge_update(...) { /* Bidirectional sync */ }

// 6. State/Stats
int module_fep_bridge_get_state(...) { /* Copy state */ }
int module_fep_bridge_get_stats(...) { /* Copy stats */ }

// 7. Bio-async
int module_fep_bridge_connect_bio_async(...) { /* Register */ }
int module_fep_bridge_disconnect_bio_async(...) { /* Unregister */ }
bool module_fep_bridge_is_bio_async_connected(...) { /* Check */ }
```

## Bio-Async Module IDs

Add to `/home/bbrelin/nimcp/include/async/nimcp_bio_messages.h`:

```c
/* FEP bridge modules (0x0F00 - 0x0FFF) */
BIO_MODULE_FEP_BUFFERING_BRIDGE = 0x0F00,
BIO_MODULE_FEP_ENCODING_BRIDGE,
BIO_MODULE_FEP_EVENTS_BRIDGE,
BIO_MODULE_FEP_INTEGRATION_BRIDGE,
BIO_MODULE_FEP_MEMORY_BRIDGE,
BIO_MODULE_FEP_NORMALIZATION_BRIDGE,
BIO_MODULE_FEP_PIPELINE_BRIDGE,
BIO_MODULE_FEP_TRAINING_BRIDGE,
```

## CMakeLists.txt Updates

### For each module in `/home/bbrelin/nimcp/src/middleware/{module}/CMakeLists.txt`:

```cmake
# Add FEP bridge source
set(MODULE_FEP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_{module}_fep_bridge.c
)

# Add to library sources
list(APPEND NIMCP_SOURCES ${MODULE_FEP_SOURCES})
```

### For tests in `/home/bbrelin/nimcp/test/unit/middleware/{module}/CMakeLists.txt`:

```cmake
# FEP bridge tests
add_executable(unit_middleware_{module}_fep_bridge
    test_{module}_fep_bridge.cpp
)

target_link_libraries(unit_middleware_{module}_fep_bridge
    PRIVATE
    nimcp
    gtest
    gtest_main
    pthread
)

add_test(
    NAME unit_middleware_{module}_fep_bridge
    COMMAND unit_middleware_{module}_fep_bridge --gtest_brief=1
)
```

## Test Structure Template

Each module needs these test categories:

```cpp
// test_{module}_fep_bridge.cpp

#include "middleware/{module}/nimcp_{module}_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include <gtest/gtest.h>

class ModuleFEPBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = {};
        module_fep_bridge_default_config(&config);
        bridge = module_fep_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            module_fep_bridge_destroy(bridge);
        }
    }

    module_fep_config_t config;
    module_fep_bridge_t* bridge;
};

// 1. Lifecycle tests
TEST_F(ModuleFEPBridgeTest, CreateDestroy) { /* ... */ }
TEST_F(ModuleFEPBridgeTest, DefaultConfig) { /* ... */ }

// 2. Connection tests
TEST_F(ModuleFEPBridgeTest, ConnectModule) { /* ... */ }
TEST_F(ModuleFEPBridgeTest, ConnectFEP) { /* ... */ }
TEST_F(ModuleFEPBridgeTest, Disconnect) { /* ... */ }

// 3. FEP → Module direction tests
TEST_F(ModuleFEPBridgeTest, PrecisionModulation) { /* ... */ }
TEST_F(ModuleFEPBridgeTest, HierarchySelection) { /* ... */ }

// 4. Module → FEP direction tests
TEST_F(ModuleFEPBridgeTest, ReportObservations) { /* ... */ }
TEST_F(ModuleFEPBridgeTest, ReportErrors) { /* ... */ }

// 5. Update cycle tests
TEST_F(ModuleFEPBridgeTest, Update) { /* ... */ }

// 6. State/Stats tests
TEST_F(ModuleFEPBridgeTest, GetState) { /* ... */ }
TEST_F(ModuleFEPBridgeTest, GetStats) { /* ... */ }

// 7. Bio-async tests
TEST_F(ModuleFEPBridgeTest, BioAsyncConnect) { /* ... */ }
TEST_F(ModuleFEPBridgeTest, BioAsyncDisconnect) { /* ... */ }
```

## Next Steps

1. **Complete buffering module** (DONE):
   - ✅ Header created
   - ✅ Implementation created
   - ⏳ Tests pending
   - ⏳ CMakeLists updates pending

2. **Implement remaining 7 modules** following the pattern above

3. **Add bio-async module IDs** to nimcp_bio_messages.h

4. **Update all CMakeLists.txt files** for build integration

5. **Write comprehensive tests** (30+ tests per module recommended)

6. **Integration testing** across modules

## Reference Implementation

The buffering module (`nimcp_circular_buffer_fep_bridge`) serves as the reference implementation. All other modules should follow the same pattern:

- Thread-safe via mutex
- Proper error handling (guard clauses)
- NIMCP coding standards (WHAT/WHY/HOW comments, <50 lines per function)
- nimcp_malloc/nimcp_free for memory
- nimcp_mutex_* for thread safety
- NIMCP_LOGGING_* for logging
- Bio-async integration
- Complete API coverage

## Estimated Implementation Time

- Per module (header + implementation + basic tests): 2-3 hours
- Total for 7 remaining modules: 14-21 hours
- Integration and comprehensive testing: 5-7 hours
- **Total estimated: 20-30 hours**

## Contact

For questions or clarifications on FEP bridge implementation, refer to:
- Reference bridges: feature_extractor, thalamic_router, sequence_detector
- FEP system: `/home/bbrelin/nimcp/include/cognitive/free_energy/nimcp_free_energy.h`
- NIMCP coding standards: `/home/bbrelin/nimcp/CLAUDE.md`
