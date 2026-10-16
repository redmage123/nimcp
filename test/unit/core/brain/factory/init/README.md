# Brain Initialization Test Suite

Comprehensive test suite for brain factory initialization functions in `nimcp_brain_init.c`.

## Test Coverage

### Functions Tested (7 total)

1. **nimcp_brain_factory_get_neuron_count()** - Returns neuron count for brain size
2. **nimcp_brain_factory_get_default_sparsity()** - Returns default sparsity for brain size
3. **nimcp_brain_factory_build_spike_params()** - Builds adaptive spike parameters
4. **nimcp_brain_factory_build_base_network_config()** - Builds base network configuration
5. **nimcp_brain_factory_build_network_config()** - Builds complete adaptive network config
6. **nimcp_brain_factory_init_brain_config()** - Initializes brain configuration structure
7. **nimcp_brain_factory_init_brain_stats()** - Initializes brain statistics structure

## Test Files

### 1. test_brain_init_config.cpp (459 lines, 26 tests)

**Purpose**: Unit tests for configuration getter functions

**Functions Tested**:
- `nimcp_brain_factory_get_neuron_count()`
- `nimcp_brain_factory_get_default_sparsity()`

**Test Categories**:
- Neuron count tests for all brain sizes (TINY, SMALL, MEDIUM, LARGE, CUSTOM)
- Neuron count ordering and scaling
- Sparsity tests for all brain sizes
- Sparsity range validation (0.0 - 1.0)
- Invalid enum handling
- Consistency checks
- Cross-configuration correlation

**Key Test Cases**:
- `NeuronCount_Tiny` - Verify TINY brain has 100 neurons
- `NeuronCount_Ordering` - Verify monotonic increase across sizes
- `Sparsity_ValidRange` - Ensure all sparsity values are in [0, 1]
- `NeuronCountSparsityCorrelation` - Verify larger networks have higher sparsity

### 2. test_brain_init_builders.cpp (726 lines, 37 tests)

**Purpose**: Unit tests for all builder functions

**Functions Tested**:
- `nimcp_brain_factory_build_spike_params()`
- `nimcp_brain_factory_build_base_network_config()`
- `nimcp_brain_factory_build_network_config()`
- `nimcp_brain_factory_init_brain_config()`
- `nimcp_brain_factory_init_brain_stats()`

**Test Categories**:

**Spike Parameters**:
- Typical, minimum, and maximum sparsity values
- Threshold range validation
- Encoding type verification
- Adaptation settings
- Soft reset configuration

**Base Network Config**:
- Typical parameter configuration
- Plasticity flags (STDP, Hebbian, Oja, homeostasis)
- Scalability flags (BCM, eligibility traces disabled)
- Integration method support (Euler, RK2, RK4)
- Layer architecture validation
- Edge cases (minimal/large dimensions)

**Adaptive Network Config**:
- Complete configuration assembly
- Sparsity parameter propagation
- Integration method verification

