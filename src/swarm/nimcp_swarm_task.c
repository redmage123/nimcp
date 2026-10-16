#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_swarm_task.c - Swarm Task Allocation System Implementation
//=============================================================================
/**
 * @file nimcp_swarm_task.c
 * @brief Implementation of per-agent task management
 *
 * WHAT: Task lifecycle management for swarm agents
 * WHY:  Enable fine-grained work distribution
 * HOW:  Hash table for O(1) lookup, linked lists for status tracking
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "swarm/nimcp_swarm_task.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_task)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Task node for hash table bucket
 */
typedef struct task_node {
    swarm_task_t task;               /**< Task data */
    struct task_node* next;          /**< Next in bucket chain */
} task_node_t;

/**
 * @brief Internal task manager structure
 */
struct swarm_task_manager {
    /** Configuration */
    swarm_task_manager_config_t config;

    /** Hash table for tasks */
    task_node_t** buckets;
    uint32_t bucket_count;

    /** Task ID generator */
    uint64_t next_task_id;

    /** Statistics */
    swarm_task_manager_stats_t stats;

    /** Thread safety */
    nimcp_mutex_t* mutex;

    /** Bio-async context */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Constants
//=============================================================================

/** Hash table bucket count (prime for better distribution) */
#define TASK_HASH_BUCKETS 257

/** Default max tasks */
#define DEFAULT_MAX_TASKS 1024

/** Default timeout */
#define DEFAULT_TIMEOUT_MS 60000

/** Default max retries */
#define DEFAULT_MAX_RETRIES 3

//=============================================================================
// Static Name Arrays
//=============================================================================

static const char* TASK_STATUS_NAMES[] = {
    "PENDING",
    "QUEUED",
    "ASSIGNED",
    "IN_PROGRESS",
    "COMPLETED",
    "FAILED",
    "CANCELLED",
    "BLOCKED"
};

static const char* TASK_PRIORITY_NAMES[] = {
    "CRITICAL",
    "HIGH",
    "NORMAL",
    "LOW",
    "IDLE"
};

static const char* TASK_TYPE_NAMES[] = {
    "MOVEMENT",
    "OBSERVATION",
    "COMMUNICATION",
    "MANIPULATION",
    "COMPUTATION",
    "COORDINATION",
    "MAINTENANCE",
    "CUSTOM"
};

static const char* TASK_FAILURE_NAMES[] = {
    "NONE",
    "TIMEOUT",
    "AGENT_LOST",
    "CAPABILITY",
    "ENERGY",
    "BLOCKED",
    "CANCELLED",
    "EXECUTION"
};

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Hash function for task IDs
 */
static inline uint32_t hash_task_id(uint64_t task_id, uint32_t bucket_count)
{
    // Simple multiplicative hash
    return (uint32_t)((task_id * 2654435769ULL) % bucket_count);
}

/**
 * @brief Find task node by ID
 */
static task_node_t* find_task_node(
    const swarm_task_manager_t* manager,
    uint64_t task_id)
{
    uint32_t bucket = hash_task_id(task_id, manager->bucket_count);
    task_node_t* node = manager->buckets[bucket];

    while (node) {
        if (node->task.task_id == task_id) {
            return node;
        }
        node = node->next;
    }

    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Check if all dependencies are satisfied
 */
static bool check_dependencies(
    const swarm_task_manager_t* manager,
    const swarm_task_t* task)
{
    for (uint32_t i = 0; i < task->dependency_count; i++) {
        task_node_t* dep_node = find_task_node(manager, task->depends_on[i]);
        if (!dep_node || dep_node->task.status != SWARM_TASK_STATUS_COMPLETED) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Invoke task callback if set
 */
static void invoke_callback(
    const swarm_task_t* task,
    const swarm_task_result_t* result)
{
    if (task->callback) {
        task->callback(
            task->task_id,
            task->status,
            result,
            task->callback_user_data
        );
    }
}

//=============================================================================
// Task Manager API Implementation
//=============================================================================

void swarm_task_manager_default_config(swarm_task_manager_config_t* config)
{
    if (!config) return;

    config->max_tasks = DEFAULT_MAX_TASKS;
    config->default_timeout_ms = DEFAULT_TIMEOUT_MS;
    config->default_max_retries = DEFAULT_MAX_RETRIES;
    config->enable_bio_async = true;
    config->enable_auto_retry = true;
    config->default_priority = SWARM_TASK_PRIORITY_NORMAL;
}

swarm_task_manager_t* swarm_task_manager_create(
    const swarm_task_manager_config_t* config)
{
    swarm_task_manager_t* manager = nimcp_malloc(sizeof(swarm_task_manager_t));
    if (!manager) {
        NIMCP_LOGGING_ERROR("Failed to allocate task manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_task_manager_create: manager is NULL");
        return NULL;
    }

    memset(manager, 0, sizeof(swarm_task_manager_t));

    // Apply configuration
    if (config) {
        manager->config = *config;
    } else {
        swarm_task_manager_default_config(&manager->config);
    }

    // Allocate hash table
    manager->bucket_count = TASK_HASH_BUCKETS;
    manager->buckets = nimcp_calloc(manager->bucket_count, sizeof(task_node_t*));
    if (!manager->buckets) {
        NIMCP_LOGGING_ERROR("Failed to allocate task hash table");
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_task_manager_create: manager->buckets is NULL");
        return NULL;
    }

    // Initialize mutex
    manager->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!manager->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate task manager mutex");
        nimcp_free(manager->buckets);
        nimcp_free(manager);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "swarm_task_manager_create: manager->mutex is NULL");
        return NULL;
    }
    nimcp_mutex_init(manager->mutex, NULL);

    // Start task ID at 1 (0 is NONE)
    manager->next_task_id = 1;

    // Connect to bio-async if enabled
    if (manager->config.enable_bio_async) {
        bio_module_info_t info = {
            .module_id = BIO_MODULE_SWARM_TASK,
            .module_name = "swarm_task_manager",
            .inbox_capacity = NIMCP_INBOX_CAPACITY_MEDIUM,
            .user_data = manager
        };
        manager->bio_ctx = bio_router_register_module(&info);
        if (manager->bio_ctx) {
            manager->bio_async_enabled = true;
            NIMCP_LOGGING_INFO("Task manager connected to bio-async");
        }
    }

    NIMCP_LOGGING_INFO("Created swarm task manager (max_tasks=%u)",
                       manager->config.max_tasks);

    return manager;
}

void swarm_task_manager_destroy(swarm_task_manager_t* manager)
{
    if (!manager) return;

    nimcp_mutex_lock(manager->mutex);

    // Disconnect bio-async
    if (manager->bio_async_enabled) {
        bio_router_unregister_module(manager->bio_ctx);
    }

    // Free all task nodes
    for (uint32_t i = 0; i < manager->bucket_count; i++) {
        task_node_t* node = manager->buckets[i];
        while (node) {
            task_node_t* next = node->next;

            // Free task data if allocated
            if (node->task.task_data) {
                nimcp_free(node->task.task_data);
            }

            nimcp_free(node);
            node = next;
        }
    }

    nimcp_free(manager->buckets);

    nimcp_mutex_unlock(manager->mutex);
    nimcp_mutex_destroy(manager->mutex);
    nimcp_free(manager->mutex);

    nimcp_free(manager);

    NIMCP_LOGGING_INFO("Destroyed swarm task manager");
}

int swarm_task_manager_get_stats(
    const swarm_task_manager_t* manager,
    swarm_task_manager_stats_t* stats)
{
    if (!manager || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_manager_get_stats: required parameter is NULL (manager, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)manager->mutex);
    *stats = manager->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)manager->mutex);

