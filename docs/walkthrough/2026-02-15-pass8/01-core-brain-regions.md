# Pass 8 Review: Core Brain Regions

**Scope**: `src/core/brain/regions/` (all subdirectories)
**Files reviewed**: 206 C source files (~180,820 lines total)
**Date**: 2026-02-15
**Reviewer**: Claude Opus 4.6

## Summary

| Priority | Count | Categories |
|----------|-------|------------|
| P1 | 6 | Division by zero (3), config state corruption (2), memory leak on partial alloc failure (1) |
| P2 | 9 | Wrong error codes (3 systemic), wrong function names in errors (2 systemic), false positive THROW (1 systemic), memory leak (1), permanent config mutation (1), data race in timestamp counters (1 systemic) |
| P3 | 2 | Wrong function name copy-paste (1 systemic), unguarded global static mutable variables (1 systemic) |

**Systemic issues** (affect many files) are counted once each but the file counts are noted below.

---

## P1 Issues

### 1. [hippocampus/nimcp_hippocampus.c:481,1567] Config state corruption - learning_rate permanently multiplied every update cycle

In `hippo_update()` (line 481):
```c
if (hippo->hypothalamus_bridge.initialized) {
    float stress_mod = 1.0f - hippo->hypothalamus_bridge.stress_level * 0.3f;
    hippo->config.default_learning_rate *= stress_mod;
}
```

And in `hippo_process_incoming()` (line 1567):
```c
float stress_mod = (stress < 0.5f) ? (1.0f + stress) : (2.0f - stress);
hippo->config.default_learning_rate *= stress_mod;
```

**Bug**: The config's `default_learning_rate` is **permanently modified** each call. After N updates, it drifts exponentially toward 0 (or infinity depending on stress_mod). Since `hippo_bidirectional_update()` calls both `hippo_process_incoming()` and `hippo_update()`, the rate is modified **twice per cycle**. Should use a local `effective_rate` variable instead of mutating the config.

### 2. [hippocampus/nimcp_hippocampus.c:1634] Config state corruption - pattern_completion_threshold permanently overwritten

```c
if (hippo->dragonfly_bridge.initialized && hippo->dragonfly_bridge.rapid_retrieval_mode) {
    hippo->config.pattern_completion_threshold = 0.2f;
}
```

**Bug**: Once dragonfly rapid retrieval mode is entered, `pattern_completion_threshold` is permanently set to 0.2f, even when dragonfly leaves rapid retrieval mode. The original config value (0.3f default) is lost.

### 3. [hippocampus/nimcp_hippocampus.c:1608,1690] Division by zero - num_subiculum_cells

In `hippo_send_outgoing()` (line 1608):
```c
hippo->mammillary_bridge.fornix_output_strength =
    fornix_signal / hippo->num_subiculum_cells;
```

And in `hippo_sync_mammillary()` (line 1690):
```c
hippo->mammillary_bridge.fornix_output_strength = output / hippo->num_subiculum_cells;
```

**Bug**: If `num_subiculum_cells` is 0 (a valid config), this is a floating-point division by zero resulting in inf/NaN. Should guard with `(hippo->num_subiculum_cells > 0) ?` ternary.

### 4. [hippocampus/nimcp_hippocampus.c:101] Division by zero - gaussian_activation with sigma=0

```c
static float gaussian_activation(float distance, float sigma) {
    return expf(-(distance * distance) / (2.0f * sigma * sigma));
}
```

Called at lines 450 and 1009 with `hippo->place_cells[i].place_field_radius` as sigma. If a place cell has `place_field_radius == 0.0f` (which is the default for uninitialized cells, or if `hippo_create_place_field` is called with radius=0), this divides by zero, producing inf which `expf()` maps to 0 or inf depending on sign.

Same pattern in:
- `parahippocampal/nimcp_parahippocampal.c:90` - `gaussian()` with sigma parameter
- `mammillary/nimcp_mammillary.c:81` - `compute_hd_tuning()` with width_deg parameter (if width_deg=0, width_rad=0, div by 0)

### 5. [parietal/nimcp_parietal_quantum_bridge.c:1001] Division by zero - total_prob can be 0

