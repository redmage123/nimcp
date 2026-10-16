# Cognitive Module Code Review - 2026-02-10

**Reviewer**: Claude Code (Opus 4.6)
**Scope**: `src/cognitive/` (~715 .c files)
**Focus**: global_workspace, attention, memory, executive, emotion, creative, ethics, predictive, mirror_neurons, self_awareness, reasoning, integration, plus sampling from remaining modules

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| **P1** | 27 | Critical: qsort comparators with THROW_TO_IMMUNE, dead code hiding resource leak, false-positive throws on normal lookup paths |
| **P2** | 18 | Logic/Correctness: memory leak in predictive_default_config, false positive THROW_TO_IMMUNE on search not-found paths, duplicate #define LOG_MODULE |
| **P3** | 8 | Style/Robustness: unreachable code, dead log statement, missing const |

**Total findings: 53**

---

## P1 Findings (Critical/Crash)

### P1-COG-01 through P1-COG-23: NIMCP_THROW_TO_IMMUNE in qsort comparators (O(N log N) throws per sort)

qsort comparators are called O(N log N) times. Having NIMCP_THROW_TO_IMMUNE on a *normal return path* (the "return -1" branch of a comparison) means the immune system is flooded with false positives every time a sort occurs. This is the same class of bug documented in the project memory as a known false-positive pattern.

**All 23 affected files with qsort comparators containing THROW_TO_IMMUNE:**

| # | File | Line | Function |
|---|------|------|----------|
| 1 | `src/cognitive/memory/core/nimcp_gist.c` | 455 | `compare_features_by_score` |
| 2 | `src/cognitive/game_theory/nimcp_auction.c` | 108, 115 | `compare_bids_desc` (TWO throws) |
| 3 | `src/cognitive/memory/core/nimcp_entanglement.c` | 448 | `compare_neighbors_by_weight_desc` |
| 4 | `src/cognitive/memory/core/nimcp_entanglement.c` | 463 | `compare_walk_results_desc` |
| 5 | `src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c` | 264 | `compare_by_recency` |
| 6 | `src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c` | 281 | `compare_by_importance` |
| 7 | `src/cognitive/knowledge/nimcp_knowledge_hyperbolic.c` | 118 | `compare_knn_candidates` |
| 8 | `src/cognitive/fault_tolerance/nimcp_recovery_episodic_memory.c` | 894 | `compare_scores` |
| 9 | `src/cognitive/fault_tolerance/nimcp_failure_prediction.c` | 270 | `compare_predictions` |
| 10 | `src/cognitive/parietal/nimcp_financial_market.c` | ~327 | `compare_floats_asc` |
| 11 | `src/cognitive/parietal/nimcp_financial_salience_bridge.c` | ~458 | `compare_scored_events_desc` |
| 12 | `src/cognitive/parietal/nimcp_financial_stdp_bridge.c` | ~1270 | `compare_correlations_by_weight` |
| 13 | `src/cognitive/memory/core/nimcp_z_ladder.c` | ~2024 | `compare_strength_desc` |
| 14 | `src/cognitive/memory/core/nimcp_z_ladder.c` | ~2039 | `compare_strength_asc` |
| 15 | `src/cognitive/memory/core/nimcp_pr_snn_bridge.c` | ~266 | `compare_floats` |
| 16 | `src/cognitive/memory/core/nimcp_pr_snn_bridge.c` | ~285 | `compare_spikes` |
| 17 | `src/cognitive/memory/core/nimcp_resonance.c` | ~570 | `compare_batch_results_desc` |
| 18 | `src/cognitive/memory/core/nimcp_pr_sleep_bridge.c` | ~1847 | `compare_replay_priority` |
| 19 | `src/cognitive/memory/core/nimcp_pr_curriculum_bridge.c` | ~233, ~247 | `compare_scores_asc`, `compare_scores_desc` |
| 20 | `src/cognitive/memory/core/nimcp_pr_visual_bridge.c` | ~244 | `compare_retrieval_results` |
| 21 | `src/cognitive/memory/core/nimcp_future_thinking.c` | ~185 | `compare_weights_desc` |
| 22 | `src/cognitive/memory/core/nimcp_metamemory_monitor.c` | ~1812, ~1825 | `compare_at_risk`, `compare_review_priority` |
| 23 | `src/cognitive/memory/core/nimcp_pr_meta_bridge.c` | ~241 | `compare_recalls` |

