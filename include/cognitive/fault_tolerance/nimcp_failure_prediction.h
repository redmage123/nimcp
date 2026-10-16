/**
 * @file nimcp_failure_prediction.h
 * @brief Predictive Coding for Failure Prediction Module
 * @version 1.0.0
 * @date 2025-11-20
 *
 * WHAT: Predict failures before they occur using leading indicators and predictive models
 * WHY:  Prevention is better than recovery - avoid crashes and instability
 * HOW:  Track metrics, calculate derivatives, predict failure probability and time
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex predictive coding (anticipate future states)
 * - Anterior cingulate cortex error detection (monitor for anomalies)
 * - Dorsolateral PFC planning (preventive action selection)
 *
 * INTEGRATION POINTS:
 * 1. Brain Structure (src/core/brain/nimcp_brain.c)
 *    - Add: failure_predictor_t* failure_predictor;
 *    - Init: brain_create_custom()
 *    - Cleanup: brain_destroy()
 *
 * 2. Configuration (src/core/brain/nimcp_brain.h)
 *    - Add: bool enable_failure_prediction;
 *    - Add: failure_predictor_config_t predictor_config;
 *
 * 3. Health Monitoring (cognitive/fault_tolerance/)
 *    - Integrate with health monitoring system
 *    - Update metrics periodically
 *    - Trigger preventive actions
 *
 * DEPENDENCIES:
 * - utils/memory/nimcp_memory.h (memory allocation)
 * - utils/logging/nimcp_logging.h (logging)
 *
 * DEPENDENT MODULES:
 * - Fault tolerance system (uses predictions for recovery)
 * - Health monitoring (provides metrics)
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 * @version 1.0.0
 */

#ifndef NIMCP_FAILURE_PREDICTION_H
#define NIMCP_FAILURE_PREDICTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define FAILURE_PREDICTOR_DEFAULT_MAX_PREDICTIONS 10
#define FAILURE_PREDICTOR_DEFAULT_MAX_INDICATORS 20
#define FAILURE_PREDICTOR_DEFAULT_THRESHOLD 0.8f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Types of failures that can be predicted
 *
 * WHAT: Categories of system failures
 * WHY:  Allow type-specific responses
 */
typedef enum {
    FAILURE_TYPE_OOM = 0,                      /**< Out of memory */
    FAILURE_TYPE_GRADIENT_EXPLOSION = 1,       /**< Training instability */
    FAILURE_TYPE_DIVERGENCE = 2,               /**< Model divergence */
    FAILURE_TYPE_PERFORMANCE_DEGRADATION = 3,  /**< Throughput drop */
    FAILURE_TYPE_ERROR_RATE_SPIKE = 4,         /**< High error rate */
    FAILURE_TYPE_THREAD_DEADLOCK = 5,          /**< Threading issue */
    FAILURE_TYPE_DISK_FULL = 6,                /**< Storage exhaustion */
    FAILURE_TYPE_NETWORK_TIMEOUT = 7,          /**< Network failure */
    FAILURE_TYPE_CUSTOM = 8                    /**< Custom failure type */
} failure_type_t;

/**
 * @brief Confidence level in prediction
 *
 * WHAT: How certain the predictor is
 * WHY:  Allow threshold-based actions
 */
typedef enum {
    CONFIDENCE_LOW = 0,        /**< < 50% confidence */
    CONFIDENCE_MEDIUM = 1,     /**< 50-75% confidence */
    CONFIDENCE_HIGH = 2,       /**< 75-90% confidence */
    CONFIDENCE_VERY_HIGH = 3   /**< > 90% confidence */
} confidence_level_t;

/**
 * @brief Metric types for leading indicators
 *
 * WHAT: Monitored system metrics
 * WHY:  Track different aspects of health
 */
typedef enum {
    METRIC_TYPE_MEMORY = 0,      /**< Memory usage */
    METRIC_TYPE_LATENCY = 1,     /**< Response time */
    METRIC_TYPE_ERROR = 2,       /**< Error rate */
    METRIC_TYPE_THROUGHPUT = 3,  /**< Processing rate */
    METRIC_TYPE_GRADIENT = 4,    /**< Gradient norm */
    METRIC_TYPE_LOSS = 5,        /**< Training loss */
    METRIC_TYPE_CACHE_HIT = 6,   /**< Cache hit rate */
    METRIC_TYPE_THREAD_WAIT = 7, /**< Thread wait time */
    METRIC_TYPE_CUSTOM = 8       /**< Custom metric */
} metric_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for failure predictor
 *
 * WHAT: Customizable predictor parameters
 * WHY:  Allow tuning for different systems
 */
