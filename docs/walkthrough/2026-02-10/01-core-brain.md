# NIMCP Core Code Review - Pass 4 (2026-02-10)

## Scope

Review of all `.c` source files under `src/core/` covering:
- `brain/` (brain core, factory, hemispheric, inference, subcortical, regions, KG, etc.)
- `brain_regions/`, `brain_oscillations/`, `brainstem/`, `cerebellum/`, `cingulate/`
- `cortical_columns/`, `hippocampus/`, `hypothalamus/`, `insula/`, `medulla/`
- `occipital/`, `parietal/`, `prefrontal/`, `temporal/`
- `events/`, `integration/`, `axon/`, `dendrite/`, `synapse_types/`, `synapse_compute/`
- `neuralnet/`, `neural_substrate/`, `neuron_models/`, `neuron_types/`
- `topology/`, `geometry/`, `sensory/`, `motor/`
- `directives/`, `logic/`

**Total files reviewed**: ~370+ `.c` files across all `src/core/` directories.

---

## P1 Findings (Critical/Crash)

### P1-1: `nimcp_brain_validation.c` uses shallow `copy_decision()` for cache -- heap corruption

**File**: `/home/bbrelin/nimcp/src/core/brain/factory/validation/nimcp_brain_validation.c`
**Line**: 172

```c
brain_decision_t* new_cached = copy_decision(decision);
```

**Bug**: The function `nimcp_brain_factory_cache_decision()` uses shallow `copy_decision()` (CoW) instead of `copy_decision_deep()`. Per project rules, brain cache MUST use `copy_decision_deep()` because CoW's shared refcount breaks when the original is freed while the cache retains a reference. This was documented as a critical rule in MEMORY.md.

**Impact**: Heap corruption / use-after-free when the original decision is freed and the cache still holds a shallow CoW reference to the freed output_vector/active_neuron_ids.

**Fix**: Change line 172 to:
```c
brain_decision_t* new_cached = copy_decision_deep(decision);
```
Also update the extern declaration at line 105 from:
```c
extern brain_decision_t* copy_decision(brain_decision_t* decision);
```
to:
```c
extern brain_decision_t* copy_decision_deep(const brain_decision_t* decision);
```

---

### P1-2: `nimcp_brain_validation.c:nimcp_brain_factory_is_cached_input()` false-positive NIMCP_THROW_TO_IMMUNE

**File**: `/home/bbrelin/nimcp/src/core/brain/factory/validation/nimcp_brain_validation.c`
**Lines**: 113-121

```c
bool nimcp_brain_factory_is_cached_input(brain_t brain, const float* features, uint32_t num_features)
{
    if (!brain->last_input || !brain->cached_decision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_brain_factory_is_cached_input: invalid parameter");
        return false;
    }
    if (brain->input_size != num_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_brain_factory_is_cached_input: invalid parameter");
        return false;
    }
```

**Bug**: These are NOT errors -- they are normal "cache miss" / "cache empty" conditions. Having `last_input == NULL` or `cached_decision == NULL` simply means nothing is cached yet. The corresponding function in `nimcp_brain.c` (line 947-957) handles these identically but WITHOUT NIMCP_THROW_TO_IMMUNE:
```c
static bool is_cached_input(brain_t brain, const float* features, uint32_t num_features) {
    if (!brain->last_input || !brain->cached_decision) return false;
    if (brain->input_size != num_features) return false;
    return memcmp(...) == 0;
}
```

**Impact**: On every first inference (before any cache exists), the immune system is needlessly triggered with a false-positive error report. This adds overhead and pollutes immune system logs.

**Fix**: Remove both `NIMCP_THROW_TO_IMMUNE` calls. Simply return false (cache miss is normal behavior).

---

### P1-3: `nimcp_distributed_cow.c:find_cached_segment()` false-positive NIMCP_THROW_TO_IMMUNE on cache miss

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_distributed_cow.c`
**Line**: 513

```c
state->cache_misses++;
nimcp_platform_rwlock_rdunlock(&state->cache_lock);
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_cached_segment: validation failed");
return NULL;
```

**Bug**: A cache miss is normal operation, not an error. This fires the immune system on EVERY cache miss (every new segment fetch). The function explicitly tracks `cache_misses++`, acknowledging this is expected.

**Impact**: Massive immune system overhead during normal distributed COW operation. Could trigger immune cascade responses to non-existent threats.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call. Just return NULL.

---

### P1-4: `nimcp_distributed_cow.c:find_cached_segment()` writes to shared state under read lock

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_distributed_cow.c`
**Lines**: 505, 511

```c
nimcp_platform_rwlock_rdlock(&state->cache_lock);

for (...) {
    if (...) {
        state->cache_hits++;   // <-- WRITE under READ lock
        nimcp_platform_rwlock_rdunlock(&state->cache_lock);
        return seg;
    }
}

state->cache_misses++;   // <-- WRITE under READ lock
nimcp_platform_rwlock_rdunlock(&state->cache_lock);
```

**Bug**: `cache_hits++` and `cache_misses++` are write operations performed while holding only a read lock. With concurrent readers, this is a data race (non-atomic increment under rwlock read).

**Impact**: Data race on `cache_hits` and `cache_misses` counters. Could cause torn reads/writes on 64-bit counters.

**Fix**: Either use `__atomic_fetch_add()` for the counters, or upgrade to a write lock, or move counter updates outside the lock.

---

### P1-5: `nimcp_distributed_cow.c:register_distributed_cow_brain()` buffer overflow -- no bounds check

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_distributed_cow.c`
**Lines**: 737-751

```c
if (!g_dcow_brains) {
    g_dcow_brains = nimcp_calloc(16, sizeof(distributed_cow_brain_t*));
}
// ...
g_dcow_brains[g_num_dcow_brains++] = entry;
```

**Bug**: The array is allocated with capacity 16, but `g_num_dcow_brains` is never checked against this capacity. After 16 registrations, this writes past the end of the array (buffer overflow).

**Impact**: Heap buffer overflow leading to memory corruption and crash.

**Fix**: Add a bounds check:
```c
if (g_num_dcow_brains >= 16) {
    // Reallocate or return error
    nimcp_platform_mutex_unlock(&g_registry_mutex);
    nimcp_free(entry);
    return false;
}
g_dcow_brains[g_num_dcow_brains++] = entry;
```

---

### P1-6: `nimcp_distributed_cow.c:find_distributed_cow_state()` false-positive NIMCP_THROW_TO_IMMUNE on lookup miss

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_distributed_cow.c`
**Line**: 773

```c
nimcp_platform_mutex_unlock(&g_registry_mutex);
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_distributed_cow_state: validation failed");
return NULL;
```

**Bug**: Not finding a brain in the distributed COW registry is a valid return (the brain may not be distributed). `brain_is_distributed_cow()` calls this function and expects NULL return for non-distributed brains.

**Impact**: False immune system trigger on every call to `brain_is_distributed_cow()` for non-distributed brains.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call.

---

### P1-7: `nimcp_hemispheric_brain.c:hemispheric_brain_validate_config()` wrong error code

**File**: `/home/bbrelin/nimcp/src/core/brain/hemispheric/nimcp_hemispheric_brain.c`
**Lines**: 112-113

