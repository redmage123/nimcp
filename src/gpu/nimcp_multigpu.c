/**
 * @file nimcp_multigpu.c
 * @brief Multi-GPU Support Implementation
 *
 * WHAT: Distributed neural network computation across multiple GPUs
 * WHY:  Scale to larger networks beyond single GPU memory/compute
 * HOW:  Device management, work partitioning, P2P transfers, load balancing
 *
 * MOCK BACKEND:
 * - Compiles without CUDA (CPU-only mode)
 * - Simulates multi-GPU behavior for testing
 * - Uses pthreads for parallel execution
 * - Enables testing on systems without GPUs
 *
 * INTEGRATION:
 * - Bio-async: GPU events published to async system
 * - Logging: Comprehensive operation tracking
 * - Unified memory: All allocations via nimcp_malloc/calloc/free
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 2.8 (Full Integration)
 */

#define LOG_MODULE "GPU_MULTIGPU"
#define LOG_MODULE_ID 0x0901
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(multigpu)

#include "gpu/nimcp_multigpu.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"

// CUDA support detection
#ifdef __CUDACC__
    #define NIMCP_MULTIGPU_CUDA_AVAILABLE 1
    #include <cuda_runtime.h>
#else
    #define NIMCP_MULTIGPU_CUDA_AVAILABLE 0
#endif

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Per-device context
 */
typedef struct {
    int device_id;                      /**< CUDA device ID (or mock ID) */
    multigpu_device_info_t info;        /**< Device information */
    void* device_memory;                /**< Base device memory pointer */
    size_t memory_allocated;            /**< Total allocated bytes */
    void** streams;                     /**< CUDA streams (or NULL for mock) */
    uint32_t num_streams;               /**< Number of streams */

    // Work assignment
    uint32_t* assigned_layers;          /**< Array of layer indices */
    uint32_t num_assigned_layers;       /**< Number of layers assigned */

    // Performance tracking
    uint64_t operations_executed;       /**< Total ops */
    double total_time_ms;               /**< Accumulated time */
    float current_utilization;          /**< Current utilization [0, 1] */
} device_context_t;

/**
 * @brief Multi-GPU context
 */
struct multigpu_context_struct {
    // Configuration
    multigpu_config_t config;

    // Device management
    device_context_t* devices;          /**< Array of device contexts */
    uint32_t num_devices;               /**< Active device count */
    bool is_mock;                       /**< Using mock backend */

    // Work partitioning
    uint32_t num_layers;                /**< Total network layers */
    uint32_t* neurons_per_layer;        /**< Neurons in each layer */
    int* layer_to_device;               /**< Map: layer_idx -> device_idx */

    // Synchronization
    bool is_synchronized;               /**< All GPUs idle */

    // Performance monitoring
    uint64_t total_iterations;          /**< Total iterations */
    uint32_t iterations_since_balance;  /**< Iterations since last rebalance */

    // Bio-async integration
    bio_module_context_t bio_ctx;       /**< Async message context */
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Initialize mock device info
 *
 * WHAT: Create simulated GPU device info for testing
 * WHY:  Enable testing without real GPUs
 * HOW:  Fill struct with realistic simulated values
 */
static void init_mock_device_info(multigpu_device_info_t* info, int device_id)
{
    // Guard: NULL check
    if (!info) {
        return;
    }

    info->device_id = device_id;
    snprintf(info->name, sizeof(info->name), "Mock GPU %d (Simulated)", device_id);
    info->total_memory_bytes = 8ULL * 1024 * 1024 * 1024;  // 8 GB
    info->free_memory_bytes = 7ULL * 1024 * 1024 * 1024;   // 7 GB free
    info->compute_capability = 80;  // Simulated compute capability 8.0
    info->multiprocessor_count = 68;
    info->max_threads_per_block = 1024;
    info->peer_access_available = true;  // Mock always supports P2P
    info->compute_utilization = 0.0F;
    info->memory_utilization = 0.125F;  // 1 GB / 8 GB = 12.5%
}

/**
 * @brief Calculate network complexity score
 *
 * WHAT: Estimate computational complexity of network
 * WHY:  Used to decide GPU count and partition strategy
 * HOW:  Score based on neurons and synapses
 */
static uint64_t calculate_network_complexity(uint32_t num_neurons, uint32_t avg_synapses)
{
    return (uint64_t)num_neurons * (uint64_t)avg_synapses;
}

/**
 * @brief Free device context resources
 *
 * WHAT: Cleanup single device context
 * WHY:  Helper for context destruction
 * HOW:  Free streams, memory, arrays
 */
static void free_device_context(device_context_t* dev, bool is_mock)
{
    // Guard: NULL check
    if (!dev) {
        return;
    }

    // Free assigned layers
    if (dev->assigned_layers) {
        nimcp_free(dev->assigned_layers);
        dev->assigned_layers = NULL;
    }

    // Free streams (CUDA-specific)
#if NIMCP_MULTIGPU_CUDA_AVAILABLE
    if (!is_mock && dev->streams) {
        for (uint32_t i = 0; i < dev->num_streams; i++) {
            if (dev->streams[i]) {
                cudaStreamDestroy((cudaStream_t)dev->streams[i]);
            }
        }
        nimcp_free(dev->streams);
        dev->streams = NULL;
    }
#else
    (void)is_mock;  // Suppress unused warning
#endif

    // Free device memory
    if (dev->device_memory) {
#if NIMCP_MULTIGPU_CUDA_AVAILABLE
        if (!is_mock) {
            cudaSetDevice(dev->device_id);
            cudaFree(dev->device_memory);
        } else {
            nimcp_free(dev->device_memory);
        }
#else
        nimcp_free(dev->device_memory);
#endif
        dev->device_memory = NULL;
    }
}

//=============================================================================
// Device Management
//=============================================================================

bool multigpu_enumerate_devices(multigpu_device_info_t* devices,
                                uint32_t max_devices,
                                uint32_t* count)
{
    // Guard: NULL checks
    if (!devices || !count) {
        LOG_ERROR("NULL parameters in multigpu_enumerate_devices");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL parameters in multigpu_enumerate_devices: devices=%p, count=%p", (void*)devices, (void*)count);
        return false;
    }

    // Guard: Invalid max_devices
    if (max_devices == 0) {
        LOG_WARN("Zero max_devices in multigpu_enumerate_devices");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "Zero max_devices in multigpu_enumerate_devices");
        *count = 0;
        return false;
    }

