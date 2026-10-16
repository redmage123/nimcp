# Pass 6 Review: Mesh Coordinator Files

**Files reviewed:**
- `src/mesh/nimcp_mesh_coordinator.c` (788 lines)
- `src/mesh/nimcp_mesh_coordinator_pool.c` (1395 lines)
- `src/mesh/nimcp_mesh_msp.c` (1487 lines)
- `src/mesh/nimcp_mesh_endorsement.c` (801 lines)

**Summary:** 6 P1, 18 P2

---

## Issues

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | nimcp_mesh_msp.c | 775-802 | P1: data race | `mesh_msp_get_channel_memberships` traverses credential linked list WITHOUT mutex protection. Other functions (issue/revoke/quarantine) modify this list under mutex. Concurrent access causes UB/crash. |
| 2 | nimcp_mesh_msp.c | 1384-1415 | P1: data race | `mesh_msp_update` traverses credential linked list and MODIFIES entries (state, quarantined flags) without mutex. Other threads concurrently modify credentials under mutex through issue/revoke/suspend functions. |
| 3 | nimcp_mesh_msp.c | 1014-1052 | P1: data race | `mesh_msp_add_policy` modifies policy linked list without mutex. All policy functions (add/remove/evaluate/set_callback) lack mutex protection entirely. Concurrent policy operations corrupt linked list. |
| 4 | nimcp_mesh_msp.c | 1054-1083 | P1: data race | `mesh_msp_remove_policy` modifies policy linked list without mutex (same root cause as #3). |
| 5 | nimcp_mesh_endorsement.c | 596-604 | P1: use-after-free | `mesh_endorsement_get_collected` returns pointer to internal `endorsement_set_t` after releasing mutex. Caller reads data that `cancel_collection` or `mesh_endorsement_add` can concurrently modify or free (the `received.endorsements` array is freed in cancel). |
| 6 | nimcp_mesh_endorsement.c | 617-625 | P1: use-after-free | `mesh_endorsement_get_selected` returns pointer to internal `endorser_set_t` after releasing mutex. Same TOCTOU: caller holds stale pointer, `cancel_collection` can swap-remove the collection and free resources. |
| 7 | nimcp_mesh_coordinator.c | 423 | P2: wrong error code | `mesh_coordinator_assign_participant` uses `NIMCP_ERROR_NO_MEMORY` when participant array is full. Should be `NIMCP_ERROR_CAPACITY_EXCEEDED` -- no allocation failed, capacity limit reached. |
| 8 | nimcp_mesh_coordinator.c | 423 | P2: false positive throw | Same line: reaching participant capacity is a normal operational condition, not an immune-system event. Remove `NIMCP_THROW_TO_IMMUNE`. |
| 9 | nimcp_mesh_coordinator.c | 627 | P2: false positive throw | `mesh_coordinator_heartbeat_timed_out` throws `NIMCP_THROW_TO_IMMUNE` on invalid coordinator handle. Parameter validation failure is not an immune event. |
| 10 | nimcp_mesh_coordinator.c | 488-489 | P2: unprotected read | `mesh_coordinator_get_participant_count` reads `participant_count` without mutex while other functions modify it under lock. Technically a data race (UB), though single-word reads are practically safe on x86. |
| 11 | nimcp_mesh_coordinator.c | 747 | P2: unprotected read | `mesh_coordinator_get_stats` reads stats via `memcpy` without mutex while `update`, `report_failure`, `report_recovery` modify stats under lock. |
| 12 | nimcp_mesh_coordinator_pool.c | 592 | P2: wrong error code | `mesh_coordinator_pool_add` uses `NIMCP_ERROR_NO_MEMORY` for pool capacity exceeded. Should be `NIMCP_ERROR_CAPACITY_EXCEEDED`. |
| 13 | nimcp_mesh_coordinator_pool.c | 592 | P2: false positive throw | Same line: coordinator pool at capacity is a normal condition, not an immune event. |
| 14 | nimcp_mesh_coordinator_pool.c | 663 | P2: wrong throw message | `mesh_coordinator_pool_get` throws with message "validate_pool is NULL" -- the actual error is that the pool handle failed validation (bad magic or NULL). Misleading for diagnostics. |
| 15 | nimcp_mesh_coordinator_pool.c | 676 | P2: wrong throw message | `mesh_coordinator_pool_get_by_index` throws with message "validate_pool is NULL" -- should describe the actual condition (invalid pool or index out of range). |
| 16 | nimcp_mesh_coordinator_pool.c | 694 | P2: wrong throw message | `mesh_coordinator_pool_get_leader` throws with message "validate_pool is NULL". |
| 17 | nimcp_mesh_coordinator_pool.c | 698 | P2: wrong throw message | Same function, second throw says "capacity exceeded" but actual condition is `leader_index >= coordinator_count` (stale leader index). |
| 18 | nimcp_mesh_coordinator_pool.c | 1050 | P2: wrong throw message | `mesh_coordinator_pool_get_assignment` throws with message "validate_pool is NULL". |
| 19 | nimcp_mesh_endorsement.c | 81 | P2: wrong error code | `allocate_collection` throws `NIMCP_ERROR_NO_MEMORY` with message "collector is NULL". Should be `NIMCP_ERROR_NULL_POINTER` when collector is NULL. |
| 20 | nimcp_mesh_endorsement.c | 549-551 | P2: false positive throw | `mesh_endorsement_quorum_met` throws `NIMCP_THROW_TO_IMMUNE` on NULL parameters. NULL check returning false is normal validation, not an immune event. Inconsistent with `mesh_endorsement_is_complete` (line 529) which does NOT throw on NULL. |
| 21 | nimcp_mesh_msp.c | 1437-1452 | P2: unprotected read | `mesh_msp_reset_stats` zeroes stats and re-counts credentials without mutex. Concurrent credential operations will produce inconsistent counts. |
| 22 | nimcp_mesh_msp.c | 1085-1141 | P2: unprotected read | `mesh_msp_evaluate_policy` traverses policy list and calls `mesh_msp_get_credential`/`mesh_msp_has_channel_membership` without mutex. Policy list and credential list could be concurrently modified. |
| 23 | nimcp_mesh_msp.c | 609-610 | P2: returning internal pointer | `mesh_msp_get_credential` returns `const credential_t*` pointer to internal data after releasing mutex. While the entry isn't freed during normal operation (only in `mesh_msp_destroy`), the credential fields can be concurrently modified by other mutex-protected functions, making the returned data potentially stale. |
| 24 | nimcp_mesh_endorsement.c | 782-784 | P2: unprotected access | `mesh_endorsement_print_collection` calls `find_collection` without mutex. Diagnostic function, but could read inconsistent data or crash if collection is concurrently cancelled. |

---

## Notes

### MSP Mutex Coverage Gap (Issues #1-#4, #21-#22)
The MSP file has good mutex coverage for credential CRUD operations (issue, revoke, suspend, restore, quarantine, authenticate) but completely lacks mutex protection for:
- Policy operations (add/remove/evaluate/set_callback) -- all 4 functions unprotected
- `mesh_msp_get_channel_memberships` -- traverses credential list without lock
- `mesh_msp_update` -- modifies credential state without lock
- `mesh_msp_reset_stats` -- modifies stats and traverses list without lock

Fix pattern: Add `nimcp_mutex_lock(msp->mutex)` / `nimcp_mutex_unlock(msp->mutex)` wrappers. For `mesh_msp_update`, use the mutex but be careful not to call other public functions that also lock (use `_unlocked` helpers or restructure).

### Endorsement Internal Pointer Returns (Issues #5-#6)
`mesh_endorsement_get_collected` and `mesh_endorsement_get_selected` return pointers to internal collection data after releasing the mutex. This is architecturally problematic. Options:
- Copy the data into caller-provided buffer (safest)
- Document that the pointer is only valid while no other operations occur (fragile)
- Hold a read-lock that the caller must release (complex)

### Coordinator Pool Throw Messages (Issues #14-#18)
Multiple functions have throw messages that say "validate_pool is NULL" when the actual condition is that `validate_pool()` returned false (which means either the pool pointer is NULL OR the magic number doesn't match). The message should say "invalid pool handle" or similar.
