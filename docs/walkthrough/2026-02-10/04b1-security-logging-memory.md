# Security Logging & Memory Bridge Walkthrough

**Date**: 2026-02-10
**Scope**: `src/security/logging/*.c`, `src/security/memory/*.c`
**Files Reviewed**: 4

| File | Lines |
|------|-------|
| `src/security/logging/nimcp_security_logging_fep_bridge.c` | 2075 |
| `src/security/logging/nimcp_security_logging_bridge.c` | 2235 |
| `src/security/memory/nimcp_security_memory_bridge.c` | 2056 |
| `src/security/memory/nimcp_security_memory_fep_bridge.c` | 1904 |

---

## P1 Findings (Critical)

### P1-1: Shared history_head/history_count causes misaligned FE and surprise history buffers
**File**: `src/security/logging/nimcp_security_logging_fep_bridge.c`
**Lines**: 911-914
**Description**: `sec_log_fep_compute_effects` calls `record_history` twice using the same `&bridge->history_head` and `&bridge->history_count` pointers -- once for `fe_history` and once for `surprise_history`. Each call advances `history_head` by 1. This means `fe_history` writes to even indices (0, 2, 4, ...) and `surprise_history` writes to odd indices (1, 3, 5, ...). The two history buffers are permanently misaligned, and each only contains half the expected number of entries. The effective history capacity is halved.
```c
record_history(bridge->fe_history, &bridge->history_head, &bridge->history_count,
               SEC_LOG_FEP_HISTORY_SIZE, current_fe);
record_history(bridge->surprise_history, &bridge->history_head, &bridge->history_count,
               SEC_LOG_FEP_HISTORY_SIZE, surprise);
```
**Fix**: Add separate `fe_history_head`/`fe_history_count` and `surprise_history_head`/`surprise_history_count` fields, or use a single index and write both values per update cycle without incrementing in between.

### P1-2: Null byte detection is dead code when strlen is used for message_len
**File**: `src/security/logging/nimcp_security_logging_fep_bridge.c`
**Lines**: 1072-1073
**Description**: `sec_log_fep_analyze_entry` passes `strlen(entry->message)` as `message_len` to `check_injection_patterns`, which then calls `contains_null_byte(message, message_len)`. Since `strlen` stops at the first `\0`, `message_len` is the length up to the first null byte. The `contains_null_byte` function then scans bytes `[0, message_len)` which, by definition, contain no null bytes. Embedded null byte injection can never be detected through this path.
```c
inject = check_injection_patterns(bridge, entry->message, strlen(entry->message),
                                   result->pattern_matched, sizeof(result->pattern_matched));
```
**Fix**: The caller should pass a declared/known buffer size (e.g., `SECURITY_LOG_MAX_MESSAGE_LEN`) instead of `strlen(entry->message)` so that `contains_null_byte` can scan past the first embedded null to detect truncation attacks.

### P1-3: TOCTOU in security_memory_check_access unlock/relock around audit
**File**: `src/security/memory/nimcp_security_memory_bridge.c`
**Lines**: 856-861
**Description**: The function drops `BRIDGE_LOCK`, calls `security_memory_audit_access` (which takes its own lock), then re-acquires `BRIDGE_LOCK` before updating latency stats. Between the unlock and re-lock, another thread can modify `bridge->stats` or `bridge->state`, leading to inconsistent latency calculations on lines 865-873.
```c
if (bridge->config.enable_audit && ...) {
    BRIDGE_UNLOCK(bridge);
    security_memory_audit_access(bridge, ...);
    BRIDGE_LOCK(bridge);
}
/* stats updated after re-lock may be inconsistent */
```
**Fix**: Create an `_unlocked` variant of `security_memory_audit_access` that assumes the lock is held, or defer the audit operation to after the function completes its locked section.

---

## P2 Findings

### P2-1: False positive NIMCP_THROW_TO_IMMUNE -- needle_len > hay_len is normal
**File**: `src/security/logging/nimcp_security_logging_fep_bridge.c`
**Lines**: 249-252
**Description**: `contains_pattern_icase` fires `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, ...)` when the needle is longer than the haystack. This is a normal "pattern not found" condition, not an error. On a busy system scanning many patterns, this fires frequently.
```c
if (needle_len > hay_len) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "contains_pattern_icase: validation failed");
    return false;
}
```
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE; just `return false`.

