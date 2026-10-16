/**
 * @file nimcp_gpu_neuron.c
 * @brief GPU Neural Network Implementation (with CPU fallback)
 *
 * WHAT: GPU-accelerated neural network with automatic CPU fallback
 * WHY:  Provide 10-100x speedup when GPU available, graceful degradation otherwise
 * HOW:  Detect GPU at runtime, use CUDA if available, CPU otherwise
 *
 * IMPLEMENTATION STRATEGY:
 * - All functions check GPU availability
 * - CUDA code compiled only when NIMCP_ENABLE_CUDA defined
 * - CPU fallback always available
 * - Same API for both paths
 * - Uses nimcp_gpu_tensor_t for GPU memory management
 *
 * TENSOR-BASED ARCHITECTURE (v2.8.0):
 * - Neuron states stored in 2D tensors [N_neurons x fields]
 * - Synapse data stored in 2D tensors [N_synapses x 4]
 * - Enables batch GPU operations and better memory coalescing
 * - Unified tensor API for GPU memory management
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.8.0
 */

#include "gpu/nimcp_gpu_neuron.h"
#include "gpu/execution/nimcp_gpu_detect.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "gpu/nimcp_execution_mode.h"
#include "utils/memory/nimcp_memory.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "GPU"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gpu_neuron)

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

//=============================================================================
// Tensor Field Indices for Neuron State
//=============================================================================

/**
 * @brief Field indices for neuron state tensors
 *
 * Neuron state is stored as a 2D tensor [N_neurons x NEURON_FIELD_COUNT]
 * Each row represents one neuron, columns are the state fields.
 */
typedef enum {
    NEURON_FIELD_MEMBRANE_POTENTIAL = 0,  /**< V_m (mV) */
    NEURON_FIELD_THRESHOLD,                /**< Spike threshold */
    NEURON_FIELD_STATE,                    /**< Activation state */
    NEURON_FIELD_BIAS,                     /**< Intrinsic bias */
    NEURON_FIELD_CALCIUM,                  /**< Calcium concentration */
    NEURON_FIELD_SYNAPTIC_TRACE,           /**< STDP trace */
    NEURON_FIELD_LEARNING_RATE,            /**< Learning rate */
    NEURON_FIELD_FIRING_RATE,              /**< Instantaneous rate (Hz) */
    NEURON_FIELD_COUNT                     /**< Total float fields per neuron */
} neuron_float_field_t;

/**
 * @brief Integer field indices for neuron metadata tensor
 */
typedef enum {
    NEURON_INT_LAST_SPIKE_LOW = 0,         /**< Last spike time (low 32 bits) */
    NEURON_INT_LAST_SPIKE_HIGH,            /**< Last spike time (high 32 bits) */
    NEURON_INT_NEURON_ID,                  /**< Global neuron ID */
    NEURON_INT_SYNAPSE_OFFSET,             /**< Index into synapse array */
    NEURON_INT_NUM_INCOMING,               /**< Number of incoming synapses */
    NEURON_INT_NUM_OUTGOING,               /**< Number of outgoing synapses */
    NEURON_INT_SPIKE_COUNT,                /**< Total spikes emitted */
    NEURON_INT_REFRACTORY_PERIOD,          /**< Refractory period (us) */
    NEURON_INT_FIELD_COUNT                 /**< Total int fields per neuron */
} neuron_int_field_t;

/**
 * @brief Field indices for synapse tensor
 */
typedef enum {
    SYNAPSE_FIELD_SOURCE_ID = 0,           /**< Pre-synaptic neuron ID (as float) */
    SYNAPSE_FIELD_TARGET_ID,               /**< Post-synaptic neuron ID (as float) */
    SYNAPSE_FIELD_WEIGHT,                  /**< Synaptic weight */
    SYNAPSE_FIELD_STRENGTH,                /**< Synaptic strength */
    SYNAPSE_FIELD_COUNT                    /**< Total fields per synapse */
} synapse_field_t;

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief GPU neural network internal structure (tensor-based)
 *
 * ARCHITECTURE:
 * - Uses nimcp_gpu_tensor_t for all GPU memory
 * - Neuron states split into float and int tensors for type safety
 * - Enables efficient batch operations via cuBLAS
 */
struct gpu_neural_network_struct {
    // Configuration
    gpu_network_config_t config;

    // GPU context (manages device, streams, handles)
    nimcp_gpu_context_t* gpu_ctx;

    //=========================================================================
    // Tensor-based Neuron Storage (GPU)
    //=========================================================================

    /**
     * @brief Neuron float state tensor [N_neurons x NEURON_FIELD_COUNT]
     *
     * Layout (row = neuron, col = field):
     * - [n][0]: membrane_potential
     * - [n][1]: threshold
     * - [n][2]: state (activation)
     * - [n][3]: bias
     * - [n][4]: calcium_concentration
     * - [n][5]: synaptic_trace
     * - [n][6]: learning_rate
     * - [n][7]: firing_rate
     */
    nimcp_gpu_tensor_t* neuron_states;

    /**
     * @brief Neuron integer metadata tensor [N_neurons x NEURON_INT_FIELD_COUNT]
     *
     * Stored as UINT32 tensor:
     * - [n][0-1]: last_spike (64-bit as 2x32)
     * - [n][2]: neuron_id
     * - [n][3]: synapse_offset
     * - [n][4]: num_incoming
     * - [n][5]: num_outgoing
     * - [n][6]: spike_count
     * - [n][7]: refractory_period
     */
    nimcp_gpu_tensor_t* neuron_metadata;

