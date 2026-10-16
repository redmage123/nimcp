/**
 * @file nimcp_gpu_bio_async_bridge.h
 * @brief GPU Module Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2025-01-13
 *
 * WHAT: Bidirectional integration between GPU modules and bio-async messaging
 * WHY:  Enable asynchronous GPU operations via biological signaling patterns
 * HOW:  Register GPU modules with bio-router, handle GPU-specific messages
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * GPU operations map to neural signaling patterns:
 * - GPU compute requests = neural activation bursts
 * - Memory transfers = axonal transport
 * - Multi-GPU coordination = inter-hemispheric communication
 * - GPU status updates = metabolic monitoring
 *
 * MESSAGE FLOW:
 * ```
 * +--------------------+     Bio-Async Router     +--------------------+
 * |                    |                          |                    |
 * |   GPU Module A     | -----> Messages ------>  |   GPU Module B     |
 * |   (Compute)        |                          |   (Transfer)       |
 * |                    | <----- Responses <-----  |                    |
 * +--------------------+                          +--------------------+
 *           |                                              |
 *           v                                              v
 * +--------------------+                          +--------------------+
 * |   GPU Device 0     |  <-- P2P Transfer -->    |   GPU Device 1     |
 * +--------------------+                          +--------------------+
 * ```
 *
 * CHANNEL ASSIGNMENT:
 * - DOPAMINE: Compute completion, successful operations
 * - NOREPINEPHRINE: GPU errors, resource alerts
 * - ACETYLCHOLINE: Fast status queries, synchronization
 * - SEROTONIN: Long-running transfers, bulk operations
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GPU_BIO_ASYNC_BRIDGE_H
#define NIMCP_GPU_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "gpu/nimcp_multigpu.h"
#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define GPU_BIO_ERROR_BASE                      20000
#define GPU_BIO_ERROR_NOT_INITIALIZED           (GPU_BIO_ERROR_BASE + 1)
#define GPU_BIO_ERROR_INVALID_DEVICE            (GPU_BIO_ERROR_BASE + 2)
#define GPU_BIO_ERROR_DEVICE_BUSY               (GPU_BIO_ERROR_BASE + 3)
#define GPU_BIO_ERROR_TRANSFER_FAILED           (GPU_BIO_ERROR_BASE + 4)
#define GPU_BIO_ERROR_COMPUTE_FAILED            (GPU_BIO_ERROR_BASE + 5)
#define GPU_BIO_ERROR_SYNC_TIMEOUT              (GPU_BIO_ERROR_BASE + 6)
#define GPU_BIO_ERROR_ROUTER_UNAVAILABLE        (GPU_BIO_ERROR_BASE + 7)
#define GPU_BIO_ERROR_MESSAGE_TOO_LARGE         (GPU_BIO_ERROR_BASE + 8)
#define GPU_BIO_ERROR_QUEUE_FULL                (GPU_BIO_ERROR_BASE + 9)
#define GPU_BIO_ERROR_INVALID_OPERATION         (GPU_BIO_ERROR_BASE + 10)

/* ============================================================================
 * GPU Message Types (0x1F00 - 0x1FFF)
 * ============================================================================ */

/**
 * @brief GPU-specific bio-async message types
 *
 * These extend bio_message_type_t for GPU operations.
 * Range 0x1F00 - 0x1FFF reserved for GPU messages.
 */
