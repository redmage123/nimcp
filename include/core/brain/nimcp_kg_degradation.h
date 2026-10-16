/**
 * @file nimcp_kg_degradation.h
 * @brief Graceful Degradation for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Circuit breaker pattern with write buffering and cache fallback for KG
 * WHY:  Ensure brain resilience when KG storage becomes unavailable or slow
 * HOW:  Multi-level degradation with automatic recovery and buffered operations
 *
 * DEGRADATION LEVELS:
 * ```
 * +-------------------------------------------------------------------+
 * |                     KG DEGRADATION LEVELS                         |
 * +-------------------------------------------------------------------+
 * |  Level 0: NONE        | Full functionality, normal operation      |
 * +-------------------------------------------------------------------+
 * |  Level 1: READ_ONLY   | Writes disabled, reads from cache         |
 * +-------------------------------------------------------------------+
 * |  Level 2: STALE_READS | Serving potentially stale cached data     |
 * +-------------------------------------------------------------------+
 * |  Level 3: CACHE_ONLY  | Only in-memory cache available            |
 * +-------------------------------------------------------------------+
 * |  Level 4: OFFLINE     | KG completely unavailable                 |
 * +-------------------------------------------------------------------+
 * ```
 *
 * CIRCUIT BREAKER STATE MACHINE:
 * ```
 *                  success_threshold
 *                     reached
 *                        |
 *   +---------+     +----v------+     +---------+
 *   |  CLOSED | --> | HALF_OPEN | --> |  OPEN   |
 *   +---------+     +-----------+     +---------+
 *        ^                                  |
 *        |                                  |
 *        +---------- timeout_ms ------------+
 *             (or recovery attempt)
 * ```
 *
 * BIOLOGICAL BASIS:
 * Models neural resilience patterns where the brain gracefully degrades
 * cognitive functions under metabolic stress or injury, prioritizing
 * essential functions while buffering deferred operations for recovery.
 *
 * THREAD SAFETY: All operations are thread-safe via internal mutex
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_DEGRADATION_H
#define NIMCP_KG_DEGRADATION_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum path length for disk buffer */
#define KG_DEGRADATION_MAX_PATH_LEN     256

/** Default write buffer capacity */
#define KG_DEGRADATION_DEFAULT_BUFFER_SIZE  1024

/** Default cache size in MB */
#define KG_DEGRADATION_DEFAULT_CACHE_MB     64

/** Default stale threshold in milliseconds */
#define KG_DEGRADATION_DEFAULT_STALE_MS     30000

/** Default circuit breaker timeout in milliseconds */
#define KG_DEGRADATION_DEFAULT_TIMEOUT_MS   60000

/** Default failure threshold before circuit opens */
#define KG_DEGRADATION_DEFAULT_FAILURE_THRESHOLD    5

/** Default success threshold to close circuit */
#define KG_DEGRADATION_DEFAULT_SUCCESS_THRESHOLD    3

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Degradation level
 *
 * WHAT: Severity levels for graceful degradation
 * WHY:  Allow progressive fallback as KG becomes unavailable
 * HOW:  Each level restricts more functionality while preserving essentials
 */
typedef enum {
    KG_DEGRADE_NONE = 0,             /**< Full functionality */
    KG_DEGRADE_READ_ONLY,            /**< Writes disabled, reads from cache */
    KG_DEGRADE_STALE_READS,          /**< Serving potentially stale data */
    KG_DEGRADE_CACHE_ONLY,           /**< Only in-memory cache available */
    KG_DEGRADE_OFFLINE               /**< KG unavailable */
} kg_degradation_level_t;

/**
 * @brief Circuit breaker state
 *
 * WHAT: States for circuit breaker pattern
 * WHY:  Prevent cascading failures by failing fast
 * HOW:  Track failure/success counts to transition between states
 */
typedef enum {
    KG_CIRCUIT_CLOSED = 0,           /**< Normal operation */
    KG_CIRCUIT_HALF_OPEN,            /**< Testing if service recovered */
    KG_CIRCUIT_OPEN                  /**< Service unavailable, failing fast */
} kg_circuit_state_t;

/**
 * @brief I/O request type
 *
 * WHAT: Type of buffered KG operation
 * WHY:  Classify operations for prioritization and replay
 * HOW:  Tag each request with its operation type
 */