```c
*probability = amplitude_to_prob(
    bridge->walk_states[selected].amplitude_real,
    bridge->walk_states[selected].amplitude_imag) / total_prob;
```

**Bug**: `total_prob` is computed as sum of amplitude probabilities (line 976-981). If all amplitudes are zero (e.g., after quantum decoherence or initialization error), `total_prob == 0.0f` causing division by zero. The guard at line 976 initializes to 0 but never checks before dividing.

### 6. [hippocampus/nimcp_hippocampus.c:548-570] Memory leak on partial allocation failure in hippo_encode_episode

```c
ep->what_content = nimcp_malloc(what_dim * sizeof(float));  // line 535 - allocated
...
ep->where_content = nimcp_malloc(where_dim * sizeof(float)); // line 547
if (!ep->where_content) {
    // what_content is NOT freed here
    return -1;  // LEAK: what_content lost
}
...
ep->when_content = nimcp_malloc(when_dim * sizeof(float));  // line 565
if (!ep->when_content) {
    // what_content AND where_content NOT freed here
    return -1;  // LEAK: what_content + where_content lost
}
```

**Bug**: When later allocations fail, earlier successful allocations for the same episode are leaked. The episode slot was obtained at line 528 (`hippo->episodes[id]`) but `num_episodes` is not incremented until line 653 (after all allocations succeed), so `hippo_destroy` will not iterate over this episode to free its contents.

---

## P2 Issues

### 7. [SYSTEMIC: ~30+ files] Wrong error code - NIMCP_ERROR_NULL_POINTER for non-null-pointer conditions

Files affected include at minimum:
- `locus_coeruleus/nimcp_locus_coeruleus.c:321` - "capacity exceeded" uses NIMCP_ERROR_NULL_POINTER (should be NIMCP_ERROR_OUT_OF_RANGE)
- `locus_coeruleus/nimcp_locus_coeruleus.c:346` - search not-found uses NIMCP_ERROR_NULL_POINTER
- `parietal/nimcp_parietal_quantum_bridge.c:917,968` - `walk_initialized` is a `bool`, not a pointer, yet error says "is NULL" and uses NIMCP_ERROR_NULL_POINTER (should be NIMCP_ERROR_INVALID_STATE)
- `hippocampus/nimcp_hippocampus.c:734` - "retrieval failed" (no matching episode) uses NIMCP_ERROR_INVALID_PARAM
- `hippocampus/nimcp_hippocampus.c:1095` - "No available cells" uses NIMCP_ERROR_INVALID_PARAM (should be NIMCP_ERROR_OUT_OF_RANGE)
- `entorhinal/nimcp_entorhinal.c:1185` - "capacity exceeded" uses NIMCP_ERROR_NULL_POINTER (should be NIMCP_ERROR_OUT_OF_RANGE)
- All `find_subscription`/`find_synapse` functions in bio_async bridges: return NIMCP_ERROR_NULL_POINTER for "not found"

**Pattern**: Throughout brain regions, NIMCP_ERROR_NULL_POINTER is used as a catch-all error code, even when the actual problem is out-of-range, invalid state, or a normal not-found condition.

### 8. [SYSTEMIC: 34 files, 245 occurrences] Wrong function name in THROW_TO_IMMUNE - "unknown:" prefix

245 instances across 34 files use `"unknown: ..."` as the function name in THROW_TO_IMMUNE error messages. Examples:
- `parietal/nimcp_parietal_quantum_bridge.c` (16 occurrences)
- `prefrontal/nimcp_prefrontal_adapter.c` (25 occurrences)
- `entorhinal/nimcp_entorhinal.c` (25 occurrences)
- `hypothalamus/nimcp_hypothalamus_drive_quantum_bridge.c` (11 occurrences)

These messages provide no diagnostic value since the function name is "unknown".

### 9. [SYSTEMIC: multiple files] Copy-paste wrong function names in THROW_TO_IMMUNE

