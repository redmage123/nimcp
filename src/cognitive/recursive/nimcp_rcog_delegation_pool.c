/**
 * @file nimcp_rcog_delegation_pool.c
 * @brief Recursive Cognition Delegation Pool Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/recursive/nimcp_rcog_delegation_pool.h"
#include "cognitive/recursive/nimcp_rcog_bio_async_bridge.h"
#include "cognitive/recursive/nimcp_rcog_immune_bridge.h"
#include "cognitive/recursive/nimcp_rcog_collective_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_timing_constants.h"

BRIDGE_BOILERPLATE(rcog_delegation_pool, MESH_ADAPTER_CATEGORY_COGNITIVE)

void rcog_delegation_pool_set_instance_health_agent(void* ctx, nimcp_health_agent_t* agent) {
    (void)ctx;
    g_rcog_delegation_pool_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_delegation_pool_training_begin(void* ctx) {
    rcog_delegation_pool_t* pool = (rcog_delegation_pool_t*)ctx;
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_delegation_pool_training_begin: NULL argument");
        return -1;
    }
    rcog_delegation_pool_heartbeat_instance(
        g_rcog_delegation_pool_instance_health_agent,
        "rcog_pool_training_begin", 0.0f);
    return 0;
}

int rcog_delegation_pool_training_step(void* ctx, float progress) {
    rcog_delegation_pool_t* pool = (rcog_delegation_pool_t*)ctx;
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_delegation_pool_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_delegation_pool_heartbeat_instance(
        g_rcog_delegation_pool_instance_health_agent,
        "rcog_pool_training_step", clamped);
    return 0;
}

int rcog_delegation_pool_training_end(void* ctx) {
    rcog_delegation_pool_t* pool = (rcog_delegation_pool_t*)ctx;
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_delegation_pool_training_end: NULL argument");
        return -1;
    }
    rcog_delegation_pool_heartbeat_instance(
        g_rcog_delegation_pool_instance_health_agent,
        "rcog_pool_training_end", 1.0f);
    return 0;
}

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define POOL_INITIAL_RESULT_CAPACITY 64
#define POOL_WORKER_POLL_INTERVAL_MS 10
#define POOL_SHUTDOWN_POLL_INTERVAL_MS 50

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Priority queue node for pending tasks
 */
typedef struct rcog_task_node {
    rcog_subtask_t* subtask;
    float effective_priority;    /**< Priority with tier boost */
    rcog_submit_options_t options;
    uint64_t submit_time_ms;
    struct rcog_task_node* next;
} rcog_task_node_t;

/**
 * @brief Per-worker queue
 */
typedef struct {
    rcog_task_node_t* head;
    rcog_task_node_t* tail;
    size_t count;
    nimcp_mutex_t* mutex;
} rcog_worker_queue_t;

/**
 * @brief Worker thread context
 */
typedef struct {
    uint32_t worker_id;
    rcog_capability_tier_t tier;
    rcog_worker_state_t state;

    /* Thread */
    nimcp_thread_t thread;
    bool thread_started;
    bool should_stop;

    /* Current task */
    rcog_subtask_t* current_task;
    uint64_t current_task_start_ms;

    /* Local queue */
    rcog_worker_queue_t queue;

    /* Statistics */
    rcog_worker_stats_t stats;
    uint64_t state_start_ms;

    /* Parent pool reference */
    struct rcog_delegation_pool* pool;
} rcog_worker_t;

/**
 * @brief Completed task result storage
 */
typedef struct {
    uint64_t task_id;
    rcog_subtask_result_t result;
    uint64_t completed_ms;
    bool retrieved;
} rcog_completed_task_t;

/**
 * @brief Batch handle implementation
 */
struct rcog_batch_handle {
    uint64_t batch_id;
    uint64_t* task_ids;
    size_t num_tasks;
    size_t capacity;
    rcog_subtask_status_t* statuses;
    size_t completed_count;
    bool all_done;
    uint64_t created_ms;
    nimcp_mutex_t* mutex;
    nimcp_cond_t* cond;
};

/**
 * @brief Delegation pool internal structure
 */
struct rcog_delegation_pool {
    /* Configuration */
    rcog_delegation_pool_config_t config;

    /* State */
    bool running;
    bool paused;

    /* Workers */
    rcog_worker_t* workers;
    uint32_t num_workers;
    uint32_t workers_per_tier[RCOG_TIER_COUNT];

    /* Global priority queue (overflow) */
    rcog_worker_queue_t global_queue;

    /* Completed results */
    rcog_completed_task_t* completed;
    size_t completed_count;
    size_t completed_capacity;
    nimcp_mutex_t* completed_mutex;

    /* Batch tracking */
    rcog_batch_handle_t** batches;
    size_t num_batches;
    size_t batches_capacity;
    uint64_t next_batch_id;
    nimcp_mutex_t* batches_mutex;

    /* Connections */
    struct rcog_tool_router* tool_router;
    struct rcog_context_store* context_store;
    struct rcog_bio_async_bridge* bio_async;
    struct rcog_immune_bridge* immune;
    struct rcog_collective_bridge* collective;

    /* Immune modulation */
    rcog_immune_modulation_t current_modulation;
    float effective_capacity;

    /* Statistics */
    rcog_delegation_pool_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Task ID generation */
    uint64_t next_task_id;
};

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

static void* worker_thread_func(void* arg);
static rcog_task_node_t* create_task_node(rcog_subtask_t* subtask,
                                          const rcog_submit_options_t* options,
                                          float tier_boost);
static void free_task_node(rcog_task_node_t* node);
static void enqueue_task(rcog_worker_queue_t* queue, rcog_task_node_t* node);
static rcog_task_node_t* dequeue_task(rcog_worker_queue_t* queue);
static rcog_task_node_t* steal_task(rcog_worker_queue_t* queue);
static void execute_task(rcog_worker_t* worker, rcog_task_node_t* node);
static void store_result(rcog_delegation_pool_t* pool,
                        uint64_t task_id,
                        const rcog_subtask_result_t* result);
static void update_batch_status(rcog_delegation_pool_t* pool,
                               uint64_t task_id,
                               rcog_subtask_status_t status);
static uint32_t find_best_worker(rcog_delegation_pool_t* pool,
                                rcog_capability_tier_t tier);
static size_t try_work_stealing(rcog_worker_t* worker);

/*=============================================================================
 * QUEUE HELPERS
 *===========================================================================*/

static void init_worker_queue(rcog_worker_queue_t* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;

    mutex_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = MUTEX_TYPE_NORMAL;
    queue->mutex = nimcp_mutex_create(&attr);
}

static void destroy_worker_queue(rcog_worker_queue_t* queue) {
    if (!queue) return;

    /* Free all pending nodes */
    rcog_task_node_t* node = queue->head;
    while (node) {
        rcog_task_node_t* next = node->next;
        free_task_node(node);
        node = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;

    if (queue->mutex) {
        nimcp_mutex_destroy(queue->mutex);
        queue->mutex = NULL;
    }
}

static rcog_task_node_t* create_task_node(rcog_subtask_t* subtask,
                                          const rcog_submit_options_t* options,
                                          float tier_boost) {
    rcog_task_node_t* node = nimcp_calloc(1, sizeof(rcog_task_node_t));
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate node");

        return NULL;

    }

    node->subtask = subtask;
    node->submit_time_ms = nimcp_platform_time_monotonic_ms();
    node->next = NULL;

    if (options) {
        node->options = *options;
        if (options->priority_override >= 0) {
            node->effective_priority = options->priority_override + tier_boost;
        } else {
            node->effective_priority = subtask->priority + tier_boost;
        }
    } else {
        node->effective_priority = subtask->priority + tier_boost;
    }

    return node;
}

