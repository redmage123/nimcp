# Walkthrough Pass 4: Glial, Integration, and Miscellaneous Headers

**Date**: 2026-02-10
**Reviewer**: Claude Opus 4.6
**Scope**: `src/glial/` (~27 files), `src/integration/` (~47 files), `include/security/` (sampled), `include/utils/bridge/` (2 files), `include/core/brain/` (sampled)
**Directories confirmed empty**: `src/fault_tolerance/`, `src/common/`

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| **P1** | 3 | Division by zero, duplicate symbols, deadlock |
| **P2** | 21 | Memory leaks, false positive throws, wrong error codes, misleading messages |
| **P3** | 5 | Hardcoded values, non-resettable counters, self-include, stub throws |
| **Total** | **29** | |

---

## P1 - Critical / Crash

### P1-01: Division by zero in `astrocyte_network_get_stats()`

**File**: `/home/bbrelin/nimcp/src/glial/astrocytes/nimcp_astrocytes.c`
**Line**: 1472-1474
**Impact**: Crash (floating-point exception or NaN propagation)

```c
// Line 1452-1474: NULL check exists for network, but NOT for num_astrocytes == 0
if (!network) {
    if (avg_calcium) *avg_calcium = 0.0F;
    ...
    return;
}
// ... loop from 0 to num_astrocytes ...
// Line 1472: Division by potentially-zero num_astrocytes
if (avg_calcium) *avg_calcium = sum_ca / network->num_astrocytes;
// Line 1474:
if (avg_glutamate) *avg_glutamate = sum_glu / network->num_astrocytes;
```

**Fix**: Add `if (network->num_astrocytes == 0)` guard after the NULL check, zeroing all outputs and returning early.

---

### P1-02: Duplicate symbol definitions between astrocyte files

**File**: `/home/bbrelin/nimcp/src/glial/astrocytes/nimcp_astrocytes.c` and `/home/bbrelin/nimcp/src/glial/astrocytes/nimcp_astrocytes_refactored.c`
**Impact**: Linker error (multiple definition) if both files are compiled into the same target

Both files define the following functions with identical signatures:
- `astrocyte_destroy()`
- `astrocyte_update_calcium()`
- `astrocyte_propagate_calcium_wave()`
- `astrocyte_network_destroy()`
- `astrocyte_network_get_stats()` (original only)
- `astrocyte_calcium_system_destroy()` (refactored only, but `astrocyte_network_create()` is duplicate)

The refactored file (`nimcp_astrocytes_refactored.c`) is labeled "Version 3.0.0 - Refactored Edition" and adds async/unified-memory/logging/config/security. The original (`nimcp_astrocytes.c`) has deadlock-safe lock ordering but lacks the refactored features.

**Fix**: One file should be retired (likely the original), or function names should be disambiguated. If both must coexist, use `static` or namespaced prefixes.

---

### P1-03: Deadlock in refactored `astrocyte_propagate_calcium_wave()`

**File**: `/home/bbrelin/nimcp/src/glial/astrocytes/nimcp_astrocytes_refactored.c`
**Lines**: 402-440
**Impact**: Deadlock under concurrent calcium wave propagation

```c
// Line 402: Locks astro->lock FIRST
nimcp_spinlock_lock(&astro->lock);
// ... loop over coupled neighbors ...
    // Line 427: Then locks neighbor->lock (arbitrary order!)
    nimcp_spinlock_lock(&neighbor->lock);
    // ... update ...
    nimcp_spinlock_unlock(&neighbor->lock);
// Line 440:
nimcp_spinlock_unlock(&astro->lock);
```

If thread A propagates astro1->astro2 while thread B propagates astro2->astro1:
- Thread A holds astro1.lock, waits for astro2.lock
- Thread B holds astro2.lock, waits for astro1.lock
- Classic ABBA deadlock

