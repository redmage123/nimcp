/**
 * @file nimcp_async_integration_bridge.c
 * @brief Async Integration Bridge - Implementation
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Implementation of the async integration bridge
 * WHY:  Provide unified async coordination across NIMCP modules
 * HOW:  Task queues, promise/future tracking, phase synchronization
 *
 * @author NIMCP Development Team
 */

#include "async/integration/nimcp_async_integration_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(async_integration_bridge)

#define LOG_MODULE "ASYNC_INTEGRATION_BRIDGE"


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define LOG_TAG "async_integration"
#define TASK_ID_BASE 0x10000000
#define PROMISE_ID_BASE 0x20000000
#define FUTURE_ID_BASE 0x30000000
#define SYNC_GROUP_ID_BASE 0x40000000

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Task entry in registry
 */
typedef struct {
    uint64_t task_id;
    uint32_t priority;
    async_op_state_t state;
    void* data;
    size_t data_size;
    async_task_callback_t callback;
    async_completion_callback_t completion;
    void* user_data;
    uint64_t submit_time_ms;
    uint64_t start_time_ms;
    uint64_t timeout_ms;
    bool active;
} task_entry_t;

/**
 * @brief Promise entry in registry
 */
typedef struct {
    uint64_t promise_id;
    nimcp_bio_promise_t bio_promise;
    nimcp_bio_channel_type_t channel;
    uint64_t create_time_ms;
    bool active;
} promise_entry_t;

/**
 * @brief Future entry in registry
 */
typedef struct {
    uint64_t future_id;
    nimcp_bio_future_t bio_future;
    float last_confidence;
    uint64_t register_time_ms;
    bool active;
} future_entry_t;

/**
 * @brief Phase sync group
 */
typedef struct {
    uint32_t group_id;
    nimcp_oscillation_band_t band;
    nimcp_phase_sync_t phase_sync;
    uint64_t* future_ids;
    uint32_t future_count;
    uint32_t max_futures;
    bool active;
} sync_group_t;

/**
 * @brief Priority queue implementation
 */
typedef struct {
    uint64_t* task_ids;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t capacity;
} priority_queue_t;

/**
 * @brief Task registry
 */
typedef struct {
    task_entry_t* entries;
    uint32_t capacity;
    uint32_t count;
    uint64_t next_id;
} task_registry_t;

/**
 * @brief Promise registry
 */
typedef struct {
    promise_entry_t* entries;
    uint32_t capacity;
    uint32_t count;
    uint64_t next_id;
} promise_registry_t;

/**
 * @brief Future registry
 */
typedef struct {
    future_entry_t* entries;
    uint32_t capacity;
    uint32_t count;
    uint64_t next_id;
} future_registry_t;

/**
 * @brief Sync group registry
 */
typedef struct {
    sync_group_t* groups;
    uint32_t capacity;
    uint32_t count;
    uint32_t next_id;
} sync_group_registry_t;

/* ============================================================================
 * Internal Helper Functions - Declarations
 * ============================================================================ */

static uint64_t get_time_ms(void);
static task_registry_t* task_registry_create(uint32_t capacity);
static void task_registry_destroy(task_registry_t* reg);
static task_entry_t* task_registry_find(task_registry_t* reg, uint64_t task_id);
static promise_registry_t* promise_registry_create(uint32_t capacity);
static void promise_registry_destroy(promise_registry_t* reg);
static future_registry_t* future_registry_create(uint32_t capacity);
static void future_registry_destroy(future_registry_t* reg);
static sync_group_registry_t* sync_group_registry_create(uint32_t capacity);
static void sync_group_registry_destroy(sync_group_registry_t* reg);
static priority_queue_t* priority_queue_create(uint32_t capacity);
static void priority_queue_destroy(priority_queue_t* queue);
static int priority_queue_enqueue(priority_queue_t* queue, uint64_t task_id);
static uint64_t priority_queue_dequeue(priority_queue_t* queue);
static bool priority_queue_is_empty(const priority_queue_t* queue);
static nimcp_error_t handle_async_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/* ============================================================================
 * Time Helper
 * ============================================================================ */

static uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* ============================================================================
 * Registry Implementations
 * ============================================================================ */

static task_registry_t* task_registry_create(uint32_t capacity)
{
    task_registry_t* reg = (task_registry_t*)nimcp_malloc(sizeof(task_registry_t));
    if (!reg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reg is NULL");

        return NULL;

    }

    reg->entries = (task_entry_t*)nimcp_calloc(capacity, sizeof(task_entry_t));
    if (!reg->entries) {
        nimcp_free(reg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "task_registry_create: reg->entries is NULL");
        return NULL;
    }

    reg->capacity = capacity;
    reg->count = 0;
    reg->next_id = TASK_ID_BASE;
    return reg;
}

static void task_registry_destroy(task_registry_t* reg)
{
    if (!reg) return;

    /* Free any remaining task data */
    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (reg->entries[i].active && reg->entries[i].data) {
            nimcp_free(reg->entries[i].data);
        }
    }

    nimcp_free(reg->entries);
    nimcp_free(reg);
}

