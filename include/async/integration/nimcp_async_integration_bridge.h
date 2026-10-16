/**
 * @file nimcp_async_integration_bridge.h
 * @brief Async Integration Bridge - Central coordinator for all async operations
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Integration bridge connecting all async-related modules to bio-async system
 * WHY:  Unified coordination of promises, futures, task queues, and async messaging
 * HOW:  Central message routing, status tracking, and coordination primitives
 *
 * BIOLOGICAL BASIS:
 * =================
 * The brain coordinates asynchronous operations across multiple timescales:
 * - Fast synaptic signaling (milliseconds)
 * - Neuromodulator diffusion (seconds)
 * - Glial wave propagation (minutes)
 * - Circadian rhythms (hours)
 *
 * This bridge models the brain's multi-level coordination mechanisms:
 * - Task queues act like neural pathways with priority routing
 * - Promises/futures model expectation and prediction
 * - Phase coupling coordinates temporal synchronization
 * - Message types enable typed inter-module communication
 *
 * ARCHITECTURE:
 * ```
 * +====================================================================+
 * |              ASYNC INTEGRATION BRIDGE                              |
 * +====================================================================+
 * |                                                                    |
 * |  +------------------+    +------------------+    +---------------+ |
 * |  | Promise/Future   |    | Task Queue       |    | Phase Sync    | |
 * |  | Management       |    | Coordination     |    | Management    | |
 * |  |                  |    |                  |    |               | |
 * |  | - Track active   |    | - Priority queues|    | - Oscillator  | |
 * |  | - Confidence     |    | - Dispatch       |    |   coupling    | |
 * |  | - Decay tracking |    | - Load balance   |    | - Coherence   | |
 * |  +--------+---------+    +--------+---------+    +-------+-------+ |
 * |           |                       |                      |         |
 * |           +-----------+-----------+----------+-----------+         |
 * |                       |                      |                     |
 * |                       v                      v                     |
 * |  +--------------------+----------------------+-------------------+ |
 * |  |                 BIO-ASYNC ROUTER                              | |
 * |  |  - Message dispatch    - Handler registration                 | |
 * |  |  - Predictive protocol - Glial wave coordination              | |
 * |  +---------------------------------------------------------------+ |
 * |                                                                    |
 * +====================================================================+
 * ```
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

#ifndef NIMCP_ASYNC_INTEGRATION_BRIDGE_H
#define NIMCP_ASYNC_INTEGRATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_future.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Module identification */
#define ASYNC_INTEGRATION_MODULE_NAME       "async_integration_bridge"
#define ASYNC_INTEGRATION_MODULE_VERSION    "1.0.0"

/** Bio-async module ID for async integration bridge */
#define BIO_MODULE_ASYNC_INTEGRATION        0x0700

/** Default configuration values */
#define ASYNC_INTEGRATION_DEFAULT_MAX_TASKS             256
#define ASYNC_INTEGRATION_DEFAULT_MAX_PROMISES          512
#define ASYNC_INTEGRATION_DEFAULT_MAX_FUTURES           1024
#define ASYNC_INTEGRATION_DEFAULT_QUEUE_CAPACITY        128
#define ASYNC_INTEGRATION_DEFAULT_UPDATE_INTERVAL_MS    10
#define ASYNC_INTEGRATION_DEFAULT_COORDINATION_TIMEOUT  5000

