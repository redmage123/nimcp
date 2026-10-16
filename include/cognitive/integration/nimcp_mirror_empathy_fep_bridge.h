/**
 * @file nimcp_mirror_empathy_fep_bridge.h
 * @brief Mirror-Empathy - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between mirror-empathy module and FEP orchestrator
 * WHY:  Enable free energy minimization for social cognition, tracking
 *       prediction errors from action mirroring accuracy, empathy prediction
 *       quality, and emotional resonance strength
 * HOW:  Register with FEP orchestrator, compute free energy from mirror-empathy
 *       metrics, update prediction errors based on social interaction outcomes
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle for social cognition
 * - Gallese (2003): Mirror neurons and embodied simulation
 * - Singer (2006): Empathy and neural basis of emotional resonance
 * - Keysers (2009): Mirror neuron system and social cognition
 *
 * BIOLOGICAL BASIS:
 * - Mirror neuron system (premotor/parietal): Action understanding uncertainty
 * - Anterior insula: Emotional resonance and interoceptive prediction errors
 * - Anterior cingulate cortex: Empathic accuracy monitoring
 * - Temporoparietal junction: Theory of mind prediction errors
 *
 * FEP INTEGRATION MODEL:
 * - Action mirroring accuracy = prediction error about observed actions
 * - Empathy prediction quality = error in predicting emotional states
 * - Emotional resonance = shared affect reducing uncertainty
 * - High resonance = minimum free energy state (successful social prediction)
 * - Intention prediction uncertainty contributes to free energy
 *
 * @see nimcp_mirror_empathy_bridge.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_MIRROR_EMPATHY_FEP_BRIDGE_H
#define NIMCP_MIRROR_EMPATHY_FEP_BRIDGE_H

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

/** @brief Mirror-Empathy bridge handle */
typedef struct mirror_empathy_bridge mirror_empathy_bridge_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum action mirroring error for FEP normalization */
#define ME_FEP_MAX_MIRRORING_ERROR          1.0f

/** @brief Free energy baseline for idle state */
#define ME_FEP_BASELINE_FREE_ENERGY         0.1f

/** @brief Maximum free energy ceiling */
#define ME_FEP_MAX_FREE_ENERGY              2.0f

/** @brief Prediction error decay rate per update cycle */
#define ME_FEP_ERROR_DECAY_RATE             0.95f

/** @brief Action mirroring accuracy weight to free energy */
#define ME_FEP_MIRRORING_WEIGHT             0.35f

/** @brief Empathy prediction quality weight to free energy */
#define ME_FEP_EMPATHY_WEIGHT               0.35f

/** @brief Emotional resonance weight to free energy */
#define ME_FEP_RESONANCE_WEIGHT             0.30f

/** @brief Bio-async module ID for mirror-empathy FEP bridge */
#define BIO_MODULE_ME_FEP                   0x1612

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    ME_FEP_STATE_UNINITIALIZED = 0,    /**< Not yet initialized */
    ME_FEP_STATE_IDLE,                 /**< Ready, no active social interactions */
    ME_FEP_STATE_ACTIVE,               /**< Processing interactions, updating FEP */
    ME_FEP_STATE_DEGRADED,             /**< High free energy, reduced empathy capacity */
    ME_FEP_STATE_ERROR                 /**< Error state */
} me_fep_state_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge configuration
 *
 * WHAT: Configuration parameters for mirror-empathy FEP integration
 * WHY:  Allow tuning of free energy computation weights and thresholds
 * HOW:  Set weights for different uncertainty sources, define thresholds
 */
