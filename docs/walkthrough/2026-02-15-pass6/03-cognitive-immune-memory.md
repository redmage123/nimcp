# Pass 6 Walkthrough: cognitive/immune/ and cognitive/memory/

**Date**: 2026-02-15
**Scope**: `src/cognitive/immune/` (38 files), `src/cognitive/memory/` + `core/` (63 files)
**Method**: Full read of 25+ representative files, systematic grep across all 101 files
**Mode**: REVIEW ONLY -- no edits

---

## Summary Statistics

| Category | immune/ | memory/ | Total |
|----------|---------|---------|-------|
| P1 (Crash/Security) | 2 | 1 | **3** |
| P2 (Logic/Wrong Behavior) | ~120 | ~280 | **~400** |
| P3 (Minor) | 3 | 0 | **3** |
| Files reviewed (read) | 25 | 12 | 37 |
| Files grep-scanned | 38 | 63 | 101 |

### Systemic P2 Breakdown

| Pattern | immune/ instances | memory/ instances |
|---------|-------------------|-------------------|
| Wrong error code: NO_MEMORY for NULL param | 59 across 28 files | 220 across 57 files |
| Wrong error code: NULL_POINTER after alloc fail | 133 across 17 files | ~0 (pattern not found) |
| False positive throw on find/search not-found | 44 across 12 files | 56 across 21 files |
| Wrong error code: NO_MEMORY for required param NULL | 16 across 16 files | - |

---

## P1 Bugs (Crash/Security/Data Corruption)

### P1-01: Deadlock in autobio_immune_bridge_update (ABBA self-deadlock)

**File**: `src/cognitive/immune/nimcp_autobiographical_immune_bridge.c`
**Lines**: 828 (lock), 831/842/847/853 (nested calls that also lock)

`autobio_immune_bridge_update()` acquires `bridge->base.mutex` at line 828, then calls:
- `autobio_immune_apply_inflammation_consolidation_effects()` (line 831) which locks same mutex at line 401
- `autobio_immune_create_sickness_landmark()` (line 842) which locks same mutex at line 490
- `autobio_immune_close_sickness_landmark()` (line 847) which locks same mutex at line 575
- `autobio_immune_apply_cytokine_encoding_effects()` (line 853) which locks same mutex at line 336

`bridge_base_init()` creates a NORMAL (non-recursive) mutex via `nimcp_mutex_init(base->mutex, NULL)`. Locking a NORMAL mutex twice from the same thread is **undefined behavior** (POSIX) -- on Linux with NPTL it causes **self-deadlock** (thread blocks on itself forever).

```c
// Line 828: update() takes the lock
nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

// Line 831: sub-function ALSO takes the lock -> DEADLOCK
autobio_immune_apply_inflammation_consolidation_effects(bridge);
// ... which at line 401 does:
nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);  // BLOCKS FOREVER
```

**Fix**: Either (a) make bridge mutex RECURSIVE via `mutex_attr_t attr = {.type = MUTEX_TYPE_RECURSIVE}`, or (b) create `_unlocked()` variants of the sub-functions and call those from update().

---

### P1-02: Use-after-free in collective_memory add_agent_to_memory realloc

**File**: `src/cognitive/memory/core/nimcp_collective_memory.c`
**Lines**: 215-221

When growing arrays, both `agent_ids` and `agent_versions` are reallocated independently. If one succeeds and the other fails, the error path frees both new pointers but does NOT update `memory->agent_ids` / `memory->agent_versions`, leaving them pointing to the OLD (now potentially freed by realloc) buffer.

```c
uint64_t* new_ids = nimcp_realloc(memory->agent_ids, new_cap * sizeof(uint64_t));
float* new_versions = nimcp_realloc(memory->agent_versions, new_cap * sizeof(float));

if (!new_ids || !new_versions) {
    nimcp_free(new_ids);     // Frees the reallocated buffer...
    nimcp_free(new_versions); // ...but memory->agent_ids is now dangling
    return COLLECTIVE_ERROR_NO_MEMORY;
}
```