static void free_task_node(rcog_task_node_t* node) {
    if (!node) return;
    /* Note: subtask ownership transferred to executor */
    nimcp_free(node);
    node = NULL;
}

static void enqueue_task(rcog_worker_queue_t* queue, rcog_task_node_t* node) {
    if (!queue || !node) return;

    nimcp_mutex_lock(queue->mutex);

    /* Insert in priority order (higher priority first) */
    if (!queue->head || node->effective_priority > queue->head->effective_priority) {
        /* Insert at head */
        node->next = queue->head;
        queue->head = node;
        if (!queue->tail) {
            queue->tail = node;
        }
    } else {
        /* Find insertion point */
        rcog_task_node_t* prev = queue->head;
        while (prev->next && prev->next->effective_priority >= node->effective_priority) {
            prev = prev->next;
        }
        node->next = prev->next;
        prev->next = node;
        if (!node->next) {
            queue->tail = node;
        }
    }

    queue->count++;
    nimcp_mutex_unlock(queue->mutex);
}

static rcog_task_node_t* dequeue_task(rcog_worker_queue_t* queue) {
    if (!queue) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue is NULL");

        return NULL;

    }

    nimcp_mutex_lock(queue->mutex);

    rcog_task_node_t* node = queue->head;
    if (node) {
        queue->head = node->next;
        if (!queue->head) {
            queue->tail = NULL;
        }
        queue->count--;
        node->next = NULL;
    }

    nimcp_mutex_unlock(queue->mutex);
    return node;
}

static rcog_task_node_t* steal_task(rcog_worker_queue_t* queue) {
    if (!queue) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue is NULL");

        return NULL;

    }

    nimcp_mutex_lock(queue->mutex);

    /* Steal from tail (lowest priority) */
    if (!queue->head || !queue->head->next) {
        nimcp_mutex_unlock(queue->mutex);
        return NULL;  /* Nothing to steal - normal condition */
    }

    /* Find second-to-last node */
    rcog_task_node_t* prev = queue->head;
    while (prev->next && prev->next->next) {
        prev = prev->next;
    }

    rcog_task_node_t* stolen = prev->next;
    prev->next = NULL;
    queue->tail = prev;
    queue->count--;

    nimcp_mutex_unlock(queue->mutex);
    return stolen;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

rcog_delegation_pool_config_t rcog_delegation_pool_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_default_config", 0.0f);


    rcog_delegation_pool_config_t config;
    memset(&config, 0, sizeof(config));

    /* Default tier configuration */
    for (int i = 0; i < RCOG_TIER_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_TIER_COUNT > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)RCOG_TIER_COUNT);
        }

        config.tiers[i].tier = (rcog_capability_tier_t)i;
        config.tiers[i].num_workers = RCOG_POOL_DEFAULT_WORKERS_PER_TIER;
        config.tiers[i].queue_capacity = RCOG_POOL_MAX_PENDING_PER_WORKER;
        config.tiers[i].priority_boost = (RCOG_TIER_COUNT - i) * 10;
        config.tiers[i].enable_work_stealing = true;
        config.tiers[i].enable_affinity = true;
    }

    /* Root tier has fewer workers (coordination only) */
    config.tiers[RCOG_TIER_ROOT].num_workers = 2;

    config.scheduling_policy = RCOG_SCHED_PRIORITY;
    config.enable_work_stealing = true;
    config.work_steal_threshold = RCOG_POOL_WORK_STEAL_THRESHOLD;
    config.work_steal_batch_size = RCOG_POOL_WORK_STEAL_BATCH_SIZE;

    config.default_task_timeout_ms = RCOG_POOL_DEFAULT_TASK_TIMEOUT_MS;
    config.shutdown_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS;

    config.max_memory_per_task = 10 * 1024 * 1024; /* 10 MB */
    config.max_pending_tasks = RCOG_POOL_MAX_TOTAL_PENDING;

    config.enable_bio_async = true;
    config.enable_immune_modulation = true;
    config.enable_collective = false;

    config.enable_tracing = false;
    config.verbose_logging = false;

    return config;
}

rcog_delegation_pool_t* rcog_delegation_pool_create(
    const rcog_delegation_pool_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_create", 0.0f);


    rcog_delegation_pool_t* pool = nimcp_calloc(1, sizeof(rcog_delegation_pool_t));
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate pool");

        return NULL;

    }

    /* Apply config */
    if (config) {
        pool->config = *config;
    } else {
        pool->config = rcog_delegation_pool_default_config();
    }

    /* Calculate total workers */
    pool->num_workers = 0;
    for (int i = 0; i < RCOG_TIER_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_TIER_COUNT > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)RCOG_TIER_COUNT);
        }

        pool->workers_per_tier[i] = pool->config.tiers[i].num_workers;
        pool->num_workers += pool->workers_per_tier[i];
    }

    if (pool->num_workers > RCOG_POOL_MAX_WORKERS) {
        pool->num_workers = RCOG_POOL_MAX_WORKERS;
    }

    /* Create mutex */
    mutex_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = MUTEX_TYPE_NORMAL;
    pool->mutex = nimcp_mutex_create(&attr);
    if (!pool->mutex) {
        nimcp_free(pool);
        pool = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_delegation_pool_create: pool->mutex is NULL");
        return NULL;
    }

    /* Create workers */
    pool->workers = nimcp_calloc(pool->num_workers, sizeof(rcog_worker_t));
    if (!pool->workers) {
        nimcp_mutex_destroy(pool->mutex);
        nimcp_free(pool);
        pool = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "rcog_delegation_pool_create: pool->workers is NULL");
        return NULL;
    }

    /* Initialize workers */
    uint32_t worker_idx = 0;
    for (int tier = 0; tier < RCOG_TIER_COUNT && worker_idx < pool->num_workers; tier++) {
        for (uint32_t i = 0; i < pool->workers_per_tier[tier] && worker_idx < pool->num_workers; i++) {
            rcog_worker_t* worker = &pool->workers[worker_idx];
            worker->worker_id = worker_idx;
            worker->tier = (rcog_capability_tier_t)tier;
            worker->state = RCOG_WORKER_STOPPED;
            worker->should_stop = false;
            worker->pool = pool;
            init_worker_queue(&worker->queue);
            worker_idx++;
        }
    }

    /* Initialize global queue */
    init_worker_queue(&pool->global_queue);

    /* Initialize completed results storage */
    pool->completed_capacity = POOL_INITIAL_RESULT_CAPACITY;
    pool->completed = nimcp_calloc(pool->completed_capacity, sizeof(rcog_completed_task_t));
    if (!pool->completed) return NULL;
    pool->completed_count = 0;
    pool->completed_mutex = nimcp_mutex_create(&attr);

    /* Initialize batch tracking */
    pool->batches_capacity = 16;
    pool->batches = nimcp_calloc(pool->batches_capacity, sizeof(rcog_batch_handle_t*));
    if (!pool->batches) return NULL;
    pool->num_batches = 0;
    pool->next_batch_id = 1;
    pool->batches_mutex = nimcp_mutex_create(&attr);

    /* Initialize modulation */
    pool->effective_capacity = 1.0f;
    pool->current_modulation.capacity_multiplier = 1.0f;
    pool->current_modulation.max_depth_multiplier = 1.0f;
    pool->current_modulation.parallelism_multiplier = 1.0f;
    pool->current_modulation.timeout_multiplier = 1.0f;

    pool->next_task_id = 1;
    pool->running = false;
    pool->paused = false;

    return pool;
}

