# Pass 8 Review: Cognitive Emotion & Social Modules

**Reviewer**: Claude Code Walkthrough
**Date**: 2026-02-15
**Scope**: ALL `.c` files under `src/cognitive/` in the following directories:
emotion, emotions, emotion_tensor, emotion_recognition, emotional_tagging,
empathetic_response, personality, social, theory_of_mind, tom, mirror_neurons,
ethics, bias, wellbeing, mental_health, health, joy, grief, remorse,
love_loyalty_friendship, shadow, shadow_emotions, inner_dialogue, self_model,
self_awareness, self_awareness_extended

**Total files in scope**: ~150+ C source files
**Total lines in scope**: ~130,000+ lines

---

## Summary

| Priority | Count |
|----------|-------|
| P1 (crash/security) | 2 |
| P2 (correctness) | 15 |
| P3 (quality) | 5 |

---

## P1 Findings (Crash / Security)

### P1-1: Division by zero in predict_distress_trajectory (wellbeing_enhanced.c)

**File**: `src/cognitive/wellbeing/nimcp_wellbeing_enhanced.c:709`
**Category**: Division by zero
**Description**: Linear regression computes `(n * sum_x2 - sum_x * sum_x)` as the denominator without checking for zero before dividing. When `n == 1` (guard only requires `history_count >= MIN_HISTORY_FOR_PREDICTION`), the denominator becomes `1 * 0 - 0 * 0 = 0`, producing NaN/Inf that propagates into trajectory classification and all downstream intervention/prediction logic.

```c
// Line 709 - NO denominator guard
float slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
```

**Comparison**: `nimcp_inner_dialogue_convergence.c:155` and `nimcp_wellbeing_eudaimonic.c:527` both correctly guard:
```c
// inner_dialogue_convergence.c:155 - CORRECT
float denom = (float)n * sum_x2 - sum_x * sum_x;
if (fabsf(denom) < 1e-12f) return 0.0f;

// wellbeing_eudaimonic.c:527 - CORRECT
float denominator = n * sum_x2 - sum_x * sum_x;
if (fabsf(denominator) > 0.001f) { ... }
```

**Fix**: Add `if (fabsf(denom) < 1e-6f) { pred->trajectory = TRAJECTORY_STABLE; return 0; }` before the division.

---

### P1-2: mirror_neurons_load() creates system without mutex

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_neurons.c:2180-2378`
**Category**: NULL pointer dereference (crash on lock)
**Description**: `mirror_neurons_load()` deserializes a mirror neuron system from file using `nimcp_calloc`, which zeroes the struct including the `mutex` field. Unlike `mirror_neurons_create()` (line 846) which explicitly creates the mutex, `mirror_neurons_load()` never calls `nimcp_mutex_create()` or `nimcp_mutex_init()`. Any subsequent operation that locks the mutex (e.g., `mirror_neurons_observe()`, `mirror_neurons_execute()`, `mirror_neurons_update()`) will dereference NULL.

Additionally, `mirror_neurons_load()` does not:
- Create or attach SNN bridge
- Create or attach plasticity bridge
- Register with bio-async router

This means a loaded mirror system is only partially functional and will crash when any locking or bridge operation is attempted.

**Fix**: After deserialization, add the same mutex/bridge initialization as `mirror_neurons_create()`:
```c
mirror->mutex = nimcp_mutex_create(NULL);
if (!mirror->mutex) { /* cleanup and return NULL */ }
```

---

## P2 Findings (Correctness)

### P2-1: mirror_neurons_save() serializes raw pointer fields

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_neurons.c:2057`
**Category**: Stale pointer on deserialization
**Description**: `mirror_neurons_save()` uses `fwrite(&mirror->neurons[i], sizeof(mirror_neuron_unit_t), 1, file)` which writes the entire `mirror_neuron_unit_t` struct including the `substrate` pointer and `has_substrate` bool. On load via `mirror_neurons_load()`, these pointer values are stale/invalid addresses from the original process. If any code checks `has_substrate == true` and then dereferences the pointer, this will cause a crash or memory corruption.

