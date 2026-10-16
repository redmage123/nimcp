/**
 * @file nimcp_kg_io_dispatcher.h
 * @brief Knowledge Graph I/O Dispatcher for High-Performance Database Operations
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Asynchronous I/O dispatcher for Knowledge Graph database operations
 * WHY:  High-throughput, low-latency KG persistence with backpressure control
 * HOW:  Thread pools, lock-free queues, connection pooling, and batched writes
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                        KG I/O DISPATCHER                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   CALLER                                                                   ║
 * ║   ──────                                                                   ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐  ║
 * ║   │  kg_io_write_async()    kg_io_query_async()    kg_io_batch_submit() │  ║
 * ║   └───────────────────────────────┬─────────────────────────────────────┘  ║
 * ║                                   │                                        ║
 * ║                                   ▼                                        ║
 * ║   PRIORITY QUEUES (Lock-Free MPSC)                                         ║
 * ║   ─────────────────────────────────                                        ║
 * ║   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      ║
 * ║   │  CRITICAL   │  │    HIGH     │  │   NORMAL    │  │     LOW     │      ║
 * ║   │   Queue     │  │   Queue     │  │   Queue     │  │   Queue     │      ║
 * ║   └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘      ║
 * ║          │                │                │                │              ║
 * ║          └────────────────┴────────────────┴────────────────┘              ║
 * ║                                   │                                        ║
 * ║                                   ▼                                        ║
 * ║   THREAD POOLS                                                             ║
 * ║   ─────────────                                                            ║
 * ║   ┌─────────────────────────┐    ┌─────────────────────────┐              ║
 * ║   │     WRITER THREADS      │    │     READER THREADS      │              ║
 * ║   │  ┌───┐ ┌───┐ ┌───┐     │    │  ┌───┐ ┌───┐ ┌───┐     │              ║
 * ║   │  │ W │ │ W │ │ W │     │    │  │ R │ │ R │ │ R │     │              ║
 * ║   │  └───┘ └───┘ └───┘     │    │  └───┘ └───┘ └───┘     │              ║
 * ║   └───────────┬─────────────┘    └───────────┬─────────────┘              ║
 * ║               │                              │                             ║
 * ║               ▼                              ▼                             ║
 * ║   CONNECTION POOL                                                          ║
 * ║   ───────────────                                                          ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐  ║
 * ║   │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐  │  ║
 * ║   │  │Conn1│ │Conn2│ │Conn3│ │Conn4│ │Conn5│ │Conn6│ │Conn7│ │Conn8│  │  ║
 * ║   │  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘ └─────┘  │  ║
 * ║   └───────────────────────────────┬─────────────────────────────────────┘  ║
 * ║                                   │                                        ║
 * ║                                   ▼                                        ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                        DATABASE (QuestDB)                           │  ║
 * ║   └─────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * I/O OPTIMIZATION BENEFITS:
 * 1. Lock-free queues: MPSC queues for write/read operations avoid contention
 * 2. Thread pool: Dedicated writer/reader threads with CPU affinity
 * 3. Connection pooling: Reuse connections, avoid handshake overhead
 * 4. Batch coalescing: Combine small writes into efficient batches
 * 5. io_uring: Linux kernel I/O with zero-copy and batched syscalls
 * 6. Backpressure: Flow control prevents memory exhaustion
 * 7. Priority queues: Critical operations bypass normal queue
 * 8. Result caching: Avoid redundant queries for hot data
 *
 * THREAD SAFETY: All public functions are thread-safe
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_IO_DISPATCHER_H
#define NIMCP_KG_IO_DISPATCHER_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/** Forward declare result type for callback signature */
typedef struct kg_io_result kg_io_result_t;

/** Forward declare dispatcher type (opaque) */
typedef struct kg_io_dispatcher kg_io_dispatcher_t;

/** Forward declare batch type (opaque) */
typedef struct kg_io_batch kg_io_batch_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum length of table name */
#define KG_IO_MAX_TABLE_NAME        128

/** Maximum length of SQL query */
#define KG_IO_MAX_QUERY_LEN         4096