If `new_ids` succeeded (realloc moved the data), then `nimcp_free(new_ids)` frees it. But `memory->agent_ids` was NOT updated and now points to freed memory. Subsequent access = use-after-free.

**Fix**: Only assign to `memory->agent_ids` after BOTH reallocs succeed. On failure, free only the one that succeeded (if it differs from the original), don't free the original.

---

### P1-03: Potential crash from bridge->mutex vs bridge->base.mutex mismatch

**File**: `src/cognitive/immune/nimcp_omni_immune_bridge.c`
**Lines**: 429, 431, 442, 444, 455, 457

Query functions lock `bridge->mutex` instead of `bridge->base.mutex`. The create function uses `bridge_base_init()` which initializes `bridge->base.mutex`. If the struct has a separate uninitialized `mutex` field, this accesses uninitialized memory.

```c
// In omni_immune_get_omni_effects (line 429):
nimcp_mutex_lock(bridge->mutex);     // WRONG - should be bridge->base.mutex
// ...
nimcp_mutex_unlock(bridge->mutex);   // WRONG
```

**Severity note**: This is P1 if `bridge->mutex` is a different field that was zeroed by memset (locking a zero-initialized mutex = undefined behavior). It's P2 if the struct layout happens to alias `bridge->mutex` to `bridge->base.mutex`.

---

## P2 Bugs (Logic/Wrong Behavior)

### P2-SYSTEMIC-01: Wrong error code NIMCP_ERROR_NO_MEMORY for NULL parameter checks

**Affected**: 59 instances across 28 immune files, 220 instances across 57 memory files

When a function parameter is NULL (user passed bad input), the code throws `NIMCP_ERROR_NO_MEMORY` instead of `NIMCP_ERROR_NULL_POINTER` or `NIMCP_ERROR_INVALID_PARAM`.

Pattern:
```c
// WRONG - NO_MEMORY implies allocation failure
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
    "bridge_create: required parameter is NULL (immune_system, ...)");

// CORRECT
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
    "bridge_create: required parameter is NULL (immune_system, ...)");
```

**Representative files (immune/)**:
| File | Line(s) |
|------|---------|
| nimcp_introspection_immune_bridge.c | 233 |
| nimcp_knowledge_immune_bridge.c | 280 |
| nimcp_mental_health_immune_bridge.c | 227 |
| nimcp_autobiographical_immune_bridge.c | 231, 481, 643, 708, 742 |
| nimcp_self_model_immune_bridge.c | 238 |
| nimcp_sleep_immune_bridge.c | 230 |
| nimcp_tom_immune_bridge.c | 225 |
| nimcp_emotion_immune_bridge.c | 192 |
| nimcp_curiosity_immune_bridge.c | 254 |
| nimcp_executive_immune_bridge.c | 269 |
| nimcp_brain_immune_fep_bridge.c | 235 |
| nimcp_reasoning_immune.c | 363 |
| nimcp_perception_immune.c | 141 |

**Representative files (memory/)**:
| File | Instances |
|------|-----------|
| nimcp_schemas.c | 17 |
| nimcp_entanglement.c | 15 |
| nimcp_social_memory.c | 11 |
| nimcp_pr_curriculum_bridge.c | 10 |
| nimcp_pr_kg_bridge.c | 10 |
| nimcp_counterfactual.c | 9 |
| nimcp_gist.c | 7 |
| nimcp_theta_gamma.c | 6 |
| nimcp_pr_continual_bridge.c | 6 |
| nimcp_temporal_replay.c | 5 |
| nimcp_systems_consolidation.c | 5 |
| nimcp_spaced_repetition.c | 5 |
| nimcp_procedural.c | 5 |
| nimcp_skill_acquisition.c | 5 |
| nimcp_pr_training_plasticity.c | 5 |
| nimcp_collective_memory.c | 5 |