```c
if (config->left_resource_fraction < 0.0f ||
    config->left_resource_fraction > 1.0f) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_brain_validate_config: config is NULL");
    return false;
}
```

**Bug**: Uses `NIMCP_ERROR_NULL_POINTER` error code and "config is NULL" message for what is actually a range validation failure. This is a copy-paste error from the guard clause above it.

**Impact**: Wrong error diagnostic reported to immune system. Misleading debug information.

**Fix**: Change to `NIMCP_ERROR_INVALID_PARAM` with message like `"left_resource_fraction out of range [0.0, 1.0]"`.

---

### P1-8: `nimcp_hemispheric_brain.c` duplicate NIMCP_THROW_TO_IMMUNE after free

**File**: `/home/bbrelin/nimcp/src/core/brain/hemispheric/nimcp_hemispheric_brain.c`
**Lines**: 233-240

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
    "hemispheric_brain_create: failed to allocate mutex");
callosum_destroy(brain->callosum);
hemisphere_destroy(brain->left);
hemisphere_destroy(brain->right);
nimcp_free(brain);
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hemispheric_brain_create: brain->mutex is NULL");
return NULL;
```

**Bug**: Two NIMCP_THROW_TO_IMMUNE calls for the same error. The first throw reports before cleanup, then cleanup runs, then a second throw fires. The immune system sees two separate error events for a single failure.

**Impact**: Duplicate immune system triggers, confusing error diagnostics.

**Fix**: Remove the first `NIMCP_THROW_TO_IMMUNE` call. Keep only the one after cleanup.

---

### P1-9: `nimcp_brain_lifecycle.c:brain_destroy()` unconditionally unregisters global bio-async

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain_lifecycle.c`
**Lines**: 1011-1018

```c
extern bool g_brain_bio_initialized;
extern bio_module_context_t g_brain_bio_ctx;
if (g_brain_bio_initialized && g_brain_bio_ctx) {
    bio_router_unregister_module(g_brain_bio_ctx);
    g_brain_bio_ctx = NULL;
    g_brain_bio_initialized = false;
}
```

**Bug**: This unregisters the global bio-async module context when ANY brain is destroyed, even if other brains still exist. The main `nimcp_brain.c` file has a reference counter (`g_brain_bio_ref_count`) specifically to prevent this, but `nimcp_brain_lifecycle.c` bypasses it.

**Impact**: If multiple brains exist and one is destroyed, all other brains lose bio-async communication. Use-after-free if other brains try to use `g_brain_bio_ctx` after it's set to NULL.

**Fix**: Check `g_brain_bio_ref_count` before unregistering:
```c
int ref = __atomic_sub_fetch(&g_brain_bio_ref_count, 1, __ATOMIC_SEQ_CST);
if (ref <= 0 && g_brain_bio_initialized && g_brain_bio_ctx) {
    bio_router_unregister_module(g_brain_bio_ctx);
    g_brain_bio_ctx = NULL;
    g_brain_bio_initialized = false;
}
```

---

### P1-10: `nimcp_bg_vigor.c` false-positive NIMCP_THROW_TO_IMMUNE on action not found

**File**: `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_vigor.c`
**Line**: 311

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "bg_vigor: error condition");
return NIMCP_ERROR_NOT_FOUND;
```

**Bug**: This fires in `bgv_remove_action()` when the action ID is not found. Not finding an action to remove is a lookup failure, not a system error warranting immune system involvement.

**Impact**: False immune trigger on every failed action removal.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call. The return code `NIMCP_ERROR_NOT_FOUND` already communicates the failure.

---

### P1-11: `nimcp_genius_profiles.c` NIMCP_THROW_TO_IMMUNE on lookup miss

**File**: `/home/bbrelin/nimcp/src/core/brain/genius/nimcp_genius_profiles.c`
**Lines**: 3045, 3210

```c
NIMCP_THROW_TO_IMMUNE(GENIUS_ERROR_INVALID_TYPE, "Profile not found for genius type");
// ...
NIMCP_THROW_TO_IMMUNE(GENIUS_ERROR_INVALID_TYPE, "Profile not found for hemispheric brain");
```

**Bug**: Looking up a genius profile and not finding it is a lookup failure, not a system error. Using `GENIUS_ERROR_INVALID_TYPE` as error code passed to `NIMCP_THROW_TO_IMMUNE` may also be out of the expected NIMCP error code range.

**Impact**: False immune trigger on profile lookup miss. Possibly invalid error code.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` calls or gate them with a check for truly invalid inputs.

---

## P2 Findings (Logic/Correctness)

### P2-1: `nimcp_brain.c:init_brain_stats()` potential integer overflow in synapse count

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`
**Lines**: 917-918 (also in `nimcp_brain_lifecycle.c:327-328`)

```c
stats->num_synapses = num_neurons * num_inputs;
stats->num_active_synapses = stats->num_synapses;
```

**Bug**: `num_neurons` (up to 5000 for LARGE) and `num_inputs` (up to 10000) could multiply to 50,000,000 which fits in uint32_t, but for CUSTOM sizes with large values, `num_neurons * num_inputs` could overflow uint32_t (max ~4.29 billion).

**Impact**: Incorrect synapse statistics, potentially zero if overflow wraps around.

**Fix**: Use `uint64_t` for `num_synapses` or add overflow check.

---

### P2-2: `nimcp_brain.c` and `nimcp_brain_lifecycle.c` duplicate function definitions

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c` and `/home/bbrelin/nimcp/src/core/brain/nimcp_brain_lifecycle.c`

**Bug**: Both files define `get_neuron_count()`, `get_default_sparsity()`, `build_spike_params()`, `build_base_network_config()`, `build_network_config()`, `init_brain_config()`, `init_brain_stats()`, `validate_creation_params()`, `allocate_brain()`, `create_brain_network()`, `init_output_labels()`, `init_attention_subsystem()`, `init_brain_regions_subsystem()`, `init_symbolic_logic_subsystem()`, `init_symbolic_reasoning_subsystem()`, `init_epistemic_subsystem()` as non-static functions.

This appears to be a migration artifact where functions were extracted to `nimcp_brain_lifecycle.c` but the originals in `nimcp_brain.c` were not removed.

**Impact**: ODR (One Definition Rule) violation. Multiple definitions of the same functions across compilation units. The linker may pick either version, leading to subtle behavior differences. The `nimcp_brain_lifecycle.c` version has slightly different implementations (e.g., `build_base_network_config` calculates `num_neurons` differently).

**Fix**: Remove the duplicates from one file. The lifecycle file should be the canonical location since it was extracted there.

---

### P2-3: `nimcp_brain_lifecycle.c:build_base_network_config()` different num_neurons calculation

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain_lifecycle.c`
**Line**: 203

```c
config.num_neurons = num_inputs + num_neurons + num_outputs;
```

vs `nimcp_brain.c` line 742:
```c
config.num_neurons = num_neurons;
```

**Bug**: The lifecycle version adds inputs+hidden+outputs while the main brain.c version just uses the hidden neuron count directly. These define the SAME function (`build_base_network_config`) with different semantics.

**Impact**: Depending on which definition the linker picks, brain networks will have very different sizes.

**Fix**: Resolve the duplicate definitions and settle on one correct implementation.

---

### P2-4: `nimcp_brain.c:build_base_network_config()` missing weight bounds

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`
**Lines**: 739-771