typedef enum {
    /* Compute messages (0x1F00 - 0x1F1F) */
    GPU_MSG_COMPUTE_REQUEST = 0x1F00,       /**< Request GPU compute operation */
    GPU_MSG_COMPUTE_COMPLETE,               /**< Compute operation completed */
    GPU_MSG_COMPUTE_FAILED,                 /**< Compute operation failed */
    GPU_MSG_COMPUTE_PROGRESS,               /**< Compute progress update */
    GPU_MSG_KERNEL_LAUNCH,                  /**< Kernel launch notification */
    GPU_MSG_KERNEL_COMPLETE,                /**< Kernel execution complete */

    /* Transfer messages (0x1F20 - 0x1F3F) */
    GPU_MSG_TRANSFER_REQUEST = 0x1F20,      /**< Request memory transfer */
    GPU_MSG_TRANSFER_COMPLETE,              /**< Transfer completed */
    GPU_MSG_TRANSFER_FAILED,                /**< Transfer failed */
    GPU_MSG_TRANSFER_PROGRESS,              /**< Transfer progress update */
    GPU_MSG_H2D_TRANSFER,                   /**< Host-to-device transfer */
    GPU_MSG_D2H_TRANSFER,                   /**< Device-to-host transfer */
    GPU_MSG_D2D_TRANSFER,                   /**< Device-to-device (P2P) transfer */
    GPU_MSG_BROADCAST_REQUEST,              /**< Broadcast to all GPUs */
    GPU_MSG_GATHER_REQUEST,                 /**< Gather from all GPUs */
    GPU_MSG_BROADCAST_COMPLETE,             /**< Broadcast completed */
    GPU_MSG_GATHER_COMPLETE,                /**< Gather completed */

    /* Status messages (0x1F40 - 0x1F5F) */
    GPU_MSG_STATUS_QUERY = 0x1F40,          /**< Query GPU status */
    GPU_MSG_STATUS_RESPONSE,                /**< GPU status response */
    GPU_MSG_UTILIZATION_UPDATE,             /**< Utilization level update */
    GPU_MSG_MEMORY_UPDATE,                  /**< Memory usage update */
    GPU_MSG_TEMPERATURE_UPDATE,             /**< Temperature update */
    GPU_MSG_POWER_UPDATE,                   /**< Power consumption update */
    GPU_MSG_ERROR_REPORT,                   /**< GPU error report */
    GPU_MSG_DEVICE_ONLINE,                  /**< Device came online */
    GPU_MSG_DEVICE_OFFLINE,                 /**< Device went offline */
    GPU_MSG_DEVICE_RESET,                   /**< Device was reset */

    /* Multi-GPU coordination (0x1F60 - 0x1F7F) */
    GPU_MSG_MULTIGPU_SYNC_REQUEST = 0x1F60, /**< Request multi-GPU sync */
    GPU_MSG_MULTIGPU_SYNC_COMPLETE,         /**< Sync completed */
    GPU_MSG_PARTITION_REQUEST,              /**< Request work partitioning */
    GPU_MSG_PARTITION_COMPLETE,             /**< Partitioning completed */
    GPU_MSG_REBALANCE_REQUEST,              /**< Request load rebalancing */
    GPU_MSG_REBALANCE_COMPLETE,             /**< Rebalancing completed */
    GPU_MSG_DEVICE_ASSIGNMENT,              /**< Device assignment update */
    GPU_MSG_P2P_ENABLED,                    /**< P2P access enabled */
    GPU_MSG_P2P_DISABLED,                   /**< P2P access disabled */
    GPU_MSG_LOAD_IMBALANCE_DETECTED,        /**< Load imbalance detected */
    GPU_MSG_COORDINATION_ERROR,             /**< Multi-GPU coordination error */

    /* Resource messages (0x1F80 - 0x1F9F) */
    GPU_MSG_MEMORY_ALLOC_REQUEST = 0x1F80,  /**< Request memory allocation */
    GPU_MSG_MEMORY_ALLOC_COMPLETE,          /**< Allocation completed */
    GPU_MSG_MEMORY_FREE_REQUEST,            /**< Request memory free */
    GPU_MSG_MEMORY_FREE_COMPLETE,           /**< Free completed */
    GPU_MSG_POOL_STATUS,                    /**< Memory pool status */
    GPU_MSG_STREAM_CREATE,                  /**< Stream created */
    GPU_MSG_STREAM_DESTROY,                 /**< Stream destroyed */
    GPU_MSG_RESOURCE_EXHAUSTED,             /**< Resources exhausted */

    /* Neural-specific GPU messages (0x1FA0 - 0x1FBF) */
    GPU_MSG_NEURON_BATCH_REQUEST = 0x1FA0,  /**< Process neuron batch */
    GPU_MSG_NEURON_BATCH_COMPLETE,          /**< Neuron batch completed */
    GPU_MSG_SYNAPSE_UPDATE_REQUEST,         /**< Synapse weight update */
    GPU_MSG_SYNAPSE_UPDATE_COMPLETE,        /**< Synapse update completed */
    GPU_MSG_SPIKE_PROPAGATION,              /**< Spike propagation batch */
    GPU_MSG_SPIKE_PROPAGATION_COMPLETE,     /**< Spike propagation done */
    GPU_MSG_PLASTICITY_BATCH,               /**< Plasticity update batch */
    GPU_MSG_PLASTICITY_BATCH_COMPLETE,      /**< Plasticity batch done */

    /* Sentinel */
    GPU_MSG_TYPE_COUNT
} gpu_message_type_t;