**Brain Config**:
- Typical configuration initialization
- Working memory defaults (Miller's 7±2)
- Theory of mind settings
- Mirror neuron configuration
- Personality system defaults
- Biological realism flags
- Quantum features (disabled by default)
- NULL pointer handling

**Brain Stats**:
- Stats initialization for all sizes
- Synapse count calculation
- Learning rate propagation
- Task name handling

### 3. test_brain_init_integration_config.cpp (602 lines, 19 tests)

**Purpose**: Integration tests for complete configuration workflows

**Test Categories**:

**Complete Workflows**:
- End-to-end configuration for all brain sizes
- Multi-step configuration consistency
- Learning rate propagation through all components

**Cross-Function Integration**:
- Spike params embedded in network config
- Layer sizes allocation and initialization
- Synapse calculation consistency
- Task name propagation

**Multi-Configuration**:
- Multiple independent configurations
- Sequential configuration without state pollution
- Thread-safe independent creation

**Advanced Integration**:
- All ODE integration methods (Euler, RK2, RK4)
- Sparsity scaling with brain size
- Memory scaling verification
- Extreme dimension handling
- Plasticity flags consistency
- Cognitive subsystem defaults
- All task type configurations
- Data integrity over repeated creation

## Test Statistics

| File | Lines | Tests | Focus Area |
|------|-------|-------|------------|
| test_brain_init_config.cpp | 459 | 26 | Config getters |
| test_brain_init_builders.cpp | 726 | 37 | Builder functions |
| test_brain_init_integration_config.cpp | 602 | 19 | Integration workflows |
| **Total** | **1,787** | **82** | **All 7 functions** |

## Building and Running Tests

### Prerequisites
- GoogleTest framework installed
- CMake build system configured
- C++17 compiler

### Build Commands
```bash
# From project root
cd build
cmake ..
make

# Run specific test suite
./test/unit_core_brain_factory_init_test_brain_init_config
./test/unit_core_brain_factory_init_test_brain_init_builders
./test/unit_core_brain_factory_init_test_brain_init_integration_config

# Or run all tests
ctest -R brain_init
```

## Test Design Principles

### 1. Comprehensive Coverage
- All brain sizes tested (TINY, SMALL, MEDIUM, LARGE, CUSTOM)
- All ODE integration methods (Euler, RK2, RK4)
- All task types (classification, regression, pattern matching, etc.)
- Edge cases and boundary conditions

### 2. NULL Safety
- All functions tested with NULL pointers where applicable
- Graceful degradation verified
- No segmentation faults

### 3. Memory Management
- Layer sizes allocation verified
- Proper cleanup in test fixtures
- No memory leaks

### 4. Data Validation
- Range checks (sparsity 0.0-1.0)
- Consistency checks across components
- NaN/infinity validation
- Ordering verification

### 5. Integration Testing
- End-to-end workflows
- Cross-function data flow
- Configuration consistency
- State independence

## Key Test Patterns

### Pattern 1: Size Enumeration Testing
```cpp
brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL,
                        BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};
for (brain_size_t size : sizes) {
    // Test each size
}
```

### Pattern 2: Configuration Lifecycle
```cpp
CompleteConfig cfg;
uint32_t num_neurons = nimcp_brain_factory_get_neuron_count(size);
float sparsity = nimcp_brain_factory_get_default_sparsity(size);
nimcp_brain_factory_init_brain_config(...);
nimcp_brain_factory_init_brain_stats(...);
cfg.network_config = nimcp_brain_factory_build_network_config(...);
// Verify consistency across all configs
```

### Pattern 3: NULL Safety Testing
```cpp
nimcp_brain_factory_init_brain_config(nullptr, ...);  // Should not crash
nimcp_brain_factory_init_brain_config(&config, ..., nullptr);  // Should not crash
```

### Pattern 4: Memory Cleanup
```cpp
adaptive_network_config_t config = nimcp_brain_factory_build_network_config(...);
// Use config
if (config.base_config.layer_sizes) {
    nimcp_free((void*)config.base_config.layer_sizes);
}
```

## Expected Test Results

All 82 tests should PASS with:
- No segmentation faults
- No memory leaks (when run with valgrind)
- No undefined behavior
- Consistent output across runs

## Coverage Summary

### By Function:
- `get_neuron_count`: 12 tests
- `get_default_sparsity`: 14 tests
- `build_spike_params`: 8 tests
- `build_base_network_config`: 7 tests
- `build_network_config`: 5 tests
- `init_brain_config`: 12 tests
- `init_brain_stats`: 7 tests
- Integration workflows: 17 tests

### By Category:
- Unit tests: 63 (77%)
- Integration tests: 19 (23%)

### By Test Type:
- Typical cases: 28
- Edge cases: 18
- NULL handling: 6
- Range validation: 12
- Consistency: 18

## Notes

1. **Floating Point Comparison**: All float comparisons use `EXPECT_NEAR` with `FLOAT_EPSILON = 1e-6f`

2. **Memory Management**: Tests use `CompleteConfig` helper class with RAII cleanup for layer_sizes

3. **Mock Strategy**: Uses `mock_get_learning_rate()` returning 0.01f for brain config tests

4. **Thread Safety**: Integration tests verify independent configuration creation (simulating concurrent use)

5. **Documentation**: Each test has WHAT/WHY comments explaining purpose and verification

## Future Enhancements

Potential additions:
- Performance benchmarks for configuration creation
- Stress tests with 1000+ configurations
- Multi-threaded integration tests
- Configuration serialization/deserialization tests
- Error injection tests (simulated allocation failures)

## Related Modules

- `/home/bbrelin/nimcp/src/core/brain/factory/init/nimcp_brain_init.c` - Implementation
- `/home/bbrelin/nimcp/src/core/brain/factory/init/nimcp_brain_init.h` - Header
- `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.h` - Brain types
- `/home/bbrelin/nimcp/src/plasticity/adaptive/nimcp_adaptive.h` - Adaptive network types
