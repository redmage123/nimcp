/**
 * @file nimcp_salience_attention_fep_bridge.h
 * @brief Salience-Attention - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between salience-attention bridge and FEP orchestrator
 * WHY:  Enable free energy minimization for attention allocation, tracking
 *       prediction errors from salience prediction accuracy, attention allocation
 *       efficiency, and priority estimation quality
 * HOW:  Register with FEP orchestrator, compute free energy from salience-attention
 *       metrics, update prediction errors based on attention outcomes
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle for attention
 * - Feldman & Friston (2010): Attention, uncertainty, and free-energy
 * - Parr & Friston (2017): Uncertainty, precision, and attention in active inference
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus: Bottom-up salience computation drives attention capture
 * - Prefrontal cortex: Top-down attention modulates salience sensitivity
 * - Parietal cortex: Priority maps integrate salience with goals
 * - Thalamic reticular nucleus: Attention filtering reduces prediction error
 *
 * FEP INTEGRATION MODEL:
 * - Salience prediction error = mismatch between expected and observed salience
 * - Attention allocation efficiency = resource utilization quality
 * - Priority estimation error = mismatch in importance ranking
 * - Efficient attention = minimum free energy state (precise predictions)
 * - Surprise = unexpected salience events requiring attention shift
 *
 * @see nimcp_salience_attention_bridge.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_SALIENCE_ATTENTION_FEP_BRIDGE_H
#define NIMCP_SALIENCE_ATTENTION_FEP_BRIDGE_H

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

/** @brief Salience-attention bridge handle */
typedef struct salience_attention_bridge salience_attention_bridge_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum salience prediction error for FEP normalization */
#define SA_FEP_MAX_SALIENCE_ERROR           1.0f

/** @brief Free energy baseline for idle state */
#define SA_FEP_BASELINE_FREE_ENERGY         0.1f

/** @brief Maximum free energy ceiling */
#define SA_FEP_MAX_FREE_ENERGY              2.0f

/** @brief Prediction error decay rate per update cycle */
#define SA_FEP_ERROR_DECAY_RATE             0.95f

/** @brief Salience prediction weight to free energy */
#define SA_FEP_SALIENCE_WEIGHT              0.35f

/** @brief Attention allocation weight to free energy */
#define SA_FEP_ATTENTION_WEIGHT             0.35f

/** @brief Priority estimation weight to free energy */
#define SA_FEP_PRIORITY_WEIGHT              0.30f

/** @brief Bio-async module ID for salience-attention FEP bridge */
#define BIO_MODULE_SA_FEP                   0x1613

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    SA_FEP_STATE_UNINITIALIZED = 0,    /**< Not yet initialized */
    SA_FEP_STATE_IDLE,                 /**< Ready, no active processing */
    SA_FEP_STATE_ACTIVE,               /**< Processing, updating FEP */
    SA_FEP_STATE_DEGRADED,             /**< High free energy, reduced capacity */
    SA_FEP_STATE_ERROR                 /**< Error state */
} sa_fep_state_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge configuration
 *
 * WHAT: Configuration parameters for salience-attention FEP integration
 * WHY:  Allow tuning of free energy computation weights and thresholds
 * HOW:  Set weights for different uncertainty sources, define thresholds
 */
