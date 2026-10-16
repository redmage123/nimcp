/**
 * @file nimcp_mesh_gpu.h
 * @brief GPU Channel and Coordinator for Mesh Network
 *
 * WHAT: GPU-accelerated mesh channel with batch processing and multi-GPU support
 * WHY:  Enable high-throughput parallel transaction processing on GPU
 * HOW:  Integrate GPU recovery, batch transactions, distribute across devices
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      GPU CHANNEL ARCHITECTURE                           │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   GPU COORDINATOR POOL                          │   │
 * │  │  [Leader]     [Worker 1]     [Worker 2]     [Standby]           │   │
 * │  │     │              │              │              │              │   │
 * │  │     ▼              ▼              ▼              ▼              │   │
 * │  │  GPU 0          GPU 1          GPU 2          (any)            │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   TRANSACTION BATCH QUEUE                       │   │
 * │  │  [tx1] [tx2] [tx3] [tx4] ...  → BATCH → GPU KERNEL              │   │
 * │  │                                                                 │   │
 * │  │  • Batch threshold: 64 transactions                            │   │
 * │  │  • Batch timeout: 50ms                                         │   │
 * │  │  • Max pending: 1024 transactions                              │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   4-TIER GPU RECOVERY                           │   │
 * │  │  Tier 1: Parameter correction                                   │   │
 * │  │  Tier 2: Resource adjustment (batch reduction)                  │   │
 * │  │  Tier 3: CPU fallback                                           │   │
 * │  │  Tier 4: Retry with backoff                                     │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * GPU PROCESSING FLOW:
 * 1. Transactions submitted to pending queue
 * 2. When batch threshold or timeout reached, flush batch
 * 3. Distribute batch across available GPUs
 * 4. Execute GPU kernels with recovery on failure
 * 5. Collect results, commit to world state
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_GPU_H
#define NIMCP_MESH_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum GPUs supported */
#define MESH_GPU_MAX_DEVICES                8

/** @brief Default batch threshold */
#define MESH_GPU_DEFAULT_BATCH_THRESHOLD    64

/** @brief Default batch timeout (ms) */
#define MESH_GPU_DEFAULT_BATCH_TIMEOUT_MS   50.0f

/** @brief Maximum pending transactions in GPU queue */
#define MESH_GPU_MAX_PENDING                1024

/** @brief Default memory threshold for batch reduction */
#define MESH_GPU_MEMORY_THRESHOLD           0.85f

/** @brief Minimum batch size after reduction */
#define MESH_GPU_MIN_BATCH_SIZE             4

/** @brief GPU recovery max retries */
#define MESH_GPU_MAX_RETRIES                3

/** @brief Well-known GPU channel ID */
#define MESH_CHANNEL_GPU_COMPUTE_ID         4

/* ============================================================================
 * GPU Transaction Types
 * ============================================================================ */

/**
 * @brief GPU-specific transaction types
 */
typedef enum mesh_gpu_tx_type {
    MESH_GPU_TX_NONE = 0,               /**< Invalid */
    MESH_GPU_TX_BELIEF_BATCH,           /**< Batch belief updates */
    MESH_GPU_TX_CONSENSUS_COMPUTE,      /**< Parallel consensus computation */
    MESH_GPU_TX_FEP_BATCH,              /**< Batch free energy computation */
    MESH_GPU_TX_TENSOR_OP,              /**< Tensor operations */
    MESH_GPU_TX_NEURAL_FORWARD,         /**< Neural network forward pass */
    MESH_GPU_TX_NEURAL_BACKWARD,        /**< Neural network backward pass */
    MESH_GPU_TX_MATRIX_MULTIPLY,        /**< Matrix multiplication */
    MESH_GPU_TX_STATISTICAL,            /**< Statistical computations */
    MESH_GPU_TX_CUSTOM                  /**< Custom GPU operation */
} mesh_gpu_tx_type_t;

/**
 * @brief GPU transaction status
 */