typedef struct {
    uint32_t max_predictions;                    /**< Max simultaneous predictions */
    uint32_t max_indicators;                     /**< Max tracked indicators */
    float prediction_threshold;                  /**< Min probability to report [0,1] */
    bool enable_memory_leak_detection;           /**< Enable memory leak prediction */
    bool enable_gradient_explosion_detection;    /**< Enable gradient explosion detection */
} failure_predictor_config_t;

/**
 * @brief Leading indicator with derivatives
 *
 * WHAT: Metric value with rate of change and acceleration
 * WHY:  Predict based on trends, not just current values
 * HOW:  Track current value, first derivative, second derivative
 */
typedef struct {
    metric_type_t metric;       /**< Type of metric */
    float current_value;        /**< Current metric value */
    float threshold;            /**< Alarm threshold */
    float rate_of_change;       /**< First derivative (velocity) */
    float acceleration;         /**< Second derivative */
    uint64_t last_update_ms;    /**< Last update timestamp */
} leading_indicator_t;

/**
 * @brief Failure prediction with details
 *
 * WHAT: Predicted failure with probability and time estimate
 * WHY:  Allow preventive action planning
 */
typedef struct {
    failure_type_t type;             /**< Type of predicted failure */
    float probability;               /**< Probability [0, 1] */
    uint64_t estimated_time_ms;      /**< Time until failure (milliseconds) */
    confidence_level_t confidence;   /**< Confidence in prediction */
    const char* reasoning;           /**< Human-readable explanation */
} failure_prediction_t;

/**
 * @brief Health metrics for prediction input
 *
 * WHAT: System health snapshot
 * WHY:  Input to prediction algorithm
 */
typedef struct {
    uint64_t memory_usage;      /**< Current memory usage (bytes) */
    uint64_t peak_memory;       /**< Peak memory usage (bytes) */
    float gradient_norm;        /**< L2 norm of gradients */
    float loss_value;           /**< Training loss */
    float learning_rate;        /**< Current learning rate */
    float throughput;           /**< Operations per second */
    float error_rate;           /**< Error rate [0, 1] */
    uint64_t timestamp_ms;      /**< Timestamp (milliseconds) */
} failure_health_metrics_t;

/**
 * @brief Opaque failure predictor handle
 *
 * WHAT: Predictor instance (opaque pointer)
 * WHY:  Encapsulation and data hiding
 */
typedef struct failure_predictor failure_predictor_t;

//=============================================================================
// Core Functions
//=============================================================================

/**
 * @brief Create failure predictor with default configuration
 *
 * WHAT: Allocate and initialize predictor
 * WHY:  Simple creation for most use cases
 * HOW:  Uses default configuration values
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~2 KB for predictor structure
 *
 * @return Predictor instance or NULL on failure
 */
failure_predictor_t* failure_predictor_create(void);

/**
 * @brief Create failure predictor with custom configuration
 *
 * WHAT: Allocate and initialize predictor with custom settings
 * WHY:  Allow tuning for specific use cases
 * HOW:  Apply provided configuration
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~2 KB base + indicator/prediction arrays
 *
 * @param config Configuration (non-NULL)
 * @return Predictor instance or NULL on failure
 */
failure_predictor_t* failure_predictor_create_custom(
    const failure_predictor_config_t* config
);

/**
 * @brief Destroy failure predictor
 *
 * WHAT: Free all predictor resources
 * WHY:  Prevent memory leaks
 * HOW:  Free indicators, predictions, and predictor structure
 *
 * COMPLEXITY: O(n) where n = number of indicators
 * MEMORY: Frees all allocated memory
 *
 * @param predictor Predictor to destroy (NULL safe)
 */
void failure_predictor_destroy(failure_predictor_t* predictor);

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible default configuration
 * WHY:  Simplify creation
 * HOW:  Return struct with default values
 *
 * @return Default configuration struct
 */
failure_predictor_config_t failure_predictor_default_config(void);

//=============================================================================
// Indicator Management
//=============================================================================

/**
 * @brief Update single leading indicator
 *
 * WHAT: Update metric value and calculate derivatives
 * WHY:  Track trends for prediction
 * HOW:  Update value, calculate rate of change and acceleration
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param predictor Predictor instance (non-NULL)
 * @param metric Metric type to update
 * @param current_value Current metric value
 * @param threshold Alarm threshold for this metric
 * @return true on success, false on error
 */
bool failure_predictor_update_indicator(
    failure_predictor_t* predictor,
    metric_type_t metric,
    float current_value,
    float threshold
);

