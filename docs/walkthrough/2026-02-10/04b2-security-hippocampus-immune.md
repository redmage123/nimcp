# Walkthrough 04b2: Security - Hippocampus, Immune, Integration

**Date**: 2026-02-10
**Directories reviewed**:
- `src/security/hippocampus/` (2 files)
- `src/security/immune/` (5 files)
- `src/security/integration/` (1 file)

**Total files**: 8

---

## Findings

### P1: Critical (NULL deref, buffer overflow, use-after-free, missing return after throw, TOCTOU)

#### P1-01: TOCTOU in verify_all_consolidations - reads array data while unlocked
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_bridge.c`
- **Line**: 811-814
- **Description**: `security_hippocampus_verify_all_consolidations()` unlocks the bridge at line 811, then immediately accesses `bridge->consolidation_events[i].memory_id` while unlocked at line 813. Another thread could modify or reallocate the `consolidation_events` array, invalidate index `i`, or change `memory_id` between the unlock and the call to `verify_consolidation`. This is a data race that could cause reading stale/corrupt data or out-of-bounds access.
- **Fix**: Copy `memory_id` into a local variable while the lock is held before unlocking, or redesign to use an internal `_unlocked()` variant of `verify_consolidation` to avoid the unlock/relock pattern.

#### P1-02: TOCTOU in bridge_update - reads shared state while unlocked
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_bridge.c`
- **Lines**: 1460-1487
- **Description**: `security_hippocampus_bridge_update()` repeatedly unlocks the bridge, reads shared state while unlocked, then calls public functions that re-lock. Specifically: line 1463 reads `bridge->hippo_effects.current_sleep_phase` while unlocked; line 1470 reads `bridge->state.last_inject_scan` while unlocked; line 1481 reads `bridge->state.last_coherence_check` while unlocked. Another thread modifying these fields creates a data race.
- **Fix**: Read all needed values into local variables while locked, then unlock and call the public functions with the local copies.

#### P1-03: TOCTOU in sec_hippo_fep_update - state mutation window between unlock/relock
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_fep_bridge.c`
- **Lines**: 500-508
- **Description**: `sec_hippo_fep_update()` acquires the lock at line 500, immediately unlocks at line 503, calls `sec_hippo_fep_compute_effects()` (which locks internally), then re-locks at line 508. Between the unlock and re-lock, another thread could modify bridge state, making the subsequent precision modulation calculations (lines 511-541) operate on inconsistent data.
- **Fix**: Create an internal `_unlocked()` variant of `sec_hippo_fep_compute_effects()` and call it while holding the lock.

---

### P2: Significant (resource leaks, wrong error codes, false positive NIMCP_THROW_TO_IMMUNE)

#### P2-01: False positive NIMCP_THROW_TO_IMMUNE on encoding "not found" path
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_bridge.c`
- **Line**: 223
- **Description**: `find_encoding()` fires `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_encoding: internal is NULL")` when an encoding is simply not found by ID. This is a normal search "not found" return, not a security event. The error message is also incorrect - it says "internal is NULL" when the actual condition is "encoding not found". This fires O(N) times per lookup miss.
- **Fix**: Remove the NIMCP_THROW_TO_IMMUNE. Simply `return NULL` for not-found.

#### P2-02: False positive NIMCP_THROW_TO_IMMUNE on bool query functions (hippocampus bridge)
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_bridge.c`
- **Lines**: 583, 699, 1731
- **Description**: Three bool-returning query functions throw to immune system when bridge is NULL: `security_hippocampus_is_fully_connected()` (line 583), `security_hippocampus_is_sleep_protected()` (line 699), `security_hippocampus_is_bio_async_connected()` (line 1731). These are read-only queries where returning `false` for NULL is the normal defensive pattern, not a security event.
- **Fix**: Remove NIMCP_THROW_TO_IMMUNE from all three; just return false.

#### P2-03: Duplicate NIMCP_THROW_TO_IMMUNE in create function error paths
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_bridge.c`
- **Lines**: 405+411, 421+428
- **Description**: In `security_hippocampus_bridge_create()`, the audit_log allocation failure path (lines 404-412) fires NIMCP_THROW_TO_IMMUNE twice - once at line 405 before cleanup and again at line 411 after cleanup. Similarly for the internal state allocation failure (lines 420-429) which throws at 421 and again at 428. The second throw in each case is redundant and generates duplicate immune system alerts for a single event.
- **Fix**: Remove the second NIMCP_THROW_TO_IMMUNE at lines 411 and 428.

