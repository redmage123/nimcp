/**
 * @file nimcp_kg_ratelimit.h
 * @brief Rate Limiting and Quotas for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Rate limiting and quota management for KG operations
 * WHY:  Prevent resource exhaustion, ensure fair access, protect system stability
 * HOW:  Multiple algorithms (token bucket, sliding window, etc.) with per-module quotas
 *
 * ARCHITECTURE:
 * ```
 * +=====================================================================+
 * |                    KG RATE LIMITING SYSTEM                          |
 * +=====================================================================+
 * |                                                                     |
 * |   +-----------------+     +-----------------+     +---------------+ |
 * |   |  Token Bucket   |     | Sliding Window  |     | Fixed Window  | |
 * |   |  Algorithm      |     | Algorithm       |     | Algorithm     | |
 * |   +-----------------+     +-----------------+     +---------------+ |
 * |          |                       |                      |          |
 * |          +----------+------------+----------+-----------+          |
 * |                     |                       |                      |
 * |                     v                       v                      |
 * |              +-------------+         +-------------+               |
 * |              | Rate Checks |         |   Quotas    |               |
 * |              +-------------+         +-------------+               |
 * |                     |                       |                      |
 * |          +----------+-----------+-----------+----------+           |
 * |          |          |           |           |          |           |
 * |          v          v           v           v          v           |
 * |      +------+   +------+   +--------+   +------+   +------+        |
 * |      | READ |   | WRITE|   | QUERY  |   |TRAVERSE| | ALL  |        |
 * |      +------+   +------+   +--------+   +------+   +------+        |
 * |                                                                    |
 * |   SCOPES:  Global | Per-Module | Per-Layer | Per-Hemisphere        |
 * +====================================================================+
 * ```
 *
 * BIOLOGICAL BASIS:
 * - Neural systems have metabolic limits on activity rates
 * - Refractory periods prevent neurons from firing too rapidly
 * - Energy budgets constrain sustained high-frequency processing
 * - Different brain regions have different metabolic demands
 *
 * USAGE:
 * ```c
 * // Create rate limiter with token bucket algorithm
 * kg_ratelimit_config_t config = {
 *     .algorithm = KG_RATELIMIT_TOKEN_BUCKET,
 *     .scope = KG_SCOPE_PER_MODULE,
 *     .operations = KG_OP_READ | KG_OP_WRITE,
 *     .rate_per_second = 1000,
 *     .burst_capacity = 100,
 *     .window_size_ms = 1000
 * };
 * kg_ratelimiter_t* limiter = kg_ratelimit_create(&config);
 *
 * // Check if operation is allowed
 * kg_ratelimit_result_t result = kg_ratelimit_check(limiter, "module_a", KG_OP_READ);
 * if (result.allowed) {
 *     // Proceed with operation
 * } else {
 *     // Wait or reject: result.wait_time_ms indicates how long to wait
 * }
 *
 * // Set quota for a module
 * kg_quota_config_t quota = {
 *     .module_name = "module_a",
 *     .max_nodes = 10000,
 *     .max_edges = 50000,
 *     .max_storage_bytes = 1024 * 1024 * 100  // 100MB
 * };
 * kg_quota_set(limiter, &quota);
 *
 * kg_ratelimit_destroy(limiter);
 * ```
 *
 * THREAD SAFETY: All operations are thread-safe via internal mutex
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_RATELIMIT_H
#define NIMCP_KG_RATELIMIT_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum module name length for quota configuration */
#define KG_RATELIMIT_MAX_MODULE_NAME    64

/** Maximum number of quotas that can be configured */
#define KG_RATELIMIT_MAX_QUOTAS         256

/** Maximum priority level */
#define KG_RATELIMIT_MAX_PRIORITY       255

/** Default burst capacity multiplier */
#define KG_RATELIMIT_DEFAULT_BURST_MULT 10

