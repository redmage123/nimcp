# Pass 8 Review: Cognitive Memory/Knowledge/Consolidation/Autobio/Introspection

**Date**: 2026-02-15
**Scope**: `src/cognitive/memory/`, `src/cognitive/knowledge/`, `src/cognitive/consolidation/`, `src/cognitive/autobiographical_memory/`, `src/cognitive/introspection/`
**Method**: Grep-based pattern scanning with targeted context reads (~20 lines per hit)

## Summary

| Priority | Count |
|----------|-------|
| P1 (crash/data corruption) | 9 |
| P2 (wrong behavior/thread-unsafe) | 15 |

---

## Findings

### P1 Findings

---

### [P1] nimcp_knowledge.c:1069 - Div-by-zero - `estimated_total` can be 0

**File**: `src/cognitive/knowledge/nimcp_knowledge.c`

```c
// Line 1069 in update_domain_stats():
stats->coverage_percentage = (float) stats->concepts_known / stats->estimated_total * 100.0F;
```

`estimated_total` is initialized to 1000 in `initialize_domain_stats()` but is a public struct field. If set to 0, this is a floating-point div-by-zero (undefined behavior on some platforms, INF/NaN on others). No guard.

**Fix**: Add `if (stats->estimated_total > 0)` guard.

---

### [P1] nimcp_knowledge.c:2372 - Div-by-zero - Same pattern in `knowledge_assess_domain`

**File**: `src/cognitive/knowledge/nimcp_knowledge.c`

```c
// Line 2372 in knowledge_assess_domain():
assessment->coverage_percentage =
    (float) assessment->concepts_known / assessment->estimated_total * 100.0F;
```

Same issue as above, different call site. `assessment` is copied from `system->domain_stats[domain]` then this division is performed without checking `estimated_total != 0`.

**Fix**: Add `if (assessment->estimated_total > 0)` guard.

---

### [P1] nimcp_pr_snn_bridge.c:626-631 - Unsafe double realloc - Memory leak + use-after-free

**File**: `src/cognitive/memory/core/nimcp_pr_snn_bridge.c`

```c
// Lines 626-634:
float* new_times = (float*)nimcp_realloc(pattern->spike_times, new_capacity * sizeof(float));
uint32_t* new_ids = (uint32_t*)nimcp_realloc(pattern->neuron_ids, new_capacity * sizeof(uint32_t));

if (!new_times || !new_ids) {
    return PR_SNN_ERROR_NO_MEMORY;  // BUG: leaks new_times if new_ids failed
}

pattern->spike_times = new_times;
pattern->neuron_ids = new_ids;
```

If `new_times` succeeds (freeing old `spike_times`) but `new_ids` fails:
1. `new_times` is leaked (never assigned to `pattern->spike_times`)
2. `pattern->spike_times` still points to freed memory (use-after-free)
3. Subsequent access to `pattern->spike_times` is undefined behavior

**Fix**: Assign each realloc result immediately, check individually.

---

### [P1] nimcp_pr_continual_bridge.c:178-213 - Unsafe sequential realloc - Inconsistent state on partial failure

**File**: `src/cognitive/memory/core/nimcp_pr_continual_bridge.c`

```c
// Lines 178-213 in ensure_fisher_capacity():
float* new_fisher = nimcp_realloc(bridge->fisher_diag, num_params * sizeof(float));
if (!new_fisher) { return -1; }
bridge->fisher_diag = new_fisher;
// ... initialize new entries ...

float* new_params = nimcp_realloc(bridge->old_params, num_params * sizeof(float));
if (!new_params) { return -1; }  // BUG: fisher_diag already resized, old_params not
bridge->old_params = new_params;

float* new_accum = nimcp_realloc(bridge->fisher_accum, num_params * sizeof(float));
if (!new_accum) { return -1; }   // BUG: both fisher_diag and old_params resized
```

If the second or third realloc fails, the bridge is left in an inconsistent state where `fisher_diag` has `num_params` entries but `old_params` has `bridge->fisher_capacity` entries. The `fisher_capacity` is never updated on early return, so subsequent code may read out of bounds on the shorter arrays.

**Fix**: Either pre-allocate all three to temp pointers before assigning any, or update `fisher_capacity` only after all three succeed.

---

### [P1] nimcp_knowledge_snn_bridge.c:432 - Div-by-zero - `neurons_per_dim - 1` when `neurons_per_dim == 1`

**File**: `src/cognitive/knowledge/nimcp_knowledge_snn_bridge.c`

```c
// Line 432:
float preferred = (float)n / (neurons_per_dim - 1);
```