typedef struct {
    /* Feature enables */
    bool enable_logging;                 /**< Enable debug logging */
    uint32_t update_interval_ms;         /**< Update interval (default 50ms) */

    /* Weighting parameters */
    float free_energy_weight;            /**< Overall weight for FE contribution */
    float salience_prediction_weight;    /**< Weight for salience prediction error */
    float attention_allocation_weight;   /**< Weight for attention allocation efficiency */
    float priority_estimation_weight;    /**< Weight for priority estimation error */

    /* Thresholds */
    float high_free_energy_threshold;    /**< Threshold for degraded mode */
    float prediction_error_threshold;    /**< Threshold for surprise trigger */
    float attention_efficiency_threshold; /**< Threshold for efficient attention */

    /* Normalization */
    float baseline_free_energy;          /**< Baseline free energy (idle) */
    float max_free_energy;               /**< Maximum free energy ceiling */
    float error_decay_rate;              /**< Prediction error decay per cycle */
} sa_fep_config_t;

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
    uint64_t salience_computations;      /**< Salience prediction computations */
    uint64_t attention_computations;     /**< Attention allocation computations */
    uint64_t priority_computations;      /**< Priority estimation computations */

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
    uint64_t attention_captures;         /**< Successful attention captures */
} sa_fep_stats_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP metrics for salience-attention
 *
 * WHAT: Current free energy state from salience-attention processing
 * WHY:  FEP orchestrator uses these to coordinate updates
 * HOW:  Updated during FEP update cycle from salience-attention statistics
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;                   /**< Current free energy estimate [0, MAX] */
    float prediction_error;              /**< Accumulated prediction error [0, 1] */
    float surprise;                      /**< Bayesian surprise from unexpected events */
    float entropy;                       /**< Attention distribution entropy */

    /* Salience-attention specific metrics */
    float salience_prediction_error;     /**< Error in salience prediction [0, 1] */
    float attention_allocation_error;    /**< Error in attention allocation [0, 1] */
    float priority_estimation_error;     /**< Error in priority estimation [0, 1] */
    float attention_efficiency;          /**< Attention allocation efficiency [0, 1] */

    /* Component contributions */
    float salience_contribution;         /**< Free energy from salience prediction */
    float attention_contribution;        /**< Free energy from attention allocation */
    float priority_contribution;         /**< Free energy from priority estimation */

    /* Attention state */
    uint32_t active_targets;             /**< Number of active attention targets */
    uint32_t salience_detections;        /**< Salience detections this cycle */
    bool attention_captured;             /**< Whether attention was captured */

    /* Timing */
    uint64_t last_update_time_ms;        /**< Last FEP update timestamp */
    uint32_t update_count;               /**< Total FEP updates performed */
} sa_fep_metrics_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct sa_fep_bridge sa_fep_bridge_t;

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
typedef void (*sa_fep_high_fe_callback_t)(
    sa_fep_bridge_t* bridge,
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
 * @param source Source of surprise (salience, attention, priority)
 * @param user_data User context
 */
typedef void (*sa_fep_surprise_callback_t)(
    sa_fep_bridge_t* bridge,
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
typedef void (*sa_fep_metrics_callback_t)(
    sa_fep_bridge_t* bridge,
    const sa_fep_metrics_t* metrics,
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
sa_fep_config_t sa_fep_config_default(void);

/**
 * @brief Create FEP bridge for salience-attention
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY:  Enable FEP orchestrator integration for salience-attention module
 * HOW:  Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
sa_fep_bridge_t* sa_fep_bridge_create(const sa_fep_config_t* config);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void sa_fep_bridge_destroy(sa_fep_bridge_t* bridge);

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
int sa_fep_bridge_reset(sa_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add salience-attention to FEP coordination
 * WHY:  Enable system-wide free energy minimization
 * HOW:  Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param bridge Bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param sa_bridge Salience-attention bridge (can be NULL for standalone testing)
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int sa_fep_bridge_register(
    sa_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    salience_attention_bridge_t* sa_bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove salience-attention from FEP coordination
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int sa_fep_bridge_unregister(sa_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool sa_fep_bridge_is_registered(sa_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t sa_fep_bridge_get_id(sa_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for salience-attention
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY:  Compute free energy from salience-attention metrics, update predictions
 * HOW:  Query bridge stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Salience contribution: salience_prediction_error * salience_weight
 * - Attention contribution: attention_allocation_error * attention_weight
 * - Priority contribution: priority_estimation_error * priority_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (sa_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int sa_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY:  Allow bridge-specific cleanup if needed
 * HOW:  Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (sa_fep_bridge_t*)
 */
void sa_fep_destroy_callback(void* handle);

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
int sa_fep_bridge_force_update(sa_fep_bridge_t* bridge);

/**
 * @brief Manually update salience prediction error
 *
 * WHAT: Set current salience prediction error value
 * WHY:  Allow external components to inject error measurements
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param error Salience prediction error [0, 1]
 * @return 0 on success, -1 on failure
 */
int sa_fep_bridge_update_salience_error(
    sa_fep_bridge_t* bridge,
    float error
);

/**
 * @brief Manually update attention allocation error
 *
 * WHAT: Set current attention allocation error
 * WHY:  Allow external components to inject allocation errors
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param error Attention allocation error [0, 1]
 * @return 0 on success, -1 on failure
 */
int sa_fep_bridge_update_attention_error(
    sa_fep_bridge_t* bridge,
    float error
);

/**
 * @brief Manually update priority estimation error
 *
 * WHAT: Set error in priority estimation
 * WHY:  Allow priority systems to report estimation quality
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param error Priority estimation error [0, 1]
 * @return 0 on success, -1 on failure
 */
int sa_fep_bridge_update_priority_error(
    sa_fep_bridge_t* bridge,
    float error
);

/**
 * @brief Update attention efficiency metric
 *
 * WHAT: Set attention allocation efficiency
 * WHY:  Track how well attention resources are being used
 * HOW:  Update internal metric
 *
 * @param bridge Bridge handle
 * @param efficiency Attention efficiency [0, 1] (1 = optimal)
 * @return 0 on success, -1 on failure
 */
int sa_fep_bridge_update_attention_efficiency(
    sa_fep_bridge_t* bridge,
    float efficiency
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
int sa_fep_bridge_get_metrics(
    const sa_fep_bridge_t* bridge,
    sa_fep_metrics_t* metrics_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int sa_fep_bridge_get_stats(
    const sa_fep_bridge_t* bridge,
    sa_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sa_fep_bridge_reset_stats(sa_fep_bridge_t* bridge);

/**
 * @brief Get current free energy contribution
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float sa_fep_bridge_get_free_energy(sa_fep_bridge_t* bridge);

/**
 * @brief Get current salience prediction error
 *
 * @param bridge Bridge handle
 * @return Salience prediction error [0, 1], -1.0f on error
 */
float sa_fep_bridge_get_salience_error(sa_fep_bridge_t* bridge);

/**
 * @brief Get current prediction error
 *
 * @param bridge Bridge handle
 * @return Current prediction error, -1.0f on error
 */
float sa_fep_bridge_get_prediction_error(sa_fep_bridge_t* bridge);

/**
 * @brief Get current attention efficiency
 *
 * @param bridge Bridge handle
 * @return Attention efficiency [0, 1], -1.0f on error
 */
float sa_fep_bridge_get_attention_efficiency(sa_fep_bridge_t* bridge);

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
sa_fep_state_t sa_fep_bridge_get_state(sa_fep_bridge_t* bridge);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool sa_fep_bridge_is_degraded(sa_fep_bridge_t* bridge);

/**
 * @brief Check if attention is efficiently allocated
 *
 * @param bridge Bridge handle
 * @return true if attention efficiency is above threshold
 */
bool sa_fep_bridge_is_efficient(sa_fep_bridge_t* bridge);

/**
 * @brief Get state name as string
 *
 * @param state Bridge state
 * @return Human-readable state name
 */
const char* sa_fep_state_name(sa_fep_state_t state);

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
int sa_fep_bridge_set_high_fe_callback(
    sa_fep_bridge_t* bridge,
    sa_fep_high_fe_callback_t callback,
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
int sa_fep_bridge_set_surprise_callback(
    sa_fep_bridge_t* bridge,
    sa_fep_surprise_callback_t callback,
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
int sa_fep_bridge_set_metrics_callback(
    sa_fep_bridge_t* bridge,
    sa_fep_metrics_callback_t callback,
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
int sa_fep_bridge_set_config(
    sa_fep_bridge_t* bridge,
    const sa_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int sa_fep_bridge_get_config(
    const sa_fep_bridge_t* bridge,
    sa_fep_config_t* config_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SALIENCE_ATTENTION_FEP_BRIDGE_H */
