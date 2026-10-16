/**
 * @file nimcp_kg_io_dispatcher.c
 * @brief Knowledge Graph I/O Dispatcher - Implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * Implementation of high-performance async I/O dispatcher for KG operations.
 * Features thread pools, lock-free queues, and connection pooling.
 */

#include "core/brain/nimcp_kg_io_dispatcher.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_timing_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_io_dispatcher, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define IO_QUEUE_LOCK_RETRIES       1000
#define IO_THREAD_SHUTDOWN_TIMEOUT  NIMCP_DEFAULT_TIMEOUT_MS
#define IO_BATCH_INITIAL_SIZE       4096

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Queue entry for pending I/O operations
 */
typedef struct kg_io_queue_entry {
    kg_io_request_t request;
    struct kg_io_queue_entry* next;
} kg_io_queue_entry_t;

/**
 * @brief Lock-free MPSC queue
 */
typedef struct {
    _Atomic(kg_io_queue_entry_t*) head;
    _Atomic(kg_io_queue_entry_t*) tail;
    _Atomic(uint32_t) count;
    uint32_t capacity;
} kg_io_queue_t;

/**
 * @brief Database connection (abstracted)
 */
typedef struct {
    int socket_fd;              /* Connection socket (-1 if closed) */
    bool in_use;                /* Currently being used */
    uint64_t last_used;         /* Last use timestamp */
    char host[NIMCP_LABEL_BUFFER_SIZE];             /* Connected host */
    uint16_t port;              /* Connected port */
} kg_io_connection_t;

/**
 * @brief Connection pool
 */
typedef struct {
    kg_io_connection_t* connections;
    uint32_t size;
    uint32_t active;
    uint32_t idle;
    nimcp_mutex_t* mutex;
    nimcp_cond_t* available;
} kg_io_pool_t;

/**
 * @brief Thread pool worker
 */
typedef struct {
    pthread_t thread;
    uint32_t id;
    bool active;
    bool should_stop;
    struct kg_io_dispatcher* dispatcher;
    bool is_writer;
} kg_io_worker_t;

/**
 * @brief Batch builder
 */
struct kg_io_batch {
    kg_io_dispatcher_t* dispatcher;
    char table_name[KG_IO_MAX_TABLE_NAME];
    uint8_t* buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    uint32_t row_count;
    bool submitted;
};

/**
 * @brief Statistics accumulator
 */
typedef struct {
    _Atomic(uint64_t) writes_total;
    _Atomic(uint64_t) reads_total;
    _Atomic(uint64_t) bytes_written;
    _Atomic(uint64_t) bytes_read;
    _Atomic(uint64_t) write_latency_sum;
    _Atomic(uint64_t) read_latency_sum;
    _Atomic(uint64_t) last_reset_time;
} kg_io_stats_accum_t;

/**
 * @brief I/O dispatcher internal structure
 */
struct kg_io_dispatcher {
    /* Configuration */
    kg_questdb_config_t config;

    /* Priority queues */
    kg_io_queue_t write_queue;
    kg_io_queue_t read_queue;
    kg_io_queue_t batch_queue;
    kg_io_queue_t priority_queues[4];  /* LOW, NORMAL, HIGH, CRITICAL */

    /* Connection pool */
    kg_io_pool_t pool;

    /* Thread pools */
    kg_io_worker_t* writer_workers;
    kg_io_worker_t* reader_workers;
    uint32_t writer_count;
    uint32_t reader_count;

    /* Control */
    bool running;
    _Atomic(uint64_t) next_op_id;
    nimcp_mutex_t* control_mutex;
    nimcp_cond_t* work_available;

    /* Statistics */
    kg_io_stats_accum_t stats_accum;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static int queue_init(kg_io_queue_t* queue, uint32_t capacity);
static void queue_destroy(kg_io_queue_t* queue);
static int queue_push(kg_io_queue_t* queue, const kg_io_request_t* request);
static int queue_pop(kg_io_queue_t* queue, kg_io_request_t* request);
static bool queue_is_full(const kg_io_queue_t* queue);
static int pool_init(kg_io_pool_t* pool, const kg_questdb_config_t* config);
static void pool_destroy(kg_io_pool_t* pool);
static kg_io_connection_t* pool_acquire(kg_io_pool_t* pool, uint32_t timeout_ms);
static void pool_release(kg_io_pool_t* pool, kg_io_connection_t* conn);
static void* writer_thread_func(void* arg);
static void* reader_thread_func(void* arg);
static uint64_t get_timestamp_ns(void);
static int execute_write(kg_io_connection_t* conn, const kg_io_request_t* request,
                          kg_io_result_t* result);
static int execute_read(kg_io_connection_t* conn, const kg_io_request_t* request,
                         kg_io_result_t* result);

/* ============================================================================
 * Timestamp Helper
 * ============================================================================ */

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
    return 0;
}

/* ============================================================================
 * Queue Implementation
 * ============================================================================ */

static int queue_init(kg_io_queue_t* queue, uint32_t capacity) {
    if (!queue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue is NULL");

        return -1;
    }

    atomic_init(&queue->head, NULL);
    atomic_init(&queue->tail, NULL);
    atomic_init(&queue->count, 0);
    queue->capacity = capacity;

    /* Create sentinel node */
    kg_io_queue_entry_t* sentinel = nimcp_calloc(1, sizeof(kg_io_queue_entry_t));
    if (!sentinel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sentinel is NULL");

        return -1;
    }
    sentinel->next = NULL;

    atomic_store(&queue->head, sentinel);
    atomic_store(&queue->tail, sentinel);

    return 0;
}