static task_entry_t* task_registry_find(task_registry_t* reg, uint64_t task_id)
{
    if (!reg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reg is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (reg->entries[i].active && reg->entries[i].task_id == task_id) {
            return &reg->entries[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "task_registry_find: validation failed");
    return NULL;
}

static promise_registry_t* promise_registry_create(uint32_t capacity)
{
    promise_registry_t* reg = (promise_registry_t*)nimcp_malloc(sizeof(promise_registry_t));
    if (!reg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reg is NULL");

        return NULL;

    }

    reg->entries = (promise_entry_t*)nimcp_calloc(capacity, sizeof(promise_entry_t));
    if (!reg->entries) {
        nimcp_free(reg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "promise_registry_create: reg->entries is NULL");
        return NULL;
    }

    reg->capacity = capacity;
    reg->count = 0;
    reg->next_id = PROMISE_ID_BASE;
    return reg;
}

static void promise_registry_destroy(promise_registry_t* reg)
{
    if (!reg) return;
    nimcp_free(reg->entries);
    nimcp_free(reg);
}

static future_registry_t* future_registry_create(uint32_t capacity)
{
    future_registry_t* reg = (future_registry_t*)nimcp_malloc(sizeof(future_registry_t));
    if (!reg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reg is NULL");

        return NULL;

    }

    reg->entries = (future_entry_t*)nimcp_calloc(capacity, sizeof(future_entry_t));
    if (!reg->entries) {
        nimcp_free(reg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "future_registry_create: reg->entries is NULL");
        return NULL;
    }

    reg->capacity = capacity;
    reg->count = 0;
    reg->next_id = FUTURE_ID_BASE;
    return reg;
}

static void future_registry_destroy(future_registry_t* reg)
{
    if (!reg) return;
    nimcp_free(reg->entries);
    nimcp_free(reg);
}

static sync_group_registry_t* sync_group_registry_create(uint32_t capacity)
{
    sync_group_registry_t* reg = (sync_group_registry_t*)nimcp_malloc(
        sizeof(sync_group_registry_t));
    if (!reg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reg is NULL");

        return NULL;

    }

    reg->groups = (sync_group_t*)nimcp_calloc(capacity, sizeof(sync_group_t));
    if (!reg->groups) {
        nimcp_free(reg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sync_group_registry_create: reg->groups is NULL");
        return NULL;
    }

    reg->capacity = capacity;
    reg->count = 0;
    reg->next_id = SYNC_GROUP_ID_BASE;
    return reg;
}

static void sync_group_registry_destroy(sync_group_registry_t* reg)
{
    if (!reg) return;

    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (reg->groups[i].active) {
            if (reg->groups[i].future_ids) {
                nimcp_free(reg->groups[i].future_ids);
            }
            if (reg->groups[i].phase_sync) {
                nimcp_phase_sync_destroy(reg->groups[i].phase_sync);
            }
        }
    }

    nimcp_free(reg->groups);
    nimcp_free(reg);
}

/* ============================================================================
 * Priority Queue Implementation
 * ============================================================================ */

static priority_queue_t* priority_queue_create(uint32_t capacity)
{
    priority_queue_t* queue = (priority_queue_t*)nimcp_malloc(sizeof(priority_queue_t));
    if (!queue) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue is NULL");

        return NULL;

    }

    queue->task_ids = (uint64_t*)nimcp_calloc(capacity, sizeof(uint64_t));
    if (!queue->task_ids) {
        nimcp_free(queue);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "priority_queue_create: queue->task_ids is NULL");
        return NULL;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->capacity = capacity;
    return queue;
}

static void priority_queue_destroy(priority_queue_t* queue)
{
    if (!queue) return;
    nimcp_free(queue->task_ids);
    nimcp_free(queue);
}

static int priority_queue_enqueue(priority_queue_t* queue, uint64_t task_id)
{
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "priority_queue_enqueue: queue is NULL");
        return -1;
    }
    if (queue->count >= queue->capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "priority_queue_enqueue: capacity exceeded");
        return -1;
    }

    queue->task_ids[queue->tail] = task_id;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    return 0;
}

