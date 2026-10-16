# Plasticity Modules Bio-Async Integration Summary

## Integration Status

This document summarizes the bio-async communication and comprehensive logging integration into all plasticity modules.

### Completed Modules (Full Integration)

1. **STDP** (`src/plasticity/stdp/`) - BIO_MODULE_STDP (0x0400)
   - Already had bio-async integration
   - Module ID: 0x0400
   - Bio-async enabled with dopamine channel
   - Full logging integration

2. **STP** (`src/plasticity/stp/`) - BIO_MODULE_STP (0x0401)
   - Header: Added bio-async includes
   - Config: Added `stp_config_t` with `enable_bio_async` field
   - Source: Added module state, init/destroy functions
   - Module ID: 0x0401
   - LOG_MODULE: "stp"

3. **BCM** (`src/plasticity/bcm/`) - BIO_MODULE_BCM (0x0403)
   - Header: Added bio-async includes and `enable_bio_async` to params
   - Source: Added module state, init/destroy functions
   - Module ID: 0x0403
   - LOG_MODULE: "bcm"

4. **Homeostatic** (`src/plasticity/homeostatic/`) - BIO_MODULE_HOMEOSTATIC (0x0402)
   - Already had bio-async context in controller struct
   - Added module-level init/destroy functions
   - Module ID: 0x0402
   - LOG_MODULE: "homeostatic"

### Module ID Assignments (Updated in bio_messages.h)

```c
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

### Remaining Modules (Require Integration)

The following modules still need bio-async and logging integration following the same pattern:

5. **Dendritic** (`src/plasticity/dendritic/`) - BIO_MODULE_DENDRITIC (0x0404)
6. **Adaptive** (`src/plasticity/adaptive/`) - BIO_MODULE_ADAPTIVE (0x0405)
7. **Attention** (`src/plasticity/attention/`) - BIO_MODULE_ATTENTION_PLASTICITY (0x0406)
8. **Predictive Coding** (`src/plasticity/predictive/`) - BIO_MODULE_PREDICTIVE_CODING (0x0407)
9. **Neuromodulators** (`src/plasticity/neuromodulators/`) - BIO_MODULE_NEUROMODULATOR (0x0408)
   - Main file: `nimcp_neuromodulators.c`
   - Supporting files also need logging:
     - `nimcp_spatial_neuromod.c`
     - `nimcp_neuromod_pink_noise.c`
     - `nimcp_metabolic_pathways.c`
     - `nimcp_phasic_tonic.c`
     - `nimcp_receptor_subtypes.c`
     - `nimcp_vesicle_packaging.c`
10. **Pink Noise** (`src/plasticity/noise/`) - BIO_MODULE_PINK_NOISE (0x0409)
11. **Eligibility Trace** (`src/plasticity/eligibility/`) - BIO_MODULE_ELIGIBILITY_TRACE (0x040A)

## Integration Pattern

### Header File Changes

1. Add includes:
```c
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
```

2. Add `enable_bio_async` field to config struct:
```c
typedef struct {
    // ... existing fields ...
    bool enable_bio_async;  /**< Enable bio-async communication */
} module_config_t;
```

3. Add module management functions:
```c
bool module_init(const module_config_t* config);
void module_destroy(void);
```

### Source File Changes

1. Add includes and LOG_MODULE:
```c
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "module_name"
```

2. Add module state:
```c
typedef struct {
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    bool initialized;
} module_state_t;

static module_state_t g_module_state = {
    .bio_ctx = NULL,
    .bio_async_enabled = false,
    .initialized = false
};
```

3. Add init function:
```c
bool module_init(const module_config_t* config) {
    if (g_module_state.initialized) {
        LOG_WARN(LOG_MODULE, "Module already initialized");
        return true;
    }

    LOG_INFO(LOG_MODULE, "Initializing MODULE_NAME plasticity module");

    g_module_state.bio_ctx = NULL;
    g_module_state.bio_async_enabled = false;

    if (config && config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_XXX,
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
    LOG_INFO(LOG_MODULE, "Module initialization complete");
    return true;
}
```

4. Add destroy function:
```c
void module_destroy(void) {
    if (!g_module_state.initialized) {
        return;
    }

    LOG_INFO(LOG_MODULE, "Destroying MODULE_NAME plasticity module");

    if (g_module_state.bio_async_enabled && g_module_state.bio_ctx) {
        bio_router_unregister_module(g_module_state.bio_ctx);
        g_module_state.bio_ctx = NULL;
        g_module_state.bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async communication unregistered");
    }

    g_module_state.initialized = false;
    LOG_INFO(LOG_MODULE, "Module destroyed");
}
```

### Logging Changes

Replace any existing logging with:
- `LOG_INFO(LOG_MODULE, "message")`
- `LOG_DEBUG(LOG_MODULE, "message")`
- `LOG_WARN(LOG_MODULE, "message")`
- `LOG_ERROR(LOG_MODULE, "message")`

## Testing

After integration, each module should:
1. Compile without errors
2. Initialize successfully with bio-async enabled
3. Register with bio-router
4. Log initialization/destruction messages
5. Unregister cleanly on destroy

## Files Modified

### Headers
- `/home/bbrelin/nimcp/include/async/nimcp_bio_messages.h` - Module ID assignments
- `/home/bbrelin/nimcp/include/plasticity/stp/nimcp_stp.h` - Added bio-async support
- `/home/bbrelin/nimcp/include/plasticity/bcm/nimcp_bcm.h` - Added bio-async support
- `/home/bbrelin/nimcp/include/plasticity/homeostatic/nimcp_homeostatic.h` - Added module functions

### Source Files
- `/home/bbrelin/nimcp/src/plasticity/stp/nimcp_stp.c` - Full bio-async integration
- `/home/bbrelin/nimcp/src/plasticity/bcm/nimcp_bcm.c` - Full bio-async integration
- `/home/bbrelin/nimcp/src/plasticity/homeostatic/nimcp_homeostatic.c` - Module functions

## Next Steps

To complete integration for remaining modules:
1. Follow the pattern above for each module
2. Update headers first (includes, config, declarations)
3. Update source files (includes, state, init/destroy)
4. Add logging throughout existing functions
5. Test compilation and functionality

## Benefits

This integration provides:
1. **Async Communication**: Modules can send/receive messages via bio-router
2. **Event-Driven Updates**: Weight updates, neuromodulator releases via dopamine/serotonin/etc channels
3. **Comprehensive Logging**: All operations logged for debugging and monitoring
4. **Modular Architecture**: Clean init/destroy lifecycle
5. **Consistent API**: All plasticity modules follow same pattern