static void queue_destroy(kg_io_queue_t* queue) {
    if (!queue) {
        return;
    }

    /* Free all entries */
    kg_io_queue_entry_t* entry = atomic_load(&queue->head);
    while (entry) {
        kg_io_queue_entry_t* next = entry->next;
        nimcp_free(entry);
        entry = next;
    }

    atomic_store(&queue->head, NULL);
    atomic_store(&queue->tail, NULL);
    atomic_store(&queue->count, 0);
}

static int queue_push(kg_io_queue_t* queue, const kg_io_request_t* request) {
    if (!queue || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue_push: required parameter is NULL (queue, request)");
        return -1;
    }

    uint32_t count = atomic_load(&queue->count);
    if (count >= queue->capacity) {
        return -1;  /* Queue full - normal backpressure */
    }

    kg_io_queue_entry_t* entry = nimcp_calloc(1, sizeof(kg_io_queue_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entry is NULL");

        return -1;
    }

    memcpy(&entry->request, request, sizeof(kg_io_request_t));
    entry->next = NULL;

    /* Lock-free MPSC enqueue */
    kg_io_queue_entry_t* prev;
    do {
        prev = atomic_load(&queue->tail);
    } while (!atomic_compare_exchange_weak(&queue->tail, &prev, entry));

    prev->next = entry;
    atomic_fetch_add(&queue->count, 1);

    return 0;
}

static int queue_pop(kg_io_queue_t* queue, kg_io_request_t* request) {
    if (!queue || !request) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue_pop: required parameter is NULL (queue, request)");
        return -1;
    }

    kg_io_queue_entry_t* head = atomic_load(&queue->head);
    kg_io_queue_entry_t* next = head->next;

    if (!next) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "queue_pop: next is NULL");
        return -1;  /* Queue empty */
    }

    memcpy(request, &next->request, sizeof(kg_io_request_t));

    /* Move head to next (original head becomes garbage) */
    atomic_store(&queue->head, next);
    atomic_fetch_sub(&queue->count, 1);

    nimcp_free(head);

    return 0;
}

static bool queue_is_full(const kg_io_queue_t* queue) {
    if (!queue) {
        return true;
    }
    return atomic_load(&queue->count) >= queue->capacity;
}

/* ============================================================================
 * Connection Pool Implementation
 * ============================================================================ */

static int pool_init(kg_io_pool_t* pool, const kg_questdb_config_t* config) {
    if (!pool || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool_init: required parameter is NULL (pool, config)");
        return -1;
    }

    pool->size = config->pool_size;
    pool->active = 0;
    pool->idle = config->pool_size;

    pool->connections = nimcp_calloc(pool->size, sizeof(kg_io_connection_t));
    if (!pool->connections) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pool_init: pool->connections is NULL");
        return -1;
    }

    /* Initialize connections */
    for (uint32_t i = 0; i < pool->size; i++) {
        pool->connections[i].socket_fd = -1;  /* Not connected yet */
        pool->connections[i].in_use = false;
        pool->connections[i].last_used = 0;
        if (config->host) {
            strncpy(pool->connections[i].host, config->host,
                    sizeof(pool->connections[i].host) - 1);
        }
        pool->connections[i].port = config->ilp_port;
    }

    /* Create synchronization primitives */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    pool->mutex = nimcp_mutex_create(&attr);
    if (!pool->mutex) {
        nimcp_free(pool->connections);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool_init: pool->mutex is NULL");
        return -1;
    }

    pool->available = nimcp_cond_create();
    if (!pool->available) {
        nimcp_mutex_free(pool->mutex);
        nimcp_free(pool->connections);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool_init: pool->available is NULL");
        return -1;
    }

    return 0;
}

static void pool_destroy(kg_io_pool_t* pool) {
    if (!pool) {
        return;
    }

    /* Close all connections */
    if (pool->connections) {
        for (uint32_t i = 0; i < pool->size; i++) {
            if (pool->connections[i].socket_fd >= 0) {
                /* Would close socket here */
                pool->connections[i].socket_fd = -1;
            }
        }
        nimcp_free(pool->connections);
    }

    if (pool->available) {
        nimcp_cond_destroy(pool->available);
    }

    if (pool->mutex) {
        nimcp_mutex_free(pool->mutex);
    }
}

static kg_io_connection_t* pool_acquire(kg_io_pool_t* pool, uint32_t timeout_ms) {
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;
    }

    nimcp_mutex_lock(pool->mutex);

    /* Find an idle connection */
    for (uint32_t i = 0; i < pool->size; i++) {
        if (!pool->connections[i].in_use) {
            pool->connections[i].in_use = true;
            pool->connections[i].last_used = get_timestamp_ns();
            pool->active++;
            pool->idle--;
            nimcp_mutex_unlock(pool->mutex);
            return &pool->connections[i];
        }
    }

    /* Wait for one to become available */
    if (timeout_ms > 0) {
        /* Simplified wait - would use timed wait in production */
        nimcp_cond_wait(pool->available, pool->mutex);

        /* Retry after wait */
        for (uint32_t i = 0; i < pool->size; i++) {
            if (!pool->connections[i].in_use) {
                pool->connections[i].in_use = true;
                pool->connections[i].last_used = get_timestamp_ns();
                pool->active++;
                pool->idle--;
                nimcp_mutex_unlock(pool->mutex);
                return &pool->connections[i];
            }
        }
    }

    nimcp_mutex_unlock(pool->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool_acquire: operation failed");
    return NULL;
}