static uint64_t priority_queue_dequeue(priority_queue_t* queue)
{
    if (!queue || queue->count == 0) return 0;

    uint64_t task_id = queue->task_ids[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return task_id;
}

static bool priority_queue_is_empty(const priority_queue_t* queue)
{
    return queue == NULL || queue->count == 0;
}

/* ============================================================================
 * Bio-Async Message Handler
 * ============================================================================ */

static nimcp_error_t handle_async_message(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg_size;
    (void)response_promise;

    async_integration_t* bridge = (async_integration_t*)user_data;
    if (!bridge || !msg) return NIMCP_SUCCESS;

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.messages_received++;

    /* Handle coordination messages */
    switch (header->type) {
        case ASYNC_MSG_COORD_STATUS_QUERY:
            /* Could send status response here */
            break;

        case ASYNC_MSG_QUEUE_STATUS:
            /* Process queue status from other modules */
            break;

        case ASYNC_MSG_PHASE_SYNC_REQUEST:
            /* Handle phase sync request */
            break;

        default:
            /* Unknown message type */
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int async_integration_default_config(async_integration_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Operation mode */
    config->mode = ASYNC_INTEGRATION_MODE_AUTOMATIC;
    config->dispatch_policy = ASYNC_DISPATCH_PRIORITY;

    /* Capacity limits */
    config->max_tasks = ASYNC_INTEGRATION_DEFAULT_MAX_TASKS;
    config->max_promises = ASYNC_INTEGRATION_DEFAULT_MAX_PROMISES;
    config->max_futures = ASYNC_INTEGRATION_DEFAULT_MAX_FUTURES;

    /* Queue configuration - per priority level */
    for (int i = 0; i < ASYNC_INTEGRATION_PRIORITY_LEVELS; i++) {
        config->priority_queues[i].capacity = ASYNC_INTEGRATION_DEFAULT_QUEUE_CAPACITY;
        config->priority_queues[i].weight = 1.0f / (i + 1); /* Higher priority = more weight */
        config->priority_queues[i].max_concurrent = 4;
    }

    /* Timing */
    config->update_interval_ms = ASYNC_INTEGRATION_DEFAULT_UPDATE_INTERVAL_MS;
    config->coordination_timeout_ms = ASYNC_INTEGRATION_DEFAULT_COORDINATION_TIMEOUT;
    config->task_default_timeout_ms = 30000; /* 30 seconds */

    /* Bio-async integration */
    config->enable_bio_async = true;
    config->default_channel = BIO_CHANNEL_DOPAMINE;
    config->default_band = BIO_OSC_GAMMA;

    /* Phase coupling */
    config->coherence_threshold = NIMCP_CONFIDENCE_HIGH;
    config->coupling_strength = NIMCP_MODULATION_DEFAULT;

    /* Features */
    config->enable_predictive_dispatch = true;
    config->enable_load_balancing = true;
    config->enable_backpressure = true;
    config->enable_statistics = true;
    config->enable_logging = false;

    return 0;
}

async_integration_t* async_integration_create(
    const async_integration_config_t* config)
{
    async_integration_config_t default_config;

    /* Use defaults if no config provided */
    if (!config) {
        async_integration_default_config(&default_config);
        config = &default_config;
    }

    /* Allocate bridge structure - nimcp_calloc zeroes all pointers */
    async_integration_t* bridge = (async_integration_t*)nimcp_calloc(
        1, sizeof(async_integration_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "async_integration_create: bridge allocation failed");
        return NULL;
    }

    /* Track how many priority queues were created for cleanup */
    int pq_created = 0;

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(async_integration_config_t));

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "async_integration") != 0) goto cleanup;
    if (!bridge->base.mutex) goto cleanup;

    /* Create task registry */
    bridge->task_registry = task_registry_create(config->max_tasks);
    if (!bridge->task_registry) goto cleanup;

    /* Create promise registry */
    bridge->promise_registry = promise_registry_create(config->max_promises);
    if (!bridge->promise_registry) goto cleanup;

    /* Create future registry */
    bridge->future_registry = future_registry_create(config->max_futures);
    if (!bridge->future_registry) goto cleanup;

    /* Create phase sync group registry */
    bridge->phase_sync_groups = sync_group_registry_create(32);
    if (!bridge->phase_sync_groups) goto cleanup;

    /* Create priority queues */
    for (int i = 0; i < ASYNC_INTEGRATION_PRIORITY_LEVELS; i++) {
        bridge->priority_queues[i] = priority_queue_create(
            config->priority_queues[i].capacity);
        if (!bridge->priority_queues[i]) goto cleanup;
        pq_created++;
    }

    bridge->initialized = true;
    bridge->running = false;
    bridge->bio_async_connected = false;

    return bridge;

cleanup:
    for (int j = pq_created - 1; j >= 0; j--) {
        priority_queue_destroy((priority_queue_t*)bridge->priority_queues[j]);
    }
    if (bridge->phase_sync_groups) {
        sync_group_registry_destroy((sync_group_registry_t*)bridge->phase_sync_groups);
    }
    if (bridge->future_registry) {
        future_registry_destroy((future_registry_t*)bridge->future_registry);
    }
    if (bridge->promise_registry) {
        promise_registry_destroy((promise_registry_t*)bridge->promise_registry);
    }
    if (bridge->task_registry) {
        task_registry_destroy((task_registry_t*)bridge->task_registry);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "async_integration_create: resource allocation failed");
    return NULL;
}

void async_integration_destroy(async_integration_t* bridge)
{
    if (!bridge) return;

    /* Stop if running */
    if (bridge->running) {
        async_integration_stop(bridge);
    }

    /* Disconnect bio-async if connected */
    if (bridge->bio_async_connected) {
        async_integration_disconnect_bio_async(bridge);
    }

    /* Destroy priority queues */
    for (int i = 0; i < ASYNC_INTEGRATION_PRIORITY_LEVELS; i++) {
        if (bridge->priority_queues[i]) {
            priority_queue_destroy((priority_queue_t*)bridge->priority_queues[i]);
        }
    }

    /* Destroy registries */
    if (bridge->phase_sync_groups) {
        sync_group_registry_destroy((sync_group_registry_t*)bridge->phase_sync_groups);
    }
    if (bridge->future_registry) {
        future_registry_destroy((future_registry_t*)bridge->future_registry);
    }
    if (bridge->promise_registry) {
        promise_registry_destroy((promise_registry_t*)bridge->promise_registry);
    }
    if (bridge->task_registry) {
        task_registry_destroy((task_registry_t*)bridge->task_registry);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int async_integration_start(async_integration_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_start: bridge is NULL");
        return -1;
    }
    if (!bridge->initialized) return ASYNC_INTEGRATION_ERROR_NOT_INITIALIZED;
    if (bridge->running) return ASYNC_INTEGRATION_ERROR_ALREADY_RUNNING;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->start_time_ms = get_time_ms();
    bridge->last_update_ms = bridge->start_time_ms;
    bridge->running = true;

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async && !bridge->bio_async_connected) {
        int result = async_integration_connect_bio_async(bridge);
        if (result < 0) {
            NIMCP_LOG_WARN(LOG_TAG, "Failed to connect bio-async: %d", result);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Async integration bridge started");
    }

    return 0;
}

int async_integration_stop(async_integration_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_stop: bridge is NULL");
        return -1;
    }
    if (!bridge->running) return 0;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->running = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Async integration bridge stopped");
    }

    return 0;
}

/* ============================================================================
 * Task Coordination API Implementation
 * ============================================================================ */

