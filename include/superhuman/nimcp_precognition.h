/**
 * @file nimcp_precognition.h
 * @brief Superhuman advanced pattern prediction module
 *
 * WHAT: Provides precognition-like advanced pattern prediction capabilities
 * WHY:  Enable superhuman foresight through long-horizon probabilistic forecasting
 * HOW:  Hierarchical temporal prediction, probabilistic state modeling, anomaly detection
 *
 * ARCHITECTURE:
 * - Multi-scale temporal prediction networks
 * - Probabilistic future state distributions
 * - Causal model inference for counterfactuals
 * - Anomaly detection and early warning
 * - Confidence-weighted prediction cascades
 *
 * BIOLOGICAL BASIS:
 * - Models enhanced prefrontal predictive coding
 * - Superior pattern completion in hippocampal-PFC circuits
 * - Heightened forward models in motor/cognitive systems
 * - Enhanced Bayesian inference mechanisms
 * - Intuition as rapid pattern matching
 *
 * @version Phase T12: Superhuman Enhancement Modules
 * @date 2026-01-13
 */

#ifndef NIMCP_PRECOGNITION_H
#define NIMCP_PRECOGNITION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

/**
 * @brief Precognition module error codes
 */
typedef enum {
    PRECOGNITION_ERROR_NONE = 0,
    PRECOGNITION_ERROR_INVALID_INPUT,
    PRECOGNITION_ERROR_PREDICTION_FAILED,
    PRECOGNITION_ERROR_INSUFFICIENT_HISTORY,
    PRECOGNITION_ERROR_HORIZON_EXCEEDED,
    PRECOGNITION_ERROR_MODEL_DIVERGENCE,
    PRECOGNITION_ERROR_CONFIDENCE_TOO_LOW,
    PRECOGNITION_ERROR_ANOMALY_OVERFLOW,
    PRECOGNITION_ERROR_CAUSAL_LOOP,
    PRECOGNITION_ERROR_NOT_INITIALIZED,
    PRECOGNITION_ERROR_INTERNAL
} precognition_error_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Processing status
 */
typedef enum {
    PRECOGNITION_STATUS_IDLE = 0,
    PRECOGNITION_STATUS_PREDICTING,
    PRECOGNITION_STATUS_ANALYZING,
    PRECOGNITION_STATUS_DETECTING_ANOMALY,
    PRECOGNITION_STATUS_MODELING_CAUSAL,
    PRECOGNITION_STATUS_READY,
    PRECOGNITION_STATUS_ERROR
} precognition_status_t;

/**
 * @brief Prediction time horizons
 */
typedef enum {
    HORIZON_IMMEDIATE = 0,           /**< Next step (ms to seconds) */
    HORIZON_SHORT,                   /**< Short term (seconds to minutes) */
    HORIZON_MEDIUM,                  /**< Medium term (minutes to hours) */
    HORIZON_LONG,                    /**< Long term (hours to days) */
    HORIZON_EXTENDED,                /**< Extended (days to weeks) */
    HORIZON_DISTANT,                 /**< Distant future (weeks to months) */
    HORIZON_COUNT
} prediction_horizon_t;

/**
 * @brief Prediction confidence levels
 */
typedef enum {
    CONFIDENCE_VERY_LOW = 0,         /**< < 20% confidence */
    CONFIDENCE_LOW,                  /**< 20-40% confidence */
    CONFIDENCE_MODERATE,             /**< 40-60% confidence */
    CONFIDENCE_HIGH,                 /**< 60-80% confidence */
    CONFIDENCE_VERY_HIGH             /**< > 80% confidence */
} confidence_level_t;

/**
 * @brief Anomaly severity levels
 */
typedef enum {
    ANOMALY_NONE = 0,
    ANOMALY_MINOR,                   /**< Slight deviation */
    ANOMALY_MODERATE,                /**< Significant deviation */
    ANOMALY_MAJOR,                   /**< Major deviation */
    ANOMALY_CRITICAL                 /**< Critical - immediate attention */
} anomaly_severity_t;

/**
 * @brief Causal relationship types
 */
