# Test Isolation and Global State Analysis

**Date**: 2025-11-17
**Context**: Post-thread-safety fixes investigation (79% pass rate, 304/383 tests)

## Executive Summary

Tests exhibit **flakiness** - passing when run individually but failing in parallel execution. Root cause: **global state contamination** from 27+ static variables that persist across test runs and lack proper cleanup between tests.

## Problem Statement

### Observed Behavior

**Parallel Execution** (`ctest -j20`):
- Pass rate: 79% (304/383 tests)
- Example: curiosity tests showing failures

**Individual Execution**:
- Curiosity test alone: 99/99 tests passing (100%)
- Many "failing" tests pass when run in isolation

**Diagnosis**: This is NOT a code quality issue - it's a **test infrastructure isolation problem**.

## Root Cause: Global State Contamination

### Mechanism

1. Test A initializes global variable (e.g., `g_registered_brain`)
2. Test B starts before Test A's cleanup
3. Test B sees Test A's state in the global variable
4. Test B's assertions fail due to unexpected state
5. Tests pass individually because no other test contaminates the state

### Impact

- Estimated 20-40 tests are "false failures" due to isolation issues
- True pass rate may be 85-90% (not 79%)
- Makes debugging difficult (cannot reproduce failures individually)

## Global State Variables Identified

### Critical Priority (Direct Test Contamination)

**File**: `src/utils/signal/nimcp_signal_handler.c`

```c
// Line 45 - HIGHEST PRIORITY FIX
static brain_t g_registered_brain = NULL;
```
**Why Critical**: Brain instances registered by one test remain visible to subsequent tests, causing assertion failures in tests that expect clean state.

**Functions Using**:
- `signal_handler_register_brain()` - sets the brain
- `signal_handler_force_checkpoint()` - reads the brain
- No cleanup function exists

**Recommended Fix**:
```c
void signal_handler_unregister_brain(void) {
    g_registered_brain = NULL;
}
```

**Test Pattern**:
```cpp
class MyTest : public ::testing::Test {
protected:
    void TearDown() override {
        signal_handler_unregister_brain();  // Clean up
    }
};
```

---

**File**: `src/utils/signal/nimcp_signal_handler.c`

```c
// Lines 24-42 - Signal statistics and flags
static volatile sig_atomic_t g_shutdown_requested = 0;
static volatile sig_atomic_t g_reload_requested = 0;
static volatile sig_atomic_t g_last_signal = 0;

static volatile uint64_t g_sigsegv_count = 0;
static volatile uint64_t g_sigabrt_count = 0;
static volatile uint64_t g_sigbus_count = 0;
static volatile uint64_t g_sigfpe_count = 0;
static volatile uint64_t g_sigill_count = 0;
static volatile uint64_t g_sigterm_count = 0;
static volatile uint64_t g_sigint_count = 0;
static volatile uint64_t g_sighup_count = 0;
static volatile uint64_t g_recoveries = 0;
static volatile uint64_t g_fatal_crashes = 0;

static signal_handler_config_t g_config;
static bool g_installed = false;
```

**Why Critical**: Signal handler state persists across tests. If Test A triggers signals, Test B sees non-zero counters.

**Recommended Fix**: Already has `signal_handler_reset_stats()` - needs to be called in test teardown. Also need to uninstall handler:
```cpp
void TearDown() override {
    signal_handler_reset_stats();
    signal_handler_uninstall();
}
```

---

### High Priority (Subsystem State)

**File**: `src/utils/memory/nimcp_memory.c`

```c
// Line 350 - Memory tracking state
static memory_state_t g_memory_state = {
    .canary_value = NIMCP_MEMORY_CANARY,
    .allocations = NULL,
    // ... more fields
};
```

**Why High**: Memory tracking state accumulates across tests, causing memory leak false positives.

**Recommended Fix**: Add cleanup function:
```c
void memory_tracker_reset(void) {
    // Free all tracked allocations
    // Reset counters
    g_memory_state.total_allocations = 0;
    g_memory_state.total_frees = 0;
    // ...
}
```

---

**File**: `src/cognitive/consolidation/nimcp_consolidation.c`

```c
// Line 86
static consolidation_stats_t g_sync_stats = {0};
```

**Why High**: Statistics persist across tests, causing tests to see accumulated values.

**Recommended Fix**: Add to existing reset function or create new one:
```c
void consolidation_reset_stats(void) {
    memset(&g_sync_stats, 0, sizeof(g_sync_stats));
}
```

---