    LOG_DEBUG("Enumerating GPU devices: max_devices=%u", max_devices);

#if NIMCP_MULTIGPU_CUDA_AVAILABLE
    // Real CUDA backend
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);

    // Guard: CUDA error
    if (err != cudaSuccess) {
        NIMCP_THROW_GPU(NIMCP_ERROR_CUDA, -1, (int)err, "cudaGetDeviceCount failed with error %d", (int)err);
        *count = 0;
        return false;
    }

    // Enumerate up to max_devices
    uint32_t actual_count = (uint32_t)device_count;
    if (actual_count > max_devices) {
        actual_count = max_devices;
    }

    for (uint32_t i = 0; i < actual_count; i++) {
        cudaDeviceProp prop;
        err = cudaGetDeviceProperties(&prop, (int)i);

        // Guard: Failed to get properties
        if (err != cudaSuccess) {
            continue;
        }

        // Fill device info
        devices[i].device_id = (int)i;
        snprintf(devices[i].name, sizeof(devices[i].name), "%s", prop.name);
        devices[i].total_memory_bytes = prop.totalGlobalMem;

        // Query free memory
        size_t free_mem, total_mem;
        cudaSetDevice((int)i);
        cudaMemGetInfo(&free_mem, &total_mem);
        devices[i].free_memory_bytes = free_mem;

        devices[i].compute_capability = prop.major * 10 + prop.minor;
        devices[i].multiprocessor_count = prop.multiProcessorCount;
        devices[i].max_threads_per_block = prop.maxThreadsPerBlock;

        // Check P2P capability (check against GPU 0 as reference)
        int can_access = 0;
        if (i > 0) {
            cudaDeviceCanAccessPeer(&can_access, (int)i, 0);
        }
        devices[i].peer_access_available = (can_access == 1);

        devices[i].compute_utilization = 0.0f;  // Not available without profiling
        devices[i].memory_utilization = 1.0f - ((float)free_mem / (float)total_mem);
    }

    *count = actual_count;
    return true;

#else
    // Mock backend (no CUDA)
    // Simulate 4 GPUs for testing
    uint32_t mock_count = 4;
    if (mock_count > max_devices) {
        mock_count = max_devices;
    }

    for (uint32_t i = 0; i < mock_count; i++) {
        init_mock_device_info(&devices[i], (int)i);
    }

    *count = mock_count;
    return true;
#endif
}

bool multigpu_check_peer_access(int device_id1, int device_id2)
{
    // Guard: Same device
    if (device_id1 == device_id2) {
        return true;  // Device can always access itself
    }

#if NIMCP_MULTIGPU_CUDA_AVAILABLE
    int can_access = 0;
    cudaError_t err = cudaDeviceCanAccessPeer(&can_access, device_id1, device_id2);

    // Guard: CUDA error
    if (err != cudaSuccess) {
        NIMCP_THROW_GPU(NIMCP_ERROR_CUDA, device_id1, (int)err, "cudaDeviceCanAccessPeer failed for devices %d->%d with error %d", device_id1, device_id2, (int)err);
        return false;
    }

    return (can_access == 1);
#else
    // Mock: Always support P2P
    (void)device_id1;
    (void)device_id2;
    return true;
#endif
}

