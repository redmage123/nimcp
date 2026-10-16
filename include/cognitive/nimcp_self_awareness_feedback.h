// ============================================================================
// nimcp_self_awareness_feedback.h - Feedback Loop System for Self-Awareness
// ============================================================================
/**
 * @file nimcp_self_awareness_feedback.h
 * @brief Advanced feedback loop management for self-awareness coordination
 *
 * WHAT: Manages feedback policies, history tracking, and inter-component transfers
 * WHY: Feedback loops need tuning, history for learning, and analysis for debugging
 * HOW: Provides configurable transfer functions, ring buffers for history, trend analysis
 *
 * BIOLOGICAL INSPIRATION:
 * - Cortical Feedback Loops: Higher areas modulate lower areas
 * - Recurrent Processing: Information flows both ways in visual cortex
 * - Predictive Coding: Top-down predictions meet bottom-up signals
 * - Homeostatic Regulation: Feedback maintains stability
 *
 * This module extends the basic feedback types in nimcp_self_awareness_coordinator.h
 * with detailed policies and analysis capabilities.
 */

#ifndef NIMCP_SELF_AWARENESS_FEEDBACK_H
#define NIMCP_SELF_AWARENESS_FEEDBACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/nimcp_self_awareness_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

/** Maximum history entries per feedback loop */
#define FEEDBACK_MAX_HISTORY_ENTRIES 128

/** Maximum number of trend samples */
#define FEEDBACK_MAX_TREND_SAMPLES 32

/** Default feedback learning rate */
#define FEEDBACK_DEFAULT_LEARNING_RATE 0.1f

/** Default feedback momentum */
#define FEEDBACK_DEFAULT_MOMENTUM 0.9f

/** Default feedback decay */
#define FEEDBACK_DEFAULT_DECAY 0.99f

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Transfer function types for feedback
 */
typedef enum {
    TRANSFER_LINEAR,           /**< Direct linear transfer */
    TRANSFER_SIGMOID,          /**< Sigmoid (smooth 0-1 mapping) */
    TRANSFER_TANH,             /**< Tanh (-1 to 1 mapping) */
    TRANSFER_EXPONENTIAL,      /**< Exponential (emphasize large values) */
    TRANSFER_LOGARITHMIC,      /**< Logarithmic (compress large values) */
    TRANSFER_STEP,             /**< Binary threshold */
    TRANSFER_GATED             /**< Only transfer if gate open */
} transfer_function_t;

/**
 * @brief Feedback trend direction
 */
typedef enum {
    TREND_STABLE,              /**< No significant change */
    TREND_INCREASING,          /**< Values increasing */
    TREND_DECREASING,          /**< Values decreasing */
    TREND_OSCILLATING,         /**< Values bouncing up and down */
    TREND_DIVERGING            /**< Values becoming unstable */
} feedback_trend_t;

/**
 * @brief Feedback health status
 */
typedef enum {
    FEEDBACK_HEALTH_OPTIMAL,   /**< Loop functioning well */
    FEEDBACK_HEALTH_DEGRADED,  /**< Some issues detected */
    FEEDBACK_HEALTH_FAILING,   /**< Significant problems */
    FEEDBACK_HEALTH_DEAD       /**< Loop not functioning */
} feedback_health_t;

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Single feedback transfer record
 */
typedef struct {
    uint64_t timestamp_ms;             /**< When transfer occurred */
    feedback_loop_type_t loop_type;    /**< Which loop */

    float source_value;                /**< Value from source component */
    float transferred_value;           /**< Value after transfer function */
    float target_delta;                /**< Change induced in target */

    float latency_ms;                  /**< Transfer latency */
    bool successful;                   /**< Did transfer succeed? */
    char error_msg[64];                /**< Error message if failed */
} feedback_transfer_record_t;

/**
 * @brief Feedback loop policy configuration
 */