Note: The **original** `nimcp_astrocytes.c` (lines 676-678) correctly prevents this with ID-based lock ordering:
```c
astrocyte_t* first = (astro->id < neighbor->id) ? astro : neighbor;
astrocyte_t* second = (astro->id < neighbor->id) ? neighbor : astro;
nimcp_spinlock_lock(&first->lock);
nimcp_spinlock_lock(&second->lock);
```

**Fix**: Port the lock-ordering pattern from the original file to the refactored version.

---

## P2 - Logic / Correctness

### P2-01: Misleading error messages and wrong error codes in `astrocyte_create()`

**File**: `/home/bbrelin/nimcp/src/glial/astrocytes/nimcp_astrocytes.c`
**Lines**: 451, 458, 1177
**Impact**: Misleading diagnostics; wrong error code sent to immune system

```c
// Line 449-452: Validates coordinates are finite, but message says "is NULL"
if (!isfinite(x) || !isfinite(y) || !isfinite(z)) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED,
        "astrocyte_create: required parameter is NULL (isfinite, isfinite, isfinite)");
    return NULL;
}
// Line 456-459: Validates radius, message says "isfinite is NULL"
if (coverage_radius < 0.0F || !isfinite(coverage_radius)) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED,
        "astrocyte_create: isfinite is NULL");
    return NULL;
}
```

These should use `NIMCP_ERROR_INVALID_PARAM` with messages like "non-finite spatial coordinates" and "invalid coverage radius".

Line 1177 has the identical pattern in `astrocyte_network_find_nearest()`.

**Fix**: Change error code to `NIMCP_ERROR_INVALID_PARAM` and fix messages to describe the actual validation failure.

---

### P2-02: Wrong error code on NULL parameter check

**File**: `/home/bbrelin/nimcp/src/glial/immune/nimcp_astrocyte_immune_plasticity.c`
**Line**: 306
**Impact**: Immune system logs `NIMCP_ERROR_NO_MEMORY` when the actual issue is NULL parameters

```c
if (!immune_system || !astrocyte_system) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
        "astro_plasticity_bridge_create: required parameter is NULL (immune_system, astrocyte_system)");
    return NULL;
}
```

Also at line 314: `NIMCP_ERROR_NULL_POINTER` used for allocation failure (should be `NIMCP_ERROR_NO_MEMORY`). The two error codes are swapped.

**Fix**: Swap error codes: line 306 should use `NIMCP_ERROR_NULL_POINTER`, line 314 should use `NIMCP_ERROR_NO_MEMORY`.

---

### P2-03: Systemic wrong error codes in FEP bridge NULL parameter checks

**Files** (all in `/home/bbrelin/nimcp/src/glial/`):
- `astrocytes/nimcp_astrocytes_fep_bridge.c:46`
- `integration/nimcp_glial_integration_fep_bridge.c:35`
- `myelin_sheath/nimcp_myelin_sheath_fep_bridge.c:35`
- `microglia/nimcp_microglia_fep_bridge.c:37`
- `oligodendrocytes/nimcp_oligodendrocytes_fep_bridge.c:35`

**Impact**: All use `NIMCP_ERROR_NO_MEMORY` for NULL parameter validation, confusing diagnostics

Pattern:
```c
if (!config || !network || !fep_system) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
        "xxx_fep_create: required parameter is NULL (config, network, fep_system)");
    return NULL;
}
```

**Fix**: Change to `NIMCP_ERROR_NULL_POINTER` in all five files.

---

### P2-04: False positive NIMCP_THROW_TO_IMMUNE when pruning is disabled

**File**: `/home/bbrelin/nimcp/src/glial/integration/nimcp_glial_integration.c`
**Line**: 704
**Impact**: Normal "feature disabled" path triggers immune system alert; message says wrong function name

```c
bool glial_integration_should_prune_synapse(...) {
    if (!gi || !gi->enable_microglia_pruning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "glial_integration_get_myelination_factor: required parameter is NULL ...");
        return false;
    }
```