rcog_delegation_pool_t* rcog_delegation_pool_create_default(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_create_default", 0.0f);


    return rcog_delegation_pool_create(NULL);
}

void rcog_delegation_pool_destroy(rcog_delegation_pool_t* pool) {
    if (!pool) return;

    /* Stop pool if running */
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_destroy", 0.0f);


    if (pool->running) {
        rcog_delegation_pool_stop(pool, pool->config.shutdown_timeout_ms);
    }

    /* Destroy workers */
    if (pool->workers) {
        for (uint32_t i = 0; i < pool->num_workers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pool->num_workers > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(i + 1) / (float)pool->num_workers);
            }

            destroy_worker_queue(&pool->workers[i].queue);
        }
        nimcp_free(pool->workers);
    }

    /* Destroy global queue */
    destroy_worker_queue(&pool->global_queue);

    /* Free completed results */
    if (pool->completed) {
        nimcp_free(pool->completed);
    }
    if (pool->completed_mutex) {
        nimcp_mutex_destroy(pool->completed_mutex);
    }

    /* Free batch handles */
    if (pool->batches) {
        for (size_t i = 0; i < pool->num_batches; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pool->num_batches > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(i + 1) / (float)pool->num_batches);
            }

            if (pool->batches[i]) {
                rcog_delegation_pool_free_batch_handle(pool->batches[i]);
            }
        }
        nimcp_free(pool->batches);
    }
    if (pool->batches_mutex) {
        nimcp_mutex_destroy(pool->batches_mutex);
    }

    /* Destroy main mutex */
    if (pool->mutex) {
        nimcp_mutex_destroy(pool->mutex);
    }

    nimcp_free(pool);
    pool = NULL;
}

int rcog_delegation_pool_start(rcog_delegation_pool_t* pool) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_start", 0.0f);


    nimcp_mutex_lock(pool->mutex);

    if (pool->running) {
        nimcp_mutex_unlock(pool->mutex);
        return RCOG_ERROR_ALREADY_INITIALIZED;
    }

    /* Start worker threads */
    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        rcog_worker_t* worker = &pool->workers[i];
        worker->should_stop = false;
        worker->state = RCOG_WORKER_IDLE;
        worker->state_start_ms = nimcp_platform_time_monotonic_ms();

        if (nimcp_thread_create(&worker->thread, worker_thread_func, worker, NULL) != 0) {
            /* Stop already started threads */
            for (uint32_t j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                     (float)(j + 1) / (float)i);
                }

                pool->workers[j].should_stop = true;
            }
            worker->thread_started = false;
            nimcp_mutex_unlock(pool->mutex);
            return RCOG_ERROR_OUT_OF_MEMORY;
        }
        worker->thread_started = true;
    }

    pool->running = true;
    pool->paused = false;

    nimcp_mutex_unlock(pool->mutex);
    return RCOG_OK;
}

int rcog_delegation_pool_stop(rcog_delegation_pool_t* pool, uint32_t timeout_ms) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_stop", 0.0f);


    nimcp_mutex_lock(pool->mutex);

    if (!pool->running) {
        nimcp_mutex_unlock(pool->mutex);
        return RCOG_OK;
    }

    /* Signal workers to stop */
    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        pool->workers[i].should_stop = true;
    }

    pool->running = false;
    nimcp_mutex_unlock(pool->mutex);

    /* Wait for workers */
    uint64_t start = nimcp_platform_time_monotonic_ms();
    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        if (pool->workers[i].thread_started) {
            uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
            uint32_t remaining = (elapsed < timeout_ms) ? (timeout_ms - elapsed) : 0;

            if (remaining > 0) {
                nimcp_thread_join(pool->workers[i].thread, NULL);
            }
            pool->workers[i].thread_started = false;
        }
        pool->workers[i].state = RCOG_WORKER_STOPPED;
    }

    return RCOG_OK;
}

int rcog_delegation_pool_pause(rcog_delegation_pool_t* pool) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_pause", 0.0f);


    nimcp_mutex_lock(pool->mutex);
    pool->paused = true;
    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

int rcog_delegation_pool_resume(rcog_delegation_pool_t* pool) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_resume", 0.0f);


    nimcp_mutex_lock(pool->mutex);
    pool->paused = false;
    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

int rcog_delegation_pool_connect_tool_router(
    rcog_delegation_pool_t* pool,
    struct rcog_tool_router* router
) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_connect_tool_router", 0.0f);


    nimcp_mutex_lock(pool->mutex);
    pool->tool_router = router;
    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

int rcog_delegation_pool_connect_context_store(
    rcog_delegation_pool_t* pool,
    struct rcog_context_store* store
) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_connect_context_stor", 0.0f);


    nimcp_mutex_lock(pool->mutex);
    pool->context_store = store;
    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

int rcog_delegation_pool_connect_bio_async(
    rcog_delegation_pool_t* pool,
    struct rcog_bio_async_bridge* bio_async
) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_connect_bio_async", 0.0f);


    nimcp_mutex_lock(pool->mutex);
    pool->bio_async = bio_async;
    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

int rcog_delegation_pool_connect_immune(
    rcog_delegation_pool_t* pool,
    struct rcog_immune_bridge* immune
) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_connect_immune", 0.0f);


    nimcp_mutex_lock(pool->mutex);
    pool->immune = immune;
    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

int rcog_delegation_pool_connect_collective(
    rcog_delegation_pool_t* pool,
    struct rcog_collective_bridge* collective
) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_connect_collective", 0.0f);


    nimcp_mutex_lock(pool->mutex);
    pool->collective = collective;
    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

/*=============================================================================
 * TASK SUBMISSION
 *===========================================================================*/

rcog_submit_options_t rcog_delegation_pool_default_submit_options(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_default_submit_optio", 0.0f);


    rcog_submit_options_t options;
    memset(&options, 0, sizeof(options));

    options.priority_override = -1.0f;  /* Use task's priority */
    options.tier_override = (rcog_capability_tier_t)(-1);  /* Use task's tier */
    options.timeout_override_ms = 0;
    options.allow_work_stealing = true;
    options.prefer_local = true;
    options.callback = NULL;
    options.callback_data = NULL;

    return options;
}