**Fix**: Either zero out pointer fields before writing, or use a custom serialization format that skips pointer fields and marks substrate as detached on load.

---

### P2-2: False positive NIMCP_THROW_TO_IMMUNE on pattern_matches() no-match path

**File**: `src/cognitive/ethics/nimcp_combinatorial_harm.c:222`
**Category**: False positive immune throw
**Description**: `pattern_matches()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, ...)` at the end of the function when a pattern simply does not match the input. This is a normal "no match" return path, not an error. The function is called in O(n*m) nested loops in `combinatorial_evaluate()`, flooding the immune system with spurious error reports during every evaluation cycle.

```c
// Line 222 - NORMAL "no match" return treated as error
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_matches: validation failed");
return false;
```

**Fix**: Remove the throw. A `return false` is the correct behavior for "pattern does not match."

---

### P2-3: Wrong error code in pattern_matches() disabled-pattern check

**File**: `src/cognitive/ethics/nimcp_combinatorial_harm.c:205`
**Category**: Wrong error code + false positive throw
**Description**: `pattern_matches()` throws `NIMCP_ERROR_NULL_POINTER` with message "pattern->enabled is NULL" when checking `if (!pattern->enabled)`. The `enabled` field is a `bool`, not a pointer. A disabled pattern is a normal operational state, not an error. The error code is wrong (`NIMCP_ERROR_NULL_POINTER` for a bool check) and the throw itself is a false positive.

```c
if (!pattern->enabled) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "pattern_matches: pattern->enabled is NULL");  // WRONG: bool, not pointer
    return false;
}
```

**Fix**: Remove the throw entirely. Disabled patterns are normal. If a guard is desired, use `return false` without throwing.

---

### P2-4: False positive throw on combinatorial_unregister_pattern() not-found

**File**: `src/cognitive/ethics/nimcp_combinatorial_harm.c:599`
**Category**: False positive immune throw
**Description**: `combinatorial_unregister_pattern()` throws when a pattern_id is not found in the registry. "Not found" is a normal search result, not an error condition. Callers may reasonably attempt to unregister a pattern that was already removed.

**Fix**: Return error code without throwing to immune.

---

### P2-5: False positive throw on find_binding() search miss (mirror_language_bridge)

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_language_bridge.c:231`
**Category**: False positive immune throw
**Description**: `find_binding()` throws `NIMCP_ERROR_NULL_POINTER` with message "find_binding: operation failed" when no binding is found in the hash table. This is called from `add_binding_entry()` (line 245) which expects NULL to mean "create new entry" - so the throw fires on every new binding creation.

```c
// Line 231 - throws on normal "not found" in hash table lookup
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_binding: operation failed");
return NULL;
```

**Fix**: Remove the throw. Return NULL without reporting to immune - callers already handle NULL as "not found."

---

### P2-6: False positive throw on find_free_simulation_slot() full

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_language_bridge.c:453`
**Category**: False positive immune throw + wrong error code
**Description**: `find_free_simulation_slot()` throws `NIMCP_ERROR_NULL_POINTER` with message "bridge->simulations is NULL" when all simulation slots are occupied. The simulations array is not NULL - it is full. This is a capacity limit, not a null pointer. The wrong error code and misleading message will confuse immune system diagnostics.

**Fix**: Remove the throw or change to `NIMCP_ERROR_OUT_OF_RANGE` with accurate message.

---

### P2-7: False positive throw on remove_binding_entry() not-found

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_language_bridge.c:299`
**Category**: False positive immune throw
**Description**: `remove_binding_entry()` throws when the target binding is not found. "Not found" for removal is a normal condition (idempotent removal).

**Fix**: Return false without throwing to immune.

---

### P2-8: False positive throw on find_synapse() search miss (plasticity_bridge)

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.c:171`
**Category**: False positive immune throw + wrong error code + wrong function name
**Description**: `find_synapse()` throws `NIMCP_ERROR_NULL_POINTER` with message "get_time_us: validation failed" when a synapse ID is not found. Three problems:
1. The throw fires on every search miss (normal behavior)
2. Wrong error code (`NIMCP_ERROR_NULL_POINTER` - nothing is NULL)
3. Wrong function name in message (`get_time_us` instead of `find_synapse`)

