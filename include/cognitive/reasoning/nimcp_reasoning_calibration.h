/**
 * @file nimcp_reasoning_calibration.h
 * @brief Confidence Calibration Learning for Reasoning Contributors
 *
 * WHAT: Online calibration system that learns per-contributor accuracy from
 *       reasoning outcomes and adjusts confidence scaling accordingly
 * WHY:  Contributors (hippocampus, semantic memory, parietal, etc.) submit
 *       evidence with confidence values taken at face value. This system
 *       learns which contributors are reliable and scales their confidence
 *       to improve overall reasoning accuracy over time
 * HOW:  Per-contributor tracking with exponential moving average of
 *       prediction error, multiplicative scale + additive bias adjustment,
 *       minimum observation threshold before adjustments apply
 *
 * BIOLOGICAL BASIS:
 * Models the brain's metacognitive calibration — the anterior cingulate
 * cortex and prefrontal cortex learn to weight evidence sources based on
 * their historical reliability, a form of Bayesian source credibility
 *
 * DESIGN PRINCIPLES:
 * - Thread-safe: concurrent contributors can record simultaneously
 * - Graceful defaults: unknown contributors get scale=1.0, bias=0.0
 * - Conservative: min_predictions_before_adjust prevents premature tuning
 * - Bounded: scale clamped to [min_scale, max_scale] to prevent runaway
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_REASONING_CALIBRATION_H
#define NIMCP_REASONING_CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum number of calibrated contributors */
#define REASONING_MAX_CALIBRATED_CONTRIBUTORS 128

/** Default learning rate for EMA error tracking */
#define REASONING_DEFAULT_CALIBRATION_LEARNING_RATE 0.05f

/** Default history size (not used for ring buffer, but for config) */
#define REASONING_DEFAULT_CALIBRATION_HISTORY 100

/** Contributor name maximum length */
#define REASONING_CALIBRATION_NAME_LEN 64

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Single calibration observation record
 *
 * WHAT: One prediction-outcome pair for a contributor
 * WHY:  Track what was predicted vs what actually happened
 */
typedef struct {
    char contributor_name[REASONING_CALIBRATION_NAME_LEN];
    float predicted_confidence;   /**< Confidence the contributor reported */
    float actual_outcome;         /**< 0.0 = wrong, 1.0 = correct */
    uint64_t timestamp_us;        /**< When this record was created */
} calibration_record_t;

/**
 * @brief Per-contributor calibration state
 *
 * WHAT: Accumulated calibration statistics for one contributor
 * WHY:  Track reliability and compute adjustments
 */
typedef struct {
    char contributor_name[REASONING_CALIBRATION_NAME_LEN];
    uint32_t total_predictions;   /**< Total observations recorded */
    uint32_t correct_predictions; /**< Predictions where outcome > 0.5 */
    float confidence_scale;       /**< Multiplicative adjustment (starts 1.0) */
    float confidence_bias;        /**< Additive adjustment (starts 0.0) */
    float ema_error;              /**< EMA of |predicted - actual| */
    float reliability_score;      /**< correct / total (0.0 if no predictions) */
} contributor_calibration_t;

/**
 * @brief Configuration for the calibration system
 *
 * WHAT: Tuneable parameters for calibration behavior
 * WHY:  Allow domain-specific tuning of learning speed and bounds
 */
typedef struct {
    bool enabled;                      /**< Master enable flag */
    float learning_rate;               /**< EMA smoothing factor (default 0.05) */
    uint32_t history_size;             /**< Config history size (default 100) */
    uint32_t min_predictions_before_adjust; /**< Min observations before adjusting (default 5) */
    float max_scale;                   /**< Maximum scale factor (default 2.0) */
    float min_scale;                   /**< Minimum scale factor (default 0.1) */
} calibration_config_t;

/**
 * @brief Aggregate calibration statistics
 *
 * WHAT: Summary statistics across all tracked contributors
 * WHY:  Monitor calibration system performance
 */