/** Default window size in milliseconds */
#define KG_RATELIMIT_DEFAULT_WINDOW_MS  1000

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Rate limit algorithm
 *
 * WHAT: Algorithm used for rate limiting calculations
 * WHY:  Different algorithms suit different use cases
 * HOW:  Each algorithm has unique characteristics for burst handling
 *
 * - TOKEN_BUCKET: Smooth rate limiting with burst allowance
 * - SLIDING_WINDOW: Rolling counter for accurate rate tracking
 * - FIXED_WINDOW: Simple counter reset at fixed intervals
 * - LEAKY_BUCKET: Constant output rate regardless of input bursts
 */
typedef enum {
    KG_RATELIMIT_TOKEN_BUCKET = 0,   /**< Smooth rate limiting with burst allowance */
    KG_RATELIMIT_SLIDING_WINDOW,      /**< Rolling window counter */
    KG_RATELIMIT_FIXED_WINDOW,        /**< Fixed time window counter */
    KG_RATELIMIT_LEAKY_BUCKET         /**< Constant output rate */
} kg_ratelimit_algo_t;

/**
 * @brief Rate limit scope
 *
 * WHAT: Scope at which rate limits are applied
 * WHY:  Different granularity for different protection needs
 * HOW:  Limits can be system-wide or scoped to specific entities
 */
typedef enum {
    KG_SCOPE_GLOBAL = 0,             /**< System-wide limit */
    KG_SCOPE_PER_MODULE,             /**< Per-module limit */
    KG_SCOPE_PER_LAYER,              /**< Per-layer limit (cortical layers) */
    KG_SCOPE_PER_HEMISPHERE          /**< Per-hemisphere limit */
} kg_ratelimit_scope_t;

/**
 * @brief Operation type for rate limiting
 *
 * WHAT: Types of operations that can be rate-limited
 * WHY:  Different operations have different resource impacts
 * HOW:  Bitmask allows combining multiple operation types
 */
typedef enum {
    KG_OP_READ     = 1 << 0,         /**< Read operations (get node, get edge) */
    KG_OP_WRITE    = 1 << 1,         /**< Write operations (add, update, delete) */
    KG_OP_QUERY    = 1 << 2,         /**< Query operations (search, filter) */
    KG_OP_TRAVERSE = 1 << 3,         /**< Traversal operations (path finding, BFS/DFS) */
    KG_OP_ALL      = 0xFFFFFFFF      /**< All operation types */
} kg_operation_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Rate limit configuration
 *
 * WHAT: Configuration for rate limiter behavior
 * WHY:  Allows customization of rate limiting parameters
 * HOW:  Algorithm, scope, and timing parameters define behavior
 */
typedef struct {
    kg_ratelimit_algo_t algorithm;   /**< Rate limiting algorithm to use */
    kg_ratelimit_scope_t scope;      /**< Scope of rate limiting */
    uint32_t operations;             /**< Bitmask of operation types (kg_operation_type_t) */
    uint64_t rate_per_second;        /**< Maximum operations per second */
    uint64_t burst_capacity;         /**< Maximum burst size (token bucket/leaky bucket) */
    uint64_t window_size_ms;         /**< Window size for windowed algorithms */
} kg_ratelimit_config_t;

/**
 * @brief Quota configuration
 *
 * WHAT: Resource quotas for a module or globally
 * WHY:  Prevent any single module from consuming excessive resources
 * HOW:  Hard limits on various resource dimensions
 */
typedef struct {
    char module_name[KG_RATELIMIT_MAX_MODULE_NAME]; /**< Module name (empty = global) */
    uint64_t max_nodes;              /**< Maximum nodes this module can create */
    uint64_t max_edges;              /**< Maximum edges this module can create */
    uint64_t max_storage_bytes;      /**< Maximum storage usage in bytes */
    uint64_t max_query_time_ms;      /**< Maximum query execution time */
    uint64_t max_result_rows;        /**< Maximum rows per query result */
    uint64_t max_connections;        /**< Maximum concurrent connections */
} kg_quota_config_t;