This function is called from multiple event callbacks (`on_ltp_event`, `on_ltd_event`, `on_consolidation_event`) that handle event synapse IDs that may not exist in this bridge.

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_time_us: validation failed");
return NULL;
```

**Fix**: Remove the throw. Return NULL and let callers handle it (they already check for NULL).

---

### P2-9: False positive throw on find_binding() search miss (mirror_hierarchy)

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_hierarchy.c:153`
**Category**: False positive immune throw
**Description**: Same pattern as P2-5. `find_binding()` throws `NIMCP_ERROR_NULL_POINTER` when a goal-motor binding is not found. This function is called from multiple places that handle NULL return as "no binding exists yet" (e.g., `mirror_hierarchy_update_binding()` at line 590 creates a new binding when NULL is returned).

**Fix**: Remove the throw.

---

### P2-10: False positive throw on mirror_omni_get_agent_state() not-found

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_omni_bridge.c:592`
**Category**: False positive immune throw
**Description**: `mirror_omni_get_agent_state()` throws when the requested agent_id is not found in the tracked agents array. This is a normal query result, not an error.

**Fix**: Remove the throw. Return -1 to indicate "agent not found" without immune reporting.

---

### P2-11: Wrong error code on allocation failure (wellbeing_enhanced.c)

**File**: `src/cognitive/wellbeing/nimcp_wellbeing_enhanced.c:988`
**Category**: Wrong error code
**Description**: `enhanced_wellbeing_create()` throws `NIMCP_ERROR_NULL_POINTER` with message "system is NULL" when `nimcp_malloc()` fails. The correct error code for allocation failure is `NIMCP_ERROR_NO_MEMORY`.

```c
if (!system) {
    NIMCP_LOGGING_ERROR("Failed to allocate enhanced wellbeing system");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");  // WRONG
    return NULL;
}
```

**Fix**: Change to `NIMCP_ERROR_NO_MEMORY`.

---

### P2-12: Wrong error code on allocation failure (mirror_omni_bridge.c)

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_omni_bridge.c:187`
**Category**: Wrong error code
**Description**: `mirror_omni_bridge_create()` throws `NIMCP_ERROR_NULL_POINTER` when `nimcp_calloc()` fails for `action_priors`. The correct error code is `NIMCP_ERROR_NO_MEMORY`.

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
    "mirror_omni_bridge_create: bridge->action_priors is NULL");
```

**Fix**: Change to `NIMCP_ERROR_NO_MEMORY`.

---

### P2-13: False positive throw on inner_dialogue stalled cycles

**File**: `src/cognitive/inner_dialogue/nimcp_inner_dialogue.c:548`
**Category**: False positive immune throw + wrong function name
**Description**: `NIMCP_THROW_TO_IMMUNE` fires when brain cycles are stalled. Stalled cycles are a normal operational state (e.g., brain is busy, context switch pending), not a coding error. The message also contains a wrong function name: "inner_dialogue_engine_reset: validation failed" when the actual function is the cycle check.

**Fix**: Remove the throw. Stalled cycles should be handled as normal control flow.

---

### P2-14: mirror_plasticity_unregister_synapse() throw on not-found

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.c:584`
**Category**: False positive immune throw
**Description**: Throws `NIMCP_ERROR_INVALID_PARAM` when a synapse ID is not found during unregistration. "Not found" is a normal search result for idempotent removal.

**Fix**: Return -1 without throwing.

---

