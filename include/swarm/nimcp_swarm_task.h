//=============================================================================
// nimcp_swarm_task.h - Swarm Task Allocation System
//=============================================================================
/**
 * @file nimcp_swarm_task.h
 * @brief Individual task allocation for swarm agents
 *
 * WHAT: Per-agent task management within swarms
 * WHY:  Enable fine-grained work distribution based on agent capabilities
 * HOW:  Task creation, assignment, tracking, and completion
 *
 * BIOLOGICAL INSPIRATION:
 * Modeled after ant colony task allocation where individual workers select
 * tasks based on:
 * - Current need (pheromone signals)
 * - Individual capability (age, size, experience)
 * - Local conditions (proximity, energy)
 *
 * This is distinct from swarm-level missions (see nimcp_swarm_multi.h).
 * Missions are high-level objectives; tasks are individual work items.
 *
 * ARCHITECTURE:
 *
 *   ┌─────────────────────────────────────────────────────────────────┐
 *   │                    SWARM MISSION                                │
 *   │                "Patrol Area Alpha"                              │
 *   └───────────────────────┬─────────────────────────────────────────┘
 *                           │
 *           ┌───────────────┼───────────────┐
 *           ▼               ▼               ▼
 *   ┌───────────────┐ ┌───────────────┐ ┌───────────────┐
 *   │    TASK 1     │ │    TASK 2     │ │    TASK 3     │
 *   │ "Scout N edge"│ │ "Scout S edge"│ │ "Report base" │
 *   └───────┬───────┘ └───────┬───────┘ └───────┬───────┘
 *           │                 │                 │
 *           ▼                 ▼                 ▼
 *       Agent #3          Agent #7          Agent #1
 *
 * FEATURES:
 * - Task dependencies (DAG-based execution order)
 * - Priority-based scheduling
 * - Capability matching
 * - Load balancing across agents
 * - Deadline tracking
 * - Progress reporting
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_SWARM_TASK_H
#define NIMCP_SWARM_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "swarm/nimcp_swarm_multi.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum task description length */
#define SWARM_TASK_MAX_DESC_LEN 256

/** Maximum number of task dependencies */
#define SWARM_TASK_MAX_DEPENDENCIES 16

/** Maximum number of tasks per mission */
#define SWARM_TASK_MAX_PER_MISSION 128

/** Maximum number of required capabilities per task */
#define SWARM_TASK_MAX_CAPABILITIES 8

/** Task ID indicating no task (null) */
#define SWARM_TASK_ID_NONE 0

/** Agent ID indicating no agent (unassigned) */
#define SWARM_AGENT_ID_NONE UINT32_MAX

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Task status
 *
 * WHAT: Current state of a task in its lifecycle
 * WHY:  Track execution progress and enable scheduling decisions
 * HOW:  State machine: PENDING → QUEUED → ASSIGNED → IN_PROGRESS → COMPLETED/FAILED
 */
typedef enum {
    SWARM_TASK_STATUS_PENDING = 0,     /**< Created but not queued */
    SWARM_TASK_STATUS_QUEUED,          /**< In scheduler queue, waiting for agent */
    SWARM_TASK_STATUS_ASSIGNED,        /**< Assigned to agent, not started */
    SWARM_TASK_STATUS_IN_PROGRESS,     /**< Agent actively working on task */
    SWARM_TASK_STATUS_COMPLETED,       /**< Successfully completed */
    SWARM_TASK_STATUS_FAILED,          /**< Execution failed */
    SWARM_TASK_STATUS_CANCELLED,       /**< Cancelled before completion */
    SWARM_TASK_STATUS_BLOCKED,         /**< Waiting on dependencies */
    SWARM_TASK_STATUS_COUNT
} swarm_task_status_t;

/**
 * @brief Task priority levels
 *
 * WHAT: Relative importance of tasks for scheduling
 * WHY:  Ensure critical tasks are executed first
 * HOW:  Higher priority tasks preempt lower priority
 */
typedef enum {
    SWARM_TASK_PRIORITY_CRITICAL = 0,  /**< Life-safety critical (highest) */
    SWARM_TASK_PRIORITY_HIGH,          /**< Urgent task */
    SWARM_TASK_PRIORITY_NORMAL,        /**< Normal priority */
    SWARM_TASK_PRIORITY_LOW,           /**< Background task */
    SWARM_TASK_PRIORITY_IDLE,          /**< Lowest priority (filler work) */
    SWARM_TASK_PRIORITY_COUNT
} swarm_task_priority_t;

