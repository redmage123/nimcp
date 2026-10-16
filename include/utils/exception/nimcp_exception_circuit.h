/**
 * @file nimcp_exception_circuit.h
 * @brief Circuit breaker pattern and exception suppression for NIMCP
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Circuit breaker for exception frequency tracking + suppression lists
 * WHY:  Prevent cascading failures and allow maintenance windows
 * HOW:  Track exception frequency per code, trip when threshold exceeded;
 *       maintain suppression list for known issues during maintenance
 *
 * CIRCUIT BREAKER STATES:
 * ```
 *                  ┌──────────────────────────────────────┐
 *                  │                                      │
 *                  v                                      │
 *            ┌──────────┐    threshold    ┌──────────┐   │ reset_timeout
 *  Normal -> │  CLOSED  │ ──────────────> │   OPEN   │ ──┤
 *            └──────────┘    exceeded     └──────────┘   │
 *                  ^                           │         │
 *                  │                           v         │
 *                  │                      ┌──────────┐   │
 *                  └───── success ─────── │HALF_OPEN│ <─┘
 *                                         └──────────┘
 * ```
 *
 * SUPPRESSION:
 * - Temporarily suppress known exceptions during maintenance
 * - Auto-expire after specified duration
 * - Suppressed exceptions are still logged but not dispatched to handlers
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXCEPTION_CIRCUIT_H
#define NIMCP_EXCEPTION_CIRCUIT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/exception/nimcp_exception.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NIMCP_CIRCUIT_MAX_TRACKED       128   /**< Max tracked exception codes */
#define NIMCP_SUPPRESSION_MAX_ENTRIES   64    /**< Max suppression entries */

#define NIMCP_CIRCUIT_DEFAULT_THRESHOLD 10    /**< Default exceptions/min to trip */
#define NIMCP_CIRCUIT_DEFAULT_RESET_MS  30000 /**< Default reset timeout (30s) */
#define NIMCP_CIRCUIT_HALF_OPEN_ALLOW   3     /**< Max exceptions in half-open */

/* ============================================================================
 * Circuit Breaker Types
 * ============================================================================ */

/**
 * @brief Circuit breaker state
 */
typedef enum {
    CIRCUIT_STATE_CLOSED = 0,    /**< Normal operation, exceptions flow through */
    CIRCUIT_STATE_OPEN,          /**< Circuit tripped, blocking exceptions */
    CIRCUIT_STATE_HALF_OPEN      /**< Testing if issue is resolved */
} nimcp_circuit_state_t;

/**
 * @brief Circuit breaker entry for a single exception code
 *
 * Tracks exception frequency and circuit state for automatic protection.
 */
typedef struct {
    nimcp_error_t code;              /**< Exception error code */
    nimcp_exception_category_t category; /**< Exception category */

    /* Frequency counters */
    uint32_t count_1min;             /**< Count in last 1 minute window */
    uint32_t count_5min;             /**< Count in last 5 minute window */
    uint32_t count_total;            /**< Total count since init */

    /* Circuit state */
    nimcp_circuit_state_t state;     /**< Current circuit state */
    uint64_t last_occurrence_us;     /**< Last exception timestamp (us) */
    uint64_t circuit_open_until_us;  /**< Time when circuit moves to half-open */
    uint64_t last_window_reset_us;   /**< Last time window counters reset */

    /* Configuration */
    uint32_t trip_threshold;         /**< Exceptions per minute to trip circuit */
    uint32_t reset_timeout_ms;       /**< Timeout before half-open test */

    /* Half-open state tracking */
    uint32_t half_open_attempts;     /**< Exceptions in half-open state */
    uint32_t half_open_successes;    /**< Successful operations in half-open */

    /* Active flag */
    bool active;                     /**< Entry is in use */
} nimcp_exception_circuit_t;

/* ============================================================================
 * Suppression Types
 * ============================================================================ */