    /**
     * @brief Synapse tensor [N_synapses x SYNAPSE_FIELD_COUNT]
     *
     * All fields stored as float for GPU efficiency:
     * - [s][0]: source_id (cast to float)
     * - [s][1]: target_id (cast to float)
     * - [s][2]: weight
     * - [s][3]: strength
     */
    nimcp_gpu_tensor_t* synapse_data;

    /**
     * @brief Spike queue tensor [max_spikes x 4]
     *
     * For collecting spike events during update:
     * - [s][0]: timestamp_low
     * - [s][1]: timestamp_high
     * - [s][2]: source_id
     * - [s][3]: amplitude
     */
    nimcp_gpu_tensor_t* spike_queue;

    //=========================================================================
    // Host-side copies (for CPU access and fallback)
    //=========================================================================
    gpu_neuron_state_t* neurons_host;
    gpu_synapse_t* synapses_host;

    // Counts
    uint32_t neurons_count;
    uint32_t neurons_capacity;
    uint32_t synapses_count;
    uint32_t synapses_capacity;

    // Execution mode
    bool using_gpu;
    bool tensors_dirty;  /**< Host data modified, need GPU sync */

    // Statistics
    uint64_t total_spikes;
    uint64_t total_updates;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// GPU Detection (delegated to nimcp_gpu_detect module)
//=============================================================================

// NOTE: gpu_is_available() and gpu_get_device_count() are now provided by
// the nimcp_gpu_detect module which does runtime GPU detection.
// See include/gpu/execution/nimcp_gpu_detect.h

//=============================================================================
// Internal Helper Functions: Host <-> Tensor Conversion
//=============================================================================

/**
 * @brief Pack a single gpu_neuron_state_t into tensor format
 *
 * WHAT: Convert struct fields to tensor row format
 * WHY:  Enable GPU-friendly memory layout
 * HOW:  Extract float and int fields into separate buffers
 */
static void pack_neuron_to_tensors(
    const gpu_neuron_state_t* neuron,
    float* float_row,
    uint32_t* int_row)
{
    // Float fields
    float_row[NEURON_FIELD_MEMBRANE_POTENTIAL] = neuron->membrane_potential;
    float_row[NEURON_FIELD_THRESHOLD] = neuron->threshold;
    float_row[NEURON_FIELD_STATE] = neuron->state;
    float_row[NEURON_FIELD_BIAS] = neuron->bias;
    float_row[NEURON_FIELD_CALCIUM] = neuron->calcium_concentration;
    float_row[NEURON_FIELD_SYNAPTIC_TRACE] = neuron->synaptic_trace;
    float_row[NEURON_FIELD_LEARNING_RATE] = neuron->learning_rate;
    float_row[NEURON_FIELD_FIRING_RATE] = neuron->firing_rate;

    // Integer fields (split 64-bit timestamp into two 32-bit values)
    int_row[NEURON_INT_LAST_SPIKE_LOW] = (uint32_t)(neuron->last_spike & 0xFFFFFFFF);
    int_row[NEURON_INT_LAST_SPIKE_HIGH] = (uint32_t)(neuron->last_spike >> 32);
    int_row[NEURON_INT_NEURON_ID] = neuron->neuron_id;
    int_row[NEURON_INT_SYNAPSE_OFFSET] = neuron->synapse_offset;
    int_row[NEURON_INT_NUM_INCOMING] = neuron->num_incoming;
    int_row[NEURON_INT_NUM_OUTGOING] = neuron->num_outgoing;
    int_row[NEURON_INT_SPIKE_COUNT] = neuron->spike_count;
    int_row[NEURON_INT_REFRACTORY_PERIOD] = neuron->refractory_period;
}

/**
 * @brief Unpack tensor row format back to gpu_neuron_state_t
 */
static void unpack_tensors_to_neuron(
    const float* float_row,
    const uint32_t* int_row,
    gpu_neuron_state_t* neuron)
{
    // Float fields
    neuron->membrane_potential = float_row[NEURON_FIELD_MEMBRANE_POTENTIAL];
    neuron->threshold = float_row[NEURON_FIELD_THRESHOLD];
    neuron->state = float_row[NEURON_FIELD_STATE];
    neuron->bias = float_row[NEURON_FIELD_BIAS];
    neuron->calcium_concentration = float_row[NEURON_FIELD_CALCIUM];
    neuron->synaptic_trace = float_row[NEURON_FIELD_SYNAPTIC_TRACE];
    neuron->learning_rate = float_row[NEURON_FIELD_LEARNING_RATE];
    neuron->firing_rate = float_row[NEURON_FIELD_FIRING_RATE];

    // Integer fields (reconstruct 64-bit timestamp)
    neuron->last_spike = ((uint64_t)int_row[NEURON_INT_LAST_SPIKE_HIGH] << 32) |
                         int_row[NEURON_INT_LAST_SPIKE_LOW];
    neuron->neuron_id = int_row[NEURON_INT_NEURON_ID];
    neuron->synapse_offset = int_row[NEURON_INT_SYNAPSE_OFFSET];
    neuron->num_incoming = int_row[NEURON_INT_NUM_INCOMING];
    neuron->num_outgoing = int_row[NEURON_INT_NUM_OUTGOING];
    neuron->spike_count = int_row[NEURON_INT_SPIKE_COUNT];
    neuron->refractory_period = int_row[NEURON_INT_REFRACTORY_PERIOD];
}

/**
 * @brief Pack synapse to tensor row format
 */
static void pack_synapse_to_tensor(
    const gpu_synapse_t* synapse,
    float* row)
{
    row[SYNAPSE_FIELD_SOURCE_ID] = (float)synapse->source_id;
    row[SYNAPSE_FIELD_TARGET_ID] = (float)synapse->target_id;
    row[SYNAPSE_FIELD_WEIGHT] = synapse->weight;
    row[SYNAPSE_FIELD_STRENGTH] = synapse->strength;
}

/**
 * @brief Unpack tensor row to synapse
 *
 * Note: Currently unused but kept for future synapse read-back operations
 */
__attribute__((unused))
static void unpack_tensor_to_synapse(
    const float* row,
    gpu_synapse_t* synapse)
{
    synapse->source_id = (uint32_t)row[SYNAPSE_FIELD_SOURCE_ID];
    synapse->target_id = (uint32_t)row[SYNAPSE_FIELD_TARGET_ID];
    synapse->weight = row[SYNAPSE_FIELD_WEIGHT];
    synapse->strength = row[SYNAPSE_FIELD_STRENGTH];
}

/**
 * @brief Sync all host neuron/synapse data to GPU tensors
 *
 * WHAT: Upload host arrays to GPU tensor memory
 * WHY:  Keep GPU tensors in sync after host-side modifications
 * HOW:  Pack data into contiguous buffers, upload via tensor API
 */
static bool sync_host_to_gpu_tensors(gpu_neural_network_t network)
{
    if (!network || !network->using_gpu || !network->tensors_dirty) {
        return true;  // Nothing to sync
    }

    // Allocate temporary packed buffers
    size_t float_size = network->neurons_count * NEURON_FIELD_COUNT * sizeof(float);
    size_t int_size = network->neurons_count * NEURON_INT_FIELD_COUNT * sizeof(uint32_t);
    size_t syn_size = network->synapses_count * SYNAPSE_FIELD_COUNT * sizeof(float);

    float* float_buf = (float*)nimcp_malloc(float_size);
    uint32_t* int_buf = (uint32_t*)nimcp_malloc(int_size);
    float* syn_buf = (float*)nimcp_malloc(syn_size);

    if (!float_buf || !int_buf || !syn_buf) {
        nimcp_free(float_buf);
        nimcp_free(int_buf);
        nimcp_free(syn_buf);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sync_host_to_gpu_tensors: required parameter is NULL (float_buf, int_buf, syn_buf)");
        return false;
    }

    // Pack neurons
    for (uint32_t i = 0; i < network->neurons_count; i++) {
        pack_neuron_to_tensors(
            &network->neurons_host[i],
            &float_buf[i * NEURON_FIELD_COUNT],
            &int_buf[i * NEURON_INT_FIELD_COUNT]
        );
    }

    // Pack synapses
    for (uint32_t i = 0; i < network->synapses_count; i++) {
        pack_synapse_to_tensor(
            &network->synapses_host[i],
            &syn_buf[i * SYNAPSE_FIELD_COUNT]
        );
    }

    // Upload to GPU tensors
    bool success = true;
    if (network->neuron_states && network->neurons_count > 0) {
        // Use the tensor's ctx for memory copy
        nimcp_gpu_context_t* ctx = network->gpu_ctx;
        if (ctx) {
            int err = nimcp_gpu_memcpy(ctx,
                                        network->neuron_states->data,
                                        float_buf,
                                        float_size,
                                        GPU_MEMCPY_HOST_TO_DEVICE);
            if (err != 0) {
                success = false;
            }

            err = nimcp_gpu_memcpy(ctx,
                                    network->neuron_metadata->data,
                                    int_buf,
                                    int_size,
                                    GPU_MEMCPY_HOST_TO_DEVICE);
            if (err != 0) {
                success = false;
            }
        }
    }

    if (network->synapse_data && network->synapses_count > 0 && success) {
        nimcp_gpu_context_t* ctx = network->gpu_ctx;
        if (ctx) {
            int err = nimcp_gpu_memcpy(ctx,
                                        network->synapse_data->data,
                                        syn_buf,
                                        syn_size,
                                        GPU_MEMCPY_HOST_TO_DEVICE);
            if (err != 0) {
                success = false;
            }
        }
    }

    nimcp_free(float_buf);
    nimcp_free(int_buf);
    nimcp_free(syn_buf);

    if (success) {
        network->tensors_dirty = false;
    }

    return success;
}

/**
 * @brief Sync GPU tensor data back to host arrays
 */
static bool sync_gpu_to_host_tensors(gpu_neural_network_t network)
{
    if (!network || !network->using_gpu) {
        return true;
    }

    // Allocate temporary packed buffers
    size_t float_size = network->neurons_count * NEURON_FIELD_COUNT * sizeof(float);
    size_t int_size = network->neurons_count * NEURON_INT_FIELD_COUNT * sizeof(uint32_t);

    float* float_buf = (float*)nimcp_malloc(float_size);
    uint32_t* int_buf = (uint32_t*)nimcp_malloc(int_size);

    if (!float_buf || !int_buf) {
        nimcp_free(float_buf);
        nimcp_free(int_buf);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sync_gpu_to_host_tensors: required parameter is NULL (float_buf, int_buf)");
        return false;
    }

    bool success = true;
    nimcp_gpu_context_t* ctx = network->gpu_ctx;

    if (ctx && network->neuron_states && network->neurons_count > 0) {
        int err = nimcp_gpu_memcpy(ctx,
                                    float_buf,
                                    network->neuron_states->data,
                                    float_size,
                                    GPU_MEMCPY_DEVICE_TO_HOST);
        if (err != 0) {
            success = false;
        }

        err = nimcp_gpu_memcpy(ctx,
                                int_buf,
                                network->neuron_metadata->data,
                                int_size,
                                GPU_MEMCPY_DEVICE_TO_HOST);
        if (err != 0) {
            success = false;
        }
    }

    // Unpack neurons
    if (success) {
        for (uint32_t i = 0; i < network->neurons_count; i++) {
            unpack_tensors_to_neuron(
                &float_buf[i * NEURON_FIELD_COUNT],
                &int_buf[i * NEURON_INT_FIELD_COUNT],
                &network->neurons_host[i]
            );
        }
    }

    nimcp_free(float_buf);
    nimcp_free(int_buf);

    return success;
}

/**
 * @brief Get device name for a specific GPU (legacy compatibility wrapper)
 *
 * WHAT: Returns the name of a GPU device
 * WHY:  Backward compatibility with existing code
 * HOW:  Delegates to gpu_get_device_info from nimcp_gpu_detect
 */
NIMCP_EXPORT bool gpu_get_device_name(uint32_t device_id, char* name, size_t max_len)
{
    if (!name || max_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_get_device_name: name is NULL");
        return false;
    }

    // Use runtime GPU detection
    gpu_device_info_t info;
    if (gpu_get_device_info(device_id, &info)) {
        snprintf(name, max_len, "%s", info.name);
        return true;
    }

    snprintf(name, max_len, "CPU Fallback");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gpu_get_device_name: validation failed");
    return false;
}

//=============================================================================
// Configuration
//=============================================================================

NIMCP_EXPORT gpu_network_config_t gpu_get_optimal_config(uint32_t num_neurons)
{
    gpu_network_config_t config = {0};

    config.num_neurons = num_neurons;
    config.num_synapses = num_neurons * 100;  // Average 100 synapses per neuron

    // GPU kernel configuration
    config.threads_per_block = 256;  // Optimal for most GPUs
    config.max_blocks = (num_neurons + config.threads_per_block - 1) / config.threads_per_block;

    // Memory configuration
    config.spike_queue_capacity = num_neurons * 10;  // 10 spikes per neuron buffer
    config.use_unified_memory = false;  // Explicit transfers faster
    config.pin_host_memory = true;      // Faster CPU↔GPU transfers

    // Execution mode (auto-detect)
    config.exec_mode = gpu_is_available() ? EXEC_MODE_GPU_CUDA : EXEC_MODE_CPU_SEQUENTIAL;

    // Learning configuration
    config.enable_stdp = true;
    config.enable_bcm = false;
    config.global_learning_rate = 0.01F;

    return config;
}

//=============================================================================
// Network Lifecycle
//=============================================================================

/**
 * @brief Create GPU tensors for network storage
 *
 * WHAT: Allocate GPU tensor memory for neurons and synapses
 * WHY:  Tensor-based storage enables efficient batch GPU operations
 * HOW:  Use nimcp_gpu_tensor_create with appropriate dimensions
 */
static bool create_gpu_tensors(gpu_neural_network_t network)
{
    if (!network || !network->gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_gpu_tensors: required parameter is NULL (network, network->gpu_ctx)");
        return false;
    }

    nimcp_gpu_context_t* ctx = network->gpu_ctx;

    // Neuron state tensor: [N_neurons x NEURON_FIELD_COUNT] (FP32)
    size_t neuron_dims[2] = { network->neurons_capacity, NEURON_FIELD_COUNT };
    network->neuron_states = nimcp_gpu_tensor_create(
        ctx, neuron_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!network->neuron_states) {
        LOG_ERROR(LOG_MODULE, "Failed to create neuron_states tensor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_gpu_tensors: network->neuron_states is NULL");
        return false;
    }

    // Neuron metadata tensor: [N_neurons x NEURON_INT_FIELD_COUNT] (UINT32)
    size_t meta_dims[2] = { network->neurons_capacity, NEURON_INT_FIELD_COUNT };
    network->neuron_metadata = nimcp_gpu_tensor_create(
        ctx, meta_dims, 2, NIMCP_GPU_PRECISION_UINT32);
    if (!network->neuron_metadata) {
        LOG_ERROR(LOG_MODULE, "Failed to create neuron_metadata tensor");
        nimcp_gpu_tensor_destroy(network->neuron_states);
        network->neuron_states = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_gpu_tensors: network->neuron_metadata is NULL");
        return false;
    }

    // Synapse tensor: [N_synapses x SYNAPSE_FIELD_COUNT] (FP32)
    size_t syn_dims[2] = { network->synapses_capacity, SYNAPSE_FIELD_COUNT };
    network->synapse_data = nimcp_gpu_tensor_create(
        ctx, syn_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!network->synapse_data) {
        LOG_ERROR(LOG_MODULE, "Failed to create synapse_data tensor");
        nimcp_gpu_tensor_destroy(network->neuron_states);
        nimcp_gpu_tensor_destroy(network->neuron_metadata);
        network->neuron_states = NULL;
        network->neuron_metadata = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_gpu_tensors: network->synapse_data is NULL");
        return false;
    }

    // Spike queue tensor: [spike_queue_capacity x 4] (FP32 for mixed data)
    // NOTE: The spike queue is OPTIONAL. If creation fails, the network operates
    // without GPU-side spike collection. This is acceptable because:
    // 1. CPU fallback path collects spikes from host arrays directly
    // 2. The 3 required tensors (neuron_states, neuron_metadata, synapse_data)
    //    are properly cleaned up by destroy_gpu_tensors() on all failure paths
    // 3. gpu_neural_network_destroy() calls destroy_gpu_tensors() which handles
    //    NULL spike_queue gracefully
    size_t spike_capacity = network->config.spike_queue_capacity;
    if (spike_capacity == 0) {
        spike_capacity = network->neurons_capacity * 10;  // Default
    }
    size_t spike_dims[2] = { spike_capacity, 4 };
    network->spike_queue = nimcp_gpu_tensor_create(
        ctx, spike_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!network->spike_queue) {
        LOG_WARN(LOG_MODULE, "Failed to create spike_queue tensor (non-fatal)");
        // Non-fatal: spike queue is optional for basic operation
    }

    // Zero-initialize all tensors
    nimcp_gpu_zeros(ctx, network->neuron_states);
    nimcp_gpu_zeros(ctx, network->neuron_metadata);
    nimcp_gpu_zeros(ctx, network->synapse_data);
    if (network->spike_queue) {
        nimcp_gpu_zeros(ctx, network->spike_queue);
    }

    LOG_INFO(LOG_MODULE, "GPU tensors created: neurons[%zu x %d], synapses[%zu x %d]",
             neuron_dims[0], NEURON_FIELD_COUNT,
             syn_dims[0], SYNAPSE_FIELD_COUNT);

    return true;
}

/**
 * @brief Destroy GPU tensors
 */
static void destroy_gpu_tensors(gpu_neural_network_t network)
{
    if (!network) {
        return;
    }

    if (network->neuron_states) {
        nimcp_gpu_tensor_destroy(network->neuron_states);
        network->neuron_states = NULL;
    }
    if (network->neuron_metadata) {
        nimcp_gpu_tensor_destroy(network->neuron_metadata);
        network->neuron_metadata = NULL;
    }
    if (network->synapse_data) {
        nimcp_gpu_tensor_destroy(network->synapse_data);
        network->synapse_data = NULL;
    }
    if (network->spike_queue) {
        nimcp_gpu_tensor_destroy(network->spike_queue);
        network->spike_queue = NULL;
    }
}

NIMCP_EXPORT gpu_neural_network_t gpu_neural_network_create(
    const gpu_network_config_t* config)
{
    if (!config || config->num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_neural_network_create: config is NULL");
        return NULL;
    }

    // Allocate network structure
    gpu_neural_network_t network = nimcp_calloc(1, sizeof(struct gpu_neural_network_struct));
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network is NULL");

        return NULL;
    }

