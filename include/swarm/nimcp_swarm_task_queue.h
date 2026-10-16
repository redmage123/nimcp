//=============================================================================
// nimcp_swarm_task_queue.h - Per-Agent Task Queue
//=============================================================================
/**
 * @file nimcp_swarm_task_queue.h
 * @brief Per-agent task queue for individual work management
 *
 * WHAT: Individual agent task queues with priority ordering
 * WHY:  Enable agents to manage their own work backlog
 * HOW:  Circular buffer with priority insertion
 *
 * DESIGN:
 * Each agent maintains its own task queue:
 * - Priority-ordered (higher priority tasks first)
 * - Bounded capacity (prevent overload)
 * - Load tracking for scheduler decisions
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_SWARM_TASK_QUEUE_H
#define NIMCP_SWARM_TASK_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "swarm/nimcp_swarm_task.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default queue capacity */
#define SWARM_TASK_QUEUE_DEFAULT_CAPACITY 32

/** Maximum queue capacity */
#define SWARM_TASK_QUEUE_MAX_CAPACITY 256

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Task queue entry
 *
 * Stores task reference with scheduling metadata
 */
typedef struct {
    uint64_t task_id;                    /**< Task identifier */
    swarm_task_priority_t priority;      /**< Task priority */
    uint64_t enqueue_time_ms;            /**< When task was queued */
    uint64_t deadline_ms;                /**< Task deadline (0 = none) */
    float estimated_duration_ms;         /**< Estimated execution time */
} swarm_task_queue_entry_t;

/**
 * @brief Per-agent task queue
 *
 * WHAT: Priority queue of tasks for a single agent
 * WHY:  Manage agent's work backlog
 * HOW:  Heap-based priority queue
 */
typedef struct {
    /** Agent ID owning this queue */
    uint32_t agent_id;

    /** Task entries (heap-ordered) */
    swarm_task_queue_entry_t* entries;

    /** Queue capacity */
    uint32_t capacity;

    /** Current queue size */
    uint32_t size;

    /** Currently active task (being executed) */
    uint64_t active_task_id;

    /** Queue statistics */
    struct {
        uint64_t total_enqueued;         /**< Tasks ever enqueued */
        uint64_t total_dequeued;         /**< Tasks ever dequeued */
        uint64_t total_cancelled;        /**< Tasks cancelled from queue */
        float avg_wait_time_ms;          /**< Average time in queue */
        float max_wait_time_ms;          /**< Max time any task waited */
    } stats;

    /** Thread safety */
    nimcp_mutex_t* mutex;

    /** Load metrics */
    float current_load;                  /**< [0-1] based on capacity */
    uint64_t estimated_completion_ms;    /**< When all tasks will complete */
} swarm_task_queue_t;

/**
 * @brief Task queue configuration
 */
typedef struct {
    uint32_t capacity;                   /**< Queue capacity */
    bool enable_deadline_sorting;        /**< Sort by deadline within priority */
    bool enable_stats;                   /**< Track statistics */
} swarm_task_queue_config_t;

//=============================================================================
// Queue API
//=============================================================================

/**
 * @brief Create a task queue for an agent
 *
 * @param agent_id Agent identifier
 * @param config Configuration (NULL for defaults)
 * @return Task queue or NULL on error
 */
swarm_task_queue_t* swarm_task_queue_create(
    uint32_t agent_id,
    const swarm_task_queue_config_t* config
);

/**
 * @brief Destroy a task queue
 *
 * @param queue Queue to destroy
 */
void swarm_task_queue_destroy(swarm_task_queue_t* queue);

/**
 * @brief Get default queue configuration
 *
 * @param config Output configuration
 */
void swarm_task_queue_default_config(swarm_task_queue_config_t* config);

/**
 * @brief Enqueue a task
 *
 * WHAT: Add task to agent's queue
 * WHY:  Agent will execute when resources available
 * HOW:  Insert in priority order
 *
 * @param queue Task queue
 * @param task_id Task to enqueue
 * @param priority Task priority
 * @param deadline_ms Task deadline (0 = none)
 * @param estimated_duration_ms Estimated execution time
 * @return 0 on success, error code otherwise
 */
