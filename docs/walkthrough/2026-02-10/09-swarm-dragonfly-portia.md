# Walkthrough Pass 4: src/swarm, src/dragonfly, src/portia, src/snn, src/lnn

**Date**: 2026-02-10
**Scope**: ~177 source files across 5 directories
**Reviewer**: Claude Opus 4.6

## Known Issues (Acknowledged, Not Re-Reported)

- Global `g_vote_counts_by_drone` in `swarm_consensus.c` needs per-context locking
- Thread-unsafe const getters in dragonfly (requires mutex redesign)
- Spike queue CAS race in `spike_event.c` (TOCTOU in lock-free queue)

---

## Summary

| Priority | Count |
|----------|-------|
| P1 (Critical/Crash) | 5 |
| P2 (Logic/Correctness) | 18 |
| P3 (Style/Robustness) | 9 |
| **Total** | **32** |

---

## P1 Findings (Critical/Crash)

### P1-1: Deadlock in `nimcp_swarm_immune_update` calling `affinity_maturation` under held mutex

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_immune.c`
**Line**: ~1381
**Category**: Deadlock

**Description**: `nimcp_swarm_immune_update()` acquires `system->mutex` then calls `nimcp_swarm_immune_affinity_maturation(system)`, which also attempts to acquire `system->mutex`. With a non-recursive mutex (the default), this causes deadlock on the calling thread.

**Suggested Fix**: Create an `affinity_maturation_unlocked()` internal helper that assumes the mutex is already held, and call that from within `nimcp_swarm_immune_update()`.

---

### P1-2: Out-of-bounds array access in Kalman filter initialization

**File**: `/home/bbrelin/nimcp/src/dragonfly/nimcp_dragonfly_tracking.c`
**Line**: 158
**Category**: Buffer overflow / out-of-bounds read

**Description**: In `kalman_init_state()`, when `velocity` is non-NULL:
```c
kf->state[3] = velocity[3];  // BUG: should be velocity[0]
kf->state[4] = velocity[1];
kf->state[5] = velocity[2];
```
`velocity` is declared as `const float velocity[3]` (indices 0-2). Accessing `velocity[3]` reads one element past the end of the array, which is undefined behavior. This will read whatever happens to be in the adjacent memory, corrupting the Kalman state and producing incorrect velocity tracking for all dragonfly pursuit operations.

**Suggested Fix**: Change `velocity[3]` to `velocity[0]` on line 158.

---

### P1-3: Static `local_brains` array shared across all `swarm_brain_t` instances

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_brain.c`
**Line**: ~1514
**Category**: Data race

**Description**: `get_local_brains()` returns a pointer to a static local array. If multiple `swarm_brain_t` instances exist (e.g., multi-swarm scenarios), they all share the same array, causing cross-contamination of local brain data. This is a data race if accessed from different threads.

**Suggested Fix**: Move the array into the `swarm_brain_t` struct as a member, or use thread-local storage.

---

### P1-4: Global `_replay_entry_map` array without synchronization

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_memory.c`
**Line**: 83, 89-90, 112-113
**Category**: Data race

**Description**: The static global array `_replay_entry_map[MAX_REPLAY_ENTRIES]` is accessed from `nimcp_replay_heap_insert()` (write) and `nimcp_replay_heap_extract()` (read+clear) without any mutex protection. While `_replay_entry_counter` uses atomic operations, the map itself has no synchronization. Concurrent insert/extract calls will race on the array entries.

**Suggested Fix**: Protect all `_replay_entry_map` accesses with the same mutex that protects the heap operations, or use a per-instance replay map stored in the `NimcpSwarmMemory` struct.

---

### P1-5: `portia_learning_init` leaks `state->mutex` on mutex init failure

**File**: `/home/bbrelin/nimcp/src/portia/nimcp_portia_learning.c`
**Line**: 255-261
**Category**: Memory leak

**Description**: When `nimcp_platform_mutex_init(state->mutex, false)` fails, the cleanup code frees `state->association_table`, `state->habituation_table`, and `state` -- but NOT `state->mutex`, which was allocated on line 246 via `nimcp_malloc`. This leaks `sizeof(nimcp_platform_mutex_t)` bytes on every init failure.

**Suggested Fix**: Add `nimcp_free(state->mutex);` before `nimcp_free(state);` in the mutex init failure cleanup block.

---

## P2 Findings (Logic/Correctness)

### P2-1: False positive NIMCP_THROW_TO_IMMUNE in `compare_resources` (qsort comparator)

**File**: `/home/bbrelin/nimcp/src/portia/nimcp_portia_attention.c`
**Line**: 176
**Category**: False positive throw, performance

**Description**: The qsort comparator `compare_resources()` fires `NIMCP_THROW_TO_IMMUNE` on every comparison where `entry_a->score > entry_b->score`. For N resources, qsort calls this O(N log N) times, generating massive immune system traffic for normal sorting. This is a well-documented false positive pattern.

**Suggested Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call entirely; this is normal comparison behavior, not an error.

---

### P2-2: False positive NIMCP_THROW_TO_IMMUNE in `compare_features_by_level` (qsort comparator)

**File**: `/home/bbrelin/nimcp/src/portia/nimcp_portia_degradation.c`
**Line**: 118
**Category**: False positive throw, performance

**Description**: Same pattern as P2-1. The qsort comparator fires NIMCP_THROW_TO_IMMUNE when `fa->resource_cost < fb->resource_cost`, which is a normal comparison result. Called O(N log N) times per sort.

**Suggested Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call.

---

### P2-3: False positive NIMCP_THROW_TO_IMMUNE in `compare_replay_priority` (qsort comparator)

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_memory.c`
**Line**: 3356
**Category**: False positive throw, performance

