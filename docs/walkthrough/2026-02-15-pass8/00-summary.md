# Pass 8 Code Walkthrough Summary

**Date**: 2026-02-15
**Scope**: Full codebase review (~2,212 C source files)
**Method**: 11 parallel review agents scanning for P1/P2/P3 issues
**Previous**: Pass 7 fixed ~103 P1 issues (53 files, commit `e4c17529f`)

## Aggregate Findings

| Report | Scope | P1 | P2 | P3 | Total |
|--------|-------|----|----|----|----|
| 01 - Core Brain Regions | 206 files | 6 | 9 | 2 | 17 |
| 02 - Core Infrastructure | ~100 files | 7 | 38 | 8 | 53 |
| 03 - Cognitive Core (attn/sal/reason/WM) | 80 files | 12 | 38+ | 1 | 51+ |
| 04a - Cognitive Memory/Knowledge | 87 files | 9 | 15 | 0 | 24 |
| 04b - Cognitive Immune/Integration | 72 files | 8 | 10 | 1 | 19 |
| 05 - Cognitive Emotion/Social | 150+ files | 2 | 15 | 5 | 22 |
| 06 - Cognitive Advanced | 200+ files | 4 | 12 | 2 | 18 |
| 07 - Mesh/Networking/IO | 71 files | 8 | ~75 | 3 | ~86 |
| 08 - Middleware/Training/Plasticity | 209 files | 8 | 15 | 5 | 28 |
| 09 - Security/Utils/Async | 214 files | 6 | 18 | 5 | 29 |
| 10 - Remaining Modules | 300+ files | 5 | 16 | 6 | 27 |
| **TOTAL** | **~2,200 files** | **~75** | **~261** | **~38** | **~374** |

## P1 Issues by Category

| Category | Count | Key Files |
|----------|-------|-----------|
| Division by zero | ~20 | hippocampus, SNN bridges (5), parietal_quantum, game_theory_snn, perception_immune, knowledge |
| Data races (static mutable) | ~25 | mesh_msp (4), mesh_topology (2), mesh_pattern_routing (all), brain region timestamps (12), plasticity statics, portia globals |
| Config state corruption | 3 | hippocampus learning_rate/theta_power/threshold permanently mutated |
| Use-after-free | 2 | brain_immune_tick mc_result, signal_wrapper (already fixed) |
| Unsafe realloc | 4 | pr_snn_bridge, pr_continual_bridge, schemas, dist_create_group |
| Memory leaks | 3 | hippo_encode_episode partial, imagination_engine_init, hyperthymesia |
| Deadlock/TOCTOU | 3 | superior_colliculus, code_immune_self_repair, portia destroy race |
| Thread-unsafe PRNG | 3 | statistics srand (3 files), kuramoto global PRNG, omni_wm per-call seed |
| Hot-path false throws | 2 | salience_snn_bridge refractory/non-spiking (O(N*C*T) throws) |
| NULL mutex crash | 1 | mirror_neurons_load() |
| Integer overflow | 1 | gt_equilibrium total_cells multiplication |
| log(0) NaN | 1 | snn_network Box-Muller |

## Systemic P2 Patterns

| Pattern | Est. Count | Description |
|---------|-----------|-------------|
| Wrong function names in throws | ~300+ | "unknown:", copy-pasted names, randn:, get_time_us: |
| False positive NIMCP_THROW_TO_IMMUNE | ~80+ | search not-found, empty state, disabled features, bool-as-pointer |
| Wrong error codes | ~50+ | NULL_POINTER for NO_MEMORY, NULL_POINTER for OUT_OF_RANGE |
| Thread-unsafe global statics | ~30+ | g_bbb_system (6 files), bridge statics, mesh globals |
| "X is NULL" for bool fields | ~40+ | Bool treated as pointer in error messages |
| nimcp_mutex_destroy vs free | ~7 | neuro_symbolic uses destroy on heap-allocated mutexes |

## Top Priority Fixes for Pass 9

### Immediate P1 Fixes (crashes/corruption)
1. **Hippocampus config corruption** (3 bugs) - learning_rate, theta_power, pattern_completion_threshold permanently mutated each cycle
2. **SNN bridge div-by-zero** (5 files) - `sqrtf(change_magnitude / num_dims)` with no guard
3. **Mesh pattern routing** - entire module has zero mutex protection
4. **Mesh MSP** - 4 functions accessing shared state without mutex
5. **Salience SNN hot-path throws** - O(N*C*T) immune system calls per simulation step
6. **mirror_neurons_load() NULL mutex** - crash on any lock after load
7. **SNN Box-Muller log(0)** - NaN propagation through weight init
8. **Portia TOCTOU** - destroy/update race on global context
9. **brain_immune_tick UAF** - read after mc_result_free()

### Systemic P2 Fixes (bulk operations)
1. Fix ~300+ wrong function names in throw messages
2. Remove ~80+ false positive NIMCP_THROW_TO_IMMUNE
3. Fix ~50+ wrong error codes
4. Make ~30+ thread-unsafe statics atomic or thread-local
5. Fix 7 nimcp_mutex_destroy -> nimcp_mutex_free in neuro_symbolic

## Detailed Reports
- [01-core-brain-regions.md](01-core-brain-regions.md)
- [02-core-infrastructure.md](02-core-infrastructure.md)
- [03-cognitive-core.md](03-cognitive-core.md)
- [04a-cognitive-memory.md](04a-cognitive-memory.md)
- [04b-cognitive-immune-integration.md](04b-cognitive-immune-integration.md)
- [05-cognitive-emotion-social.md](05-cognitive-emotion-social.md)
- [06-cognitive-advanced.md](06-cognitive-advanced.md)
- [07-mesh-networking-io.md](07-mesh-networking-io.md)
- [08-middleware-training-plasticity.md](08-middleware-training-plasticity.md)
- [09-security-utils.md](09-security-utils.md)
- [10-remaining-modules.md](10-remaining-modules.md)
