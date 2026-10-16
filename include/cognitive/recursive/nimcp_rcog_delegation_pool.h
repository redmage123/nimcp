/**
 * @file nimcp_rcog_delegation_pool.h
 * @brief Recursive Cognition Delegation Pool - Worker Management and Task Execution
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Manages worker threads that execute delegated subtasks
 * WHY:  Maps to RLM's "sub-workers" that have access to specific tool tiers
 * HOW:  Thread pool with capability-tiered workers, work stealing, load balancing
 *
 * BIOLOGICAL BASIS:
 * The delegation pool models cortical column processing:
 * - Workers represent cortical minicolumns
 * - Capability tiers reflect cortical layer specialization
 * - Work stealing mirrors lateral inhibition/excitation
 * - Load balancing reflects metabolic resource allocation
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+     +----------------------+
 * |    ORCHESTRATOR      |     |   DELEGATION POOL    |
 * |                      |     |                      |
 * | dispatch(subtasks)   |---->|  Priority Queue      |
 * |                      |     |  (by tier + priority)|
 * | await_batch(handle)  |<----|                      |
 * +----------------------+     +----------+-----------+
 *                                         |
 *          +------------------------------+------------------------------+
 *          |              |               |               |              |
 *    +-----v-----+  +-----v-----+   +-----v-----+   +-----v-----+  +-----v-----+
 *    | Worker 0  |  | Worker 1  |   | Worker 2  |   | Worker 3  |  | Worker N  |
 *    | (Tier L1) |  | (Tier L2) |   | (Tier L3) |   | (Tier L4) |  | (Tier L1) |
 *    +-----------+  +-----------+   +-----------+   +-----------+  +-----------+
 * ```
 *
 * KEY CONCEPTS:
 * - Tiered Workers: Each worker has a capability tier determining tool access
 * - Priority Scheduling: Higher priority subtasks execute first
 * - Work Stealing: Idle workers steal from busy workers' queues
 * - Immune Modulation: Pool capacity adjusts based on system health
 * - Bio-Async Integration: Neuromodulators affect worker performance
 */

#ifndef NIMCP_RCOG_DELEGATION_POOL_H
#define NIMCP_RCOG_DELEGATION_POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct rcog_tool_router;
struct rcog_context_store;
struct rcog_bio_async_bridge;
struct rcog_immune_bridge;
struct rcog_collective_bridge;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum workers in pool */
#define RCOG_POOL_MAX_WORKERS 64

/** Default worker count per tier */
#define RCOG_POOL_DEFAULT_WORKERS_PER_TIER 4

/** Maximum pending tasks per worker */
#define RCOG_POOL_MAX_PENDING_PER_WORKER 128

/** Maximum total pending tasks */
#define RCOG_POOL_MAX_TOTAL_PENDING 1024

/** Default task timeout (ms) */
#define RCOG_POOL_DEFAULT_TASK_TIMEOUT_MS 30000

/** Work stealing threshold (steal when queue size differs by this ratio) */
#define RCOG_POOL_WORK_STEAL_THRESHOLD 2.0f

/** Default batch size for work stealing */
#define RCOG_POOL_WORK_STEAL_BATCH_SIZE 4

/*=============================================================================
 * WORKER TYPES
 *===========================================================================*/

/**
 * @brief Worker state
 */
typedef enum {
    RCOG_WORKER_IDLE = 0,        /**< Waiting for work */
    RCOG_WORKER_BUSY,            /**< Processing a task */
    RCOG_WORKER_STEALING,        /**< Attempting work steal */
    RCOG_WORKER_PAUSED,          /**< Paused (immune modulation) */
    RCOG_WORKER_STOPPING,        /**< Shutting down */
    RCOG_WORKER_STOPPED          /**< Terminated */
} rcog_worker_state_t;

/**
 * @brief Worker statistics
 */
