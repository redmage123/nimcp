# Pass 8 Walkthrough: Security, Utils, Async, and API Modules

**Date**: 2026-02-15
**Reviewer**: Claude Opus 4.6 (automated code review)
**Scope**: `src/security/`, `src/utils/`, `src/async/`, `src/api/`
**Method**: Systematic file-by-file review of critical infrastructure, then pattern-based scanning of remaining files.

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| P1 (Critical) | 6 | Thread-unsafe `srand()`, potential recursion in CoW/memory guards, signal handler race conditions |
| P2 (Important) | 18 | Wrong error codes, thread-unsafe statistics, missing volatile, racy state accesses |
| P3 (Minor) | 5 | Dead code, redundant checks, documentation gaps |

---

## Critical Rules Verification

### Rule: `src/utils/exception/` must NEVER have raw NIMCP_THROW_TO_IMMUNE
**VERIFIED PASS**: Grep found 3 matches in the exception directory, but all are comments/documentation references (line 72 in nimcp_exception_immune.c, lines 45/305 in nimcp_exception.c). No actual NIMCP_THROW_TO_IMMUNE function calls exist in the exception source files.

### Rule: `src/utils/memory/nimcp_memory.c` must use raw malloc/free
**VERIFIED PASS**: File properly `#undef`s `malloc`, `calloc`, `realloc`, `free` macros (lines 222-225) and uses raw `malloc()` for internal tracking structures. Uses `MEMORY_SAFE_THROW` macro instead of `NIMCP_THROW_TO_IMMUNE`.

### Rule: `src/utils/memory/nimcp_unified_memory.c` must use raw malloc/free
**VERIFIED PASS**: File uses raw `calloc`/`malloc`/`free` throughout (not `nimcp_*` variants). Uses `UMM_SAFE_THROW` macro for exception reporting.

### Rule: `src/security/nimcp_constant_time.c` must gate throws with `nimcp_exception_system_is_initialized()`
**VERIFIED PASS**: Line 354 gates NIMCP_THROW_TO_IMMUNE with `nimcp_exception_system_is_initialized()` check. Uses constructor/destructor attributes for module init/cleanup with atomic CAS.

### Rule: Variables modified between setjmp and longjmp must be volatile
**VERIFIED PASS**: `nimcp_try_context_t` (nimcp_exception_handlers.h:219-226) properly marks `exception` as `volatile nimcp_exception_t*` and `exception_caught` as `volatile bool`. Signal handler's `signal_recovery_ctx_t` (nimcp_signal_handler.h:532-541) properly marks `valid`, `result`, `crash_signal`, and `retry_count` as `volatile`.

### Rule: `nimcp_memory.h` must be at file scope
**VERIFIED PASS**: All reviewed files include `nimcp_memory.h` at file scope, not inside `#ifdef` blocks.

---

## P1 Findings (Critical)

### P1-U1: Thread-unsafe `srand()` calls in statistics modules
**Files**:
- `/home/bbrelin/nimcp/src/utils/statistics/nimcp_ml_statistics.c` (lines 194, 1065)
- `/home/bbrelin/nimcp/src/utils/statistics/nimcp_multivariate.c` (line 1192)

**Issue**: `srand()` modifies global state and is not thread-safe. Multiple threads calling `srand()` simultaneously causes data races. Additionally, `srand((unsigned int)time(NULL))` called from multiple threads within the same second produces identical seeds.

**Evidence**:
```c
// nimcp_ml_statistics.c:194
srand((unsigned int)time(NULL));

// nimcp_ml_statistics.c:1065
srand((unsigned int)time(NULL));

// nimcp_multivariate.c:1192
srand(ica->random_state);
```

**Note**: The subsequent `rand()` calls have been replaced with `nimcp_tl_rand()` (thread-safe), but the `srand()` calls remain and affect the global PRNG state used by any code still calling `rand()`. The `srand()` calls should be removed entirely since `nimcp_tl_rand()` has its own seeding mechanism.

**Impact**: Data race on global PRNG state. Could cause non-deterministic behavior or crashes if another thread calls `rand()` concurrently.

