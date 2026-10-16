/**
 * @file nimcp_omni_wm_logging_bridge.h
 * @brief World Model Logging Bridge - Audit Trail for World Model Operations
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with logging/audit systems
 * WHY:  Enable comprehensive audit trails, debugging, and performance monitoring
 * HOW:  Capture predictions, training updates, anomalies, and operational metrics
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * NEURAL CORRELATES OF METACOGNITION:
 * ------------------------------------
 * The prefrontal cortex maintains "records" of cognitive operations for
 * introspection and error correction. This bridge models that capability:
 *
 *   World Model Operations -> Logging -> Analysis -> Feedback
 *   (Prediction/Training)    (Record)   (Audit)   (Improvement)
 *
 * AUDIT TRAIL IMPORTANCE:
 * -----------------------
 * For AI safety and interpretability, world model operations must be:
 *   1. TRACEABLE: Every prediction can be traced to inputs
 *   2. REPRODUCIBLE: Training steps are logged for replay
 *   3. AUDITABLE: Anomalies and errors are captured for analysis
 *   4. MONITORABLE: Performance metrics enable optimization
 *
 * EVENTS LOGGED:
 * --------------
 *   - Prediction requests and outcomes
 *   - Training updates (RSSM parameters)
 *   - Anomaly detections
 *   - Confidence calibration changes
 *   - Replay buffer operations
 *
 * DATA FLOW:
 * ----------
 *   WM -> Logging: Prediction events, training metrics, anomalies
 *   Logging -> WM: Pattern analysis, anomaly alerts (optional feedback)
 *
 * INTEGRATION POINTS:
 * -------------------
 *   - World Model (nimcp_omni_world_model.h): RSSM predictions
 *   - NIMCP Logging (nimcp_logging.h): General logging infrastructure
 *   - Security Audit (nimcp_security_audit.h): Tamper-proof audit trail
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E64
 *   Message Range: 0x6400-0x64FF
 */

#ifndef NIMCP_OMNI_WM_LOGGING_BRIDGE_H
#define NIMCP_OMNI_WM_LOGGING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"  /* BIO_MSG_WM_LOG_* message types */
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* World Model (from nimcp_omni_world_model.h) */
typedef struct omni_world_model omni_world_model_t;

/* Logging system (from nimcp_logging.h) */
typedef struct nimcp_logger_struct* nimcp_logger_t;

/* Security audit (from nimcp_security_audit.h) */
typedef struct nimcp_audit_log nimcp_audit_log_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Logging Bridge */
#define BIO_MODULE_WM_LOGGING_BRIDGE         0x0E64

/** Maximum log message length */
#define WM_LOG_MAX_MESSAGE_LEN               512

/** Maximum details/context length */
#define WM_LOG_MAX_DETAILS_LEN               1024

/** Maximum module source identifier length */
#define WM_LOG_MAX_SOURCE_LEN                64

/** Maximum log entries in memory buffer */
#define WM_LOG_DEFAULT_BUFFER_SIZE           4096

/** Default batch size for log aggregation */
#define WM_LOG_DEFAULT_BATCH_SIZE            32

/** Default flush interval in milliseconds */
#define WM_LOG_DEFAULT_FLUSH_INTERVAL_MS     1000

/** Maximum dimensions for logged vectors (truncated beyond) */
#define WM_LOG_MAX_VECTOR_DIM                64

/* ============================================================================
 * Bio-Async Message Types (0x6400-0x64FF)
 * ============================================================================
 * Message types are defined in nimcp_bio_messages.h to avoid duplication.
 * Key message types used by this bridge:
 *   - BIO_MSG_WM_LOG_PREDICTION (0x6400): Log prediction event
 *   - BIO_MSG_WM_LOG_TRAINING: Log training step
 *   - BIO_MSG_WM_LOG_ANOMALY: Log anomaly detection
 *   - BIO_MSG_WM_LOG_CONFIDENCE: Log confidence calibration
 * ============================================================================ */

/** @brief Message type alias for Logging bridge (uses bio_message_type_t from nimcp_bio_messages.h) */
typedef bio_message_type_t omni_wm_logging_msg_type_t;

/* ============================================================================
 * Log Event Categories
 * ============================================================================ */

/**
 * @brief World Model log event categories
 *
 * WHAT: Classification of world model events for filtering and analysis
 * WHY:  Enable targeted logging, filtering, and reporting
 */
