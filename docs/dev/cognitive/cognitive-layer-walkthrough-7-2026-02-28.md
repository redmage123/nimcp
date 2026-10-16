# Cognitive Layer Walkthrough #7 — Complete Evaluation Report

**Date**: 2026-02-28
**Scope**: All 817 .c files (~783K lines) across 68 subdirectories under `src/cognitive/`
**Method**: 10 parallel audit agents, each reading every file in their assigned directories
**Previous fixes excluded**: mutex unlock, unchecked allocs, NULL-after-free, div-by-zero guards, overflow in malloc, uninit vars, strcat→strncat

---

## Executive Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 51 |
| HIGH | 41 |
| MEDIUM | 53 |
| LOW | 12 |
| **TOTAL** | **157** |

**Overall Score: 6.5/10** (up from 5.5/10 in walkthrough #6)

The automated fix campaign from walkthrough #6 eliminated ~7,000 instances of 7 bug categories, significantly improving the codebase. However, this walkthrough reveals a dominant new bug class: **`return -1` from pointer-returning functions** (type confusion), accounting for ~60% of CRITICAL bugs. The codebase also has systemic issues with immune system pollution (normal "not found" results throwing to immune), missing thread safety in several modules, and memory leaks on error paths.

---

## Bug Category Summary

| Category | CRIT | HIGH | MED | LOW | Total |
|----------|------|------|-----|-----|-------|
| Type confusion: `return -1` from pointer functions | 41 | 1 | 0 | 0 | 42 |
| Race condition / missing thread safety | 3 | 7 | 6 | 2 | 18 |
| API misuse: immune pollution on normal paths | 0 | 9 | 5 | 0 | 14 |
| Memory leak on error paths | 0 | 7 | 3 | 0 | 10 |
| Logic errors | 1 | 5 | 5 | 1 | 12 |
| Type confusion: other (bool, uint32_t, void) | 0 | 0 | 9 | 0 | 9 |
| Buffer overflow / OOB | 1 | 2 | 2 | 0 | 5 |
| Deadlock / mutex issues | 3 | 2 | 1 | 0 | 6 |
| Use-after-free / dangling pointer | 1 | 1 | 1 | 0 | 3 |
| Dead code | 0 | 1 | 1 | 2 | 4 |
| Unsigned integer underflow | 0 | 1 | 3 | 0 | 4 |
| Other | 1 | 5 | 17 | 7 | 30 |

---

## Top Priority Fixes

### 1. CRITICAL: `return -1` from Pointer-Returning Functions (~42 instances across ~35 files)

**The #1 systemic bug.** Functions returning `*_t*` (pointers) use `return -1` on allocation failure. On 64-bit systems, `-1` becomes `0xFFFFFFFFFFFFFFFF` — a non-NULL garbage pointer. Callers checking `if (!ptr)` will think it succeeded, then SIGSEGV on dereference.

**Affected areas** (by agent group):
- **Memory/knowledge**: 9 files (hopfield, temporal_replay, social_memory, autobio_snn, knowledge_snn, consolidation_snn, knowledge_lifecycle, working_memory_lifecycle)
- **Integration/bridges**: 12 files (bargaining, auction, gt_spatial, fep_orchestrator, omni lifecycle, omni wm_state, omni io)
- **Creative**: 4 files (creative_bridge, onnx_runtime, diffusion_bridge)
- **Executive/attention**: 4 files (executive_snn, sleep_wake_snn, meta_learning_snn, rcog_orchestrator)
- **Immune**: 1 file (regulatory_tcells)
- **Health/fault/self**: 4 files (metacognition, self_awareness_snn, wellbeing_snn, mental_health_snn, self_model_snn)
- **Reasoning/logic**: 3 files (reasoning_snn, symbolic_logic, epistemic_snn)
- **Personality/parietal/ethics**: 15+ files (personality_snn, tom_snn, shadow_snn, introspection_snn, ethics_snn, parietal_snn, genius_snn, financial_jepa, financial_predictive, linguistics_snn, spatial_reasoning, conceptual_blending, physics_nn, biology, intuitive_reasoning, counterfactual, intuition_integrations)
- **Mirror neurons**: 0 (pointer functions clean)

**Fix**: Global regex replacement: In all `*_create*()` functions returning pointers, change `return -1;` to proper cleanup + `return NULL;`.

### 2. CRITICAL: Wrong Mutex Field (NULL Mutex Lock)

- `nimcp_omni_immune_bridge.c` lines 432,445,458: Locks `bridge->mutex` (NULL from calloc) instead of `bridge->base.mutex`
- `nimcp_omni_logic_bridge.c` lines 378,435,611,624,637: Same pattern — `bridge->mutex` vs `bridge->base.mutex`

**Fix**: Change all to `bridge->base.mutex`.

### 3. CRITICAL: Uninitialized Mutex

- `nimcp_mucosal_immunity.c` lines 243-248: Mutex allocated via raw malloc but never initialized with `nimcp_platform_mutex_init()`

### 4. CRITICAL: Missing bridge_base_init (No Thread Safety)

- `nimcp_mirror_language_bridge.c`: No `bridge_base_init()`, NULL mutex, all state unprotected
- `nimcp_mirror_substrate_bridge.c`: Same — `bridge_base_notify_coordinator_tick()` will crash on NULL mutex
- `nimcp_audio_logic_bridge.c`: No `bridge_base_init()`
- 10+ bridge files in reasoning/logic group: Missing `bridge_base_init()`

### 5. HIGH: Immune System Pollution (~14 instances)

Normal "not found" search results fire `NIMCP_THROW_TO_IMMUNE()`, flooding the immune system with false positives:
- `nimcp_shared_intentionality.c` (3 instances)
- `nimcp_mirror_prefrontal_bridge.c`, `nimcp_mirror_tom_bridge.c`, `nimcp_mirror_self_other.c`, `nimcp_mirror_social_context.c`, `nimcp_mirror_motor_bridge.c` (4 instances)
- `nimcp_symbolic_logic.c` terms_equal/atoms_equal
- `nimcp_epistemic_plasticity_bridge.c` find_source
- `nimcp_epistemic_snn_bridge.c` neuron_step (normal refractory/no-spike)
- `nimcp_recursive/` (4 instances)
- `nimcp_salience/` (3 instances)

---

## Detailed Bug Reports by Area

### Memory/Knowledge/Working Memory/Consolidation (19 bugs: 9C/5H/3M/2L)

| Bug | File | Sev | Category |
|-----|------|-----|----------|
| `return -1` from ptr func | hopfield_memory.c:300, temporal_replay.c:1120-1181, social_memory.c:285, autobio_snn.c:213-219, knowledge_snn.c:211-217, consolidation_snn.c:212-218, knowledge_lifecycle.c:228-240, working_memory_lifecycle.c:79-91 | CRIT x9 | Type confusion |
| immune throw on normal GPU check | temporal_replay.c:124-146 | HIGH | API misuse |
| realloc on CoW memory | wm_transfer.c:573 | HIGH | API misuse |
| mutex leak on init failure | memory_consolidation_substrate.c:397-401 | HIGH | Memory leak |
| `(bool)(-1)` = true on error | consolidation.c:1665-1671 | HIGH | Logic error |
| void func `return -1` | metamemory_monitor.c:1553-1559 | HIGH | Logic error |
| Static global race | resonance.c:47 | MED | Race condition |
| Unsorted find_similar results | semantic_memory.c | MED | Logic error |
| Zero init consolidation_strength | engram.c:660 | MED | Logic error |
| Wrong error code | collective_memory.c:311, fractal.c:573,792 | LOW x2 | Wrong code |

### Integration/Bridges/Omni/Game Theory (15 bugs: 1C/7H/5M/2L)

| Bug | File | Sev | Category |
|-----|------|-----|----------|
| `return -1` from ptr func (30+ sites) | bargaining.c, auction.c, gt_spatial.c, fep_orchestrator.c, omni lifecycle/state/io, 6 omni bridge files | CRIT | Type confusion |
| Mutex leak on alloc fail | gt_equilibrium.c:675,677 | HIGH | Deadlock |
| Mutex leak on alloc fail | gt_learning.c:885 | HIGH | Deadlock |
| Thread-unsafe static locals | security_cognitive_hub_bridge.c:236-249 | HIGH | Race condition |
| Unused once_t guard | cognitive_integration_fep.c:64 | HIGH | Race condition |
| TOCTOU in FEP registration | predictive_attention_fep_bridge.c:490-527 | HIGH | Race condition |
| Callbacks under mutex lock | salience_attention_fep_bridge.c:710 | HIGH | Deadlock |
| Memory leak on bridge init fail | gw_cognitive_bridge.c:~258 | HIGH | Memory leak |
| Memory leaks (25+ sites) | fep_sleep, fep_snn, fep_consciousness, fep_planning, predictive, gt_spatial, gt_learning, 4 omni bridges | MED x2 | Memory leak |
| Alignment issue in payload cast | predictive_attention_bridge.c:204-205 | MED | UB |
| Dangling pointer for bridge name | fep_orchestrator_part_core.c:173 | MED | UAF risk |
| Duplicate function definitions (ODR) | omni_wm_state.c vs lifecycle.c | MED | ODR |
| Dead code after null check | omni processing.c:43-44 | LOW | Dead code |
| Immune throw on normal lookup miss | omni_precision.c:94,113 | LOW | API misuse |

### Creative (19 bugs: 4C/7H/6M/2L)

| Bug | File | Sev | Category |
|-----|------|-----|----------|
| `return -1` from ptr func | creative_bridge.c:172, onnx_runtime.c:136,308, diffusion_bridge.c:172 | CRIT x4 | Type confusion |
| Shallow copy use-after-free | influence_blending.c:900 | HIGH | UAF |
| OOB read archetype names | style_perception.c:39-96,704-724 | HIGH | Buffer overread |
| Div-by-zero when steps==1 | gan_bridge.c:873 | HIGH | Div-by-zero |
| Div-by-zero when steps==1 | visual_generation.c:185 | HIGH | Div-by-zero |
| Unsigned underflow `num_found-1` | creative_bridge.c:832 | HIGH | Underflow |
| Unused mutex on feedback buffer | creative_training_bridge.c:865-931 | HIGH | Race condition |
| Memory leak in optimize loop | influence_blending.c:743-765 | HIGH | Memory leak |
| Unsigned underflow `total_frames` | video_generation.c:770 | MED | Underflow |
| Div-by-zero when frames==1 | video_generation.c:372 | MED | Div-by-zero |
| Unsigned underflow (2 instances) | creative_pattern_extractor.c:770,864 | MED x2 | Underflow |
| Unsigned underflow | creative_memory_bridge.c:446 | MED | Underflow |
| fminf truncates size_t to float | multimodal_director.c:202 | MED | Type truncation |
| Minor key detection false positive | music_generation.c:1042 | LOW | Logic error |
| JSON injection in API body | creative_api_client.c:534 | LOW | Injection |

### Executive/Attention/Salience/Sleep/Meta/Recursive/Fractal (16 bugs: 5C/2H/7M/2L)

| Bug | File | Sev | Category |
|-----|------|-----|----------|
| Double mutex unlock | executive_substrate_bridge.c:474+478 | CRIT | UB |
| `return -1` from ptr func | executive_snn.c:257-264, sleep_wake_snn.c:239-245, meta_learning_snn.c:212-218, rcog_orchestrator.c:50 | CRIT x4 | Type confusion |
| Memory leak chain on alloc fail | executive_snn.c:~250-265 | HIGH | Memory leak |
| Dead loop (return on first iter) | fractal_cognitive_fep_bridge.c:275-276 | HIGH | Logic error |
| Race: no mutex in boost_task | executive.c:~1723 | MED | Race condition |
| Race: no mutex in get_load | executive.c:~1680 | MED | Race condition |
| Race: stale pointer from get_task | executive.c:~1319 | MED | Race condition |
| Memory leak in substrate create | fractal_cognitive_substrate_bridge.c:142-148 | MED | Memory leak |
| Immune throw on not-found (8x) | rcog_context_store, rcog_tool_router, surprise bridges, rcog_engine | MED x3 | API misuse |
| Non-thread-safe GPU globals | executive.c:53-56 | LOW | Race condition |
| Dead code after return (5 files) | rcog_imagination/immune/collective/bio_async/brain_kg bridges | LOW | Dead code |

### Immune System (15 bugs: 3C/7H/4M/1L)

| Bug | File | Sev | Category |
|-----|------|-----|----------|
| Wrong mutex field (NULL lock) | omni_immune_bridge.c:432,445,458 | CRIT | Wrong field |
| `return -1` from ptr func | regulatory_tcells.c:210 | CRIT | Type confusion |
| Uninitialized mutex | mucosal_immunity.c:243-248 | CRIT | UB |
| No thread safety at all | mucosal_immunity.c (all public funcs) | HIGH | Race condition |
| TOCTOU race on capacity check | brain_immune_part_core.c:645/656 | HIGH | Race condition |
| Unlock after failed lock (5x) | regulatory_tcells.c:331,449,538,610,701 | HIGH | UB |
| find_b_cell without mutex | brain_immune_part_core.c:281 | HIGH | Race condition |
| Placeholder time=0 (5 bridges) | introspection/tom/wellbeing/sleep/executive immune bridges | HIGH | Logic error |
| Wrong API for mutex free | trained_immunity.c:416 | HIGH | Type confusion |
| Shared callback_user_data | regulatory_tcells.c:783,805,827 | HIGH | Logic error |
| memset zeroes cumulative counters | immune_exhaustion.c:467 | MED | Data loss |
| Cross-module access without mutex | regulatory_tcells.c:347 | MED | Race condition |
| 32-bit overflow in timestamp | complement_system.c:110, immune_persistence.c:157 | MED | Overflow |
| Direct PTHREAD_MUTEX_INITIALIZER | brain_immune_plasticity.c:43 | MED | Portability |
| Useless local NULL assignment | brain_immune_lifecycle.c:110 | LOW | Dead code |

### Health/Fault/Self-Awareness/Wellbeing/Mental Health (17 bugs: 5C/4H/8M/0L)

| Bug | File | Sev | Category |
|-----|------|-----|----------|
| `return -1` (explicit cast) | metacognition.c:158 | CRIT | Type confusion |
| `return -1` from ptr func | self_awareness_snn.c:242-249, wellbeing_snn.c:212-218, mental_health_snn.c:258-264, self_model_snn.c:259-265 | CRIT x4 | Type confusion |
| Leaked bridge (never stored) | health_cognitive_bridge.c:1013-1024 | HIGH | Memory leak |
| OOB array access | predictive_immune.c:393-394 | HIGH | OOB |
| Counter always adds zero | recovery_consolidation.c:871 | HIGH | Logic error |
| Pointer-after-unlock race | failure_prediction.c:1265-1269 | HIGH | Race condition |
| Dead code after return | health_cognitive_bridge.c:277 | MED | Dead code |
| Div-by-zero on zero baseline | metacognition.c:951-955 | MED | Div-by-zero |
| Immune throw on clean shutdown | recovery_consolidation.c:272 | MED | API misuse |
| Mutex leak on create failure | fault_tolerance_substrate.c:142-145 | MED | Memory leak |
| NULL region to trigger func | predictive_immune.c:863 | MED | NULL deref risk |
| Immune throw on clean shutdown | mental_health_guardian.c:803-804 | MED | API misuse |
| Mutex leak on create failure | self_awareness_extended_substrate.c:131-136 | MED | Memory leak |
| Dead code after return | recovery_parietal_bridge.c:280-283 | MED | Dead code |

### Reasoning/Logic/Symbolic/Epistemic/Analysis (18 bugs: 4C/3H/9M/2L)

| Bug | File | Sev | Category |
|-----|------|-----|----------|
| `return -1` from ptr func | reasoning_snn.c:216-222 | CRIT | Type confusion |
| `return -1` from ptr func | symbolic_logic.c:92,209,721 | CRIT x2 | Type confusion |
| `return -1` from ptr func | epistemic_snn.c:259-265 | CRIT | Type confusion |
| Callbacks under non-recursive mutex | reasoning_snn.c:614-632 | HIGH | Deadlock |
| Wrong mutex field | omni_logic_bridge.c:378,435,611,624,637 | HIGH | Race condition |
| Wrong num_dims in SNN encoding | reasoning_snn.c encode functions | HIGH | Logic error |
| No bridge_base_init (10 files) | logic/symbolic/epistemic/analysis/explanations bridges | MED | Race condition |
| Immune throw on normal inequality | symbolic_logic.c terms_equal/atoms_equal | MED | API misuse |
| Mutex leak on init failure | reasoning_substrate_bridge.c | MED | Memory leak |
| No mutex in curiosity/attention | reasoning_curiosity.c, reasoning_attention.c | MED x2 | Race condition |
| Immune throw on normal no-spike | epistemic_snn.c:169,185 | MED | API misuse |
| Immune throw on normal not-found | epistemic_plasticity_bridge.c:118 | MED | API misuse |
| Dead loop (zero bound) | analysis_fep_bridge.c:275 | MED | Logic error |
| Incorrect heartbeat arg type | explanations_thalamic.c:353,366,377 | MED | API misuse |
| Non-thread-safe bio-async init | knowledge_base_interface.c | LOW | Race condition |
| Fragile compatibility struct | reasoning_convergent.c | LOW | Maintenance |

### Mirror Neurons + Collective Cognition (30 bugs: 4C/9H/14M/3L)

| Bug | File | Sev | Category |
|-----|------|-----|----------|
| Dangling pointer (serialized ptr) | mirror_neurons_part_io.c:87,274 | CRIT | UAF |
| No bridge_base_init | mirror_language_bridge.c:620-668 | CRIT | Race condition |
| No bridge_base_init | mirror_substrate_bridge.c:~108 | CRIT | Race condition |
| Fake timestamp (counter not time) | shared_intentionality.c:85-88 | CRIT | Logic error |
| No thread safety in motor bridge | mirror_motor_bridge.c | HIGH | Race condition |
| No thread safety (2 files) | hyperscanning.c, shared_intentionality.c | HIGH | Race condition |
| Immune throw on not-found (5 files) | shared_intentionality, prefrontal, tom, self_other, social_context, motor bridges | HIGH x5 | API misuse |
| State corruption (early increment) | mirror_neurons_lifecycle.c:102 | HIGH | State corruption |
| `return -1` in uint32_t func | mirror_neurons_lifecycle.c:110 | MED | Type confusion |
| `return -1` in void func | mirror_self_other.c:384 | MED | Type confusion |
| `return -1` in uint32_t (4 files) | mirror_self_other, habituation, vicarious_reward, attention_bridge | MED x4 | Type confusion |
| `return -1` in bool func (2x) | mirror_vicarious_reward.c:901,941 | MED x2 | Type confusion |
| Hardcoded buffer size mismatch | mirror_neurons_io.c:115 | MED | Buffer risk |
| Div-by-zero in motor bridge | mirror_motor_bridge.c:473 | MED | Div-by-zero |
| Mutex leak on create failure | mirror_immune_integration.c:157 | MED | Memory leak |
| Wrong error code from mesh_register | mirror_neurons_fep_bridge.c:34 | MED | API inconsistency |
| Misleading error code | shared_intentionality.c:124 | MED | API misuse |
| Config drift via multiplication | mirror_omni_bridge.c:716-718 | LOW | Logic error |
| Biased averaging | hyperscanning.c:718 | LOW | Statistics |
| Goal threshold scaling | shared_intentionality.c:605 | LOW | Design |

### Personality/Parietal/Ethics/ToM/Bias/Shadow/Grief/Joy/Introspection (32 bugs: 20C/9H/2M/1L)

| Bug | File | Sev | Category |
|-----|------|-----|----------|
| `return -1` from ptr func (22 files) | personality_snn, tom_snn, shadow_snn, introspection_snn, ethics_snn, ethics_evaluation, parietal_snn, genius_snn, financial_jepa, financial_predictive (x2), linguistics_snn, spatial_reasoning (x2), conceptual_blending, physics_nn, biology, intuitive_reasoning, counterfactual, intuition_integrations | CRIT x18 | Type confusion |
| NULL pointer deref in guard | ethics_hyperbolic.c:229-231 | CRIT | NULL deref |
| `return -1` from void + leak | linguistics_mesh.c:766-770 | CRIT | Type confusion |
| Bio-async resource leak on reset | grief_and_loss.c grief_system_reset | HIGH | Resource leak |
| Bio-async resource leak on reset | joy_euphoria.c joy_system_reset | HIGH | Resource leak |
| Immune throw on normal neuron state | bias_snn_bridge.c:161,183 | HIGH | API misuse |
| `sizeof(pointer)` buffer overflow | scientific_reasoning.c:415,420,424 | HIGH | Buffer overflow |
| Dead loop (zero bound before loop) | love_loyalty_fep_bridge.c:171 | HIGH | Logic error |
| Memory leak in physics_nn predict | physics_nn.c:1464 | HIGH | Memory leak |
| Memory leaks in suggest_experiment | scientific_reasoning.c:1135-1139 | HIGH | Memory leak |
| Biology create leaks + deadlock | biology.c:765,809 | HIGH | Leak + deadlock |
| Return -1 from pointer + ethics | ethics_evaluation.c:271 | HIGH | Type confusion |
| Unguarded array access | ethics_helpers.c:376 | MED | OOB risk |
| Const-correctness / UB | tom_fep_bridge.c:456,473 | MED | UB |
| Dead code after return | parietal_training_bridge.c:299-302 | LOW | Dead code |

---

## Recommendations (Priority Order)

1. **Fix `return -1` from pointer functions** — ~42+ CRITICAL instances. Write a script to grep all `*_create*` and pointer-returning functions for `return -1` and change to cleanup + `return NULL`. This single fix eliminates ~50% of all CRITICAL bugs.

2. **Fix wrong mutex fields** — 2 CRITICAL files using `bridge->mutex` instead of `bridge->base.mutex`.

3. **Fix uninitialized mutex** — `nimcp_mucosal_immunity.c` malloc without init.

4. **Add `bridge_base_init()`** to ~15 bridge files missing it (thread safety).

5. **Remove immune throws on normal paths** — ~14 instances polluting immune diagnostics.

6. **Fix `sizeof(pointer)` buffer overflow** in `nimcp_scientific_reasoning.c`.

7. **Fix placeholder timestamp functions** — 5 immune bridges always return 0 or garbage.

8. **Fix unsigned underflow in bubble sort loops** — 5 instances of `i < count - 1` when count is uint32_t.

9. **Fix memory leaks on error paths** — ~10 instances across bridge create functions.

10. **Fix race conditions** in executive accessor functions and modules lacking mutex protection.

---

## Score Justification: 6.5/10

**Improvements from walkthrough #6** (+1.0):
- 7,000+ instances of 7 bug categories fixed (mutex unlock, unchecked allocs, NULL-after-free, div-by-zero, overflow, uninit vars, strcat)
- Majority of common bug patterns are now clean

**Remaining issues** (-3.5):
- 51 CRITICAL bugs (predominantly `return -1` from pointer functions — systemic copy-paste issue)
- Systemic immune system pollution (degrades fault detection capability)
- Multiple modules lacking basic thread safety
- Several buffer overflow risks
- Memory leak patterns on error paths

The `return -1` from pointer functions is a single pattern that accounts for the bulk of remaining CRITICALs. A single automated fix pass would bring the score to ~8/10.