The version in `nimcp_brain.c` does NOT set `config.min_weight` and `config.max_weight`, while `nimcp_brain_lifecycle.c` line 227-228 sets them:
```c
config.min_weight = -1.0F;
config.max_weight = 1.0F;
```

**Bug**: Without weight bounds, weights get clamped to the default range (possibly [0,0]), making all weights zero and the network unable to learn. This was documented as a BUGFIX in the lifecycle version.

**Impact**: If the linker picks the brain.c version, networks cannot learn.

**Fix**: Add weight bounds to the brain.c version, or better yet, resolve the duplicate.

---

### P2-5: `nimcp_hypothalamus_homeostasis.c` continues without mutex on creation failure

**File**: `/home/bbrelin/nimcp/src/core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.c`
**Lines**: 342-345

```c
system->mutex = nimcp_mutex_create(&mutex_attr);
if (system->mutex) {
    system->mutex_owned = true;
}
```

**Bug**: If mutex creation fails (`nimcp_mutex_create` returns NULL), the function continues without a mutex. All subsequent operations that lock this mutex will crash or cause undefined behavior.

**Impact**: NULL pointer dereference in any thread-safe operation on the homeostasis system.

**Fix**: Return NULL on mutex creation failure:
```c
if (!system->mutex) {
    nimcp_free(system);
    return NULL;
}
system->mutex_owned = true;
```

---

### P2-6: `nimcp_brain_lifecycle.c:brain_destroy()` missing longterm_memory cleanup

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain_lifecycle.c`
**Lines**: 740-1026

**Bug**: The `brain_destroy()` in lifecycle.c does not free `brain->longterm_memory` or its internal features arrays. The version in `nimcp_brain.c` (lines 2282-2287) does:
```c
for (uint32_t i = 0; i < brain->longterm_count; i++) {
    if (brain->longterm_memory[i].features) {
        nimcp_free(brain->longterm_memory[i].features);
    }
}
nimcp_free(brain->longterm_memory);
```

**Impact**: Memory leak of all long-term memory entries when brain is destroyed through the lifecycle path.

**Fix**: Add longterm_memory cleanup to `brain_destroy()` in nimcp_brain_lifecycle.c.

---

### P2-7: `nimcp_brain.c:brain_decide()` cache hit returns shallow CoW copy

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`
**Line**: 3513

```c
brain_decision_t* cached_copy = copy_decision(brain->cached_decision);
```

**Bug**: On a cache hit, `brain_decide()` uses `copy_decision()` (shallow CoW) to return a copy. While the cache itself uses `copy_decision_deep()` to store (which is correct), returning a shallow copy to the caller creates a shared reference. If the cache is later cleared (clearing the shared data), the returned copy becomes a dangling pointer.

The original `nimcp_brain.c` cache_decision() function at line 999 correctly uses `copy_decision_deep()`, but the retrieval path at line 3513 uses shallow copy.

**Impact**: Potential use-after-free if caller holds the returned decision while the cache is cleared.

**Fix**: Change to `copy_decision_deep(brain->cached_decision)`.

---

### P2-8: `nimcp_distributed_cow.c:serialize_network_segment()` unaligned memory access

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_distributed_cow.c`
**Lines**: 288-317

```c
*(uint32_t*)(temp_buffer + offset) = start_neuron;
// ...
*(float*)(temp_buffer + offset) = neuron_state;
```

**Bug**: These casts create potentially unaligned pointers. On architectures that require alignment (ARM, MIPS, etc.), this causes undefined behavior or a bus error. Even on x86, unaligned access can cause performance penalties.

**Impact**: Crash on non-x86 platforms. Performance penalty on x86.

**Fix**: Use `memcpy()` instead of pointer casts for serialization:
```c
memcpy(temp_buffer + offset, &start_neuron, sizeof(uint32_t));
```

---

### P2-9: `nimcp_distributed_cow.c:deserialize_network_segment()` unaligned reads

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_distributed_cow.c`
**Lines**: 435-472

Same pattern as P2-8 but for deserialization reads:
```c
uint32_t start_neuron = *(const uint32_t*)(data + offset);
float activation = *(const float*)(data + offset);
```

**Impact**: Same as P2-8.

**Fix**: Use `memcpy()` for reading from wire format.

---

### P2-10: `nimcp_distributed_cow.c:get_base_network()` relies on fragile struct layout assumption

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_distributed_cow.c`
**Lines**: 199-212

```c
static neural_network_t get_base_network(adaptive_network_t network) {
    // ...
    return *((neural_network_t*)network);
}
```

**Bug**: This casts the opaque `adaptive_network_t` pointer to `neural_network_t*` assuming `base_network` is the first field. Any reordering of the adaptive_network_struct will silently break this. The code even acknowledges this fragility in its comment.

**Impact**: Incorrect pointer interpretation if struct layout changes, leading to memory corruption.

**Fix**: Add a proper public accessor function to the adaptive network API (e.g., `adaptive_network_get_base_network()`).

---

### P2-11: Multiple subcortical modules continue without mutex on creation failure

**Files**:
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_beta_oscillations.c:243`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_sequence_chunking.c:132`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_nucleus_accumbens.c:99`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_hierarchical_rl.c:187`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_superior_colliculus.c:187`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_striatal_interneurons.c:187`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_cerebellar_coord.c:199`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_model_based.c:220`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_neuromodulators.c:114`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_basal_ganglia_enhanced.c:214`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_temporal_credit.c:190`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_striosome_matrix.c:156`
- `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_bg_outcome_devaluation.c:164`

**Pattern**:
```c
system->mutex = nimcp_mutex_create(NULL);
// No NULL check -- if mutex creation fails, later operations will deref NULL
```

**Bug**: `nimcp_mutex_create()` returns `nimcp_mutex_t*` (pointer). If it returns NULL, subsequent `nimcp_mutex_lock()` calls will crash. Most of these files do NOT check the return value.

**Impact**: NULL pointer dereference on any subsequent lock operation if mutex creation fails.

**Fix**: Each instance needs a NULL check and cleanup:
```c
system->mutex = nimcp_mutex_create(NULL);
if (!system->mutex) {
    // cleanup and return NULL
}
```

---

### P2-12: Several brain region modules missing mutex NULL check after creation

**Files** (same pattern as P2-11):
- `/home/bbrelin/nimcp/src/core/brain/regions/ofc/nimcp_ofc.c:166`
- `/home/bbrelin/nimcp/src/core/brain/regions/red_nucleus/nimcp_red_nucleus.c:253`
- `/home/bbrelin/nimcp/src/core/brain/regions/reticular/nimcp_reticular.c:308`
- `/home/bbrelin/nimcp/src/core/brain/regions/pag/nimcp_pag.c:384`
- `/home/bbrelin/nimcp/src/core/brain/regions/motor/nimcp_motor_adapter.c:515`

**Impact**: Same as P2-11 -- NULL pointer dereference.

**Fix**: Add NULL check and error return after each `nimcp_mutex_create()` call.

---

### P2-13: `nimcp_brain.c` LOG_MODULE defined twice

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`
**Lines**: 33, 61

```c
#define LOG_MODULE "BRAIN"
// ... 28 lines later ...
#define LOG_MODULE "BRAIN"
```

**Bug**: Duplicate `#define` -- the preprocessor will issue a warning. While harmless since both define the same value, this is a maintenance risk.