    // Copy configuration
    network->config = *config;
    network->neurons_capacity = config->num_neurons;
    network->synapses_capacity = config->num_synapses;
    network->neurons_count = 0;
    network->synapses_count = 0;
    network->total_spikes = 0;
    network->total_updates = 0;
    network->tensors_dirty = false;
    network->gpu_ctx = NULL;

    // Initialize tensor pointers to NULL
    network->neuron_states = NULL;
    network->neuron_metadata = NULL;
    network->synapse_data = NULL;
    network->spike_queue = NULL;

    // Allocate host memory for neurons (64-byte aligned for gpu_neuron_state_t)
    size_t neurons_size = network->neurons_capacity * sizeof(gpu_neuron_state_t);
    // Round up to multiple of 64 for aligned_alloc requirement
    neurons_size = ((neurons_size + 63) / 64) * 64;
    network->neurons_host = (gpu_neuron_state_t*)nimcp_aligned_alloc(64, neurons_size);
    if (!network->neurons_host) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_neural_network_create: network->neurons_host is NULL");
        return NULL;
    }
    // Zero-initialize the aligned memory
    memset(network->neurons_host, 0, neurons_size);

    /**
     * WHAT: Allocate host memory for synapses with 16-byte alignment
     * WHY:  gpu_synapse_t requires 16-byte alignment (Issue #GPU-NEURON-001)
     * HOW:  Use nimcp_aligned_alloc(16, size) instead of nimcp_calloc
     */
    size_t synapses_size = network->synapses_capacity * sizeof(gpu_synapse_t);
    network->synapses_host = (gpu_synapse_t*)nimcp_aligned_alloc(16, synapses_size);
    if (!network->synapses_host) {
        nimcp_aligned_free(network->neurons_host);  // BUGFIX: neurons_host uses aligned_alloc
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_neural_network_create: network->synapses_host is NULL");
        return NULL;
    }
    // Zero-initialize the aligned memory
    memset(network->synapses_host, 0, synapses_size);

    // Try to create GPU context and tensors if CUDA available
    network->using_gpu = false;

    if (config->exec_mode == EXEC_MODE_GPU_CUDA || config->exec_mode == EXEC_MODE_HYBRID) {
        // Create GPU context (handles device selection, streams, etc.)
        network->gpu_ctx = nimcp_gpu_context_create_auto();

        if (network->gpu_ctx && nimcp_gpu_context_is_valid(network->gpu_ctx)) {
            // Create GPU tensors for neuron/synapse storage
            if (create_gpu_tensors(network)) {
                network->using_gpu = true;
                LOG_INFO(LOG_MODULE, "GPU neural network created with tensor-based CUDA acceleration");
                LOG_INFO(LOG_MODULE, "Device: %s", network->gpu_ctx->device_info.name);
            } else {
                // Tensor creation failed, destroy context and fall back
                LOG_WARN(LOG_MODULE, "GPU tensor creation failed, falling back to CPU");
                nimcp_gpu_context_destroy(network->gpu_ctx);
                network->gpu_ctx = NULL;
            }
        } else {
            // Context creation failed
            if (network->gpu_ctx) {
                nimcp_gpu_context_destroy(network->gpu_ctx);
                network->gpu_ctx = NULL;
            }
            LOG_WARN(LOG_MODULE, "GPU context creation failed, falling back to CPU");
        }
    }

    if (!network->using_gpu) {
        // CPU fallback mode
        LOG_INFO(LOG_MODULE, "GPU neural network created with CPU fallback");
    }

    // Bio-async registration
    network->bio_ctx = NULL;
    network->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_SYSTEM_GPU_NEURON,
            .module_name = "gpu_neuron",
            .inbox_capacity = 64,
            .user_data = network
        };
        network->bio_ctx = bio_router_register_module(&bio_info);
        if (network->bio_ctx) {
            network->bio_async_enabled = true;
            LOG_INFO(LOG_MODULE, "Bio-async registered for GPU neuron network");
        } else {
            LOG_WARN(LOG_MODULE, "Bio-async registration failed for GPU neuron network");
        }
    }

    return network;
}