### P2-2: False positive NIMCP_THROW_TO_IMMUNE -- rate limiting is normal
**File**: `src/security/logging/nimcp_security_logging_fep_bridge.c`
**Lines**: 1688-1690
**Description**: `sec_log_fep_execute_protection` fires NIMCP_THROW_TO_IMMUNE when the rate limiter rejects a request. Rate limiting is expected behavior, not an error.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sec_log_fep_execute_protection: validation failed");
return -1;  /* Rate limited */
```
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE; keep only `return -1`.

### P2-3: False positive NIMCP_THROW_TO_IMMUNE -- inactive bridge is not a NULL pointer
**File**: `src/security/logging/nimcp_security_logging_fep_bridge.c`
**Lines**: 898-900
**Description**: `sec_log_fep_compute_effects` checks `!bridge->state.active` but throws with error code `NIMCP_ERROR_NULL_POINTER` and message "bridge->state is NULL". The bridge being inactive is an operational state, not a null pointer. The error code and message are both wrong.
```c
if (!bridge->state.active) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_log_fep_compute_effects: bridge->state is NULL");
    return -1;
}
```
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE entirely and just `return -1` (or 0 since inactive is normal). If keeping, change to `NIMCP_ERROR_INVALID_STATE` with message "bridge is not active".

### P2-4: Wrong error code -- NIMCP_ERROR_NO_MEMORY for NULL parameter
**File**: `src/security/logging/nimcp_security_logging_fep_bridge.c`
**Line**: 667
**Description**: `sec_log_fep_create` throws `NIMCP_ERROR_NO_MEMORY` when `log_bridge` or `fep_system` is NULL. These are null pointer parameters, not out-of-memory conditions.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sec_log_fep_create: required parameter is NULL (log_bridge, fep_system)");
```
**Fix**: Change to `NIMCP_ERROR_NULL_POINTER`.

### P2-5: False positive NIMCP_THROW_TO_IMMUNE -- buffer full is normal
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Lines**: 254-258
**Description**: `ring_buffer_push` fires `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "overwrite_on_full is NULL")` when the buffer is full and `overwrite_on_full` is false. A bool parameter is not a pointer. The error code is wrong (should not be NULL_POINTER) and the message is wrong (overwrite_on_full is false, not NULL). This is a normal full-buffer condition.
```c
if (!overwrite_on_full) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "overwrite_on_full is NULL");
    return NIMCP_ERROR_OPERATION_FAILED;
}
```
**Fix**: Remove NIMCP_THROW_TO_IMMUNE. Just `return NIMCP_ERROR_OPERATION_FAILED`.

### P2-6: False positive NIMCP_THROW_TO_IMMUNE -- index out of range is normal
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Lines**: 276-278
**Description**: `ring_buffer_get` fires NIMCP_THROW_TO_IMMUNE when `index >= rb->count`. Index out of range is a normal "not found" condition for buffer queries. The error message also claims "rb, rb->entries" are NULL even when the real issue is an out-of-range index.
```c
if (!rb || !rb->entries || index >= rb->count) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ring_buffer_get: required parameter is NULL (rb, rb->entries)");
    return NULL;
}
```
**Fix**: Split into separate checks: throw for actual NULL pointers, return NULL without throw for out-of-range index.

### P2-7: Wrong error code -- NIMCP_ERROR_NULL_POINTER for failed fopen
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Line**: 1857
**Description**: `security_logging_export_to_file` returns `NIMCP_ERROR_NULL_POINTER` when `fopen` fails. A file open failure is not a null pointer error.
```c
return NIMCP_ERROR_NULL_POINTER;
```
**Fix**: Change to `NIMCP_ERROR_OPERATION_FAILED` or a file-specific error code.

### P2-8: False positive NIMCP_THROW_TO_IMMUNE -- stream slots full is normal
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Lines**: 1373-1374
**Description**: `security_logging_register_stream` fires NIMCP_THROW_TO_IMMUNE when no free callback slots are available. This is a normal resource exhaustion condition.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "security_logging_register_stream: operation failed");
return -1;
```
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE. Just `return -1`.

### P2-9: False positive NIMCP_THROW_TO_IMMUNE -- is_connected NULL check is a getter
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Lines**: 829-831
**Description**: `security_logging_is_connected` is a simple boolean getter. Firing NIMCP_THROW_TO_IMMUNE for NULL bridge on a read-only getter is excessive. The function already returns false for NULL.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_is_connected: bridge is NULL");
return false;
```
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE; just `return false`.

### P2-10: False positive NIMCP_THROW_TO_IMMUNE -- bio_async not enabled is not a NULL pointer
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Lines**: 2214-2216
**Description**: `security_logging_broadcast_event` fires `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge->base is NULL")` when `!bridge->base.bio_async_enabled`. The bio_async subsystem not being enabled is a valid operational state, not a null pointer. Wrong error code and message.
```c
if (!bridge->base.bio_async_enabled) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_broadcast_event: bridge->base is NULL");
    return -1;
}
```
**Fix**: Remove NIMCP_THROW_TO_IMMUNE. Return `NIMCP_ERROR_INVALID_STATE` or just -1.