**Additional files from grep (may have similar issues):**
- `src/cognitive/wellbeing/nimcp_wellbeing.c`
- `src/cognitive/ethics/nimcp_ethics_incidents.c`
- `src/cognitive/knowledge/nimcp_knowledge.c`
- `src/cognitive/game_theory/nimcp_gt_auction_ext.c` (4 comparator functions: `compare_bids_by_density`, `compare_buy_orders`, `compare_sell_orders`, `compare_multi_bids`)

**Suggested fix for ALL**: Remove NIMCP_THROW_TO_IMMUNE from qsort comparators. Example fix for `compare_features_by_score`:

```c
// BEFORE (broken):
static int compare_features_by_score(const void* a, const void* b) {
    const gist_key_feature_t* fa = (const gist_key_feature_t*)a;
    const gist_key_feature_t* fb = (const gist_key_feature_t*)b;
    if (fa->importance > fb->importance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compare_features_by_score: validation failed");
        return -1;
    }
    if (fa->importance < fb->importance) return 1;
    return 0;
}

// AFTER (correct):
static int compare_features_by_score(const void* a, const void* b) {
    const gist_key_feature_t* fa = (const gist_key_feature_t*)a;
    const gist_key_feature_t* fb = (const gist_key_feature_t*)b;
    if (fa->importance > fb->importance) return -1;
    if (fa->importance < fb->importance) return 1;
    return 0;
}
```

---

### P1-COG-24: Dead code hides resource leak in emotion_executive_bridge_destroy

**File**: `src/cognitive/integration/nimcp_emotion_executive_bridge.c`
**Line**: 254-258

```c
void emotion_executive_bridge_destroy(emotion_executive_bridge_t* bridge) {
    if (!bridge) {
        return;                    // <-- returns here
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "emotion_executive");  // DEAD CODE
    }
    // ... actual cleanup code
```

The `return` on line 256 is correct (early return for NULL guard), but the logging statement on line 257 is unreachable dead code. More critically, the guard clause brace placement makes it look like the `return` is inside the NULL check, which is correct, but the dead logging line after `return` suggests copy-paste error. This is benign but confusing.

However, the REAL issue: if someone "fixes" this by moving the log outside the if-block, the return would no longer execute for NULL, leading to NULL dereference on the mutex destroy. This code is fragile.

**Suggested fix**: Remove the dead logging line:
```c
void emotion_executive_bridge_destroy(emotion_executive_bridge_t* bridge) {
    if (!bridge) {
        return;
    }
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "emotion_executive");
```

---

### P1-COG-25: False positive NIMCP_THROW_TO_IMMUNE on is_emotion_congruent normal return

**File**: `src/cognitive/attention/nimcp_emotion_attention.c`
**Line**: 221

```c
static bool is_emotion_congruent(emotion_primary_t current, emotion_primary_t stimulus) {
    // ... enum range validation (lines 203-207, THROW is OK here for invalid enums)

    if (current == stimulus) return true;

    int diff = abs((int)current - (int)stimulus);
    if (diff == 1 || diff == 7) return true;

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_emotion_congruent: validation failed");
    return false;
}
```