    return 0;
}

//=============================================================================
// Task Lifecycle API Implementation
//=============================================================================

int swarm_task_create(
    swarm_task_manager_t* manager,
    const char* description,
    swarm_task_type_t type,
    swarm_task_priority_t priority,
    uint64_t* task_id)
{
    if (!manager || !description || !task_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_create: required parameter is NULL (manager, description, task_id)");
        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    // Check capacity
    if (manager->stats.total_tasks_created - manager->stats.tasks_completed -
        manager->stats.tasks_failed - manager->stats.tasks_cancelled >=
        manager->config.max_tasks) {
        NIMCP_LOGGING_WARN("Task manager at capacity");
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    // Allocate task node
    task_node_t* node = nimcp_malloc(sizeof(task_node_t));
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -3;
    }

    memset(node, 0, sizeof(task_node_t));

    // Initialize task
    swarm_task_t* task = &node->task;
    task->task_id = manager->next_task_id++;
    task->mission_id = 0;
    strncpy(task->description, description, SWARM_TASK_MAX_DESC_LEN - 1);
    task->type = type;
    task->status = SWARM_TASK_STATUS_PENDING;
    task->priority = priority;
    task->assigned_agent_id = SWARM_AGENT_ID_NONE;
    task->progress = 0.0f;
    task->created_time_ms = nimcp_time_get_ms();
    task->max_retries = manager->config.default_max_retries;

    // Insert into hash table
    uint32_t bucket = hash_task_id(task->task_id, manager->bucket_count);
    node->next = manager->buckets[bucket];
    manager->buckets[bucket] = node;

    // Update stats
    manager->stats.total_tasks_created++;
    manager->stats.tasks_pending++;

    *task_id = task->task_id;

    NIMCP_LOGGING_DEBUG("Created task %llu: %s",
                        (unsigned long long)task->task_id,
                        description);

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_set_requirements(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    const swarm_task_requirements_t* requirements)
{
    if (!manager || !requirements) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_set_requirements: required parameter is NULL (manager, requirements)");
        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    if (node->task.status != SWARM_TASK_STATUS_PENDING) {
        nimcp_mutex_unlock(manager->mutex);
        return -3;
    }

    node->task.requirements = *requirements;

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_add_dependency(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    uint64_t depends_on_id)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    if (node->task.status != SWARM_TASK_STATUS_PENDING) {
        nimcp_mutex_unlock(manager->mutex);
        return -3;
    }

    if (node->task.dependency_count >= SWARM_TASK_MAX_DEPENDENCIES) {
        nimcp_mutex_unlock(manager->mutex);
        return -4;
    }

    // Verify dependency exists
    if (!find_task_node(manager, depends_on_id)) {
        nimcp_mutex_unlock(manager->mutex);
        return -5;
    }

    // Check for circular dependency (simple check)
    if (depends_on_id == task_id) {
        nimcp_mutex_unlock(manager->mutex);
        return -6;
    }

    node->task.depends_on[node->task.dependency_count++] = depends_on_id;

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_set_deadline(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    uint64_t deadline_ms)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    node->task.deadline_ms = deadline_ms;

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_set_callback(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    swarm_task_callback_t callback,
    void* user_data)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    node->task.callback = callback;
    node->task.callback_user_data = user_data;

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_set_data(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    const void* data,
    size_t data_size)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    // Free existing data
    if (node->task.task_data) {
        nimcp_free(node->task.task_data);
        node->task.task_data = NULL;
        node->task.task_data_size = 0;
    }

    if (data && data_size > 0) {
        node->task.task_data = nimcp_malloc(data_size);
        if (!node->task.task_data) {
            nimcp_mutex_unlock(manager->mutex);
            return -3;
        }
        memcpy(node->task.task_data, data, data_size);
        node->task.task_data_size = data_size;
    }

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_set_mission(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    uint64_t mission_id)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    node->task.mission_id = mission_id;

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_submit(
    swarm_task_manager_t* manager,
    uint64_t task_id)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    if (node->task.status != SWARM_TASK_STATUS_PENDING) {
        nimcp_mutex_unlock(manager->mutex);
        return -3;
    }

    // Check dependencies
    if (!check_dependencies(manager, &node->task)) {
        node->task.status = SWARM_TASK_STATUS_BLOCKED;
        NIMCP_LOGGING_DEBUG("Task %llu blocked on dependencies",
                            (unsigned long long)task_id);
    } else {
        node->task.status = SWARM_TASK_STATUS_QUEUED;
        manager->stats.tasks_pending--;
    }

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_assign(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    uint32_t agent_id)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    if (node->task.status != SWARM_TASK_STATUS_QUEUED) {
        nimcp_mutex_unlock(manager->mutex);
        return -3;
    }

    node->task.assigned_agent_id = agent_id;
    node->task.status = SWARM_TASK_STATUS_ASSIGNED;

    NIMCP_LOGGING_DEBUG("Task %llu assigned to agent %u",
                        (unsigned long long)task_id, agent_id);

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_start(
    swarm_task_manager_t* manager,
    uint64_t task_id)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    if (node->task.status != SWARM_TASK_STATUS_ASSIGNED) {
        nimcp_mutex_unlock(manager->mutex);
        return -3;
    }

    node->task.status = SWARM_TASK_STATUS_IN_PROGRESS;
    node->task.started_time_ms = nimcp_time_get_ms();
    manager->stats.tasks_in_progress++;

    NIMCP_LOGGING_DEBUG("Task %llu started",
                        (unsigned long long)task_id);

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_update_progress(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    float progress)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    // Clamp progress
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    if (node->task.status != SWARM_TASK_STATUS_IN_PROGRESS) {
        nimcp_mutex_unlock(manager->mutex);
        return -3;
    }

    node->task.progress = progress;

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_complete(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    const swarm_task_result_t* result)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    if (node->task.status != SWARM_TASK_STATUS_IN_PROGRESS) {
        nimcp_mutex_unlock(manager->mutex);
        return -3;
    }

    node->task.status = SWARM_TASK_STATUS_COMPLETED;
    node->task.progress = 1.0f;
    node->task.completed_time_ms = nimcp_time_get_ms();

    manager->stats.tasks_in_progress--;
    manager->stats.tasks_completed++;

    // Update average completion time
    float completion_time = (float)(node->task.completed_time_ms -
                                    node->task.started_time_ms);
    float count = (float)manager->stats.tasks_completed;
    manager->stats.avg_completion_time_ms =
        ((count - 1.0f) * manager->stats.avg_completion_time_ms +
         completion_time) / count;

    NIMCP_LOGGING_DEBUG("Task %llu completed (%.1f ms)",
                        (unsigned long long)task_id,
                        completion_time);

    nimcp_mutex_unlock(manager->mutex);

    // Invoke callback (outside lock)
    invoke_callback(&node->task, result);

    return 0;
}

int swarm_task_fail(
    swarm_task_manager_t* manager,
    uint64_t task_id,
    swarm_task_failure_reason_t reason)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    // Update status based on previous state
    if (node->task.status == SWARM_TASK_STATUS_IN_PROGRESS) {
        manager->stats.tasks_in_progress--;
    }

    node->task.status = SWARM_TASK_STATUS_FAILED;
    node->task.failure_reason = reason;
    node->task.completed_time_ms = nimcp_time_get_ms();
    manager->stats.tasks_failed++;

    NIMCP_LOGGING_WARN("Task %llu failed: %s",
                       (unsigned long long)task_id,
                       swarm_task_failure_name(reason));

    // Check for deadline miss
    if (reason == SWARM_TASK_FAIL_TIMEOUT) {
        manager->stats.deadlines_missed++;
    }

    nimcp_mutex_unlock(manager->mutex);

    // Invoke callback
    swarm_task_result_t result = {
        .task_id = task_id,
        .final_status = SWARM_TASK_STATUS_FAILED,
        .failure_reason = reason,
        .completed_time_ms = node->task.completed_time_ms
    };
    invoke_callback(&node->task, &result);

    return 0;
}

int swarm_task_cancel(
    swarm_task_manager_t* manager,
    uint64_t task_id)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    // Update stats based on previous state
    switch (node->task.status) {
        case SWARM_TASK_STATUS_PENDING:
            manager->stats.tasks_pending--;
            break;
        case SWARM_TASK_STATUS_IN_PROGRESS:
            manager->stats.tasks_in_progress--;
            break;
        default:
            break;
    }

    node->task.status = SWARM_TASK_STATUS_CANCELLED;
    node->task.failure_reason = SWARM_TASK_FAIL_CANCELLED;
    manager->stats.tasks_cancelled++;

    NIMCP_LOGGING_DEBUG("Task %llu cancelled",
                        (unsigned long long)task_id);

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

int swarm_task_retry(
    swarm_task_manager_t* manager,
    uint64_t task_id)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return -1;
    }

    nimcp_mutex_lock(manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock(manager->mutex);
        return -2;
    }

    if (node->task.status != SWARM_TASK_STATUS_FAILED) {
        nimcp_mutex_unlock(manager->mutex);
        return -3;
    }

    if (node->task.retry_count >= node->task.max_retries) {
        nimcp_mutex_unlock(manager->mutex);
        return -4;
    }

    node->task.retry_count++;
    node->task.status = SWARM_TASK_STATUS_QUEUED;
    node->task.assigned_agent_id = SWARM_AGENT_ID_NONE;
    node->task.progress = 0.0f;
    node->task.failure_reason = SWARM_TASK_FAIL_NONE;

    manager->stats.tasks_failed--;
    manager->stats.retries_performed++;

    NIMCP_LOGGING_DEBUG("Task %llu retry #%u",
                        (unsigned long long)task_id,
                        node->task.retry_count);

    nimcp_mutex_unlock(manager->mutex);

    return 0;
}

