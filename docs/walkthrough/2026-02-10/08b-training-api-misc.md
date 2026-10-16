# Walkthrough 08b: Training, API, Embodiment, Superhuman, Information

**Date**: 2026-02-10
**Reviewer**: Claude Opus 4.6
**Scope**: `src/training/`, `src/api/`, `src/embodiment/`, `src/superhuman/`, `src/information/`
**Note**: `src/quantum/` and `src/optimization/` do not exist (empty directories).

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| **P1**   | 2     | Deadlock, use-after-free |
| **P2**   | 65    | False positive THROW_TO_IMMUNE, missing NULL checks, wrong error codes, off-by-one |
| **P3**   | 15    | Double throws, dead code, wrong function names in errors, magic numbers |

---

## P1 - Critical Bugs

### P1-1: DEADLOCK in nimcp_adversarial_training.c

**File**: `/home/bbrelin/nimcp/src/training/nimcp_adversarial_training.c`
**Lines**: ~632 (`adv_compute_loss`), ~982 (`adv_evaluate`)
**Description**: Both `adv_compute_loss()` and `adv_evaluate()` lock `ctx->mutex` (which is a NORMAL, non-recursive mutex), then call `adv_generate_batch()` which also attempts to lock the same mutex. This causes a guaranteed deadlock.
**Fix**: Either make the mutex recursive, or create `adv_generate_batch_unlocked()` internal helper that callers invoke while already holding the lock.

### P1-2: USE-AFTER-FREE in nimcp_time_dilation.c