When `enable_microglia_pruning` is false, this is normal behavior (feature disabled), not an error. The error message also references the wrong function name ("get_myelination_factor" vs "should_prune_synapse").

**Fix**: Remove the throw for the `!gi->enable_microglia_pruning` case; keep it only for `!gi`. Fix the function name in the message.

---

### P2-05: False positive NIMCP_THROW_TO_IMMUNE on normal search miss in prune path

**File**: `/home/bbrelin/nimcp/src/glial/integration/nimcp_glial_integration.c`
**Line**: 733
**Impact**: Normal "synapse not assigned to microglia" path triggers immune alert

```c
    // After searching microglia network and not finding assignment:
    nimcp_mutex_unlock(&gi->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
        "glial_integration_get_myelination_factor: operation failed");
    return false;
```

Not finding a microglia assignment for a synapse is a normal condition (not all synapses are monitored). Message also references wrong function name.

**Fix**: Remove the throw; just return false. Fix function name in any retained logging.

---

### P2-06: Pool exhaustion throws as false positive (3 pool allocators)

**Files**:
- `/home/bbrelin/nimcp/src/glial/myelin_sheath/nimcp_myelin_sheath.c:175,290`
- `/home/bbrelin/nimcp/src/glial/microglia/nimcp_microglia.c:1842`
- `/home/bbrelin/nimcp/src/glial/oligodendrocytes/nimcp_oligodendrocytes.c:2182,2312`

**Impact**: Pool exhaustion is a normal runtime condition, not an error. Throws with `NIMCP_ERROR_NULL_POINTER` (wrong code -- nothing is NULL)

```c
// myelin_sheath.c:175 - after scanning all slots and finding none free:
nimcp_spinlock_unlock(&pool->lock);
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
    "myelin_sheath_pool_alloc: operation failed");
return NULL;  // Pool exhausted
```

Same pattern in all five pool allocators.

**Fix**: Remove the throws (pool exhaustion is expected behavior), or change to `NIMCP_ERROR_BUFFER_OVERFLOW` / `NIMCP_ERROR_NO_MEMORY` if alerting is desired.

---

### P2-07: False positive NIMCP_THROW_TO_IMMUNE on subscription not found

**File**: `/home/bbrelin/nimcp/src/glial/integration/nimcp_glial_bio_async_bridge.c`
**Line**: 527
**Impact**: Unsubscribing a module_id not in the subscription list triggers immune alert

```c
int glial_bio_async_unsubscribe_module(..., uint32_t module_id) {
    // ... search loop ...
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
        "glial_bio_async_unsubscribe_module: validation failed");
    return -1;
}
```

Not finding the module in the subscription list may be normal (double-unsubscribe, module already removed).

**Fix**: Remove the throw; return -1 or 0 with optional debug logging.

---

### P2-08: False positive NIMCP_THROW_TO_IMMUNE for non-adjacent layers

**File**: `/home/bbrelin/nimcp/src/integration/core/nimcp_layer_types.c`
**Line**: 134
**Impact**: Querying adjacency of non-adjacent layers triggers immune alert

```c
bool nimcp_layers_are_adjacent(nimcp_layer_id_t a, nimcp_layer_id_t b) {
    // ... extensive adjacency checks ...
    // Line 134: if no adjacency found:
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
        "nimcp_layers_are_adjacent: validation failed");
    return false;
}
```

Returning false (layers not adjacent) is the expected answer for most layer pairs. This is not an error.

**Fix**: Remove the throw; just return false.

---

### P2-09: Inflated layers_registered counter on re-registration

**File**: `/home/bbrelin/nimcp/src/integration/core/nimcp_layer_coordinator.c`
**Line**: 155
**Impact**: `stats.layers_registered` is incremented even when `NIMCP_LAYER_ERR_ALREADY_REGISTERED` is returned

