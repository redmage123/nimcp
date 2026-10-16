# Cognitive Layer Full Code Walkthrough Report

**Date**: 2026-02-28
**Scope**: `src/cognitive/` — 68 sub-modules, ~782K lines, ~900 files
**Reviewed**: All 68 sub-modules, ~782K lines — complete coverage
**Method**: 15 parallel review agents, file-by-file reading

---

## Executive Summary

| Severity | Count | Description |
|----------|-------|-------------|
| **CRITICAL** | 45 | Crashes, memory corruption, undefined behavior |
| **HIGH** | 94 | Data races, memory leaks, logic errors affecting correctness |
| **MEDIUM** | 149 | Wrong error codes, missing validation, stubs, numeric issues |
| **LOW** | 81 | Code smells, portability, dead code, cosmetic |
| **Total** | **369** | |

**Overall Quality Score: 5.5 / 10**

The cognitive layer is architecturally ambitious but has systemic quality issues, primarily in thread safety and error handling. Ten cross-cutting patterns account for ~70% of all bugs, meaning targeted template-level fixes would have outsized impact. Notable bright spots: recursive cognition (0 bugs), wellbeing/curiosity/collective_cognition (75% of files clean), and the BRIDGE_BOILERPLATE macro pattern (when fully applied) effectively eliminates entire classes of bridge bugs.

---

## Top 10 Systemic Patterns (Priority Order)

### 1. Missing `bridge_base_init()` — ~25 bridges affected (CRITICAL)

Bridges allocate `bridge_base_t base` via `nimcp_calloc` but never call `bridge_base_init()`, leaving the mutex NULL. Any thread-safe access attempt dereferences NULL.

**Affected modules**: logic (5 bridges), game_theory (2), predictive (1), symbolic_logic (2), mental_health (1), mirror_neurons (1), salience (1), memory (2+), parietal (2+)

**Fix**: Grep for `nimcp_calloc.*bridge` and verify every bridge calls `bridge_base_init()` in create and `bridge_base_cleanup()` in destroy.

### 2. `NIMCP_THROW_TO_IMMUNE` on Normal Code Paths — ~60 instances (HIGH)

Search/lookup functions fire immune exceptions when items are "not found" — a normal condition. This floods the immune system with false positives, degrading its ability to detect real threats.

**Worst offenders**:
- `find_speaker()`, `find_grounded_slot()`, `find_module_state()`, `find_opponent()` — all throw on normal "not found"
- `terms_equal()`, `atoms_equal()` — throw on every comparison mismatch (thousands per inference cycle)
- `mental_health_intervene()` — throws when no intervention needed
- `guardian_monitor_thread` — throws on every clean shutdown
- `nimcp_fairness_is_envy_free()` — throws when envy exists (the expected result)

**Fix**: Remove THROW from all search/comparison/"not found" paths. Reserve THROW for genuine errors only.

### 3. Callbacks Invoked Under Mutex Lock — ~15 bridges (CRITICAL/HIGH)

User-provided callbacks are invoked while the bridge mutex is held. If a callback re-enters the bridge API, deadlock occurs (default mutex is non-recursive).

**Affected**: game_theory (SNN, FEP, plasticity), predictive (SNN, plasticity), attention (plasticity), reasoning (SNN), salience bridges

**Fix**: Copy callback pointer + data under lock, release lock, invoke callback outside lock.

### 4. FEP Bridge `mesh_register` Returns `-1` — 37+ files (HIGH)

All FEP bridge `mesh_register` functions return `-1` for NULL registry instead of `NIMCP_ERROR_NULL_POINTER`. The return type is `nimcp_error_t` and `-1` is not a valid error code.

**Fix**: Global find-replace in all `*_fep_bridge.c` files: change `return -1;` to `return NIMCP_ERROR_NULL_POINTER;` in `mesh_register` functions.

### 5. Mutex Allocated But Never Used — 5+ modules (HIGH)

Some modules create a mutex in `_create()` and destroy it in `_destroy()` but never lock/unlock it in any API function, leaving all shared state unprotected.

**Affected**: mirror_neurons (entire public API), omni world model (all operations), game_theory integration (4 modules), theory_of_mind, social bonds

**Fix**: Add mutex lock/unlock around all public API functions that read or modify shared state.

### 6. `nimcp_free(bridge)` Before `NIMCP_THROW_TO_IMMUNE` — 60+ instances (HIGH)

Pattern: free the bridge struct, then throw to immune. If the immune handler references the bridge (via health agent, global pointer, etc.), this is use-after-free.

