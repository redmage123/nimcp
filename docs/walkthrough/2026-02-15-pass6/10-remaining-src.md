# Pass 6 Code Walkthrough: Remaining src/ Directories

**Date**: 2026-02-15
**Scope**: src/api/, src/async/, src/chemistry/, src/core/axon/, src/core/dendrite/,
src/core/directives/, src/core/events/, src/core/integration/, src/core/medulla/,
src/core/neural_substrate/, src/core/neuralnet/, src/core/neuron_models/,
src/core/neuron_types/, src/core/topology/, src/information/, src/integration/,
src/io/, src/language/, src/lib/perception/, src/lnn/, src/perception/, src/physics/
**Type**: REVIEW ONLY (no edits)

---

## P1 Bugs (Crash / Data Corruption)

| # | File | Line | Bug | Description |
|---|------|------|-----|-------------|
| 1 | src/api/nimcp_api_brain.c | ~95 | Race condition | `get_brain_module_ctx()` lazy init has no mutex/once protection - two threads can both see NULL and double-init |
| 2 | src/physics/dynamics/nimcp_dynamical_systems.c | 585 | Div-by-zero | `param_step = ... / (float)(num_points - 1)` -- if `num_points == 1`, divides by zero. No guard for `num_points < 2` |
| 3 | src/physics/biophysics/nimcp_hodgkin_huxley.c | 1139 | Div-by-zero | `step = (I_max - I_min) / (float)(num_points - 1)` -- if `num_points == 1`, divides by zero. Guard at line 1137 only checks `> 0` |
| 4 | src/physics/bridges/nimcp_thermo_quantum_bridge.c | 316 | Div-by-zero | `temp_step = (temp_max - temp_min) / (float)(num_points - 1)` -- if `num_points == 1`, divides by zero. Guard at line 304 only checks `== 0` |
| 5 | src/physics/dynamics/nimcp_dynamical_systems.c | 673 | Integer underflow (uint32_t) | `num_points = series_length - (embed_dim - 1) * delay` -- if product exceeds series_length, wraps to huge value. Guard at line 675 (`< 10`) fails to catch because result is huge positive. Leads to massive allocation + OOB writes |

**P1 Total: 5**

---

## P2 Bugs (Wrong error codes, wrong func names, logic errors)

### P2a: NIMCP_ERROR_NO_MEMORY used for NULL input parameters (should be NIMCP_ERROR_NULL_POINTER)