### P2-15: mirror_plasticity_get_synapse() throw on not-found

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.c:610`
**Category**: False positive immune throw
**Description**: Same pattern. Throws when querying a non-existent synapse ID. This is a normal "not found" query result.

**Fix**: Return -1 without throwing.

---

## P3 Findings (Quality)

### P3-1: mirror_neurons_destroy() does not free substrate backings

**File**: `src/cognitive/mirror_neurons/nimcp_mirror_neurons.c:969-1027`
**Category**: Memory leak
**Description**: `mirror_neurons_destroy()` frees stdp_system, resonance_system, and hierarchy_system, but does not free individual neuron substrate backings or the substrate_pool when substrate integration was enabled. If substrate was active, this leaks memory.

**Fix**: Add cleanup for substrate-related allocations in the destroy path.

---

### P3-2: Heartbeat in tight inner loops with small bounds

**File**: Multiple mirror neuron bridge files
**Category**: Unnecessary overhead
**Description**: Many loops with small upper bounds (e.g., `MAX_ACTIVE_SIMULATIONS = 16`, `MAX_PHONEME_TEMPLATES = 64`) have heartbeat checks that will never trigger due to the `> 256` guard. While harmless (the condition short-circuits), these add dead code to tight loops:

```c
// MAX_ACTIVE_SIMULATIONS = 16, so (16 > 256) is always false
if ((i & 0xFF) == 0 && MAX_ACTIVE_SIMULATIONS > 256) {
    mirror_language_bridge_heartbeat(...);
}
```

**Impact**: None (compiler may optimize away). Informational only.

---

### P3-3: Placeholder values in free energy and eudaimonic integrations

**File**: `src/cognitive/wellbeing/nimcp_wellbeing_enhanced.c:516,529,543,551,552,598,613,620,627`
**Category**: Incomplete implementation
**Description**: Multiple subsystems use hardcoded placeholder values instead of querying actual modules:
- `effects->free_energy_level = 0.5f; /* Placeholder */`
- `effects->precision_level = 0.6f; /* Placeholder */`
- `eud->purpose_meaning = 0.6f; /* Would compute from goal system */`
- `eud->mastery = 0.5f; /* Would compute from learning metrics */`

These placeholders mean the free energy, precision, eudaimonic purpose, mastery, connection, and growth metrics are static constants rather than dynamic values. The wellbeing system is computing based on fixed values, which defeats its purpose.

**Impact**: Functional but produces meaningless outputs for these dimensions.

---

### P3-4: Trajectory classification ordering bug in predict_distress_trajectory

**File**: `src/cognitive/wellbeing/nimcp_wellbeing_enhanced.c:714-722`
**Category**: Logic error (unreachable branch)
**Description**: The trajectory classification checks are ordered such that `TRAJECTORY_CRITICAL` (slope > 5x threshold) can never be reached because `TRAJECTORY_WORSENING` (slope > 2x threshold) catches it first:

```c
} else if (slope > TRAJECTORY_THRESHOLD * 2.0f) {
    pred->trajectory = TRAJECTORY_WORSENING;       // catches slope > 2x
} else if (slope > TRAJECTORY_THRESHOLD * 5.0f) {
    pred->trajectory = TRAJECTORY_CRITICAL;         // UNREACHABLE - already caught above
}
```

**Fix**: Swap the order so `TRAJECTORY_CRITICAL` (5x) is checked before `TRAJECTORY_WORSENING` (2x):
```c
} else if (slope > TRAJECTORY_THRESHOLD * 5.0f) {
    pred->trajectory = TRAJECTORY_CRITICAL;
} else if (slope > TRAJECTORY_THRESHOLD * 2.0f) {
    pred->trajectory = TRAJECTORY_WORSENING;
}
```

---

### P3-5: self_model_get() error message is generic

**File**: `src/cognitive/self_model/nimcp_self_model.c:304-305`
**Category**: Quality
**Description**: The THROW message for null parameter validation is just `"if: invalid parameters"` - likely a copy/paste artifact from template code. Should be `"self_model_get: system or model is NULL"`.

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
    "if: invalid parameters");  // Should identify function name
```

