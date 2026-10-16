/**
 * @file nimcp_rate_limiter.h
 * @brief Token bucket rate limiter for BBB validation APIs
 *
 * WHAT: Token bucket algorithm for rate limiting requests
 * WHY:  Prevent abuse and DoS attacks against BBB validation
 * HOH: Token bucket with configurable rate, burst, and penalty system
 *
 * TOKEN BUCKET ALGORITHM:
 * ┌────────────────────────────────────────────────────────────────┐
 * │                     TOKEN BUCKET                               │
 * ├────────────────────────────────────────────────────────────────┤
 * │  Capacity: MAX_BURST tokens                                    │
 * │  Refill Rate: RATE tokens per second                           │
 * │                                                                │
 * │  ┌─────────────────┐                                          │
 * │  │ Bucket (10/10)  │  ← Request arrives                       │
 * │  │ ▓▓▓▓▓▓▓▓▓▓      │  ← Consume 1 token                       │
 * │  │ ▓▓▓▓▓▓▓▓▓       │  ← Allow request                         │
 * │  └─────────────────┘                                          │
 * │         ↑                                                      │
 * │         └── Refill at RATE tokens/sec                         │
 * │                                                                │
 * │  If bucket empty → DENY request                               │
 * │  If repeated violations → Apply PENALTY (reduce rate)         │
 * └────────────────────────────────────────────────────────────────┘
 *
 * SLIDING WINDOW VARIANT:
 * ┌────────────────────────────────────────────────────────────────┐
 * │  Time:     [-------- 1 second window --------]                │
 * │  Requests:  ■ ■ ■   ■■  ■   ■■■  ■  ■  (12 requests)          │
 * │  Limit:     10 requests/sec                                   │
 * │  Result:    DENY (exceeded limit)                             │
 * │                                                                │
 * │  Advantage: Smoother limiting, prevents bursts at boundaries  │
 * └────────────────────────────────────────────────────────────────┘
 *
 * PENALTY SYSTEM:
 * ┌────────────────────────────────────────────────────────────────┐
 * │  Violation 1:  Warning (rate unchanged)                       │
 * │  Violation 2:  Reduce rate by 25%                             │
 * │  Violation 3:  Reduce rate by 50%                             │
 * │  Violation 4+: Block for cooldown period                      │
 * └────────────────────────────────────────────────────────────────┘
 *
 * USAGE:
 * ```c
 * // Create rate limiter
 * nimcp_rate_limit_config_t config = {
 *     .requests_per_second = 100,
 *     .burst_size = 150,
 *     .algorithm = RATE_LIMIT_TOKEN_BUCKET,
 *     .enable_penalties = true,
 *     .penalty_cooldown_ms = 60000
 * };
 * nimcp_rate_limiter_t limiter = nimcp_rate_limiter_create(&config);
 *
 * // Check if request allowed
 * if (nimcp_rate_limiter_allow(limiter, "client_192.168.1.100")) {
 *     // Process request
 *     process_bbb_validation(data);
 * } else {
 *     // Reject request (rate limit exceeded)
 *     return NIMCP_ERROR_RATE_LIMIT;
 * }
 *
 * // Get statistics
 * nimcp_rate_limit_stats_t stats;
 * nimcp_rate_limiter_get_stats(limiter, &stats);
 * printf("Allowed: %lu, Denied: %lu\n", stats.requests_allowed, stats.requests_denied);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#ifndef NIMCP_RATE_LIMITER_H
#define NIMCP_RATE_LIMITER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Default requests per second */
#define NIMCP_RATE_LIMIT_DEFAULT_RPS 100

/** @brief Default burst size */
#define NIMCP_RATE_LIMIT_DEFAULT_BURST 150

/** @brief Default penalty cooldown (milliseconds) */
#define NIMCP_RATE_LIMIT_DEFAULT_COOLDOWN_MS 60000

/** @brief Maximum client ID length */
#define NIMCP_RATE_LIMIT_MAX_CLIENT_ID 128