typedef struct {
    uint64_t tasks_completed;
    uint64_t tasks_failed;
    uint64_t tasks_stolen;       /**< Tasks taken from other workers */
    uint64_t tasks_given;        /**< Tasks stolen by others */
    uint64_t total_processing_ms;
    uint64_t idle_time_ms;
    float avg_task_duration_ms;
    float utilization;           /**< Busy time / total time */
} rcog_worker_stats_t;

/**
 * @brief Worker information (read-only view)
 */
typedef struct {
    uint32_t worker_id;
    rcog_capability_tier_t tier;
    rcog_worker_state_t state;
    size_t queue_depth;
    uint64_t current_task_id;    /**< 0 if idle */
    uint64_t current_task_start_ms;
    rcog_worker_stats_t stats;
} rcog_worker_info_t;

/*=============================================================================
 * POOL CONFIGURATION
 *===========================================================================*/

/**
 * @brief Worker tier configuration
 */
typedef struct {
    rcog_capability_tier_t tier;
    uint32_t num_workers;        /**< Workers at this tier */
    uint32_t queue_capacity;     /**< Per-worker queue capacity */
    uint32_t priority_boost;     /**< Priority boost for tier-matched tasks */
    bool enable_work_stealing;   /**< Allow stealing from this tier */
    bool enable_affinity;        /**< Prefer tier-matched workers */
} rcog_tier_config_t;

/**
 * @brief Pool configuration
 */
typedef struct {
    /* Worker configuration */
    rcog_tier_config_t tiers[RCOG_TIER_COUNT];
    uint32_t total_workers;      /**< Total worker count (computed) */

    /* Scheduling */
    rcog_scheduling_policy_t scheduling_policy;
    bool enable_work_stealing;
    float work_steal_threshold;
    uint32_t work_steal_batch_size;

    /* Timeouts */
    uint32_t default_task_timeout_ms;
    uint32_t shutdown_timeout_ms;

    /* Resource limits */
    size_t max_memory_per_task;
    uint32_t max_pending_tasks;

    /* Integration */
    bool enable_bio_async;       /**< Enable neuromodulator effects */
    bool enable_immune_modulation; /**< Enable capacity scaling */
    bool enable_collective;      /**< Enable swarm distribution */

    /* Debug */
    bool enable_tracing;
    bool verbose_logging;
} rcog_delegation_pool_config_t;

/*=============================================================================
 * POOL STATISTICS
 *===========================================================================*/

/**
 * @brief Pool-wide statistics
 */
typedef struct {
    /* Task counts */
    uint64_t tasks_submitted;
    uint64_t tasks_completed;
    uint64_t tasks_failed;
    uint64_t tasks_cancelled;
    uint64_t tasks_timeout;
    uint64_t tasks_stolen_total;

    /* Queue stats */
    size_t current_pending;
    size_t peak_pending;
    float avg_queue_depth;
    float avg_wait_time_ms;

    /* Worker stats */
    uint32_t active_workers;
    uint32_t idle_workers;
    uint32_t paused_workers;
    float avg_worker_utilization;

    /* Timing */
    float avg_task_duration_ms;
    uint64_t total_processing_time_ms;

    /* Memory */
    size_t current_memory_usage;
    size_t peak_memory_usage;

    /* Modulation */
    float current_capacity_factor;  /**< From immune modulation */
} rcog_delegation_pool_stats_t;

/*=============================================================================
 * TASK SUBMISSION
 *===========================================================================*/

/**
 * @brief Submission options for tasks
 */
typedef struct {
    float priority_override;     /**< Override task priority (-1 = use task's) */
    rcog_capability_tier_t tier_override; /**< Override tier (-1 = use task's) */
    uint32_t timeout_override_ms; /**< Override timeout (0 = use default) */
    bool allow_work_stealing;    /**< Can this task be stolen? */
    bool prefer_local;           /**< Prefer local execution over swarm */
    rcog_subtask_callback_t callback; /**< Completion callback */
    void* callback_data;         /**< Callback user data */
} rcog_submit_options_t;

/**
 * @brief Batch submission result
 */