//=============================================================================
// Task Query API Implementation
//=============================================================================

/**
 * CRITICAL THREAD SAFETY WARNING - POINTER-AFTER-UNLOCK ISSUE:
 * ============================================================
 * This function returns a pointer to internal task data AFTER releasing the
 * mutex, creating a race condition window where:
 *   1. Another thread may modify the task (status, progress, etc.)
 *   2. Another thread may remove the task, invalidating the pointer
 *   3. Memory corruption if caller accesses stale pointer
 *
 * DEPRECATED: Use swarm_task_get_copy() for thread-safe access.
 *
 * SAFE USAGE PATTERNS (caller MUST ensure one of these):
 *   - Single-threaded access to the task manager
 *   - Caller holds external synchronization preventing task removal
 *   - Copy needed data immediately after call, don't store pointer
 *
 * FOR THREAD-SAFE QUERIES: Use swarm_task_get_copy() instead, which
 * returns a copy of the task data that is safe to use across threads.
 *
 * RISK LEVEL: HIGH - Use with extreme caution in multi-threaded code.
 */
const swarm_task_t* swarm_task_get(
    const swarm_task_manager_t* manager,
    uint64_t task_id)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "manager is NULL");

        return NULL;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);

    nimcp_mutex_unlock((nimcp_mutex_t*)manager->mutex);

    /* WARNING: returned pointer may be invalidated by concurrent operations.
     * Caller must use immediately or hold external synchronization. */
    return node ? &node->task : NULL;
}