int rcog_delegation_pool_submit(
    rcog_delegation_pool_t* pool,
    rcog_subtask_t* subtask,
    const rcog_submit_options_t* options
) {
    if (!pool || !subtask) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_submit", 0.0f);


    nimcp_mutex_lock(pool->mutex);

    if (!pool->running) {
        nimcp_mutex_unlock(pool->mutex);
        return RCOG_ERROR_NOT_INITIALIZED;
    }

    /* Check capacity */
    size_t total_pending = pool->global_queue.count;
    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        total_pending += pool->workers[i].queue.count;
    }

    if (total_pending >= pool->config.max_pending_tasks) {
        nimcp_mutex_unlock(pool->mutex);
        return RCOG_ERROR_CONTEXT_FULL;
    }

    /* Assign task ID if not set */
    if (subtask->task_id == 0) {
        subtask->task_id = pool->next_task_id++;
    }

    /* Determine target tier */
    rcog_capability_tier_t tier = subtask->tier;
    if (options && (int)options->tier_override >= 0) {
        tier = options->tier_override;
    }

    /* Get tier boost for priority */
    float tier_boost = (float)pool->config.tiers[tier].priority_boost;

    /* Create task node */
    rcog_task_node_t* node = create_task_node(subtask, options, tier_boost);
    if (!node) {
        nimcp_mutex_unlock(pool->mutex);
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    /* Find best worker */
    uint32_t worker_id = find_best_worker(pool, tier);

    nimcp_mutex_unlock(pool->mutex);

    /* Enqueue task */
    if (worker_id < pool->num_workers) {
        enqueue_task(&pool->workers[worker_id].queue, node);
    } else {
        enqueue_task(&pool->global_queue, node);
    }

    /* Update stats */
    nimcp_mutex_lock(pool->mutex);
    pool->stats.tasks_submitted++;
    pool->stats.current_pending++;
    if (pool->stats.current_pending > pool->stats.peak_pending) {
        pool->stats.peak_pending = pool->stats.current_pending;
    }
    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

int rcog_delegation_pool_submit_batch(
    rcog_delegation_pool_t* pool,
    rcog_subtask_t** subtasks,
    size_t num_subtasks,
    const rcog_submit_options_t* options,
    rcog_batch_handle_t** handle
) {
    if (!pool || !subtasks || !handle) return RCOG_ERROR_NULL_POINTER;
    if (num_subtasks == 0) return RCOG_ERROR_INVALID_CONFIG;

    /* Create batch handle */
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_submit_batch", 0.0f);


    rcog_batch_handle_t* batch = nimcp_calloc(1, sizeof(rcog_batch_handle_t));
    if (!batch) return RCOG_ERROR_OUT_OF_MEMORY;

    batch->capacity = num_subtasks;
    batch->task_ids = nimcp_calloc(num_subtasks, sizeof(uint64_t));
    batch->statuses = nimcp_calloc(num_subtasks, sizeof(rcog_subtask_status_t));

    if (!batch->task_ids || !batch->statuses) {
        if (batch->task_ids) nimcp_free(batch->task_ids);
        if (batch->statuses) nimcp_free(batch->statuses);
        nimcp_free(batch);
        batch = NULL;
        return RCOG_ERROR_OUT_OF_MEMORY;
    }

    mutex_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.type = MUTEX_TYPE_NORMAL;
    batch->mutex = nimcp_mutex_create(&attr);
    batch->cond = nimcp_calloc(1, sizeof(nimcp_cond_t));
    if (batch->cond) {
        nimcp_cond_init(batch->cond);
    }

    nimcp_mutex_lock(pool->mutex);
    batch->batch_id = pool->next_batch_id++;
    batch->created_ms = nimcp_platform_time_monotonic_ms();
    nimcp_mutex_unlock(pool->mutex);

    /* Submit tasks */
    size_t submitted = 0;
    for (size_t i = 0; i < num_subtasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_subtasks > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)num_subtasks);
        }

        int err = rcog_delegation_pool_submit(pool, subtasks[i], options);
        if (err == RCOG_OK) {
            batch->task_ids[submitted] = subtasks[i]->task_id;
            batch->statuses[submitted] = RCOG_SUBTASK_QUEUED;
            submitted++;
        }
    }

    batch->num_tasks = submitted;

    /* Register batch */
    nimcp_mutex_lock(pool->batches_mutex);

    if (pool->num_batches >= pool->batches_capacity) {
        size_t new_cap = pool->batches_capacity * 2;
        rcog_batch_handle_t** new_batches = nimcp_realloc(
            pool->batches, new_cap * sizeof(rcog_batch_handle_t*)
        );
        if (new_batches) {
            pool->batches = new_batches;
            pool->batches_capacity = new_cap;
        }
    }

    if (pool->num_batches < pool->batches_capacity) {
        pool->batches[pool->num_batches++] = batch;
    }

    nimcp_mutex_unlock(pool->batches_mutex);

    *handle = batch;
    return RCOG_OK;
}

/*=============================================================================
 * BATCH MANAGEMENT
 *===========================================================================*/

int rcog_delegation_pool_await_batch(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle,
    uint32_t timeout_ms
) {
    if (!pool || !handle) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_await_batch", 0.0f);


    uint64_t start = nimcp_platform_time_monotonic_ms();

    while (true) {
        nimcp_mutex_lock(handle->mutex);

        if (handle->all_done) {
            nimcp_mutex_unlock(handle->mutex);
            return RCOG_OK;
        }

        /* Check timeout */
        if (timeout_ms > 0) {
            uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
            if (elapsed >= timeout_ms) {
                nimcp_mutex_unlock(handle->mutex);
                return RCOG_ERROR_TIMEOUT;
            }

            /* Wait with timeout */
            uint32_t remaining = timeout_ms - (uint32_t)elapsed;
            nimcp_cond_timedwait(handle->cond, handle->mutex, remaining);
        } else {
            nimcp_cond_wait(handle->cond, handle->mutex);
        }

        nimcp_mutex_unlock(handle->mutex);
    }
}

bool rcog_delegation_pool_poll_batch(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle,
    size_t* completed,
    size_t* total
) {
    if (!pool || !handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_delegation_pool_poll_batch: required parameter is NULL (pool, handle)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_poll_batch", 0.0f);


    nimcp_mutex_lock(handle->mutex);

    if (completed) *completed = handle->completed_count;
    if (total) *total = handle->num_tasks;
    bool done = handle->all_done;

    nimcp_mutex_unlock(handle->mutex);

    return done;
}

int rcog_delegation_pool_get_batch_results(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle,
    rcog_subtask_result_t* results,
    size_t max_results,
    size_t* num_results
) {
    if (!pool || !handle || !results || !num_results) return RCOG_ERROR_NULL_POINTER;

    *num_results = 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_batch_results", 0.0f);


    nimcp_mutex_lock(pool->completed_mutex);

    for (size_t i = 0; i < handle->num_tasks && *num_results < max_results; i++) {
        uint64_t task_id = handle->task_ids[i];

        /* Find result */
        for (size_t j = 0; j < pool->completed_count; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && pool->completed_count > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(j + 1) / (float)pool->completed_count);
            }

            if (pool->completed[j].task_id == task_id && !pool->completed[j].retrieved) {
                results[*num_results] = pool->completed[j].result;
                pool->completed[j].retrieved = true;
                (*num_results)++;
                break;
            }
        }
    }

    nimcp_mutex_unlock(pool->completed_mutex);

    return RCOG_OK;
}