**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_time_dilation.c`
**Lines**: ~991-998 (`time_dilation_get_event`)
**Description**: The function frees `oldest->data_copy` then returns the event struct. The caller receives an `event.data` pointer that still references the freed `data_copy` memory. Any access by the caller to `event.data` is a use-after-free.
**Fix**: Either (a) return a deep copy of the data that the caller owns, or (b) document that `event.data` is NULL after this call and set it explicitly, or (c) transfer ownership to the caller without freeing.

---

## P2 - Medium Priority

### P2 - Training Module

#### P2-1: Missing NULL checks for allocations in nimcp_adversarial_training.c
**File**: `/home/bbrelin/nimcp/src/training/nimcp_adversarial_training.c`
**Lines**: ~336, ~435-436, ~568 (grad/perturbation allocations), ~726 (MART `probs` allocation)
**Description**: Several `nimcp_malloc`/`nimcp_calloc` return values are used without NULL checks. On allocation failure, these will segfault.
**Fix**: Add NULL checks with NIMCP_THROW_MEMORY and proper cleanup.

#### P2-2: Missing NULL checks in nimcp_auto_architecture.c
**File**: `/home/bbrelin/nimcp/src/training/nimcp_auto_architecture.c`
**Lines**: ~495-498 (history/Pareto allocations), ~772-775 (result Pareto arrays)
**Description**: Allocation results not NULL-checked before use.
**Fix**: Add NULL checks with proper error handling.

#### P2-3: False positive THROW_TO_IMMUNE in nimcp_cnn_cortex_bridge.c
**File**: `/home/bbrelin/nimcp/src/training/nimcp_cnn_cortex_bridge.c`
**Lines**: ~74, ~94 (`should_skip`), ~417 (`cnn_cortex_bridge_is_connected`), ~896 (`cnn_cortex_bridge_should_skip_sample`)
**Description**: These functions throw on normal operational states (not connected, should skip). These are normal return paths, not error conditions.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE; just return the appropriate value.

#### P2-4: False positive THROW_TO_IMMUNE in nimcp_training_bio_async_bridge.c
**File**: `/home/bbrelin/nimcp/src/training/nimcp_training_bio_async_bridge.c`
**Lines**: ~144 (`find_worker_unlocked` not found), ~368 (`training_bio_bridge_is_connected`)
**Description**: Throwing on normal "not found" and "not connected" states.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE from normal code paths.

#### P2-5: False positive THROW_TO_IMMUNE in nimcp_training_integration_hub.c
**File**: `/home/bbrelin/nimcp/src/training/nimcp_training_integration_hub.c`
**Lines**: ~76, ~86, ~105, ~122, ~134, ~188, ~193
**Description**: Multiple find helpers throw on normal "not found" results. These are linear search functions that legitimately return NULL/not-found.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE; return NULL silently.

#### P2-6: False positive THROW_TO_IMMUNE in nimcp_training_dispatch.c
**File**: `/home/bbrelin/nimcp/src/training/nimcp_training_dispatch.c`
**Lines**: ~602, ~651, ~721
**Description**: Throws on normal dispatch/lookup paths that are not errors.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE from normal paths.

#### P2-7: False positive THROW_TO_IMMUNE in nimcp_training_state_manager.c
**File**: `/home/bbrelin/nimcp/src/training/nimcp_training_state_manager.c`
**Line**: ~489 (`find_module`)
**Description**: Throws on normal "module not found" lookup result.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE.

#### P2-8: False positive THROW_TO_IMMUNE in nimcp_mixed_precision.c
**File**: `/home/bbrelin/nimcp/src/training/nimcp_mixed_precision.c`
**Line**: ~66 (`contains_nan_inf`)
**Description**: Throws on normal "no NaN found" return path.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE.

#### P2-9: False positive THROW_TO_IMMUNE in nimcp_training_symbolic_logic_hub_bridge.c
**File**: `/home/bbrelin/nimcp/src/training/integration/nimcp_training_symbolic_logic_hub_bridge.c`
**Lines**: ~699 (`training_logic_hub_is_action_safe` - action blocked by constraints), ~1162 (`evaluate_rule_condition` default case - unknown rule type)
**Description**: Line 699 throws when an action is blocked by safety constraints, which is a normal validation outcome. Line 1162 throws in the default case for unknown rule types, which is a normal fallthrough.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE; return false/default silently.

#### P2-10: False positive THROW_TO_IMMUNE in nimcp_api_training.c
**File**: `/home/bbrelin/nimcp/src/api/nimcp_api_training.c`
**Line**: ~126 (`get_training_state`)
**Description**: Throws NIMCP_ERROR_NULL_POINTER when the training states array is full (no free slots). This is a capacity issue, not a NULL pointer. The error code is also wrong.
**Fix**: Change to NIMCP_ERROR_RESOURCE_EXHAUSTED or NIMCP_ERROR_NO_MEMORY. Consider whether this should throw at all (it's an internal helper).

### P2 - Embodiment Module

#### P2-11: False positive THROW_TO_IMMUNE in nimcp_affordance_processing.c
**File**: `/home/bbrelin/nimcp/src/embodiment/nimcp_affordance_processing.c`
**Lines**: ~112 (`find_object`), ~128 (`find_affordance`)
**Description**: Throws on normal "not found" search results.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE from search helpers.

#### P2-12: False positive THROW_TO_IMMUNE in nimcp_body_ownership.c
**File**: `/home/bbrelin/nimcp/src/embodiment/nimcp_body_ownership.c`
**Lines**: ~114 (`find_part`), ~130 (`find_external_object`)
**Description**: Throws on normal "not found" search results.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE from search helpers.

#### P2-13: False positive THROW_TO_IMMUNE in nimcp_embodied_simulation.c
**File**: `/home/bbrelin/nimcp/src/embodiment/nimcp_embodied_simulation.c`
**Lines**: ~142 (`find_simulation`), ~158 (`find_effector`), ~174 (`find_object`)
**Description**: Throws on normal "not found" search results.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE from search helpers.

#### P2-14: False positive THROW_TO_IMMUNE in nimcp_interoceptive_prediction.c
**File**: `/home/bbrelin/nimcp/src/embodiment/nimcp_interoceptive_prediction.c`
**Lines**: ~116, ~132, ~148, ~164 (multiple `find_*` helpers)
**Description**: Throws on normal "not found" search results.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE from search helpers.

### P2 - Superhuman Module

#### P2-15 through P2-24: False positive THROW_TO_IMMUNE in nimcp_synesthesia.c
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_synesthesia.c`