/** @brief Magic number for validation */
#define NIMCP_RATE_LIMITER_MAGIC 0xBADC0FFE

//=============================================================================
// Error Codes
//=============================================================================

/** @brief Rate limit exceeded */
#define NIMCP_ERROR_RATE_LIMIT (9000 + 100)

/** @brief Client blocked due to violations */
#define NIMCP_ERROR_CLIENT_BLOCKED (9000 + 101)

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Rate limiter handle (opaque)
 */
typedef struct nimcp_rate_limiter_impl* nimcp_rate_limiter_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Rate limiting algorithm
 */
typedef enum {
    RATE_LIMIT_TOKEN_BUCKET = 0,  /**< Classic token bucket */
    RATE_LIMIT_SLIDING_WINDOW,    /**< Sliding window counter */
    RATE_LIMIT_FIXED_WINDOW,      /**< Fixed window counter */
    RATE_LIMIT_LEAKY_BUCKET       /**< Leaky bucket (smoother) */
} nimcp_rate_limit_algorithm_t;

/**
 * @brief Penalty action for violations
 */
typedef enum {
    PENALTY_NONE = 0,           /**< No penalty */
    PENALTY_WARN,               /**< Warning only */
    PENALTY_REDUCE_RATE_25,     /**< Reduce rate by 25% */
    PENALTY_REDUCE_RATE_50,     /**< Reduce rate by 50% */
    PENALTY_REDUCE_RATE_75,     /**< Reduce rate by 75% */
    PENALTY_BLOCK_TEMPORARY,    /**< Block for cooldown period */
    PENALTY_BLOCK_PERMANENT     /**< Permanent block (manual unblock) */
} nimcp_penalty_action_t;

/**
 * @brief Client state
 */
typedef enum {
    CLIENT_STATE_NORMAL = 0,
    CLIENT_STATE_WARNING,
    CLIENT_STATE_RATE_REDUCED,
    CLIENT_STATE_BLOCKED_TEMP,
    CLIENT_STATE_BLOCKED_PERM
} nimcp_client_state_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Penalty configuration
 */
typedef struct {
    bool enabled;                   /**< Enable penalty system */
    uint32_t violation_threshold;   /**< Violations before penalty */
    nimcp_penalty_action_t actions[5]; /**< Penalties per violation level */
    uint32_t cooldown_ms;           /**< Cooldown period for temp blocks */
    uint32_t violation_window_ms;   /**< Time window for violation counting */
    bool reset_on_good_behavior;    /**< Reset penalties after good behavior */
    uint32_t good_behavior_count;   /**< Requests needed to reset */
} nimcp_penalty_config_t;

/**
 * @brief Rate limiter configuration
 */
typedef struct {
    // Basic rate limiting
    float requests_per_second;      /**< Rate limit (requests/sec) */
    uint32_t burst_size;            /**< Maximum burst tokens */
    nimcp_rate_limit_algorithm_t algorithm; /**< Algorithm to use */

    // Per-client/per-resource
    bool per_client;                /**< Separate limit per client */
    bool per_resource;              /**< Separate limit per resource */
    uint32_t max_tracked_clients;   /**< Maximum tracked clients */

    // Penalty system
    nimcp_penalty_config_t penalty; /**< Penalty configuration */

    // Statistics
    bool enable_statistics;         /**< Track statistics */
    bool enable_logging;            /**< Log events */

    // Cleanup
    uint32_t cleanup_interval_ms;   /**< Cleanup expired entries (ms) */
    uint32_t idle_timeout_ms;       /**< Remove idle clients after (ms) */
} nimcp_rate_limit_config_t;

/**
 * @brief Per-client statistics
 */
