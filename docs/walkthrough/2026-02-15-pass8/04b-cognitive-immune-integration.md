# Pass 8 Walkthrough: Cognitive Immune / Integration / Collective Cognition

**Date**: 2026-02-15
**Scope**: `src/cognitive/immune/` (39 files), `src/cognitive/integration/` (24 files), `src/cognitive/collective_cognition/` (9 files)
**Total**: 72 C files reviewed

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| P1 | 8 | Data races, use-after-free, div-by-zero |
| P2 | 10 | Wrong func names, false positive throws, logic bugs, API mismatch |
| P3 | 1 | Dead code stubs |

---

## P1 - Critical Bugs

### P1-1: Data race on unprotected global `g_stats`
- **File**: `src/cognitive/immune/nimcp_brain_immune_plasticity.c:86`
- **Bug**: `static immune_plasticity_stats_t g_stats = {0}` is a mutable global struct accessed from multiple functions (`immune_plasticity_compute_ltp_modulation`, `immune_plasticity_compute_ltd_modulation`, `immune_plasticity_get_stats`, `immune_plasticity_reset_stats`) with no mutex protection.
- **Impact**: Concurrent callers corrupt stat counters. Torn reads on multi-word fields.
- **Fix**: Add a static mutex or make individual fields `_Atomic`.

### P1-2: Non-atomic training globals in collective_cognition
- **File**: `src/cognitive/collective_cognition/nimcp_collective_cognition.c:1426-1429`
- **Bug**: `static uint64_t g_collective_cognition_training_steps`, `static double g_collective_cognition_training_total_error`, `static double g_collective_cognition_training_best_error`, `static bool g_collective_cognition_training_active` are plain (non-atomic) static globals accessed from training begin/step/end which can be called from different threads.
- **Impact**: Data race, torn reads on 64-bit values on 32-bit platforms.
- **Fix**: Use `_Atomic` qualifier (as done correctly in `nimcp_collective_phi.c`, `nimcp_extended_mind.c`, `nimcp_shared_intentionality.c`).

### P1-3: Non-atomic training globals in hyperscanning
- **File**: `src/cognitive/collective_cognition/nimcp_hyperscanning.c:1579-1582`
- **Bug**: Identical to P1-2. `static uint64_t g_hyperscanning_training_steps`, `static double g_hyperscanning_training_total_error`, `static double g_hyperscanning_training_best_error`, `static bool g_hyperscanning_training_active` are plain (non-atomic).
- **Impact**: Data race on training counters.
- **Fix**: Use `_Atomic` qualifier.

### P1-4: Use-after-free in MC severity estimation
- **File**: `src/cognitive/immune/nimcp_brain_immune_tick.c:471-475`
- **Bug**: `mc_result_free(&result)` is called on line 471, then `result.std_error` is read on line 475 inside `LOG_DEBUG`. After `mc_result_free`, the `result` struct fields may be zeroed or invalidated.
- **Impact**: Reads freed/zeroed data. If `mc_result_free` only frees internal heap pointers and `std_error` is a plain float member, this may only produce a zero value. But the access pattern is undefined behavior.
- **Fix**: Move `mc_result_free(&result)` to after the LOG_DEBUG call, or save `result.std_error` to a local variable before freeing.

### P1-5: Div-by-zero in perception immune (visual overload)
- **File**: `src/cognitive/immune/nimcp_perception_immune.c:645,658`
- **Bug**: `mean /= num_features` (line 645) and `variance /= num_features` (line 658) divide by `num_features` with no guard for `num_features == 0`. The function validates `!ctx`, `!features`, `!overload` but not `num_features == 0`.
- **Impact**: Floating-point div-by-zero (produces NaN/Inf on IEEE 754, but still a logic bug; on non-IEEE platforms, crash).
- **Fix**: Add `if (num_features == 0) { *overload = false; return 0; }` guard.

### P1-6: Div-by-zero in perception immune (audio overload)
- **File**: `src/cognitive/immune/nimcp_perception_immune.c:693`
- **Bug**: `energy = sqrtf(energy / num_bins)` divides by `num_bins` with no guard for `num_bins == 0`. Same missing guard as P1-5.
- **Impact**: Floating-point div-by-zero.
- **Fix**: Add `if (num_bins == 0) { *overload = false; return 0; }` guard.