/** Maximum length of error message */
#define KG_IO_MAX_ERROR_MSG         256

/** Default write queue capacity */
#define KG_IO_DEFAULT_WRITE_QUEUE   1024

/** Default read queue capacity */
#define KG_IO_DEFAULT_READ_QUEUE    256

/** Default batch queue capacity */
#define KG_IO_DEFAULT_BATCH_QUEUE   64

/** Default number of writer threads */
#define KG_IO_DEFAULT_WRITER_THREADS  4

/** Default number of reader threads */
#define KG_IO_DEFAULT_READER_THREADS  2

/** Default connection pool size */
#define KG_IO_DEFAULT_POOL_SIZE     16

/** Default operation timeout (ms) */
#define KG_IO_DEFAULT_TIMEOUT_MS    5000

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief I/O operation types
 *
 * WHAT: Classification of database operations
 * WHY:  Route operations to appropriate queues and handlers
 */
typedef enum {
    KG_IO_WRITE = 0,                 /**< Single row write */
    KG_IO_WRITE_BATCH,               /**< Batched writes */
    KG_IO_READ,                      /**< Query read */
    KG_IO_READ_STREAM,               /**< Streaming read */
    KG_IO_FLUSH,                     /**< Force flush buffers */
    KG_IO_SYNC                       /**< Sync to disk */
} kg_io_op_type_t;

/**
 * @brief I/O operation priority
 *
 * WHAT: Priority levels for operation scheduling
 * WHY:  Ensure critical operations complete promptly
 */
typedef enum {
    KG_IO_PRIORITY_LOW = 0,          /**< Background operations */
    KG_IO_PRIORITY_NORMAL,           /**< Standard operations */
    KG_IO_PRIORITY_HIGH,             /**< Time-sensitive */
    KG_IO_PRIORITY_CRITICAL          /**< Must complete immediately */
} kg_io_priority_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Async I/O completion callback
 *
 * @param result Operation result containing success/failure and any data
 * @param user_data User-provided context from request
 */
typedef void (*kg_io_callback_fn)(kg_io_result_t* result, void* user_data);

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief I/O operation request
 *
 * WHAT: Complete specification of an I/O operation
 * WHY:  Encapsulate all parameters for async queue submission
 */
typedef struct {
    kg_io_op_type_t type;            /**< Operation type */
    kg_io_priority_t priority;       /**< Operation priority */
    const char* table_name;          /**< Target table */

    /* For writes */
    const void* data;                /**< Row data or batch */
    size_t data_size;                /**< Data size in bytes */
    uint32_t row_count;              /**< Row count for batches */

    /* For reads */
    const char* query;               /**< SQL query */
    uint32_t timeout_ms;             /**< Operation timeout */

    /* Async completion */
    kg_io_callback_fn callback;      /**< Completion callback */
    void* user_data;                 /**< User context for callback */

    /* Internal tracking */
    uint64_t submit_time_ns;         /**< Submission timestamp (ns) */
    uint64_t op_id;                  /**< Unique operation ID */
} kg_io_request_t;

/**
 * @brief I/O operation result
 *
 * WHAT: Result of a completed I/O operation
 * WHY:  Provide success/error status and returned data
 */
struct kg_io_result {
    uint64_t op_id;                  /**< Operation ID from request */
    bool success;                    /**< True if operation succeeded */
    int error_code;                  /**< Error code (0 on success) */
    char error_message[KG_IO_MAX_ERROR_MSG]; /**< Error description */

    /* For reads */
    void* result_data;               /**< Query result data (caller frees) */
    size_t result_size;              /**< Result data size */
    uint32_t row_count;              /**< Number of rows returned */

    /* Timing statistics */
    uint64_t queue_latency_ns;       /**< Time spent in queue */
    uint64_t exec_latency_ns;        /**< Execution time */
    uint64_t total_latency_ns;       /**< Total time (queue + exec) */
};

/**
 * @brief I/O dispatcher statistics
 *
 * WHAT: Real-time performance metrics for the dispatcher
 * WHY:  Enable monitoring, alerting, and capacity planning
 */
