# Security Small Domain Bridges Walkthrough (04c2)

**Date**: 2026-02-10
**Scope**: 9 `.c` files across 5 subdirectories under `src/security/`
**Reviewer**: Claude Opus 4.6

---

## Files Reviewed

| # | File | Lines |
|---|------|-------|
| 1 | `src/security/async/nimcp_security_async_fep_bridge.c` | ~1445 |
| 2 | `src/security/async/nimcp_security_async_bridge.c` | ~1657 |
| 3 | `src/security/collective/nimcp_security_collective_fep_bridge.c` | ~1042 |
| 4 | `src/security/collective/nimcp_security_collective_bridge.c` | ~1236 |
| 5 | `src/security/distributed/nimcp_security_distributed_training_fep_bridge.c` | ~919 |
| 6 | `src/security/distributed/nimcp_security_distributed_training_bridge.c` | ~1493 |
| 7 | `src/security/executive/nimcp_security_executive_bridge.c` | ~1135 |
| 8 | `src/security/game_theory/nimcp_security_game_theory_fep_bridge.c` | ~1431 |
| 9 | `src/security/game_theory/nimcp_security_game_theory_bridge.c` | ~1204 |

---

## P1 Findings (4)

### P1-1: Deadlock in `sgt_fep_compute_effects` calling `sgt_fep_update`
- **File**: `src/security/game_theory/nimcp_security_game_theory_fep_bridge.c`
- **Lines**: 424, 346
- **Description**: `sgt_fep_compute_effects()` locks `bridge->base.mutex` at line 424 via `nimcp_platform_mutex_lock()`, then calls `sgt_fep_update(bridge)` at line 427. `sgt_fep_update()` also calls `nimcp_platform_mutex_lock(bridge->base.mutex)` at line 346. If the mutex is non-recursive (the NIMCP default), this is a guaranteed deadlock on every call.
- **Fix**: Either (a) extract the body of `sgt_fep_update` into a `sgt_fep_update_unlocked()` helper and call that from `sgt_fep_compute_effects`, or (b) have `sgt_fep_compute_effects` call `sgt_fep_update_unlocked` instead of `sgt_fep_update`.

### P1-2: NULL dereference of `bridge->fep_system` in `sgt_fep_update`
- **File**: `src/security/game_theory/nimcp_security_game_theory_fep_bridge.c`
- **Line**: 349
- **Description**: `sgt_fep_update()` checks `!bridge` and `!bridge->state.active`, but does NOT check `bridge->fep_system` for NULL before passing it to `fep_get_free_energy()` at line 349, `fep_compute_surprise()` at line 350, and `fep_get_prediction_error()` at line 372. If `fep_system` is NULL, this is a NULL dereference.
- **Fix**: Add `if (!bridge->fep_system) { return NIMCP_ERROR_INVALID_STATE; }` after the `!bridge->state.active` check at line 342.

### P1-3: NULL dereference of `bridge->fep_system` in `sec_async_fep_compute_effects`
- **File**: `src/security/async/nimcp_security_async_fep_bridge.c`
- **Line**: 376
- **Description**: `sec_async_fep_compute_effects()` checks `!bridge` and `!bridge->state.active`, but does NOT check `bridge->fep_system` for NULL before passing it to `fep_get_free_energy()` at line 376, `fep_compute_surprise()` at line 377, and `fep_get_prediction_error()` at line 378. Compare with `security_dist_fep_compute_effects()` (distributed FEP bridge, line 386) which correctly checks `!bridge->fep_system`.
- **Fix**: Change line 368 guard to: `if (!bridge->state.active || !bridge->fep_system) {` and update the error message.

### P1-4: NULL dereference of `bridge->fep_system` in `security_collective_fep_compute_effects`
- **File**: `src/security/collective/nimcp_security_collective_fep_bridge.c`
- **Line**: 321
- **Description**: `security_collective_fep_compute_effects()` checks `!bridge` and `!bridge->state.active`, but does NOT check `bridge->fep_system` for NULL before passing it to `fep_get_free_energy()` at line 321 and `fep_compute_surprise()` at line 322. Compare with the distributed FEP bridge which correctly checks.
- **Fix**: Add `if (!bridge->fep_system) { nimcp_platform_mutex_unlock(bridge->base.mutex); return NIMCP_ERROR_INVALID_STATE; }` before line 321, or add the check before the mutex lock like the distributed variant does.

---

## P2 Findings (21)

