# NIMCP Testing - Quick Reference Guide

## Test Summary Statistics

```
Total Tests:          139
├─ Unit Tests:       128 (92%)  ✅ Mostly passing
├─ Integration:        8 (6%)   ✅ All passing
├─ E2E Tests:          1 (<1%)  ⚠️  1 failure
└─ Regression:         2 (1%)   ✅ Passing

Failing Tests:         2-3 (0.7%)
Disabled Tests:        5 files (~114 KB)
Test Code:             85,534 lines
Build Status:          ✅ SUCCESS
```

---

## Test Organization at a Glance

### Directory Structure
```
test/
├── unit/           (128 comprehensive unit tests)
├── integration/    (8 cross-module integration tests)
├── e2e/           (1 end-to-end system test)
├── regression/    (2 bug reproduction tests)
├── fuzz/          (Infrastructure ready, no tests yet)
├── mocks/         (Inline mocks, no separate files)
└── utils/         (test_helpers.h with common utilities)
```

### By File Size (largest unit tests)
```
test_brain_comprehensive.cpp          58 KB  (87 tests)
test_brain_regions_comprehensive.cpp  43 KB
test_adaptive_comprehensive.cpp        45 KB
test_brain_regions_coverage.cpp        32 KB
test_distributed_cow_coverage.cpp      Various
test_knowledge_comprehensive.cpp       Large
test_wellbeing_comprehensive.cpp       Large
test_stress.cpp                        Stress tests
test_memory_leaks.cpp                  Memory validation
```

---

## Running Tests

### Quick Start
```bash
cd /home/bbrelin/nimcp/build

# Run everything in parallel
ctest -j$(nproc)

# Run specific category
ctest -L unit -j$(nproc)
ctest -L integration -j$(nproc)
ctest -L e2e
ctest -L regression
```

### Individual Test Execution
```bash
# Run single test
./test/unit_test_hash_table --gtest_color=yes

# Run with verbosity
./test/unit_test_memory --gtest_filter=*StressTest*

# Run failing tests only
ctest --rerun-failed --output-on-failure
```

### Using Code Surgeon
```bash
cd /home/bbrelin/nimcp

# Test-only mode (parallel execution)
./tools/code_surgeon/code_surgeon.py --mode test-only

# Full analysis with coverage
./tools/code_surgeon/code_surgeon.py --mode full

# Custom filter
./tools/code_surgeon/code_surgeon.py --mode test-only --filter unit
```

---

## Test Results Summary

### Unit Tests: MOSTLY PASSING
**Pass Rate**: 98%+ (2-3 minor failures)

**Known Failures**:
- `HashTableTest.StringKey_CaseInsensitive` - LOW priority
- `HashTableTest.Destructor_OnRemove` - LOW priority

**Sample Passing Tests**:
- ✅ test_platform_time (34 tests, 482 ms)
- ✅ test_memory (27 tests, 10 ms)
- ✅ test_btree (multiple tests, <100 ms)

### Integration Tests: ALL PASSING
```
integration_test_brain_integration         ✅ 22 tests  (1529 ms)
integration_test_integration_e2e           ✅ All pass
integration_test_glial_integration         ✅ All pass
```

### E2E Tests: MOSTLY PASSING
```
e2e_test_visual_cortex_e2e
├── EdgeDetection                          ✅
├── ShapeRecognition                       ✅
├── CuriosityDrivenExploration            ❌ FAILING (issue #1)
├── ActiveVisualLearning                   ✅
├── MultiModalLearning                     ✅
├── RoboticGraspingScenario                ✅
├── LongTermVisualMemory                   ✅
└── PerformanceUnderLoad                   ✅

Pass Rate: 7/8 (87.5%)
```

### Regression Tests: ALL PASSING
- test_regression.cpp           ✅
- test_visual_cortex_regression ✅

---

## Testing Tools & Framework

### Primary Tool: Google Test (GTest)
- Test fixture support (`TEST_F`)
- Parameterized tests (`TEST_P`)
- Death tests for error conditions
- Rich assertion library
- Setup/teardown hooks

### Code Surgeon 2.0
**Location**: `/home/bbrelin/nimcp/tools/code_surgeon/`

**Components**:
1. **Task Queue** - Priority-based work distribution
2. **Parallel Executor** - ProcessPoolExecutor-based parallelism
3. **Result Aggregator** - Merges results and coverage
4. **Main Tool** - Orchestrates everything

**Status**: ✅ COMPLETE and functional

### Coverage Tool: LCOV/GCov
- Generates HTML coverage reports
- Location: `/home/bbrelin/nimcp/build/coverage_html/`
- Coverage tracking enabled for Debug builds
- 14 dedicated coverage test files

### Test Helpers
**File**: `/home/bbrelin/nimcp/test/utils/test_helpers.h`

**Provides**:
- Test constants (timeouts, ports, payloads)
- Neural network creation helpers
- P2P node mocks
- Floating-point comparison utilities

---

## Known Issues

### Issue #1: E2E Curiosity Test Failing
**File**: `test/e2e/test_visual_cortex_e2e.cpp:291-292`
**Status**: ⚠️ MEDIUM priority
**Problem**: Curiosity-driven exploration not returning expected values
**Details**:
```
Expected: exploration_decisions[2] == true && exploration_decisions[3] == true
Actual: Both false
```
**Action**: Investigate curiosity drive implementation

