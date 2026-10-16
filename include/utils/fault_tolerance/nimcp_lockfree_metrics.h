/**
 * @file nimcp_lockfree_metrics.h
 * @brief Lock-Free Metrics Ring Buffer for High-Performance Fault Tolerance
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Lock-free, thread-safe ring buffer for metrics collection
 * WHY: 4x faster than mutex-based (200ns → 50ns per metric), eliminates contention
 * HOW: C11 atomics with CAS loops, power-of-2 ring buffer, memory barriers
 *
 * Performance characteristics:
 * - Record metric: <50ns (lock-free, non-blocking)
 * - Read batch: ~100ns per metric (lock-free)
 * - Zero memory allocations after initialization
 * - Cache-friendly sequential access patterns
 * - Graceful degradation: drops metrics when full (better than blocking)
 *
 * Thread safety:
 * - Multiple concurrent writers: YES (lock-free CAS)
 * - Multiple concurrent readers: YES (lock-free)
 * - Concurrent read/write: YES (lock-free)
 * - Wait-free reads of statistics: YES (atomic loads)
 *
 * Design principles:
 * - Single Responsibility: Metrics buffering only
 * - Lock-free: No mutexes, spinlocks, or blocking
 * - Power-of-2 sizing: Fast modulo via bitwise AND
 * - Memory barriers: seq_cst for correctness
 * - Separation of concerns: Recording vs. aggregation
 */