When `neurons_per_dim == 1`, this divides by 0. The config field `neurons_per_dim` comes from user configuration. No validation prevents value of 1.

Same pattern in:
- `src/cognitive/consolidation/nimcp_consolidation_snn_bridge.c:430`
- `src/cognitive/autobiographical_memory/nimcp_autobio_snn_bridge.c:443`

**Fix**: Guard with `if (neurons_per_dim <= 1) preferred = 0.0f; else preferred = (float)n / (neurons_per_dim - 1);`

---

### [P1] nimcp_consolidation_snn_bridge.c:430 - Div-by-zero - Same `neurons_per_dim - 1` pattern

**File**: `src/cognitive/consolidation/nimcp_consolidation_snn_bridge.c`

```c
// Line 430:
float preferred = (float)n / (neurons_per_dim - 1);
```

Identical to the knowledge SNN bridge issue. `neurons_per_dim` from config, no validation.

---

### [P1] nimcp_autobio_snn_bridge.c:443 - Div-by-zero - Same `neurons_per_dim - 1` pattern

**File**: `src/cognitive/autobiographical_memory/nimcp_autobio_snn_bridge.c`

```c
// Line 443:
float preferred = (float)n / (neurons_per_dim - 1);
```

Identical to the knowledge and consolidation SNN bridge issues.

---

### [P1] nimcp_schemas.c:2189-2199 - Silent realloc failure - Silently proceeds with undersized array

**File**: `src/cognitive/memory/core/nimcp_schemas.c`

```c
// Lines 2189-2202 in schema_create_hierarchy():
if (parent_mut->num_children >= parent_mut->children_capacity) {
    size_t new_cap = parent_mut->children_capacity * 2;
    if (new_cap > SCHEMA_MAX_CHILDREN) new_cap = SCHEMA_MAX_CHILDREN;

    uint64_t* new_children = (uint64_t*)nimcp_realloc(
        parent_mut->child_schemas, new_cap * sizeof(uint64_t));
    if (new_children) {           // <-- silently ignores failure
        parent_mut->child_schemas = new_children;
        parent_mut->children_capacity = new_cap;
    }
}

if (parent_mut->num_children < parent_mut->children_capacity) {
    parent_mut->child_schemas[parent_mut->num_children] = child->schema_id;
```

If `nimcp_realloc` fails, `children_capacity` is not increased. But `num_children >= children_capacity` was already true, so the write at line 2202 is out-of-bounds. The `if (num_children < children_capacity)` guard protects against the OOB write, but the child is silently not added to the parent with no error returned. This is better described as data corruption (missing hierarchy link, silently dropped).

**Fix**: Return error if realloc fails.

---

### [P1] nimcp_kuramoto.c:124-136 - Thread-unsafe mutable static PRNG

**File**: `src/cognitive/memory/core/nimcp_kuramoto.c`

```c
// Lines 124-136:
static uint32_t g_kuramoto_rng_state = 0;

static uint32_t xorshift32(void) {
    uint32_t x = g_kuramoto_rng_state;
    if (x == 0) {
        x = (uint32_t)time(NULL) ^ 0xDEADBEEF;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_kuramoto_rng_state = x;
    return x;
}
```

Mutable static `g_kuramoto_rng_state` is not thread-safe. Multiple threads calling `xorshift32()` concurrently will produce data races (undefined behavior). Additionally, two threads calling simultaneously when `x == 0` will seed with the same value.

**Fix**: Use `__thread` or CAS-loop atomic PRNG pattern.

---

### P2 Findings

---

### [P2] nimcp_knowledge.c:410 - False positive NIMCP_THROW_TO_IMMUNE on normal "not found" path

**File**: `src/cognitive/knowledge/nimcp_knowledge.c`

```c
// Lines 396-411 in knowledge_hash_table_find():
while (entry) {
    if (strcasecmp(entry->concept, concept_str) == 0) {
        return (int32_t) entry->index;
    }
    entry = entry->next;
}

NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_hash_table_find: validation failed");
return -1;
```

This throw fires on every hash table lookup miss, which is normal search behavior. NIMCP_ERROR_INVALID_PARAM is also the wrong error code - the params are valid, the item just doesn't exist.

---

### [P2] nimcp_knowledge.c:1572 - False positive NIMCP_THROW_TO_IMMUNE on normal flow exit

**File**: `src/cognitive/knowledge/nimcp_knowledge.c`

```c
// Line 1572 at end of process_concept():
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "process_concept: validation failed");
return false;
```

Fires on every concept processing that doesn't match existing concept in repository - this is normal "concept already known" flow. Not an error.

---

