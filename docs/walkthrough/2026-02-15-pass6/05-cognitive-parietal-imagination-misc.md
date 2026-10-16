# Pass 6 Walkthrough: Cognitive Parietal, Imagination, and Misc Modules

**Date**: 2026-02-15
**Scope**: 22 cognitive subdirectories (~200+ .c files)
**Method**: Manual code review + systematic Grep-based pattern searches

## Directories Reviewed

| Directory | Files | Status |
|-----------|-------|--------|
| parietal/ (incl. linguistics/) | ~77 | Reviewed |
| imagination/ | 10 | Reviewed |
| game_theory/ (incl. integration/) | 22 | Reviewed |
| knowledge/ | 11 | Reviewed |
| mirror_neurons/ | 27 | Reviewed |
| introspection/ | 12 | Reviewed |
| self_awareness/ | 5 | Reviewed |
| self_model/ | 7 | Reviewed |
| theory_of_mind/ | 7 | Reviewed |
| tom/ | 2 | Reviewed |
| predictive/ | 7 | Reviewed |
| wellbeing/ | 14 | Reviewed |
| mental_health/ | 10 | Reviewed |
| personality/ | 5 | Reviewed |
| social/ | 6 | Reviewed |
| shadow/ | 5 | Reviewed |
| bias/ | 6 | Reviewed |
| epistemic/ | 6 | Reviewed |
| joy/ | 4 | Reviewed |
| logic/ | 8 | Reviewed |
| creative/ (incl. subdirs) | 23 | Reviewed |
| neuro_symbolic/ | 12 | Reviewed |

---

## P1 Issues (Crash / Data Corruption)

| # | File | Line | Issue | Description |
|---|------|------|-------|-------------|
| 1 | `parietal/nimcp_mathematical_genius.c` | 1122 | Integer overflow | `genius_gauss_modular_pow()`: `result = (result * base) % mod` -- uint64_t multiplication can overflow before modulo for large base/mod values. Should use `__uint128_t` intermediate or split multiplication. |
| 2 | `parietal/nimcp_mathematical_genius.c` | 1066 | Precision loss | `genius_gauss_is_prime()`: `sqrtf((float)n)` loses precision for n > 2^24 (float has 23-bit mantissa). For large uint64_t values, sqrt limit will be wrong, causing incorrect primality results. Should use `sqrt((double)n)` or integer sqrt. |
| 3 | `parietal/nimcp_financial_market.c` | 890-892 | Stack overflow risk | `compute_indicator()` MACD: `float macd_buf[FIN_MKT_MAX_SERIES_LENGTH]` etc. on stack. If `FIN_MKT_MAX_SERIES_LENGTH` is large (e.g., 10000), three such arrays consume ~120KB stack per call. |
| 4 | `parietal/nimcp_financial_market.c` | 909-911 | Stack overflow risk | `compute_indicator()` Bollinger: `float bb_upper[FIN_MKT_MAX_SERIES_LENGTH]`, `float bb_lower[FIN_MKT_MAX_SERIES_LENGTH]` on stack. Same risk as above. |

**P1 Total: 4**

---

## P2 Issues (Wrong Behavior / Correctness)

### P2-A: Wrong Error Code (NIMCP_ERROR_NO_MEMORY for NULL pointer / invalid param)

These throw `NIMCP_ERROR_NO_MEMORY` when the actual error is a NULL input parameter, invalid config, or allocation of a sub-component -- NOT a memory allocation failure for the primary object. The correct error code should be `NIMCP_ERROR_NULL_POINTER` or `NIMCP_ERROR_INVALID_PARAM`.

**Systemic count across all 22 directories: ~118 occurrences in 77 files** (from Grep search `NIMCP_ERROR_NO_MEMORY.*required parameter is NULL`)

Additionally, many throws use `NIMCP_ERROR_NO_MEMORY` when checking if a sub-allocation succeeded (e.g., mutex, bridge base) after the parent was already allocated. While technically a memory error, the message says "X is NULL" which is misleading -- the throw message should indicate allocation failure, not NULL pointer.