typedef enum {
    KG_IO_TYPE_NODE_CREATE = 0,      /**< Create node operation */
    KG_IO_TYPE_NODE_UPDATE,          /**< Update node operation */
    KG_IO_TYPE_NODE_DELETE,          /**< Delete node operation */
    KG_IO_TYPE_EDGE_CREATE,          /**< Create edge operation */
    KG_IO_TYPE_EDGE_UPDATE,          /**< Update edge operation */
    KG_IO_TYPE_EDGE_DELETE,          /**< Delete edge operation */
    KG_IO_TYPE_METADATA_UPDATE,      /**< Metadata update operation */
    KG_IO_TYPE_COUNT
} kg_io_type_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/** Degradation context (opaque) */
typedef struct kg_degradation_ctx kg_degradation_ctx_t;

/* ============================================================================
 * Data Structures - I/O Request
 * ============================================================================ */

/**
 * @brief I/O request for buffered operations
 *
 * WHAT: Single buffered KG write operation
 * WHY:  Queue writes during degraded mode for later replay
 * HOW:  Store operation type, targets, and payload
 */
typedef struct {
    kg_io_type_t type;               /**< Operation type */
    brain_kg_node_id_t node_id;      /**< Target node ID (if applicable) */
    brain_kg_node_id_t edge_from;    /**< Edge source (if applicable) */
    brain_kg_node_id_t edge_to;      /**< Edge target (if applicable) */
    uint64_t timestamp;              /**< Request timestamp */
    uint32_t payload_size;           /**< Size of payload data */
    void* payload;                   /**< Operation-specific payload */
    uint32_t retry_count;            /**< Number of replay attempts */
    uint32_t priority;               /**< Request priority (higher = more urgent) */
} kg_io_request_t;

/* ============================================================================
 * Data Structures - Write Buffer
 * ============================================================================ */

/**
 * @brief Write buffer for offline queuing
 *
 * WHAT: Queue for buffered writes during outage
 * WHY:  Preserve write operations for replay on recovery
 * HOW:  In-memory buffer with optional disk spill
 */
typedef struct {
    kg_io_request_t* pending_writes; /**< Queued writes during outage */
    uint32_t write_count;            /**< Number of pending writes */
    uint32_t max_buffer_size;        /**< Maximum buffer capacity */
    uint64_t oldest_write_timestamp; /**< Oldest queued write */
    uint64_t buffer_memory_bytes;    /**< Memory used by buffer */
    bool overflow_to_disk;           /**< Spill to disk if memory full */
    char disk_buffer_path[KG_DEGRADATION_MAX_PATH_LEN]; /**< Path for disk spill */
} kg_write_buffer_t;

/* ============================================================================
 * Data Structures - Cache Configuration
 * ============================================================================ */

/**
 * @brief Read-through cache configuration
 *
 * WHAT: Configuration for degradation cache
 * WHY:  Tune cache behavior for resilience vs freshness tradeoffs
 * HOW:  Set size limits, staleness thresholds, and prefetch behavior
 */
typedef struct {
    uint32_t cache_size_mb;          /**< Maximum cache size */
    uint32_t stale_threshold_ms;     /**< Time before data considered stale */
    uint32_t max_stale_serve_ms;     /**< Maximum time to serve stale data */
    bool serve_stale_on_error;       /**< Serve stale data if DB unreachable */
    bool prefetch_on_access;         /**< Prefetch related nodes on access */
} kg_cache_config_t;

/* ============================================================================
 * Data Structures - Circuit Breaker Configuration
 * ============================================================================ */

/**
 * @brief Circuit breaker configuration
 *
 * WHAT: Configuration for circuit breaker behavior
 * WHY:  Tune failure detection and recovery sensitivity
 * HOW:  Set thresholds and timeouts for state transitions
 */
typedef struct {
    uint32_t failure_threshold;      /**< Failures before opening circuit */
    uint32_t success_threshold;      /**< Successes to close circuit */
    uint32_t timeout_ms;             /**< Time circuit stays open */
    uint32_t half_open_requests;     /**< Requests to allow in half-open */
    bool enable_adaptive_timeout;    /**< Adjust timeout based on recovery */
} kg_circuit_config_t;

/* ============================================================================
 * Data Structures - Degradation Configuration
 * ============================================================================ */

/**
 * @brief Overall degradation configuration
 *
 * WHAT: Configuration for graceful degradation system
 * WHY:  Customize degradation behavior for deployment needs
 * HOW:  Combine cache, circuit, and buffer configurations
 */