/* ============================================================================
 * GPU Module Identifiers (extend bio_module_id_t)
 * ============================================================================ */

/**
 * @brief GPU-specific module identifiers
 *
 * Range 0x1F00 - 0x1FFF for GPU modules
 */
typedef enum {
    BIO_MODULE_GPU_BRIDGE = 0x1F00,         /**< Main GPU bio-async bridge */
    BIO_MODULE_GPU_COMPUTE,                 /**< GPU compute module */
    BIO_MODULE_GPU_TRANSFER,                /**< GPU transfer module */
    BIO_MODULE_GPU_STATUS,                  /**< GPU status monitor */
    BIO_MODULE_GPU_MULTIGPU,                /**< Multi-GPU coordinator */
    BIO_MODULE_GPU_MEMORY,                  /**< GPU memory manager */
    BIO_MODULE_GPU_NEURON,                  /**< GPU neuron processing */
    BIO_MODULE_GPU_SYNAPSE,                 /**< GPU synapse processing */
    BIO_MODULE_GPU_PLASTICITY,              /**< GPU plasticity updates */
    BIO_MODULE_GPU_TENSOR,                  /**< GPU tensor operations */
    BIO_MODULE_GPU_INFERENCE,               /**< GPU inference engine */
    BIO_MODULE_GPU_TRAINING,                /**< GPU training operations */
    BIO_MODULE_GPU_QUANTUM,                 /**< GPU quantum simulation */
    BIO_MODULE_GPU_CONTEXT,                 /**< GPU context manager */
} gpu_module_id_t;

/* ============================================================================
 * GPU Operation Types
 * ============================================================================ */

/**
 * @brief Types of GPU operations for messaging
 */
typedef enum {
    GPU_OP_COMPUTE,                         /**< General compute */
    GPU_OP_TRANSFER_H2D,                    /**< Host to device */
    GPU_OP_TRANSFER_D2H,                    /**< Device to host */
    GPU_OP_TRANSFER_D2D,                    /**< Device to device */
    GPU_OP_SYNC,                            /**< Synchronization */
    GPU_OP_BROADCAST,                       /**< Broadcast to all */
    GPU_OP_GATHER,                          /**< Gather from all */
    GPU_OP_PARTITION,                       /**< Work partitioning */
    GPU_OP_REBALANCE,                       /**< Load rebalancing */
    GPU_OP_NEURON_STEP,                     /**< Neuron simulation step */
    GPU_OP_SYNAPSE_UPDATE,                  /**< Synapse weight update */
    GPU_OP_SPIKE_PROPAGATE,                 /**< Spike propagation */
    GPU_OP_PLASTICITY,                      /**< Plasticity update */
    GPU_OP_CUSTOM                           /**< Custom operation */
} gpu_operation_type_t;

/**
 * @brief GPU device status
 */