**Fix**: Swap order — throw first, then free. Or: save error info, free, return error code without throwing.

### 7. Const-Correctness Violations — 20+ functions (MEDIUM)

`*_get_state()` and `*_get_stats()` functions take `const` bridge pointers but call `nimcp_mutex_lock()`, which modifies the mutex. This is undefined behavior in C.

**Fix**: Remove `const` from function parameters that require mutex locking.

### 8. Instance Health Agent Ignores Instance Parameter — 30+ files (MEDIUM)

`set_instance_health_agent(instance, agent)` stores agent in a global variable, not the instance. All instances share one health agent, defeating per-instance monitoring.

**Fix**: Store agent in the instance struct: `instance->health_agent = agent`.

### 9. Division by Zero in Population Encoding — 5+ SNN bridges (MEDIUM)

`float preferred = (float)n / (neurons_per_dim - 1)` divides by zero when `neurons_per_dim == 1`.

**Affected**: game_theory SNN, predictive SNN, mental_health SNN, salience SNN

**Fix**: `float preferred = (neurons_per_dim > 1) ? (float)n / (neurons_per_dim - 1) : 0.5f;`

### 10. `1u << 32` Undefined Behavior — game_theory (CRITICAL)

Coalition bitmask operations use `1u << n` where `n` can be 32 (NIMCP_GT_MAX_PLAYERS on FULL tier). Shifting a 32-bit value by 32 is UB per C standard.

**Affected files**: nimcp_gt_coalition.c, nimcp_credit_assignment.c, nimcp_gt_auction_ext.c

**Fix**: Use `(uint64_t)1 << n` with bounds check, or cap `n` to 30 for exact enumeration methods.

---

## Per-Module Bug Summary