size_t rcog_delegation_pool_cancel_batch(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle
) {
    if (!pool || !handle) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_cancel_batch", 0.0f);


    size_t cancelled = 0;

    for (size_t i = 0; i < handle->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && handle->num_tasks > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)handle->num_tasks);
        }

        if (rcog_delegation_pool_cancel_task(pool, handle->task_ids[i]) == RCOG_OK) {
            cancelled++;
        }
    }

    return cancelled;
}

void rcog_delegation_pool_free_batch_handle(rcog_batch_handle_t* handle) {
    if (!handle) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_free_batch_handle", 0.0f);


    if (handle->task_ids) nimcp_free(handle->task_ids);
    if (handle->statuses) nimcp_free(handle->statuses);
    if (handle->mutex) nimcp_mutex_destroy(handle->mutex);
    if (handle->cond) {
        nimcp_cond_destroy(handle->cond);
        nimcp_free(handle->cond);
    }

    nimcp_free(handle);
    handle = NULL;
}

/*=============================================================================
 * TASK MANAGEMENT
 *===========================================================================*/

int rcog_delegation_pool_get_task_status(
    rcog_delegation_pool_t* pool,
    uint64_t task_id,
    rcog_subtask_status_t* status
) {
    if (!pool || !status) return RCOG_ERROR_NULL_POINTER;

    /* Check completed */
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_task_status", 0.0f);


    nimcp_mutex_lock(pool->completed_mutex);
    for (size_t i = 0; i < pool->completed_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->completed_count > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->completed_count);
        }

        if (pool->completed[i].task_id == task_id) {
            *status = pool->completed[i].result.status;
            nimcp_mutex_unlock(pool->completed_mutex);
            return RCOG_OK;
        }
    }
    nimcp_mutex_unlock(pool->completed_mutex);

    /* Check workers */
    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        rcog_worker_t* worker = &pool->workers[i];

        if (worker->current_task && worker->current_task->task_id == task_id) {
            *status = RCOG_SUBTASK_RUNNING;
            return RCOG_OK;
        }

        /* Check queue */
        nimcp_mutex_lock(worker->queue.mutex);
        rcog_task_node_t* node = worker->queue.head;
        while (node) {
            if (node->subtask->task_id == task_id) {
                *status = RCOG_SUBTASK_QUEUED;
                nimcp_mutex_unlock(worker->queue.mutex);
                return RCOG_OK;
            }
            node = node->next;
        }
        nimcp_mutex_unlock(worker->queue.mutex);
    }

    /* Check global queue */
    nimcp_mutex_lock(pool->global_queue.mutex);
    rcog_task_node_t* node = pool->global_queue.head;
    while (node) {
        if (node->subtask->task_id == task_id) {
            *status = RCOG_SUBTASK_QUEUED;
            nimcp_mutex_unlock(pool->global_queue.mutex);
            return RCOG_OK;
        }
        node = node->next;
    }
    nimcp_mutex_unlock(pool->global_queue.mutex);

    return RCOG_ERROR_CONTEXT_NOT_FOUND;
}

int rcog_delegation_pool_get_task_result(
    rcog_delegation_pool_t* pool,
    uint64_t task_id,
    rcog_subtask_result_t* result
) {
    if (!pool || !result) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_task_result", 0.0f);


    nimcp_mutex_lock(pool->completed_mutex);

    for (size_t i = 0; i < pool->completed_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->completed_count > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->completed_count);
        }

        if (pool->completed[i].task_id == task_id) {
            *result = pool->completed[i].result;
            nimcp_mutex_unlock(pool->completed_mutex);
            return RCOG_OK;
        }
    }

    nimcp_mutex_unlock(pool->completed_mutex);
    return RCOG_ERROR_CONTEXT_NOT_FOUND;
}

int rcog_delegation_pool_cancel_task(
    rcog_delegation_pool_t* pool,
    uint64_t task_id
) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Cannot cancel running task easily, only pending */

    /* Check worker queues */
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_cancel_task", 0.0f);


    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        rcog_worker_t* worker = &pool->workers[i];

        nimcp_mutex_lock(worker->queue.mutex);

        rcog_task_node_t* prev = NULL;
        rcog_task_node_t* node = worker->queue.head;

        while (node) {
            if (node->subtask->task_id == task_id) {
                /* Remove from queue */
                if (prev) {
                    prev->next = node->next;
                } else {
                    worker->queue.head = node->next;
                }
                if (worker->queue.tail == node) {
                    worker->queue.tail = prev;
                }
                worker->queue.count--;

                nimcp_mutex_unlock(worker->queue.mutex);

                /* Store cancelled result */
                rcog_subtask_result_t result = {0};
                result.task_id = task_id;
                result.status = RCOG_SUBTASK_CANCELLED;
                result.success = false;
                store_result(pool, task_id, &result);

                free_task_node(node);

                nimcp_mutex_lock(pool->mutex);
                pool->stats.tasks_cancelled++;
                pool->stats.current_pending--;
                nimcp_mutex_unlock(pool->mutex);

                return RCOG_OK;
            }
            prev = node;
            node = node->next;
        }

        nimcp_mutex_unlock(worker->queue.mutex);
    }

    /* Check global queue */
    nimcp_mutex_lock(pool->global_queue.mutex);

    rcog_task_node_t* prev = NULL;
    rcog_task_node_t* node = pool->global_queue.head;

    while (node) {
        if (node->subtask->task_id == task_id) {
            if (prev) {
                prev->next = node->next;
            } else {
                pool->global_queue.head = node->next;
            }
            if (pool->global_queue.tail == node) {
                pool->global_queue.tail = prev;
            }
            pool->global_queue.count--;

            nimcp_mutex_unlock(pool->global_queue.mutex);

            rcog_subtask_result_t result = {0};
            result.task_id = task_id;
            result.status = RCOG_SUBTASK_CANCELLED;
            result.success = false;
            store_result(pool, task_id, &result);

            free_task_node(node);

            nimcp_mutex_lock(pool->mutex);
            pool->stats.tasks_cancelled++;
            pool->stats.current_pending--;
            nimcp_mutex_unlock(pool->mutex);

            return RCOG_OK;
        }
        prev = node;
        node = node->next;
    }

    nimcp_mutex_unlock(pool->global_queue.mutex);

    return RCOG_ERROR_CONTEXT_NOT_FOUND;
}