typedef enum {
    GPU_STATUS_UNKNOWN,                     /**< Status unknown */
    GPU_STATUS_OFFLINE,                     /**< Device offline */
    GPU_STATUS_IDLE,                        /**< Device idle */
    GPU_STATUS_BUSY,                        /**< Device busy */
    GPU_STATUS_OVERLOADED,                  /**< Device overloaded */
    GPU_STATUS_ERROR,                       /**< Device in error state */
    GPU_STATUS_RESETTING                    /**< Device resetting */
} gpu_device_status_t;

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief GPU compute request payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard message header */
    uint32_t device_id;                     /**< Target GPU device */
    gpu_operation_type_t operation;         /**< Operation type */
    uint64_t operation_id;                  /**< Unique operation ID */
    size_t data_size;                       /**< Size of operation data */
    void* data_ptr;                         /**< Pointer to operation data */
    float priority;                         /**< Priority [0, 1] */
    uint32_t timeout_ms;                    /**< Operation timeout */
} gpu_compute_request_msg_t;

/**
 * @brief GPU compute complete payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard message header */
    uint32_t device_id;                     /**< GPU device that completed */
    uint64_t operation_id;                  /**< Matching operation ID */
    int32_t result_code;                    /**< Result code (0 = success) */
    uint64_t elapsed_us;                    /**< Elapsed time in microseconds */
    float gpu_utilization;                  /**< GPU utilization during op */
} gpu_compute_complete_msg_t;

/**
 * @brief GPU transfer request payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard message header */
    uint32_t src_device_id;                 /**< Source device (-1 for host) */
    uint32_t dst_device_id;                 /**< Destination device (-1 for host) */
    uint64_t transfer_id;                   /**< Unique transfer ID */
    void* src_ptr;                          /**< Source pointer */
    void* dst_ptr;                          /**< Destination pointer */
    size_t size;                            /**< Transfer size in bytes */
    bool async;                             /**< Asynchronous transfer */
} gpu_transfer_request_msg_t;

/**
 * @brief GPU transfer complete payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard message header */
    uint64_t transfer_id;                   /**< Matching transfer ID */
    int32_t result_code;                    /**< Result code */
    uint64_t elapsed_us;                    /**< Elapsed time */
    float bandwidth_gbps;                   /**< Achieved bandwidth */
} gpu_transfer_complete_msg_t;

/**
 * @brief GPU status update payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard message header */
    uint32_t device_id;                     /**< Device ID */
    gpu_device_status_t status;             /**< Current status */
    float compute_utilization;              /**< Compute utilization [0, 1] */
    float memory_utilization;               /**< Memory utilization [0, 1] */
    uint64_t memory_free;                   /**< Free memory in bytes */
    uint64_t memory_total;                  /**< Total memory in bytes */
    float temperature_celsius;              /**< Temperature */
    float power_watts;                      /**< Power consumption */
} gpu_status_msg_t;

/**
 * @brief Multi-GPU sync request payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard message header */
    uint32_t* device_ids;                   /**< Devices to sync */
    uint32_t device_count;                  /**< Number of devices */
    uint64_t sync_id;                       /**< Sync operation ID */
    uint32_t timeout_ms;                    /**< Sync timeout */
} gpu_multigpu_sync_msg_t;

/**
 * @brief GPU coordination status for multi-GPU
 */
typedef struct {
    bio_message_header_t header;            /**< Standard message header */
    uint32_t active_device_count;           /**< Active GPUs */
    float load_imbalance;                   /**< Load imbalance [0, 1] */
    bool p2p_enabled;                       /**< P2P access enabled */
    multigpu_partition_strategy_t partition; /**< Current partition strategy */
    uint64_t last_rebalance_us;             /**< Time since last rebalance */
} gpu_coordination_status_msg_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief GPU bio-async bridge configuration
 */
typedef struct {
    /* Bio-async settings */
    nimcp_bio_channel_type_t primary_channel;   /**< Primary channel for GPU messages */
    nimcp_bio_channel_type_t error_channel;     /**< Channel for error messages */
    bool enable_broadcast;                      /**< Enable status broadcasts */
    uint32_t broadcast_interval_ms;             /**< Broadcast interval (ms) */

    /* Multi-GPU settings */
    bool enable_multigpu_coordination;          /**< Enable multi-GPU coordination */
    uint32_t sync_timeout_ms;                   /**< Default sync timeout */
    float load_imbalance_threshold;             /**< Threshold for imbalance alerts */

    /* Message handling */
    uint32_t max_pending_operations;            /**< Max pending operations */
    size_t max_message_size;                    /**< Max message payload size */
    bool enable_operation_logging;              /**< Log all operations */

    /* Performance */
    bool enable_prefetch;                       /**< Enable message prefetching */
    bool enable_batching;                       /**< Enable operation batching */
    uint32_t batch_size;                        /**< Batch size for batching */
} gpu_bio_bridge_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief GPU bio-async bridge statistics
 */