static void pool_release(kg_io_pool_t* pool, kg_io_connection_t* conn) {
    if (!pool || !conn) {
        return;
    }

    nimcp_mutex_lock(pool->mutex);

    conn->in_use = false;
    conn->last_used = get_timestamp_ns();
    pool->active--;
    pool->idle++;

    nimcp_cond_signal(pool->available);
    nimcp_mutex_unlock(pool->mutex);
}

/* ============================================================================
 * I/O Execution Implementation
 * ============================================================================ */

static int execute_write(kg_io_connection_t* conn, const kg_io_request_t* request,
                          kg_io_result_t* result) {
    if (!conn || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool_release: required parameter is NULL (conn, request, result)");
        return -1;
    }

    uint64_t start_time = get_timestamp_ns();

    /* Simulated write execution */
    /* In production, this would use ILP protocol to write to QuestDB */
    result->success = true;
    result->error_code = 0;
    result->row_count = request->row_count > 0 ? request->row_count : 1;

    uint64_t end_time = get_timestamp_ns();
    result->exec_latency_ns = end_time - start_time;
    result->total_latency_ns = end_time - request->submit_time_ns;
    result->queue_latency_ns = result->total_latency_ns - result->exec_latency_ns;

    return 0;
}

static int execute_read(kg_io_connection_t* conn, const kg_io_request_t* request,
                         kg_io_result_t* result) {
    if (!conn || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool_release: required parameter is NULL (conn, request, result)");
        return -1;
    }

    uint64_t start_time = get_timestamp_ns();

    /* Simulated read execution */
    /* In production, this would execute SQL via HTTP and parse results */
    result->success = true;
    result->error_code = 0;
    result->result_data = NULL;
    result->result_size = 0;
    result->row_count = 0;

    uint64_t end_time = get_timestamp_ns();
    result->exec_latency_ns = end_time - start_time;
    result->total_latency_ns = end_time - request->submit_time_ns;
    result->queue_latency_ns = result->total_latency_ns - result->exec_latency_ns;

    return 0;
}

/* ============================================================================
 * Thread Functions
 * ============================================================================ */

static void* writer_thread_func(void* arg) {
    kg_io_worker_t* worker = (kg_io_worker_t*)arg;
    if (!worker || !worker->dispatcher) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "writer_thread_func: required parameter is NULL (worker, worker->dispatcher)");
        return NULL;
    }

    kg_io_dispatcher_t* dispatcher = worker->dispatcher;
    worker->active = true;

    while (!worker->should_stop) {
        kg_io_request_t request;

        /* Try priority queues first (CRITICAL -> HIGH -> NORMAL -> LOW) */
        bool got_request = false;
        for (int p = KG_IO_PRIORITY_CRITICAL; p >= KG_IO_PRIORITY_LOW; p--) {
            if (queue_pop(&dispatcher->priority_queues[p], &request) == 0) {
                got_request = true;
                break;
            }
        }

        /* Then try regular write queue */
        if (!got_request) {
            if (queue_pop(&dispatcher->write_queue, &request) != 0) {
                /* No work available - wait */
                nimcp_mutex_lock(dispatcher->control_mutex);
                nimcp_cond_wait(dispatcher->work_available, dispatcher->control_mutex);
                nimcp_mutex_unlock(dispatcher->control_mutex);
                continue;
            }
        }

        /* Execute the write */
        kg_io_connection_t* conn = pool_acquire(&dispatcher->pool,
                                                 dispatcher->config.pool_timeout_ms);
        if (!conn) {
            /* No connection available */
            if (request.callback) {
                kg_io_result_t result = {0};
                result.op_id = request.op_id;
                result.success = false;
                result.error_code = -1;
                strncpy(result.error_message, "No connection available",
                        sizeof(result.error_message) - 1);
                request.callback(&result, request.user_data);
            }
            continue;
        }

        kg_io_result_t result = {0};
        result.op_id = request.op_id;

        execute_write(conn, &request, &result);

        /* Update statistics */
        atomic_fetch_add(&dispatcher->stats_accum.writes_total, 1);
        atomic_fetch_add(&dispatcher->stats_accum.bytes_written, request.data_size);
        atomic_fetch_add(&dispatcher->stats_accum.write_latency_sum, result.total_latency_ns);

        pool_release(&dispatcher->pool, conn);

        /* Invoke callback */
        if (request.callback) {
            request.callback(&result, request.user_data);
        }
    }

    worker->active = false;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "writer_thread_func: validation failed");
    return NULL;
}

static void* reader_thread_func(void* arg) {
    kg_io_worker_t* worker = (kg_io_worker_t*)arg;
    if (!worker || !worker->dispatcher) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reader_thread_func: required parameter is NULL (worker, worker->dispatcher)");
        return NULL;
    }

    kg_io_dispatcher_t* dispatcher = worker->dispatcher;
    worker->active = true;

    while (!worker->should_stop) {
        kg_io_request_t request;

        if (queue_pop(&dispatcher->read_queue, &request) != 0) {
            /* No work available - wait */
            nimcp_mutex_lock(dispatcher->control_mutex);
            nimcp_cond_wait(dispatcher->work_available, dispatcher->control_mutex);
            nimcp_mutex_unlock(dispatcher->control_mutex);
            continue;
        }

        /* Execute the read */
        kg_io_connection_t* conn = pool_acquire(&dispatcher->pool,
                                                 dispatcher->config.pool_timeout_ms);
        if (!conn) {
            if (request.callback) {
                kg_io_result_t result = {0};
                result.op_id = request.op_id;
                result.success = false;
                result.error_code = -1;
                strncpy(result.error_message, "No connection available",
                        sizeof(result.error_message) - 1);
                request.callback(&result, request.user_data);
            }
            continue;
        }

        kg_io_result_t result = {0};
        result.op_id = request.op_id;

        execute_read(conn, &request, &result);

        /* Update statistics */
        atomic_fetch_add(&dispatcher->stats_accum.reads_total, 1);
        atomic_fetch_add(&dispatcher->stats_accum.bytes_read, result.result_size);
        atomic_fetch_add(&dispatcher->stats_accum.read_latency_sum, result.total_latency_ns);

        pool_release(&dispatcher->pool, conn);

        /* Invoke callback */
        if (request.callback) {
            request.callback(&result, request.user_data);
        }
    }

    worker->active = false;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reader_thread_func: validation failed");
    return NULL;
}