#### P2-04: False positive NIMCP_THROW_TO_IMMUNE in disconnect functions (all immune bridges)
- **File**: `src/security/immune/nimcp_anomaly_immune_bridge.c` line 434
- **File**: `src/security/immune/nimcp_pattern_db_immune_bridge.c` line 475
- **File**: `src/security/immune/nimcp_rate_limiter_immune_bridge.c` line 432
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c` line 1693
- **Description**: All four `*_disconnect_bio_async()` functions fire NIMCP_THROW_TO_IMMUNE when bridge is NULL OR when `bio_async_enabled` is false. Disconnecting when not connected is a normal no-op, not a security event. The error message in all cases incorrectly says "required parameter is NULL (bridge, bridge->base)".
- **Fix**: Return 0 (no-op) when not connected; only throw if bridge itself is NULL.

#### P2-05: Resource leak on bridge_base_init failure in pattern_db_immune_create
- **File**: `src/security/immune/nimcp_pattern_db_immune_bridge.c`
- **Line**: 239
- **Description**: When `bridge_base_init()` fails at line 239, only `nimcp_free(bridge)` is called. But `bridge->mappings` was allocated at line 228-229 and is not freed in this error path, causing a memory leak.
- **Fix**: Add `nimcp_free(bridge->mappings)` before `nimcp_free(bridge)` on the `bridge_base_init` failure path.

#### P2-06: False positive NIMCP_THROW_TO_IMMUNE on search "not found" paths (pattern_db, unified, bio_async)
- **File**: `src/security/immune/nimcp_pattern_db_immune_bridge.c` line 521
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c` lines 292, 314
- **File**: `src/security/integration/nimcp_security_bio_async_bridge.c` line 130
- **Description**: Multiple search/lookup functions fire NIMCP_THROW_TO_IMMUNE when an item is not found:
  - `pattern_db_immune_get_mapping()` (line 521): throws on pattern not found
  - `find_tolerance_entry()` (line 292): throws on tolerance entry not found
  - `find_memory_cell_by_id()` (line 314): throws on memory cell not found
  - `find_subscription()` (line 130): throws on subscription not found
  These are all normal search miss paths that fire O(N) times per lookup. They are not security events.
- **Fix**: Remove NIMCP_THROW_TO_IMMUNE from all "not found" return paths. Simply return NULL or -1.

#### P2-07: Wrong error code NIMCP_ERROR_NO_MEMORY used for NULL pointer checks
- **File**: `src/security/immune/nimcp_security_immune_fep_bridge.c` line 183
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c` lines 246, 305, 314, 1254, 1352, 1356
- **Description**: Multiple functions use `NIMCP_ERROR_NO_MEMORY` when the actual error is a NULL pointer argument. For example, `security_immune_fep_create()` at line 183 throws `NIMCP_ERROR_NO_MEMORY` when `unified_bridge`, `immune_system`, or `fep_system` is NULL - should be `NIMCP_ERROR_NULL_POINTER`. Similarly `allocate_tolerance_whitelist()` (line 246), `find_memory_cell_by_id()` (line 305), `sec_immune_unified_form_memory()` (line 1254), and `sec_immune_unified_check_memory()` (lines 1352, 1356) all use the wrong error code.
- **Fix**: Change to `NIMCP_ERROR_NULL_POINTER` for NULL argument checks; keep `NIMCP_ERROR_NO_MEMORY` only for allocation failures.

#### P2-08: Wrong error code NIMCP_ERROR_MUTEX_INIT used for capacity/not-found errors
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c`
- **Lines**: 1265, 1376, 1446
- **Description**: `NIMCP_ERROR_MUTEX_INIT` (6004, "Mutex init failed") is used incorrectly in three places:
  - Line 1265: `sec_immune_unified_form_memory()` returns it when memory cell capacity is exceeded (should be `NIMCP_ERROR_OUT_OF_RANGE`)
  - Line 1376: `sec_immune_unified_check_memory()` returns it when memory is not found (should be `NIMCP_ERROR_NOT_FOUND`)
  - Line 1446: `sec_immune_unified_add_tolerance()` returns it when whitelist capacity is exceeded (should be `NIMCP_ERROR_OUT_OF_RANGE`)