typedef enum {
    WM_LOG_CAT_PREDICTION = 0,      /**< Prediction operations */
    WM_LOG_CAT_TRAINING,            /**< Training/learning operations */
    WM_LOG_CAT_ANOMALY,             /**< Anomaly detection events */
    WM_LOG_CAT_CONFIDENCE,          /**< Confidence/calibration events */
    WM_LOG_CAT_REPLAY,              /**< Replay buffer operations */
    WM_LOG_CAT_ROLLOUT,             /**< Policy rollout events */
    WM_LOG_CAT_DREAMING,            /**< Offline simulation (dreaming) */
    WM_LOG_CAT_SYSTEM,              /**< System/bridge events */
    WM_LOG_CAT_COUNT                /**< Number of categories */
} wm_log_category_t;

/**
 * @brief World Model log severity levels
 */
typedef enum {
    WM_LOG_SEV_TRACE = 0,           /**< Detailed tracing */
    WM_LOG_SEV_DEBUG,               /**< Debug information */
    WM_LOG_SEV_INFO,                /**< Informational */
    WM_LOG_SEV_NOTICE,              /**< Normal but significant */
    WM_LOG_SEV_WARNING,             /**< Warning conditions */
    WM_LOG_SEV_ERROR,               /**< Error conditions */
    WM_LOG_SEV_CRITICAL,            /**< Critical conditions */
    WM_LOG_SEV_COUNT                /**< Number of severity levels */
} wm_log_severity_t;

/**
 * @brief Anomaly types for world model
 */
typedef enum {
    WM_ANOMALY_NONE = 0,            /**< No anomaly */
    WM_ANOMALY_HIGH_PE,             /**< High prediction error */
    WM_ANOMALY_DIVERGENCE,          /**< Trajectory divergence */
    WM_ANOMALY_NAN_INF,             /**< NaN or Inf in computation */
    WM_ANOMALY_GRADIENT_EXPLODE,    /**< Gradient explosion */
    WM_ANOMALY_GRADIENT_VANISH,     /**< Gradient vanishing */
    WM_ANOMALY_CONFIDENCE_DROP,     /**< Sudden confidence drop */
    WM_ANOMALY_REPLAY_CORRUPT,      /**< Corrupted replay data */
    WM_ANOMALY_STATE_OOB,           /**< State out of bounds */
    WM_ANOMALY_CUSTOM,              /**< Custom/application-defined */
    WM_ANOMALY_COUNT                /**< Number of anomaly types */
} wm_anomaly_type_t;

/* ============================================================================
 * Log Entry Structures
 * ============================================================================ */

/**
 * @brief Prediction log entry
 *
 * WHAT: Complete record of a prediction operation
 * WHY:  Enable reconstruction and audit of prediction history
 */
typedef struct {
    uint64_t entry_id;              /**< Unique entry identifier */
    uint64_t timestamp_ns;          /**< Nanosecond timestamp */
    uint32_t sequence_number;       /**< Monotonic sequence */

    /* Input summary (truncated for storage) */
    float input_summary[WM_LOG_MAX_VECTOR_DIM];  /**< Input state summary */
    uint32_t input_dim;             /**< Original input dimension */
    float input_norm;               /**< L2 norm of full input */

    /* Output summary */
    float output_summary[WM_LOG_MAX_VECTOR_DIM]; /**< Output state summary */
    uint32_t output_dim;            /**< Original output dimension */
    float output_norm;              /**< L2 norm of full output */

    /* Confidence and error */
    float confidence;               /**< Prediction confidence [0,1] */
    float prediction_error;         /**< If available, actual PE */
    float uncertainty;              /**< Estimated uncertainty */

    /* Metadata */
    uint32_t horizon;               /**< Prediction horizon (steps) */
    uint32_t direction;             /**< Forward/backward/lateral */
    bool is_counterfactual;         /**< Counterfactual query */
} wm_prediction_log_entry_t;

/**
 * @brief Training step log entry
 *
 * WHAT: Record of a single training update
 * WHY:  Track learning progress and detect training issues
 */