size_t rcog_delegation_pool_cancel_all(rcog_delegation_pool_t* pool) {
    if (!pool) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_cancel_all", 0.0f);


    size_t cancelled = 0;

    /* Cancel from worker queues */
    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        rcog_worker_t* worker = &pool->workers[i];

        nimcp_mutex_lock(worker->queue.mutex);

        while (worker->queue.head) {
            rcog_task_node_t* node = worker->queue.head;
            worker->queue.head = node->next;
            worker->queue.count--;

            rcog_subtask_result_t result = {0};
            result.task_id = node->subtask->task_id;
            result.status = RCOG_SUBTASK_CANCELLED;
            result.success = false;
            store_result(pool, node->subtask->task_id, &result);

            free_task_node(node);
            cancelled++;
        }
        worker->queue.tail = NULL;

        nimcp_mutex_unlock(worker->queue.mutex);
    }

    /* Cancel from global queue */
    nimcp_mutex_lock(pool->global_queue.mutex);

    while (pool->global_queue.head) {
        rcog_task_node_t* node = pool->global_queue.head;
        pool->global_queue.head = node->next;
        pool->global_queue.count--;

        rcog_subtask_result_t result = {0};
        result.task_id = node->subtask->task_id;
        result.status = RCOG_SUBTASK_CANCELLED;
        result.success = false;
        store_result(pool, node->subtask->task_id, &result);

        free_task_node(node);
        cancelled++;
    }
    pool->global_queue.tail = NULL;

    nimcp_mutex_unlock(pool->global_queue.mutex);

    nimcp_mutex_lock(pool->mutex);
    pool->stats.tasks_cancelled += cancelled;
    pool->stats.current_pending = 0;
    nimcp_mutex_unlock(pool->mutex);

    return cancelled;
}

/*=============================================================================
 * WORKER MANAGEMENT
 *===========================================================================*/

uint32_t rcog_delegation_pool_get_worker_count(
    const rcog_delegation_pool_t* pool,
    rcog_capability_tier_t tier
) {
    if (!pool || tier >= RCOG_TIER_COUNT) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_worker_count", 0.0f);


    return pool->workers_per_tier[tier];
}

uint32_t rcog_delegation_pool_get_total_workers(
    const rcog_delegation_pool_t* pool
) {
    if (!pool) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_total_workers", 0.0f);


    return pool->num_workers;
}

int rcog_delegation_pool_get_worker_info(
    const rcog_delegation_pool_t* pool,
    uint32_t worker_id,
    rcog_worker_info_t* info
) {
    if (!pool || !info) return RCOG_ERROR_NULL_POINTER;
    if (worker_id >= pool->num_workers) return RCOG_ERROR_INVALID_CONFIG;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_worker_info", 0.0f);


    const rcog_worker_t* worker = &pool->workers[worker_id];

    info->worker_id = worker->worker_id;
    info->tier = worker->tier;
    info->state = worker->state;
    info->queue_depth = worker->queue.count;
    info->current_task_id = worker->current_task ? worker->current_task->task_id : 0;
    info->current_task_start_ms = worker->current_task_start_ms;
    info->stats = worker->stats;

    return RCOG_OK;
}

int rcog_delegation_pool_get_all_workers(
    const rcog_delegation_pool_t* pool,
    rcog_worker_info_t* infos,
    size_t max_infos,
    size_t* num_infos
) {
    if (!pool || !infos || !num_infos) return RCOG_ERROR_NULL_POINTER;

    *num_infos = 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_all_workers", 0.0f);


    for (uint32_t i = 0; i < pool->num_workers && *num_infos < max_infos; i++) {
        rcog_delegation_pool_get_worker_info(pool, i, &infos[*num_infos]);
        (*num_infos)++;
    }

    return RCOG_OK;
}

int rcog_delegation_pool_scale_tier(
    rcog_delegation_pool_t* pool,
    rcog_capability_tier_t tier,
    uint32_t new_count
) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;
    if (tier >= RCOG_TIER_COUNT) return RCOG_ERROR_INVALID_CONFIG;

    /* Dynamic scaling not yet implemented */
    /* Would require stopping/starting workers dynamically */

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_scale_tier", 0.0f);


    return RCOG_ERROR_INVALID_CONFIG;
}

/*=============================================================================
 * IMMUNE MODULATION
 *===========================================================================*/

int rcog_delegation_pool_apply_immune_modulation(
    rcog_delegation_pool_t* pool,
    const rcog_immune_modulation_t* modulation
) {
    if (!pool || !modulation) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_apply_immune_modulat", 0.0f);


    nimcp_mutex_lock(pool->mutex);

    pool->current_modulation = *modulation;
    pool->effective_capacity = modulation->capacity_multiplier;

    /* Pause excess workers if capacity reduced */
    if (modulation->capacity_multiplier < 1.0f) {
        uint32_t target_active = (uint32_t)(pool->num_workers * modulation->capacity_multiplier);
        if (target_active < 1) target_active = 1;

        uint32_t active_count = 0;
        for (uint32_t i = 0; i < pool->num_workers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pool->num_workers > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(i + 1) / (float)pool->num_workers);
            }

            if (active_count >= target_active) {
                pool->workers[i].state = RCOG_WORKER_PAUSED;
            } else if (pool->workers[i].state == RCOG_WORKER_PAUSED) {
                pool->workers[i].state = RCOG_WORKER_IDLE;
            }
            if (pool->workers[i].state != RCOG_WORKER_PAUSED) {
                active_count++;
            }
        }
    } else {
        /* Resume all paused workers */
        for (uint32_t i = 0; i < pool->num_workers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pool->num_workers > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(i + 1) / (float)pool->num_workers);
            }

            if (pool->workers[i].state == RCOG_WORKER_PAUSED) {
                pool->workers[i].state = RCOG_WORKER_IDLE;
            }
        }
    }

    pool->stats.current_capacity_factor = modulation->capacity_multiplier;

    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

float rcog_delegation_pool_get_effective_capacity(
    const rcog_delegation_pool_t* pool
) {
    if (!pool) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_effective_capaci", 0.0f);


    return pool->effective_capacity;
}

/*=============================================================================
 * WORK STEALING
 *===========================================================================*/

int rcog_delegation_pool_set_work_stealing(
    rcog_delegation_pool_t* pool,
    bool enable
) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_set_work_stealing", 0.0f);


    nimcp_mutex_lock(pool->mutex);
    pool->config.enable_work_stealing = enable;
    nimcp_mutex_unlock(pool->mutex);

    return RCOG_OK;
}

size_t rcog_delegation_pool_rebalance(rcog_delegation_pool_t* pool) {
    if (!pool) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_rebalance", 0.0f);


    size_t moved = 0;

    /* Find worker with most tasks */
    uint32_t max_worker = 0;
    size_t max_count = 0;

    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        if (pool->workers[i].queue.count > max_count) {
            max_count = pool->workers[i].queue.count;
            max_worker = i;
        }
    }

    /* Find idle workers and steal */
    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        if (i != max_worker && pool->workers[i].queue.count == 0) {
            /* Try to steal */
            rcog_task_node_t* stolen = steal_task(&pool->workers[max_worker].queue);
            if (stolen) {
                enqueue_task(&pool->workers[i].queue, stolen);
                moved++;
                pool->workers[i].stats.tasks_stolen++;
                pool->workers[max_worker].stats.tasks_given++;
            }
        }
    }

    return moved;
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int rcog_delegation_pool_get_stats(
    const rcog_delegation_pool_t* pool,
    rcog_delegation_pool_stats_t* stats
) {
    if (!pool || !stats) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_stats", 0.0f);


    nimcp_mutex_lock(((rcog_delegation_pool_t*)pool)->mutex);
    *stats = pool->stats;

    /* Compute derived stats */
    uint32_t active = 0, idle = 0, paused = 0;
    float total_util = 0.0f;

    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        switch (pool->workers[i].state) {
            case RCOG_WORKER_BUSY:
            case RCOG_WORKER_STEALING:
                active++;
                break;
            case RCOG_WORKER_IDLE:
                idle++;
                break;
            case RCOG_WORKER_PAUSED:
                paused++;
                break;
            default:
                break;
        }
        total_util += pool->workers[i].stats.utilization;
    }

    stats->active_workers = active;
    stats->idle_workers = idle;
    stats->paused_workers = paused;
    stats->avg_worker_utilization = pool->num_workers > 0 ?
        total_util / pool->num_workers : 0.0f;

    nimcp_mutex_unlock(((rcog_delegation_pool_t*)pool)->mutex);

    return RCOG_OK;
}