Representative samples:

| # | File | Line | Issue |
|---|------|------|-------|
| 1 | `game_theory/nimcp_gt_equilibrium.c` | 193 | `NIMCP_ERROR_NO_MEMORY` for NULL/invalid config -- should be `NIMCP_ERROR_INVALID_PARAM` |
| 2 | `predictive/nimcp_predictive_hierarchy.c` | 361 | `NIMCP_ERROR_NO_MEMORY` for NULL config/level_configs -- should be `NIMCP_ERROR_NULL_POINTER` |
| 3 | `game_theory/nimcp_credit_assignment.c` | 157 | `NIMCP_ERROR_NO_MEMORY` for NULL config -- should be `NIMCP_ERROR_NULL_POINTER` |
| 4 | `game_theory/nimcp_gt_auction_ext.c` | 1342 | `NIMCP_ERROR_NO_MEMORY` for NULL config -- should be `NIMCP_ERROR_NULL_POINTER` |
| 5 | `knowledge/nimcp_knowledge_cow.c` | 129 | `NIMCP_ERROR_NO_MEMORY` for NULL config -- should be `NIMCP_ERROR_NULL_POINTER` |
| 6 | `mental_health/nimcp_mental_health_guardian.c` | 1583 | `NIMCP_ERROR_NO_MEMORY` for NULL guardian/working_memory params |
| 7 | `theory_of_mind/nimcp_theory_of_mind.c` | 523 | `NIMCP_ERROR_NO_MEMORY` for tom alloc failure (this one is correct) |
| 8 | `imagination/nimcp_hippocampus_imagination_bridge.c` | 641 | `NIMCP_ERROR_NO_MEMORY` when `bridge->config is NULL` -- not a memory error |
| 9 | `logic/nimcp_symbolic_logic.c` | 1495 | `NIMCP_ERROR_NO_MEMORY` when consolidation disabled -- wrong error code entirely |
| 10 | `creative/bridges/nimcp_creative_training_bridge.c` | 177 | `NIMCP_ERROR_NO_MEMORY` for NULL config -- should be `NIMCP_ERROR_NULL_POINTER` |

### P2-B: Wrong Function Name in Throw Message

| # | File | Line | Wrong Name | Correct Name |
|---|------|------|------------|--------------|
| 1 | `parietal/nimcp_parietal.c` | 270 | `"get_time_us: nn->layers is NULL"` | `"physics_nn_create: ..."` |
| 2 | `parietal/nimcp_parietal.c` | 298 | `"get_time_us: required parameter is NULL"` | `"physics_nn_create: ..."` |
| 3 | `parietal/nimcp_parietal.c` | 770 | `"parietal_destroy: ..."` | `"parietal_attach_to_brain: ..."` |
| 4 | `parietal/nimcp_parietal.c` | 811 | `"parietal_get_brain_region: ..."` | `"parietal_connect_to_region: ..."` |
| 5 | `mirror_neurons/nimcp_mirror_plasticity_bridge.c` | 171 | `"get_time_us: validation failed"` | actual function name |
| 6 | `creative/bridges/nimcp_creative_training_bridge.c` | 177,184 | `"get_elapsed_seconds: ..."` | `"creative_training_bridge_create: ..."` |
| 7 | `creative/appreciation/nimcp_creative_emotion_bridge.c` | 87 | `"creative_emotion_bridge_config_defaults: ..."` | actual function name (if different) |
| 8 | `creative/appreciation/nimcp_creative_memory_bridge.c` | 147,164,176,189 | `"generate_memory_id: ..."` | actual function names |
| 9 | `creative/appreciation/nimcp_aesthetic_evaluation.c` | 102 | `"aesthetic_evaluator_config_defaults: ..."` | actual function name |
| 10 | `creative/inspiration/nimcp_creative_knowledge_bridge.c` | 285 | `"artist_matches_name: ..."` | actual function name |
| 11 | `creative/bridges/nimcp_creative_ethics_bridge.c` | 144 | `"contains_keyword: ..."` | actual function name |
| 12 | `creative/bridges/nimcp_creative_neural_bridge.c` | 130,137 | `"creative_neural_bridge_config_defaults: ..."` | actual function names |
| 13 | `logic/nimcp_symbolic_logic.c` | 1490 | `"...nimcp_validate_pointer is NULL"` | misleading -- validation failed, func is not NULL |
| 14 | `logic/nimcp_symbolic_logic.c` | 1501 | `"...symbolic_logic_add_fact is NULL"` | misleading -- add_fact returned false, not NULL |

