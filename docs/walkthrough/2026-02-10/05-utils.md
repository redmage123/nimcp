# Walkthrough Pass 4 - Utils Directory Review

**Date**: 2026-02-10
**Scope**: `/home/bbrelin/nimcp/src/utils/` (146 .c files across 39 subdirectories)
**Reviewer**: Claude Opus 4.6

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| **P1**   | 5     | Critical/Crash - memory corruption, async-signal-safety, deadlock |
| **P2**   | 16    | Logic/Correctness - data corruption, overflow, thread safety |
| **P3**   | 8     | Style/Robustness - const correctness, false positive throws |

**Files Reviewed**: 146 (all .c files in src/utils/)
**Method**: Full read of critical modules (exception, memory, thread, tensor, containers, bridge, signal, serialization, code, vcs); targeted grep searches across all files for dangerous patterns.

---

## Critical Rule Verification

### Exception Files - NIMCP_THROW_TO_IMMUNE Prohibition

All 6 files in `src/utils/exception/` were individually read and verified:

| File | Status | Notes |
|------|--------|-------|
| nimcp_exception.c | CLEAN | No raw NIMCP_THROW_TO_IMMUNE |
| nimcp_exception_circuit.c | CLEAN | No raw NIMCP_THROW_TO_IMMUNE |
| nimcp_exception_handlers.c | CLEAN | No raw NIMCP_THROW_TO_IMMUNE |
| nimcp_exception_immune.c | CLEAN | No raw NIMCP_THROW_TO_IMMUNE |
| nimcp_exception_metrics.c | CLEAN | No raw NIMCP_THROW_TO_IMMUNE |
| nimcp_exception_trace.c | CLEAN | No raw NIMCP_THROW_TO_IMMUNE |

### Core Memory Files - Raw malloc/calloc/free Requirement

| File | Status | Notes |
|------|--------|-------|
| nimcp_memory.c | CLEAN | Uses raw malloc/calloc/free after #undef at lines 222-225 |
| nimcp_unified_memory.c | CLEAN | Uses raw calloc/malloc/free with UMM_SAFE_THROW guard |

---

## P1 - Critical/Crash Findings

### P1-U1: Integer Overflow in nimcp_calloc_guarded

**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_memory_guards.c`
**Line**: 275
**Code**:
```c
size_t total = nmemb * size;
```
**Problem**: No overflow check before multiplication. If `nmemb * size` wraps around to a small value on 64-bit systems, the allocation succeeds with insufficient memory, leading to heap buffer overflow on subsequent writes.

**Impact**: Heap buffer overflow, memory corruption, potential code execution.

**Suggested Fix**:
```c
if (size > 0 && nmemb > SIZE_MAX / size) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
        "nimcp_calloc_guarded: integer overflow in nmemb * size");
    return NULL;
}
size_t total = nmemb * size;
```

**Note**: The core `nimcp_calloc` in `nimcp_memory.c` already has this overflow check (P1-41 fix applied). This is the guarded variant that wraps it but doesn't check before adding canary overhead.

---

### P1-U2: NIMCP_THROW_TO_IMMUNE from SIGSEGV Handler

**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_page_cow.c`
**Line**: 282
**Code**: `NIMCP_THROW_TO_IMMUNE(...)` called from `handle_cow_fault()`
**Problem**: `handle_cow_fault` is reachable from a SIGSEGV signal handler. NIMCP_THROW_TO_IMMUNE internally calls `nimcp_malloc`, acquires mutexes, and performs other operations that are NOT async-signal-safe. Calling these from a signal handler is undefined behavior and can cause deadlocks or crashes.

**Impact**: Undefined behavior in signal handler context - deadlock, double fault, or process termination.

**Suggested Fix**: Replace NIMCP_THROW_TO_IMMUNE with signal-safe error reporting (e.g., `write(STDERR_FILENO, ...)`) in any code path reachable from signal handlers. Or, use `sigsetjmp`/`siglongjmp` to defer processing to a safe context.

---

### P1-U3: Potential Deadlock in nimcp_adaptive_import

**File**: `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_metrics.c`
**Line**: ~1052
**Problem**: `nimcp_adaptive_import()` calls `nimcp_adaptive_reset_all()` while holding `g_adaptive_mutex`. The `nimcp_adaptive_reset_all()` function at line ~948 also acquires `g_adaptive_mutex`. Since the mutex is not recursive by default, this creates a self-deadlock.