typedef struct {
    /* Throughput */
    uint64_t writes_per_sec;         /**< Write operations per second */
    uint64_t reads_per_sec;          /**< Read operations per second */
    uint64_t bytes_written_per_sec;  /**< Write throughput in bytes/sec */
    uint64_t bytes_read_per_sec;     /**< Read throughput in bytes/sec */

    /* Latency (nanoseconds) */
    uint64_t avg_write_latency_ns;   /**< Average write latency */
    uint64_t p99_write_latency_ns;   /**< 99th percentile write latency */
    uint64_t avg_read_latency_ns;    /**< Average read latency */
    uint64_t p99_read_latency_ns;    /**< 99th percentile read latency */

    /* Queue depths */
    uint32_t write_queue_depth;      /**< Current write queue depth */
    uint32_t read_queue_depth;       /**< Current read queue depth */
    uint32_t batch_queue_depth;      /**< Current batch queue depth */

    /* Pool status */
    uint32_t active_connections;     /**< Connections in use */
    uint32_t idle_connections;       /**< Idle connections */
    uint32_t pending_requests;       /**< Requests waiting for connection */

    /* Thread pool */
    uint32_t active_writer_threads;  /**< Active writer threads */
    uint32_t active_reader_threads;  /**< Active reader threads */
} kg_io_stats_t;

/**
 * @brief QuestDB connection configuration
 *
 * WHAT: Database connection parameters
 * WHY:  Configure connection pool and protocol settings
 */
typedef struct {
    /* Connection */
    const char* host;                /**< Database host */
    uint16_t ilp_port;               /**< ILP (write) port */
    uint16_t http_port;              /**< HTTP (query) port */
    bool use_tls;                    /**< Enable TLS encryption */

    /* Authentication */
    const char* username;            /**< Username (if auth enabled) */
    const char* password;            /**< Password (if auth enabled) */

    /* Pool settings */
    uint32_t pool_size;              /**< Connection pool size */
    uint32_t pool_timeout_ms;        /**< Connection acquire timeout */

    /* Thread settings */
    uint32_t writer_threads;         /**< Number of writer threads */
    uint32_t reader_threads;         /**< Number of reader threads */

    /* Queue settings */
    uint32_t write_queue_size;       /**< Write queue capacity */
    uint32_t read_queue_size;        /**< Read queue capacity */
    uint32_t batch_queue_size;       /**< Batch queue capacity */

    /* Timeouts */
    uint32_t connect_timeout_ms;     /**< Connection timeout */
    uint32_t write_timeout_ms;       /**< Write timeout */
    uint32_t read_timeout_ms;        /**< Read timeout */

    /* Batching */
    uint32_t batch_size_threshold;   /**< Auto-batch size threshold */
    uint32_t batch_time_threshold_ms;/**< Auto-batch time threshold */
} kg_questdb_config_t;

/* ============================================================================
 * I/O Dispatcher Lifecycle
 * ============================================================================ */

/**
 * @brief Get default QuestDB configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Provide working defaults that can be customized
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int kg_io_default_config(kg_questdb_config_t* config);

/**
 * @brief Create I/O dispatcher with configuration
 *
 * WHAT: Initialize dispatcher with thread pools and connection pool
 * WHY:  Establish infrastructure for async I/O operations
 * HOW:  Allocate resources, start threads, initialize queues
 *
 * @param config Configuration (NULL for defaults)
 * @return Dispatcher handle or NULL on error
 */
kg_io_dispatcher_t* kg_io_dispatcher_create(const kg_questdb_config_t* config);

/**
 * @brief Destroy dispatcher and release all resources
 *
 * WHAT: Clean shutdown of dispatcher
 * WHY:  Properly release threads, connections, and memory
 * HOW:  Stop threads, drain queues, close connections
 *
 * @param dispatcher Dispatcher to destroy (NULL safe)
 */
void kg_io_dispatcher_destroy(kg_io_dispatcher_t* dispatcher);