typedef struct {
    uint64_t entry_id;              /**< Unique entry identifier */
    uint64_t timestamp_ns;          /**< Nanosecond timestamp */
    uint32_t step_number;           /**< Training step number */

    /* Loss metrics */
    float total_loss;               /**< Total loss value */
    float dynamics_loss;            /**< Dynamics model loss */
    float reconstruction_loss;      /**< Reconstruction/decoder loss */
    float kl_divergence;            /**< KL divergence (VAE) */
    float reward_loss;              /**< Reward prediction loss */

    /* Gradient metrics */
    float gradient_norm;            /**< Gradient L2 norm */
    float max_gradient;             /**< Maximum gradient value */
    bool gradient_clipped;          /**< Was gradient clipped */

    /* Learning state */
    float learning_rate;            /**< Current learning rate */
    float weight_norm;              /**< Model weight L2 norm */
    float weight_change;            /**< Weight change magnitude */

    /* Batch info */
    uint32_t batch_size;            /**< Training batch size */
    uint32_t replay_buffer_size;    /**< Current replay buffer size */
    bool from_dreaming;             /**< Training from imagination */
} wm_training_log_entry_t;

/**
 * @brief Anomaly log entry
 *
 * WHAT: Record of detected anomaly
 * WHY:  Enable root cause analysis and system monitoring
 */
typedef struct {
    uint64_t entry_id;              /**< Unique entry identifier */
    uint64_t timestamp_ns;          /**< Nanosecond timestamp */
    wm_anomaly_type_t type;         /**< Anomaly type */
    wm_log_severity_t severity;     /**< Severity level */

    /* Anomaly details */
    float anomaly_score;            /**< Anomaly score/magnitude */
    float threshold;                /**< Threshold that was exceeded */
    char details[WM_LOG_MAX_DETAILS_LEN];  /**< Detailed description */

    /* Context */
    char source[WM_LOG_MAX_SOURCE_LEN];    /**< Source component */
    uint64_t related_entry_id;      /**< Related prediction/training entry */
    bool auto_resolved;             /**< Was automatically resolved */
    uint64_t resolution_time_ns;    /**< Resolution timestamp */
} wm_anomaly_log_entry_t;

/**
 * @brief General log entry (flexible format)
 *
 * WHAT: Generic log entry for miscellaneous events
 * WHY:  Cover events not fitting specialized formats
 */
typedef struct {
    uint64_t entry_id;              /**< Unique entry identifier */
    uint64_t timestamp_ns;          /**< Nanosecond timestamp */
    wm_log_category_t category;     /**< Event category */
    wm_log_severity_t severity;     /**< Severity level */
    char message[WM_LOG_MAX_MESSAGE_LEN];  /**< Log message */
    char details[WM_LOG_MAX_DETAILS_LEN];  /**< Extended details */
    char source[WM_LOG_MAX_SOURCE_LEN];    /**< Source component */
} wm_general_log_entry_t;

/* ============================================================================
 * Metrics Structures
 * ============================================================================ */

/**
 * @brief Training metrics for logging
 *
 * WHAT: Aggregated training performance metrics
 * WHY:  Provide summary statistics for analysis
 */
typedef struct {
    float loss;                     /**< Current loss value */
    float loss_ema;                 /**< Exponential moving average */
    float gradient_norm;            /**< Gradient norm */
    float learning_rate;            /**< Current learning rate */
    uint64_t steps;                 /**< Total training steps */
    float mean_pe;                  /**< Mean prediction error */
    float dynamics_loss;            /**< Dynamics model loss */
    float kl_loss;                  /**< KL divergence loss */
} wm_training_metrics_t;

/**
 * @brief Confidence calibration metrics
 *
 * WHAT: Track calibration between confidence and accuracy
 * WHY:  Ensure predictions are well-calibrated
 */