int swarm_task_queue_enqueue(
    swarm_task_queue_t* queue,
    uint64_t task_id,
    swarm_task_priority_t priority,
    uint64_t deadline_ms,
    float estimated_duration_ms
);

/**
 * @brief Dequeue highest priority task
 *
 * WHAT: Remove and return next task to execute
 * WHY:  Agent ready to work on new task
 * HOW:  Pop from heap
 *
 * @param queue Task queue
 * @param task_id Output task ID
 * @return 0 on success, error code if empty
 */
int swarm_task_queue_dequeue(
    swarm_task_queue_t* queue,
    uint64_t* task_id
);

/**
 * @brief Peek at highest priority task without removing
 *
 * @param queue Task queue
 * @param task_id Output task ID
 * @return 0 on success, error code if empty
 */
int swarm_task_queue_peek(
    const swarm_task_queue_t* queue,
    uint64_t* task_id
);

/**
 * @brief Remove a specific task from queue
 *
 * @param queue Task queue
 * @param task_id Task to remove
 * @return 0 on success, error code if not found
 */
int swarm_task_queue_remove(
    swarm_task_queue_t* queue,
    uint64_t task_id
);

/**
 * @brief Update priority of a queued task
 *
 * @param queue Task queue
 * @param task_id Task to update
 * @param new_priority New priority
 * @return 0 on success, error code otherwise
 */
int swarm_task_queue_update_priority(
    swarm_task_queue_t* queue,
    uint64_t task_id,
    swarm_task_priority_t new_priority
);

/**
 * @brief Check if task is in queue
 *
 * @param queue Task queue
 * @param task_id Task to check
 * @return true if in queue, false otherwise
 */
bool swarm_task_queue_contains(
    const swarm_task_queue_t* queue,
    uint64_t task_id
);

/**
 * @brief Get queue size
 *
 * @param queue Task queue
 * @return Number of tasks in queue
 */
uint32_t swarm_task_queue_size(const swarm_task_queue_t* queue);

/**
 * @brief Check if queue is empty
 *
 * @param queue Task queue
 * @return true if empty, false otherwise
 */
bool swarm_task_queue_is_empty(const swarm_task_queue_t* queue);

/**
 * @brief Check if queue is full
 *
 * @param queue Task queue
 * @return true if full, false otherwise
 */
bool swarm_task_queue_is_full(const swarm_task_queue_t* queue);

/**
 * @brief Get current load factor
 *
 * @param queue Task queue
 * @return Load factor [0-1]
 */
float swarm_task_queue_get_load(const swarm_task_queue_t* queue);

/**
 * @brief Clear all tasks from queue
 *
 * @param queue Task queue
 * @return Number of tasks removed
 */
uint32_t swarm_task_queue_clear(swarm_task_queue_t* queue);

/**
 * @brief Set the currently active task
 *
 * @param queue Task queue
 * @param task_id Active task ID (SWARM_TASK_ID_NONE to clear)
 */
void swarm_task_queue_set_active(
    swarm_task_queue_t* queue,
    uint64_t task_id
);

/**
 * @brief Get the currently active task
 *
 * @param queue Task queue
 * @return Active task ID or SWARM_TASK_ID_NONE
 */
uint64_t swarm_task_queue_get_active(const swarm_task_queue_t* queue);

/**
 * @brief Get estimated completion time for all queued tasks
 *
 * @param queue Task queue
 * @return Estimated completion timestamp (ms)
 */
uint64_t swarm_task_queue_estimated_completion(const swarm_task_queue_t* queue);

/**
 * @brief Get tasks past their deadline
 *
 * @param queue Task queue
 * @param current_time_ms Current timestamp
 * @param task_ids Output array for overdue task IDs
 * @param max_tasks Maximum tasks to return
 * @param count Output count
 * @return 0 on success, error code otherwise
 */
int swarm_task_queue_get_overdue(
    const swarm_task_queue_t* queue,
    uint64_t current_time_ms,
    uint64_t* task_ids,
    uint32_t max_tasks,
    uint32_t* count
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SWARM_TASK_QUEUE_H
