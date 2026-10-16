/**
 * @file nimcp_anomaly_detector.h
 * @brief ML-based anomaly detection using Bayesian networks for NIMCP security
 *
 * WHAT: Behavior-based anomaly detection system using Bayesian inference
 * WHY:  Detect malicious inputs and behavioral anomalies beyond pattern matching
 * HOW:  Extract features, train Bayesian network, compute anomaly scores
 *
 * FEATURES:
 * - Multi-dimensional feature extraction (content, behavior, timing)
 * - Online learning with incremental updates
 * - Adaptive thresholds based on learned baseline
 * - Explainable anomaly detection (which features triggered)
 * - Bio-async integration for real-time threat detection
 *
 * ARCHITECTURE:
 *   Input → Feature Extraction → Bayesian Network → Anomaly Score
 *                                      ↓
 *                             [Content] [Behavior] [Timing]
 *                                      ↓
 *                             Overall Anomaly Score
 *
 * BAYESIAN NETWORK STRUCTURE:
 *   [Length] [Entropy] [SpecialChars] [Ngrams] [Timing]
 *       \       |         |            /         /
 *        \      |         |           /         /
 *         v     v         v          v         v
 *        [Content Anomaly]    [Behavior Anomaly]
 *                  \                /
 *                   \              /
 *                    v            v
 *                  [Overall Anomaly]
 *
 * PERFORMANCE:
 * - Detection: < 1ms for typical inputs
 * - Memory: < 10MB for model
 * - Incremental learning without full retraining
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#ifndef NIMCP_ANOMALY_DETECTOR_H
#define NIMCP_ANOMALY_DETECTOR_H

#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * OPAQUE TYPES
 *============================================================================*/

/**
 * @brief Anomaly detector opaque handle
 */
typedef struct nimcp_anomaly_detector_internal* nimcp_anomaly_detector_t;

/**
 * @brief Bayesian network opaque handle
 */
typedef struct nimcp_bayesian_network_internal* nimcp_bayesian_network_t;

/*=============================================================================
 * CONSTANTS AND MAGIC NUMBERS
 *============================================================================*/

#define NIMCP_ANOMALY_DETECTOR_MAGIC 0x414E4F4D  /**< 'ANOM' */
#define NIMCP_BAYESIAN_NETWORK_MAGIC 0x42415945  /**< 'BAYE' */

/** Feature indices */
#define NIMCP_FEATURE_LENGTH 0
#define NIMCP_FEATURE_ENTROPY 1
#define NIMCP_FEATURE_ALPHA_RATIO 2
#define NIMCP_FEATURE_NUMERIC_RATIO 3
#define NIMCP_FEATURE_SPECIAL_RATIO 4
#define NIMCP_FEATURE_CONTROL_RATIO 5
#define NIMCP_FEATURE_BIGRAM_ENTROPY 6
#define NIMCP_FEATURE_TRIGRAM_ENTROPY 7
#define NIMCP_FEATURE_NESTING_DEPTH 8
#define NIMCP_FEATURE_REQUEST_RATE 9
#define NIMCP_FEATURE_BURST_SCORE 10
#define NIMCP_FEATURE_REPEAT_RATIO 11
#define NIMCP_FEATURE_COUNT 12

/** Bayesian network nodes */
#define NIMCP_BN_NODE_LENGTH 0
#define NIMCP_BN_NODE_ENTROPY 1
#define NIMCP_BN_NODE_SPECIAL_CHARS 2
#define NIMCP_BN_NODE_NGRAMS 3
#define NIMCP_BN_NODE_TIMING 4
#define NIMCP_BN_NODE_CONTENT_ANOMALY 5
#define NIMCP_BN_NODE_BEHAVIOR_ANOMALY 6
#define NIMCP_BN_NODE_OVERALL_ANOMALY 7
#define NIMCP_BN_NODE_COUNT 8

/** Feature trigger flags */
#define NIMCP_TRIGGER_LENGTH (1U << 0)
#define NIMCP_TRIGGER_ENTROPY (1U << 1)
#define NIMCP_TRIGGER_ALPHA_RATIO (1U << 2)
#define NIMCP_TRIGGER_NUMERIC_RATIO (1U << 3)
#define NIMCP_TRIGGER_SPECIAL_RATIO (1U << 4)
#define NIMCP_TRIGGER_CONTROL_RATIO (1U << 5)
#define NIMCP_TRIGGER_BIGRAM_ENTROPY (1U << 6)
#define NIMCP_TRIGGER_TRIGRAM_ENTROPY (1U << 7)
#define NIMCP_TRIGGER_NESTING_DEPTH (1U << 8)
#define NIMCP_TRIGGER_REQUEST_RATE (1U << 9)
#define NIMCP_TRIGGER_BURST_SCORE (1U << 10)
#define NIMCP_TRIGGER_REPEAT_RATIO (1U << 11)

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *============================================================================*/