### P1-7: Div-by-zero in FEP bridge threshold comparison
- **File**: `src/cognitive/immune/nimcp_brain_immune_fep_bridge.c:343-345`
- **Bug**: `(fe - fep_tolerance_threshold) / (fep_threat_threshold - fep_tolerance_threshold)` divides by the difference of two configurable thresholds. If `fep_threat_threshold == fep_tolerance_threshold`, this is a div-by-zero.
- **Impact**: Floating-point div-by-zero producing NaN/Inf, corrupting subsequent threat severity calculations.
- **Fix**: Add guard: `if (fep_threat_threshold <= fep_tolerance_threshold) { ... use fallback ... }`.

### P1-8: TOCTOU in code_immune auto-repair loop
- **File**: `src/cognitive/immune/nimcp_code_immune_self_repair.c:714-733`
- **Bug**: `code_immune_process_auto_repairs()` iterates over antigens inside a loop that unlocks the mutex to call `code_immune_attempt_self_repair()`, then relocks. Between unlock and relock, `antigen_count` and antigen array contents can change (other threads can add/remove antigens), potentially causing out-of-bounds access or skipped/double-processed antigens.
- **Impact**: Array index may exceed new `antigen_count` after relock, or antigen at index `i` may have been removed/replaced.
- **Fix**: Snapshot antigen IDs under lock, iterate snapshot after unlock. Or copy the antigen data needed for repair before unlocking.

---

## P2 - Moderate Bugs

### P2-1: Wrong function names in tick.c THROW messages (8 occurrences)
- **File**: `src/cognitive/immune/nimcp_brain_immune_tick.c`
- **Locations**:
  - Line 1345: Says `"brain_immune_tick_default_config"` but is in `brain_immune_tick_init`
  - Lines 1762, 1773: Say `"brain_immune_tick_has_health_agent"` but are in `brain_immune_tick_get_stats`
  - Lines 1798, 1809: Say `"brain_immune_tick_reset_stats"` but are in `brain_immune_tick_get_config`
  - Lines 1820, 1830: Say `"brain_immune_tick_reset_stats"` but are in `brain_immune_tick_set_config`
- **Impact**: Misleading error diagnostics in logs. Developers waste time looking at wrong functions during debugging.
- **Fix**: Update each throw message to match the actual containing function name.

### P2-2: False positive THROW_TO_IMMUNE in find_*_by_id helpers (systemic, ~30+ occurrences)
- **Files**: `nimcp_brain_immune.c` (lines 223, 249, 275, 301), `nimcp_code_immune.c` (lines 299, 325, 351, 1082, 2246), `nimcp_mucosal_immunity.c` (lines 115, 144, 173), `nimcp_complement_system.c` (lines 180, 206), `nimcp_heal_bridge.c` (lines 131, 158, 185), `nimcp_immune_vaccine.c` (line 176), `nimcp_immune_exhaustion.c` (line 196), `nimcp_code_immune_self_repair.c` (lines 151, 221), `nimcp_surface_immune_bridge.c` (lines 373, 395, 433, 453), `nimcp_immune_tolerance.c` (lines 1138, 1169), `nimcp_immune_bridge_coordinator.c` (line 128), `nimcp_claude_healer.c` (line 221)
- **Bug**: `find_*` helper functions call `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...)` when an item is not found by ID. "Not found" is normal lookup behavior, not an error condition. These helpers are called in loops and lookup paths where "not found" is expected.
- **Impact**: Immune system flooded with false exception reports for normal lookups. Performance degradation from unnecessary exception processing.
- **Fix**: Remove NIMCP_THROW_TO_IMMUNE from "not found" return paths. Keep the NULL pointer guard for the system parameter. Note: `find_inflammation_by_id` in `nimcp_brain_immune.c:327` is already correct (returns NULL without throw).