typedef enum {
    CAUSAL_NONE = 0,
    CAUSAL_DIRECT,                   /**< A directly causes B */
    CAUSAL_INDIRECT,                 /**< A causes B through intermediary */
    CAUSAL_BIDIRECTIONAL,            /**< A and B mutually influence */
    CAUSAL_CONFOUNDED,               /**< Common cause relationship */
    CAUSAL_SPURIOUS                  /**< Correlation without causation */
} causal_type_t;

/**
 * @brief Pattern types for prediction
 */
typedef enum {
    PATTERN_LINEAR = 0,              /**< Linear trend */
    PATTERN_PERIODIC,                /**< Periodic/cyclic pattern */
    PATTERN_EXPONENTIAL,             /**< Exponential growth/decay */
    PATTERN_CHAOTIC,                 /**< Chaotic dynamics */
    PATTERN_STATIONARY,              /**< Stationary process */
    PATTERN_MIXED                    /**< Mixed patterns */
} pattern_type_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define PRECOGNITION_DEFAULT_HISTORY_LENGTH         1000
#define PRECOGNITION_DEFAULT_STATE_DIM              128
#define PRECOGNITION_DEFAULT_HIDDEN_DIM             256
#define PRECOGNITION_DEFAULT_NUM_FUTURES            32
#define PRECOGNITION_DEFAULT_MAX_HORIZON_STEPS      100
#define PRECOGNITION_DEFAULT_MIN_CONFIDENCE         0.3f
#define PRECOGNITION_DEFAULT_ANOMALY_THRESHOLD      3.0f
#define PRECOGNITION_DEFAULT_LEARNING_RATE          0.01f

/**
 * @brief Precognition module configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t history_length;             /**< Maximum history to maintain */
    uint32_t max_horizon_steps;          /**< Maximum prediction horizon */
    uint32_t num_future_samples;         /**< Number of probabilistic futures */

    /* Feature dimensions */
    uint32_t state_dim;                  /**< State representation dimension */
    uint32_t hidden_dim;                 /**< Hidden layer dimension */
    uint32_t num_prediction_heads;       /**< Multi-scale prediction heads */

    /* Prediction parameters */
    float min_confidence;                /**< Minimum confidence threshold */
    float confidence_decay;              /**< Confidence decay per horizon step */
    float temporal_discount;             /**< Temporal discounting factor */

    /* Anomaly detection */
    float anomaly_threshold;             /**< Z-score threshold for anomaly */
    float novelty_threshold;             /**< Threshold for novel patterns */
    bool enable_early_warning;           /**< Enable early warning system */

    /* Causal modeling */
    bool enable_causal_inference;        /**< Enable causal model learning */
    float causal_threshold;              /**< Minimum causal strength */
    uint32_t max_causal_depth;           /**< Maximum causal chain depth */

    /* Learning parameters */
    float learning_rate;                 /**< Online learning rate */
    bool enable_online_learning;         /**< Learn from observations */
    bool enable_meta_learning;           /**< Meta-learn prediction strategies */

    /* Processing options */
    bool enable_uncertainty_estimation;  /**< Estimate prediction uncertainty */
    bool enable_counterfactual;          /**< Enable counterfactual reasoning */
    uint32_t max_parallel_predictions;   /**< Max concurrent predictions */
} precognition_config_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Timestamp with uncertainty
 */
typedef struct {
    uint64_t timestamp_ms;               /**< Millisecond timestamp */
    float uncertainty_ms;                /**< Temporal uncertainty */
} uncertain_time_t;

/**
 * @brief Observable state at a point in time
 */
typedef struct {
    float* features;                     /**< State feature vector */
    uint32_t feature_count;
    uncertain_time_t time;               /**< Observation time */
    float observation_noise;             /**< Estimated observation noise */
} observation_t;

/**
 * @brief Predicted future state
 */
typedef struct {
    float* mean;                         /**< Mean prediction */
    float* variance;                     /**< Prediction variance */
    uint32_t feature_count;
    uncertain_time_t time;               /**< Predicted time */
    float confidence;                    /**< Overall confidence [0, 1] */
    confidence_level_t confidence_level;
    pattern_type_t dominant_pattern;     /**< Dominant pattern type */
} predicted_state_t;

/**
 * @brief Probabilistic future trajectory
 */