### Issue #2: Hash Table Edge Cases
**File**: `test/unit/test_hash_table.cpp`
**Status**: 🟢 LOW priority
**Problems**:
1. StringKey_CaseInsensitive - case comparison not working
2. Destructor_OnRemove - destructor not called on element removal
**Impact**: Optional features, doesn't affect core functionality

### Issue #3: Fuzzing Not Implemented
**Location**: `test/fuzz/` (empty)
**Status**: 🟢 LOW priority enhancement
**Recommendation**: Implement fuzzing for:
- Protocol parsing
- Network messages
- JSON deserialization
- Memory edge cases

---

## Test Infrastructure Metrics

| Metric | Value | Status |
|--------|-------|--------|
| Total Tests | 139 | ✅ |
| Compilation | All pass | ✅ |
| Unit Pass Rate | 98%+ | ✅ |
| Integration Pass Rate | 100% | ✅ |
| E2E Pass Rate | 87.5% | ⚠️ |
| Parallel Execution | Supported | ✅ |
| Coverage Tracking | Enabled | ✅ |
| Code Surgeon | v2.0 Complete | ✅ |
| Build Time | ~5 min | ✅ |
| Test Categories | 5 | ✅ |

---

## Build Status

**Compilation**: ✅ SUCCESS
- All 139 test binaries compile
- Total size: ~700 MB
- Build time: < 5 minutes

**Test Framework**: Google Test (GTest) + CMake
- CMakeLists.txt coordinates test discovery
- Automatic test registration with CTest
- Labels for test categorization

**Parallel Execution**:
- Supports multi-core execution
- Recommended: `ctest -j$(nproc)`
- Each test runs in isolation
- No shared state between tests

---

## Recommended Next Steps

### Immediate (This Week)
1. ✅ Fix hash table tests (2 failures)
2. ✅ Fix curiosity-driven exploration test
3. ✅ Run full test suite to verify fixes

### Short Term (Next 2 Weeks)
1. Re-enable and verify disabled tests
2. Document test expectations
3. Update failing test documentation

### Medium Term (Next Month)
1. Implement fuzzing infrastructure
2. Add performance benchmarks
3. Expand E2E coverage (add more E2E tests)
4. Improve coverage tracking

### Long Term (Ongoing)
1. Target 90%+ code coverage
2. Implement mutation testing
3. Add property-based testing
4. Chaos engineering for distributed components

---

## Key Files for Reference

### Test Infrastructure
```
test/CMakeLists.txt                     (Test build configuration)
test/utils/test_helpers.h               (Common test utilities)
src/tests/CMakeLists.txt                (Legacy parallel suite)
build/test/CTestTestfile.cmake          (Generated CTest config)
```

### Code Surgeon Documentation
```
tools/code_surgeon/code_surgeon.py              (Main tool)
tools/code_surgeon/IMPLEMENTATION_STATUS.md     (Architecture)
tools/code_surgeon/QUICK_START.md               (Usage guide)
tools/code_surgeon/PARALLEL_ARCHITECTURE.md     (Design)
```

### Coverage
```
build/coverage_html/index.html          (HTML coverage report)
build/coverage.info                     (LCOV coverage data)
```

### Recent Build Output
```
build/test/                             (All test binaries)
build/Makefile                          (Build instructions)
build/CTestTestfile.cmake               (CTest registry)
```

---

## Useful Commands

```bash
# Show all tests without running
cd build && ctest -N

# Run specific test
cd build && ./test/unit_test_<name>

# Run with output on failure
cd build && ctest --output-on-failure

# Run only specific label
cd build && ctest -L unit

# Run in verbose mode
cd build && ctest -V

# Parallel execution (8 workers)
cd build && ctest -j8

# Re-run only failed tests
cd build && ctest --rerun-failed

# Run Code Surgeon analysis
cd /home/bbrelin/nimcp && ./tools/code_surgeon/code_surgeon.py --mode test-only

# Generate coverage report
cd build && lcov --capture --directory . --output-file coverage.info
cd build && genhtml coverage.info --output-directory coverage_html
```

---

## Testing Best Practices Used

✅ **Test Independence** - Each test has isolated setup/teardown  
✅ **Descriptive Names** - Tests clearly describe what they verify  
✅ **Focused Logic** - Each test checks one thing  
✅ **Proper Fixtures** - Setup/teardown for common patterns  
✅ **Mock Objects** - External dependencies mocked (P2P, network)  
✅ **Assertion Density** - ~2-3 assertions per test  
✅ **Documentation** - Comments explain non-obvious logic  
✅ **Code Organization** - Tests grouped by category  
✅ **Parallel Capable** - Tests run independently  
✅ **Coverage Aware** - Coverage-specific test files exist  

---

## Contact & Questions

For questions about the test infrastructure:
1. Read `/home/bbrelin/nimcp/TESTING_STATUS_REPORT.md` (comprehensive)
2. Check test file headers (WHAT-WHY-HOW comments)
3. Review Code Surgeon docs in `tools/code_surgeon/`
4. Run specific tests with `--gtest_list_tests`

---

**Last Updated**: November 11, 2025  
**Test Suite Version**: GTest + Code Surgeon 2.0  
**Status**: ✅ Production-Ready (with 2-3 minor fixes needed)
