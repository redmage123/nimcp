# Pass 8 Walkthrough: Cognitive Advanced Modules

**Date**: 2026-02-15
**Scope**: `src/cognitive/` subdirectories: parietal, vae, game_theory, creative, recursive, free_energy, fault_tolerance, neuro_symbolic, jepa, imagination, logic, symbolic_logic, sleep_wake, epistemic, omni, fractal_cognitive, extrapolation, analysis, explanations, predictive_immune
**Reviewer**: Claude Opus 4.6 (automated walkthrough)

---

## Summary

| Priority | Count | Categories |
|----------|-------|------------|
| **P1** | 4 | div-by-zero (1), integer overflow (1), memory leak (1), thread-unsafe PRNG (1) |
| **P2** | 12 | wrong function name (6 patterns, ~175 occurrences), false positive THROW (3 patterns), mutex API misuse (1 pattern, 7 files) |
| **P3** | 2 | unused variable (1), excessive heartbeat in small loops (systemic) |

---

## P1: Crash / Security Bugs

### P1-1: Division by zero in `game_theory_snn_encode_payoff` when `num_actions == 0`

**File**: `src/cognitive/game_theory/nimcp_game_theory_snn_bridge.c:498`
**Category**: Division by zero
**Description**: `avg_payoff = sum_payoff / num_actions` divides by `num_actions` without any guard for `num_actions == 0`. The function receives `num_actions` as a parameter with no validation.

```c
// Line 498 - no guard for num_actions == 0
float avg_payoff = sum_payoff / num_actions;
```

**Fix**: Add a guard `if (num_actions == 0) return -1;` or similar at the top of the function.

---

### P1-2: Integer overflow in `nimcp_game_matrix_create` total_cells multiplication

**File**: `src/cognitive/game_theory/nimcp_gt_equilibrium.c:346`
**Category**: Integer overflow
**Description**: `total_cells *= strategies_per_player[i]` multiplies `uint32_t` values in a loop across up to `NIMCP_GT_MAX_PLAYERS` players, each with up to `NIMCP_GT_MAX_STRATEGIES` strategies. No overflow check is performed. With enough players/strategies, this easily overflows `uint32_t`, leading to undersized allocation and subsequent buffer overwrite.

```c
// Line 346 - potential uint32_t overflow
matrix->total_cells *= strategies_per_player[i];
```

**Fix**: Check for overflow before each multiplication: `if (matrix->total_cells > UINT32_MAX / strategies_per_player[i]) return error;`

---

### P1-3: Memory leak in `imagination_engine_init_for_brain`

**File**: `src/cognitive/imagination/nimcp_imagination_engine.c:565-591`
**Category**: Memory leak
**Description**: Creates an `imagination_engine_t*` via `imagination_engine_create(config)` and stores a brain reference (`engine->brain = brain`), but never attaches the engine to the brain or returns it. The engine pointer is lost when the function returns, leaking all memory allocated by `imagination_engine_create()`. The comment on line 589 acknowledges this: "This would typically call: brain_set_imagination_engine(brain, engine);" but the call is never made.

```c
// Lines 579-591
imagination_engine_t* engine = imagination_engine_create(config);
if (!engine) { ... return -1; }
engine->brain = brain;
/* Attach to brain (implementation depends on brain structure) */
/* This would typically call: brain_set_imagination_engine(brain, engine); */
return 0;  // engine pointer leaked
```

**Fix**: Either call `brain_set_imagination_engine(brain, engine)` or return the engine pointer to the caller.

---

### P1-4: Thread-unsafe per-call PRNG seeding in `omni_wm_mdn_sample`

**File**: `src/cognitive/omni/nimcp_omni_world_model.c:1486-1487`
**Category**: Thread-unsafe rand / poor PRNG quality
**Description**: `omni_wm_mdn_sample()` creates a new `unsigned int seed = (unsigned int)time(NULL)` on every call, then uses `rand_r(&seed)`. Since `time(NULL)` has 1-second granularity, all calls within the same second produce identical random values. This is both a quality issue (correlated samples) and a thread-safety concern (the function may be called from multiple threads). Other `rand_r` calls in `omni_world_model.c` (lines 308, 309, 1723, 1877) correctly use `wm->rand_seed` (instance-level seed), making this one an outlier.

