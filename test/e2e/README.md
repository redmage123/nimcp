# NIMCP End-to-End (E2E) Test Framework

**Version**: 1.0.0
**Created**: 2025-11-28
**Purpose**: Comprehensive testing of complete NIMCP pipelines

---

## Overview

The E2E test framework validates complete NIMCP workflows end-to-end, testing real-world usage scenarios across multiple subsystems. Unlike unit tests (which test individual functions) or integration tests (which test module interactions), E2E tests validate entire pipelines from start to finish.

### Key Features

- **Pipeline Stage Tracking**: Automatic timing and validation of each pipeline stage
- **Timeout Assertions**: Configurable timeouts per stage to catch performance regressions
- **Memory Leak Detection**: Automatic leak detection across entire pipelines
- **Result Aggregation**: Statistical analysis across multiple pipeline runs
- **Rich Diagnostics**: Detailed error reporting with timing breakdowns

---

## Framework Components

### 1. Core Framework Files

#### `e2e_test_framework.h`
Header defining the E2E test infrastructure:
- **Pipeline Tracking**: `PipelineTracker` class for multi-stage pipelines
- **Timing Utilities**: High-precision `Timer` class
- **Memory Tracking**: `MemoryLeakDetector` for leak detection
- **Test Data Generation**: `TestDataGenerator` for synthetic data
- **Result Aggregation**: `ResultAggregator` for performance analysis
- **Macros**: Convenient macros (`E2E_TEST`, `E2E_STAGE_BEGIN`, etc.)

#### `e2e_test_framework.cpp`
Implementation of framework utilities (660 lines)

### 2. Test Suites

#### `test_brain_pipeline.cpp` (550+ lines)
Tests complete brain lifecycle:
- **SimpleBrainLifecycle**: Create → Train → Infer → Save → Restore
- **SnapshotWorkflow**: Snapshot creation and restoration
- **BrainProbeStatistics**: State inspection and metrics
- **COWCloning**: Copy-on-write cloning efficiency
- **TrainingCallbacks**: Event-driven training monitoring

#### `test_cognitive_pipeline.cpp` (520+ lines)
Tests cognitive processing pipelines:
- **PerceptionToDecision**: Perception → Workspace → Decision → Response
- **WorkingMemoryManagement**: Working memory operations
- **EthicalDecisionPipeline**: Ethics-constrained decision making
- **MultiModuleCognitiveIntegration**: Full cognitive system integration

#### `test_distributed_pipeline.cpp` (550+ lines)
Tests distributed brain operations:
- **LocalToDistributedTransition**: Local brain → distributed setup
- **BrainStateReplication**: Master/replica synchronization
- **ConcurrentBrainOperations**: Concurrent training and inference
- **COWDistributedReplicas**: Efficient distributed replicas via COW

### 3. Build Configuration

#### `CMakeLists.txt`
CMake configuration for E2E tests:
- Custom build rules (longer timeouts, sequential execution)
- Links full NIMCP library with all dependencies
- Configures test environment (Python paths, LSAN suppressions)

---

## Usage

### Running E2E Tests

```bash
# Build E2E tests
cd /home/bbrelin/nimcp/build
cmake ..
make e2e_test_brain_pipeline e2e_test_cognitive_pipeline e2e_test_distributed_pipeline

# Run all E2E tests
ctest -L e2e

# Run with verbose output
ctest -L e2e -V

# Run specific test suite
./test/e2e/e2e_test_brain_pipeline
```

### Writing New E2E Tests

```cpp
#include "e2e_test_framework.h"

using namespace nimcp::e2e;

E2E_TEST(MyTestSuite, MyPipelineTest) {
    E2E_PIPELINE_START("My Custom Pipeline");

    // Stage 1: Setup
    E2E_STAGE_BEGIN("Setup phase", 100);  // 100ms timeout
    {
        // Setup code...
        nimcp_brain_t brain = nimcp_brain_create(...);
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Execution
    E2E_STAGE_BEGIN("Execution phase", 5000);  // 5s timeout
    {
        // Execution code...
        nimcp_status_t status = nimcp_brain_train_step(...);
        E2E_ASSERT_SUCCESS(status, "Training failed");
    }
    E2E_STAGE_END();

    // Stage 3: Validation
    E2E_STAGE_BEGIN("Validation phase", 200);
    {
        // Validation code...
        float accuracy = compute_accuracy(...);
        E2E_ASSERT(accuracy > 0.8f, "Accuracy too low");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
```

### Framework Macros

| Macro | Purpose |
|-------|---------|
| `E2E_TEST(suite, name)` | Define an E2E test case |
| `E2E_PIPELINE_START(name)` | Start a pipeline with name |
| `E2E_PIPELINE_END()` | End pipeline and verify success |
| `E2E_STAGE_BEGIN(name, timeout_ms)` | Begin a stage with timeout |
| `E2E_STAGE_END()` | End current stage |
| `E2E_STAGE_FAIL(message)` | Fail current stage with error |
| `E2E_ASSERT(cond, msg)` | Assert condition in E2E context |
| `E2E_ASSERT_NOT_NULL(ptr, msg)` | Assert pointer is not NULL |
| `E2E_ASSERT_SUCCESS(status, msg)` | Assert NIMCP status is success |

### Helper Utilities

