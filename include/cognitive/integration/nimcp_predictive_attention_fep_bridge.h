/**
 * @file nimcp_predictive_attention_fep_bridge.h
 * @brief Predictive-Attention - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between predictive-attention module and FEP orchestrator
 * WHY:  Enable free energy minimization for predictive-attention integration, tracking
 *       prediction errors from prediction accuracy, attention precision weighting, and
 *       error signal quality
 * HOW:  Register with FEP orchestrator, compute free energy from predictive-attention metrics,
 *       update prediction errors based on attention allocation outcomes
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle and predictive coding
 * - Feldman & Friston (2010): Attention, uncertainty, and free-energy
 * - Parr & Friston (2017): Working memory, attention, and salience
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Precision weighting and prediction generation
 * - Parietal cortex: Attention allocation and spatial precision
 * - Anterior cingulate: Error monitoring and attention switching
 * - Superior colliculus: Salience-driven attention shifts
 *
 * FEP INTEGRATION MODEL:
 * - Prediction accuracy = prediction error magnitude
 * - Attention precision = reliability of attention allocation
 * - Error signal quality = clarity and informativeness of prediction errors
 * - High free energy = poor predictions or misallocated attention
 * - Low free energy = accurate predictions and well-targeted attention
 *
 * @see nimcp_predictive_attention_bridge.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_PREDICTIVE_ATTENTION_FEP_BRIDGE_H
#define NIMCP_PREDICTIVE_ATTENTION_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

/** @brief FEP orchestrator handle */
typedef struct fep_orchestrator fep_orchestrator_t;

/** @brief Predictive-Attention bridge handle */
typedef struct predictive_attention_bridge predictive_attention_bridge_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum prediction error for FEP normalization */
#define PA_FEP_MAX_PREDICTION_ERROR       1.0f

/** @brief Free energy baseline for idle state */
#define PA_FEP_BASELINE_FREE_ENERGY       0.1f

/** @brief Maximum free energy ceiling */
#define PA_FEP_MAX_FREE_ENERGY            2.0f

/** @brief Prediction error decay rate per update cycle */
#define PA_FEP_ERROR_DECAY_RATE           0.95f

/** @brief Prediction accuracy weight to free energy */
#define PA_FEP_PREDICTION_WEIGHT          0.40f

/** @brief Attention precision weight to free energy */
#define PA_FEP_PRECISION_WEIGHT           0.35f

/** @brief Error signal quality weight to free energy */
#define PA_FEP_ERROR_QUALITY_WEIGHT       0.25f

/** @brief Bio-async module ID for predictive-attention FEP bridge */
#define BIO_MODULE_PA_FEP                 0x1614

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    PA_FEP_STATE_UNINITIALIZED = 0,    /**< Not yet initialized */
    PA_FEP_STATE_IDLE,                 /**< Ready, no active processing */
    PA_FEP_STATE_ACTIVE,               /**< Processing predictions, updating FEP */
    PA_FEP_STATE_DEGRADED,             /**< High free energy, reduced capacity */
    PA_FEP_STATE_ERROR                 /**< Error state */
} pa_fep_state_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge configuration
 *
 * WHAT: Configuration parameters for predictive-attention FEP integration
 * WHY:  Allow tuning of free energy computation weights and thresholds
 * HOW:  Set weights for different uncertainty sources, define thresholds
 */
typedef struct {
    /* Feature enables */
    bool enable_logging;                 /**< Enable debug logging */
    uint32_t update_interval_ms;         /**< Update interval (default 50ms) */

    /* Weighting parameters */
    float free_energy_weight;            /**< Overall weight for FE contribution */
    float prediction_accuracy_weight;    /**< Weight for prediction accuracy */
    float attention_precision_weight;    /**< Weight for attention precision */
    float error_signal_quality_weight;   /**< Weight for error signal quality */

    /* Thresholds */
    float high_free_energy_threshold;    /**< Threshold for degraded mode */
    float prediction_error_threshold;    /**< Threshold for surprise trigger */
    float precision_epsilon;             /**< Epsilon for precision convergence */

    /* Normalization */
    float baseline_free_energy;          /**< Baseline free energy (idle) */
    float max_free_energy;               /**< Maximum free energy ceiling */
    float error_decay_rate;              /**< Prediction error decay per cycle */
} pa_fep_config_t;

/*=============================================================================
 * STATISTICS STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Tracks FEP computation performance and outcomes
 * WHY:  Monitor bridge health and optimization opportunities
 * HOW:  Accumulate counters and timing metrics during updates
 */