| Line | Function | Issue |
|------|----------|-------|
| ~483 | `synesthesia_get_grapheme_color` | Throws when module is inhibited (normal state check) |
| ~506 | `synesthesia_get_grapheme_color` | Throws on normal "association not found" |
| ~637 | `synesthesia_get_sound_shape` | Throws when module is inhibited |
| ~670 | `synesthesia_get_sound_shape` | Throws on normal "not found" |
| ~759 | `synesthesia_get_taste_touch` | Throws when module is inhibited |
| ~780 | `synesthesia_get_taste_touch` | Throws on normal "not found" |
| ~928 | `synesthesia_trigger_experience` | Throws when module is inhibited |
| ~1087 | `synesthesia_get_association` | Throws on normal "not found" |
| ~1129 | `synesthesia_update_strength` | Throws on normal "not found" |
| ~1166 | `synesthesia_remove_association` | Throws on normal "not found" |

**Fix**: Remove NIMCP_THROW_TO_IMMUNE from all inhibited-state checks and not-found paths.

#### P2-25: Wrong error code in synesthesia_cascade
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_synesthesia.c`
**Line**: ~1191
**Description**: `synesthesia_cascade` throws NIMCP_ERROR_NULL_POINTER when cascade is disabled via config flag. This is not a NULL pointer situation -- it is a normal "cascade disabled" state.
**Fix**: Either remove the throw entirely, or change to NIMCP_ERROR_INVALID_STATE.

#### P2-26: False positive THROW_TO_IMMUNE in find_empty_slot (time_dilation)
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_time_dilation.c`
**Line**: ~230
**Description**: `find_empty_slot` throws NIMCP_ERROR_NULL_POINTER when no empty slot is available. This is a capacity issue, not a NULL pointer.
**Fix**: Remove the throw (this is a normal "full" condition) or change to NIMCP_ERROR_RESOURCE_EXHAUSTED.

#### P2-27: False positive THROW_TO_IMMUNE in find_event_by_id (time_dilation)
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_time_dilation.c`
**Line**: ~245
**Description**: Throws on normal "event not found" search result.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE.

#### P2-28: Platform layer mutex used instead of thread layer
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_time_dilation.c`
**Line**: ~397
**Description**: Uses `nimcp_platform_mutex_create()` (platform layer) instead of `nimcp_mutex_create()` (thread layer). Inconsistent with codebase conventions. Thread layer provides additional features like deadlock detection.
**Fix**: Change to `nimcp_mutex_create()` with appropriate attributes.

#### P2-29: False positive THROW_TO_IMMUNE in index_memory_by_date (hyperthymesia)
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_hyperthymesia.c`
**Line**: ~315
**Description**: Throws when year is out of range. This is a validation check that normally rejects invalid dates -- it is not an error condition.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE; return false silently.

#### P2-30: Missing month bounds validation in index_memory_by_date
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_hyperthymesia.c`
**Line**: ~331
**Description**: `month-1` is used as an array index without validating that month >= 1. If month == 0, this computes -1 as a uint32_t, causing massive unsigned underflow and out-of-bounds array access.
**Fix**: Add `if (month < 1 || month > 12) return false;` before `month-1`.