| Module | Files | C | H | M | L | Total | Key Issues |
|--------|-------|---|---|---|---|-------|------------|
| memory | 70 | 1 | 3 | 4 | 4 | 12 | BRIDGE_BBB_VALIDATE inside NULL check, mutex leaks on error paths, thread-unsafe resonance stats |
| parietal | 77 | 1 | 2 | 4 | 2 | 9 | int literal `5` as `void*`, stub engineering modules, no-op heartbeats |
| immune | 46 | 1 | 3 | 5 | 3 | 12 | TOCTOU race on capacity checks, stale pointer from find outside lock, stack overflow risk |
| omni | 26 | 1 | 3 | 4 | 3 | 11 | Stray union decls in header, mutex never used, NULL state not checked, counterfactual dangling ptr |
| mirror_neurons | 34 | 1 | 3 | 5 | 5 | 14 | Serializing raw pointers, mutex never used, load capacity mismatch, FEP cross-lock |
| game_theory | 22 | 14 | 14 | 15 | 8 | 51 | 1u<<32 UB, argmax type confusion, SNN dim index mismatch, deadlock in callbacks, division by zero |
| reasoning | 28 | 1 | 2 | 4 | 2 | 9 | Compat struct memory corruption risk, bio-async race, knowledge base wrong stat field |
| creative | 23 | 1 | 2 | 0 | 0 | 3 | visual_image_destroy ignores owns_pixels, feedback buffer mutex never used, dangling content ptr |
| recursive | 26 | 0 | 0 | 0 | 0 | 0 | (Clean — no significant issues found) |
| fault_tolerance | 15 | 0 | 0 | 1 | 1 | 2 | THROW on normal not-found |
| salience | 23 | 1 | 2 | 2 | 2 | 7 | SIMD no scalar fallback, plasticity bridge missing mutex, race on eval_time |
| free_energy | 22 | 0 | 2 | 5 | 0 | 7 | training_begin destroys production num_contexts, MCTS infinite loop risk, memory leak |
| jepa | 12 | 0 | 0 | 1 | 1 | 2 | GPU init race on plain bool |
| introspection | 18 | 0 | 1 | 1 | 1 | 3 | Mutex leak on error path, dead heartbeat code, non-portable zero-init |
| ethics | 22 | 0 | 2 | 1 | 0 | 3 | Double NIMCP_THROW on error paths, possible infinite recursion in golden_rule |
| attention | 7 | 0 | 2 | 1 | 2 | 5 | FEP mesh_register -1, data race on attention_width after rwlock, raw pthread usage |
| logic | 8 | 6 | 5 | 4 | 2 | 17 | 5 bridges missing mutex init, wrong mutex field (bridge->mutex vs base.mutex), terms_equal floods immune |
| predictive | 7 | 2 | 2 | 5 | 4 | 13 | Div-by-zero, callback deadlock, memory leak on partial alloc, bio-async dangling pointer |
| symbolic_logic | 7 | 0 | 4 | 2 | 1 | 7 | Missing mutex init (2 bridges), hub bridge race conditions, FEP mesh_register -1 |
| mental_health | 10 | 2 | 6 | 8 | 4 | 20 | Spurious immune throw on shutdown, substrate missing init, negative_prior detection is no-op |
| emotion_tensor | 4 | 0 | 1 | 0 | 1 | 2 | Unsigned enum < 0 check, raw pthread_rwlock |
| personality | 5 | 0 | 1 | 0 | 0 | 1 | Constructor-based bio-async init is unsafe |
| emotion | 6 | 0 | 0 | 1 | 1 | 2 | FEP mesh_register -1, stub get_state |
| theory_of_mind | 7 | 0 | 0 | 1 | 0 | 1 | No mutex protection |
| social | 6 | 0 | 0 | 1 | 0 | 1 | No mutex protection |
| autobiographical_memory | 7 | 0 | 1 | 0 | 0 | 1 | Case-insensitive search is actually case-sensitive |
| self_awareness | 5 | 0 | 1 | 1 | 0 | 2 | Potential deadlock in phi monitoring, non-standard mutex creation |
| fractal_cognitive | 3 | 1 | 0 | 2 | 0 | 3 | NULL passed to fractal_get_central_neighbors, O(N²) bubble sort, degree always 1.0 |
| mental_health_monitor | 1 | 2 | 1 | 0 | 0 | 3 | decision_variance always zero, quarantine LR restoration is fragile |
| bias | 6 | 0 | 0 | 1 | 0 | 1 | THROW on normal "not found" |
| top-level FEP bridges | 4 | 0 | 0 | 1 | 0 | 1 | Const-correctness in get_state |
| knowledge | 12 | 2 | 5 | 3 | 2 | 12 | Dangling stack pointer in hyperbolic path, raw pointer serialization, missing NULL checks in lifecycle |
| global_workspace | 8 | 0 | 0 | 3 | 3 | 6 | 3 bridges missing bridge_base_init (substrate, thalamic, Shannon), Shannon get_current_time_ms is counter not real time, Shannon global mapping array race |
| executive | 6 | 0 | 2 | 4 | 3 | 9 | executive_load never inits task_mutex, 3 FEP mesh_register -1, unvalidated num_items in load |
| vae | 15 | 2 | 6 | 9 | 5 | 22 | Thread-unsafe static Box-Muller RNG in latent.c, decoder backward is a stub, THROW without return after NaN in loss.c, 4 bridges missing mutex |
| integration | 12 | 2 | 5 | 9 | 4 | 20 | TOCTOU in FEP global init (nimcp_once_t declared but never used), static locals in security query handler, 4 bio-async handler stubs, dead code after return in 9 destroy functions |
| imagination | 10 | 0 | 4 | 2 | 0 | 6 | Memory leak in scenario error path, 3 bridges memset over tensor pointers without freeing |
| wellbeing | 20 | 2 | 1 | 0 | 0 | 3 | Stray `}` and orphaned code at file scope in sleep_bridge.c and mental_health_bridge.c (incomplete macro migration), nimcp_mutex_create with no args |
| curiosity | 11 | 0 | 0 | 3 | 0 | 3 | THROW on GPU unavailable (normal condition), platform mutex instead of thread mutex, raw calloc/free bypasses tracking |
| collective_cognition | 16 | 1 | 0 | 1 | 0 | 2 | Stray comment fragments at file scope in internal.h, strict aliasing violation via float* to uint32_t* cast |
| neuro_symbolic | 15 | 0 | 2 | 8 | 1 | 11 | eps_random_int off-by-one, thread-unsafe static Box-Muller RNG, 5 THROW on normal lookups, NIMCP_ERROR_MEMORY vs NIMCP_ERROR_NO_MEMORY |
| 30 other small modules | ~80 | 0 | 1 | 3 | 2 | 6 | Stub training hooks, missing thread safety, emotion compound duplicates |

---

## CRITICAL Bugs — Full List (45)

