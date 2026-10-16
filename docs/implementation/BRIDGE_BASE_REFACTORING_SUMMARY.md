# Bridge Base Refactoring Summary for src/core/ Bridges

## Overview
This document summarizes the refactoring of 39 bridge files in `/home/bbrelin/nimcp/src/core/` to use the standardized `bridge_base_t` OO pattern.

## Files Refactored

### Brain/Hemispheric (5 files)
- `src/core/brain/hemispheric/nimcp_hemispheric_portia_bridge.c` (STARTED)
- `src/core/brain/hemispheric/nimcp_hemispheric_glial_bridge.c`
- `src/core/brain/hemispheric/nimcp_hemispheric_immune_bridge.c`
- `src/core/brain/hemispheric/nimcp_hemispheric_sleep_bridge.c`
- `src/core/brain/hemispheric/nimcp_hemispheric_fep_bridge.c`

### Brain/Subcortical (4 files)
- `src/core/brain/subcortical/nimcp_amygdala_attention_bridge.c`
- `src/core/brain/subcortical/nimcp_amygdala_autobio_bridge.c`
- `src/core/brain/subcortical/nimcp_amygdala_stress_bridge.c`
- `src/core/brain/subcortical/nimcp_amygdala_training_bridge.c`

### Brain/Oscillations (1 file)
- `src/core/brain/oscillations/nimcp_oscillations_fep_bridge.c`

### Brain/Regions/Broca (1 file)
- `src/core/brain/regions/broca/nimcp_language_production_bridge.c`

### Cortical Columns (3 files)
- `src/core/cortical_columns/nimcp_cortical_substrate_bridge.c`
- `src/core/cortical_columns/nimcp_cortical_plasticity_bridge.c`
- `src/core/cortical_columns/nimcp_cortical_column_fep_bridge.c`

### Cortical Columns/Sleep (8 files)
- `src/core/cortical_columns/sleep/nimcp_cortical_layers_sleep_bridge.c`
- `src/core/cortical_columns/sleep/nimcp_cortical_column_sleep_bridge.c`
- `src/core/cortical_columns/sleep/nimcp_cortical_hierarchy_sleep_bridge.c`
- `src/core/cortical_columns/sleep/nimcp_cortical_attention_gain_sleep_bridge.c`
- `src/core/cortical_columns/sleep/nimcp_cortical_predictive_coding_sleep_bridge.c`
- `src/core/cortical_columns/sleep/nimcp_cortical_temporal_sleep_bridge.c`
- `src/core/cortical_columns/sleep/nimcp_cortical_dendritic_sleep_bridge.c`
- `src/core/cortical_columns/sleep/nimcp_cortical_neuromodulation_sleep_bridge.c`

### Brain Regions (3 files)
- `src/core/brain_regions/nimcp_language_production_bridge.c`
- `src/core/brain_regions/nimcp_brain_regions_immune_bridge.c`
- `src/core/brain_regions/nimcp_predictive_regions_fep_bridge.c`

### Brain Oscillations (3 files)
- `src/core/brain_oscillations/nimcp_oscillations_sleep_bridge.c`
- `src/core/brain_oscillations/nimcp_oscillations_pink_noise_bridge.c`
- `src/core/brain_oscillations/nimcp_oscillations_immune_bridge.c`

### Neuron Models (2 files)
- `src/core/neuron_models/nimcp_neuron_substrate_bridge.c`
- `src/core/neuron_models/nimcp_sfa_pink_noise_bridge.c`

### Directives (2 files)
- `src/core/directives/nimcp_core_directives_fep_bridge.c`
- `src/core/directives/nimcp_core_directives_immune_bridge.c`

### Other (4 files)
- `src/core/synapse_compute/nimcp_synapse_substrate_bridge.c`
- `src/core/neural_substrate/nimcp_substrate_immune_bridge.c`
- `src/core/medulla/nimcp_medulla_immune_bridge.c`
- `src/core/logic/nimcp_neural_logic_quantum_bridge.c`
- `src/core/nimcp_axon_dendrite_substrate_bridge.c`

**TOTAL: 39 bridge files**

## Refactoring Pattern

### Header File Changes

#### 1. Add bridge_base.h include
```c
// Add after other utils includes
#include "utils/bridge/nimcp_bridge_base.h"
```