typedef enum mesh_gpu_tx_status {
    MESH_GPU_TX_STATUS_PENDING = 0,     /**< In queue, not yet processed */
    MESH_GPU_TX_STATUS_BATCHED,         /**< Added to current batch */
    MESH_GPU_TX_STATUS_PROCESSING,      /**< Currently executing on GPU */
    MESH_GPU_TX_STATUS_COMPLETED,       /**< Successfully completed */
    MESH_GPU_TX_STATUS_FAILED,          /**< Failed after recovery attempts */
    MESH_GPU_TX_STATUS_FALLBACK         /**< Completed via CPU fallback */
} mesh_gpu_tx_status_t;

/* ============================================================================
 * GPU Transaction Structure
 * ============================================================================ */

/**
 * @brief GPU transaction for batch processing
 */
typedef struct mesh_gpu_transaction {
    mesh_tx_id_t base_id;               /**< Base transaction ID */
    mesh_gpu_tx_type_t gpu_type;        /**< GPU-specific type */
    mesh_gpu_tx_status_t status;        /**< Current status */

    /* Input data */
    void* input_data;                   /**< Input data pointer */
    size_t input_size;                  /**< Input size in bytes */

    /* Output data */
    void* output_data;                  /**< Output buffer (caller allocated) */
    size_t output_size;                 /**< Output size in bytes */

    /* GPU specifics */
    int target_device;                  /**< Target GPU (-1 = any) */
    size_t required_memory;             /**< Estimated memory requirement */
    uint32_t priority;                  /**< Processing priority (higher = sooner) */

    /* Timing */
    uint64_t submitted_ns;              /**< Submission timestamp */
    uint64_t started_ns;                /**< Processing start */
    uint64_t completed_ns;              /**< Completion timestamp */

    /* Result */
    nimcp_error_t error;                /**< Error code if failed */
    char error_msg[128];                /**< Error message */

    /* Callback */
    mesh_tx_callback_t callback;        /**< Completion callback */
    void* callback_ctx;                 /**< Callback context */
} mesh_gpu_transaction_t;

/**
 * @brief GPU transaction batch
 */
typedef struct mesh_gpu_batch {
    mesh_gpu_transaction_t** transactions;  /**< Array of transaction pointers */
    size_t count;                           /**< Number in batch */
    size_t capacity;                        /**< Array capacity */

    /* Batch metadata */
    mesh_gpu_tx_type_t batch_type;          /**< Dominant type in batch */
    size_t total_input_size;                /**< Total input bytes */
    size_t total_output_size;               /**< Total output bytes */

    /* Device assignment */
    int assigned_device;                    /**< GPU device for this batch */

    /* Timing */
    uint64_t created_ns;                    /**< Batch creation time */
    uint64_t flushed_ns;                    /**< When flushed for processing */
} mesh_gpu_batch_t;

/* ============================================================================
 * GPU Channel Configuration
 * ============================================================================ */

/**
 * @brief GPU coordinator configuration
 */
typedef struct mesh_gpu_coordinator_config {
    int device_id;                          /**< CUDA device ID */
    size_t memory_limit;                    /**< Memory limit for this device */
    bool enable_async;                      /**< Enable async execution */
    uint32_t max_concurrent_batches;        /**< Max concurrent batches */
} mesh_gpu_coordinator_config_t;

/**
 * @brief GPU channel configuration
 */
typedef struct mesh_gpu_channel_config {
    /* Device configuration */
    mesh_gpu_coordinator_config_t* device_configs;  /**< Per-device config */
    size_t device_count;                            /**< Number of devices */

    /* Batching configuration */
    size_t batch_threshold;                 /**< Transactions before flush */
    float batch_timeout_ms;                 /**< Max time before flush */
    size_t max_pending;                     /**< Max pending transactions */

    /* Recovery configuration */
    bool enable_cpu_fallback;               /**< Allow CPU fallback */
    bool enable_batch_reduction;            /**< Reduce batch on OOM */
    uint32_t max_retries;                   /**< Max retry attempts */
    float memory_threshold;                 /**< Memory warning threshold */

    /* Load balancing */
    bool round_robin_devices;               /**< Round-robin vs. load-based */
    bool colocate_related;                  /**< Group related txs on same GPU */
} mesh_gpu_channel_config_t;