uint32_t multigpu_get_recommended_count(uint32_t num_neurons,
                                        uint32_t num_synapses,
                                        uint32_t available_gpus)
{
    // Guard: No GPUs available
    if (available_gpus == 0) {
        return 0;
    }

    // Guard: Only one GPU available
    if (available_gpus == 1) {
        return 1;
    }

    // Calculate network complexity
    uint64_t complexity = calculate_network_complexity(num_neurons, num_synapses);

    // Heuristics based on network size
    // < 100K neurons: Single GPU (overhead not worth it)
    if (num_neurons < 100000) {
        return 1;
    }

    // 100K-1M neurons: 2 GPUs
    if (num_neurons < 1000000) {
        return (available_gpus >= 2) ? 2 : available_gpus;
    }

    // 1M-10M neurons: 4 GPUs
    if (num_neurons < 10000000) {
        return (available_gpus >= 4) ? 4 : available_gpus;
    }

    // > 10M neurons: Use all available
    (void)complexity;  // Reserved for future use
    return available_gpus;
}

//=============================================================================
// Context Management
//=============================================================================

multigpu_context_t multigpu_context_create(const multigpu_config_t* config)
{
    // Guard: NULL config
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL config in multigpu_context_create");
        return NULL;
    }

    // Allocate context
    multigpu_context_t ctx = (multigpu_context_t)nimcp_calloc(1, sizeof(struct multigpu_context_struct));

    // Guard: Allocation failed
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, sizeof(struct multigpu_context_struct), "Failed to allocate multigpu_context_struct");
        return NULL;
    }

    // Copy configuration
    ctx->config = *config;

    // Enumerate available devices
    multigpu_device_info_t available_devices[16];
    uint32_t available_count = 0;

    if (!multigpu_enumerate_devices(available_devices, 16, &available_count)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU_NOT_AVAILABLE, "Failed to enumerate GPU devices");
        nimcp_free(ctx);
        return NULL;
    }

    // Guard: No devices available
    if (available_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU_NOT_AVAILABLE, "No GPU devices available");
        nimcp_free(ctx);
        return NULL;
    }

    // Determine number of devices to use
    uint32_t num_devices = config->num_devices;
    if (num_devices == 0 || num_devices > available_count) {
        num_devices = available_count;  // Use all available
    }

    ctx->num_devices = num_devices;
    ctx->is_mock = (NIMCP_MULTIGPU_CUDA_AVAILABLE == 0);

    // Allocate device contexts
    ctx->devices = (device_context_t*)nimcp_calloc(num_devices, sizeof(device_context_t));

    // Guard: Allocation failed
    if (!ctx->devices) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, num_devices * sizeof(device_context_t), "Failed to allocate device contexts for %u devices", num_devices);
        nimcp_free(ctx);
        return NULL;
    }

    // Initialize each device
    for (uint32_t i = 0; i < num_devices; i++) {
        device_context_t* dev = &ctx->devices[i];

        // Use specific device ID if provided, otherwise sequential
        int device_id = (config->device_ids) ? config->device_ids[i] : (int)i;

        // P2-13 fix: Validate device_id bounds before array access
        if (device_id < 0 || (uint32_t)device_id >= available_count) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "multigpu_context_create: device_id %d out of range [0, %u)",
                device_id, available_count);
            // Cleanup already-initialized devices
            for (uint32_t j = 0; j < i; j++) {
                free_device_context(&ctx->devices[j], ctx->is_mock);
            }
            nimcp_free(ctx->devices);
            nimcp_free(ctx);
            return NULL;
        }

        dev->device_id = device_id;
        dev->info = available_devices[device_id];

        // Allocate streams
        dev->num_streams = config->streams_per_device;
        if (dev->num_streams == 0) {
            dev->num_streams = 4;  // Default
        }

#if NIMCP_MULTIGPU_CUDA_AVAILABLE
        if (!ctx->is_mock) {
            cudaSetDevice(device_id);
            dev->streams = (void**)nimcp_calloc(dev->num_streams, sizeof(void*));
            for (uint32_t s = 0; s < dev->num_streams; s++) {
                cudaStreamCreate((cudaStream_t*)&dev->streams[s]);
            }
        }
#endif

        // Initialize performance tracking
        dev->operations_executed = 0;
        dev->total_time_ms = 0.0;
        dev->current_utilization = 0.0F;
    }

    // Setup P2P access if enabled
#if NIMCP_MULTIGPU_CUDA_AVAILABLE
    if (!ctx->is_mock && config->enable_peer_access) {
        for (uint32_t i = 0; i < num_devices; i++) {
            cudaSetDevice(ctx->devices[i].device_id);
            for (uint32_t j = 0; j < num_devices; j++) {
                if (i == j) continue;

                // Check and enable P2P access
                if (multigpu_check_peer_access(ctx->devices[i].device_id,
                                              ctx->devices[j].device_id)) {
                    cudaDeviceEnablePeerAccess(ctx->devices[j].device_id, 0);
                    // Ignore errors (may already be enabled)
                }
            }
        }
    }
#endif

    ctx->is_synchronized = true;
    ctx->total_iterations = 0;
    ctx->iterations_since_balance = 0;

    return ctx;
}