### P2-3: Mutex API mismatch in brain_immune_destroy
- **File**: `src/cognitive/immune/nimcp_brain_immune.c:647`
- **Bug**: `brain_immune_destroy()` calls `nimcp_mutex_free(system->mutex)` but the file's macro block (line 90) defines `nimcp_mutex_destroy` (not `nimcp_mutex_free`). The mutex was created via macro `nimcp_mutex_create()` which maps to `nimcp_platform_mutex_create()` (platform layer). `nimcp_mutex_free` is a thread-layer API function that may expect a thread-layer mutex, not a platform-layer one.
- **Impact**: If the thread-layer `nimcp_mutex_free` calls `nimcp_mutex_destroy` (thread layer), which internally calls `nimcp_platform_mutex_destroy`, this may work by accident. But it is an API layer mismatch that could break under refactoring.
- **Fix**: Use `nimcp_mutex_destroy(system->mutex)` (the defined macro) instead of `nimcp_mutex_free`.

### P2-4: Global static counter corrupts per-instance running average
- **File**: `src/cognitive/integration/nimcp_cognitive_bio_async_bridge.c:706-709`
- **Bug**: `static _Atomic uint64_t update_count = 0` is a function-local static shared across ALL `cognitive_bio_bridge_t` instances. The running average formula `(avg * (count - 1) + elapsed) / count` uses this global counter instead of a per-bridge counter. If 3 bridges each call update once, the third bridge computes its average as if it had been called 3 times, weighting its first measurement incorrectly.
- **Impact**: `avg_update_time_us` stat becomes increasingly inaccurate with multiple bridge instances.
- **Fix**: Move the counter into the `cognitive_bio_bridge_t` struct (e.g., `bridge->stats.update_count`).

### P2-5: TOCTOU in code_immune_process_repair_outcomes
- **File**: `src/cognitive/immune/nimcp_code_immune_self_repair.c:860-893`
- **Bug**: Same unlock/relock pattern as P1-8 but for repair outcomes processing. The loop unlocks mutex to call external outcome processing, then relocks, but the iteration index and array contents may have changed.
- **Impact**: Same as P1-8 but for outcome processing path.
- **Fix**: Same approach as P1-8 - snapshot data under lock.

### P2-6: Wrong error code in bridge create (allocation failure)
- **Files**: Multiple immune bridge files use `NIMCP_ERROR_NO_MEMORY` for NULL parameter guards and `NIMCP_ERROR_NULL_POINTER` for allocation failures, or vice versa. Examples:
  - `nimcp_curiosity_immune_bridge.c:254`: `NIMCP_ERROR_NO_MEMORY` for "required parameter is NULL" (should be `NIMCP_ERROR_NULL_POINTER`)
  - `nimcp_curiosity_immune_bridge.c:267`: `NIMCP_ERROR_NULL_POINTER` for "Allocation failed" (should be `NIMCP_ERROR_NO_MEMORY`)
  - `nimcp_executive_immune_bridge.c:269`: Same swap pattern
  - `nimcp_introspection_immune_bridge.c:233`: Same swap pattern
- **Impact**: Error categorization is inverted - allocation failures report as null pointer, null parameters report as out of memory.
- **Fix**: Swap error codes to match the actual failure condition.

### P2-7: Salience attention bridge running average assumes count >= 1
- **File**: `src/cognitive/integration/nimcp_salience_attention_bridge.c:908-910`
- **Bug**: `bridge->stats.avg_salience_score = (bridge->stats.avg_salience_score * (count - 1) + score) / count` where `count = bridge->stats.salience_detections`. The counter was just incremented on line 899 so `count >= 1`, but if `salience_detections` starts at a non-zero value (e.g., loaded from persistence), the initial `avg_salience_score * (count - 1)` computation uses stale average data.
- **Same pattern at**: Line 1082 with `avg_attention_strength` and `focus_notifications`.
- **Impact**: Minor - running average can be slightly off if stats are loaded from persistence without resetting averages.
- **Fix**: Use Welford's online algorithm or validate initial state on stats load.

### P2-8: Immune persistence merge_incremental is a stub that throws
- **File**: `src/cognitive/immune/nimcp_immune_persistence.c:1202-1204`
- **Bug**: `immune_persistence_merge_incremental()` logs a warning that it's not yet implemented, then calls `NIMCP_THROW_TO_IMMUNE` with a misleading "NULL parameter" error message before returning -1. The function IS a stub, but its throw message claims "required parameter is NULL" which is false - the parameters were already validated.
- **Impact**: False error report in immune system when incremental merge is attempted.
- **Fix**: Either implement the function or change the throw to `NIMCP_ERROR_NOT_IMPLEMENTED` with appropriate message, or remove the throw entirely and just return -1.