**Description**: Same pattern as P2-1/P2-2. The qsort comparator fires NIMCP_THROW_TO_IMMUNE on a normal "greater than" comparison result.

**Suggested Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call.

---

### P2-4: False positive NIMCP_THROW_TO_IMMUNE in `compare_vector_clocks`

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_collective_workspace.c`
**Line**: 158
**Category**: False positive throw

**Description**: `compare_vector_clocks()` fires NIMCP_THROW_TO_IMMUNE when returning -1 (a happened-before b). This is a normal causal ordering result, not an error condition. In an active swarm with frequent updates, this fires on nearly every comparison.

**Suggested Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call; a happened-before ordering is expected behavior.

---

### P2-5: False positive NIMCP_THROW_TO_IMMUNE in `is_byzantine_fault` normal returns

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_consensus.c`
**Lines**: ~1044, ~1085
**Category**: False positive throw

**Description**: `is_byzantine_fault()` fires NIMCP_THROW_TO_IMMUNE when returning false for having fewer than 3 votes (line ~1044) and at the function's final `return false` (line ~1085). Both are normal code paths -- having few votes or no byzantine fault detected are expected outcomes, not errors.

**Suggested Fix**: Remove both NIMCP_THROW_TO_IMMUNE calls; replace with debug logging if needed.

---

### P2-6: False positive NIMCP_THROW_TO_IMMUNE in `consciousness_monitor_thread` normal exit

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_consciousness.c`
**Line**: ~713
**Category**: False positive throw

**Description**: The monitor thread fires NIMCP_THROW_TO_IMMUNE when exiting its main loop normally (i.e., `monitoring_active` becomes false). Thread shutdown is expected behavior, not an error.

**Suggested Fix**: Remove the NIMCP_THROW_TO_IMMUNE; use debug logging instead.

---

### P2-7: `monitoring_active` flag data race in consciousness monitor thread

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_consciousness.c`
**Line**: ~688
**Category**: Data race

**Description**: The `monitoring_active` flag is read in the monitor thread's loop condition without holding the context mutex, but it is written under the mutex in `stop_monitoring()`. On most architectures this "works" but it is technically a data race with undefined behavior per the C standard.

**Suggested Fix**: Use `atomic_bool` for `monitoring_active`, or read it under the mutex in the monitor loop.

---

### P2-8: Pointer-after-unlock in `nimcp_swarm_immune_get_threat`

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_immune.c`
**Line**: ~1303
**Category**: Use-after-free risk

**Description**: `nimcp_swarm_immune_get_threat()` returns a pointer to an internal array element after unlocking the mutex. A concurrent call to update/modify the threat array could invalidate or overwrite the pointed-to data while the caller reads it.

**Suggested Fix**: Copy the threat data into a caller-provided buffer before unlocking the mutex (deep copy pattern), or document that the caller must hold an external lock.

---

### P2-9: Double NIMCP_THROW_TO_IMMUNE in `nimcp_swarm_immune_create` error paths

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_immune.c`
**Lines**: ~337-338, ~352-353
**Category**: Redundant error reporting

**Description**: Error paths in `nimcp_swarm_immune_create` call NIMCP_THROW_TO_IMMUNE twice in succession for the same error (e.g., once for the failed sub-allocation, once for the overall failure). This double-reports to the immune system, making diagnostics confusing.

