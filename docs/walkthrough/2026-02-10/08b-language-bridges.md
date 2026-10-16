# Walkthrough: Language Bridges Code Review

**Date**: 2026-02-10
**Scope**: All 17 `.c` files in `src/language/bridges/`
**Reviewer**: Claude (automated static analysis)
**Total Lines**: ~11,900

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| **P1**   | 6     | NULL deref, out-of-bounds, resource leak, logic bug, dead code |
| **P2**   | 13    | Wrong error codes, false positive THROW, resource leaks |
| **P3**   | 5     | Missing validation, inconsistent error handling |
| **Total** | **24** | |

---

## P1 Findings

### P1-1: NULL dereference in `dequeue_command(bridge, NULL)` call

**File**: `src/language/bridges/nimcp_language_motor_bridge.c`
**Line**: 251

```c
if (!pending->is_valid) {
    dequeue_command(bridge, NULL);  // <-- passes NULL
    continue;
}
```

The `dequeue_command` function (line 157) dereferences the second parameter unconditionally:
```c
*cmd = bridge->command_queue[bridge->queue_head].command;
```

Passing NULL causes a crash.

**Fix**: Add a NULL check in `dequeue_command`, or pass a throwaway local variable:
```c
// Option A: guard in dequeue_command
if (cmd) {
    *cmd = bridge->command_queue[bridge->queue_head].command;
}

// Option B: at call site
articulator_command_t discard;
dequeue_command(bridge, &discard);
```

---

### P1-2: Out-of-bounds array access in `language_thalamic_bridge_send_signal`

**File**: `src/language/bridges/nimcp_language_thalamic_bridge.c`
**Line**: 262

```c
float gating = bridge->gating_state[signal->target];
```

No bounds check on `signal->target` before the array access. If `signal->target >= LANG_THAL_NUCLEUS_COUNT`, this reads out of bounds. Note that `language_thalamic_bridge_gate_nucleus` (line 339) correctly checks `if (nucleus >= LANG_THAL_NUCLEUS_COUNT) return -1;` but `send_signal` does not.

**Fix**: Add bounds check before array access:
```c
if (signal->target >= LANG_THAL_NUCLEUS_COUNT) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
        "language_thalamic_bridge_send_signal: signal->target out of bounds");
    return -1;
}
float gating = bridge->gating_state[signal->target];
```

---

### P1-3: Resource leak on partial allocation failure in `language_hippocampus_retrieve`

**File**: `src/language/bridges/nimcp_language_hippocampus_bridge.c`
**Lines**: 440-448

```c
result->memories = nimcp_calloc(request->max_results, sizeof(word_memory_t));
result->similarities = nimcp_calloc(request->max_results, sizeof(float));
result->count = 0;
result->status = MEM_OP_NOT_FOUND;

if (!result->memories || !result->similarities) {
    bridge->state = LH_STATE_IDLE;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...");
    return -1;
}
```

If `result->memories` succeeds but `result->similarities` fails, the combined `||` check catches the failure but does not free `result->memories`. The caller receives `-1` with a dangling allocation.

**Fix**: Free whichever allocation succeeded before returning:
```c
if (!result->memories || !result->similarities) {
    nimcp_free(result->memories);   // safe if NULL
    nimcp_free(result->similarities); // safe if NULL
    result->memories = NULL;
    result->similarities = NULL;
    bridge->state = LH_STATE_IDLE;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "...");
    return -1;
}
```

---

### P1-4: Logic bug - `time_inflamed_ms` always adds zero

**File**: `src/language/bridges/nimcp_language_immune_bridge.c`
**Lines**: 766, 805

```c
// Line 766 - sets last_update_time_ms FIRST
bridge->stats.last_update_time_ms = current_time_ms;

// ... lines 769-801 (other processing) ...

// Line 805 - reads it back: delta is always 0
bridge->stats.time_inflamed_ms += (current_time_ms - bridge->stats.last_update_time_ms);
```

`last_update_time_ms` was set to `current_time_ms` on line 766, so the delta on line 805 is always `(current_time_ms - current_time_ms) = 0`. The statistic never accumulates.

**Fix**: Compute the delta before overwriting `last_update_time_ms`:
```c
uint64_t prev_update = bridge->stats.last_update_time_ms;
bridge->stats.last_update_time_ms = current_time_ms;
// ... (other processing) ...
if (bridge->cytokines.composite_inflammatory > LANGUAGE_IMMUNE_MILD_THRESHOLD) {
    bridge->stats.time_inflamed_ms += (current_time_ms - prev_update);
}
```

---