### P2-9: Stale `result.std_error` used in LOG_DEBUG message
- **File**: `src/cognitive/immune/nimcp_brain_immune_tick.c:473-475`
- **Note**: This is the same location as P1-4. Even if `std_error` is not actually freed (being a float, not a pointer), the LOG_DEBUG uses `estimate` (local copy, safe) but also uses `result.std_error` after `mc_result_free`. If `mc_result_free` memsets the struct, this logs 0.000 instead of the actual std_error, producing misleading diagnostics.

### P2-10: Emotion immune bridge compute_sickness_behavior stub
- **File**: `src/cognitive/immune/nimcp_emotion_immune_bridge.c:101-112`
- **Bug**: `compute_sickness_behavior()` always returns 0.0f with a `/* Stub */` comment. Helper functions `get_inflammation_duration_sec()` (lines 121-127) and `get_max_inflammation_level()` (lines 136-143) are also stubs returning 0/NONE. These are called from the bridge update loop, meaning the emotion-immune bridge's sickness behavior pathway is completely non-functional.
- **Impact**: Emotion system never receives sickness behavior signals from the immune system, breaking a core bridge contract.
- **Fix**: Implement using the same patterns as `nimcp_executive_immune_bridge.c` which has working implementations of the same helper functions.

---

## P3 - Low Priority

### P3-1: Dead code / stub helper functions in emotion immune bridge
- **File**: `src/cognitive/immune/nimcp_emotion_immune_bridge.c:101-143`
- **Description**: Three helper functions (`compute_sickness_behavior`, `get_inflammation_duration_sec`, `get_max_inflammation_level`) are stubs that always return 0/NONE. They are called but produce no useful output. This is also noted as P2-10 above due to functional impact.

---

## Systemic Patterns Observed

### Pattern A: Boilerplate correctness
The mesh participant registration, health agent heartbeat, knowledge graph self-awareness query, bio-async connect/disconnect, and training integration boilerplate is consistent and correct across all 72 files. No bugs found in these sections.

### Pattern B: Bridge base usage
Files using `bridge_base_t` as first struct member for mutex management are consistently correct in their lock/unlock patterns. The `bridge_base_init()` / `bridge_base_cleanup()` lifecycle is properly handled.

### Pattern C: Division guards
Most division operations in the codebase have proper `count > 0` guards (e.g., `nimcp_collective_phi.c:387`, `nimcp_extended_mind.c:1008`, `nimcp_game_theory_executive_fep_bridge.c:831`). The exceptions noted in P1-5 through P1-7 are outliers where the parameter-level guard is missing.

### Pattern D: Atomic training globals
5 of 9 collective_cognition files correctly use `_Atomic` for training globals (`nimcp_collective_phi.c`, `nimcp_extended_mind.c`, `nimcp_shared_intentionality.c`, `nimcp_collective_fep_bridge.c`, `nimcp_collective_plasticity_bridge.c`). The 2 files that don't (`nimcp_collective_cognition.c`, `nimcp_hyperscanning.c`) appear to have been written earlier, before the atomic pattern was established.

### Pattern E: False positive throws in find_* helpers
This is the most pervasive P2 issue. 12 immune files contain find_* helper functions that throw `NIMCP_THROW_TO_IMMUNE` on the "not found" path. The one exception is `find_inflammation_by_id` in `nimcp_brain_immune.c:327` which correctly returns NULL without throwing. All other find_* helpers should follow this pattern.

---

## Files Reviewed in Detail