```c
for (size_t i = 0; ...) {
    nimcp_layer_error_t err = nimcp_layer_registry_register_layer(coord->registry, &config);
    if (err != NIMCP_LAYER_OK && err != NIMCP_LAYER_ERR_ALREADY_REGISTERED) {
        return err;
    }
    coord->stats.layers_registered++;  // Runs even when ALREADY_REGISTERED
}
```

**Fix**: Only increment when `err == NIMCP_LAYER_OK`.

---

### P2-10: Partial allocation leak in 5 adapter `_init()` callbacks

**Files**:
1. `/home/bbrelin/nimcp/src/integration/adapters/chemistry/nimcp_calcium_adapter.c:76-81`
2. `/home/bbrelin/nimcp/src/integration/adapters/chemistry/nimcp_metabolic_adapter.c:72-77`
3. `/home/bbrelin/nimcp/src/integration/adapters/chemistry/nimcp_neurotransmitter_adapter.c:74-80`
4. `/home/bbrelin/nimcp/src/integration/adapters/physics/nimcp_ephaptic_coupling_adapter.c:60-65`
5. `/home/bbrelin/nimcp/src/integration/adapters/physics/nimcp_hh_dynamics_adapter.c:60-66`

**Impact**: Memory leak on partial allocation failure

Pattern (calcium_adapter shown):
```c
adapter->calcium_conc = (float*)nimcp_calloc(n, sizeof(float));
adapter->calcium_er = (float*)nimcp_calloc(n, sizeof(float));

if (!adapter->calcium_conc || !adapter->calcium_er) {
    return NIMCP_LAYER_ERR_NO_MEMORY;  // Leaks whichever succeeded
}
```

If the first `calloc` succeeds but the second fails, the first allocation is leaked.

**Fix**: Free any successfully-allocated arrays before returning:
```c
if (!adapter->calcium_conc || !adapter->calcium_er) {
    nimcp_free(adapter->calcium_conc);
    nimcp_free(adapter->calcium_er);
    adapter->calcium_conc = NULL;
    adapter->calcium_er = NULL;
    return NIMCP_LAYER_ERR_NO_MEMORY;
}
```

---

### P2-11: Misleading error messages in sensory KG registration

**File**: `/home/bbrelin/nimcp/src/integration/knowledge/nimcp_sensory_kg_wiring.c`
**Lines**: 279, 360, 452
**Impact**: Error message says "wiring->config is NULL" when the actual condition is feature-disabled

```c
// Line 278-280:
if (!wiring->config.enable_somatosensory) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
        "sensory_kg_register_somatosensory: wiring->config is NULL");
    return -1;
}
```

Same pattern at lines 360 (olfactory) and 452 (gustatory).

**Fix**: Change messages to "feature not enabled" and error code to `NIMCP_ERROR_INVALID_PARAM` or remove the throw (feature being disabled is normal).

---

### P2-12: False positive NIMCP_THROW_TO_IMMUNE on KG edge search misses

**File**: `/home/bbrelin/nimcp/src/integration/knowledge/nimcp_sensory_kg_wiring.c`
**Lines**: 686, 703
**Impact**: Normal "edge not found" returns trigger immune alerts

```c
// sensory_kg_remove_edge - Line 686:
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
    "sensory_kg_remove_edge: validation failed");
return -1;

// sensory_kg_update_edge_weight - Line 703:
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
    "sensory_kg_update_edge_weight: validation failed");
return -1;
```

When the edge_id is not found in the edges array, this is a lookup miss, not a validation failure.

**Fix**: Remove the throws; return -1 with optional debug logging.

---

### P2-13: False positive NIMCP_THROW_TO_IMMUNE on competitive choice

**File**: `/home/bbrelin/nimcp/src/integration/inter/neuromod_gametheory/nimcp_neuromod_gametheory_bridge.c`
**Line**: 450
**Impact**: Every competitive (non-cooperative) decision triggers an immune alert

