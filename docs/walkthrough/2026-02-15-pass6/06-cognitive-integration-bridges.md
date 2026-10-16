# Pass 6 Walkthrough: Cognitive Integration, Recursive, Omni, VAE, and Related Bridges

**Date**: 2026-02-15
**Reviewer**: Claude Opus 4.6 (automated walkthrough)
**Scope**: 16 directory groups, ~170 .c files
**Mode**: Review only (no edits)

## Directories Reviewed

| Directory | File Count |
|-----------|-----------|
| `src/cognitive/integration/` | 24 |
| `src/cognitive/recursive/` | 15 |
| `src/cognitive/omni/` + `omni/bridges/` | 16 |
| `src/cognitive/global_workspace/` | 8 |
| `src/cognitive/health/` | 4 |
| `src/cognitive/fault_tolerance/` | 15 |
| `src/cognitive/collective_cognition/` | 9 |
| `src/cognitive/vae/` + `vae/bridges/` | 22 |
| `src/cognitive/autobiographical_memory/` | 7 |
| `src/cognitive/empathetic_response/` | 6 |
| `src/cognitive/ethics/` | 17 |
| `src/cognitive/free_energy/` | 15 |
| `src/cognitive/meta_learning/` | 5 |
| `src/cognitive/symbolic_logic/` + bridges | 7 |
| `src/cognitive/jepa/` | 12 |
| `src/cognitive/nimcp_*.c` (top-level) | 9 |
| **Total** | **~191** |

---

## P1 Bugs (Crash/Corruption/Race)