/**
 * @brief Rate limit result
 *
 * WHAT: Result of a rate limit check or acquisition
 * WHY:  Provides information for clients to handle rate limiting
 * HOW:  Contains decision and timing information
 */
typedef struct {
    bool allowed;                    /**< Was the operation allowed? */
    uint64_t wait_time_ms;           /**< Time to wait if not allowed (ms) */
    uint64_t remaining_tokens;       /**< Remaining capacity (tokens/requests) */
    uint64_t reset_time_ms;          /**< When limit resets (Unix timestamp ms) */
} kg_ratelimit_result_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Rate limiter handle (opaque)
 */
typedef struct kg_ratelimiter kg_ratelimiter_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create a new rate limiter
 *
 * WHAT: Allocate and initialize a rate limiter instance
 * WHY:  Entry point for rate limiting functionality
 * HOW:  Allocates internal structures based on configuration
 *
 * @param config Rate limit configuration (NULL for defaults)
 * @return Rate limiter handle or NULL on error
 *
 * @note Caller must call kg_ratelimit_destroy() when done
 * @note Default config uses token bucket, global scope, 1000 ops/sec
 */
kg_ratelimiter_t* kg_ratelimit_create(const kg_ratelimit_config_t* config);

/**
 * @brief Destroy a rate limiter
 *
 * WHAT: Free all resources associated with rate limiter
 * WHY:  Proper cleanup to prevent memory leaks
 * HOW:  Frees internal structures and limiter itself
 *
 * @param limiter Rate limiter to destroy (NULL safe)
 */
void kg_ratelimit_destroy(kg_ratelimiter_t* limiter);

/**
 * @brief Get default rate limit configuration
 *
 * WHAT: Populate configuration with sensible defaults
 * WHY:  Convenience for common use cases
 * HOW:  Sets token bucket algorithm with standard parameters
 *
 * @param config Configuration to populate
 * @return 0 on success, -1 on error (NULL config)
 */
int kg_ratelimit_default_config(kg_ratelimit_config_t* config);

/* ============================================================================
 * Rate Limiting Operations
 * ============================================================================ */

/**
 * @brief Check if an operation would be allowed
 *
 * WHAT: Check rate limit without consuming tokens
 * WHY:  Allows clients to check before attempting operation
 * HOW:  Evaluates current state against limits without modification
 *
 * @param limiter Rate limiter handle
 * @param module_name Module requesting check (NULL for global)
 * @param op Operation type being checked
 * @return Rate limit result with allowed status and wait time
 *
 * @note Does NOT consume tokens - use kg_ratelimit_acquire() for that
 */
kg_ratelimit_result_t kg_ratelimit_check(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_operation_type_t op
);

/**
 * @brief Acquire permission for an operation
 *
 * WHAT: Request and consume a token for an operation
 * WHY:  Primary interface for rate-limited operation execution
 * HOW:  Checks limit and consumes token if allowed
 *
 * @param limiter Rate limiter handle
 * @param module_name Module requesting operation (NULL for global)
 * @param op Operation type being requested
 * @return Rate limit result with allowed status and wait time
 *
 * @note Consumes token if allowed - use kg_ratelimit_check() for read-only check
 */
kg_ratelimit_result_t kg_ratelimit_acquire(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_operation_type_t op
);

/**
 * @brief Release a previously acquired token
 *
 * WHAT: Return an unused token to the pool
 * WHY:  Allows correction if operation was not actually performed
 * HOW:  Adds token back to bucket/counter
 *
 * @param limiter Rate limiter handle
 * @param module_name Module returning token (NULL for global)
 * @param op Operation type being released
 * @return 0 on success, -1 on error
 *
 * @note Only call if acquire was called but operation was not performed
 */
int kg_ratelimit_release(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_operation_type_t op
);

/**
 * @brief Reset rate limiter state
 *
 * WHAT: Reset all counters and tokens to initial state
 * WHY:  Recovery from error conditions or configuration changes
 * HOW:  Reinitializes internal state based on configuration
 *
 * @param limiter Rate limiter handle
 * @return 0 on success, -1 on error
 */