typedef struct {
    size_t submitted;            /**< Number successfully submitted */
    size_t rejected;             /**< Number rejected (queue full, etc.) */
    size_t distributed;          /**< Number sent to swarm */
    rcog_error_t first_error;    /**< First error encountered */
} rcog_batch_submit_result_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Get default pool configuration
 * @return Default configuration
 */
rcog_delegation_pool_config_t rcog_delegation_pool_default_config(void);

/**
 * @brief Create delegation pool with configuration
 * @param config Configuration (NULL for defaults)
 * @return Pool handle or NULL on error
 */
rcog_delegation_pool_t* rcog_delegation_pool_create(
    const rcog_delegation_pool_config_t* config
);

/**
 * @brief Create pool with default configuration
 * @return Pool handle or NULL on error
 */
rcog_delegation_pool_t* rcog_delegation_pool_create_default(void);

/**
 * @brief Destroy pool and free resources
 *
 * Waits for pending tasks to complete or timeout.
 *
 * @param pool Pool handle (NULL safe)
 */
void rcog_delegation_pool_destroy(rcog_delegation_pool_t* pool);

/**
 * @brief Start pool (begin accepting and processing tasks)
 * @param pool Pool handle
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_start(rcog_delegation_pool_t* pool);

/**
 * @brief Stop pool (finish pending, reject new)
 * @param pool Pool handle
 * @param timeout_ms Time to wait for pending tasks (0 = immediate)
 * @return 0 on success, error code on failure/timeout
 */
int rcog_delegation_pool_stop(
    rcog_delegation_pool_t* pool,
    uint32_t timeout_ms
);

/**
 * @brief Pause pool (stop processing, keep queue)
 * @param pool Pool handle
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_pause(rcog_delegation_pool_t* pool);

/**
 * @brief Resume pool after pause
 * @param pool Pool handle
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_resume(rcog_delegation_pool_t* pool);

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

/**
 * @brief Connect to tool router for task execution
 * @param pool Pool handle
 * @param router Tool router handle
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_connect_tool_router(
    rcog_delegation_pool_t* pool,
    struct rcog_tool_router* router
);

/**
 * @brief Connect to context store for variable access
 * @param pool Pool handle
 * @param store Context store handle
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_connect_context_store(
    rcog_delegation_pool_t* pool,
    struct rcog_context_store* store
);

/**
 * @brief Connect to bio-async bridge for neuromodulation
 * @param pool Pool handle
 * @param bio_async Bio-async bridge handle
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_connect_bio_async(
    rcog_delegation_pool_t* pool,
    struct rcog_bio_async_bridge* bio_async
);

/**
 * @brief Connect to immune bridge for capacity modulation
 * @param pool Pool handle
 * @param immune Immune bridge handle
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_connect_immune(
    rcog_delegation_pool_t* pool,
    struct rcog_immune_bridge* immune
);

/**
 * @brief Connect to collective bridge for swarm distribution
 * @param pool Pool handle
 * @param collective Collective bridge handle
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_connect_collective(
    rcog_delegation_pool_t* pool,
    struct rcog_collective_bridge* collective
);

/*=============================================================================
 * TASK SUBMISSION
 *===========================================================================*/

/**
 * @brief Submit single subtask for execution
 * @param pool Pool handle
 * @param subtask Subtask to execute (pool takes ownership)
 * @param options Submission options (NULL for defaults)
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_submit(
    rcog_delegation_pool_t* pool,
    rcog_subtask_t* subtask,
    const rcog_submit_options_t* options
);

/**
 * @brief Submit batch of subtasks
 * @param pool Pool handle
 * @param subtasks Array of subtasks (pool takes ownership)
 * @param num_subtasks Number of subtasks
 * @param options Submission options (NULL for defaults)
 * @param handle Output batch handle for tracking
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_submit_batch(
    rcog_delegation_pool_t* pool,
    rcog_subtask_t** subtasks,
    size_t num_subtasks,
    const rcog_submit_options_t* options,
    rcog_batch_handle_t** handle
);

/**
 * @brief Get default submission options
 * @return Default options
 */
rcog_submit_options_t rcog_delegation_pool_default_submit_options(void);