### P1-5: Dead code after `return` in `language_motor_bridge_destroy`

**File**: `src/language/bridges/nimcp_language_motor_bridge.c`
**Lines**: 392-395

```c
void language_motor_bridge_destroy(language_motor_bridge_t* bridge)
{
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_motor");  // DEAD
    }
```

The debug log is unreachable after the `return` statement. The intent was likely to log before returning or to log outside the null-check block.

**Fix**: Move the debug log before the `return` or remove it:
```c
if (!bridge) {
    return;
}
NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_motor");
```

---

### P1-6: Dead code after `return` in `language_cingulate_bridge_destroy`

**File**: `src/language/bridges/nimcp_language_cingulate_bridge.c`
**Lines**: 418-421

```c
void language_cingulate_bridge_destroy(language_cingulate_bridge_t* bridge)
{
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "language_cingulate");  // DEAD
    }
```

Same pattern as P1-5. Unreachable code.

**Fix**: Same as P1-5 -- move or remove the log.

---

## P2 Findings

### P2-1: Wrong error code -- `NIMCP_ERROR_NULL_POINTER` for `!bridge->active` (boolean flag)

**Files**: 10 bridge files, ~43 instances total

| File | Lines (sample) | Count |
|------|----------------|-------|
| `nimcp_language_cognitive_bridge.c` | 520, 553, 652, 745, 790, 811, 871, 902 | 8 |
| `nimcp_language_perception_bridge.c` | 496, 532, 552, 583, 613, 660, 683, 707, 728 | 9 |
| `nimcp_language_gpu_bridge.c` | 475, 510, 542, 572, 602 | 5 |
| `nimcp_language_training_bridge.c` | 443, 488, 529, 658, 706, 813 | 6 |
| `nimcp_language_thalamic_bridge.c` | 149, 177, 204, 231, 257 | 5 |
| `nimcp_language_logic_bridge.c` | 164, 213, 270, 296, 350 | 5 |
| `nimcp_language_omni_bridge.c` | 369, 408, 445, 564 | 4 |
| `nimcp_language_immune_bridge.c` | 468, 832 | 2 |

Pattern:
```c
if (!bridge->active) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...: bridge->active is NULL");
    return -1;
}
```

`bridge->active` is a boolean flag, not a pointer. The error code should be `NIMCP_ERROR_INVALID_STATE` and the message should say "bridge is not active" rather than "is NULL".

**Fix**: Change all to:
```c
if (!bridge->active) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
        "function_name: bridge is not active");
    return -1;
}
```

---

### P2-2: Wrong error code -- `NIMCP_ERROR_NULL_POINTER` for `!bridge->config.enable_*` (boolean config fields)

**Files**: 6 bridge files, 12 instances total

| File | Lines | Count |
|------|-------|-------|
| `nimcp_language_parietal_bridge.c` | 234, 274, 344, 413 | 4 |
| `nimcp_language_logic_bridge.c` | 168, 217, 274 | 3 |
| `nimcp_language_cerebellum_bridge.c` | 255, 330 | 2 |
| `nimcp_language_cingulate_bridge.c` | 225 | 1 |
| `nimcp_language_insula_bridge.c` | 159, 252 | 2 |

Pattern:
```c
if (!bridge->config.enable_spatial_language) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "...: bridge->config is NULL");
    return -1;
}
```

These check boolean config fields (e.g., `enable_spatial_language`, `enable_number_processing`, `enable_self_correction`), not whether `bridge->config` is NULL. The error code should be `NIMCP_ERROR_INVALID_STATE` or `NIMCP_ERROR_NOT_INITIALIZED`, and the message should say "feature not enabled" not "config is NULL".

**Fix**: Change to:
```c
if (!bridge->config.enable_spatial_language) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE,
        "function_name: spatial language not enabled");
    return -1;
}
```

---

### P2-3: False positive NIMCP_THROW_TO_IMMUNE in queue/dequeue (normal full/empty)

**File**: `src/language/bridges/nimcp_language_motor_bridge.c`
**Lines**: 136, 153

```c
// Line 136 - queue_command
if (bridge->queue_count >= MAX_PENDING_COMMANDS) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "queue_command: capacity exceeded");
    return false;
}

// Line 153 - dequeue_command
if (bridge->queue_count == 0) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dequeue_command: bridge->queue_count is zero");
    return false;
}
```

Queue full and queue empty are normal operational states, not bugs. These fire on every queue overflow/underflow, creating noise in the immune system.

**Fix**: Remove both NIMCP_THROW_TO_IMMUNE calls; the `return false` is sufficient for callers.