/**
 * @brief Exception suppression entry
 *
 * Allows temporary suppression of known exceptions during maintenance.
 */
typedef struct {
    nimcp_error_t code;              /**< Exception code to suppress */
    uint64_t suppress_until_us;      /**< Time when suppression expires */
    const char* reason;              /**< Reason for suppression (for logging) */
    uint64_t created_us;             /**< When suppression was created */
    uint32_t suppressed_count;       /**< Number of exceptions suppressed */
    bool active;                     /**< Entry is active */
} nimcp_suppression_entry_t;

/* ============================================================================
 * Circuit Breaker Statistics
 * ============================================================================ */

/**
 * @brief Circuit breaker statistics
 */
typedef struct {
    size_t total_tracked;            /**< Number of tracked exception codes */
    size_t circuits_open;            /**< Number of circuits currently open */
    size_t circuits_half_open;       /**< Number of circuits in half-open */
    uint64_t total_exceptions;       /**< Total exceptions recorded */
    uint64_t total_blocked;          /**< Total exceptions blocked by open circuits */
    uint64_t total_suppressed;       /**< Total exceptions suppressed */
} nimcp_circuit_stats_t;

/* ============================================================================
 * Circuit Breaker API
 * ============================================================================ */

/**
 * @brief Initialize the circuit breaker system
 *
 * WHAT: Initialize circuit breaker and suppression tracking
 * WHY:  Must be called before using circuit breaker functions
 * HOW:  Allocates tracking structures and initializes mutex
 *
 * @return 0 on success, -1 on error
 */
int nimcp_circuit_init(void);

/**
 * @brief Shutdown the circuit breaker system
 *
 * WHAT: Clean up circuit breaker resources
 * WHY:  Free memory and release mutex
 * HOW:  Clears all tracking data and destroys mutex
 */
void nimcp_circuit_shutdown(void);

/**
 * @brief Check if circuit breaker system is initialized
 *
 * @return true if initialized
 */
bool nimcp_circuit_is_initialized(void);

/**
 * @brief Record an exception occurrence
 *
 * WHAT: Record exception and update circuit state
 * WHY:  Track frequency for circuit breaker decisions
 * HOW:  Increment counters, check threshold, update state
 *
 * @param ex Exception to record
 * @return 0 if exception should proceed, 1 if blocked by circuit, -1 on error
 */
int nimcp_circuit_record(nimcp_exception_t* ex);

/**
 * @brief Get circuit state for an error code
 *
 * @param code Error code to check
 * @return Circuit state (CLOSED if not tracked)
 */
nimcp_circuit_state_t nimcp_circuit_get_state(nimcp_error_t code);

/**
 * @brief Check if circuit is open (blocking exceptions)
 *
 * @param code Error code to check
 * @return true if circuit is open or half-open at limit
 */
bool nimcp_circuit_is_open(nimcp_error_t code);

/**
 * @brief Set threshold and reset timeout for a specific error code
 *
 * WHAT: Configure circuit breaker for specific exception code
 * WHY:  Allow fine-tuning for different error types
 * HOW:  Creates or updates circuit entry with new thresholds
 *
 * @param code Error code to configure
 * @param threshold Exceptions per minute to trip (0 for default)
 * @param reset_ms Milliseconds before half-open test (0 for default)
 * @return 0 on success, -1 on error
 */
int nimcp_circuit_set_threshold(nimcp_error_t code, uint32_t threshold, uint32_t reset_ms);

/**
 * @brief Manually reset a circuit to closed state
 *
 * @param code Error code to reset
 * @return 0 on success, -1 if not found
 */
int nimcp_circuit_reset(nimcp_error_t code);

/**
 * @brief Reset all circuits to closed state
 */
void nimcp_circuit_reset_all(void);

/**
 * @brief Get exception count for a code within a time window
 *
 * @param code Error code to query
 * @param window_seconds Time window (60 for 1min, 300 for 5min, 0 for total)
 * @return Exception count
 */