Many functions use the wrong function name copied from a neighboring function:
- `hippocampus/nimcp_hippocampus.c:505,515,521,538,550,568` - `hippo_encode_episode` uses "hippo_update:" in error messages
- `hippocampus/nimcp_hippocampus.c:808,853,873,897,926,967` - multiple functions use "hippo_forget_episode:" in error messages
- `hippocampus/nimcp_hippocampus.c:1023,1044,1078,1095,1103,1123` - multiple functions use "hippo_update_position:" in error messages
- `hippocampus/nimcp_hippocampus.c:1356,1371,1386,1401,1415` - multiple functions use "hippo_get_consolidation_level:" in error messages

### 10. [SYSTEMIC: 12 files] Data race in static timestamp counters

12 files use `static uint64_t counter = 0; return counter++;` as a timestamp implementation without any atomic or mutex protection:
- `somatosensory/nimcp_somatosensory.c:71`
- `somatosensory/bridges/nimcp_soma_medulla_bridge.c:92`
- `locus_coeruleus/nimcp_lc_plasticity_bridge.c:100`
- `locus_coeruleus/nimcp_lc_snn_bridge.c:99`
- `gustatory/bridges/nimcp_gust_bio_async_bridge.c:139`
- `gustatory/nimcp_gustatory.c:57`
- `wernicke/nimcp_wernicke_broca_bridge.c:221`
- `vta/nimcp_vta_snn_bridge.c:89`
- `olfactory/nimcp_olfactory.c:57`
- `sensory_integration/nimcp_sensory_swarm_bridge.c:98`
- `sensory_integration/nimcp_chemosensory_bridge.c:93`
- `retrosplenial/nimcp_retrosplenial.c:172`

If called from multiple threads, the increment is not atomic. While these are used only for ordering/stats (not security-critical), concurrent increments could yield duplicate timestamps or lost increments.

### 11. [locus_coeruleus/nimcp_locus_coeruleus.c:346] False positive THROW_TO_IMMUNE on normal search-not-found path

```c
nimcp_lc_projection_t* nimcp_lc_get_projection_by_target(
    nimcp_lc_system_t* lc, nimcp_lc_target_t target) {
    ...
    for (uint32_t i = 0; i < lc->num_projections; i++) {
        if (lc->projections[i].target == target) {
            return &lc->projections[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "nimcp_lc_get_projection_by_target: validation failed");
    return NULL;
}
```

**Bug**: Not finding a projection for a given target is a normal "not found" result, not an error worth reporting to the immune system. This is a false positive throw that fires on every unsuccessful lookup.

Same pattern in:
- `raphe/nimcp_raphe.c:627` - `nimcp_raphe_get_projection` not-found path
- `somatosensory/bridges/nimcp_soma_bio_async_bridge.c:99` - `find_subscription` not-found
- Multiple `find_subscription` functions across bio_async bridges

### 12. [hippocampus/nimcp_hippocampus.c:734] False positive THROW_TO_IMMUNE on normal retrieval miss

```c
if (best_id == UINT32_MAX) {
    hippo->last_error = HIPPO_ERROR_RETRIEVAL_FAILED;
    hippo->status = HIPPO_STATUS_READY;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
    return -1;
}
```

**Bug**: Not finding a matching episode is a normal result when querying with a novel cue. Throwing to the immune system for a normal retrieval miss is a false positive.

### 13. [hippocampus/nimcp_hippocampus.c:1588] Permanent theta_power degradation

```c
if (hippo->cognitive_bridge.initialized) {
    hippo->theta_power *= (0.5f + 0.5f * hippo->cognitive_bridge.attention_level);
}
```

Called from `hippo_process_incoming()` which is called every update cycle. If attention_level < 1.0, theta_power is permanently reduced each cycle (multiplied by a value < 1.0). Over time, theta_power decays to 0. Similarly at line 486:
```c
if (hippo->immune_bridge.initialized && hippo->immune_bridge.neuroinflammation) {
    hippo->theta_power *= 0.9f;
}
```

---

## P3 Issues

### 14. [SYSTEMIC: ~50+ functions in hippocampus] Copy-pasted function names in error messages