| # | File | Line | Throw message |
|---|------|------|---------------|
| 1 | src/core/integration/nimcp_multimodal_integration.c | 67 | "config is NULL" |
| 2 | src/integration/knowledge/nimcp_sensory_kg_wiring.c | 104 | "allocate_node: wiring is NULL" |
| 3 | src/integration/knowledge/nimcp_sensory_kg_wiring.c | 115 | "allocate_edge: wiring is NULL" |
| 4 | src/physics/dynamics/nimcp_dynamical_systems.c | 531 | "dynsys_bifurcation_create: required parameter is NULL (config, sys)" |
| 5 | src/physics/dynamics/nimcp_dynamical_systems.c | 646 | "dynsys_attractor_create: config is NULL" |
| 6 | src/physics/dynamics/nimcp_dynamical_systems.c | 742 | "dynsys_energy_create: required parameter is NULL (config, sys)" |
| 7 | src/physics/dynamics/nimcp_dynamical_systems.c | 864 | "dynsys_slowfast_create: required parameter is NULL (config, sys)" |
| 8 | src/physics/dynamics/nimcp_dynamical_systems.c | 923 | "dynsys_bridge_create: required parameter is NULL (config, sys)" |
| 9 | src/physics/dynamics/nimcp_dynamical_systems.c | 985 | "dynsys_bridge_create_wiring: bridge is NULL" |
| 10 | src/physics/dynamics/nimcp_dynamical_systems.c | 989 | "dynsys_bridge_create_wiring: bridge is NULL" |
| 11 | src/lib/perception/nimcp_hair_cells.c | 121 | "hair_cell_bank_create: config is NULL" |
| 12 | src/lib/perception/nimcp_hair_cells.c | 126 | "hair_cell_bank_create: n is zero" (should be INVALID_PARAM) |
| 13 | src/lib/perception/nimcp_hair_cells.c | 213 | "ihc_output_create: num_channels is zero" (should be INVALID_PARAM) |
| 14 | src/lib/perception/nimcp_hair_cells.c | 312 | "ihc_bank_create: config is NULL" |
| 15 | src/lib/perception/nimcp_hair_cells.c | 317 | "ihc_bank_create: bank is NULL" (this one IS alloc failure, correct NO_MEMORY) |
| 16 | src/lib/perception/nimcp_hair_cells.c | 392 | "ohc_bank_create: config is NULL" |
| 17 | src/lib/perception/nimcp_hair_cells.c | 482 | "ihc_output_create: num_channels is zero" (should be INVALID_PARAM) |
| 18 | src/lib/perception/nimcp_hair_cells.c | 513 | "ohc_output_create: num_channels is zero" (should be INVALID_PARAM) |
| 19 | src/lib/perception/nimcp_speech_cortex.c | 91 | "speech_cortex_create: config is NULL" |
| 20 | src/language/bridges/nimcp_language_cerebellum_bridge.c | 160 | "bridge is NULL" |
| 21 | src/language/bridges/nimcp_language_hippocampus_bridge.c | 177 | "bridge is NULL" |
| 22 | src/language/bridges/nimcp_language_insula_bridge.c | 92 | "bridge is NULL" |
| 23 | src/language/bridges/nimcp_language_parietal_bridge.c | 172 | "bridge is NULL" |
| 24 | src/language/bridges/nimcp_language_motor_bridge.c | 341 | "bridge is NULL" |
| 25 | src/language/bridges/nimcp_language_cingulate_bridge.c | 364 | "bridge is NULL" |
| 26 | src/language/bridges/nimcp_language_temporal_bridge.c | 270 | "bridge is NULL" |
| 27 | src/perception/cortical/nimcp_speech_cortical_bridge.c | 281 | "bridge is NULL" |
| 28 | src/perception/cortical/nimcp_visual_cortical_bridge.c | 254 | "bridge is NULL" |
| 29 | src/perception/cortical/nimcp_audio_cortical_bridge.c | 196 | "bridge is NULL" |
| 30 | src/perception/nimcp_visual_jepa_fep_bridge.c | 74 | "bridge is NULL" |
| 31 | src/perception/integration/nimcp_perception_bio_async_bridge.c | 189 | "bridge is NULL" |
| 32 | src/lnn/nimcp_lnn_sleep_bridge.c | 130 | "bridge is NULL" |

### P2b: NIMCP_ERROR_NULL_POINTER used for allocation failures (should be NIMCP_ERROR_NO_MEMORY)