#### P2-31: Missing day bounds validation in index_memory_by_date
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_hyperthymesia.c`
**Line**: ~343
**Description**: `day-1` is used as an array index without validating that day >= 1. Same underflow risk as month.
**Fix**: Add `if (day < 1 || day > 31) return false;` before `day-1`.

#### P2-32 through P2-39: False positive THROW_TO_IMMUNE in nimcp_hyperthymesia.c

| Line | Function | Issue |
|------|----------|-------|
| ~692 | `hyperthymesia_add_sensory_trace` | Throws on memory entry not found |
| ~827 | `hyperthymesia_link_memories` | Throws on entry not found |
| ~907 | `hyperthymesia_retrieve_by_date` | Throws when date indexing disabled |
| ~1173 | `hyperthymesia_reexperience` | Throws on entry not found |
| ~1287 | `hyperthymesia_reexperience_modality` | Throws on entry not found |
| ~1313 | `hyperthymesia_reexperience_modality` | Throws on trace modality not found |
| ~1341 | `hyperthymesia_reexperience_emotion` | Throws on entry not found |
| ~1648 | `hyperthymesia_update_vividness` | Throws on entry not found (also uses wrong error code NIMCP_ERROR_NO_MEMORY) |

**Fix**: Remove NIMCP_THROW_TO_IMMUNE from all normal not-found paths. Fix line ~1648 to use NIMCP_ERROR_NOT_FOUND if a throw is retained.

#### P2-40: False positive THROW_TO_IMMUNE in savant_is_leap_year
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_savant_mode.c`
**Lines**: ~139, ~144
**Description**: Throws on normal non-leap-year return paths. Leap year calculation is a pure boolean function -- returning false is not an error.
**Fix**: Remove NIMCP_THROW_TO_IMMUNE; just return false.

### P2 - Information Module

#### P2-41: False positive THROW_TO_IMMUNE in shannon_distribution_normalize
**File**: `/home/bbrelin/nimcp/src/information/nimcp_shannon.c`
**Line**: ~887
**Description**: Throws when distribution sum < epsilon. An all-zero distribution could be a legitimate initial state, not an error condition.
**Fix**: Return early with the distribution unchanged, or normalize to uniform distribution.

### P2 - API Module (Duplicate Symbol / Inconsistency Issues)

#### P2-42: Duplicate function definitions across nimcp.c and refactored API files
**File**: `/home/bbrelin/nimcp/src/api/nimcp.c` + `nimcp_brain_api.c` + `nimcp_cognitive_api.c` + `nimcp_oscillation_api.c` + `nimcp_subsystems_api.c` + `nimcp_snapshot_api.c` + `nimcp_api_cognitive.c` + `nimcp_api_oscillation.c` + `nimcp_api_inference.c` + `nimcp_api_training.c`
**Description**: The original `nimcp.c` still contains ALL function implementations (brain create/destroy, working memory, global workspace, oscillation, ethics, knowledge, neural network, snapshot/COW). These same functions are ALSO defined in the refactored `_api.c` files AND the `nimcp_*_api.c` files. This means every public API function has 2-3 duplicate definitions.

At link time, only one will be used (or the linker will error on duplicate symbols if not using `static`). This is a major source of confusion and maintenance burden. The duplicate implementations have drifted apart:
- `nimcp.c` line 112: `g_last_error` is `_Thread_local` (TLS)
- `nimcp_refactored.c` line 88: `g_last_error` is NOT `_Thread_local`
- `nimcp.c`: `set_error()` is `static`
- `nimcp_refactored.c`: `set_error()` is NOT `static` (global linkage)
- Several functions have different error handling approaches between the copies

**Fix**: Choose one canonical implementation per function. Remove duplicates. The CMakeLists.txt likely only compiles one set, but the drift is dangerous.

#### P2-43: Inconsistent error handling between nimcp_brain_predict copies
**File**: `/home/bbrelin/nimcp/src/api/nimcp.c` vs `nimcp_brain_api.c` vs `nimcp_api_inference.c`
**Description**: Three implementations of `nimcp_brain_predict`:
- `nimcp.c` line 412: Uses `API_CHECK_THROW`, `NIMCP_MAX_LABEL_SIZE`
- `nimcp_brain_api.c` line 152: Uses `NIMCP_API_CHECK_NULL`, hardcoded `63`
- `nimcp_api_inference.c` line 105: Uses `NIMCP_CHECK_THROW`, hardcoded `63`