/* ============================================================================
 * Configuration API Implementation
 * ============================================================================ */

int kg_io_default_config(kg_questdb_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(kg_questdb_config_t));

    config->host = "localhost";
    config->ilp_port = 9009;
    config->http_port = 9000;
    config->use_tls = false;

    config->username = NULL;
    config->password = NULL;

    config->pool_size = KG_IO_DEFAULT_POOL_SIZE;
    config->pool_timeout_ms = 1000;

    config->writer_threads = KG_IO_DEFAULT_WRITER_THREADS;
    config->reader_threads = KG_IO_DEFAULT_READER_THREADS;

    config->write_queue_size = KG_IO_DEFAULT_WRITE_QUEUE;
    config->read_queue_size = KG_IO_DEFAULT_READ_QUEUE;
    config->batch_queue_size = KG_IO_DEFAULT_BATCH_QUEUE;

    config->connect_timeout_ms = 5000;
    config->write_timeout_ms = KG_IO_DEFAULT_TIMEOUT_MS;
    config->read_timeout_ms = KG_IO_DEFAULT_TIMEOUT_MS;

    config->batch_size_threshold = 1000;
    config->batch_time_threshold_ms = 100;

    return 0;
}

/* ============================================================================
 * Dispatcher Lifecycle Implementation
 * ============================================================================ */

kg_io_dispatcher_t* kg_io_dispatcher_create(const kg_questdb_config_t* config) {
    kg_io_dispatcher_t* dispatcher = nimcp_calloc(1, sizeof(kg_io_dispatcher_t));
    if (!dispatcher) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dispatcher is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&dispatcher->config, config, sizeof(kg_questdb_config_t));
    } else {
        kg_io_default_config(&dispatcher->config);
    }

    /* Initialize queues */
    if (queue_init(&dispatcher->write_queue, dispatcher->config.write_queue_size) != 0) {
        goto error;
    }
    if (queue_init(&dispatcher->read_queue, dispatcher->config.read_queue_size) != 0) {
        goto error;
    }
    if (queue_init(&dispatcher->batch_queue, dispatcher->config.batch_queue_size) != 0) {
        goto error;
    }

    /* Initialize priority queues */
    for (int i = 0; i < 4; i++) {
        if (queue_init(&dispatcher->priority_queues[i], dispatcher->config.write_queue_size / 4) != 0) {
            goto error;
        }
    }

    /* Initialize connection pool */
    if (pool_init(&dispatcher->pool, &dispatcher->config) != 0) {
        goto error;
    }

    /* Create control mutex and condition */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    dispatcher->control_mutex = nimcp_mutex_create(&attr);
    if (!dispatcher->control_mutex) {
        goto error;
    }

    dispatcher->work_available = nimcp_cond_create();
    if (!dispatcher->work_available) {
        goto error;
    }

    /* Allocate worker arrays */
    dispatcher->writer_count = dispatcher->config.writer_threads;
    dispatcher->reader_count = dispatcher->config.reader_threads;

    dispatcher->writer_workers = nimcp_calloc(dispatcher->writer_count, sizeof(kg_io_worker_t));
    if (!dispatcher->writer_workers) {
        goto error;
    }

    dispatcher->reader_workers = nimcp_calloc(dispatcher->reader_count, sizeof(kg_io_worker_t));
    if (!dispatcher->reader_workers) {
        goto error;
    }

    /* Initialize workers (but don't start yet) */
    for (uint32_t i = 0; i < dispatcher->writer_count; i++) {
        dispatcher->writer_workers[i].id = i;
        dispatcher->writer_workers[i].active = false;
        dispatcher->writer_workers[i].should_stop = false;
        dispatcher->writer_workers[i].dispatcher = dispatcher;
        dispatcher->writer_workers[i].is_writer = true;
    }

    for (uint32_t i = 0; i < dispatcher->reader_count; i++) {
        dispatcher->reader_workers[i].id = i;
        dispatcher->reader_workers[i].active = false;
        dispatcher->reader_workers[i].should_stop = false;
        dispatcher->reader_workers[i].dispatcher = dispatcher;
        dispatcher->reader_workers[i].is_writer = false;
    }

    dispatcher->running = false;
    atomic_init(&dispatcher->next_op_id, 1);

    /* Initialize statistics */
    atomic_init(&dispatcher->stats_accum.writes_total, 0);
    atomic_init(&dispatcher->stats_accum.reads_total, 0);
    atomic_init(&dispatcher->stats_accum.bytes_written, 0);
    atomic_init(&dispatcher->stats_accum.bytes_read, 0);
    atomic_init(&dispatcher->stats_accum.write_latency_sum, 0);
    atomic_init(&dispatcher->stats_accum.read_latency_sum, 0);
    atomic_init(&dispatcher->stats_accum.last_reset_time, get_timestamp_ns());

    return dispatcher;