| # | File | Line | Throw message |
|---|------|------|---------------|
| 1 | src/core/integration/nimcp_multimodal_integration.c | 73 | "integration is NULL" (after calloc) |
| 2 | src/lib/perception/nimcp_visual_cortex.c | 768 | "cortex allocation failed" |
| 3 | src/lib/perception/nimcp_hair_cells.c | 247 | "hc_bank_output_create: operation failed" (array alloc) |
| 4 | src/lib/perception/bridges/nimcp_cochlea_thalamic_bridge.c | 180 | "array alloc failed" |
| 5 | src/lib/perception/bridges/nimcp_cochlea_cortical_deep_bridge.c | 189 | "array alloc failed" |
| 6 | src/lib/perception/bridges/nimcp_cochlea_fep_bridge.c | 162 | "array alloc failed" |
| 7 | src/lib/perception/bridges/nimcp_cochlea_audio_cortex_bridge.c | 208 | "array alloc failed" |
| 8 | src/perception/sleep/nimcp_visual_cortex_sleep_bridge.c | 123 | "bridge->base is NULL" (after alloc) |
| 9 | src/perception/sleep/nimcp_audio_cortex_sleep_bridge.c | 123 | "bridge->base is NULL" (after alloc) |
| 10 | src/perception/sleep/nimcp_speech_cortex_sleep_bridge.c | 123 | "bridge->base is NULL" (after alloc) |
| 11 | src/perception/sleep/nimcp_retina_sleep_bridge.c | 125 | "bridge->base is NULL" (after alloc) |
| 12 | src/perception/integration/nimcp_perception_bio_async_bridge.c | 215 | "bridge->subscriptions is NULL" (after alloc) |
| 13 | src/perception/nimcp_speech_jepa_bridge.c | 265 | "operation failed" (after alloc) |
| 14 | src/physics/bridges/nimcp_ephaptic_fft_bridge.c | 352 | "bridge->power_spectrum is NULL" (after alloc) |
| 15 | src/async/nimcp_future.c | 221 | allocation failure |
| 16 | src/core/axon/nimcp_axon.c | 1502 | "axon_spike_pool_alloc: operation failed" |
| 17 | src/core/axon/nimcp_axon.c | 1653 | "axon_segment_pool_alloc: operation failed" |
| 18 | src/core/dendrite/nimcp_dendrite.c | 1926 | allocation failure |
| 19 | src/lnn/nimcp_lnn_layer.c | 281 | "lnn_layer_create: operation failed" (after alloc) |
| 20 | src/lnn/nimcp_lnn_layer.c | 319 | "lnn_layer_create: operation failed" (after alloc) |
| 21 | src/lnn/nimcp_lnn_wiring_ternary.c | 346 | "adjacency is NULL" (after alloc) |
| 22 | src/language/bridges/nimcp_language_training_bridge.c | 300 | "bridge->event_log is NULL" (after alloc) |
| 23 | src/lnn/nimcp_lnn_parallel.c | 424 | "g_parallel_initialized is NULL" (not alloc, wrong code for not-initialized) |

### P2c: NIMCP_ERROR_NULL_POINTER for "capacity exceeded" (should be OUT_OF_RANGE)

| # | File | Line | Throw message |
|---|------|------|---------------|
| 1 | src/lnn/nimcp_lnn_wiring.c | 835 | "capacity exceeded" |
| 2 | src/chemistry/ph/nimcp_ph_dynamics.c | 349 | "capacity exceeded" |
| 3 | src/chemistry/gasotransmitters/nimcp_nitric_oxide.c | 359 | "capacity exceeded" |

### P2d: Wrong function name in throw message

| # | File | Line | Says | Should Be |
|---|------|------|------|-----------|
| 1 | src/lib/perception/nimcp_hair_cells.c | 342 | "ihc_bank_destroy" | "ihc_bank_process" |
| 2 | src/lib/perception/nimcp_hair_cells.c | 367 | "ihc_bank_reset" | "ihc_bank_get_state" |
| 3 | src/lib/perception/nimcp_hair_cells.c | 443 | "ohc_bank_reset" | "ohc_bank_set_health" |
| 4 | src/lib/perception/nimcp_hair_cells.c | 60 | "unknown:" | should name actual function "ihc_bank_set_health" |
| 5 | src/lib/perception/nimcp_hair_cells.c | 64 | "unknown:" | should name actual function "ihc_bank_set_health" |
| 6 | src/lib/perception/nimcp_visual_cortex.c | 985 | "unknown:" | should name actual function |
| 7 | src/language/bridges/nimcp_language_omni_bridge.c | 560 | "unknown:" | should name "language_omni_bridge_report_error" |
| 8 | src/language/bridges/nimcp_language_omni_bridge.c | 564 | "unknown:" | should name "language_omni_bridge_report_error" |
| 9 | src/language/bridges/nimcp_language_omni_bridge.c | 592 | "unknown:" | should name "language_omni_bridge_get_errors" |
| 10 | src/async/nimcp_bio_router.c | 1601 | "unknown:" | should name "bio_router_send_async" |

### P2e: Swapped error codes (NO_MEMORY vs NULL_POINTER in same create function)