/** Priority queue levels */
#define ASYNC_INTEGRATION_PRIORITY_LEVELS   4
#define ASYNC_INTEGRATION_PRIORITY_CRITICAL 0
#define ASYNC_INTEGRATION_PRIORITY_HIGH     1
#define ASYNC_INTEGRATION_PRIORITY_NORMAL   2
#define ASYNC_INTEGRATION_PRIORITY_LOW      3

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define ASYNC_INTEGRATION_ERROR_BASE                    11000
#define ASYNC_INTEGRATION_ERROR_NOT_INITIALIZED         (ASYNC_INTEGRATION_ERROR_BASE + 1)
#define ASYNC_INTEGRATION_ERROR_INVALID_CONFIG          (ASYNC_INTEGRATION_ERROR_BASE + 2)
#define ASYNC_INTEGRATION_ERROR_QUEUE_FULL              (ASYNC_INTEGRATION_ERROR_BASE + 3)
#define ASYNC_INTEGRATION_ERROR_PROMISE_LIMIT           (ASYNC_INTEGRATION_ERROR_BASE + 4)
#define ASYNC_INTEGRATION_ERROR_FUTURE_LIMIT            (ASYNC_INTEGRATION_ERROR_BASE + 5)
#define ASYNC_INTEGRATION_ERROR_TASK_LIMIT              (ASYNC_INTEGRATION_ERROR_BASE + 6)
#define ASYNC_INTEGRATION_ERROR_COORDINATION_TIMEOUT    (ASYNC_INTEGRATION_ERROR_BASE + 7)
#define ASYNC_INTEGRATION_ERROR_NOT_CONNECTED           (ASYNC_INTEGRATION_ERROR_BASE + 8)
#define ASYNC_INTEGRATION_ERROR_ALREADY_RUNNING         (ASYNC_INTEGRATION_ERROR_BASE + 9)
#define ASYNC_INTEGRATION_ERROR_DISPATCH_FAILED         (ASYNC_INTEGRATION_ERROR_BASE + 10)

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct async_integration_bridge async_integration_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Async coordination message types
 *
 * WHAT: Message types for async coordination across modules
 * WHY:  Enable typed communication for task management and synchronization
 * HOW:  Message categories for different async operations
 */
typedef enum {
    /* Task coordination messages (0x0700 - 0x070F) */
    ASYNC_MSG_TASK_SUBMIT = 0x0700,         /**< Submit task to queue */
    ASYNC_MSG_TASK_COMPLETE,                /**< Task completed */
    ASYNC_MSG_TASK_FAILED,                  /**< Task execution failed */
    ASYNC_MSG_TASK_CANCELLED,               /**< Task was cancelled */
    ASYNC_MSG_TASK_PROGRESS,                /**< Task progress update */
    ASYNC_MSG_TASK_PRIORITY_CHANGE,         /**< Task priority changed */

    /* Promise/Future coordination (0x0710 - 0x071F) */
    ASYNC_MSG_PROMISE_CREATED = 0x0710,     /**< New promise created */
    ASYNC_MSG_PROMISE_RESOLVED,             /**< Promise resolved with value */
    ASYNC_MSG_PROMISE_REJECTED,             /**< Promise rejected with error */
    ASYNC_MSG_FUTURE_WAIT_START,            /**< Future wait operation started */
    ASYNC_MSG_FUTURE_WAIT_COMPLETE,         /**< Future wait completed */
    ASYNC_MSG_FUTURE_CONFIDENCE_UPDATE,     /**< Future confidence decayed */

    /* Queue coordination (0x0720 - 0x072F) */
    ASYNC_MSG_QUEUE_STATUS = 0x0720,        /**< Queue status update */
    ASYNC_MSG_QUEUE_BACKPRESSURE,           /**< Queue experiencing backpressure */
    ASYNC_MSG_QUEUE_DRAINED,                /**< Queue emptied */
    ASYNC_MSG_LOAD_BALANCE_REQUEST,         /**< Request load balancing */
    ASYNC_MSG_LOAD_BALANCE_RESPONSE,        /**< Load balancing result */

    /* Phase synchronization (0x0730 - 0x073F) */
    ASYNC_MSG_PHASE_SYNC_REQUEST = 0x0730,  /**< Request phase synchronization */
    ASYNC_MSG_PHASE_SYNC_ACHIEVED,          /**< Phase coherence achieved */
    ASYNC_MSG_PHASE_SYNC_TIMEOUT,           /**< Phase sync timed out */
    ASYNC_MSG_COHERENCE_UPDATE,             /**< Coherence level update */

    /* Coordination control (0x0740 - 0x074F) */
    ASYNC_MSG_COORD_START = 0x0740,         /**< Start coordination */
    ASYNC_MSG_COORD_STOP,                   /**< Stop coordination */
    ASYNC_MSG_COORD_PAUSE,                  /**< Pause coordination */
    ASYNC_MSG_COORD_RESUME,                 /**< Resume coordination */
    ASYNC_MSG_COORD_STATUS_QUERY,           /**< Query coordination status */
    ASYNC_MSG_COORD_STATUS_RESPONSE         /**< Coordination status response */
} async_coordination_msg_type_t;

