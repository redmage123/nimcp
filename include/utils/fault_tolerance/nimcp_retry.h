/**
 * @file nimcp_retry.h
 * @brief Retry framework with exponential backoff for fault tolerance
 *
 * WHAT: Generic retry mechanism with configurable backoff, jitter, and circuit breaker
 * WHY:  Transient failures (network, resource contention, timing) often succeed on retry
 * HOW:  Exponential backoff with jitter prevents thundering herd; circuit breaker stops
 *       futile retries; recovery cache accelerates known-good strategy selection
 *
 * RETRY ARCHITECTURE:
 * ```
 * ┌──────────────────────────────────────────────────────────────┐
 * │                    nimcp_retry_with_backoff()                │
 * │                                                              │
 * │  ┌─────────────┐    ┌──────────────┐    ┌───────────────┐  │
 * │  │ Circuit      │───>│ Execute      │───>│ Record        │  │
 * │  │ Breaker      │    │ Operation    │    │ Result        │  │
 * │  │ Check        │    │              │    │               │  │
 * │  └─────────────┘    └──────────────┘    └───────────────┘  │
 * │         │                   │                    │          │
 * │         │ blocked      fail │              success│          │
 * │         v                   v                    v          │
 * │     RETURN           ┌──────────┐          RETURN          │
 * │     ERROR            │ Backoff  │          SUCCESS          │
 * │                      │ + Jitter │                           │
 * │                      └──────────┘                           │
 * │                           │                                 │
 * │                      next attempt                           │
 * └──────────────────────────────────────────────────────────────┘
 * ```
 *
 * BACKOFF FORMULA:
 *   delay = initial_delay_ms * backoff_factor^attempt
 *   jittered_delay = delay * (1.0 + random(-1,1) * jitter_factor)
 *   clamped_delay = min(jittered_delay, max_delay_ms)
 *
 * DEFAULT SEQUENCE (10ms base, 2x factor):
 *   Attempt 0: 10ms, Attempt 1: 20ms, Attempt 2: 40ms,
 *   Attempt 3: 80ms, Attempt 4: 160ms
 *
 * INTEGRATION POINTS:
 * - Circuit breaker: Prevents retrying operations that consistently fail
 * - Recovery cache: Caches successful strategies for faster future recovery
 * - Health agent: Reports heartbeats during retry loops
 *
 * USAGE:
 * ```c
 * nimcp_retry_config_t config = nimcp_retry_default_config();
 * nimcp_retry_result_t result;
 *
 * operation_t op = {
 *     .name = "connect_to_peer",
 *     .execute = my_connect_fn,
 *     .rollback = my_disconnect_fn,
 *     .context = &my_ctx
 * };
 *
 * nimcp_error_t err = nimcp_retry_with_backoff(&op, &config, my_breaker, &result);
 * if (err == NIMCP_OK && result.success) {
 *     // Operation succeeded after result.attempts attempts
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_RETRY_H
#define NIMCP_RETRY_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CALLBACK TYPES
 * ============================================================================ */

/**
 * @brief Callback invoked before each retry attempt
 *
 * WHAT: User-defined hook called between retry attempts
 * WHY:  Allow logging, metric collection, or state adjustment between retries
 * HOW:  Called after backoff delay, before next execute() call
 *
 * @param attempt   Current attempt number (0-based)
 * @param delay_ms  Delay that was applied before this attempt (0 for first)
 * @param context   User-provided context from the operation
 */
typedef void (*nimcp_retry_on_retry_fn)(uint32_t attempt, uint32_t delay_ms, void* context);

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/**
 * @brief Retry configuration with exponential backoff parameters
 *
 * WHAT: All knobs for controlling retry behavior
 * WHY:  Different operations need different retry profiles
 * HOW:  Struct initialized via nimcp_retry_default_config() then customized
 */