typedef struct {
    transfer_function_t transfer_func; /**< How to transform values */

    float learning_rate;               /**< How fast to apply feedback */
    float momentum;                    /**< Carry momentum from previous */
    float decay;                       /**< Decay old values */

    float min_threshold;               /**< Ignore values below this */
    float max_threshold;               /**< Clamp values above this */

    float gate_threshold;              /**< For TRANSFER_GATED: open above this */
    bool gate_open;                    /**< Current gate state */

    bool adaptive_rate;                /**< Adjust rate based on stability */
    float rate_increase_factor;        /**< How much to increase rate */
    float rate_decrease_factor;        /**< How much to decrease rate */

    uint32_t cooldown_ms;              /**< Min time between transfers */
} feedback_policy_t;

/**
 * @brief Feedback loop analysis results
 */
typedef struct {
    feedback_trend_t trend;            /**< Current trend direction */
    feedback_health_t health;          /**< Health assessment */

    float mean_value;                  /**< Mean transfer value */
    float variance;                    /**< Variance of values */
    float min_value;                   /**< Minimum observed */
    float max_value;                   /**< Maximum observed */

    float success_rate;                /**< Percentage successful */
    float avg_latency_ms;              /**< Average latency */
    float max_latency_ms;              /**< Maximum latency */

    float trend_slope;                 /**< Trend line slope */
    float trend_confidence;            /**< Confidence in trend */

    uint64_t time_since_last_ms;       /**< Time since last transfer */
    bool is_stale;                     /**< No recent transfers */
} feedback_analysis_t;

/**
 * @brief Feedback history ring buffer
 */
typedef struct {
    feedback_transfer_record_t records[FEEDBACK_MAX_HISTORY_ENTRIES];
    uint32_t head;                     /**< Next write position */
    uint32_t count;                    /**< Number of valid entries */
    uint32_t capacity;                 /**< Maximum entries */
} feedback_history_t;

/**
 * @brief Complete feedback loop manager for one loop
 */
typedef struct {
    feedback_loop_type_t type;         /**< Which loop this manages */

    feedback_policy_t policy;          /**< Loop policy */
    feedback_history_t history;        /**< Transfer history */
    feedback_analysis_t analysis;      /**< Latest analysis */

    /* Momentum state */
    float momentum_value;              /**< Current momentum */
    float ema_value;                   /**< Exponential moving average */

    /* Timing */
    uint64_t last_transfer_ms;         /**< Last transfer timestamp */
    uint64_t last_analysis_ms;         /**< Last analysis timestamp */

    /* Adaptive rate state */
    float current_learning_rate;       /**< Current (possibly adapted) rate */
    uint32_t consecutive_failures;     /**< For rate adaptation */
    uint32_t consecutive_successes;    /**< For rate adaptation */
} feedback_loop_manager_t;

/**
 * @brief Feedback system managing all loops
 */
typedef struct {
    feedback_loop_manager_t loops[FEEDBACK_LOOP_COUNT];

    /* Global state */
    uint64_t total_transfers;          /**< Total across all loops */
    uint64_t total_failures;           /**< Total failures */

    /* Analysis interval */
    uint32_t analysis_interval_ms;     /**< How often to analyze */
    uint64_t last_global_analysis_ms;  /**< Last global analysis */

    bool initialized;                  /**< Is system initialized? */
} feedback_system_t;

// ============================================================================
// Policy Functions
// ============================================================================

/**
 * @brief Get default feedback policy
 *
 * WHAT: Returns sensible default policy for feedback loops
 * WHY: Easy setup without manual configuration
 * HOW: Linear transfer, moderate learning rate, no gating
 *
 * @param policy Output policy structure
 * @return 0 on success, negative on error
 */
int feedback_default_policy(feedback_policy_t* policy);

/**
 * @brief Get conservative feedback policy
 *
 * WHAT: Returns slow, stable policy for critical loops
 * WHY: Some loops need conservative updates (e.g., identity beliefs)
 * HOW: Low learning rate, high momentum, sigmoid transfer
 *
 * @param policy Output policy structure
 * @return 0 on success, negative on error
 */