/**
 * @brief Anomaly detector configuration
 *
 * WHAT: Configuration for anomaly detection system
 * WHY:  Customize thresholds and learning behavior
 * HOW:  Set thresholds, learning parameters, and feature toggles
 */
typedef struct {
    /** Anomaly thresholds */
    float content_anomaly_threshold;   /**< Threshold for content anomalies [0.0, 1.0] */
    float behavior_anomaly_threshold;  /**< Threshold for behavior anomalies [0.0, 1.0] */
    float timing_anomaly_threshold;    /**< Threshold for timing anomalies [0.0, 1.0] */
    float overall_anomaly_threshold;   /**< Overall threshold [0.0, 1.0] */

    /** Learning parameters */
    uint32_t learning_window_size;     /**< Samples for baseline (default: 1000) */
    float learning_rate;               /**< Learning rate for online updates (default: 0.01) */
    bool enable_adaptive_threshold;    /**< Adjust thresholds based on false positives */
    bool enable_online_learning;       /**< Continuously update model */

    /** Feature configuration */
    uint32_t max_input_length;         /**< Maximum input length to analyze */
    uint32_t max_ngram_size;           /**< Maximum n-gram size (2-5) */
    float timing_window_sec;           /**< Time window for rate calculation */

    /** Performance tuning */
    bool enable_caching;               /**< Cache feature computations */
    bool enable_fast_mode;             /**< Skip expensive features if score already high */

    /** Bio-async integration */
    bool enable_bio_async;             /**< Enable bio-async messaging */
    uint32_t alert_module_id;          /**< Module to notify of anomalies */
} nimcp_anomaly_config_t;

/*=============================================================================
 * RESULT STRUCTURES
 *============================================================================*/

/**
 * @brief Anomaly detection result
 *
 * WHAT: Result of anomaly detection with score and explanation
 * WHY:  Provide actionable information about detected anomalies
 * HOW:  Score, confidence, triggered features, and explanation
 */
typedef struct {
    float anomaly_score;               /**< 0.0 (normal) to 1.0 (highly anomalous) */
    float confidence;                  /**< Confidence in score [0.0, 1.0] */

    /** Component scores */
    float content_score;               /**< Content anomaly score */
    float behavior_score;              /**< Behavior anomaly score */
    float timing_score;                /**< Timing anomaly score */

    /** Triggered features */
    uint32_t triggered_features;       /**< Bitmask of NIMCP_TRIGGER_* flags */

    /** Explanation */
    char explanation[256];             /**< Human-readable explanation */

    /** Metadata */
    uint64_t timestamp_us;             /**< Detection timestamp */
    uint32_t sample_count;             /**< Number of training samples */
} nimcp_anomaly_result_t;

/**
 * @brief Anomaly detector statistics
 *
 * WHAT: Statistics about anomaly detection performance
 * WHY:  Monitor effectiveness and tune parameters
 * HOW:  Track detections, false positives, and performance metrics
 */
typedef struct {
    /** Detection counts */
    uint64_t total_detections;         /**< Total inputs analyzed */
    uint64_t anomalies_detected;       /**< Inputs flagged as anomalous */
    uint64_t false_positives;          /**< Known false positives */
    uint64_t false_negatives;          /**< Known false negatives */

    /** Training statistics */
    uint64_t training_samples;         /**< Total training samples */
    uint64_t normal_samples;           /**< Normal training samples */
    uint64_t anomalous_samples;        /**< Anomalous training samples */

    /** Performance metrics */
    float avg_detection_time_us;       /**< Average detection time */
    float max_detection_time_us;       /**< Maximum detection time */
    float precision;                   /**< TP / (TP + FP) */
    float recall;                      /**< TP / (TP + FN) */
    float f1_score;                    /**< 2 * (precision * recall) / (precision + recall) */

    /** Current state */
    float current_content_threshold;   /**< Current content threshold */
    float current_behavior_threshold;  /**< Current behavior threshold */
    float current_timing_threshold;    /**< Current timing threshold */
    uint64_t model_update_count;       /**< Number of model updates */
} nimcp_anomaly_stats_t;

/*=============================================================================
 * ANOMALY DETECTOR API
 *============================================================================*/

/**
 * @brief Get default anomaly detector configuration
 *
 * WHAT: Get default configuration with sensible defaults
 * WHY:  Simplify initialization for common use cases
 * HOW:  Return pre-configured structure
 *
 * @return Default configuration
 */