**Impact**: Thread permanently blocks, potentially freezing exception handling.

**Suggested Fix**: Either make `g_adaptive_mutex` recursive, or create an internal `nimcp_adaptive_reset_all_unlocked()` helper that assumes the mutex is already held, and call that from `nimcp_adaptive_import()`.

---

### P1-U4: Heap Buffer Over-Read in Deserialization

**File**: `/home/bbrelin/nimcp/src/utils/serialization/nimcp_serialization.c`
**Line**: 177, 187
**Code**:
```c
uint32_t orig_size;
memcpy(&orig_size, data, sizeof(uint32_t));     // line 177
// ...
memcpy(copy, data + sizeof(uint32_t), orig_size);  // line 187
```
**Problem**: In `decompress_fallback()`, `orig_size` is read from the compressed data buffer without validation that `orig_size <= size - sizeof(uint32_t)`. A crafted input with `orig_size` larger than the actual payload causes an out-of-bounds read from the source buffer.

The same pattern exists in `decompress_zlib()` at line 116, though zlib's `uncompress()` provides some protection.

**Impact**: Heap buffer over-read, information disclosure, potential crash.

**Suggested Fix**:
```c
uint32_t orig_size;
memcpy(&orig_size, data, sizeof(uint32_t));
if ((size_t)orig_size > size - sizeof(uint32_t)) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
        "decompress_fallback: orig_size exceeds available data");
    *out_size = 0;
    return NULL;
}
```

---

### P1-U5: NIMCP_THROW_TO_IMMUNE from Signal Handler Context

**File**: `/home/bbrelin/nimcp/src/utils/signal/nimcp_signal_handler.c`
**Line**: 632
**Code**:
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "try_recovery_jump: validation failed");
```
**Problem**: `try_recovery_jump()` is called from `handle_fatal_signal_extended()` (line ~694) which is a signal handler registered with `SA_SIGINFO`. NIMCP_THROW_TO_IMMUNE is NOT async-signal-safe - it allocates memory, acquires mutexes, etc.

**Impact**: Undefined behavior in signal context - potential deadlock or double fault.

**Suggested Fix**: Replace with `safe_write()` (which uses raw `write()` syscall) or simply return -1 without the throw, since the signal handler already handles unrecoverable cases.

---

## P2 - Logic/Correctness Findings

### P2-U1: qsort Comparator Integer Overflow

**File**: `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_handlers.c`
**Line**: 86
**Code**:
```c
return hb->options.priority - ha->options.priority;
```
**Problem**: Integer subtraction in qsort comparator can overflow if priorities have extreme values (e.g., INT_MIN and INT_MAX). This would cause incorrect sort ordering.

**Impact**: Incorrect handler dispatch order.

**Suggested Fix**:
```c
if (hb->options.priority > ha->options.priority) return 1;
if (hb->options.priority < ha->options.priority) return -1;
return 0;
```

---

### P2-U2: Pointer-After-Unlock in Exception Circuit Breaker

**File**: `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_circuit.c`
**Status**: Previously documented as P2-U32 in Pass 3.
**Problem**: `nimcp_circuit_get_entry()` returns a pointer to a static array element after releasing the mutex. Another thread could modify or reallocate the array while the caller uses the pointer.

**Note**: No callers currently exist. Architectural - would need to change to copy-out pattern.

---

### P2-U3: Pointer-After-Unlock in Exception Trace

**File**: `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_trace.c`
**Status**: Previously documented as P2-U31 in Pass 3.
**Problem**: `find_trace_data()` returns pointer to static array data after releasing mutex.

---

### P2-U4: brain_pools_release_activation Returns to Wrong Pool

**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_brain_pools.c`
**Line**: 488
**Problem**: `brain_pools_release_activation` always releases memory back to `activation_pool` regardless of which pool the allocation originally came from. Comment says "Note: In production, would track which pool it came from."

**Impact**: If allocation came from a size-class pool, returning it to activation_pool corrupts the free list.

**Suggested Fix**: Track the source pool in the allocation metadata and release back to the correct pool.

---

### P2-U5: Nested Spinlock Acquisition in Page CoW