typedef struct {
    /* Update counts */
    uint64_t total_updates;              /**< Total FEP update cycles */
    uint64_t prediction_computations;    /**< Prediction error computations */
    uint64_t precision_updates;          /**< Attention precision updates */

    /* Timing */
    float avg_update_time_us;            /**< Average update duration (microseconds) */
    uint64_t total_update_time_us;       /**< Cumulative update time */

    /* FEP metrics */
    float total_free_energy_contribution; /**< Cumulative FE contributed */
    float avg_free_energy;               /**< Average free energy level */
    float peak_free_energy;              /**< Peak free energy observed */

    /* Event counts */
    uint64_t degraded_mode_entries;      /**< Times entered degraded mode */
    uint64_t surprise_events;            /**< High-surprise events */
} pa_fep_stats_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP metrics for predictive-attention
 *
 * WHAT: Current free energy state from predictive-attention integration
 * WHY:  FEP orchestrator uses these to coordinate updates
 * HOW:  Updated during FEP update cycle from predictive-attention statistics
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;                   /**< Current free energy estimate [0, MAX] */
    float prediction_error;              /**< Accumulated prediction error [0, 1] */
    float surprise;                      /**< Bayesian surprise from unexpected outcomes */
    float entropy;                       /**< Uncertainty in attention allocation */

    /* Predictive-attention specific metrics */
    float prediction_accuracy;           /**< Accuracy of predictions [0, 1] (1=accurate) */
    float attention_precision;           /**< Precision of attention allocation [0, 1] */
    float error_signal_quality;          /**< Quality/informativeness of error signals [0, 1] */
    float precision_variance;            /**< Variance in precision estimates */

    /* Component contributions */
    float prediction_contribution;       /**< Free energy from prediction inaccuracy */
    float precision_contribution;        /**< Free energy from attention imprecision */
    float error_quality_contribution;    /**< Free energy from poor error signals */

    /* State indicators */
    uint32_t active_predictions;         /**< Number of active predictions */
    uint32_t attention_shifts;           /**< Attention shifts this cycle */
    bool high_precision_mode;            /**< Currently in high precision mode */

    /* Timing */
    uint64_t last_update_time_ms;        /**< Last FEP update timestamp */
    uint32_t update_count;               /**< Total FEP updates performed */
} pa_fep_metrics_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct pa_fep_bridge pa_fep_bridge_t;

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

/**
 * @brief High free energy callback
 *
 * Called when free energy exceeds threshold (entering degraded mode)
 *
 * @param bridge Bridge handle
 * @param free_energy Current free energy level
 * @param user_data User context
 */
typedef void (*pa_fep_high_fe_callback_t)(
    pa_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
);

/**
 * @brief Surprise event callback
 *
 * Called when prediction error causes significant surprise
 *
 * @param bridge Bridge handle
 * @param surprise Surprise magnitude
 * @param source Source of surprise (prediction, precision, error_quality)
 * @param user_data User context
 */
typedef void (*pa_fep_surprise_callback_t)(
    pa_fep_bridge_t* bridge,
    float surprise,
    const char* source,
    void* user_data
);

/**
 * @brief Metrics update callback
 *
 * Called after each FEP update cycle
 *
 * @param bridge Bridge handle
 * @param metrics Updated metrics
 * @param user_data User context
 */