typedef struct {
    /* Feature enables */
    bool enable_logging;                 /**< Enable debug logging */
    uint32_t update_interval_ms;         /**< Update interval (default 50ms) */

    /* Weighting parameters */
    float free_energy_weight;            /**< Overall weight for FE contribution */
    float mirroring_accuracy_weight;     /**< Weight for action mirroring accuracy */
    float empathy_prediction_weight;     /**< Weight for empathy prediction quality */
    float emotional_resonance_weight;    /**< Weight for emotional resonance strength */

    /* Thresholds */
    float high_free_energy_threshold;    /**< Threshold for degraded mode */
    float prediction_error_threshold;    /**< Threshold for surprise trigger */
    float resonance_epsilon;             /**< Epsilon for high resonance check */

    /* Normalization */
    float baseline_free_energy;          /**< Baseline free energy (idle) */
    float max_free_energy;               /**< Maximum free energy ceiling */
    float error_decay_rate;              /**< Prediction error decay per cycle */
} me_fep_config_t;

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
    uint64_t mirroring_computations;     /**< Action mirroring computations */
    uint64_t empathy_computations;       /**< Empathy prediction computations */
    uint64_t resonance_computations;     /**< Resonance strength computations */

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
} me_fep_stats_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP metrics for mirror-empathy
 *
 * WHAT: Current free energy state from social cognition processing
 * WHY:  FEP orchestrator uses these to coordinate updates
 * HOW:  Updated during FEP update cycle from mirror-empathy statistics
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;                   /**< Current free energy estimate [0, MAX] */
    float prediction_error;              /**< Accumulated prediction error [0, 1] */
    float surprise;                      /**< Bayesian surprise from unexpected outcomes */
    float entropy;                       /**< Social prediction entropy */

    /* Mirror-empathy specific metrics */
    float mirroring_error;               /**< Error in action mirroring [0, 1] */
    float empathy_prediction_error;      /**< Error in empathy prediction [0, 1] */
    float resonance_deficit;             /**< Deficit in emotional resonance [0, 1] */
    float intention_uncertainty;         /**< Uncertainty in intention prediction */

    /* Component contributions */
    float mirroring_contribution;        /**< Free energy from mirroring error */
    float empathy_contribution;          /**< Free energy from empathy error */
    float resonance_contribution;        /**< Free energy from resonance deficit */

    /* Social state */
    uint32_t active_interactions;        /**< Number of active social interactions */
    uint32_t successful_predictions;     /**< Successful empathy predictions this cycle */
    bool high_resonance_state;           /**< Currently in high resonance state */

    /* Timing */
    uint64_t last_update_time_ms;        /**< Last FEP update timestamp */
    uint32_t update_count;               /**< Total FEP updates performed */
} me_fep_metrics_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct me_fep_bridge me_fep_bridge_t;

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
typedef void (*me_fep_high_fe_callback_t)(
    me_fep_bridge_t* bridge,
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
 * @param source Source of surprise (mirroring, empathy, resonance)
 * @param user_data User context
 */
typedef void (*me_fep_surprise_callback_t)(
    me_fep_bridge_t* bridge,
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
typedef void (*me_fep_metrics_callback_t)(
    me_fep_bridge_t* bridge,
    const me_fep_metrics_t* metrics,
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
me_fep_config_t me_fep_config_default(void);

/**
 * @brief Create FEP bridge for mirror-empathy
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY:  Enable FEP orchestrator integration for mirror-empathy module
 * HOW:  Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
me_fep_bridge_t* me_fep_bridge_create(const me_fep_config_t* config);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void me_fep_bridge_destroy(me_fep_bridge_t* bridge);

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
int me_fep_bridge_reset(me_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add mirror-empathy to FEP coordination
 * WHY:  Enable system-wide free energy minimization
 * HOW:  Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param bridge Bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param me_bridge Mirror-empathy bridge (can be NULL for standalone testing)
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int me_fep_bridge_register(
    me_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    mirror_empathy_bridge_t* me_bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove mirror-empathy from FEP coordination
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int me_fep_bridge_unregister(me_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool me_fep_bridge_is_registered(me_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t me_fep_bridge_get_id(me_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for mirror-empathy
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY:  Compute free energy from mirror-empathy metrics, update predictions
 * HOW:  Query mirror-empathy stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Mirroring contribution: mirroring_error * mirroring_weight
 * - Empathy contribution: empathy_prediction_error * empathy_weight
 * - Resonance contribution: (1 - resonance_strength) * resonance_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (me_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int me_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY:  Allow bridge-specific cleanup if needed
 * HOW:  Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (me_fep_bridge_t*)
 */
void me_fep_destroy_callback(void* handle);

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
int me_fep_bridge_force_update(me_fep_bridge_t* bridge);

/**
 * @brief Manually update action mirroring error
 *
 * WHAT: Set current action mirroring error value
 * WHY:  Allow external components to inject error measurements
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param error Action mirroring error [0, 1]
 * @return 0 on success, -1 on failure
 */
int me_fep_bridge_update_mirroring_error(
    me_fep_bridge_t* bridge,
    float error
);

/**
 * @brief Manually update empathy prediction error
 *
 * WHAT: Set current empathy prediction error
 * WHY:  Allow external components to inject prediction errors
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param error Empathy prediction error [0, 1]
 * @return 0 on success, -1 on failure
 */
int me_fep_bridge_update_empathy_error(
    me_fep_bridge_t* bridge,
    float error
);

/**
 * @brief Manually update emotional resonance deficit
 *
 * WHAT: Set deficit in emotional resonance (1 - resonance_strength)
 * WHY:  Allow emotional system to report resonance levels
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param deficit Resonance deficit [0, 1] (0 = high resonance)
 * @return 0 on success, -1 on failure
 */
int me_fep_bridge_update_resonance_deficit(
    me_fep_bridge_t* bridge,
    float deficit
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
int me_fep_bridge_get_metrics(
    const me_fep_bridge_t* bridge,
    me_fep_metrics_t* metrics_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int me_fep_bridge_get_stats(
    const me_fep_bridge_t* bridge,
    me_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int me_fep_bridge_reset_stats(me_fep_bridge_t* bridge);

/**
 * @brief Get current free energy contribution
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float me_fep_bridge_get_free_energy(me_fep_bridge_t* bridge);

/**
 * @brief Get current mirroring error
 *
 * @param bridge Bridge handle
 * @return Mirroring error [0, 1], -1.0f on error
 */
float me_fep_bridge_get_mirroring_error(me_fep_bridge_t* bridge);

/**
 * @brief Get current prediction error
 *
 * @param bridge Bridge handle
 * @return Current prediction error, -1.0f on error
 */
float me_fep_bridge_get_prediction_error(me_fep_bridge_t* bridge);

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
me_fep_state_t me_fep_bridge_get_state(me_fep_bridge_t* bridge);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool me_fep_bridge_is_degraded(me_fep_bridge_t* bridge);

/**
 * @brief Check if in high resonance state
 *
 * @param bridge Bridge handle
 * @return true if currently in high resonance state
 */
bool me_fep_bridge_is_high_resonance(me_fep_bridge_t* bridge);

/**
 * @brief Get state name as string
 *
 * @param state Bridge state
 * @return Human-readable state name
 */
const char* me_fep_state_name(me_fep_state_t state);

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
int me_fep_bridge_set_high_fe_callback(
    me_fep_bridge_t* bridge,
    me_fep_high_fe_callback_t callback,
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
int me_fep_bridge_set_surprise_callback(
    me_fep_bridge_t* bridge,
    me_fep_surprise_callback_t callback,
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
int me_fep_bridge_set_metrics_callback(
    me_fep_bridge_t* bridge,
    me_fep_metrics_callback_t callback,
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
int me_fep_bridge_set_config(
    me_fep_bridge_t* bridge,
    const me_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int me_fep_bridge_get_config(
    const me_fep_bridge_t* bridge,
    me_fep_config_t* config_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_EMPATHY_FEP_BRIDGE_H */