typedef struct {
    float calibration_error;        /**< Expected calibration error */
    float overconfidence_rate;      /**< Rate of overconfident predictions */
    float underconfidence_rate;     /**< Rate of underconfident predictions */
    float mean_confidence;          /**< Mean prediction confidence */
    float mean_accuracy;            /**< Mean prediction accuracy */
    uint32_t calibration_bins[10];  /**< Histogram of confidence bins */
    uint64_t total_predictions;     /**< Total predictions tracked */
} wm_calibration_metrics_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model Logging Bridge configuration
 *
 * WHAT: Parameters controlling WM-Logging integration
 * WHY:  Tune logging verbosity, filtering, and output destinations
 * HOW:  Configurable severity thresholds, categories, and batch sizes
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;             /**< Enable logging (master switch) */
    float sensitivity;                  /**< Logging sensitivity [0.5-2.0] */

    /* Filtering */
    wm_log_severity_t min_severity;     /**< Minimum severity to log */
    uint32_t enabled_categories;        /**< Bitmask of enabled categories */

    /* Prediction Logging */
    bool enable_prediction_logging;     /**< Log predictions */
    float prediction_sample_rate;       /**< Sampling rate [0.0-1.0] */
    bool log_input_output_vectors;      /**< Include vector summaries */
    float high_pe_threshold;            /**< PE threshold for warnings */

    /* Training Logging */
    bool enable_training_logging;       /**< Log training steps */
    uint32_t training_log_interval;     /**< Log every N steps */
    bool log_gradient_stats;            /**< Include gradient statistics */
    bool log_weight_stats;              /**< Include weight statistics */

    /* Anomaly Logging */
    bool enable_anomaly_logging;        /**< Log anomaly detections */
    float anomaly_threshold;            /**< Anomaly detection threshold */
    bool auto_alert_on_critical;        /**< Auto-alert on critical anomalies */

    /* Confidence/Calibration Logging */
    bool enable_calibration_logging;    /**< Log calibration changes */
    uint32_t calibration_update_interval; /**< Update interval (predictions) */

    /* Replay Logging */
    bool enable_replay_logging;         /**< Log replay operations */
    bool log_dream_episodes;            /**< Log dreaming/imagination */

    /* Output Settings */
    bool log_to_console;                /**< Echo to console */
    bool log_to_file;                   /**< Write to log file */
    bool log_to_audit;                  /**< Send to security audit */
    bool enable_json_format;            /**< Use JSON format for structured logs */

    /* Buffering */
    uint32_t buffer_size;               /**< In-memory buffer size */
    uint32_t flush_interval_ms;         /**< Auto-flush interval */
    uint32_t batch_size;                /**< Batch size for aggregation */

    /* Bio-async Settings */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} omni_wm_logging_bridge_config_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief Effects from World Model to Logging
 *
 * WHAT: WM events pending logging
 * WHY:  Track bidirectional data flow
 */
typedef struct {
    uint64_t predictions_pending;       /**< Predictions awaiting log */
    uint64_t training_steps_pending;    /**< Training steps awaiting log */
    uint64_t anomalies_pending;         /**< Anomalies awaiting log */
    wm_log_severity_t max_severity;     /**< Highest pending severity */
    bool has_critical_anomaly;          /**< Critical anomaly pending */
    float current_loss;                 /**< Most recent loss value */
    float current_pe;                   /**< Most recent prediction error */
} omni_wm_to_logging_effects_t;

/**
 * @brief Effects from Logging to World Model (feedback)
 *
 * WHAT: Analysis results that may inform WM operation
 * WHY:  Enable log-driven feedback (optional)
 */
typedef struct {
    bool anomaly_pattern_detected;      /**< Pattern of anomalies found */
    uint32_t anomaly_pattern_type;      /**< Type of pattern */
    float performance_trend;            /**< Performance trend (-1 to +1) */
    bool suggest_lr_adjustment;         /**< Suggest learning rate change */
    float suggested_lr_multiplier;      /**< Suggested LR multiplier */
    bool training_stable;               /**< Training appears stable */
} logging_to_omni_wm_effects_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model Logging Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, and optimization
 * HOW:  Counters, averages, and timing metrics
 */
