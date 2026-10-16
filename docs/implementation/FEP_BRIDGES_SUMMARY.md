# FEP Middleware Bridges Implementation Summary

## Completed Work

### 1. Buffering Module (Circular Buffer) ✅
**Files Created**:
- `/home/bbrelin/nimcp/include/middleware/buffering/nimcp_circular_buffer_fep_bridge.h` (569 lines)
- `/home/bbrelin/nimcp/src/middleware/buffering/nimcp_circular_buffer_fep_bridge.c` (370 lines)

**Key Features**:
- Prediction horizon → buffer capacity adjustment
- Precision → attention window sizing
- Buffer utilization → FEP capacity constraint feedback
- Overflow events → surprise signal generation
- Full bio-async integration
- Thread-safe implementation
- Complete API coverage

**Biological Basis**: Temporal buffering represents prediction horizon management in active inference. Buffer depth must match FEP planning horizon for adequate temporal context.

**API Highlights**:
```c
// FEP → Buffer
int circular_buffer_fep_adjust_horizon(bridge, horizon);
int circular_buffer_fep_set_precision_window(bridge, precision);
int circular_buffer_fep_prime_sequence(bridge, expected_fill_rate);

// Buffer → FEP
int circular_buffer_fep_report_utilization(bridge, utilization);
int circular_buffer_fep_report_overflow(bridge, overflow_count);
int circular_buffer_fep_report_patterns(bridge);
```

## Pending Work

### Implementation Plan for Remaining 7 Modules

Each module requires:
1. **Header file** (~400-600 lines) with:
   - Config struct
   - Effects structs (bidirectional)
   - State and stats structs
   - Bridge struct
   - Complete API (15-20 functions)

2. **Implementation file** (~300-500 lines) with:
   - Lifecycle functions
   - Connection management
   - Bidirectional integration functions
   - Update cycle
   - Bio-async integration

3. **Test file** (~500-800 lines) with:
   - Lifecycle tests (5-7)
   - Connection tests (4-6)
   - Direction-specific tests (10-15 each direction)
   - Update cycle tests (3-5)
   - State/stats tests (4-6)
   - Bio-async tests (3-5)
   - **Total: 30-40 tests per module**

### Module Specifications

#### 2. Encoding (Population Coding)
**Module**: `nimcp_population_coding.h`
**Biological Basis**: Neural encoding as likelihood mapping

**FEP → Encoding**:
- Precision → encoding granularity (sparse vs. dense coding)
- Hierarchy level → encoding type selection (rate/temporal/population/oscillation)
- Expected states → encoding priors for population vectors

**Encoding → FEP**:
- Vector sum results → low-level observations
- Synchrony metrics → precision estimates
- Sparse code overlap → observation similarity
- PCA projections → hierarchical observations

**Key Integration Points**:
- `population_coding_encode_sparse()` - precision determines sparsity target
- `population_coding_compute_synchrony()` - synchrony feeds back as precision
- `population_coding_encode_vector_sum()` - population vectors as observations

**Estimated LOC**: Header 550, Implementation 420, Tests 650

---

#### 3. Events (Event Bus)
**Module**: `nimcp_event_bus.h`
**Biological Basis**: Event detection as significant prediction error thresholding

**FEP → Events**:
- Prediction error magnitude → event detection threshold
- Precision → event priority assignment
- Surprise → event generation trigger
- Expected events → event filtering

**Events → FEP**:
- Event occurrences → discrete observations
- Event frequency → temporal precision estimate
- Dropped events → capacity constraints
- Event types → categorical observations

**Key Integration Points**:
- `event_bus_publish()` - PE-gated event generation
- Event priorities - precision-weighted
- Queue overflow - capacity constraint feedback

**Estimated LOC**: Header 480, Implementation 380, Tests 580

---

#### 4. Integration (Multimodal Integration)
**Module**: `nimcp_multimodal_integration.h`
**Biological Basis**: Multi-modal integration as hierarchical inference

**FEP → Integration**:
- Hierarchy level → modality selection (which to integrate)
- Precision weights → modality-specific attention
- Cross-modal predictions → expected integration patterns
- Integration method selection based on uncertainty