### P2-1: Wrong error code for inactive state check
- **File**: `src/security/async/nimcp_security_async_fep_bridge.c`
- **Lines**: 368-370
- **Description**: `!bridge->state.active` throws `NIMCP_ERROR_NULL_POINTER` with message "bridge->state is NULL". This is not a NULL pointer -- it is an inactive state. The error code and message are both wrong.
- **Fix**: Change to `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "sec_async_fep_compute_effects: bridge is not active")`.

### P2-2: Stats bug - avg_free_energy assigned from avg_surprise
- **File**: `src/security/async/nimcp_security_async_fep_bridge.c`
- **Line**: 418
- **Description**: `bridge->stats.avg_free_energy = bridge->state.avg_surprise` copies the wrong field. Line 419 also copies `avg_surprise`, so both stats end up with the surprise value. The free energy average is lost.
- **Fix**: Change line 418 to: `bridge->stats.avg_free_energy = bridge->state.avg_free_energy;`

### P2-3: False positive NIMCP_THROW_TO_IMMUNE in boolean query `is_bio_async_connected`
- **File**: `src/security/async/nimcp_security_async_bridge.c`
- **Lines**: 463-465
- **Description**: `security_async_is_bio_async_connected()` throws `NIMCP_THROW_TO_IMMUNE` when bridge is NULL. Boolean query functions should not throw to the immune system -- they should silently return `false`. This fires on every NULL check, generating O(1) immune events per call.
- **Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call; just `return false`.

### P2-4: Wrong error code for NULL bridge in `broadcast_rate_limit`
- **File**: `src/security/async/nimcp_security_async_bridge.c`
- **Lines**: 783-784
- **Description**: Returns `NIMCP_ERROR_OPERATION_FAILED` when `bridge` is NULL. The correct error code for a NULL pointer argument is `NIMCP_ERROR_NULL_POINTER`.
- **Fix**: Change to `return NIMCP_ERROR_NULL_POINTER;`

### P2-5: False positive NIMCP_THROW_TO_IMMUNE in boolean query `is_connected`
- **File**: `src/security/async/nimcp_security_async_bridge.c`
- **Lines**: 1353-1355
- **Description**: `security_async_is_connected()` throws `NIMCP_THROW_TO_IMMUNE` on NULL bridge. Boolean query functions should not throw -- just return `false`.
- **Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call; just `return false`.

### P2-6: Wrong error code for cache full condition
- **File**: `src/security/async/nimcp_security_async_bridge.c`
- **Line**: 1392
- **Description**: Returns `NIMCP_ERROR_MUTEX_INIT` when the threat intel cache is full. This error code is entirely unrelated to capacity limits. Should be `NIMCP_ERROR_OPERATION_FAILED` or a capacity error.
- **Fix**: Change to `return NIMCP_ERROR_OPERATION_FAILED;`

### P2-7: False positive NIMCP_THROW_TO_IMMUNE on lookup not found
- **File**: `src/security/async/nimcp_security_async_bridge.c`
- **Line**: 1432
- **Description**: `security_async_lookup_threat_intel()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, ...)` when a threat hash is not found in the cache. Lookup not-found is normal behavior, not an error. This generates an immune event for every cache miss.
- **Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call; just `return false`.

### P2-8: Wrong error code for NULL bridge in `get_intel_stats`
- **File**: `src/security/async/nimcp_security_async_bridge.c`
- **Line**: 1460
- **Description**: Returns `NIMCP_ERROR_OPERATION_FAILED` for NULL bridge. Should be `NIMCP_ERROR_NULL_POINTER`.
- **Fix**: Change to `return NIMCP_ERROR_NULL_POINTER;`

### P2-9: False positive NIMCP_THROW_TO_IMMUNE in boolean query `is_emergency_mode`
- **File**: `src/security/async/nimcp_security_async_bridge.c`
- **Lines**: 1559-1561
- **Description**: `security_async_is_emergency_mode()` throws `NIMCP_THROW_TO_IMMUNE` on NULL bridge. Boolean query functions should not throw -- just return `false`.
- **Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call; just `return false`.

### P2-10: Wrong error code for NULL bio_ctx in `send_bio_message`
- **File**: `src/security/async/nimcp_security_async_bridge.c`
- **Line**: 1599
- **Description**: Returns `NIMCP_ERROR_MUTEX_INIT` when `bridge->base.bio_ctx` is NULL. This is completely unrelated to mutex initialization. Should be `NIMCP_ERROR_INVALID_STATE` (bio_ctx not connected).
- **Fix**: Change to `return NIMCP_ERROR_INVALID_STATE;`

### P2-11: Wrong error code for bio-async connection failure
- **File**: `src/security/collective/nimcp_security_collective_fep_bridge.c`
- **Line**: 858
- **Description**: Throws `NIMCP_ERROR_INVALID_PARAM` when bio-async connection fails (`bio_router_register_module` returned NULL). This is a connection/operation failure, not an invalid parameter.
- **Fix**: Change to `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "security_collective_fep_connect_bio_async: bio-async connection failed")`.