---

### P2-4: False positive NIMCP_THROW_TO_IMMUNE in speech queue (normal full/empty)

**File**: `src/language/bridges/nimcp_language_temporal_bridge.c`
**Lines**: 189, 206

```c
// Line 189 - queue_speech_event (buffer full)
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "queue_speech_event: validation failed");

// Line 206 - dequeue_speech_event (buffer empty)
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dequeue_speech_event: validation failed");
```

Same as P2-3: buffer full/empty is normal. Additionally the error code `NIMCP_ERROR_INVALID_PARAM` is wrong -- these are not parameter validation failures.

**Fix**: Remove both NIMCP_THROW_TO_IMMUNE calls.

---

### P2-5: False positive NIMCP_THROW_TO_IMMUNE for "word not found" lookup

**File**: `src/language/bridges/nimcp_language_parietal_bridge.c`
**Lines**: 258, 397

```c
// Line 258
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
    "language_parietal_process_spatial_word: operation failed");
return -1;  /* Word not in spatial vocabulary */

// Line 397
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
    "language_parietal_word_to_number: operation failed");
return -1;  /* Word not recognized as number */
```

Both are normal "not found" lookup results. The comments even say so. These fire on every unrecognized word.

**Fix**: Remove both NIMCP_THROW_TO_IMMUNE calls. Return -1 is sufficient.

---

### P2-6: False positive NIMCP_THROW_TO_IMMUNE for "word not found" in hippocampus

**File**: `src/language/bridges/nimcp_language_hippocampus_bridge.c`
**Lines**: 509, 414

```c
// Line 509 - retrieve_by_word: entry not found
word_memory_entry_t* entry = find_memory_by_word(bridge, word);
if (!entry) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...: entry is NULL");
    return -1;
}

// Line 414 - strengthen_memory: entry not found
word_memory_entry_t* entry = find_memory_by_id(bridge, memory_id);
if (!entry) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...: entry is NULL");
    return -1;
}
```

Memory not found is normal lookup behavior. These fire on every miss.

**Fix**: Remove the NIMCP_THROW_TO_IMMUNE calls. Keep the `return -1`.

---

### P2-7: False positive NIMCP_THROW_TO_IMMUNE for TODO stub

**File**: `src/language/bridges/nimcp_language_temporal_bridge.c`
**Lines**: 1025-1026

```c
/* TODO: Return actual bio context when integrated */
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
    "language_temporal_get_bio_context: bridge is NULL");
return NULL;
```

This fires on EVERY call to `get_bio_context` even when the bridge is valid and non-NULL. The function always throws unconditionally after the initial NULL check passes (line 1017-1021).

**Fix**: Remove the throw. Return NULL silently or use a different signaling mechanism:
```c
/* TODO: Return actual bio context when integrated */
return NULL;
```

---

### P2-8: False positive NIMCP_THROW_TO_IMMUNE for self-correction disabled

**File**: `src/language/bridges/nimcp_language_cingulate_bridge.c`
**Line**: 225

```c
if (!bridge->config.enable_self_correction) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "should_trigger_correction: bridge->config is NULL");
    return false;
}
```

Self-correction being disabled is a configuration choice, not an error. This fires every time the function is called with the feature disabled.

**Fix**: Remove the NIMCP_THROW_TO_IMMUNE. Return false silently. (Note: the wrong error code is also covered under P2-2.)

---

### P2-9: Wrong error code `NIMCP_ERROR_NO_MEMORY` for NULL parameter checks

**File**: `src/language/bridges/nimcp_language_hippocampus_bridge.c`
**Lines**: 503, 643

```c
// Line 503
if (!bridge || !word || !memory) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "...: required parameter is NULL");
    return -1;
}

// Line 643
if (!bridge || !memory) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "...: required parameter is NULL");
    return -1;
}
```

`NIMCP_ERROR_NO_MEMORY` means allocation failure. These are NULL pointer parameter checks and should use `NIMCP_ERROR_NULL_POINTER`.

**Fix**: Change to `NIMCP_ERROR_NULL_POINTER`.

---

### P2-10: Resource leak -- `log_ctx` not freed in destroy (motor bridge)

**File**: `src/language/bridges/nimcp_language_motor_bridge.c`
**Lines**: 379 (create), 390-402 (destroy)

```c
// Create (line 379)
bridge->log_ctx = nimcp_log_context_create(NULL, LANGUAGE_MOTOR_MODULE_NAME);

// Destroy (lines 397-401)
if (bridge->log_ctx) {
    NIMCP_LOG_INFO(bridge->log_ctx, "Language-Motor bridge destroyed");
}
nimcp_free(bridge);  // <-- log_ctx not freed before bridge is freed
```