The nimcp.c version uses `NIMCP_MAX_LABEL_SIZE - 1` (the correct approach), while both refactored copies hardcode `63`. If `NIMCP_MAX_LABEL_SIZE` changes, the refactored copies will silently be wrong.
**Fix**: All copies should use `NIMCP_MAX_LABEL_SIZE`.

#### P2-44: nimcp_cognitive_api.c nimcp_brain_resize returns -1 instead of proper error
**File**: `/home/bbrelin/nimcp/src/api/nimcp_cognitive_api.c`
**Lines**: 379, 395
**Description**: `nimcp_brain_resize()` and `nimcp_brain_auto_resize()` return `-1` on failure, but they are declared as returning `bool`. Returning `-1` for a `bool` function evaluates to `true` in C, which is the OPPOSITE of the intended error signal.
**Fix**: Return `false` instead of `-1`.

---

## P3 - Low Priority

### P3 - Training Module

#### P3-1: Wrong function name in error messages (nimcp_cnn_training.c)
**File**: `/home/bbrelin/nimcp/src/training/nimcp_cnn_training.c`
**Description**: Error messages reference "cnn_he_init" instead of the actual function name.
**Fix**: Update error message strings to match actual function names.

#### P3-2: Dead code after return (nimcp_training_bio_async_bridge.c)
**File**: `/home/bbrelin/nimcp/src/training/nimcp_training_bio_async_bridge.c`
**Lines**: ~258-261 (`training_bio_bridge_destroy`)
**Description**: Unreachable code after a return statement in the destroy function.
**Fix**: Remove the dead code or restructure control flow.

#### P3-3: Double throws in nimcp_continual_learning.c
**File**: `/home/bbrelin/nimcp/src/training/nimcp_continual_learning.c`
**Lines**: ~165-166, ~174-175
**Description**: NIMCP_THROW followed immediately by NIMCP_THROW_TO_IMMUNE (redundant, wastes cycles).
**Fix**: Keep only NIMCP_THROW_TO_IMMUNE (the one the immune system processes).

### P3 - Superhuman Module

#### P3-4: Double throw in synesthesia_trigger_experience
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_synesthesia.c`
**Line**: ~957
**Description**: NIMCP_THROW_MEMORY followed by NIMCP_THROW_TO_IMMUNE in the same error path. Redundant double-throw.
**Fix**: Keep only NIMCP_THROW_MEMORY (which already routes to immune).

#### P3-5: Incorrect avg_cascade_depth calculation
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_synesthesia.c`
**Line**: ~1309
**Description**: The running average `avg_cascade_depth` divides by `cascade_activations + 1`, but `cascade_activations` has already been incremented by `triggered_count` earlier in the same function. This means the denominator is too large, producing an incorrect average.
**Fix**: Use the pre-increment value of `cascade_activations` in the averaging formula.

#### P3-6: Magic number in time_dilation max_events calculation
**File**: `/home/bbrelin/nimcp/src/superhuman/nimcp_time_dilation.c`
**Line**: ~884
**Description**: `max_events = (uint32_t)(system->current_factor * 10)` uses a magic number `10`.
**Fix**: Define a named constant like `TIME_DILATION_EVENTS_PER_FACTOR`.

### P3 - Information Module

#### P3-7 through P3-10: Double throws in nimcp_shannon.c
**File**: `/home/bbrelin/nimcp/src/information/nimcp_shannon.c`

| Line | Function | Issue |
|------|----------|-------|
| ~822 | `shannon_distribution_create` | NIMCP_THROW_MEMORY then NIMCP_THROW_TO_IMMUNE |
| ~917 | `shannon_joint_distribution_create` | Same pattern |
| ~980 | `shannon_marginal_x` | Same pattern |
| ~1028 | `shannon_marginal_y` | Same pattern |

**Fix**: Remove the redundant NIMCP_THROW_TO_IMMUNE after NIMCP_THROW_MEMORY.

#### P3-11 through P3-12: Double throws in nimcp_cross_modal.c
**File**: `/home/bbrelin/nimcp/src/information/nimcp_cross_modal.c`