typedef struct {
    predicted_state_t* states;           /**< Sequence of predicted states */
    uint32_t state_count;
    prediction_horizon_t horizon;        /**< Horizon covered */
    float trajectory_probability;        /**< Probability of this trajectory */
    float cumulative_confidence;         /**< Cumulative confidence */
    uint64_t prediction_id;              /**< Unique prediction identifier */
} future_trajectory_t;

/**
 * @brief Prediction ensemble (multiple futures)
 */
typedef struct {
    future_trajectory_t* trajectories;   /**< Array of possible futures */
    uint32_t trajectory_count;
    float* weights;                      /**< Probability weights */
    predicted_state_t consensus;         /**< Weighted consensus prediction */
    float ensemble_uncertainty;          /**< Overall ensemble uncertainty */
    prediction_horizon_t max_horizon;    /**< Maximum reliable horizon */
} prediction_ensemble_t;

/**
 * @brief Detected anomaly
 */
typedef struct {
    uint64_t anomaly_id;                 /**< Unique identifier */
    uncertain_time_t detection_time;     /**< When detected */
    uncertain_time_t predicted_time;     /**< When anomaly expected */
    anomaly_severity_t severity;
    float z_score;                       /**< Statistical Z-score */
    float* observed_features;            /**< Observed state features */
    float* expected_features;            /**< Expected state features */
    uint32_t feature_count;
    float* feature_deviations;           /**< Per-feature deviation scores */
    char description[256];               /**< Human-readable description */
} detected_anomaly_t;

/**
 * @brief Early warning alert
 */
typedef struct {
    uint64_t alert_id;                   /**< Unique identifier */
    uncertain_time_t alert_time;         /**< When alert generated */
    uncertain_time_t event_time;         /**< Predicted event time */
    float probability;                   /**< Event probability */
    float lead_time_ms;                  /**< Advance warning time */
    anomaly_severity_t expected_severity;
    float confidence;                    /**< Alert confidence */
    char message[512];                   /**< Alert message */
    float* associated_features;          /**< Features triggering alert */
    uint32_t feature_count;
} early_warning_t;

/**
 * @brief Causal relationship between features
 */
typedef struct {
    uint32_t cause_index;                /**< Causing feature index */
    uint32_t effect_index;               /**< Affected feature index */
    causal_type_t type;                  /**< Relationship type */
    float strength;                      /**< Causal strength [0, 1] */
    float lag_ms;                        /**< Time lag of effect */
    float confidence;                    /**< Confidence in relationship */
} causal_link_t;

/**
 * @brief Causal model structure
 */
typedef struct {
    causal_link_t* links;                /**< Array of causal links */
    uint32_t link_count;
    uint32_t num_variables;              /**< Number of variables modeled */
    float model_fit;                     /**< Model fit quality [0, 1] */
    uint64_t last_update_ms;             /**< Last model update time */
} causal_model_t;

/**
 * @brief Counterfactual query
 */
typedef struct {
    float* intervention_features;        /**< Features to intervene on */
    float* intervention_values;          /**< Values to set */
    uint32_t intervention_count;
    uncertain_time_t intervention_time;  /**< When to intervene */
    prediction_horizon_t horizon;        /**< How far to predict */
} counterfactual_query_t;

/**
 * @brief Counterfactual result
 */
typedef struct {
    prediction_ensemble_t factual;       /**< Predictions without intervention */
    prediction_ensemble_t counterfactual; /**< Predictions with intervention */
    float causal_effect;                 /**< Estimated causal effect */
    float effect_confidence;             /**< Confidence in effect estimate */
} counterfactual_result_t;

/**
 * @brief Prediction verification result
 */
typedef struct {
    uint64_t prediction_id;              /**< Original prediction ID */
    predicted_state_t predicted;         /**< What was predicted */
    observation_t observed;              /**< What was observed */
    float prediction_error;              /**< Mean squared error */
    float calibration_score;             /**< How well-calibrated */
    bool within_confidence;              /**< Within confidence interval? */
} verification_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Module statistics
 */