typedef struct {
    /* Prediction Logging Stats */
    uint64_t predictions_logged;        /**< Total predictions logged */
    uint64_t predictions_sampled_out;   /**< Predictions skipped by sampling */
    float mean_logged_confidence;       /**< Mean confidence of logged preds */
    float mean_logged_pe;               /**< Mean PE of logged predictions */

    /* Training Logging Stats */
    uint64_t training_steps_logged;     /**< Total training steps logged */
    uint64_t training_batches_logged;   /**< Training batches logged */
    float mean_logged_loss;             /**< Mean loss of logged steps */
    float mean_logged_gradient;         /**< Mean gradient norm logged */

    /* Anomaly Logging Stats */
    uint64_t anomalies_logged;          /**< Total anomalies logged */
    uint64_t anomalies_by_type[WM_ANOMALY_COUNT]; /**< Per-type counts */
    uint64_t critical_anomalies;        /**< Critical anomalies logged */
    uint64_t auto_resolved_anomalies;   /**< Auto-resolved anomalies */

    /* Output Stats */
    uint64_t entries_to_console;        /**< Entries written to console */
    uint64_t entries_to_file;           /**< Entries written to file */
    uint64_t entries_to_audit;          /**< Entries sent to audit */
    uint64_t entries_dropped;           /**< Entries dropped (overflow) */
    uint64_t buffer_flushes;            /**< Buffer flush operations */

    /* Timing Stats */
    uint64_t total_updates;             /**< Total update cycles */
    double total_logging_time_ms;       /**< Total time spent logging */
    double mean_log_time_us;            /**< Average time per log entry */
    uint64_t last_update_time_us;       /**< Last update timestamp */

    /* Calibration Stats */
    uint64_t calibration_updates;       /**< Calibration updates performed */
    float current_calibration_error;    /**< Current calibration error */

    /* Error Stats */
    uint64_t errors_total;              /**< Total errors encountered */
    uint64_t errors_io;                 /**< I/O related errors */
    uint64_t errors_buffer;             /**< Buffer related errors */
} omni_wm_logging_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Logging Bridge
 *
 * WHAT: Main bridge structure connecting WM with logging systems
 * WHY:  Centralized audit trail for world model operations
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_logging_bridge {
    bridge_base_t base;                 /**< MUST be first: base infrastructure */

    /* Configuration */
    omni_wm_logging_bridge_config_t config; /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;    /**< World model (RSSM) */
    nimcp_logger_t logger;              /**< NIMCP logger */
    nimcp_audit_log_t* audit_log;       /**< Security audit log (optional) */

    /* Bidirectional Effects */
    omni_wm_to_logging_effects_t wm_to_logging;   /**< WM -> Logging */
    logging_to_omni_wm_effects_t logging_to_wm;   /**< Logging -> WM */

    /* Internal State */
    bool logging_active;                /**< Logging currently active */
    uint64_t next_entry_id;             /**< Next entry ID */
    uint32_t sequence_number;           /**< Monotonic sequence counter */
    uint64_t last_flush_time_us;        /**< Last buffer flush time */

    /* Metrics Tracking */
    wm_training_metrics_t training_metrics;     /**< Training metrics */
    wm_calibration_metrics_t calibration;       /**< Calibration metrics */

    /* Log Buffer */
    wm_general_log_entry_t* log_buffer; /**< Circular log buffer */
    uint32_t buffer_head;               /**< Buffer write position */
    uint32_t buffer_tail;               /**< Buffer read position */
    uint32_t buffer_count;              /**< Entries in buffer */
    uint32_t buffer_capacity;           /**< Buffer capacity */

    /* Prediction Log Buffer (specialized) */
    wm_prediction_log_entry_t* pred_buffer; /**< Prediction log buffer */
    uint32_t pred_buffer_count;         /**< Predictions buffered */
    uint32_t pred_buffer_capacity;      /**< Prediction buffer capacity */

    /* Training Log Buffer (specialized) */
    wm_training_log_entry_t* train_buffer;  /**< Training log buffer */
    uint32_t train_buffer_count;        /**< Training steps buffered */
    uint32_t train_buffer_capacity;     /**< Training buffer capacity */

    /* Statistics */
    omni_wm_logging_bridge_stats_t stats; /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;    /**< Per-instance health agent */
} omni_wm_logging_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with reasonable defaults
 *
 * @param config Configuration structure to initialize
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_default_config(
    omni_wm_logging_bridge_config_t* config);

/**
 * @brief Create World Model Logging Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_logging_bridge_t* omni_wm_logging_bridge_create(
    const omni_wm_logging_bridge_config_t* config);

/**
 * @brief Destroy World Model Logging Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_logging_bridge_destroy(omni_wm_logging_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset buffers and statistics, keep configuration
 * WHY:  Allow fresh start without reconnection
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_reset(omni_wm_logging_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all logging systems to bridge
 *
 * WHAT: Establish connections to WM, logger, and audit
 * WHY:  Single call to wire up systems
 *
 * @param bridge Bridge instance
 * @param world_model World model - required
 * @param logger NIMCP logger - optional
 * @param audit_log Audit log - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_connect(
    omni_wm_logging_bridge_t* bridge,
    omni_world_model_t* world_model,
    nimcp_logger_t logger,
    nimcp_audit_log_t* audit_log);

/**
 * @brief Connect world model
 */