```cpp
// Generate test data
std::vector<float> features = TestDataGenerator::generate_features(dim);
std::vector<float> one_hot = TestDataGenerator::generate_one_hot(num_classes, label);
TestDataGenerator::generate_training_batch(batch_size, input_dim, output_dim,
                                           features, labels);

// Timing
Timer timer;
timer.start();
// ... code to measure ...
timer.stop();
uint64_t elapsed_ms = timer.elapsed_ms();

// Memory leak detection
MemoryLeakDetector detector;
// ... allocations ...
detector.checkpoint();
bool has_leaks = detector.has_leaks();
size_t leaked_bytes = detector.get_leaked_bytes();

// Result aggregation
ResultAggregator aggregator;
for (int i = 0; i < 100; ++i) {
    aggregator.add_sample("latency_ms", measure_latency());
}
double mean = aggregator.get_mean("latency_ms");
double stddev = aggregator.get_stddev("latency_ms");
```

---

## Test Coverage Summary

### Brain Pipeline Tests (6 tests)
- ✅ Simple lifecycle (create, train, infer, save, restore)
- ✅ Snapshot workflow (save/restore named snapshots)
- ✅ Brain probe and statistics
- ✅ Copy-on-write cloning
- ✅ Training callbacks

### Cognitive Pipeline Tests (4 tests)
- ✅ Perception to decision flow
- ✅ Working memory management
- ✅ Ethical decision making
- ✅ Multi-module integration

### Distributed Pipeline Tests (4 tests)
- ✅ Local to distributed transition
- ✅ Brain state replication
- ✅ Concurrent brain operations
- ✅ COW distributed replicas

**Total: 14 comprehensive E2E tests**

---

## Performance Expectations

E2E tests are designed to catch performance regressions:

| Pipeline Stage | Typical Duration | Max Timeout |
|----------------|------------------|-------------|
| Brain creation | 10-50ms | 100ms |
| Training (100 steps) | 500-2000ms | 5000ms |
| Single inference | 1-10ms | 100ms |
| Save/Load state | 50-200ms | 500ms |
| COW clone | 5-20ms | 50ms |
| Working memory ops | 10-50ms | 200ms |
| Workspace competition | 10-100ms | 200ms |

Tests will fail if operations exceed their configured timeouts, helping catch performance regressions early.

---

## Memory Leak Detection

Every E2E test automatically tracks memory allocations:

```
[E2E Memory] Leak Detection Report:
  Initial allocated: 1024 bytes
  Final allocated:   1024 bytes
  Status: No leaks detected
```

If leaks are detected, tests fail with detailed information about leaked allocations.

---

## Output Example

```
[E2E Pipeline] Starting: Brain Lifecycle
[E2E Stage] BEGIN: Create brain (timeout: 100ms)
[E2E Stage] END: Create brain (12.5ms) [OK]
[E2E Stage] BEGIN: Training (timeout: 5000ms)
[E2E Stage] END: Training (1234.5ms) [OK]
[E2E Stage] BEGIN: Inference (timeout: 100ms)
[E2E Stage] END: Inference (8.2ms) [OK]

[E2E Pipeline] Summary: Brain Lifecycle
  Total stages: 3
  Status: SUCCESS
  Total duration: 1255ms

  Stage breakdown:
    Create brain                      12.5ms  (timeout: 100ms)
    Training                       1234.5ms  (timeout: 5000ms)
    Inference                         8.2ms  (timeout: 100ms)
```

---

## Best Practices

1. **Stage Granularity**: Break pipelines into logical stages (5-10 stages per pipeline)
2. **Timeout Setting**: Set timeouts 2-3x expected duration to avoid flakiness
3. **Cleanup**: Always cleanup resources in final stage
4. **Assertions**: Use specific E2E assertions for better error messages
5. **Data Generation**: Use `TestDataGenerator` for reproducible synthetic data
6. **Memory Tracking**: Enable for all E2E tests to catch leaks early

---

## Integration with CI/CD

E2E tests are designed for CI/CD integration:

```yaml
# Example CI configuration
- name: Run E2E Tests
  run: |
    cd build
    ctest -L e2e --output-on-failure
  timeout-minutes: 10
```

Tests run sequentially (`RUN_SERIAL TRUE`) to avoid resource contention and have extended timeouts (120s default).

---

## Troubleshooting

### Test Timeout
- Check if stage timeout is too aggressive
- Look for performance regression in code
- Verify hardware has sufficient resources

### Memory Leaks
- Review stage-by-stage allocation tracking
- Use `nimcp_memory_dump_allocations()` for detailed leak info
- Check for missing `nimcp_brain_destroy()` calls

### Test Failures
- Run with `-V` flag for verbose output
- Check stage breakdown for which stage failed
- Review error messages from `E2E_ASSERT_*` macros

---

## Future Enhancements

Planned improvements:
- [ ] Async pipeline support with futures
- [ ] Distributed testing across multiple machines
- [ ] Performance regression database
- [ ] Visual test reports (HTML)
- [ ] Automatic bisection for performance regressions

---

## Statistics

- **Total Lines**: ~3,155 lines
- **Test Files**: 6 files
- **Test Coverage**: 14 comprehensive E2E tests
- **Framework Size**: ~1,200 lines (header + implementation)
- **Documentation**: This README + inline comments

---

For questions or issues, refer to the NIMCP development team or file an issue in the project repository.