/* ============================================================================
 * GPU Device State
 * ============================================================================ */

/**
 * @brief GPU device state
 */
typedef struct mesh_gpu_device_state {
    int device_id;                          /**< CUDA device ID */
    bool available;                         /**< Device is available */
    bool healthy;                           /**< Device is healthy */

    /* Memory */
    size_t total_memory;                    /**< Total device memory */
    size_t free_memory;                     /**< Current free memory */
    size_t used_by_channel;                 /**< Memory used by this channel */

    /* Workload */
    uint32_t pending_batches;               /**< Batches waiting */
    uint32_t processing_batches;            /**< Batches currently processing */
    float utilization;                      /**< Current utilization [0,1] */

    /* Statistics */
    uint64_t batches_processed;             /**< Total batches processed */
    uint64_t transactions_processed;        /**< Total transactions */
    uint64_t failures;                      /**< Total failures */
    uint64_t fallbacks;                     /**< CPU fallbacks used */
    float avg_batch_time_ms;                /**< Average batch processing time */
} mesh_gpu_device_state_t;

/* ============================================================================
 * GPU Channel Statistics
 * ============================================================================ */

/**
 * @brief GPU channel statistics
 */
typedef struct mesh_gpu_channel_stats {
    /* Transaction counts */
    uint64_t total_submitted;               /**< Total transactions submitted */
    uint64_t total_completed;               /**< Successfully completed */
    uint64_t total_failed;                  /**< Failed after recovery */
    uint64_t total_fallbacks;               /**< Completed via CPU fallback */

    /* Batching */
    uint64_t batches_created;               /**< Total batches created */
    uint64_t batches_timeout_flush;         /**< Batches flushed by timeout */
    uint64_t batches_threshold_flush;       /**< Batches flushed by threshold */
    float avg_batch_size;                   /**< Average transactions per batch */

    /* Recovery */
    uint64_t recovery_attempts;             /**< Total recovery attempts */
    uint64_t recovery_successes;            /**< Successful recoveries */
    uint64_t batch_reductions;              /**< Times batch was reduced */

    /* Timing */
    float avg_queue_time_ms;                /**< Average time in queue */
    float avg_processing_time_ms;           /**< Average processing time */
    float avg_total_latency_ms;             /**< Submit to complete */

    /* Per-device stats */
    mesh_gpu_device_state_t* device_stats;  /**< Per-device statistics */
    size_t device_count;                    /**< Number of devices */
} mesh_gpu_channel_stats_t;

/* ============================================================================
 * Opaque Types
 * ============================================================================ */

/**
 * @brief Opaque GPU channel context
 */
typedef struct mesh_gpu_channel_internal* mesh_gpu_channel_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create default GPU channel configuration
 *
 * WHAT: Returns sensible defaults for GPU channel
 * WHY:  Easy starting point
 *
 * @return Default configuration
 */
mesh_gpu_channel_config_t mesh_gpu_channel_default_config(void);

/**
 * @brief Create GPU channel
 *
 * WHAT: Initialize GPU channel with coordinator pool
 * WHY:  Enable GPU-accelerated transaction processing
 * HOW:  Detect GPUs, create coordinators, initialize batching
 *
 * @param config Configuration (NULL for defaults)
 * @return Channel handle or NULL on failure
 */
mesh_gpu_channel_t mesh_gpu_channel_create(const mesh_gpu_channel_config_t* config);

/**
 * @brief Destroy GPU channel
 *
 * WHAT: Free all GPU channel resources
 * WHY:  Prevent memory and GPU resource leaks
 *
 * @param channel Channel to destroy
 */