### P1-U2: Potential infinite recursion in nimcp_cow_manager.c NIMCP_THROW_TO_IMMUNE
**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_cow_manager.c`

**Issue**: `cow_manager_create()` (line 174) calls `nimcp_calloc()` to allocate the manager, and has raw `NIMCP_THROW_TO_IMMUNE` calls on the allocation failure path (line 177). If exception creation internally triggers a CoW-managed memory operation (through nimcp_calloc -> unified memory -> CoW), this creates a potential recursion loop:
```
NIMCP_THROW_TO_IMMUNE -> nimcp_exception_create -> nimcp_calloc -> cow_manager -> NIMCP_THROW_TO_IMMUNE
```

**Evidence**: 15+ raw NIMCP_THROW_TO_IMMUNE calls throughout the file (lines 123, 169, 177, 195, 219, 253, 263, 344, 352, 365, 396, 419, 476, 516).

**Impact**: Stack overflow from infinite recursion if the exception system and CoW memory are both active and allocation fails.

**Mitigation**: The CoW manager is not in the nimcp_malloc critical path (it's a separate system), so this is unlikely in practice. However, if unified memory uses CoW internally, the chain becomes possible.

### P1-U3: Potential infinite recursion in nimcp_memory_guards.c NIMCP_THROW_TO_IMMUNE
**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_memory_guards.c`

**Issue**: `nimcp_malloc_guarded()` (line 218, 229) and other guarded allocation functions use raw NIMCP_THROW_TO_IMMUNE. If exception creation allocates memory through the guarded allocator (when `g_initialized && g_config.enable_guards` is true), infinite recursion occurs.

**Evidence**:
```c
// Line 218 - inside nimcp_malloc_guarded:
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_malloc_guarded: size is zero");

// Line 229 - inside nimcp_malloc_guarded:
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_malloc_guarded: header is NULL");
```

**Impact**: Stack overflow if guards are enabled and exception system allocates through guarded allocator.

### P1-U4: deadlock_detector_print_stats reads g_stats without lock
**File**: `/home/bbrelin/nimcp/src/utils/thread/nimcp_deadlock_detector.c`

**Issue**: `deadlock_detector_print_stats()` (lines 558-569) reads all `g_stats` fields without holding `g_detector_mutex`. This is a data race because other threads can concurrently modify `g_stats.total_locks`, `g_stats.order_violations`, etc. while the print function reads them.

**Evidence**:
```c
void deadlock_detector_print_stats(void) {
    printf("\n=== Deadlock Detector Statistics ===\n");
    printf("Total lock attempts:  %lu\n", (unsigned long)g_stats.total_locks);  // No lock!
    printf("Total unlocks:        %lu\n", (unsigned long)g_stats.total_unlocks);
    // ... 6 more unprotected reads
}
```

**Note**: `deadlock_detector_get_stats()` correctly copies stats under lock (lines 551-555), but `print_stats` does not.

**Impact**: Torn reads of 64-bit counters on 32-bit architectures; inconsistent snapshot of statistics.

### P1-U5: g_recovery_jump_buf race condition in signal handler
**File**: `/home/bbrelin/nimcp/src/utils/signal/nimcp_signal_handler.c`

**Issue**: `g_recovery_jump_buf` (line 122) is a global `sigjmp_buf` shared by all threads. If two threads set recovery points via `signal_handler_set_recovery_point()` (line 1502), they overwrite each other's jump buffer. A crash in one thread would jump to the wrong context.

**Evidence**:
```c
static sigjmp_buf g_recovery_jump_buf;  // Global, shared by all threads

int signal_handler_set_recovery_point(void) {
    int result = sigsetjmp(g_recovery_jump_buf, 1);  // Thread A and B race here
    if (result == 0) {
        g_recovery_jump_valid = 1;
    }
    return result;
}
```

**Mitigation**: The extended API (`signal_handler_set_recovery_point_ex()`) uses thread-local contexts, which is correct. The legacy global API should be deprecated.

**Impact**: Crash recovery jumps to wrong thread's context, causing undefined behavior.

### P1-U6: nimcp_constant_time.c avg_comparison_time_ns race
**File**: `/home/bbrelin/nimcp/src/security/nimcp_constant_time.c`

