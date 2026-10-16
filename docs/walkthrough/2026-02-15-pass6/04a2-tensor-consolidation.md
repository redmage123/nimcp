# Pass 6 Walkthrough: emotion_tensor + consolidation

**Date**: 2026-02-15
**Scope**: `src/cognitive/emotion_tensor/` (4 files), `src/cognitive/consolidation/` (7 files)
**Mode**: Review only (no edits)

## Summary

| Priority | Count |
|----------|-------|
| P1 (crash/race/leak) | 5 |
| P2 (wrong code/false positive/style) | 31 |
| **Total** | **36** |

## Files Reviewed

| File | Lines | Status |
|------|-------|--------|
| `emotion_tensor/nimcp_emotion_tensor.c` | 1270 | Clean |
| `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 1148 | 25 issues |
| `emotion_tensor/nimcp_emotion_tensor_thalamic_bridge.c` | 318 | 2 issues |
| `emotion_tensor/nimcp_emotion_tensor_substrate_bridge.c` | 422 | 1 issue |
| `consolidation/nimcp_consolidation.c` | 1637 | 4 issues |
| `consolidation/nimcp_consolidation_fep_bridge.c` | 333 | Clean |
| `consolidation/nimcp_consolidation_plasticity_bridge.c` | 1176 | 1 issue |
| `consolidation/nimcp_consolidation_snn_bridge.c` | 1149 | 2 issues |
| `consolidation/nimcp_consolidation_substrate_bridge.c` | 427 | 1 issue |
| `consolidation/nimcp_consolidation_thalamic_bridge.c` | 322 | 2 issues (same as emotion_tensor thalamic) |
| `consolidation/nimcp_emotion_consolidation.c` | 695 | 1 issue |

## Findings

### P1: Crashes, Races, Leaks

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | `emotion_tensor/nimcp_emotion_tensor_substrate_bridge.c` | 138-143 | P1 Leak | On `nimcp_platform_mutex_init` failure, `nimcp_free(bridge)` is called but `bridge->base.mutex` (separately allocated on line 130 via `nimcp_malloc`) is NOT freed first. Leaks the mutex allocation. |
| 2 | `consolidation/nimcp_consolidation_substrate_bridge.c` | 138-143 | P1 Leak | Same pattern as #1. On mutex init failure, `nimcp_free(bridge)` leaks the separately allocated `bridge->base.mutex`. |
| 3 | `consolidation/nimcp_consolidation.c` | 807 | P1 False positive throw | `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_thread_fn: operation failed")` at end of normal thread exit path. Every thread shutdown fires an immune alert. Should be removed or gated. |
| 4 | `consolidation/nimcp_consolidation.c` | 665 | P1 Wrong return value | `consolidate_pruning()` returns `bool` but line 665 has `return 0.0F;` (float literal). Should be `return false;`. Compiles but semantically wrong and may confuse static analysis. |
| 5 | `consolidation/nimcp_consolidation.c` | 1305 | P1 Race | `consolidation_reset_global_state()` resets `g_sync_stats_init = NIMCP_ONCE_INIT` without holding any lock. A concurrent call to `ensure_sync_stats_init()` could race on the once-flag, leading to double-init or use of partially-initialized mutex. |

### P2: Wrong Error Codes

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 6 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 438 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 7 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 507 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 8 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 524 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 9 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 555 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 10 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 556 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 11 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 606 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 12 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 664 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 13 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 707 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 14 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 708 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 15 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 761 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 16 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 784 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 17 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 833 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 18 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 874 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 19 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 903 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 20 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 938 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 21 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 998 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 22 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 1056 | P2 Wrong error code | Returns `NIMCP_INVALID_PARAM` - should be `NIMCP_ERROR_INVALID_PARAM` |
| 23 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 469 | P2 Wrong error code | Returns `NIMCP_NOT_INITIALIZED` - should be `NIMCP_ERROR_NOT_INITIALIZED` |
| 24 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 607 | P2 Wrong error code | Returns `NIMCP_NOT_INITIALIZED` - should be `NIMCP_ERROR_NOT_INITIALIZED` |
| 25 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 834 | P2 Wrong error code | Returns `NIMCP_NOT_INITIALIZED` - should be `NIMCP_ERROR_NOT_INITIALIZED` |
| 26 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 877 | P2 Wrong error code | Returns `NIMCP_NOT_INITIALIZED` - should be `NIMCP_ERROR_NOT_INITIALIZED` |
| 27 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 906 | P2 Wrong error code | Returns `NIMCP_NOT_INITIALIZED` - should be `NIMCP_ERROR_NOT_INITIALIZED` |
| 28 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 939 | P2 Wrong error code | Returns `NIMCP_NOT_INITIALIZED` - should be `NIMCP_ERROR_NOT_INITIALIZED` |
| 29 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 999 | P2 Wrong error code | Returns `NIMCP_NOT_INITIALIZED` - should be `NIMCP_ERROR_NOT_INITIALIZED` |
| 30 | `consolidation/nimcp_consolidation.c` | 861 | P2 Wrong error code | Uses `NIMCP_ERROR_NULL_ARG` which is nonstandard. Should be `NIMCP_ERROR_NULL_POINTER`. |
| 31 | `consolidation/nimcp_consolidation.c` | 915 | P2 Wrong error code | `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...handle is NULL")` for allocation failure. Should be `NIMCP_ERROR_NO_MEMORY` with appropriate message. |
| 32 | `consolidation/nimcp_consolidation_snn_bridge.c` | 225 | P2 Wrong error code | `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...invalid dimensions")` for invalid config dimensions. Should be `NIMCP_ERROR_INVALID_PARAM` or `NIMCP_ERROR_OUT_OF_RANGE`. |