typedef struct {
    kg_cache_config_t cache;         /**< Cache configuration */
    kg_circuit_config_t circuit;     /**< Circuit breaker configuration */
    uint32_t max_buffer_size;        /**< Maximum write buffer size */
    bool enable_disk_buffer;         /**< Enable disk overflow for buffer */
    char disk_buffer_path[KG_DEGRADATION_MAX_PATH_LEN]; /**< Disk buffer path */
    bool auto_recovery;              /**< Attempt automatic recovery */
    uint32_t recovery_interval_ms;   /**< Interval between recovery attempts */
} kg_degradation_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Degradation state change callback
 *
 * WHAT: Callback invoked when degradation level changes
 * WHY:  Allow modules to adapt to changing KG availability
 * HOW:  Invoked with old/new levels and context
 *
 * @param old_level Previous degradation level
 * @param new_level New degradation level
 * @param ctx Degradation context
 * @param user_data User-provided context
 */
typedef void (*kg_degradation_callback_fn)(
    kg_degradation_level_t old_level,
    kg_degradation_level_t new_level,
    kg_degradation_ctx_t* ctx,
    void* user_data
);

/* ============================================================================
 * Data Structures - Statistics
 * ============================================================================ */

/**
 * @brief Degradation statistics
 *
 * WHAT: Runtime statistics for degradation system
 * WHY:  Monitor health and tune configuration
 * HOW:  Track cache hits, buffer usage, circuit trips
 */
typedef struct {
    uint64_t cache_hits;             /**< Cache hit count */
    uint64_t cache_misses;           /**< Cache miss count */
    uint64_t stale_serves;           /**< Stale data served count */
    uint64_t buffered_writes;        /**< Total writes buffered */
    uint64_t dropped_writes;         /**< Writes dropped (buffer full) */
    uint64_t circuit_trips;          /**< Times circuit has opened */
    uint64_t recovery_attempts;      /**< Recovery attempts made */
    uint64_t successful_recoveries;  /**< Successful recoveries */
    uint64_t total_degradation_time_ms; /**< Total time in degraded state */
} kg_degradation_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default degradation configuration
 *
 * WHAT: Populate configuration with sensible defaults
 * WHY:  Simplify initialization with good starting values
 * HOW:  Set all fields to default constants
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int kg_degradation_default_config(kg_degradation_config_t* config);

/**
 * @brief Create degradation context
 *
 * WHAT: Initialize graceful degradation system
 * WHY:  Enable resilient KG operations
 * HOW:  Allocate context, initialize buffers and cache
 *
 * @param config Configuration (NULL for defaults)
 * @return Degradation context or NULL on error
 */
kg_degradation_ctx_t* kg_degradation_create(
    const kg_degradation_config_t* config
);

/**
 * @brief Destroy degradation context
 *
 * WHAT: Clean up degradation system resources
 * WHY:  Free memory and flush pending operations
 * HOW:  Flush buffer, free cache, destroy mutex
 *
 * @param ctx Context to destroy (NULL safe)
 */
void kg_degradation_destroy(kg_degradation_ctx_t* ctx);

/* ============================================================================
 * State Query API
 * ============================================================================ */

/**
 * @brief Get current degradation level
 *
 * @param ctx Degradation context
 * @return Current degradation level
 */
kg_degradation_level_t kg_degradation_get_level(
    const kg_degradation_ctx_t* ctx
);

/**
 * @brief Get circuit breaker state
 *
 * @param ctx Degradation context
 * @return Current circuit state
 */
kg_circuit_state_t kg_degradation_get_circuit_state(
    const kg_degradation_ctx_t* ctx
);

/**
 * @brief Check if writes are currently allowed
 *
 * WHAT: Query if write operations can proceed
 * WHY:  Allow callers to check before attempting writes
 * HOW:  Check degradation level and circuit state
 *
 * @param ctx Degradation context
 * @return true if writes allowed, false otherwise
 */
bool kg_degradation_can_write(const kg_degradation_ctx_t* ctx);

/**
 * @brief Check if reads are currently allowed
 *
 * WHAT: Query if read operations can proceed
 * WHY:  Even in degraded mode, reads may be possible from cache
 * HOW:  Check degradation level (always true except OFFLINE)
 *
 * @param ctx Degradation context
 * @return true if reads allowed (possibly stale), false if offline
 */
bool kg_degradation_can_read(const kg_degradation_ctx_t* ctx);

/**
 * @brief Get degradation statistics
 *
 * @param ctx Degradation context
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int kg_degradation_get_stats(
    const kg_degradation_ctx_t* ctx,
    kg_degradation_stats_t* stats
);

/* ============================================================================
 * Write Buffer API
 * ============================================================================ */