- **Fix**: Use `NIMCP_ERROR_OUT_OF_RANGE` for capacity exceeded and `NIMCP_ERROR_NOT_FOUND` for search misses.

#### P2-09: Wrong error code NIMCP_ERROR_NULL_POINTER for allocation failures
- **File**: `src/security/immune/nimcp_anomaly_immune_bridge.c` line 212
- **File**: `src/security/immune/nimcp_pattern_db_immune_bridge.c` line 208
- **File**: `src/security/immune/nimcp_rate_limiter_immune_bridge.c` line 211
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c` lines 427, 1734
- **Description**: When `nimcp_malloc()` returns NULL (allocation failure), these locations throw `NIMCP_ERROR_NULL_POINTER` instead of the correct `NIMCP_ERROR_NO_MEMORY`. The error message says "bridge is NULL" or "msg_buffer is NULL" rather than indicating an allocation failure.
- **Fix**: Change to `NIMCP_ERROR_NO_MEMORY` with descriptive "allocation failed" messages.

#### P2-10: Wrong error code NIMCP_ERROR_INVALID_PARAM for bio-async connection failure
- **File**: `src/security/immune/nimcp_security_immune_fep_bridge.c`
- **Line**: 1108
- **Description**: `security_immune_fep_connect_bio_async()` throws `NIMCP_ERROR_INVALID_PARAM` when `bio_router_register_module()` fails. The error is not an invalid parameter - the registration itself failed. Should be `NIMCP_ERROR_OPERATION_FAILED` or `NIMCP_ERROR_INVALID_STATE`.
- **Fix**: Change to `NIMCP_ERROR_OPERATION_FAILED`.

#### P2-11: False positive NIMCP_THROW_TO_IMMUNE on bool query functions (immune bridges)
- **File**: `src/security/immune/nimcp_security_immune_fep_bridge.c` line 985
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c` line 1804
- **Description**: `security_immune_fep_is_emergency_mode()` and `sec_immune_unified_is_emergency_mode()` throw to the immune system on NULL bridge for read-only bool queries. Returning `false` for NULL is the standard defensive pattern and should not generate immune alerts.
- **Fix**: Remove NIMCP_THROW_TO_IMMUNE; just return false.

#### P2-12: False positive NIMCP_THROW_TO_IMMUNE with wrong message on disabled feature
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c`
- **Line**: 1523
- **Description**: `sec_immune_unified_is_tolerated()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge->config is NULL")` when `enable_tolerance_system` is false. The tolerance system being disabled is a normal configuration state, not a NULL pointer error or a security event. The error message is also completely wrong.
- **Fix**: Remove the NIMCP_THROW_TO_IMMUNE; just return false.

#### P2-13: False positive NIMCP_THROW_TO_IMMUNE on validation rejection paths (bio_async_bridge)
- **File**: `src/security/integration/nimcp_security_bio_async_bridge.c`
- **Lines**: 252, 437, 479, 641
- **Description**: Four locations throw to the immune system for normal validation rejections:
  - Line 252: `security_bio_bridge_connect()` - already connected
  - Line 437: `security_bio_bridge_initiate_lockdown()` - already in lockdown
  - Line 479: `security_bio_bridge_end_lockdown()` - not in lockdown (wrong message: "bridge->internal is NULL")
  - Line 641: `security_bio_bridge_propose_consensus()` - consensus disabled (wrong message: "bridge->config is NULL")
  These are normal operational states, not security events. Lines 479 and 641 also have incorrect error messages.
- **Fix**: Remove NIMCP_THROW_TO_IMMUNE; return appropriate error codes without throwing.