typedef struct {
    /* Message statistics */
    uint64_t compute_requests_sent;             /**< Compute requests sent */
    uint64_t compute_requests_completed;        /**< Compute requests completed */
    uint64_t transfer_requests_sent;            /**< Transfer requests sent */
    uint64_t transfer_requests_completed;       /**< Transfer requests completed */
    uint64_t status_updates_sent;               /**< Status updates broadcasted */

    /* Error statistics */
    uint64_t compute_failures;                  /**< Compute failures */
    uint64_t transfer_failures;                 /**< Transfer failures */
    uint64_t sync_timeouts;                     /**< Sync timeouts */
    uint64_t message_drops;                     /**< Dropped messages */

    /* Performance */
    float avg_compute_latency_us;               /**< Average compute latency */
    float avg_transfer_latency_us;              /**< Average transfer latency */
    float avg_message_latency_us;               /**< Average message routing latency */
    float peak_bandwidth_gbps;                  /**< Peak achieved bandwidth */

    /* Multi-GPU */
    uint64_t multigpu_syncs;                    /**< Multi-GPU syncs performed */
    uint64_t rebalance_events;                  /**< Load rebalance events */
    uint64_t p2p_transfers;                     /**< P2P transfers */
} gpu_bio_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief GPU bio-async bridge
 *
 * WHAT: Main bridge connecting GPU modules to bio-async messaging
 * WHY:  Enables asynchronous GPU operations with biological signaling
 * HOW:  Registers with bio-router, handles GPU-specific message types
 */
typedef struct {
    bridge_base_t base;                         /* MUST be first: base bridge infrastructure */

    /* Configuration */
    gpu_bio_bridge_config_t config;

    /* GPU context */
    multigpu_context_t multigpu_ctx;            /**< Multi-GPU context */
    uint32_t device_count;                      /**< Number of GPU devices */

    /* Per-device status */
    gpu_device_status_t* device_status;         /**< Per-device status array */
    float* device_utilization;                  /**< Per-device utilization */
    uint64_t* device_memory_free;               /**< Per-device free memory */

    /* Operation tracking */
    uint64_t next_operation_id;                 /**< Next operation ID */
    uint64_t next_transfer_id;                  /**< Next transfer ID */
    uint32_t pending_operations;                /**< Current pending operations */

    /* Statistics */
    gpu_bio_bridge_stats_t stats;

    /* Timing */
    uint64_t last_broadcast_time_us;            /**< Last broadcast timestamp */
    uint64_t last_rebalance_time_us;            /**< Last rebalance timestamp */

} gpu_bio_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default GPU bio-async bridge configuration
 *
 * WHAT: Provide sensible defaults for GPU-bio-async integration
 * WHY:  Easy initialization with optimized parameters
 * HOW:  Returns configuration optimized for GPU messaging
 *
 * @param config Output configuration structure
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_default_config(gpu_bio_bridge_config_t* config);

/**
 * @brief Create GPU bio-async bridge
 *
 * WHAT: Initialize GPU-bio-async integration bridge
 * WHY:  Enable asynchronous GPU operations via messaging
 * HOW:  Allocate bridge, connect to bio-router, register handlers
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param multigpu_ctx Multi-GPU context (NULL for single-GPU mode)
 * @return Bridge instance or NULL on failure
 */
gpu_bio_bridge_t* gpu_bio_bridge_create(
    const gpu_bio_bridge_config_t* config,
    multigpu_context_t multigpu_ctx
);