**Impact**: Compiler warning. Risk of divergence if only one is updated.

**Fix**: Remove one of the definitions.

---

### P2-14: `nimcp_neuralnet.c` LOG_MODULE defined twice

**File**: `/home/bbrelin/nimcp/src/core/neuralnet/nimcp_neuralnet.c`
**Lines**: 71, 75

```c
#define LOG_MODULE "neuralnet"
// ...
#define LOG_MODULE "neuralnet"
```

**Impact**: Same as P2-13.

**Fix**: Remove one definition.

---

### P2-15: `nimcp_brain_lifecycle.c` LOG_MODULE defined twice

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain_lifecycle.c`
**Lines**: 34, 75

```c
#define LOG_MODULE "BRAIN_LIFECYCLE"
// ...
#define LOG_MODULE "BRAIN_LIFECYCLE"
```

**Impact**: Same as P2-13.

**Fix**: Remove one definition.

---

### P2-16: `nimcp_brain_lifecycle.c:create_personality()` wrong error code on NULL config

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain_lifecycle.c`
**Line**: 673

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_personality: config is NULL");
```

**Bug**: Uses `NIMCP_ERROR_NO_MEMORY` for a NULL pointer check on config. Should be `NIMCP_ERROR_NULL_POINTER`.

**Impact**: Misleading error classification (reports memory exhaustion when it's actually a null pointer).

**Fix**: Change to `NIMCP_ERROR_NULL_POINTER`.

---

### P2-17: `nimcp_hemispheric_brain.c:hemispheric_brain_update()` fires NIMCP_THROW_TO_IMMUNE on inactive brain

**File**: `/home/bbrelin/nimcp/src/core/brain/hemispheric/nimcp_hemispheric_brain.c`
**Lines**: 298-300

```c
int hemispheric_brain_update(hemispheric_brain_t* brain, float dt) {
    if (!brain || !brain->is_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...);
```

**Bug**: An inactive brain is a valid state (after injury simulation, sleep mode, etc.), not a NULL pointer error. The `!brain->is_active` check is a normal control flow path. Using `NIMCP_ERROR_NULL_POINTER` for an inactive state is semantically wrong.

**Impact**: False immune trigger when updating an inactive brain. Wrong error code.

**Fix**: Split the checks:
```c
if (!brain) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...);
    return -1;
}
if (!brain->is_active) {
    return 0;  // Not an error, just skip update
}
```

---

## P3 Findings (Style/Robustness)

### P3-1: Boilerplate mesh registration code duplicated in every file

**Files**: ALL files in `src/core/` (370+ files)

Every single `.c` file in the codebase contains a nearly identical 20-line mesh participant registration block:
```c
static mesh_participant_id_t g_XXX_mesh_id = 0;
static mesh_participant_registry_t* g_XXX_mesh_registry = NULL;
nimcp_error_t XXX_mesh_register(...) { ... }
void XXX_mesh_unregister(void) { ... }
```

**Impact**: Massive code duplication. Each change to the registration pattern requires updating 370+ files.

**Fix**: Create a macro like `NIMCP_DEFINE_MESH_PARTICIPANT(name, category)` that expands to the entire boilerplate.

---

### P3-2: `nimcp_brain.c` and `nimcp_brain_lifecycle.c` both define `strategy_destroy()` as static

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c:544`

The function is defined as static in `nimcp_brain.c` but `nimcp_brain_lifecycle.c` calls `strategy_destroy(brain->strategy)` at line 789 -- this works only because `nimcp_brain_lifecycle.c` has a separate extern declaration or because both compile into the same translation unit.

**Impact**: Fragile linkage dependency.

**Fix**: Make `strategy_destroy()` non-static and provide a proper header declaration.

---

### P3-3: `nimcp_distributed_cow.c` magic numbers

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_distributed_cow.c`

Multiple magic numbers:
- Line 737: `16` (initial registry capacity)
- Line 277: `1024 + num_neurons * (32 + 50 * 8)` (buffer size estimate)
- Line 396: `data_size * 10` (decompression estimate)
- Line 956: `65536` (response buffer size)

**Impact**: Unclear meaning, maintenance risk.

**Fix**: Define named constants:
```c
#define DCOW_INITIAL_REGISTRY_CAPACITY 16
#define DCOW_RESPONSE_BUFFER_SIZE 65536
```

---

### P3-4: `nimcp_brain.c:strategy_association_transform()` crashes on all-zero output

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`
**Lines**: 446-460

```c
static void strategy_association_transform(float* output, uint32_t size)
{
    float max_val = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        if (fabsf(output[i]) > max_val)
            max_val = fabsf(output[i]);
    }
    if (max_val > 0.0F) {
        for (uint32_t i = 0; i < size; i++) {
            output[i] /= max_val;
        }
    }
}
```

**Bug**: Missing NULL check on `output` parameter (unlike `strategy_classification_transform` which has it).

**Impact**: NULL pointer dereference if called with NULL output.

**Fix**: Add `if (!output || size == 0) return;` guard.

---

### P3-5: `nimcp_brain.c:strategy_pattern_transform()` missing NULL check

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`
**Lines**: 412-418

```c
static void strategy_pattern_transform(float* output, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        output[i] = output[i] > 0.5F ? 1.0F : 0.0F;
    }
}
```

**Bug**: No NULL check on `output`. Other transform functions have it.

**Fix**: Add `if (!output) return;`.

---

### P3-6: `nimcp_brain.c:strategy_classification_loss()` and similar -- missing NULL checks on `pred`/`target`

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`
**Lines**: 355-365, 386-398, 420-432, 462-473

None of the loss functions check for NULL `pred` or `target` parameters.

**Impact**: NULL pointer dereference if called with NULL arrays.

**Fix**: Add NULL guards to each loss function.

---

### P3-7: `nimcp_event_bus.c` uses `nimcp_mutex_t` as value type, not pointer

**File**: `/home/bbrelin/nimcp/src/core/events/nimcp_event_bus.c`
**Lines**: 89, 103, 129

```c
nimcp_mutex_t mutex;          // In event_queue_t
nimcp_mutex_t subscriber_mutex;  // In event_bus_internal_t
nimcp_mutex_t stats_mutex;
```