```c
if (coop_score > 0.5f) {
    bridge->stats.cooperative_choices++;
    return true;
} else {
    bridge->stats.competitive_choices++;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
        "neuromod_gametheory_evaluate_offer: validation failed");
    return false;
}
```

Choosing competition over cooperation is a valid game-theoretic decision, not an error.

**Fix**: Remove the throw from the competitive choice branch.

---

### P2-14: False positive NIMCP_THROW_TO_IMMUNE when no mode switch needed

**File**: `/home/bbrelin/nimcp/src/integration/inter/neuromod_reasoning/nimcp_neuromod_reasoning_bridge.c`
**Line**: 488
**Impact**: Every "no switch needed" evaluation triggers an immune alert

```c
bool neuromod_reasoning_should_switch_mode(...) {
    // ... various conditions that return true ...
    if (bridge->state.error_signal > 0.7f) {
        return true;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
        "neuromod_reasoning_should_switch_mode: validation failed");
    return false;
}
```

When no switching condition is met, returning false ("stay in current mode") is the expected normal outcome.

**Fix**: Remove the throw; just return false.

---

### P2-15: Systemic wrong error codes in immune bridge `_create()` functions

**Files** (all in `/home/bbrelin/nimcp/src/glial/immune/`):
- `nimcp_microglia_immune_bridge.c:197` - `NIMCP_ERROR_NO_MEMORY` for "bridge->base is NULL" (init failure)
- `nimcp_oligodendrocytes_immune_bridge.c:142` - Same pattern
- `nimcp_astrocyte_immune_bridge.c:169` - Same pattern

Also in sleep bridges:
- `nimcp_astrocytes_sleep_bridge.c:378`
- `nimcp_microglia_sleep_bridge.c:267`
- `nimcp_oligodendrocytes_sleep_bridge.c:339`

**Impact**: Uses `NIMCP_ERROR_NO_MEMORY` when the failure is base initialization (which could be any error), not necessarily out-of-memory.

**Fix**: Change to `NIMCP_ERROR_OPERATION_FAILED` or check the actual return from `base_init()`.

---

### P2-16: Systemic wrong error codes in oligodendrocyte/microglia NULL parameter checks

**Files**:
- `/home/bbrelin/nimcp/src/glial/oligodendrocytes/nimcp_oligodendrocytes.c:646,820,843` - `NIMCP_ERROR_NO_MEMORY` for NULL parameter checks
- `/home/bbrelin/nimcp/src/glial/microglia/nimcp_microglia.c:673` - Same pattern

```c
// nimcp_oligodendrocytes.c:820
if (!config) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
        "oligodendrocyte_network_default_config: config is NULL");
}
```

**Fix**: Change to `NIMCP_ERROR_NULL_POINTER`.

---

## P3 - Style / Robustness

### P3-01: Hardcoded queue depth in inter-layer router

**File**: `/home/bbrelin/nimcp/src/integration/core/nimcp_inter_layer_router.c`
**Impact**: The `config.default_queue_depth` field is defined but the actual queue capacity is hardcoded to 256

**Fix**: Use the config value for queue initialization.

---

### P3-02: Non-resettable global sequence counter

**File**: `/home/bbrelin/nimcp/src/integration/core/nimcp_layer_types.c`
**Line**: 167

```c
static uint32_t next_sequence_num = 0;
```

This global counter is never reset, even on system shutdown/restart. In long-running systems it will eventually wrap around (harmless but may confuse logging/debugging). No reset function is provided.

**Fix**: Add a `nimcp_layer_msg_reset_sequence()` function or reset in the layer coordinator shutdown path.

---

### P3-03: KG deserialization stub always throws

**File**: `/home/bbrelin/nimcp/src/integration/knowledge/nimcp_sensory_kg_wiring.c`
**Line**: ~1131

The `sensory_kg_deserialize()` stub function always throws `NIMCP_THROW_TO_IMMUNE`. If called at all, this will trigger a false alarm.

**Fix**: Return an error code indicating "not implemented" without throwing to immune.