uint64_t async_integration_submit_task(
    async_integration_t* bridge,
    uint32_t priority,
    const void* task_data,
    size_t task_size,
    async_task_callback_t callback,
    async_completion_callback_t completion,
    void* user_data,
    uint64_t timeout_ms)
{
    if (!bridge || !callback) return 0;
    if (priority >= ASYNC_INTEGRATION_PRIORITY_LEVELS) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    task_registry_t* reg = (task_registry_t*)bridge->task_registry;
    if (reg->count >= reg->capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Find free slot */
    task_entry_t* entry = NULL;
    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (!reg->entries[i].active) {
            entry = &reg->entries[i];
            break;
        }
    }

    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Allocate and copy task data */
    void* data_copy = NULL;
    if (task_data && task_size > 0) {
        data_copy = nimcp_malloc(task_size);
        if (!data_copy) {
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
        memcpy(data_copy, task_data, task_size);
    }

    /* Initialize entry */
    entry->task_id = reg->next_id++;
    entry->priority = priority;
    entry->state = ASYNC_OP_STATE_PENDING;
    entry->data = data_copy;
    entry->data_size = task_size;
    entry->callback = callback;
    entry->completion = completion;
    entry->user_data = user_data;
    entry->submit_time_ms = get_time_ms();
    entry->start_time_ms = 0;
    entry->timeout_ms = timeout_ms > 0 ? timeout_ms : bridge->config.task_default_timeout_ms;
    entry->active = true;

    reg->count++;

    /* Enqueue task */
    priority_queue_t* queue = (priority_queue_t*)bridge->priority_queues[priority];
    if (priority_queue_enqueue(queue, entry->task_id) < 0) {
        /* Queue full - cleanup */
        if (data_copy) nimcp_free(data_copy);
        entry->active = false;
        reg->count--;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    bridge->stats.tasks_submitted++;

    uint64_t task_id = entry->task_id;
    nimcp_mutex_unlock(bridge->base.mutex);

    return task_id;
}

int async_integration_cancel_task(
    async_integration_t* bridge,
    uint64_t task_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_cancel_task: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    task_registry_t* reg = (task_registry_t*)bridge->task_registry;
    task_entry_t* entry = task_registry_find(reg, task_id);

    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_cancel_task: entry is NULL");
        return -1;
    }

    /* Can only cancel pending tasks */
    if (entry->state != ASYNC_OP_STATE_PENDING) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_integration_cancel_task: validation failed");
        return -1;
    }

    entry->state = ASYNC_OP_STATE_CANCELLED;

    /* Invoke completion callback with cancellation */
    if (entry->completion) {
        entry->completion(task_id, NULL, 0, -1, entry->user_data);
    }

    /* Free task data */
    if (entry->data) {
        nimcp_free(entry->data);
        entry->data = NULL;
    }

    entry->active = false;
    reg->count--;
    bridge->stats.tasks_cancelled++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

async_op_state_t async_integration_get_task_state(
    const async_integration_t* bridge,
    uint64_t task_id)
{
    if (!bridge) return ASYNC_OP_STATE_IDLE;

    nimcp_mutex_lock(((async_integration_t*)bridge)->mutex);

    task_registry_t* reg = (task_registry_t*)bridge->task_registry;
    task_entry_t* entry = task_registry_find(reg, task_id);

    async_op_state_t state = entry ? entry->state : ASYNC_OP_STATE_IDLE;

    nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);
    return state;
}

int async_integration_update_task_priority(
    async_integration_t* bridge,
    uint64_t task_id,
    uint32_t new_priority)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_update_task_priority: bridge is NULL");
        return -1;
    }
    if (new_priority >= ASYNC_INTEGRATION_PRIORITY_LEVELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_integration_update_task_priority: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    task_registry_t* reg = (task_registry_t*)bridge->task_registry;
    task_entry_t* entry = task_registry_find(reg, task_id);

    if (!entry || entry->state != ASYNC_OP_STATE_PENDING) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_integration_update_task_priority: entry is NULL");
        return -1;
    }

    entry->priority = new_priority;
    /* Note: Task remains in original queue - priority used at dispatch time */

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Promise/Future Management API Implementation
 * ============================================================================ */

uint64_t async_integration_register_promise(
    async_integration_t* bridge,
    nimcp_bio_promise_t promise,
    nimcp_bio_channel_type_t channel)
{
    if (!bridge || !promise) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    promise_registry_t* reg = (promise_registry_t*)bridge->promise_registry;
    if (reg->count >= reg->capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Find free slot */
    promise_entry_t* entry = NULL;
    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (!reg->entries[i].active) {
            entry = &reg->entries[i];
            break;
        }
    }

    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    entry->promise_id = reg->next_id++;
    entry->bio_promise = promise;
    entry->channel = channel;
    entry->create_time_ms = get_time_ms();
    entry->active = true;

    reg->count++;
    bridge->stats.promises_created++;

    uint64_t promise_id = entry->promise_id;
    nimcp_mutex_unlock(bridge->base.mutex);

    return promise_id;
}

uint64_t async_integration_register_future(
    async_integration_t* bridge,
    nimcp_bio_future_t future)
{
    if (!bridge || !future) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    future_registry_t* reg = (future_registry_t*)bridge->future_registry;
    if (reg->count >= reg->capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Find free slot */
    future_entry_t* entry = NULL;
    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (!reg->entries[i].active) {
            entry = &reg->entries[i];
            break;
        }
    }

    if (!entry) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    entry->future_id = reg->next_id++;
    entry->bio_future = future;
    entry->last_confidence = nimcp_bio_future_get_confidence(future);
    entry->register_time_ms = get_time_ms();
    entry->active = true;

    reg->count++;

    uint64_t future_id = entry->future_id;
    nimcp_mutex_unlock(bridge->base.mutex);

    return future_id;
}