```c
// Line 1486-1487
unsigned int seed = (unsigned int)time(NULL);
float r = (float)rand_r(&seed) / RAND_MAX;
```

**Fix**: Use `wm->rand_seed` or `nimcp_tl_rand()` instead of creating a per-call seed from `time(NULL)`.

---

## P2: Correctness Bugs

### P2-1: Wrong function name "randn:" in THROW messages (4 occurrences, 2 files)

**Files**:
- `src/cognitive/omni/nimcp_omni_world_model.c:318` -- says "randn:" should be "dynamics_create:"
- `src/cognitive/omni/nimcp_omni_world_model.c:346` -- says "randn:" should be "dynamics_create:"
- `src/cognitive/vae/nimcp_vae_encoder.c:184` -- says "randn:" should be "create_encoder_layer:"
- `src/cognitive/vae/nimcp_vae_encoder.c:210` -- says "randn:" should be "create_encoder_layer:"

**Category**: Wrong function name in error message
**Description**: These THROW messages use "randn:" as the function prefix, which is the name of a static helper function (Box-Muller random normal), not the function in which the THROW occurs. This makes debugging impossible because the error trace points to the wrong function.

---

### P2-2: Wrong function name "get_time_us:" in THROW messages (3 occurrences, 2 files)

**Files**:
- `src/cognitive/parietal/nimcp_parietal.c:270` -- says "get_time_us:" should be "physics_nn_create:"
- `src/cognitive/parietal/nimcp_parietal.c:298` -- says "get_time_us:" should be "physics_nn_create:"
- `src/cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.c:171` -- says "get_time_us:" should be actual function name

**Category**: Wrong function name in error message
**Description**: Same pattern as P2-1. The function name in the THROW string comes from a copy-paste of an unrelated function name.

---

### P2-3: Wrong function name "tensor_norm:" in THROW messages (2 occurrences)

**File**: `src/cognitive/imagination/nimcp_imagination_engine.c:236,242`
**Category**: Wrong function name in error message
**Description**: These THROWs are inside `blend_tensors()` but the error messages say "tensor_norm:".

```c
// Line 236 - inside blend_tensors(), but says "tensor_norm:"
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
    "tensor_norm: required parameter is NULL (result, a, b)");
```

---

### P2-4: Wrong function name "brain_get_imagination_engine:" in THROW messages (12 occurrences)

**File**: `src/cognitive/imagination/nimcp_imagination_engine.c:608,621,641,661,681,701,721,741,761,781,805,825`
**Category**: Wrong function name in error message
**Description**: Multiple `imagination_connect_*` and `imagination_disconnect_*` functions have THROW messages that say "brain_get_imagination_engine:" instead of the actual function name. This is a copy-paste artifact from the `brain_get_imagination_engine()` function that precedes them.

---

### P2-5: Wrong function name "unknown:" in THROW messages (~165 occurrences, 28 files in scope)

**Files** (in-scope modules only):
- `src/cognitive/imagination/nimcp_imagination_engine.c` (46 occurrences)
- `src/cognitive/creative/` (67 occurrences across 11 files)
- `src/cognitive/omni/bridges/` (37 occurrences across 9 files)
- `src/cognitive/neuro_symbolic/` (6 occurrences across 2 files)
- `src/cognitive/game_theory/` (5 occurrences across 2 files)
- `src/cognitive/vae/` (3 occurrences across 3 files)
- `src/cognitive/imagination/nimcp_imagination_workspace.c` (1 occurrence)

**Category**: Wrong function name in error message
**Description**: These THROW messages use "unknown:" as the function prefix. This appears to be a systemic issue from automated THROW insertion where the tool could not determine the function name. While not a crash bug, it renders immune system error tracing useless for these call sites.

---

### P2-6: False positive THROW in `find_bridge_by_id` (normal "not found" case)

**File**: `src/cognitive/free_energy/nimcp_fep_orchestrator.c:125`
**Category**: False positive NIMCP_THROW_TO_IMMUNE
**Description**: `find_bridge_by_id()` is a search function that returns NULL when the bridge is not found. This is normal behavior, not an error condition. The THROW fires on every "not found" result, flooding the immune system with false positives.

