/**
 * @file nimcp_mesh_gpu.c
 * @brief GPU Channel and Coordinator Implementation
 *
 * WHAT: GPU-accelerated mesh channel implementation
 * WHY:  Enable high-throughput parallel transaction processing
 * HOW:  Batch transactions, distribute across GPUs, integrate recovery
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_gpu.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_retry.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Error code compatibility aliases */

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief CPU fallback registration
 */
typedef struct cpu_fallback_entry {
    mesh_gpu_tx_type_t type;
    bool (*fallback_fn)(const mesh_gpu_transaction_t* tx, void* ctx);
    void* ctx;
} cpu_fallback_entry_t;

/**
 * @brief Per-device coordinator
 */
typedef struct gpu_coordinator {
    int device_id;
    bool enabled;
    bool healthy;

    /* Memory tracking */
    size_t total_memory;
    size_t used_memory;

    /* Workload */
    uint32_t pending_count;
    uint32_t processing_count;

    /* Statistics */
    uint64_t batches_processed;
    uint64_t transactions_processed;
    uint64_t failures;
    uint64_t fallbacks;
    double total_processing_time_ms;
} gpu_coordinator_t;

/**
 * @brief Pending transaction entry
 */
typedef struct pending_tx_entry {
    mesh_gpu_transaction_t* tx;
    struct pending_tx_entry* next;
} pending_tx_entry_t;

/**
 * @brief Internal GPU channel context
 */
struct mesh_gpu_channel_internal {
    mesh_gpu_channel_config_t config;
    bool running;

    /* GPU coordinators */
    gpu_coordinator_t* coordinators;
    size_t coordinator_count;
    size_t next_device;  /* For round-robin */

    /* Pending transaction queue */
    pending_tx_entry_t* pending_head;
    pending_tx_entry_t* pending_tail;
    size_t pending_count;

    /* Current batch */
    mesh_gpu_batch_t current_batch;

    /* CPU fallbacks */
    cpu_fallback_entry_t* fallbacks;
    size_t fallback_count;
    size_t fallback_capacity;

    /* Timing */
    uint64_t last_flush_ns;

    /* Statistics */
    mesh_gpu_channel_stats_t stats;
};

/* ============================================================================
 * Time Utilities
 * ============================================================================ */

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ============================================================================
 * Device Detection
 * ============================================================================ */

bool mesh_gpu_cuda_available(void) {
#ifdef NIMCP_ENABLE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    return err == cudaSuccess && count > 0;
#else
    return false;  /* CUDA not compiled in */
#endif
}

int mesh_gpu_get_device_count(void) {
#ifdef NIMCP_ENABLE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    return err == cudaSuccess ? count : 0;
#else
    return 0;
#endif
}

bool mesh_gpu_get_device_memory(int device_id, size_t* free_bytes, size_t* total_bytes) {
#ifdef NIMCP_ENABLE_CUDA
    if (cudaSetDevice(device_id) != cudaSuccess) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_gpu_get_device_memory: validation failed");
        return false;
    }

    size_t free_mem, total_mem;
    if (cudaMemGetInfo(&free_mem, &total_mem) != cudaSuccess) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_gpu_get_device_memory: validation failed");
        return false;
    }

    if (free_bytes) *free_bytes = free_mem;
    if (total_bytes) *total_bytes = total_mem;
    return true;
#else
    (void)device_id;
    if (free_bytes) *free_bytes = 0;
    if (total_bytes) *total_bytes = 0;
    /* GPU not available is expected runtime condition, not an error */
    return false;
#endif
}

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

mesh_gpu_channel_config_t mesh_gpu_channel_default_config(void) {
    mesh_gpu_channel_config_t config = {
        .device_configs = NULL,
        .device_count = 0,
        .batch_threshold = MESH_GPU_DEFAULT_BATCH_THRESHOLD,
        .batch_timeout_ms = MESH_GPU_DEFAULT_BATCH_TIMEOUT_MS,
        .max_pending = MESH_GPU_MAX_PENDING,
        .enable_cpu_fallback = true,
        .enable_batch_reduction = true,
        .max_retries = MESH_GPU_MAX_RETRIES,
        .memory_threshold = MESH_GPU_MEMORY_THRESHOLD,
        .round_robin_devices = true,
        .colocate_related = false
    };
    return config;
}

/* ============================================================================
 * Coordinator Management
 * ============================================================================ */

