# Complete Plasticity Modules Bio-Async Integration Guide

## Summary

This document provides a comprehensive summary of bio-async communication and comprehensive logging integration into all plasticity modules in NIMCP.

## Modules Status

### Fully Integrated (4/11 modules)

1. **STDP** - `src/plasticity/stdp/nimcp_stdp.c`
   - Module ID: `BIO_MODULE_STDP` (0x0400)
   - LOG_MODULE: "stdp" (already had bio-async from previous work)
   - Status: ✅ Complete

2. **STP** - `src/plasticity/stp/nimcp_stp.c`
   - Module ID: `BIO_MODULE_STP` (0x0401)
   - LOG_MODULE: "stp"
   - Status: ✅ Complete
   - Files modified:
     - `include/plasticity/stp/nimcp_stp.h` - Added bio-async includes, config struct
     - `src/plasticity/stp/nimcp_stp.c` - Added module state, init/destroy

3. **BCM** - `src/plasticity/bcm/nimcp_bcm.c`
   - Module ID: `BIO_MODULE_BCM` (0x0403)
   - LOG_MODULE: "bcm"
   - Status: ✅ Complete
   - Files modified:
     - `include/plasticity/bcm/nimcp_bcm.h` - Added bio-async includes, enable_bio_async to params
     - `src/plasticity/bcm/nimcp_bcm.c` - Added module state, init/destroy

4. **Homeostatic** - `src/plasticity/homeostatic/nimcp_homeostatic.c`
   - Module ID: `BIO_MODULE_HOMEOSTATIC` (0x0402)
   - LOG_MODULE: "homeostatic"
   - Status: ✅ Complete
   - Files modified:
     - `include/plasticity/homeostatic/nimcp_homeostatic.h` - Added module functions
     - `src/plasticity/homeostatic/nimcp_homeostatic.c` - Added module init/destroy

### Remaining Modules (7/11 modules)

5. **Dendritic** - `src/plasticity/dendritic/nimcp_dendritic.c`
   - Module ID: `BIO_MODULE_DENDRITIC` (0x0404)
   - LOG_MODULE: "dendritic"
   - Status: ⏳ Pending

6. **Adaptive** - `src/plasticity/adaptive/nimcp_adaptive.c`
   - Module ID: `BIO_MODULE_ADAPTIVE` (0x0405)
   - LOG_MODULE: "adaptive"
   - Status: ⏳ Pending

7. **Attention** - `src/plasticity/attention/nimcp_attention.c`
   - Module ID: `BIO_MODULE_ATTENTION_PLASTICITY` (0x0406)
   - LOG_MODULE: "attention"
   - Status: ⏳ Pending

8. **Predictive Coding** - `src/plasticity/predictive/nimcp_predictive_coding.c`
   - Module ID: `BIO_MODULE_PREDICTIVE_CODING` (0x0407)
   - LOG_MODULE: "predictive"
   - Status: ⏳ Pending

9. **Neuromodulators** - `src/plasticity/neuromodulators/nimcp_neuromodulators.c`
   - Module ID: `BIO_MODULE_NEUROMODULATOR` (0x0408)
   - LOG_MODULE: "neuromodulator"
   - Status: ⏳ Pending
   - Note: Also convert logging in supporting files:
     - `nimcp_spatial_neuromod.c`
     - `nimcp_neuromod_pink_noise.c`
     - `nimcp_metabolic_pathways.c`
     - `nimcp_phasic_tonic.c`
     - `nimcp_receptor_subtypes.c`
     - `nimcp_vesicle_packaging.c`

10. **Pink Noise** - `src/plasticity/noise/nimcp_pink_noise.c`
    - Module ID: `BIO_MODULE_PINK_NOISE` (0x0409)
    - LOG_MODULE: "pink_noise"
    - Status: ⏳ Pending

11. **Eligibility Trace** - `src/plasticity/eligibility/nimcp_eligibility_trace.c`
    - Module ID: `BIO_MODULE_ELIGIBILITY_TRACE` (0x040A)
    - LOG_MODULE: "eligibility"
    - Status: ⏳ Pending

## Integration Template

For each remaining module, follow this exact pattern:

### Step 1: Update Header File

Add bio-async includes at top:
```c
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
```

Add `enable_bio_async` to config/params struct:
```c
typedef struct {
    // ... existing fields ...
    bool enable_bio_async;  /**< Enable bio-async communication */
} module_config_t;
```

Add module management function declarations before `#endif`:
```c
//=============================================================================
// Module Management
//=============================================================================

/**
 * @brief Initialize MODULE_NAME module with bio-async support
 *
 * WHAT: Sets up bio-async communication for MODULE_NAME module
 * WHY: Enables async event-driven updates
 * HOW: Registers with bio-router and initializes module state
 *
 * @param config Module configuration (NULL = no bio-async)
 * @return true on success, false on failure
 */
bool module_init(const module_config_t* config);

/**
 * @brief Destroy MODULE_NAME module and cleanup resources
 *
 * WHAT: Cleans up bio-async resources and module state
 * WHY: Proper resource cleanup on shutdown
 * HOW: Unregisters from bio-router and resets state
 */
void module_destroy(void);
```