```c
// Line 125 - fires every time bridge is not found (normal search result)
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
    "find_bridge_by_id: validation failed");
return NULL;
```

---

### P2-7: False positive THROW on bool check "cat_cfg->enabled is NULL"

**File**: `src/cognitive/free_energy/nimcp_fep_orchestrator.c:138-139`
**Category**: False positive NIMCP_THROW_TO_IMMUNE
**Description**: `cat_cfg->enabled` is a `bool` field, not a pointer. When `enabled == false`, the THROW fires with the message "cat_cfg->enabled is NULL", which is nonsensical. A disabled category is a normal configuration state, not an error.

```c
// Line 138-139 - bool treated as pointer
if (!cat_cfg->enabled) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "category_needs_update: cat_cfg->enabled is NULL");
    return false;
}
```

**Fix**: Remove the THROW entirely; `enabled == false` is normal behavior. Just `return false;`.

---

### P2-8: False positive THROW in `find_primitive_by_id`, `find_primitive_by_name`, `find_rule_by_id`

**File**: `src/cognitive/extrapolation/nimcp_compositional_systematic.c:157,176,195`
**Category**: False positive NIMCP_THROW_TO_IMMUNE
**Description**: Three search functions that iterate arrays looking for matches. When no match is found, they fire NIMCP_THROW_TO_IMMUNE with "validation failed" message. This is a normal "not found" search result, not an error. The error codes used (`NIMCP_ERROR_NULL_POINTER` and `NIMCP_ERROR_INVALID_PARAM`) are also wrong -- these are not null pointer or invalid parameter situations.

```c
// Line 157 - normal "not found" case
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
    "find_primitive_by_id: validation failed");
return NULL;
```

---

### P2-9: False positive THROW on `scenario->is_active` (bool check with "is NULL" message)

**File**: `src/cognitive/imagination/nimcp_imagination_engine.c:1365-1366`
**Category**: False positive NIMCP_THROW_TO_IMMUNE
**Description**: `scenario->is_active` is a bool field. When the scenario is inactive (`is_active == false`), the THROW fires with "scenario->is_active is NULL". An inactive scenario is a valid state, not an error. Same issue at line 3678.

```c
if (!scenario->is_active || scenario->is_paused) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "unknown: scenario->is_active is NULL");
    return -1;
}
```

---

### P2-10: Wrong function name "imagination_engine_reset:" in init_for_brain

**File**: `src/cognitive/imagination/nimcp_imagination_engine.c:570,581`
**Category**: Wrong function name in error message
**Description**: The function `imagination_engine_init_for_brain()` has THROW messages that say "imagination_engine_reset:" instead of the correct function name.

---

### P2-11: `nimcp_mutex_destroy()` used on heap-allocated mutexes (should be `nimcp_mutex_free()`)

**Files** (7 occurrences in neuro_symbolic module):
- `src/cognitive/neuro_symbolic/nimcp_hypergraph.c:308`
- `src/cognitive/neuro_symbolic/nimcp_quantum_mcts.c:272`
- `src/cognitive/neuro_symbolic/nimcp_energy_consistency.c:200,226`
- `src/cognitive/neuro_symbolic/nimcp_evolutionary_proof.c:414`
- `src/cognitive/neuro_symbolic/nimcp_quantum_math_engine.c:360`
- `src/cognitive/neuro_symbolic/nimcp_genius_math_orchestrator.c:249`

**Category**: Memory leak / API misuse
**Description**: All 7 files declare `nimcp_mutex_t* mutex;` (heap-allocated pointer) and create via `nimcp_mutex_create(&attr)`, but destroy with `nimcp_mutex_destroy()`. Per `nimcp_thread.h:201`: "Do NOT use on embedded mutexes - use nimcp_mutex_destroy() instead", which implies `nimcp_mutex_free()` should be used for heap-allocated mutexes (those created by `nimcp_mutex_create()`). Using `nimcp_mutex_destroy()` deinitializes the mutex but does not free the heap allocation, causing a memory leak.

---