typedef struct {
    char client_id[NIMCP_RATE_LIMIT_MAX_CLIENT_ID]; /**< Client identifier */
    nimcp_client_state_t state;     /**< Current client state */
    uint64_t requests_allowed;      /**< Requests allowed */
    uint64_t requests_denied;       /**< Requests denied */
    uint32_t current_penalty_level; /**< Current penalty level */
    uint32_t violations;            /**< Total violations */
    uint64_t last_request_time_ms;  /**< Last request timestamp */
    float current_rate;             /**< Current rate (may be reduced) */
    uint32_t tokens_available;      /**< Current tokens in bucket */
} nimcp_client_stats_t;

/**
 * @brief Overall rate limiter statistics
 */
typedef struct {
    uint64_t total_requests;        /**< Total requests processed */
    uint64_t requests_allowed;      /**< Requests allowed */
    uint64_t requests_denied;       /**< Requests denied */
    uint64_t penalties_applied;     /**< Penalties applied */
    uint64_t clients_blocked_temp;  /**< Clients temporarily blocked */
    uint64_t clients_blocked_perm;  /**< Clients permanently blocked */
    uint32_t active_clients;        /**< Currently tracked clients */
    float overall_allow_rate;       /**< Percentage of requests allowed */
    float avg_tokens_consumed;      /**< Average tokens per request */
} nimcp_rate_limit_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Rate limit violation callback
 *
 * WHAT: Called when rate limit is exceeded
 * WHY:  Allow custom logging or actions on violations
 * HOW:  Invoked with client info and violation details
 *
 * @param client_id Client identifier
 * @param violation_count Number of violations
 * @param penalty Penalty being applied
 * @param user_data User context
 */