**Suggested Fix**: Remove one of the duplicate throws on each error path; keep only the most descriptive one.

---

### P2-10: Unaligned pointer cast in `handle_vote_cast`

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_brain.c`
**Lines**: ~776, ~803
**Category**: Undefined behavior (strict aliasing / alignment)

**Description**: `handle_vote_cast()` casts a `void* data` pointer directly to `uint32_t*` and `vote_decision_t*`. Similarly, `handle_workspace_update()` casts `data` to `*(uint32_t*)data` and `*(float*)(data + 4)`. If the data buffer is not naturally aligned for these types, this is undefined behavior on platforms with strict alignment requirements (ARM, SPARC).

**Suggested Fix**: Use `memcpy` to copy data into properly aligned local variables instead of direct pointer casts.

---

### P2-11: Thread-unsafe `swarm_consciousness_ctx_storage` static global

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_consciousness.c`
**Line**: ~101
**Category**: Data race

**Description**: `swarm_consciousness_ctx_storage` is a bare static global without any synchronization. In multi-swarm scenarios, concurrent access from different swarm contexts will race.

**Suggested Fix**: Protect with a mutex or use per-swarm context allocation.

---

### P2-12: Thread-unsafe `local_brain_cache` in `swarm_brain_get_drone_brain_internal`

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_consciousness.c`
**Line**: ~1386-1388
**Category**: Data race

**Description**: `swarm_brain_get_drone_brain_internal()` uses a `static brain_t local_brain_cache` which is shared across all threads. Multiple concurrent callers will corrupt this cache.

**Suggested Fix**: Return a dynamically allocated copy, or use thread-local storage.

---

### P2-13: Missing overflow check in `nimcp_flocking_add_obstacle` capacity doubling

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_flocking.c`
**Line**: ~504
**Category**: Integer overflow

**Description**: When doubling the obstacle array capacity via `realloc`, the new capacity `capacity * 2` has no overflow check. In contrast, `nimcp_flocking_add_boid` (elsewhere in the same file) correctly checks for overflow. If `capacity * 2` overflows, the realloc gets a small size, and subsequent writes overflow the buffer.

**Suggested Fix**: Add overflow check: `if (new_capacity > SIZE_MAX / sizeof(obstacle_t)) { /* error */ }`.

---

### P2-14: `nimcp_replay_heap_extract` false positive NIMCP_THROW_TO_IMMUNE on empty heap

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_memory.c`
**Line**: 108
**Category**: False positive throw

**Description**: When `nimcp_min_heap_extract_min()` returns false (heap empty), the code fires NIMCP_THROW_TO_IMMUNE. An empty heap is a normal condition during replay cycles, not an error.

**Suggested Fix**: Return NULL without throwing; empty heap is expected when no replays are pending.

---

### P2-15: Non-atomic global `g_spike_buffer_overflow_count` counter

**File**: `/home/bbrelin/nimcp/src/snn/nimcp_snn_network.c`
**Lines**: 172-173, 199-209
**Category**: Data race

**Description**: The static globals `g_spike_buffer_overflow_count` and `g_last_overflow_warning_count` are accessed from `record_spike()` which can be called from multiple threads simultaneously during SNN simulation. These are `uint64_t`, not `atomic_uint64_t`, creating a data race. The read-modify-write sequence (`g_spike_buffer_overflow_count++`) is particularly problematic.

**Suggested Fix**: Change to `static _Atomic uint64_t` and use `atomic_fetch_add` for increment.

---

### P2-16: `get_or_create_voxel` false positive NIMCP_THROW_TO_IMMUNE on out-of-bounds

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_pheromone.c`
**Line**: 173
**Category**: False positive throw

**Description**: When `is_index_valid()` returns false (position is outside the grid), `get_or_create_voxel()` fires NIMCP_THROW_TO_IMMUNE. An agent querying a position outside the grid is a normal boundary condition, not an error. The error message "is_index_valid is NULL" is also misleading (it is not a NULL pointer issue).

**Suggested Fix**: Return NULL without throwing; out-of-bounds grid queries are expected behavior. Fix error message if throw is kept for other reasons.

---