uint32_t async_integration_get_active_promises(
    const async_integration_t* bridge)
{
    if (!bridge) return 0;

    nimcp_mutex_lock(((async_integration_t*)bridge)->mutex);
    promise_registry_t* reg = (promise_registry_t*)bridge->promise_registry;
    uint32_t count = reg ? reg->count : 0;
    nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);

    return count;
}

uint32_t async_integration_get_active_futures(
    const async_integration_t* bridge)
{
    if (!bridge) return 0;

    nimcp_mutex_lock(((async_integration_t*)bridge)->mutex);
    future_registry_t* reg = (future_registry_t*)bridge->future_registry;
    uint32_t count = reg ? reg->count : 0;
    nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);

    return count;
}

float async_integration_get_avg_confidence(
    const async_integration_t* bridge)
{
    if (!bridge) return 0.0f;

    nimcp_mutex_lock(((async_integration_t*)bridge)->mutex);

    future_registry_t* reg = (future_registry_t*)bridge->future_registry;
    if (!reg || reg->count == 0) {
        nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);
        return 0.0f;
    }

    float sum = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (reg->entries[i].active && reg->entries[i].bio_future) {
            float conf = nimcp_bio_future_get_confidence(reg->entries[i].bio_future);
            sum += conf;
            active_count++;
        }
    }

    float avg = active_count > 0 ? sum / active_count : 0.0f;
    nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);

    return avg;
}

/* ============================================================================
 * Phase Synchronization API Implementation
 * ============================================================================ */

uint32_t async_integration_create_sync_group(
    async_integration_t* bridge,
    nimcp_oscillation_band_t band)
{
    if (!bridge) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    sync_group_registry_t* reg = (sync_group_registry_t*)bridge->phase_sync_groups;
    if (reg->count >= reg->capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Find free slot */
    sync_group_t* group = NULL;
    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (!reg->groups[i].active) {
            group = &reg->groups[i];
            break;
        }
    }

    if (!group) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Create phase sync */
    nimcp_phase_sync_t phase_sync = nimcp_phase_sync_create(band);
    if (!phase_sync) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Allocate future ID array */
    uint32_t max_futures = 16;
    group->future_ids = (uint64_t*)nimcp_calloc(max_futures, sizeof(uint64_t));
    if (!group->future_ids) {
        nimcp_phase_sync_destroy(phase_sync);
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    group->group_id = reg->next_id++;
    group->band = band;
    group->phase_sync = phase_sync;
    group->future_count = 0;
    group->max_futures = max_futures;
    group->active = true;

    reg->count++;

    uint32_t group_id = group->group_id;
    nimcp_mutex_unlock(bridge->base.mutex);

    return group_id;
}

int async_integration_add_to_sync_group(
    async_integration_t* bridge,
    uint32_t sync_group_id,
    uint64_t future_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_add_to_sync_group: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sync_group_registry_t* sync_reg = (sync_group_registry_t*)bridge->phase_sync_groups;
    future_registry_t* future_reg = (future_registry_t*)bridge->future_registry;

    /* Find sync group */
    sync_group_t* group = NULL;
    for (uint32_t i = 0; i < sync_reg->capacity; i++) {
        if (sync_reg->groups[i].active && sync_reg->groups[i].group_id == sync_group_id) {
            group = &sync_reg->groups[i];
            break;
        }
    }

    if (!group) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_add_to_sync_group: group is NULL");
        return -1;
    }

    /* Find future */
    future_entry_t* future_entry = NULL;
    for (uint32_t i = 0; i < future_reg->capacity; i++) {
        if (future_reg->entries[i].active && future_reg->entries[i].future_id == future_id) {
            future_entry = &future_reg->entries[i];
            break;
        }
    }

    if (!future_entry || !future_entry->bio_future) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_add_to_sync_group: required parameter is NULL (future_entry, future_entry->bio_future)");
        return -1;
    }

    /* Check capacity */
    if (group->future_count >= group->max_futures) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "async_integration_add_to_sync_group: capacity exceeded");
        return -1;
    }

    /* Add to phase sync */
    nimcp_error_t err = nimcp_phase_sync_add_future(
        group->phase_sync, future_entry->bio_future);
    if (err != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_integration_add_to_sync_group: validation failed");
        return -1;
    }

    /* Track future ID */
    group->future_ids[group->future_count++] = future_id;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int async_integration_wait_coherence(
    async_integration_t* bridge,
    uint32_t sync_group_id,
    float coherence_threshold,
    uint64_t timeout_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_wait_coherence: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sync_group_registry_t* reg = (sync_group_registry_t*)bridge->phase_sync_groups;

    /* Find sync group */
    sync_group_t* group = NULL;
    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (reg->groups[i].active && reg->groups[i].group_id == sync_group_id) {
            group = &reg->groups[i];
            break;
        }
    }

    if (!group || !group->phase_sync) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_wait_coherence: required parameter is NULL (group, group->phase_sync)");
        return -1;
    }

    nimcp_phase_sync_t phase_sync = group->phase_sync;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Use default timeout if not specified */
    if (timeout_ms == 0) {
        timeout_ms = bridge->config.coordination_timeout_ms;
    }

    /* Wait for coherence */
    bridge->stats.phase_sync_requests++;
    nimcp_error_t err = nimcp_phase_sync_wait_coherent(
        phase_sync, coherence_threshold, timeout_ms);

    if (err == NIMCP_SUCCESS) {
        bridge->stats.phase_sync_achieved++;
        return 0;
    } else {
        bridge->stats.phase_sync_timeouts++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "async_integration_wait_coherence: validation failed");
        return -1;
    }
}