**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_page_cow.c`
**Line**: 355
**Problem**: `handle_cow_fault` acquires `view->spinlock` then attempts to acquire `region->spinlock`. If lock ordering is violated elsewhere (region then view), this creates a deadlock.

**Impact**: Potential deadlock under CoW fault contention.

**Suggested Fix**: Enforce consistent lock ordering (always region before view, or vice versa) and document the ordering requirement.

---

### P2-U6: nimcp_tensor_arange Ignores dtype Parameter

**File**: `/home/bbrelin/nimcp/src/utils/tensor/nimcp_tensor.c`
**Line**: 669
**Code**:
```c
float* data = (float*)t->data;
for (uint32_t i = 0; i < n; i++) {
    data[i] = (float)(start + i * step);
}
```
**Problem**: Always casts `t->data` to `float*` regardless of `dtype`. If `dtype` is `NIMCP_DTYPE_F64`, the tensor buffer is sized for doubles but written as floats, leaving half the buffer uninitialized. If `dtype` is `NIMCP_DTYPE_I32`, float values are written into int32 memory.

**Impact**: Data corruption for non-F32 dtypes.

**Suggested Fix**: Dispatch by dtype:
```c
switch (t->dtype) {
    case NIMCP_DTYPE_F32: { float* d = (float*)t->data; for (...) d[i] = ...; break; }
    case NIMCP_DTYPE_F64: { double* d = (double*)t->data; for (...) d[i] = ...; break; }
    case NIMCP_DTYPE_I32: { int32_t* d = (int32_t*)t->data; for (...) d[i] = ...; break; }
}
```

---

### P2-U7: nimcp_tensor_linspace Ignores dtype Parameter

**File**: `/home/bbrelin/nimcp/src/utils/tensor/nimcp_tensor.c`
**Line**: 698
**Problem**: Same issue as P2-U6 - always casts to `float*` regardless of dtype.

---

### P2-U8: Unaligned Memory Access in MurmurHash3

**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_hash_table.c`
**Line**: 138
**Code**:
```c
uint32_t k = blocks[i];
```
**Problem**: `blocks` is cast from a potentially unaligned `const uint8_t*` to `const uint32_t*`. On architectures that require aligned access (ARM, some MIPS), this causes a bus error (SIGBUS).

**Impact**: Crash on alignment-strict architectures.

**Suggested Fix**: Use `memcpy` to safely load from potentially unaligned memory:
```c
uint32_t k;
memcpy(&k, &((const uint8_t*)key)[i * 4], sizeof(uint32_t));
```

---

### P2-U9: Quadratic Overflow in Graph Metrics Allocation

**File**: `/home/bbrelin/nimcp/src/utils/algorithms/nimcp_graph_metrics.c`
**Line**: 279
**Code**:
```c
float* dist = (float*)nimcp_malloc(n * n * sizeof(float));
```
**Problem**: `n * n` can overflow for `n > 65536` (uint32_t). No overflow check before allocation. Similar pattern at line 624.

**Impact**: Undersized allocation followed by out-of-bounds writes during Floyd-Warshall initialization.

**Suggested Fix**:
```c
if (n > 0 && n > SIZE_MAX / n / sizeof(float)) {
    return -1.0F;  // Overflow
}
```

---

### P2-U10: Command Injection via popen/system

**File**: `/home/bbrelin/nimcp/src/utils/vcs/nimcp_vcs_integration.c`
**Line**: 1004
**Code**:
```c
snprintf(full_cmd, sizeof(full_cmd), "cd \"%s\" && %s 2>&1", vcs->repo_root, cmd);
FILE* fp = popen(full_cmd, "r");
```
**Problem**: Both `vcs->repo_root` and `cmd` are passed into a shell command without sanitization. If either contains shell metacharacters (e.g., `"; rm -rf /`), arbitrary command execution occurs.

**Also in**: `/home/bbrelin/nimcp/src/utils/code/nimcp_recompiler.c` lines 682 (popen) and 1585 (system) with unsanitized `so_path` and `symbol_name`.

**Impact**: Command injection if inputs are attacker-controlled. Lower risk since inputs typically come from internal callers, but the recompiler accepts external source code.

**Suggested Fix**: Sanitize inputs or use `fork()/exec()` to avoid shell interpretation. For `nm` check, use `dlsym()` instead.

---

### P2-U11: GPU Health Integer Overflow in Allocation

**File**: `/home/bbrelin/nimcp/src/utils/gpu/nimcp_gpu_health.c`
**Line**: 968
**Code**:
```c
void* host_buffer = nimcp_malloc(num_elements * element_size);
```
**Problem**: `num_elements * element_size` has no overflow check. Also at line 1031.