/**
 * @brief Buffer a write operation for later replay
 *
 * WHAT: Queue a write operation during degraded mode
 * WHY:  Preserve writes for replay when KG recovers
 * HOW:  Copy request to buffer, spill to disk if needed
 *
 * @param ctx Degradation context
 * @param write Write request to buffer
 * @return 0 on success, -1 on error, -2 if buffer full
 */
int kg_degradation_buffer_write(
    kg_degradation_ctx_t* ctx,
    const kg_io_request_t* write
);

/**
 * @brief Flush buffered writes to KG
 *
 * WHAT: Replay all buffered writes to the KG
 * WHY:  Apply deferred operations after recovery
 * HOW:  Iterate buffer, apply each operation, handle failures
 *
 * @param ctx Degradation context
 * @return Number of writes flushed, or -1 on error
 */
int kg_degradation_flush_buffer(kg_degradation_ctx_t* ctx);

/**
 * @brief Get current buffer size
 *
 * @param ctx Degradation context
 * @return Number of pending writes in buffer
 */
uint32_t kg_degradation_get_buffer_size(const kg_degradation_ctx_t* ctx);

/**
 * @brief Get buffer memory usage
 *
 * @param ctx Degradation context
 * @return Memory used by buffer in bytes
 */
uint64_t kg_degradation_get_buffer_memory(const kg_degradation_ctx_t* ctx);

/**
 * @brief Discard all buffered writes
 *
 * WHAT: Clear the write buffer without flushing
 * WHY:  Reset after unrecoverable failure or timeout
 * HOW:  Free all buffered requests
 *
 * @param ctx Degradation context
 * @return Number of writes discarded, or -1 on error
 */
int kg_degradation_discard_buffer(kg_degradation_ctx_t* ctx);

/* ============================================================================
 * Circuit Breaker API
 * ============================================================================ */

/**
 * @brief Record a successful operation
 *
 * WHAT: Notify circuit breaker of successful KG operation
 * WHY:  Track success for recovery detection
 * HOW:  Increment success counter, possibly close circuit
 *
 * @param ctx Degradation context
 * @return 0 on success, -1 on error
 */
int kg_degradation_record_success(kg_degradation_ctx_t* ctx);

/**
 * @brief Record a failed operation
 *
 * WHAT: Notify circuit breaker of failed KG operation
 * WHY:  Track failures for outage detection
 * HOW:  Increment failure counter, possibly open circuit
 *
 * @param ctx Degradation context
 * @return 0 on success, -1 on error
 */
int kg_degradation_record_failure(kg_degradation_ctx_t* ctx);

/**
 * @brief Force circuit to open state
 *
 * WHAT: Manually open the circuit breaker
 * WHY:  Allow manual intervention when outage is known
 * HOW:  Set state to OPEN, start timeout
 *
 * @param ctx Degradation context
 * @return 0 on success, -1 on error
 */
int kg_degradation_force_open_circuit(kg_degradation_ctx_t* ctx);

/**
 * @brief Force circuit to closed state
 *
 * WHAT: Manually close the circuit breaker
 * WHY:  Allow manual recovery when service is known healthy
 * HOW:  Set state to CLOSED, reset counters
 *
 * @param ctx Degradation context
 * @return 0 on success, -1 on error
 */
int kg_degradation_force_close_circuit(kg_degradation_ctx_t* ctx);

/**
 * @brief Get failure count since last success
 *
 * @param ctx Degradation context
 * @return Current consecutive failure count
 */
uint32_t kg_degradation_get_failure_count(const kg_degradation_ctx_t* ctx);

/**
 * @brief Get success count since circuit opened
 *
 * @param ctx Degradation context
 * @return Current success count in half-open state
 */
uint32_t kg_degradation_get_success_count(const kg_degradation_ctx_t* ctx);

/* ============================================================================
 * Cache API
 * ============================================================================ */

/**
 * @brief Put data into degradation cache
 *
 * WHAT: Cache node data for degraded reads
 * WHY:  Enable reads when KG is unavailable
 * HOW:  Store data with timestamp for staleness tracking
 *
 * @param ctx Degradation context
 * @param id Node ID to cache
 * @param data Data to cache
 * @param size Size of data in bytes
 * @return 0 on success, -1 on error
 */
int kg_degradation_cache_put(
    kg_degradation_ctx_t* ctx,
    brain_kg_node_id_t id,
    const void* data,
    size_t size
);

/**
 * @brief Get data from degradation cache
 *
 * WHAT: Retrieve cached node data
 * WHY:  Serve reads during degraded operation
 * HOW:  Look up by ID, check staleness, return data
 *
 * @param ctx Degradation context
 * @param id Node ID to retrieve
 * @param data Output data pointer (caller must free)
 * @param size Output data size
 * @param is_stale Output flag indicating if data is stale
 * @return 0 on success, -1 if not found, -2 on error
 */