`nimcp_log_context_create` allocates a log context, but `language_motor_bridge_destroy` never calls `nimcp_log_context_destroy()` before freeing the bridge struct. The log context is leaked.

**Fix**: Add `nimcp_log_context_destroy(bridge->log_ctx);` before `nimcp_free(bridge)`.

---

### P2-11: Resource leak -- `log_ctx` not freed in destroy (cingulate bridge)

**File**: `src/language/bridges/nimcp_language_cingulate_bridge.c`
**Lines**: 405 (create), 416-428 (destroy)

Same pattern as P2-10. `nimcp_log_context_create` on line 405, but `language_cingulate_bridge_destroy` (lines 416-428) does not call `nimcp_log_context_destroy()`.

**Fix**: Add `nimcp_log_context_destroy(bridge->log_ctx);` before `nimcp_free(bridge)`.

---

### P2-12: GPU memory pool `used_memory` not decremented on re-upload

**File**: `src/language/bridges/nimcp_language_gpu_bridge.c`
**Lines**: 349-366, 388-407

```c
// Line 349-351 - free old embeddings
if (bridge->word_embeddings_gpu) {
    nimcp_free(bridge->word_embeddings_gpu);
}
// Line 366 - increment used_memory
bridge->memory_pool.used_memory += size;
```

When re-uploading embeddings, the old allocation is freed (line 350) but `used_memory` is not decremented by the old size. Similarly for concept embeddings (line 389) and semantic graph uploads. Over time, `used_memory` only grows, giving an inaccurate memory usage reading.

**Fix**: Track and subtract the old size before adding the new:
```c
if (bridge->word_embeddings_gpu) {
    size_t old_size = (size_t)bridge->word_embedding_count * bridge->embedding_dim * sizeof(float);
    bridge->memory_pool.used_memory -= old_size;
    nimcp_free(bridge->word_embeddings_gpu);
}
```

---

### P2-13: Temporal bridge `cache_add` -- dangling pointer on embedding allocation failure

**File**: `src/language/bridges/nimcp_language_temporal_bridge.c`
**Lines**: 163-175

```c
// Line 163 - shallow struct copy (copies embedding pointer)
bridge->concept_cache[target_idx].activation = *concept;

// Lines 168-175 - deep copy embedding
if (concept->embedding && concept->embedding_dim > 0) {
    bridge->concept_cache[target_idx].activation.embedding =
        nimcp_malloc(concept->embedding_dim * sizeof(float));
    if (bridge->concept_cache[target_idx].activation.embedding) {
        memcpy(...);
    }
    // If malloc fails, .embedding still holds concept->embedding (from struct copy)
}
```

If `nimcp_malloc` fails on line 170, the cache entry's `embedding` field still points to the original concept's embedding (copied on line 163). When the cache entry is later evicted (line 158 `nimcp_free`), it frees memory it does not own.

**Fix**: NULL out the embedding pointer after the struct copy, before attempting the deep copy:
```c
bridge->concept_cache[target_idx].activation = *concept;
bridge->concept_cache[target_idx].activation.embedding = NULL;  // clear shallow copy
bridge->concept_cache[target_idx].last_access_ms = timestamp_ms;
bridge->concept_cache[target_idx].valid = true;

if (concept->embedding && concept->embedding_dim > 0) {
    bridge->concept_cache[target_idx].activation.embedding =
        nimcp_malloc(concept->embedding_dim * sizeof(float));
    // ...
}
```

---

## P3 Findings

### P3-1: Missing bounds check in `language_thalamic_bridge_gate_nucleus` return

**File**: `src/language/bridges/nimcp_language_thalamic_bridge.c`
**Line**: 339

```c
if (nucleus >= LANG_THAL_NUCLEUS_COUNT) return -1;
```

This correctly rejects out-of-bounds values, but returns `-1` without any NIMCP_THROW_TO_IMMUNE. This is inconsistent with the rest of the file where invalid parameters trigger throws. Not a bug per se, but a P3 inconsistency.

**Fix**: Add a throw before the return for consistency:
```c
if (nucleus >= LANG_THAL_NUCLEUS_COUNT) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
        "language_thalamic_bridge_gate_nucleus: nucleus out of range");
    return -1;
}
```

---

### P3-2: Missing error code in `language_hippocampus_retrieve` throw

**File**: `src/language/bridges/nimcp_language_hippocampus_bridge.c`
**Line**: 447

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
    "language_hippocampus_retrieve: required parameter is NULL (result->memories, result->similarities)");
