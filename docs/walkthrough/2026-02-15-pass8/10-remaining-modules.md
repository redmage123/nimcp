# Pass 8 Walkthrough: Remaining Modules Review

**Date**: 2026-02-15
**Scope**: src/physics/, src/glial/, src/swarm/, src/dragonfly/, src/portia/, src/snn/, src/lnn/, src/integration/, src/perception/, src/language/, src/sleep/, src/embodiment/, src/chemistry/, src/biology/, src/quantum/, src/optimization/, src/information/, src/superhuman/, src/lib/
**Files in scope**: ~300+ C source files
**Method**: Read core files in each module; scan bridges/smaller files for patterns

---

## Summary

| Priority | Count |
|----------|-------|
| P1 (crash/security) | 5 |
| P2 (correctness) | 16 |
| P3 (quality) | 6 |
| **Total** | **27** |

---

## P1 Findings (Crash / Security)

### P1-1: SNN Box-Muller log(0) possible crash
- **File**: `src/snn/nimcp_snn_network.c` ~line 963
- **Category**: div-by-zero / domain error
- **Description**: The Box-Muller transform computes `logf(u1)` where `u1 = nimcp_tl_rand() / RAND_MAX`. If `nimcp_tl_rand()` returns 0, `u1 = 0.0f` and `logf(0.0f)` produces `-INFINITY`, leading to `sqrtf(-INFINITY)` = `NaN` which propagates through all weight initialization. The LNN neuron module (`nimcp_lnn_neuron.c` line ~290) has a guard (`u1 < 1e-7f ? 1e-7f : u1`) but the SNN module lacks this guard.
- **Impact**: Weight initialization with NaN propagates to all SNN computation, producing garbage outputs or FPE signals.

### P1-2: dragonfly_version() static buffer data race
- **File**: `src/dragonfly/nimcp_dragonfly.c` line 993-1000
- **Category**: thread-unsafe static
- **Description**: `dragonfly_version()` writes to a `static char version[32]` buffer via `snprintf` on every call. If two threads call this function concurrently, they race on the same buffer. Since `DRAGONFLY_VERSION_*` are compile-time constants, the result is deterministic but the write itself is a data race (undefined behavior per C11).
- **Impact**: Torn reads if one thread reads while another writes. Low probability of actual crash but technically UB.

### P1-3: Portia portia_update() TOCTOU on g_portia_ctx
- **File**: `src/portia/nimcp_portia.c` lines 563-572
- **Category**: TOCTOU race condition
- **Description**: `portia_update()` calls `portia_is_initialized()` which does `atomic_load(&g_portia_ctx)` and checks `ctx->initialized`, then on line 569 does another `atomic_load(&g_portia_ctx)` to get the context. Between these two loads, `portia_destroy()` could run on another thread, setting `g_portia_ctx = NULL` and freeing `ctx`. The second `atomic_load` would then return NULL, and `nimcp_mutex_lock(&ctx->lock)` on line 572 would dereference NULL.
- **Impact**: NULL pointer dereference crash if destroy races with update. The same TOCTOU pattern exists in `portia_get_status()`, `portia_set_tier()`, `portia_set_degradation_level()`, etc.

### P1-4: Portia portia_set_tier() uses g_portia_ctx without atomic load
- **File**: `src/portia/nimcp_portia.c` line 692
- **Category**: data race
- **Description**: `portia_set_tier()` reads `portia_context_t* ctx = g_portia_ctx` (plain load, not atomic) after the `portia_is_initialized()` check. All other functions use `atomic_load(&g_portia_ctx)`. This is a data race with `portia_destroy()` which uses `atomic_store(&g_portia_ctx, NULL)`. The same non-atomic access pattern appears at line 752 in `portia_set_degradation_level()`, line 841 in `portia_set_auto_switching()`, line 743 in `portia_get_current_tier()`, and line 868 in `portia_get_accelerators()`.
- **Impact**: Torn pointer read on architectures where pointer writes are not atomic, potential use-after-free.

