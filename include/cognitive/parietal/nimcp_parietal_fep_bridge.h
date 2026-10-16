/**
 * @file nimcp_parietal_fep_bridge.h
 * @brief Parietal Lobe - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between parietal lobe and FEP orchestrator
 * WHY:  Enable free energy minimization tracking for spatial and mathematical processing,
 *       contributing parietal-specific metrics to system-wide FEP coordination
 * HOW:  Register with FEP orchestrator, compute free energy from spatial uncertainty,
 *       body schema errors, and mathematical processing prediction errors
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle for hierarchical cognition
 * - Wolpert & Ghahramani (2000): Computational motor control
 * - Andersen & Buneo (2002): Posterior parietal cortex spatial processing
 * - Dehaene et al. (2003): Number sense and parietal representations
 *
 * BIOLOGICAL BASIS:
 * - Posterior parietal cortex: Spatial cognition and body schema
 * - Intraparietal sulcus: Numerical magnitude processing
 * - Superior parietal lobule: Sensorimotor integration
 * - Prediction error signals drive spatial model refinement
 *
 * FEP INTEGRATION MODEL:
 * - Free energy increases with spatial uncertainty
 * - Body schema prediction errors contribute to free energy
 * - Mathematical prediction mismatches increase free energy
 * - Accurate spatial predictions minimize free energy
 *
 * @see nimcp_parietal.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_PARIETAL_FEP_BRIDGE_H
#define NIMCP_PARIETAL_FEP_BRIDGE_H

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

/** @brief Parietal lobe handle (from nimcp_parietal.h) */
typedef struct parietal_lobe parietal_lobe_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum spatial uncertainty for FEP normalization */
#define PARIETAL_FEP_MAX_SPATIAL_UNCERTAINTY    1.0f

/** @brief Free energy baseline for idle state */
#define PARIETAL_FEP_BASELINE_FREE_ENERGY       0.1f

/** @brief Maximum free energy ceiling */
#define PARIETAL_FEP_MAX_FREE_ENERGY            2.0f

/** @brief Prediction error decay rate per update cycle */
#define PARIETAL_FEP_ERROR_DECAY_RATE           0.95f

/** @brief Spatial uncertainty weight to free energy */
#define PARIETAL_FEP_SPATIAL_WEIGHT             0.4f

/** @brief Body schema error weight to free energy */
#define PARIETAL_FEP_BODY_SCHEMA_WEIGHT         0.3f

/** @brief Mathematical error weight to free energy */
#define PARIETAL_FEP_MATH_WEIGHT                0.3f

/** @brief Bio-async module ID for parietal FEP bridge */
#define BIO_MODULE_PARIETAL_FEP                 0x0385

/** @brief Default update interval (50ms cognitive timescale) */
#define PARIETAL_FEP_DEFAULT_UPDATE_MS          50

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    PARIETAL_FEP_STATE_UNINITIALIZED = 0,  /**< Not yet initialized */
    PARIETAL_FEP_STATE_IDLE,               /**< Ready, no active processing */
    PARIETAL_FEP_STATE_ACTIVE,             /**< Processing, updating FEP */
    PARIETAL_FEP_STATE_DEGRADED,           /**< High free energy, reduced capacity */
    PARIETAL_FEP_STATE_ERROR               /**< Error state */
} parietal_fep_state_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge configuration
 */