/**
 * @brief Start I/O processing threads
 *
 * WHAT: Activate writer and reader thread pools
 * WHY:  Begin processing queued operations
 *
 * @param dispatcher Dispatcher handle
 * @return 0 on success, -1 on error
 */
int kg_io_dispatcher_start(kg_io_dispatcher_t* dispatcher);

/**
 * @brief Stop I/O processing threads
 *
 * WHAT: Gracefully stop thread pools
 * WHY:  Pause processing while keeping dispatcher alive
 *
 * @param dispatcher Dispatcher handle
 * @return 0 on success, -1 on error
 */
int kg_io_dispatcher_stop(kg_io_dispatcher_t* dispatcher);

/* ============================================================================
 * Async I/O Operations (Non-blocking)
 * ============================================================================ */

/**
 * @brief Submit async write (returns immediately)
 *
 * WHAT: Queue a single row write operation
 * WHY:  Non-blocking write for high throughput
 * HOW:  Add to write queue, callback invoked on completion
 *
 * @param dispatcher Dispatcher handle
 * @param table Target table name
 * @param row_data Row data to write
 * @param size Data size in bytes
 * @param callback Completion callback (can be NULL)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error (queue full, etc.)
 */
int kg_io_write_async(kg_io_dispatcher_t* dispatcher,
                      const char* table, const void* row_data, size_t size,
                      kg_io_callback_fn callback, void* user_data);

/**
 * @brief Submit async batch write
 *
 * WHAT: Queue a batch of rows for efficient writing
 * WHY:  Reduce per-row overhead for bulk operations
 *
 * @param dispatcher Dispatcher handle
 * @param table Target table name
 * @param batch_data Batch data buffer
 * @param size Total data size
 * @param row_count Number of rows in batch
 * @param callback Completion callback
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int kg_io_write_batch_async(kg_io_dispatcher_t* dispatcher,
                            const char* table, const void* batch_data,
                            size_t size, uint32_t row_count,
                            kg_io_callback_fn callback, void* user_data);

/**
 * @brief Submit async query
 *
 * WHAT: Queue a SQL query for execution
 * WHY:  Non-blocking query for responsive applications
 *
 * @param dispatcher Dispatcher handle
 * @param sql SQL query string
 * @param priority Query priority
 * @param callback Completion callback with results
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int kg_io_query_async(kg_io_dispatcher_t* dispatcher,
                      const char* sql, kg_io_priority_t priority,
                      kg_io_callback_fn callback, void* user_data);

/**
 * @brief Stream query results asynchronously
 *
 * WHAT: Execute query with streaming result batches
 * WHY:  Handle large result sets without memory exhaustion
 * HOW:  Callback invoked for each batch of rows
 *
 * @param dispatcher Dispatcher handle
 * @param sql SQL query string
 * @param batch_size Rows per callback batch
 * @param callback Callback invoked per batch
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int kg_io_stream_async(kg_io_dispatcher_t* dispatcher,
                       const char* sql, uint32_t batch_size,
                       kg_io_callback_fn callback, void* user_data);

/* ============================================================================
 * Sync I/O Operations (Blocking convenience wrappers)
 * ============================================================================ */

/**
 * @brief Blocking write (waits for completion)
 *
 * WHAT: Synchronous single-row write
 * WHY:  Simple API for when caller can block
 *
 * @param dispatcher Dispatcher handle
 * @param table Target table name
 * @param row_data Row data to write
 * @param size Data size
 * @param timeout_ms Maximum wait time
 * @return 0 on success, -1 on error or timeout
 */
int kg_io_write_sync(kg_io_dispatcher_t* dispatcher,
                     const char* table, const void* row_data, size_t size,
                     uint32_t timeout_ms);

/**
 * @brief Blocking query (waits for results)
 *
 * WHAT: Synchronous query execution
 * WHY:  Simple API for when caller can block
 *
 * @param dispatcher Dispatcher handle
 * @param sql SQL query string
 * @param timeout_ms Maximum wait time
 * @return Result structure (caller must free with kg_io_result_free)
 */
kg_io_result_t* kg_io_query_sync(kg_io_dispatcher_t* dispatcher,
                                 const char* sql, uint32_t timeout_ms);