/**
 * @brief Update all indicators from health metrics
 *
 * WHAT: Bulk update all indicators from health snapshot
 * WHY:  Convenience function for common use case
 * HOW:  Extract metrics and update corresponding indicators
 *
 * COMPLEXITY: O(m) where m = number of metric types
 * MEMORY: O(1)
 *
 * @param predictor Predictor instance (non-NULL)
 * @param metrics Health metrics snapshot (non-NULL)
 * @return true on success, false on error
 */
bool failure_predictor_update_from_health_metrics(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
);

/**
 * @brief Get specific indicator
 *
 * WHAT: Retrieve indicator for specific metric
 * WHY:  Inspect current state
 * HOW:  Find indicator by metric type
 *
 * COMPLEXITY: O(n) where n = number of indicators
 * MEMORY: O(1)
 *
 * @param predictor Predictor instance (non-NULL)
 * @param metric Metric type to find
 * @param indicator Output indicator (non-NULL)
 * @return true if found, false otherwise
 */
bool failure_predictor_get_indicator(
    failure_predictor_t* predictor,
    metric_type_t metric,
    leading_indicator_t* indicator
);

/**
 * @brief Get all indicators
 *
 * WHAT: Retrieve all current indicators
 * WHY:  Full system inspection
 * HOW:  Allocate array and copy indicators
 *
 * COMPLEXITY: O(n) where n = number of indicators
 * MEMORY: Allocates array (caller must free with nimcp_free)
 *
 * @param predictor Predictor instance (non-NULL)
 * @param count Output indicator count (non-NULL)
 * @return Array of indicators (must be freed) or NULL on error
 */
leading_indicator_t* failure_predictor_get_all_indicators(
    failure_predictor_t* predictor,
    uint32_t* count
);

//=============================================================================
// Prediction Functions
//=============================================================================

/**
 * @brief Predict failures from current indicators
 *
 * WHAT: Analyze indicators and generate failure predictions
 * WHY:  Core prediction functionality
 * HOW:  Evaluate trends, calculate probabilities, estimate time
 *
 * COMPLEXITY: O(n*m) where n = indicators, m = failure types
 * MEMORY: Allocates prediction array (caller must free)
 *
 * @param predictor Predictor instance (non-NULL)
 * @param metrics Current health metrics (non-NULL)
 * @return Array of predictions (must be freed) or NULL on error
 *
 * @note Predictions are sorted by probability (highest first)
 * @note Only predictions above threshold are returned
 */
failure_prediction_t* failure_predictor_predict(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
);

/**
 * @brief Get prediction count
 *
 * WHAT: Number of current predictions
 * WHY:  Determine array size after predict()
 *
 * @param predictor Predictor instance (non-NULL)
 * @return Number of predictions
 */
uint32_t failure_predictor_get_prediction_count(
    failure_predictor_t* predictor
);

/**
 * @brief Get prediction by failure type
 *
 * WHAT: Find specific prediction type
 * WHY:  Check for specific failure
 *
 * @param predictor Predictor instance (non-NULL)
 * @param type Failure type to find
 * @param prediction Output prediction (non-NULL)
 * @return true if found, false otherwise
 */
bool failure_predictor_get_prediction_by_type(
    failure_predictor_t* predictor,
    failure_type_t type,
    failure_prediction_t* prediction
);

/**
 * @brief Get highest probability prediction (DEPRECATED - UNSAFE)
 *
 * CRITICAL THREAD SAFETY WARNING - POINTER-AFTER-UNLOCK ISSUE:
 * This function returned a pointer to internal data after releasing the lock.
 * It is DEPRECATED and now always returns NULL.
 *
 * Use failure_predictor_get_highest_probability_prediction_copy() instead.
 *
 * @param predictor Predictor instance (non-NULL)
 * @return Always returns NULL (deprecated)
 *
 * @deprecated Use failure_predictor_get_highest_probability_prediction_copy()
 */
failure_prediction_t* failure_predictor_get_highest_probability_prediction(
    failure_predictor_t* predictor
);

/**
 * @brief Get thread-safe copy of highest probability prediction
 *
 * WHAT: Get copy of highest probability prediction, safe for multi-threaded access
 * WHY:  Original function returned pointer that could be invalidated
 * HOW:  Copy prediction data while holding lock
 *
 * This is the RECOMMENDED API for accessing the highest probability prediction.
 *
 * @param predictor Predictor instance (non-NULL)
 * @param out_prediction Output: copy of prediction data (caller-allocated)
 * @return true if prediction found and copied, false if no predictions or error
 */
bool failure_predictor_get_highest_probability_prediction_copy(
    failure_predictor_t* predictor,
    failure_prediction_t* out_prediction
);