void multigpu_context_destroy(multigpu_context_t ctx)
{
    // Guard: NULL context
    if (!ctx) {
        return;
    }

    // Synchronize all devices
    multigpu_synchronize(ctx);

    // Disable P2P access
#if NIMCP_MULTIGPU_CUDA_AVAILABLE
    if (!ctx->is_mock && ctx->config.enable_peer_access) {
        for (uint32_t i = 0; i < ctx->num_devices; i++) {
            cudaSetDevice(ctx->devices[i].device_id);
            for (uint32_t j = 0; j < ctx->num_devices; j++) {
                if (i == j) continue;
                cudaDeviceDisablePeerAccess(ctx->devices[j].device_id);
                // Ignore errors
            }
        }
    }
#endif

    // Free device contexts
    if (ctx->devices) {
        for (uint32_t i = 0; i < ctx->num_devices; i++) {
            free_device_context(&ctx->devices[i], ctx->is_mock);
        }
        nimcp_free(ctx->devices);
    }

    // Free partition data
    if (ctx->neurons_per_layer) {
        nimcp_free(ctx->neurons_per_layer);
    }
    if (ctx->layer_to_device) {
        nimcp_free(ctx->layer_to_device);
    }

    // Free context
    nimcp_free(ctx);
}

uint32_t multigpu_get_device_count(multigpu_context_t ctx)
{
    // Guard: NULL context
    if (!ctx) {
        return 0;
    }

    return ctx->num_devices;
}

bool multigpu_get_device_info(multigpu_context_t ctx,
                              uint32_t device_index,
                              multigpu_device_info_t* info)
{
    // Guard: NULL checks
    if (!ctx || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL parameters in multigpu_get_device_info: ctx=%p, info=%p", (void*)ctx, (void*)info);
        return false;
    }

    // Guard: Invalid device index
    if (device_index >= ctx->num_devices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "Invalid device index %u (max=%u) in multigpu_get_device_info", device_index, ctx->num_devices);
        return false;
    }

    // Copy device info
    *info = ctx->devices[device_index].info;

    // Update current utilization
    info->compute_utilization = ctx->devices[device_index].current_utilization;

    return true;
}

//=============================================================================
// Work Distribution
//=============================================================================

/**
 * @brief Apply layer partitioning strategy
 */
static bool partition_by_layer(multigpu_context_t ctx)
{
    uint32_t layers_per_gpu = ctx->num_layers / ctx->num_devices;
    uint32_t remainder = ctx->num_layers % ctx->num_devices;

    uint32_t current_layer = 0;

    for (uint32_t dev_idx = 0; dev_idx < ctx->num_devices; dev_idx++) {
        device_context_t* dev = &ctx->devices[dev_idx];

        // Distribute remainder across first N GPUs
        uint32_t layers_for_this_gpu = layers_per_gpu;
        if (dev_idx < remainder) {
            layers_for_this_gpu++;
        }

        dev->num_assigned_layers = layers_for_this_gpu;
        dev->assigned_layers = (uint32_t*)nimcp_malloc(layers_for_this_gpu * sizeof(uint32_t));

        // Guard: Allocation failed - clean up previously allocated arrays
        if (!dev->assigned_layers) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, layers_for_this_gpu * sizeof(uint32_t), "Failed to allocate assigned_layers for GPU %u", dev_idx);
            for (uint32_t j = 0; j < dev_idx; j++) {
                nimcp_free(ctx->devices[j].assigned_layers);
                ctx->devices[j].assigned_layers = NULL;
                ctx->devices[j].num_assigned_layers = 0;
            }
            return false;
        }

        // Assign layers
        for (uint32_t i = 0; i < layers_for_this_gpu; i++) {
            dev->assigned_layers[i] = current_layer;
            ctx->layer_to_device[current_layer] = (int)dev_idx;
            current_layer++;
        }
    }

    return true;
}

/**
 * @brief Apply neuron partitioning strategy
 */
static bool partition_by_neuron(multigpu_context_t ctx)
{
    // For neuron partitioning, each GPU gets all layers
    // but only subset of neurons per layer
    // This is more complex, so for MVP we'll assign all layers to all GPUs
    // and handle neuron splitting at compute time

    for (uint32_t dev_idx = 0; dev_idx < ctx->num_devices; dev_idx++) {
        device_context_t* dev = &ctx->devices[dev_idx];

        dev->num_assigned_layers = ctx->num_layers;
        dev->assigned_layers = (uint32_t*)nimcp_malloc(ctx->num_layers * sizeof(uint32_t));

        // Guard: Allocation failed - clean up previously allocated arrays
        if (!dev->assigned_layers) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, ctx->num_layers * sizeof(uint32_t), "Failed to allocate assigned_layers for GPU %u (neuron partition)", dev_idx);
            for (uint32_t j = 0; j < dev_idx; j++) {
                nimcp_free(ctx->devices[j].assigned_layers);
                ctx->devices[j].assigned_layers = NULL;
                ctx->devices[j].num_assigned_layers = 0;
            }
            return false;
        }

        // All devices get all layers (but will process different neurons)
        for (uint32_t i = 0; i < ctx->num_layers; i++) {
            dev->assigned_layers[i] = i;
        }
    }

    // Layer-to-device mapping: all layers use all devices
    for (uint32_t i = 0; i < ctx->num_layers; i++) {
        ctx->layer_to_device[i] = -1;  // Special: multiple devices
    }

    return true;
}