### P1-5: Hyperthymesia copy_memory_context partial allocation leak
- **File**: `src/superhuman/nimcp_hyperthymesia.c` lines 214-268
- **Category**: memory leak on error path
- **Description**: `copy_memory_context()` allocates `spatial_context`, `social_context`, `activity_context`, and `semantic_context` sequentially. If any allocation fails (e.g., `activity_context` at line 245), the function returns `false` but does NOT free the previously successful allocations (`spatial_context` at line 224 and `social_context` at line 234). The caller only checks the return value and does not call `free_memory_context()` on failure since the destination was memset to zero before the partial copies.
- **Impact**: Memory leak when allocation fails partway through context copy. Could accumulate under memory pressure.

---

## P2 Findings (Correctness)

### P2-1: dynamical_systems.c wrong error code NIMCP_ERROR_NO_MEMORY for NULL pointer checks
- **File**: `src/physics/dynamics/nimcp_dynamical_systems.c` lines 531, 647, 748, 871, 929
- **Category**: wrong error code
- **Description**: Multiple functions use `NIMCP_ERROR_NO_MEMORY` in the NIMCP_THROW_TO_IMMUNE call when checking if a parameter pointer is NULL. These are NULL pointer validation checks, not memory allocation failures. Should use `NIMCP_ERROR_NULL_POINTER`.
- **Impact**: Incorrect error classification - immune system would treat parameter validation errors as OOM events.

### P2-2: dynamical_systems.c dynsys_bridge_create_wiring always returns NULL
- **File**: `src/physics/dynamics/nimcp_dynamical_systems.c` line 996
- **Category**: broken stub / dead code
- **Description**: `dynsys_bridge_create_wiring()` always calls `NIMCP_THROW_TO_IMMUNE` and returns NULL even when the bridge parameter is valid. This makes the wiring creation function permanently broken.
- **Impact**: Dynamic system bridge wiring can never be created, limiting integration capabilities.

### P2-3: LNN neuron reset_gradients off-by-one in grad_w_tau memset
- **File**: `src/lnn/nimcp_lnn_neuron.c` line 714
- **Category**: off-by-one / undersized memset
- **Description**: `lnn_neuron_reset_gradients()` computes `n_tau = n_inputs + n_recurrent` for the memset size of `grad_w_tau`. However, the allocation at line 270 was `1 + n_inputs + n_recurrent` (the +1 accounts for the bias term tau weight). The memset at line 715 is thus one element short, leaving `grad_w_tau[n_inputs + n_recurrent]` (the bias tau gradient) with a stale value from the previous training step.
- **Impact**: Stale bias tau gradient accumulates across training steps, potentially causing divergent tau learning.

### P2-4: Swarm consensus global vote tracking not per-context
- **File**: `src/swarm/nimcp_swarm_consensus.c` (global scope)
- **Category**: shared mutable state
- **Description**: `g_vote_counts_by_drone[MAX_TRACKED_DRONE_ID]` is a global static array protected by a single global mutex, shared across all consensus context instances. When multiple independent consensus operations run concurrently, they share the same vote tracking state, leading to incorrect vote counts for individual consensus rounds.
- **Impact**: Incorrect consensus outcomes when multiple consensus contexts are used simultaneously.

### P2-5: Flocking find_boid_index false positive NIMCP_THROW_TO_IMMUNE
- **File**: `src/swarm/nimcp_swarm_flocking.c` lines 42-43
- **Category**: false positive throw
- **Description**: `flocking_find_boid_index()` calls `NIMCP_THROW_TO_IMMUNE` when a boid ID is not found. This is a normal search miss (the function returns -1 which callers check), not an error condition. Called from many places where not-found is expected behavior, generating noise in the immune system.
- **Impact**: Immune system receives spurious error reports for normal operation, potentially triggering unnecessary responses.