int kg_ratelimit_reset(kg_ratelimiter_t* limiter);

/**
 * @brief Update rate limiter configuration
 *
 * WHAT: Update rate limiting parameters without recreation
 * WHY:  Dynamic adjustment based on system load
 * HOW:  Updates internal parameters and optionally resets counters
 *
 * @param limiter Rate limiter handle
 * @param config New configuration
 * @param reset_counters If true, reset counters after config update
 * @return 0 on success, -1 on error
 */
int kg_ratelimit_update_config(
    kg_ratelimiter_t* limiter,
    const kg_ratelimit_config_t* config,
    bool reset_counters
);

/* ============================================================================
 * Quota Management
 * ============================================================================ */

/**
 * @brief Set quota for a module
 *
 * WHAT: Configure resource limits for a specific module
 * WHY:  Prevent individual modules from consuming excessive resources
 * HOW:  Stores quota and enforces during subsequent operations
 *
 * @param limiter Rate limiter handle
 * @param quota Quota configuration to set
 * @return 0 on success, -1 on error
 *
 * @note Empty module_name sets global quota
 * @note Existing quota for same module is replaced
 */
int kg_quota_set(
    kg_ratelimiter_t* limiter,
    const kg_quota_config_t* quota
);

/**
 * @brief Get quota configuration for a module
 *
 * WHAT: Retrieve current quota settings
 * WHY:  Allow inspection of configured limits
 * HOW:  Copies quota configuration to output parameter
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to query (NULL or empty for global)
 * @param quota Output quota configuration
 * @return 0 on success, -1 if not found or error
 */
int kg_quota_get(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_quota_config_t* quota
);

/**
 * @brief Get current usage against quota
 *
 * WHAT: Retrieve current resource usage for a module
 * WHY:  Monitor how close modules are to their limits
 * HOW:  Populates quota structure with current usage values
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to query (NULL or empty for global)
 * @param usage Output usage values (same structure as quota)
 * @return 0 on success, -1 if not found or error
 *
 * @note Usage values use same fields as kg_quota_config_t
 */
int kg_quota_get_usage(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_quota_config_t* usage
);

/**
 * @brief Check if operation is within quota
 *
 * WHAT: Check if operation would exceed quota limits
 * WHY:  Pre-check before resource-intensive operations
 * HOW:  Compares current usage + amount against quota
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to check (NULL or empty for global)
 * @param op Operation type being checked
 * @param amount Resource amount being requested
 * @return true if within quota, false if would exceed
 */
bool kg_quota_check(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_operation_type_t op,
    uint64_t amount
);

/**
 * @brief Remove quota for a module
 *
 * WHAT: Delete quota configuration for a module
 * WHY:  Clean up quotas for removed modules
 * HOW:  Removes quota entry from internal storage
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to remove quota for
 * @return 0 on success, -1 if not found or error
 */
int kg_quota_remove(
    kg_ratelimiter_t* limiter,
    const char* module_name
);

/**
 * @brief Reset usage counters for a module
 *
 * WHAT: Reset accumulated usage to zero
 * WHY:  Start fresh accounting period
 * HOW:  Zeros usage counters while preserving quota limits
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to reset (NULL for all modules)
 * @return 0 on success, -1 on error
 */
int kg_quota_reset_usage(
    kg_ratelimiter_t* limiter,
    const char* module_name
);

/* ============================================================================
 * Priority Lanes
 * ============================================================================ */

/**
 * @brief Set priority level for a module
 *
 * WHAT: Assign priority level affecting rate limit behavior
 * WHY:  Critical modules may need higher throughput
 * HOW:  Higher priority modules get preferential treatment
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to set priority for
 * @param priority Priority level (0 = lowest, 255 = highest)
 * @return 0 on success, -1 on error
 *
 * @note Higher priority modules are less likely to be throttled
 * @note Default priority is 128 (middle)
 */
int kg_ratelimit_set_priority(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    uint32_t priority
);