nimcp_anomaly_config_t nimcp_anomaly_detector_default_config(void);

/**
 * @brief Create anomaly detector
 *
 * WHAT: Initialize anomaly detection system
 * WHY:  Prepare Bayesian network and feature extractors
 * HOW:  Allocate structures, initialize BN, set thresholds
 *
 * @param config Configuration (NULL for defaults)
 * @return Detector handle or NULL on failure
 */
nimcp_anomaly_detector_t nimcp_anomaly_detector_create(const nimcp_anomaly_config_t* config);

/**
 * @brief Destroy anomaly detector
 *
 * WHAT: Clean up and free detector resources
 * WHY:  Prevent memory leaks
 * HOW:  Free all allocated memory, destroy BN
 *
 * @param detector Detector handle
 */
void nimcp_anomaly_detector_destroy(nimcp_anomaly_detector_t detector);

/**
 * @brief Analyze input for anomalies
 *
 * WHAT: Detect anomalies in input data
 * WHY:  Identify malicious or suspicious inputs
 * HOW:  Extract features, run inference, compute score
 *
 * PERFORMANCE: < 1ms for typical inputs
 *
 * @param detector Detector handle
 * @param input Input data to analyze
 * @param input_len Length of input
 * @param result Output: detection result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_anomaly_detect(
    nimcp_anomaly_detector_t detector,
    const void* input,
    size_t input_len,
    nimcp_anomaly_result_t* result
);

/**
 * @brief Train detector on labeled sample
 *
 * WHAT: Update model with known-good or known-bad sample
 * WHY:  Improve detection accuracy through supervised learning
 * HOW:  Extract features, update priors, adjust thresholds
 *
 * @param detector Detector handle
 * @param input Input data
 * @param input_len Length of input
 * @param is_normal true for normal, false for anomalous
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_anomaly_train(
    nimcp_anomaly_detector_t detector,
    const void* input,
    size_t input_len,
    bool is_normal
);

/**
 * @brief Get detector statistics
 *
 * WHAT: Retrieve performance and accuracy statistics
 * WHY:  Monitor and tune detection system
 * HOW:  Return accumulated statistics
 *
 * @param detector Detector handle
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_anomaly_get_stats(
    nimcp_anomaly_detector_t detector,
    nimcp_anomaly_stats_t* stats
);

/**
 * @brief Reset detector statistics
 *
 * WHAT: Clear statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero out counters, keep model
 *
 * @param detector Detector handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_anomaly_reset_stats(nimcp_anomaly_detector_t detector);

/**
 * @brief Update adaptive thresholds
 *
 * WHAT: Adjust thresholds based on feedback
 * WHY:  Reduce false positives/negatives
 * HOW:  Use false positive/negative counts to tune thresholds
 *
 * @param detector Detector handle
 * @param false_positive Increment false positive count
 * @param false_negative Increment false negative count
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_anomaly_update_thresholds(
    nimcp_anomaly_detector_t detector,
    bool false_positive,
    bool false_negative
);

/**
 * @brief Save detector model to file
 *
 * WHAT: Serialize model state to file
 * WHY:  Persist trained model across restarts
 * HOW:  Save BN parameters, thresholds, statistics
 *
 * @param detector Detector handle
 * @param filepath Path to save model
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_anomaly_save_model(
    nimcp_anomaly_detector_t detector,
    const char* filepath
);

/**
 * @brief Load detector model from file
 *
 * WHAT: Deserialize model state from file
 * WHY:  Restore trained model
 * HOW:  Load BN parameters, thresholds, statistics
 *
 * @param detector Detector handle
 * @param filepath Path to load model from
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_anomaly_load_model(
    nimcp_anomaly_detector_t detector,
    const char* filepath
);

/*=============================================================================
 * BAYESIAN NETWORK API
 *============================================================================*/

/**
 * @brief Create Bayesian network
 *
 * WHAT: Initialize Bayesian network for inference
 * WHY:  Model probabilistic relationships between features
 * HOW:  Allocate nodes, CPTs, initialize structures
 *
 * @param num_nodes Number of nodes in network
 * @return Network handle or NULL on failure
 */
nimcp_bayesian_network_t nimcp_bn_create(uint32_t num_nodes);

/**
 * @brief Destroy Bayesian network
 *
 * WHAT: Clean up and free network resources
 * WHY:  Prevent memory leaks
 * HOW:  Free all allocated memory
 *
 * @param bn Network handle
 */
void nimcp_bn_destroy(nimcp_bayesian_network_t bn);

