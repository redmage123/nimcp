#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_swarm_task_queue.c - Per-Agent Task Queue Implementation
//=============================================================================
/**
 * @file nimcp_swarm_task_queue.c
 * @brief Implementation of per-agent task queue
 *
 * WHAT: Priority heap-based task queue for individual agents
 * WHY:  Efficient O(log n) enqueue/dequeue with priority ordering
 * HOW:  Binary heap with priority comparison
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "swarm/nimcp_swarm_task_queue.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_task_queue)

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Compare two queue entries for heap ordering
 *
 * Higher priority (lower enum value) comes first.
 * Within same priority, earlier deadline comes first.
 * Within same deadline, earlier enqueue time comes first.
 *
 * @return true if a should come before b
 */
static inline bool entry_compare(
    const swarm_task_queue_entry_t* a,
    const swarm_task_queue_entry_t* b,
    bool deadline_sorting)
{
    // Priority is primary sort (lower enum = higher priority)
    if (a->priority != b->priority) {
        return a->priority < b->priority;
    }

    // Deadline is secondary sort (if enabled)
    if (deadline_sorting) {
        if (a->deadline_ms != b->deadline_ms) {
            // Earlier deadline comes first (0 means no deadline, sort last)
            if (a->deadline_ms == 0) {
                return false;  /* No deadline sorts after entries with deadline */
            }
            if (b->deadline_ms == 0) return true;
            return a->deadline_ms < b->deadline_ms;
        }
    }

    // Enqueue time is tertiary (FIFO within same priority/deadline)
    return a->enqueue_time_ms < b->enqueue_time_ms;
}

/**
 * @brief Swap two heap entries
 */
static inline void entry_swap(
    swarm_task_queue_entry_t* a,
    swarm_task_queue_entry_t* b)
{
    swarm_task_queue_entry_t temp = *a;
    *a = *b;
    *b = temp;
}

/**
 * @brief Heapify up (after insertion)
 */
static void heap_up(
    swarm_task_queue_t* queue,
    uint32_t index,
    bool deadline_sorting)
{
    while (index > 0) {
        uint32_t parent = (index - 1) / 2;

        if (entry_compare(&queue->entries[index],
                          &queue->entries[parent],
                          deadline_sorting)) {
            entry_swap(&queue->entries[index], &queue->entries[parent]);
            index = parent;
        } else {
            break;
        }
    }
}

/**
 * @brief Heapify down (after removal)
 */
static void heap_down(
    swarm_task_queue_t* queue,
    uint32_t index,
    bool deadline_sorting)
{
    while (true) {
        uint32_t left = 2 * index + 1;
        uint32_t right = 2 * index + 2;
        uint32_t smallest = index;

        if (left < queue->size &&
            entry_compare(&queue->entries[left],
                          &queue->entries[smallest],
                          deadline_sorting)) {
            smallest = left;
        }

        if (right < queue->size &&
            entry_compare(&queue->entries[right],
                          &queue->entries[smallest],
                          deadline_sorting)) {
            smallest = right;
        }

        if (smallest != index) {
            entry_swap(&queue->entries[index], &queue->entries[smallest]);
            index = smallest;
        } else {
            break;
        }
    }
}

/**
 * @brief Find entry index by task ID
 */