### [P2] nimcp_social_memory.c:682 - False positive NIMCP_THROW_TO_IMMUNE on "person not found"

**File**: `src/cognitive/memory/core/nimcp_social_memory.c`

```c
// Lines 673-683 in social_memory_get_person_by_name():
while (entry) {
    if (entry->person->name && strcmp(entry->person->name, name) == 0) {
        return entry->person;
    }
    entry = entry->next;
}

NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "social_memory_get_person_by_name: validation failed");
return NULL;
```

Normal "person not found" path throws to immune system. Also uses wrong error code (INVALID_PARAM vs NOT_FOUND-equivalent).

---

### [P2] nimcp_social_memory.c:2736 - False positive NIMCP_THROW_TO_IMMUNE on "person entry not found"

**File**: `src/cognitive/memory/core/nimcp_social_memory.c`

```c
// Line 2736 in find_person_entry():
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_person_entry: validation failed");
return NULL;
```

Normal hash table miss throws with wrong error code (NULL_POINTER instead of "not found").

---

### [P2] nimcp_social_memory.c:2993 - False positive NIMCP_THROW_TO_IMMUNE on matrix index search

**File**: `src/cognitive/memory/core/nimcp_social_memory.c`

```c
// Line 2993 in get_matrix_index():
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_matrix_index: validation failed");
return -1;
```

Normal "index not found" path for relationship matrix. Called from `assign_matrix_index` on every new person.

---

### [P2] nimcp_social_memory.c:2829 - False positive NIMCP_THROW_TO_IMMUNE on "person not in bucket"

**File**: `src/cognitive/memory/core/nimcp_social_memory.c`

```c
// Line 2829 in remove_person_entry():
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "remove_person_entry: operation failed");
return false;
```

Throws on normal "person not found for removal" path.

---

### [P2] nimcp_pr_predictive_bridge.c:121-126 - Thread-unsafe mutable static PE histories

**File**: `src/cognitive/memory/core/nimcp_pr_predictive_bridge.c`

```c
// Lines 121-126:
static char s_last_error[256] = {0};        // thread-unsafe error buffer
static pe_history_t s_visual_pe_history = {0};
static pe_history_t s_audio_pe_history = {0};
static pe_history_t s_speech_pe_history = {0};
```

All four static variables are read/written without synchronization. The PE histories accumulate values via `add_to_pe_history()` and are read via `compute_variance()`. Comment at line 123 acknowledges: "thread-local would be better".

---

### [P2] nimcp_pr_audio_bridge.c:650 - Thread-unsafe mutable static `last_energy`

**File**: `src/cognitive/memory/core/nimcp_pr_audio_bridge.c`

```c
// Lines 650-652:
static float last_energy = 0.0f;
features->spectral_flux = fabsf(features->rms_energy - last_energy);
last_energy = features->rms_energy;
```

Mutable static read and written without synchronization. Multiple threads processing audio simultaneously will get incorrect spectral flux values (data race).

---

### [P2] nimcp_resonance.c:89 - Thread-unsafe mutable static global stats

**File**: `src/cognitive/memory/core/nimcp_resonance.c`

```c
// Lines 88-89:
/** Global statistics (consider making thread-local or atomic for production) */
static resonance_stats_t s_stats = {0};
```

Comment acknowledges thread safety concern. Stats struct is read and modified by multiple functions without synchronization.

---

### [P2] nimcp_skill_acquisition.c:944 - Thread-unsafe mutable static counter

**File**: `src/cognitive/memory/core/nimcp_skill_acquisition.c`

```c
// Lines 944-945:
static uint64_t next_skill_id = 1;
skill->skill_id = next_skill_id++;
```

Non-atomic increment of static counter. Two threads creating skills simultaneously can get the same ID.

---

### [P2] nimcp_systems_consolidation.c:178 - Thread-unsafe mutable static counter

**File**: `src/cognitive/memory/nimcp_systems_consolidation.c`

```c
// Lines 178-180:
static uint64_t counter = 0;
uint64_t timestamp = nimcp_platform_time_monotonic_ms();
return (timestamp << 16) | (counter++ & 0xFFFF);
```

Non-atomic increment of static counter in `generate_node_id()`. Two threads generating IDs simultaneously can produce duplicates.

---

### [P2] nimcp_temporal_patterns.c:650-658 - Thread-unsafe lazy initialization with static local

**File**: `src/cognitive/introspection/nimcp_temporal_patterns.c`

```c
// Lines 650-658 in get_pattern_context():
static pattern_detection_context_t global_pattern_ctx;
static bool initialized = false;

if (!initialized) {
    memset(&global_pattern_ctx, 0, sizeof(pattern_detection_context_t));
    global_pattern_ctx.config = temporal_pattern_default_config();
    nimcp_mutex_init(&global_pattern_ctx.lock, false);
    initialized = true;
}
```