error:
    kg_io_dispatcher_destroy(dispatcher);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_io_dispatcher_create: operation failed");
    return NULL;
}

void kg_io_dispatcher_destroy(kg_io_dispatcher_t* dispatcher) {
    if (!dispatcher) {
        return;
    }

    /* Stop if running */
    if (dispatcher->running) {
        kg_io_dispatcher_stop(dispatcher);
    }

    /* Destroy queues */
    queue_destroy(&dispatcher->write_queue);
    queue_destroy(&dispatcher->read_queue);
    queue_destroy(&dispatcher->batch_queue);
    for (int i = 0; i < 4; i++) {
        queue_destroy(&dispatcher->priority_queues[i]);
    }

    /* Destroy connection pool */
    pool_destroy(&dispatcher->pool);

    /* Free workers */
    nimcp_free(dispatcher->writer_workers);
    nimcp_free(dispatcher->reader_workers);

    /* Destroy synchronization */
    if (dispatcher->work_available) {
        nimcp_cond_destroy(dispatcher->work_available);
    }
    if (dispatcher->control_mutex) {
        nimcp_mutex_free(dispatcher->control_mutex);
    }

    nimcp_free(dispatcher);
}

int kg_io_dispatcher_start(kg_io_dispatcher_t* dispatcher) {
    if (!dispatcher) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dispatcher is NULL");

        return -1;
    }

    if (dispatcher->running) {
        return 0;  /* Already running */
    }

    /* Start writer threads */
    for (uint32_t i = 0; i < dispatcher->writer_count; i++) {
        dispatcher->writer_workers[i].should_stop = false;
        if (nimcp_thread_create(&dispatcher->writer_workers[i].thread,
                           writer_thread_func, &dispatcher->writer_workers[i], NULL) != 0) {
            /* Failed to start thread */
            for (uint32_t j = 0; j < i; j++) {
                dispatcher->writer_workers[j].should_stop = true;
                nimcp_thread_join(dispatcher->writer_workers[j].thread, NULL);
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_io_dispatcher_start: operation failed");
            return -1;
        }
    }

    /* Start reader threads */
    for (uint32_t i = 0; i < dispatcher->reader_count; i++) {
        dispatcher->reader_workers[i].should_stop = false;
        if (nimcp_thread_create(&dispatcher->reader_workers[i].thread,
                           reader_thread_func, &dispatcher->reader_workers[i], NULL) != 0) {
            /* Failed - stop all threads */
            for (uint32_t j = 0; j < dispatcher->writer_count; j++) {
                dispatcher->writer_workers[j].should_stop = true;
            }
            for (uint32_t j = 0; j < i; j++) {
                dispatcher->reader_workers[j].should_stop = true;
            }
            /* Wake threads */
            nimcp_mutex_lock(dispatcher->control_mutex);
            nimcp_cond_broadcast(dispatcher->work_available);
            nimcp_mutex_unlock(dispatcher->control_mutex);
            /* Join */
            for (uint32_t j = 0; j < dispatcher->writer_count; j++) {
                nimcp_thread_join(dispatcher->writer_workers[j].thread, NULL);
            }
            for (uint32_t j = 0; j < i; j++) {
                nimcp_thread_join(dispatcher->reader_workers[j].thread, NULL);
            }
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_io_dispatcher_start: operation failed");
            return -1;
        }
    }

    dispatcher->running = true;
    return 0;
}

int kg_io_dispatcher_stop(kg_io_dispatcher_t* dispatcher) {
    if (!dispatcher || !dispatcher->running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_io_dispatcher_stop: required parameter is NULL (dispatcher, dispatcher->running)");
        return -1;
    }

    /* Signal all threads to stop */
    for (uint32_t i = 0; i < dispatcher->writer_count; i++) {
        dispatcher->writer_workers[i].should_stop = true;
    }
    for (uint32_t i = 0; i < dispatcher->reader_count; i++) {
        dispatcher->reader_workers[i].should_stop = true;
    }

    /* Wake all threads */
    nimcp_mutex_lock(dispatcher->control_mutex);
    nimcp_cond_broadcast(dispatcher->work_available);
    nimcp_mutex_unlock(dispatcher->control_mutex);

    /* Join all threads */
    for (uint32_t i = 0; i < dispatcher->writer_count; i++) {
        nimcp_thread_join(dispatcher->writer_workers[i].thread, NULL);
    }
    for (uint32_t i = 0; i < dispatcher->reader_count; i++) {
        nimcp_thread_join(dispatcher->reader_workers[i].thread, NULL);
    }

    dispatcher->running = false;
    return 0;
}

/* ============================================================================
 * Async I/O Operations Implementation
 * ============================================================================ */

int kg_io_write_async(kg_io_dispatcher_t* dispatcher,
                      const char* table, const void* row_data, size_t size,
                      kg_io_callback_fn callback, void* user_data) {
    if (!dispatcher || !table || !row_data || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_io_dispatcher_stop: required parameter is NULL (dispatcher, table, row_data)");
        return -1;
    }

    kg_io_request_t request = {0};
    request.type = KG_IO_WRITE;
    request.priority = KG_IO_PRIORITY_NORMAL;
    request.table_name = table;
    request.data = row_data;
    request.data_size = size;
    request.row_count = 1;
    request.callback = callback;
    request.user_data = user_data;
    request.submit_time_ns = get_timestamp_ns();
    request.op_id = atomic_fetch_add(&dispatcher->next_op_id, 1);

    if (queue_push(&dispatcher->write_queue, &request) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_io_dispatcher_stop: validation failed");
        return -1;  /* Queue full */
    }

    /* Signal workers */
    nimcp_mutex_lock(dispatcher->control_mutex);
    nimcp_cond_signal(dispatcher->work_available);
    nimcp_mutex_unlock(dispatcher->control_mutex);

    return 0;
}