/**
 * @brief Task type categories
 *
 * WHAT: Classification of task types for capability matching
 * WHY:  Enable efficient task-to-agent matching
 * HOW:  Agent capabilities are matched against task requirements
 */
typedef enum {
    SWARM_TASK_TYPE_MOVEMENT = 0,      /**< Navigate to location */
    SWARM_TASK_TYPE_OBSERVATION,       /**< Observe/sense environment */
    SWARM_TASK_TYPE_COMMUNICATION,     /**< Send/relay message */
    SWARM_TASK_TYPE_MANIPULATION,      /**< Physical manipulation */
    SWARM_TASK_TYPE_COMPUTATION,       /**< Local computation */
    SWARM_TASK_TYPE_COORDINATION,      /**< Coordinate other agents */
    SWARM_TASK_TYPE_MAINTENANCE,       /**< Self-maintenance (recharge, etc.) */
    SWARM_TASK_TYPE_CUSTOM,            /**< User-defined type */
    SWARM_TASK_TYPE_COUNT
} swarm_task_type_t;

/**
 * @brief Task failure reasons
 */
typedef enum {
    SWARM_TASK_FAIL_NONE = 0,          /**< No failure */
    SWARM_TASK_FAIL_TIMEOUT,           /**< Deadline exceeded */
    SWARM_TASK_FAIL_AGENT_LOST,        /**< Agent became unavailable */
    SWARM_TASK_FAIL_CAPABILITY,        /**< Agent lacks required capability */
    SWARM_TASK_FAIL_ENERGY,            /**< Insufficient energy */
    SWARM_TASK_FAIL_BLOCKED,           /**< Dependencies failed */
    SWARM_TASK_FAIL_CANCELLED,         /**< Explicitly cancelled */
    SWARM_TASK_FAIL_EXECUTION,         /**< Execution error */
    SWARM_TASK_FAIL_COUNT
} swarm_task_failure_reason_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Agent capability profile
 *
 * WHAT: Describes what an agent can do
 * WHY:  Match tasks to capable agents
 * HOW:  Bitmask of capabilities with proficiency levels
 */
typedef struct {
    uint32_t agent_id;                               /**< Agent identifier */

    /** Capability bitmask (from nimcp_swarm_capability_type_t) */
    uint32_t capabilities_mask;

    /** Proficiency for each capability [0-1] */
    float proficiency[NIMCP_SWARM_CAP_COUNT];

    /** Current energy level [0-1] */
    float energy_level;

    /** Current load (tasks in queue) */
    uint32_t current_load;

    /** Maximum concurrent tasks */
    uint32_t max_concurrent_tasks;

    /** Is agent currently available? */
    bool is_available;

    /** Position (for locality-aware scheduling) */
    nimcp_coord3d_t position;
} swarm_agent_profile_t;

/**
 * @brief Task requirement specification
 *
 * WHAT: What an agent needs to execute a task
 * WHY:  Filter unsuitable agents during assignment
 * HOW:  Required capabilities, energy, etc.
 */
typedef struct {
    /** Required capabilities (bitmask) */
    uint32_t required_capabilities;

    /** Minimum proficiency for each required capability */
    float min_proficiency[SWARM_TASK_MAX_CAPABILITIES];
    uint32_t proficiency_count;

    /** Minimum energy level required */
    float min_energy;

    /** Preferred location (for locality) */
    nimcp_coord3d_t preferred_location;
    bool location_specified;

    /** Maximum acceptable distance from preferred location */
    float max_distance;
} swarm_task_requirements_t;

/**
 * @brief Task execution result
 *
 * WHAT: Outcome of task execution
 * WHY:  Track completion status and metrics
 * HOW:  Reported by agent upon completion
 */
typedef struct {
    uint64_t task_id;                    /**< Task that was executed */
    swarm_task_status_t final_status;    /**< Final status */
    swarm_task_failure_reason_t failure_reason; /**< If failed, why */

    float actual_duration_ms;            /**< Actual execution time */
    float energy_consumed;               /**< Energy used */

    /** Task-specific result data */
    void* result_data;
    size_t result_data_size;

    /** Timestamp of completion */
    uint64_t completed_time_ms;
} swarm_task_result_t;

