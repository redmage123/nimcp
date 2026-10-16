# Pass 8 Walkthrough: Cognitive Core Modules

**Date**: 2026-02-15
**Scope**: All `.c` files in 9 directories under `src/cognitive/`:
- `attention/` (7 files)
- `salience/` (17 files)
- `reasoning/` (14 files)
- `working_memory/` (5 files)
- `executive/` (7 files)
- `global_workspace/` (8 files)
- `predictive/` (7 files)
- `meta_learning/` (5 files)
- `curiosity/` (10 files)

**Total files in scope**: 80

---

## Summary

| Priority | Count | Categories |
|----------|-------|------------|
| P1 (Critical) | 12 | div-by-zero (5), data race (4), TOCTOU (1), false positive throw in hot path (2) |
| P2 (Important) | 38+ | wrong error code (~25), wrong func name in message (5), false positive throw (8) |
| P3 (Minor) | 1 | systemic mesh globals (all bridge files) |

---

## P1 Findings (Critical)

### P1-1: Division by zero in SNN bridge `sqrtf(change_magnitude / num_dims)` [SYSTEMIC]

Five SNN bridges within scope compute `sqrtf(change_magnitude / num_dims)` without checking `num_dims > 0`. The reasoning SNN bridge has the correct guard: `(num_dims > 0) ? sqrtf(...) : 0.0f`.

If `num_dims` is passed as 0, the division produces `NaN` (0/0) or `Inf`, which propagates through change detection logic.

| File | Line |
|------|------|
| `src/cognitive/curiosity/nimcp_curiosity_snn_bridge.c` | 460 |
| `src/cognitive/executive/nimcp_executive_snn_bridge.c` | 466 |
| `src/cognitive/meta_learning/nimcp_meta_learning_snn_bridge.c` | 463 |
| `src/cognitive/predictive/nimcp_predictive_snn_bridge.c` | 458 |
| `src/cognitive/global_workspace/nimcp_gw_snn_bridge.c` | 461 |

**Reference** (correct pattern):
```c
// src/cognitive/reasoning/nimcp_reasoning_snn_bridge.c:468
conflict_magnitude = (num_dims > 0) ? sqrtf(conflict_magnitude / num_dims) : 0.0f;
```

---

### P1-2: TOCTOU race on `hist->count` in salience history buffer

`src/cognitive/salience/nimcp_salience.c:504`

`history_buffer_compute_novelty()` reads `hist->count` without holding `hist->lock`. The callers (`compute_salience_fast`, `compute_salience_balanced`, `compute_salience_accurate`) hold `eval->eval_lock` but NOT `hist->lock`. Meanwhile, `history_buffer_add()` at line 266 modifies `hist->count` under `hist->lock`.

This is a TOCTOU race: `hist->count` can change between the read at line 504 and the loop that uses it, causing reads from uninitialized history entries.

---

### P1-3: Thread-unsafe `static char last_error[]` buffers (no `__thread`)

Three files declare `static char last_error[...]` without `__thread`, creating data races when multiple threads call these functions concurrently:

| File | Line | Size |
|------|------|------|
| `src/cognitive/predictive/nimcp_predictive.c` | 104 | 512 |
| `src/cognitive/meta_learning/nimcp_meta_learning.c` | 107 | 512 |
| `src/cognitive/working_memory/nimcp_working_memory.c` | 421 | 256 |

**Note**: `nimcp_executive.c` correctly uses `__thread` for its `last_error` at line 548. `nimcp_salience.c` correctly uses `__thread` for `g_salience_error` at line 126.

---

### P1-4: False positive `NIMCP_THROW_TO_IMMUNE` in neuron step hot path

`src/cognitive/salience/nimcp_salience_snn_bridge.c`

- **Line 167**: `neuron_step()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuron_step: validation failed")` when a neuron is in its refractory period. Refractory is normal biological behavior, not an error. This fires on every refractory neuron on every simulation step -- potentially thousands of throws per simulation.

- **Line 181**: `neuron_step()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neuron_step: capacity exceeded")` when a neuron does NOT spike. Not spiking is the common case. This throws on every non-spiking neuron on every step -- the dominant code path.

Both are in a tight inner loop called from `salience_snn_step()` which iterates over all channels and neurons. Combined, these produce O(neurons * channels * steps) immune system exceptions per simulation.

---

## P2 Findings (Important)

### P2-1: Wrong error code `NIMCP_ERROR_NO_MEMORY` for NULL pointer checks [SYSTEMIC]