int feedback_conservative_policy(feedback_policy_t* policy);

/**
 * @brief Get aggressive feedback policy
 *
 * WHAT: Returns fast policy for rapid adaptation
 * WHY: Some loops need quick response (e.g., attention guidance)
 * HOW: High learning rate, low momentum, linear transfer
 *
 * @param policy Output policy structure
 * @return 0 on success, negative on error
 */
int feedback_aggressive_policy(feedback_policy_t* policy);

/**
 * @brief Get gated feedback policy
 *
 * WHAT: Returns gated policy (only transfers when gate is open)
 * WHY: Some loops should only work under certain conditions
 * HOW: TRANSFER_GATED with configurable threshold
 *
 * @param policy Output policy structure
 * @param gate_threshold Threshold for opening gate
 * @return 0 on success, negative on error
 */
int feedback_gated_policy(feedback_policy_t* policy, float gate_threshold);

// ============================================================================
// Feedback System Functions
// ============================================================================

/**
 * @brief Initialize feedback system
 *
 * WHAT: Initializes the feedback management system
 * WHY: Required before using feedback functions
 * HOW: Sets up all loop managers with default policies
 *
 * @param system Feedback system to initialize
 * @return 0 on success, negative on error
 */
int feedback_system_init(feedback_system_t* system);

/**
 * @brief Cleanup feedback system
 *
 * WHAT: Cleans up feedback system resources
 * WHY: Prevent memory leaks
 * HOW: Resets all state to uninitialized
 *
 * @param system Feedback system to cleanup
 */
void feedback_system_cleanup(feedback_system_t* system);

/**
 * @brief Set policy for specific loop
 *
 * WHAT: Configures policy for one feedback loop
 * WHY: Different loops need different behaviors
 * HOW: Copies policy to loop manager
 *
 * @param system Feedback system
 * @param loop_type Which loop to configure
 * @param policy Policy to apply
 * @return 0 on success, negative on error
 */
int feedback_set_policy(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    const feedback_policy_t* policy
);

/**
 * @brief Get policy for specific loop
 *
 * @param system Feedback system
 * @param loop_type Which loop to query
 * @param policy Output policy
 * @return 0 on success, negative on error
 */
int feedback_get_policy(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type,
    feedback_policy_t* policy
);

// ============================================================================
// Transfer Functions
// ============================================================================

/**
 * @brief Apply transfer function to value
 *
 * WHAT: Transforms a value using specified transfer function
 * WHY: Different loops need different value transformations
 * HOW: Applies mathematical function based on type
 *
 * @param value Input value
 * @param func Transfer function type
 * @param gate_threshold Threshold for gated transfer
 * @param gate_open Gate state for gated transfer
 * @return Transformed value
 */
float feedback_apply_transfer(
    float value,
    transfer_function_t func,
    float gate_threshold,
    bool gate_open
);

/**
 * @brief Record a feedback transfer
 *
 * WHAT: Logs a feedback transfer to history
 * WHY: History enables analysis and debugging
 * HOW: Adds record to ring buffer
 *
 * @param system Feedback system
 * @param loop_type Which loop performed transfer
 * @param source_value Value from source
 * @param transferred_value Value after transfer function
 * @param target_delta Change induced in target
 * @param latency_ms Transfer latency
 * @param successful Whether transfer succeeded
 * @param error_msg Error message if failed (can be NULL)
 * @return 0 on success, negative on error
 */
int feedback_record_transfer(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    float source_value,
    float transferred_value,
    float target_delta,
    float latency_ms,
    bool successful,
    const char* error_msg
);

/**
 * @brief Compute feedback value with policy
 *
 * WHAT: Computes final feedback value applying all policy rules
 * WHY: Centralized computation of policy-adjusted values
 * HOW: Applies transfer, learning rate, momentum, clamping
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @param raw_value Raw input value
 * @param output Output computed value
 * @return 0 on success, negative on error
 */
int feedback_compute_value(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    float raw_value,
    float* output
);