typedef void (*nimcp_rate_limit_violation_callback_t)(
    const char* client_id,
    uint32_t violation_count,
    nimcp_penalty_action_t penalty,
    void* user_data
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create rate limiter with configuration
 *
 * WHAT: Creates rate limiter instance
 * WHY:  Initialize rate limiting for BBB or other APIs
 * HOW:  Allocates structures, initializes token buckets
 *
 * @param config Configuration (NULL for defaults)
 * @return Limiter handle or NULL on failure
 *
 * COMPLEXITY: O(n) where n = max_tracked_clients
 * THREAD SAFETY: Thread-safe
 */
nimcp_rate_limiter_t nimcp_rate_limiter_create(
    const nimcp_rate_limit_config_t* config
);

/**
 * @brief Destroy rate limiter
 *
 * WHAT: Cleanup rate limiter resources
 * WHY:  Free memory, destroy locks
 * HOW:  Releases all client buckets and structures
 *
 * @param limiter Limiter handle (NULL safe)
 */
void nimcp_rate_limiter_destroy(nimcp_rate_limiter_t limiter);

/**
 * @brief Get default configuration
 *
 * @return Default configuration with sensible values
 */
nimcp_rate_limit_config_t nimcp_rate_limiter_default_config(void);

//=============================================================================
// Core Rate Limiting API
//=============================================================================

/**
 * @brief Check if request is allowed
 *
 * WHAT: Check rate limit and consume token if allowed
 * WHY:  Core rate limiting decision
 * HOW:  Checks bucket, consumes token, updates statistics
 *
 * @param limiter Limiter handle
 * @param client_id Client identifier (NULL for global limit)
 * @return true if allowed, false if denied
 *
 * COMPLEXITY: O(1) average, O(log n) worst case (hash collision)
 * THREAD SAFETY: Thread-safe
 *
 * SIDE EFFECTS:
 * - Consumes token if allowed
 * - Updates statistics
 * - May apply penalties on violations
 * - May trigger violation callbacks
 */
bool nimcp_rate_limiter_allow(
    nimcp_rate_limiter_t limiter,
    const char* client_id
);

/**
 * @brief Check if request allowed for specific resource
 *
 * @param limiter Limiter handle
 * @param client_id Client identifier
 * @param resource_id Resource identifier
 * @return true if allowed, false if denied
 */
bool nimcp_rate_limiter_allow_resource(
    nimcp_rate_limiter_t limiter,
    const char* client_id,
    const char* resource_id
);

/**
 * @brief Try to acquire N tokens atomically
 *
 * WHAT: Try to acquire multiple tokens at once
 * WHY:  Support batch operations
 * HOW:  Checks if N tokens available, consumes all or none
 *
 * @param limiter Limiter handle
 * @param client_id Client identifier
 * @param count Number of tokens to acquire
 * @return true if acquired, false if insufficient tokens
 */
bool nimcp_rate_limiter_acquire(
    nimcp_rate_limiter_t limiter,
    const char* client_id,
    uint32_t count
);

/**
 * @brief Check if request would be allowed (without consuming token)
 *
 * @param limiter Limiter handle
 * @param client_id Client identifier
 * @return true if would be allowed
 */
bool nimcp_rate_limiter_check(
    nimcp_rate_limiter_t limiter,
    const char* client_id
);

/**
 * @brief Get time until next token available (milliseconds)
 *
 * @param limiter Limiter handle
 * @param client_id Client identifier
 * @return Milliseconds until next token, or 0 if tokens available
 */
uint64_t nimcp_rate_limiter_time_until_ready(
    nimcp_rate_limiter_t limiter,
    const char* client_id
);

//=============================================================================
// Client Management API
//=============================================================================

/**
 * @brief Reset rate limit for specific client
 *
 * @param limiter Limiter handle
 * @param client_id Client identifier
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_reset_client(
    nimcp_rate_limiter_t limiter,
    const char* client_id
);

/**
 * @brief Block client permanently
 *
 * @param limiter Limiter handle
 * @param client_id Client identifier
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_block_client(
    nimcp_rate_limiter_t limiter,
    const char* client_id
);

/**
 * @brief Unblock client
 *
 * @param limiter Limiter handle
 * @param client_id Client identifier
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_unblock_client(
    nimcp_rate_limiter_t limiter,
    const char* client_id
);

/**
 * @brief Get client state
 *
 * @param limiter Limiter handle
 * @param client_id Client identifier
 * @param stats Output client statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_get_client_stats(
    nimcp_rate_limiter_t limiter,
    const char* client_id,
    nimcp_client_stats_t* stats
);

//=============================================================================
// Statistics and Monitoring API
//=============================================================================

/**
 * @brief Get overall statistics
 *
 * @param limiter Limiter handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_get_stats(
    nimcp_rate_limiter_t limiter,
    nimcp_rate_limit_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param limiter Limiter handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_reset_stats(nimcp_rate_limiter_t limiter);

/**
 * @brief Get number of active clients
 *
 * @param limiter Limiter handle
 * @return Number of tracked clients
 */
uint32_t nimcp_rate_limiter_get_active_clients(nimcp_rate_limiter_t limiter);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set violation callback
 *
 * @param limiter Limiter handle
 * @param callback Callback function
 * @param user_data User context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_set_violation_callback(
    nimcp_rate_limiter_t limiter,
    nimcp_rate_limit_violation_callback_t callback,
    void* user_data
);

/**
 * @brief Update rate limit dynamically
 *
 * @param limiter Limiter handle
 * @param requests_per_second New rate limit
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_set_rate(
    nimcp_rate_limiter_t limiter,
    float requests_per_second
);

/**
 * @brief Update burst size dynamically
 *
 * @param limiter Limiter handle
 * @param burst_size New burst size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_set_burst(
    nimcp_rate_limiter_t limiter,
    uint32_t burst_size
);

//=============================================================================
// Bio-Async Integration API
//=============================================================================

/**
 * @brief Register with bio-async router
 *
 * @param limiter Limiter handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_rate_limiter_register_bio_async(
    nimcp_rate_limiter_t limiter
);

/**
 * @brief Process bio-async inbox messages
 *
 * @param limiter Limiter handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t nimcp_rate_limiter_process_inbox(
    nimcp_rate_limiter_t limiter,
    uint32_t max_messages
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RATE_LIMITER_H */