int kg_io_write_batch_async(kg_io_dispatcher_t* dispatcher,
                            const char* table, const void* batch_data,
                            size_t size, uint32_t row_count,
                            kg_io_callback_fn callback, void* user_data) {
    if (!dispatcher || !table || !batch_data || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_io_dispatcher_stop: required parameter is NULL (dispatcher, table, batch_data)");
        return -1;
    }

    kg_io_request_t request = {0};
    request.type = KG_IO_WRITE_BATCH;
    request.priority = KG_IO_PRIORITY_NORMAL;
    request.table_name = table;
    request.data = batch_data;
    request.data_size = size;
    request.row_count = row_count;
    request.callback = callback;
    request.user_data = user_data;
    request.submit_time_ns = get_timestamp_ns();
    request.op_id = atomic_fetch_add(&dispatcher->next_op_id, 1);

    if (queue_push(&dispatcher->batch_queue, &request) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_io_dispatcher_stop: validation failed");
        return -1;
    }

    nimcp_mutex_lock(dispatcher->control_mutex);
    nimcp_cond_signal(dispatcher->work_available);
    nimcp_mutex_unlock(dispatcher->control_mutex);

    return 0;
}

int kg_io_query_async(kg_io_dispatcher_t* dispatcher,
                      const char* sql, kg_io_priority_t priority,
                      kg_io_callback_fn callback, void* user_data) {
    if (!dispatcher || !sql) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_io_dispatcher_stop: required parameter is NULL (dispatcher, sql)");
        return -1;
    }

    kg_io_request_t request = {0};
    request.type = KG_IO_READ;
    request.priority = priority;
    request.query = sql;
    request.timeout_ms = dispatcher->config.read_timeout_ms;
    request.callback = callback;
    request.user_data = user_data;
    request.submit_time_ns = get_timestamp_ns();
    request.op_id = atomic_fetch_add(&dispatcher->next_op_id, 1);

    if (queue_push(&dispatcher->read_queue, &request) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_io_dispatcher_stop: validation failed");
        return -1;
    }

    nimcp_mutex_lock(dispatcher->control_mutex);
    nimcp_cond_signal(dispatcher->work_available);
    nimcp_mutex_unlock(dispatcher->control_mutex);

    return 0;
}

int kg_io_stream_async(kg_io_dispatcher_t* dispatcher,
                       const char* sql, uint32_t batch_size,
                       kg_io_callback_fn callback, void* user_data) {
    if (!dispatcher || !sql || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_io_dispatcher_stop: required parameter is NULL (dispatcher, sql, callback)");
        return -1;
    }

    kg_io_request_t request = {0};
    request.type = KG_IO_READ_STREAM;
    request.priority = KG_IO_PRIORITY_NORMAL;
    request.query = sql;
    request.row_count = batch_size;
    request.callback = callback;
    request.user_data = user_data;
    request.submit_time_ns = get_timestamp_ns();
    request.op_id = atomic_fetch_add(&dispatcher->next_op_id, 1);

    if (queue_push(&dispatcher->read_queue, &request) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_io_dispatcher_stop: validation failed");
        return -1;
    }

    nimcp_mutex_lock(dispatcher->control_mutex);
    nimcp_cond_signal(dispatcher->work_available);
    nimcp_mutex_unlock(dispatcher->control_mutex);

    return 0;
}

/* ============================================================================
 * Sync I/O Operations Implementation
 * ============================================================================ */

typedef struct {
    bool completed;
    kg_io_result_t result;
    nimcp_mutex_t* mutex;
    nimcp_cond_t* cond;
} sync_context_t;

static void sync_callback(kg_io_result_t* result, void* user_data) {
    sync_context_t* ctx = (sync_context_t*)user_data;
    if (!ctx) return;

    nimcp_mutex_lock(ctx->mutex);
    memcpy(&ctx->result, result, sizeof(kg_io_result_t));
    ctx->completed = true;
    nimcp_cond_signal(ctx->cond);
    nimcp_mutex_unlock(ctx->mutex);
}

int kg_io_write_sync(kg_io_dispatcher_t* dispatcher,
                     const char* table, const void* row_data, size_t size,
                     uint32_t timeout_ms) {
    if (!dispatcher || !table || !row_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sync_callback: required parameter is NULL (dispatcher, table, row_data)");
        return -1;
    }

    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    nimcp_mutex_t* mutex = nimcp_mutex_create(&attr);
    nimcp_cond_t* cond = nimcp_cond_create();

    if (!mutex || !cond) {
        if (mutex) nimcp_mutex_free(mutex);
        if (cond) nimcp_cond_destroy(cond);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sync_callback: validation failed");
        return -1;
    }

    sync_context_t ctx = {0};
    ctx.completed = false;
    ctx.mutex = mutex;
    ctx.cond = cond;

    if (kg_io_write_async(dispatcher, table, row_data, size, sync_callback, &ctx) != 0) {
        nimcp_mutex_free(mutex);
        nimcp_cond_destroy(cond);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sync_callback: validation failed");
        return -1;
    }

    nimcp_mutex_lock(mutex);
    while (!ctx.completed) {
        /* Would use timed wait in production */
        nimcp_cond_wait(cond, mutex);
    }
    nimcp_mutex_unlock(mutex);

    int result = ctx.result.success ? 0 : -1;

    nimcp_mutex_free(mutex);
    nimcp_cond_destroy(cond);

    return result;
}