#### 2. Modify bridge struct
```c
// OLD:
typedef struct {
    system_a_t* system_a;
    system_b_t* system_b;
    config_t config;
    effects_t effects;
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    nimcp_mutex_t* mutex;
} my_bridge_t;

// NEW:
typedef struct {
    bridge_base_t base;                 // MUST be first
    config_t config;
    effects_t effects;
} my_bridge_t;

/* Accessor macros for type-safe system pointers */
#define MY_BRIDGE_GET_SYSTEM_A(bridge) ((system_a_t*)(bridge)->base.system_a)
#define MY_BRIDGE_GET_SYSTEM_B(bridge) ((system_b_t*)(bridge)->base.system_b)
```

### Implementation File Changes

#### 1. Replace create function
```c
// OLD:
my_bridge_t* my_bridge_create(const my_config_t* config, system_a_t* a, system_b_t* b) {
    if (!a || !b) return NULL;

    my_bridge_t* bridge = nimcp_malloc(sizeof(my_bridge_t));
    if (!bridge) return NULL;
    memset(bridge, 0, sizeof(my_bridge_t));

    bridge->system_a = a;
    bridge->system_b = b;

    // ... config setup ...

    bridge->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    nimcp_mutex_init(bridge->mutex, NULL);

    return bridge;
}

// NEW:
my_bridge_t* my_bridge_create(const my_config_t* config, system_a_t* a, system_b_t* b) {
    if (!a || !b) return NULL;

    // Use bridge_base abstraction for creation
    BRIDGE_CREATE_BEGIN(my_bridge_t, bridge, BIO_MODULE_MY_BRIDGE, "my_bridge");

    // Initialize configuration
    if (config) {
        memcpy(&bridge->config, config, sizeof(my_config_t));
    } else {
        my_bridge_default_config(&bridge->config);
    }

    // Connect systems using bridge_base (OUTSIDE lock!)
    bridge_base_connect_a(&bridge->base, a);
    bridge_base_connect_b(&bridge->base, b);

    // ... initialize effects ...

    return bridge;
}
```

#### 2. Replace destroy function
```c
// OLD:
void my_bridge_destroy(my_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->bio_async_enabled) {
        my_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
        nimcp_free(bridge->mutex);
    }

    nimcp_free(bridge);
}

// NEW:
void my_bridge_destroy(my_bridge_t* bridge) {
    BRIDGE_DESTROY(bridge);
}
```

#### 3. Replace mutex calls
```c
// OLD:
pthread_mutex_lock(bridge->mutex);    // or nimcp_mutex_lock(bridge->mutex)
pthread_mutex_unlock(bridge->mutex);  // or nimcp_mutex_unlock(bridge->mutex)

// NEW:
BRIDGE_LOCK(bridge);
BRIDGE_UNLOCK(bridge);
```

#### 4. Add bridge_base_record_update in update functions
```c
// In update function, BEFORE BRIDGE_UNLOCK:
int my_bridge_update(my_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    system_a_t* system_a = MY_BRIDGE_GET_SYSTEM_A(bridge);
    system_b_t* system_b = MY_BRIDGE_GET_SYSTEM_B(bridge);

    BRIDGE_LOCK(bridge);

    // ... compute effects ...

    // Record update in base bridge
    bridge_base_record_update(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    return 0;
}
```

#### 5. Use accessor macros for system pointers
```c
// OLD:
system_a_t* a = bridge->system_a;
system_b_t* b = bridge->system_b;

// NEW:
system_a_t* a = MY_BRIDGE_GET_SYSTEM_A(bridge);
system_b_t* b = MY_BRIDGE_GET_SYSTEM_B(bridge);
```

#### 6. Use BRIDGE_DEFINE_BIO_ASYNC_FUNCS macro
```c
// Replace manual bio-async functions with macro:
BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(my_bridge, my_bridge_t)

// This generates:
// - int my_bridge_connect_bio_async(my_bridge_t* bridge)
// - int my_bridge_disconnect_bio_async(my_bridge_t* bridge)
// - bool my_bridge_is_bio_async_connected(const my_bridge_t* bridge)
```

## Automated Changes Applied

The following changes were applied automatically via script:

1. ✅ Added `#include "utils/bridge/nimcp_bridge_base.h"` to headers
2. ✅ Replaced `pthread_mutex_lock(bridge->mutex)` → `BRIDGE_LOCK(bridge)`
3. ✅ Replaced `pthread_mutex_unlock(bridge->mutex)` → `BRIDGE_UNLOCK(bridge)`
4. ✅ Replaced `nimcp_mutex_lock(bridge->mutex)` → `BRIDGE_LOCK(bridge)`
5. ✅ Replaced `nimcp_mutex_unlock(bridge->mutex)` → `BRIDGE_UNLOCK(bridge)`

## Manual Changes Required

The following changes require manual intervention:

1. ⚠️ Add `bridge_base_t base;` as FIRST member in all bridge structs
2. ⚠️ Remove old system pointer fields (`system_a`, `system_b`)
3. ⚠️ Remove old bio-async fields (`bio_ctx`, `bio_async_enabled`)
4. ⚠️ Remove old mutex field (`nimcp_mutex_t* mutex`)
5. ⚠️ Add accessor macros below struct definition
6. ⚠️ Replace create function with `BRIDGE_CREATE_BEGIN` macro
7. ⚠️ Replace destroy function with `BRIDGE_DESTROY` macro
8. ⚠️ Add `bridge_base_connect_a/b()` calls in create (OUTSIDE lock)
9. ⚠️ Add `bridge_base_record_update()` calls in update functions (INSIDE lock, before unlock)
10. ⚠️ Replace system pointer accesses with accessor macros
11. ⚠️ Replace manual bio-async functions with `BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE` macro

## Example: Hemispheric Portia Bridge

### Header (PARTIALLY COMPLETED)

File: `include/core/brain/hemispheric/nimcp_hemispheric_portia_bridge.h`

**Changes Applied:**
- ✅ Added `#include "utils/bridge/nimcp_bridge_base.h"`
- ✅ Added `bridge_base_t base;` as first struct member
- ✅ Removed old system pointer fields
- ✅ Removed `bio_ctx`, `bio_async_enabled`, `mutex` fields
- ✅ Added accessor macros

**Remaining:**
- ⚠️ None (header complete)

### Implementation (PENDING)

File: `src/core/brain/hemispheric/nimcp_hemispheric_portia_bridge.c`

**Automated Changes:**
- ✅ Replaced all `pthread_mutex_lock` with `BRIDGE_LOCK`
- ✅ Replaced all `pthread_mutex_unlock` with `BRIDGE_UNLOCK`

**Remaining Manual Changes:**
- ⚠️ Replace `hemispheric_portia_create()` with `BRIDGE_CREATE_BEGIN` pattern
- ⚠️ Replace `hemispheric_portia_destroy()` with `BRIDGE_DESTROY` pattern
- ⚠️ Add `bridge_base_connect_a/b()` calls in create function
- ⚠️ Replace `bridge->brain` with `HEMISPHERIC_PORTIA_GET_BRAIN(bridge)`
- ⚠️ Replace `bridge->portia_system` with `HEMISPHERIC_PORTIA_GET_PORTIA(bridge)`
- ⚠️ Add `bridge_base_record_update(&bridge->base)` in `hemispheric_portia_update()`
- ⚠️ Replace bio-async functions with `BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(hemispheric_portia, hemispheric_portia_bridge_t)`

## Benefits of Refactoring

1. **60-70% Code Reduction**: Eliminates boilerplate in create/destroy functions
2. **Consistent Threading**: All bridges use same mutex abstraction
3. **Standardized Bio-async**: Uniform bio-async connection pattern
4. **Better Encapsulation**: System pointers are opaque, accessed via macros
5. **Update Tracking**: Automatic timestamp and counter management
6. **Easier Maintenance**: Changes to bridge infrastructure propagate automatically

## Testing Strategy

After refactoring each bridge:

1. Ensure it compiles without errors
2. Run unit tests for the bridge
3. Run integration tests involving the bridge
4. Verify bio-async connectivity still works
5. Check for memory leaks (valgrind)

## Progress Tracking

- **Headers Updated**: 1/39 (2.6%)
  - ✅ nimcp_hemispheric_portia_bridge.h
- **Implementations Updated**: 0/39 (0%)
- **Fully Refactored**: 0/39 (0%)

## Next Steps

1. Complete remaining 38 header files
2. Refactor all 39 implementation files
3. Test each bridge after refactoring
4. Update CLAUDE.md with new bridge pattern
5. Create migration guide for external users

## Reference Implementation

See `/home/bbrelin/nimcp/src/cognitive/working_memory/nimcp_working_memory_substrate_bridge.c` for the canonical bridge_base pattern.

## BIO_MODULE_* IDs Used

All bridges must have unique BIO_MODULE_* identifiers. Existing IDs:

- `BIO_MODULE_HEMISPHERIC_PORTIA = 0x1304`
- `BIO_MODULE_HEMISPHERIC_IMMUNE = 0x1305`
- `BIO_MODULE_HEMISPHERIC_SLEEP = 0x1306`
- `BIO_MODULE_HEMISPHERIC_FEP = 0x1307`
- `BIO_MODULE_HEMISPHERIC_GLIAL = 0x1308`

Check `include/async/nimcp_bio_messages.h` for complete list and assign new IDs as needed.