void mesh_gpu_channel_destroy(mesh_gpu_channel_t channel);

/**
 * @brief Start GPU channel processing
 *
 * WHAT: Begin accepting and processing transactions
 * WHY:  Separate creation from activation
 *
 * @param channel GPU channel
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_start(mesh_gpu_channel_t channel);

/**
 * @brief Stop GPU channel processing
 *
 * WHAT: Stop accepting new transactions, drain pending
 * WHY:  Graceful shutdown
 *
 * @param channel GPU channel
 * @param drain Wait for pending transactions to complete
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_stop(mesh_gpu_channel_t channel, bool drain);

/* ============================================================================
 * Transaction Submission
 * ============================================================================ */

/**
 * @brief Submit transaction to GPU channel
 *
 * WHAT: Add transaction to pending queue
 * WHY:  Queue for batch processing
 * HOW:  Add to queue, batch when threshold/timeout reached
 *
 * @param channel GPU channel
 * @param tx Transaction to submit (channel takes ownership)
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_submit(
    mesh_gpu_channel_t channel,
    mesh_gpu_transaction_t* tx
);

/**
 * @brief Submit transaction with callback
 *
 * @param channel GPU channel
 * @param tx Transaction to submit
 * @param callback Completion callback
 * @param ctx Callback context
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_submit_async(
    mesh_gpu_channel_t channel,
    mesh_gpu_transaction_t* tx,
    mesh_tx_callback_t callback,
    void* ctx
);

/**
 * @brief Force flush current batch
 *
 * WHAT: Immediately process pending transactions
 * WHY:  Low-latency when batch threshold not yet reached
 *
 * @param channel GPU channel
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_flush(mesh_gpu_channel_t channel);

/* ============================================================================
 * Transaction Management
 * ============================================================================ */

/**
 * @brief Create GPU transaction
 *
 * @param gpu_type GPU transaction type
 * @param input_data Input data (copied)
 * @param input_size Input size
 * @param output_size Expected output size
 * @return New transaction or NULL
 */
mesh_gpu_transaction_t* mesh_gpu_transaction_create(
    mesh_gpu_tx_type_t gpu_type,
    const void* input_data,
    size_t input_size,
    size_t output_size
);

/**
 * @brief Destroy GPU transaction
 *
 * @param tx Transaction to destroy
 */
void mesh_gpu_transaction_destroy(mesh_gpu_transaction_t* tx);

/**
 * @brief Wait for transaction completion
 *
 * @param channel GPU channel
 * @param tx_id Transaction ID to wait for
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return NIMCP_OK on completion, error on timeout/failure
 */
nimcp_error_t mesh_gpu_channel_wait(
    mesh_gpu_channel_t channel,
    const mesh_tx_id_t* tx_id,
    uint32_t timeout_ms
);

/**
 * @brief Get transaction status
 *
 * @param channel GPU channel
 * @param tx_id Transaction ID
 * @param status Output status
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_get_status(
    mesh_gpu_channel_t channel,
    const mesh_tx_id_t* tx_id,
    mesh_gpu_tx_status_t* status
);

/* ============================================================================
 * Device Management
 * ============================================================================ */

/**
 * @brief Get number of available GPU devices
 *
 * @param channel GPU channel
 * @return Number of devices
 */
size_t mesh_gpu_channel_device_count(mesh_gpu_channel_t channel);

/**
 * @brief Get device state
 *
 * @param channel GPU channel
 * @param device_idx Device index
 * @param state Output state
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_get_device_state(
    mesh_gpu_channel_t channel,
    size_t device_idx,
    mesh_gpu_device_state_t* state
);

/**
 * @brief Mark device as unavailable
 *
 * WHAT: Temporarily disable a GPU device
 * WHY:  Hardware failure or maintenance
 *
 * @param channel GPU channel
 * @param device_idx Device index
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_disable_device(
    mesh_gpu_channel_t channel,
    size_t device_idx
);

/**
 * @brief Re-enable a disabled device
 *
 * @param channel GPU channel
 * @param device_idx Device index
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_enable_device(
    mesh_gpu_channel_t channel,
    size_t device_idx
);

/* ============================================================================
 * Batch Management
 * ============================================================================ */