/**
 * @brief Async operation state
 *
 * WHAT: States for tracked async operations
 * WHY:  Monitor lifecycle of async tasks and promises
 * HOW:  State machine for operation tracking
 */
typedef enum {
    ASYNC_OP_STATE_IDLE = 0,            /**< Not yet started */
    ASYNC_OP_STATE_PENDING,             /**< Waiting for execution */
    ASYNC_OP_STATE_RUNNING,             /**< Currently executing */
    ASYNC_OP_STATE_COMPLETED,           /**< Successfully completed */
    ASYNC_OP_STATE_FAILED,              /**< Failed with error */
    ASYNC_OP_STATE_CANCELLED,           /**< Cancelled before completion */
    ASYNC_OP_STATE_TIMEOUT              /**< Timed out */
} async_op_state_t;

/**
 * @brief Integration bridge operational mode
 *
 * WHAT: How the bridge processes async operations
 * WHY:  Different use cases need different automation levels
 * HOW:  Controls dispatch and coordination behavior
 */
typedef enum {
    ASYNC_INTEGRATION_MODE_DISABLED = 0,    /**< Bridge disabled */
    ASYNC_INTEGRATION_MODE_MONITOR,         /**< Monitor only, no dispatch */
    ASYNC_INTEGRATION_MODE_MANUAL,          /**< Manual dispatch control */
    ASYNC_INTEGRATION_MODE_AUTOMATIC,       /**< Automatic dispatch */
    ASYNC_INTEGRATION_MODE_COORDINATED      /**< Full coordination with other bridges */
} async_integration_mode_t;

/**
 * @brief Task dispatch policy
 *
 * WHAT: How tasks are dispatched from queues
 * WHY:  Different workloads benefit from different policies
 * HOW:  Controls task selection from priority queues
 */
typedef enum {
    ASYNC_DISPATCH_FIFO = 0,            /**< First-in-first-out */
    ASYNC_DISPATCH_PRIORITY,            /**< Strict priority ordering */
    ASYNC_DISPATCH_FAIR,                /**< Fair scheduling across priorities */
    ASYNC_DISPATCH_WEIGHTED             /**< Weighted priority scheduling */
} async_dispatch_policy_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Task execution callback
 *
 * @param task_id Task identifier
 * @param task_data Task data pointer
 * @param task_size Task data size
 * @param user_data User context
 * @return 0 on success, negative on error
 */
typedef int (*async_task_callback_t)(
    uint64_t task_id,
    const void* task_data,
    size_t task_size,
    void* user_data
);

/**
 * @brief Task completion callback
 *
 * @param task_id Task identifier
 * @param result Task result (NULL if failed)
 * @param result_size Result size
 * @param error Error code (0 if success)
 * @param user_data User context
 */
typedef void (*async_completion_callback_t)(
    uint64_t task_id,
    const void* result,
    size_t result_size,
    int error,
    void* user_data
);

/**
 * @brief Coordination event callback
 *
 * @param event_type Event message type
 * @param event_data Event data
 * @param event_size Event data size
 * @param user_data User context
 */
typedef void (*async_coord_event_callback_t)(
    async_coordination_msg_type_t event_type,
    const void* event_data,
    size_t event_size,
    void* user_data
);

/* ============================================================================
 * Message Structures
 * ============================================================================ */

/**
 * @brief Task submission message
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */
    uint64_t task_id;                   /**< Unique task identifier */
    uint32_t priority;                  /**< Task priority level */
    uint32_t flags;                     /**< Task flags */
    uint64_t timeout_ms;                /**< Task timeout (0 = no timeout) */
    size_t data_size;                   /**< Task data size */
    /* Followed by task data */
} async_task_submit_msg_t;

/**
 * @brief Task completion message
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */
    uint64_t task_id;                   /**< Task identifier */
    async_op_state_t final_state;       /**< Final task state */
    int32_t error_code;                 /**< Error code if failed */
    uint64_t execution_time_us;         /**< Execution time in microseconds */
    size_t result_size;                 /**< Result data size */
    /* Followed by result data */
} async_task_complete_msg_t;

/**
 * @brief Promise status message
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */
    uint64_t promise_id;                /**< Promise identifier */
    nimcp_bio_channel_type_t channel;   /**< Neuromodulator channel */
    float confidence;                   /**< Current confidence level */
    nimcp_bio_future_state_t state;     /**< Promise state */
} async_promise_status_msg_t;