Across all 9 directories, many NULL pointer guard clauses use `NIMCP_ERROR_NO_MEMORY` instead of `NIMCP_ERROR_NULL_POINTER`. This misattributes the error -- "out of memory" is semantically different from "caller passed NULL". Only cases where `nimcp_calloc`/`nimcp_malloc` returns NULL should use `NIMCP_ERROR_NO_MEMORY`.

**Representative in-scope instances** (not exhaustive):

| File | Line | Message |
|------|------|---------|
| `nimcp_meta_learning.c` | 261 | `"num_regions is zero"` -- should be `NIMCP_ERROR_INVALID_PARAM` |
| `nimcp_executive.c` | 1472 | `"required parameter is NULL (exec, goal)"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_executive.c` | 1989 | `"exec is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_salience_thalamic_bridge.c` | 266 | `"bridge is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_reasoning_sleep_bridge.c` | 249 | `"sleep is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_reasoning_integration.c` | 679 | `"validate_config is NULL"` -- should be `NIMCP_ERROR_INVALID_PARAM` (validation failure) |
| `nimcp_reasoning_integration.c` | 980 | `"config is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_executive_substrate_bridge.c` | 146 | `"executive is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_executive_substrate_bridge.c` | 152 | `"substrate is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_global_workspace_shannon.c` | 344 | `"workspace is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_global_workspace_immune.c` | 303 | `"ctx is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_predictive_hierarchy.c` | 361 | `"required parameter is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_predictive_hierarchy.c` | 384 | `"hier->mutex is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_meta_learning.c` | 1001 | `"name is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_meta_learning.c` | 1321 | `"examples is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_curiosity_enhanced.c` | 326 | `"required parameter is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_curiosity_enhanced.c` | 975 | `"sys is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_curiosity.c` | 809 | `"required parameter is NULL"` -- should be `NIMCP_ERROR_NULL_POINTER` |
| `nimcp_working_memory.c` | 1030 | `"item_copy is NULL"` -- correct (alloc failure) |
| `nimcp_working_memory.c` | 1152 | `"working_memory_add is NULL"` -- wrong msg (add() returned error, not NULL) |

**Total estimated**: ~25 instances within the 9 directories.

---

### P2-2: Wrong function name in throw messages [SYSTEMIC]

| File | Line | Actual Function | Error Says |
|------|------|----------------|------------|
| `nimcp_predictive_hierarchy.c` | 1184 | `pred_hier_result_create` | `"pred_hier_disconnect_bio_async: dims is NULL"` |
| `nimcp_predictive_hierarchy.c` | 1198 | `pred_hier_result_create` | `"pred_hier_disconnect_bio_async: result->level_results is NULL"` |
| `nimcp_predictive_hierarchy.c` | 1214 | `pred_hier_result_create` | `"pred_hier_disconnect_bio_async: required parameter is NULL"` |
| `nimcp_salience_thalamic_bridge.c` | 236 | `salience_thalamic_route_detection` | `"routed_ok is NULL"` (routed_ok is `bool`, not pointer) |
| `nimcp_salience_thalamic_bridge.c` | 311 | `salience_thalamic_route_priority` | `"routed_ok is NULL"` (routed_ok is `bool`, not pointer) |

---

### P2-3: False positive `NIMCP_THROW_TO_IMMUNE` on normal conditions

| File | Line | Function | Issue |
|------|------|----------|-------|
| `nimcp_working_memory.c` | 450 | `find_lowest_salience_index` (internal) | Throws on empty WM buffer during eviction -- empty is edge case, not error |
| `nimcp_working_memory.c` | 1716 | `find_highest_salience` (public) | Throws on empty WM -- empty is valid state, should return -1 |
| `nimcp_working_memory.c` | 1769 | `find_lowest_salience` (public) | Same as above |
| `nimcp_working_memory.c` | 1439 | `working_memory_refresh` | `NIMCP_ERROR_NULL_POINTER` for "enable_attention_refresh is NULL" -- it's a `bool`, not pointer |
| `nimcp_working_memory.c` | 1137 | `working_memory_add_with_emotion` | `"emotional_tag_is_valid is NULL"` -- wrong, it's validation failure not NULL |
| `nimcp_executive.c` | 1058 | `broadcast_decision_to_workspace` | Throws for workspace_integration not enabled AND confidence below threshold -- both are normal conditions |
| `nimcp_salience_plasticity_bridge.c` | 162 | `find_feature` (internal) | Throws `NIMCP_ERROR_OUT_OF_RANGE` when feature not found -- feature lookup miss is normal, not an error. Callers at lines 439, 508, 581, 935, 951, 967 handle NULL return gracefully |
| `nimcp_curiosity_enhanced.c` | 354 | `interest_get_or_create` | Throws `NIMCP_ERROR_NO_MEMORY` for hash table insert failure with message "success is NULL" -- `success` is a `bool`, not pointer |