/**
 * @brief Task callback function type
 *
 * Called when task status changes (completion, failure, etc.)
 */
typedef void (*swarm_task_callback_t)(
    uint64_t task_id,
    swarm_task_status_t new_status,
    const swarm_task_result_t* result,
    void* user_data
);

/**
 * @brief Individual swarm task
 *
 * WHAT: Single unit of work assigned to an agent
 * WHY:  Enable fine-grained work distribution
 * HOW:  Created from mission, assigned to capable agent
 */
typedef struct {
    /** Unique task identifier */
    uint64_t task_id;

    /** Parent mission ID (0 if standalone) */
    uint64_t mission_id;

    /** Human-readable description */
    char description[SWARM_TASK_MAX_DESC_LEN];

    /** Task type */
    swarm_task_type_t type;

    /** Current status */
    swarm_task_status_t status;

    /** Priority level */
    swarm_task_priority_t priority;

    /** Requirements for agent assignment */
    swarm_task_requirements_t requirements;

    /** Dependencies (tasks that must complete first) */
    uint64_t depends_on[SWARM_TASK_MAX_DEPENDENCIES];
    uint32_t dependency_count;

    /** Assigned agent (SWARM_AGENT_ID_NONE if unassigned) */
    uint32_t assigned_agent_id;

    /** Progress [0-1] */
    float progress;

    /** Estimated duration (ms) */
    uint32_t estimated_duration_ms;

    /** Timing */
    uint64_t created_time_ms;
    uint64_t deadline_ms;               /**< 0 = no deadline */
    uint64_t started_time_ms;
    uint64_t completed_time_ms;

    /** Failure tracking */
    swarm_task_failure_reason_t failure_reason;
    uint32_t retry_count;
    uint32_t max_retries;

    /** Completion callback */
    swarm_task_callback_t callback;
    void* callback_user_data;

    /** Task-specific data */
    void* task_data;
    size_t task_data_size;
} swarm_task_t;

/**
 * @brief Task manager configuration
 */
typedef struct {
    /** Maximum tasks to track */
    uint32_t max_tasks;

    /** Default task timeout (ms) */
    uint32_t default_timeout_ms;

    /** Default max retries */
    uint32_t default_max_retries;

    /** Enable bio-async notifications */
    bool enable_bio_async;

    /** Enable automatic retry on failure */
    bool enable_auto_retry;

    /** Default priority for new tasks */
    swarm_task_priority_t default_priority;
} swarm_task_manager_config_t;

/**
 * @brief Task manager statistics
 */
typedef struct {
    uint32_t total_tasks_created;
    uint32_t tasks_pending;
    uint32_t tasks_in_progress;
    uint32_t tasks_completed;
    uint32_t tasks_failed;
    uint32_t tasks_cancelled;

    float avg_completion_time_ms;
    float avg_wait_time_ms;

    uint32_t retries_performed;
    uint32_t deadlines_missed;
} swarm_task_manager_stats_t;

/** Opaque task manager type */
typedef struct swarm_task_manager swarm_task_manager_t;

//=============================================================================
// Task Manager API
//=============================================================================

/**
 * @brief Create a task manager
 *
 * WHAT: Initialize task management system
 * WHY:  Central point for task lifecycle management
 * HOW:  Allocate internal structures, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return Task manager or NULL on error
 */
swarm_task_manager_t* swarm_task_manager_create(
    const swarm_task_manager_config_t* config
);

/**
 * @brief Destroy task manager
 *
 * WHAT: Clean up task manager resources
 * WHY:  Free memory, cancel pending tasks
 * HOW:  Destroy all tracked tasks, free manager
 *
 * @param manager Task manager to destroy
 */
void swarm_task_manager_destroy(swarm_task_manager_t* manager);

/**
 * @brief Get default task manager configuration
 *
 * @param config Output configuration
 */
void swarm_task_manager_default_config(swarm_task_manager_config_t* config);

/**
 * @brief Get task manager statistics
 *
 * @param manager Task manager
 * @param stats Output statistics
 * @return 0 on success, error code otherwise
 */
int swarm_task_manager_get_stats(
    const swarm_task_manager_t* manager,
    swarm_task_manager_stats_t* stats
);

//=============================================================================
// Task Lifecycle API
//=============================================================================