---

### P3-04: Self-include in bridge base header

**File**: `/home/bbrelin/nimcp/include/utils/bridge/nimcp_bridge_base.h`
**Line**: 22

```c
#include "utils/bridge/nimcp_bridge_base.h"  // Self-include (redundant)
```

Harmless due to include guards but indicates a copy-paste error.

**Fix**: Remove the self-include.

---

### P3-05: Systemic misleading "operation failed" / "validation failed" messages

Across both `src/glial/` and `src/integration/`, many NIMCP_THROW_TO_IMMUNE calls use generic messages like "operation failed" or "validation failed" that do not describe the actual condition. Examples:
- `astrocyte_calcium_system_create: operation failed` (line 194 of nimcp_astrocyte_calcium.c)
- `microglia_create: operation failed` (line 603 of nimcp_microglia.c)
- `myelin_sheath_pool_alloc: operation failed` (line 175 of nimcp_myelin_sheath.c)

These are not individually harmful but make debugging harder.

**Fix**: Replace with descriptive messages (e.g., "pool exhausted", "base init returned error", "mutex creation failed").

---

## Files Reviewed

### src/glial/ (27 files)

| File | Lines | Status |
|------|-------|--------|
| astrocytes/nimcp_astrocyte_calcium.c | ~500 | P2-01 (misleading messages) |
| astrocytes/nimcp_astrocytes.c | ~1900 | **P1-01** (div-by-zero), **P1-02** (dup symbols), P2-01 |
| astrocytes/nimcp_astrocytes_fep_bridge.c | ~260 | P2-03 (wrong error code) |
| astrocytes/nimcp_astrocytes_refactored.c | ~850 | **P1-02** (dup symbols), **P1-03** (deadlock) |
| astrocytes/nimcp_gliotransmission_pink_noise_bridge.c | ~110 | Clean |
| astrocyte_types/nimcp_astrocyte_types.c | ~50 | Clean |
| immune/nimcp_astrocyte_immune_base.c | ~200 | Clean |
| immune/nimcp_astrocyte_immune_bridge.c | ~700 | Clean |
| immune/nimcp_astrocyte_immune_plasticity.c | ~400 | P2-02 (wrong error code) |
| immune/nimcp_astrocytes_immune_bridge.c | ~100 | Clean |
| immune/nimcp_microglia_immune_bridge.c | ~750 | P2-15 (wrong error code) |
| immune/nimcp_myelin_immune_bridge.c | ~100 | Clean |
| immune/nimcp_oligodendrocytes_immune_bridge.c | ~400 | P2-15 (wrong error code) |
| integration/nimcp_glial_bio_async_bridge.c | ~580 | P2-07 (false positive) |
| integration/nimcp_glial_integration.c | ~1000 | P2-04, P2-05 (false positives, wrong func name) |
| integration/nimcp_glial_integration_fep_bridge.c | ~180 | P2-03 (wrong error code) |
| microglia/nimcp_microglia.c | ~1900 | P2-06, P2-16 (pool exhaustion, wrong code) |
| microglia/nimcp_microglia_fep_bridge.c | ~210 | P2-03 (wrong error code) |
| myelin_sheath/nimcp_myelin_math.c | ~180 | Clean |
| myelin_sheath/nimcp_myelin_sheath.c | ~2400 | P2-06 (pool exhaustion throws) |
| myelin_sheath/nimcp_myelin_sheath_fep_bridge.c | ~180 | P2-03 (wrong error code) |
| nimcp_glial_substrate_bridge.c | ~900 | Clean |
| oligodendrocytes/nimcp_oligodendrocytes.c | ~2400 | P2-06, P2-16 (pool exhaustion, wrong code) |
| oligodendrocytes/nimcp_oligodendrocytes_fep_bridge.c | ~200 | P2-03 (wrong error code) |
| sleep/nimcp_astrocytes_sleep_bridge.c | ~880 | P2-15 (wrong error code) |
| sleep/nimcp_microglia_sleep_bridge.c | ~630 | P2-15 (wrong error code) |
| sleep/nimcp_oligodendrocytes_sleep_bridge.c | ~740 | P2-15 (wrong error code) |