/**
 * WHAT: Get thread-safe copy of task data by ID
 * WHY:  swarm_task_get() returns pointer that can be invalidated by concurrent ops
 * HOW:  Copy task data while holding mutex, return copy via out parameter
 *
 * This is the RECOMMENDED API for multi-threaded access.
 *
 * Note: The task_data pointer in the copied struct still points to internal
 * memory. If you need the task_data, copy it separately before releasing
 * any external locks, or use single-threaded access patterns.
 *
 * @param manager Task manager
 * @param task_id Task identifier
 * @param out_task Output: copy of task data (caller-allocated)
 * @return 0 on success, -1 if not found, -2 if NULL parameters
 */
int swarm_task_get_copy(
    const swarm_task_manager_t* manager,
    uint64_t task_id,
    swarm_task_t* out_task)
{
    if (!manager || !out_task) {
        return -2;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    int result = -1;

    if (node) {
        /* Copy task data while holding mutex - thread-safe */
        *out_task = node->task;
        result = 0;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)manager->mutex);
    return result;
}

swarm_task_status_t swarm_task_get_status(
    const swarm_task_manager_t* manager,
    uint64_t task_id)
{
    const swarm_task_t* task = swarm_task_get(manager, task_id);
    return task ? task->status : SWARM_TASK_STATUS_COUNT;
}