### Step 2: Update Source File

Add includes after existing includes:
```c
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "module_name"
```

Add module state after includes/before other code:
```c
//=============================================================================
// Module State
//=============================================================================

/**
 * @brief Global MODULE_NAME module state
 */
typedef struct {
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    bool initialized;
} module_name_module_state_t;

static module_name_module_state_t g_module_state = {
    .bio_ctx = NULL,
    .bio_async_enabled = false,
    .initialized = false
};
```

Add module management functions at end of file:
```c
//=============================================================================
// Module Management
//=============================================================================

/**
 * @brief Initialize MODULE_NAME module with bio-async support
 */
bool module_init(const module_config_t* config) {
    if (g_module_state.initialized) {
        LOG_WARN(LOG_MODULE, "MODULE_NAME module already initialized");
        return true;
    }

    LOG_INFO(LOG_MODULE, "Initializing MODULE_NAME plasticity module");

    // Initialize bio-async if requested
    g_module_state.bio_ctx = NULL;
    g_module_state.bio_async_enabled = false;

    if (config && config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_XXX,  // Use correct module ID
            .module_name = "module_name",
            .inbox_capacity = 64,
            .user_data = &g_module_state
        };

        g_module_state.bio_ctx = bio_router_register_module(&bio_info);
        if (g_module_state.bio_ctx) {
            g_module_state.bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async communication registered");
        } else {
            LOG_WARN(LOG_MODULE, "Failed to register bio-async communication");
        }
    }

    g_module_state.initialized = true;
    LOG_INFO(LOG_MODULE, "MODULE_NAME module initialization complete");
    return true;
}

/**
 * @brief Destroy MODULE_NAME module and cleanup resources
 */
void module_destroy(void) {
    if (!g_module_state.initialized) {
        return;
    }

    LOG_INFO(LOG_MODULE, "Destroying MODULE_NAME plasticity module");

    // Unregister bio-async
    if (g_module_state.bio_async_enabled && g_module_state.bio_ctx) {
        bio_router_unregister_module(g_module_state.bio_ctx);
        g_module_state.bio_ctx = NULL;
        g_module_state.bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async communication unregistered");
    }

    g_module_state.initialized = false;
    LOG_INFO(LOG_MODULE, "MODULE_NAME module destroyed");
}
```

### Step 3: Update Logging

Replace any old logging patterns with new ones throughout the source file:
- `NIMCP_LOGGING_INFO(...)` → `LOG_INFO(LOG_MODULE, ...)`
- `NIMCP_LOGGING_DEBUG(...)` → `LOG_DEBUG(LOG_MODULE, ...)`
- `NIMCP_LOGGING_WARN(...)` → `LOG_WARN(LOG_MODULE, ...)`
- `NIMCP_LOGGING_ERROR(...)` → `LOG_ERROR(LOG_MODULE, ...)`

## Module ID Reference

All module IDs have been assigned in `/home/bbrelin/nimcp/include/async/nimcp_bio_messages.h`:

```c
/* Plasticity modules */
BIO_MODULE_STDP = 0x0400,
BIO_MODULE_STP = 0x0401,
BIO_MODULE_HOMEOSTATIC = 0x0402,
BIO_MODULE_BCM = 0x0403,
BIO_MODULE_DENDRITIC = 0x0404,
BIO_MODULE_ADAPTIVE = 0x0405,
BIO_MODULE_ATTENTION_PLASTICITY = 0x0406,
BIO_MODULE_PREDICTIVE_CODING = 0x0407,
BIO_MODULE_NEUROMODULATOR = 0x0408,
BIO_MODULE_PINK_NOISE = 0x0409,
BIO_MODULE_ELIGIBILITY_TRACE = 0x040A,
```

## Files Modified

### Core Bio-Async Infrastructure
- `/home/bbrelin/nimcp/include/async/nimcp_bio_messages.h` - Module ID assignments

### Completed Integrations
- `/home/bbrelin/nimcp/include/plasticity/stp/nimcp_stp.h`
- `/home/bbrelin/nimcp/src/plasticity/stp/nimcp_stp.c`
- `/home/bbrelin/nimcp/include/plasticity/bcm/nimcp_bcm.h`
- `/home/bbrelin/nimcp/src/plasticity/bcm/nimcp_bcm.c`
- `/home/bbrelin/nimcp/include/plasticity/homeostatic/nimcp_homeostatic.h`
- `/home/bbrelin/nimcp/src/plasticity/homeostatic/nimcp_homeostatic.c`