**Bug**: The event bus uses `nimcp_mutex_t` as a value-type member (not pointer), while most other modules use `nimcp_mutex_t*` via `nimcp_mutex_create()`. This inconsistency means different initialization and destruction patterns are needed. If `nimcp_mutex_t` is a pointer type (typedef'd), this would be correct. But if it's a struct, the semantics differ from the rest of the codebase.

**Impact**: Potential initialization mismatch. Less of a bug, more of a consistency concern.

**Fix**: Verify `nimcp_mutex_t` definition and ensure consistent usage pattern.

---

### P3-8: `nimcp_thalamus.c:thal_nucleus_create()` uses malloc+memset instead of calloc

**File**: `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_thalamus.c`
**Lines**: 172-178

```c
thal_nucleus_t* nucleus = nimcp_malloc(sizeof(thal_nucleus_t));
if (!nucleus) { ... }
memset(nucleus, 0, sizeof(thal_nucleus_t));
```

**Impact**: Stylistic. `nimcp_calloc(1, sizeof(thal_nucleus_t))` would be cleaner and less error-prone.

**Fix**: Use `nimcp_calloc()`.

---

### P3-9: `nimcp_basal_ganglia.c:find_winning_action()` no NULL check on bg->thalamic_output

**File**: `/home/bbrelin/nimcp/src/core/brain/subcortical/nimcp_basal_ganglia.c`
**Lines**: 194-200

```c
static uint32_t find_winning_action(const basal_ganglia_t* bg) {
    uint32_t winner = 0;
    float max_val = bg->thalamic_output[0];
```

**Bug**: No check that `bg->thalamic_output` is non-NULL or that `bg->num_actions > 0`.

**Impact**: NULL dereference or out-of-bounds read if thalamic_output is uninitialized or num_actions is 0.

**Fix**: Add `if (!bg->thalamic_output || bg->num_actions == 0) return 0;`

---

### P3-10: `nimcp_brain.c:force_unlock_with_logging()` -- emergency unlock is dangerous

**File**: `/home/bbrelin/nimcp/src/core/brain/nimcp_brain.c`
**Lines**: 1088-1112

**Concern**: The function attempts multiple unlocks of a mutex, which is undefined behavior for non-recursive mutexes. If the mutex is not held by the current thread, unlocking it is UB.

**Impact**: Undefined behavior. Could corrupt mutex state or deadlock other threads.

**Fix**: This recovery strategy should be documented as last-resort only and should verify mutex ownership before attempting recovery.

---

---

## Additional Findings -- Expanded Review (cortical_columns, directives, medulla, topology, axon, dendrite, synapse_types, brain_oscillations, logic, neural_substrate, brain_regions)

---

## P1 Findings (Critical/Crash) -- Continued

### P1-12: `nimcp_community_detection.c:build_adjacency_list()` realloc failure leaks entire graph

**File**: `/home/bbrelin/nimcp/src/core/topology/nimcp_community_detection.c`
**Lines**: 179-188

```c
for (uint32_t i = 0; i < num_nodes; i++) {
    // ... build adjacency for node i ...
    float* new_weights = nimcp_realloc(adj->weights[i], new_cap * sizeof(float));
    if (!new_weights) {
        return NULL;  // Leaks all adj->neighbors[0..i], adj->weights[0..i]
    }
}
```

**Bug**: When `nimcp_realloc()` fails mid-loop, the function returns NULL without freeing the already-built adjacency nodes for neurons 0..i. The entire partially-constructed graph is leaked.

**Impact**: Memory leak proportional to graph size on allocation failure.

**Fix**: Add cleanup loop before returning NULL that frees `adj->neighbors[j]` and `adj->weights[j]` for `j = 0..i`, then free the adj structure itself.

---

### P1-13: `nimcp_axon.c:axon_network_find()` false-positive NIMCP_THROW_TO_IMMUNE on lookup miss

**File**: `/home/bbrelin/nimcp/src/core/axon/nimcp_axon.c`
**Line**: 1280

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon_network_find: validation failed");
return NULL;
```

**Bug**: Not finding an axon by ID in the network is normal search behavior, not an error. This triggers the immune system on every failed lookup.

**Impact**: False immune system triggers during normal axon network queries.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call.

---

### P1-14: `nimcp_axon.c:axon_cow_release()` race condition -- reads shared state after releasing mutex

**File**: `/home/bbrelin/nimcp/src/core/axon/nimcp_axon.c`
**Lines**: 1861-1891

```c
nimcp_mutex_lock(&axon->mutex);
axon->cow_ref_count--;
bool should_free = (axon->cow_ref_count == 0);
nimcp_mutex_unlock(&axon->mutex);  // line 1877

// After unlock, another thread could modify cow_ref_count
if (should_free && axon->cow_original) {  // line 1880 - TOCTOU
    // ... free original ...
}
```

**Bug**: The code checks `cow_ref_count` under lock, stores the result in `should_free`, then acts on it after releasing the lock. However, between the unlock and the `if` check, another thread could decrement `cow_ref_count` again or modify `cow_original`, creating a time-of-check-to-time-of-use (TOCTOU) race.

**Impact**: Double-free of the original axon segments, or use-after-free if another thread frees first.

**Fix**: Perform the entire check-and-free sequence while holding the mutex, or use a pattern where the last releaser does the cleanup under lock.

---

### P1-15: `nimcp_brain_regions.c:brain_module_get_region()` false-positive NIMCP_THROW_TO_IMMUNE on lookup miss

**File**: `/home/bbrelin/nimcp/src/core/brain_regions/nimcp_brain_regions.c`
**Lines**: 306-313

```c
for (uint32_t i = 0; i < brain->num_regions; i++) {
    if (brain->regions[i]->id == region_id) {
        return brain->regions[i];
    }
}
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_module_get_region: validation failed");
return NULL;
```

**Bug**: Not finding a region by ID is a normal lookup miss (the caller `brain_module_connect_regions()` checks for NULL return). This also uses the wrong error code `NIMCP_ERROR_NULL_POINTER` for a "not found" condition.

**Impact**: False immune trigger on every region lookup miss. Wrong error code.

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` or replace with a LOG_DEBUG.

---

### P1-16: `nimcp_brain_regions.c:brain_module_get_region_by_type()` same false-positive pattern

**File**: `/home/bbrelin/nimcp/src/core/brain_regions/nimcp_brain_regions.c`
**Lines**: 326-333

Same pattern as P1-15 but for type-based lookup. Uses `NIMCP_ERROR_NULL_POINTER` for "not found".

**Fix**: Remove the `NIMCP_THROW_TO_IMMUNE` call.

---

## P2 Findings (Logic/Correctness) -- Continued

### P2-18: `nimcp_cortical_column.c:hypercolumn_run_competition()` fragile trylock pattern

**File**: `/home/bbrelin/nimcp/src/core/cortical_columns/nimcp_cortical_column.c`
**Line**: 1186

```c
int lock_result = nimcp_platform_mutex_trylock(&hcol->mutex);
// if trylock fails, assumes "already locked by caller" and skips locking
```

**Bug**: `nimcp_platform_mutex_trylock()` returns non-zero on failure, but failure could mean either "already held by this thread" OR "contention from another thread". The code assumes the former, but if another thread holds the lock, the function proceeds WITHOUT any synchronization.

**Impact**: Data race if another thread is concurrently modifying the hypercolumn while this thread skips locking due to contention.

**Fix**: Use a recursive mutex, or restructure the calling code to pass a "lock_held" parameter.

---

### P2-19: `nimcp_cortical_column.c` wrong error code for validation failure

**File**: `/home/bbrelin/nimcp/src/core/cortical_columns/nimcp_cortical_column.c`
**Line**: 577

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "validate_hypercolumn_config: ...");
```

**Bug**: Uses `NIMCP_ERROR_BUFFER_OVERFLOW` for a feature_space range validation. Should be `NIMCP_ERROR_INVALID_PARAM`.

**Impact**: Misleading error classification.

**Fix**: Change to `NIMCP_ERROR_INVALID_PARAM`.

---

### P2-20: `nimcp_cortical_column.c` wrong error code for allocation failure

**File**: `/home/bbrelin/nimcp/src/core/cortical_columns/nimcp_cortical_column.c`
**Line**: 330

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...: activation_pool failed");
```