NIMCP_EXPORT void gpu_neural_network_destroy(gpu_neural_network_t network)
{
    if (!network) {
        return;
    }

    LOG_DEBUG(LOG_MODULE, "Destroying GPU neural network");

    // Bio-async unregistration
    if (network->bio_async_enabled && network->bio_ctx) {
        bio_router_unregister_module(network->bio_ctx);
        network->bio_ctx = NULL;
        network->bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for GPU neuron network");
    }

    if (network->using_gpu) {
        // Synchronize GPU before cleanup
        if (network->gpu_ctx) {
            nimcp_gpu_context_synchronize(network->gpu_ctx);
        }

        // Destroy GPU tensors
        destroy_gpu_tensors(network);
        LOG_DEBUG(LOG_MODULE, "GPU tensors destroyed");

        // Destroy GPU context
        if (network->gpu_ctx) {
            nimcp_gpu_context_destroy(network->gpu_ctx);
            network->gpu_ctx = NULL;
        }
        LOG_DEBUG(LOG_MODULE, "GPU context destroyed");
    }

    // Free host memory
    nimcp_aligned_free(network->neurons_host);
    nimcp_aligned_free(network->synapses_host);  // BUGFIX: synapses_host uses aligned_alloc
    nimcp_free(network);
    LOG_DEBUG(LOG_MODULE, "GPU neural network destroyed");
}

