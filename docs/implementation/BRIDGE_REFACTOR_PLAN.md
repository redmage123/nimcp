# Bridge Base Refactoring Plan

## Summary
Refactor 18 bridge files (14 remaining after calcium_sleep_bridge) to use bridge_base pattern.

## Files to Refactor

### Calcium Bridges (3 remaining)
1. `/home/bbrelin/nimcp/include/plasticity/calcium/nimcp_calcium_immune_bridge.h` + `.c`
2. `/home/bbrelin/nimcp/include/plasticity/calcium/nimcp_calcium_pink_noise_bridge.h` + `.c`

Note: nimcp_calcium_dynamics.c is not a bridge file (skipped)

### Astrocyte Bridges (2 files)
1. `/home/bbrelin/nimcp/include/plasticity/astrocyte/nimcp_astrocyte_immune_bridge.h` + `.c`
2. `/home/bbrelin/nimcp/include/plasticity/astrocyte/nimcp_astrocyte_sleep_bridge.h` + `.c`

Note: nimcp_astrocyte_plasticity.c is not a bridge file (skipped)

### Metabolic Bridges (3 files)
1. `/home/bbrelin/nimcp/include/plasticity/metabolic/nimcp_metabolic_immune_bridge.h` + `.c`
2. `/home/bbrelin/nimcp/include/plasticity/metabolic/nimcp_metabolic_sleep_bridge.h` + `.c`
3. `/home/bbrelin/nimcp/include/plasticity/metabolic/nimcp_metabolic_pink_noise_bridge.h` + `.c`

Note: nimcp_metabolic_plasticity.c is not a bridge file (skipped)

### Immune Bridges (7 files)
1. `/home/bbrelin/nimcp/include/plasticity/immune/nimcp_eligibility_immune_bridge.h` + `.c`
2. `/home/bbrelin/nimcp/include/plasticity/immune/nimcp_synaptic_scaling_immune_bridge.h` + `.c`
3. `/home/bbrelin/nimcp/include/plasticity/immune/nimcp_bcm_immune_bridge.h` + `.c`
4. `/home/bbrelin/nimcp/include/plasticity/immune/nimcp_homeostatic_immune_bridge.h` + `.c`
5. `/home/bbrelin/nimcp/include/plasticity/immune/nimcp_stdp_immune_bridge.h` + `.c`
6. `/home/bbrelin/nimcp/include/plasticity/immune/nimcp_dendritic_immune_bridge.h` + `.c`

Note: nimcp_neuromodulator_immune.c is not a bridge file (skipped)

## Standard Refactoring Pattern

### Header File Changes

1. **Add bridge_base include:**
```c
#include "utils/bridge/nimcp_bridge_base.h"
```

2. **Modify bridge struct (change from opaque to concrete):**
```c
// Before:
typedef struct foo_bridge_struct* foo_bridge_t;

// After:
typedef struct foo_bridge_struct {
    bridge_base_t base;                 /**< MUST be first member */
    foo_config_t config;
    foo_effects_t effects;
    // ... other fields except mutex ...
} foo_bridge_t;
```

3. **Add accessor macros:**
```c
#define FOO_GET_SYSTEM_A(bridge) ((type_a)(bridge)->base.system_a)
#define FOO_GET_SYSTEM_B(bridge) ((type_b)(bridge)->base.system_b)
```

4. **Update function signatures (add pointers):**
```c
// Before:
foo_bridge_t foo_bridge_create(...);
void foo_bridge_destroy(foo_bridge_t bridge);
int foo_update(foo_bridge_t bridge);

// After:
foo_bridge_t* foo_bridge_create(...);
void foo_bridge_destroy(foo_bridge_t* bridge);
int foo_update(foo_bridge_t* bridge);
```

5. **Add bio-async function declarations:**
```c
int foo_connect_bio_async(foo_bridge_t* bridge);
int foo_disconnect_bio_async(foo_bridge_t* bridge);
bool foo_is_bio_async_connected(const foo_bridge_t* bridge);
```

### Implementation File Changes

1. **Remove internal struct definition**
2. **Replace includes:**
```c
// Remove:
#include "utils/platform/nimcp_platform_mutex.h"

// Add:
#include "async/nimcp_bio_messages.h"
```

3. **Update create function:**
```c
foo_bridge_t* foo_bridge_create(...) {
    // Use bridge_base abstraction
    BRIDGE_CREATE_BEGIN(foo_bridge_t, bridge, BIO_MODULE_FOO, "foo_bridge");

    // Configuration
    if (config) {
        bridge->config = *config;
    } else {
        foo_default_config(&bridge->config);
    }

    // Connect systems OUTSIDE lock
    bridge_base_connect_a(&bridge->base, system_a);
    bridge_base_connect_b(&bridge->base, system_b);

    // Initialize effects/state

    return bridge;
}
```

4. **Update destroy function:**
```c
void foo_bridge_destroy(foo_bridge_t* bridge) {
    if (!bridge) return;

    // Custom cleanup if needed

    BRIDGE_DESTROY(bridge);
}
```

5. **Add bio-async functions:**
```c
BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(foo, foo_bridge_t)
```

6. **Replace mutex operations:**
```c
// Before:
pthread_mutex_lock(bridge->mutex);
pthread_mutex_unlock(bridge->mutex);

// After:
BRIDGE_LOCK(bridge);
BRIDGE_UNLOCK(bridge);
```

7. **Replace mutex allocation:**
```c
// Before:
bridge->mutex = nimcp_malloc(sizeof(pthread_mutex_t));
pthread_mutex_init(bridge->mutex, NULL);
pthread_mutex_destroy(bridge->mutex);
nimcp_free(bridge->mutex);

// After:
// Handled by BRIDGE_CREATE_BEGIN and BRIDGE_DESTROY macros
```

8. **Update functions to use BRIDGE_NULL_CHECK and accessor macros:**
```c
int foo_update(foo_bridge_t* bridge) {
    BRIDGE_NULL_CHECK(bridge);

    system_a sys_a = FOO_GET_SYSTEM_A(bridge);
    system_b sys_b = FOO_GET_SYSTEM_B(bridge);

    BRIDGE_LOCK(bridge);

    // Update logic

    bridge_base_record_update(&bridge->base);  // At end, INSIDE lock

    BRIDGE_UNLOCK(bridge);
    return 0;
}
```

## BIO_MODULE_* IDs to Use

From nimcp_bio_messages.h, assign appropriate module IDs:

- BIO_MODULE_CALCIUM_SLEEP (already exists)
- BIO_MODULE_CALCIUM_IMMUNE
- BIO_MODULE_CALCIUM_PINK_NOISE
- BIO_MODULE_ASTROCYTE_IMMUNE
- BIO_MODULE_ASTROCYTE_SLEEP
- BIO_MODULE_METABOLIC_IMMUNE
- BIO_MODULE_METABOLIC_SLEEP
- BIO_MODULE_METABOLIC_PINK_NOISE
- BIO_MODULE_ELIGIBILITY_IMMUNE
- BIO_MODULE_SYNAPTIC_SCALING_IMMUNE
- BIO_MODULE_BCM_IMMUNE
- BIO_MODULE_HOMEOSTATIC_IMMUNE
- BIO_MODULE_STDP_IMMUNE
- BIO_MODULE_DENDRITIC_IMMUNE

## Completed
- ✅ `/home/bbrelin/nimcp/include/plasticity/calcium/nimcp_calcium_sleep_bridge.h` + `.c`

## In Progress
- 🔄 Refactoring remaining calcium bridges
- ⏳ Astrocyte bridges
- ⏳ Metabolic bridges
- ⏳ Immune bridges