### P2-C: "unknown:" Function Name in Throw Message

**Systemic count: 261 occurrences across 50 files** in the assigned directories.

These throws use the literal string `"unknown:"` instead of the actual function name. This makes debugging much harder as you cannot identify the throwing function from the error message.

Affected files (in assigned directories only):

| File | Count |
|------|-------|
| `imagination/nimcp_imagination_engine.c` | 46 |
| `shadow/nimcp_shadow_emotions.c` | 8 |
| `creative/bridges/nimcp_creative_neural_bridge.c` | 12 |
| `creative/bridges/nimcp_creative_ethics_bridge.c` | 11 |
| `creative/inspiration/nimcp_style_representation.c` | 11 |
| `creative/generation/nimcp_text_generation.c` | 10 |
| `creative/generation/nimcp_music_generation.c` | 9 |
| `mirror_neurons/nimcp_mirror_tom_bridge.c` | 3 |
| `game_theory/nimcp_gt_fairness.c` | 4 |
| `game_theory/nimcp_gt_equilibrium.c` | 1 |
| `bias/nimcp_bias_detection.c` | 3 |
| `neuro_symbolic/nimcp_evolutionary_proof.c` | 3 |
| `neuro_symbolic/nimcp_quantum_math_engine.c` | 3 |
| `creative/appreciation/nimcp_style_perception.c` | 1 |
| `creative/appreciation/nimcp_creative_memory_bridge.c` | 2 |
| `creative/bridges/nimcp_creative_training_bridge.c` | 3 |
| `creative/inspiration/nimcp_creative_knowledge_bridge.c` | 3 |
| `creative/inspiration/nimcp_creative_pattern_extractor.c` | 2 |
| `creative/inspiration/nimcp_influence_blending.c` | 3 |
| `parietal/nimcp_parietal.c` | 7 |
| `parietal/nimcp_genius_plasticity_bridge.c` | 2 |
| `parietal/nimcp_intuition_integrations.c` | 2 |
| `mental_health/interventions.c` | 1 |

### P2-D: "*_destroy:" in Non-Destroy Functions (Wrong Function Name)

**55 occurrences across 11 files** where a throw message contains a `_destroy:` function name but the throwing function is NOT the destroy function.

| # | File | Count | Description |
|---|------|-------|-------------|
| 1 | `creative/appreciation/nimcp_style_perception.c` | 7 | `"style_perception_destroy:"` used in `style_perception_analyze`, `style_perception_compare`, etc. |
| 2 | `creative/appreciation/nimcp_aesthetic_evaluation.c` | 5 | `"aesthetic_evaluator_destroy:"` used in evaluate functions |
| 3 | `creative/appreciation/nimcp_creative_emotion_bridge.c` | 2 | `"creative_emotion_bridge_destroy:"` in bridge operation functions |
| 4 | `creative/bridges/nimcp_creative_neural_bridge.c` | 4 | `"creative_neural_bridge_destroy:"` in non-destroy functions |
| 5 | `creative/generation/nimcp_text_generation.c` | 11 | `"text_generator_destroy:"` used in generate, continue, and edit functions |
| 6 | `creative/bridges/nimcp_creative_ethics_bridge.c` | 3 | `"creative_ethics_bridge_destroy:"` in evaluation functions |
| 7 | `creative/generation/nimcp_music_generation.c` | 7 | `"music_generator_destroy:"` used in generate and compose functions |
| 8 | `creative/appreciation/nimcp_creative_memory_bridge.c` | 2 | `"creative_memory_bridge_destroy:"` in non-destroy functions |