float async_integration_get_coherence(
    const async_integration_t* bridge,
    uint32_t sync_group_id)
{
    if (!bridge) return -1.0f;

    nimcp_mutex_lock(((async_integration_t*)bridge)->mutex);

    sync_group_registry_t* reg = (sync_group_registry_t*)bridge->phase_sync_groups;

    /* Find sync group */
    sync_group_t* group = NULL;
    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (reg->groups[i].active && reg->groups[i].group_id == sync_group_id) {
            group = &reg->groups[i];
            break;
        }
    }

    float coherence = -1.0f;
    if (group && group->phase_sync) {
        coherence = nimcp_phase_sync_get_coherence(group->phase_sync);
    }

    nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);
    return coherence;
}

int async_integration_destroy_sync_group(
    async_integration_t* bridge,
    uint32_t sync_group_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_destroy_sync_group: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    sync_group_registry_t* reg = (sync_group_registry_t*)bridge->phase_sync_groups;

    /* Find sync group */
    sync_group_t* group = NULL;
    for (uint32_t i = 0; i < reg->capacity; i++) {
        if (reg->groups[i].active && reg->groups[i].group_id == sync_group_id) {
            group = &reg->groups[i];
            break;
        }
    }

    if (!group) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_destroy_sync_group: group is NULL");
        return -1;
    }

    /* Cleanup */
    if (group->phase_sync) {
        nimcp_phase_sync_destroy(group->phase_sync);
    }
    if (group->future_ids) {
        nimcp_free(group->future_ids);
    }

    memset(group, 0, sizeof(sync_group_t));
    reg->count--;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int async_integration_connect_bio_async(async_integration_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->bio_async_connected) return 0;
    if (!bio_router_is_initialized()) return ASYNC_INTEGRATION_ERROR_NOT_CONNECTED;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_ASYNC_INTEGRATION,
        .module_name = ASYNC_INTEGRATION_MODULE_NAME,
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->bio_context = bio_router_register_module(&info);
    if (!bridge->bio_context) {
        return ASYNC_INTEGRATION_ERROR_NOT_CONNECTED;
    }

    /* Register handlers for coordination messages */
    bio_router_register_category_handler(
        bridge->bio_context,
        0x0700,  /* ASYNC_MSG base */
        handle_async_message
    );

    bridge->bio_async_connected = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Connected to bio-async router");
    }

    return 0;
}

int async_integration_disconnect_bio_async(async_integration_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->bio_async_connected) return 0;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;

    if (bridge->config.enable_logging) {
        NIMCP_LOG_INFO(LOG_TAG, "Disconnected from bio-async router");
    }

    return 0;
}

bool async_integration_is_bio_async_connected(
    const async_integration_t* bridge)
{
    return bridge && bridge->bio_async_connected;
}

int async_integration_send_message(
    async_integration_t* bridge,
    async_coordination_msg_type_t msg_type,
    bio_module_id_t target,
    const void* data,
    size_t size)
{
    if (!bridge || !bridge->bio_async_connected) {
        return ASYNC_INTEGRATION_ERROR_NOT_CONNECTED;
    }

    /* Allocate message buffer */
    size_t msg_size = sizeof(bio_message_header_t) + size;
    void* msg = nimcp_malloc(msg_size);
    if (!msg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "async_integration_send_message: msg is NULL");
        return -1;
    }

    /* Fill header */
    bio_message_header_t* header = (bio_message_header_t*)msg;
    header->type = msg_type;
    header->source_module = BIO_MODULE_ASYNC_INTEGRATION;
    header->target_module = target;
    header->payload_size = size;
    header->timestamp_us = get_time_ms() * 1000;  /* Convert ms to us */

    /* Copy data */
    if (data && size > 0) {
        memcpy((uint8_t*)msg + sizeof(bio_message_header_t), data, size);
    }

    /* Send via router */
    nimcp_error_t err = bio_router_send(bridge->bio_context, msg, msg_size, 0);

    nimcp_free(msg);

    if (err == NIMCP_SUCCESS) {
        bridge->stats.messages_sent++;
        return 0;
    } else {
        bridge->stats.dispatch_errors++;
        return ASYNC_INTEGRATION_ERROR_DISPATCH_FAILED;
    }
}

/* ============================================================================
 * Update and Processing API Implementation
 * ============================================================================ */

