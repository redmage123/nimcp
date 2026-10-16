# NIMCP Code Coverage Report

**Generated:** 2025-11-15
**Current Overall Coverage:** 36.8% (16,628 / 45,203 lines)

## Coverage Summary

- **Line Coverage:** 36.8% (16,628 out of 45,203 lines)
- **Function Coverage:** 53.7% (1,626 out of 3,027 functions)
- **Branch Coverage:** 21.4% (7,656 out of 35,698 branches)

## Coverage by Category

### High Coverage Modules (>80%)
- `src/utils/containers/nimcp_btree.c` - 82% (314/382 lines)
- `src/utils/containers/nimcp_graph.c` - 84% (272/322 lines)
- `src/utils/containers/nimcp_vector.c` - 95% (68/71 lines)
- `src/utils/platform/nimcp_platform_rwlock.c` - 100% (32/32 lines)
- `src/utils/platform/nimcp_platform_thread.c` - 100% (11/11 lines)
- `src/utils/platform/nimcp_platform_once.c` - 100% (4/4 lines)
- `src/utils/platform/nimcp_platform_mutex.c` - 96% (26/27 lines)
- `src/utils/platform/nimcp_platform_cond.c` - 93% (30/32 lines)

### Medium Coverage Modules (40-79%)
- `src/utils/spectral/nimcp_fft.c` - 75% (228/304 lines)
- `src/utils/json/nimcp_json.c` - 72% (221/303 lines)
- `src/utils/logging/nimcp_logging.c` - 75% (42/56 lines)
- `src/utils/validation/nimcp_validate.c` - 72% (155/215 lines)
- `src/utils/platform/nimcp_platform_time.c` - 81% (27/33 lines)
- `src/utils/time/nimcp_time.c` - 70% (34/48 lines)
- `src/utils/containers/nimcp_hash_table.c` - 58% (248/423 lines)
- `src/utils/queue_manager/nimcp_queue_manager.c` - 60% (147/243 lines)
- `src/utils/thread/nimcp_thread_pool.c` - 55% (62/111 lines)
- `src/utils/memory/nimcp_memory.c` - 53% (335/629 lines)
- `src/security/nimcp_security.c` - 53% (360/675 lines)
- `src/utils/containers/nimcp_min_heap.c` - 52% (64/123 lines)
- `src/utils/containers/nimcp_queue.c` - 52% (69/132 lines)

### Low Coverage Modules (1-39%)
- `src/utils/thread/nimcp_thread.c` - 39% (133/335 lines)
- `src/utils/cache/nimcp_cache.c` - 16% (47/281 lines)

### Zero Coverage Modules (0%)

#### Utilities (0% coverage)
- `src/utils/config/nimcp_config.c` - 0/219 lines
- `src/utils/config/nimcp_dynamic_config.c` - 0/292 lines
- `src/utils/error/nimcp_error_codes.c` - 0/137 lines
- `src/utils/geometry/nimcp_hyperbolic.c` - 0/212 lines
- `src/utils/memory/nimcp_memory_guards.c` - 0/222 lines
- `src/utils/metrics/nimcp_metrics.c` - 0/341 lines
- `src/utils/numerical/nimcp_integration.c` - 0/240 lines
- `src/utils/platform/nimcp_platform.c` - 0/6 lines
- `src/utils/signal/nimcp_signal_handler.c` - 0/190 lines
- `src/utils/tensor_networks/nimcp_mps.c` - 0/257 lines
- `src/utils/thread/nimcp_deadlock_detector.c` - 0/288 lines
- `src/utils/quantum/nimcp_quantum_shannon.c` - 0/299 lines
- `src/utils/quantum/nimcp_quantum_walk.c` - 0/285 lines
- `src/utils/quantum/nimcp_quantum_walk.h` - 0/4 lines

#### Cognitive Modules (0% coverage)
- Grief/Joy modules - 0/247-378 lines each
- Memory modules (engram, semantic, wm_transfer) - 0/74-320 lines
- Executive function modules - 0/160+ lines
- Multiple other cognitive functions

#### Core Modules (0% coverage)
- Various brain subsystems not yet tested

## Priority Areas for Coverage Improvement

### P0 - Critical Infrastructure (Target: 90%+)
These are foundational modules that other code depends on:

1. **Configuration System** (0% → 90%)
   - `src/utils/config/nimcp_config.c` (219 lines)
   - `src/utils/config/nimcp_dynamic_config.c` (292 lines)
   - **Impact:** Configuration is used throughout the system
   - **Test Files Needed:** `test/unit/utils/config/test_config.cpp`

2. **Error Handling** (0% → 90%)
   - `src/utils/error/nimcp_error_codes.c` (137 lines)
   - **Impact:** Error handling affects all modules
   - **Test Files Needed:** `test/unit/utils/error/test_error_codes.cpp`

3. **Memory Guards** (0% → 90%)
   - `src/utils/memory/nimcp_memory_guards.c` (222 lines)
   - **Impact:** Critical for memory safety
   - **Test Files Needed:** `test/unit/utils/memory/test_memory_guards.cpp`

4. **Platform Abstraction** (0% → 90%)
   - `src/utils/platform/nimcp_platform.c` (6 lines)
   - **Impact:** Cross-platform compatibility
   - **Test Files Needed:** `test/unit/utils/platform/test_platform.cpp`

### P1 - High-Value Features (Target: 75%+)
These modules provide important functionality:

1. **Metrics & Monitoring** (0% → 75%)
   - `src/utils/metrics/nimcp_metrics.c` (341 lines)
   - **Test Files Needed:** `test/unit/utils/metrics/test_metrics.cpp`

2. **Signal Handling** (0% → 75%)
   - `src/utils/signal/nimcp_signal_handler.c` (190 lines)
   - **Test Files Needed:** `test/unit/utils/signal/test_signal_handler.cpp`

3. **Deadlock Detection** (0% → 75%)
   - `src/utils/thread/nimcp_deadlock_detector.c` (288 lines)
   - **Test Files Needed:** `test/unit/utils/thread/test_deadlock_detector.cpp`

4. **Cache** (16% → 75%)
   - `src/utils/cache/nimcp_cache.c` (281 lines, currently 47 covered)
   - **Test Files Needed:** Expand `test/unit/utils/cache/test_cache.cpp`

### P2 - Specialized Features (Target: 60%+)
Advanced features that are important but less critical:

1. **Quantum Computing** (0% → 60%)
   - `src/utils/quantum/nimcp_quantum_shannon.c` (299 lines)
   - `src/utils/quantum/nimcp_quantum_walk.c` (285 lines)
   - **Test Files Needed:** `test/unit/utils/quantum/test_quantum_*.cpp`

2. **Tensor Networks** (0% → 60%)
   - `src/utils/tensor_networks/nimcp_mps.c` (257 lines)
   - **Test Files Needed:** `test/unit/utils/tensor_networks/test_mps.cpp`

3. **Numerical Integration** (0% → 60%)
   - `src/utils/numerical/nimcp_integration.c` (240 lines)
   - **Test Files Needed:** `test/unit/utils/numerical/test_integration.cpp`

4. **Hyperbolic Geometry** (0% → 60%)
   - `src/utils/geometry/nimcp_hyperbolic.c` (212 lines)
   - **Test Files Needed:** `test/unit/utils/geometry/test_hyperbolic.cpp`

### P3 - Cognitive Modules (Target: 50%+)
Higher-level cognitive functions:

1. **Memory Systems** (0% → 50%)
   - Engram formation (315 lines)
   - Semantic memory (320 lines)
   - Working memory transfer (74 lines)

2. **Emotional Systems** (0% → 50%)
   - Joy/Euphoria (247 lines)
   - Grief/Loss (378 lines)

3. **Executive Functions** (0% → 50%)
   - Various executive function modules

## Recommended Testing Strategy

### Phase 1: Foundation (Week 1-2)
**Goal:** Achieve 50% overall coverage by testing critical infrastructure

1. Create tests for configuration system
2. Create tests for error handling
3. Create tests for memory guards
4. Create tests for platform abstraction
5. Expand existing cache tests

**Expected Result:** ~50% overall coverage

### Phase 2: High-Value Features (Week 3-4)
**Goal:** Achieve 60% overall coverage

1. Create tests for metrics/monitoring
2. Create tests for signal handling
3. Create tests for deadlock detection
4. Improve thread pool coverage
5. Improve hash table coverage

**Expected Result:** ~60% overall coverage

### Phase 3: Specialized Features (Week 5-6)
**Goal:** Achieve 70% overall coverage

1. Create tests for quantum modules
2. Create tests for tensor networks
3. Create tests for numerical integration
4. Create tests for hyperbolic geometry
5. Improve security module coverage to 75%+

**Expected Result:** ~70% overall coverage

### Phase 4: Cognitive Modules (Week 7-8)
**Goal:** Achieve 80% overall coverage

1. Create tests for memory systems
2. Create tests for emotional systems
3. Create tests for executive functions
4. Integration tests for cognitive pipelines

**Expected Result:** ~80% overall coverage

### Phase 5: Edge Cases & Branches (Week 9-10)
**Goal:** Achieve 90%+ overall coverage

1. Add tests for error paths
2. Add tests for edge cases
3. Add tests for boundary conditions
4. Increase branch coverage to 70%+

**Expected Result:** ~90% overall coverage

## Quick Wins

These modules can be easily tested to boost coverage quickly:

1. **Platform Time** (81% → 95%) - Only 6 lines uncovered
2. **Logging** (75% → 95%) - Only 14 lines uncovered
3. **Vector** (95% → 100%) - Only 3 lines uncovered
4. **Platform Mutex** (96% → 100%) - Only 1 line uncovered
5. **Platform Cond** (93% → 100%) - Only 2 lines uncovered

**Estimated Effort:** 2-4 hours
**Coverage Gain:** +0.5%

## Build Issues to Fix

Some tests have compilation errors that need fixing:
- `test/unit/cognitive/salience/test_salience_comprehensive.cpp` - Type conversion error
- Other salience and cognitive tests may have similar issues

These should be fixed before expanding test coverage in those modules.

## Next Steps

1. Fix existing build errors in test suite
2. Start with Phase 1 (Foundation) tests
3. Run coverage analysis after each phase
4. Adjust priorities based on coverage gains
5. Create integration tests to improve branch coverage

## Tools & Commands

```bash
# Generate coverage report
gcovr --root . --filter 'src/' --exclude '_deps' \\
  --merge-mode-functions=merge-use-line-min \\
  --gcov-ignore-parse-errors all \\
  --html-details coverage.html

# Run specific test category
ctest -L unit -j$(nproc)

# Run specific module tests
ctest -R utils_config

# View coverage for specific file
gcovr --root . --filter 'src/utils/config/' --print-summary
```