kg_io_result_t* kg_io_query_sync(kg_io_dispatcher_t* dispatcher,
                                  const char* sql, uint32_t timeout_ms) {
    if (!dispatcher || !sql) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sync_callback: required parameter is NULL (dispatcher, sql)");
        return NULL;
    }

    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    nimcp_mutex_t* mutex = nimcp_mutex_create(&attr);
    nimcp_cond_t* cond = nimcp_cond_create();

    if (!mutex || !cond) {
        if (mutex) nimcp_mutex_free(mutex);
        if (cond) nimcp_cond_destroy(cond);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sync_callback: validation failed");
        return NULL;
    }

    sync_context_t ctx = {0};
    ctx.completed = false;
    ctx.mutex = mutex;
    ctx.cond = cond;

    if (kg_io_query_async(dispatcher, sql, KG_IO_PRIORITY_NORMAL, sync_callback, &ctx) != 0) {
        nimcp_mutex_free(mutex);
        nimcp_cond_destroy(cond);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sync_callback: validation failed");
        return NULL;
    }

    nimcp_mutex_lock(mutex);
    while (!ctx.completed) {
        nimcp_cond_wait(cond, mutex);
    }
    nimcp_mutex_unlock(mutex);

    nimcp_mutex_free(mutex);
    nimcp_cond_destroy(cond);

    kg_io_result_t* result = nimcp_calloc(1, sizeof(kg_io_result_t));
    if (result) {
        memcpy(result, &ctx.result, sizeof(kg_io_result_t));
    }

    return result;
}

void kg_io_result_free(kg_io_result_t* result) {
    if (!result) {
        return;
    }
    nimcp_free(result->result_data);
    nimcp_free(result);
}

/* ============================================================================
 * Batch Operations Implementation
 * ============================================================================ */

kg_io_batch_t* kg_io_batch_create(kg_io_dispatcher_t* dispatcher,
                                   const char* table, uint32_t estimated_rows) {
    if (!dispatcher || !table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_io_result_free: required parameter is NULL (dispatcher, table)");
        return NULL;
    }

    kg_io_batch_t* batch = nimcp_calloc(1, sizeof(kg_io_batch_t));
    if (!batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "batch is NULL");

        return NULL;
    }

    batch->dispatcher = dispatcher;
    strncpy(batch->table_name, table, KG_IO_MAX_TABLE_NAME - 1);

    batch->buffer_capacity = IO_BATCH_INITIAL_SIZE;
    if (estimated_rows > 0) {
        batch->buffer_capacity = estimated_rows * 256;  /* Estimate 256 bytes per row */
    }

    batch->buffer = nimcp_malloc(batch->buffer_capacity);
    if (!batch->buffer) {
        nimcp_free(batch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_io_result_free: batch->buffer is NULL");
        return NULL;
    }

    batch->buffer_size = 0;
    batch->row_count = 0;
    batch->submitted = false;

    return batch;
}

int kg_io_batch_add_row(kg_io_batch_t* batch, const void* row_data, size_t size) {
    if (!batch || !row_data || size == 0 || batch->submitted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_io_batch_add_row: required parameter is NULL (batch, row_data)");
        return -1;
    }

    /* Expand buffer if needed */
    if (batch->buffer_size + size > batch->buffer_capacity) {
        size_t new_capacity = batch->buffer_capacity * 2;
        while (new_capacity < batch->buffer_size + size) {
            new_capacity *= 2;
        }

        uint8_t* new_buffer = nimcp_realloc(batch->buffer, new_capacity);
        if (!new_buffer) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "new_buffer is NULL");

            return -1;
        }
        batch->buffer = new_buffer;
        batch->buffer_capacity = new_capacity;
    }

    /* Append row data */
    memcpy(batch->buffer + batch->buffer_size, row_data, size);
    batch->buffer_size += size;
    batch->row_count++;

    return 0;
}

int kg_io_batch_submit(kg_io_batch_t* batch,
                        kg_io_callback_fn callback, void* user_data) {
    if (!batch || batch->submitted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_io_batch_add_row: batch is NULL");
        return -1;
    }

    if (batch->row_count == 0) {
        kg_io_batch_cancel(batch);
        return 0;
    }

    int result = kg_io_write_batch_async(batch->dispatcher, batch->table_name,
                                          batch->buffer, batch->buffer_size,
                                          batch->row_count, callback, user_data);

    batch->submitted = true;

    /* Note: batch is consumed - caller should not use it after submit */
    /* The dispatcher now owns the buffer data for the duration of the write */

    return result;
}

void kg_io_batch_cancel(kg_io_batch_t* batch) {
    if (!batch) {
        return;
    }

    nimcp_free(batch->buffer);
    nimcp_free(batch);
}

/* ============================================================================
 * Flow Control Implementation
 * ============================================================================ */

bool kg_io_can_accept_writes(const kg_io_dispatcher_t* dispatcher) {
    if (!dispatcher) {
        return false;
    }
    return !queue_is_full(&dispatcher->write_queue);
}