/**
 * @brief Add edge to Bayesian network
 *
 * WHAT: Connect parent node to child node
 * WHY:  Define conditional dependencies
 * HOW:  Update child's parent list, resize CPT if needed
 *
 * @param bn Network handle
 * @param parent Parent node ID
 * @param child Child node ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_bn_add_edge(
    nimcp_bayesian_network_t bn,
    uint32_t parent,
    uint32_t child
);

/**
 * @brief Set conditional probability table
 *
 * WHAT: Set CPT for a node
 * WHY:  Define P(child|parents)
 * HOW:  Copy CPT data to node structure
 *
 * @param bn Network handle
 * @param node Node ID
 * @param cpt Probability table (row-major, flattened)
 * @param cpt_size Size of CPT in floats
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_bn_set_cpt(
    nimcp_bayesian_network_t bn,
    uint32_t node,
    const float* cpt,
    size_t cpt_size
);

/**
 * @brief Run Bayesian inference
 *
 * WHAT: Compute posterior probabilities given evidence
 * WHY:  Determine anomaly score from features
 * HOW:  Variable elimination or belief propagation
 *
 * ALGORITHM: Variable elimination with max-product
 *
 * @param bn Network handle
 * @param evidence Evidence values (NaN for unobserved)
 * @param posteriors Output: posterior probabilities
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_bn_infer(
    nimcp_bayesian_network_t bn,
    const float* evidence,
    float* posteriors
);

/**
 * @brief Learn from sample
 *
 * WHAT: Update CPTs based on observed sample
 * WHY:  Improve model accuracy through online learning
 * HOW:  Incremental maximum likelihood estimation
 *
 * @param bn Network handle
 * @param sample Observed values for all nodes
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_bn_learn(
    nimcp_bayesian_network_t bn,
    const float* sample
);

/**
 * @brief Get log-likelihood of sample
 *
 * WHAT: Compute log P(sample) under current model
 * WHY:  Measure how surprising the sample is (anomaly score)
 * HOW:  Sum log probabilities along the graph
 *
 * @param bn Network handle
 * @param sample Sample to evaluate
 * @param log_likelihood Output: log P(sample)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_bn_log_likelihood(
    nimcp_bayesian_network_t bn,
    const float* sample,
    float* log_likelihood
);

/*=============================================================================
 * FEATURE EXTRACTION API (Internal, exposed for testing)
 *============================================================================*/

/**
 * @brief Extract all features from input
 *
 * WHAT: Compute feature vector from raw input
 * WHY:  Convert unstructured data to structured features
 * HOW:  Analyze length, entropy, character classes, n-grams, etc.
 *
 * @param input Input data
 * @param input_len Length of input
 * @param features Output: feature vector (size NIMCP_FEATURE_COUNT)
 * @param timing_context Optional: timing context for rate features
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_extract_features(
    const void* input,
    size_t input_len,
    float* features,
    void* timing_context
);

/**
 * @brief Calculate Shannon entropy
 *
 * WHAT: Compute entropy of byte sequence
 * WHY:  Measure randomness/compression of input
 * HOW:  H = -Σ p(x) log2 p(x)
 *
 * @param data Data to analyze
 * @param len Length of data
 * @return Entropy in bits per byte [0, 8]
 */
float nimcp_calculate_entropy(const uint8_t* data, size_t len);

/**
 * @brief Calculate n-gram entropy
 *
 * WHAT: Compute entropy of n-gram distribution
 * WHY:  Detect unusual character sequences
 * HOW:  Build n-gram histogram, compute entropy
 *
 * @param data Data to analyze
 * @param len Length of data
 * @param n N-gram size (2 or 3)
 * @return N-gram entropy
 */
float nimcp_calculate_ngram_entropy(const uint8_t* data, size_t len, uint32_t n);

/**
 * @brief Detect nesting depth
 *
 * WHAT: Measure maximum nesting of brackets/parens
 * WHY:  Detect deeply nested structures (common in attacks)
 * HOW:  Track depth during scan
 *
 * @param data Data to analyze
 * @param len Length of data
 * @return Maximum nesting depth
 */
uint32_t nimcp_detect_nesting_depth(const uint8_t* data, size_t len);

/**
 * @brief Calculate repeated substring ratio
 *
 * WHAT: Measure how much of input is repeated substrings
 * WHY:  Detect pattern-based attacks
 * HOW:  Find longest repeated substring, compute ratio
 *
 * @param data Data to analyze
 * @param len Length of data
 * @return Ratio of repeated content [0, 1]
 */
float nimcp_calculate_repeat_ratio(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANOMALY_DETECTOR_H */