/**
 * @brief Destroy GPU bio-async bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Free memory and disconnect from router
 * HOW:  Unregister handlers, free state, destroy mutex
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void gpu_bio_bridge_destroy(gpu_bio_bridge_t* bridge);

/**
 * @brief Initialize GPU bio-async bridge (without allocation)
 *
 * WHAT: Initialize pre-allocated bridge structure
 * WHY:  Allow stack or embedded allocation
 * HOW:  Initialize fields without allocation
 *
 * @param bridge Pre-allocated bridge structure
 * @param config Bridge configuration (NULL for defaults)
 * @param multigpu_ctx Multi-GPU context (NULL for single-GPU)
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_init(
    gpu_bio_bridge_t* bridge,
    const gpu_bio_bridge_config_t* config,
    multigpu_context_t multigpu_ctx
);

/* ============================================================================
 * Bio-Async Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register GPU bridge with bio-async messaging system
 * WHY:  Enable message send/receive for GPU operations
 * HOW:  Register module context, install message handlers
 *
 * @param bridge GPU bio bridge
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_connect_bio_async(gpu_bio_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown of async integration
 * HOW:  Unregister handlers, release module context
 *
 * @param bridge GPU bio bridge
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_disconnect_bio_async(gpu_bio_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge GPU bio bridge
 * @return true if connected to bio-router
 */
bool gpu_bio_bridge_is_bio_async_connected(const gpu_bio_bridge_t* bridge);

/* ============================================================================
 * Compute Message API
 * ============================================================================ */

/**
 * @brief Send GPU compute request
 *
 * WHAT: Submit compute operation via bio-async
 * WHY:  Enable asynchronous GPU computation with messaging
 * HOW:  Create message, send to target module, return promise
 *
 * @param bridge GPU bio bridge
 * @param device_id Target GPU device
 * @param operation Operation type
 * @param data Operation data
 * @param data_size Data size
 * @param priority Priority [0, 1]
 * @return Operation ID (0 on failure)
 */
uint64_t gpu_bio_bridge_send_compute_request(
    gpu_bio_bridge_t* bridge,
    uint32_t device_id,
    gpu_operation_type_t operation,
    const void* data,
    size_t data_size,
    float priority
);

/**
 * @brief Send compute complete notification
 *
 * WHAT: Notify completion of compute operation
 * WHY:  Inform waiting modules of completion
 * HOW:  Send completion message with result
 *
 * @param bridge GPU bio bridge
 * @param operation_id Completed operation ID
 * @param device_id Device that completed
 * @param result_code Result (0 = success)
 * @param elapsed_us Elapsed time in microseconds
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_send_compute_complete(
    gpu_bio_bridge_t* bridge,
    uint64_t operation_id,
    uint32_t device_id,
    int32_t result_code,
    uint64_t elapsed_us
);

/* ============================================================================
 * Transfer Message API
 * ============================================================================ */

/**
 * @brief Send GPU transfer request
 *
 * WHAT: Submit memory transfer via bio-async
 * WHY:  Enable asynchronous GPU transfers with messaging
 * HOW:  Create transfer message, send to transfer module
 *
 * @param bridge GPU bio bridge
 * @param src_device Source device (-1 for host)
 * @param dst_device Destination device (-1 for host)
 * @param src_ptr Source pointer
 * @param dst_ptr Destination pointer
 * @param size Transfer size
 * @param async Asynchronous transfer
 * @return Transfer ID (0 on failure)
 */
uint64_t gpu_bio_bridge_send_transfer_request(
    gpu_bio_bridge_t* bridge,
    int32_t src_device,
    int32_t dst_device,
    const void* src_ptr,
    void* dst_ptr,
    size_t size,
    bool async
);

/**
 * @brief Send transfer complete notification
 *
 * @param bridge GPU bio bridge
 * @param transfer_id Completed transfer ID
 * @param result_code Result (0 = success)
 * @param elapsed_us Elapsed time
 * @param bandwidth_gbps Achieved bandwidth
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_send_transfer_complete(
    gpu_bio_bridge_t* bridge,
    uint64_t transfer_id,
    int32_t result_code,
    uint64_t elapsed_us,
    float bandwidth_gbps
);

/* ============================================================================
 * Status Message API
 * ============================================================================ */