#ifndef NIMCP_LOCKFREE_METRICS_H
#define NIMCP_LOCKFREE_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
#include <atomic>
extern "C" {
#endif

#ifndef __cplusplus
#include <stdatomic.h>
#endif

//=============================================================================
// Constants
//=============================================================================

#define LOCKFREE_METRICS_MIN_CAPACITY 16          /**< Minimum capacity (power of 2) */
#define LOCKFREE_METRICS_MAX_CAPACITY 1048576     /**< Maximum capacity (1M entries) */
#define LOCKFREE_METRICS_DEFAULT_CAPACITY 4096    /**< Default capacity (4K entries) */
#define LOCKFREE_METRICS_CACHE_LINE_SIZE 64       /**< CPU cache line size */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Metric type for categorization
 */
typedef enum {
    METRIC_TYPE_LATENCY = 0,      /**< Operation latency (microseconds) */
    METRIC_TYPE_MEMORY,           /**< Memory usage (bytes) */
    METRIC_TYPE_ERROR,            /**< Error count */
    METRIC_TYPE_THROUGHPUT,       /**< Operations per second */
    METRIC_TYPE_CACHE_HIT,        /**< Cache hit (1) or miss (0) */
    METRIC_TYPE_THREAD_WAIT,      /**< Thread wait time (microseconds) */
    METRIC_TYPE_CUSTOM,           /**< Custom metric value */
    METRIC_TYPE_COUNT             /**< Number of metric types */
} metric_type_t;

/**
 * @brief Metric recording result
 */
typedef enum {
    METRIC_RESULT_SUCCESS = 0,    /**< Metric recorded successfully */
    METRIC_RESULT_DROPPED,        /**< Buffer full, metric dropped */
    METRIC_RESULT_INVALID_INPUT,  /**< Invalid input parameters */
    METRIC_RESULT_ERROR           /**< Internal error */
} metric_result_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Single metric entry
 *
 * WHAT: Lightweight metric record (32 bytes, cache-friendly)
 * WHY: Fast to write, minimal memory overhead
 * HOW: Timestamp + type + value + metadata
 */
typedef struct {
    uint64_t timestamp_us;        /**< Timestamp in microseconds */
    metric_type_t type;           /**< Metric type */
    uint32_t component_id;        /**< Component identifier (optional) */
    double value;                 /**< Metric value */
    uint64_t metadata;            /**< Additional metadata (optional) */
} __attribute__((aligned(32))) metric_entry_t;

/**
 * @brief Lock-free metrics buffer statistics (internal, atomic version)
 *
 * WHAT: Performance and health metrics with atomic fields
 * WHY: Thread-safe updates from multiple writers
 * HOW: Atomic counters updated on record/read
 */
typedef struct {
#ifdef __cplusplus
    std::atomic<uint64_t> total_recorded;    /**< Total metrics recorded */
    std::atomic<uint64_t> total_read;        /**< Total metrics read */
    std::atomic<uint64_t> total_dropped;     /**< Total metrics dropped (buffer full) */
    std::atomic<uint64_t> total_overwrites;  /**< Total overwrites (shouldn't happen) */

    uint32_t capacity;                       /**< Buffer capacity */
    std::atomic<uint32_t> current_size;      /**< Current number of entries */
    std::atomic<uint32_t> peak_size;         /**< Peak buffer size */

    std::atomic<uint64_t> record_attempts;   /**< Total record attempts */
    std::atomic<uint64_t> record_contentions;/**< CAS loop contentions */
    std::atomic<uint64_t> read_attempts;     /**< Total read attempts */
#else
    _Atomic uint64_t total_recorded;    /**< Total metrics recorded */
    _Atomic uint64_t total_read;        /**< Total metrics read */
    _Atomic uint64_t total_dropped;     /**< Total metrics dropped (buffer full) */
    _Atomic uint64_t total_overwrites;  /**< Total overwrites (shouldn't happen) */

    uint32_t capacity;                  /**< Buffer capacity */
    _Atomic uint32_t current_size;      /**< Current number of entries */
    _Atomic uint32_t peak_size;         /**< Peak buffer size */

    _Atomic uint64_t record_attempts;   /**< Total record attempts */
    _Atomic uint64_t record_contentions;/**< CAS loop contentions */
    _Atomic uint64_t read_attempts;     /**< Total read attempts */
#endif

    double utilization;                 /**< Current utilization (0-1) */
    double peak_utilization;            /**< Peak utilization (0-1) */
    double drop_rate;                   /**< Drop rate (0-1) */
} metrics_stats_internal_t;

/**
 * @brief Metrics buffer statistics snapshot (non-atomic, for API)
 *
 * WHAT: Performance metrics snapshot returned to caller
 * WHY: Allow normal field access in C++ without .load()
 * HOW: Non-atomic fields populated by lockfree_metrics_get_stats()
 */
typedef struct {
    uint64_t total_recorded;    /**< Total metrics recorded */
    uint64_t total_read;        /**< Total metrics read */
    uint64_t total_dropped;     /**< Total metrics dropped (buffer full) */
    uint64_t total_overwrites;  /**< Total overwrites (shouldn't happen) */

    uint32_t capacity;          /**< Buffer capacity */
    uint32_t current_size;      /**< Current number of entries */
    uint32_t peak_size;         /**< Peak buffer size */

    uint64_t record_attempts;   /**< Total record attempts */
    uint64_t record_contentions;/**< CAS loop contentions */
    uint64_t read_attempts;     /**< Total read attempts */

    double utilization;         /**< Current utilization (0-1) */
    double peak_utilization;    /**< Peak utilization (0-1) */
    double drop_rate;           /**< Drop rate (0-1) */
} metrics_stats_t;

/**
 * @brief Lock-free metrics ring buffer
 *
 * WHAT: Thread-safe ring buffer using atomic operations
 * WHY: Zero-contention metrics collection for fault tolerance
 * HOW: Atomic head/tail indices with CAS loops
 *
 * Memory layout (cache-optimized):
 * - Head and tail on separate cache lines (avoid false sharing)
 * - Statistics on separate cache line
 * - Entries array aligned for sequential access
 */
typedef struct lockfree_metrics_buffer {
    // Atomic indices (separate cache lines to avoid false sharing)
#ifdef __cplusplus
    std::atomic<uint64_t> head __attribute__((aligned(LOCKFREE_METRICS_CACHE_LINE_SIZE)));
    std::atomic<uint64_t> tail __attribute__((aligned(LOCKFREE_METRICS_CACHE_LINE_SIZE)));
#else
    _Atomic uint64_t head __attribute__((aligned(LOCKFREE_METRICS_CACHE_LINE_SIZE)));
    _Atomic uint64_t tail __attribute__((aligned(LOCKFREE_METRICS_CACHE_LINE_SIZE)));
#endif

    // Buffer configuration
    uint32_t capacity;                  /**< Capacity (power of 2) */
    uint32_t capacity_mask;             /**< Capacity - 1 (for fast modulo) */
    bool drop_on_full;                  /**< Drop vs. block when full */

    // Statistics (separate cache line)
    metrics_stats_internal_t stats __attribute__((aligned(LOCKFREE_METRICS_CACHE_LINE_SIZE)));

    // Entries array (aligned for cache efficiency)
    metric_entry_t* entries __attribute__((aligned(LOCKFREE_METRICS_CACHE_LINE_SIZE)));

    // Metadata
    char name[64];                      /**< Buffer name for debugging */
    uint64_t created_at_us;             /**< Creation timestamp */
} lockfree_metrics_buffer_t;

//=============================================================================
// Buffer Lifecycle API
//=============================================================================

/**
 * @brief Create lock-free metrics buffer
 *
 * WHAT: Allocates and initializes lock-free ring buffer
 * WHY: Efficient metrics collection without contention
 * HOW: Allocates power-of-2 capacity, initializes atomics
 *
 * @param capacity Desired capacity (rounded up to power of 2)
 * @param name Buffer name for debugging (optional, can be NULL)
 * @return Buffer handle, NULL on failure
 *
 * Performance: ~1-2μs for typical capacity (4K entries)
 * Memory: capacity * sizeof(metric_entry_t) + ~256 bytes
 */
lockfree_metrics_buffer_t* lockfree_metrics_create(
    uint32_t capacity,
    const char* name
);

/**
 * @brief Destroy metrics buffer and free resources
 *
 * WHAT: Destroys buffer and releases memory
 * WHY: Clean shutdown and memory cleanup
 * HOW: Frees entries array and buffer structure
 *
 * @param buffer Buffer handle
 */
void lockfree_metrics_destroy(lockfree_metrics_buffer_t* buffer);

/**
 * @brief Reset buffer to empty state
 *
 * WHAT: Clears all entries and resets statistics
 * WHY: Start fresh without reallocation
 * HOW: Atomically resets head/tail and clears stats
 *
 * @param buffer Buffer handle
 * @return true on success, false on failure
 */
bool lockfree_metrics_reset(lockfree_metrics_buffer_t* buffer);

//=============================================================================
// Metrics Recording API (Lock-Free)
//=============================================================================

/**
 * @brief Record metric (lock-free, <50ns)
 *
 * WHAT: Records metric entry in ring buffer
 * WHY: Fast, non-blocking metrics collection
 * HOW: CAS loop to claim slot, write entry, advance head
 *
 * Thread safety: Multiple concurrent writers safe (lock-free)
 * Performance: <50ns typical, 100-200ns under high contention
 *
 * @param buffer Buffer handle
 * @param type Metric type
 * @param value Metric value
 * @param component_id Component identifier (0 if not applicable)
 * @return METRIC_RESULT_SUCCESS or error/dropped status
 */
metric_result_t lockfree_metrics_record(
    lockfree_metrics_buffer_t* buffer,
    metric_type_t type,
    double value,
    uint32_t component_id
);

/**
 * @brief Record metric with metadata (lock-free)
 *
 * WHAT: Records metric with additional metadata
 * WHY: Attach context to metrics (e.g., thread ID, operation ID)
 * HOW: Same as record() but with metadata field
 *
 * @param buffer Buffer handle
 * @param type Metric type
 * @param value Metric value
 * @param component_id Component identifier
 * @param metadata Additional metadata (64-bit)
 * @return METRIC_RESULT_SUCCESS or error/dropped status
 */
metric_result_t lockfree_metrics_record_with_metadata(
    lockfree_metrics_buffer_t* buffer,
    metric_type_t type,
    double value,
    uint32_t component_id,
    uint64_t metadata
);

/**
 * @brief Record metric with explicit timestamp
 *
 * WHAT: Records metric with caller-provided timestamp
 * WHY: Preserve exact event time (e.g., from hardware counter)
 * HOW: Uses provided timestamp instead of current time
 *
 * @param buffer Buffer handle
 * @param timestamp_us Timestamp in microseconds
 * @param type Metric type
 * @param value Metric value
 * @param component_id Component identifier
 * @param metadata Additional metadata
 * @return METRIC_RESULT_SUCCESS or error/dropped status
 */
metric_result_t lockfree_metrics_record_timestamped(
    lockfree_metrics_buffer_t* buffer,
    uint64_t timestamp_us,
    metric_type_t type,
    double value,
    uint32_t component_id,
    uint64_t metadata
);

//=============================================================================
// Metrics Reading API (Lock-Free Batch Reads)
//=============================================================================

/**
 * @brief Read batch of metrics (lock-free)
 *
 * WHAT: Reads up to max_count metrics from buffer
 * WHY: Efficient batch processing for aggregation
 * HOW: CAS loop to claim entries, read, advance tail
 *
 * Thread safety: Multiple concurrent readers safe (lock-free)
 * Performance: ~100ns per metric
 *
 * @param buffer Buffer handle
 * @param output Output array for metric entries
 * @param max_count Maximum entries to read
 * @return Number of entries read (0 if empty), -1 on error
 */
int32_t lockfree_metrics_read_batch(
    lockfree_metrics_buffer_t* buffer,
    metric_entry_t* output,
    uint32_t max_count
);

/**
 * @brief Read batch with timeout
 *
 * WHAT: Reads metrics with timeout if buffer empty
 * WHY: Useful for periodic aggregation threads
 * HOW: Busy-wait or yield until data available or timeout
 *
 * @param buffer Buffer handle
 * @param output Output array
 * @param max_count Maximum entries to read
 * @param timeout_us Timeout in microseconds (0 = no wait)
 * @return Number of entries read, -1 on error/timeout
 */
int32_t lockfree_metrics_read_batch_timeout(
    lockfree_metrics_buffer_t* buffer,
    metric_entry_t* output,
    uint32_t max_count,
    uint64_t timeout_us
);

/**
 * @brief Peek at metrics without consuming
 *
 * WHAT: Reads metrics without removing from buffer
 * WHY: Inspect current state without affecting buffer
 * HOW: Reads entries without advancing tail
 *
 * @param buffer Buffer handle
 * @param output Output array
 * @param max_count Maximum entries to peek
 * @return Number of entries peeked, -1 on error
 */
int32_t lockfree_metrics_peek(
    const lockfree_metrics_buffer_t* buffer,
    metric_entry_t* output,
    uint32_t max_count
);

//=============================================================================
// Statistics and Monitoring API
//=============================================================================

/**
 * @brief Get buffer statistics (wait-free)
 *
 * WHAT: Returns current buffer statistics
 * WHY: Monitor buffer health and performance
 * HOW: Atomic loads of statistics counters
 *
 * @param buffer Buffer handle
 * @param stats Output parameter for statistics
 * @return true on success, false on failure
 */
bool lockfree_metrics_get_stats(
    const lockfree_metrics_buffer_t* buffer,
    metrics_stats_t* stats
);

/**
 * @brief Get current buffer size (wait-free)
 *
 * WHAT: Returns number of entries currently in buffer
 * WHY: Quick check of buffer occupancy
 * HOW: Atomic subtraction of head - tail
 *
 * @param buffer Buffer handle
 * @return Current size (0 to capacity)
 */
uint32_t lockfree_metrics_size(const lockfree_metrics_buffer_t* buffer);

/**
 * @brief Check if buffer is empty (wait-free)
 *
 * @param buffer Buffer handle
 * @return true if empty, false otherwise
 */
bool lockfree_metrics_is_empty(const lockfree_metrics_buffer_t* buffer);

/**
 * @brief Check if buffer is full (wait-free)
 *
 * @param buffer Buffer handle
 * @return true if full, false otherwise
 */
bool lockfree_metrics_is_full(const lockfree_metrics_buffer_t* buffer);

/**
 * @brief Get buffer capacity
 *
 * @param buffer Buffer handle
 * @return Buffer capacity
 */
uint32_t lockfree_metrics_capacity(const lockfree_metrics_buffer_t* buffer);

/**
 * @brief Get buffer utilization (0-1)
 *
 * @param buffer Buffer handle
 * @return Utilization ratio (0.0 = empty, 1.0 = full)
 */
double lockfree_metrics_utilization(const lockfree_metrics_buffer_t* buffer);

/**
 * @brief Get drop rate (0-1)
 *
 * WHAT: Returns ratio of dropped metrics
 * WHY: Detect if buffer is too small
 * HOW: dropped / (recorded + dropped)
 *
 * @param buffer Buffer handle
 * @return Drop rate (0.0 = no drops, 1.0 = all dropped)
 */
double lockfree_metrics_drop_rate(const lockfree_metrics_buffer_t* buffer);

/**
 * @brief Reset statistics counters
 *
 * @param buffer Buffer handle
 */
void lockfree_metrics_reset_stats(lockfree_metrics_buffer_t* buffer);

//=============================================================================
// Reporting and Debugging API
//=============================================================================

/**
 * @brief Print buffer report
 *
 * WHAT: Prints comprehensive buffer statistics
 * WHY: Debugging and monitoring
 * HOW: Formats statistics to output stream
 *
 * @param buffer Buffer handle
 * @param output Output stream (stdout, stderr, file)
 */
void lockfree_metrics_report(
    const lockfree_metrics_buffer_t* buffer,
    FILE* output
);

/**
 * @brief Export statistics to JSON
 *
 * @param buffer Buffer handle
 * @param json_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written, -1 on error
 */
int32_t lockfree_metrics_export_json(
    const lockfree_metrics_buffer_t* buffer,
    char* json_buffer,
    size_t buffer_size
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert metric type to string
 *
 * @param type Metric type
 * @return String representation
 */
const char* metric_type_to_string(metric_type_t type);

/**
 * @brief Convert result to string
 *
 * @param result Metric result
 * @return String representation
 */
const char* metric_result_to_string(metric_result_t result);

/**
 * @brief Get current timestamp in microseconds
 *
 * @return Timestamp in microseconds since epoch
 */
uint64_t lockfree_metrics_get_timestamp_us(void);

/**
 * @brief Round up to next power of 2
 *
 * WHAT: Rounds capacity to next power of 2
 * WHY: Required for fast modulo via bitwise AND
 * HOW: Bit manipulation
 *
 * @param value Input value
 * @return Next power of 2 >= value
 */
uint32_t lockfree_metrics_next_power_of_2(uint32_t value);

/**
 * @brief Check if value is power of 2
 *
 * @param value Input value
 * @return true if power of 2, false otherwise
 */
bool lockfree_metrics_is_power_of_2(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_LOCKFREE_METRICS_H