Classic double-check-locking race: if two threads enter simultaneously before `initialized` is true, both execute `memset` and `nimcp_mutex_init`, corrupting each other's work. Even after initialization, the single `global_pattern_ctx` is shared across all introspection contexts without proper synchronization (beyond the mutex that itself is unsafely initialized).

**Fix**: Use `nimcp_platform_once()` for the initialization.

---

### [P2] nimcp_temporal_patterns.c:601 - Potential div-by-zero - `window` from user config

**File**: `src/cognitive/introspection/nimcp_temporal_patterns.c`

```c
// Lines 570-601:
uint32_t window = cfg->trend_window;
if (window > history_count) {
    window = history_count;
}
// ... 30 lines later ...
trend.mean_value = sum / window;
trend.variance = (sum_sq / window) - (trend.mean_value * trend.mean_value);
```

If user passes a config with `trend_window = 0`, the window stays 0 (not clamped by the `if` check since `0 <= history_count`). Division by zero follows. The `history_count >= 2` guard earlier doesn't help because `window` comes from config, not `history_count`.

**Fix**: Add `if (window == 0) window = history_count;` or `if (window < 1) return trend;`.

---

### [P2] nimcp_kuramoto.c:223-230 - Silent realloc failure

**File**: `src/cognitive/memory/core/nimcp_kuramoto.c`

```c
// Lines 223-230:
uint32_t* new_map = nimcp_realloc(system->module_to_index,
                             new_size * sizeof(uint32_t));
if (new_map) {
    // ... update ...
    system->module_to_index = new_map;
    system->module_map_size = new_size;
}
```

If realloc fails, the function silently continues. The caller proceeds to use `module_to_index[module_id]` which is out-of-bounds since the map was not resized. This will read/write garbage memory.

**Fix**: Return error on realloc failure.

---

### [P2] nimcp_collective_memory.c:216-228 - Partial realloc success leaves inconsistent capacity

**File**: `src/cognitive/memory/core/nimcp_collective_memory.c`

```c
// Lines 216-228:
uint64_t* new_ids = nimcp_realloc(memory->agent_ids, new_cap * sizeof(uint64_t));
if (!new_ids) {
    return COLLECTIVE_ERROR_NO_MEMORY;
}
memory->agent_ids = new_ids;

float* new_versions = nimcp_realloc(memory->agent_versions, new_cap * sizeof(float));
if (!new_versions) {
    /* agent_ids was already updated; agent_versions is still valid at old size */
    return COLLECTIVE_ERROR_NO_MEMORY;
}
```

Comment acknowledges the issue. If `new_versions` fails, `agent_ids` has been resized to `new_cap` but `agent_versions` has not, and `agent_capacity` is never updated. Subsequent code assumes both arrays are the same size. On retry, `agent_ids` will be realloced again (possibly to a larger size than needed), while `agent_versions` may still be at the old size.

---

## Systemic Patterns

### False Positive NIMCP_THROW_TO_IMMUNE on Search/Lookup Paths
Files affected: `nimcp_knowledge.c`, `nimcp_social_memory.c`
Total instances: ~6 (hash table miss, person lookup, matrix index search, etc.)
These throw on every lookup miss, which is normal operation. High-frequency paths in knowledge and social memory.

### Thread-Unsafe Mutable Static Variables (non-mesh-id)
Files affected:
- `nimcp_kuramoto.c` (P1 - PRNG state)
- `nimcp_pr_predictive_bridge.c` (3 PE histories + error buffer)
- `nimcp_pr_audio_bridge.c` (last_energy)
- `nimcp_resonance.c` (global stats)
- `nimcp_skill_acquisition.c` (skill ID counter)
- `nimcp_systems_consolidation.c` (node ID counter)
- `nimcp_temporal_patterns.c` (lazy init race)

Total: 8 instances across 7 files. All mesh_participant_id statics (65+ instances) were excluded as they are module-level IDs set once during init.

### `neurons_per_dim - 1` Div-by-Zero Pattern
Files affected: 3 SNN bridge files (knowledge, consolidation, autobio)
All have identical unguarded `(float)n / (neurons_per_dim - 1)` where `neurons_per_dim` comes from user config.

### Realloc Without Proper Error Handling
Multiple files realloc multiple related arrays sequentially. If the second realloc fails after the first succeeded, the data structure is left in an inconsistent state. Worst case is `nimcp_pr_snn_bridge.c` (use-after-free + leak).