---

### P2-SYSTEMIC-02: Wrong error code NIMCP_ERROR_NULL_POINTER after allocation failure

**Affected**: 133 instances across 17 immune files

After `nimcp_malloc`/`nimcp_calloc` returns NULL (out of memory), the code throws `NIMCP_ERROR_NULL_POINTER` instead of `NIMCP_ERROR_NO_MEMORY`.

Pattern:
```c
bridge = nimcp_malloc(sizeof(...));
if (!bridge) {
    // WRONG - this is an allocation failure, not a NULL parameter
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    return NULL;
}
```

This pattern appears in nearly ALL immune bridge `_create()` functions, consistently. All the bridge files that have a `_create` function exhibit this on the `nimcp_malloc` failure path (the line after `LOG_MODULE_ERROR("... Allocation failed")`).

| File | Line |
|------|------|
| nimcp_introspection_immune_bridge.c | 246 |
| nimcp_knowledge_immune_bridge.c | 293 |
| nimcp_mental_health_immune_bridge.c | 240 |
| nimcp_autobiographical_immune_bridge.c | 244 |
| nimcp_self_model_immune_bridge.c | 251 |
| nimcp_sleep_immune_bridge.c | 243 |
| nimcp_tom_immune_bridge.c | 238 |
| nimcp_wellbeing_immune_bridge.c | 272 |
| nimcp_emotion_immune_bridge.c | 205 |
| nimcp_curiosity_immune_bridge.c | 267 |
| nimcp_executive_immune_bridge.c | 282 |

Also at error cleanup labels:
| File | Line |
|------|------|
| nimcp_hopfield_memory.c | 379 (error label uses NULL_POINTER for alloc cascade fail) |

---

### P2-SYSTEMIC-03: False positive NIMCP_THROW_TO_IMMUNE on find/search not-found paths

**Affected**: 44 instances across 12 immune files, 56 instances across 21 memory files

`find_*` helper functions throw to the immune system when an item is not found. This is normal search behavior (returning -1/NULL), NOT an error condition. These throws flood the immune system with spurious alerts and can cause O(N) throws per operation (e.g., searching for a non-existent item in a list).

**Immune files with false positive find throws**:

| File | Function(s) | Line(s) |
|------|-------------|---------|
| nimcp_code_immune.c | find_scan_by_id, find_quarantine_by_id, find_history_by_id | multiple |
| nimcp_code_immune_self_repair.c | find_repair_by_id, find_patch_by_id | multiple |
| nimcp_complement_system.c | find_c3b_by_id, find_mac_by_id | 180, 206 |
| nimcp_heal_bridge.c | find_candidate_unlocked, find_chain_unlocked, find_rollback_entry | 131, 158, 185 |
| nimcp_claude_healer.c | find_string | 211, 221 |
| nimcp_brain_immune.c | find_bcell_by_id, find_tcell_by_id, find_antigen_by_id, find_antibody_by_id | multiple |
| nimcp_immune_tolerance.c | find_*_by_id | 2 instances |
| nimcp_surface_immune_bridge.c | find_* | 5 instances |
| nimcp_mucosal_immunity.c | find_* | 3 instances |
| nimcp_immune_vaccine.c | find_* | 4 instances |
| nimcp_immune_exhaustion.c | find_* | 2 instances |
| nimcp_immune_bridge_coordinator.c | find_* | 1 instance |

**Memory files with false positive find throws**:

| File | Function(s) | Instances |
|------|-------------|-----------|
| nimcp_transactive.c | find_agent_entry, find_domain_entry, find_expertise_entry, find_delegation, find_free_delegation | 7 |
| nimcp_schemas.c | find_slot_index, schema_find_by_id, schema_find_by_name | 6 |
| nimcp_procedural.c | find_skill, find_habit, find_free_skill_slot, find_free_habit_slot | 4 |
| nimcp_collective_memory.c | find_agent_index, find_memory_index | 4 |
| nimcp_source_memory.c | find_entry, find_agent | 4 |
| nimcp_prospective_scheduler.c | find_scheduled_by_id, find_conflict_group_for_intention, find_conflict_group_by_id | 3 |
| nimcp_pr_curriculum_bridge.c | find_cache_entry | 3 |
| nimcp_pr_meta_bridge.c | find_task_slot | 3 |
| nimcp_pr_mental_health_bridge.c | find_rumination_pattern, find_or_create_intrusion_record, find_or_create_rumination_pattern | 3 |
| nimcp_social_memory.c | find_person_entry, find_episode_entry | 2 |
| nimcp_pr_immune_bridge.c | find_cleanup_tag_unlocked | 2 |
| nimcp_pr_kg_bridge.c | pr_kg_find_similar_memories, pr_kg_find_similar_by_signature | 2 |
| nimcp_entanglement.c | find_node_unlocked, find_edge_unlocked, find_or_create_node_unlocked, find_node_index | 4 |
| nimcp_pr_attention_bridge.c | find_memory_index | 1 |
| nimcp_pr_plasticity_bridge.c | find_bcm_node | 1 |
| nimcp_skill_acquisition.c | find_state | 1 |
| nimcp_flashbulb.c | find_flashbulb_by_id | 1 |
| nimcp_reconsolidation.c | find_window_by_memory | 1 |
| nimcp_counterfactual.c | counterfactual_find_most_actionable | 1 |
| nimcp_prospective.c | find_intention | 1 |
| nimcp_semantic_memory.c | semantic_memory_find_similar | 2 |

---

### P2-04: Wrong function name in throw messages

**File**: `src/cognitive/immune/nimcp_complement_system.c`

`generate_c3b()` (line ~220) and `generate_c5b()` (line ~254) throw with message containing "find_mac_by_id" instead of their own function name.

---

### P2-05: False positive throw on validation failure

**File**: `src/cognitive/immune/nimcp_claude_healer.c`
**Lines**: 336-338

`validate_fix_code()` throws NIMCP_THROW_TO_IMMUNE on normal validation rejection (code doesn't pass safety check). This is expected behavior, not an error.

---

### P2-06: Wrong error code on validation failure

**File**: `src/cognitive/memory/core/nimcp_flashbulb.c`
**Line**: 229

`flashbulb_create()` throws `NIMCP_ERROR_NO_MEMORY` when `flashbulb_config_validate()` fails. Config validation failure should use `NIMCP_ERROR_INVALID_PARAM`.

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
    "flashbulb_create: flashbulb_config_validate is NULL");