**Bug**: Uses `NIMCP_ERROR_NULL_POINTER` for what is an allocation failure. Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P2-21: `nimcp_cortical_hierarchy.c:cortical_hierarchy_create()` continues without mutex on init failure

**File**: `/home/bbrelin/nimcp/src/core/cortical_columns/nimcp_cortical_hierarchy.c`
**Lines**: 309-311

```c
if (nimcp_mutex_init(&hierarchy->mutex, NULL) != 0) {
    LOG_WARN(...);
    // continues without mutex!
}
```

**Bug**: If `nimcp_mutex_init()` fails, the function logs a warning but continues. All subsequent operations use `nimcp_mutex_lock(&hierarchy->mutex)` which would operate on an uninitialized mutex -- undefined behavior.

**Impact**: Undefined behavior on all subsequent mutex operations.

**Fix**: Return NULL and clean up on mutex init failure.

---

### P2-22: `nimcp_cortical_hierarchy.c:cortical_hierarchy_get_area_activity()` missing mutex for read

**File**: `/home/bbrelin/nimcp/src/core/cortical_columns/nimcp_cortical_hierarchy.c`
**Line**: ~1083

**Bug**: This getter reads area activity data without acquiring the mutex, while concurrent propagation operations modify the same data under lock.

**Impact**: Data race -- torn reads of activity values during concurrent propagation.

**Fix**: Acquire the hierarchy mutex for the duration of the read.

---

### P2-23: `nimcp_core_directives.c:core_directives_evaluate()` action_context memory leak on goto

**File**: `/home/bbrelin/nimcp/src/core/directives/nimcp_core_directives.c`
**Lines**: 357-434

```c
action_context_t* ctx = action_to_context(action);  // allocates affected_agents
// ...
if (blocked) {
    goto evaluation_complete;  // Leaks ctx
}
// ... only on non-blocking path:
free_action_context(ctx);
```

**Bug**: `action_to_context()` allocates an `action_context_t` with internal `affected_agents` array. When Step 1 (harm prevention) or Step 3 (self-preservation) blocks the action via `goto evaluation_complete`, the allocated context is never freed.

**Impact**: Memory leak on every blocked action evaluation.

**Fix**: Add `free_action_context(ctx)` before the `goto evaluation_complete` jump targets, or restructure to use a single cleanup path.

---

### P2-24: `nimcp_harm_prevention.c` no thread safety despite claiming thread-safe operation

**File**: `/home/bbrelin/nimcp/src/core/directives/nimcp_harm_prevention.c`
**Line**: 228

```c
// Mutex intentionally left as NULL (void*)
```

**Bug**: The harm prevention system has a mutex field but intentionally leaves it NULL. All mutation of stats (counters, totals) is unprotected. The module header comments suggest thread-safe operation.

**Impact**: Data races on statistics counters with concurrent harm evaluations.

**Fix**: Initialize the mutex or document that the module is single-threaded only.

---

### P2-25: `nimcp_medulla.c:medulla_update()` NIMCP_THROW_TO_IMMUNE for normal state check

**File**: `/home/bbrelin/nimcp/src/core/medulla/nimcp_medulla.c`
**Line**: 821

```c
if (medulla->state != MEDULLA_STATE_RUNNING) {
    NIMCP_THROW_TO_IMMUNE(..., "medulla not running");
    return -1;
}
```

**Bug**: Not being in RUNNING state is a normal condition (e.g., during initialization, shutdown, or pause). This fires the immune system on every update call when the medulla is not running.

**Impact**: False immune trigger during normal state transitions.

**Fix**: Remove NIMCP_THROW_TO_IMMUNE; just return -1 or 0 (not an error).

---

### P2-26: `nimcp_community_detection.c:louvain_phase1()` O(n^2) temporary allocations

**File**: `/home/bbrelin/nimcp/src/core/topology/nimcp_community_detection.c`

**Bug**: In the Louvain phase 1 loop, `comm_weight` array of size `num_nodes` is allocated on every iteration of the outer node loop, creating O(n^2) total malloc/free calls.

**Impact**: Severe performance degradation for large graphs. Each allocation is `num_nodes * sizeof(float)`.

**Fix**: Allocate `comm_weight` once before the loop and reuse it, zeroing with `memset` per iteration.

---

### P2-27: `nimcp_axon.c:axon_spike_queue_pop()` false-positive NIMCP_THROW_TO_IMMUNE on empty/not-ready

**File**: `/home/bbrelin/nimcp/src/core/axon/nimcp_axon.c`
**Lines**: 1052, 1062

```c
// Line 1052: Queue empty
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_..., "queue empty");
// Line 1062: No spike ready yet
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_..., "no spike ready");
```

**Bug**: Empty queue and "no spike ready" are both normal conditions during simulation. The queue is polled regularly and these conditions occur on most poll cycles.

**Impact**: Massive immune system spam during normal simulation -- fires on every poll when no spikes are pending.

**Fix**: Remove both NIMCP_THROW_TO_IMMUNE calls. These are normal return paths.

---

### P2-28: `nimcp_axon.c:axon_initiate_spike()` wrong error code

**File**: `/home/bbrelin/nimcp/src/core/axon/nimcp_axon.c`
**Line**: 576

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "axon not functional");
```

**Bug**: Uses `NIMCP_ERROR_NULL_POINTER` for a non-functional axon state. Should be `NIMCP_ERROR_INVALID_STATE`.

---

### P2-29: `nimcp_axon.c:axon_spike_arrived()` false-positive NIMCP_THROW_TO_IMMUNE on non-active state

**File**: `/home/bbrelin/nimcp/src/core/axon/nimcp_axon.c`
**Line**: 633

**Bug**: Firing NIMCP_THROW_TO_IMMUNE when a spike arrives at a non-active axon. The axon may have been deactivated between spike initiation and arrival -- this is a normal timing condition.

**Fix**: Remove the throw; return silently or with a debug log.

---

### P2-30: `nimcp_dendrite.c:dendrite_initiate_nmda_spike()` wrong error messages

**File**: `/home/bbrelin/nimcp/src/core/dendrite/nimcp_dendrite.c`
**Lines**: 1296, 1302, 1308, 1320, 1327

Multiple NIMCP_THROW_TO_IMMUNE calls use the wrong function name `"calculate_mg_block"` instead of `"dendrite_initiate_nmda_spike"`. Also, line 1327 uses `NIMCP_ERROR_NULL_POINTER` with message `"segment->has_active_properties is NULL"` but `has_active_properties` is a boolean, not a pointer -- the error code and message are both wrong.

**Impact**: Misleading error diagnostics. The immune system reports errors from the wrong function.

**Fix**: Correct function names in error messages. Change boolean checks to use `NIMCP_ERROR_INVALID_STATE`.

---

### P2-31: `nimcp_dendrite.c:dendrite_can_generate_nmda_spike()` NIMCP_THROW_TO_IMMUNE on threshold/readiness checks

**File**: `/home/bbrelin/nimcp/src/core/dendrite/nimcp_dendrite.c`
**Lines**: 1415, 1421, 1427, 1434

**Bug**: This function is a query ("can this generate a spike?"). Returning false for "below threshold", "no active properties", "already active", or "Mg block too strong" are all normal answers to the query. NIMCP_THROW_TO_IMMUNE should not fire for a "no" answer to a readiness check.

**Impact**: Immune system triggered every time a segment cannot generate an NMDA spike (which is the common case).

**Fix**: Remove all NIMCP_THROW_TO_IMMUNE calls. Return false with optional LOG_DEBUG.

---

### P2-32: `nimcp_dendrite.c:dendrite_spine_process_input()` recursive deadlock potential

**File**: `/home/bbrelin/nimcp/src/core/dendrite/nimcp_dendrite.c`
**Lines**: 2179, 2216

```c
nimcp_mutex_lock(&dendrite->lock);       // line 2179
// ...
dendrite_stdp_pre_spike(dendrite, ...);  // line 2216 -- also locks dendrite->lock!
// ...
nimcp_mutex_unlock(&dendrite->lock);
```

**Bug**: `dendrite_spine_process_input()` acquires `dendrite->lock` at line 2179, then calls `dendrite_stdp_pre_spike()` at line 2216 which also tries to acquire `dendrite->lock` at line 1782. If the mutex is not recursive, this is a deadlock.

**Impact**: Deadlock on every spine input processing call.

**Fix**: Either make the mutex recursive, or create an internal `_unlocked` variant of `dendrite_stdp_pre_spike()`.

---

### P2-33: `nimcp_neural_substrate.c:substrate_update()` static variable is not thread-safe

**File**: `/home/bbrelin/nimcp/src/core/neural_substrate/nimcp_neural_substrate.c`
**Line**: 440

```c
static float s_prev_imagination_capacity = 1.0f;
```

**Bug**: This function-local static variable is shared across all `substrate_update()` calls, but the function can be called from different threads with different substrate instances. Multiple threads updating different substrates will race on this shared state.

**Impact**: Data race on `s_prev_imagination_capacity`. False/missed imagination capacity notifications.

**Fix**: Move this tracking variable into the `neural_substrate_t` struct as a per-instance field.

---

### P2-34: `nimcp_neural_logic_factory.c` wrong error codes

**File**: `/home/bbrelin/nimcp/src/core/logic/nimcp_neural_logic_factory.c`
**Lines**: 208, 215, 250, 287

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "...: nimcp_validate_pointer is NULL");
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "...: validate_config is NULL");
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "...: nimcp_validate_pointer is NULL");
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...: success is NULL");
```

