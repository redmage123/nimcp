/**
 * @file nimcp_neuron_context.c
 * @brief AWS Inferentia NeuronCore Device Context Implementation
 *
 * WHAT: Runtime NRT (Neuron Runtime) loading and device management
 * WHY:  Enables NeuronCore inference without compile-time NRT dependency
 * HOW:  dlopen/dlsym pattern matching nimcp_gpu_detect.c
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#define LOG_MODULE "NEURON_CTX"
#define LOG_MODULE_ID 0x0920
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuron_context)

#include "gpu/neuron/nimcp_neuron_context.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>

// Platform-specific dynamic loading
#ifdef _WIN32
    #include <windows.h>
    typedef HMODULE lib_handle_t;
    #define NRT_LIB_OPEN(name)       LoadLibraryA(name)
    #define NRT_LIB_CLOSE(handle)    FreeLibrary(handle)
    #define NRT_LIB_SYM(handle, sym) ((void*)GetProcAddress(handle, sym))
#else
    #include <dlfcn.h>
    typedef void* lib_handle_t;
    #define NRT_LIB_OPEN(name)       dlopen(name, RTLD_LAZY)
    #define NRT_LIB_CLOSE(handle)    dlclose(handle)
    #define NRT_LIB_SYM(handle, sym) dlsym(handle, sym)
#endif

//=============================================================================
// NRT Library Search Paths
//=============================================================================

/** Library search paths for libnrt.so (in priority order) */
static const char* s_nrt_lib_paths[] = {
    "libnrt.so",                           // System path
    "libnrt.so.1",                         // Versioned
    "/opt/aws/neuron/lib/libnrt.so",       // AWS Neuron SDK default
    "/opt/aws/neuron/lib/libnrt.so.1",     // Versioned in SDK path
    NULL
};

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Try to load NRT library from known paths
 *
 * @return Library handle, or NULL if not found
 */
static lib_handle_t load_nrt_library(void)
{
    for (int i = 0; s_nrt_lib_paths[i] != NULL; i++) {
        lib_handle_t lib = NRT_LIB_OPEN(s_nrt_lib_paths[i]);
        if (lib) {
            LOG_INFO("Loaded NRT library from: %s", s_nrt_lib_paths[i]);
            return lib;
        }
    }

    LOG_DEBUG("NRT library not found in any search path");
    return NULL;
}

/**
 * @brief Resolve all NRT function pointers via dlsym
 *
 * @param ctx Context to populate
 * @return true if all required symbols resolved
 */
static bool resolve_nrt_symbols(nimcp_neuron_context_t* ctx)
{
    lib_handle_t lib = (lib_handle_t)ctx->nrt_lib;

    ctx->fn_init = (nrt_init_fn)NRT_LIB_SYM(lib, "nrt_init");
    ctx->fn_close = (nrt_close_fn)NRT_LIB_SYM(lib, "nrt_close");
    ctx->fn_get_device_count = (nrt_get_device_count_fn)NRT_LIB_SYM(lib, "nrt_get_total_nc_count");
    ctx->fn_load = (nrt_load_fn)NRT_LIB_SYM(lib, "nrt_load");
    ctx->fn_unload = (nrt_unload_fn)NRT_LIB_SYM(lib, "nrt_unload");
    ctx->fn_execute = (nrt_execute_fn)NRT_LIB_SYM(lib, "nrt_execute");
    ctx->fn_get_error = (nrt_get_error_fn)NRT_LIB_SYM(lib, "nrt_get_error_string");

    // Required: init, load, unload, execute
    if (!ctx->fn_init || !ctx->fn_load || !ctx->fn_unload || !ctx->fn_execute) {
        LOG_WARN("Missing required NRT symbols (init=%p, load=%p, unload=%p, execute=%p)",
                 (void*)ctx->fn_init, (void*)ctx->fn_load,
                 (void*)ctx->fn_unload, (void*)ctx->fn_execute);
        return false;
    }

    // Optional: get_device_count, close, get_error
    if (!ctx->fn_get_device_count) {
        LOG_DEBUG("nrt_get_total_nc_count not found — device count will default to 1");
    }
    if (!ctx->fn_get_error) {
        LOG_DEBUG("nrt_get_error_string not found — error messages unavailable");
    }

    return true;
}

//=============================================================================
// Public API: Context Lifecycle
//=============================================================================