### src/integration/core/ (4 files)

| File | Lines | Status |
|------|-------|--------|
| nimcp_inter_layer_router.c | 309 | P3-01 |
| nimcp_layer_coordinator.c | 411 | P2-09 |
| nimcp_layer_registry.c | 423 | Clean |
| nimcp_layer_types.c | 244 | P2-08, P3-02 |

### src/integration/adapters/ (14 files)

| File | Lines | Status |
|------|-------|--------|
| chemistry/nimcp_calcium_adapter.c | ~95 | P2-10 |
| chemistry/nimcp_metabolic_adapter.c | ~95 | P2-10 |
| chemistry/nimcp_neurotransmitter_adapter.c | ~100 | P2-10 |
| executive/nimcp_basal_ganglia_adapter.c | ~90 | Clean |
| executive/nimcp_pfc_adapter.c | ~90 | Clean |
| memory/nimcp_hippocampus_adapter.c | ~90 | Clean |
| memory/nimcp_pr_memory_adapter.c | ~90 | Clean |
| neuromod/nimcp_neuromod_nucleus_adapter.c | ~90 | Clean |
| neuromod/nimcp_neuromod_pool_adapter.c | ~90 | Clean |
| physics/nimcp_ephaptic_coupling_adapter.c | ~80 | P2-10 |
| physics/nimcp_hh_dynamics_adapter.c | ~80 | P2-10 |
| physics/nimcp_thermodynamics_adapter.c | ~90 | Clean |
| sensory/nimcp_auditory_adapter.c | ~90 | Clean |
| sensory/nimcp_visual_adapter.c | ~90 | Clean |

### src/integration/inter/ (19 bridge files)

| File | Lines | Status |
|------|-------|--------|
| biology_neuromod/nimcp_biology_neuromod_bridge.c | 155 | Clean |
| chemistry_biology/nimcp_chemistry_biology_bridge.c | ~155 | Clean |
| executive_integration/nimcp_executive_integration_bridge.c | 166 | Clean |
| integration_superhuman/nimcp_integration_superhuman_bridge.c | 165 | Clean |
| memory_executive/nimcp_memory_executive_bridge.c | 164 | Clean |
| memory_integration/nimcp_memory_integration_bridge.c | 163 | Clean |
| neuromod_attention/nimcp_neuromod_attention_bridge.c | 488 | Clean |
| neuromod_emotion/nimcp_neuromod_emotion_bridge.c | 619 | Clean |
| neuromod_executive/nimcp_neuromod_executive_bridge.c | 167 | Clean |
| neuromod_gametheory/nimcp_neuromod_gametheory_bridge.c | 694 | P2-13 |
| neuromod_memory/nimcp_neuromod_memory_bridge.c | 160 | Clean |
| neuromod_plasticity/nimcp_neuromod_plasticity_bridge.c | 690 | Clean |
| neuromod_reasoning/nimcp_neuromod_reasoning_bridge.c | 763 | P2-14 |
| neuromod_sensory/nimcp_neuromod_sensory_bridge.c | ~155 | Clean |
| neuromod_wm/nimcp_neuromod_wm_bridge.c | 549 | Clean |
| physics_chemistry/nimcp_physics_chemistry_bridge.c | ~155 | Clean |
| sensory_executive/nimcp_sensory_executive_bridge.c | 163 | Clean |
| sensory_memory/nimcp_sensory_memory_bridge.c | ~155 | Clean |
| superhuman_neuromod/nimcp_superhuman_neuromod_bridge.c | 170 | Clean |

### src/integration/intra/ (9 coordinator files)