### P2-E: Misleading "*_validate_config is NULL" Throw Messages

**20 occurrences across 19 files.** These say the validation function itself is NULL, but actually validation *failed*. The message should say "validation failed" or "invalid config".

| # | File | Line | Message |
|---|------|------|---------|
| 1 | `parietal/nimcp_parietal.c` | 599 | `"parietal_create_custom: parietal_validate_config is NULL"` |
| 2 | `parietal/nimcp_number_sense.c` | 254 | `"number_sense_create_custom: number_sense_validate_config is NULL"` |
| 3 | `parietal/nimcp_physics_nn.c` | 559 | `"physics_nn_create_custom: physics_nn_validate_config is NULL"` |
| 4 | `parietal/nimcp_software_engineering.c` | 212 | `"software_eng_create_custom: software_eng_validate_config is NULL"` |
| 5 | `parietal/nimcp_mathematical_intuition.c` | 416 | `"math_intuition_create_custom: math_intuition_validate_config is NULL"` |
| 6 | `parietal/nimcp_chemistry.c` | 275 | `"chemistry_create_custom: chemistry_validate_config is NULL"` |
| 7 | `parietal/nimcp_equation_manipulation.c` | 213 | `"equation_engine_create_custom: equation_validate_config is NULL"` |
| 8 | `parietal/nimcp_biology.c` | 259 | `"biology_create_custom: biology_validate_config is NULL"` |
| 9 | `parietal/nimcp_scientific_reasoning.c` | 235 | `"scientific_reasoning_create_custom: scientific_validate_config is NULL"` |
| 10 | `parietal/linguistics/nimcp_parietal_linguistics_mesh.c` | 1749 | `"linguistics_mesh_create: linguistics_mesh_validate_config is NULL"` |
| 11 | `imagination/nimcp_imagination_engine.c` | 382 | `"imagination_engine_default_config: imagination_engine_validate_config is NULL"` |
| 12 | `reasoning/nimcp_reasoning_integration.c` | 679,1212 | (2 instances) |
| 13 | `reasoning/integration/nimcp_reasoning_attention.c` | 225,481 | (2 instances) |

### P2-F: False Positive Throws (Non-Error Paths)

| # | File | Line | Issue |
|---|------|------|-------|
| 1 | `shadow/nimcp_shadow_emotions.c` | 964 | `shadow_get_detected_in_other()` throws when person_id not found -- this is a normal lookup miss, not an error |
| 2 | `parietal/nimcp_number_sense.c` | ~201 | `number_sense_validate_config()` throws for normal validation rejection |
| 3 | `parietal/nimcp_parietal.c` | ~599 | `parietal_validate_config()` throws for normal validation rejection |
| 4 | `logic/nimcp_symbolic_logic.c` | 1495 | Throws `NIMCP_ERROR_NO_MEMORY` when `enable_memory_consolidation` is false -- this is a config check, not an error |
| 5 | `parietal/nimcp_code_generation.c` | 658 | `code_gen_match_historical_pattern()` throws when pattern matching disabled -- normal behavior |

### P2-G: Unreachable Code

| # | File | Line | Issue |
|---|------|------|-------|
| 1 | `parietal/nimcp_mathematical_genius.c` | 540 | `return NIMCP_SUCCESS` after closing brace of block that already returned -- dead code |
| 2 | `parietal/nimcp_mathematical_genius.c` | 627 | Same pattern in `genius_prove_theorem()` -- dead code |

### P2-H: Wrong Error Code (NIMCP_ERROR_NULL_POINTER for "not found")

| # | File | Line | Issue |
|---|------|------|-------|
| 1 | `parietal/nimcp_code_generation.c` | 1036 | `code_gen_get_fix()` throws `NIMCP_ERROR_NULL_POINTER` when fix not found -- should be `NIMCP_ERROR_NOT_FOUND` |