| # | File | Lines | Description |
|---|------|-------|-------------|
| 1 | src/core/integration/nimcp_multimodal_integration.c | 67, 73 | Line 67: NO_MEMORY for "config is NULL" (wrong), Line 73: NULL_POINTER for alloc failure (wrong). Both codes are swapped. |
| 2 | src/lib/perception/nimcp_hair_cells.c | 121, 132 | Line 121: NO_MEMORY for "config is NULL" (wrong), Line 132: NO_MEMORY for alloc fail (correct). Inconsistent guards. |
| 3 | src/physics/dynamics/nimcp_dynamical_systems.c | 531, 537 | Line 531: NO_MEMORY for NULL params (wrong), Line 537: NO_MEMORY for alloc fail (correct). |

### P2f: Other logic errors

| # | File | Line | Description |
|---|------|------|-------------|
| 1 | src/api/nimcp_refactored.c | 88 | `g_last_error` is NOT `_Thread_local` unlike nimcp.c -- thread-safety regression |
| 2 | src/api/nimcp_api_inference.c | 140-141 | Hardcoded `63` for label copy size instead of `NIMCP_MAX_LABEL_SIZE - 1` |
| 3 | src/api/nimcp_brain_api.c | 186-188 | Hardcoded `63` for label copy size instead of `NIMCP_MAX_LABEL_SIZE - 1` |
| 4 | src/core/dendrite/nimcp_dendrite.c | 1110 | NIMCP_ERROR_NO_MEMORY for "max_dendrites is zero" (should be INVALID_PARAM) |
| 5 | src/physics/dynamics/nimcp_dynamical_systems.c | 982-989 | `dynsys_bridge_create_wiring()` always throws and returns NULL even when bridge is valid (line 989 unreachable guard + always-throw) |
| 6 | src/lnn/nimcp_lnn_parallel.c | 424 | NIMCP_ERROR_NULL_POINTER for "g_parallel_initialized is NULL" -- this is a not-initialized condition, should be NIMCP_ERROR_NOT_INITIALIZED |
| 7 | src/integration/knowledge/nimcp_sensory_kg_wiring.c | 104 | Combines two conditions (NULL wiring OR capacity full) into single error code/message |
| 8 | src/integration/knowledge/nimcp_sensory_kg_wiring.c | 115 | Same issue for allocate_edge |
| 9 | src/lnn/nimcp_lnn_network.c | 1288, 1293 | Duplicate throws: line 1288 in guard, line 1293 unconditionally throws same message (dead code or double-throw) |

**P2 Total: ~80** (32 NO_MEMORY-for-NULL-pointer + 23 NULL_POINTER-for-alloc + 3 NULL_POINTER-for-capacity + 10 wrong-func-name + 3 swapped-codes + 9 other)

---

## P3 Bugs (Minor / Style)

| # | File | Line | Description |
|---|------|------|-------------|
| 1 | src/core/events/nimcp_event_bus.c | - | Uses snapshot-based delivery (good) but copies full subscriber array on each publish -- O(N) allocation per publish |
| 2 | src/physics/dynamics/nimcp_dynamical_systems.c | 260-264 | RK4 allocates 5 temp arrays per step via nimcp_calloc -- performance issue for tight integration loops |
| 3 | src/lib/perception/nimcp_hair_cells.c | 306 | `(void)mode;` -- mode parameter is accepted but ignored |
| 4 | src/lib/perception/nimcp_hair_cells.c | 386 | `(void)mode;` -- mode parameter is accepted but ignored |
| 5 | src/perception/nimcp_lip_reading.c | 84-96 | audiovisual_integrator has fixed-size buffer `[32]` -- may silently lose old data without warning |
| 6 | src/integration/core/nimcp_inter_layer_router.c | 22, 96 | Hardcoded `256` for queue size instead of using config.default_queue_depth |
| 7 | src/lnn/nimcp_lnn_ternary.c | 333 | NIMCP_ERROR_NULL_POINTER for "dst->dense is NULL" after alloc -- should be NO_MEMORY |
| 8 | src/async/nimcp_bio_router.c | 1612 | NIMCP_ERROR_NULL_POINTER for promise alloc failure -- should be NO_MEMORY |

**P3 Total: 8**

---

## Statistics