static int find_entry_index(
    const swarm_task_queue_t* queue,
    uint64_t task_id)
{
    for (uint32_t i = 0; i < queue->size; i++) {
        if (queue->entries[i].task_id == task_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found - normal search miss */
}

//=============================================================================
// Queue API Implementation
//=============================================================================

void swarm_task_queue_default_config(swarm_task_queue_config_t* config)
{
    if (!config) return;

    config->capacity = SWARM_TASK_QUEUE_DEFAULT_CAPACITY;
    config->enable_deadline_sorting = true;
    config->enable_stats = true;
}

swarm_task_queue_t* swarm_task_queue_create(
    uint32_t agent_id,
    const swarm_task_queue_config_t* config)
{
    swarm_task_queue_t* queue = nimcp_malloc(sizeof(swarm_task_queue_t));
    if (!queue) {
        NIMCP_LOGGING_ERROR("Failed to allocate task queue for agent %u", agent_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate task queue for agent %u", agent_id);
        return NULL;
    }

    memset(queue, 0, sizeof(swarm_task_queue_t));

    // Apply configuration
    swarm_task_queue_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        swarm_task_queue_default_config(&cfg);
    }

    // Validate capacity
    if (cfg.capacity == 0) {
        cfg.capacity = SWARM_TASK_QUEUE_DEFAULT_CAPACITY;
    }
    if (cfg.capacity > SWARM_TASK_QUEUE_MAX_CAPACITY) {
        cfg.capacity = SWARM_TASK_QUEUE_MAX_CAPACITY;
    }

    queue->agent_id = agent_id;
    queue->capacity = cfg.capacity;
    queue->active_task_id = SWARM_TASK_ID_NONE;

    // Allocate entries array
    queue->entries = nimcp_calloc(queue->capacity, sizeof(swarm_task_queue_entry_t));
    if (!queue->entries) {
        NIMCP_LOGGING_ERROR("Failed to allocate queue entries for agent %u", agent_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate queue entries for agent %u", agent_id);
        nimcp_free(queue);
        return NULL;
    }

    // Allocate mutex
    queue->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!queue->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate queue mutex for agent %u", agent_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate queue mutex for agent %u", agent_id);
        nimcp_free(queue->entries);
        nimcp_free(queue);
        return NULL;
    }
    nimcp_mutex_init(queue->mutex, NULL);

    NIMCP_LOGGING_DEBUG("Created task queue for agent %u (capacity=%u)",
                        agent_id, queue->capacity);

    return queue;
}

void swarm_task_queue_destroy(swarm_task_queue_t* queue)
{
    if (!queue) return;

    nimcp_mutex_lock(queue->mutex);

    if (queue->entries) {
        nimcp_free(queue->entries);
    }

    nimcp_mutex_unlock(queue->mutex);
    nimcp_mutex_destroy(queue->mutex);
    nimcp_free(queue->mutex);

    NIMCP_LOGGING_DEBUG("Destroyed task queue for agent %u", queue->agent_id);

    nimcp_free(queue);
}

int swarm_task_queue_enqueue(
    swarm_task_queue_t* queue,
    uint64_t task_id,
    swarm_task_priority_t priority,
    uint64_t deadline_ms,
    float estimated_duration_ms)
{
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_queue_enqueue: queue is NULL");
        return -1;
    }

    nimcp_mutex_lock(queue->mutex);

    if (queue->size >= queue->capacity) {
        NIMCP_LOGGING_WARN("Task queue full for agent %u", queue->agent_id);
        nimcp_mutex_unlock(queue->mutex);
        return -2;
    }

    // Create entry
    swarm_task_queue_entry_t entry = {
        .task_id = task_id,
        .priority = priority,
        .enqueue_time_ms = nimcp_time_get_ms(),
        .deadline_ms = deadline_ms,
        .estimated_duration_ms = estimated_duration_ms
    };

    // Insert at end and heapify up
    queue->entries[queue->size] = entry;
    heap_up(queue, queue->size, true);  // deadline_sorting enabled
    queue->size++;

    // Update stats
    queue->stats.total_enqueued++;

    // Update load factor
    queue->current_load = (float)queue->size / (float)queue->capacity;

    // Update estimated completion time
    queue->estimated_completion_ms = nimcp_time_get_ms();
    for (uint32_t i = 0; i < queue->size; i++) {
        queue->estimated_completion_ms += (uint64_t)queue->entries[i].estimated_duration_ms;
    }

    NIMCP_LOGGING_DEBUG("Enqueued task %llu for agent %u (queue size: %u)",
                        (unsigned long long)task_id, queue->agent_id, queue->size);

    nimcp_mutex_unlock(queue->mutex);

    return 0;
}

