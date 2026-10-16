/**
 * @file nimcp_rcog_fep_bridge.h
 * @brief Recursive Cognition - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between recursive cognition engine and FEP orchestrator
 * WHY:  Enable free energy minimization across recursive processing, tracking
 *       prediction errors from recursion depth, task decomposition, and refinement
 * HOW:  Register with FEP orchestrator, compute free energy from recursion metrics,
 *       update prediction errors based on decomposition quality
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle for hierarchical cognition
 * - Badcock et al. (2019): Hierarchical predictive processing
 * - Clark (2013): Predictive processing and action-perception loops
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Predictive coding for goal-directed behavior
 * - Anterior cingulate: Error monitoring and adjustment
 * - Dorsolateral PFC: Working memory and recursive planning
 * - Prediction error signals drive recursive refinement
 *
 * FEP INTEGRATION MODEL:
 * - Free energy increases with recursion depth (more uncertainty)
 * - Prediction error from task decomposition mismatches
 * - Surprise when refinement doesn't converge as expected
 * - Entropy tracks answer state uncertainty
 *
 * @see nimcp_rcog_engine.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_RCOG_FEP_BRIDGE_H
#define NIMCP_RCOG_FEP_BRIDGE_H

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

/** @brief Recursive cognition engine handle */
typedef struct rcog_engine rcog_engine_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum recursion depth for FEP normalization */
#define RCOG_FEP_MAX_DEPTH_NORM         16

/** @brief Free energy baseline for idle state */
#define RCOG_FEP_BASELINE_FREE_ENERGY   0.1f

/** @brief Maximum free energy ceiling */
#define RCOG_FEP_MAX_FREE_ENERGY        2.0f

/** @brief Prediction error decay rate per update cycle */
#define RCOG_FEP_ERROR_DECAY_RATE       0.95f

/** @brief Depth contribution weight to free energy */
#define RCOG_FEP_DEPTH_WEIGHT           0.4f

/** @brief Decomposition success weight to free energy */
#define RCOG_FEP_DECOMP_WEIGHT          0.3f

/** @brief Refinement progress weight to free energy */
#define RCOG_FEP_REFINE_WEIGHT          0.3f

/** @brief Bio-async module ID for rcog FEP bridge */
#define BIO_MODULE_RCOG_FEP             0x0D70

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    RCOG_FEP_STATE_UNINITIALIZED = 0,  /**< Not yet initialized */
    RCOG_FEP_STATE_IDLE,               /**< Ready, no active processing */
    RCOG_FEP_STATE_ACTIVE,             /**< Processing goals, updating FEP */
    RCOG_FEP_STATE_DEGRADED,           /**< High free energy, reduced capacity */
    RCOG_FEP_STATE_ERROR               /**< Error state */
} rcog_fep_state_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP metrics for recursive cognition
 *
 * WHAT: Tracks free energy and prediction error from recursive processing
 * WHY: FEP orchestrator uses these to coordinate updates and detect anomalies
 * HOW: Updated during FEP update cycle from rcog engine statistics
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;               /**< Current free energy estimate [0, MAX] */
    float prediction_error;          /**< Accumulated prediction error [0, 1] */
    float surprise;                  /**< Bayesian surprise from unexpected events */
    float entropy;                   /**< State uncertainty measure */

    /* Recursion-specific metrics */
    float depth_contribution;        /**< Free energy from recursion depth */
    float decomp_contribution;       /**< Free energy from decomposition quality */
    float refine_contribution;       /**< Free energy from refinement progress */
    float normalized_depth;          /**< Current depth / max depth [0, 1] */

    /* Decomposition metrics */
    float decomp_success_rate;       /**< Subtask success rate [0, 1] */
    float decomp_efficiency;         /**< Subtasks created vs completed ratio */
    uint32_t current_subtasks;       /**< Active subtasks */
    uint32_t completed_subtasks;     /**< Completed subtasks this cycle */

    /* Refinement metrics */
    float refinement_progress;       /**< Answer refinement progress [0, 1] */
    float confidence_delta;          /**< Change in confidence per step */
    uint32_t refinement_steps;       /**< Current refinement iteration */
    bool answer_converging;          /**< True if answer is converging */

    /* Timing */
    uint64_t last_update_time_ms;    /**< Last FEP update timestamp */
    uint32_t update_count;           /**< Total FEP updates performed */
    float avg_update_time_us;        /**< Average update duration */
} rcog_fep_metrics_t;

/**
 * @brief FEP bridge configuration
 */