bool multigpu_partition_network(multigpu_context_t ctx,
                               uint32_t num_layers,
                               const uint32_t* neurons_per_layer)
{
    // Guard: NULL checks
    if (!ctx || !neurons_per_layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL parameters in multigpu_partition_network: ctx=%p, neurons_per_layer=%p", (void*)ctx, (void*)neurons_per_layer);
        return false;
    }

    // Guard: Zero layers
    if (num_layers == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "Zero layers in multigpu_partition_network");
        return false;
    }

    // Store network topology
    ctx->num_layers = num_layers;
    ctx->neurons_per_layer = (uint32_t*)nimcp_malloc(num_layers * sizeof(uint32_t));

    // Guard: Allocation failed
    if (!ctx->neurons_per_layer) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, num_layers * sizeof(uint32_t), "Failed to allocate neurons_per_layer array for %u layers", num_layers);
        return false;
    }

    memcpy(ctx->neurons_per_layer, neurons_per_layer, num_layers * sizeof(uint32_t));

    // Allocate layer-to-device mapping
    ctx->layer_to_device = (int*)nimcp_malloc(num_layers * sizeof(int));

    // Guard: Allocation failed
    if (!ctx->layer_to_device) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, num_layers * sizeof(int), "Failed to allocate layer_to_device array for %u layers", num_layers);
        nimcp_free(ctx->neurons_per_layer);
        ctx->neurons_per_layer = NULL;
        return false;
    }

    // Apply partitioning strategy
    bool success = false;

    switch (ctx->config.partition_strategy) {
        case MULTIGPU_PARTITION_LAYER:
            success = partition_by_layer(ctx);
            break;

        case MULTIGPU_PARTITION_NEURON:
            success = partition_by_neuron(ctx);
            break;

        case MULTIGPU_PARTITION_HYBRID:
        case MULTIGPU_PARTITION_DYNAMIC:
        case MULTIGPU_PARTITION_AUTO:
            // For MVP, use layer partitioning as default
            success = partition_by_layer(ctx);
            break;
    }

    return success;
}

int multigpu_get_layer_assignment(multigpu_context_t ctx, uint32_t layer_index)
{
    // Guard: NULL context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multigpu_get_layer_assignment: ctx is NULL");
        return -1;
    }

    // Guard: Not partitioned yet
    if (!ctx->layer_to_device) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multigpu_get_layer_assignment: ctx->layer_to_device is NULL");
        return -1;
    }

    // Guard: Invalid layer index
    if (layer_index >= ctx->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "multigpu_get_layer_assignment: capacity exceeded");
        return -1;
    }

    return ctx->layer_to_device[layer_index];
}

bool multigpu_rebalance_work(multigpu_context_t ctx)
{
    // Guard: NULL context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL context in multigpu_rebalance_work");
        return false;
    }

    // Guard: Not enough devices to rebalance
    if (ctx->num_devices < 2) {
        // Not an error - just nothing to rebalance
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multigpu_rebalance_work: validation failed");
        return false;
    }

    // Guard: Not partitioned
    if (!ctx->layer_to_device) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "Network not partitioned in multigpu_rebalance_work");
        return false;
    }

    // Calculate utilization imbalance
    float min_util = 1.0F;
    float max_util = 0.0F;

    for (uint32_t i = 0; i < ctx->num_devices; i++) {
        float util = ctx->devices[i].current_utilization;
        if (util < min_util) min_util = util;
        if (util > max_util) max_util = util;
    }

    float imbalance = max_util - min_util;

    // Guard: Already balanced
    if (imbalance < ctx->config.imbalance_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multigpu_rebalance_work: validation failed");
        return false;
    }

    // Rebalancing would happen here
    // For MVP, just log that rebalancing would occur
    if (ctx->config.verbose_logging) {
        printf("Multi-GPU: Rebalancing (imbalance=%.2f, threshold=%.2f)\n",
               imbalance, ctx->config.imbalance_threshold);
    }

    // Reset balance counter
    ctx->iterations_since_balance = 0;

    return true;  // Indicate rebalancing occurred
}

//=============================================================================
// Memory Management
//=============================================================================