### P2-I: Architectural Mismatch (Thread-Local vs Per-Instance State)

| # | File | Line | Issue |
|---|------|------|-------|
| 1 | `parietal/nimcp_financial_market.c` | 1188 | `financial_market_detect_regime()` uses `static _Thread_local fin_market_condition_t prev_regime` -- tracks per-thread state but function takes per-engine instance, so different engines on the same thread share regime tracking |
| 2 | `parietal/nimcp_financial_market.c` | 1600 | `static _Thread_local float avg_volatility = 0.2f` -- same architectural issue |

**P2 Total: ~476** (118 wrong error codes + 261 "unknown:" func names + 55 destroy-in-non-destroy + 20 validate_config-is-NULL + 14 wrong func names + 5 false positive throws + 2 unreachable code + 1 wrong-error-for-not-found)

---

## P3 Issues (Minor / Style)

| # | File | Line | Issue |
|---|------|------|-------|
| 1 | `introspection/nimcp_introspection.c` | 249 | Heartbeat string `"introspectio_loop"` truncated (missing 'n') |
| 2 | `mental_health/nimcp_mental_health_guardian.c` | 196 | Heartbeat string `"mental_healt_default_config"` truncated (missing 'h') |
| 3 | `theory_of_mind/nimcp_theory_of_mind.c` | 517 | Heartbeat string `"theory_of_mi_tom_create"` truncated |
| 4 | `predictive/nimcp_predictive_hierarchy.c` | 366 | Heartbeat string `"predictive_h_pred_hier_create"` truncated |
| 5 | `self_awareness/nimcp_self_awareness_extended.c` | 262 | `"generate_self_narrative: self_model_get is NULL"` -- misleading, self_model_get returned false, not NULL |
| 6 | `neuro_symbolic/nimcp_hypergraph.c` | 152-193 | Double throw when mutex creation fails in create path (throws twice before returning) |

**P3 Total: 6**

---

## Statistics Summary

| Severity | Count | Description |
|----------|-------|-------------|
| **P1** | 4 | Integer overflow (1), precision loss (1), stack overflow risk (2) |
| **P2** | ~476 | Wrong error codes (119), wrong/unknown func names (350), false positive throws (5), unreachable code (2) |
| **P3** | 6 | Truncated heartbeat strings (4), misleading messages (1), double throw (1) |
| **Total** | ~486 | |

## Systemic Patterns

### 1. "unknown:" Function Names (261 occurrences)
Many functions, especially in imagination_engine.c (46), creative modules, and shadow_emotions, use `"unknown:"` as the function name in `NIMCP_THROW_TO_IMMUNE` calls. This appears to be a mass-generation artifact where function names were not filled in.

### 2. NIMCP_ERROR_NO_MEMORY Used for Non-Memory Errors (118 occurrences)
The most common wrong error code pattern. Functions throw `NIMCP_ERROR_NO_MEMORY` when validating input parameters or checking configs, when the actual error is `NIMCP_ERROR_NULL_POINTER` or `NIMCP_ERROR_INVALID_PARAM`.

### 3. Destroy Function Names in Non-Destroy Paths (55 occurrences)
Concentrated in the `creative/` module. Functions like `style_perception_analyze()`, `text_generator_generate()`, etc. have throw messages containing `"*_destroy:"` as the function name, suggesting copy-paste from destroy functions.

### 4. validate_config "is NULL" Messages (20 occurrences)
Pattern: `NIMCP_THROW_TO_IMMUNE(ERROR, "func: validate_config is NULL")` when validation returned false. The message says the validate function pointer is NULL, but the function was called and returned failure.

### Notes
- No raw `rand()` calls found -- all use `nimcp_tl_rand()` (good)
- `NIMCP_ERROR_MEMORY` is an alias for `NIMCP_ERROR_NO_MEMORY` (both = 2000), so those usages in `neuro_symbolic/` are technically correct
- Thread-local error buffers are used appropriately throughout for thread safety
- Bio-async registration and mesh participant patterns are consistent and correct across all modules