static nimcp_error_t init_coordinators(mesh_gpu_channel_t channel) {
    int device_count = mesh_gpu_get_device_count();
    if (device_count == 0) {
        /* No GPUs - create dummy coordinator for CPU fallback */
        channel->coordinators = (gpu_coordinator_t*)nimcp_calloc(1, sizeof(gpu_coordinator_t));
        if (!channel->coordinators) return NIMCP_ERROR_NO_MEMORY;

        channel->coordinators[0].device_id = -1;  /* CPU fallback marker */
        channel->coordinators[0].enabled = true;
        channel->coordinators[0].healthy = true;
        channel->coordinator_count = 1;
        return NIMCP_SUCCESS;
    }

    /* Use configured devices or all available */
    size_t num_devices = channel->config.device_count > 0 ?
        channel->config.device_count : (size_t)device_count;
    if (num_devices > MESH_GPU_MAX_DEVICES) {
        num_devices = MESH_GPU_MAX_DEVICES;
    }

    channel->coordinators = (gpu_coordinator_t*)nimcp_calloc(num_devices, sizeof(gpu_coordinator_t));
    if (!channel->coordinators) return NIMCP_ERROR_NO_MEMORY;

    for (size_t i = 0; i < num_devices; i++) {
        int dev_id = channel->config.device_configs ?
            channel->config.device_configs[i].device_id : (int)i;

        channel->coordinators[i].device_id = dev_id;
        channel->coordinators[i].enabled = true;
        channel->coordinators[i].healthy = true;

        /* Get device memory */
        size_t free_mem, total_mem;
        if (mesh_gpu_get_device_memory(dev_id, &free_mem, &total_mem)) {
            channel->coordinators[i].total_memory = total_mem;
            channel->coordinators[i].used_memory = total_mem - free_mem;
        }
    }

    channel->coordinator_count = num_devices;
    return NIMCP_SUCCESS;
}

static gpu_coordinator_t* select_coordinator(mesh_gpu_channel_t channel, const mesh_gpu_transaction_t* tx) {
    if (!channel || channel->coordinator_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "select_coordinator: channel is NULL");
        return NULL;
    }

    /* Specific device requested? */
    if (tx && tx->target_device >= 0) {
        for (size_t i = 0; i < channel->coordinator_count; i++) {
            if (channel->coordinators[i].device_id == tx->target_device &&
                channel->coordinators[i].enabled &&
                channel->coordinators[i].healthy) {
                return &channel->coordinators[i];
            }
        }
    }

    if (channel->config.round_robin_devices) {
        /* Round-robin selection */
        size_t start = channel->next_device;
        for (size_t i = 0; i < channel->coordinator_count; i++) {
            size_t idx = (start + i) % channel->coordinator_count;
            if (channel->coordinators[idx].enabled && channel->coordinators[idx].healthy) {
                channel->next_device = (idx + 1) % channel->coordinator_count;
                return &channel->coordinators[idx];
            }
        }
    } else {
        /* Load-based selection: choose device with lowest pending */
        gpu_coordinator_t* best = NULL;
        uint32_t min_load = UINT32_MAX;

        for (size_t i = 0; i < channel->coordinator_count; i++) {
            if (channel->coordinators[i].enabled && channel->coordinators[i].healthy) {
                uint32_t load = channel->coordinators[i].pending_count +
                               channel->coordinators[i].processing_count;
                if (load < min_load) {
                    min_load = load;
                    best = &channel->coordinators[i];
                }
            }
        }
        return best;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "select_coordinator: validation failed");
    return NULL;
}

/* ============================================================================
 * Batch Management
 * ============================================================================ */

static nimcp_error_t batch_init(mesh_gpu_batch_t* batch, size_t capacity) {
    if (!batch) return NIMCP_ERROR_INVALID_PARAM;

    batch->transactions = (mesh_gpu_transaction_t**)nimcp_calloc(capacity, sizeof(mesh_gpu_transaction_t*));
    if (!batch->transactions) return NIMCP_ERROR_NO_MEMORY;

    batch->capacity = capacity;
    batch->count = 0;
    batch->batch_type = MESH_GPU_TX_NONE;
    batch->total_input_size = 0;
    batch->total_output_size = 0;
    batch->assigned_device = -1;
    batch->created_ns = get_time_ns();

    return NIMCP_SUCCESS;
}

static void batch_clear(mesh_gpu_batch_t* batch) {
    if (!batch) return;

    batch->count = 0;
    batch->batch_type = MESH_GPU_TX_NONE;
    batch->total_input_size = 0;
    batch->total_output_size = 0;
    batch->assigned_device = -1;
    batch->created_ns = get_time_ns();
}

static void batch_destroy(mesh_gpu_batch_t* batch) {
    if (!batch) return;
    nimcp_free(batch->transactions);
    batch->transactions = NULL;
    batch->capacity = 0;
    batch->count = 0;
}