### P2-12: False positive NIMCP_THROW_TO_IMMUNE in `find_worker_index` not-found path
- **File**: `src/security/distributed/nimcp_security_distributed_training_fep_bridge.c`
- **Line**: 160
- **Description**: `find_worker_index()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, ...)` when a worker ID is not found in the array. Search not-found is normal behavior during worker management, not an error worth reporting to the immune system.
- **Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call at line 160; just `return -1`.

### P2-13: Memory leak in `security_dist_fep_reset` - worker_precisions pointer zeroed without free
- **File**: `src/security/distributed/nimcp_security_distributed_training_fep_bridge.c`
- **Lines**: 323-330
- **Description**: At lines 323-327, the code iterates `bridge->fep_effects.worker_precisions` to reset values. Then at line 330, `memset(&bridge->fep_effects, 0, sizeof(...))` zeroes the entire `fep_effects` struct, including the `worker_precisions` pointer. The allocated memory is leaked because the pointer is zeroed without being freed first.
- **Fix**: Add `nimcp_free(bridge->fep_effects.worker_precisions);` before the `memset` at line 330. Or restructure to preserve the pointer across reset.

### P2-14: Wrong error message for inactive state check
- **File**: `src/security/distributed/nimcp_security_distributed_training_fep_bridge.c`
- **Line**: 387
- **Description**: The guard `!bridge->state.active || !bridge->fep_system` throws with message "required parameter is NULL (bridge->state, bridge->fep_system)". The first condition (`!bridge->state.active`) is a boolean state check, not a NULL check. The message is misleading.
- **Fix**: Change message to: "security_dist_fep_compute_effects: bridge inactive or fep_system is NULL".

### P2-15: Allocation count set before verifying malloc success
- **File**: `src/security/distributed/nimcp_security_distributed_training_fep_bridge.c`
- **Lines**: 467-468
- **Description**: After `nimcp_malloc()` at line 467, `num_worker_precisions` is set to `num_workers` at line 468 regardless of whether the allocation succeeded. If `nimcp_malloc` returns NULL, the count is wrong. On the next call, the count matches so no reallocation is attempted, and the NULL pointer update loop is silently skipped forever.
- **Fix**: Move line 468 inside a `if (bridge->fep_effects.worker_precisions)` guard, or set it to 0 on failure.

### P2-16: False positive NIMCP_THROW_TO_IMMUNE in `hash_equals`
- **File**: `src/security/distributed/nimcp_security_distributed_training_bridge.c`
- **Line**: 134
- **Description**: `hash_equals()` is an internal static helper that throws `NIMCP_THROW_TO_IMMUNE` when either hash pointer is NULL. This is a hot comparison function called in loops; throwing to immune on NULL is excessive overhead. Callers should validate inputs before calling.
- **Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call; just `return false`.

### P2-17: Race condition - stats modified after mutex unlock
- **File**: `src/security/distributed/nimcp_security_distributed_training_bridge.c`
- **Line**: 446
- **Description**: `stats->total_workers_registered++` at line 446 occurs after `BRIDGE_UNLOCK(bridge)` at line 444. The stats modification is unprotected by the mutex, creating a data race if multiple threads register workers concurrently.
- **Fix**: Move `stats->total_workers_registered++` before the `BRIDGE_UNLOCK(bridge)` call at line 444.

### P2-18: False positive NIMCP_THROW_TO_IMMUNE on zero params validation
- **File**: `src/security/distributed/nimcp_security_distributed_training_bridge.c`
- **Line**: 978
- **Description**: `security_distributed_training_validate_aggregated()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, ...)` when `num_params == 0`. Zero params is a validation rejection, not a security event worth reporting to the immune system.
- **Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call; just `return false`.

### P2-19: False positive NIMCP_THROW_TO_IMMUNE in boolean query `is_under_attack`
- **File**: `src/security/distributed/nimcp_security_distributed_training_bridge.c`
- **Lines**: 1329-1331
- **Description**: `security_distributed_training_is_under_attack()` throws `NIMCP_THROW_TO_IMMUNE` on NULL bridge. Boolean query functions should not throw to immune -- just return `false`.
- **Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call; just `return false`.

### P2-20: Wrong error code for allocation failure in executive bridge create
- **File**: `src/security/executive/nimcp_security_executive_bridge.c`
- **Line**: 196
- **Description**: `security_executive_bridge_create()` throws `NIMCP_ERROR_NULL_POINTER` with message "bridge is NULL" when `nimcp_malloc` fails. This is an allocation failure, not a NULL pointer argument. The error code should be `NIMCP_ERROR_NO_MEMORY`.
- **Fix**: Change to `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_executive_bridge_create: allocation failed")`.