### P2-6: Portia portia_set_tier() wrong error code for invalid tier
- **File**: `src/portia/nimcp_portia.c` line 688
- **Category**: wrong error code
- **Description**: When `tier >= PLATFORM_TIER_COUNT`, the function calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...)`. This should be `NIMCP_ERROR_INVALID_PARAM` since the issue is an out-of-range tier value, not a NULL pointer.
- **Impact**: Incorrect error classification in immune system.

### P2-7: Portia portia_get_current_tier() no thread safety
- **File**: `src/portia/nimcp_portia.c` line 743
- **Category**: thread-unsafe read
- **Description**: `portia_get_current_tier()` accesses `g_portia_ctx->tier_manager->current_tier` without holding any lock. The `portia_set_tier()` function modifies `current_tier` under `mgr->lock`. Reading without the lock is a data race.
- **Impact**: Could read a torn or stale tier value.

### P2-8: Portia nimcp_ph_get_region wrong error code
- **File**: `src/chemistry/ph/nimcp_ph_dynamics.c` line 349
- **Category**: wrong error code
- **Description**: When `region_id >= system->num_regions`, the function calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ph_get_region: capacity exceeded")`. Should be `NIMCP_ERROR_OUT_OF_RANGE` since this is a bounds check failure, not a NULL pointer.
- **Impact**: Incorrect error classification.

### P2-9: pH dynamics nimcp_ph_get_region wrong error code for uninitialized
- **File**: `src/chemistry/ph/nimcp_ph_dynamics.c` line 344
- **Category**: wrong error code
- **Description**: When `!system->initialized`, the function calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...)`. Should be `NIMCP_ERROR_NOT_INITIALIZED` or `NIMCP_ERROR_INVALID_STATE`.
- **Impact**: Incorrect error classification.

### P2-10: Astrocyte calcium message unaligned payload access
- **File**: `src/glial/astrocytes/nimcp_astrocytes.c` lines 78-79
- **Category**: potential unaligned access
- **Description**: Payload data is accessed via type-punned pointer casts: `*(const uint32_t*)payload` and `*(const float*)(payload + sizeof(uint32_t))`. If `bio_message_header_t` has odd size or padding, the payload pointer may not be aligned for uint32_t/float access. This is undefined behavior on strict-alignment architectures (ARM).
- **Impact**: Bus error / SIGBUS on ARM or other strict-alignment platforms. Works on x86 but is technically UB.

### P2-11: Microglia alert message unaligned payload access
- **File**: `src/glial/microglia/nimcp_microglia.c` lines 97-99
- **Category**: potential unaligned access
- **Description**: Same pattern as P2-10: `*(const uint32_t*)payload`, `*(const uint32_t*)(payload + sizeof(uint32_t))`, `*(const float*)(payload + 2 * sizeof(uint32_t))` without alignment guarantees.
- **Impact**: Same as P2-10.

### P2-12: Embodied simulation execute_step false positive NIMCP_THROW_TO_IMMUNE
- **File**: `src/embodiment/nimcp_embodied_simulation.c` line 335
- **Category**: false positive throw
- **Description**: When effector is not found during simulation step execution, `execute_step()` throws `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execute_step: eff is NULL")`. An effector not being found in a simulation is a normal validation failure (the caller returns false), not a system error worthy of immune system notification.
- **Impact**: Spurious immune system alerts during normal simulation validation.

### P2-13: Layer registry get_module_count wrong error code for unregistered layer
- **File**: `src/integration/core/nimcp_layer_registry.c` line 251-253
- **Category**: wrong error code
- **Description**: `nimcp_layer_registry_get_module_count()` calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "registry->layers is NULL")` when the layer is not registered. This is not a NULL pointer error -- the layer simply has not been registered. Should use `NIMCP_ERROR_NOT_INITIALIZED` or a layer-specific error. The message is also misleading since `registry->layers` is an array member that is always valid.
- **Impact**: Incorrect error classification; misleading diagnostic message.

### P2-14: Inter-layer router get_queue_depth wrong error code for NULL router
- **File**: `src/integration/core/nimcp_inter_layer_router.c` line 226
- **Category**: wrong error code
- **Description**: `nimcp_inter_layer_router_get_queue_depth()` calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "...router is NULL")` when router is NULL. Should use `NIMCP_ERROR_NULL_POINTER`. The check combines both NULL-router and out-of-range layer_id into one branch, using the wrong code for the NULL case.
- **Impact**: Incorrect error classification in immune system.