/**
 * @brief Broadcast GPU status update
 *
 * WHAT: Send GPU status to all interested modules
 * WHY:  Enable system-wide GPU state awareness
 * HOW:  Create status message, broadcast to all modules
 *
 * @param bridge GPU bio bridge
 * @param device_id Device to report status for
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_broadcast_status(
    gpu_bio_bridge_t* bridge,
    uint32_t device_id
);

/**
 * @brief Broadcast status for all devices
 *
 * @param bridge GPU bio bridge
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_broadcast_all_status(gpu_bio_bridge_t* bridge);

/**
 * @brief Send GPU error report
 *
 * WHAT: Report GPU error via bio-async
 * WHY:  Enable error handling via messaging
 * HOW:  Send error message on norepinephrine channel
 *
 * @param bridge GPU bio bridge
 * @param device_id Device with error
 * @param error_code Error code
 * @param error_message Error description
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_send_error_report(
    gpu_bio_bridge_t* bridge,
    uint32_t device_id,
    int32_t error_code,
    const char* error_message
);

/* ============================================================================
 * Multi-GPU Coordination API
 * ============================================================================ */

/**
 * @brief Send multi-GPU sync request
 *
 * WHAT: Request synchronization across multiple GPUs
 * WHY:  Coordinate multi-GPU operations via messaging
 * HOW:  Send sync request, wait for all devices
 *
 * @param bridge GPU bio bridge
 * @param device_ids Devices to synchronize (NULL for all)
 * @param device_count Number of devices (0 for all)
 * @param timeout_ms Sync timeout
 * @return Sync ID (0 on failure)
 */
uint64_t gpu_bio_bridge_send_sync_request(
    gpu_bio_bridge_t* bridge,
    const uint32_t* device_ids,
    uint32_t device_count,
    uint32_t timeout_ms
);

/**
 * @brief Send rebalance request
 *
 * WHAT: Request load rebalancing across GPUs
 * WHY:  Optimize multi-GPU utilization via messaging
 * HOW:  Send rebalance request to coordinator
 *
 * @param bridge GPU bio bridge
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_send_rebalance_request(gpu_bio_bridge_t* bridge);

/**
 * @brief Broadcast coordination status
 *
 * WHAT: Broadcast multi-GPU coordination state
 * WHY:  Keep all modules aware of GPU coordination
 * HOW:  Send coordination status message
 *
 * @param bridge GPU bio bridge
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_broadcast_coordination_status(gpu_bio_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get device status
 *
 * @param bridge GPU bio bridge
 * @param device_id Device ID
 * @return Device status
 */
gpu_device_status_t gpu_bio_bridge_get_device_status(
    const gpu_bio_bridge_t* bridge,
    uint32_t device_id
);

/**
 * @brief Get device utilization
 *
 * @param bridge GPU bio bridge
 * @param device_id Device ID
 * @return Utilization [0, 1]
 */
float gpu_bio_bridge_get_device_utilization(
    const gpu_bio_bridge_t* bridge,
    uint32_t device_id
);

/**
 * @brief Get pending operation count
 *
 * @param bridge GPU bio bridge
 * @return Number of pending operations
 */