### P2-21: Wrong error code for allocation failure in game theory bridge create
- **File**: `src/security/game_theory/nimcp_security_game_theory_bridge.c`
- **Line**: 204
- **Description**: `security_gt_bridge_create()` throws `NIMCP_ERROR_NULL_POINTER` with message "bridge is NULL" when `nimcp_malloc` fails. Same issue as P2-20.
- **Fix**: Change to `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_gt_bridge_create: allocation failed")`.

---

## P3 Findings (2)

### P3-1: Unsigned integer underflow in deadline warning computation
- **File**: `src/security/executive/nimcp_security_executive_bridge.c`
- **Line**: 626
- **Description**: `uint64_t warning_threshold = deadline_ms - bridge->config.deadline_grace_period_ms` will wrap to near-UINT64_MAX if `deadline_grace_period_ms > deadline_ms`. The effect is that `now > warning_threshold` is always false, so deadline warnings are silently suppressed for tasks with very short deadlines relative to the grace period.
- **Fix**: Add a guard: `if (bridge->config.deadline_grace_period_ms >= deadline_ms) { /* skip warning check */ } else { uint64_t warning_threshold = deadline_ms - bridge->config.deadline_grace_period_ms; ... }`

### P3-2: Ambiguous time unit in `last_update_time`
- **File**: `src/security/game_theory/nimcp_security_game_theory_fep_bridge.c`
- **Line**: 393
- **Description**: `nimcp_platform_time_monotonic_us() * 1000ULL` converts microseconds to nanoseconds, stored in `last_update_time` (uint64_t). The field name and header comment say "timestamp" without specifying the unit. This pattern is consistent within this file (9 occurrences) but differs from codebase convention where timestamps are typically in milliseconds (`_ms`) or microseconds (`_us`).
- **Fix**: Either rename the field to `last_update_time_ns` for clarity, or convert to a standard unit (e.g., `nimcp_platform_time_monotonic_us()` for microseconds).

---

## Summary

| Priority | Count | Categories |
|----------|-------|------------|
| **P1** | 4 | 1 deadlock, 3 NULL dereference |
| **P2** | 21 | 8 false positive NIMCP_THROW_TO_IMMUNE, 8 wrong error code, 1 stats copy bug, 1 memory leak, 1 race condition, 1 wrong error message, 1 allocation count mismatch |
| **P3** | 2 | 1 unsigned underflow, 1 ambiguous time unit |
| **Total** | **27** | |

### Files by Finding Count

| File | P1 | P2 | P3 | Total |
|------|----|----|----|----|
| `async/nimcp_security_async_bridge.c` | 0 | 8 | 0 | 8 |
| `game_theory/nimcp_security_game_theory_fep_bridge.c` | 2 | 0 | 1 | 3 |
| `distributed/nimcp_security_distributed_training_fep_bridge.c` | 0 | 4 | 0 | 4 |
| `distributed/nimcp_security_distributed_training_bridge.c` | 0 | 4 | 0 | 4 |
| `async/nimcp_security_async_fep_bridge.c` | 1 | 2 | 0 | 3 |
| `collective/nimcp_security_collective_fep_bridge.c` | 1 | 1 | 0 | 2 |
| `executive/nimcp_security_executive_bridge.c` | 0 | 1 | 1 | 2 |
| `game_theory/nimcp_security_game_theory_bridge.c` | 0 | 1 | 0 | 1 |
| `collective/nimcp_security_collective_bridge.c` | 0 | 0 | 0 | 0 |

### Notes
- `BRIDGE_NULL_CHECK` (defined in `include/utils/bridge/nimcp_bridge_base.h:412`) includes `return NIMCP_ERROR_NULL_POINTER` -- no missing-return-after-throw issues for files using this macro.
- `BRIDGE_NULL_CHECK_BOOL` (line 422) includes `return false` -- same, no issue.
- The P1-1 deadlock in `sgt_fep_compute_effects` is a guaranteed hang on every invocation since the default NIMCP mutex type is non-recursive.
- The three P1 fep_system NULL dereferences (P1-2, P1-3, P1-4) follow the same pattern: checking `bridge->state.active` but not `bridge->fep_system`. The distributed FEP bridge is the only one that correctly checks both.
- `collective/nimcp_security_collective_bridge.c` is the cleanest file -- no findings. It consistently uses `BRIDGE_NULL_CHECK` macros and standard patterns.
