# Pass 8 Walkthrough: Core Infrastructure (02)

**Date**: 2026-02-15
**Scope**: `src/core/` -- subcortical, cortical_columns, topology, neuron_models, brain_oscillations, brain_regions, axon, synapse_types, synapse_compute, neuralnet, logic, sensory, directives, medulla, geometry, dendrite, brain/*.c (excluding brain/regions/)
**Reviewer**: Claude Opus 4.6

---

## Summary

| Priority | Count |
|----------|-------|
| P1 (crash/security) | 7 |
| P2 (correctness) | 38 |
| P3 (quality) | 8 |

---

## P1 Issues (Crash / Security)

### P1-1: Deadlock in superior_colliculus -- sc_step calls sc_command_saccade while holding mutex

**File**: `src/core/brain/subcortical/nimcp_superior_colliculus.c`
**Lines**: 523, 572

`sc_step()` acquires `sc->mutex` at line 523, then calls `sc_command_saccade(sc, &target, type)` at line 572. `sc_command_saccade()` also calls `nimcp_mutex_lock(sc->mutex)` at line 449. Since the mutex is created via `nimcp_mutex_create(NULL)` (non-recursive), this is a guaranteed deadlock.

**Fix**: Create an `sc_command_saccade_unlocked()` helper that skips the lock, and call that from within `sc_step()`.

---

### P1-2: Static mutable shared across all basal_ganglia instances (data race)

**File**: `src/core/brain/subcortical/nimcp_basal_ganglia.c`
**Line**: 797

```c
static uint64_t high_conflict_count = 0;
```

This static variable is incremented and read inside `basal_ganglia_resolve_conflict()` which is called per-instance. If multiple `basal_ganglia_t` instances exist (or the same one is accessed from multiple threads), this is a data race. Additionally, all BG instances share a single counter, which is logically incorrect.

**Fix**: Move `high_conflict_count` into the `basal_ganglia_t` struct as an instance field.

---

### P1-3: Division by zero in thalamus_pulvinar_attention

**File**: `src/core/brain/subcortical/nimcp_thalamus.c`
**Line**: 957

```c
avg_attention /= (float)size;
```

The `size` parameter is not guarded against zero before this division. If called with `size == 0`, this produces a floating-point division by zero (NaN/Inf propagation).

**Fix**: Add `if (size == 0) return 0.0f;` guard before the division.

---

### P1-4: Division by zero in brain_complex_oscillation_compute_coherence_subset

**File**: `src/core/brain/oscillations/nimcp_brain_complex_oscillations.c`
**Line**: 324

```c
result->mean_amplitude = total_amplitude / num_neurons;
```

While there is a guard on `neuron_indices` being NULL, `num_neurons` is not explicitly checked for zero before use as a divisor. If `num_neurons == 0`, `nimcp_malloc(0)` returns a valid pointer on many platforms but the division produces undefined behavior. The malloc of zero bytes itself is also problematic.

**Fix**: Add `if (num_neurons == 0) { return false; }` guard at function entry.

---

### P1-5: Division by zero in brain_region_organize_columns -- neurons_per_column

**File**: `src/core/brain_regions/nimcp_brain_regions.c`
**Line**: 488

```c
uint32_t neurons_per_column = region->total_neurons / num_columns;
```

While `num_columns` is guarded against zero (line 473 checks `columns_x == 0 || columns_y == 0`), if `region->total_neurons` is 0, the division produces 0 (which is safe for unsigned), but subsequent logic at line 503 computes `col_idx * neurons_per_column` which would always be 0, meaning all columns reference the same neuron offset. Similarly at line 506, `region->layer_sizes[layer] / num_columns` could produce 0, leading to empty columns with allocated but uninitialized arrays. This is a logical correctness issue rather than a crash, but worth flagging.

**Reclassified**: P2 (logical correctness, not a crash).

---

### P1-5 (revised): Static mutable random_state in kg_algorithms (data race)

**File**: `src/core/brain/nimcp_kg_algorithms.c`
**Line**: 368

```c
static uint32_t random_state = KG_ALGO_RANDOM_SEED;
```

This static mutable is used across all callers for random sampling in graph algorithms. Multiple threads calling KG algorithm functions simultaneously will have data races on this variable.

**Fix**: Use `_Thread_local` or pass RNG state through function parameters.

---

### P1-6: Static mutable rng_state in hypothalamus drive quantum bridge (data race)

**File**: `src/core/brain/regions/hypothalamus/nimcp_hypothalamus_drive_quantum_bridge.c`
**Line**: 829

```c
static uint32_t rng_state = 12345;
```

Same pattern as P1-5 -- static mutable RNG state shared across threads without synchronization.

**Fix**: Use `_Thread_local` or `nimcp_tl_rand()`.

---

### P1-7: Static mutable last_mode in lc_adapter (data race)

**File**: `src/core/brain/regions/locus_coeruleus/nimcp_lc_adapter.c`
**Line**: 518

```c
static nimcp_lc_mode_t last_mode = LC_MODE_TONIC;
```

Static mutable state tracking the last mode. If the locus coeruleus adapter is called from multiple threads, this is a data race.

**Fix**: Move into the LC adapter instance struct or use `_Thread_local`.

---

## P2 Issues (Correctness)

### P2-1: Wrong function names in NIMCP_THROW_TO_IMMUNE messages (basal_ganglia.c)

**File**: `src/core/brain/subcortical/nimcp_basal_ganglia.c`

Multiple functions have wrong function names in their throw messages:
- Line 675: `"basal_ganglia_reset"` in `basal_ganglia_set_action_value`
- Lines 878, 941, 987, 1024: `"basal_ganglia_suppress_action"` in other functions
- Line 1100: `"basal_ganglia_get_rpe"` in `basal_ganglia_register_habit`
- Line 1143: `"basal_ganglia_get_rpe"` in `basal_ganglia_strengthen_habit_unlocked`
- Line 1516: `"basal_ganglia_is_bio_async_connected"` in `basal_ganglia_get_stats`

---

### P2-2: Wrong function names in striatum.c

**File**: `src/core/brain/subcortical/nimcp_striatum.c`

- Lines 98, 106: `"sigmoid"` in `init_pathway` error messages
- Line 293: `"striatum_reset"` in `striatum_process_input`
- Line 425: `"striatum_step"` in `striatum_update_weights`

---

### P2-3: Wrong function names in superior_colliculus.c

**File**: `src/core/brain/subcortical/nimcp_superior_colliculus.c`

- Lines 246, 250, 289, 327, 331, 348, 445, 599: Multiple wrong function names
- Line 637: `"sc_get_stats: validation failed"` in `sc_get_corollary_discharge`

---

### P2-4: Wrong error code in superior_colliculus.c

**File**: `src/core/brain/subcortical/nimcp_superior_colliculus.c`
**Lines**: 386, 404

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...)
```

Used when `saccade_ready` is false, which is an invalid state condition, not a null pointer. Should be `NIMCP_ERROR_INVALID_STATE`.

---

### P2-5: Wrong error code in basal_ganglia.c cleanup

**File**: `src/core/brain/subcortical/nimcp_basal_ganglia.c`
**Line**: 601

Throws `NIMCP_ERROR_NULL_POINTER` on memory allocation failure in cleanup path. Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P2-6: False positive THROW_TO_IMMUNE in bg_vigor.c find functions

**File**: `src/core/brain/subcortical/nimcp_bg_vigor.c`
**Lines**: 76, 95

`bgv_find_action()` and `bgv_find_action_const()` throw `NIMCP_THROW_TO_IMMUNE` on normal "not found" path. This is a search/lookup function where not finding an item is normal behavior, not an error.

**Fix**: Remove the throw or change to `NIMCP_LOGGING_DEBUG`.

---

### P2-7: nac_trigger_dopamine_pause ignores magnitude parameter

**File**: `src/core/brain/subcortical/nimcp_nucleus_accumbens.c`
**Line**: 298

The `magnitude` parameter is accepted but never used in the function body. The dopamine pause effect is hardcoded instead of being scaled by `magnitude`.

---

### P2-8: Wrong function names in bg_outcome_devaluation.c

**File**: `src/core/brain/subcortical/nimcp_bg_outcome_devaluation.c`

Multiple functions use `"bgod_reset"` as the function name in throw messages even though they are in different functions:
- Line 239: in `bgod_register_outcome`
- Line 247: in `bgod_register_outcome` (capacity check)
- Line 270: in `bgod_set_outcome_value`
- Line 292: in `bgod_register_association`
- Line 296: in `bgod_register_association`
- Line 300: in `bgod_register_association`

---

### P2-9: Wrong function names in bg_model_based.c

**File**: `src/core/brain/subcortical/nimcp_bg_model_based.c`

Multiple functions use wrong function names in throw messages:
- Line 350: `"bg_mb_reset"` in `bg_mb_update_transition`
- Line 390: `"bg_mb_reset"` in `bg_mb_update_reward`
- Line 458: `"bg_mb_reset"` in `bg_mb_plan`
- Line 462: `"bg_mb_reset"` in `bg_mb_plan`
- Line 516: `"unknown"` in `bg_mb_simulate_trajectory`
- Line 520: `"unknown"` in `bg_mb_simulate_trajectory`
- Line 626: `"bg_mb_get_state_value"` in `bg_mb_arbitrate`
- Line 650: `"bg_mb_get_state_value"` in `bg_mb_update_arbitration`
- Line 689: `"bg_mb_get_state_value"` in `bg_mb_get_arbitration`
- Line 699: `"bg_mb_get_state_value"` in `bg_mb_set_arbitration_mode`

---

### P2-10: Wrong function names in bg_cerebellar_coord.c

**File**: `src/core/brain/subcortical/nimcp_bg_cerebellar_coord.c`

Multiple functions use wrong function names in throw messages:
- Line 280: `"bgcb_reset"` in `bgcb_receive_bg_command`
- Line 306: `"bgcb_reset"` in `bgcb_receive_cb_refinement`
- Line 419: `"bgcb_coordinate"` in `bgcb_get_motor_output`
- Line 456: `"bgcb_trigger_handoff"` in `bgcb_get_shared_error`
- Line 470: `"bgcb_trigger_handoff"` in `bgcb_set_timing_prediction`
- Line 487: `"bgcb_trigger_handoff"` in `bgcb_report_actual_timing`
- Line 512: `"bgcb_trigger_handoff"` in `bgcb_get_timing_state`
- Line 620: `"bgcb_get_stats"` in `bgcb_update_learning`

---

### P2-11: Wrong function names in bg_beta_oscillations.c

**File**: `src/core/brain/subcortical/nimcp_bg_beta_oscillations.c`

- Line 520: `"bg_beta_apply_dopamine"` in `bg_beta_signal_movement_intent`
- Lines 554, 558, 573: `"bg_beta_signal_movement_complete"` in `bg_beta_set_pathology` and `bg_beta_get_pathology`

---

### P2-12: Wrong function names in bg_enhanced.c

**File**: `src/core/brain/subcortical/nimcp_basal_ganglia_enhanced.c`

- Line 356: `"bg_enhanced_get_training_bridge"` in `bg_enhanced_select_action`
- Line 449: `"bg_enhanced_get_training_bridge"` in `bg_enhanced_select_action_for_goal`
- Line 487: `"bg_enhanced_start_option"` mentions `bge->hrl` in check but uses wrong func name
- Line 593: `"bg_enhanced_get_liking"` in `bg_enhanced_plan_to_goal`

---

### P2-13: Wrong error code in bg_beta_oscillations.c create

**File**: `src/core/brain/subcortical/nimcp_bg_beta_oscillations.c`
**Line**: 185

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");
```

This is thrown on allocation failure (calloc returned NULL). Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P2-14: Wrong error code in bg_cerebellar_coord.c create cleanup

**File**: `src/core/brain/subcortical/nimcp_bg_cerebellar_coord.c`
**Lines**: 169, 189

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_create: operation failed");
```

These are thrown on allocation failure paths. Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P2-15: Wrong error code in bg_model_based.c create cleanup

**File**: `src/core/brain/subcortical/nimcp_bg_model_based.c`
**Line**: 230

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_mb_create: operation failed");
```

Thrown on allocation failure. Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P2-16: Wrong error code in bg_outcome_devaluation.c create cleanup

**File**: `src/core/brain/subcortical/nimcp_bg_outcome_devaluation.c`
**Line**: 171

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgod_create: operation failed");
```

Thrown on allocation failure. Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P2-17: False positive THROW_TO_IMMUNE in executive bridge find_goal

**File**: `src/core/brain/subcortical/nimcp_basal_ganglia_executive_bridge.c`
**Line**: 72

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_goal: validation failed");
return NULL;
```

`find_goal()` is a lookup function. Not finding a goal by ID is a normal "not found" condition, not a null pointer error. This will throw on every failed lookup.

---

### P2-18: False positive THROW_TO_IMMUNE in brain_regions_immune_bridge find_sensitivity

**File**: `src/core/brain_regions/nimcp_brain_regions_immune_bridge.c`
**Line**: 265

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_sensitivity: validation failed");
```

Same pattern -- lookup function throwing on normal not-found path.

---

### P2-19: False positive THROW_TO_IMMUNE in cortical_plasticity_bridge find_column_state

**File**: `src/core/cortical_columns/nimcp_cortical_plasticity_bridge.c`
**Line**: 46

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_column_state: validation failed");
```

Same pattern -- lookup function throwing on normal not-found path.

---

### P2-20: False positive THROW_TO_IMMUNE in sparse_synapse find_size_class

**File**: `src/core/neuralnet/nimcp_sparse_synapse.c`
**Line**: 120

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_size_class: validation failed");
```

Same pattern -- internal lookup function throwing on normal not-found path.

---

### P2-21: False positive THROW_TO_IMMUNE in kg_federation find_peer

**File**: `src/core/brain/nimcp_kg_federation.c`
**Line**: 122

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_peer: validation failed");
```

Same pattern -- lookup function throwing on normal not-found path.

---

### P2-22: False positive THROW_TO_IMMUNE in kg_federation find_conflict

**File**: `src/core/brain/nimcp_kg_federation.c`
**Line**: 139

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_conflict: operation failed");
```

Same pattern.

---

### P2-23: False positive THROW_TO_IMMUNE in kg_metadata find_entry

**File**: `src/core/brain/nimcp_kg_metadata.c`
**Line**: 110

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_entry: validation failed");
```

Same pattern -- metadata entry lookup, not finding an entry is normal.

---

### P2-24: False positive THROW_TO_IMMUNE in kg_algorithms find_node_index

**File**: `src/core/brain/nimcp_kg_algorithms.c`
**Line**: 298

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_node_index: validation failed");
```

Same pattern -- graph node lookup, not finding a node is normal.

---

### P2-25: False positive THROW_TO_IMMUNE in kg_disaster_recovery find_replica

**File**: `src/core/brain/nimcp_kg_disaster_recovery.c`
**Line**: 188

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_replica: validation failed");
```

Same pattern.

---

### P2-26: Wrong error code in logic_batch_result_create

**File**: `src/core/logic/nimcp_neural_logic_evaluation.c`
**Line**: 153

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_batch_result_create: num_gates is zero");
```

The error is that `num_gates == 0`, not a memory error. Should be `NIMCP_ERROR_INVALID_PARAM`.

---

### P2-27: Wrong error code in logic_batch_result_create allocation failure

**File**: `src/core/logic/nimcp_neural_logic_evaluation.c`
**Line**: 161

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");
```

Thrown on allocation failure. Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P2-28: Wrong error code in surface_geometry_create mutex init failure

**File**: `src/core/geometry/nimcp_surface_geometry.c`
**Line**: 131-132

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, ...);
nimcp_free(ctx);
```

After the throw, `ctx->mutex` is freed but `ctx` itself leaks. The `nimcp_free(ctx)` only frees `ctx` but not `ctx->mutex` which was already allocated at line 124.

**Fix**: Add `nimcp_free(ctx->mutex);` before `nimcp_free(ctx);`.

---

### P2-29: Wrong function name in kg_find_path_safe

**File**: `src/core/brain/nimcp_brain_kg_helpers.c`
**Line**: 368

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_find_path_safe: kg_has_node is NULL");
```

The message says `kg_has_node is NULL` but `kg_has_node` is a function call returning a bool, not a pointer. The actual condition is `!kg_has_node(ctx) || target == BRAIN_KG_INVALID_NODE`. The message is misleading.

---

### P2-30: Missing NULL check for amygdala_attention_create mutex allocation

**File**: `src/core/brain/subcortical/nimcp_amygdala_attention_bridge.c`
**Lines**: 120-127

The mutex is allocated via `nimcp_malloc(sizeof(nimcp_mutex_t))` then initialized with `nimcp_mutex_init()`. However, if `nimcp_mutex_init()` fails, there is no error check -- the function proceeds with an uninitialized mutex.

**Fix**: Check the return value of `nimcp_mutex_init()`.

---

### P2-31: brain_region_organize_columns does not guard region->total_neurons == 0

**File**: `src/core/brain_regions/nimcp_brain_regions.c`
**Line**: 488

```c
uint32_t neurons_per_column = region->total_neurons / num_columns;
```

If `region->total_neurons` is 0 (e.g., if the region was created with 0 neurons), `neurons_per_column` is 0 and all columns will have no neurons. The function does not return an error in this case, leading to empty, useless columns being created.

---

### P2-32: THROW after free in basal_ganglia.c cleanup paths

**File**: `src/core/brain/subcortical/nimcp_basal_ganglia.c`
**Lines**: 520, 534

Pattern: `nimcp_free(bg); NIMCP_THROW_TO_IMMUNE(...)`. The throw happens after the bg struct is freed. While the throw itself does not reference bg, the immune system's error handler may attempt to inspect the module state. This is misleading at minimum.

---

### P2-33: Wrong error code NIMCP_ERROR_BUFFER_OVERFLOW for enum range check

**File**: `src/core/brain/subcortical/nimcp_bg_beta_oscillations.c`
**Line**: 558

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "bg_beta_signal_movement_complete: capacity exceeded");
```

The check is `if (pathology >= BG_PATHOLOGY_COUNT)` -- an enum range validation. Should be `NIMCP_ERROR_OUT_OF_RANGE` or `NIMCP_ERROR_INVALID_PARAM`, not `NIMCP_ERROR_BUFFER_OVERFLOW`.

---

### P2-34: bg_beta_is_movement_blocked throws on NULL (const getter)

**File**: `src/core/brain/subcortical/nimcp_bg_beta_oscillations.c`
**Line**: 652

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bg_beta_is_movement_blocked: system is NULL");
return false;
```

This is a const getter function that returns `bool`. Other const getters in the same file (e.g., `bg_beta_get_state`, `bg_beta_get_power`, `bg_beta_get_coherence`) simply return a default value without throwing. This function should follow the same pattern for consistency, or all should throw -- currently inconsistent.

---

### P2-35: Wrong error code in bg_beta_oscillations.c step

**File**: `src/core/brain/subcortical/nimcp_bg_beta_oscillations.c`
**Line**: 309

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bg_beta_step: system is NULL");
```

The condition also checks `dt_ms <= 0`. When `system` is valid but `dt_ms <= 0`, the error message "system is NULL" is wrong. Should differentiate between the two conditions.

---

### P2-36: bg_enhanced_select_action uses stack array with BG_MAX_ACTIONS

**File**: `src/core/brain/subcortical/nimcp_basal_ganglia_enhanced.c`
**Line**: 363

```c
float modified_input[BG_MAX_ACTIONS];
```

Then at line 366:
```c
memcpy(modified_input, cortical_input, num_actions * sizeof(float));
```

If `num_actions > BG_MAX_ACTIONS`, this would be a stack buffer overflow. The `num_actions` comes from `bge->config.core_config.num_actions` which is configurable. There is no check that `num_actions <= BG_MAX_ACTIONS` before the memcpy.

**Severity note**: This is arguably P1 if `num_actions` can exceed `BG_MAX_ACTIONS`, but the default config uses `num_actions = 16` and `BG_MAX_ACTIONS` is likely >= 64. Adding a guard is still recommended.

---

### P2-37: bg_enhanced_get_action_value reads thalamic_out without bounds check

**File**: `src/core/brain/subcortical/nimcp_basal_ganglia_enhanced.c`
**Lines**: 469-471

```c
float thalamic_out[BG_MAX_ACTIONS];
basal_ganglia_get_thalamic_output(bge->core, thalamic_out);
value = thalamic_out[action_id];
```

There is no check that `action_id < BG_MAX_ACTIONS`. If `action_id >= BG_MAX_ACTIONS`, this is a stack buffer read out of bounds.

---

### P2-38: Memory leak in surface_geometry_create on mutex init failure

**File**: `src/core/geometry/nimcp_surface_geometry.c`
**Lines**: 130-134

```c
if (nimcp_mutex_init(ctx->mutex, NULL) != 0) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "...");
    nimcp_free(ctx);       // ctx->mutex is leaked!
    return NULL;
}
```

When `nimcp_mutex_init` fails, `ctx->mutex` was allocated at line 124 but is not freed before `nimcp_free(ctx)`. The `nimcp_free(ctx)` only frees the ctx struct; the separately allocated mutex pointer leaks.

---

## P3 Issues (Quality)

### P3-1: Redundant clamp_f helper defined in multiple subcortical files

**Files**: Multiple subcortical files including `nimcp_bg_cerebellar_coord.c`, `nimcp_bg_model_based.c`, `nimcp_bg_outcome_devaluation.c`, `nimcp_bg_beta_oscillations.c`, `nimcp_axon.c`

Each file defines its own identical `static float clamp_f(float val, float min_val, float max_val)` or `static float clamp(float v, float min, float max)` helper. This should be a shared utility function.

---

### P3-2: Redundant clamp helper in bridge files

**Files**: `nimcp_basal_ganglia_amygdala_bridge.c`, `nimcp_basal_ganglia_thalamus_bridge.c`, `nimcp_basal_ganglia_executive_bridge.c`, `nimcp_basal_ganglia_fep_bridge.c`, `nimcp_amygdala_fep_bridge.c`

Same `static float clamp()` helper duplicated in every bridge file.

---

### P3-3: Unused parameter in nac_trigger_dopamine_pause

**File**: `src/core/brain/subcortical/nimcp_nucleus_accumbens.c`
**Line**: 298

The `magnitude` parameter is declared but not used in the function body. Should be either used or documented as reserved with `(void)magnitude`.

---

### P3-4: brain_processing.c and brain_core.c are mostly empty stubs

**Files**: `src/core/brain/nimcp_brain_processing.c`, `src/core/brain/nimcp_brain_core.c`

These files contain mesh registration boilerplate and forward declarations / extern references to functions that remain in `nimcp_brain.c`. The actual implementations have not been migrated. This is dead organizational structure.

---

### P3-5: Multiple lerp_f definitions

**File**: `src/core/brain/subcortical/nimcp_bg_beta_oscillations.c`
**Line**: 122

Defines `static float lerp_f()` locally. This is another utility function that could be shared.

---

### P3-6: Inconsistent NULL-check error handling in const getters

**Files**: Multiple subcortical files

Some const getter functions throw `NIMCP_THROW_TO_IMMUNE` on NULL input while others silently return default values. Within the same file (`nimcp_bg_beta_oscillations.c`), `bg_beta_get_state()` returns a default, while `bg_beta_is_movement_blocked()` throws. This inconsistency makes the API unpredictable.

---

### P3-7: Dead code -- unused bio_ctx process_inbox calls in hot paths

**File**: `src/core/neuron_models/nimcp_izhikevich.c`
**Line**: 340-342

```c
if (bio_ctx) {
    bio_router_process_inbox(bio_ctx, 5);
}
```

Processing bio-async messages inside `izhikevich_update()` (called per-neuron per-timestep) is extremely expensive and likely unnecessary. This should be moved to a higher-level update loop.

---

### P3-8: fractal_topology.c defines partial struct for neural_network internals

**File**: `src/core/topology/nimcp_fractal_topology.c`
**Lines**: 82-87

```c
struct neural_network_struct {
    void* neurons;
    uint32_t num_neurons;
    uint32_t capacity;
};
```

Redefining an opaque struct's internals violates encapsulation and is fragile -- any change to the actual struct layout will silently break this code. Should use a proper accessor function instead.

---

## Systemic Patterns

### Pattern A: Wrong function names in NIMCP_THROW_TO_IMMUNE messages

This is the most common P2 issue across the codebase. Nearly every subcortical file has throw messages with function names that do not match the enclosing function. This appears to be caused by copy-paste during development. Files affected:
- `nimcp_basal_ganglia.c` (~10 instances)
- `nimcp_striatum.c` (4 instances)
- `nimcp_superior_colliculus.c` (~10 instances)
- `nimcp_bg_model_based.c` (~10 instances)
- `nimcp_bg_cerebellar_coord.c` (~8 instances)
- `nimcp_bg_outcome_devaluation.c` (~6 instances)
- `nimcp_bg_beta_oscillations.c` (~4 instances)
- `nimcp_basal_ganglia_enhanced.c` (~4 instances)

**Estimated total**: ~55+ instances across subcortical files alone.

### Pattern B: False positive NIMCP_THROW_TO_IMMUNE in find/lookup functions

Internal `find_*` helper functions that search arrays/lists throw `NIMCP_THROW_TO_IMMUNE` when an item is not found. This is normal behavior, not an error. Files affected:
- `nimcp_bg_vigor.c`: `bgv_find_action`, `bgv_find_action_const`
- `nimcp_basal_ganglia_executive_bridge.c`: `find_goal`
- `nimcp_brain_regions_immune_bridge.c`: `find_sensitivity`
- `nimcp_cortical_plasticity_bridge.c`: `find_column_state`
- `nimcp_sparse_synapse.c`: `find_size_class`
- `nimcp_kg_federation.c`: `find_peer`, `find_conflict`
- `nimcp_kg_metadata.c`: `find_entry`
- `nimcp_kg_algorithms.c`: `find_node_index`
- `nimcp_kg_disaster_recovery.c`: `find_replica`

**Estimated total**: ~12 instances.

### Pattern C: Wrong error codes on allocation failure

Multiple files throw `NIMCP_ERROR_NULL_POINTER` instead of `NIMCP_ERROR_NO_MEMORY` when `nimcp_calloc` returns NULL. The NULL pointer is the symptom; the root cause is out-of-memory.

---

## Files Reviewed

### Fully reviewed (all lines read):
- `nimcp_basal_ganglia.c` (1548 lines)
- `nimcp_amygdala.c` (1206 lines)
- `nimcp_thalamus.c` (1093 lines)
- `nimcp_striatum.c` (474 lines)
- `nimcp_bg_vigor.c` (699 lines)
- `nimcp_nucleus_accumbens.c` (382 lines)
- `nimcp_superior_colliculus.c` (655 lines)
- `nimcp_subthalamic.c` (549 lines)
- `nimcp_substantia_nigra.c` (548 lines)
- `nimcp_globus_pallidus.c` (396 lines)
- `nimcp_basal_ganglia_enhanced.c` (600 lines)
- `nimcp_bg_cerebellar_coord.c` (657 lines)
- `nimcp_bg_model_based.c` (700 lines)
- `nimcp_bg_outcome_devaluation.c` (300 lines)
- `nimcp_bg_beta_oscillations.c` (699 lines)

### Partially reviewed (200+ lines read):
- `nimcp_basal_ganglia_amygdala_bridge.c`
- `nimcp_basal_ganglia_training_bridge.c`
- `nimcp_basal_ganglia_thalamus_bridge.c`
- `nimcp_basal_ganglia_executive_bridge.c`
- `nimcp_basal_ganglia_fep_bridge.c`
- `nimcp_amygdala_attention_bridge.c`
- `nimcp_amygdala_fep_bridge.c`
- `nimcp_cortical_column.c`
- `nimcp_cortical_hierarchy.c`
- `nimcp_neuralnet.c`
- `nimcp_neuralnet_backprop.c`
- `nimcp_neuralnet_learning.c`
- `nimcp_izhikevich.c`
- `nimcp_brain_oscillations.c`
- `nimcp_brain_regions.c`
- `nimcp_axon.c`
- `nimcp_synapse_types.c`
- `nimcp_dendrite.c`
- `nimcp_neural_logic_evaluation.c`
- `nimcp_core_directives.c`
- `nimcp_medulla.c`
- `nimcp_circadian.c`
- `nimcp_arousal_state.c`
- `nimcp_surface_geometry.c`
- `nimcp_fractal_topology.c`
- `nimcp_network_builder.c`
- `nimcp_brain_core.c`
- `nimcp_brain_processing.c`
- `nimcp_brain_lifecycle.c`
- `nimcp_brain_complex_oscillations.c`

### Scanned via grep (pattern-based review):
- All remaining `src/core/**/*.c` files for division-by-zero, false positive throws, static mutable state, and find/lookup throw patterns