**Bug**: Multiple wrong error codes: `NIMCP_ERROR_NO_MEMORY` used for NULL pointer checks and validation failures. The error messages also incorrectly say things like "nimcp_validate_pointer is NULL" when the actual issue is that the config pointer failed validation.

**Impact**: Misleading error diagnostics.

**Fix**: Use `NIMCP_ERROR_NULL_POINTER` for NULL checks, `NIMCP_ERROR_INVALID_PARAM` for validation failures, and `NIMCP_ERROR_OPERATION_FAILED` for attachment failures. Fix messages to describe the actual condition.

---

### P2-35: `nimcp_brain_oscillations.c:brain_oscillation_compute_coherence()` uses unprotected activity_buffer

**File**: `/home/bbrelin/nimcp/src/core/brain_oscillations/nimcp_brain_oscillations.c`
**Lines**: 1466-1467

```c
if (!fft_execute_real(analyzer->fft_plan, analyzer->activity_buffer,
                      analyzer->spectrum)) {
```

**Bug**: The `brain_oscillation_compute_coherence()` function reads directly from `analyzer->activity_buffer` without acquiring `buffer_mutex`, while `brain_oscillation_record_value()` writes to this buffer under lock. Other functions like `brain_oscillation_get_wave_power()` correctly copy the buffer under lock before processing.

**Impact**: Data race between recording and coherence computation. Corrupted FFT input.

**Fix**: Copy the activity buffer under lock before performing FFT, consistent with the pattern in `brain_oscillation_get_wave_power()`.

---

### P2-36: `nimcp_brain_regions.c:brain_module_create()` wrong error code

**File**: `/home/bbrelin/nimcp/src/core/brain_regions/nimcp_brain_regions.c`
**Line**: 226

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");
```

**Bug**: Uses `NIMCP_ERROR_NULL_POINTER` for an allocation failure (calloc returned NULL). Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P2-37: `nimcp_brain_regions.c:brain_region_create()` same wrong error code

**File**: `/home/bbrelin/nimcp/src/core/brain_regions/nimcp_brain_regions.c`
**Line**: 344

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "region is NULL");
```

**Bug**: Same as P2-36 -- allocation failure reported as NULL pointer error.

---

### P2-38: `nimcp_network_builder.c` exposes incomplete struct declaration of internal type

**File**: `/home/bbrelin/nimcp/src/core/topology/nimcp_network_builder.c`
**Lines**: 244-249

```c
struct neural_network_struct {
    neuron_t* neurons;
    uint32_t num_neurons;
    uint32_t capacity;
    // ... other fields not needed here
};
```

**Bug**: This file re-declares `struct neural_network_struct` with only the fields it needs, then casts `neural_network_t` to this partial struct. If the real struct has different field ordering, padding, or additional fields before `neurons`, the cast produces wrong results. This is essentially the same fragile struct layout issue as P2-10.

**Impact**: Memory corruption if the real struct layout differs from this assumption.

**Fix**: Add a proper `neural_network_get_neurons()` accessor to the neural network API.

---

## P3 Findings (Style/Robustness) -- Continued

### P3-11: `nimcp_cortical_column.c` wrong error code for validation failure

**File**: `/home/bbrelin/nimcp/src/core/cortical_columns/nimcp_cortical_column.c`
**Line**: 449

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "...: validation failed");
```

**Bug**: Uses `NIMCP_ERROR_NO_MEMORY` for a validation failure. Should be `NIMCP_ERROR_INVALID_PARAM`.

---

### P3-12: `nimcp_dendrite.c:dendrite_cow_release()` calls `dendrite_destroy()` which locks destroyed mutex

**File**: `/home/bbrelin/nimcp/src/core/dendrite/nimcp_dendrite.c`
**Lines**: 2149-2167

```c
void dendrite_cow_release(dendrite_t* dendrite) {
    nimcp_mutex_lock(&dendrite->lock);
    if (dendrite->cow_ref_count > 0) {
        dendrite->cow_ref_count--;
    }
    if (dendrite->cow_ref_count == 0) {
        nimcp_mutex_unlock(&dendrite->lock);
        dendrite_destroy(dendrite);  // dendrite_destroy calls nimcp_mutex_destroy
        return;
    }
    nimcp_mutex_unlock(&dendrite->lock);
}
```

**Bug**: On the last-reference path, the function unlocks the mutex then calls `dendrite_destroy()` which destroys the mutex. Between the unlock and destroy, another thread could acquire the lock and then find the dendrite freed from under it. However, since `cow_ref_count` is 0, no other thread should hold a reference. The larger concern is that the mutex is unlocked before destroy -- if there is a race, the mutex state is undefined.

**Impact**: Possible race between unlock and destroy in high-contention scenarios.

---

### P3-13: `nimcp_dendrite.c:dendrite_pool_alloc_spine()` wrong error code on pool exhaustion

**File**: `/home/bbrelin/nimcp/src/core/dendrite/nimcp_dendrite.c`
**Line**: 2000

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_pool_alloc_spine: operation failed");
```