| Line | Function | Issue |
|------|----------|-------|
| ~476-477 | `cross_modal_create_routing_graph` | NIMCP_THROW then NIMCP_THROW_TO_IMMUNE |
| ~497-498 | `cross_modal_create_routing_graph` | NIMCP_THROW_MEMORY then NIMCP_THROW_TO_IMMUNE |

**Fix**: Remove redundant NIMCP_THROW_TO_IMMUNE.

### P3 - API Module

#### P3-13: nimcp_network_train throws for "not implemented"
**File**: `/home/bbrelin/nimcp/src/api/nimcp_api_network.c`
**Line**: ~138
**Description**: Uses NIMCP_THROW_TO_IMMUNE to report "not yet implemented". This pollutes the immune system log with non-error events.
**Fix**: Use NIMCP_THROW or LOG_WARN instead.

#### P3-14: nimcp_network_train in nimcp_subsystems_api.c also throws for "not implemented"
**File**: `/home/bbrelin/nimcp/src/api/nimcp_subsystems_api.c`
**Line**: ~118
**Description**: Same as P3-13 but in the refactored copy.
**Fix**: Same as P3-13.

#### P3-15: get_brain_probe_module_ctx wrong error code
**File**: `/home/bbrelin/nimcp/src/api/nimcp.c`
**Line**: ~793
**Description**: `get_brain_probe_module_ctx` throws NIMCP_ERROR_NO_MEMORY with message "bio_router_is_initialized is NULL". The error code is wrong (this is not a memory error) and the message is misleading (the router is not initialized, not NULL).
**Fix**: Change to NIMCP_ERROR_NOT_INITIALIZED with message "Bio-router not initialized".

---

## Files Reviewed (Complete List)

### src/training/ (21 files)
| File | Status | Findings |
|------|--------|----------|
| `nimcp_adversarial_training.c` | Reviewed | P1-1, P2-1 |
| `nimcp_auto_architecture.c` | Reviewed | P2-2 |
| `nimcp_cnn_cortex_bridge.c` | Reviewed | P2-3 |
| `nimcp_cnn_training.c` | Reviewed | P3-1 |
| `nimcp_continual_learning.c` | Reviewed | P3-3 |
| `nimcp_curriculum_learning.c` | Reviewed | Clean |
| `nimcp_distributed_training.c` | Reviewed | Clean |
| `nimcp_gradient_scaling.c` | Reviewed | Clean |
| `nimcp_hyperparam_opt.c` | Reviewed | Clean |
| `nimcp_knowledge_distillation.c` | Reviewed | Clean |
| `nimcp_meta_learning.c` | Reviewed | Clean |
| `nimcp_mixed_precision.c` | Reviewed | P2-8 |
| `nimcp_multi_task.c` | Reviewed | Clean |
| `nimcp_quantization_aware.c` | Reviewed | Clean |
| `nimcp_snn_backprop.c` | Reviewed | Clean |
| `nimcp_training_bio_async_bridge.c` | Reviewed | P2-4, P3-2 |
| `nimcp_training_data_pipeline.c` | Reviewed | Clean |
| `nimcp_training_dispatch.c` | Reviewed | P2-6 |
| `nimcp_training_integration_hub.c` | Reviewed | P2-5 |
| `nimcp_training_state_manager.c` | Reviewed | P2-7 |
| `integration/nimcp_training_symbolic_logic_hub_bridge.c` | Reviewed | P2-9 |