#### P2-14: Subscription slot leak in unsubscribe_module
- **File**: `src/security/integration/nimcp_security_bio_async_bridge.c`
- **Line**: 830-833
- **Description**: `security_bio_bridge_unsubscribe_module()` sets `sub->active = false` but does not decrement `subscription_count` (only decrements `stats.active_subscriptions`). Since `subscribe_module()` at line 792 checks `subscription_count >= subscription_capacity`, unsubscribed slots are never reclaimed. After enough subscribe/unsubscribe cycles, the bridge permanently runs out of subscription capacity.
- **Fix**: Either compact the array when unsubscribing (shift entries down) or change subscribe to scan for inactive slots before checking capacity.

#### P2-15: False positive NIMCP_THROW_TO_IMMUNE on unified bridge broadcast when not connected
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c`
- **Lines**: 1718-1721
- **Description**: `sec_immune_unified_broadcast_security_event()` throws `NIMCP_THROW_TO_IMMUNE` when `bridge->base.bio_async_enabled` is false. Not being connected to bio-async is a normal configuration state. The error message incorrectly says "required parameter is NULL".
- **Fix**: Return an error code without throwing, or return 0 (no-op) when not connected.

#### P2-16: const-correctness violation casting away const for mutex lock
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c`
- **Line**: 1827
- **Description**: `sec_immune_unified_get_stats()` takes `const sec_immune_unified_bridge_t* bridge` but casts away const to lock the mutex: `nimcp_platform_mutex_lock(((sec_immune_unified_bridge_t*)bridge)->base.mutex)`. This violates the const contract and could cause undefined behavior if the caller passes a truly read-only object.
- **Fix**: Either remove const from the bridge parameter or make the mutex field `mutable` (in C, remove const from the parameter since C has no `mutable`).

---

### P3: Minor (missing validation, code quality)

#### P3-01: Missing mutex initialization in security_bio_bridge
- **File**: `src/security/integration/nimcp_security_bio_async_bridge.c`
- **Line**: 197-224
- **Description**: `security_bio_bridge_create()` never calls `bridge_base_init()` and never creates a mutex for the bridge. The `bridge_base_t base` field's mutex pointer remains NULL. While no function in this file currently uses mutex locking (so there is no immediate crash), the bridge is not thread-safe and any future addition of locking would cause a NULL deref.
- **Fix**: Add `bridge_base_init(&bridge->base, BIO_MODULE_SECURITY, "security_bio_async")` after allocation, or document that the bridge is single-threaded only.

#### P3-02: Missing validation of sec_hippo_sleep_phase_t enum range
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_bridge.c`
- **Line**: 611-632
- **Description**: `security_hippocampus_protect_sleep()` has a switch on `phase` but no default case. If an invalid enum value is passed, `protection_level` stays 0.0 and `should_protect` stays false, which is a safe fallback, but there is no error logging or validation.
- **Fix**: Add a `default:` case that logs a warning and sets `should_protect = false`.

#### P3-03: Redundant duplicate NIMCP_THROW_TO_IMMUNE calls in create error paths
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_bridge.c`
- **Lines**: 411, 428
- **Description**: In `security_hippocampus_bridge_create()`, both the audit_log allocation failure path (line 411) and the internal state allocation failure path (line 428) fire a second NIMCP_THROW_TO_IMMUNE after cleanup is already done and the first throw was already fired. These are unreachable-in-effect duplicates that generate double immune alerts.
- **Fix**: Remove the second NIMCP_THROW_TO_IMMUNE at lines 411 and 428.

#### P3-04: Missing validation of fep_system pointer in compute_effects
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_fep_bridge.c`
- **Lines**: 336-338
- **Description**: `sec_hippo_fep_compute_effects()` calls `fep_get_free_energy(bridge->fep_system)`, `fep_compute_surprise()`, and `fep_get_prediction_error()` without validating that `bridge->fep_system` is still valid. If the FEP system was destroyed externally after bridge creation, these calls would dereference a dangling pointer.
- **Fix**: Add a NULL check on `bridge->fep_system` before calling FEP functions.

#### P3-05: Missing range validation on integrity_score, consolidation_rate, replay_fidelity parameters
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_fep_bridge.c`
- **Lines**: 562-569
- **Description**: `sec_hippo_fep_detect_threat()` accepts `integrity_score`, `consolidation_rate`, and `replay_fidelity` as float parameters but does not validate they are in expected ranges (e.g., 0.0-1.0 for integrity and fidelity). Out-of-range values would produce incorrect free energy calculations.
- **Fix**: Clamp or validate input parameters.