typedef struct {
    /* Prediction counts */
    uint64_t total_predictions;          /**< Total predictions made */
    uint64_t verified_predictions;       /**< Predictions verified */
    uint64_t accurate_predictions;       /**< Predictions within tolerance */

    /* Anomaly counts */
    uint64_t anomalies_detected;         /**< Total anomalies detected */
    uint64_t true_positives;             /**< Correctly predicted anomalies */
    uint64_t false_positives;            /**< False anomaly alerts */
    uint64_t warnings_issued;            /**< Early warnings issued */

    /* Quality metrics */
    float mean_prediction_error;         /**< Average prediction error */
    float calibration_score;             /**< Overall calibration */
    float avg_confidence;                /**< Average prediction confidence */
    float avg_lead_time_ms;              /**< Average early warning lead time */

    /* Per-horizon accuracy */
    float accuracy_by_horizon[HORIZON_COUNT];

    /* Model metrics */
    float causal_model_fit;              /**< Causal model quality */
    uint32_t causal_links_discovered;    /**< Causal relationships found */

    /* Performance */
    float avg_prediction_time_us;        /**< Average prediction time */
    float avg_analysis_time_us;          /**< Average analysis time */

    /* Resource usage */
    size_t memory_used_bytes;            /**< Total memory usage */
    uint32_t history_entries;            /**< Current history size */
} precognition_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for prediction completion
 */
typedef void (*precognition_prediction_callback_t)(
    const prediction_ensemble_t* ensemble,
    void* user_data
);

/**
 * @brief Callback for anomaly detection
 */
typedef void (*precognition_anomaly_callback_t)(
    const detected_anomaly_t* anomaly,
    void* user_data
);

/**
 * @brief Callback for early warning alerts
 */
typedef void (*precognition_warning_callback_t)(
    const early_warning_t* warning,
    void* user_data
);

/**
 * @brief Callback for prediction verification
 */
typedef void (*precognition_verification_callback_t)(
    const verification_result_t* result,
    void* user_data
);

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/**
 * @brief Opaque precognition module handle
 */
typedef struct precognition_module precognition_module_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provide starting point for customization
 * HOW:  Initialize with prediction-optimized values
 *
 * @return Default configuration structure
 */
precognition_config_t precognition_default_config(void);

/**
 * @brief Create precognition module
 *
 * WHAT: Allocate and initialize the module
 * WHY:  Enable advanced pattern prediction
 * HOW:  Create prediction networks, initialize history buffer
 *
 * @param config Configuration (NULL for defaults)
 * @return New module instance, or NULL on failure
 */
precognition_module_t* precognition_create(const precognition_config_t* config);

/**
 * @brief Destroy precognition module
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroy networks, free buffers
 *
 * @param module Module to destroy
 */
void precognition_destroy(precognition_module_t* module);

/**
 * @brief Reset module state
 *
 * WHAT: Clear history and predictions
 * WHY:  Allow fresh start without reallocation
 * HOW:  Clear history buffer and prediction state
 *
 * @param module Module instance
 * @return true on success
 */
bool precognition_reset(precognition_module_t* module);

/*=============================================================================
 * OBSERVATION AND HISTORY
 *===========================================================================*/

/**
 * @brief Add observation to history
 *
 * WHAT: Record new observation for prediction
 * WHY:  Build temporal history for forecasting
 * HOW:  Add to history buffer, update models
 *
 * @param module Module instance
 * @param observation New observation
 * @return true on success
 */
bool precognition_observe(
    precognition_module_t* module,
    const observation_t* observation
);

/**
 * @brief Add feature vector observation
 *
 * WHAT: Convenience function for feature-only observation
 * WHY:  Simple interface for common case
 * HOW:  Create observation with auto-timestamp
 *
 * @param module Module instance
 * @param features Feature vector
 * @param feature_count Number of features
 * @return true on success
 */
bool precognition_observe_features(
    precognition_module_t* module,
    const float* features,
    uint32_t feature_count
);

/**
 * @brief Get observation history
 *
 * WHAT: Retrieve recent observation history
 * WHY:  Inspect historical data
 * HOW:  Copy from history buffer
 *
 * @param module Module instance
 * @param observations Output array (pre-allocated)
 * @param max_count Maximum observations to retrieve
 * @param count Output: actual count retrieved
 * @return true on success
 */