### P2: False Positive Throws, Style Issues

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 33 | `emotion_tensor/nimcp_emotion_tensor_bridge.c` | 1034-1037 | P2 False positive throw | `emotion_tensor_bridge_needs_sync()` throws `NIMCP_THROW_TO_IMMUNE` for NULL bridge. This is a query/predicate function - NULL check should return `false`, not fire immune alert. |
| 34 | `consolidation/nimcp_consolidation_plasticity_bridge.c` | 1082 | P2 Wrong code + message | Throws `NIMCP_ERROR_NULL_POINTER` with message "bridge->config is NULL" when `bridge->config.enable_bio_async` is false. Not a null pointer - bio_async is disabled. Should be `NIMCP_ERROR_INVALID_STATE` with message "bio_async not enabled". |
| 35 | `consolidation/nimcp_consolidation_snn_bridge.c` | 1055 | P2 Wrong code + message | Same pattern as #34. Throws `NIMCP_ERROR_NULL_POINTER` / "bridge->config is NULL" when bio_async is not enabled. Should be `NIMCP_ERROR_INVALID_STATE`. |
| 36 | `consolidation/nimcp_emotion_consolidation.c` | 293 | P2 Raw pthread | Uses `pthread_rwlock_init(&system->lock, NULL)` instead of `nimcp_rwlock_init()`. Inconsistent with `emotion_tensor.c` which uses the nimcp wrapper. Uses raw `pthread_rwlock_*` throughout the file. |

### P2: Const-Correctness (Informational)

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| - | `emotion_tensor/nimcp_emotion_tensor_thalamic_bridge.c` | 214, 219 | P2 Const-cast | `nimcp_mutex_lock(bridge->base.mutex)` called on `const` bridge parameter. Implicitly casts away const. Common pattern but technically UB if optimizer assumes const. |
| - | `consolidation/nimcp_consolidation_thalamic_bridge.c` | 219, 234 | P2 Const-cast | Same pattern as above. `nimcp_mutex_lock` on const bridge parameter. |

---

## Systemic Patterns

### 1. `NIMCP_INVALID_PARAM` / `NIMCP_NOT_INITIALIZED` (24 instances in emotion_tensor_bridge.c)
The entire `nimcp_emotion_tensor_bridge.c` file uses unprefixed error code names (`NIMCP_INVALID_PARAM`, `NIMCP_NOT_INITIALIZED`) instead of the standard `NIMCP_ERROR_*` prefix. This is a bulk find-and-replace fix:
- `NIMCP_INVALID_PARAM` -> `NIMCP_ERROR_INVALID_PARAM` (17 instances)
- `NIMCP_NOT_INITIALIZED` -> `NIMCP_ERROR_NOT_INITIALIZED` (7 instances)

### 2. Mutex Leak in Substrate Bridges (2 instances)
Both `emotion_tensor_substrate_bridge.c` and `consolidation_substrate_bridge.c` have the same leak pattern in their `_create()` functions. When `nimcp_platform_mutex_init()` fails after `nimcp_malloc()` succeeds for the mutex, the mutex memory is leaked. Fix: add `nimcp_free(bridge->base.mutex)` before `nimcp_free(bridge)`.

### 3. Bio-Async "Not Enabled" Misreported as NULL Pointer (2 instances)
Both `consolidation_plasticity_bridge.c` and `consolidation_snn_bridge.c` throw `NIMCP_ERROR_NULL_POINTER` with message "bridge->config is NULL" when `enable_bio_async` is simply `false`. The pointer is not null; the feature is disabled. Should use `NIMCP_ERROR_INVALID_STATE`.