nimcp_error_t omni_wm_logging_bridge_connect_world_model(
    omni_wm_logging_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect NIMCP logger
 */
nimcp_error_t omni_wm_logging_bridge_connect_logger(
    omni_wm_logging_bridge_t* bridge,
    nimcp_logger_t logger);

/**
 * @brief Connect security audit log
 */
nimcp_error_t omni_wm_logging_bridge_connect_audit(
    omni_wm_logging_bridge_t* bridge,
    nimcp_audit_log_t* audit_log);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge to check
 * @return true if world model connected (minimum requirement)
 */
bool omni_wm_logging_bridge_is_connected(const omni_wm_logging_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process pending log entries, flush buffers if needed
 * WHY:  Called each timestep to process buffered events
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_update(
    omni_wm_logging_bridge_t* bridge,
    float dt);

/**
 * @brief Flush all pending log entries
 *
 * WHAT: Force write all buffered entries
 * WHY:  Ensure logs are persisted on demand
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_flush(
    omni_wm_logging_bridge_t* bridge);

/* ============================================================================
 * Prediction Logging API
 * ============================================================================ */

/**
 * @brief Log a prediction event
 *
 * WHAT: Record a world model prediction operation
 * WHY:  Audit trail for prediction tracing
 *
 * @param bridge Bridge instance
 * @param input Input state values
 * @param input_dim Input dimensionality
 * @param output Output state values
 * @param output_dim Output dimensionality
 * @param confidence Prediction confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_prediction(
    omni_wm_logging_bridge_t* bridge,
    const float* input,
    uint32_t input_dim,
    const float* output,
    uint32_t output_dim,
    float confidence);

/**
 * @brief Log prediction with full details
 *
 * @param bridge Bridge instance
 * @param entry Complete prediction log entry
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_prediction_entry(
    omni_wm_logging_bridge_t* bridge,
    const wm_prediction_log_entry_t* entry);

/**
 * @brief Log prediction error
 *
 * @param bridge Bridge instance
 * @param predicted Predicted values
 * @param actual Actual values
 * @param dim Dimensionality
 * @param error_magnitude Computed error magnitude
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_prediction_error(
    omni_wm_logging_bridge_t* bridge,
    const float* predicted,
    const float* actual,
    uint32_t dim,
    float error_magnitude);

/* ============================================================================
 * Training Logging API
 * ============================================================================ */

/**
 * @brief Log a training step
 *
 * WHAT: Record a world model training update
 * WHY:  Track learning progress and detect issues
 *
 * @param bridge Bridge instance
 * @param loss Loss value
 * @param metrics Training metrics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_training_step(
    omni_wm_logging_bridge_t* bridge,
    float loss,
    const wm_training_metrics_t* metrics);

/**
 * @brief Log training with full details
 *
 * @param bridge Bridge instance
 * @param entry Complete training log entry
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_training_entry(
    omni_wm_logging_bridge_t* bridge,
    const wm_training_log_entry_t* entry);

/**
 * @brief Log learning rate change
 *
 * @param bridge Bridge instance
 * @param old_lr Previous learning rate
 * @param new_lr New learning rate
 * @param reason Reason for change
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_lr_change(
    omni_wm_logging_bridge_t* bridge,
    float old_lr,
    float new_lr,
    const char* reason);

/* ============================================================================
 * Anomaly Logging API
 * ============================================================================ */

/**
 * @brief Log an anomaly detection
 *
 * WHAT: Record a detected anomaly
 * WHY:  Enable root cause analysis and monitoring
 *
 * @param bridge Bridge instance
 * @param type Anomaly type
 * @param details Description of the anomaly
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_anomaly(
    omni_wm_logging_bridge_t* bridge,
    wm_anomaly_type_t type,
    const char* details);

/**
 * @brief Log anomaly with full details
 *
 * @param bridge Bridge instance
 * @param entry Complete anomaly log entry
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_anomaly_entry(
    omni_wm_logging_bridge_t* bridge,
    const wm_anomaly_log_entry_t* entry);

/**
 * @brief Log anomaly resolution
 *
 * @param bridge Bridge instance
 * @param original_entry_id Original anomaly entry ID
 * @param resolution_details How it was resolved
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_anomaly_resolved(
    omni_wm_logging_bridge_t* bridge,
    uint64_t original_entry_id,
    const char* resolution_details);

/* ============================================================================
 * Confidence/Calibration Logging API
 * ============================================================================ */

/**
 * @brief Log confidence calibration update
 *
 * @param bridge Bridge instance
 * @param calibration Updated calibration metrics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_calibration(
    omni_wm_logging_bridge_t* bridge,
    const wm_calibration_metrics_t* calibration);

/**
 * @brief Update calibration with new prediction
 *
 * @param bridge Bridge instance
 * @param confidence Prediction confidence
 * @param was_accurate Whether prediction was accurate
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_update_calibration(
    omni_wm_logging_bridge_t* bridge,
    float confidence,
    bool was_accurate);

/* ============================================================================
 * Replay Buffer Logging API
 * ============================================================================ */

/**
 * @brief Log replay buffer operation
 *
 * @param bridge Bridge instance
 * @param operation "add", "sample", "clear"
 * @param count Number of experiences involved
 * @param buffer_size Current buffer size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_replay_operation(
    omni_wm_logging_bridge_t* bridge,
    const char* operation,
    uint32_t count,
    uint32_t buffer_size);

/**
 * @brief Log dream/imagination episode
 *
 * @param bridge Bridge instance
 * @param episode_length Dream episode length
 * @param episode_reward Total reward in dream
 * @param training_updates Updates from dream
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log_dream(
    omni_wm_logging_bridge_t* bridge,
    uint32_t episode_length,
    float episode_reward,
    uint32_t training_updates);

/* ============================================================================
 * General Logging API
 * ============================================================================ */

/**
 * @brief Log general message
 *
 * @param bridge Bridge instance
 * @param category Event category
 * @param severity Severity level
 * @param message Log message
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_log(
    omni_wm_logging_bridge_t* bridge,
    wm_log_category_t category,
    wm_log_severity_t severity,
    const char* message);

/**
 * @brief Log formatted message
 *
 * @param bridge Bridge instance
 * @param category Event category
 * @param severity Severity level
 * @param format Printf-style format string
 * @param ... Format arguments
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_logf(
    omni_wm_logging_bridge_t* bridge,
    wm_log_category_t category,
    wm_log_severity_t severity,
    const char* format,
    ...);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from WM to logging
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_logging_effects_t* omni_wm_logging_bridge_get_wm_effects(
    const omni_wm_logging_bridge_t* bridge);

/**
 * @brief Get current effects from logging to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const logging_to_omni_wm_effects_t* omni_wm_logging_bridge_get_logging_effects(
    const omni_wm_logging_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_get_stats(
    const omni_wm_logging_bridge_t* bridge,
    omni_wm_logging_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_reset_stats(
    omni_wm_logging_bridge_t* bridge);

/**
 * @brief Get current training metrics
 *
 * @param bridge Bridge instance
 * @return Pointer to training metrics (do not free)
 */
const wm_training_metrics_t* omni_wm_logging_bridge_get_training_metrics(
    const omni_wm_logging_bridge_t* bridge);

/**
 * @brief Get current calibration metrics
 *
 * @param bridge Bridge instance
 * @return Pointer to calibration metrics (do not free)
 */
const wm_calibration_metrics_t* omni_wm_logging_bridge_get_calibration(
    const omni_wm_logging_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_connect_bio_async(
    omni_wm_logging_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_logging_bridge_disconnect_bio_async(
    omni_wm_logging_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_logging_bridge_is_bio_async_connected(
    const omni_wm_logging_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_logging_msg_type_to_string(omni_wm_logging_msg_type_t msg_type);

/**
 * @brief Get category name string
 *
 * @param category Log category
 * @return Human-readable category name
 */
const char* wm_log_category_to_string(wm_log_category_t category);

/**
 * @brief Get severity name string
 *
 * @param severity Severity level
 * @return Human-readable severity name
 */
const char* wm_log_severity_to_string(wm_log_severity_t severity);

/**
 * @brief Get anomaly type name string
 *
 * @param type Anomaly type
 * @return Human-readable anomaly name
 */
const char* wm_anomaly_type_to_string(wm_anomaly_type_t type);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_logging_bridge_validate_config(
    const omni_wm_logging_bridge_config_t* config);

/**
 * @brief Create category bitmask
 *
 * @param category Category to include
 * @return Bitmask with category enabled
 */
#define WM_LOG_CAT_MASK(category) (1u << (category))

/**
 * @brief Bitmask for all categories
 */
#define WM_LOG_CAT_ALL ((1u << WM_LOG_CAT_COUNT) - 1)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_LOGGING_BRIDGE_H */