/**
 * @brief Queue status message
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */
    uint32_t queue_id;                  /**< Queue identifier */
    uint32_t pending_count;             /**< Pending tasks */
    uint32_t running_count;             /**< Running tasks */
    uint32_t capacity;                  /**< Queue capacity */
    float utilization;                  /**< Queue utilization [0-1] */
} async_queue_status_msg_t;

/**
 * @brief Phase synchronization request message
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */
    nimcp_oscillation_band_t band;      /**< Oscillation band */
    float target_coherence;             /**< Target coherence threshold */
    uint32_t participant_count;         /**< Number of participants */
    uint64_t timeout_ms;                /**< Sync timeout */
} async_phase_sync_msg_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Per-priority queue configuration
 */
typedef struct {
    uint32_t capacity;                  /**< Queue capacity */
    float weight;                       /**< Scheduling weight (for weighted policy) */
    uint32_t max_concurrent;            /**< Max concurrent tasks from this queue */
} async_queue_config_t;

/**
 * @brief Async integration bridge configuration
 */
typedef struct {
    /* Operation mode */
    async_integration_mode_t mode;      /**< Operational mode */
    async_dispatch_policy_t dispatch_policy; /**< Task dispatch policy */

    /* Capacity limits */
    uint32_t max_tasks;                 /**< Maximum tracked tasks */
    uint32_t max_promises;              /**< Maximum tracked promises */
    uint32_t max_futures;               /**< Maximum tracked futures */

    /* Queue configuration */
    async_queue_config_t priority_queues[ASYNC_INTEGRATION_PRIORITY_LEVELS];

    /* Timing */
    uint32_t update_interval_ms;        /**< Update loop interval */
    uint32_t coordination_timeout_ms;   /**< Default coordination timeout */
    uint32_t task_default_timeout_ms;   /**< Default task timeout */

    /* Bio-async integration */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
    nimcp_oscillation_band_t default_band;    /**< Default oscillation band */

    /* Phase coupling */
    float coherence_threshold;          /**< Default coherence threshold */
    float coupling_strength;            /**< Phase coupling strength */

    /* Features */
    bool enable_predictive_dispatch;    /**< Predictive task dispatch */
    bool enable_load_balancing;         /**< Cross-module load balancing */
    bool enable_backpressure;           /**< Enable backpressure handling */
    bool enable_statistics;             /**< Track detailed statistics */
    bool enable_logging;                /**< Enable debug logging */
} async_integration_config_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief Per-queue state
 */
typedef struct {
    uint32_t pending_count;             /**< Pending tasks in queue */
    uint32_t running_count;             /**< Currently running tasks */
    uint32_t completed_count;           /**< Completed tasks (since reset) */
    uint32_t failed_count;              /**< Failed tasks (since reset) */
    float avg_wait_time_ms;             /**< Average wait time in queue */
    float avg_execution_time_ms;        /**< Average execution time */
} async_queue_state_t;

/**
 * @brief Promise/Future tracking state
 */
typedef struct {
    uint32_t active_promises;           /**< Currently active promises */
    uint32_t active_futures;            /**< Currently active futures */
    uint32_t pending_futures;           /**< Futures waiting for results */
    uint32_t decayed_futures;           /**< Futures that decayed */
    float avg_confidence;               /**< Average future confidence */
    float avg_wait_time_ms;             /**< Average future wait time */
} async_promise_state_t;

/**
 * @brief Phase synchronization state
 */
typedef struct {
    uint32_t active_sync_groups;        /**< Active sync groups */
    float current_coherence;            /**< Current global coherence */
    float mean_phase;                   /**< Mean phase angle */
    uint32_t sync_achieved_count;       /**< Successful syncs */
    uint32_t sync_timeout_count;        /**< Timed out syncs */
} async_phase_state_t;

/**
 * @brief Complete bridge state snapshot
 */
