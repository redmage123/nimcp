/**
 * @file nimcp_exception_metrics.h
 * @brief Exception metrics, telemetry, and adaptive recovery strategies
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Automatic metrics collection for exceptions and adaptive recovery
 * WHY:  Enable observability (exceptions/sec, recovery rates, MTTR) and
 *       use immune memory to improve recovery strategies over time
 * HOW:  Atomic counters for thread-safe metrics, EMA for rates,
 *       pattern learning for adaptive recovery
 *
 * METRICS ARCHITECTURE:
 * ```
 * Exception Raised
 *       |
 *       v
 * +------------------+
 * | Record Metrics   |  <-- Category count, rate update, timestamp
 * +------------------+
 *       |
 *       v
 * +------------------+
 * | Adaptive Lookup  |  <-- Check if we've seen this pattern before
 * +------------------+
 *       |
 *       v (pattern found)
 * +------------------+
 * | Suggest Action   |  <-- Use learned success rates to suggest best action
 * +------------------+
 *       |
 *       v (after recovery)
 * +------------------+
 * | Record Outcome   |  <-- Update success rates, MTTR
 * +------------------+
 * ```
 *
 * ADAPTIVE RECOVERY:
 * - Track success/failure rates per exception pattern per recovery action
 * - Use immune system memory style learning
 * - After sufficient data, suggest best action based on historical success
 * - Reset patterns that have too many consecutive failures
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EXCEPTION_METRICS_H
#define NIMCP_EXCEPTION_METRICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utils/exception/nimcp_exception.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NIMCP_METRICS_MAX_PATTERNS       256   /**< Max tracked exception patterns */
#define NIMCP_METRICS_HISTORY_SIZE       1000  /**< Max history entries */
#define NIMCP_METRICS_EMA_ALPHA          0.1f  /**< EMA smoothing factor */
#define NIMCP_METRICS_MIN_SAMPLES        5     /**< Min samples before pattern is "learned" */
#define NIMCP_METRICS_MAX_CONSECUTIVE_FAILURES 10 /**< Reset pattern after this many failures */
#define NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT 12 /**< Number of recovery action types */
#define NIMCP_METRICS_MAX_CATEGORIES     20    /**< Max exception categories */

/* ============================================================================
 * Per-Category Metrics
 * ============================================================================ */

/**
 * @brief Metrics tracked per exception category
 *
 * Tracks count, rates, and timing for each exception category.
 */
typedef struct {
    nimcp_exception_category_t category;  /**< Exception category */
    uint64_t total_count;                 /**< Total exceptions in this category */
    uint64_t count_last_minute;           /**< Count in last minute window */
    uint64_t count_last_hour;             /**< Count in last hour window */
    float rate_per_second;                /**< Exponential moving average rate */
    uint64_t last_occurrence_us;          /**< Timestamp of last occurrence (us) */
} nimcp_category_metrics_t;

/* ============================================================================
 * Recovery Metrics
 * ============================================================================ */

/**
 * @brief Metrics tracked per recovery action type
 *
 * Tracks success rates and timing statistics for each recovery action.
 */
typedef struct {
    nimcp_exception_recovery_action_t action;       /**< Recovery action type */
    uint64_t attempts;                    /**< Total attempts */
    uint64_t successes;                   /**< Successful recoveries */
    uint64_t failures;                    /**< Failed recoveries */
    float success_rate;                   /**< Success rate (0.0 - 1.0) */
    uint64_t total_time_us;               /**< Total recovery time (microseconds) */
    float avg_time_us;                    /**< Average recovery time (MTTR) */
    uint64_t min_time_us;                 /**< Minimum recovery time */
    uint64_t max_time_us;                 /**< Maximum recovery time */
} nimcp_recovery_metrics_t;

/* ============================================================================
 * Adaptive Pattern Strategy
 * ============================================================================ */

/**
 * @brief Pattern-specific adaptive recovery strategy
 *
 * Tracks per-epitope success rates for each recovery action,
 * enabling learned recovery strategies based on immune memory.
 */
typedef struct {
    uint8_t epitope[64];                  /**< Exception fingerprint */
    size_t epitope_len;                   /**< Epitope length */
    nimcp_exception_recovery_action_t preferred_action; /**< Currently preferred action */
    float action_success_rates[NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT]; /**< Per-action success rate */
    uint32_t action_attempts[NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT];   /**< Per-action attempt count */
    uint64_t last_success_us;             /**< Timestamp of last successful recovery */
    uint32_t consecutive_failures;        /**< Consecutive failures (reset trigger) */
    bool learned;                         /**< Has enough data to be reliable */
} nimcp_adaptive_pattern_t;

/* ============================================================================
 * Global Exception Metrics
 * ============================================================================ */

/**
 * @brief Global exception metrics structure
 *
 * Aggregates all metrics for system-wide observability.
 */