bool precognition_get_history(
    const precognition_module_t* module,
    observation_t* observations,
    uint32_t max_count,
    uint32_t* count
);

/**
 * @brief Clear observation history
 *
 * WHAT: Remove all historical observations
 * WHY:  Reset temporal context
 * HOW:  Clear history buffer
 *
 * @param module Module instance
 * @return true on success
 */
bool precognition_clear_history(precognition_module_t* module);

/*=============================================================================
 * LONG-HORIZON FORECASTING
 *===========================================================================*/

/**
 * @brief Predict future state ensemble
 *
 * WHAT: Generate probabilistic predictions to horizon
 * WHY:  Core precognition capability
 * HOW:  Multi-scale temporal prediction with uncertainty
 *
 * @param module Module instance
 * @param horizon Prediction horizon
 * @param num_samples Number of future samples
 * @param ensemble Output prediction ensemble
 * @return true on success
 */
bool precognition_predict(
    precognition_module_t* module,
    prediction_horizon_t horizon,
    uint32_t num_samples,
    prediction_ensemble_t* ensemble
);

/**
 * @brief Predict specific number of steps ahead
 *
 * WHAT: Predict exact number of timesteps
 * WHY:  Fine-grained prediction control
 * HOW:  Iterative prediction with uncertainty propagation
 *
 * @param module Module instance
 * @param steps Number of steps to predict
 * @param step_size_ms Milliseconds per step
 * @param ensemble Output prediction ensemble
 * @return true on success
 */
bool precognition_predict_steps(
    precognition_module_t* module,
    uint32_t steps,
    float step_size_ms,
    prediction_ensemble_t* ensemble
);

/**
 * @brief Get most likely future trajectory
 *
 * WHAT: Extract single most probable future
 * WHY:  Simple point prediction
 * HOW:  Select highest probability trajectory
 *
 * @param module Module instance
 * @param horizon Prediction horizon
 * @param trajectory Output trajectory
 * @return true on success
 */
bool precognition_predict_most_likely(
    precognition_module_t* module,
    prediction_horizon_t horizon,
    future_trajectory_t* trajectory
);

/**
 * @brief Predict specific feature evolution
 *
 * WHAT: Forecast single feature over time
 * WHY:  Focused feature prediction
 * HOW:  Extract feature from ensemble prediction
 *
 * @param module Module instance
 * @param feature_index Feature to predict
 * @param horizon Prediction horizon
 * @param predictions Output predictions (pre-allocated)
 * @param confidence Output confidences (pre-allocated)
 * @param max_points Maximum prediction points
 * @param point_count Output: actual point count
 * @return true on success
 */
bool precognition_predict_feature(
    precognition_module_t* module,
    uint32_t feature_index,
    prediction_horizon_t horizon,
    float* predictions,
    float* confidence,
    uint32_t max_points,
    uint32_t* point_count
);

/*=============================================================================
 * PROBABILISTIC FUTURE STATES
 *===========================================================================*/

/**
 * @brief Get probability distribution at future time
 *
 * WHAT: Compute full probability distribution
 * WHY:  Uncertainty quantification
 * HOW:  Aggregate ensemble at specific time
 *
 * @param module Module instance
 * @param time_ahead_ms Time in the future
 * @param state Output predicted state with variance
 * @return true on success
 */
bool precognition_get_distribution(
    precognition_module_t* module,
    float time_ahead_ms,
    predicted_state_t* state
);

/**
 * @brief Compute probability of event
 *
 * WHAT: Estimate probability of specific outcome
 * WHY:  Binary event prediction
 * HOW:  Count ensemble trajectories meeting condition
 *
 * @param module Module instance
 * @param feature_index Feature to check
 * @param threshold Threshold value
 * @param above_threshold true if P(feature > threshold)
 * @param horizon Time horizon
 * @param probability Output probability [0, 1]
 * @return true on success
 */
bool precognition_event_probability(
    precognition_module_t* module,
    uint32_t feature_index,
    float threshold,
    bool above_threshold,
    prediction_horizon_t horizon,
    float* probability
);