int async_integration_update(
    async_integration_t* bridge,
    float dt_ms)
{
    if (!bridge || !bridge->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_update: required parameter is NULL (bridge, bridge->running)");
        return -1;
    }
    (void)dt_ms;

    uint64_t start_time = get_time_ms();
    int operations = 0;

    nimcp_mutex_lock(bridge->base.mutex);

    task_registry_t* task_reg = (task_registry_t*)bridge->task_registry;

    /* Dispatch tasks from priority queues */
    if (bridge->config.mode == ASYNC_INTEGRATION_MODE_AUTOMATIC ||
        bridge->config.mode == ASYNC_INTEGRATION_MODE_COORDINATED) {

        /* Process queues in priority order */
        for (int p = 0; p < ASYNC_INTEGRATION_PRIORITY_LEVELS; p++) {
            priority_queue_t* queue = (priority_queue_t*)bridge->priority_queues[p];

            while (!priority_queue_is_empty(queue)) {
                uint64_t task_id = priority_queue_dequeue(queue);
                task_entry_t* entry = task_registry_find(task_reg, task_id);

                if (!entry || entry->state != ASYNC_OP_STATE_PENDING) continue;

                /* Execute task */
                entry->state = ASYNC_OP_STATE_RUNNING;
                entry->start_time_ms = get_time_ms();
                bridge->active_task_count++;

                nimcp_mutex_unlock(bridge->base.mutex);

                int result = entry->callback(
                    task_id, entry->data, entry->data_size, entry->user_data);

                nimcp_mutex_lock(bridge->base.mutex);

                /* Update state based on result */
                if (result == 0) {
                    entry->state = ASYNC_OP_STATE_COMPLETED;
                    bridge->stats.tasks_completed++;
                } else {
                    entry->state = ASYNC_OP_STATE_FAILED;
                    bridge->stats.tasks_failed++;
                }

                bridge->active_task_count--;

                /* Calculate latency */
                uint64_t latency = get_time_ms() - entry->submit_time_ms;
                bridge->stats.avg_task_latency_ms =
                    (bridge->stats.avg_task_latency_ms * bridge->stats.tasks_completed +
                     latency) / (bridge->stats.tasks_completed + 1);

                /* Invoke completion callback */
                if (entry->completion) {
                    entry->completion(task_id, NULL, 0, result, entry->user_data);
                }

                /* Free task data */
                if (entry->data) {
                    nimcp_free(entry->data);
                    entry->data = NULL;
                }

                entry->active = false;
                task_reg->count--;
                operations++;

                /* Limit tasks per update cycle */
                if (operations >= 8) break;
            }

            if (operations >= 8) break;
        }
    }

    /* Update future confidence tracking */
    future_registry_t* future_reg = (future_registry_t*)bridge->future_registry;
    float confidence_sum = 0.0f;
    uint32_t confidence_count = 0;

    for (uint32_t i = 0; i < future_reg->capacity; i++) {
        if (future_reg->entries[i].active && future_reg->entries[i].bio_future) {
            float conf = nimcp_bio_future_get_confidence(future_reg->entries[i].bio_future);
            future_reg->entries[i].last_confidence = conf;
            confidence_sum += conf;
            confidence_count++;

            /* Check for decayed futures */
            if (nimcp_bio_future_state(future_reg->entries[i].bio_future) ==
                BIO_FUTURE_DECAYED) {
                bridge->stats.futures_timeout++;
            }
        }
    }

    if (confidence_count > 0) {
        bridge->stats.avg_future_confidence = confidence_sum / confidence_count;
    }

    bridge->last_update_ms = get_time_ms();
    bridge->stats.total_updates++;

    /* Track update timing */
    float update_time_us = (float)(get_time_ms() - start_time) * 1000.0f;
    bridge->stats.avg_update_time_us =
        (bridge->stats.avg_update_time_us * (bridge->stats.total_updates - 1) +
         update_time_us) / bridge->stats.total_updates;

    if (update_time_us > bridge->stats.max_update_time_us) {
        bridge->stats.max_update_time_us = update_time_us;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return operations;
}

uint32_t async_integration_process_messages(
    async_integration_t* bridge,
    uint32_t max_messages)
{
    if (!bridge || !bridge->bio_async_connected) return 0;
    return bio_router_process_inbox(bridge->bio_context, max_messages);
}

/* ============================================================================
 * Callback Registration API Implementation
 * ============================================================================ */

int async_integration_set_event_callback(
    async_integration_t* bridge,
    async_coord_event_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_set_event_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->event_callback = callback;
    bridge->event_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * State and Statistics API Implementation
 * ============================================================================ */

int async_integration_get_state(
    const async_integration_t* bridge,
    async_integration_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(((async_integration_t*)bridge)->mutex);

    memset(state, 0, sizeof(*state));

    state->mode = bridge->config.mode;
    state->running = bridge->running;
    state->bio_async_connected = bridge->bio_async_connected;

    /* Queue states */
    state->total_pending_tasks = 0;
    for (int i = 0; i < ASYNC_INTEGRATION_PRIORITY_LEVELS; i++) {
        priority_queue_t* queue = (priority_queue_t*)bridge->priority_queues[i];
        if (queue) {
            state->queues[i].pending_count = queue->count;
            state->total_pending_tasks += queue->count;
        }
    }

    state->total_running_tasks = bridge->active_task_count;

    /* Promise/Future state */
    promise_registry_t* promise_reg = (promise_registry_t*)bridge->promise_registry;
    future_registry_t* future_reg = (future_registry_t*)bridge->future_registry;

    state->promises.active_promises = promise_reg ? promise_reg->count : 0;
    state->promises.active_futures = future_reg ? future_reg->count : 0;
    state->promises.avg_confidence = bridge->stats.avg_future_confidence;

    /* Phase state */
    sync_group_registry_t* sync_reg = (sync_group_registry_t*)bridge->phase_sync_groups;
    state->phase.active_sync_groups = sync_reg ? sync_reg->count : 0;
    state->phase.sync_achieved_count = bridge->stats.phase_sync_achieved;
    state->phase.sync_timeout_count = bridge->stats.phase_sync_timeouts;

    /* Timing */
    state->uptime_ms = bridge->start_time_ms > 0 ?
        get_time_ms() - bridge->start_time_ms : 0;
    state->last_update_ms = bridge->last_update_ms;
    state->tasks_dispatched = bridge->stats.tasks_submitted;

    nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);

    return 0;
}

int async_integration_get_stats(
    const async_integration_t* bridge,
    async_integration_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_integration_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(((async_integration_t*)bridge)->mutex);
    memcpy(stats, &bridge->stats, sizeof(async_integration_stats_t));
    nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);

    return 0;
}