### P2-15: Cochlea create wrong error code for validation failure
- **File**: `src/lib/perception/nimcp_cochlea.c` line 168
- **Category**: wrong error code
- **Description**: `cochlea_create()` calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_create: validation failed")` when `cochlea_config_validate()` returns an error. Config validation failure is an invalid parameter error, not an out-of-memory condition. Should use `NIMCP_ERROR_INVALID_PARAM` to match what `cochlea_config_validate()` actually returns.
- **Impact**: Incorrect error classification; callers may believe OOM occurred when config was simply invalid.

### P2-16: Layer registry get_module_count wrong error code for NULL registry
- **File**: `src/integration/core/nimcp_layer_registry.c` line 248
- **Category**: wrong error code
- **Description**: `nimcp_layer_registry_get_module_count()` calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "...registry is NULL")` when registry is NULL or layer_id is out of range. The NULL-registry case should use `NIMCP_ERROR_NULL_POINTER`, not `NIMCP_ERROR_OUT_OF_RANGE`. The combined check conflates two different error conditions.
- **Impact**: Incorrect error classification for NULL pointer passed as registry.

---

## P3 Findings (Quality)

### P3-1: thermodynamics s_state_count unbounded increment
- **File**: `src/physics/thermodynamics/nimcp_thermodynamics.c`
- **Category**: missing bounds check
- **Description**: `s_state_count` (thread-local counter for internal states) increments without checking against the maximum array size (256 entries in `s_internal_states`). If more than 256 `nimcp_thermo_init()` calls are made from the same thread, the index overflows the array.
- **Impact**: Stack-allocated array out-of-bounds write if >256 thermodynamic state instances created per thread.

### P3-2: Dragonfly error label uses wrong error code
- **File**: `src/dragonfly/nimcp_dragonfly.c` line 273
- **Category**: misleading error code
- **Description**: The `error:` label in `dragonfly_system_create()` calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_system_create: operation failed")`. This is reached when a subsystem creation fails (which could be any error), not specifically a NULL pointer error. Should use `NIMCP_ERROR_OPERATION_FAILED` to match the message.
- **Impact**: Incorrect error classification in immune system logs.