### P2-17: `nimcp_hash_table_get` throws on NULL parameters in normal search paths

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_memory.c`
**Line**: 61
**Category**: False positive throw

**Description**: The inline wrapper `nimcp_hash_table_get()` fires NIMCP_THROW_TO_IMMUNE if either `table` or `key` is NULL. While NULL `table` is a genuine error, there may be code paths that pass a NULL key during normal search-miss scenarios. The function is used frequently in the memory consolidation hot path.

**Suggested Fix**: For NULL `table`, keep the throw. For NULL `key`, return NULL without throwing (or review callers to ensure key is never NULL).

---

### P2-18: `check_health_ratio` false positive NIMCP_THROW_TO_IMMUNE on zero connected drones

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_emergence.c`
**Line**: ~261
**Category**: False positive throw

**Description**: `check_health_ratio()` fires NIMCP_THROW_TO_IMMUNE when `connected_drones` is 0. At swarm startup or when a swarm is forming, having zero connected drones is a valid initial state, not an error condition.

**Suggested Fix**: Return a default health ratio (e.g., 0.0) without throwing when there are no connected drones.

---

## P3 Findings (Style/Robustness)

### P3-1: Wrong function names in NIMCP_THROW_TO_IMMUNE messages in flocking module

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_flocking.c`
**Lines**: Multiple (throughout file)
**Category**: Misleading diagnostics

**Description**: Many NIMCP_THROW_TO_IMMUNE messages in functions like `nimcp_flocking_separation`, `nimcp_flocking_alignment`, `nimcp_flocking_cohesion` etc. reference "nimcp_flocking_clear_formation" as the function name. This makes error diagnosis confusing when reading immune system logs.

**Suggested Fix**: Correct each THROW message to reference the actual enclosing function name.

---

### P3-2: Const-cast on mutex in emergence module getters

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_emergence.c`
**Lines**: In `swarm_emergence_get_capabilities()` and `swarm_emergence_get_stats()`
**Category**: Const correctness

**Description**: These functions take `const` context pointers but need to lock the mutex, so they cast away const with `(nimcp_mutex_t*)&ctx->mutex`. This is technically undefined behavior if the original object was declared const.

**Suggested Fix**: Use the `mutable` mutex pattern -- either remove `const` from the function parameter, or declare the mutex as `mutable` (in C, this means the struct should not be const if it has internal synchronization).

---

### P3-3: Const-cast on mutex in `swarm_brain_get_stats`

**File**: `/home/bbrelin/nimcp/src/swarm/nimcp_swarm_brain.c`
**Category**: Const correctness

**Description**: `swarm_brain_get_stats()` takes `const swarm_brain_t*` but calls `nimcp_platform_mutex_lock(swarm->stats_lock)` which requires non-const. This silently casts away const.

**Suggested Fix**: Same as P3-2: either remove const qualifier or use documented mutable pattern.

---

### P3-4: Copy-paste error messages in SNN encoding module

**File**: `/home/bbrelin/nimcp/src/snn/nimcp_snn_encoding.c`
**Lines**: 120, 141, 165, 187, 253, 276, 298
**Category**: Misleading diagnostics

**Description**: Multiple NIMCP_THROW_TO_IMMUNE messages reference "snn_rate_decoder_config_default" or "snn_encoder_destroy" regardless of the actual function context. These appear to be copy-paste artifacts:
- Line 120: `snn_encoder_create_rate` says "snn_rate_decoder_config_default: encoder is NULL"
- Line 253: `snn_decoder_create_rate` says "snn_encoder_destroy: decoder is NULL"

**Suggested Fix**: Update each error message to reference the correct enclosing function.

---

### P3-5: Misleading error code in `lnn_network_create` config validation failure

**File**: `/home/bbrelin/nimcp/src/lnn/nimcp_lnn_network.c`
**Line**: 58
**Category**: Wrong error code

**Description**: When `lnn_config_validate()` fails, the throw uses `NIMCP_ERROR_NO_MEMORY` but the actual error is invalid configuration. The message "validation failed" is correct but the error code is misleading.

**Suggested Fix**: Change to `NIMCP_ERROR_INVALID_PARAM`.

---

### P3-6: Misleading error code in `lnn_network_create_ncp` zero-dimension check

**File**: `/home/bbrelin/nimcp/src/lnn/nimcp_lnn_network.c`
**Line**: 180
**Category**: Wrong error code

**Description**: When dimensions are zero, the throw uses `NIMCP_ERROR_NO_MEMORY` with message "n_inputs is zero". Should be `NIMCP_ERROR_INVALID_PARAM`.

**Suggested Fix**: Change error code to `NIMCP_ERROR_INVALID_PARAM`.

---

### P3-7: Inconsistent error codes in portia_deception_init validation

**File**: `/home/bbrelin/nimcp/src/portia/nimcp_portia_deception.c`
**Lines**: 270, 279, 291
**Category**: Wrong error codes

