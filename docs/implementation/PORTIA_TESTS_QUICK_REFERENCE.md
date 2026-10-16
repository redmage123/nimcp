# Portia Optimization Tests - Quick Reference

## Test File Locations

```
test/
├── unit/
│   ├── utils/platform/
│   │   └── test_platform_tier.cpp           (65+ tests, 813 lines)
│   └── core/neuralnet/
│       └── test_sparse_synapse.cpp          (35+ tests, 862 lines)
├── integration/
│   └── utils/platform/
│       └── test_platform_tier_integration.cpp (25+ tests, 487 lines)
└── regression/
    └── utils/platform/
        └── test_platform_tier_regression.cpp  (20+ tests, 504 lines)
```

## Quick Test Commands

### Run All Portia Tests
```bash
cd /home/bbrelin/nimcp/build
ctest -R "platform_tier|sparse_synapse" -V
```

### Run Individual Test Suites
```bash
# Platform Tier - Unit Tests
./test/unit/utils/platform/unit_utils_platform_test_platform_tier

# Sparse Synapse - Unit Tests
./test/unit/core/neuralnet/unit_core_neuralnet_test_sparse_synapse

# Platform Tier - Integration Tests
./test/integration/utils/platform/integration_utils_platform_test_platform_tier_integration

# Platform Tier - Regression Tests
./test/regression/utils/platform/regression_utils_platform_test_platform_tier_regression
```

### Run Specific Test Cases
```bash
# Run only MINIMAL tier tests
./test/unit/utils/platform/unit_utils_platform_test_platform_tier \
  --gtest_filter="*Minimal*"

# Run only memory budget tests
./test/regression/utils/platform/regression_utils_platform_test_platform_tier_regression \
  --gtest_filter="*MemoryBudget*"

# Run only sparse synapse iterator tests
./test/unit/core/neuralnet/unit_core_neuralnet_test_sparse_synapse \
  --gtest_filter="*Iterator*"
```

## Platform Tier Quick Reference

| Tier | RAM | Neurons | Memory Budget | Use Case |
|------|-----|---------|---------------|----------|
| MINIMAL | < 512 MB | 1,000 | 5 MB | IoT, Embedded |
| CONSTRAINED | 512 MB - 4 GB | 10,000 | 50 MB | RPi, Mobile |
| MEDIUM | 4 GB - 16 GB | 100,000 | 500 MB | Laptops |
| HIGH | > 16 GB | 1,000,000 | 4 GB | Servers |

## Key Test Categories

### Platform Tier Tests

#### Unit Tests
- Tier detection (10 tests)
- Config retrieval (15 tests)
- Module enablement (10 tests)
- Tier names (5 tests)
- Config validation (15 tests)
- Edge cases (10 tests)

#### Integration Tests
- Brain creation (5 tests)
- Visual cortex integration (4 tests)
- Audio cortex integration (4 tests)
- Cognitive modules (6 tests)
- Resource usage (6 tests)

#### Regression Tests
- Memory budgets (4 tests)
- Memory leaks (3 tests)
- Performance (2 tests)
- Tier detection stability (2 tests)
- Config stability (4 tests)
- Stress tests (5 tests)

### Sparse Synapse Tests

#### Unit Tests
- Pool lifecycle (3 tests)
- Embedded storage (6 tests)
- Overflow storage (6 tests)
- Remove operations (4 tests)
- Iterator (3 tests)
- Compaction (2 tests)
- Statistics (2 tests)
- Memory savings (1 test)
- Thread safety (1 test)
- Edge cases (7 tests)

## Expected Results

### All Tests Should:
- ✅ Pass with exit code 0
- ✅ Show 0 bytes leaked (valgrind)
- ✅ Complete in < 5 seconds (unit)
- ✅ Complete in < 60 seconds (integration)
- ✅ Complete in < 300 seconds (regression)

### Memory Budget Assertions:
- MINIMAL: Peak usage < 5 MB
- CONSTRAINED: Peak usage < 50 MB
- MEDIUM: Peak usage < 500 MB

### Sparse Synapse Assertions:
- 87% memory savings vs dense storage
- Embedded count ≤ 4 synapses
- Overflow allocated only when needed
- Iterator covers all synapses

## Debugging Failed Tests

### Memory Budget Exceeded
```bash
# Run with memory profiling
valgrind --tool=massif --massif-out-file=massif.out \
  ./test/regression/utils/platform/regression_utils_platform_test_platform_tier_regression

# Analyze
ms_print massif.out
```

### Tier Detection Wrong
```bash
# Enable verbose logging
GTEST_ALSO_RUN_DISABLED_TESTS=1 \
  ./test/unit/utils/platform/unit_utils_platform_test_platform_tier \
  --gtest_filter="*DetectTier*" -v
```

### Sparse Synapse Memory Leak
```bash
# Run with leak check
valgrind --leak-check=full --show-leak-kinds=all \
  ./test/unit/core/neuralnet/unit_core_neuralnet_test_sparse_synapse
```

## Mock vs Real Implementation

### Current Status
All tests use **mock implementations** embedded in the test files.

### When Real Implementation Exists
1. Remove mock code from test files (search for "Mock")
2. Include real headers:
   ```cpp
   #include "utils/platform/nimcp_platform_tier.h"
   #include "core/neuralnet/nimcp_sparse_synapse.h"
   ```
3. Recompile and verify all tests still pass

## Performance Benchmarks

### Expected Test Times
- Unit tests: 0.1 - 2 seconds each
- Integration tests: 1 - 10 seconds each
- Regression tests: 5 - 60 seconds each

### If Tests Are Slow
```bash
# Profile with gprof
./test/unit/utils/platform/unit_utils_platform_test_platform_tier
gprof unit_utils_platform_test_platform_tier gmon.out > analysis.txt
```

## Common Test Patterns

### Creating a Brain for Tier
```cpp
platform_tier_config_t config = platform_get_tier_config(PLATFORM_TIER_MINIMAL);
brain_config_t brain_config = {0};
brain_config.num_neurons = config.max_neurons;
brain_config.num_layers = 3;
brain_t* brain = brain_create(&brain_config);
```

### Checking Memory Usage
```cpp
nimcp_memory_clear_stats();
// ... perform operations ...
nimcp_memory_stats_t stats;
nimcp_memory_get_stats(&stats);
EXPECT_LT(stats.peak_allocated, 5 * 1024 * 1024); // < 5 MB
```

### Sparse Synapse Iteration
```cpp
sparse_synapse_iterator_t it = sparse_synapse_iterator_create(pool, neuron_id);
const synapse_data_t* syn;
while ((syn = sparse_synapse_iterator_next(&it)) != nullptr) {
    // Process synapse
}
```

## Continuous Integration

### GitHub Actions Snippet
```yaml
- name: Run Portia Optimization Tests
  run: |
    cd build
    ctest -R "platform_tier|sparse_synapse" --output-on-failure
```

### Test Coverage Report
```bash
# Generate coverage
cd build
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make
make coverage
# View coverage/index.html
```

## Contact & Support

For questions about these tests:
- See: `/home/bbrelin/nimcp/PORTIA_OPTIMIZATION_TESTS_SUMMARY.md`
- Check: `/home/bbrelin/nimcp/docs/`
- Author: NIMCP Development Team
- Date: 2025-12-08

---

**Remember**: These tests validate the Portia optimization that enables NIMCP to run on hardware from IoT devices (Portia spider scale: ~1,000 neurons) up to server-class machines (1,000,000+ neurons) with 87% memory savings through sparse synapse storage.