### Remaining Files (Apply Template)

**Dendritic:**
- `/home/bbrelin/nimcp/include/plasticity/dendritic/nimcp_dendritic.h`
- `/home/bbrelin/nimcp/src/plasticity/dendritic/nimcp_dendritic.c`

**Adaptive:**
- `/home/bbrelin/nimcp/include/plasticity/adaptive/nimcp_adaptive.h`
- `/home/bbrelin/nimcp/src/plasticity/adaptive/nimcp_adaptive.c`

**Attention:**
- `/home/bbrelin/nimcp/include/plasticity/attention/nimcp_attention.h`
- `/home/bbrelin/nimcp/src/plasticity/attention/nimcp_attention.c`

**Predictive:**
- `/home/bbrelin/nimcp/include/plasticity/predictive/nimcp_predictive_coding.h`
- `/home/bbrelin/nimcp/src/plasticity/predictive/nimcp_predictive_coding.c`

**Neuromodulators (main + 6 supporting files):**
- `/home/bbrelin/nimcp/include/plasticity/neuromodulators/nimcp_neuromodulators.h`
- `/home/bbrelin/nimcp/src/plasticity/neuromodulators/nimcp_neuromodulators.c`
- `/home/bbrelin/nimcp/src/plasticity/neuromodulators/nimcp_spatial_neuromod.c` (logging only)
- `/home/bbrelin/nimcp/src/plasticity/neuromodulators/nimcp_neuromod_pink_noise.c` (logging only)
- `/home/bbrelin/nimcp/src/plasticity/neuromodulators/nimcp_metabolic_pathways.c` (logging only)
- `/home/bbrelin/nimcp/src/plasticity/neuromodulators/nimcp_phasic_tonic.c` (logging only)
- `/home/bbrelin/nimcp/src/plasticity/neuromodulators/nimcp_receptor_subtypes.c` (logging only)
- `/home/bbrelin/nimcp/src/plasticity/neuromodulators/nimcp_vesicle_packaging.c` (logging only)

**Pink Noise:**
- `/home/bbrelin/nimcp/include/plasticity/noise/nimcp_pink_noise.h`
- `/home/bbrelin/nimcp/src/plasticity/noise/nimcp_pink_noise.c`

**Eligibility:**
- `/home/bbrelin/nimcp/include/plasticity/eligibility/nimcp_eligibility_trace.h`
- `/home/bbrelin/nimcp/src/plasticity/eligibility/nimcp_eligibility_trace.c`

## Quick Reference - Function Names

Each module needs these functions (replace MODULE_NAME with actual name):

**Dendritic:**
- `dendritic_module_init(const dendritic_config_t* config)`
- `dendritic_module_destroy(void)`

**Adaptive:**
- `adaptive_module_init(const adaptive_config_t* config)`
- `adaptive_module_destroy(void)`

**Attention:**
- `attention_module_init(const attention_config_t* config)`
- `attention_module_destroy(void)`

**Predictive:**
- `predictive_module_init(const predictive_config_t* config)`
- `predictive_module_destroy(void)`

**Neuromodulator:**
- `neuromodulator_module_init(const neuromodulator_config_t* config)`
- `neuromodulator_module_destroy(void)`

**Pink Noise:**
- `pink_noise_module_init(const pink_noise_config_t* config)`
- `pink_noise_module_destroy(void)`

**Eligibility:**
- `eligibility_module_init(const eligibility_config_t* config)`
- `eligibility_module_destroy(void)`

## Testing Checklist

After integrating each module:
- [ ] Code compiles without errors
- [ ] No warnings about missing includes
- [ ] Module init returns true
- [ ] Bio-router registration succeeds (if bio-async enabled)
- [ ] Logging outputs appear correctly
- [ ] Module destroy cleans up properly
- [ ] No memory leaks (valgrind/asan)

## Benefits of This Integration

1. **Unified Communication**: All plasticity modules can communicate via bio-async channels
2. **Event-Driven Architecture**: Weight updates, neuromodulator releases propagate asynchronously
3. **Comprehensive Logging**: All operations tracked for debugging and analysis
4. **Consistent Lifecycle**: All modules follow init/destroy pattern
5. **Modular Design**: Each module is self-contained and independently testable
6. **Performance**: Async communication reduces blocking and improves responsiveness

## Next Steps

To complete the integration:

1. Work through each of the 7 remaining modules in order
2. Follow the template exactly for each module
3. Test after each module
4. Update this document as you complete each one
5. Create final integration test that initializes all modules together

## References

- Bio-async system: `/home/bbrelin/nimcp/include/async/nimcp_bio_async.h`
- Logging system: `/home/bbrelin/nimcp/include/utils/logging/nimcp_logging.h`
- Example integration: `/home/bbrelin/nimcp/src/plasticity/stp/nimcp_stp.c`