/**
 * @brief Create a new task
 *
 * WHAT: Allocate and initialize a task
 * WHY:  Define work to be distributed to agents
 * HOW:  Allocate task, assign ID, set initial state
 *
 * @param manager Task manager
 * @param description Human-readable description
 * @param type Task type
 * @param priority Priority level
 * @param task_id Output task ID
 * @return 0 on success, error code otherwise
 */
int swarm_task_create(
    swarm_task_manager_t* manager,
    const char* description,
    swarm_task_type_t type,
    swarm_task_priority_t priority,
    uint64_t* task_id
);

/**
 * @brief Set task requirements
 *
 * @param manager Task manager
 * @param task_id Task to configure
 * @param requirements Capability requirements
 * @return 0 on success, error code otherwise
 */
int swarm_task_set_requirements(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    const swarm_task_requirements_t* requirements
);

/**
 * @brief Add task dependency
 *
 * WHAT: Specify that this task depends on another
 * WHY:  Enforce execution order (DAG)
 * HOW:  Task won't start until dependency completes
 *
 * @param manager Task manager
 * @param task_id Task to modify
 * @param depends_on_id Task that must complete first
 * @return 0 on success, error code otherwise
 */
int swarm_task_add_dependency(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    uint64_t depends_on_id
);

/**
 * @brief Set task deadline
 *
 * @param manager Task manager
 * @param task_id Task to modify
 * @param deadline_ms Deadline timestamp (ms since epoch)
 * @return 0 on success, error code otherwise
 */
int swarm_task_set_deadline(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    uint64_t deadline_ms
);

/**
 * @brief Set task callback
 *
 * @param manager Task manager
 * @param task_id Task to modify
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success, error code otherwise
 */
int swarm_task_set_callback(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    swarm_task_callback_t callback,
    void* user_data
);

/**
 * @brief Set task-specific data
 *
 * @param manager Task manager
 * @param task_id Task to modify
 * @param data Task data (copied)
 * @param data_size Size of data
 * @return 0 on success, error code otherwise
 */
int swarm_task_set_data(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    const void* data,
    size_t data_size
);

/**
 * @brief Associate task with mission
 *
 * @param manager Task manager
 * @param task_id Task to modify
 * @param mission_id Parent mission ID
 * @return 0 on success, error code otherwise
 */
int swarm_task_set_mission(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    uint64_t mission_id
);

/**
 * @brief Submit task for scheduling
 *
 * WHAT: Move task from PENDING to QUEUED state
 * WHY:  Make task available for agent assignment
 * HOW:  Add to scheduler queue, check dependencies
 *
 * @param manager Task manager
 * @param task_id Task to submit
 * @return 0 on success, error code otherwise
 */
int swarm_task_submit(
    swarm_task_manager_t* manager,
    uint64_t task_id
);

/**
 * @brief Assign task to agent
 *
 * WHAT: Assign task to specific agent
 * WHY:  Begin task execution by capable agent
 * HOW:  Verify capability match, update state
 *
 * @param manager Task manager
 * @param task_id Task to assign
 * @param agent_id Agent to assign to
 * @return 0 on success, error code otherwise
 */
int swarm_task_assign(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    uint32_t agent_id
);

/**
 * @brief Mark task as started
 *
 * @param manager Task manager
 * @param task_id Task that started
 * @return 0 on success, error code otherwise
 */
int swarm_task_start(
    swarm_task_manager_t* manager,
    uint64_t task_id
);

/**
 * @brief Update task progress
 *
 * @param manager Task manager
 * @param task_id Task to update
 * @param progress Progress [0-1]
 * @return 0 on success, error code otherwise
 */
int swarm_task_update_progress(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    float progress
);

/**
 * @brief Mark task as completed
 *
 * @param manager Task manager
 * @param task_id Task that completed
 * @param result Execution result (may be NULL)
 * @return 0 on success, error code otherwise
 */
int swarm_task_complete(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    const swarm_task_result_t* result
);

/**
 * @brief Mark task as failed
 *
 * @param manager Task manager
 * @param task_id Task that failed
 * @param reason Failure reason
 * @return 0 on success, error code otherwise
 */
int swarm_task_fail(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    swarm_task_failure_reason_t reason
);

/**
 * @brief Cancel task
 *
 * @param manager Task manager
 * @param task_id Task to cancel
 * @return 0 on success, error code otherwise
 */