typedef struct {
    uint64_t total_exceptions;            /**< Total exceptions raised */
    uint64_t total_recoveries_attempted;  /**< Total recovery attempts */
    uint64_t total_recoveries_succeeded;  /**< Successful recoveries */
    float overall_recovery_rate;          /**< Overall recovery success rate */
    float current_rate_per_second;        /**< Current exceptions per second (EMA) */
    uint64_t peak_rate_per_second;        /**< Peak rate observed */
    uint64_t uptime_us;                   /**< System uptime in microseconds */

    /* Per-category metrics */
    nimcp_category_metrics_t categories[NIMCP_METRICS_MAX_CATEGORIES];
    size_t category_count;                /**< Number of active categories */

    /* Per-action recovery metrics */
    nimcp_recovery_metrics_t recovery[NIMCP_METRICS_EXCEPTION_RECOVERY_COUNT];
} nimcp_exception_metrics_t;

/* ============================================================================
 * Metrics Configuration
 * ============================================================================ */

/**
 * @brief Metrics subsystem configuration
 */
typedef struct {
    bool enable_rate_tracking;            /**< Track exception rates */
    bool enable_adaptive_recovery;        /**< Enable adaptive learning */
    float ema_alpha;                      /**< EMA smoothing factor (0.0 - 1.0) */
    uint32_t min_samples_for_learning;    /**< Min samples before pattern is learned */
    uint32_t rate_update_interval_ms;     /**< How often to update rates */
    bool persist_patterns;                /**< Persist learned patterns */
} nimcp_metrics_config_t;

/* ============================================================================
 * Metrics API
 * ============================================================================ */

/**
 * @brief Initialize metrics subsystem
 *
 * WHAT: Initialize metrics tracking structures
 * WHY:  Enable exception observability
 * HOW:  Allocate tracking structures, start timers
 *
 * @return 0 on success, -1 on error
 */
int nimcp_metrics_init(void);

/**
 * @brief Initialize with custom configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int nimcp_metrics_init_with_config(const nimcp_metrics_config_t* config);

/**
 * @brief Shutdown metrics subsystem
 *
 * Releases all resources, optionally persists patterns.
 */
void nimcp_metrics_shutdown(void);

/**
 * @brief Get default configuration
 *
 * @param config Output configuration struct
 */
void nimcp_metrics_default_config(nimcp_metrics_config_t* config);

/**
 * @brief Record an exception occurrence
 *
 * WHAT: Update metrics for an exception
 * WHY:  Track exception frequency and patterns
 * HOW:  Increment counters, update EMA rate, record timestamp
 *
 * Thread-safe via atomic operations.
 *
 * @param ex Exception to record
 */
void nimcp_metrics_record_exception(nimcp_exception_t* ex);

/**
 * @brief Record a recovery attempt result
 *
 * WHAT: Update recovery metrics
 * WHY:  Track recovery success rates and timing
 * HOW:  Update success/failure counts, timing stats
 *
 * @param ex Exception that triggered recovery
 * @param action Recovery action taken
 * @param success Whether recovery succeeded
 * @param duration_us Recovery duration in microseconds
 */
void nimcp_metrics_record_recovery(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    bool success,
    uint64_t duration_us
);

/**
 * @brief Get current metrics snapshot
 *
 * @param metrics Output metrics structure
 */
void nimcp_metrics_get(nimcp_exception_metrics_t* metrics);

/**
 * @brief Reset all metrics
 *
 * Clears all counters and rates. Does not reset learned patterns.
 */
void nimcp_metrics_reset(void);

/* ============================================================================
 * Per-Category Query API
 * ============================================================================ */

/**
 * @brief Get exception rate for a category
 *
 * @param category Exception category
 * @return Exceptions per second (EMA)
 */
float nimcp_metrics_get_rate(nimcp_exception_category_t category);

/**
 * @brief Get exception count for a category in time window
 *
 * @param category Exception category
 * @param window_seconds Time window in seconds (60 = last minute, 3600 = last hour)
 * @return Count of exceptions in window
 */
uint64_t nimcp_metrics_get_count(
    nimcp_exception_category_t category,
    uint32_t window_seconds
);

/**
 * @brief Get recovery success rate for an action type
 *
 * @param action Recovery action type
 * @return Success rate (0.0 - 1.0)
 */
float nimcp_metrics_get_recovery_rate(nimcp_exception_recovery_action_t action);

/**
 * @brief Get Mean Time To Recovery for an action type
 *
 * @param action Recovery action type
 * @return Average recovery time in microseconds
 */
float nimcp_metrics_get_mttr(nimcp_exception_recovery_action_t action);

/* ============================================================================
 * Top-N Query API
 * ============================================================================ */

/**
 * @brief Get top categories by exception count
 *
 * @param out Output array of category metrics
 * @param max_count Maximum number to return
 * @return Number of categories returned
 */