size_t nimcp_circuit_get_count(nimcp_error_t code, uint32_t window_seconds);

/**
 * @brief Get circuit breaker entry for inspection
 *
 * @param code Error code to find
 * @return Pointer to circuit entry or NULL if not tracked
 */
const nimcp_exception_circuit_t* nimcp_circuit_get_entry(nimcp_error_t code);

/**
 * @brief Get circuit breaker statistics
 *
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int nimcp_circuit_get_stats(nimcp_circuit_stats_t* stats);

/**
 * @brief Report successful operation (for half-open recovery)
 *
 * WHAT: Report that an operation succeeded after exception
 * WHY:  Allow circuit to close from half-open state
 * HOW:  If in half-open and enough successes, close circuit
 *
 * @param code Error code that was previously failing
 * @return 0 on success, -1 on error
 */
int nimcp_circuit_report_success(nimcp_error_t code);

/* ============================================================================
 * Suppression API
 * ============================================================================ */

/**
 * @brief Suppress an exception code temporarily
 *
 * WHAT: Add exception to suppression list
 * WHY:  Allow maintenance without flooding logs/handlers
 * HOW:  Add entry with expiration time
 *
 * @param code Exception code to suppress
 * @param duration_ms Duration in milliseconds (0 for indefinite)
 * @param reason Reason for suppression (for logging)
 * @return 0 on success, -1 on error
 */
int nimcp_exception_suppress(nimcp_error_t code, uint64_t duration_ms, const char* reason);

/**
 * @brief Remove suppression for an exception code
 *
 * @param code Exception code to unsuppress
 * @return 0 on success, -1 if not found
 */
int nimcp_exception_unsuppress(nimcp_error_t code);

/**
 * @brief Check if an exception code is suppressed
 *
 * Also handles auto-expiration of suppression entries.
 *
 * @param code Exception code to check
 * @return true if suppressed
 */
bool nimcp_exception_is_suppressed(nimcp_error_t code);

/**
 * @brief List all active suppressions
 *
 * @param codes Output array for suppressed codes
 * @param max_codes Maximum number of codes to return
 * @return Number of active suppressions
 */
size_t nimcp_suppression_list_active(nimcp_error_t* codes, size_t max_codes);

/**
 * @brief Clear all suppressions
 */
void nimcp_suppression_clear_all(void);

/**
 * @brief Clear expired suppressions
 *
 * Called automatically by nimcp_exception_is_suppressed, but can be
 * called explicitly for cleanup.
 */
void nimcp_suppression_clear_expired(void);

/**
 * @brief Get suppression entry for a code
 *
 * @param code Error code to find
 * @return Pointer to suppression entry or NULL if not suppressed
 */
const nimcp_suppression_entry_t* nimcp_suppression_get_entry(nimcp_error_t code);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

/**
 * @brief Convert circuit state to string
 *
 * @param state Circuit state
 * @return String representation
 */
const char* nimcp_circuit_state_to_string(nimcp_circuit_state_t state);

/* ============================================================================
 * Integration with Exception System
 * ============================================================================ */

/**
 * @brief Check if exception should be processed
 *
 * WHAT: Combined check for circuit breaker and suppression
 * WHY:  Single call for exception handling path
 * HOW:  Check suppression first, then circuit breaker
 *
 * @param ex Exception to check
 * @return true if exception should be processed
 */
bool nimcp_exception_should_process(nimcp_exception_t* ex);

/**
 * @brief Update time windows and clean up expired entries
 *
 * WHAT: Maintenance function for circuit breaker
 * WHY:  Keep frequency counters accurate
 * HOW:  Reset window counters, clear expired suppressions
 *
 * Call periodically (e.g., every second) from a maintenance thread.
 */
void nimcp_circuit_maintenance(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXCEPTION_CIRCUIT_H */
