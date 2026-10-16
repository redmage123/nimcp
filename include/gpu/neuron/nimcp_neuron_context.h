/**
 * @file nimcp_neuron_context.h
 * @brief AWS Inferentia NeuronCore Device Context
 *
 * WHAT: Device management for AWS Inferentia NeuronCores via NRT (Neuron Runtime)
 * WHY:  Enables NIMCP inference acceleration on AWS Inferentia (inf2) instances
 * HOW:  dlopen/dlsym for NRT API — no compile-time NRT headers needed
 *
 * IMPORTANT: NeuronCores are tensor accelerators (like TPUs), NOT neuromorphic
 * cores. They excel at dense matrix operations (GEMM/GEMV). NIMCP's sparse/
 * event-driven spike propagation stays on CPU. Only the dense forward pass
 * moves to NeuronCores.
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#ifndef NIMCP_NEURON_CONTEXT_H
#define NIMCP_NEURON_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// NRT Function Pointer Types (resolved via dlsym at runtime)
//=============================================================================

/** NRT error code (matches NRT_STATUS enum) */
typedef int nrt_status_t;
#define NRT_SUCCESS 0

/** NRT opaque handles */
typedef void* nrt_model_t;

/** NRT function signatures for dlsym resolution */
typedef nrt_status_t (*nrt_init_fn)(void);
typedef nrt_status_t (*nrt_close_fn)(void);
typedef nrt_status_t (*nrt_get_device_count_fn)(uint32_t*);
typedef nrt_status_t (*nrt_load_fn)(const char* neff_path, int device_id,
                                     nrt_model_t* model);
typedef nrt_status_t (*nrt_unload_fn)(nrt_model_t model);
typedef nrt_status_t (*nrt_execute_fn)(nrt_model_t model,
                                        const void* input, size_t input_size,
                                        void* output, size_t output_size);
typedef const char*  (*nrt_get_error_fn)(nrt_status_t status);

//=============================================================================
// Neuron Context
//=============================================================================

/**
 * @brief AWS Neuron device context
 *
 * WHAT: Holds NRT library handle, function pointers, and loaded model state
 * WHY:  Encapsulates all NRT interaction — no NRT headers needed at compile time
 * HOW:  dlopen libnrt.so, dlsym all needed functions, manage model lifecycle
 */
typedef struct nimcp_neuron_context {
    // Library handle
    void* nrt_lib;                    /**< dlopen handle for libnrt.so */
    bool initialized;                  /**< Context successfully initialized */

    // Device info
    int device_id;                     /**< NeuronCore device index */
    uint32_t device_count;             /**< Total available devices */

    // NRT function pointers (resolved via dlsym)
    nrt_init_fn fn_init;
    nrt_close_fn fn_close;
    nrt_get_device_count_fn fn_get_device_count;
    nrt_load_fn fn_load;
    nrt_unload_fn fn_unload;
    nrt_execute_fn fn_execute;
    nrt_get_error_fn fn_get_error;

    // Loaded model state
    nrt_model_t loaded_model;          /**< Currently loaded NEFF model */
    bool model_loaded;                 /**< True if a model is loaded */

    // Memory tracking
    size_t total_allocated;            /**< Total bytes allocated on device */
    uint64_t total_inferences;         /**< Total inference calls */
} nimcp_neuron_context_t;

//=============================================================================
// Context Lifecycle API
//=============================================================================

/**
 * @brief Create a Neuron context for the specified device
 *
 * WHAT: Opens libnrt.so, resolves symbols, initializes NRT
 * WHY:  Required before any NeuronCore operations
 * HOW:  dlopen with fallback paths, dlsym all function pointers
 *
 * @param device_id NeuronCore device index (0-based)
 * @return Context pointer, or NULL if NRT not available
 */
NIMCP_EXPORT nimcp_neuron_context_t* nimcp_neuron_context_create(int device_id);

/**
 * @brief Destroy Neuron context and release resources
 *
 * @param ctx Context to destroy (NULL-safe)
 */
NIMCP_EXPORT void nimcp_neuron_context_destroy(nimcp_neuron_context_t* ctx);

/**
 * @brief Check if Neuron context is valid and ready
 *
 * @param ctx Context to check
 * @return true if context is initialized and usable
 */
NIMCP_EXPORT bool nimcp_neuron_context_is_valid(const nimcp_neuron_context_t* ctx);

//=============================================================================
// Model Management API
//=============================================================================

/**
 * @brief Load a compiled NEFF model onto the NeuronCore
 *
 * WHAT: Loads a pre-compiled Neuron Executable File Format (NEFF) model
 * WHY:  NEFF is the optimized binary format for NeuronCore execution
 * HOW:  Calls nrt_load() to transfer model to device memory
 *
 * @param ctx Neuron context
 * @param neff_path Path to .neff file
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_neuron_load_neff(nimcp_neuron_context_t* ctx,
                                         const char* neff_path);

/**
 * @brief Unload current model from NeuronCore
 *
 * @param ctx Neuron context
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_neuron_unload_model(nimcp_neuron_context_t* ctx);

//=============================================================================
// Inference API
//=============================================================================

/**
 * @brief Execute inference on the loaded model
 *
 * WHAT: Runs a single forward pass on the NeuronCore
 * WHY:  Hot path — this is where the speedup happens
 * HOW:  Copies input to device, executes, copies output back
 *
 * @param ctx Neuron context (must have model loaded)
 * @param input Input tensor data (host memory)
 * @param input_size Input size in bytes
 * @param output Output tensor data (host memory, pre-allocated)
 * @param output_size Output buffer size in bytes
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_neuron_execute(nimcp_neuron_context_t* ctx,
                                       const void* input, size_t input_size,
                                       void* output, size_t output_size);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURON_CONTEXT_H