**Issue**: `avg_comparison_time_ns` update (around line 507-509) is a non-atomic float read-modify-write operation. Multiple threads performing constant-time comparisons concurrently will race on this floating-point average calculation.

**Impact**: Corrupted timing statistics. While this doesn't affect security of the constant-time operations themselves, it could cause incorrect timing side-channel reporting.

---

## P2 Findings (Important)

### P2-U1: cow_manager_create wrong error code
**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_cow_manager.c` (line 169)

**Issue**: When `config` is NULL, the error code is `NIMCP_ERROR_NO_MEMORY` but the message says "config is NULL". Should be `NIMCP_ERROR_NULL_POINTER` or `NIMCP_ERROR_INVALID_PARAM`.

```c
if (!config || config->data_size == 0) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cow_manager_create: config is NULL");
    // ^ Wrong error code: NIMCP_ERROR_NO_MEMORY vs NIMCP_ERROR_NULL_POINTER
```

### P2-U2: nimcp_ring_buffer false positive throws on empty/out-of-range
**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_ring_buffer.c`

**Issue**: Multiple functions throw NIMCP_THROW_TO_IMMUNE when accessing empty buffers or out-of-range indices. These are normal usage patterns (checking front of empty buffer), not errors worth presenting to the immune system.

**Locations**: Lines 190, 198, 206, 214, 223, 231, 268, 286, 302.

**Example**:
```c
// Line 206 - nimcp_ring_buffer_front:
if (!rb || rb->size == 0) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ring_buffer_front: rb is NULL");
    // ^ False positive: empty buffer is normal state, not an error
```