typedef struct {
    async_integration_mode_t mode;      /**< Current mode */
    bool running;                       /**< Bridge is running */
    bool bio_async_connected;           /**< Bio-async connected */

    /* Queue states */
    async_queue_state_t queues[ASYNC_INTEGRATION_PRIORITY_LEVELS];
    uint32_t total_pending_tasks;       /**< Total pending across all queues */
    uint32_t total_running_tasks;       /**< Total running tasks */

    /* Promise/Future state */
    async_promise_state_t promises;     /**< Promise/future state */

    /* Phase state */
    async_phase_state_t phase;          /**< Phase sync state */

    /* Timing */
    uint64_t uptime_ms;                 /**< Bridge uptime */
    uint64_t last_update_ms;            /**< Last update timestamp */
    uint64_t tasks_dispatched;          /**< Total tasks dispatched */
} async_integration_state_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Async integration bridge statistics
 */
typedef struct {
    /* Task statistics */
    uint64_t tasks_submitted;           /**< Total tasks submitted */
    uint64_t tasks_completed;           /**< Tasks completed successfully */
    uint64_t tasks_failed;              /**< Tasks that failed */
    uint64_t tasks_cancelled;           /**< Tasks cancelled */
    uint64_t tasks_timeout;             /**< Tasks that timed out */
    float avg_task_latency_ms;          /**< Average task latency */
    float max_task_latency_ms;          /**< Maximum task latency */

    /* Promise/Future statistics */
    uint64_t promises_created;          /**< Total promises created */
    uint64_t promises_resolved;         /**< Promises resolved */
    uint64_t promises_rejected;         /**< Promises rejected */
    uint64_t futures_waited;            /**< Future wait operations */
    uint64_t futures_timeout;           /**< Future wait timeouts */
    float avg_future_confidence;        /**< Average confidence at resolution */

    /* Queue statistics */
    uint64_t queue_enqueues;            /**< Total enqueue operations */
    uint64_t queue_dequeues;            /**< Total dequeue operations */
    uint64_t backpressure_events;       /**< Backpressure triggered count */
    float avg_queue_depth;              /**< Average queue depth */
    float max_queue_depth;              /**< Maximum queue depth seen */

    /* Phase sync statistics */
    uint64_t phase_sync_requests;       /**< Phase sync requests */
    uint64_t phase_sync_achieved;       /**< Successful phase syncs */
    uint64_t phase_sync_timeouts;       /**< Phase sync timeouts */
    float avg_coherence;                /**< Average achieved coherence */
    float avg_sync_time_ms;             /**< Average sync time */

    /* Message statistics */
    uint64_t messages_sent;             /**< Total messages sent */
    uint64_t messages_received;         /**< Total messages received */
    uint64_t dispatch_errors;           /**< Message dispatch errors */

    /* Timing */
    float avg_update_time_us;           /**< Average update cycle time */
    float max_update_time_us;           /**< Maximum update cycle time */
    uint64_t total_updates;             /**< Total update cycles */
} async_integration_stats_t;

/* ============================================================================
 * Main Structure
 * ============================================================================ */

/**
 * @brief Async integration bridge internal structure
 *
 * NOTE: Opaque structure - access via API only
 */
struct async_integration_bridge {
    async_integration_config_t config;  /**< Configuration */
    bool running;                       /**< Running state */
    bool initialized;                   /**< Initialization state */

    /* Bio-async integration */
    bio_module_context_t bio_context;   /**< Bio-async module context */
    bool bio_async_connected;           /**< Bio-async connection state */

    /* Task tracking */
    void* task_registry;                /**< Task registry (internal) */
    uint32_t active_task_count;         /**< Active tasks */

    /* Promise/Future tracking */
    void* promise_registry;             /**< Promise registry (internal) */
    void* future_registry;              /**< Future registry (internal) */

    /* Priority queues */
    void* priority_queues[ASYNC_INTEGRATION_PRIORITY_LEVELS]; /**< Task queues */

    /* Phase synchronization */
    void* phase_sync_groups;            /**< Phase sync groups */

    /* Statistics */
    async_integration_stats_t stats;    /**< Statistics */

    /* Callbacks */
    async_coord_event_callback_t event_callback;    /**< Event callback */
    void* event_callback_data;                      /**< Event callback user data */

    /* Timing */
    uint64_t start_time_ms;             /**< Start timestamp */
    uint64_t last_update_ms;            /**< Last update timestamp */