//=============================================================================
// Neuron/Synapse Operations
//=============================================================================

NIMCP_EXPORT uint32_t gpu_neural_network_add_neuron(
    gpu_neural_network_t network,
    const gpu_neuron_state_t* initial_state)
{
    if (!network || !initial_state) {
        return UINT32_MAX;
    }

    if (network->neurons_count >= network->neurons_capacity) {
        return UINT32_MAX;  // No space
    }

    uint32_t neuron_id = network->neurons_count;
    network->neurons_host[neuron_id] = *initial_state;
    network->neurons_host[neuron_id].neuron_id = neuron_id;
    network->neurons_count++;

    // Mark tensors as dirty so GPU gets updated before next compute
    if (network->using_gpu) {
        network->tensors_dirty = true;
    }

    return neuron_id;
}

NIMCP_EXPORT bool gpu_neural_network_add_synapse(
    gpu_neural_network_t network,
    uint32_t source_id,
    uint32_t target_id,
    float weight,
    float strength)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_neural_network_add_synapse: network is NULL");
        return false;
    }

    if (source_id >= network->neurons_count || target_id >= network->neurons_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gpu_neural_network_add_synapse: capacity exceeded");
        return false;
    }

    if (network->synapses_count >= network->synapses_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gpu_neural_network_add_synapse: capacity exceeded");
        return false;
    }

    uint32_t synapse_idx = network->synapses_count;
    network->synapses_host[synapse_idx].source_id = source_id;
    network->synapses_host[synapse_idx].target_id = target_id;
    network->synapses_host[synapse_idx].weight = weight;
    network->synapses_host[synapse_idx].strength = strength;
    network->synapses_count++;

    // Update target neuron's incoming synapse count
    network->neurons_host[target_id].num_incoming++;

    // Mark tensors as dirty so GPU gets updated before next compute
    if (network->using_gpu) {
        network->tensors_dirty = true;
    }

    return true;
}