typedef struct {
    /* Logging and timing */
    bool enable_logging;                /**< Enable debug logging */
    uint32_t update_interval_ms;        /**< Update interval in milliseconds */

    /* Weighting parameters */
    float free_energy_weight;           /**< Overall free energy weight in system */
    float spatial_uncertainty_weight;   /**< Weight for spatial uncertainty contribution */
    float body_schema_error_weight;     /**< Weight for body schema error contribution */
    float math_error_weight;            /**< Weight for mathematical error contribution */

    /* Thresholds */
    float high_free_energy_threshold;   /**< Threshold for degraded mode */
    float prediction_error_threshold;   /**< Threshold for surprise trigger */

    /* Normalization */
    float baseline_free_energy;         /**< Baseline free energy (idle) */
    float max_free_energy;              /**< Maximum free energy ceiling */

    /* Behavior */
    bool enable_adaptive_weights;       /**< Adjust weights based on state */
    bool enable_degraded_mode;          /**< Enable degraded mode on high FE */
    bool enable_callbacks;              /**< Enable surprise/high-FE callbacks */
    float error_decay_rate;             /**< Prediction error decay per cycle */
} parietal_fep_config_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP metrics for parietal processing
 *
 * WHAT: Tracks free energy and prediction error from spatial/mathematical processing
 * WHY: FEP orchestrator uses these to coordinate updates and detect anomalies
 * HOW: Updated during FEP update cycle from parietal lobe statistics
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;                  /**< Current free energy estimate [0, MAX] */
    float prediction_error;             /**< Accumulated prediction error [0, 1] */
    float surprise;                     /**< Bayesian surprise from unexpected events */
    float entropy;                      /**< State uncertainty measure */

    /* Parietal-specific metrics */
    float spatial_uncertainty;          /**< Uncertainty in spatial representations */
    float body_schema_error;            /**< Mismatch between predicted/actual body state */
    float math_prediction_error;        /**< Error in mathematical computations */

    /* Contribution breakdown */
    float spatial_contribution;         /**< Free energy from spatial uncertainty */
    float body_schema_contribution;     /**< Free energy from body schema errors */
    float math_contribution;            /**< Free energy from math errors */

    /* Processing metrics */
    uint32_t spatial_computations;      /**< Spatial operations this cycle */
    uint32_t body_schema_updates;       /**< Body schema updates this cycle */
    uint32_t math_operations;           /**< Mathematical operations this cycle */

    /* Timing */
    uint64_t last_update_time_ms;       /**< Last FEP update timestamp */
    uint32_t update_count;              /**< Total FEP updates performed */
    float avg_update_time_us;           /**< Average update duration */
} parietal_fep_metrics_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;             /**< Total FEP update cycles */
    uint64_t spatial_computations;      /**< Total spatial computations */
    uint64_t body_schema_updates;       /**< Total body schema updates */
    uint64_t math_operations;           /**< Total mathematical operations */
    float avg_update_time_us;           /**< Average update time (microseconds) */
    float total_free_energy_contribution; /**< Cumulative free energy contribution */
    uint64_t degraded_mode_entries;     /**< Times entered degraded mode */
    uint64_t surprise_events;           /**< High-surprise events */
    float peak_free_energy;             /**< Peak free energy observed */
    float avg_free_energy;              /**< Average free energy */
    float avg_prediction_error;         /**< Average prediction error */
    uint64_t total_update_time_us;      /**< Cumulative update time */
} parietal_fep_stats_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct parietal_fep_bridge parietal_fep_bridge_t;

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
typedef void (*parietal_fep_high_fe_callback_t)(
    parietal_fep_bridge_t* bridge,
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
 * @param source Source of surprise (spatial, body_schema, math)
 * @param user_data User context
 */
typedef void (*parietal_fep_surprise_callback_t)(
    parietal_fep_bridge_t* bridge,
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
typedef void (*parietal_fep_metrics_callback_t)(
    parietal_fep_bridge_t* bridge,
    const parietal_fep_metrics_t* metrics,
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
parietal_fep_config_t parietal_fep_config_default(void);

/**
 * @brief Create FEP bridge for parietal lobe
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY: Enable FEP orchestrator integration for parietal processing
 * HOW: Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
parietal_fep_bridge_t* parietal_fep_bridge_create(const parietal_fep_config_t* config);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY: Proper resource deallocation
 * HOW: Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void parietal_fep_bridge_destroy(parietal_fep_bridge_t* bridge);

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
int parietal_fep_bridge_reset(parietal_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add parietal lobe to FEP coordination
 * WHY: Enable system-wide free energy minimization
 * HOW: Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param bridge Pre-created bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param parietal Parietal lobe module
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int parietal_fep_bridge_register(
    parietal_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    parietal_lobe_t* parietal,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove parietal lobe from FEP coordination
 * WHY: Clean shutdown or reconfiguration
 * HOW: Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int parietal_fep_bridge_unregister(parietal_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool parietal_fep_bridge_is_registered(parietal_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t parietal_fep_bridge_get_id(parietal_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for parietal lobe
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY: Compute free energy from spatial/body schema/math metrics
 * HOW: Query parietal stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Spatial contribution: spatial_uncertainty * spatial_weight
 * - Body schema contribution: body_schema_error * body_schema_weight
 * - Math contribution: math_error * math_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (parietal_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int parietal_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY: Allow bridge-specific cleanup if needed
 * HOW: Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (parietal_fep_bridge_t*)
 */
void parietal_fep_destroy_callback(void* handle);

/*=============================================================================
 * OPERATIONS
 *===========================================================================*/

/**
 * @brief Manual update of parietal FEP metrics
 *
 * WHAT: Trigger an FEP update outside normal orchestrator cycle
 * WHY: Allow manual synchronization or testing
 * HOW: Perform full FEP computation and metric update
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_fep_bridge_update(parietal_fep_bridge_t* bridge);

/**
 * @brief Force an FEP update (for testing/debugging)
 *
 * WHAT: Force immediate FEP computation
 * WHY: Testing, debugging, or manual synchronization
 * HOW: Bypass registration check, perform minimal update
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int parietal_fep_bridge_force_update(parietal_fep_bridge_t* bridge);

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
int parietal_fep_bridge_get_metrics(
    const parietal_fep_bridge_t* bridge,
    parietal_fep_metrics_t* metrics_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int parietal_fep_bridge_get_stats(
    const parietal_fep_bridge_t* bridge,
    parietal_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int parietal_fep_bridge_reset_stats(parietal_fep_bridge_t* bridge);

/*=============================================================================
 * ACCESSORS
 *===========================================================================*/

/**
 * @brief Get current free energy contribution
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float parietal_fep_bridge_get_free_energy_contribution(parietal_fep_bridge_t* bridge);

/**
 * @brief Get current spatial uncertainty
 *
 * @param bridge Bridge handle
 * @return Spatial uncertainty [0, 1], -1.0f on error
 */
float parietal_fep_bridge_get_spatial_uncertainty(parietal_fep_bridge_t* bridge);

/**
 * @brief Get current body schema error
 *
 * @param bridge Bridge handle
 * @return Body schema error [0, 1], -1.0f on error
 */
float parietal_fep_bridge_get_body_schema_error(parietal_fep_bridge_t* bridge);

/**
 * @brief Get current prediction error
 *
 * @param bridge Bridge handle
 * @return Current prediction error, -1.0f on error
 */
float parietal_fep_bridge_get_prediction_error(parietal_fep_bridge_t* bridge);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
parietal_fep_state_t parietal_fep_bridge_get_state(parietal_fep_bridge_t* bridge);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool parietal_fep_bridge_is_degraded(parietal_fep_bridge_t* bridge);

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
int parietal_fep_bridge_set_high_fe_callback(
    parietal_fep_bridge_t* bridge,
    parietal_fep_high_fe_callback_t callback,
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
int parietal_fep_bridge_set_surprise_callback(
    parietal_fep_bridge_t* bridge,
    parietal_fep_surprise_callback_t callback,
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
int parietal_fep_bridge_set_metrics_callback(
    parietal_fep_bridge_t* bridge,
    parietal_fep_metrics_callback_t callback,
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
int parietal_fep_bridge_set_config(
    parietal_fep_bridge_t* bridge,
    const parietal_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int parietal_fep_bridge_get_config(
    const parietal_fep_bridge_t* bridge,
    parietal_fep_config_t* config_out
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
const char* parietal_fep_state_name(parietal_fep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_FEP_BRIDGE_H */