    /* Thread safety */
    nimcp_mutex_t* mutex;               /**< Thread-safe operations */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default async integration configuration
 *
 * WHAT: Populate config with sensible defaults
 * WHY:  Easy initialization with biologically-plausible settings
 * HOW:  Set default capacities, timeouts, and enable flags
 *
 * @param config Configuration to populate (must not be NULL)
 * @return 0 on success, -1 on error
 */
int async_integration_default_config(async_integration_config_t* config);

/**
 * @brief Create async integration bridge
 *
 * WHAT: Allocate and initialize the async integration bridge
 * WHY:  Central coordination point for all async operations
 * HOW:  Allocate structures, initialize queues, set up registries
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
async_integration_t* async_integration_create(
    const async_integration_config_t* config
);

/**
 * @brief Destroy async integration bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Stop if running, free all internal structures
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void async_integration_destroy(async_integration_t* bridge);

/**
 * @brief Start async integration bridge
 *
 * WHAT: Begin async coordination operations
 * WHY:  Activate the bridge for task dispatch and coordination
 * HOW:  Connect bio-async, start update loop
 *
 * @param bridge Bridge to start
 * @return 0 on success, negative on error
 */
int async_integration_start(async_integration_t* bridge);

/**
 * @brief Stop async integration bridge
 *
 * WHAT: Halt async coordination operations
 * WHY:  Graceful shutdown
 * HOW:  Disconnect bio-async, stop dispatch
 *
 * @param bridge Bridge to stop
 * @return 0 on success, negative on error
 */
int async_integration_stop(async_integration_t* bridge);

/* ============================================================================
 * Task Coordination API
 * ============================================================================ */

/**
 * @brief Submit task to coordination queue
 *
 * WHAT: Add task to priority queue for dispatch
 * WHY:  Centralized task management with priority
 * HOW:  Enqueue task, track in registry, dispatch when ready
 *
 * @param bridge Bridge instance
 * @param priority Priority level (0=critical, 3=low)
 * @param task_data Task data
 * @param task_size Task data size
 * @param callback Execution callback
 * @param completion Completion callback (may be NULL)
 * @param user_data User context for callbacks
 * @param timeout_ms Task timeout (0 = use default)
 * @return Task ID on success, 0 on failure
 */
uint64_t async_integration_submit_task(
    async_integration_t* bridge,
    uint32_t priority,
    const void* task_data,
    size_t task_size,
    async_task_callback_t callback,
    async_completion_callback_t completion,
    void* user_data,
    uint64_t timeout_ms
);

/**
 * @brief Cancel pending task
 *
 * WHAT: Cancel a task before execution
 * WHY:  Allow task cancellation for cleanup
 * HOW:  Remove from queue, mark as cancelled
 *
 * @param bridge Bridge instance
 * @param task_id Task identifier
 * @return 0 on success, -1 if not found or already running
 */
int async_integration_cancel_task(
    async_integration_t* bridge,
    uint64_t task_id
);

/**
 * @brief Get task state
 *
 * WHAT: Query current state of a task
 * WHY:  Monitor task progress
 * HOW:  Lookup in registry
 *
 * @param bridge Bridge instance
 * @param task_id Task identifier
 * @return Task state or ASYNC_OP_STATE_IDLE if not found
 */
async_op_state_t async_integration_get_task_state(
    const async_integration_t* bridge,
    uint64_t task_id
);

/**
 * @brief Update task priority
 *
 * WHAT: Change priority of pending task
 * WHY:  Dynamic priority adjustment
 * HOW:  Move task between priority queues
 *
 * @param bridge Bridge instance
 * @param task_id Task identifier
 * @param new_priority New priority level
 * @return 0 on success, -1 on error
 */
int async_integration_update_task_priority(
    async_integration_t* bridge,
    uint64_t task_id,
    uint32_t new_priority
);

/* ============================================================================
 * Promise/Future Management API
 * ============================================================================ */

/**
 * @brief Register promise for tracking
 *
 * WHAT: Register bio-promise with integration bridge
 * WHY:  Centralized promise tracking and coordination
 * HOW:  Add to registry, track confidence decay
 *
 * @param bridge Bridge instance
 * @param promise Bio-promise to track
 * @param channel Neuromodulator channel
 * @return Internal promise ID on success, 0 on failure
 */
uint64_t async_integration_register_promise(
    async_integration_t* bridge,
    nimcp_bio_promise_t promise,
    nimcp_bio_channel_type_t channel
);

/**
 * @brief Register future for tracking
 *
 * WHAT: Register bio-future with integration bridge
 * WHY:  Centralized future tracking and coordination
 * HOW:  Add to registry, monitor confidence
 *
 * @param bridge Bridge instance
 * @param future Bio-future to track
 * @return Internal future ID on success, 0 on failure
 */
uint64_t async_integration_register_future(
    async_integration_t* bridge,
    nimcp_bio_future_t future
);

/**
 * @brief Get active promise count
 *
 * WHAT: Query number of active promises
 * WHY:  Resource monitoring
 * HOW:  Return registry count
 *
 * @param bridge Bridge instance
 * @return Number of active promises
 */
uint32_t async_integration_get_active_promises(
    const async_integration_t* bridge
);

/**
 * @brief Get active future count
 *
 * WHAT: Query number of active futures
 * WHY:  Resource monitoring
 * HOW:  Return registry count
 *
 * @param bridge Bridge instance
 * @return Number of active futures
 */
uint32_t async_integration_get_active_futures(
    const async_integration_t* bridge
);

/**
 * @brief Get average future confidence
 *
 * WHAT: Query average confidence across all futures
 * WHY:  Monitor system confidence health
 * HOW:  Compute mean confidence from registry
 *
 * @param bridge Bridge instance
 * @return Average confidence [0-1]
 */
float async_integration_get_avg_confidence(
    const async_integration_t* bridge
);

/* ============================================================================
 * Phase Synchronization API
 * ============================================================================ */

/**
 * @brief Create phase synchronization group
 *
 * WHAT: Create new phase sync group for coordinated operations
 * WHY:  Enable synchronized async operations
 * HOW:  Allocate sync group with oscillation band
 *
 * @param bridge Bridge instance
 * @param band Oscillation band for synchronization
 * @return Sync group ID on success, 0 on failure
 */
uint32_t async_integration_create_sync_group(
    async_integration_t* bridge,
    nimcp_oscillation_band_t band
);

/**
 * @brief Add future to sync group
 *
 * WHAT: Add future to phase sync group
 * WHY:  Include future in synchronized wait
 * HOW:  Register future with sync group
 *
 * @param bridge Bridge instance
 * @param sync_group_id Sync group identifier
 * @param future_id Future identifier (from register_future)
 * @return 0 on success, negative on error
 */
int async_integration_add_to_sync_group(
    async_integration_t* bridge,
    uint32_t sync_group_id,
    uint64_t future_id
);

/**
 * @brief Wait for phase coherence
 *
 * WHAT: Block until sync group reaches coherence
 * WHY:  Synchronized completion of multiple futures
 * HOW:  Monitor phase coherence, return when threshold met
 *
 * @param bridge Bridge instance
 * @param sync_group_id Sync group identifier
 * @param coherence_threshold Target coherence [0-1]
 * @param timeout_ms Timeout (0 = use default)
 * @return 0 on success, negative on error/timeout
 */
int async_integration_wait_coherence(
    async_integration_t* bridge,
    uint32_t sync_group_id,
    float coherence_threshold,
    uint64_t timeout_ms
);

/**
 * @brief Get current coherence of sync group
 *
 * WHAT: Query coherence level of sync group
 * WHY:  Monitor synchronization progress
 * HOW:  Compute Kuramoto order parameter
 *
 * @param bridge Bridge instance
 * @param sync_group_id Sync group identifier
 * @return Coherence level [0-1] or -1 on error
 */
float async_integration_get_coherence(
    const async_integration_t* bridge,
    uint32_t sync_group_id
);

/**
 * @brief Destroy sync group
 *
 * WHAT: Clean up sync group resources
 * WHY:  Resource management
 * HOW:  Remove from registry, free memory
 *
 * @param bridge Bridge instance
 * @param sync_group_id Sync group identifier
 * @return 0 on success, negative on error
 */
int async_integration_destroy_sync_group(
    async_integration_t* bridge,
    uint32_t sync_group_id
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async messaging
 * WHY:  Enable inter-module coordination
 * HOW:  Register module, set up handlers
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int async_integration_connect_bio_async(async_integration_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on error
 */
int async_integration_disconnect_bio_async(async_integration_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * WHAT: Query bio-async connection state
 * WHY:  Verify messaging is available
 * HOW:  Return internal flag
 *
 * @param bridge Bridge instance
 * @return true if connected, false otherwise
 */
bool async_integration_is_bio_async_connected(
    const async_integration_t* bridge
);

/**
 * @brief Send coordination message
 *
 * WHAT: Send async coordination message via bio-async
 * WHY:  Inter-module coordination
 * HOW:  Route through bio-async router
 *
 * @param bridge Bridge instance
 * @param msg_type Message type
 * @param target Target module ID (BIO_MODULE_BROADCAST for all)
 * @param data Message data
 * @param size Data size
 * @return 0 on success, negative on error
 */
int async_integration_send_message(
    async_integration_t* bridge,
    async_coordination_msg_type_t msg_type,
    bio_module_id_t target,
    const void* data,
    size_t size
);

/* ============================================================================
 * Update and Processing API
 * ============================================================================ */

/**
 * @brief Process pending operations
 *
 * WHAT: Run one update cycle of the bridge
 * WHY:  Process queues, dispatch tasks, update state
 * HOW:  Dispatch ready tasks, update statistics
 *
 * @param bridge Bridge instance
 * @param dt_ms Time since last update (milliseconds)
 * @return Number of operations processed, negative on error
 */
int async_integration_update(
    async_integration_t* bridge,
    float dt_ms
);

/**
 * @brief Process incoming bio-async messages
 *
 * WHAT: Process messages from bio-async inbox
 * WHY:  Handle coordination messages from other modules
 * HOW:  Read from inbox, dispatch to handlers
 *
 * @param bridge Bridge instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t async_integration_process_messages(
    async_integration_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Callback Registration API
 * ============================================================================ */

/**
 * @brief Register coordination event callback
 *
 * WHAT: Register callback for coordination events
 * WHY:  Allow external monitoring of coordination state
 * HOW:  Store callback, invoke on events
 *
 * @param bridge Bridge instance
 * @param callback Event callback function
 * @param user_data User context
 * @return 0 on success, negative on error
 */
int async_integration_set_event_callback(
    async_integration_t* bridge,
    async_coord_event_callback_t callback,
    void* user_data
);

/* ============================================================================
 * State and Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge state snapshot
 *
 * WHAT: Retrieve current bridge state
 * WHY:  State inspection and monitoring
 * HOW:  Copy internal state to output
 *
 * @param bridge Bridge instance
 * @param state Output state structure
 * @return 0 on success, negative on error
 */
int async_integration_get_state(
    const async_integration_t* bridge,
    async_integration_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve accumulated statistics
 * WHY:  Performance monitoring
 * HOW:  Copy internal stats to output
 *
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int async_integration_get_stats(
    const async_integration_t* bridge,
    async_integration_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero all stat counters
 *
 * @param bridge Bridge instance
 */
void async_integration_reset_stats(async_integration_t* bridge);

/**
 * @brief Get queue depth for priority level
 *
 * WHAT: Query pending task count in queue
 * WHY:  Monitor queue utilization
 * HOW:  Return queue pending count
 *
 * @param bridge Bridge instance
 * @param priority Priority level
 * @return Pending count or 0 if invalid
 */
uint32_t async_integration_get_queue_depth(
    const async_integration_t* bridge,
    uint32_t priority
);

/**
 * @brief Get total pending tasks across all queues
 *
 * WHAT: Query total pending task count
 * WHY:  Overall load monitoring
 * HOW:  Sum across all priority queues
 *
 * @param bridge Bridge instance
 * @return Total pending tasks
 */
uint32_t async_integration_get_total_pending(
    const async_integration_t* bridge
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Convert operation state to string
 *
 * @param state Operation state
 * @return Human-readable string
 */
const char* async_op_state_to_string(async_op_state_t state);

/**
 * @brief Convert integration mode to string
 *
 * @param mode Integration mode
 * @return Human-readable string
 */
const char* async_integration_mode_to_string(async_integration_mode_t mode);

/**
 * @brief Convert dispatch policy to string
 *
 * @param policy Dispatch policy
 * @return Human-readable string
 */
const char* async_dispatch_policy_to_string(async_dispatch_policy_t policy);

/**
 * @brief Convert coordination message type to string
 *
 * @param msg_type Message type
 * @return Human-readable string
 */
const char* async_coord_msg_to_string(async_coordination_msg_type_t msg_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASYNC_INTEGRATION_BRIDGE_H */