/**
 * @brief Free result from synchronous query
 *
 * @param result Result to free (NULL safe)
 */
void kg_io_result_free(kg_io_result_t* result);

/* ============================================================================
 * Batch Operations
 * ============================================================================ */

/**
 * @brief Create batch builder for efficient multi-row inserts
 *
 * WHAT: Initialize a batch accumulator
 * WHY:  Efficient bulk writes by combining rows
 *
 * @param dispatcher Dispatcher handle
 * @param table Target table name
 * @param estimated_rows Estimated number of rows (for pre-allocation)
 * @return Batch handle or NULL on error
 */
kg_io_batch_t* kg_io_batch_create(kg_io_dispatcher_t* dispatcher,
                                  const char* table, uint32_t estimated_rows);

/**
 * @brief Add row to batch
 *
 * WHAT: Append a row to the batch buffer
 * WHY:  Accumulate rows for efficient bulk write
 *
 * @param batch Batch handle
 * @param row_data Row data
 * @param size Row data size
 * @return 0 on success, -1 on error
 */
int kg_io_batch_add_row(kg_io_batch_t* batch, const void* row_data, size_t size);

/**
 * @brief Submit batch asynchronously
 *
 * WHAT: Queue the accumulated batch for writing
 * WHY:  Trigger the actual database write
 *
 * @param batch Batch handle (consumed, do not reuse)
 * @param callback Completion callback
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
int kg_io_batch_submit(kg_io_batch_t* batch,
                       kg_io_callback_fn callback, void* user_data);

/**
 * @brief Cancel and free batch without submitting
 *
 * @param batch Batch to cancel (NULL safe)
 */
void kg_io_batch_cancel(kg_io_batch_t* batch);

/* ============================================================================
 * Flow Control
 * ============================================================================ */

/**
 * @brief Check if dispatcher is accepting writes (backpressure)
 *
 * WHAT: Query write queue availability
 * WHY:  Allow callers to implement backpressure
 *
 * @param dispatcher Dispatcher handle
 * @return true if writes are accepted, false if queue is full
 */
bool kg_io_can_accept_writes(const kg_io_dispatcher_t* dispatcher);

/**
 * @brief Flush all pending writes (blocking)
 *
 * WHAT: Wait for all queued writes to complete
 * WHY:  Ensure data persistence before proceeding
 *
 * @param dispatcher Dispatcher handle
 * @param timeout_ms Maximum wait time
 * @return 0 on success, -1 on error or timeout
 */
int kg_io_flush(kg_io_dispatcher_t* dispatcher, uint32_t timeout_ms);

/**
 * @brief Sync data to disk (blocking)
 *
 * WHAT: Force database to persist buffered data
 * WHY:  Guarantee durability after critical writes
 *
 * @param dispatcher Dispatcher handle
 * @param timeout_ms Maximum wait time
 * @return 0 on success, -1 on error or timeout
 */
int kg_io_sync(kg_io_dispatcher_t* dispatcher, uint32_t timeout_ms);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

/**
 * @brief Get current dispatcher statistics
 *
 * WHAT: Snapshot of performance metrics
 * WHY:  Enable monitoring and capacity planning
 *
 * @param dispatcher Dispatcher handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int kg_io_get_stats(const kg_io_dispatcher_t* dispatcher, kg_io_stats_t* stats);

/**
 * @brief Reset dispatcher statistics
 *
 * WHAT: Zero all counters and latency measurements
 * WHY:  Start fresh measurement period
 *
 * @param dispatcher Dispatcher handle
 */
void kg_io_reset_stats(kg_io_dispatcher_t* dispatcher);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert I/O operation type to string
 *
 * @param type Operation type
 * @return Static string representation
 */
const char* kg_io_op_type_to_string(kg_io_op_type_t type);

/**
 * @brief Convert I/O priority to string
 *
 * @param priority Priority level
 * @return Static string representation
 */
const char* kg_io_priority_to_string(kg_io_priority_t priority);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_IO_DISPATCHER_H */