| Category | Count |
|----------|-------|
| **P1 (Crash/Corruption)** | **5** |
| P1 Race conditions | 1 |
| P1 Division by zero | 3 |
| P1 Integer underflow | 1 |
| **P2 (Wrong codes/Logic)** | **~80** |
| P2 NO_MEMORY for NULL-pointer inputs | 32 |
| P2 NULL_POINTER for alloc failures | 23 |
| P2 NULL_POINTER for capacity exceeded | 3 |
| P2 Wrong function name in throw | 10 |
| P2 Swapped error codes | 3 |
| P2 Other logic errors | 9 |
| **P3 (Minor)** | **8** |
| **TOTAL** | **~93** |

### Files Reviewed

| Directory | Files | Status |
|-----------|-------|--------|
| src/api/ | 14 | All reviewed |
| src/async/ | 17 | All reviewed |
| src/chemistry/ | 4 | All reviewed |
| src/core/axon/ | 2 | All reviewed |
| src/core/dendrite/ | 1 | Reviewed |
| src/core/directives/ | 10 | All reviewed |
| src/core/events/ | 1 | Reviewed |
| src/core/integration/ | 2 | All reviewed |
| src/core/medulla/ | 7 | All reviewed |
| src/core/neural_substrate/ | 2 | All reviewed |
| src/core/neuralnet/ | 1 | Reviewed |
| src/core/neuron_models/ | 5+ | Reviewed |
| src/core/neuron_types/ | 2 | Reviewed |
| src/core/topology/ | 4+ | Reviewed |
| src/information/ | 3+ | Reviewed |
| src/integration/ | 47 | All reviewed (grep + spot reads) |
| src/io/ | 5+ | Reviewed |
| src/language/ | 21 | All reviewed (grep + spot reads) |
| src/lib/perception/ | 24 | All reviewed (grep + spot reads) |
| src/lnn/ | 15 | All reviewed (grep + spot reads) |
| src/perception/ | 19 | All reviewed (grep + spot reads) |
| src/physics/ | 25 | All reviewed (grep + spot reads) |

### Systemic Patterns

1. **NIMCP_ERROR_NO_MEMORY for NULL input parameters**: Pervasive across dynamical_systems.c (8 occurrences), language bridges (7 bridges), perception cortical bridges (3 bridges), hair_cells.c (5+), and other files. The pattern is: `if (!config) { THROW(NO_MEMORY, "config is NULL"); return NULL; }` -- should use NULL_POINTER.

2. **NIMCP_ERROR_NULL_POINTER for allocation failures**: Pervasive across cochlea bridges (4 files), perception sleep bridges (4 files), lnn layer/wiring (4 occurrences), and other files. The pattern is: checking calloc result, then throwing NULL_POINTER -- should use NO_MEMORY.

3. **"unknown:" function name in throws**: Found in hair_cells.c (2), visual_cortex.c (1), language_omni_bridge.c (3), bio_router.c (1). These provide no diagnostic value since the actual function name is missing.

4. **Division by `(num_points - 1)` without num_points >= 2 guard**: Found in 3 physics files. All use `(float)(num_points - 1)` as divisor where the guard only checks for `> 0` or `== 0`, allowing `num_points == 1` through.

5. **Hardcoded sizes instead of constants**: label copy uses `63` instead of `NIMCP_MAX_LABEL_SIZE - 1` (2 API files), queue size uses `256` instead of config value (1 router file).

### Positive Observations

- **Thread-safe PRNG**: All directories use `nimcp_tl_rand()` or custom CAS-loop PRNGs. No raw `rand()` found.
- **Div-by-zero guards**: Most math-heavy code (LNN, chemistry, ephaptic) uses EPSILON guards or explicit checks. The physics division issues are an exception.
- **Memory cleanup on failure**: Create functions generally have proper cascading cleanup (free earlier allocations on later failures).
- **Health agent macros**: Consistently declared across all modules.
- **Event bus architecture**: Uses snapshot-based delivery to avoid holding locks during callbacks -- good deadlock prevention.
- **Cochlea/perception pipeline**: Well-structured signal processing chain with proper config defaults.
- **Integration layer**: Clean adapter/bridge/coordinator architecture with proper lifecycle management.
- **RK4 implementation**: Includes isfinite() divergence detection.