/**
 * @brief Get priority level for a module
 *
 * WHAT: Retrieve current priority setting
 * WHY:  Allow inspection of priority configuration
 * HOW:  Returns currently assigned priority level
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to query
 * @param priority Output priority level
 * @return 0 on success, -1 if not found or error
 */
int kg_ratelimit_get_priority(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    uint32_t* priority
);

/**
 * @brief Reserve bandwidth percentage for a module
 *
 * WHAT: Guarantee minimum throughput for a module
 * WHY:  Ensure critical modules have dedicated capacity
 * HOW:  Reserves portion of total rate limit for module
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to reserve bandwidth for
 * @param percent Percentage of total bandwidth to reserve (0.0-100.0)
 * @return 0 on success, -1 on error
 *
 * @note Total reserved bandwidth cannot exceed 100%
 * @note Unreserved bandwidth is shared among all modules
 */
int kg_ratelimit_reserve_bandwidth(
    kg_ratelimiter_t* limiter,
    const char* module_name,
    float percent
);

/**
 * @brief Get reserved bandwidth for a module
 *
 * WHAT: Retrieve bandwidth reservation
 * WHY:  Allow inspection of bandwidth allocation
 * HOW:  Returns currently reserved percentage
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to query
 * @param percent Output reserved percentage
 * @return 0 on success, -1 if not found or error
 */
int kg_ratelimit_get_reserved_bandwidth(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    float* percent
);

/**
 * @brief Release bandwidth reservation for a module
 *
 * WHAT: Remove bandwidth reservation
 * WHY:  Return reserved capacity to shared pool
 * HOW:  Removes reservation entry
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to release reservation for
 * @return 0 on success, -1 if not found or error
 */
int kg_ratelimit_release_bandwidth(
    kg_ratelimiter_t* limiter,
    const char* module_name
);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

/**
 * @brief Rate limiter statistics
 */
typedef struct {
    uint64_t total_requests;         /**< Total requests processed */
    uint64_t allowed_requests;       /**< Requests that were allowed */
    uint64_t denied_requests;        /**< Requests that were denied */
    uint64_t current_rate;           /**< Current request rate (per second) */
    uint64_t peak_rate;              /**< Peak request rate observed */
    uint64_t total_wait_time_ms;     /**< Total wait time accumulated */
    float avg_wait_time_ms;          /**< Average wait time for denied requests */
    float utilization;               /**< Current utilization (0.0-1.0) */
} kg_ratelimit_stats_t;

/**
 * @brief Get rate limiter statistics
 *
 * WHAT: Retrieve operational statistics
 * WHY:  Monitor rate limiter performance and load
 * HOW:  Aggregates internal counters into stats structure
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to get stats for (NULL for global)
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int kg_ratelimit_get_stats(
    const kg_ratelimiter_t* limiter,
    const char* module_name,
    kg_ratelimit_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Reset all statistics to zero
 * WHY:  Start fresh measurement period
 * HOW:  Zeros all counters without affecting rate limit state
 *
 * @param limiter Rate limiter handle
 * @param module_name Module to reset stats for (NULL for all)
 * @return 0 on success, -1 on error
 */
int kg_ratelimit_reset_stats(
    kg_ratelimiter_t* limiter,
    const char* module_name
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert rate limit algorithm to string
 *
 * @param algo Algorithm enum value
 * @return String representation or "UNKNOWN"
 */
const char* kg_ratelimit_algo_to_string(kg_ratelimit_algo_t algo);

/**
 * @brief Convert rate limit scope to string
 *
 * @param scope Scope enum value
 * @return String representation or "UNKNOWN"
 */
const char* kg_ratelimit_scope_to_string(kg_ratelimit_scope_t scope);

/**
 * @brief Convert operation type to string
 *
 * @param op Operation type enum value
 * @return String representation or "UNKNOWN"
 *
 * @note For combined operations (bitmask), returns first matching type
 */
const char* kg_operation_type_to_string(kg_operation_type_t op);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_RATELIMIT_H */