| # | Module | File | Line(s) | Description |
|---|--------|------|---------|-------------|
| 1 | memory | working_memory_snn_bridge.c | 705-706 | BRIDGE_BBB_VALIDATE inside `if (!bridge)` — guaranteed NULL deref |
| 2 | parietal | 11 financial files | various | Integer `5` passed as `void*` — UB on 19 call sites |
| 3 | immune | brain_immune_part_core.c | 645-656 | TOCTOU: capacity check outside mutex, array access inside |
| 4 | omni | omni_world_model_internal.h | 173-179 | Stray file-scope union declarations with duplicate names |
| 5 | mirror | mirror_neurons_part_io.c | 87, 274 | Serializing raw pointers — stale pointer on load |
| 6 | salience | salience_part_helpers.c | SIMD block | SIMD functions have no scalar fallback — undefined return |
| 7 | reasoning | reasoning_convergent.c | 85-113 | Compat struct can silently corrupt memory if source struct changes |
| 8 | creative | creative.c | 482-489 | visual_image_destroy ignores owns_pixels — double-free risk |
| 9 | mental_health_monitor | mental_health_monitor.c | 574-575 | decision_variance is always zero (computed after overwrite) |
| 10 | mental_health_monitor | mental_health_monitor.c | quarantine fn | LR restoration `*= 10` doesn't match original LR |
| 11 | fractal_cognitive | nimcp_fractal_cognitive.c | 506 | NULL passed to fractal_get_central_neighbors |
| 12 | game_theory | gt_spatial.c | 1398, 1454 | set_custom_network never reallocates (dead code → buffer overflow) |
| 13 | game_theory | gt_learning.c | 1498, 2089 | argmax() casts uint32_t* to float* — type confusion |
| 14 | game_theory | gt_coalition.c | 301, 985 | `1u << n` when n==32 — undefined behavior |
| 15 | game_theory | gt_coalition.c | 1509, 1541 | `1u << num_players` when ==32 — UB |
| 16 | game_theory | gt_mechanism.c | 680, 731 | Division by zero when probability type is 0.0 |
| 17 | game_theory | snn_bridge.c | 482-500 | SNN encode writes to wrong dimension indices |
| 18 | game_theory | snn_bridge.c | 631-658 | Callbacks invoked inside mutex — deadlock risk |
| 19 | game_theory | fep_bridge.c | 674, 721 | Callbacks invoked inside mutex — deadlock risk |
| 20 | game_theory | plasticity_bridge.c | 527-529 | Callback invoked inside mutex — deadlock risk |
| 21 | game_theory | gt_tom.c | 260-261 | THROW on normal opponent not-found path |
| 22 | game_theory | gt_global_workspace.c | 105-106 | THROW on normal module not-found path |
| 23 | game_theory | gt_auction_ext.c | 411, 467 | `1u << idx` when idx > 63 — UB |
| 24 | game_theory | nimcp_auction.c | 119 | Integer overflow in max_bids → heap buffer overflow |
| 25 | game_theory | credit_assignment.c | 236, 512 | `1u << n` when n==32 — UB |
| 26 | logic | omni_logic_bridge.c | 375, 378, 432 | `bridge->mutex` instead of `bridge->base.mutex` — wrong field |
| 27 | logic | logic_thalamic_bridge.c | 95-112 | No bridge_base_init — mutex is NULL |
| 28 | logic | somatosensory_logic_bridge.c | 177-209 | No bridge_base_init — mutex is NULL |
| 29 | logic | audio_logic_bridge.c | 187-219 | No bridge_base_init — mutex is NULL |
| 30 | logic | visual_logic_bridge.c | 153-186 | No bridge_base_init — mutex is NULL |
| 31 | logic | logic_substrate_bridge.c | 104-132 | No bridge_base_init — mutex is NULL |
| 32 | predictive | predictive_snn_bridge.c | 435 | Division by zero when neurons_per_dim == 1 |
| 33 | predictive | predictive_snn_bridge.c | 639-653 | Callbacks invoked inside mutex — deadlock risk |
| 34 | predictive | predictive_plasticity_bridge.c | 534-536 | Callback invoked inside mutex — deadlock risk |
| 35 | mental_health | guardian.c | 803 | Spurious THROW on every clean guardian shutdown |
| 36 | mental_health | substrate_bridge.c | 99-128 | No bridge_base_init — mutex is NULL |
| 37 | knowledge | knowledge_hyperbolic.c | various | Dangling stack pointer — hierarchical path returned from local buffer |
| 38 | knowledge | knowledge_part_lifecycle.c | various | Missing NULL checks on critical allocations in create path |
| 39 | vae | vae_latent.c | various | Thread-unsafe static Box-Muller RNG — `static bool has_spare` shared across all instances |
| 40 | vae | vae_loss.c | various | NIMCP_THROW_TO_IMMUNE after NaN detection without return — execution continues with NaN data |
| 41 | integration | integration_fep.c | global init | TOCTOU race — `nimcp_once_t` declared but never used, multiple threads can init simultaneously |
| 42 | integration | security_query_handler.c | various | Static local variables in query handler — thread-unsafe shared state |
| 43 | wellbeing | wellbeing_sleep_bridge.c | 28-34 | Stray `}` and orphaned code at file scope — incomplete BRIDGE_BOILERPLATE macro migration |
| 44 | wellbeing | wellbeing_mental_health_bridge.c | 26-32 | Stray `}` and orphaned code at file scope — incomplete BRIDGE_BOILERPLATE macro migration |
| 45 | collective | collective_cognition_internal.h | 37-38 | Stray comment fragments outside comment block at file scope — syntax error |