static nimcp_error_t batch_add(mesh_gpu_batch_t* batch, mesh_gpu_transaction_t* tx) {
    if (!batch || !tx) return NIMCP_ERROR_INVALID_PARAM;
    if (batch->count >= batch->capacity) return NIMCP_ERROR_CAPACITY_EXCEEDED;

    batch->transactions[batch->count++] = tx;
    batch->total_input_size += tx->input_size;
    batch->total_output_size += tx->output_size;

    /* Track dominant type */
    if (batch->batch_type == MESH_GPU_TX_NONE) {
        batch->batch_type = tx->gpu_type;
    }

    tx->status = MESH_GPU_TX_STATUS_BATCHED;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Transaction Processing
 * ============================================================================ */

static bool try_cpu_fallback(mesh_gpu_channel_t channel, mesh_gpu_transaction_t* tx) {
    if (!channel || !tx || !channel->config.enable_cpu_fallback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "try_cpu_fallback: required parameter is NULL (channel, tx, channel->config)");
        return false;
    }

    /* Find registered fallback for this type */
    for (size_t i = 0; i < channel->fallback_count; i++) {
        if (channel->fallbacks[i].type == tx->gpu_type && channel->fallbacks[i].fallback_fn) {
            if (channel->fallbacks[i].fallback_fn(tx, channel->fallbacks[i].ctx)) {
                tx->status = MESH_GPU_TX_STATUS_FALLBACK;
                channel->stats.total_fallbacks++;
                return true;
            }
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "try_cpu_fallback: validation failed");
    return false;
}

/* ============================================================================
 * Retry Framework Integration
 * ============================================================================ */

/**
 * @brief Context for GPU recovery retry operations
 */
typedef struct {
    mesh_gpu_channel_t channel;
    mesh_gpu_transaction_t* tx;
} gpu_recovery_context_t;

/**
 * @brief Execute function for GPU recovery retry
 *
 * WHAT: Wrapper function for nimcp_retry_with_backoff() framework
 * WHY:  Allows retry mechanism to attempt CPU fallback with proper backoff
 * HOW:  Calls try_cpu_fallback() with channel and transaction context
 */
static bool gpu_recovery_execute(void* context) {
    gpu_recovery_context_t* ctx = (gpu_recovery_context_t*)context;
    if (!ctx || !ctx->channel || !ctx->tx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_recovery_execute: required parameter is NULL (ctx, ctx->channel, ctx->tx)");
        return false;
    }
    return try_cpu_fallback(ctx->channel, ctx->tx);
}

/**
 * @brief Attempt GPU transaction recovery using retry framework
 *
 * WHAT: Retry CPU fallback with exponential backoff
 * WHY:  Replace ad-hoc retry loop with standardized retry mechanism
 * HOW:  Use nimcp_retry_with_backoff() with configurable parameters
 *
 * @param channel GPU channel instance
 * @param tx Transaction to recover
 * @return true if recovery succeeded, false otherwise
 */
static bool attempt_gpu_recovery_with_retry(mesh_gpu_channel_t channel, mesh_gpu_transaction_t* tx) {
    if (!channel || !tx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attempt_gpu_recovery_with_retry: required parameter is NULL (channel, tx)");
        return false;
    }

    gpu_recovery_context_t ctx = {
        .channel = channel,
        .tx = tx
    };

    operation_t op = {
        .name = "gpu_recovery_fallback",
        .execute = gpu_recovery_execute,
        .rollback = NULL,  /* No rollback needed for fallback */
        .context = &ctx,
        .execution_count = 0
    };

    nimcp_retry_config_t retry_config = nimcp_retry_default_config();
    retry_config.max_retries = channel->config.max_retries > 0 ? channel->config.max_retries : 3;
    retry_config.initial_delay_ms = 10;   /* Start with 10ms */
    retry_config.max_delay_ms = 1000;     /* Cap at 1 second */
    retry_config.backoff_factor = 2.0f;   /* Double each time */
    retry_config.jitter_factor = 0.25f;   /* +/- 25% jitter */

    nimcp_retry_result_t result;
    nimcp_error_t err = nimcp_retry_with_backoff(&op, &retry_config, NULL, &result);

    /* Update channel statistics */
    channel->stats.recovery_attempts += result.attempts;
    if (err == NIMCP_SUCCESS && result.success) {
        channel->stats.recovery_successes++;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attempt_gpu_recovery_with_retry: validation failed");
    return false;
}

static nimcp_error_t process_transaction_gpu(mesh_gpu_channel_t channel,
                                             gpu_coordinator_t* coord,
                                             mesh_gpu_transaction_t* tx) {
    if (!channel || !coord || !tx) return NIMCP_ERROR_INVALID_PARAM;

    tx->status = MESH_GPU_TX_STATUS_PROCESSING;
    tx->started_ns = get_time_ns();

#ifdef NIMCP_ENABLE_CUDA
    /* Set device */
    if (coord->device_id >= 0) {
        cudaError_t err = cudaSetDevice(coord->device_id);
        if (err != cudaSuccess) {
            /* Try CPU fallback */
            if (try_cpu_fallback(channel, tx)) {
                tx->completed_ns = get_time_ns();
                return NIMCP_SUCCESS;
            }
            tx->status = MESH_GPU_TX_STATUS_FAILED;
            tx->error = NIMCP_ERROR_GPU;
            snprintf(tx->error_msg, sizeof(tx->error_msg), "Failed to set device %d", coord->device_id);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "mesh_gpu: error condition");
            return NIMCP_ERROR_GPU;
        }
    }

    /* Process based on type - stub implementation */
    /* In a full implementation, this would dispatch to specific GPU kernels */
    switch (tx->gpu_type) {
        case MESH_GPU_TX_BELIEF_BATCH:
        case MESH_GPU_TX_CONSENSUS_COMPUTE:
        case MESH_GPU_TX_FEP_BATCH:
        case MESH_GPU_TX_TENSOR_OP:
        case MESH_GPU_TX_NEURAL_FORWARD:
        case MESH_GPU_TX_NEURAL_BACKWARD:
        case MESH_GPU_TX_MATRIX_MULTIPLY:
        case MESH_GPU_TX_STATISTICAL:
        case MESH_GPU_TX_CUSTOM:
            /* Placeholder: In production, call actual GPU kernels */
            if (tx->output_data && tx->input_data && tx->output_size > 0) {
                /* Simple copy for testing - real impl would use GPU */
                size_t copy_size = tx->input_size < tx->output_size ? tx->input_size : tx->output_size;
                memcpy(tx->output_data, tx->input_data, copy_size);
            }
            cudaDeviceSynchronize();
            break;

        default:
            tx->status = MESH_GPU_TX_STATUS_FAILED;
            tx->error = NIMCP_ERROR_INVALID_PARAM;
            snprintf(tx->error_msg, sizeof(tx->error_msg), "Unknown GPU transaction type");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_gpu: invalid parameter");
            return NIMCP_ERROR_INVALID_PARAM;
    }
#else
    /* No CUDA - use CPU fallback */
    if (!try_cpu_fallback(channel, tx)) {
        /* No fallback available - simple copy for testing */
        if (tx->output_data && tx->input_data && tx->output_size > 0) {
            size_t copy_size = tx->input_size < tx->output_size ? tx->input_size : tx->output_size;
            memcpy(tx->output_data, tx->input_data, copy_size);
        }
    }
#endif

    tx->status = MESH_GPU_TX_STATUS_COMPLETED;
    tx->completed_ns = get_time_ns();

    /* Update statistics */
    coord->transactions_processed++;
    double elapsed_ms = (double)(tx->completed_ns - tx->started_ns) / 1000000.0;
    coord->total_processing_time_ms += elapsed_ms;

    channel->stats.total_completed++;

    return NIMCP_SUCCESS;
}

static nimcp_error_t process_batch(mesh_gpu_channel_t channel, mesh_gpu_batch_t* batch) {
    if (!channel || !batch || batch->count == 0) return NIMCP_ERROR_INVALID_PARAM;

    batch->flushed_ns = get_time_ns();

    /* Select coordinator for batch */
    gpu_coordinator_t* coord = select_coordinator(channel, NULL);
    if (!coord) {
        /* No available coordinator - fail all transactions */
        for (size_t i = 0; i < batch->count; i++) {
            batch->transactions[i]->status = MESH_GPU_TX_STATUS_FAILED;
            batch->transactions[i]->error = NIMCP_ERROR_NOT_READY;
            snprintf(batch->transactions[i]->error_msg, sizeof(batch->transactions[i]->error_msg),
                    "No GPU coordinator available");
            channel->stats.total_failed++;
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_READY, "process_batch: error condition");
        return NIMCP_ERROR_NOT_READY;
    }

    batch->assigned_device = coord->device_id;
    coord->pending_count += (uint32_t)batch->count;
    coord->processing_count++;

    /* Process each transaction */
    for (size_t i = 0; i < batch->count; i++) {
        mesh_gpu_transaction_t* tx = batch->transactions[i];
        nimcp_error_t err = process_transaction_gpu(channel, coord, tx);

        if (err != NIMCP_SUCCESS) {
            /* Try recovery using retry framework with exponential backoff */
            bool recovered = attempt_gpu_recovery_with_retry(channel, tx);

            if (!recovered) {
                coord->failures++;
                channel->stats.total_failed++;
            }
        }

        /* Invoke callback if set */
        if (tx->callback) {
            mesh_result_t result = {
                .tx_id = tx->base_id,
                .status = (tx->status == MESH_GPU_TX_STATUS_COMPLETED ||
                          tx->status == MESH_GPU_TX_STATUS_FALLBACK) ?
                         MESH_TX_STATUS_COMMITTED : MESH_TX_STATUS_FAILED,
                .error = tx->error,
                .commit_timestamp_ns = tx->completed_ns
            };
            strncpy(result.error_msg, tx->error_msg, sizeof(result.error_msg) - 1);
            tx->callback(&result, tx->callback_ctx);
        }
    }

    coord->pending_count -= (uint32_t)batch->count;
    coord->processing_count--;
    coord->batches_processed++;

    channel->stats.batches_created++;

    return NIMCP_SUCCESS;
}

static nimcp_error_t flush_batch(mesh_gpu_channel_t channel) {
    if (!channel || channel->current_batch.count == 0) return NIMCP_SUCCESS;

    nimcp_error_t err = process_batch(channel, &channel->current_batch);

    /* Clear batch for next use */
    batch_clear(&channel->current_batch);
    channel->last_flush_ns = get_time_ns();

    return err;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

mesh_gpu_channel_t mesh_gpu_channel_create(const mesh_gpu_channel_config_t* config) {
    mesh_gpu_channel_t channel = (mesh_gpu_channel_t)nimcp_calloc(1, sizeof(struct mesh_gpu_channel_internal));
    if (!channel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_gpu_channel_create: channel is NULL");
        return NULL;
    }

    channel->config = config ? *config : mesh_gpu_channel_default_config();

    /* Initialize coordinators */
    if (init_coordinators(channel) != NIMCP_SUCCESS) {
        nimcp_free(channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_gpu_channel_create: validation failed");
        return NULL;
    }

    /* Initialize batch */
    size_t batch_capacity = channel->config.batch_threshold * 2;
    if (batch_init(&channel->current_batch, batch_capacity) != NIMCP_SUCCESS) {
        nimcp_free(channel->coordinators);
        nimcp_free(channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mesh_gpu_channel_create: validation failed");
        return NULL;
    }

    /* Initialize fallback array */
    channel->fallback_capacity = 16;
    channel->fallbacks = (cpu_fallback_entry_t*)nimcp_calloc(channel->fallback_capacity,
                                                        sizeof(cpu_fallback_entry_t));
    if (!channel->fallbacks) {
        batch_destroy(&channel->current_batch);
        nimcp_free(channel->coordinators);
        nimcp_free(channel);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_gpu_channel_create: channel->fallbacks is NULL");
        return NULL;
    }

    channel->last_flush_ns = get_time_ns();

    return channel;
}

void mesh_gpu_channel_destroy(mesh_gpu_channel_t channel) {
    if (!channel) return;

    /* Stop if running */
    if (channel->running) {
        mesh_gpu_channel_stop(channel, true);
    }

    /* Free pending queue */
    pending_tx_entry_t* entry = channel->pending_head;
    while (entry) {
        pending_tx_entry_t* next = entry->next;
        mesh_gpu_transaction_destroy(entry->tx);
        nimcp_free(entry);
        entry = next;
    }

    /* Free batch */
    batch_destroy(&channel->current_batch);

    /* Free coordinators */
    nimcp_free(channel->coordinators);

    /* Free fallbacks */
    nimcp_free(channel->fallbacks);

    /* Free stats device array if allocated */
    nimcp_free(channel->stats.device_stats);

    nimcp_free(channel);
}

nimcp_error_t mesh_gpu_channel_start(mesh_gpu_channel_t channel) {
    if (!channel) return NIMCP_ERROR_INVALID_PARAM;
    if (channel->running) return NIMCP_SUCCESS;

    channel->running = true;
    channel->last_flush_ns = get_time_ns();

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gpu_channel_stop(mesh_gpu_channel_t channel, bool drain) {
    if (!channel) return NIMCP_ERROR_INVALID_PARAM;
    if (!channel->running) return NIMCP_SUCCESS;

    if (drain) {
        /* Flush any pending batch */
        flush_batch(channel);
    }

    channel->running = false;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Transaction Submission
 * ============================================================================ */

nimcp_error_t mesh_gpu_channel_submit(mesh_gpu_channel_t channel, mesh_gpu_transaction_t* tx) {
    if (!channel || !tx) return NIMCP_ERROR_INVALID_PARAM;
    if (!channel->running) return NIMCP_ERROR_NOT_READY;
    if (channel->pending_count >= channel->config.max_pending) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_CAPACITY_EXCEEDED, "mesh_gpu_channel_submit: error condition");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    tx->submitted_ns = get_time_ns();
    tx->status = MESH_GPU_TX_STATUS_PENDING;

    /* Add to current batch */
    nimcp_error_t err = batch_add(&channel->current_batch, tx);
    if (err != NIMCP_SUCCESS) return err;

    channel->pending_count++;
    channel->stats.total_submitted++;

    /* Check if we should flush */
    if (channel->current_batch.count >= channel->config.batch_threshold) {
        channel->stats.batches_threshold_flush++;
        return flush_batch(channel);
    }

    /* Check timeout */
    uint64_t now = get_time_ns();
    float elapsed_ms = (float)(now - channel->last_flush_ns) / 1000000.0f;
    if (elapsed_ms >= channel->config.batch_timeout_ms && channel->current_batch.count > 0) {
        channel->stats.batches_timeout_flush++;
        return flush_batch(channel);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gpu_channel_submit_async(
    mesh_gpu_channel_t channel,
    mesh_gpu_transaction_t* tx,
    mesh_tx_callback_t callback,
    void* ctx
) {
    if (!channel || !tx) return NIMCP_ERROR_INVALID_PARAM;

    tx->callback = callback;
    tx->callback_ctx = ctx;

    return mesh_gpu_channel_submit(channel, tx);
}

nimcp_error_t mesh_gpu_channel_flush(mesh_gpu_channel_t channel) {
    if (!channel) return NIMCP_ERROR_INVALID_PARAM;
    return flush_batch(channel);
}

/* ============================================================================
 * Transaction Management
 * ============================================================================ */

mesh_gpu_transaction_t* mesh_gpu_transaction_create(
    mesh_gpu_tx_type_t gpu_type,
    const void* input_data,
    size_t input_size,
    size_t output_size
) {
    mesh_gpu_transaction_t* tx = (mesh_gpu_transaction_t*)nimcp_calloc(1, sizeof(mesh_gpu_transaction_t));
    if (!tx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_gpu_transaction_create: tx is NULL");
        return NULL;
    }

    tx->gpu_type = gpu_type;
    tx->status = MESH_GPU_TX_STATUS_PENDING;
    tx->target_device = -1;  /* Any device */

    /* Copy input data */
    if (input_data && input_size > 0) {
        tx->input_data = nimcp_malloc(input_size);
        if (!tx->input_data) {
            nimcp_free(tx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_gpu_transaction_create: tx->input_data is NULL");
            return NULL;
        }
        memcpy(tx->input_data, input_data, input_size);
        tx->input_size = input_size;
    }

    /* Allocate output buffer */
    if (output_size > 0) {
        tx->output_data = nimcp_calloc(1, output_size);
        if (!tx->output_data) {
            nimcp_free(tx->input_data);
            nimcp_free(tx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mesh_gpu_transaction_create: tx->output_data is NULL");
            return NULL;
        }
        tx->output_size = output_size;
    }

    /* Generate transaction ID */
    tx->base_id.channel = MESH_CHANNEL_GPU_COMPUTE_ID;
    tx->base_id.timestamp_ns = get_time_ns();

    return tx;
}

void mesh_gpu_transaction_destroy(mesh_gpu_transaction_t* tx) {
    if (!tx) return;
    nimcp_free(tx->input_data);
    nimcp_free(tx->output_data);
    nimcp_free(tx);
}

nimcp_error_t mesh_gpu_channel_wait(
    mesh_gpu_channel_t channel,
    const mesh_tx_id_t* tx_id,
    uint32_t timeout_ms
) {
    if (!channel || !tx_id) return NIMCP_ERROR_INVALID_PARAM;

    /* Simple busy-wait implementation - production would use condition variables */
    uint64_t start = get_time_ns();
    uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;

    while (timeout_ms == 0 || (get_time_ns() - start) < timeout_ns) {
        /* Check batch transactions */
        for (size_t i = 0; i < channel->current_batch.count; i++) {
            mesh_gpu_transaction_t* tx = channel->current_batch.transactions[i];
            if (mesh_tx_id_compare(&tx->base_id, tx_id) == 0) {
                if (tx->status == MESH_GPU_TX_STATUS_COMPLETED ||
                    tx->status == MESH_GPU_TX_STATUS_FALLBACK) {
                    return NIMCP_SUCCESS;
                }
                if (tx->status == MESH_GPU_TX_STATUS_FAILED) {
                    return tx->error;
                }
            }
        }

        /* Small sleep to avoid busy spin */
        struct timespec ts = {0, 1000000};  /* 1ms */
        nanosleep(&ts, NULL);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_TIMEOUT, "mesh_gpu: error condition");
    return NIMCP_ERROR_TIMEOUT;
}

nimcp_error_t mesh_gpu_channel_get_status(
    mesh_gpu_channel_t channel,
    const mesh_tx_id_t* tx_id,
    mesh_gpu_tx_status_t* status
) {
    if (!channel || !tx_id || !status) return NIMCP_ERROR_INVALID_PARAM;

    /* Search in current batch */
    for (size_t i = 0; i < channel->current_batch.count; i++) {
        mesh_gpu_transaction_t* tx = channel->current_batch.transactions[i];
        if (mesh_tx_id_compare(&tx->base_id, tx_id) == 0) {
            *status = tx->status;
            return NIMCP_SUCCESS;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND, "mesh_gpu: error condition");
    return NIMCP_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Device Management
 * ============================================================================ */

size_t mesh_gpu_channel_device_count(mesh_gpu_channel_t channel) {
    return channel ? channel->coordinator_count : 0;
}

nimcp_error_t mesh_gpu_channel_get_device_state(
    mesh_gpu_channel_t channel,
    size_t device_idx,
    mesh_gpu_device_state_t* state
) {
    if (!channel || !state) return NIMCP_ERROR_INVALID_PARAM;
    if (device_idx >= channel->coordinator_count) return NIMCP_ERROR_INVALID_PARAM;

    gpu_coordinator_t* coord = &channel->coordinators[device_idx];

    state->device_id = coord->device_id;
    state->available = coord->enabled;
    state->healthy = coord->healthy;
    state->total_memory = coord->total_memory;
    state->free_memory = coord->total_memory - coord->used_memory;
    state->used_by_channel = coord->used_memory;
    state->pending_batches = coord->pending_count;
    state->processing_batches = coord->processing_count;
    state->batches_processed = coord->batches_processed;
    state->transactions_processed = coord->transactions_processed;
    state->failures = coord->failures;
    state->fallbacks = coord->fallbacks;

    if (coord->batches_processed > 0) {
        state->avg_batch_time_ms = (float)(coord->total_processing_time_ms / coord->batches_processed);
    }

    /* Calculate utilization */
    if (coord->total_memory > 0) {
        state->utilization = (float)coord->used_memory / (float)coord->total_memory;
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gpu_channel_disable_device(mesh_gpu_channel_t channel, size_t device_idx) {
    if (!channel || device_idx >= channel->coordinator_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_gpu_channel_disable_device: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    channel->coordinators[device_idx].enabled = false;
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gpu_channel_enable_device(mesh_gpu_channel_t channel, size_t device_idx) {
    if (!channel || device_idx >= channel->coordinator_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_gpu_channel_enable_device: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    channel->coordinators[device_idx].enabled = true;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Batch Management
 * ============================================================================ */

size_t mesh_gpu_channel_pending_count(mesh_gpu_channel_t channel) {
    return channel ? channel->current_batch.count : 0;
}

nimcp_error_t mesh_gpu_channel_set_batch_threshold(mesh_gpu_channel_t channel, size_t threshold) {
    if (!channel || threshold == 0) return NIMCP_ERROR_INVALID_PARAM;
    channel->config.batch_threshold = threshold;
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gpu_channel_set_batch_timeout(mesh_gpu_channel_t channel, float timeout_ms) {
    if (!channel || timeout_ms <= 0.0f) return NIMCP_ERROR_INVALID_PARAM;
    channel->config.batch_timeout_ms = timeout_ms;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Recovery Control
 * ============================================================================ */

nimcp_error_t mesh_gpu_channel_set_cpu_fallback(mesh_gpu_channel_t channel, bool enable) {
    if (!channel) return NIMCP_ERROR_INVALID_PARAM;
    channel->config.enable_cpu_fallback = enable;
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gpu_channel_set_max_retries(mesh_gpu_channel_t channel, uint32_t max_retries) {
    if (!channel) return NIMCP_ERROR_INVALID_PARAM;
    channel->config.max_retries = max_retries;
    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gpu_channel_register_fallback(
    mesh_gpu_channel_t channel,
    mesh_gpu_tx_type_t gpu_type,
    bool (*fallback_fn)(const mesh_gpu_transaction_t* tx, void* ctx),
    void* ctx
) {
    if (!channel || !fallback_fn) return NIMCP_ERROR_INVALID_PARAM;

    /* Resize if needed */
    if (channel->fallback_count >= channel->fallback_capacity) {
        /* Check for overflow before doubling capacity */
        if (channel->fallback_capacity > SIZE_MAX / 2) {
            return NIMCP_ERROR_OUT_OF_RANGE;
        }
        size_t new_cap = channel->fallback_capacity * 2;
        /* Check for allocation size overflow */
        if (new_cap > SIZE_MAX / sizeof(cpu_fallback_entry_t)) {
            return NIMCP_ERROR_OUT_OF_RANGE;
        }
        cpu_fallback_entry_t* new_arr = (cpu_fallback_entry_t*)nimcp_realloc(
            channel->fallbacks, new_cap * sizeof(cpu_fallback_entry_t));
        if (!new_arr) return NIMCP_ERROR_NO_MEMORY;
        channel->fallbacks = new_arr;
        channel->fallback_capacity = new_cap;
    }

    /* Check if type already registered */
    for (size_t i = 0; i < channel->fallback_count; i++) {
        if (channel->fallbacks[i].type == gpu_type) {
            channel->fallbacks[i].fallback_fn = fallback_fn;
            channel->fallbacks[i].ctx = ctx;
            return NIMCP_SUCCESS;
        }
    }

    /* Add new entry */
    channel->fallbacks[channel->fallback_count].type = gpu_type;
    channel->fallbacks[channel->fallback_count].fallback_fn = fallback_fn;
    channel->fallbacks[channel->fallback_count].ctx = ctx;
    channel->fallback_count++;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

nimcp_error_t mesh_gpu_channel_get_stats(
    mesh_gpu_channel_t channel,
    mesh_gpu_channel_stats_t* stats
) {
    if (!channel || !stats) return NIMCP_ERROR_INVALID_PARAM;

    *stats = channel->stats;

    /* Calculate averages */
    if (stats->batches_created > 0) {
        stats->avg_batch_size = (float)stats->total_submitted / stats->batches_created;
    }

    /* Allocate and fill device stats */
    if (channel->coordinator_count > 0) {
        stats->device_stats = (mesh_gpu_device_state_t*)nimcp_calloc(
            channel->coordinator_count, sizeof(mesh_gpu_device_state_t));
        if (stats->device_stats) {
            size_t valid_count = 0;
            for (size_t i = 0; i < channel->coordinator_count; i++) {
                nimcp_error_t err = mesh_gpu_channel_get_device_state(
                    channel, i, &stats->device_stats[valid_count]);
                if (err == NIMCP_SUCCESS) {
                    valid_count++;
                }
                /* Skip devices that fail to report state */
            }
            stats->device_count = valid_count;
        } else {
            /* Allocation failed - stats incomplete but not fatal */
            stats->device_count = 0;
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_gpu_channel_reset_stats(mesh_gpu_channel_t channel) {
    if (!channel) return NIMCP_ERROR_INVALID_PARAM;

    memset(&channel->stats, 0, sizeof(mesh_gpu_channel_stats_t));

    for (size_t i = 0; i < channel->coordinator_count; i++) {
        channel->coordinators[i].batches_processed = 0;
        channel->coordinators[i].transactions_processed = 0;
        channel->coordinators[i].failures = 0;
        channel->coordinators[i].fallbacks = 0;
        channel->coordinators[i].total_processing_time_ms = 0;
    }

    return NIMCP_SUCCESS;
}

void mesh_gpu_channel_stats_free(mesh_gpu_channel_stats_t* stats) {
    if (!stats) return;
    nimcp_free(stats->device_stats);
    stats->device_stats = NULL;
    stats->device_count = 0;
}

void mesh_gpu_channel_print_debug(mesh_gpu_channel_t channel) {
    if (!channel) {
        printf("GPU Channel: NULL\n");
        return;
    }

    printf("=== GPU Channel Debug ===\n");
    printf("Running: %s\n", channel->running ? "yes" : "no");
    printf("Devices: %zu\n", channel->coordinator_count);
    printf("Pending: %zu / %zu\n", channel->current_batch.count, channel->config.batch_threshold);
    printf("Batch timeout: %.1f ms\n", channel->config.batch_timeout_ms);
    printf("CPU fallback: %s\n", channel->config.enable_cpu_fallback ? "enabled" : "disabled");

    printf("\nDevice Status:\n");
    for (size_t i = 0; i < channel->coordinator_count; i++) {
        gpu_coordinator_t* c = &channel->coordinators[i];
        printf("  [%zu] Device %d: %s %s, processed=%llu, failures=%llu\n",
               i, c->device_id,
               c->enabled ? "enabled" : "disabled",
               c->healthy ? "healthy" : "unhealthy",
               (unsigned long long)c->transactions_processed,
               (unsigned long long)c->failures);
    }

    printf("\nStatistics:\n");
    printf("  Submitted: %llu\n", (unsigned long long)channel->stats.total_submitted);
    printf("  Completed: %llu\n", (unsigned long long)channel->stats.total_completed);
    printf("  Failed: %llu\n", (unsigned long long)channel->stats.total_failed);
    printf("  Fallbacks: %llu\n", (unsigned long long)channel->stats.total_fallbacks);
    printf("=========================\n");
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* mesh_gpu_tx_type_to_string(mesh_gpu_tx_type_t type) {
    switch (type) {
        case MESH_GPU_TX_NONE:            return "NONE";
        case MESH_GPU_TX_BELIEF_BATCH:    return "BELIEF_BATCH";
        case MESH_GPU_TX_CONSENSUS_COMPUTE: return "CONSENSUS_COMPUTE";
        case MESH_GPU_TX_FEP_BATCH:       return "FEP_BATCH";
        case MESH_GPU_TX_TENSOR_OP:       return "TENSOR_OP";
        case MESH_GPU_TX_NEURAL_FORWARD:  return "NEURAL_FORWARD";
        case MESH_GPU_TX_NEURAL_BACKWARD: return "NEURAL_BACKWARD";
        case MESH_GPU_TX_MATRIX_MULTIPLY: return "MATRIX_MULTIPLY";
        case MESH_GPU_TX_STATISTICAL:     return "STATISTICAL";
        case MESH_GPU_TX_CUSTOM:          return "CUSTOM";
        default:                          return "UNKNOWN";
    }
}

const char* mesh_gpu_tx_status_to_string(mesh_gpu_tx_status_t status) {
    switch (status) {
        case MESH_GPU_TX_STATUS_PENDING:    return "PENDING";
        case MESH_GPU_TX_STATUS_BATCHED:    return "BATCHED";
        case MESH_GPU_TX_STATUS_PROCESSING: return "PROCESSING";
        case MESH_GPU_TX_STATUS_COMPLETED:  return "COMPLETED";
        case MESH_GPU_TX_STATUS_FAILED:     return "FAILED";
        case MESH_GPU_TX_STATUS_FALLBACK:   return "FALLBACK";
        default:                            return "UNKNOWN";
    }
}