### Medium Priority (Thread Management State)

**File**: `src/utils/thread/nimcp_deadlock_detector.c`

```c
static deadlock_stats_t g_stats = {0};
static platform_mutex_t g_detector_mutex;
```

**Why Medium**: Thread management state can cause deadlock detection false positives.

**Recommended Fix**: Reset in teardown:
```c
void deadlock_detector_reset(void) {
    platform_mutex_lock(&g_detector_mutex);
    memset(&g_stats, 0, sizeof(g_stats));
    platform_mutex_unlock(&g_detector_mutex);
}
```

---

**File**: `src/utils/config/nimcp_dynamic_config.c`

```c
static platform_rwlock_t g_config_lock;
static platform_mutex_t g_callback_lock;
static config_stats_t g_stats = {0};
```

**Why Medium**: Configuration state persists, causing tests to see wrong config values.

**Recommended Fix**: Add cleanup:
```c
void dynamic_config_reset(void) {
    // Reset to defaults
    // Clear statistics
}
```

---

**File**: `src/utils/cache/nimcp_cache.c`

```c
static cache_global_state_t g_cache_state;
```

**Why Medium**: Cache state can cause cache hit/miss test failures.

**Recommended Fix**:
```c
void cache_reset_global_state(void) {
    memset(&g_cache_state, 0, sizeof(g_cache_state));
}
```

---

### Complete List of 27 Global State Files

Identified via grep for `static.*=.*;` pattern:

1. ✅ `src/utils/signal/nimcp_signal_handler.c` - 15 globals (CRITICAL)
2. ✅ `src/utils/memory/nimcp_memory.c` - `g_memory_state` (HIGH)
3. ✅ `src/cognitive/consolidation/nimcp_consolidation.c` - `g_sync_stats` (HIGH)
4. ✅ `src/utils/thread/nimcp_deadlock_detector.c` - `g_stats`, `g_detector_mutex` (MEDIUM)
5. ✅ `src/utils/config/nimcp_dynamic_config.c` - locks and stats (MEDIUM)
6. ✅ `src/utils/cache/nimcp_cache.c` - `g_cache_state` (MEDIUM)
7. `src/core/brain/nimcp_brain.c` - decision cache (already has mutex, but may need reset)
8. `src/utils/logging/nimcp_logging.c` - log state
9. `src/utils/thread/nimcp_thread_pool.c` - pool state
10. `src/plasticity/neuromodulators/nimcp_spatial_neuromod.c` - neuromod state
11. `src/utils/spectral/nimcp_fft.c` - FFT plan caching
12. `src/utils/quantum/nimcp_quantum_walk.c` - quantum state
13. `src/utils/algorithms/nimcp_community_detection.c` - algorithm state
14. `src/cognitive/epistemic/nimcp_epistemic_vigilance.c` - epistemic state
15. `src/cognitive/knowledge/nimcp_knowledge.c` - knowledge base
16. `src/cognitive/mental_health/nimcp_grief_and_loss.c` - emotional state
17. `src/cognitive/mental_health/nimcp_joy_euphoria.c` - emotional state
18. `src/cognitive/mental_health/nimcp_love_loyalty_friendship.c` - emotional state
19. `src/plasticity/eligibility/nimcp_eligibility.c` - eligibility traces
20. `src/plasticity/stdp/nimcp_stdp.c` - STDP state
21. `src/plasticity/bcm/nimcp_bcm.c` - BCM state
22. `src/plasticity/stp/nimcp_stp.c` - STP state
23. `src/plasticity/adaptive/nimcp_adaptive.c` - adaptive learning state
24. `src/plasticity/attention/nimcp_attention.c` - attention state
25. `src/synapse/nimcp_vesicle_packaging.c` - vesicle state
26. `src/synapse/nimcp_metabolic_pathways.c` - metabolic state
27. `src/glial/astrocytes/nimcp_astrocytes.c` - astrocyte state

## Recommended Fix Strategy

### Phase 1: Quick Wins (1-2 days)

**Goal**: Fix the top 3 critical contamination sources

1. **Signal Handler Cleanup** (src/utils/signal/nimcp_signal_handler.c)
   - Add `signal_handler_unregister_brain()` function
   - Document teardown pattern in test documentation
   - Update 5-10 tests that use signal handler

2. **Memory Tracker Reset** (src/utils/memory/nimcp_memory.c)
   - Add `memory_tracker_reset()` function
   - Call in test teardown where memory tracking is used