nimcp_neuron_context_t* nimcp_neuron_context_create(int device_id)
{
    LOG_INFO("Creating Neuron context for device %d", device_id);

    // Load NRT library
    lib_handle_t lib = load_nrt_library();
    if (!lib) {
        LOG_DEBUG("NRT not available — Neuron context creation failed");
        return NULL;
    }

    // Allocate context
    nimcp_neuron_context_t* ctx = nimcp_calloc(1, sizeof(nimcp_neuron_context_t));
    if (!ctx) {
        NRT_LIB_CLOSE(lib);
        LOG_ERROR("Failed to allocate Neuron context");
        return NULL;
    }

    ctx->nrt_lib = lib;
    ctx->device_id = device_id;

    // Resolve function pointers
    if (!resolve_nrt_symbols(ctx)) {
        NRT_LIB_CLOSE(lib);
        nimcp_free(ctx);
        LOG_ERROR("Failed to resolve NRT symbols");
        return NULL;
    }

    // Initialize NRT
    nrt_status_t status = ctx->fn_init();
    if (status != NRT_SUCCESS) {
        const char* err = ctx->fn_get_error ? ctx->fn_get_error(status) : "unknown";
        LOG_ERROR("nrt_init failed: %s (status=%d)", err, status);
        NRT_LIB_CLOSE(lib);
        nimcp_free(ctx);
        return NULL;
    }

    // Query device count
    if (ctx->fn_get_device_count) {
        uint32_t count = 0;
        status = ctx->fn_get_device_count(&count);
        if (status == NRT_SUCCESS) {
            ctx->device_count = count;
        } else {
            ctx->device_count = 1;  // Assume at least 1 if init succeeded
        }
    } else {
        ctx->device_count = 1;
    }

    // Validate device_id
    if (device_id < 0 || (uint32_t)device_id >= ctx->device_count) {
        LOG_ERROR("Invalid device_id %d (available: %u)", device_id, ctx->device_count);
        if (ctx->fn_close) ctx->fn_close();
        NRT_LIB_CLOSE(lib);
        nimcp_free(ctx);
        return NULL;
    }

    ctx->initialized = true;
    LOG_INFO("Neuron context created: device=%d, total_devices=%u",
             device_id, ctx->device_count);

    return ctx;
}

void nimcp_neuron_context_destroy(nimcp_neuron_context_t* ctx)
{
    if (!ctx) return;

    // Unload model if loaded
    if (ctx->model_loaded && ctx->loaded_model) {
        nimcp_neuron_unload_model(ctx);
    }

    // Close NRT
    if (ctx->fn_close) {
        ctx->fn_close();
    }

    // Close library
    if (ctx->nrt_lib) {
        NRT_LIB_CLOSE((lib_handle_t)ctx->nrt_lib);
    }

    LOG_INFO("Neuron context destroyed (device=%d, inferences=%lu)",
             ctx->device_id, (unsigned long)ctx->total_inferences);

    nimcp_free(ctx);
}

bool nimcp_neuron_context_is_valid(const nimcp_neuron_context_t* ctx)
{
    return ctx && ctx->initialized && ctx->nrt_lib;
}

//=============================================================================
// Public API: Model Management
//=============================================================================

int nimcp_neuron_load_neff(nimcp_neuron_context_t* ctx, const char* neff_path)
{
    if (!ctx || !ctx->initialized) {
        LOG_ERROR("nimcp_neuron_load_neff: invalid context");
        return -1;
    }
    if (!neff_path) {
        LOG_ERROR("nimcp_neuron_load_neff: neff_path is NULL");
        return -1;
    }

    // Unload existing model first
    if (ctx->model_loaded) {
        nimcp_neuron_unload_model(ctx);
    }

    nrt_model_t model = NULL;
    nrt_status_t status = ctx->fn_load(neff_path, ctx->device_id, &model);
    if (status != NRT_SUCCESS) {
        const char* err = ctx->fn_get_error ? ctx->fn_get_error(status) : "unknown";
        LOG_ERROR("nrt_load failed for '%s': %s (status=%d)", neff_path, err, status);
        return -1;
    }

    ctx->loaded_model = model;
    ctx->model_loaded = true;

    LOG_INFO("NEFF model loaded: %s (device=%d)", neff_path, ctx->device_id);
    return 0;
}

int nimcp_neuron_unload_model(nimcp_neuron_context_t* ctx)
{
    if (!ctx || !ctx->initialized) return -1;
    if (!ctx->model_loaded) return 0;

    nrt_status_t status = ctx->fn_unload(ctx->loaded_model);
    if (status != NRT_SUCCESS) {
        const char* err = ctx->fn_get_error ? ctx->fn_get_error(status) : "unknown";
        LOG_WARN("nrt_unload failed: %s (status=%d)", err, status);
        // Still mark as unloaded to avoid double-free
    }

    ctx->loaded_model = NULL;
    ctx->model_loaded = false;

    LOG_DEBUG("Model unloaded from device %d", ctx->device_id);
    return (status == NRT_SUCCESS) ? 0 : -1;
}

//=============================================================================
// Public API: Inference
//=============================================================================

int nimcp_neuron_execute(nimcp_neuron_context_t* ctx,
                          const void* input, size_t input_size,
                          void* output, size_t output_size)
{
    if (!ctx || !ctx->initialized || !ctx->model_loaded) {
        return -1;
    }
    if (!input || !output || input_size == 0 || output_size == 0) {
        return -1;
    }

    nrt_status_t status = ctx->fn_execute(ctx->loaded_model,
                                           input, input_size,
                                           output, output_size);
    if (status != NRT_SUCCESS) {
        const char* err = ctx->fn_get_error ? ctx->fn_get_error(status) : "unknown";
        LOG_ERROR("nrt_execute failed: %s (status=%d)", err, status);
        return -1;
    }

    ctx->total_inferences++;
    return 0;
}