**Bug**: Uses `NIMCP_ERROR_NULL_POINTER` when the pool is exhausted (all slots used). Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P3-14: `nimcp_synapse_types.c:synapse_type_name()` signed comparison warning

**File**: `/home/bbrelin/nimcp/src/core/synapse_types/nimcp_synapse_types.c`
**Line**: 662

```c
if (type < 0 || type >= SYNAPSE_TYPE_COUNT) {
```

**Bug**: If `synapse_type_t` is an unsigned enum (common in C), `type < 0` is always false and generates a compiler warning for signed/unsigned comparison.

**Impact**: Compiler warning; dead code.

**Fix**: Remove `type < 0` check if enum is unsigned.

---

### P3-15: `nimcp_brain_oscillations.c:brain_oscillation_detect_abnormality()` uses placeholder coherence/synchrony values

**File**: `/home/bbrelin/nimcp/src/core/brain_oscillations/nimcp_brain_oscillations.c`
**Lines**: 1894, 1901

```c
float coherence = 0.5F;  // Placeholder
float synchrony = 0.5F;  // Placeholder
```

**Bug**: The abnormality detection uses hardcoded placeholder values for coherence and synchrony instead of computing them from actual oscillation data. The `low_coherence` and `low_synchrony` flags will never be set with these values.

**Impact**: Incomplete abnormality detection -- coherence and synchrony checks are effectively disabled.

**Fix**: Call `brain_oscillation_compute_coherence()` and `brain_oscillation_compute_synchrony()` to get actual values.

---

### P3-16: `nimcp_dendrite.c:dendrite_initiate_bap()` wrong function name in error messages

**File**: `/home/bbrelin/nimcp/src/core/dendrite/nimcp_dendrite.c`
**Lines**: 1449, 1455

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendrite_can_generate_nmda_spike: dendrite is NULL");
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dendrite_can_generate_nmda_spike: validation failed");
```

**Bug**: Error messages reference `dendrite_can_generate_nmda_spike` but the function is `dendrite_initiate_bap`. Copy-paste error from NMDA spike functions.

**Impact**: Misleading error diagnostics.

---

### P3-17: `nimcp_dendrite.c:dendrite_create_spine_pool()` wrong error code for allocation failure

**File**: `/home/bbrelin/nimcp/src/core/dendrite/nimcp_dendrite.c`
**Line**: 1927

```c
NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "...: spine_pool->spines is NULL");
```

**Bug**: Uses `NIMCP_ERROR_NULL_POINTER` for a `nimcp_calloc` allocation failure. Should be `NIMCP_ERROR_NO_MEMORY`.

---

### P3-18: `nimcp_brain_regions.c:brain_region_organize_columns()` silently continues on column allocation failure

**File**: `/home/bbrelin/nimcp/src/core/brain_regions/nimcp_brain_regions.c`
**Line**: 496

```c
brain_minicolumn_t* col = (brain_minicolumn_t*)nimcp_calloc(1, sizeof(brain_minicolumn_t));
if (!col) continue;  // Silently skips this column
```

**Bug**: If `nimcp_calloc` fails for a minicolumn, the code silently skips it, leaving a NULL entry in the `minicolumns` array. Subsequent code that iterates `minicolumns[0..num_minicolumns-1]` may dereference these NULL entries.

**Impact**: NULL dereference in any code that iterates minicolumns without NULL checking each entry.

---

## Summary

| Priority | Count | Description |
|----------|-------|-------------|
| **P1** | **16** | Heap corruption via shallow copy_decision (P1-1), false-positive NIMCP_THROW_TO_IMMUNE on normal paths (P1-2,3,6,10,11,13,15,16), data race under read lock (P1-4), buffer overflow in registry (P1-5), wrong error code copy-paste (P1-7), duplicate immune throw (P1-8), global bio-async unregistration without refcount (P1-9), realloc failure leaks graph (P1-12), axon CoW release TOCTOU race (P1-14) |
| **P2** | **38** | Duplicate function definitions (P2-2,3,4), integer overflow risk (P2-1), missing mutex NULL checks in 13+ modules (P2-5,11,12), missing longterm_memory cleanup (P2-6), shallow copy on cache hit (P2-7), unaligned memory access (P2-8,9), fragile struct layout (P2-10,38), duplicate LOG_MODULE (P2-13,14,15), wrong error codes (P2-16,19,20,28,30,34,36,37), false immune on inactive brain (P2-17), fragile trylock pattern (P2-18), mutex init failure ignored (P2-21), missing read mutex (P2-22), action_context leak (P2-23), no thread safety in harm_prevention (P2-24), false immune on normal states (P2-25,27,29,31), O(n^2) allocations (P2-26), recursive deadlock (P2-32), thread-unsafe static variable (P2-33), unprotected buffer read (P2-35) |
| **P3** | **18** | Mesh boilerplate duplication (P3-1), fragile static linkage (P3-2), magic numbers (P3-3), missing NULL checks (P3-4,5,6), inconsistent mutex usage (P3-7), malloc+memset vs calloc (P3-8), missing bounds check (P3-9), dangerous emergency unlock (P3-10), wrong error codes (P3-11,13,17), CoW release race (P3-12), signed comparison (P3-14), placeholder detection values (P3-15), wrong function names in errors (P3-16), silent allocation failure (P3-18) |
| **Total** | **72** | |

### Key Themes

1. **Shallow vs Deep Copy in Cache**: The brain decision cache has been fixed in `nimcp_brain.c` to use `copy_decision_deep()`, but the extracted `nimcp_brain_validation.c` still uses the old shallow `copy_decision()`. This is the highest-priority fix.

2. **False-Positive NIMCP_THROW_TO_IMMUNE**: Pervasive across the codebase. Lookup misses in brain_regions, axon_network, distributed_cow, genius profiles, and community detection all fire immune system alerts for normal "not found" conditions. Query functions like `dendrite_can_generate_nmda_spike()` fire immune alerts for answering "no". Spike queue poll returning empty fires immune alerts on every cycle. Medulla "not running" state triggers immune alerts during normal state transitions.

3. **Duplicate Function Definitions**: The migration from `nimcp_brain.c` to `nimcp_brain_lifecycle.c` left duplicate definitions of 15+ functions with diverged semantics.

4. **Missing Mutex NULL Checks**: At least 18 modules in `subcortical/` and `regions/` create mutexes with `nimcp_mutex_create()` but do not check the return for NULL.

5. **Global Bio-Async Lifecycle**: The `brain_destroy()` in `nimcp_brain_lifecycle.c` bypasses the reference counter for global bio-async registration.

6. **Wrong Error Codes**: Many modules use `NIMCP_ERROR_NULL_POINTER` for allocation failures (should be `NO_MEMORY`), `NIMCP_ERROR_BUFFER_OVERFLOW` for validation failures (should be `INVALID_PARAM`), and copy-pasted error messages referencing wrong function names.

7. **Thread Safety Gaps**: The cortical hierarchy ignores mutex init failures, the harm prevention module has no thread safety despite claiming it, neural substrate uses a thread-unsafe static variable, brain oscillation coherence reads the activity buffer without locking, and dendrite spine processing has a recursive deadlock with its own STDP function.

8. **Fragile Struct Layout Assumptions**: Both `nimcp_distributed_cow.c` and `nimcp_network_builder.c` cast opaque pointers to partial struct declarations, which will silently break if the real struct layout changes.