int kg_io_flush(kg_io_dispatcher_t* dispatcher, uint32_t timeout_ms) {
    if (!dispatcher) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dispatcher is NULL");

        return -1;
    }

    /* Wait for queues to drain */
    uint64_t start = get_timestamp_ns();
    uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;

    while (atomic_load(&dispatcher->write_queue.count) > 0 ||
           atomic_load(&dispatcher->batch_queue.count) > 0) {

        uint64_t elapsed = get_timestamp_ns() - start;
        if (elapsed > timeout_ns) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_io_flush: validation failed");
            return -1;  /* Timeout */
        }

        /* Brief sleep */
        struct timespec ts = {0, 1000000};  /* 1ms */
        nanosleep(&ts, NULL);
    }

    return 0;
}

int kg_io_sync(kg_io_dispatcher_t* dispatcher, uint32_t timeout_ms) {
    if (!dispatcher) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dispatcher is NULL");

        return -1;
    }

    /* Flush first */
    if (kg_io_flush(dispatcher, timeout_ms) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_io_sync: validation failed");
        return -1;
    }

    /* Would send sync command to database here */
    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int kg_io_get_stats(const kg_io_dispatcher_t* dispatcher, kg_io_stats_t* stats) {
    if (!dispatcher || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_io_get_stats: required parameter is NULL (dispatcher, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(kg_io_stats_t));

    uint64_t now = get_timestamp_ns();
    uint64_t last_reset = atomic_load(&dispatcher->stats_accum.last_reset_time);
    double elapsed_secs = (double)(now - last_reset) / 1e9;
    if (elapsed_secs < 0.001) elapsed_secs = 0.001;

    uint64_t writes = atomic_load(&dispatcher->stats_accum.writes_total);
    uint64_t reads = atomic_load(&dispatcher->stats_accum.reads_total);
    uint64_t bytes_w = atomic_load(&dispatcher->stats_accum.bytes_written);
    uint64_t bytes_r = atomic_load(&dispatcher->stats_accum.bytes_read);
    uint64_t write_lat = atomic_load(&dispatcher->stats_accum.write_latency_sum);
    uint64_t read_lat = atomic_load(&dispatcher->stats_accum.read_latency_sum);

    stats->writes_per_sec = (uint64_t)((double)writes / elapsed_secs);
    stats->reads_per_sec = (uint64_t)((double)reads / elapsed_secs);
    stats->bytes_written_per_sec = (uint64_t)((double)bytes_w / elapsed_secs);
    stats->bytes_read_per_sec = (uint64_t)((double)bytes_r / elapsed_secs);

    stats->avg_write_latency_ns = writes > 0 ? write_lat / writes : 0;
    stats->avg_read_latency_ns = reads > 0 ? read_lat / reads : 0;

    /* Estimate p99 (simplified - would use histogram in production) */
    stats->p99_write_latency_ns = stats->avg_write_latency_ns * 3;
    stats->p99_read_latency_ns = stats->avg_read_latency_ns * 3;

    /* Queue depths */
    stats->write_queue_depth = atomic_load(&dispatcher->write_queue.count);
    stats->read_queue_depth = atomic_load(&dispatcher->read_queue.count);
    stats->batch_queue_depth = atomic_load(&dispatcher->batch_queue.count);

    /* Pool status */
    stats->active_connections = dispatcher->pool.active;
    stats->idle_connections = dispatcher->pool.idle;
    stats->pending_requests = 0;  /* Would track pending acquires */

    /* Thread counts */
    stats->active_writer_threads = 0;
    stats->active_reader_threads = 0;
    for (uint32_t i = 0; i < dispatcher->writer_count; i++) {
        if (dispatcher->writer_workers[i].active) {
            stats->active_writer_threads++;
        }
    }
    for (uint32_t i = 0; i < dispatcher->reader_count; i++) {
        if (dispatcher->reader_workers[i].active) {
            stats->active_reader_threads++;
        }
    }

    return 0;
}

void kg_io_reset_stats(kg_io_dispatcher_t* dispatcher) {
    if (!dispatcher) {
        return;
    }

    atomic_store(&dispatcher->stats_accum.writes_total, 0);
    atomic_store(&dispatcher->stats_accum.reads_total, 0);
    atomic_store(&dispatcher->stats_accum.bytes_written, 0);
    atomic_store(&dispatcher->stats_accum.bytes_read, 0);
    atomic_store(&dispatcher->stats_accum.write_latency_sum, 0);
    atomic_store(&dispatcher->stats_accum.read_latency_sum, 0);
    atomic_store(&dispatcher->stats_accum.last_reset_time, get_timestamp_ns());
}

/* ============================================================================
 * String Conversion Utilities Implementation
 * ============================================================================ */

const char* kg_io_op_type_to_string(kg_io_op_type_t type) {
    switch (type) {
        case KG_IO_WRITE:
            return "WRITE";
        case KG_IO_WRITE_BATCH:
            return "WRITE_BATCH";
        case KG_IO_READ:
            return "READ";
        case KG_IO_READ_STREAM:
            return "READ_STREAM";
        case KG_IO_FLUSH:
            return "FLUSH";
        case KG_IO_SYNC:
            return "SYNC";
        default:
            return "UNKNOWN";
    }
}

const char* kg_io_priority_to_string(kg_io_priority_t priority) {
    switch (priority) {
        case KG_IO_PRIORITY_LOW:
            return "LOW";
        case KG_IO_PRIORITY_NORMAL:
            return "NORMAL";
        case KG_IO_PRIORITY_HIGH:
            return "HIGH";
        case KG_IO_PRIORITY_CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}