**Description**: All validation failures in `portia_deception_init` use `NIMCP_ERROR_NULL_POINTER` regardless of the actual issue:
- Line 279: Invalid emission level (should be `NIMCP_ERROR_INVALID_PARAM`)
- Line 291: Memory allocation failure (should be `NIMCP_ERROR_NO_MEMORY`)

**Suggested Fix**: Use appropriate error codes for each failure type.

---

### P3-8: Inconsistent error codes in portia_planning_init validation

**File**: `/home/bbrelin/nimcp/src/portia/nimcp_portia_planning.c`
**Lines**: 233, 240, 252, 265
**Category**: Wrong error codes

**Description**: All validation failures use `NIMCP_ERROR_NULL_POINTER` even for:
- Invalid `max_waypoints` range (should be `NIMCP_ERROR_INVALID_PARAM`)
- Invalid `max_plans` range (should be `NIMCP_ERROR_INVALID_PARAM`)
- Memory allocation failure (should be `NIMCP_ERROR_NO_MEMORY`)

**Suggested Fix**: Use appropriate error codes.

---

### P3-9: `snn_training_destroy` throws NIMCP_THROW_TO_IMMUNE on NULL input

**File**: `/home/bbrelin/nimcp/src/snn/nimcp_snn_training.c`
**Line**: 239
**Category**: Defensive programming anti-pattern

**Description**: `snn_training_destroy(NULL)` fires NIMCP_THROW_TO_IMMUNE and returns. Destroy/free functions should be NULL-safe without throwing (following the `free(NULL)` convention). This can cause spurious immune system alerts during cleanup paths where the context may legitimately be NULL (e.g., partial initialization rollback).

**Suggested Fix**: Change to silently return on NULL: `if (!ctx) return;` without throwing.

---

## Appendix: Files Reviewed

### src/swarm/ (63 files)
Core files read in detail: `nimcp_swarm_consensus.c`, `nimcp_swarm_brain.c`, `nimcp_swarm_consciousness.c`, `nimcp_swarm_emergence.c`, `nimcp_swarm_flocking.c`, `nimcp_swarm_immune.c`, `nimcp_swarm_memory.c`, `nimcp_swarm_pheromone.c`, `nimcp_collective_workspace.c`.
Pattern-scanned: All 63 files for THROW_TO_IMMUNE patterns, mutex lock/unlock, qsort comparators.

### src/dragonfly/ (33 files)
Core files read in detail: `nimcp_dragonfly.c`, `nimcp_dragonfly_tracking.c`, `nimcp_dragonfly_collision.c`, `nimcp_dragonfly_energy.c`, `nimcp_dragonfly_learning.c`, `nimcp_dragonfly_prediction.c`, `nimcp_dragonfly_intercept.c`, `nimcp_dragonfly_tsdn.c`.
Pattern-scanned: All 33 files for THROW_TO_IMMUNE patterns.

### src/portia/ (19 files)
Core files read in detail: `nimcp_portia.c`, `nimcp_portia_planning.c`, `nimcp_portia_deception.c`, `nimcp_portia_learning.c`, `nimcp_portia_attention.c`, `nimcp_portia_swarm_logic_bridge.c`, `nimcp_portia_collective_bridge.c`, `nimcp_portia_degradation.c`.
Pattern-scanned: All 19 files for THROW_TO_IMMUNE patterns, qsort comparators.

### src/snn/ (46 files)
Core files read in detail: `nimcp_snn_network.c`, `nimcp_snn_training.c`, `nimcp_snn_encoding.c`.
Pattern-scanned: All 46 files for THROW_TO_IMMUNE patterns.

### src/lnn/ (15 files)
Core files read in detail: `nimcp_lnn.c`, `nimcp_lnn_network.c`, `nimcp_lnn_layer.c`.
Pattern-scanned: All 15 files for THROW_TO_IMMUNE patterns.

### Automated Pattern Searches Performed
1. `NIMCP_THROW_TO_IMMUNE` followed by return (all 5 dirs) -- verified proper guard clause pattern
2. `NIMCP_THROW_TO_IMMUNE` at end of line without immediate return (all 5 dirs) -- checked for missing returns
3. `qsort`/`compare_` functions (swarm, portia) -- found false positive throws in comparators
4. `velocity[3]` out-of-bounds access (dragonfly tracking)
5. Static global variables without synchronization (snn, swarm)
6. Mutex lock/unlock mismatch patterns (all 5 dirs)