/**
 * @brief Clear all predictions
 *
 * WHAT: Reset prediction state
 * WHY:  Allow fresh predictions after recovery
 *
 * @param predictor Predictor instance (non-NULL)
 */
void failure_predictor_clear_predictions(failure_predictor_t* predictor);

//=============================================================================
// Specific Detection Functions
//=============================================================================

/**
 * @brief Detect memory leak
 *
 * WHAT: Identify sustained memory growth pattern
 * WHY:  Predict OOM before it happens
 * HOW:  Check if memory growth rate > threshold AND acceleration > 0
 *
 * ALGORITHM:
 * 1. Calculate memory growth rate (MB/sec)
 * 2. Check if rate > 10 MB/sec
 * 3. Check if acceleration positive (growth accelerating)
 * 4. Estimate time to OOM: (MAX_MEMORY - current) / growth_rate
 *
 * @param predictor Predictor instance (non-NULL)
 * @param metrics Current health metrics (non-NULL)
 * @return true if leak detected, false otherwise
 */
bool failure_predictor_detect_memory_leak(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
);

/**
 * @brief Detect gradient explosion
 *
 * WHAT: Identify exponentially growing gradients
 * WHY:  Prevent training divergence
 * HOW:  Check if gradient norm growing exponentially
 *
 * ALGORITHM:
 * 1. Calculate gradient norm rate of change
 * 2. Check if rate > 100 AND norm > 1000
 * 3. Estimate time to divergence based on growth rate
 *
 * @param predictor Predictor instance (non-NULL)
 * @param metrics Current health metrics (non-NULL)
 * @return true if explosion detected, false otherwise
 */
bool failure_predictor_detect_gradient_explosion(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
);

/**
 * @brief Estimate time to OOM
 *
 * WHAT: Calculate milliseconds until out of memory
 * WHY:  Allow preventive action timing
 * HOW:  Extrapolate current growth rate
 *
 * @param predictor Predictor instance (non-NULL)
 * @param metrics Current health metrics (non-NULL)
 * @return Estimated milliseconds to OOM (0 if no leak detected)
 */
uint64_t failure_predictor_estimate_time_to_oom(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
);

/**
 * @brief Estimate time to gradient explosion
 *
 * WHAT: Calculate milliseconds until divergence
 * WHY:  Allow preventive action timing
 *
 * @param predictor Predictor instance (non-NULL)
 * @param metrics Current health metrics (non-NULL)
 * @return Estimated milliseconds to explosion (0 if stable)
 */
uint64_t failure_predictor_estimate_time_to_explosion(
    failure_predictor_t* predictor,
    const failure_health_metrics_t* metrics
);

//=============================================================================
// Preventive Action Functions
//=============================================================================

/**
 * @brief Check if preventive action needed
 *
 * WHAT: Determine if prediction requires action
 * WHY:  Trigger preventive measures
 * HOW:  Check probability, confidence, and time estimate
 *
 * CRITERIA:
 * - Probability > 0.8 OR
 * - (Probability > 0.6 AND estimated_time < 10 seconds)
 *
 * @param predictor Predictor instance (non-NULL)
 * @param prediction Prediction to evaluate (non-NULL)
 * @return true if action needed, false otherwise
 */
bool failure_predictor_needs_prevention(
    failure_predictor_t* predictor,
    const failure_prediction_t* prediction
);

/**
 * @brief Get recommended preventive action
 *
 * WHAT: Suggest action for specific prediction
 * WHY:  Guide response to prediction
 * HOW:  Return type-specific recommendation
 *
 * RECOMMENDATIONS:
 * - OOM: "Trigger garbage collection" or "Free caches"
 * - GRADIENT_EXPLOSION: "Reduce learning rate" or "Apply gradient clipping"
 * - DIVERGENCE: "Restart from checkpoint"
 * - PERFORMANCE_DEGRADATION: "Reduce batch size"
 *
 * @param predictor Predictor instance (non-NULL)
 * @param prediction Prediction to get action for (non-NULL)
 * @return Recommended action string (do not free) or NULL
 */
const char* failure_predictor_get_preventive_action(
    failure_predictor_t* predictor,
    const failure_prediction_t* prediction
);

/**
 * @brief Get highest priority action from all predictions
 *
 * WHAT: Most urgent preventive action
 * WHY:  Decide what to do first
 * HOW:  Find highest probability prediction and return its action
 *
 * @param predictor Predictor instance (non-NULL)
 * @return Recommended action string (do not free) or NULL if no predictions
 */
const char* failure_predictor_get_highest_priority_action(
    failure_predictor_t* predictor
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_FAILURE_PREDICTION_H