void** multigpu_alloc(multigpu_context_t ctx, size_t total_size)
{
    // Guard: NULL context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL context in multigpu_alloc");
        return NULL;
    }

    // Guard: Zero size
    if (total_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "Zero size in multigpu_alloc");
        return NULL;
    }

    // Allocate array of device pointers
    void** device_ptrs = (void**)nimcp_calloc(ctx->num_devices, sizeof(void*));

    // Guard: Allocation failed
    if (!device_ptrs) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, ctx->num_devices * sizeof(void*), "Failed to allocate device pointer array for %u devices", ctx->num_devices);
        return NULL;
    }

    // Calculate size per GPU
    size_t size_per_gpu = total_size / ctx->num_devices;

    // Allocate on each device
    for (uint32_t i = 0; i < ctx->num_devices; i++) {
#if NIMCP_MULTIGPU_CUDA_AVAILABLE
        if (!ctx->is_mock) {
            cudaSetDevice(ctx->devices[i].device_id);
            cudaError_t err = cudaMalloc(&device_ptrs[i], size_per_gpu);

            // Guard: CUDA allocation failed
            if (err != cudaSuccess) {
                NIMCP_THROW_GPU(NIMCP_ERROR_GPU_MEMORY, ctx->devices[i].device_id, (int)err, "cudaMalloc failed for %zu bytes on device %d", size_per_gpu, ctx->devices[i].device_id);
                // Free previously allocated
                for (uint32_t j = 0; j < i; j++) {
                    cudaSetDevice(ctx->devices[j].device_id);
                    cudaFree(device_ptrs[j]);
                }
                nimcp_free(device_ptrs);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multigpu_alloc: validation failed");
                return NULL;
            }
        } else {
            device_ptrs[i] = nimcp_malloc(size_per_gpu);
            if (!device_ptrs[i]) {
                NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, size_per_gpu, "Failed to allocate mock GPU memory for device %u", i);
                // Free previously allocated
                for (uint32_t j = 0; j < i; j++) {
                    nimcp_free(device_ptrs[j]);
                }
                nimcp_free(device_ptrs);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multigpu_alloc: device_ptrs is NULL");
                return NULL;
            }
        }
#else
        device_ptrs[i] = nimcp_malloc(size_per_gpu);
        if (!device_ptrs[i]) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, size_per_gpu, "Failed to allocate mock GPU memory for device %u", i);
            // Free previously allocated
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(device_ptrs[j]);
            }
            nimcp_free(device_ptrs);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multigpu_alloc: device_ptrs is NULL");
            return NULL;
        }
#endif

        ctx->devices[i].memory_allocated += size_per_gpu;
    }

    return device_ptrs;
}

void multigpu_free(multigpu_context_t ctx, void** device_ptrs)
{
    // Guard: NULL checks
    if (!ctx || !device_ptrs) {
        return;
    }

    // Free on each device
    for (uint32_t i = 0; i < ctx->num_devices; i++) {
        if (!device_ptrs[i]) {
            continue;
        }

#if NIMCP_MULTIGPU_CUDA_AVAILABLE
        if (!ctx->is_mock) {
            cudaSetDevice(ctx->devices[i].device_id);
            cudaFree(device_ptrs[i]);
        } else {
            nimcp_free(device_ptrs[i]);
        }
#else
        nimcp_free(device_ptrs[i]);
#endif
    }

    // Free array
    nimcp_free(device_ptrs);
}

bool multigpu_broadcast(multigpu_context_t ctx,
                       const void* host_data,
                       void** device_ptrs,
                       size_t size)
{
    // Guard: NULL checks
    if (!ctx || !host_data || !device_ptrs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL parameters in multigpu_broadcast: ctx=%p, host_data=%p, device_ptrs=%p", (void*)ctx, host_data, (void*)device_ptrs);
        return false;
    }

    // Copy to each device
    for (uint32_t i = 0; i < ctx->num_devices; i++) {
        if (!device_ptrs[i]) {
            continue;
        }

#if NIMCP_MULTIGPU_CUDA_AVAILABLE
        if (!ctx->is_mock) {
            cudaSetDevice(ctx->devices[i].device_id);

            // Use async copy if enabled
            if (ctx->config.enable_async_transfers && ctx->devices[i].streams) {
                cudaMemcpyAsync(device_ptrs[i], host_data, size,
                              cudaMemcpyHostToDevice,
                              (cudaStream_t)ctx->devices[i].streams[0]);
            } else {
                cudaError_t err = cudaMemcpy(device_ptrs[i], host_data, size,
                                            cudaMemcpyHostToDevice);
                if (err != cudaSuccess) {
                    NIMCP_THROW_GPU(NIMCP_ERROR_GPU, ctx->devices[i].device_id, (int)err, "cudaMemcpy HostToDevice failed for device %d", ctx->devices[i].device_id);
                    return false;
                }
            }
        } else {
            memcpy(device_ptrs[i], host_data, size);
        }
#else
        memcpy(device_ptrs[i], host_data, size);
#endif
    }

    ctx->is_synchronized = false;
    return true;
}