#### P3-06: Missing NULL check on replay_sequences array access
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_bridge.c`
- **Line**: 1059
- **Description**: `security_hippocampus_validate_replay()` accesses `bridge->replay_sequences[i]` without checking if `bridge->replay_sequences` is non-NULL. While it is allocated in create, if the bridge were partially initialized or corrupted, this would crash.
- **Fix**: Add a NULL check on `bridge->replay_sequences` before the loop.

#### P3-07: Missing error return value check for brain_immune_secondary_response
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c`
- **Line**: 1403
- **Description**: `sec_immune_unified_secondary_response()` calls `brain_immune_secondary_response()` but ignores the return value. The comment says "may fail if memory_id doesn't map" but the failure is silently ignored.
- **Fix**: Check return value and log a warning if it fails.

#### P3-08: Missing error return value check for brain_immune_execute_antibody
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c`
- **Line**: 1170
- **Description**: `sec_immune_unified_execute_antibody_action()` calls `brain_immune_execute_antibody()` but ignores the return value entirely.
- **Fix**: Check return value and log on failure.

#### P3-09: Missing validation that antigen_id output pointer is set in present_bbb_threat
- **File**: `src/security/immune/nimcp_security_immune_unified_bridge.c`
- **Line**: 868
- **Description**: `sec_immune_unified_present_bbb_threat()` does not validate the `antigen_id` output pointer. If NULL is passed, it would crash at line 925 when dereferencing `*antigen_id` inside the auto-inflammation block.
- **Fix**: Add `if (!antigen_id) return NIMCP_ERROR_NULL_POINTER;` guard clause.

#### P3-10: Potential divide-by-zero in consolidation latency calculation
- **File**: `src/security/hippocampus/nimcp_security_hippocampus_bridge.c`
- **Lines**: 774-777
- **Description**: The mean latency calculation divides by `bridge->stats.consolidation_checks` which was just incremented at line 724. If `consolidation_checks` was 0 before increment (first call), the divisor is 1, which is fine. However, if `consolidation_checks` ever wraps around (uint64_t, extremely unlikely), it could become 0.
- **Fix**: This is extremely low risk. Document that uint64_t wrap-around is not expected in any realistic scenario.

---

## Summary

| Priority | Count | Categories |
|----------|-------|------------|
| **P1**   | 3     | TOCTOU (3) |
| **P2**   | 16    | False positive NIMCP_THROW_TO_IMMUNE (8), wrong error codes (4), resource leak (1), subscription slot leak (1), const violation (1), duplicate throws (1) |
| **P3**   | 10    | Missing validation (7), missing mutex init (1), missing return value checks (2) |
| **Total**| **29**|            |

### P2 Breakdown by Category

| Category | Count | Findings |
|----------|-------|----------|
| False positive NIMCP_THROW_TO_IMMUNE | 8 | P2-01, P2-02, P2-04, P2-06, P2-11, P2-12, P2-13, P2-15 |
| Wrong error codes | 4 | P2-07, P2-08, P2-09, P2-10 |
| Resource/slot leaks | 2 | P2-05, P2-14 |
| Duplicate throws | 1 | P2-03 |
| Const violation | 1 | P2-16 |

### Findings by File

| File | P1 | P2 | P3 | Total |
|------|----|----|----|----|
| `nimcp_security_hippocampus_fep_bridge.c` | 1 | 0 | 2 | 3 |
| `nimcp_security_hippocampus_bridge.c` | 2 | 3 | 3 | 8 |
| `nimcp_anomaly_immune_bridge.c` | 0 | 2 | 0 | 2 |
| `nimcp_pattern_db_immune_bridge.c` | 0 | 3 | 0 | 3 |
| `nimcp_rate_limiter_immune_bridge.c` | 0 | 2 | 0 | 2 |
| `nimcp_security_immune_fep_bridge.c` | 0 | 3 | 0 | 3 |
| `nimcp_security_immune_unified_bridge.c` | 0 | 7 | 3 | 10 |
| `nimcp_security_bio_async_bridge.c` | 0 | 3 | 1 | 4 |