---

## Files Reviewed

### Fully reviewed (>500 lines read):
- `nimcp_mirror_neurons.c` (3157 lines)
- `nimcp_combinatorial_harm.c` (2089 lines)
- `nimcp_wellbeing_enhanced.c` (2290 lines - key sections)
- `nimcp_wellbeing.c` (2252 lines - key sections)
- `nimcp_mirror_language_bridge.c` (1750 lines - key sections)
- `nimcp_mirror_omni_bridge.c` (1590 lines - key sections)
- `nimcp_mirror_plasticity_bridge.c` (1583 lines - key sections)
- `nimcp_theory_of_mind.c` (1548 lines - key sections)
- `nimcp_mental_health_guardian.c` (1745 lines - key sections)
- `nimcp_mirror_hierarchy.c` (1100+ lines - key sections)
- `nimcp_self_model.c` (500+ lines)
- `nimcp_ethics.c` (800+ lines)
- `nimcp_ethics_evaluation.c` (300+ lines)
- `nimcp_core_directives.c` (300+ lines)
- `nimcp_shadow_emotions.c` (1465 lines - key sections)
- `nimcp_inner_dialogue.c` (1362 lines - key sections)
- `nimcp_emotional_system.c` (1244 lines - key sections)
- `nimcp_personality.c` (first 300 lines)
- `disorder_detectors.c` (1360 lines - key sections)
- `nimcp_inner_dialogue_convergence.c` (key sections)
- `nimcp_wellbeing_eudaimonic.c` (key sections)

### Scanned via Grep (all in-scope files):
- Thread-unsafe `rand()`/`srand()`: **None found** (all use `nimcp_tl_rand()` or `nimcp_rand_*()`)
- Division by zero patterns: **1 real bug found** (wellbeing_enhanced.c:709)
- False positive NIMCP_THROW_TO_IMMUNE: **11 instances found and reported**
- Wrong error codes: **3 instances found and reported**
- Wrong function names in error messages: **2 instances found** (P2-8, P2-13)

### Not deeply reviewed (small bridge files, <300 lines, or standard boilerplate):
- emotion_tensor/*.c, emotion_recognition/*.c, emotional_tagging/*.c bridges
- empathetic_response/*.c bridges
- bias/*.c bridges
- health/*.c bridges
- joy/*.c, grief/*.c, remorse/*.c, love_loyalty_friendship/*.c
- self_awareness_extended/*.c
- shadow_emotions/*.c (small files)
- Remaining mirror_neurons bridge files (snn_bridge, substrate_bridge, thalamic_bridge, sleep_bridge, resonance, stdp - standard bridge boilerplate)

---

## Systemic Patterns Observed

1. **False positive NIMCP_THROW_TO_IMMUNE on search/lookup misses**: This is the dominant bug pattern in this review. Multiple `find_*()` helper functions throw to the immune system when a search target is not found. This is a project-wide systemic issue also noted in Pass 2-5 reviews.

2. **Wrong error codes**: `NIMCP_ERROR_NULL_POINTER` used for allocation failures (should be `NIMCP_ERROR_NO_MEMORY`) and for bool checks (disabled patterns). Also `NIMCP_ERROR_NULL_POINTER` used when arrays are full (should be `NIMCP_ERROR_OUT_OF_RANGE`).

3. **Wrong function names in THROW messages**: Automated/template-generated throw messages contain wrong function names (e.g., `get_time_us` in `find_synapse`, `inner_dialogue_engine_reset` in cycle check).

4. **Bridge boilerplate consistency**: All bridge files follow a consistent pattern (mesh registration, health agent, bridge_base_t as first field, security setters). This is well-structured and no bridge-specific bugs were found in the boilerplate sections.

5. **Thread safety**: Most modules properly use mutex locking with unlock-on-all-paths patterns. The personality module uses global static state with constructor/destructor lifecycle, which is appropriate for its value-type API.