```

The message also incorrectly says "is NULL" -- it's not a NULL check, it's a validation failure.

---

### P2-07: Queries without mutex lock (thread-unsafe reads)

**File**: `src/cognitive/immune/nimcp_tom_immune_bridge.c`
**Lines**: 787-806

Query functions `tom_immune_get_cytokine_effects()` and `tom_immune_get_inflammation_state()` read bridge data without acquiring the mutex, while update functions write under lock. This can cause torn reads of multi-word struct copies.

**File**: `src/cognitive/immune/nimcp_wellbeing_immune_bridge.c`
**Lines**: 773-774, 789-790

Same pattern -- query functions copy data without mutex protection.

---

### P2-08: Wrong error code on landmark close not-found

**File**: `src/cognitive/immune/nimcp_autobiographical_immune_bridge.c`
**Line**: 594

`autobio_immune_close_sickness_landmark()` throws `NIMCP_ERROR_NULL_POINTER` when a landmark is not found by ID. Should be `NIMCP_ERROR_NOT_FOUND` or `NIMCP_ERROR_INVALID_PARAM`.

---

## P3 Bugs (Minor)

### P3-01: nimcp_mutex_free() instead of nimcp_mutex_destroy macro

**Files**:
- `src/cognitive/immune/nimcp_trained_immunity.c` (line 461)
- `src/cognitive/immune/nimcp_immune_tolerance.c` (line 298)
- `src/cognitive/immune/nimcp_regulatory_tcells.c` (line 339)

These files call `nimcp_mutex_free(system->mutex)` in destroy functions instead of using the local `nimcp_mutex_destroy` macro pattern that includes `nimcp_platform_mutex_destroy()` + `nimcp_free()` + NULL assignment. This skips the platform-level mutex cleanup, potentially leaking OS mutex resources.

---

## Files Reviewed (Read)

### immune/ (25 files read)
1. nimcp_brain_immune.c (1500 lines)
2. nimcp_brain_immune_integration.c (500 lines)
3. nimcp_brain_immune_plasticity.c (full)
4. nimcp_brain_immune_fep_bridge.c (500 lines)
5. nimcp_brain_immune_substrate_bridge.c (full)
6. nimcp_brain_immune_thalamic_bridge.c (full)
7. nimcp_brain_immune_tick.c (500 lines)
8. nimcp_attention_immune_bridge.c (full)
9. nimcp_code_immune.c (500 lines)
10. nimcp_code_immune_self_repair.c (500 lines)
11. nimcp_emotion_immune_bridge.c (300 lines)
12. nimcp_curiosity_immune_bridge.c (300 lines)
13. nimcp_executive_immune_bridge.c (300 lines)
14. nimcp_immune_persistence.c (500 lines)
15. nimcp_omni_immune_bridge.c (full, 647 lines)
16. nimcp_self_model_immune_bridge.c (full, 1029 lines)
17. nimcp_sleep_immune_bridge.c (full, 1001 lines)
18. nimcp_tom_immune_bridge.c (full, 1022 lines)
19. nimcp_wellbeing_immune_bridge.c (full, 1057 lines)
20. nimcp_claude_healer.c (full, 1583 lines)
21. nimcp_heal_bridge.c (500 lines)
22. nimcp_complement_system.c (500 lines)
23. nimcp_trained_immunity.c (500 lines)
24. nimcp_immune_tolerance.c (500 lines)
25. nimcp_regulatory_tcells.c (500 lines)
26. nimcp_perception_immune.c (500 lines)
27. nimcp_introspection_immune_bridge.c (full, 913 lines)
28. nimcp_knowledge_immune_bridge.c (full, 1065 lines)
29. nimcp_mental_health_immune_bridge.c (full, 1029 lines)
30. nimcp_autobiographical_immune_bridge.c (full, 1140 lines)

### memory/ (12 files read)
1. nimcp_engram.c (300 lines)
2. nimcp_semantic_memory.c (300 lines)
3. nimcp_temporal_replay.c (300 lines)
4. nimcp_procedural.c (350 lines)
5. nimcp_flashbulb.c (350 lines)
6. nimcp_collective_memory.c (250 lines)
7. nimcp_transactive.c (350 lines)
8. nimcp_prospective_scheduler.c (500 lines)
9. nimcp_hopfield_memory.c (400 lines)
10. nimcp_systems_consolidation.c (450 lines)
11. nimcp_working_memory_snn_bridge.c (400 lines)
12. nimcp_source_memory.c (230 lines - via grep context)

### Supplementary
- `src/utils/bridge/nimcp_bridge_base.c` (confirmed NORMAL mutex type)

---

## Cross-Reference to Previous Passes

- **Pass 5** identified ~500 wrong error codes, ~150 false positive throws systemically. This pass confirms the patterns continue in immune/ and memory/ directories.
- **Pass 4** fixed ~170 false positive throws and ~80 wrong error codes. The immune/ and memory/ directories still have ~100 false positive throws and ~279 wrong error codes combined.
- The P1 deadlock in autobio_immune_bridge_update is newly identified in this pass.
- The P1 realloc use-after-free in collective_memory is newly identified.