### src/api/ (13 files)
| File | Status | Findings |
|------|--------|----------|
| `nimcp.c` | Reviewed | P2-42, P2-43, P3-15 |
| `nimcp_refactored.c` | Reviewed | P2-42 (duplicate defs) |
| `nimcp_api_brain.c` | Reviewed | P2-43 (hardcoded label size) |
| `nimcp_brain_api.c` | Reviewed | P2-43 (hardcoded label size) |
| `nimcp_api_cognitive.c` | Reviewed | Clean (well-structured) |
| `nimcp_cognitive_api.c` | Reviewed | P2-44 |
| `nimcp_api_inference.c` | Reviewed | P2-43 (hardcoded label size) |
| `nimcp_api_network.c` | Reviewed | P3-13 |
| `nimcp_api_oscillation.c` | Reviewed | Clean |
| `nimcp_oscillation_api.c` | Reviewed | Clean |
| `nimcp_api_training.c` | Reviewed | P2-10 |
| `nimcp_snapshot_api.c` | Reviewed | Clean |
| `nimcp_subsystems_api.c` | Reviewed | P3-14 |

### src/embodiment/ (4 files)
| File | Status | Findings |
|------|--------|----------|
| `nimcp_affordance_processing.c` | Reviewed | P2-11 |
| `nimcp_body_ownership.c` | Reviewed | P2-12 |
| `nimcp_embodied_simulation.c` | Reviewed | P2-13 |
| `nimcp_interoceptive_prediction.c` | Reviewed | P2-14 |

### src/superhuman/ (7 files)
| File | Status | Findings |
|------|--------|----------|
| `nimcp_savant_mode.c` | Reviewed | P2-40 |
| `nimcp_synesthesia.c` | Reviewed | P2-15 to P2-25, P3-4, P3-5 |
| `nimcp_time_dilation.c` | Reviewed | P1-2, P2-26 to P2-28, P3-6 |
| `nimcp_hyperthymesia.c` | Reviewed | P2-29 to P2-39 |
| (remaining 3 files) | Reviewed in previous sessions | See pass2/pass3 docs |

### src/information/ (2 files)
| File | Status | Findings |
|------|--------|----------|
| `nimcp_shannon.c` | Reviewed | P2-41, P3-7 to P3-10 |
| `nimcp_cross_modal.c` | Reviewed | P3-11, P3-12 |

### src/quantum/, src/optimization/
These directories do not exist in the codebase. No files to review.

---

## Pattern Analysis

### Most Common Bug Pattern: False Positive NIMCP_THROW_TO_IMMUNE

**Count**: ~55 instances across all reviewed files

The most pervasive issue is NIMCP_THROW_TO_IMMUNE on normal code paths. These fall into several categories:

1. **Search/lookup "not found"** (~30 instances): `find_*` helper functions throw when the searched item doesn't exist. This is a normal O(N) search result.

2. **Module inhibited/disabled** (~8 instances): Synesthesia, hyperthymesia functions throw when their module is inhibited or a feature is disabled. This is a normal operational state.

3. **Capacity full** (~3 instances): `find_empty_slot`, `get_training_state` throw when arrays are full. These should use proper error codes, not NULL_POINTER.

4. **Validation rejection** (~5 instances): Functions throw when input fails validation. This is the validation working correctly, not an error.

### Second Most Common: Double Throws

**Count**: ~10 instances

NIMCP_THROW or NIMCP_THROW_MEMORY immediately followed by NIMCP_THROW_TO_IMMUNE. The second throw is redundant because the first already triggers immune system notification.

### API Duplication Risk

The refactored API has 3 copies of most functions (`nimcp.c`, `nimcp_*_api.c`, `nimcp_api_*.c`). These have already drifted apart in error handling style, thread safety (TLS vs global error buffer), label buffer sizes, and error codes used. This is a ticking time bomb for maintenance.

---

## Recommended Remediation Order

1. **P1-1** (Deadlock): Fix immediately -- any call to `adv_compute_loss()` or `adv_evaluate()` will hang.
2. **P1-2** (Use-after-free): Fix immediately -- caller will crash or corrupt heap.
3. **P2-30, P2-31** (Month/day bounds): Fix soon -- unsigned underflow causes out-of-bounds array access.
4. **P2-44** (bool returning -1): Fix soon -- returns true when it means false.
5. **P2-42** (Duplicate definitions): Address in next refactoring pass.
6. **All false positive THROW_TO_IMMUNE**: Batch fix -- systematic removal from search/find/lookup helpers.