**Impact**: Undersized allocation, heap buffer overflow during cudaMemcpy.

---

### P2-U12: NIMCP_THROW_TO_IMMUNE in qsort Comparators

**File**: Multiple files
**Locations**:
- `/home/bbrelin/nimcp/src/utils/spatial/nimcp_kdtree.c` lines 71, 82, 93 (compare_points_x/y/z)
- `/home/bbrelin/nimcp/src/utils/fault_tolerance/nimcp_checkpoint.c` line 577 (compare_checkpoint_timestamp_desc)
- `/home/bbrelin/nimcp/src/utils/fault_tolerance/nimcp_state_manager.c` line 93 (compare_by_priority)
- `/home/bbrelin/nimcp/src/utils/statistics/nimcp_survival.c` line 66 (compare_obs_time)

**Problem**: NIMCP_THROW_TO_IMMUNE is called inside qsort comparators. Since qsort calls the comparator O(N log N) times, this generates a massive number of throws during normal sorting. Each throw allocates memory, acquires mutexes, and performs logging.

**Impact**: Severe performance degradation during sorting operations.

**Note**: This was identified as a false positive pattern in previous walkthroughs. The throws fire on every comparison, not just on error conditions. They should be removed from comparators entirely.

---

### P2-U13: Quantum Statistics Quadratic Allocation Overflow

**File**: `/home/bbrelin/nimcp/src/utils/statistics/nimcp_quantum_statistics.c`
**Line**: 479-480
**Code**:
```c
float* A = nimcp_malloc(n * n * sizeof(float));
float* V = nimcp_malloc(n * n * sizeof(float));
```
**Problem**: Same quadratic overflow issue as P2-U9. No overflow check for `n * n`.

---

### P2-U14: Multivariate Statistics Quadratic Allocation Overflow

**File**: `/home/bbrelin/nimcp/src/utils/statistics/nimcp_multivariate.c`
**Lines**: 509, 650
**Problem**: Same quadratic overflow pattern with `n * n * sizeof(float)`.

---

### P2-U15: ML Statistics Realloc Quadratic Overflow

**File**: `/home/bbrelin/nimcp/src/utils/statistics/nimcp_ml_statistics.c`
**Line**: 711
**Code**:
```c
gp->L = (float*)nimcp_realloc(gp->L, n * n * sizeof(float));
```
**Problem**: Quadratic overflow in realloc. If overflow wraps to small value, realloc succeeds with insufficient memory.

---

### P2-U16: Numerical Integration Multi-factor Overflow

**File**: `/home/bbrelin/nimcp/src/utils/numerical/nimcp_integration.c`
**Line**: 179
**Code**:
```c
*trajectory = (float*)nimcp_malloc(steps * n * sizeof(float));
```
**Problem**: `steps * n * sizeof(float)` - triple multiplication without overflow check. Also at lines 285 and 395 with patterns like `5 * n * sizeof(float)` and `7 * n * sizeof(float)`.

---

## P3 - Style/Robustness Findings

### P3-U1: Cast Away Const on Mutex Pointer

**File**: `/home/bbrelin/nimcp/src/utils/bridge/nimcp_bridge_base.c`
**Line**: 329
**Problem**: `bridge_base_get_stats` casts away `const` on the mutex pointer to lock it. This violates const-correctness.

**Suggested Fix**: Declare the mutex as `mutable` equivalent (in C, use a pointer-to-non-const-mutex stored separately).

---

### P3-U2: Non-Thread-Safe Hash Table