---

### P2-4: Integer edge case in `sources_capacity *= 2`

`src/cognitive/curiosity/nimcp_curiosity.c:1705`

```c
sources_capacity *= 2;
```

If `sources_capacity` starts at 0 (e.g., from zero-initialized struct), this stays 0. The subsequent `realloc(ptr, 0 * sizeof(char*))` is implementation-defined (may return NULL or a unique pointer that cannot be dereferenced). The `new_sources` NULL check at line 1710 would then throw, but the root cause is the capacity staying at 0.

---

### P2-5: `nimcp_malloc` without `memset` in reasoning sleep bridge

`src/cognitive/reasoning/nimcp_reasoning_sleep_bridge.c:259`

Uses `nimcp_malloc` (not `nimcp_calloc`) to allocate the bridge struct, then does `memset(bridge, 0, ...)` at line 268. While the explicit `memset` makes this safe, the pattern is inconsistent with other bridges that use `nimcp_calloc` (which zero-initializes). Not a bug, but fragile -- if anyone adds code between the malloc and memset that accesses struct fields, they would read uninitialized memory.

---

### P2-6: Memory leak on `bridge_base_init` failure in executive substrate bridge

`src/cognitive/executive/nimcp_executive_substrate_bridge.c:194`

```c
if (nimcp_platform_mutex_init(bridge->base.mutex, false) != 0) {
    nimcp_free(bridge);     // Leaks bridge->base.mutex allocation from line 184
    ...
}
```

Line 184 allocates `bridge->base.mutex` with `nimcp_malloc`. If `nimcp_platform_mutex_init` fails at line 192, only `bridge` is freed, but the separately allocated `bridge->base.mutex` from line 184 is leaked.

---

## P3 Findings (Minor)

### P3-1: Unprotected static mesh registration globals [SYSTEMIC]

Every bridge file in scope has a pair of unprotected static globals:
```c
static mesh_participant_id_t g_*_mesh_id = 0;
static mesh_participant_registry_t* g_*_mesh_registry = NULL;
```

The `mesh_register` function checks `if (g_*_mesh_id != 0) return NIMCP_SUCCESS` as a guard against double-registration, but this check-then-act pattern is not atomic. Concurrent registration calls from different threads could race.

In practice, mesh registration typically happens at startup (single-threaded), so this is a theoretical concern rather than a practical crash risk. Listing as P3 because the systemic fix (atomic CAS or init-once guard) is desirable but not urgent.

**Affected files**: All 50+ bridge files across the 9 directories (every file matching `*_bridge.c`).

---

## Files Reviewed

### Fully Read (core implementations):
- `src/cognitive/attention/nimcp_emotion_attention.c` -- clean
- `src/cognitive/attention/nimcp_attention_fep_bridge.c` -- clean
- `src/cognitive/attention/nimcp_attention_thalamic_bridge.c` -- clean
- `src/cognitive/attention/nimcp_attention_sleep_bridge.c` -- clean
- `src/cognitive/attention/nimcp_attention_substrate_bridge.c` -- clean
- `src/cognitive/attention/nimcp_attention_plasticity_bridge.c` -- clean, good div guards
- `src/cognitive/attention/nimcp_attention_snn_bridge.c` -- clean, proper unlocked helpers
- `src/cognitive/salience/nimcp_salience.c` -- P1-2 TOCTOU
- `src/cognitive/salience/nimcp_surprise_amplifier.c` -- clean
- `src/cognitive/salience/nimcp_salience_snn_bridge.c` -- P1-4 false positive throws in hot path
- `src/cognitive/salience/nimcp_salience_fep_bridge.c` -- clean, good mutex usage
- `src/cognitive/salience/nimcp_salience_thalamic_bridge.c` -- P2-1 wrong error code, P2-2 wrong message
- `src/cognitive/salience/nimcp_salience_plasticity_bridge.c` -- P2-3 false positive throw in find_feature
- `src/cognitive/salience/nimcp_salience_substrate_bridge.c` -- clean
- `src/cognitive/salience/nimcp_surprise_snn_bridge.c` -- clean, good code quality
- `src/cognitive/working_memory/nimcp_working_memory.c` -- P1-3 data race, P2-1/P2-3 multiple
- `src/cognitive/executive/nimcp_executive.c` -- P2-1 wrong error code, P2-3 false positive
- `src/cognitive/global_workspace/nimcp_global_workspace.c` -- clean
- `src/cognitive/predictive/nimcp_predictive.c` (partial) -- P1-3 data race
- `src/cognitive/predictive/nimcp_predictive_hierarchy.c` (partial) -- P2-2 wrong func names
- `src/cognitive/meta_learning/nimcp_meta_learning.c` (partial) -- P1-3 data race, P2-1 wrong code
- `src/cognitive/curiosity/nimcp_curiosity.c` (partial) -- P2-4 capacity edge case