/**
 * @brief Get current batch size
 *
 * @param channel GPU channel
 * @return Number of pending transactions in current batch
 */
size_t mesh_gpu_channel_pending_count(mesh_gpu_channel_t channel);

/**
 * @brief Set batch threshold
 *
 * @param channel GPU channel
 * @param threshold New threshold
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_set_batch_threshold(
    mesh_gpu_channel_t channel,
    size_t threshold
);

/**
 * @brief Set batch timeout
 *
 * @param channel GPU channel
 * @param timeout_ms Timeout in milliseconds
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_set_batch_timeout(
    mesh_gpu_channel_t channel,
    float timeout_ms
);

/* ============================================================================
 * Recovery Control
 * ============================================================================ */

/**
 * @brief Enable/disable CPU fallback
 *
 * @param channel GPU channel
 * @param enable Enable CPU fallback
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_set_cpu_fallback(
    mesh_gpu_channel_t channel,
    bool enable
);

/**
 * @brief Set maximum retries
 *
 * @param channel GPU channel
 * @param max_retries Maximum retry attempts
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_set_max_retries(
    mesh_gpu_channel_t channel,
    uint32_t max_retries
);

/**
 * @brief Register CPU fallback function for transaction type
 *
 * @param channel GPU channel
 * @param gpu_type Transaction type
 * @param fallback_fn CPU implementation
 * @param ctx Fallback context
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_register_fallback(
    mesh_gpu_channel_t channel,
    mesh_gpu_tx_type_t gpu_type,
    bool (*fallback_fn)(const mesh_gpu_transaction_t* tx, void* ctx),
    void* ctx
);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Get channel statistics
 *
 * @param channel GPU channel
 * @param stats Output statistics
 * @return NIMCP_OK on success
 *
 * @note Caller must free stats->device_stats if not NULL
 */
nimcp_error_t mesh_gpu_channel_get_stats(
    mesh_gpu_channel_t channel,
    mesh_gpu_channel_stats_t* stats
);

/**
 * @brief Reset channel statistics
 *
 * @param channel GPU channel
 * @return NIMCP_OK on success
 */
nimcp_error_t mesh_gpu_channel_reset_stats(mesh_gpu_channel_t channel);

/**
 * @brief Free statistics resources
 *
 * @param stats Statistics to free (fields only)
 */
void mesh_gpu_channel_stats_free(mesh_gpu_channel_stats_t* stats);

/**
 * @brief Print channel debug information
 *
 * @param channel GPU channel
 */
void mesh_gpu_channel_print_debug(mesh_gpu_channel_t channel);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get GPU transaction type name
 *
 * @param type GPU transaction type
 * @return Type name string
 */
const char* mesh_gpu_tx_type_to_string(mesh_gpu_tx_type_t type);

/**
 * @brief Get GPU transaction status name
 *
 * @param status Transaction status
 * @return Status name string
 */
const char* mesh_gpu_tx_status_to_string(mesh_gpu_tx_status_t status);

/**
 * @brief Check if CUDA is available
 *
 * @return true if CUDA runtime is available
 */
bool mesh_gpu_cuda_available(void);

/**
 * @brief Get number of CUDA devices
 *
 * @return Number of CUDA-capable devices
 */
int mesh_gpu_get_device_count(void);

/**
 * @brief Get device memory info
 *
 * @param device_id CUDA device ID
 * @param free_bytes Output: free memory
 * @param total_bytes Output: total memory
 * @return true on success
 */
bool mesh_gpu_get_device_memory(
    int device_id,
    size_t* free_bytes,
    size_t* total_bytes
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_GPU_H */