/**
 * @brief Get confidence intervals
 *
 * WHAT: Compute confidence intervals for predictions
 * WHY:  Uncertainty bounds
 * HOW:  Percentile extraction from ensemble
 *
 * @param module Module instance
 * @param confidence_pct Confidence percentage (e.g., 95)
 * @param horizon Time horizon
 * @param lower_bound Output lower bound features
 * @param upper_bound Output upper bound features
 * @param feature_count Feature count
 * @return true on success
 */
bool precognition_confidence_interval(
    precognition_module_t* module,
    float confidence_pct,
    prediction_horizon_t horizon,
    float* lower_bound,
    float* upper_bound,
    uint32_t feature_count
);

/*=============================================================================
 * ANOMALY PREDICTION
 *===========================================================================*/

/**
 * @brief Detect current anomalies
 *
 * WHAT: Check if current state is anomalous
 * WHY:  Real-time anomaly detection
 * HOW:  Compare observation to prediction
 *
 * @param module Module instance
 * @param anomaly Output anomaly (if detected)
 * @return true if anomaly detected
 */
bool precognition_detect_anomaly(
    precognition_module_t* module,
    detected_anomaly_t* anomaly
);

/**
 * @brief Predict future anomalies
 *
 * WHAT: Forecast potential future anomalies
 * WHY:  Proactive anomaly prediction
 * HOW:  Analyze trajectory divergence
 *
 * @param module Module instance
 * @param horizon Prediction horizon
 * @param anomalies Output array (pre-allocated)
 * @param max_anomalies Maximum anomalies to report
 * @param anomaly_count Output: actual count
 * @return true on success
 */
bool precognition_predict_anomalies(
    precognition_module_t* module,
    prediction_horizon_t horizon,
    detected_anomaly_t* anomalies,
    uint32_t max_anomalies,
    uint32_t* anomaly_count
);

/**
 * @brief Generate early warning
 *
 * WHAT: Check for conditions requiring early warning
 * WHY:  Proactive alerting
 * HOW:  Monitor predictions for warning conditions
 *
 * @param module Module instance
 * @param warning Output warning (if generated)
 * @return true if warning generated
 */
bool precognition_check_early_warning(
    precognition_module_t* module,
    early_warning_t* warning
);

/**
 * @brief Set anomaly threshold
 *
 * WHAT: Configure anomaly detection sensitivity
 * WHY:  Tune detection
 * HOW:  Update Z-score threshold
 *
 * @param module Module instance
 * @param z_score_threshold New threshold (standard deviations)
 * @return true on success
 */
bool precognition_set_anomaly_threshold(
    precognition_module_t* module,
    float z_score_threshold
);

/*=============================================================================
 * CAUSAL MODELING
 *===========================================================================*/

/**
 * @brief Learn causal model from history
 *
 * WHAT: Infer causal relationships from data
 * WHY:  Understand causal structure
 * HOW:  Statistical causal inference
 *
 * @param module Module instance
 * @return true on success
 */
bool precognition_learn_causal_model(precognition_module_t* module);

/**
 * @brief Get causal model
 *
 * WHAT: Retrieve learned causal structure
 * WHY:  Inspect causal relationships
 * HOW:  Copy causal model
 *
 * @param module Module instance
 * @param model Output causal model
 * @return true on success
 */
bool precognition_get_causal_model(
    const precognition_module_t* module,
    causal_model_t* model
);

/**
 * @brief Query causal effect
 *
 * WHAT: Estimate causal effect of intervention
 * WHY:  Counterfactual reasoning
 * HOW:  Causal inference computation
 *
 * @param module Module instance
 * @param cause_index Cause feature index
 * @param effect_index Effect feature index
 * @param strength Output causal strength
 * @param confidence Output confidence
 * @return true if causal link exists
 */
bool precognition_query_causal_effect(
    precognition_module_t* module,
    uint32_t cause_index,
    uint32_t effect_index,
    float* strength,
    float* confidence
);

/**
 * @brief Run counterfactual analysis
 *
 * WHAT: Predict outcome under intervention
 * WHY:  "What if" reasoning
 * HOW:  Intervene in causal model and predict
 *
 * @param module Module instance
 * @param query Counterfactual query
 * @param result Output counterfactual result
 * @return true on success
 */