int swarm_task_get_by_mission(
    const swarm_task_manager_t* manager,
    uint64_t mission_id,
    uint64_t* task_ids,
    uint32_t max_tasks,
    uint32_t* count)
{
    if (!manager || !task_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_get_by_mission: required parameter is NULL (manager, task_ids, count)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)manager->mutex);

    *count = 0;

    for (uint32_t i = 0; i < manager->bucket_count && *count < max_tasks; i++) {
        task_node_t* node = manager->buckets[i];
        while (node && *count < max_tasks) {
            if (node->task.mission_id == mission_id) {
                task_ids[(*count)++] = node->task.task_id;
            }
            node = node->next;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)manager->mutex);

    return 0;
}

int swarm_task_get_by_agent(
    const swarm_task_manager_t* manager,
    uint32_t agent_id,
    uint64_t* task_ids,
    uint32_t max_tasks,
    uint32_t* count)
{
    if (!manager || !task_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_get_by_agent: required parameter is NULL (manager, task_ids, count)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)manager->mutex);

    *count = 0;

    for (uint32_t i = 0; i < manager->bucket_count && *count < max_tasks; i++) {
        task_node_t* node = manager->buckets[i];
        while (node && *count < max_tasks) {
            if (node->task.assigned_agent_id == agent_id) {
                task_ids[(*count)++] = node->task.task_id;
            }
            node = node->next;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)manager->mutex);

    return 0;
}