// ============================================================================
// Analysis Functions
// ============================================================================

/**
 * @brief Analyze feedback loop
 *
 * WHAT: Performs statistical analysis on feedback loop history
 * WHY: Understand loop behavior and detect issues
 * HOW: Computes statistics from history ring buffer
 *
 * @param system Feedback system
 * @param loop_type Which loop to analyze
 * @param analysis Output analysis results
 * @return 0 on success, negative on error
 */
int feedback_analyze_loop(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    feedback_analysis_t* analysis
);

/**
 * @brief Analyze all feedback loops
 *
 * WHAT: Updates analysis for all loops
 * WHY: Periodic health check of all feedback
 * HOW: Calls feedback_analyze_loop for each loop
 *
 * @param system Feedback system
 * @return 0 on success, negative on error
 */
int feedback_analyze_all(feedback_system_t* system);

/**
 * @brief Get loop health
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @return Health status
 */
feedback_health_t feedback_get_health(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type
);

/**
 * @brief Get loop trend
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @return Trend direction
 */
feedback_trend_t feedback_get_trend(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type
);

/**
 * @brief Check if any loops are unhealthy
 *
 * @param system Feedback system
 * @return True if any loop is FAILING or DEAD
 */
bool feedback_has_unhealthy_loops(const feedback_system_t* system);

// ============================================================================
// History Functions
// ============================================================================

/**
 * @brief Get recent history for loop
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @param records Output array of records
 * @param max_records Maximum records to return
 * @param count Output number of records returned
 * @return 0 on success, negative on error
 */
int feedback_get_history(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type,
    feedback_transfer_record_t* records,
    uint32_t max_records,
    uint32_t* count
);

/**
 * @brief Clear history for loop
 *
 * @param system Feedback system
 * @param loop_type Which loop (or FEEDBACK_LOOP_COUNT for all)
 * @return 0 on success, negative on error
 */
int feedback_clear_history(
    feedback_system_t* system,
    feedback_loop_type_t loop_type
);

// ============================================================================
// Gate Functions
// ============================================================================

/**
 * @brief Open gate for gated loop
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @return 0 on success, negative on error
 */
int feedback_open_gate(
    feedback_system_t* system,
    feedback_loop_type_t loop_type
);

/**
 * @brief Close gate for gated loop
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @return 0 on success, negative on error
 */
int feedback_close_gate(
    feedback_system_t* system,
    feedback_loop_type_t loop_type
);

/**
 * @brief Check if gate is open
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @return True if gate is open
 */
bool feedback_is_gate_open(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type
);

// ============================================================================
// Adaptive Rate Functions
// ============================================================================

/**
 * @brief Enable adaptive learning rate for loop
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @param increase_factor Factor to increase rate on success
 * @param decrease_factor Factor to decrease rate on failure
 * @return 0 on success, negative on error
 */
int feedback_enable_adaptive_rate(
    feedback_system_t* system,
    feedback_loop_type_t loop_type,
    float increase_factor,
    float decrease_factor
);

/**
 * @brief Disable adaptive learning rate for loop
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @return 0 on success, negative on error
 */
int feedback_disable_adaptive_rate(
    feedback_system_t* system,
    feedback_loop_type_t loop_type
);

/**
 * @brief Get current (adapted) learning rate for loop
 *
 * @param system Feedback system
 * @param loop_type Which loop
 * @return Current learning rate
 */
float feedback_get_current_rate(
    const feedback_system_t* system,
    feedback_loop_type_t loop_type
);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get string name for transfer function
 *
 * @param func Transfer function type
 * @return String name
 */
const char* feedback_transfer_name(transfer_function_t func);

/**
 * @brief Get string name for trend
 *
 * @param trend Trend direction
 * @return String name
 */
const char* feedback_trend_name(feedback_trend_t trend);

/**
 * @brief Get string name for health
 *
 * @param health Health status
 * @return String name
 */
const char* feedback_health_name(feedback_health_t health);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_AWARENESS_FEEDBACK_H */