int swarm_task_cancel(
    swarm_task_manager_t* manager,
    uint64_t task_id
);

/**
 * @brief Retry failed task
 *
 * @param manager Task manager
 * @param task_id Task to retry
 * @return 0 on success, error code otherwise
 */
int swarm_task_retry(
    swarm_task_manager_t* manager,
    uint64_t task_id
);

//=============================================================================
// Task Query API
//=============================================================================

/**
 * @brief Get task by ID (UNSAFE - see warning)
 *
 * CRITICAL THREAD SAFETY WARNING - POINTER-AFTER-UNLOCK ISSUE:
 * The returned pointer may be invalidated by concurrent task removal.
 * For thread-safe access, use swarm_task_get_copy() instead.
 *
 * DEPRECATED: Use swarm_task_get_copy() for multi-threaded code.
 *
 * @param manager Task manager
 * @param task_id Task ID
 * @return Pointer to task (read-only) or NULL if not found
 */
const swarm_task_t* swarm_task_get(
    const swarm_task_manager_t* manager,
    uint64_t task_id
);

/**
 * @brief Get thread-safe copy of task by ID
 *
 * WHAT: Get copy of task data, safe for multi-threaded access
 * WHY:  swarm_task_get() returns pointer that can be invalidated
 * HOW:  Copy task data while holding mutex
 *
 * This is the RECOMMENDED API for multi-threaded access.
 *
 * @param manager Task manager
 * @param task_id Task ID
 * @param out_task Output: copy of task data (caller-allocated)
 * @return 0 on success, -1 if not found, -2 if NULL parameters
 */
int swarm_task_get_copy(
    const swarm_task_manager_t* manager,
    uint64_t task_id,
    swarm_task_t* out_task
);

/**
 * @brief Get task status
 *
 * @param manager Task manager
 * @param task_id Task ID
 * @return Task status
 */
swarm_task_status_t swarm_task_get_status(
    const swarm_task_manager_t* manager,
    uint64_t task_id
);

/**
 * @brief Get tasks for a mission
 *
 * @param manager Task manager
 * @param mission_id Mission ID
 * @param task_ids Output array for task IDs
 * @param max_tasks Maximum tasks to return
 * @param count Output task count
 * @return 0 on success, error code otherwise
 */
int swarm_task_get_by_mission(
    const swarm_task_manager_t* manager,
    uint64_t mission_id,
    uint64_t* task_ids,
    uint32_t max_tasks,
    uint32_t* count
);

/**
 * @brief Get tasks assigned to agent
 *
 * @param manager Task manager
 * @param agent_id Agent ID
 * @param task_ids Output array for task IDs
 * @param max_tasks Maximum tasks to return
 * @param count Output task count
 * @return 0 on success, error code otherwise
 */
int swarm_task_get_by_agent(
    const swarm_task_manager_t* manager,
    uint32_t agent_id,
    uint64_t* task_ids,
    uint32_t max_tasks,
    uint32_t* count
);

/**
 * @brief Get pending tasks (ready for assignment)
 *
 * @param manager Task manager
 * @param task_ids Output array for task IDs
 * @param max_tasks Maximum tasks to return
 * @param count Output task count
 * @return 0 on success, error code otherwise
 */
int swarm_task_get_pending(
    const swarm_task_manager_t* manager,
    uint64_t* task_ids,
    uint32_t max_tasks,
    uint32_t* count
);

/**
 * @brief Check if task dependencies are satisfied
 *
 * @param manager Task manager
 * @param task_id Task to check
 * @return true if all dependencies completed, false otherwise
 */
bool swarm_task_dependencies_satisfied(
    const swarm_task_manager_t* manager,
    uint64_t task_id
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get task status name
 *
 * @param status Task status
 * @return Human-readable status name
 */
const char* swarm_task_status_name(swarm_task_status_t status);

/**
 * @brief Get task priority name
 *
 * @param priority Task priority
 * @return Human-readable priority name
 */
const char* swarm_task_priority_name(swarm_task_priority_t priority);

/**
 * @brief Get task type name
 *
 * @param type Task type
 * @return Human-readable type name
 */
const char* swarm_task_type_name(swarm_task_type_t type);

/**
 * @brief Get failure reason name
 *
 * @param reason Failure reason
 * @return Human-readable reason name
 */
const char* swarm_task_failure_name(swarm_task_failure_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SWARM_TASK_H