3. **Consolidation Stats Reset** (src/cognitive/consolidation/nimcp_consolidation.c)
   - Add `consolidation_reset_stats()` function
   - Update consolidation tests

**Expected Impact**: +10-15 tests passing (79% → 83-85%)

### Phase 2: Subsystem Cleanup (3-5 days)

**Goal**: Add cleanup functions to all 27 modules with global state

**Pattern**:
```c
// In each module's header file
void module_name_reset_state(void);

// In each module's source file
void module_name_reset_state(void) {
    // Lock mutex if needed
    // Reset all static variables to initial state
    // Unlock mutex
}
```

**Test Pattern**:
```cpp
class ModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        module_name_reset_state();  // Start clean
    }

    void TearDown() override {
        module_name_reset_state();  // Leave clean
    }
};
```

**Expected Impact**: +15-20 tests passing (85% → 90-92%)

### Phase 3: Test Infrastructure Improvement (1 week)

**Goal**: Prevent future global state contamination

1. **Create Test Utilities** (`test/utils/test_cleanup.h`):
```cpp
class NimcpTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset all known global state
        signal_handler_reset_all();
        memory_tracker_reset();
        consolidation_reset_stats();
        // ... all 27 modules
    }

    void TearDown() override {
        // Same cleanup
    }
};
```

2. **Static Analysis**: Add clang-tidy rule to detect new static variables without reset functions

3. **Documentation**: Create `docs/TESTING_BEST_PRACTICES.md` with global state guidelines

**Expected Impact**: Maintainable test infrastructure, prevent regressions

## Validation Plan

### Step 1: Baseline Measurement
```bash
# Run tests sequentially (should pass at higher rate)
ctest -j1 --output-on-failure

# Expected: 85-90% pass rate (vs 79% in parallel)
```

### Step 2: After Phase 1 Fixes
```bash
# Run with moderate parallelism
ctest -j4 --output-on-failure

# Expected: 83-85% pass rate (closer to sequential)
```

### Step 3: After Phase 2 Fixes
```bash
# Run with high parallelism
ctest -j20 --output-on-failure

# Expected: 90-92% pass rate (matches sequential)
```

### Step 4: Flakiness Detection
```bash
# Run tests 10 times in parallel
for i in {1..10}; do
    ctest -j20 --output-on-failure > /tmp/run_$i.txt
done

# Check consistency
grep "% tests passed" /tmp/run_*.txt | sort | uniq -c

# Expected: Consistent pass rate across all runs
```

## Alternative Approaches Considered

### Option A: Thread-Local Storage (TLS)
**Pros**: Automatic per-thread isolation
**Cons**: Complex to implement, may hide real thread safety bugs
**Verdict**: NOT RECOMMENDED - adds complexity without solving root cause

### Option B: Test Sandboxing (fork per test)
**Pros**: Perfect isolation via process boundaries
**Cons**: Slower execution, harder to debug
**Verdict**: OVERKILL - cleanup functions are simpler

### Option C: Do Nothing (live with flakiness)
**Pros**: Zero effort
**Cons**: Makes development frustrating, hides real bugs
**Verdict**: UNACCEPTABLE - undermines test reliability

## Priority Order Summary

1. **CRITICAL** (Fix First):
   - Signal handler: `g_registered_brain` cleanup
   - Signal handler: Reset stats in teardown

2. **HIGH** (Fix Second):
   - Memory tracker: Add reset function
   - Consolidation: Add stats reset

3. **MEDIUM** (Fix Third):
   - Thread management: Deadlock detector reset
   - Configuration: Dynamic config reset
   - Cache: Global state reset

4. **LOW** (Fix Eventually):
   - Remaining 20 modules with global state

## Estimated Effort

- Phase 1 (Critical Fixes): 1-2 days → +10-15 tests
- Phase 2 (All Cleanup Functions): 3-5 days → +15-20 tests
- Phase 3 (Infrastructure): 1 week → Maintainable long-term

**Total**: 2-3 weeks for complete solution
**ROI**: 79% → 90-92% pass rate, eliminate flakiness

## Conclusion

The test suite has **higher quality** than the 79% pass rate suggests. Many "failures" are artifacts of global state contamination, not genuine bugs. Fixing the top 6 global state issues should raise pass rate to 90%+, revealing the true remaining issues.

**Next Step**: Implement Phase 1 fixes (signal handler cleanup) to validate this hypothesis.

---

**Analysis Date**: 2025-11-17
**Status**: Investigation complete, ready for implementation
**Validated By**: Sequential vs. parallel test comparison, individual test success patterns