Throughout `hippocampus/nimcp_hippocampus.c`, error messages reference the wrong function name due to copy-paste. While the code still works, these are misleading for debugging:
- `hippo_find_similar_episodes` says "hippo_forget_episode:"
- `hippo_get_episodes_by_type` says "hippo_forget_episode:"
- `hippo_get_recent_episodes` says "hippo_forget_episode:"
- `hippo_pattern_separate` says "hippo_forget_episode:"
- `hippo_pattern_complete` says "hippo_forget_episode:"
- `hippo_assess_novelty` says "hippo_forget_episode:"
- `hippo_get_dg_pattern` says "hippo_get_consolidation_level:"
- `hippo_get_ca3_pattern` says "hippo_get_consolidation_level:"
- `hippo_get_ca1_pattern` says "hippo_get_consolidation_level:"
- `hippo_get_subiculum_output` says "hippo_get_consolidation_level:"
- `hippo_activate_dg` says "hippo_get_consolidation_level:"

### 15. [SYSTEMIC: 206 files] Unguarded global static mutable mesh registration variables

Every file in brain regions has:
```c
static mesh_participant_id_t g_<module>_mesh_id = 0;
static mesh_participant_registry_t* g_<module>_mesh_registry = NULL;
```

These are written in `_mesh_register()` and read/written in `_mesh_unregister()` without any synchronization. However, since mesh registration is typically done once at startup and unregistration at shutdown, this is unlikely to cause actual data races in practice. Noted as P3 for completeness.

---

## Files Reviewed in Detail

The following files were read and analyzed line-by-line or with targeted pattern searches:

**Full review** (read >50% of file):
- `hippocampus/nimcp_hippocampus.c` (2614 lines)
- `hypothalamus/nimcp_hypothalamus_drives.c` (1246 lines)
- `vta/nimcp_vta.c` (partial)
- `reticular/nimcp_reticular.c` (partial)
- `retrosplenial/nimcp_retrosplenial.c` (partial)
- `ofc/nimcp_ofc.c` (partial)
- `parietal/nimcp_parietal_quantum_bridge.c` (partial)
- `prefrontal/nimcp_prefrontal_adapter.c` (partial)
- `cerebellum/nimcp_cerebellum_adapter.c` (partial)
- `locus_coeruleus/nimcp_locus_coeruleus.c` (partial)
- `claustrum/nimcp_claustrum.c` (partial)
- `entorhinal/nimcp_entorhinal.c` (partial)
- `hippocampus/nimcp_hippocampus_adapter.c` (partial)
- `red_nucleus/nimcp_red_nucleus.c` (partial)
- `broca/nimcp_discourse_manager.c` (partial)
- `vta/nimcp_dopamine_release.c` (partial)
- `locus_coeruleus/nimcp_norepinephrine_release.c` (partial)
- `mammillary/nimcp_mammillary.c` (partial)
- `parahippocampal/nimcp_parahippocampal.c` (partial)
- `raphe/nimcp_raphe_adapter.c` (partial)
- `hypothalamus/nimcp_hypothalamus_homeostasis.c` (partial)
- `hypothalamus/nimcp_hypothalamus_perception_bridge.c` (partial)
- `nimcp_brain_region_predictive.c` (partial)

**Pattern search only** (grep for specific bug patterns across all 206 files):
- Division by zero patterns
- Raw `rand()`/`srand()` usage
- Static mutable variables
- Mutex lock/unlock pairing
- NIMCP_THROW_TO_IMMUNE error code correctness
- Function name accuracy in error messages
- Memory allocation/free patterns

## Positive Observations

1. **No raw rand() calls** - All randomness uses `nimcp_tl_rand()` (thread-local) or `rand_r()` with thread-local seeds
2. **Mutex handling is generally clean** - `ofc/nimcp_ofc.c` and `reticular/nimcp_reticular.c` consistently unlock on all paths
3. **NULL pointer checks** are present at function entry for nearly all public functions
4. **Division guards** are present in many places (ternary checks for count > 0, total > epsilon)
5. **Memory cleanup in destroy functions** is thorough (iterating arrays to free sub-allocations)
6. **Proper use of `nimcp_calloc`** for zero-initialization of allocated structures