/*=============================================================================
 * BATCH MANAGEMENT
 *===========================================================================*/

/**
 * @brief Wait for batch completion
 * @param pool Pool handle
 * @param handle Batch handle
 * @param timeout_ms Timeout (0 = infinite)
 * @return 0 on completion, RCOG_ERROR_TIMEOUT on timeout
 */
int rcog_delegation_pool_await_batch(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle,
    uint32_t timeout_ms
);

/**
 * @brief Poll batch status (non-blocking)
 * @param pool Pool handle
 * @param handle Batch handle
 * @param completed Output number of completed tasks
 * @param total Output total tasks in batch
 * @return true if all tasks complete
 */
bool rcog_delegation_pool_poll_batch(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle,
    size_t* completed,
    size_t* total
);

/**
 * @brief Get results from completed batch
 * @param pool Pool handle
 * @param handle Batch handle
 * @param results Output array of results (caller allocates)
 * @param max_results Maximum results to retrieve
 * @param num_results Output number of results
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_get_batch_results(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle,
    rcog_subtask_result_t* results,
    size_t max_results,
    size_t* num_results
);

/**
 * @brief Cancel pending tasks in batch
 * @param pool Pool handle
 * @param handle Batch handle
 * @return Number of tasks cancelled
 */
size_t rcog_delegation_pool_cancel_batch(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle
);

/**
 * @brief Free batch handle
 * @param handle Handle to free
 */
void rcog_delegation_pool_free_batch_handle(rcog_batch_handle_t* handle);

/*=============================================================================
 * TASK MANAGEMENT
 *===========================================================================*/

/**
 * @brief Get task status
 * @param pool Pool handle
 * @param task_id Task ID
 * @param status Output status
 * @return 0 on success, RCOG_ERROR_CONTEXT_NOT_FOUND if not found
 */
int rcog_delegation_pool_get_task_status(
    rcog_delegation_pool_t* pool,
    uint64_t task_id,
    rcog_subtask_status_t* status
);

/**
 * @brief Get task result (if completed)
 * @param pool Pool handle
 * @param task_id Task ID
 * @param result Output result
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_get_task_result(
    rcog_delegation_pool_t* pool,
    uint64_t task_id,
    rcog_subtask_result_t* result
);

/**
 * @brief Cancel specific task
 * @param pool Pool handle
 * @param task_id Task ID
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_cancel_task(
    rcog_delegation_pool_t* pool,
    uint64_t task_id
);

/**
 * @brief Cancel all pending tasks
 * @param pool Pool handle
 * @return Number of tasks cancelled
 */
size_t rcog_delegation_pool_cancel_all(rcog_delegation_pool_t* pool);

/*=============================================================================
 * WORKER MANAGEMENT
 *===========================================================================*/

/**
 * @brief Get worker count by tier
 * @param pool Pool handle
 * @param tier Capability tier
 * @return Number of workers at tier
 */
uint32_t rcog_delegation_pool_get_worker_count(
    const rcog_delegation_pool_t* pool,
    rcog_capability_tier_t tier
);

/**
 * @brief Get total worker count
 * @param pool Pool handle
 * @return Total workers
 */
uint32_t rcog_delegation_pool_get_total_workers(
    const rcog_delegation_pool_t* pool
);

/**
 * @brief Get worker information
 * @param pool Pool handle
 * @param worker_id Worker ID
 * @param info Output worker info
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_get_worker_info(
    const rcog_delegation_pool_t* pool,
    uint32_t worker_id,
    rcog_worker_info_t* info
);

/**
 * @brief Get all worker information
 * @param pool Pool handle
 * @param infos Output array of worker info
 * @param max_infos Maximum entries
 * @param num_infos Output number of workers
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_get_all_workers(
    const rcog_delegation_pool_t* pool,
    rcog_worker_info_t* infos,
    size_t max_infos,
    size_t* num_infos
);

/**
 * @brief Scale worker count for a tier
 *
 * Dynamically adjust worker count. New workers start idle,
 * removed workers finish current task first.
 *
 * @param pool Pool handle
 * @param tier Tier to scale
 * @param new_count New worker count
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_scale_tier(
    rcog_delegation_pool_t* pool,
    rcog_capability_tier_t tier,
    uint32_t new_count
);

/*=============================================================================
 * IMMUNE MODULATION
 *===========================================================================*/