**Integration → FEP**:
- Integrated features → multi-modal observations
- Modality conflicts → prediction errors
- Attention weights → learned precision per modality
- Integration quality → observation reliability

**Key Integration Points**:
- `multimodal_integrate()` - precision-weighted fusion
- `multimodal_get_attention()` - learned precision feedback
- Modality conflicts - prediction error generation

**Estimated LOC**: Header 520, Implementation 410, Tests 620

---

#### 5. Memory (Gradient Manager)
**Module**: `nimcp_gradient_manager.h`
**Biological Basis**: Memory operations as belief storage and retrieval

**FEP → Memory**:
- Prediction errors → gradient signals
- Precision → learning rate / gradient scaling
- Free energy → consolidation trigger
- Belief updates → gradient accumulation control

**Memory → FEP**:
- Gradient statistics → belief update metrics
- NaN/Inf detection → instability/surprise signals
- Accumulation state → temporal integration progress
- Gradient norms → update magnitude (PE proxy)

**Key Integration Points**:
- Gradient accumulation - temporal belief integration
- Clipping - precision-based gradient bounds
- Health checks - instability as surprise

**Estimated LOC**: Header 510, Implementation 430, Tests 640

---

#### 6. Normalization (Z-Score Normalizer)
**Module**: `nimcp_zscore_normalizer.h`
**Biological Basis**: Divisive normalization as precision normalization

**FEP → Normalization**:
- Precision → target variance for normalization
- Expected statistics → normalization priors (mean/variance)
- Belief confidence → adaptation rate for running stats
- Precision changes → re-normalization triggers

**Normalization → FEP**:
- Normalized observations → precision-weighted inputs
- Running statistics → observation reliability estimates
- Outliers (beyond N*sigma) → surprise signals
- Variance changes → precision updates

**Key Integration Points**:
- `zscore_normalizer_transform()` - precision-based scaling
- Running variance - observation precision estimate
- Outlier detection - surprise generation

**Estimated LOC**: Header 490, Implementation 390, Tests 590

---

#### 7. Pipeline (Middleware Pipeline)
**Module**: `nimcp_middleware_pipeline.h`
**Biological Basis**: Processing pipeline as hierarchical message passing

**FEP → Pipeline**:
- Hierarchy level → active pipeline stages
- Precision → stage execution thresholds
- Predictions → expected stage outputs
- Planning horizon → pipeline depth

**Pipeline → FEP**:
- Stage outputs → hierarchical observations (one per stage)
- Execution failures → prediction errors
- Processing times → computational cost estimates
- Stage dependencies → hierarchical structure

**Key Integration Points**:
- `middleware_pipeline_execute()` - hierarchical processing
- Stage enables/disables - hierarchy-level gating
- Execution stats - cost/benefit for active inference

**Estimated LOC**: Header 540, Implementation 450, Tests 670

---

#### 8. Training (Brain Training Integration)
**Module**: `nimcp_brain_training_integration.h`
**Biological Basis**: Training as generative model optimization

**FEP → Training**:
- Free energy → loss function (F = Loss)
- Prediction errors → gradients (∂F/∂θ)
- Precision → learning rate modulation
- EFE → policy selection for learning
- Hierarchy updates → layer-wise learning rates

**Training → FEP**:
- Loss values → free energy estimates
- Gradients → belief update signals
- Convergence → belief stability indicator
- Divergence → surprise / model failure
- Training steps → belief update iterations

**Key Integration Points**:
- Loss = Free Energy (direct equivalence)
- Gradients = Prediction Errors
- Learning rate = Precision-weighted
- Convergence = FEP equilibrium

**Estimated LOC**: Header 600, Implementation 520, Tests 750

---

## Implementation Checklist

### For Each Module:

- [ ] **Header File** (`nimcp_{module}_fep_bridge.h`):
  - [ ] File header with WHAT/WHY/HOW and biological basis
  - [ ] Constants (#define for thresholds, ranges)
  - [ ] Config struct with enable flags and sensitivities
  - [ ] Effects struct (FEP → Module)
  - [ ] State struct (current state)
  - [ ] Stats struct (runtime statistics)
  - [ ] Bridge struct (opaque pointer pattern)
  - [ ] Lifecycle API (default_config, create, destroy)
  - [ ] Connection API (connect_module, connect_fep, disconnect)
  - [ ] FEP → Module direction functions (3-5)
  - [ ] Module → FEP direction functions (3-5)
  - [ ] Update cycle (update function)
  - [ ] State/Stats API (get_state, get_stats)
  - [ ] Bio-async API (connect/disconnect/is_connected)

- [ ] **Implementation File** (`nimcp_{module}_fep_bridge.c`):
  - [ ] All includes (module, FEP, utils)
  - [ ] Lifecycle implementations
  - [ ] Connection implementations
  - [ ] Bidirectional integration implementations
  - [ ] Update cycle implementation
  - [ ] State/Stats implementations
  - [ ] Bio-async implementations
  - [ ] Proper error handling (guard clauses)
  - [ ] Thread safety (mutex usage)
  - [ ] Logging (NIMCP_LOGGING_*)
  - [ ] Memory management (nimcp_malloc/free)

- [ ] **Test File** (`test_{module}_fep_bridge.cpp`):
  - [ ] Test fixture class
  - [ ] Lifecycle tests
  - [ ] Connection tests
  - [ ] FEP → Module direction tests
  - [ ] Module → FEP direction tests
  - [ ] Update cycle tests
  - [ ] State/Stats tests
  - [ ] Bio-async tests
  - [ ] Edge case tests
  - [ ] Error handling tests

- [ ] **CMakeLists.txt Updates**:
  - [ ] Add source to `src/middleware/{module}/CMakeLists.txt`
  - [ ] Add test executable to `test/unit/middleware/{module}/CMakeLists.txt`
  - [ ] Add test to CTest

- [ ] **Bio-Async Integration**:
  - [ ] Add `BIO_MODULE_FEP_{MODULE}_BRIDGE` to `nimcp_bio_messages.h`
  - [ ] Implement registration in bridge
  - [ ] Test bio-async connectivity

## Build Integration

### Bio-Async Module IDs

Add to `/home/bbrelin/nimcp/include/async/nimcp_bio_messages.h` around line 250:

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

### CMakeLists.txt Pattern

For each `src/middleware/{module}/CMakeLists.txt`:
```cmake
# Existing sources
set(MODULE_SOURCES
    # existing files...
)

# Add FEP bridge
list(APPEND MODULE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/nimcp_{module}_fep_bridge.c
)
```

For each `test/unit/middleware/{module}/CMakeLists.txt`:
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

## Testing Strategy

### Test Categories (per module):

1. **Lifecycle Tests** (5-7 tests):
   - Create with default config
   - Create with custom config
   - Destroy (NULL safe)
   - Multiple create/destroy cycles
   - Config validation

2. **Connection Tests** (4-6 tests):
   - Connect module
   - Connect FEP
   - Connect both
   - Disconnect
   - Double connect handling

3. **FEP → Module Tests** (8-12 tests):
   - Precision modulation
   - Hierarchy selection
   - Expected state priming
   - Parameter scaling
   - Threshold adjustment
   - Edge cases (precision 0, 1, >1)

4. **Module → FEP Tests** (8-12 tests):
   - Report observations
   - Report prediction errors
   - Report statistics
   - Report surprises
   - State feedback
   - Edge cases

5. **Update Cycle Tests** (3-5 tests):
   - Basic update
   - Update with connections
   - Update without connections
   - Update timing
   - Update stats accumulation

6. **State/Stats Tests** (4-6 tests):
   - Get state
   - Get stats
   - Stats accumulation
   - State changes over time

7. **Bio-Async Tests** (3-5 tests):
   - Connect bio-async
   - Disconnect bio-async
   - Check connection status
   - Message handling (if applicable)

**Total per module: 35-53 tests**

## Documentation

Each bridge should include:

1. **Header documentation**:
   - Comprehensive file header with biological basis
   - ASCII art architecture diagram
   - All public API documented with WHAT/WHY/HOW
   - Parameter descriptions
   - Return value descriptions
   - Complexity notes where relevant

2. **Implementation comments**:
   - Function-level WHAT/WHY/HOW
   - Complex algorithm explanations
   - Biological justifications
   - Edge case handling notes

3. **Test documentation**:
   - Test purpose comments
   - Expected behavior descriptions
   - Edge case coverage notes

## Estimated Effort

| Module | Header | Implementation | Tests | CMake | Total |
|--------|--------|---------------|-------|-------|-------|
| encoding | 2h | 2h | 3h | 0.5h | 7.5h |
| events | 1.5h | 1.5h | 2.5h | 0.5h | 6h |
| integration | 2h | 2h | 3h | 0.5h | 7.5h |
| memory | 2h | 2h | 3h | 0.5h | 7.5h |
| normalization | 1.5h | 1.5h | 2.5h | 0.5h | 6h |
| pipeline | 2h | 2.5h | 3.5h | 0.5h | 8.5h |
| training | 2.5h | 3h | 4h | 0.5h | 10h |
| **Total** | **13.5h** | **14.5h** | **21.5h** | **3.5h** | **53h** |

Add 10-15% for integration testing and debugging: **58-61 hours total**

## Quality Standards

All implementations must meet:

- ✅ NIMCP coding standards (CLAUDE.md)
- ✅ Functions < 50 lines
- ✅ Guard clauses (early returns)
- ✅ WHAT/WHY/HOW comments on all functions
- ✅ Thread safety via mutex
- ✅ Memory safety (nimcp_malloc/free)
- ✅ Proper error handling
- ✅ Consistent API patterns
- ✅ Complete test coverage (>30 tests)
- ✅ Bio-async integration
- ✅ Statistics tracking
- ✅ State inspection API

## Next Steps

1. Complete **buffering** module:
   - Write tests (~30-40 tests)
   - Update CMakeLists.txt
   - Verify builds and tests pass

2. Implement **encoding** module (population coding):
   - Most complex due to multiple encoding types
   - Critical for sensory processing
   - ~7.5 hours

3. Implement **training** module:
   - Direct FEP ↔ Training equivalence
   - Largest module
   - ~10 hours

4. Implement remaining 5 modules:
   - events, integration, memory, normalization, pipeline
   - ~33 hours total

5. Integration testing:
   - Cross-module FEP coordination
   - End-to-end FEP inference with middleware
   - ~8 hours

6. Documentation and cleanup:
   - Update CLAUDE.md
   - Create usage examples
   - ~3 hours

**Total estimated completion time: 60-65 hours**

## References

- **Buffering bridge** (complete reference):
  - `/home/bbrelin/nimcp/include/middleware/buffering/nimcp_circular_buffer_fep_bridge.h`
  - `/home/bbrelin/nimcp/src/middleware/buffering/nimcp_circular_buffer_fep_bridge.c`

- **Existing FEP bridges** (for patterns):
  - `/home/bbrelin/nimcp/include/middleware/features/nimcp_feature_extractor_fep_bridge.h`
  - `/home/bbrelin/nimcp/include/middleware/routing/nimcp_thalamic_router_fep_bridge.h`
  - `/home/bbrelin/nimcp/include/middleware/patterns/nimcp_sequence_detector_fep_bridge.h`

- **FEP System**:
  - `/home/bbrelin/nimcp/include/cognitive/free_energy/nimcp_free_energy.h`

- **Module APIs**:
  - Circular buffer: `include/middleware/buffering/nimcp_circular_buffer.h`
  - Population coding: `include/middleware/encoding/nimcp_population_coding.h`
  - Event bus: `include/middleware/events/nimcp_event_bus.h`
  - Multimodal integration: `include/core/integration/nimcp_multimodal_integration.h`
  - Gradient manager: `include/middleware/memory/nimcp_gradient_manager.h`
  - Z-score normalizer: `include/middleware/normalization/nimcp_zscore_normalizer.h`
  - Middleware pipeline: `include/middleware/pipeline/nimcp_middleware_pipeline.h`
  - Brain training: `include/middleware/training/nimcp_brain_training_integration.h`

---

## Status

**Current**: 1/8 modules complete (buffering)
**Remaining**: 7 modules + integration testing
**Estimated completion**: 58-61 hours

The implementation guide (`FEP_MIDDLEWARE_BRIDGES_IMPLEMENTATION_GUIDE.md`) provides detailed templates and patterns for completing the remaining modules.