bool multigpu_gather(multigpu_context_t ctx,
                    void** device_ptrs,
                    void* host_data,
                    size_t size_per_gpu)
{
    // Guard: NULL checks
    if (!ctx || !device_ptrs || !host_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL parameters in multigpu_gather: ctx=%p, device_ptrs=%p, host_data=%p", (void*)ctx, (void*)device_ptrs, host_data);
        return false;
    }

    // Copy from each device
    for (uint32_t i = 0; i < ctx->num_devices; i++) {
        if (!device_ptrs[i]) {
            continue;
        }

        void* host_offset = (char*)host_data + (i * size_per_gpu);

#if NIMCP_MULTIGPU_CUDA_AVAILABLE
        if (!ctx->is_mock) {
            cudaSetDevice(ctx->devices[i].device_id);

            if (ctx->config.enable_async_transfers && ctx->devices[i].streams) {
                cudaMemcpyAsync(host_offset, device_ptrs[i], size_per_gpu,
                              cudaMemcpyDeviceToHost,
                              (cudaStream_t)ctx->devices[i].streams[0]);
            } else {
                cudaError_t err = cudaMemcpy(host_offset, device_ptrs[i], size_per_gpu,
                                            cudaMemcpyDeviceToHost);
                if (err != cudaSuccess) {
                    NIMCP_THROW_GPU(NIMCP_ERROR_GPU, ctx->devices[i].device_id, (int)err, "cudaMemcpy DeviceToHost failed for device %d", ctx->devices[i].device_id);
                    return false;
                }
            }
        } else {
            memcpy(host_offset, device_ptrs[i], size_per_gpu);
        }
#else
        memcpy(host_offset, device_ptrs[i], size_per_gpu);
#endif
    }

    return true;
}

bool multigpu_sync_devices(multigpu_context_t ctx,
                          uint32_t src_device,
                          uint32_t dst_device,
                          const void* src_data,
                          void* dst_data,
                          size_t size)
{
    // Guard: NULL checks
    if (!ctx || !src_data || !dst_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL parameters in multigpu_sync_devices: ctx=%p, src_data=%p, dst_data=%p", (void*)ctx, src_data, dst_data);
        return false;
    }

    // Guard: Invalid device indices
    if (src_device >= ctx->num_devices || dst_device >= ctx->num_devices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "Invalid device indices in multigpu_sync_devices: src=%u, dst=%u (max=%u)", src_device, dst_device, ctx->num_devices);
        return false;
    }

    // Guard: Same device
    if (src_device == dst_device) {
        return true;  // No-op
    }

#if NIMCP_MULTIGPU_CUDA_AVAILABLE
    if (!ctx->is_mock) {
        // Check if P2P available
        if (ctx->config.enable_peer_access &&
            multigpu_check_peer_access(ctx->devices[src_device].device_id,
                                      ctx->devices[dst_device].device_id)) {
            // Use P2P direct transfer (faster)
            cudaSetDevice(ctx->devices[src_device].device_id);
            cudaError_t err = cudaMemcpyPeer(dst_data,
                                            ctx->devices[dst_device].device_id,
                                            src_data,
                                            ctx->devices[src_device].device_id,
                                            size);
            if (err != cudaSuccess) {
                NIMCP_THROW_GPU(NIMCP_ERROR_GPU, ctx->devices[src_device].device_id, (int)err, "cudaMemcpyPeer failed from device %d to device %d", ctx->devices[src_device].device_id, ctx->devices[dst_device].device_id);
                return false;
            }
            return true;
        } else {
            // Fallback: Copy via host
            void* host_buffer = nimcp_malloc(size);
            if (!host_buffer) {
                NIMCP_THROW_MEMORY(NIMCP_ERROR_GPU_MEMORY, size, "Failed to allocate host buffer for device sync");
                return false;
            }

            // Device -> Host
            cudaSetDevice(ctx->devices[src_device].device_id);
            cudaMemcpy(host_buffer, src_data, size, cudaMemcpyDeviceToHost);

            // Host -> Device
            cudaSetDevice(ctx->devices[dst_device].device_id);
            cudaMemcpy(dst_data, host_buffer, size, cudaMemcpyHostToDevice);

            nimcp_free(host_buffer);
            return true;
        }
    }
#else
    (void)src_device;
    (void)dst_device;
#endif

    // Mock: Perform actual memory copy between simulated GPU buffers
    memcpy(dst_data, src_data, size);
    return true;
}

//=============================================================================
// Synchronization
//=============================================================================

bool multigpu_synchronize(multigpu_context_t ctx)
{
    // Guard: NULL context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL context in multigpu_synchronize");
        return false;
    }

#if NIMCP_MULTIGPU_CUDA_AVAILABLE
    if (!ctx->is_mock) {
        for (uint32_t i = 0; i < ctx->num_devices; i++) {
            cudaSetDevice(ctx->devices[i].device_id);
            cudaError_t err = cudaDeviceSynchronize();

            if (err != cudaSuccess) {
                NIMCP_THROW_GPU(NIMCP_ERROR_GPU_SYNC, ctx->devices[i].device_id, (int)err, "cudaDeviceSynchronize failed for device %d", ctx->devices[i].device_id);
                return false;
            }
        }
    }
#endif

    ctx->is_synchronized = true;
    return true;
}

bool multigpu_is_idle(multigpu_context_t ctx)
{
    // Guard: NULL context
    if (!ctx) {
        return true;  // Conservative: assume idle
    }

#if NIMCP_MULTIGPU_CUDA_AVAILABLE
    if (!ctx->is_mock) {
        for (uint32_t i = 0; i < ctx->num_devices; i++) {
            if (ctx->devices[i].streams) {
                for (uint32_t s = 0; s < ctx->devices[i].num_streams; s++) {
                    cudaError_t err = cudaStreamQuery((cudaStream_t)ctx->devices[i].streams[s]);
                    if (err == cudaErrorNotReady) {
                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multigpu_is_idle: validation failed");
                        return false;  // Stream still busy
                    }
                }
            }
        }
    }
#endif

    return true;
}