typedef struct {
    uint32_t total_records;                /**< Total observations recorded */
    uint32_t total_contributors_tracked;   /**< Number of unique contributors */
    float avg_reliability;                 /**< Mean reliability across contributors */
    char best_contributor_name[REASONING_CALIBRATION_NAME_LEN];   /**< Highest reliability */
    char worst_contributor_name[REASONING_CALIBRATION_NAME_LEN];  /**< Lowest reliability */
    float avg_scale_factor;                /**< Mean scale factor across contributors */
} calibration_stats_t;

/**
 * @brief Opaque calibration system handle
 */
typedef struct reasoning_calibration reasoning_calibration_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create a calibration system
 *
 * WHAT: Allocate and initialize a calibration system instance
 * WHY:  Required before recording any calibration data
 * HOW:  Allocate struct, copy config, create mutex, zero state
 *
 * @param config Configuration (NULL for defaults)
 * @return Calibration system or NULL on allocation failure
 *
 * COMPLEXITY: O(1)
 */
reasoning_calibration_t* reasoning_calibration_create(
    const calibration_config_t* config);

/**
 * @brief Destroy a calibration system
 *
 * WHAT: Free all calibration resources
 * WHY:  Prevent memory leaks
 *
 * @param cal Calibration system (NULL safe)
 */
void reasoning_calibration_destroy(reasoning_calibration_t* cal);

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Get default calibration configuration
 *
 * WHAT: Return configuration with sensible defaults
 * WHY:  Simplify creation with proven parameters
 *
 * @return Default configuration struct
 */
calibration_config_t reasoning_calibration_default_config(void);

/*=============================================================================
 * CORE OPERATIONS
 *===========================================================================*/

/**
 * @brief Record a prediction-outcome pair for a contributor
 *
 * WHAT: Update calibration state with a new observation
 * WHY:  Online learning of contributor reliability
 * HOW:  Find/create contributor entry, update EMA error,
 *       reliability score, and scale/bias adjustments
 *
 * @param cal Calibration system
 * @param contributor_name Contributor identifier
 * @param predicted_confidence Confidence the contributor reported [0-1]
 * @param actual_outcome Actual outcome (0.0=wrong, 1.0=correct)
 * @return 0 on success, -1 on error
 */
int reasoning_calibration_record(reasoning_calibration_t* cal,
                                  const char* contributor_name,
                                  float predicted_confidence,
                                  float actual_outcome);

/**
 * @brief Get scale and bias adjustments for a contributor
 *
 * WHAT: Look up learned adjustments for confidence calibration
 * WHY:  Apply learned corrections to contributor evidence
 * HOW:  Find contributor, return scale and bias; unknown returns 1.0, 0.0
 *
 * @param cal Calibration system
 * @param contributor_name Contributor identifier
 * @param scale_out Output: multiplicative scale factor
 * @param bias_out Output: additive bias
 * @return 0 on success, -1 on error (NULL inputs)
 */
int reasoning_calibration_get_adjustment(const reasoning_calibration_t* cal,
                                          const char* contributor_name,
                                          float* scale_out,
                                          float* bias_out);

/**
 * @brief Get per-contributor calibration statistics
 *
 * @param cal Calibration system
 * @param contributor_name Contributor to query
 * @param out Output contributor stats
 * @return 0 on success, -1 on error or not found
 */
int reasoning_calibration_get_contributor_stats(
    const reasoning_calibration_t* cal,
    const char* contributor_name,
    contributor_calibration_t* out);

/**
 * @brief Get aggregate calibration statistics
 *
 * @param cal Calibration system
 * @param stats Output statistics struct
 * @return 0 on success, -1 on error
 */
int reasoning_calibration_get_stats(const reasoning_calibration_t* cal,
                                     calibration_stats_t* stats);

/**
 * @brief Reset all calibration state
 *
 * WHAT: Clear all contributor data, starting fresh
 * WHY:  Allow recalibration from scratch
 *
 * @param cal Calibration system
 * @return 0 on success, -1 on error
 */
int reasoning_calibration_reset(reasoning_calibration_t* cal);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_CALIBRATION_H */