### P2-11: False positive NIMCP_THROW_TO_IMMUNE -- subject not found is normal search
**File**: `src/security/memory/nimcp_security_memory_bridge.c`
**Lines**: 137-138
**Description**: `find_subject_index` fires NIMCP_THROW_TO_IMMUNE when a subject is not found in the access table. Not-found is a normal search result. This function is called in hot paths (every access check). The immune system will be flooded with normal lookup misses.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_subject_index: validation failed");
return -1;
```
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE; just `return -1`.

### P2-12: False positive NIMCP_THROW_TO_IMMUNE -- unknown operation is normal default
**File**: `src/security/memory/nimcp_security_memory_bridge.c`
**Lines**: 173-175
**Description**: `check_operation_permission` fires NIMCP_THROW_TO_IMMUNE in the default case of the switch. An unrecognized operation type should just be denied, not reported to the immune system.
```c
default:
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_operation_permission: operation failed");
    return false;
```
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE; just `return false`.

### P2-13: False positive NIMCP_THROW_TO_IMMUNE -- decryption auth failure is normal
**File**: `src/security/memory/nimcp_security_memory_bridge.c`
**Lines**: 350-352
**Description**: `stub_decrypt` fires NIMCP_THROW_TO_IMMUNE when tag verification fails. Decryption authentication failure is a normal outcome when processing potentially tampered data -- it is the expected result for corrupted or invalid ciphertext.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stub_decrypt: validation failed");
return false;
```
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE; just `return false`.

### P2-14: Resource leak -- bridge_base_cleanup not called on mutex NULL check
**File**: `src/security/memory/nimcp_security_memory_fep_bridge.c`
**Lines**: 227-231
**Description**: In `sec_mem_fep_create`, after `bridge_base_init` succeeds (line 226), if `bridge->base.mutex` is NULL (line 227), the code calls `nimcp_free(bridge)` without first calling `bridge_base_cleanup`. Any resources allocated by `bridge_base_init` (other than the mutex) are leaked.
```c
if (bridge_base_init(&bridge->base, 0, "security_memory_fep") != 0) { nimcp_free(bridge); return NULL; }
if (!bridge->base.mutex) {
    nimcp_free(bridge);        // missing bridge_base_cleanup
    return NULL;
}
```
**Fix**: Call `bridge_base_cleanup(&bridge->base)` before `nimcp_free(bridge)` in the mutex NULL path.

### P2-15: False positive NIMCP_THROW_TO_IMMUNE -- inactive bridge is not NULL
**File**: `src/security/memory/nimcp_security_memory_fep_bridge.c`
**Lines**: 414-416
**Description**: Same issue as P2-3. `sec_mem_fep_compute_effects` checks `!bridge->state.active` but throws `NIMCP_ERROR_NULL_POINTER` with message "bridge->state is NULL". The bridge being inactive is normal, not a null pointer.
```c
if (!bridge->state.active) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sec_mem_fep_compute_effects: bridge->state is NULL");
    return -1;
}
```
**Fix**: Remove or change to `NIMCP_ERROR_INVALID_STATE` with correct message.

### P2-16: Wrong metric assigned -- avg_free_energy overwritten with avg_surprise
**File**: `src/security/memory/nimcp_security_memory_fep_bridge.c`
**Line**: 465
**Description**: In `sec_mem_fep_compute_effects`, `bridge->stats.avg_free_energy` is assigned from `bridge->state.avg_surprise` instead of from a free energy value. The correctly computed `bridge->stats.avg_free_energy` (updated in `update_running_averages` on line 1872) is immediately overwritten with the wrong metric.
```c
bridge->stats.avg_free_energy = bridge->state.avg_surprise;  // BUG: should not overwrite
```
**Fix**: Remove this line. The `update_running_averages` function already correctly maintains `bridge->stats.avg_free_energy`.