void async_integration_reset_stats(async_integration_t* bridge)
{
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(async_integration_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

uint32_t async_integration_get_queue_depth(
    const async_integration_t* bridge,
    uint32_t priority)
{
    if (!bridge || priority >= ASYNC_INTEGRATION_PRIORITY_LEVELS) return 0;

    nimcp_mutex_lock(((async_integration_t*)bridge)->mutex);
    priority_queue_t* queue = (priority_queue_t*)bridge->priority_queues[priority];
    uint32_t depth = queue ? queue->count : 0;
    nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);

    return depth;
}

uint32_t async_integration_get_total_pending(
    const async_integration_t* bridge)
{
    if (!bridge) return 0;

    nimcp_mutex_lock(((async_integration_t*)bridge)->mutex);

    uint32_t total = 0;
    for (int i = 0; i < ASYNC_INTEGRATION_PRIORITY_LEVELS; i++) {
        priority_queue_t* queue = (priority_queue_t*)bridge->priority_queues[i];
        if (queue) {
            total += queue->count;
        }
    }

    nimcp_mutex_unlock(((async_integration_t*)bridge)->mutex);

    return total;
}

/* ============================================================================
 * Utility API Implementation
 * ============================================================================ */

const char* async_op_state_to_string(async_op_state_t state)
{
    switch (state) {
        case ASYNC_OP_STATE_IDLE:       return "idle";
        case ASYNC_OP_STATE_PENDING:    return "pending";
        case ASYNC_OP_STATE_RUNNING:    return "running";
        case ASYNC_OP_STATE_COMPLETED:  return "completed";
        case ASYNC_OP_STATE_FAILED:     return "failed";
        case ASYNC_OP_STATE_CANCELLED:  return "cancelled";
        case ASYNC_OP_STATE_TIMEOUT:    return "timeout";
        default:                        return "unknown";
    }
}

const char* async_integration_mode_to_string(async_integration_mode_t mode)
{
    switch (mode) {
        case ASYNC_INTEGRATION_MODE_DISABLED:       return "disabled";
        case ASYNC_INTEGRATION_MODE_MONITOR:        return "monitor";
        case ASYNC_INTEGRATION_MODE_MANUAL:         return "manual";
        case ASYNC_INTEGRATION_MODE_AUTOMATIC:      return "automatic";
        case ASYNC_INTEGRATION_MODE_COORDINATED:    return "coordinated";
        default:                                    return "unknown";
    }
}

const char* async_dispatch_policy_to_string(async_dispatch_policy_t policy)
{
    switch (policy) {
        case ASYNC_DISPATCH_FIFO:       return "fifo";
        case ASYNC_DISPATCH_PRIORITY:   return "priority";
        case ASYNC_DISPATCH_FAIR:       return "fair";
        case ASYNC_DISPATCH_WEIGHTED:   return "weighted";
        default:                        return "unknown";
    }
}

const char* async_coord_msg_to_string(async_coordination_msg_type_t msg_type)
{
    switch (msg_type) {
        /* Task messages */
        case ASYNC_MSG_TASK_SUBMIT:             return "task_submit";
        case ASYNC_MSG_TASK_COMPLETE:           return "task_complete";
        case ASYNC_MSG_TASK_FAILED:             return "task_failed";
        case ASYNC_MSG_TASK_CANCELLED:          return "task_cancelled";
        case ASYNC_MSG_TASK_PROGRESS:           return "task_progress";
        case ASYNC_MSG_TASK_PRIORITY_CHANGE:    return "task_priority_change";

        /* Promise/Future messages */
        case ASYNC_MSG_PROMISE_CREATED:         return "promise_created";
        case ASYNC_MSG_PROMISE_RESOLVED:        return "promise_resolved";
        case ASYNC_MSG_PROMISE_REJECTED:        return "promise_rejected";
        case ASYNC_MSG_FUTURE_WAIT_START:       return "future_wait_start";
        case ASYNC_MSG_FUTURE_WAIT_COMPLETE:    return "future_wait_complete";
        case ASYNC_MSG_FUTURE_CONFIDENCE_UPDATE: return "future_confidence_update";

        /* Queue messages */
        case ASYNC_MSG_QUEUE_STATUS:            return "queue_status";
        case ASYNC_MSG_QUEUE_BACKPRESSURE:      return "queue_backpressure";
        case ASYNC_MSG_QUEUE_DRAINED:           return "queue_drained";
        case ASYNC_MSG_LOAD_BALANCE_REQUEST:    return "load_balance_request";
        case ASYNC_MSG_LOAD_BALANCE_RESPONSE:   return "load_balance_response";

        /* Phase sync messages */
        case ASYNC_MSG_PHASE_SYNC_REQUEST:      return "phase_sync_request";
        case ASYNC_MSG_PHASE_SYNC_ACHIEVED:     return "phase_sync_achieved";
        case ASYNC_MSG_PHASE_SYNC_TIMEOUT:      return "phase_sync_timeout";
        case ASYNC_MSG_COHERENCE_UPDATE:        return "coherence_update";

        /* Coordination control */
        case ASYNC_MSG_COORD_START:             return "coord_start";
        case ASYNC_MSG_COORD_STOP:              return "coord_stop";
        case ASYNC_MSG_COORD_PAUSE:             return "coord_pause";
        case ASYNC_MSG_COORD_RESUME:            return "coord_resume";
        case ASYNC_MSG_COORD_STATUS_QUERY:      return "coord_status_query";
        case ASYNC_MSG_COORD_STATUS_RESPONSE:   return "coord_status_response";

        default:                                return "unknown";
    }
}