//=============================================================================
// Simulation (CPU Fallback Implementation)
//=============================================================================

/**
 * @brief CPU implementation of neuron update
 */
static uint32_t cpu_update_neurons(
    gpu_neural_network_t network,
    uint64_t timestamp,
    uint64_t delta_t)
{
    uint32_t spike_count = 0;

    for (uint32_t i = 0; i < network->neurons_count; i++) {
        gpu_neuron_state_t* neuron = &network->neurons_host[i];

        // Check refractory period
        if (neuron->last_spike > 0 &&
            (timestamp - neuron->last_spike) < neuron->refractory_period) {
            continue;
        }

        // Compute synaptic input
        float synaptic_input = 0.0F;
        for (uint32_t s = 0; s < network->synapses_count; s++) {
            gpu_synapse_t* syn = &network->synapses_host[s];
            if (syn->target_id == i) {
                gpu_neuron_state_t* pre = &network->neurons_host[syn->source_id];
                if (pre->state > pre->threshold) {
                    synaptic_input += pre->state * syn->weight * syn->strength;
                }
            }
        }

        // Update membrane potential
        float potential = neuron->bias + synaptic_input;
        potential *= (1.0F + neuron->calcium_concentration);

        // Store old state
        float old_state = neuron->state;

        // Update state with leaky integration
        float decay = expf(-(float)delta_t / 20000.0F);  // 20ms time constant
        if (!isfinite(decay)) decay = 0.0F;
        neuron->membrane_potential = neuron->membrane_potential * decay + potential * (1.0F - decay);
        if (!isfinite(neuron->membrane_potential)) neuron->membrane_potential = potential;
        neuron->state = neuron->membrane_potential;

        // Check for spike
        if (old_state <= neuron->threshold && neuron->state > neuron->threshold) {
            neuron->last_spike = timestamp;
            neuron->spike_count++;
            spike_count++;

            // Update calcium concentration
            neuron->calcium_concentration += 0.1F;
        }

        // Decay calcium
        neuron->calcium_concentration *= 0.99F;
    }

    return spike_count;
}