### P3-3: Dragonfly get_primary_target throws on normal not-found
- **File**: `src/dragonfly/nimcp_dragonfly.c` line 643
- **Category**: false positive throw (borderline P2)
- **Description**: `dragonfly_get_primary_target()` calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, ...)` when no primary target is found. Before any target is locked, this is the normal state. Querying for the primary target when none exists is expected usage.
- **Impact**: Immune system receives error reports for normal "no target yet" queries.

### P3-4: Body ownership position_to_grid potential div-by-zero
- **File**: `src/embodiment/nimcp_body_ownership.c` lines 218-220
- **Category**: potential div-by-zero
- **Description**: `position_to_grid()` divides by `ctx->peripersonal.grid_resolution`. If `peripersonal_range` is 0.0 (or very small), `grid_resolution` (computed as `peripersonal_range * 2.0 / GRID_SIZE`) would be 0.0 (or near-zero), causing division by zero or extreme indices. The `enable_peripersonal` flag and default range of 1.0 makes this unlikely but there is no explicit guard.
- **Impact**: FPE or extreme grid indices if peripersonal_range is set to 0.

### P3-5: Quantum annealing uses time(NULL) for seeding
- **File**: `src/optimization/quantum_annealing/nimcp_quantum_annealing.c` line 107
- **Category**: weak seeding
- **Description**: When `config.seed == 0`, the annealer falls back to `time(NULL)` for seeding. If multiple annealers are created within the same second, they get identical seeds and produce identical optimization trajectories. Should use a higher-resolution time source or mix with thread ID.
- **Impact**: Duplicate optimization runs waste compute when multiple annealers created in quick succession.

### P3-6: Myelin pool create wrong error code for zero capacity
- **File**: `src/glial/myelin_sheath/nimcp_myelin_sheath.c` line 92
- **Category**: wrong error code
- **Description**: `myelin_sheath_pool_create()` calls `NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, ...)` when `capacity == 0`. This is a validation error (invalid parameter), not an OOM condition. Should use `NIMCP_ERROR_INVALID_PARAM`.
- **Impact**: Incorrect error classification.

---

## Module-Level Notes

### Physics (src/physics/)
- **dynamical_systems.c** (1145 lines): 5x wrong error code, 1 broken stub. Otherwise clean with proper bifurcation/Jacobian guards.
- **hodgkin_huxley.c** (1211 lines): Well-implemented. Uses safe_exp, proper dt clamping. Population firing rate division by dt is safe because dt is validated upstream.
- **ephaptic.c** (912 lines): Clean. EPSILON guards for distance divisions, proper realloc.
- **thermodynamics.c** (763 lines): Unbounded state count (P3-1). MIN_VALID_TEMP_K guard prevents temperature div-by-zero.

### Glial (src/glial/)
- **astrocytes.c**: Bio-async handlers have unaligned payload access (P2-10). Otherwise well-structured with platform_once init.
- **microglia.c**: Same unaligned payload pattern (P2-11). RK4 state derivatives correctly structured.
- **oligodendrocytes.c**: Clean bio-async integration. Same payload access pattern but less critical.
- **myelin_sheath.c**: Bitmap pool implementation is sound. Wrong error code for zero capacity (P3-6).

### Swarm (src/swarm/)
- **consensus.c** (1207 lines): Global vote state shared across contexts (P2-4, known architectural issue).
- **flocking.c** (1572 lines): False positive throw on boid not-found (P2-5). Good EPSILON guards for division.

### Dragonfly (src/dragonfly/)
- **dragonfly.c** (1001 lines): Static version buffer race (P1-2). Good mutex discipline throughout. Error label wrong code (P3-2). get_primary_target throw on normal state (P3-3).

### Portia (src/portia/)
- **portia.c** (~1050 lines): TOCTOU race on g_portia_ctx (P1-3). Non-atomic g_portia_ctx reads (P1-4). Wrong error code in set_tier (P2-6). Unprotected tier read (P2-7). Well-structured init/destroy with double-check locking.

### SNN (src/snn/)
- **snn_network.c** (1219 lines): Box-Muller log(0) (P1-1). Stack array `float outputs[256]` has bounds check. Thread-safe PRNG via nimcp_tl_rand().

### LNN (src/lnn/)
- **lnn_neuron.c** (795 lines): Off-by-one in grad_w_tau reset (P2-3). Thread-safe rand_r with TLS seed. Tau division properly guarded.
- **lnn_ode.c** (228 lines): Clean. tensor ODE step always uses Euler (TODO noted).
- **lnn.c** (597 lines): Clean facade with atomic init flag.

### Embodiment (src/embodiment/)
- **body_ownership.c** (1390 lines): Clean implementation. Grid resolution div-by-zero (P3-4). No thread safety (appears single-threaded by design).
- **embodied_simulation.c** (1382 lines): False positive throw in execute_step (P2-12). Well-structured simulation with proper bounds checks.

### Chemistry (src/chemistry/)
- **ph_dynamics.c** (1024 lines): Wrong error codes in nimcp_ph_get_region (P2-8, P2-9). Otherwise comprehensive and well-guarded pH simulation.

### Biology (src/biology/)
- **epigenetics.c**: Clean lifecycle management with proper cascading free on allocation failure. Standard patterns throughout.
- **neurogenesis.c**: Uses nimcp_rand_uniform() for stem cell decisions. Clean allocation/deallocation.

### Information (src/information/)
- **shannon.c**: Clean with safe_log2 and SHANNON_EPSILON guards. Proper malloc/free pairs.
- **cross_modal.c**: Has integer overflow checks for allocation (good). Clean entropy computation.

### Optimization (src/optimization/)
- **quantum_annealing.c**: Box-Muller has proper `u1 < 1e-10F` guard (good). Weak time(NULL) seeding (P3-5). Temperature calculation handles N<=1 edge case.

### Superhuman (src/superhuman/)
- **hyperthymesia.c**: Partial allocation leak in copy_memory_context (P1-5). Hash table and temporal index are well-structured.
- **time_dilation.c**: Clean implementation with mutex for thread safety. Reaction tracker circular buffer is correct.

### Integration Core (src/integration/core/)
- **layer_coordinator.c** (~400 lines): Clean implementation. Good lifecycle management with cascading destroy. Properly delegates to registry and router. connection count incrementing does not check return value of `nimcp_layer_registry_register_connection` (minor: stats could be wrong on failure).
- **inter_layer_router.c** (~310 lines): Fixed-size message queues (256 entries). get_queue_depth wrong error code for NULL (P2-14). Message processing and destroy correctly drain queues. `__sync_fetch_and_add` for thread-safe sequence numbering is good.
- **layer_registry.c** (~300 lines): get_module_count has two wrong error codes (P2-13, P2-16). Otherwise clean with proper bounds checks throughout.
- **layer_types.c** (~240 lines): Clean utility functions. Message create/clone/destroy handle payloads correctly with owned flag. Thread-safe sequence numbers via atomic built-in.

### Integration Adapters and Bridges (src/integration/adapters/, src/integration/inter/, src/integration/intra/)
- ~45 files total, all following consistent adapter/bridge/coordinator patterns.
- Bridge files follow bio-async handler registration via KG-driven wiring callback with legacy fallback.
- Intra-coordinators are lightweight state holders with init/update/shutdown lifecycle.
- No significant issues found in the patterns reviewed.

### Perception (src/perception/, src/lib/perception/)
- **cochlea.c** (~800 lines): Wrong error code for validation failure (P2-15). Otherwise comprehensive and well-structured with multi-species support. Proper graceful degradation when extended hearing fails.
- **visual_cortex.c** (~1500+ lines): Extensive implementation with neuromodulation, CoW support, bio-async, memory pools. Uses `nimcp_tl_rand()` for thread-safe random. Well-guarded with validate_pointer checks.
- **audio_cortex.c** (~1500+ lines): Similar structure to visual_cortex. Cocktail party effect implementation is clean. Phasic/tonic neuromodulator dynamics properly handle NULL state.
- **lip_reading.c** (~800+ lines): Comprehensive lip reading with face detection, viseme classification, audiovisual integration. Proper bounds checking in image pixel access. Clean error label pattern in create.
- **speech_cortex.c**: Not individually reviewed (bridge file pattern).
- Bridge files (immune, sleep, cortical, FEP, JEPA): Follow consistent bio-async registration patterns. No significant issues in sampled files.

### Language (src/language/)
- **language_orchestrator.c** (~400+ lines read): Clean state machine implementation with full bridge lifecycle management. Proper cascading start/stop for all bridges. Uses 0/-1 return convention correctly.
- Bridge files (~15 files): Follow consistent pattern. Not individually reviewed.

### Sleep, Quantum, Lib
- **sleep_bio_async_bridge.c**: Single bridge file, follows bio-async pattern.
- **quantum bridges**: 2 bridge files (surface, bio-async). Follow standard patterns.
- Most bridge files are <200 lines with straightforward handler registration and signal publishing.

---

## Previously Known Issues Confirmed

The following issues from MEMORY.md were confirmed still present:
- Global vote tracking in swarm_consensus.c (architectural, needs per-context locking)
- Thread-unsafe const getters in dragonfly/perception modules