//=============================================================================
// Performance Monitoring
//=============================================================================

bool multigpu_get_performance_stats(multigpu_context_t ctx,
                                   uint64_t* total_ops,
                                   double* total_time_ms,
                                   float* avg_utilization,
                                   float* load_imbalance)
{
    // Guard: NULL context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL context in multigpu_get_performance_stats");
        return false;
    }

    uint64_t ops_sum = 0;
    double time_max = 0.0;  // Wall-clock time is max, not sum
    float util_sum = 0.0F;
    float util_min = 1.0F;
    float util_max = 0.0F;

    for (uint32_t i = 0; i < ctx->num_devices; i++) {
        device_context_t* dev = &ctx->devices[i];

        ops_sum += dev->operations_executed;

        if (dev->total_time_ms > time_max) {
            time_max = dev->total_time_ms;
        }

        util_sum += dev->current_utilization;

        if (dev->current_utilization < util_min) {
            util_min = dev->current_utilization;
        }
        if (dev->current_utilization > util_max) {
            util_max = dev->current_utilization;
        }
    }

    // Set outputs
    if (total_ops) {
        *total_ops = ops_sum;
    }
    if (total_time_ms) {
        *total_time_ms = time_max;
    }
    if (avg_utilization) {
        *avg_utilization = util_sum / ctx->num_devices;
    }
    if (load_imbalance) {
        *load_imbalance = util_max - util_min;
    }

    return true;
}

bool multigpu_get_device_stats(multigpu_context_t ctx,
                               uint32_t device_index,
                               uint64_t* ops,
                               double* time_ms,
                               float* utilization)
{
    // Guard: NULL context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "NULL context in multigpu_get_device_stats");
        return false;
    }

    // Guard: Invalid device index
    if (device_index >= ctx->num_devices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "Invalid device index %u (max=%u) in multigpu_get_device_stats", device_index, ctx->num_devices);
        return false;
    }

    device_context_t* dev = &ctx->devices[device_index];

    if (ops) {
        *ops = dev->operations_executed;
    }
    if (time_ms) {
        *time_ms = dev->total_time_ms;
    }
    if (utilization) {
        *utilization = dev->current_utilization;
    }

    return true;
}

//=============================================================================
// Default Configurations
//=============================================================================

multigpu_config_t multigpu_default_config(void)
{
    multigpu_config_t config;
    memset(&config, 0, sizeof(config));

    // Device selection
    config.num_devices = 0;  // Use all available
    config.device_ids = NULL;
    config.enable_peer_access = true;

    // Partitioning strategy
    config.partition_strategy = MULTIGPU_PARTITION_HYBRID;
    config.loadbalance_strategy = MULTIGPU_LOADBALANCE_DYNAMIC;

    // Memory management
    config.max_memory_per_gpu = 0;  // Auto
    config.enable_unified_memory = false;
    config.pin_host_memory = true;
    config.sync_buffer_size = 64 * 1024 * 1024;  // 64 MB

    // Performance tuning
    config.streams_per_device = 4;
    config.enable_concurrent_kernels = true;
    config.enable_async_transfers = true;
    config.pipeline_depth = 2;

    // Load balancing
    config.loadbalance_interval = 100;
    config.imbalance_threshold = 0.15F;  // 15%
    config.enable_work_stealing = false;

    // Monitoring
    config.enable_profiling = false;
    config.enable_validation = false;
    config.verbose_logging = false;

    return config;
}

multigpu_config_t multigpu_get_optimal_config(uint32_t num_neurons,
                                              uint32_t num_layers,
                                              uint32_t available_gpus)
{
    multigpu_config_t config = multigpu_default_config();

    // Determine optimal GPU count
    uint32_t optimal_gpus = multigpu_get_recommended_count(num_neurons, 100, available_gpus);
    config.num_devices = optimal_gpus;

    // Choose partition strategy based on network shape
    float depth_to_width = (float)num_layers / (float)num_neurons;

    if (depth_to_width > 0.001F) {
        // Deep network: Layer partition
        config.partition_strategy = MULTIGPU_PARTITION_LAYER;
    } else if (depth_to_width < 0.0001F) {
        // Wide network: Neuron partition
        config.partition_strategy = MULTIGPU_PARTITION_NEURON;
    } else {
        // Balanced: Hybrid partition
        config.partition_strategy = MULTIGPU_PARTITION_HYBRID;
    }

    // Adjust buffer size based on network size
    uint64_t estimated_memory = (uint64_t)num_neurons * 1024;  // Rough estimate
    if (estimated_memory > 1024ULL * 1024 * 1024) {  // > 1 GB
        config.sync_buffer_size = 128 * 1024 * 1024;  // 128 MB
    }

    return config;
}