/**
 * @brief GPU kernel launch wrapper for neuron update
 *
 * WHAT: Launches CUDA kernel with tensor data pointers
 * WHY:  Provides 10-100x speedup over CPU
 * HOW:  Extracts raw pointers from tensors, calls kernel
 */
#ifdef NIMCP_ENABLE_CUDA
static uint32_t gpu_update_neurons_tensor(
    gpu_neural_network_t network,
    uint64_t timestamp,
    uint64_t delta_t)
{
    if (!network || !network->gpu_ctx || !network->neuron_states) {
        return 0;
    }

    // Sync host data to GPU if dirty
    if (network->tensors_dirty) {
        if (!sync_host_to_gpu_tensors(network)) {
            LOG_ERROR(LOG_MODULE, "Failed to sync host to GPU tensors");
            return cpu_update_neurons(network, timestamp, delta_t);
        }
    }

    // Calculate kernel launch configuration
    uint32_t block_size = network->config.threads_per_block;
    if (block_size == 0) {
        block_size = 256;  // Default
    }
    uint32_t grid_size = (network->neurons_count + block_size - 1) / block_size;

    // Get stream from context
    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(network->gpu_ctx);

    // Get raw device pointers from tensors
    // Note: The kernels expect legacy struct layout, so we need to use the
    // tensor-based kernel that works with the new layout
    float* neuron_states_ptr = (float*)network->neuron_states->data;
    uint32_t* neuron_meta_ptr = (uint32_t*)network->neuron_metadata->data;
    float* synapse_ptr = (float*)network->synapse_data->data;
    float* spike_queue_ptr = network->spike_queue ? (float*)network->spike_queue->data : NULL;

    // Launch the tensor-based update kernel
    // The kernel is defined in nimcp_gpu_kernels.cu
    extern void launch_kernel_update_neurons_tensor(
        float* neuron_states,
        uint32_t* neuron_meta,
        float* synapses,
        float* spike_queue,
        uint32_t num_neurons,
        uint32_t num_synapses,
        uint32_t neuron_stride,
        uint32_t meta_stride,
        uint32_t synapse_stride,
        uint64_t timestamp,
        uint64_t delta_t,
        uint32_t grid_size,
        uint32_t block_size,
        cudaStream_t cuda_stream);

    launch_kernel_update_neurons_tensor(
        neuron_states_ptr,
        neuron_meta_ptr,
        synapse_ptr,
        spike_queue_ptr,
        network->neurons_count,
        network->synapses_count,
        NEURON_FIELD_COUNT,
        NEURON_INT_FIELD_COUNT,
        SYNAPSE_FIELD_COUNT,
        timestamp,
        delta_t,
        grid_size,
        block_size,
        stream
    );

    // Synchronize to get spike count
    nimcp_gpu_stream_synchronize(network->gpu_ctx);

    // Sync results back to host
    if (!sync_gpu_to_host_tensors(network)) {
        LOG_WARN(LOG_MODULE, "Failed to sync GPU results back to host");
    }

    // Count spikes from host data
    uint32_t spike_count = 0;
    for (uint32_t i = 0; i < network->neurons_count; i++) {
        if (network->neurons_host[i].last_spike == timestamp) {
            spike_count++;
        }
    }

    return spike_count;
}
#endif

NIMCP_EXPORT uint32_t gpu_neural_network_update(
    gpu_neural_network_t network,
    uint64_t timestamp,
    uint64_t delta_t)
{
    if (!network) {
        return 0;
    }

    // Process pending bio-async messages
    if (network->bio_async_enabled && network->bio_ctx) {
        bio_router_process_inbox(network->bio_ctx, 5);
    }

    uint32_t spike_count = 0;

#ifdef NIMCP_ENABLE_CUDA
    if (network->using_gpu && network->gpu_ctx && network->neuron_states) {
        // Launch GPU kernel with tensor data
        spike_count = gpu_update_neurons_tensor(network, timestamp, delta_t);
    } else
#endif
    {
        // CPU fallback
        spike_count = cpu_update_neurons(network, timestamp, delta_t);
    }

    network->total_spikes += spike_count;
    network->total_updates++;

    return spike_count;
}