typedef void (*pa_fep_metrics_callback_t)(
    pa_fep_bridge_t* bridge,
    const pa_fep_metrics_t* metrics,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Set weights, thresholds, and behavior flags
 *
 * @return Default configuration structure
 */
pa_fep_config_t pa_fep_config_default(void);

/**
 * @brief Create FEP bridge for predictive-attention
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY:  Enable FEP orchestrator integration for predictive-attention module
 * HOW:  Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
pa_fep_bridge_t* pa_fep_bridge_create(const pa_fep_config_t* config);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void pa_fep_bridge_destroy(pa_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset all metrics and state to initial values
 * WHY:  Recovery from error state or fresh start
 * HOW:  Clear metrics, reset counters, return to idle
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_reset(pa_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add predictive-attention to FEP coordination
 * WHY:  Enable system-wide free energy minimization
 * HOW:  Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param bridge Bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param pa_bridge Predictive-attention bridge (can be NULL for standalone testing)
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int pa_fep_bridge_register(
    pa_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    predictive_attention_bridge_t* pa_bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove predictive-attention from FEP coordination
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int pa_fep_bridge_unregister(pa_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool pa_fep_bridge_is_registered(pa_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t pa_fep_bridge_get_id(pa_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for predictive-attention
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY:  Compute free energy from predictive-attention metrics, update predictions
 * HOW:  Query bridge stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Prediction contribution: (1 - prediction_accuracy) * prediction_weight
 * - Precision contribution: (1 - attention_precision) * precision_weight
 * - Error quality contribution: (1 - error_signal_quality) * error_quality_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (pa_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int pa_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY:  Allow bridge-specific cleanup if needed
 * HOW:  Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (pa_fep_bridge_t*)
 */
void pa_fep_destroy_callback(void* handle);

/*=============================================================================
 * MANUAL UPDATE OPERATIONS
 *===========================================================================*/

/**
 * @brief Force an FEP update (for testing/debugging)
 *
 * WHAT: Trigger FEP computation outside normal cycle
 * WHY:  Testing, debugging, or manual control
 * HOW:  Call update logic directly, bypass orchestrator timing
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_force_update(pa_fep_bridge_t* bridge);

/**
 * @brief Manually update prediction accuracy
 *
 * WHAT: Set current prediction accuracy value
 * WHY:  Allow external components to inject accuracy measurements
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param accuracy Prediction accuracy [0, 1] (1 = perfect predictions)
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_update_prediction_accuracy(
    pa_fep_bridge_t* bridge,
    float accuracy
);

/**
 * @brief Manually update attention precision
 *
 * WHAT: Set current attention precision value
 * WHY:  Allow attention system to inject precision estimates
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param precision Attention precision [0, 1] (1 = high precision)
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_update_attention_precision(
    pa_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Manually update error signal quality
 *
 * WHAT: Set error signal quality
 * WHY:  Allow error monitoring to report signal informativeness
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param quality Error signal quality [0, 1] (1 = clear, informative)
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_update_error_signal_quality(
    pa_fep_bridge_t* bridge,
    float quality
);

/*=============================================================================
 * METRICS AND STATISTICS
 *===========================================================================*/

/**
 * @brief Get current FEP metrics
 *
 * @param bridge Bridge handle
 * @param metrics_out Output: current metrics
 * @return 0 on success, -1 on error
 */
int pa_fep_bridge_get_metrics(
    const pa_fep_bridge_t* bridge,
    pa_fep_metrics_t* metrics_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int pa_fep_bridge_get_stats(
    const pa_fep_bridge_t* bridge,
    pa_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int pa_fep_bridge_reset_stats(pa_fep_bridge_t* bridge);

/**
 * @brief Get current free energy contribution
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float pa_fep_bridge_get_free_energy(pa_fep_bridge_t* bridge);

/**
 * @brief Get current prediction accuracy
 *
 * @param bridge Bridge handle
 * @return Prediction accuracy [0, 1], -1.0f on error
 */
float pa_fep_bridge_get_prediction_accuracy(pa_fep_bridge_t* bridge);

/**
 * @brief Get current prediction error
 *
 * @param bridge Bridge handle
 * @return Current prediction error, -1.0f on error
 */
float pa_fep_bridge_get_prediction_error(pa_fep_bridge_t* bridge);

/**
 * @brief Get current attention precision
 *
 * @param bridge Bridge handle
 * @return Attention precision [0, 1], -1.0f on error
 */
float pa_fep_bridge_get_attention_precision(pa_fep_bridge_t* bridge);

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
pa_fep_state_t pa_fep_bridge_get_state(pa_fep_bridge_t* bridge);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool pa_fep_bridge_is_degraded(pa_fep_bridge_t* bridge);

/**
 * @brief Check if in high precision mode
 *
 * @param bridge Bridge handle
 * @return true if currently in high precision mode
 */
bool pa_fep_bridge_is_high_precision(pa_fep_bridge_t* bridge);

/**
 * @brief Get state name as string
 *
 * @param state Bridge state
 * @return Human-readable state name
 */
const char* pa_fep_state_name(pa_fep_state_t state);

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

/**
 * @brief Register high free energy callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_set_high_fe_callback(
    pa_fep_bridge_t* bridge,
    pa_fep_high_fe_callback_t callback,
    void* user_data
);

/**
 * @brief Register surprise event callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_set_surprise_callback(
    pa_fep_bridge_t* bridge,
    pa_fep_surprise_callback_t callback,
    void* user_data
);

/**
 * @brief Register metrics update callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_set_metrics_callback(
    pa_fep_bridge_t* bridge,
    pa_fep_metrics_callback_t callback,
    void* user_data
);

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Update bridge configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_set_config(
    pa_fep_bridge_t* bridge,
    const pa_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int pa_fep_bridge_get_config(
    const pa_fep_bridge_t* bridge,
    pa_fep_config_t* config_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_ATTENTION_FEP_BRIDGE_H */