int swarm_task_get_pending(
    const swarm_task_manager_t* manager,
    uint64_t* task_ids,
    uint32_t max_tasks,
    uint32_t* count)
{
    if (!manager || !task_ids || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_get_pending: required parameter is NULL (manager, task_ids, count)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)manager->mutex);

    *count = 0;

    for (uint32_t i = 0; i < manager->bucket_count && *count < max_tasks; i++) {
        task_node_t* node = manager->buckets[i];
        while (node && *count < max_tasks) {
            if (node->task.status == SWARM_TASK_STATUS_QUEUED) {
                task_ids[(*count)++] = node->task.task_id;
            }
            node = node->next;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)manager->mutex);

    return 0;
}

bool swarm_task_dependencies_satisfied(
    const swarm_task_manager_t* manager,
    uint64_t task_id)
{
    if (!manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_dependencies_satisfied: manager is NULL");
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)manager->mutex);

    task_node_t* node = find_task_node(manager, task_id);
    if (!node) {
        nimcp_mutex_unlock((nimcp_mutex_t*)manager->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_task_dependencies_satisfied: node is NULL");
        return false;
    }

    bool satisfied = check_dependencies(manager, &node->task);

    nimcp_mutex_unlock((nimcp_mutex_t*)manager->mutex);

    return satisfied;
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* swarm_task_status_name(swarm_task_status_t status)
{
    if (status >= SWARM_TASK_STATUS_COUNT) {
        return "UNKNOWN";
    }
    return TASK_STATUS_NAMES[status];
}

const char* swarm_task_priority_name(swarm_task_priority_t priority)
{
    if (priority >= SWARM_TASK_PRIORITY_COUNT) {
        return "UNKNOWN";
    }
    return TASK_PRIORITY_NAMES[priority];
}

const char* swarm_task_type_name(swarm_task_type_t type)
{
    if (type >= SWARM_TASK_TYPE_COUNT) {
        return "UNKNOWN";
    }
    return TASK_TYPE_NAMES[type];
}

const char* swarm_task_failure_name(swarm_task_failure_reason_t reason)
{
    if (reason >= SWARM_TASK_FAIL_COUNT) {
        return "UNKNOWN";
    }
    return TASK_FAILURE_NAMES[reason];
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * @brief Query knowledge graph for module self-knowledge
 *
 * WHAT: Introspect module identity from knowledge graph
 * WHY:  Enable self-awareness and runtime reflection
 * HOW:  Query KG for Swarm_Task entity and its relations
 *
 * @param kg Knowledge graph reader
 * @return 1 if self-knowledge found, 0 otherwise
 */
int swarm_task_query_self_knowledge(kg_reader_t* kg)
{
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Swarm_Task");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            NIMCP_LOGGING_DEBUG("Task self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Swarm_Task");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Swarm_Task");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