### P2-12: `rand_r()` used for PRNG in gt_learning.c (per-instance seed, acceptable but non-ideal)

**File**: `src/cognitive/game_theory/nimcp_gt_learning.c:178,891,1005`
**Category**: Thread-unsafe rand (mitigated)
**Description**: Uses `rand_r(&learner->rand_seed)` where `rand_seed` is a per-learner-instance field. This is technically thread-safe if no two threads share the same learner instance, but it is not using the project-standard `nimcp_tl_rand()`. Unlike the P1-4 case in omni_world_model.c, this seed is persistent per learner instance and does not reset on each call.

**Severity note**: Downgraded from P1 to P2 because the seed is per-instance, not per-call. The primary concern is inconsistency with the project standard.

---

## P3: Quality Issues

### P3-1: Unused variable `level_diff` in omni_world_model.c

**File**: `src/cognitive/omni/nimcp_omni_world_model.c:1191`
**Category**: Dead code
**Description**: `level_diff` is computed as `abs(source_level - target_level)` but never used in the subsequent calculation. Only the `scale` variable (based on a level comparison) is used.

---

### P3-2: Excessive heartbeat instrumentation in tight loops with small bounds

**Files**: Multiple files across all scoped modules
**Category**: Redundant checks
**Description**: Many heartbeat checks guard with `if ((i & 0xFF) == 0 && count > 256)`, which means they never fire for small collections. This is correct defensive behavior, but the pattern is applied even in loops that iterate over 3 neural network layers or other tiny fixed-size arrays. The guard condition prevents actual overhead, so this is purely a code readability concern.

---

## Files Reviewed

### Deeply Reviewed (key sections read, >500 lines)

| File | Lines | Findings |
|------|-------|----------|
| `omni/nimcp_omni_world_model.c` | 4350 | P1-4, P2-1, P2-5, P3-1 |
| `imagination/nimcp_imagination_engine.c` | 3872 | P1-3, P2-3, P2-4, P2-5, P2-9, P2-10 |
| `game_theory/nimcp_gt_spatial.c` | 2654 | Clean (degree==0 properly guarded) |
| `parietal/nimcp_parietal.c` | 2582 | P2-2 |
| `jepa/nimcp_jepa_predictor.c` | 2725 | Clean |
| `neuro_symbolic/nimcp_hypergraph.c` | 2406 | P2-11 |
| `game_theory/nimcp_gt_equilibrium.c` | 2079 | P1-2 |
| `game_theory/nimcp_gt_learning.c` | 2611 | P2-12 |
| `recursive/nimcp_rcog_orchestrator.c` | 2295 | Clean |
| `recursive/nimcp_rcog_delegation_pool.c` | 2324 | Clean |
| `parietal/nimcp_financial_investment.c` | 2261 | Clean (total_cap guarded) |
| `parietal/nimcp_intuitive_reasoning.c` | 2193 | Clean (num_features > 0 checked) |
| `parietal/nimcp_financial_market.c` | 1838 | Clean (total_value guarded) |
| `recursive/nimcp_rcog_engine.c` | 1837 | Clean |
| `jepa/nimcp_jepa_context.c` | 1846 | Clean |
| `jepa/nimcp_jepa_multimodal.c` | 1719 | Clean |
| `game_theory/nimcp_gt_auction_ext.c` | 1718 | Clean |
| `game_theory/nimcp_gt_mechanism.c` | 1726 | Clean (uses nimcp_tl_rand) |
| `omni/nimcp_omni_metacognition.c` | 1661 | Clean (all divzero guarded) |
| `parietal/nimcp_mathematical_intuition.c` | 1618 | Clean |
| `symbolic_logic/nimcp_symbolic_logic_lgss_loader.c` | 1620 | Clean |
| `free_energy/nimcp_fep_orchestrator.c` | 1423 | P2-6, P2-7 |
| `fault_tolerance/nimcp_self_repair.c` | 1592 | Clean |
| `vae/nimcp_vae.c` | 1470 | Clean |
| `vae/nimcp_vae_encoder.c` | ~1000 | P2-1 |
| `extrapolation/nimcp_compositional_systematic.c` | 1686 | P2-8 |
| `game_theory/nimcp_game_theory_snn_bridge.c` | ~800 | P1-1 |
| `neuro_symbolic/nimcp_quantum_mcts.c` | ~1700 | P2-11 |
| `neuro_symbolic/nimcp_energy_consistency.c` | ~1400 | P2-11 |
| `neuro_symbolic/nimcp_evolutionary_proof.c` | ~1300 | P2-5, P2-11 |
| `neuro_symbolic/nimcp_quantum_math_engine.c` | ~1200 | P2-5, P2-11 |
| `neuro_symbolic/nimcp_genius_math_orchestrator.c` | ~600 | P2-11 |