### Spot-checked via grep (SNN bridges div-by-zero):
- `src/cognitive/curiosity/nimcp_curiosity_snn_bridge.c:460` -- UNGUARDED (P1-1)
- `src/cognitive/executive/nimcp_executive_snn_bridge.c:466` -- UNGUARDED (P1-1)
- `src/cognitive/meta_learning/nimcp_meta_learning_snn_bridge.c:463` -- UNGUARDED (P1-1)
- `src/cognitive/predictive/nimcp_predictive_snn_bridge.c:458` -- UNGUARDED (P1-1)
- `src/cognitive/global_workspace/nimcp_gw_snn_bridge.c:461` -- UNGUARDED (P1-1)
- `src/cognitive/reasoning/nimcp_reasoning_snn_bridge.c:468` -- HAS guard (reference)

### Partially read / header-scanned (remaining bridges):
- `src/cognitive/salience/nimcp_surprise_fep_bridge.c` -- headers clean
- `src/cognitive/salience/nimcp_surprise_attention_bridge.c` -- headers clean
- `src/cognitive/salience/nimcp_surprise_gw_bridge.c` -- headers clean
- `src/cognitive/salience/nimcp_surprise_self_model_bridge.c` -- headers clean
- `src/cognitive/salience/nimcp_surprise_thalamic_bridge.c` -- headers clean
- `src/cognitive/salience/nimcp_surprise_pink_noise_bridge.c` -- headers clean
- `src/cognitive/salience/nimcp_surprise_imagination_bridge.c` -- not read (similar pattern)
- `src/cognitive/salience/nimcp_surprise_plasticity_bridge.c` -- not read (similar pattern)
- `src/cognitive/salience/nimcp_surprise_substrate_bridge.c` -- not read (similar pattern)
- `src/cognitive/reasoning/nimcp_reasoning_integration.c` (partial) -- P2-1 wrong error code
- `src/cognitive/reasoning/nimcp_reasoning_sleep_bridge.c` (partial) -- P2-1, P2-5
- `src/cognitive/executive/nimcp_executive_substrate_bridge.c` (partial) -- P2-1, P2-6
- `src/cognitive/global_workspace/nimcp_global_workspace_shannon.c` (partial) -- P2-1
- `src/cognitive/curiosity/nimcp_curiosity_enhanced.c` (partial) -- P2-1, P2-3
- `src/cognitive/predictive/nimcp_predictive_hierarchy.c` (partial) -- P2-2

### Not individually read (covered by pattern-based grep):
- Remaining reasoning bridge files (fep, factory, backward/forward chaining, unification, symbolic logic, thalamic, substrate, plasticity)
- Working memory bridge files (thalamic, sleep, substrate, fep)
- Executive bridge files (fep, thalamic, sleep, plasticity)
- Global workspace bridge files (fep, immune, substrate, thalamic, plasticity)
- Predictive bridge files (fep, substrate, thalamic, plasticity)
- Meta learning bridge files (substrate, thalamic, plasticity)
- Curiosity bridge files (fep, substrate, thalamic, plasticity, sleep)
- Curiosity sub-files (fractal, hyperbolic)

---

## Positive Observations

1. **Attention module** (all 7 files): Clean, well-structured code. Proper rwlock usage, correct unlocked helper pattern, good division guards (`bcm_activity_tau`, `dt_ms`, `spontaneous_recovery_tau`).

2. **Surprise SNN bridge**: High code quality -- proper mutex usage, clean lifecycle management, bio-async messaging outside mutex, good encoding strategy dispatch.

3. **Global workspace**: Clean competition resolution, proper content buffer ownership (malloc+copy, free on clear), refractory period treated as normal.

4. **Executive**: Good deadlock prevention (line 1420 releases lock before calling `executive_switch_task`), proper `__thread` for both MC seed and last_error, bounds check prevents uint32_t underflow.

5. **Salience FEP bridge**: Excellent documentation with biological basis references, clean mutex lock/unlock pairs, proper feature-disable short-circuit returns.

6. **Bridge base pattern**: Consistently used across all modules with `bridge_base_t` as first member, enabling uniform lifecycle management.