uint32_t gpu_bio_bridge_get_pending_operations(const gpu_bio_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge GPU bio bridge
 * @param stats Output statistics structure
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_get_stats(
    const gpu_bio_bridge_t* bridge,
    gpu_bio_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge GPU bio bridge
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_reset_stats(gpu_bio_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Process pending messages and update state
 * WHY:  Maintain bridge state and process async operations
 * HOW:  Poll router, update device status, broadcast if needed
 *
 * @param bridge GPU bio bridge
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_update(gpu_bio_bridge_t* bridge);

/**
 * @brief Process incoming messages
 *
 * WHAT: Handle pending messages in inbox
 * WHY:  Process GPU operation messages
 * HOW:  Call bio_router_process_inbox for GPU module
 *
 * @param bridge GPU bio bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t gpu_bio_bridge_process_messages(
    gpu_bio_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Message Handler Registration
 * ============================================================================ */

/**
 * @brief Register handler for GPU message type
 *
 * WHAT: Register custom handler for GPU messages
 * WHY:  Allow modules to handle GPU messages
 * HOW:  Register handler with bio-router
 *
 * @param bridge GPU bio bridge
 * @param msg_type GPU message type
 * @param handler Handler function
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_register_handler(
    gpu_bio_bridge_t* bridge,
    gpu_message_type_t msg_type,
    bio_message_handler_t handler
);

/**
 * @brief Unregister handler for GPU message type
 *
 * @param bridge GPU bio bridge
 * @param msg_type GPU message type
 * @return 0 on success, error code on failure
 */
int gpu_bio_bridge_unregister_handler(
    gpu_bio_bridge_t* bridge,
    gpu_message_type_t msg_type
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get GPU message type name
 */
static inline const char* gpu_msg_type_name(gpu_message_type_t type) {
    switch (type) {
        case GPU_MSG_COMPUTE_REQUEST:           return "COMPUTE_REQUEST";
        case GPU_MSG_COMPUTE_COMPLETE:          return "COMPUTE_COMPLETE";
        case GPU_MSG_COMPUTE_FAILED:            return "COMPUTE_FAILED";
        case GPU_MSG_TRANSFER_REQUEST:          return "TRANSFER_REQUEST";
        case GPU_MSG_TRANSFER_COMPLETE:         return "TRANSFER_COMPLETE";
        case GPU_MSG_STATUS_QUERY:              return "STATUS_QUERY";
        case GPU_MSG_STATUS_RESPONSE:           return "STATUS_RESPONSE";
        case GPU_MSG_MULTIGPU_SYNC_REQUEST:     return "MULTIGPU_SYNC_REQUEST";
        case GPU_MSG_MULTIGPU_SYNC_COMPLETE:    return "MULTIGPU_SYNC_COMPLETE";
        case GPU_MSG_REBALANCE_REQUEST:         return "REBALANCE_REQUEST";
        case GPU_MSG_REBALANCE_COMPLETE:        return "REBALANCE_COMPLETE";
        case GPU_MSG_ERROR_REPORT:              return "ERROR_REPORT";
        default:                                return "UNKNOWN";
    }
}

/**
 * @brief Get GPU device status name
 */
static inline const char* gpu_status_name(gpu_device_status_t status) {
    switch (status) {
        case GPU_STATUS_UNKNOWN:    return "unknown";
        case GPU_STATUS_OFFLINE:    return "offline";
        case GPU_STATUS_IDLE:       return "idle";
        case GPU_STATUS_BUSY:       return "busy";
        case GPU_STATUS_OVERLOADED: return "overloaded";
        case GPU_STATUS_ERROR:      return "error";
        case GPU_STATUS_RESETTING:  return "resetting";
        default:                    return "unknown";
    }
}

/**
 * @brief Get GPU operation type name
 */
static inline const char* gpu_op_type_name(gpu_operation_type_t op) {
    switch (op) {
        case GPU_OP_COMPUTE:        return "compute";
        case GPU_OP_TRANSFER_H2D:   return "transfer_h2d";
        case GPU_OP_TRANSFER_D2H:   return "transfer_d2h";
        case GPU_OP_TRANSFER_D2D:   return "transfer_d2d";
        case GPU_OP_SYNC:           return "sync";
        case GPU_OP_BROADCAST:      return "broadcast";
        case GPU_OP_GATHER:         return "gather";
        case GPU_OP_PARTITION:      return "partition";
        case GPU_OP_REBALANCE:      return "rebalance";
        case GPU_OP_NEURON_STEP:    return "neuron_step";
        case GPU_OP_SYNAPSE_UPDATE: return "synapse_update";
        case GPU_OP_SPIKE_PROPAGATE: return "spike_propagate";
        case GPU_OP_PLASTICITY:     return "plasticity";
        case GPU_OP_CUSTOM:         return "custom";
        default:                    return "unknown";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GPU_BIO_ASYNC_BRIDGE_H */