size_t nimcp_metrics_top_categories(
    nimcp_category_metrics_t* out,
    size_t max_count
);

/**
 * @brief Get top exception patterns by occurrence
 *
 * @param out Output array of patterns
 * @param max_count Maximum number to return
 * @return Number of patterns returned
 */
size_t nimcp_metrics_top_patterns(
    nimcp_adaptive_pattern_t* out,
    size_t max_count
);

/* ============================================================================
 * Adaptive Recovery API
 * ============================================================================ */

/**
 * @brief Initialize adaptive recovery subsystem
 *
 * WHAT: Initialize pattern learning structures
 * WHY:  Enable immune-style recovery learning
 * HOW:  Allocate pattern storage, initialize hash tables
 *
 * @return 0 on success, -1 on error
 */
int nimcp_adaptive_init(void);

/**
 * @brief Shutdown adaptive recovery subsystem
 *
 * Optionally persists learned patterns before shutdown.
 */
void nimcp_adaptive_shutdown(void);

/**
 * @brief Suggest best recovery action for an exception
 *
 * WHAT: Get recommended recovery action based on learned patterns
 * WHY:  Improve recovery over time using immune memory
 * HOW:  Look up epitope, return action with highest historical success rate
 *
 * @param ex Exception needing recovery
 * @return Suggested recovery action (or EXCEPTION_RECOVERY_NONE if not learned)
 */
nimcp_exception_recovery_action_t nimcp_adaptive_suggest_action(nimcp_exception_t* ex);

/**
 * @brief Record recovery outcome for pattern learning
 *
 * WHAT: Update pattern's action success rates
 * WHY:  Learn which actions work for which exception patterns
 * HOW:  Update success rate, recalculate preferred action
 *
 * @param ex Exception that was recovered
 * @param action Recovery action taken
 * @param success Whether recovery succeeded
 * @return 0 on success, -1 on error
 */
int nimcp_adaptive_record_outcome(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    bool success
);

/**
 * @brief Get confidence level for an action on a pattern
 *
 * Returns confidence based on number of samples and success rate variance.
 *
 * @param ex Exception
 * @param action Recovery action
 * @return Confidence level (0.0 - 1.0)
 */
float nimcp_adaptive_get_confidence(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action
);

/**
 * @brief Force a specific action for a pattern (manual override)
 *
 * Administrators can force a specific action for a pattern,
 * bypassing learned suggestions.
 *
 * @param epitope Pattern epitope
 * @param len Epitope length
 * @param action Action to force
 * @return 0 on success, -1 if pattern not found
 */
int nimcp_adaptive_force_action(
    const uint8_t* epitope,
    size_t len,
    nimcp_exception_recovery_action_t action
);

/**
 * @brief Reset learning for a specific pattern
 *
 * Clears all learned data for a pattern, useful when recovery
 * strategy changes or pattern needs relearning.
 *
 * @param epitope Pattern epitope
 * @param len Epitope length
 */
void nimcp_adaptive_reset_pattern(const uint8_t* epitope, size_t len);

/**
 * @brief Reset all learned patterns
 *
 * Clears all learned data. Use with caution.
 */
void nimcp_adaptive_reset_all(void);

/* ============================================================================
 * Persistence API
 * ============================================================================ */

/**
 * @brief Export learned patterns to buffer
 *
 * Serializes all learned patterns for persistence.
 *
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of bytes written, or required size if buffer too small
 */
size_t nimcp_adaptive_export(uint8_t* buffer, size_t size);

/**
 * @brief Import learned patterns from buffer
 *
 * Deserializes and loads previously saved patterns.
 *
 * @param buffer Input buffer
 * @param size Buffer size
 * @return 0 on success, -1 on error
 */
int nimcp_adaptive_import(const uint8_t* buffer, size_t size);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Adaptive recovery statistics
 */
typedef struct {
    uint64_t total_patterns;              /**< Total patterns tracked */
    uint64_t learned_patterns;            /**< Patterns with enough data */
    uint64_t suggestions_made;            /**< Total suggestions made */
    uint64_t suggestions_followed;        /**< Suggestions that were followed */
    float suggestion_accuracy;            /**< Accuracy of suggestions */
    uint64_t patterns_reset;              /**< Patterns reset due to failures */
} nimcp_adaptive_stats_t;

/**
 * @brief Get adaptive recovery statistics
 *
 * @param stats Output statistics structure
 */
void nimcp_adaptive_get_stats(nimcp_adaptive_stats_t* stats);

/**
 * @brief Check if metrics subsystem is initialized
 *
 * @return true if initialized
 */
bool nimcp_metrics_is_initialized(void);

/**
 * @brief Check if adaptive recovery is initialized
 *
 * @return true if initialized
 */
bool nimcp_adaptive_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXCEPTION_METRICS_H */