### P2-17: False positive NIMCP_THROW_TO_IMMUNE -- is_bio_async_connected NULL is a getter
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Lines**: 2186-2188
**Description**: `security_logging_is_bio_async_connected` fires NIMCP_THROW_TO_IMMUNE for a NULL bridge on a read-only boolean getter.
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_logging_is_bio_async_connected: bridge is NULL");
return false;
```
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE; just `return false`.

---

## P3 Findings

### P3-1: Wrong function name in error message
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Line**: 248
**Description**: `ring_buffer_push` error message says "ring_buffer_cleanup" instead of "ring_buffer_push".
```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ring_buffer_cleanup: required parameter is NULL (rb, entry, rb->entries)");
```
**Fix**: Change "ring_buffer_cleanup" to "ring_buffer_push".

### P3-2: Missing JSON escaping in entry_to_json
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Lines**: 1925-1952
**Description**: `security_logging_entry_to_json` interpolates `entry->message`, `entry->source_module`, and `entry->source_id` directly into JSON using `%s` format. If these fields contain quotes, backslashes, or control characters, the output is malformed JSON. In a security logging context, injection attacks frequently contain these characters, causing the very logs recording the attacks to be unreadable.
**Fix**: Add a JSON string escaping function and use it for all string fields.

### P3-3: Missing fep_system NULL validation in compute_effects
**File**: `src/security/logging/nimcp_security_logging_fep_bridge.c`
**Lines**: 906-908
**Description**: `sec_log_fep_compute_effects` calls `fep_get_free_energy(bridge->fep_system)`, `fep_compute_surprise(bridge->fep_system)`, and `fep_get_prediction_error(bridge->fep_system, 0)` without verifying `bridge->fep_system` is non-NULL. While the create function validates this, external modification could cause NULL dereference.
**Fix**: Add `if (!bridge->fep_system) { BRIDGE_UNLOCK(bridge); return -1; }` before FEP calls.

### P3-4: Missing fep_system NULL validation in analyze_entry
**File**: `src/security/logging/nimcp_security_logging_fep_bridge.c`
**Lines**: 1057-1059
**Description**: Same as P3-3 but in `sec_log_fep_analyze_entry`. Calls FEP functions without checking `bridge->fep_system`.
**Fix**: Add NULL check for `bridge->fep_system` after acquiring lock.

### P3-5: Missing fep_system NULL validation in sec_mem_fep_compute_effects
**File**: `src/security/memory/nimcp_security_memory_fep_bridge.c`
**Lines**: 422-424
**Description**: Same pattern. `sec_mem_fep_compute_effects` calls `fep_get_free_energy(bridge->fep_system)` without validating the pointer.
**Fix**: Add NULL check for `bridge->fep_system`.

### P3-6: Missing internal state NULL check in security_memory_check_access
**File**: `src/security/memory/nimcp_security_memory_bridge.c`
**Line**: 770
**Description**: `security_memory_check_access` retrieves `internal` from `bridge->base.system_b` without a NULL check, then passes it to `is_subject_locked_out`. If `system_b` was not set (bridge not fully initialized), this dereferences NULL.
**Fix**: Add `if (!internal) { BRIDGE_UNLOCK(bridge); return false; }` after line 770.

### P3-7: Missing validation of is_bio_async_connected NULL bridge
**File**: `src/security/memory/nimcp_security_memory_bridge.c`
**Lines**: 1913-1916
**Description**: `security_memory_is_bio_async_connected` fires NIMCP_THROW_TO_IMMUNE on NULL bridge, which is excessive for a boolean getter. This should just return false silently.
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE; just `return false`.

### P3-8: Missing validation of is_fully_connected NULL bridge
**File**: `src/security/memory/nimcp_security_memory_bridge.c`
**Lines**: 736-738
**Description**: `security_memory_is_fully_connected` fires NIMCP_THROW_TO_IMMUNE for NULL bridge. Boolean getters should return false silently for NULL.
**Fix**: Remove the NIMCP_THROW_TO_IMMUNE; just `return false`.

### P3-9: localtime is not thread-safe
**File**: `src/security/logging/nimcp_security_logging_bridge.c`
**Line**: 1977
**Description**: `security_logging_rotate` calls `localtime(&now)` which returns a pointer to a static buffer shared across threads. In a multi-threaded security logging context, this can produce corrupted timestamps in rotated log filenames.
**Fix**: Use `localtime_r(&now, &tm_info)` (POSIX) for thread safety.

---

## Summary

| Priority | Count | Categories |
|----------|-------|------------|
| **P1** | 3 | 1 data corruption (shared history indices), 1 dead code security bypass (null byte detection), 1 TOCTOU |
| **P2** | 17 | 12 false positive NIMCP_THROW_TO_IMMUNE, 3 wrong error codes/messages, 1 resource leak, 1 wrong metric assignment |
| **P3** | 9 | 5 missing validation, 1 wrong function name in error message, 1 missing JSON escaping, 1 thread-unsafe localtime, 1 excessive throw on getter |
| **Total** | **29** | |

### False Positive NIMCP_THROW_TO_IMMUNE Breakdown (12 instances)

| Pattern | Files | Lines |
|---------|-------|-------|
| Search/lookup not found | memory_bridge | 137 |
| Validation rejection (bool false, not NULL) | logging_bridge | 254 |
| Index out of range | logging_bridge | 276 |
| Rate limiting normal condition | logging_fep | 1688 |
| Inactive bridge treated as NULL | logging_fep:898, memory_fep:414 |
| Boolean getter NULL check | logging_bridge:829, 2186 |
| Resource limit full | logging_bridge:1373 |
| Bio-async not enabled | logging_bridge:2214 |
| Unknown operation default | memory_bridge:173 |
| Decryption auth failure | memory_bridge:350 |