---

## Recommended Fix Priority

### Phase 1 — Immediate (prevents crashes + corruption)
1. Add `bridge_base_init()` to all ~28 bridges missing it (including knowledge_substrate, gw_thalamic, gw_substrate, 4 VAE bridges)
2. Fix `bridge->mutex` → `bridge->base.mutex` in omni_logic_bridge
3. Fix `1u << 32` UB in coalition/credit/auction (use uint64_t + bounds check)
4. Fix argmax type confusion (uint32_t* → float*)
5. Fix visual_image_destroy owns_pixels check
6. Fix BRIDGE_BBB_VALIDATE placement (move outside NULL check)
7. Add scalar fallback for SIMD functions in salience
8. Fix SNN encode dimension index mismatch
9. Fix mirror_neurons pointer serialization
10. Fix dangling stack pointer in knowledge_hyperbolic.c
11. Fix THROW without return after NaN in vae_loss.c
12. Fix TOCTOU race in integration FEP global init (wire up nimcp_once_t)
13. Delete stray `}` / orphaned code in wellbeing sleep_bridge.c + mental_health_bridge.c
14. Fix stray comment fragments in collective_cognition_internal.h
15. Fix thread-unsafe static Box-Muller RNG in vae_latent.c and quantum_math_engine.c

### Phase 2 — High Priority (prevents deadlocks + data races)
1. Move callback invocations outside mutex locks (~15 bridges)
2. Add mutex lock/unlock to modules with unused mutexes (mirror, omni, etc.)
3. Fix TOCTOU races in immune system (move capacity checks inside lock)
4. Fix `nimcp_free` before `NIMCP_THROW_TO_IMMUNE` ordering (~60 instances)
5. Fix executive_load to initialize task_mutex after deserialization
6. Free tensor pointers before memset in 3 imagination bridges (hippocampus, jepa, sleep)
7. Fix eps_random_int off-by-one in evolutionary_proof.c
8. Fix nimcp_mutex_create() called with no args in wellbeing_sleep_bridge.c
9. Fix static locals in integration security_query_handler.c
10. Implement VAE decoder backward (currently stub — blocks training)

### Phase 3 — Quality (correctness + maintainability)
1. Fix FEP mesh_register `-1` → `NIMCP_ERROR_NULL_POINTER` (42+ files, including 5 newly found)
2. Remove THROW from normal not-found/comparison paths (~70+ instances, including imagination workspace, curiosity GPU, neuro_symbolic lookups)
3. Fix division-by-zero guards in SNN population encoding (5 bridges)
4. Fix const-correctness violations (20+ functions)
5. Fix mental_health decision_variance (compute before overwrite)
6. Fix negative_prior detection no-op
7. Implement instance-level health agent storage
8. Fix Shannon get_current_time_ms (counter, not real time) in global_workspace
9. Fix strict aliasing violation in collective_cognition_immune_bridge.c (use memcpy)
10. Replace raw calloc/free with nimcp_calloc/nimcp_free in curiosity information_forager.c
11. Replace nimcp_platform_mutex_lock with nimcp_mutex_lock in curiosity_fep_bridge.c

### Phase 4 — Cleanup
1. Delete dead code files (disorder_detectors.c, interventions.c)
2. Wire stub training hooks or document as intentional no-ops
3. Replace raw pthread_rwlock with nimcp abstractions
4. Fix wrong error codes (NIMCP_ERROR_NO_MEMORY for param validation, NIMCP_ERROR_MEMORY → NIMCP_ERROR_NO_MEMORY in neuro_symbolic)
5. Remove dead code after return in 9 integration destroy functions
6. Wire 4 bio-async handler stubs in integration module
7. Implement VAE visual/training bridge stubs or document as planned