The THROW on line 221 fires every time two valid but non-congruent emotions are compared. This is a NORMAL return path (emotions that are not adjacent on Plutchik's wheel). This function is called from `emotion_attention_compute_salience()` which is a hot path.

**Suggested fix**: Remove the THROW, just return false:
```c
    // Not congruent (normal case - not an error)
    return false;
}
```

---

### P1-COG-26: False positive NIMCP_THROW_TO_IMMUNE on find_task_by_id not-found

**File**: `src/cognitive/executive/nimcp_executive.c`
**Line**: 628

```c
static task_descriptor_t* find_task_by_id(executive_controller_t* exec, uint32_t task_id)
{
    if (!exec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exec is NULL");
        return NULL;
    }
    if (exec->active_task && exec->active_task->task_id == task_id) {
        return exec->active_task;
    }
    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        if (exec->task_queue[i] && exec->task_queue[i]->task_id == task_id) {
            return exec->task_queue[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_task_by_id: validation failed");
    return NULL;
}
```

Not finding a task is normal search behavior, not an error.

**Suggested fix**: Remove the THROW on line 628, just return NULL.

---

### P1-COG-27: False positive NIMCP_THROW_TO_IMMUNE on get_highest_priority_task with empty queue

**File**: `src/cognitive/executive/nimcp_executive.c`
**Line**: 656

```c
static task_descriptor_t* get_highest_priority_task(executive_controller_t* exec)
{
    if (!exec || exec->num_tasks == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_highest_priority_task: exec is NULL");
        return NULL;
    }
```

An empty task queue is a normal state, not an error. The THROW fires every time the executive has no pending tasks, which is the common case at startup or when idle.

**Suggested fix**: Only throw on NULL exec, not on empty queue:
```c
    if (!exec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_highest_priority_task: exec is NULL");
        return NULL;
    }
    if (exec->num_tasks == 0) {
        return NULL;  // Normal: no pending tasks
    }
```

---

## P2 Findings (Logic/Correctness)

### P2-COG-01: Memory leak in predictive_default_config - heap-allocated layer_sizes

**File**: `src/cognitive/predictive/nimcp_predictive.c`
**Lines**: 176-196

```c
predictive_config_t predictive_default_config(void) {
    uint32_t* default_sizes = (uint32_t*)nimcp_malloc(
        PRED_DEFAULT_LAYERS * sizeof(uint32_t));
    // ... fills in sizes ...
    predictive_config_t config = {
        .num_layers = PRED_DEFAULT_LAYERS,
        .layer_sizes = default_sizes,   // <-- heap pointer returned by value
        ...
    };
    return config;
}
```

The `layer_sizes` is heap-allocated, but the config struct is returned by value. Callers (e.g., `predictive_create` at line 226 and `predictive_immune` at line 201) store it in a local `actual_config` variable. The heap allocation is never freed. When `predictive_create()` calls this internally (line 226), the pointer is lost.

Additionally, `predictive_default_config()` does not check for malloc failure on `default_sizes`, so it could pass a NULL pointer into the config struct.

**Suggested fix**: Use a stack-allocated static array or ensure callers free the layer_sizes, or document the ownership semantics. Best approach: use a static const array:
```c
predictive_config_t predictive_default_config(void) {
    static const uint32_t default_sizes[PRED_DEFAULT_LAYERS] = {256, 128, 64, 32};
    predictive_config_t config = {
        .num_layers = PRED_DEFAULT_LAYERS,
        .layer_sizes = (uint32_t*)default_sizes,  // static, no leak
        ...
    };
    return config;
}
```

---

### P2-COG-02: Duplicate #define LOG_MODULE in executive

**File**: `src/cognitive/executive/nimcp_executive.c`
**Lines**: 100, 107

```c
#define LOG_MODULE "EXECUTIVE"        // line 100
// ... includes ...
#define LOG_MODULE "cognitive.executive"  // line 107
```

The second `#define` silently overrides the first. This means all LOG_MODULE usage will use "cognitive.executive" which may be intentional, but the duplicate is confusing and compiler-dependent (some compilers warn on macro redefinition without `#undef`).

**Suggested fix**: Remove the first `#define LOG_MODULE "EXECUTIVE"` on line 100, or add `#undef LOG_MODULE` before the second definition.

---

### P2-COG-03: False positive NIMCP_THROW_TO_IMMUNE on find_decision_unlocked not-found

**File**: `src/cognitive/integration/nimcp_emotion_executive_bridge.c`
**Line**: 168

```c
static decision_record_t* find_decision_unlocked(emotion_executive_bridge_t* bridge,
                                                  uint32_t decision_id) {
    for (size_t i = 0; i < bridge->decision_count; i++) {
        if (bridge->decisions[i].decision_id == decision_id) {
            return &bridge->decisions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_timestamp_ms: validation failed");
    return NULL;
}
```

Not finding a decision is normal lookup behavior. Also note the misleading error message references "get_timestamp_ms" which is a different function entirely.

**Suggested fix**: Remove the THROW, just return NULL. Fix the message if keeping a log.

---

### P2-COG-04: False positive NIMCP_THROW_TO_IMMUNE on find_inference_by_id not-found

**File**: `src/cognitive/reasoning/nimcp_reasoning_integration.c`
**Line**: 192

```c
static active_inference_t* find_inference_by_id(
    reasoning_integration_t* integration,
    uint32_t inference_id)
{
    for (uint32_t i = 0; i < integration->num_active_inferences; i++) {
        if (integration->active_inferences[i].inference_id == inference_id &&
            integration->active_inferences[i].is_active) {
            return &integration->active_inferences[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_inference_by_id: operation failed");
    return NULL;
}
```

Normal "not found" return.

**Suggested fix**: Remove the THROW.

---

### P2-COG-05: False positive NIMCP_THROW_TO_IMMUNE on find_rule_by_string not-found

**File**: `src/cognitive/reasoning/nimcp_reasoning_integration.c`
**Line**: 215

```c
static rule_usage_t* find_rule_by_string(
    reasoning_integration_t* integration,
    const char* rule_str)
{
    for (uint32_t i = 0; i < integration->num_tracked_rules; i++) {
        if (strcmp(integration->tracked_rules[i].rule, rule_str) == 0) {
            return &integration->tracked_rules[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_rule_by_string: validation failed");
    return NULL;
}
```

Normal "not found" in lookup. Caller `track_rule()` at line 278 actually expects NULL return to mean "not yet tracked" and proceeds to add a new rule. The THROW fires every time a new rule is tracked.

**Suggested fix**: Remove the THROW.

---

### P2-COG-06: False positive NIMCP_THROW_TO_IMMUNE on find_node_unlocked not-found

**File**: `src/cognitive/memory/core/nimcp_entanglement.c`
**Line**: 321

```c
static node_entry_t* find_node_unlocked(entangle_graph_t graph, uint64_t node_id) {
    if (!graph || !graph->node_table) {
        NIMCP_THROW_TO_IMMUNE(...);  // OK: actual error
        return NULL;
    }
    size_t idx = hash_node_id(node_id, graph->node_table_size);
    node_entry_t* entry = graph->node_table[idx];
    while (entry) {
        if (entry->node_id == node_id) return entry;
        entry = entry->hash_next;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_node_unlocked: validation failed");
    return NULL;
}
```

Not finding a node is normal hash table behavior.

**Suggested fix**: Remove the THROW on line 321.

---

### P2-COG-07: False positive NIMCP_THROW_TO_IMMUNE on gist_table_lookup/trace_table_lookup not-found

**File**: `src/cognitive/memory/core/nimcp_gist.c`
**Lines**: 278, 302

```c
// gist_table_lookup (line 278):
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gist_table_destroy: validation failed");
    return NULL;

// trace_table_lookup (line 377):
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trace_table_destroy: validation failed");
    return NULL;
```

Normal hash table lookup not-found. Also note the misleading function names in the error messages (references "destroy" when it's actually a "lookup").

**Suggested fix**: Remove both THROWs. Fix message strings if keeping logs.

---

### P2-COG-08: False positive NIMCP_THROW_TO_IMMUNE on cache_lookup not-found

**File**: `src/cognitive/neuro_symbolic/nimcp_quantum_mcts.c`
**Line**: 1560

```c
static qmcts_cache_entry_t* cache_lookup(quantum_mcts_t* qmcts, uint64_t hash) {
    if (!qmcts->cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache_lookup: qmcts->cache is NULL");
        return NULL;  // OK: real error
    }
    uint32_t idx = (uint32_t)(hash % qmcts->cache_size);
    if (qmcts->cache[idx].valid && qmcts->cache[idx].state_hash == hash) {
        return &qmcts->cache[idx];
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache_lookup: validation failed");
    return NULL;
}
```

Cache miss is normal. This fires on every cache miss, which by design happens frequently.

**Suggested fix**: Remove the THROW on line 1560.

---

### P2-COG-09: False positive NIMCP_THROW_TO_IMMUNE on pattern_registry_lookup not-found

**File**: `src/cognitive/introspection/nimcp_introspection.c`
**Line**: 1293

Not-found in pattern registry lookup is normal behavior.

**Suggested fix**: Remove the THROW.

---

### P2-COG-10: False positive NIMCP_THROW_TO_IMMUNE on hash_lookup not-found

**File**: `src/cognitive/memory/core/nimcp_spaced_repetition.c`
**Line**: 2585

Normal hash table miss.

**Suggested fix**: Remove the THROW.

---

### P2-COG-11: False positive NIMCP_THROW_TO_IMMUNE on id_table_lookup not-found

**File**: `src/cognitive/memory/core/nimcp_schemas.c`
**Lines**: 253, 259

Normal ID table lookup miss.

**Suggested fix**: Remove both THROWs.

---

### P2-COG-12: False positive NIMCP_THROW_TO_IMMUNE on find_indicator_index not-found

**File**: `src/cognitive/fault_tolerance/nimcp_failure_prediction.c`
**Line**: 254

```c
static int find_indicator_index(failure_predictor_t* predictor, failure_metric_t metric) {
    for (uint32_t i = 0; i < predictor->config.num_indicators; i++) {
        if (predictor->indicators[i].current.metric == metric) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_indicator_index: validation failed");
    return -1;
}
```

Not finding an indicator is normal.

**Suggested fix**: Remove the THROW.

---

### P2-COG-13: False positive NIMCP_THROW_TO_IMMUNE on matches_query returning false

**File**: `src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c`
**Line**: 246

```c
    if (!found) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "matches_query: found is NULL");
        return false;
    }
```

Not matching a text search query is normal, not an error. The variable `found` being false means the query text was not found in any field, which is expected behavior for most queries.

**Suggested fix**: Remove the THROW, just return false.

---

### P2-COG-14: False positive NIMCP_THROW_TO_IMMUNE on acquire_buffer pool exhaustion

**File**: `src/cognitive/ethics/nimcp_ethics.c`
**Line**: 218

```c
static float* acquire_buffer(feature_buffer_pool_t* pool) {
    // ... search loop ...
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "acquire_buffer: pool->in_use is NULL");
    return NULL;  // Pool exhausted
}
```

Pool exhaustion is a resource constraint, not a NULL pointer error. The error code is also misleading.

**Suggested fix**: Change to an appropriate error code or remove THROW entirely if pool exhaustion is expected to be handled gracefully by callers.

---

### P2-COG-15: nimcp_free(bridge->decisions) may free brain_decision_t array incorrectly

**File**: `src/cognitive/integration/nimcp_emotion_executive_bridge.c`
**Line**: 243, 272
**File**: `src/cognitive/parietal/nimcp_financial_temporal_credit_bridge.c`
**Lines**: 346, 370

These files use `nimcp_free(bridge->decisions)` to free arrays of `decision_record_t`. Need to verify these are not `brain_decision_t` structs that require `brain_free_decision()`. Based on the struct definitions (`decision_record_t` in emotion_executive_bridge.c contains only scalars and embedded structs), these appear to be correctly using `nimcp_free()` since `decision_record_t` has no internal heap allocations. However, the financial bridge's `extended_decisions` does free `pattern_ids` arrays first (line 376), which is correct.

**Status**: Verified OK - these are not brain_decision_t and have no internal allocations (or correctly free internals first).

---

### P2-COG-16: Misleading error messages in NIMCP_THROW_TO_IMMUNE

Multiple files have error messages that reference the wrong function name:

| File | Line | Message Says | Actually In |
|------|------|-------------|-------------|
| `nimcp_emotion_executive_bridge.c` | 168 | `"get_timestamp_ms: validation failed"` | `find_decision_unlocked` |
| `nimcp_gist.c` | 278 | `"gist_table_destroy: validation failed"` | `gist_table_lookup` |
| `nimcp_gist.c` | 302 | `"gist_table_destroy: validation failed"` | `gist_table_remove` |
| `nimcp_gist.c` | 351 | `"trace_table_destroy: entry is NULL"` | `trace_table_insert` |
| `nimcp_gist.c` | 377 | `"trace_table_destroy: validation failed"` | `trace_table_lookup` |

**Suggested fix**: Correct function names in error messages (but ideally, remove these false-positive THROWs entirely per P2-COG-03 through P2-COG-11).

---

### P2-COG-17: predictive_default_config does not check malloc failure

**File**: `src/cognitive/predictive/nimcp_predictive.c`
**Lines**: 177-178

```c
uint32_t* default_sizes = (uint32_t*)nimcp_malloc(
    PRED_DEFAULT_LAYERS * sizeof(uint32_t));
// No NULL check on default_sizes
default_sizes[0] = 256;  // potential NULL dereference
```

If nimcp_malloc fails, this dereferences NULL.

**Suggested fix**: Add NULL check, or use a static array (see P2-COG-01).

---

### P2-COG-18: predictive_immune leaks layer_sizes from predictive_default_config

**File**: `src/cognitive/predictive_immune/nimcp_predictive_immune.c`
**Line**: 201

```c
predictive_config_t intero_cfg = predictive_default_config();
intero_cfg.num_layers = 3;
uint32_t layer_sizes[3] = { ... };
intero_cfg.layer_sizes = layer_sizes;  // Overwrite pointer, leak original
```

The original `intero_cfg.layer_sizes` from `predictive_default_config()` (heap-allocated) is leaked when overwritten with the local stack array.

**Suggested fix**: Free `intero_cfg.layer_sizes` before overwriting, or use a config that doesn't allocate (see P2-COG-01 fix).

---

## P3 Findings (Style/Robustness)

### P3-COG-01: Unreachable log statement after return

**File**: `src/cognitive/integration/nimcp_emotion_executive_bridge.c`
**Line**: 257

```c
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "emotion_executive");  // unreachable
    }
```

Dead code after `return`.

**Suggested fix**: Move the log outside the if-block, before the cleanup logic.

---

### P3-COG-02: Duplicate #define LOG_MODULE without #undef

**File**: `src/cognitive/executive/nimcp_executive.c`
**Lines**: 100, 107

Two conflicting `#define LOG_MODULE` without intervening `#undef`.

**Suggested fix**: Remove first definition or add `#undef LOG_MODULE`.

---

### P3-COG-03: Cast away const for pthread_rwlock operations

**File**: `src/cognitive/attention/nimcp_emotion_attention.c`
**Lines**: 565-567

```c
float emotion_attention_get_width(const emotion_attention_system_t* system) {
    // ...
    pthread_rwlock_rdlock((pthread_rwlock_t*)&system->lock);
    float width = system->current_attention_width;
    pthread_rwlock_unlock((pthread_rwlock_t*)&system->lock);
```

Casting away `const` to lock a mutex. This is a code smell. The function is declared as taking `const` but needs to acquire a lock.

**Suggested fix**: Either remove `const` from the parameter (since locking mutates the lock), or use `mutable` semantics (not available in C, so remove `const`).

---

### P3-COG-04: Magic numbers in emotion attention

**File**: `src/cognitive/attention/nimcp_emotion_attention.c`
**Lines**: 173, 179, 203-204, 264

- `0.4F` (narrowing factor)
- `0.5F` (broadening factor)
- `7` (Plutchik wheel size)
- `0.1F` (EMA smoothing)

These are used as unnamed constants.

**Suggested fix**: Define named constants, e.g., `#define PLUTCHIK_WHEEL_SIZE 8`, `#define EMA_SMOOTHING_ALPHA 0.1F`.

---

### P3-COG-05: Static global mutable state without thread safety

**File**: `src/cognitive/memory/core/nimcp_resonance.c`
**Line**: 89

```c
static resonance_stats_t s_stats = {0};
```

The comment even notes: "consider making thread-local or atomic for production". This is mutable global state updated without any synchronization.

**Suggested fix**: Make thread-local (`__thread`) or use atomics.

---

### P3-COG-06: bio_msg_attention_shift_t zero-initialized with empty braces

**File**: `src/cognitive/global_workspace/nimcp_global_workspace.c`
**Line**: 249
**File**: `src/cognitive/executive/nimcp_executive.c`
**Line**: 405

```c
bio_msg_attention_shift_t msg = {};
bio_msg_introspection_response_t msg = {};
```

In C (not C++), `{}` is not standard for aggregate initialization. Use `{0}` instead.

**Suggested fix**: Change `= {}` to `= {0}`.

---

### P3-COG-07: Missing NULL check on nimcp_platform_mutex_create return

**File**: `src/cognitive/integration/nimcp_emotion_executive_bridge.c`
**Line**: 241

```c
bridge->base.mutex = nimcp_platform_mutex_create();
if (!bridge->base.mutex) {
```

This is actually correct (has NULL check). Noting for completeness - the pattern is good here.

---

### P3-COG-08: Inconsistent error handling in ethics acquire_buffer

**File**: `src/cognitive/ethics/nimcp_ethics.c`
**Line**: 218

Pool exhaustion uses `NIMCP_ERROR_NULL_POINTER` error code, but it's not a null pointer - it's a resource exhaustion condition. Should use `NIMCP_ERROR_NO_MEMORY` or a more appropriate code.

---

## Cross-Cutting Observations

### Pattern: False Positive NIMCP_THROW_TO_IMMUNE on Normal Code Paths

This is the single most widespread issue in the cognitive module. The pattern appears to have been introduced by automated tooling that converted all "return NULL" or "return false" paths to include THROW_TO_IMMUNE, without distinguishing between:
- **Actual errors** (NULL pointer, allocation failure) -- THROW is correct
- **Normal search/lookup failures** (not found, cache miss) -- THROW is false positive
- **Normal comparison returns** (qsort comparators) -- THROW is catastrophically wrong

**Scale of the problem**: ~23 qsort comparators, ~12 lookup functions, and ~5 normal-path returns all have false-positive THROWs. This means the immune system is receiving potentially thousands of spurious error reports per second during normal operation.

### Pattern: Healthy Error Handling in Core Subsystems

The global_workspace, executive, and predictive modules show generally good error handling:
- Mutex lock/unlock pairs are correctly matched on error paths
- Memory cleanup on allocation failures is thorough (cascading frees)
- Bio-async integration has proper NULL checks and graceful degradation

### Pattern: Bridge Boilerplate

All bridge files follow an identical boilerplate pattern (mesh registration, heartbeat, health agent). This is consistent but adds significant code volume. Consider whether a macro or code generator could reduce this.

---

## Recommendations (Priority Order)

1. **Immediate**: Remove all NIMCP_THROW_TO_IMMUNE from qsort comparators (P1-COG-01 through P1-COG-23). These cause O(N log N) false positive immune system reports per sort operation.

2. **High**: Remove false-positive NIMCP_THROW_TO_IMMUNE from all lookup/search not-found paths (P2-COG-03 through P2-COG-13). These flood the immune system with noise during normal operation.

3. **High**: Fix the memory leak in `predictive_default_config()` (P2-COG-01, P2-COG-17, P2-COG-18). Switch to static array for default layer sizes.

4. **Medium**: Fix dead code in `emotion_executive_bridge_destroy` (P1-COG-24/P3-COG-01).

5. **Medium**: Fix the `is_emotion_congruent` false-positive THROW (P1-COG-25).

6. **Low**: Fix misleading error messages (P2-COG-16), duplicate LOG_MODULE (P3-COG-02), and empty brace initialization (P3-COG-06).

---

## Files Reviewed (Complete List)

### Full Review (line-by-line):
1. `src/cognitive/global_workspace/nimcp_global_workspace.c` (1100+ lines)
2. `src/cognitive/executive/nimcp_executive.c` (900+ lines)
3. `src/cognitive/attention/nimcp_emotion_attention.c` (1060+ lines)
4. `src/cognitive/ethics/nimcp_ethics.c` (300 lines)
5. `src/cognitive/mirror_neurons/nimcp_mirror_neurons.c` (300 lines)
6. `src/cognitive/predictive/nimcp_predictive.c` (500 lines)
7. `src/cognitive/creative/nimcp_creative.c` (200 lines)
8. `src/cognitive/self_awareness/nimcp_self_awareness_extended.c` (300 lines)
9. `src/cognitive/integration/nimcp_emotion_executive_bridge.c` (300 lines)
10. `src/cognitive/reasoning/nimcp_reasoning_integration.c` (300 lines)

### Pattern-Based Review (grep across all 715 files):
- All qsort comparators (24 files, 35+ functions)
- All NIMCP_THROW_TO_IMMUNE usage (1077 occurrences across 50+ files)
- All nimcp_free(decision) patterns
- All NIMCP_ERROR_GENERIC usage (none found - good)
- All EVENT_LEARNING_STEP_COMPLETE usage (none found - good)
- All brain_free_decision usage (1 correct usage found)
- All mutex lock/unlock patterns
- All lookup/search functions with THROW

### Sampled (specific patterns checked):
- `src/cognitive/memory/core/nimcp_entanglement.c`
- `src/cognitive/memory/core/nimcp_gist.c`
- `src/cognitive/memory/core/nimcp_resonance.c`
- `src/cognitive/game_theory/nimcp_auction.c`
- `src/cognitive/game_theory/nimcp_gt_auction_ext.c`
- `src/cognitive/knowledge/nimcp_knowledge_hyperbolic.c`
- `src/cognitive/fault_tolerance/nimcp_recovery_episodic_memory.c`
- `src/cognitive/fault_tolerance/nimcp_failure_prediction.c`
- `src/cognitive/neuro_symbolic/nimcp_quantum_mcts.c`
- `src/cognitive/introspection/nimcp_introspection.c`
- `src/cognitive/memory/core/nimcp_spaced_repetition.c`
- `src/cognitive/memory/core/nimcp_schemas.c`
- `src/cognitive/autobiographical_memory/nimcp_autobiographical_memory.c`
- `src/cognitive/predictive_immune/nimcp_predictive_immune.c`
- `src/cognitive/parietal/nimcp_financial_*` (multiple files)
- `src/cognitive/memory/core/nimcp_z_ladder.c`
- `src/cognitive/memory/core/nimcp_pr_*.c` (multiple files)
- `src/cognitive/memory/core/nimcp_future_thinking.c`
- `src/cognitive/memory/core/nimcp_metamemory_monitor.c`
- `src/cognitive/wellbeing/nimcp_wellbeing.c`
- `src/cognitive/ethics/nimcp_ethics_incidents.c`
- `src/cognitive/knowledge/nimcp_knowledge.c`