### P2-U3: hash_table_lookup_generic throws on NULL parameters in search path
**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_hash_table.c` (line 625)

**Issue**: `hash_table_lookup_generic` throws NIMCP_THROW_TO_IMMUNE for NULL table/key parameters. In hot search paths, this generates excessive immune system traffic for what should be a simple NULL check + return NULL.

### P2-U4: tracked_mutex_init compares nimcp_mutex_init return to 0 instead of NIMCP_SUCCESS
**File**: `/home/bbrelin/nimcp/src/utils/thread/nimcp_deadlock_detector.c` (line 281)

**Issue**: `nimcp_mutex_init()` returns `nimcp_result_t` (NIMCP_SUCCESS = 0), but comparing to literal `0` instead of `NIMCP_SUCCESS` is inconsistent with the codebase convention.

```c
if (nimcp_mutex_init(&mutex->mutex, NULL) != 0) {  // Should be != NIMCP_SUCCESS
```

### P2-U5: cow_manager_destroy does not clean up active handles
**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_cow_manager.c` (lines 226-248)

**Issue**: When `handle_count > 0`, the function logs a warning comment but does nothing. Active handles still point to the now-freed `template_data`, creating use-after-free if any handle is subsequently read or written.

```c
if (manager->handle_count > 0) {
    // WARNING: Handles still active
    // In production, might want to force cleanup or assert
}
```

**Impact**: Use-after-free if any active handle accesses data after manager destruction.

### P2-U6: g_immune_initialized in exception_immune.c is non-atomic
**File**: `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_immune.c` (line 46)

**Issue**: `g_immune_initialized` is a plain `bool` accessed by multiple threads (checked in the NIMCP_THROW_TO_IMMUNE path, set during init/shutdown). Without atomic semantics, this is a data race.

**Impact**: Torn reads possible; thread could see partially-written bool value.

### P2-U7: Exception immune statistics race condition
**File**: `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_immune.c`

**Issue**: Average response time calculation (around lines 499-505, 559-561) performs read-modify-write on statistics without lock protection. Multiple threads throwing exceptions concurrently will produce incorrect averages.

### P2-U8: Exception handler dispatch holds mutex while calling callbacks
**File**: `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_handlers.c` (lines 229-241)

**Issue**: `nimcp_exception_dispatch()` holds `g_handler_mutex` while iterating and calling handler callbacks. If a callback tries to register/unregister a handler, it will deadlock (attempting to lock the same mutex).

**Impact**: Deadlock if handler callbacks modify the handler registry.

### P2-U9: Exception trace g_trace_system_initialized is non-atomic
**File**: `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_trace.c` (line 43)

**Issue**: `g_trace_system_initialized` is a plain `bool` used to gate operations across threads. Should be `_Atomic bool` or accessed with `__atomic` builtins.

### P2-U10: nimcp_realloc tracking gap on failure
**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_memory.c` (around line 1658)

**Issue**: `nimcp_realloc` removes the old pointer from the tracking list before calling `realloc()`. If `realloc()` fails, the original pointer is still valid but no longer tracked. This creates a memory tracking gap where the allocation is invisible to leak detection.

### P2-U11: thread_pool error path uses NIMCP_THROW_TO_IMMUNE with wrong error code
**File**: `/home/bbrelin/nimcp/src/utils/thread/nimcp_thread_pool.c` (line 805)

**Issue**: When thread creation fails during pool creation, the throw uses `NIMCP_ERROR_NULL_POINTER` with message "operation failed". The error code should be `NIMCP_ERROR_SYSTEM` since this is a system-level thread creation failure.

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_pool_create: operation failed");
// ^ Wrong: should be NIMCP_ERROR_SYSTEM
```

### P2-U12: bio_async handle_tracker_shutdown sets initialized=false then uses mutex
**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_async.c` (lines 352-357)

**Issue**: The comment says "Set initialized=false BEFORE destroying mutex", but the actual code sets `initialized=false` while holding the mutex, then unlocks it, then destroys it. Another thread could check `initialized==false`, skip the mutex, and access stale data. The ordering should be:
1. Set initialized=false (under lock)
2. Unlock mutex
3. Destroy mutex

The current code does this correctly, but there is a window between unlock (line 356) and destroy (line 357) where another thread could lock the mutex. This is acceptable because the `initialized` flag gate prevents useful work.

**Verdict**: Low risk, but worth noting the race window.

### P2-U13: bio_router NIMCP_THROW_TO_IMMUNE in get_router_brain_kg_safe
**File**: `/home/bbrelin/nimcp/src/async/nimcp_bio_router.c` (line 199)

**Issue**: `get_router_brain_kg_safe()` throws NIMCP_THROW_TO_IMMUNE when the mutex is not initialized. This is a normal state during startup before the router is initialized, not an error worthy of immune system notification.

```c
if (!atomic_load_explicit(&g_router_brain_kg_mutex_initialized, memory_order_acquire)) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_router_brain_kg_safe: mutex not initialized");
    // ^ False positive: normal pre-init state
```

### P2-U14: cow_write copies from template_data but handle could point to different data
**File**: `/home/bbrelin/nimcp/src/utils/memory/nimcp_cow_manager.c` (lines 400-406)

**Issue**: `cow_write()` always copies from `manager->template_data` (line 402), but theoretically a handle's `data` pointer might have been updated by another operation. This is correct for the current implementation (handles always start pointing to template), but is fragile if the design evolves.

### P2-U15: hash_table MurmurHash3 unaligned access
**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_hash_table.c` (line 136-138)

**Issue**: `hash_murmur3()` casts `data` to `uint32_t*` and reads 4-byte blocks directly:
```c
const uint32_t* blocks = (const uint32_t*) (data);
for (int i = 0; i < nblocks; i++) {
    uint32_t k = blocks[i];  // Unaligned read on strict-alignment platforms
```

On x86 this works but on ARM/MIPS with strict alignment requirements, this causes a bus error (SIGBUS) or silent data corruption.

**Impact**: Crash on non-x86 architectures.

### P2-U16: deadlock_detector detect_cycle_recursive uses wrong throw message
**File**: `/home/bbrelin/nimcp/src/utils/thread/nimcp_deadlock_detector.c` (line 126-127)

**Issue**: When recursion depth exceeds MAX_THREADS, the throw message says "check_lock_ordering: validation failed" but the function is `detect_cycle_recursive`. Wrong function name in error message.

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_lock_ordering: validation failed");
// ^ Wrong function name: should be "detect_cycle_recursive"
```

### P2-U17: thread_pool nimcp_pool_wait returns error on shutdown even when tasks completed
**File**: `/home/bbrelin/nimcp/src/utils/thread/nimcp_thread_pool.c` (line 1052)

**Issue**: `nimcp_pool_wait()` returns `NIMCP_ERROR_SYSTEM` if `pool->shutdown` is true, even if all tasks completed successfully before shutdown. The caller cannot distinguish between "shutdown interrupted wait" and "all tasks completed then pool was shut down".

### P2-U18: ring_buffer push modulo by capacity could be zero
**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_ring_buffer.c`

**Issue**: While `logical_to_physical()` guards against capacity==0 (line 68), the `push` function (lines 254, 261) uses `% rb->capacity` directly without the same guard. If capacity is somehow 0 (should be impossible after create, but defensive programming principle), this would be division by zero.

**Impact**: Extremely unlikely due to create-time validation, but the inconsistency is worth noting.

---

## P3 Findings (Minor)

### P3-U1: Verbose comments in thread_pool.c
**File**: `/home/bbrelin/nimcp/src/utils/thread/nimcp_thread_pool.c`

**Issue**: 442 lines of comments (lines 1-442) before the actual code begins. While educational, this is excessive for a production file and makes the file harder to navigate. Consider moving architectural documentation to a separate `.md` file.

### P3-U2: hash_table no dynamic resizing
**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_hash_table.c` (lines 53-58)

**Issue**: Hash table has no resize capability. Fixed at creation-time bucket count (default 256). For large entry counts, this degrades lookup from O(1) to O(n). Documented as P3 limitation in the source code itself.

### P3-U3: deadlock_detector uses printf in shutdown while holding lock
**File**: `/home/bbrelin/nimcp/src/utils/thread/nimcp_deadlock_detector.c` (lines 247-255)

**Issue**: `deadlock_detector_shutdown()` calls `printf()` while holding `g_detector_mutex`. If printf itself triggers lock tracking (through the logging system), this could cause issues. The code comments explain this is intentional (using raw printf instead of LOG macros to avoid recursion), but calling printf while holding a mutex is still a blocking I/O operation under lock.

### P3-U4: nimcp_vector.c LOG_DEBUG on every function entry
**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_vector.c`

**Issue**: Every vector math function (dot_product, norm_l2, norm_l1, etc.) calls `LOG_DEBUG("Entering ...")` at entry. For hot-path math operations called millions of times, this adds overhead even when debug logging is disabled (string formatting still occurs in some LOG implementations).

### P3-U5: Unused padding in MPMC queue
**File**: `/home/bbrelin/nimcp/src/utils/containers/nimcp_queue_mpmc.c` (lines 60-63)

**Issue**: The padding arrays `_pad1` and `_pad2` are only meaningful when `sizeof(atomic_size_t) < NIMCP_QUEUE_CACHE_LINE_SIZE`. If `NIMCP_QUEUE_CACHE_LINE_SIZE` equals `sizeof(atomic_size_t)`, the padding is zero-sized which is valid in GNU C but technically non-standard.

---

## Verified Clean Patterns

The following patterns were checked and found to be correct:

1. **Exception directory**: No raw NIMCP_THROW_TO_IMMUNE calls (only comments/references)
2. **Memory core files**: Raw malloc/free properly used in nimcp_memory.c and nimcp_unified_memory.c
3. **Constant-time security**: Properly gates throws with initialization check
4. **setjmp/longjmp volatile**: Both nimcp_try_context_t and signal_recovery_ctx_t properly use volatile for fields modified between setjmp and longjmp
5. **Thread pool**: Correct producer-consumer pattern with proper shutdown sequence (drain queue before exit)
6. **Ring buffer**: Proper overflow checks on capacity * element_size (line 93)
7. **MPMC queue**: Lock-free Vyukov algorithm correctly implemented with cache-line alignment
8. **Thread resource locks**: Proper reference counting with mutex-protected operations
9. **RW lock wrapper**: Clean adapter over pthread_rwlock with consistent error handling
10. **Bio-async handle tracker**: Proper platform_once initialization, reset on shutdown
11. **Bio-router**: TOCTOU fixes with atomic flags and mutex-protected access to brain_kg
12. **BBB input gate**: Proper handling of malloc failure in validation (returns false, not true)
13. **nimcp_tl_rand()**: Thread-safe replacement for rand() is used correctly throughout (with the 3 residual srand() exceptions noted in P1-U1)
14. **Hash table**: Proper integer overflow check avoided in case-insensitive key normalization (stack buffer for small keys, heap fallback)

---

## Files Reviewed

### Fully reviewed (read and analyzed):
- `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception.c`
- `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_immune.c`
- `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_trace.c`
- `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_circuit.c`
- `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_handlers.c`
- `/home/bbrelin/nimcp/src/utils/exception/nimcp_exception_metrics.c`
- `/home/bbrelin/nimcp/src/utils/memory/nimcp_memory.c`
- `/home/bbrelin/nimcp/src/utils/memory/nimcp_unified_memory.c`
- `/home/bbrelin/nimcp/src/utils/memory/nimcp_memory_guards.c`
- `/home/bbrelin/nimcp/src/utils/memory/nimcp_cow_manager.c`
- `/home/bbrelin/nimcp/src/utils/thread/nimcp_thread_mutex.c`
- `/home/bbrelin/nimcp/src/utils/thread/nimcp_thread_pool.c`
- `/home/bbrelin/nimcp/src/utils/thread/nimcp_deadlock_detector.c`
- `/home/bbrelin/nimcp/src/utils/thread/nimcp_thread_rwlock.c`
- `/home/bbrelin/nimcp/src/utils/thread/nimcp_thread_resource.c`
- `/home/bbrelin/nimcp/src/utils/thread/nimcp_thread.c` (partial - first 500 lines)
- `/home/bbrelin/nimcp/src/utils/containers/nimcp_ring_buffer.c`
- `/home/bbrelin/nimcp/src/utils/containers/nimcp_hash_table.c`
- `/home/bbrelin/nimcp/src/utils/containers/nimcp_queue_mpmc.c` (first 200 lines)
- `/home/bbrelin/nimcp/src/utils/containers/nimcp_vector.c` (first 200 lines)
- `/home/bbrelin/nimcp/src/utils/signal/nimcp_signal_handler.c` (key sections)
- `/home/bbrelin/nimcp/src/security/nimcp_constant_time.c`
- `/home/bbrelin/nimcp/src/security/nimcp_bbb_input_gate.c` (first 500 lines)
- `/home/bbrelin/nimcp/src/async/nimcp_bio_async.c` (first 500 lines)
- `/home/bbrelin/nimcp/src/async/nimcp_bio_router.c` (first 500 lines)
- `/home/bbrelin/nimcp/include/utils/exception/nimcp_exception_handlers.h`
- `/home/bbrelin/nimcp/include/utils/signal/nimcp_signal_handler.h`

### Pattern-scanned (grep/search across all files):
- All `src/security/` files (94 files) - scanned for `srand()`, `rand()`, NIMCP_THROW_TO_IMMUNE counts
- All `src/utils/` files (~90 files) - scanned for `srand()`, `rand()`, `setjmp`, `longjmp`, `volatile`, division patterns
- All `src/async/` files (17 files) - scanned for `srand()`, `rand()`
- All `src/api/` files (13 files) - scanned for `srand()`, `rand()`

---

## Recommendations

### Immediate (P1 fixes):
1. Remove all 3 `srand()` calls in statistics modules - they serve no purpose since `nimcp_tl_rand()` has its own seeding
2. Add recursion guard or use `MEMORY_SAFE_THROW` pattern in `nimcp_memory_guards.c` and `nimcp_cow_manager.c`
3. Add lock to `deadlock_detector_print_stats()` or have it call `deadlock_detector_get_stats()` first
4. Deprecate `signal_handler_set_recovery_point()` (global jump buffer) in favor of thread-local `_ex` variant

### Short-term (P2 fixes):
1. Fix wrong error codes (P2-U1, P2-U4, P2-U11, P2-U16)
2. Convert `g_immune_initialized` and `g_trace_system_initialized` to `_Atomic bool`
3. Remove false positive NIMCP_THROW_TO_IMMUNE from ring buffer empty/out-of-range checks
4. Fix MurmurHash3 unaligned access with `memcpy`-based load pattern

### Long-term:
1. Add hash table dynamic resizing when load factor > 0.75
2. Consider replacing verbose thread pool comments with external documentation
3. Review all 636 "is NULL" throws across 94 security files for false positives