| File | Lines | Status |
|------|-------|--------|
| biology/nimcp_biology_intra_coordinator.c | 173 | Clean |
| chemistry/nimcp_chemistry_intra_coordinator.c | 176 | Clean |
| executive/nimcp_executive_intra_coordinator.c | 185 | Clean |
| integration_layer/nimcp_integration_intra_coordinator.c | 191 | Clean |
| memory/nimcp_memory_intra_coordinator.c | 203 | Clean |
| neuromodulatory/nimcp_neuromod_intra_coordinator.c | 196 | Clean |
| physics/nimcp_physics_intra_coordinator.c | 235 | Clean |
| sensory/nimcp_sensory_intra_coordinator.c | 181 | Clean |
| superhuman/nimcp_superhuman_intra_coordinator.c | 238 | Clean |

### src/integration/knowledge/ (1 file)

| File | Lines | Status |
|------|-------|--------|
| nimcp_sensory_kg_wiring.c | 1134 | P2-11, P2-12, P3-03 |

### include/utils/bridge/ (2 files)

| File | Lines | Status |
|------|-------|--------|
| nimcp_bridge_base.h | 733 | P3-04 (self-include) |
| nimcp_bridge_gpu.h | 296 | Clean |

### include/core/brain/ (sampled)

| File | Lines | Status |
|------|-------|--------|
| nimcp_brain_core.h | 32 | Clean |
| nimcp_brain.h | >2000 | Too large to read in full (51,969 tokens) |

### include/security/ (sampled 4 of 49)

| File | Lines | Status |
|------|-------|--------|
| nimcp_security.h | ~200+ | Clean |
| nimcp_blood_brain_barrier.h | ~200+ | Clean |
| nimcp_toctou_guard.h | ~200+ | Clean |
| nimcp_shadow_stack.h | ~200+ | Clean |

---

## Systemic Observations

### 1. False Positive NIMCP_THROW_TO_IMMUNE Epidemic

This walkthrough identified **11 distinct false positive NIMCP_THROW_TO_IMMUNE locations** across glial and integration code:
- Pool exhaustion in object pools (5 locations across 3 files)
- Normal search misses / lookup failures (4 locations)
- Feature-disabled checks (1 location)
- Game-theoretic competitive choices (1 location)
- Reasoning mode "no switch needed" (1 location)

This continues the systemic pattern identified in Pass 2 and Pass 3.

### 2. Wrong Error Code Pattern

At least **15 locations** use the wrong `NIMCP_ERROR_*` code:
- `NIMCP_ERROR_NO_MEMORY` for NULL parameter checks (should be `NULL_POINTER`)
- `NIMCP_ERROR_NULL_POINTER` for allocation failures (should be `NO_MEMORY`)
- `NIMCP_ERROR_NOT_INITIALIZED` for input validation (should be `INVALID_PARAM`)
- `NIMCP_ERROR_NULL_POINTER` for pool exhaustion (should be `NO_MEMORY` or `BUFFER_OVERFLOW`)

### 3. Generic Error Messages

Many throw sites use "operation failed" or "validation failed" as the message, providing no diagnostic value. This is particularly problematic in the glial object pool allocators and integration knowledge graph.

### 4. Integration Layer Architecture Quality

The integration layer is well-architected with clear separation:
- **Core**: Router, coordinator, registry, types -- solid infrastructure
- **Adapters**: Clean, uniform pattern with minor allocation leak issues
- **Inter-layer bridges**: Two tiers (simple template and complex neuromodulatory), both well-implemented
- **Intra-layer coordinators**: All 9 follow identical clean template pattern
- **Knowledge graph**: Good concept but has error-handling issues

### 5. Glial Dual-File Problem

The presence of both `nimcp_astrocytes.c` and `nimcp_astrocytes_refactored.c` with duplicate symbols is the most significant architectural issue. The refactored version adds modern features (async, unified memory, logging, config, security) but loses the deadlock prevention that the original has. This needs to be resolved by merging the best of both.