int swarm_task_queue_dequeue(
    swarm_task_queue_t* queue,
    uint64_t* task_id)
{
    if (!queue || !task_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_queue_dequeue: required parameter is NULL (queue, task_id)");
        return -1;
    }

    nimcp_mutex_lock(queue->mutex);

    if (queue->size == 0) {
        nimcp_mutex_unlock(queue->mutex);
        return -2;
    }

    // Get highest priority task (root)
    *task_id = queue->entries[0].task_id;

    // Calculate wait time for stats
    uint64_t now = nimcp_time_get_ms();
    float wait_time = (float)(now - queue->entries[0].enqueue_time_ms);

    // Update average wait time
    float count = (float)(queue->stats.total_dequeued + 1);
    queue->stats.avg_wait_time_ms =
        (queue->stats.avg_wait_time_ms * (count - 1.0f) + wait_time) / count;

    if (wait_time > queue->stats.max_wait_time_ms) {
        queue->stats.max_wait_time_ms = wait_time;
    }

    // Remove root: move last to root and heapify down
    queue->size--;
    if (queue->size > 0) {
        queue->entries[0] = queue->entries[queue->size];
        heap_down(queue, 0, true);
    }

    queue->stats.total_dequeued++;
    queue->current_load = (float)queue->size / (float)queue->capacity;

    NIMCP_LOGGING_DEBUG("Dequeued task %llu from agent %u (queue size: %u)",
                        (unsigned long long)*task_id, queue->agent_id, queue->size);

    nimcp_mutex_unlock(queue->mutex);

    return 0;
}

int swarm_task_queue_peek(
    const swarm_task_queue_t* queue,
    uint64_t* task_id)
{
    if (!queue || !task_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_queue_peek: required parameter is NULL (queue, task_id)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)queue->mutex);

    if (queue->size == 0) {
        nimcp_mutex_unlock((nimcp_mutex_t*)queue->mutex);
        return -2;
    }

    *task_id = queue->entries[0].task_id;

    nimcp_mutex_unlock((nimcp_mutex_t*)queue->mutex);

    return 0;
}

int swarm_task_queue_remove(
    swarm_task_queue_t* queue,
    uint64_t task_id)
{
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_queue_remove: queue is NULL");
        return -1;
    }

    nimcp_mutex_lock(queue->mutex);

    int index = find_entry_index(queue, task_id);
    if (index < 0) {
        nimcp_mutex_unlock(queue->mutex);
        return -2;
    }

    // Move last element to this position and reheapify
    queue->size--;
    if ((uint32_t)index < queue->size) {
        queue->entries[index] = queue->entries[queue->size];

        // May need to heapify up or down depending on new value
        heap_up(queue, index, true);
        heap_down(queue, index, true);
    }

    queue->stats.total_cancelled++;
    queue->current_load = (float)queue->size / (float)queue->capacity;

    NIMCP_LOGGING_DEBUG("Removed task %llu from agent %u queue",
                        (unsigned long long)task_id, queue->agent_id);

    nimcp_mutex_unlock(queue->mutex);

    return 0;
}

int swarm_task_queue_update_priority(
    swarm_task_queue_t* queue,
    uint64_t task_id,
    swarm_task_priority_t new_priority)
{
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_queue_update_priority: queue is NULL");
        return -1;
    }

    nimcp_mutex_lock(queue->mutex);

    int index = find_entry_index(queue, task_id);
    if (index < 0) {
        nimcp_mutex_unlock(queue->mutex);
        return -2;
    }

    swarm_task_priority_t old_priority = queue->entries[index].priority;
    queue->entries[index].priority = new_priority;

    // Reheapify based on priority change
    if (new_priority < old_priority) {
        // Higher priority (lower value) - heapify up
        heap_up(queue, index, true);
    } else if (new_priority > old_priority) {
        // Lower priority (higher value) - heapify down
        heap_down(queue, index, true);
    }

    nimcp_mutex_unlock(queue->mutex);

    return 0;
}