NIMCP_EXPORT uint32_t gpu_neural_network_apply_stdp(
    gpu_neural_network_t network,
    uint64_t timestamp)
{
    if (!network || !network->config.enable_stdp) {
        return 0;
    }

    (void)timestamp;  // Not used in basic implementation

    // Simple STDP implementation
    uint32_t modified_count = 0;
    float learning_rate = network->config.global_learning_rate;

    for (uint32_t i = 0; i < network->synapses_count; i++) {
        gpu_synapse_t* syn = &network->synapses_host[i];
        gpu_neuron_state_t* pre = &network->neurons_host[syn->source_id];
        gpu_neuron_state_t* post = &network->neurons_host[syn->target_id];

        if (pre->last_spike > 0 && post->last_spike > 0) {
            int64_t delta_t = (int64_t)post->last_spike - (int64_t)pre->last_spike;

            if (delta_t > 0 && delta_t < 20000) {  // Pre before post (LTP)
                syn->weight += learning_rate * expf(-(float)delta_t / 20000.0F);
                modified_count++;
            } else if (delta_t < 0 && delta_t > -20000) {  // Post before pre (LTD)
                syn->weight -= learning_rate * expf((float)delta_t / 20000.0F);
                modified_count++;
            }

            // Clamp weights
            if (syn->weight < 0.0F) syn->weight = 0.0F;
            if (syn->weight > 1.0F) syn->weight = 1.0F;
        }
    }

    return modified_count;
}

NIMCP_EXPORT bool gpu_neural_network_synchronize(gpu_neural_network_t network)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_neural_network_synchronize: network is NULL");
        return false;
    }

    if (network->using_gpu && network->gpu_ctx) {
        int err = nimcp_gpu_context_synchronize(network->gpu_ctx);
        return (err == 0);
    }

    return true;  // CPU mode always synchronized
}

//=============================================================================
// Data Access
//=============================================================================

NIMCP_EXPORT bool gpu_neural_network_get_neuron_state(
    gpu_neural_network_t network,
    uint32_t neuron_id,
    gpu_neuron_state_t* state)
{
    if (!network || !state || neuron_id >= network->neurons_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_neural_network_get_neuron_state: required parameter is NULL (network, state)");
        return false;
    }

    *state = network->neurons_host[neuron_id];
    return true;
}

NIMCP_EXPORT bool gpu_neural_network_set_neuron_state(
    gpu_neural_network_t network,
    uint32_t neuron_id,
    const gpu_neuron_state_t* state)
{
    if (!network || !state || neuron_id >= network->neurons_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_neural_network_set_neuron_state: required parameter is NULL (network, state)");
        return false;
    }

    network->neurons_host[neuron_id] = *state;
    return true;
}

NIMCP_EXPORT uint32_t gpu_neural_network_get_all_states(
    gpu_neural_network_t network,
    gpu_neuron_state_t* states,
    uint32_t max_neurons)
{
    if (!network || !states) {
        return 0;
    }

    uint32_t count = (max_neurons < network->neurons_count) ? max_neurons : network->neurons_count;
    memcpy(states, network->neurons_host, count * sizeof(gpu_neuron_state_t));

    return count;
}

//=============================================================================
// Statistics
//=============================================================================

NIMCP_EXPORT bool gpu_neural_network_get_stats(
    gpu_neural_network_t network,
    uint64_t* total_spikes,
    float* avg_firing_rate,
    uint64_t* gpu_memory_used)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_neural_network_get_stats: network is NULL");
        return false;
    }

    if (total_spikes) {
        *total_spikes = network->total_spikes;
    }

    if (avg_firing_rate) {
        if (network->total_updates > 0 && network->neurons_count > 0) {
            *avg_firing_rate = (float)network->total_spikes /
                              (float)(network->total_updates * network->neurons_count);
        } else {
            *avg_firing_rate = 0.0F;
        }
    }

    if (gpu_memory_used) {
        *gpu_memory_used = 0;
        if (network->using_gpu && network->gpu_ctx) {
            // Calculate memory from tensor sizes
            size_t mem = 0;

            // Neuron states tensor: [N x NEURON_FIELD_COUNT] * sizeof(float)
            if (network->neuron_states) {
                mem += network->neuron_states->numel * network->neuron_states->elem_size;
            }

            // Neuron metadata tensor: [N x NEURON_INT_FIELD_COUNT] * sizeof(uint32_t)
            if (network->neuron_metadata) {
                mem += network->neuron_metadata->numel * network->neuron_metadata->elem_size;
            }

            // Synapse tensor: [S x SYNAPSE_FIELD_COUNT] * sizeof(float)
            if (network->synapse_data) {
                mem += network->synapse_data->numel * network->synapse_data->elem_size;
            }

            // Spike queue tensor
            if (network->spike_queue) {
                mem += network->spike_queue->numel * network->spike_queue->elem_size;
            }

            *gpu_memory_used = (uint64_t)mem;
        }
    }

    return true;
}