| File | Lines | Status |
|------|-------|--------|
| `nimcp_brain_immune.c` | ~2000+ | P2-2, P2-3 |
| `nimcp_brain_immune_integration.c` | 839 | Clean |
| `nimcp_brain_immune_tick.c` | 1890 | P1-4, P2-1 |
| `nimcp_brain_immune_plasticity.c` | 1144 | P1-1 |
| `nimcp_brain_immune_substrate_bridge.c` | 400 | Clean |
| `nimcp_brain_immune_thalamic_bridge.c` | 294 | Clean (formula safe due to pre-increment) |
| `nimcp_brain_immune_fep_bridge.c` | 656 | P1-7 |
| `nimcp_attention_immune_bridge.c` | 832 | Clean |
| `nimcp_emotion_immune_bridge.c` | 855 | P2-10, P3-1 |
| `nimcp_code_immune_self_repair.c` | 1167 | P1-8, P2-2, P2-5 |
| `nimcp_curiosity_immune_bridge.c` | 976 | P2-6 |
| `nimcp_executive_immune_bridge.c` | 1084 | P2-6 |
| `nimcp_immune_persistence.c` | 1292 | P2-8 |
| `nimcp_introspection_immune_bridge.c` | 913 | P2-6 |
| `nimcp_perception_immune.c` | ~700 | P1-5, P1-6 |
| `nimcp_immune_bridge_coordinator.c` | ~1000 | P2-2 |
| `nimcp_complement_system.c` | ~1000 | P2-2 |
| `nimcp_immune_exhaustion.c` | ~800 | P2-2 |
| `nimcp_regulatory_tcells.c` | ~800 | Clean |
| `nimcp_salience_attention_bridge.c` | ~1100 | P2-7 |
| `nimcp_cognitive_bio_async_bridge.c` | ~800 | P2-4 |
| `nimcp_game_theory_executive_fep_bridge.c` | ~900 | Clean (has total > 0 guard) |
| `nimcp_collective_cognition.c` | ~1500 | P1-2 |
| `nimcp_hyperscanning.c` | ~1650 | P1-3 |
| `nimcp_collective_phi.c` | ~1300 | Clean (uses _Atomic) |
| `nimcp_extended_mind.c` | ~1250 | Clean (uses _Atomic, has count > 0 guard) |
| `nimcp_shared_intentionality.c` | ~1600 | Clean (uses _Atomic) |
| `nimcp_imagination_reasoning_bridge.c` | ~1750 | Clean |

Remaining immune files (bridge boilerplate only, no unique logic bugs found via pattern scan):
`nimcp_knowledge_immune_bridge.c`, `nimcp_mental_health_immune_bridge.c`, `nimcp_omni_immune_bridge.c`, `nimcp_self_model_immune_bridge.c`, `nimcp_sleep_immune_bridge.c`, `nimcp_tom_immune_bridge.c`, `nimcp_wellbeing_immune_bridge.c`, `nimcp_claude_healer.c`, `nimcp_heal_bridge.c`, `nimcp_immune_metrics.c`, `nimcp_mucosal_immunity.c`, `nimcp_self_heal.c`, `nimcp_surface_immune_bridge.c`, `nimcp_memory_immune_integration.c`, `nimcp_reasoning_immune.c`, `nimcp_code_immune.c`, `nimcp_immune_tolerance.c`, `nimcp_trained_immunity.c`, `nimcp_immune_vaccine.c`, `nimcp_autobiographical_immune_bridge.c`

Remaining integration files (no unique logic bugs found via pattern scan):
`nimcp_cognitive_integration_fep.c`, `nimcp_attention_wm_bridge.c`, `nimcp_ethics_executive_bridge.c`, `nimcp_collective_hub_bridge.c`, `nimcp_mirror_empathy_bridge.c`, `nimcp_mirror_empathy_fep_bridge.c`, `nimcp_predictive_attention_bridge.c`, `nimcp_predictive_attention_fep_bridge.c`, `nimcp_salience_attention_fep_bridge.c`, `nimcp_security_cognitive_hub_bridge.c`, `nimcp_cognitive_integration_hub.c`, `nimcp_curiosity_reasoning_bridge.c`, `nimcp_rcog_hub_bridge.c`, `nimcp_tom_social_bridge.c`, `nimcp_emotion_memory_bridge.c`, `nimcp_emotion_executive_bridge.c`, `nimcp_self_introspection_bridge.c`, `nimcp_gw_cognitive_bridge.c`, `nimcp_game_theory_executive_bridge.c`

Remaining collective_cognition files:
`nimcp_collective_fep_bridge.c`, `nimcp_collective_plasticity_bridge.c`, `nimcp_collective_snn_bridge.c`, `nimcp_collective_cognition_immune_bridge.c`