/**
 * @brief Apply immune modulation to pool
 *
 * Adjusts pool capacity based on immune system state:
 * - Reduces active workers under inflammation
 * - Pauses workers when capacity_multiplier is low
 * - Increases timeouts for slower processing
 *
 * @param pool Pool handle
 * @param modulation Modulation parameters
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_apply_immune_modulation(
    rcog_delegation_pool_t* pool,
    const rcog_immune_modulation_t* modulation
);

/**
 * @brief Get current effective capacity
 * @param pool Pool handle
 * @return Effective capacity (0.0-1.0)
 */
float rcog_delegation_pool_get_effective_capacity(
    const rcog_delegation_pool_t* pool
);

/*=============================================================================
 * WORK STEALING
 *===========================================================================*/

/**
 * @brief Enable/disable work stealing
 * @param pool Pool handle
 * @param enable True to enable
 * @return 0 on success
 */
int rcog_delegation_pool_set_work_stealing(
    rcog_delegation_pool_t* pool,
    bool enable
);

/**
 * @brief Trigger manual rebalance
 *
 * Forces work stealing to rebalance queues immediately.
 *
 * @param pool Pool handle
 * @return Number of tasks moved
 */
size_t rcog_delegation_pool_rebalance(rcog_delegation_pool_t* pool);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get pool statistics
 * @param pool Pool handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_delegation_pool_get_stats(
    const rcog_delegation_pool_t* pool,
    rcog_delegation_pool_stats_t* stats
);

/**
 * @brief Reset pool statistics
 * @param pool Pool handle
 */
void rcog_delegation_pool_reset_stats(rcog_delegation_pool_t* pool);

/**
 * @brief Get queue depth
 * @param pool Pool handle
 * @return Total pending tasks
 */
size_t rcog_delegation_pool_get_queue_depth(
    const rcog_delegation_pool_t* pool
);

/**
 * @brief Get queue depth by tier
 * @param pool Pool handle
 * @param tier Capability tier
 * @return Pending tasks for tier
 */
size_t rcog_delegation_pool_get_tier_queue_depth(
    const rcog_delegation_pool_t* pool,
    rcog_capability_tier_t tier
);

/*=============================================================================
 * UTILITY
 *===========================================================================*/

/**
 * @brief Select best worker for task
 *
 * Returns the optimal worker based on:
 * - Tier match
 * - Queue depth
 * - Current load
 * - Affinity settings
 *
 * @param pool Pool handle
 * @param subtask Task to match
 * @param worker_id Output worker ID
 * @return 0 on success, RCOG_ERROR_WORKER_POOL_EXHAUSTED if no suitable worker
 */
int rcog_delegation_pool_select_worker(
    rcog_delegation_pool_t* pool,
    const rcog_subtask_t* subtask,
    uint32_t* worker_id
);

/**
 * @brief Estimate wait time for new task
 * @param pool Pool handle
 * @param tier Required tier
 * @return Estimated wait time in ms
 */
uint32_t rcog_delegation_pool_estimate_wait_time(
    const rcog_delegation_pool_t* pool,
    rcog_capability_tier_t tier
);

/**
 * @brief Check if pool can accept more tasks
 * @param pool Pool handle
 * @return true if pool has capacity
 */
bool rcog_delegation_pool_has_capacity(
    const rcog_delegation_pool_t* pool
);

/**
 * @brief Drain pool (wait for all tasks to complete)
 * @param pool Pool handle
 * @param timeout_ms Maximum wait time
 * @return 0 on success, RCOG_ERROR_TIMEOUT on timeout
 */
int rcog_delegation_pool_drain(
    rcog_delegation_pool_t* pool,
    uint32_t timeout_ms
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_DELEGATION_POOL_H */