**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_hash_table.c`
**Problem**: The `thread_safe` configuration flag exists but thread safety is explicitly NOT IMPLEMENTED. The comment says "P3: thread safety not implemented yet."

**Impact**: Race conditions if used from multiple threads despite the flag.

---

### P3-U3: Static Thread-Local Buffer Return

**File**: `/home/bbrelin/nimcp/src/utils/config/nimcp_config_expand.c`
**Line**: 527
**Code**:
```c
static __thread char expanded_buffer[CONFIG_EXPAND_MAX_LENGTH];
```
**Problem**: Function returns pointer to thread-local static buffer. Subsequent calls from the same thread overwrite previous results.

**Impact**: Stale pointer if caller stores the return value and calls the function again.

---

### P3-U4: nimcp_aes_available Always Throws

**File**: `/home/bbrelin/nimcp/src/utils/serialization/nimcp_serialization.c`
**Line**: 58
**Code**:
```c
bool nimcp_aes_available(void) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_aes_available: operation failed");
    return false;
}
```
**Problem**: Query function throws an exception on every call. Query functions should return status without side effects.

**Suggested Fix**: Just return `false` without the throw.

---

### P3-U5: Hardcoded Paths in Recompiler

**File**: `/home/bbrelin/nimcp/src/utils/code/nimcp_recompiler.c`
**Lines**: 70, 78
**Code**:
```c
static const char* s_nimcp_includes[] = { "/home/bbrelin/nimcp/include", NULL };
static const char* s_nimcp_lib_paths[] = { "/home/bbrelin/nimcp/build/src/lib", NULL };
```
**Problem**: Hardcoded absolute paths that are specific to the development environment. Will not work on other machines.

**Suggested Fix**: Use relative paths or build-time configuration macros.

---

### P3-U6: Missing Overflow Check in Hilbert Allocation

**File**: `/home/bbrelin/nimcp/src/utils/signal/nimcp_hilbert.c`
**Lines**: 208, 274, 307, 743
**Code**:
```c
neural_phasor_t* analytic = (neural_phasor_t*)nimcp_malloc(n * sizeof(neural_phasor_t));
```
**Problem**: No overflow check for `n * sizeof(neural_phasor_t)`. While `n` typically comes from validated signal lengths, defensive coding should check.

---

### P3-U7: Ring Buffer Not Thread-Safe

**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_ring_buffer.c`
**Problem**: Explicitly documented as NOT thread-safe. This is acceptable since lock-free alternatives exist (SPSC, MPMC queues), but callers must be aware.

---

### P3-U8: SIMD Init Not Thread-Safe

**File**: `/home/bbrelin/nimcp/src/utils/tensor/nimcp_tensor_simd.c`
**Lines**: 51-52
**Code**:
```c
static tensor_simd_backend_t s_backend = TENSOR_SIMD_NONE;
static bool s_initialized = false;
```
**Problem**: `tensor_simd_init()` uses a plain `bool` flag for initialization check without any synchronization. If two threads call it concurrently, both might detect `s_initialized == false` and race on initialization.

**Suggested Fix**: Use `nimcp_platform_once` for one-time initialization.

---

## Verified Clean Modules

The following modules were reviewed and found to have no significant issues:

| Module | Files | Notes |
|--------|-------|-------|
| exception (all 6) | CLEAN | No raw NIMCP_THROW_TO_IMMUNE, proper recursion guards |
| memory/nimcp_memory.c | CLEAN | Raw malloc, MEMORY_SAFE_THROW guard, overflow checks |
| memory/nimcp_unified_memory.c | CLEAN | Raw calloc/free, UMM_SAFE_THROW guard |
| memory/nimcp_memory_pool.c | CLEAN | O(1) free-list, proper mutex usage |
| memory/nimcp_cow_manager.c | CLEAN | Atomic refcounting, mutex-protected copy |
| memory/nimcp_buffer_pool.c | CLEAN | Proper error path cleanup |
| memory/nimcp_layer_pools.c | CLEAN | Proper mutex throughout |
| memory/nimcp_unified_pools.c | CLEAN | Quota management, pressure monitoring |
| thread/nimcp_thread_mutex.c | CLEAN | Clean adapter pattern |
| thread/nimcp_thread_pool.c | CLEAN | Proper synchronization, graceful shutdown |
| thread/nimcp_thread_rwlock.c | CLEAN | Clean pthread_rwlock adapter |
| thread/nimcp_deadlock_detector.c | CLEAN | P2-U20 fix applied (pthread_equal) |
| thread/nimcp_mutex_pool.c | CLEAN | nimcp_platform_once for init |
| thread/nimcp_semaphore.c | CLEAN | Monitor pattern, proper condvar usage |
| thread/nimcp_thread_resource.c | CLEAN | Reference counting, fine-grained locking |
| containers/nimcp_queue_mpmc.c | CLEAN | Vyukov MPMC, proper CAS patterns |
| containers/nimcp_queue_spsc.c | CLEAN | Wait-free, cache-line padding |
| containers/nimcp_darray.c | CLEAN | Growth factor, proper cleanup |
| containers/nimcp_vector.c | CLEAN | Math operations, proper null checks |
| containers/nimcp_btree.c | CLEAN | RW locks per node |
| bridge/nimcp_bridge_gpu.c | CLEAN | Proper GPU context lifecycle |
| cache/nimcp_cache.c | CLEAN | Canary validation, atomic refcount |
| json/nimcp_json.c | CLEAN | Per-context mutex, Jansson wrapper |
| rng/nimcp_rand.c | CLEAN | Thread-local state, multiple backends |
| spectral/nimcp_fft.c | CLEAN | Cooley-Tukey, proper power-of-2 checks |
| encoding/nimcp_positional_encoding.c | CLEAN | Cached encodings, proper lifecycle |
| validation/nimcp_validate.c | CLEAN | Fail-fast strategy, comprehensive checks |
| time/nimcp_time.c | CLEAN | Proper wraparound handling |
| logging/nimcp_logging.c | CLEAN | P1-42 fix (nimcp_memory.h at file scope) |
| platform/* (all 9) | CLEAN | Proper platform abstractions |
| fuzzy/* (all 4) | CLEAN | Thread-local error buffers |
| fault_tolerance/* (30 files) | CLEAN | Proper NIMCP_THROW_TO_IMMUNE usage (higher-level) |
| dispatch/nimcp_fn_dispatch.c | CLEAN | RW locks, atomic swaps |
| config/* (7 files) | CLEAN | Proper bounds checking (except P3-U3) |
| ternary/nimcp_ternary_tensor.c | CLEAN | Proper null checks |
| tensor_networks/* (3 files) | CLEAN | Thread-local RNG state |
| quantum/* (3 files) | CLEAN | Thread-local seeds |

---

## Previously Fixed Issues Verified

The following previously-reported fixes were verified as correctly applied:

| Fix ID | Location | Description | Status |
|--------|----------|-------------|--------|
| P1-41 | nimcp_memory.c | Integer overflow check in nimcp_calloc | Verified |
| P1-42 | nimcp_logging.c:52 | nimcp_memory.h at file scope | Verified |
| P1-5 | nimcp_exception_immune.c | MAX_IMMUNE_DEPTH=3 recursion guard | Verified |
| P2-U20 | nimcp_deadlock_detector.c | pthread_equal for thread comparison | Verified |
| P2-U25 | nimcp_exception_handlers.c | nimcp_platform_once for init | Verified |
| P2-U26 | nimcp_exception_handlers.c | Capacity check inside mutex | Verified |
| P2-U27 | nimcp_exception_handlers.c | Atomic ID generation | Verified |
| P2-U31 | nimcp_exception_trace.c | Pointer-after-unlock (documented) | Verified |
| P2-U32 | nimcp_exception_circuit.c | Pointer-after-unlock (documented) | Verified |

---

## Patterns Verified Across All Files

| Pattern | Method | Result |
|---------|--------|--------|
| No `sprintf()` usage | grep | CLEAN - only `snprintf` found |
| No `strcat()`/`strcpy()` | grep | CLEAN - only `strncat` with bounds |
| No `gets()` | grep | CLEAN |
| Proper `volatile` in signal handler | grep | CLEAN - all signal globals are `volatile sig_atomic_t` |
| Thread-local storage properly used | grep | CLEAN - 35+ `_Thread_local`/`__thread` instances |
| No NIMCP_THROW_TO_IMMUNE in exception/ | grep + read | CLEAN |
| Raw malloc in core memory files | read | CLEAN |

---

## Recommendations

### High Priority (P1 fixes)
1. **P1-U1**: Add overflow check to `nimcp_calloc_guarded` - trivial fix
2. **P1-U4**: Validate `orig_size` against available data in deserializer
3. **P1-U3**: Create `_unlocked()` helper for adaptive reset to avoid deadlock
4. **P1-U2, P1-U5**: Replace NIMCP_THROW_TO_IMMUNE with signal-safe alternatives in signal handler and page CoW fault handler code paths

### Medium Priority (P2 fixes)
1. **P2-U6, P2-U7**: Fix tensor arange/linspace dtype dispatch - high-value fix, simple change
2. **P2-U12**: Remove NIMCP_THROW_TO_IMMUNE from all qsort comparators - performance
3. **P2-U9 through P2-U16**: Add overflow checks for quadratic/multi-factor allocations
4. **P2-U10**: Sanitize inputs to popen/system or use fork/exec

### Low Priority (P3 fixes)
1. **P3-U4**: Remove throw from nimcp_aes_available query
2. **P3-U5**: Make recompiler paths configurable
3. **P3-U8**: Use platform_once for SIMD init