### Pattern-Searched (grep for known bug patterns)

All .c files in the 20+ scoped directories were searched for:
- `rand()` / `srand()` / `rand_r()` (thread-unsafe PRNG)
- Division by variable without zero guard
- `nimcp_mutex_destroy` on heap-allocated mutexes
- `"unknown:"` / `"randn:"` / `"get_time_us:"` / `"tensor_norm:"` / `"brain_get_imagination_engine:"` wrong function names
- `"is NULL"` on bool fields (false positive throws)
- `"not found"` / `"validation failed"` on search functions (false positive throws)

### Modules Reviewed (no findings beyond patterns above)

- `cognitive/vae/` (10+ bridge files) -- uses nimcp_tl_rand(), no raw rand
- `cognitive/creative/` (23 files) -- uses nimcp_tl_rand(), 67 "unknown:" throws (P2-5)
- `cognitive/recursive/` (15 files) -- clean, no raw rand
- `cognitive/free_energy/` (15 files) -- P2-6, P2-7 in orchestrator
- `cognitive/fault_tolerance/` (15 files) -- clean
- `cognitive/jepa/` (12 files) -- uses nimcp_tl_rand()
- `cognitive/logic/` (8 files) -- clean
- `cognitive/symbolic_logic/` (7 files) -- clean
- `cognitive/sleep_wake/` (6 files) -- clean
- `cognitive/epistemic/` (6 files) -- clean
- `cognitive/fractal_cognitive/` (3 files) -- clean
- `cognitive/extrapolation/` (3 files) -- P2-8 in compositional_systematic
- `cognitive/analysis/` (4 files) -- clean
- `cognitive/explanations/` (4 files) -- clean
- `cognitive/predictive_immune/` (3 files) -- clean

---

## Systemic Patterns

### 1. Wrong function names in THROW messages (~175 total occurrences in scope)
The most widespread issue is incorrect function name prefixes in `NIMCP_THROW_TO_IMMUNE` messages. This appears to stem from automated THROW insertion that either copied function names from nearby code ("randn:", "get_time_us:", "tensor_norm:", "brain_get_imagination_engine:") or used a placeholder ("unknown:"). This makes immune system error tracing unreliable for the affected call sites.

### 2. False positive THROW on search "not found" paths (5 occurrences confirmed)
Search/lookup functions that return NULL on "not found" fire NIMCP_THROW_TO_IMMUNE. These are normal code paths, not errors. Confirmed in: `find_bridge_by_id`, `find_primitive_by_id`, `find_primitive_by_name`, `find_rule_by_id`.

### 3. False positive THROW on bool fields with "is NULL" message (3 occurrences confirmed)
Bool fields checked with `!field` trigger THROW with "field is NULL" messages. Confirmed in: `cat_cfg->enabled` (fep_orchestrator), `scenario->is_active` (imagination_engine, 2 locations).

### 4. `nimcp_mutex_destroy()` on heap-allocated mutexes (7 files, all in neuro_symbolic)
All neuro_symbolic module files with mutexes use `nimcp_mutex_create()` (heap allocation) but `nimcp_mutex_destroy()` (deinit without free). Should use `nimcp_mutex_free()`.

### 5. `rand_r()` with per-instance seeds (gt_learning.c, omni_world_model.c)
Two files use `rand_r()` instead of the project-standard `nimcp_tl_rand()`. The gt_learning.c usage is per-learner-instance (acceptable but non-standard). The omni_world_model.c usage at line 1486 is per-call with `time(NULL)` seed (P1 bug).