void rcog_delegation_pool_reset_stats(rcog_delegation_pool_t* pool) {
    if (!pool) return;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_reset_stats", 0.0f);


    nimcp_mutex_lock(pool->mutex);
    memset(&pool->stats, 0, sizeof(pool->stats));

    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        memset(&pool->workers[i].stats, 0, sizeof(rcog_worker_stats_t));
    }

    nimcp_mutex_unlock(pool->mutex);
}

size_t rcog_delegation_pool_get_queue_depth(
    const rcog_delegation_pool_t* pool
) {
    if (!pool) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_queue_depth", 0.0f);


    size_t total = pool->global_queue.count;
    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        total += pool->workers[i].queue.count;
    }
    return total;
}

size_t rcog_delegation_pool_get_tier_queue_depth(
    const rcog_delegation_pool_t* pool,
    rcog_capability_tier_t tier
) {
    if (!pool || tier >= RCOG_TIER_COUNT) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_get_tier_queue_depth", 0.0f);


    size_t total = 0;
    for (uint32_t i = 0; i < pool->num_workers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_workers > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_workers);
        }

        if (pool->workers[i].tier == tier) {
            total += pool->workers[i].queue.count;
        }
    }
    return total;
}

/*=============================================================================
 * UTILITY
 *===========================================================================*/

int rcog_delegation_pool_select_worker(
    rcog_delegation_pool_t* pool,
    const rcog_subtask_t* subtask,
    uint32_t* worker_id
) {
    if (!pool || !subtask || !worker_id) return RCOG_ERROR_NULL_POINTER;

    *worker_id = find_best_worker(pool, subtask->tier);

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_select_worker", 0.0f);


    if (*worker_id >= pool->num_workers) {
        return RCOG_ERROR_WORKER_POOL_EXHAUSTED;
    }

    return RCOG_OK;
}

uint32_t rcog_delegation_pool_estimate_wait_time(
    const rcog_delegation_pool_t* pool,
    rcog_capability_tier_t tier
) {
    if (!pool || tier >= RCOG_TIER_COUNT) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_estimate_wait_time", 0.0f);


    size_t tier_pending = rcog_delegation_pool_get_tier_queue_depth(pool, tier);
    uint32_t tier_workers = pool->workers_per_tier[tier];

    if (tier_workers == 0) return UINT32_MAX;

    /* Estimate based on average task duration */
    float avg_duration = pool->stats.avg_task_duration_ms;
    if (avg_duration <= 0) avg_duration = 100.0f;  /* Default estimate */

    return (uint32_t)((tier_pending / (float)tier_workers) * avg_duration);
}

bool rcog_delegation_pool_has_capacity(const rcog_delegation_pool_t* pool) {
    if (!pool) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_has_capacity", 0.0f);


    size_t total = rcog_delegation_pool_get_queue_depth(pool);
    return total < pool->config.max_pending_tasks;
}

int rcog_delegation_pool_drain(
    rcog_delegation_pool_t* pool,
    uint32_t timeout_ms
) {
    if (!pool) return RCOG_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_drain", 0.0f);


    uint64_t start = nimcp_platform_time_monotonic_ms();

    while (true) {
        size_t pending = rcog_delegation_pool_get_queue_depth(pool);

        /* Also check if any worker is busy */
        bool any_busy = false;
        for (uint32_t i = 0; i < pool->num_workers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pool->num_workers > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(i + 1) / (float)pool->num_workers);
            }

            if (pool->workers[i].state == RCOG_WORKER_BUSY) {
                any_busy = true;
                break;
            }
        }

        if (pending == 0 && !any_busy) {
            return RCOG_OK;
        }

        uint64_t elapsed = nimcp_platform_time_monotonic_ms() - start;
        if (timeout_ms > 0 && elapsed >= timeout_ms) {
            return RCOG_ERROR_TIMEOUT;
        }

        nimcp_platform_sleep_ms(POOL_SHUTDOWN_POLL_INTERVAL_MS);
    }
}

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static uint32_t find_best_worker(rcog_delegation_pool_t* pool,
                                  rcog_capability_tier_t tier) {
    uint32_t best_worker = pool->num_workers;  /* Invalid */
    size_t min_queue = SIZE_MAX;

    /* First pass: find workers at matching tier */
    if (pool->config.tiers[tier].enable_affinity) {
        for (uint32_t i = 0; i < pool->num_workers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pool->num_workers > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(i + 1) / (float)pool->num_workers);
            }

            if (pool->workers[i].tier == tier &&
                pool->workers[i].state != RCOG_WORKER_PAUSED &&
                pool->workers[i].state != RCOG_WORKER_STOPPING &&
                pool->workers[i].queue.count < min_queue) {
                min_queue = pool->workers[i].queue.count;
                best_worker = i;
            }
        }
    }

    /* Second pass: any available worker */
    if (best_worker >= pool->num_workers) {
        for (uint32_t i = 0; i < pool->num_workers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pool->num_workers > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(i + 1) / (float)pool->num_workers);
            }

            if (pool->workers[i].state != RCOG_WORKER_PAUSED &&
                pool->workers[i].state != RCOG_WORKER_STOPPING &&
                pool->workers[i].queue.count < min_queue) {
                min_queue = pool->workers[i].queue.count;
                best_worker = i;
            }
        }
    }

    return best_worker;
}

static void store_result(rcog_delegation_pool_t* pool,
                        uint64_t task_id,
                        const rcog_subtask_result_t* result) {
    nimcp_mutex_lock(pool->completed_mutex);

    /* Grow if needed */
    if (pool->completed_count >= pool->completed_capacity) {
        size_t new_cap = pool->completed_capacity * 2;
        rcog_completed_task_t* new_completed = nimcp_realloc(
            pool->completed,
            new_cap * sizeof(rcog_completed_task_t)
        );
        if (new_completed) {
            pool->completed = new_completed;
            pool->completed_capacity = new_cap;
        }
    }

    if (pool->completed_count < pool->completed_capacity) {
        pool->completed[pool->completed_count].task_id = task_id;
        pool->completed[pool->completed_count].result = *result;
        pool->completed[pool->completed_count].completed_ms =
            nimcp_platform_time_monotonic_ms();
        pool->completed[pool->completed_count].retrieved = false;
        pool->completed_count++;
    }

    nimcp_mutex_unlock(pool->completed_mutex);

    /* Update batch status */
    update_batch_status(pool, task_id, result->status);
}