int kg_degradation_cache_get(
    const kg_degradation_ctx_t* ctx,
    brain_kg_node_id_t id,
    void** data,
    size_t* size,
    bool* is_stale
);

/**
 * @brief Invalidate a cache entry
 *
 * WHAT: Remove a node from the cache
 * WHY:  Clear stale data after known update
 * HOW:  Remove entry by ID
 *
 * @param ctx Degradation context
 * @param id Node ID to invalidate
 * @return 0 on success, -1 if not found
 */
int kg_degradation_cache_invalidate(
    kg_degradation_ctx_t* ctx,
    brain_kg_node_id_t id
);

/**
 * @brief Clear all cache entries
 *
 * WHAT: Remove all cached data
 * WHY:  Reset cache after schema change or corruption
 * HOW:  Free all entries
 *
 * @param ctx Degradation context
 * @return Number of entries cleared, or -1 on error
 */
int kg_degradation_cache_clear(kg_degradation_ctx_t* ctx);

/**
 * @brief Get cache statistics
 *
 * @param ctx Degradation context
 * @param entries Output: number of cached entries
 * @param memory_bytes Output: memory used by cache
 * @return 0 on success, -1 on error
 */
int kg_degradation_cache_stats(
    const kg_degradation_ctx_t* ctx,
    uint32_t* entries,
    uint64_t* memory_bytes
);

/* ============================================================================
 * Recovery API
 * ============================================================================ */

/**
 * @brief Attempt to recover from degraded state
 *
 * WHAT: Try to restore normal KG operation
 * WHY:  Automatic or manual recovery initiation
 * HOW:  Test KG connectivity, transition circuit state
 *
 * @param ctx Degradation context
 * @return 0 if recovered, -1 if still degraded, -2 on error
 */
int kg_degradation_attempt_recovery(kg_degradation_ctx_t* ctx);

/**
 * @brief Sync buffered writes to database
 *
 * WHAT: Flush buffer and verify writes applied
 * WHY:  Ensure consistency after recovery
 * HOW:  Flush buffer, verify each write
 *
 * @param ctx Degradation context
 * @return Number of writes synced, or -1 on error
 */
int kg_degradation_sync_buffer_to_db(kg_degradation_ctx_t* ctx);

/**
 * @brief Set degradation level manually
 *
 * WHAT: Force a specific degradation level
 * WHY:  Allow external control for testing or manual intervention
 * HOW:  Set level, invoke callbacks
 *
 * @param ctx Degradation context
 * @param level New degradation level
 * @return 0 on success, -1 on error
 */
int kg_degradation_set_level(
    kg_degradation_ctx_t* ctx,
    kg_degradation_level_t level
);

/* ============================================================================
 * Callback API
 * ============================================================================ */

/**
 * @brief Register degradation state change callback
 *
 * WHAT: Register callback for degradation level changes
 * WHY:  Allow modules to react to KG availability changes
 * HOW:  Add to callback list, invoke on state change
 *
 * @param ctx Degradation context
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @return 0 on success, -1 if max callbacks reached
 */
int kg_degradation_register_callback(
    kg_degradation_ctx_t* ctx,
    kg_degradation_callback_fn callback,
    void* user_data
);

/**
 * @brief Unregister degradation callback
 *
 * @param ctx Degradation context
 * @param callback Callback to remove
 * @return 0 on success, -1 if not found
 */
int kg_degradation_unregister_callback(
    kg_degradation_ctx_t* ctx,
    kg_degradation_callback_fn callback
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Convert degradation level to string
 *
 * @param level Degradation level
 * @return String representation (static, do not free)
 */
const char* kg_degradation_level_to_string(kg_degradation_level_t level);

/**
 * @brief Convert circuit state to string
 *
 * @param state Circuit state
 * @return String representation (static, do not free)
 */
const char* kg_circuit_state_to_string(kg_circuit_state_t state);

/**
 * @brief Convert I/O request type to string
 *
 * @param type I/O request type
 * @return String representation (static, do not free)
 */
const char* kg_io_type_to_string(kg_io_type_t type);

/**
 * @brief Free an I/O request's payload
 *
 * WHAT: Free the payload memory of an I/O request
 * WHY:  Proper cleanup of buffered request data
 * HOW:  Free payload if non-NULL
 *
 * @param request Request to free payload from
 */
void kg_io_request_free_payload(kg_io_request_t* request);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_DEGRADATION_H */