| # | File | Line | Type | Description |
|---|------|------|------|-------------|
| 1 | `src/cognitive/recursive/nimcp_rcog_orchestrator.c` | 345 | **False positive throw on normal path** | `detect_cycle_dfs()`: After processing all edges and popping `in_stack[idx]`, the normal "no cycle found" return path at line 345 calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, ...)` BEFORE returning `false`. This is called on EVERY node that completes DFS without a cycle -- O(N) false positive immune alerts per validation call. The throw is also misleading since this is the success path. |
| 2 | `src/cognitive/recursive/nimcp_rcog_orchestrator.c` | 320-322 | **False positive throw on memoization path** | `detect_cycle_dfs()`: When `visited[idx]` is true but `in_stack[idx]` is false, this is the normal DFS memoization case (node already fully processed). The throw `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_cycle_dfs: validation failed")` fires on this normal path. |
| 3 | `src/cognitive/recursive/nimcp_rcog_delegation_pool.c` | 2295 | **False positive throw on normal thread exit** | `worker_thread_func()`: At end of worker loop (line 2295), after `worker->state = RCOG_WORKER_STOPPED`, a `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "worker_thread_func: validation failed")` fires on EVERY normal worker thread shutdown. O(num_workers) false alerts per pool shutdown. |
| 4 | `src/cognitive/recursive/nimcp_rcog_delegation_pool.c` | 425-428 | **False positive throw on empty queue** | `steal_task()`: When the queue has 0 or 1 items (cannot steal), the function throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...)` at line 427. This is a normal "nothing to steal" condition, not an error. Called frequently by work-stealing workers. |
| 5 | `src/cognitive/integration/nimcp_cognitive_bio_async_bridge.c` | 705 | **Thread-unsafe static counter** | `static uint64_t update_count = 0;` in an instance method. Shared across all bridge instances without synchronization. Multiple bridges incrementing causes data races. |
| 6 | `src/cognitive/integration/nimcp_game_theory_executive_bridge.c` | 789 | **Thread-unsafe static counter** | `static uint64_t rec_id_counter = 0;` shared across instances. Concurrent recommendation creation will race on this counter, potentially producing duplicate IDs. |
| 7 | `src/cognitive/integration/nimcp_game_theory_executive_bridge.c` | 682 | **Integer overflow risk** | `size_t utilities_size = num_actions * num_outcomes * sizeof(float);` -- no overflow check on the multiplication chain. If `num_actions` and `num_outcomes` are user-supplied or large, this can wrap around, leading to undersized allocation followed by buffer overflow. |
| 8 | `src/cognitive/vae/nimcp_vae_decoder.c` | 435,437 | **Thread-unsafe `srand()` call** | `srand((unsigned int)seed)` and `srand((unsigned int)time(NULL))` seed the global PRNG state. If multiple VAE decoders call `init_weights()` concurrently, or any other thread uses `rand()`, this causes a race condition. Should use `nimcp_tl_rand()` or a local PRNG. |
| 9 | `src/cognitive/vae/nimcp_vae_encoder.c` | 451,453 | **Thread-unsafe `srand()` call** | Same issue as decoder -- `srand()` modifies global state. The `xavier_init()` function likely calls `rand()` internally, making all weight initialization thread-unsafe. |
| 10 | `src/cognitive/vae/nimcp_vae_bio_async.c` | 674 | **Thread-unsafe static variable** | `static uint64_t last_metrics_us = 0;` shared across instances without synchronization. |
| 11 | `src/cognitive/collective_cognition/nimcp_collective_phi.c` | 145 | **Thread-unsafe static counter** | `static uint64_t counter = 0;` in function body, shared across instances without atomics. |
| 12 | `src/cognitive/collective_cognition/nimcp_extended_mind.c` | 140 | **Thread-unsafe static counter** | `static uint64_t counter = 0;` same pattern. |
| 13 | `src/cognitive/collective_cognition/nimcp_shared_intentionality.c` | 130 | **Thread-unsafe static counter** | `static uint64_t counter = 0;` same pattern. |
| 14 | `src/cognitive/global_workspace/nimcp_global_workspace_shannon.c` | 184,235 | **Thread-unsafe static variables** | `static uint32_t g_num_mappings = 0;` and `static uint64_t counter = 0;` shared without synchronization. |
| 15 | `src/cognitive/integration/nimcp_gw_cognitive_bridge.c` | 157 | **Thread-unsafe static counter** | `static uint64_t counter = 0;` in function body, same pattern. |

**P1 Total: 15**

---

## P2 Bugs (Wrong Behavior/Logic Errors)

### P2-A: Wrong Error Code in NIMCP_THROW_TO_IMMUNE

Uses `NIMCP_ERROR_NULL_POINTER` for conditions that are NOT null pointer errors.

| # | File | Line(s) | Correct Code | Condition |
|---|------|---------|-------------|-----------|
| 1 | `nimcp_rcog_engine.c` | 219 | `NIMCP_ERROR_NOT_FOUND` | `find_request_by_id`: request not found in array |
| 2 | `nimcp_rcog_engine.c` | 238 | `NIMCP_ERROR_CAPACITY_EXCEEDED` | `allocate_request_slot`: no free slots (msg says "is NULL" but slots are full) |
| 3 | `nimcp_rcog_orchestrator.c` | 281 | Remove throw entirely | `task_deps_satisfied`: dependency not completed yet (normal scheduling state) |
| 4 | `nimcp_rcog_delegation_pool.c` | 427 | Remove throw entirely | `steal_task`: queue has <=1 items (normal work-stealing miss) |
| 5 | `nimcp_collective_snn_bridge.c` | 233 | `NIMCP_ERROR_INIT_FAILED` | SNN bridge creation operation failed |
| 6 | `nimcp_gw_snn_bridge.c` | 231 | `NIMCP_ERROR_INIT_FAILED` | GW SNN bridge creation failed |
| 7 | `nimcp_autobio_snn_bridge.c` | 236 | `NIMCP_ERROR_INIT_FAILED` | Autobio SNN bridge creation failed |
| 8 | `nimcp_empathy_snn_bridge.c` | 226 | `NIMCP_ERROR_INIT_FAILED` | Empathy SNN bridge creation failed |
| 9 | `nimcp_global_workspace.c` | 629,645,662,674,703 | `NIMCP_ERROR_INIT_FAILED` | Various create_custom validation failures |
| 10 | `nimcp_global_workspace_shannon.c` | 222 | `NIMCP_ERROR_NOT_FOUND` | get_shannon_state: lookup not found |
| 11 | `nimcp_collective_cognition.c` | 668 | `NIMCP_ERROR_INIT_FAILED` | create validation failed |
| 12 | `nimcp_shared_intentionality.c` | 191,226,272 | `NIMCP_ERROR_NOT_FOUND` | find_goal/commitment/attention: lookup not found |
| 13 | `nimcp_collective_phi.c` | 249 | `NIMCP_ERROR_CAPACITY_EXCEEDED` | get_or_create_flow: slot not available |
| 14 | `nimcp_hyperscanning.c` | 330 | `NIMCP_ERROR_NOT_FOUND` | find_pair: pair not found |
| 15 | `nimcp_cognitive_integration_hub.c` | 139,442,462,483 | Various | find_module and create failures |
| 16 | `nimcp_game_theory_executive_bridge.c` | 201 | `NIMCP_ERROR_CAPACITY_EXCEEDED` | find_or_create_opponent_model: slot full |
| 17 | `nimcp_imagination_reasoning_bridge.c` | 206 | `NIMCP_ERROR_CAPACITY_EXCEEDED` | find_free_scenario_slot: full |
| 18 | `nimcp_imagination_reasoning_bridge.c` | 232 | `NIMCP_ERROR_NOT_FOUND` | find_scenario_by_id: not found |
| 19 | `nimcp_omni_wm_logging_bridge.c` | 703 | `NIMCP_ERROR_INIT_FAILED` | update_effects: operation failed |
| 20 | `nimcp_omni_wm_thalamic_bridge.c` | 679 | `NIMCP_ERROR_INIT_FAILED` | update effects failed |
| 21 | `nimcp_omni_wm_cognitive_bridge.c` | 712 | `NIMCP_ERROR_INIT_FAILED` | default config failed |
| 22 | `nimcp_omni_wm_tom_bridge.c` | 738 | `NIMCP_ERROR_UNKNOWN` | operation failed (also wrong func name "unknown") |
| 23 | `nimcp_omni_wm_kg_bridge.c` | 763 | `NIMCP_ERROR_UNKNOWN` | operation failed (also wrong func name "unknown") |
| 24 | `nimcp_vae_training_bridge.c` | 307 | `NIMCP_ERROR_INIT_FAILED` | VAE training bridge creation failed |
| 25 | `nimcp_symbolic_logic_lgss_loader.c` | 244,506 | `NIMCP_ERROR_INVALID_PARAM` | JSON parse validation failed |
| 26 | `nimcp_executive.c` | 3471 | `NIMCP_ERROR_CAPACITY_EXCEEDED` | executive_replan_mcts capacity exceeded (msg even says "capacity exceeded") |

**Subtotal: ~26 instances of wrong NIMCP_ERROR_NULL_POINTER**

### P2-B: False Positive NIMCP_THROW_TO_IMMUNE

Throws on normal/expected code paths that are not errors.

| # | File | Line | Pattern | Description |
|---|------|------|---------|-------------|
| 1 | `nimcp_rcog_orchestrator.c` | 281 | Dep not met | `task_deps_satisfied`: dep not yet completed is normal scheduling |
| 2 | `nimcp_rcog_orchestrator.c` | 315 | Task not found | `detect_cycle_dfs`: task_id not in subtasks array (could be valid edge to external task) |
| 3 | `nimcp_rcog_orchestrator.c` | 321 | Already visited | DFS memoization (already-visited node) is normal behavior |
| 4 | `nimcp_rcog_orchestrator.c` | 345 | Normal return | DFS node completed without cycle - fires on every non-cycle node |
| 5 | `nimcp_rcog_delegation_pool.c` | 427 | Queue too small | `steal_task`: queue has <=1 items, can't steal (normal) |
| 6 | `nimcp_rcog_delegation_pool.c` | 2295 | Normal thread exit | Worker thread normal shutdown |
| 7 | `nimcp_imagination_reasoning_bridge.c` | 206 | Slot full | `find_free_scenario_slot`: no free slots (capacity issue, not null) |
| 8 | `nimcp_shared_intentionality.c` | 191,226,272 | Lookup miss | find_goal/commitment/attention returns not-found (normal search) |
| 9 | `nimcp_shared_intentionality.c` | 828,1014 | Getter miss | get_goal/get_attention: "found is NULL" on search miss |
| 10 | `nimcp_extended_mind.c` | 493 | Getter miss | get_extension: "found is NULL" on search miss |
| 11 | `nimcp_collective_phi.c` | 249 | Capacity full | get_or_create_flow: no slot (normal capacity limit) |
| 12 | `nimcp_hyperscanning.c` | 330 | Lookup miss | find_pair: not found |
| 13 | `nimcp_global_workspace_shannon.c` | 222 | Lookup miss | get_shannon_state: validation failed (search miss) |

**Subtotal: ~13 false positive throw instances**

### P2-C: Wrong Function Name in NIMCP_THROW_TO_IMMUNE

| # | File | Line | Wrong Name | Should Be |
|---|------|------|-----------|-----------|
| 1 | `nimcp_imagination_reasoning_bridge.c` | 216 | `find_free_scenario_slot_unlocked` | `find_scenario_by_id_unlocked` |
| 2 | `nimcp_omni_wm_tom_bridge.c` | 738 | `unknown` | actual function name |
| 3 | `nimcp_omni_wm_kg_bridge.c` | 763 | `unknown` | actual function name |
| 4 | `nimcp_imagination_engine.c` | 1353 | `unknown` | actual function name |
| 5 | `nimcp_consolidation.c` | 807 | `consolidation_thread_fn` | verify correct |

**Subtotal: ~5 wrong function name instances**

### P2-D: Thread-Unsafe Static Training Variables

Multiple files have static training variables at file scope without synchronization. These are used for accumulating training stats. While less critical than P1 races (training stats are typically advisory), they will produce incorrect stats under concurrent use.

| # | File | Lines | Variables |
|---|------|-------|-----------|
| 1 | `nimcp_collective_cognition.c` | 1426-1428 | `g_collective_cognition_training_steps`, `total_error`, `best_error` |
| 2 | `nimcp_collective_phi.c` | 1204-1206 | `g_collective_phi_training_steps`, `total_error`, `best_error` |
| 3 | `nimcp_extended_mind.c` | 1159-1161 | `g_extended_mind_training_steps`, `total_error`, `best_error` |
| 4 | `nimcp_shared_intentionality.c` | 1536-1538 | `g_shared_intentionality_training_steps`, `total_error`, `best_error` |
| 5 | `nimcp_hyperscanning.c` | 1579-1581 | `g_hyperscanning_training_steps`, `total_error`, `best_error` |

**Subtotal: 5 files, 15 variables**

### P2-E: Dead Code After Return in Destroy Functions

Many bridge destroy functions have `if (!bridge) return;` followed by `NIMCP_LOGGING_DEBUG(...)` that is unreachable because the return exits before the logging. This is a widespread pattern from automated code generation.

**Files affected (in scope directories only):**

| # | File | Lines |
|---|------|-------|
| 1 | `nimcp_cognitive_bio_async_bridge.c` | 394-396 |
| 2 | `nimcp_collective_hub_bridge.c` | 443-445 |
| 3 | `nimcp_game_theory_executive_bridge.c` | 454-457 |
| 4 | `nimcp_imagination_reasoning_bridge.c` | 578-581 |
| 5 | `nimcp_gw_plasticity_bridge.c` | 237-238 |
| 6 | `nimcp_gw_snn_bridge.c` | 311-312 |
| 7 | `nimcp_collective_snn_bridge.c` | 313-314 |
| 8 | `nimcp_collective_plasticity_bridge.c` | 264-265 |
| 9 | `nimcp_collective_cognition_immune_bridge.c` | 288-289 |
| 10 | `nimcp_autobio_substrate_bridge.c` | 136-137 |
| 11 | `nimcp_autobio_snn_bridge.c` | 316-317 |
| 12 | `nimcp_autobio_plasticity_bridge.c` | 268-269 |
| 13 | `nimcp_empathy_snn_bridge (estimated)` | Similar pattern |

**Subtotal: ~13+ files with dead code in destroy functions**

Note: This is NOT a crash bug. The `return` correctly prevents NULL deref. The debug log after the return is simply unreachable. This is technically dead code, not a behavioral error.

**P2 Total: ~62 instances across categories**

---

## P3 Bugs (Minor/Style/Improvement)

| # | File | Line | Type | Description |
|---|------|------|------|-------------|
| 1 | `nimcp_rcog_engine.c` | 1673 | const-cast | `nimcp_mutex_lock(((rcog_engine_t*)engine)->mutex)` -- casting away const in get_progress. Safe but brittle. |
| 2 | `nimcp_rcog_orchestrator.c` | 1657-1660 | const-cast | Same pattern in `get_effective_limits` and `get_stats` and `get_trace`. |
| 3 | `nimcp_rcog_delegation_pool.c` | 1778 | const-cast | Same pattern in `get_stats`. |
| 4 | Multiple bridge files | Various | Heartbeat in getters | Simple getters (e.g., `get_state`, `is_ready`) send heartbeats, which is unnecessary overhead for trivial reads. |
| 5 | `nimcp_rcog_orchestrator.c` | 451-452 | Missing NULL check | When `enable_trace` is true, `orch->trace` is allocated but not checked for NULL before setting `trace_enabled`. If calloc fails, `trace_enabled` is true but `trace` is NULL, leading to potential deref in `add_trace_entry`. |
| 6 | `nimcp_rcog_delegation_pool.c` | 909 | Enum cast | `options->tier_override = (rcog_capability_tier_t)(-1)` -- casting -1 to enum. Works but implementation-defined. |

**P3 Total: ~6 instances**

---

## Statistics Summary

| Category | Count |
|----------|-------|
| **P1 (Crash/Race/Corruption)** | **15** |
| **P2 (Wrong Error Code)** | **~26** |
| **P2 (False Positive Throw)** | **~13** |
| **P2 (Wrong Function Name)** | **~5** |
| **P2 (Thread-unsafe training vars)** | **15 vars in 5 files** |
| **P2 (Dead code in destroy)** | **~13 files** |
| **P2 Total** | **~62** |
| **P3 (Minor)** | **~6** |
| **Grand Total** | **~83** |

---

## Top Priority Fixes

### Critical (Fix First)

1. **`nimcp_rcog_orchestrator.c` lines 320-322, 345**: The `detect_cycle_dfs()` function throws on EVERY normally-traversed node. This means `rcog_orchestrator_validate_decomposition()` generates O(N) false immune alerts per call. The throw at line 345 (normal return after DFS completes) is on the hot path. Remove all three throw calls from this function.

2. **`nimcp_rcog_delegation_pool.c` line 2295**: Worker thread shutdown throws on every normal exit. With N workers per pool, each pool shutdown generates N false immune alerts. Remove this throw.

3. **`nimcp_vae_encoder.c` and `nimcp_vae_decoder.c` `srand()` calls**: Replace with `nimcp_tl_rand()` or a local PRNG seeded from the `seed` parameter. The `xavier_init()` function needs to use thread-safe random as well.

4. **`nimcp_rcog_delegation_pool.c` line 427 `steal_task()` false positive**: This is called in a tight loop by idle workers. Each failed steal attempt generates an immune alert. Remove the throw.

### Important (Fix Soon)

5. **Static counters without synchronization** (P1 items 5,6,10-15): Use `atomic_uint64_t` or protect with module-level mutex.

6. **Integer overflow in game_theory_executive_bridge.c** (P1 item 7): Add overflow check before allocation.

7. **Wrong error codes** (P2-A): Systematic replacement of `NIMCP_ERROR_NULL_POINTER` with correct error codes across ~26 instances.

---

## Systemic Patterns

### Pattern 1: "validation failed" / "operation failed" as Generic Throw Messages
Many THROW_TO_IMMUNE calls use generic messages like "validation failed" or "operation failed" with `NIMCP_ERROR_NULL_POINTER` regardless of the actual condition. This makes debugging difficult and generates misleading immune system data.

### Pattern 2: False Positive Throws in Search/Lookup Functions
Search functions that return NULL on "not found" also throw to immune. In a healthy system, "not found" is a normal outcome, not an error. This pattern generates excessive immune noise.

### Pattern 3: Static File-Scope Counters Without Atomics
Training step counters and internal counters declared as `static uint64_t` at file scope are shared across all instances. These need either atomic operations or mutex protection.

### Pattern 4: `srand()`/`rand()` Instead of Thread-Safe PRNG
The VAE encoder and decoder use `srand()` to seed the global PRNG. The project has `nimcp_tl_rand()` for thread-safe random numbers, which should be used instead.

### Pattern 5: Dead Code After Early Return in Destroy
Destroy functions guard against NULL with `if (!ptr) return;` followed by a debug log that can never execute. The log should be moved BEFORE the return, or removed.