static void update_batch_status(rcog_delegation_pool_t* pool,
                               uint64_t task_id,
                               rcog_subtask_status_t status) {
    nimcp_mutex_lock(pool->batches_mutex);

    for (size_t i = 0; i < pool->num_batches; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pool->num_batches > 256) {
            rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                             (float)(i + 1) / (float)pool->num_batches);
        }

        rcog_batch_handle_t* batch = pool->batches[i];
        if (!batch) continue;

        nimcp_mutex_lock(batch->mutex);

        for (size_t j = 0; j < batch->num_tasks; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && batch->num_tasks > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(j + 1) / (float)batch->num_tasks);
            }

            if (batch->task_ids[j] == task_id) {
                batch->statuses[j] = status;

                if (status == RCOG_SUBTASK_COMPLETED ||
                    status == RCOG_SUBTASK_FAILED ||
                    status == RCOG_SUBTASK_CANCELLED ||
                    status == RCOG_SUBTASK_TIMEOUT) {
                    batch->completed_count++;

                    if (batch->completed_count >= batch->num_tasks) {
                        batch->all_done = true;
                        nimcp_cond_broadcast(batch->cond);
                    }
                }
                break;
            }
        }

        nimcp_mutex_unlock(batch->mutex);
    }

    nimcp_mutex_unlock(pool->batches_mutex);
}

static size_t try_work_stealing(rcog_worker_t* worker) {
    if (!worker || !worker->pool) return 0;

    rcog_delegation_pool_t* pool = worker->pool;

    if (!pool->config.enable_work_stealing) return 0;

    size_t stolen = 0;

    /* First try global queue */
    rcog_task_node_t* node = dequeue_task(&pool->global_queue);
    if (node) {
        enqueue_task(&worker->queue, node);
        stolen++;
        worker->stats.tasks_stolen++;
    }

    /* Then try other workers */
    if (stolen == 0) {
        for (uint32_t i = 0; i < pool->num_workers && stolen < pool->config.work_steal_batch_size; i++) {
            if (i == worker->worker_id) continue;

            rcog_worker_t* victim = &pool->workers[i];

            /* Only steal if victim has significantly more work */
            if (victim->queue.count > worker->queue.count * pool->config.work_steal_threshold) {
                node = steal_task(&victim->queue);
                if (node) {
                    enqueue_task(&worker->queue, node);
                    stolen++;
                    worker->stats.tasks_stolen++;
                    victim->stats.tasks_given++;
                }
            }
        }
    }

    return stolen;
}

static void execute_task(rcog_worker_t* worker, rcog_task_node_t* node) {
    if (!worker || !node || !node->subtask) return;

    rcog_delegation_pool_t* pool = worker->pool;
    rcog_subtask_t* subtask = node->subtask;

    /* Update state */
    worker->state = RCOG_WORKER_BUSY;
    worker->current_task = subtask;
    worker->current_task_start_ms = nimcp_platform_time_monotonic_ms();
    subtask->status = RCOG_SUBTASK_RUNNING;
    subtask->started_ms = worker->current_task_start_ms;

    /* Prepare result */
    rcog_subtask_result_t result;
    memset(&result, 0, sizeof(result));
    result.task_id = subtask->task_id;

    /* Execute task */
    /* In a real implementation, this would call the tool router */
    /* For now, simulate execution */

    uint32_t timeout = subtask->timeout_ms;
    if (timeout == 0) timeout = pool->config.default_task_timeout_ms;

    /* Apply immune modulation to timeout */
    timeout = (uint32_t)(timeout * pool->current_modulation.timeout_multiplier);

    /* Simulated execution (placeholder for actual tool execution) */
    /* TODO: Connect to tool_router for real execution */
    nimcp_platform_sleep_ms(1);  /* Minimal delay */

    /* Complete task */
    uint64_t end_time = nimcp_platform_time_monotonic_ms();
    uint64_t duration = end_time - worker->current_task_start_ms;

    result.status = RCOG_SUBTASK_COMPLETED;
    result.success = true;
    result.confidence = 0.9f;
    result.duration_ms = duration;

    subtask->status = RCOG_SUBTASK_COMPLETED;
    subtask->completed_ms = end_time;

    /* Store result */
    store_result(pool, subtask->task_id, &result);

    /* Update stats */
    nimcp_mutex_lock(pool->mutex);
    pool->stats.tasks_completed++;
    pool->stats.current_pending--;
    pool->stats.total_processing_time_ms += duration;

    if (pool->stats.tasks_completed > 0) {
        pool->stats.avg_task_duration_ms =
            (float)pool->stats.total_processing_time_ms / pool->stats.tasks_completed;
    }
    nimcp_mutex_unlock(pool->mutex);

    /* Update worker stats */
    worker->stats.tasks_completed++;
    worker->stats.total_processing_ms += duration;
    if (worker->stats.tasks_completed > 0) {
        worker->stats.avg_task_duration_ms =
            (float)worker->stats.total_processing_ms / worker->stats.tasks_completed;
    }

    /* Call callback if set */
    if (node->options.callback) {
        node->options.callback(&result, node->options.callback_data);
    }

    /* Clear current task */
    worker->current_task = NULL;
    worker->current_task_start_ms = 0;
    worker->state = RCOG_WORKER_IDLE;

    /* Free task node */
    free_task_node(node);
}

static void* worker_thread_func(void* arg) {
    rcog_worker_t* worker = (rcog_worker_t*)arg;
    if (!worker || !worker->pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "worker_thread_func: required parameter is NULL (worker, worker->pool)");
        return NULL;
    }

    rcog_delegation_pool_t* pool = worker->pool;

    while (!worker->should_stop) {
        /* Check if paused */
        if (pool->paused || worker->state == RCOG_WORKER_PAUSED) {
            nimcp_platform_sleep_ms(POOL_WORKER_POLL_INTERVAL_MS);
            continue;
        }

        /* Try to get task from local queue */
        rcog_task_node_t* node = dequeue_task(&worker->queue);

        /* If no local task, try work stealing */
        if (!node && pool->config.enable_work_stealing) {
            worker->state = RCOG_WORKER_STEALING;
            try_work_stealing(worker);
            node = dequeue_task(&worker->queue);
            if (!node) {
                worker->state = RCOG_WORKER_IDLE;
            }
        }

        if (node) {
            execute_task(worker, node);
        } else {
            /* No work, sleep briefly */
            worker->state = RCOG_WORKER_IDLE;
            uint64_t now = nimcp_platform_time_monotonic_ms();
            if (worker->state_start_ms == 0) {
                worker->state_start_ms = now;
            }
            worker->stats.idle_time_ms += POOL_WORKER_POLL_INTERVAL_MS;

            nimcp_platform_sleep_ms(POOL_WORKER_POLL_INTERVAL_MS);
        }

        /* Update utilization */
        uint64_t total_time = worker->stats.total_processing_ms + worker->stats.idle_time_ms;
        if (total_time > 0) {
            worker->stats.utilization =
                (float)worker->stats.total_processing_ms / total_time;
        }
    }

    worker->state = RCOG_WORKER_STOPPED;
    return NULL;  /* Normal worker thread exit */
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int rcog_delegation_pool_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    rcog_delegation_pool_heartbeat("rcog_delegat_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Recursive_Cognition_Delegation_Pool_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                rcog_delegation_pool_heartbeat("rcog_delegat_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Log self-knowledge observations */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Recursive_Cognition_Delegation_Pool_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Recursive_Cognition_Delegation_Pool_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}