typedef struct {
    uint32_t max_retries;          /**< Maximum retry attempts (0 = execute once, no retry) */
    uint32_t initial_delay_ms;     /**< Initial backoff delay in milliseconds */
    uint32_t max_delay_ms;         /**< Maximum backoff delay cap in milliseconds */
    float    backoff_factor;       /**< Multiplier per attempt (typically 2.0) */
    float    jitter_factor;        /**< Random jitter range 0.0-1.0 (0.25 = +/-25%) */

    nimcp_retry_on_retry_fn on_retry;  /**< Optional callback before each retry (can be NULL) */
} nimcp_retry_config_t;

/* ============================================================================
 * RESULT
 * ============================================================================ */

/**
 * @brief Result of a retry-with-backoff operation
 *
 * WHAT: Outcome details from nimcp_retry_with_backoff()
 * WHY:  Caller needs to know how many attempts, total time, and final status
 * HOW:  Populated by the retry loop, returned via out-parameter
 */
typedef struct {
    bool        success;           /**< true if operation eventually succeeded */
    uint32_t    attempts;          /**< Total attempts made (1 = first try worked) */
    uint32_t    total_delay_ms;    /**< Cumulative backoff delay in milliseconds */
    nimcp_error_t last_error;      /**< Last error code (NIMCP_OK if success) */
} nimcp_retry_result_t;

/* ============================================================================
 * API FUNCTIONS
 * ============================================================================ */

/**
 * @brief Get default retry configuration
 *
 * WHAT: Returns sensible defaults for exponential backoff
 * WHY:  Convenient starting point; caller can override individual fields
 * HOW:  Returns stack-allocated struct with standard values
 *
 * DEFAULTS:
 *   max_retries     = 5
 *   initial_delay_ms = 10   (10ms)
 *   max_delay_ms     = 5000 (5s cap)
 *   backoff_factor   = 2.0
 *   jitter_factor    = 0.25 (+/-25%)
 *   on_retry         = NULL
 *
 * @return Default configuration struct
 */
nimcp_retry_config_t nimcp_retry_default_config(void);

/**
 * @brief Retry an operation with exponential backoff
 *
 * WHAT: Execute operation, retrying on failure with increasing delays
 * WHY:  Transient failures (contention, timing, resources) often resolve themselves
 * HOW:  Loop: check circuit breaker -> execute -> on success return; on fail sleep with
 *       jittered exponential backoff -> repeat until max_retries exhausted
 *
 * CIRCUIT BREAKER INTEGRATION:
 * If a circuit_breaker_t* is provided, the breaker is checked before each attempt.
 * If the circuit is OPEN, the retry loop exits immediately without executing.
 * Success/failure results are recorded to the breaker.
 *
 * ROLLBACK:
 * If all retries are exhausted and op->rollback is set, rollback is called once.
 *
 * THREAD SAFETY:
 * The function itself is thread-safe. The operation's execute/rollback functions
 * must be safe to call from the calling thread.
 *
 * @param op      Operation to execute (must not be NULL, op->execute must not be NULL)
 * @param config  Retry configuration (must not be NULL)
 * @param cb      Circuit breaker (can be NULL to skip breaker checks)
 * @param result  Output result struct (must not be NULL)
 * @return NIMCP_OK on success, NIMCP_ERROR_INVALID_PARAM for bad args,
 *         NIMCP_ERROR_OPERATION_FAILED if all retries exhausted,
 *         NIMCP_ERROR_INVALID_STATE if circuit breaker is open
 */
nimcp_error_t nimcp_retry_with_backoff(
    operation_t* op,
    const nimcp_retry_config_t* config,
    circuit_breaker_t* cb,
    nimcp_retry_result_t* result
);

/**
 * @brief Set health agent for the retry module
 *
 * WHAT: Assign health agent for heartbeat reporting during retries
 * WHY:  Long retry loops should report liveness to the health monitor
 * HOW:  Stores agent pointer atomically for thread-safe access
 *
 * @param agent Health agent instance (can be NULL to disable)
 */
struct nimcp_health_agent;
void retry_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETRY_H */