bool swarm_task_queue_contains(
    const swarm_task_queue_t* queue,
    uint64_t task_id)
{
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_queue_contains: queue is NULL");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)queue->mutex);

    bool found = find_entry_index(queue, task_id) >= 0;

    nimcp_mutex_unlock((nimcp_mutex_t*)queue->mutex);

    return found;
}

uint32_t swarm_task_queue_size(const swarm_task_queue_t* queue)
{
    if (!queue) {
        return 0;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)queue->mutex);
    uint32_t size = queue->size;
    nimcp_mutex_unlock((nimcp_mutex_t*)queue->mutex);

    return size;
}

bool swarm_task_queue_is_empty(const swarm_task_queue_t* queue)
{
    return swarm_task_queue_size(queue) == 0;
}

bool swarm_task_queue_is_full(const swarm_task_queue_t* queue)
{
    if (!queue) {
        return true;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)queue->mutex);
    bool full = queue->size >= queue->capacity;
    nimcp_mutex_unlock((nimcp_mutex_t*)queue->mutex);

    return full;
}

float swarm_task_queue_get_load(const swarm_task_queue_t* queue)
{
    if (!queue) {
        return 1.0f;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)queue->mutex);
    float load = queue->current_load;
    nimcp_mutex_unlock((nimcp_mutex_t*)queue->mutex);

    return load;
}

uint32_t swarm_task_queue_clear(swarm_task_queue_t* queue)
{
    if (!queue) {
        return 0;
    }

    nimcp_mutex_lock(queue->mutex);

    uint32_t removed = queue->size;
    queue->size = 0;
    queue->current_load = 0.0f;
    queue->estimated_completion_ms = nimcp_time_get_ms();
    queue->stats.total_cancelled += removed;

    NIMCP_LOGGING_DEBUG("Cleared %u tasks from agent %u queue",
                        removed, queue->agent_id);

    nimcp_mutex_unlock(queue->mutex);

    return removed;
}

void swarm_task_queue_set_active(
    swarm_task_queue_t* queue,
    uint64_t task_id)
{
    if (!queue) return;

    nimcp_mutex_lock(queue->mutex);
    queue->active_task_id = task_id;
    nimcp_mutex_unlock(queue->mutex);
}

uint64_t swarm_task_queue_get_active(const swarm_task_queue_t* queue)
{
    if (!queue) {
        return SWARM_TASK_ID_NONE;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)queue->mutex);
    uint64_t active = queue->active_task_id;
    nimcp_mutex_unlock((nimcp_mutex_t*)queue->mutex);

    return active;
}

uint64_t swarm_task_queue_estimated_completion(const swarm_task_queue_t* queue)
{
    if (!queue) {
        return 0;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)queue->mutex);
    uint64_t estimate = queue->estimated_completion_ms;
    nimcp_mutex_unlock((nimcp_mutex_t*)queue->mutex);

    return estimate;
}

int swarm_task_queue_get_overdue(
    const swarm_task_queue_t* queue,
    uint64_t current_time_ms,
    uint64_t* task_ids,
    uint32_t max_tasks,
    uint32_t* count)
{
    if (!queue || !task_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_queue_get_overdue: required parameter is NULL (queue, task_ids, count)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)queue->mutex);

    *count = 0;

    for (uint32_t i = 0; i < queue->size && *count < max_tasks; i++) {
        const swarm_task_queue_entry_t* entry = &queue->entries[i];

        // Check if deadline passed
        if (entry->deadline_ms > 0 && entry->deadline_ms < current_time_ms) {
            task_ids[(*count)++] = entry->task_id;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)queue->mutex);

    return 0;
}