typedef struct {
    /* Weighting parameters */
    float depth_weight;              /**< Weight for depth contribution */
    float decomp_weight;             /**< Weight for decomposition contribution */
    float refine_weight;             /**< Weight for refinement contribution */

    /* Thresholds */
    float high_free_energy_threshold;/**< Threshold for degraded mode */
    float prediction_error_threshold;/**< Threshold for surprise trigger */
    float convergence_threshold;     /**< Delta threshold for convergence */

    /* Normalization */
    uint32_t max_depth_norm;         /**< Max depth for normalization */
    float baseline_free_energy;      /**< Baseline free energy (idle) */
    float max_free_energy;           /**< Maximum free energy ceiling */

    /* Behavior */
    bool enable_adaptive_weights;    /**< Adjust weights based on state */
    bool enable_degraded_mode;       /**< Enable degraded mode on high FE */
    bool enable_surprise_callbacks;  /**< Trigger callbacks on surprise */
    float error_decay_rate;          /**< Prediction error decay per cycle */
} rcog_fep_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total FEP update cycles */
    uint64_t degraded_mode_entries;  /**< Times entered degraded mode */
    uint64_t surprise_events;        /**< High-surprise events */
    float peak_free_energy;          /**< Peak free energy observed */
    float avg_free_energy;           /**< Average free energy */
    float avg_prediction_error;      /**< Average prediction error */
    uint64_t total_update_time_us;   /**< Cumulative update time */
} rcog_fep_stats_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct rcog_fep_bridge rcog_fep_bridge_t;

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
typedef void (*rcog_fep_high_fe_callback_t)(
    rcog_fep_bridge_t* bridge,
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
 * @param source Source of surprise (depth, decomp, refine)
 * @param user_data User context
 */
typedef void (*rcog_fep_surprise_callback_t)(
    rcog_fep_bridge_t* bridge,
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
typedef void (*rcog_fep_metrics_callback_t)(
    rcog_fep_bridge_t* bridge,
    const rcog_fep_metrics_t* metrics,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY: Easy initialization with biologically-plausible parameters
 * HOW: Set weights, thresholds, and behavior flags
 *
 * @return Default configuration structure
 */
rcog_fep_config_t rcog_fep_config_default(void);

/**
 * @brief Create FEP bridge for recursive cognition
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY: Enable FEP orchestrator integration for rcog engine
 * HOW: Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
rcog_fep_bridge_t* rcog_fep_bridge_create(const rcog_fep_config_t* config);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY: Proper resource deallocation
 * HOW: Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void rcog_fep_bridge_destroy(rcog_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset all metrics and state to initial values
 * WHY: Recovery from error state or fresh start
 * HOW: Clear metrics, reset counters, return to idle
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_fep_bridge_reset(rcog_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add recursive cognition to FEP coordination
 * WHY: Enable system-wide free energy minimization
 * HOW: Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param orchestrator FEP orchestrator instance
 * @param engine Recursive cognition engine
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int rcog_fep_bridge_register(
    fep_orchestrator_t* orchestrator,
    rcog_engine_t* engine,
    uint32_t* bridge_id_out
);

/**
 * @brief Register bridge with explicit bridge handle
 *
 * WHAT: Register pre-created bridge with orchestrator
 * WHY: Allow custom configuration before registration
 * HOW: Store engine reference, register with orchestrator
 *
 * @param bridge Pre-created bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param engine Recursive cognition engine
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int rcog_fep_bridge_register_ex(
    rcog_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    rcog_engine_t* engine,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove recursive cognition from FEP coordination
 * WHY: Clean shutdown or reconfiguration
 * HOW: Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int rcog_fep_bridge_unregister(rcog_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool rcog_fep_bridge_is_registered(rcog_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t rcog_fep_bridge_get_id(rcog_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for recursive cognition
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY: Compute free energy from recursion metrics, update predictions
 * HOW: Query engine stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Depth contribution: normalized_depth * depth_weight
 * - Decomp contribution: (1 - success_rate) * decomp_weight
 * - Refine contribution: (1 - refinement_progress) * refine_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (rcog_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int rcog_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY: Allow bridge-specific cleanup if needed
 * HOW: Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (rcog_fep_bridge_t*)
 */
void rcog_fep_destroy_callback(void* handle);

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
int rcog_fep_bridge_get_metrics(
    const rcog_fep_bridge_t* bridge,
    rcog_fep_metrics_t* metrics_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int rcog_fep_bridge_get_stats(
    const rcog_fep_bridge_t* bridge,
    rcog_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int rcog_fep_bridge_reset_stats(rcog_fep_bridge_t* bridge);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float rcog_fep_bridge_get_free_energy(rcog_fep_bridge_t* bridge);

/**
 * @brief Get current prediction error
 *
 * @param bridge Bridge handle
 * @return Current prediction error, -1.0f on error
 */
float rcog_fep_bridge_get_prediction_error(rcog_fep_bridge_t* bridge);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
rcog_fep_state_t rcog_fep_bridge_get_state(rcog_fep_bridge_t* bridge);

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool rcog_fep_bridge_is_degraded(rcog_fep_bridge_t* bridge);

/**
 * @brief Check if answer is converging
 *
 * @param bridge Bridge handle
 * @return true if refinement is making progress
 */
bool rcog_fep_bridge_is_converging(rcog_fep_bridge_t* bridge);

/**
 * @brief Get normalized recursion depth
 *
 * @param bridge Bridge handle
 * @return Normalized depth [0, 1], -1.0f on error
 */
float rcog_fep_bridge_get_normalized_depth(rcog_fep_bridge_t* bridge);

/**
 * @brief Get decomposition success rate
 *
 * @param bridge Bridge handle
 * @return Success rate [0, 1], -1.0f on error
 */
float rcog_fep_bridge_get_decomp_success_rate(rcog_fep_bridge_t* bridge);

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
int rcog_fep_bridge_set_high_fe_callback(
    rcog_fep_bridge_t* bridge,
    rcog_fep_high_fe_callback_t callback,
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
int rcog_fep_bridge_set_surprise_callback(
    rcog_fep_bridge_t* bridge,
    rcog_fep_surprise_callback_t callback,
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
int rcog_fep_bridge_set_metrics_callback(
    rcog_fep_bridge_t* bridge,
    rcog_fep_metrics_callback_t callback,
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
int rcog_fep_bridge_set_config(
    rcog_fep_bridge_t* bridge,
    const rcog_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int rcog_fep_bridge_get_config(
    const rcog_fep_bridge_t* bridge,
    rcog_fep_config_t* config_out
);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get state name as string
 *
 * @param state Bridge state
 * @return Human-readable state name
 */
const char* rcog_fep_state_name(rcog_fep_state_t state);

/**
 * @brief Force an FEP update (for testing/debugging)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int rcog_fep_bridge_force_update(rcog_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_FEP_BRIDGE_H */