bool precognition_counterfactual(
    precognition_module_t* module,
    const counterfactual_query_t* query,
    counterfactual_result_t* result
);

/*=============================================================================
 * VERIFICATION AND LEARNING
 *===========================================================================*/

/**
 * @brief Verify past prediction
 *
 * WHAT: Compare prediction to actual observation
 * WHY:  Calibration and learning
 * HOW:  Match prediction to observation by time
 *
 * @param module Module instance
 * @param prediction_id Prediction to verify
 * @param observation Actual observation
 * @param result Output verification result
 * @return true on success
 */
bool precognition_verify_prediction(
    precognition_module_t* module,
    uint64_t prediction_id,
    const observation_t* observation,
    verification_result_t* result
);

/**
 * @brief Update models from verification
 *
 * WHAT: Learn from prediction errors
 * WHY:  Improve future predictions
 * HOW:  Online model update
 *
 * @param module Module instance
 * @param result Verification result
 * @return true on success
 */
bool precognition_learn_from_error(
    precognition_module_t* module,
    const verification_result_t* result
);

/**
 * @brief Get prediction accuracy metrics
 *
 * WHAT: Compute accuracy statistics
 * WHY:  Model evaluation
 * HOW:  Aggregate verification results
 *
 * @param module Module instance
 * @param horizon Horizon to evaluate (or all if HORIZON_COUNT)
 * @param accuracy Output accuracy [0, 1]
 * @param calibration Output calibration score
 * @return true on success
 */
bool precognition_get_accuracy(
    const precognition_module_t* module,
    prediction_horizon_t horizon,
    float* accuracy,
    float* calibration
);

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

/**
 * @brief Set prediction callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool precognition_set_prediction_callback(
    precognition_module_t* module,
    precognition_prediction_callback_t callback,
    void* user_data
);

/**
 * @brief Set anomaly callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool precognition_set_anomaly_callback(
    precognition_module_t* module,
    precognition_anomaly_callback_t callback,
    void* user_data
);

/**
 * @brief Set warning callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool precognition_set_warning_callback(
    precognition_module_t* module,
    precognition_warning_callback_t callback,
    void* user_data
);

/**
 * @brief Set verification callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool precognition_set_verification_callback(
    precognition_module_t* module,
    precognition_verification_callback_t callback,
    void* user_data
);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current status
 *
 * @param module Module instance
 * @return Current status
 */
precognition_status_t precognition_get_status(const precognition_module_t* module);

/**
 * @brief Get last error code
 *
 * @param module Module instance
 * @return Last error code
 */
precognition_error_t precognition_get_last_error(const precognition_module_t* module);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable description
 */
const char* precognition_error_string(precognition_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable description
 */
const char* precognition_status_string(precognition_status_t status);

/**
 * @brief Get module statistics
 *
 * @param module Module instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool precognition_get_stats(const precognition_module_t* module, precognition_stats_t* stats);

/**
 * @brief Get module configuration
 *
 * @param module Module instance
 * @param config Output configuration structure
 * @return true on success
 */
bool precognition_get_config(const precognition_module_t* module, precognition_config_t* config);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Free prediction ensemble
 *
 * @param ensemble Ensemble to free
 */
void precognition_free_ensemble(prediction_ensemble_t* ensemble);

/**
 * @brief Free trajectory
 *
 * @param trajectory Trajectory to free
 */
void precognition_free_trajectory(future_trajectory_t* trajectory);

/**
 * @brief Free causal model
 *
 * @param model Model to free
 */
void precognition_free_causal_model(causal_model_t* model);

/**
 * @brief Free counterfactual result
 *
 * @param result Result to free
 */
void precognition_free_counterfactual(counterfactual_result_t* result);

/**
 * @brief Get horizon name string
 *
 * @param horizon Horizon enum value
 * @return Human-readable name
 */
const char* precognition_horizon_string(prediction_horizon_t horizon);

/**
 * @brief Get confidence level name
 *
 * @param level Confidence level
 * @return Human-readable name
 */
const char* precognition_confidence_string(confidence_level_t level);

/**
 * @brief Get current timestamp
 *
 * @return Current time in milliseconds
 */
uint64_t precognition_get_current_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PRECOGNITION_H */