```

This is an allocation failure, not a NULL parameter. Error code should be `NIMCP_ERROR_NO_MEMORY`.

**Fix**: Change to `NIMCP_ERROR_NO_MEMORY`.

---

### P3-3: Missing `bridge->is_initialized` check in `hippocampus_bridge_update`

**File**: `src/language/bridges/nimcp_language_hippocampus_bridge.c`
**Line**: 292-293

```c
if (!bridge || !bridge->is_initialized) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "...: required parameter is NULL (bridge, bridge->is_initialized)");
```

Error message says "is NULL" but `bridge->is_initialized` is a boolean. Should say "bridge not initialized" and use `NIMCP_ERROR_NOT_INITIALIZED`.

**Fix**: Split the checks:
```c
if (!bridge) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...: bridge is NULL");
    return -1;
}
if (!bridge->is_initialized) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "...: bridge not initialized");
    return -1;
}
```

---

### P3-4: Missing validation of metabolic state ranges

**File**: `src/language/bridges/nimcp_language_substrate_bridge.c`

Functions that accept metabolic state values (glucose, oxygen, ATP levels) do not validate that values are within expected physiological ranges (e.g., 0.0-1.0). Callers could pass negative or extremely high values without warning.

**Fix**: Add range clamping or validation for metabolic parameters.

---

### P3-5: Missing validation of sensitivity array values

**File**: `src/language/bridges/nimcp_language_immune_bridge.c`

`language_immune_bridge_set_sensitivities` and similar functions do not validate that sensitivity values are within reasonable bounds. Extreme values could cause numerical instability in downstream computations.

**Fix**: Add range validation or clamping for sensitivity values.

---

## Files Reviewed (Clean)

The following files had no P1 or P2 findings (may have minor P3 observations not individually listed):

| File | Lines | Notes |
|------|-------|-------|
| `nimcp_language_prefrontal_bridge.c` | ~853 | Clean. Proper goal linked list management. |
| `nimcp_language_cerebellum_bridge.c` | ~531 | Clean (P2-2 config checks noted above). |
| `nimcp_language_insula_bridge.c` | ~319 | Clean (P2-2 config checks noted above). |
| `nimcp_language_substrate_bridge.c` | ~350 | Clean except P3-4. |

---

## Consolidated Statistics

**By priority**:
- P1: 6 findings (1 NULL deref, 1 out-of-bounds, 1 resource leak, 1 logic bug, 2 dead code)
- P2: 13 findings (2 wrong error code patterns [~55 instances], 6 false positive THROW, 2 resource leak, 1 memory tracking, 1 dangling pointer, 1 wrong error code)
- P3: 5 findings (1 inconsistent error handling, 1 wrong error code, 1 misleading message, 2 missing validation)

**By file** (P1+P2 only):

| File | P1 | P2 |
|------|----|----|
| `nimcp_language_motor_bridge.c` | 2 (NULL deref, dead code) | 2 (false positive queue, log_ctx leak) |
| `nimcp_language_thalamic_bridge.c` | 1 (out-of-bounds) | 1 (wrong error code pattern) |
| `nimcp_language_hippocampus_bridge.c` | 1 (resource leak) | 2 (false positive lookup, wrong error code) |
| `nimcp_language_immune_bridge.c` | 1 (logic bug) | 1 (wrong error code pattern) |
| `nimcp_language_cingulate_bridge.c` | 1 (dead code) | 2 (false positive config, log_ctx leak) |
| `nimcp_language_temporal_bridge.c` | 0 | 3 (false positive queue/stub, dangling pointer) |
| `nimcp_language_parietal_bridge.c` | 0 | 2 (false positive lookup, wrong error code pattern) |
| `nimcp_language_gpu_bridge.c` | 0 | 2 (wrong error code pattern, memory tracking) |
| `nimcp_language_cognitive_bridge.c` | 0 | 1 (wrong error code pattern) |
| `nimcp_language_perception_bridge.c` | 0 | 1 (wrong error code pattern) |
| `nimcp_language_training_bridge.c` | 0 | 1 (wrong error code pattern) |
| `nimcp_language_logic_bridge.c` | 0 | 1 (wrong error code pattern) |
| `nimcp_language_omni_bridge.c` | 0 | 1 (wrong error code pattern) |

**Systemic patterns** (not counted individually above but captured in P2-1 and P2-2):
- `bridge->active is NULL` wrong error code: ~43 instances across 10 files
- `bridge->config is NULL` wrong error code: ~12 instances across 6 files
