/**
 * @file nimcp_imagination_fep_bridge.h
 * @brief Imagination - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between imagination engine and FEP orchestrator
 * WHY:  Enable free energy minimization across mental simulation, tracking
 *       prediction errors from imagination divergence and counterfactual costs
 * HOW:  Register with FEP orchestrator, compute free energy from simulation
 *       accuracy, counterfactual reasoning costs, and prediction quality
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle for generative models
 * - Clark (2016): Surfing Uncertainty - prediction and mental simulation
 * - Hassabis & Maguire (2009): The construction system of the brain
 * - Kosslyn (2005): Mental imagery and the brain
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Generative model for simulation
 * - Hippocampus: Scene construction and pattern completion
 * - Visual cortex: Mental imagery generation
 * - Default mode network: Spontaneous imagination
 * - Prediction error signals drive imagination refinement
 *
 * FEP INTEGRATION MODEL:
 * - Mental simulation = internal world model predictions
 * - Simulation divergence from reality = prediction error
 * - Counterfactual reasoning has computational/metabolic cost
 * - Imagination that improves future predictions reduces long-term free energy
 * - Vivid imagination with poor reality testing = high free energy
 *
 * FREE ENERGY COMPUTATION:
 * - Simulation divergence: Distance between predicted and actual outcomes
 * - Counterfactual cost: Metabolic expense of what-if reasoning
 * - Coherence penalty: Incoherent imagination increases free energy
 * - Prediction accuracy bonus: Accurate simulation reduces free energy
 *
 * @see nimcp_imagination_engine.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_IMAGINATION_FEP_BRIDGE_H
#define NIMCP_IMAGINATION_FEP_BRIDGE_H

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

/** @brief Imagination engine handle */
typedef struct imagination_engine imagination_engine_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum simulation history for FEP computation */
#define IMAGINATION_FEP_MAX_HISTORY         64

/** @brief Free energy baseline for idle imagination state */
#define IMAGINATION_FEP_BASELINE_FREE_ENERGY   0.1f

/** @brief Maximum free energy ceiling */
#define IMAGINATION_FEP_MAX_FREE_ENERGY        2.0f

/** @brief Simulation divergence weight to free energy */
#define IMAGINATION_FEP_DIVERGENCE_WEIGHT      0.4f

/** @brief Counterfactual cost weight to free energy */
#define IMAGINATION_FEP_COUNTERFACTUAL_WEIGHT  0.3f

/** @brief Coherence contribution weight to free energy */
#define IMAGINATION_FEP_COHERENCE_WEIGHT       0.2f

/** @brief Prediction accuracy weight (bonus - reduces free energy) */
#define IMAGINATION_FEP_PREDICTION_WEIGHT      0.1f

/** @brief Bio-async module ID for imagination FEP bridge */
#define BIO_MODULE_IMAGINATION_FEP             0x1A10

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    IMAGINATION_FEP_STATE_UNINITIALIZED = 0, /**< Not yet initialized */
    IMAGINATION_FEP_STATE_IDLE,               /**< Ready, no active processing */
    IMAGINATION_FEP_STATE_ACTIVE,             /**< Processing simulations, updating FEP */
    IMAGINATION_FEP_STATE_DEGRADED,           /**< High free energy, reduced capacity */
    IMAGINATION_FEP_STATE_ERROR               /**< Error state */
} imagination_fep_state_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge configuration
 *
 * WHAT: Configuration parameters for imagination-FEP integration
 * WHY:  Allow tuning of free energy computation and thresholds
 * HOW:  Weights, thresholds, and behavior flags
 */
typedef struct imagination_fep_config {
    /* Feature flags */
    bool enable_logging;                   /**< Enable debug logging */
    bool enable_degraded_mode;             /**< Enable degraded mode on high FE */
    bool enable_callbacks;                 /**< Enable state change callbacks */

    /* Timing */
    uint32_t update_interval_ms;           /**< FEP update interval (default: 50ms) */

    /* Weighting parameters */
    float free_energy_weight;              /**< Overall FE contribution weight */
    float simulation_divergence_weight;    /**< Weight for divergence from reality */
    float counterfactual_cost;             /**< Metabolic cost per counterfactual */
    float coherence_weight;                /**< Weight for scene coherence */
    float prediction_accuracy_weight;      /**< Weight for prediction accuracy bonus */

    /* Thresholds */
    float high_free_energy_threshold;      /**< Threshold for degraded mode */
    float divergence_threshold;            /**< High divergence trigger threshold */
    float coherence_threshold;             /**< Low coherence trigger threshold */

    /* Normalization */
    float baseline_free_energy;            /**< Baseline free energy (idle) */
    float max_free_energy;                 /**< Maximum free energy ceiling */
} imagination_fep_config_t;

/*=============================================================================
 * STATISTICS STRUCTURES
 *===========================================================================*/

/**
 * @brief Bridge statistics
 *
 * WHAT: Tracks FEP bridge performance and state
 * WHY:  Monitoring, debugging, and performance optimization
 * HOW:  Updated during each FEP update cycle
 */
typedef struct imagination_fep_stats {
    uint64_t total_updates;                /**< Total FEP update cycles */
    uint64_t simulations_run;              /**< Total simulations processed */
    uint64_t counterfactuals_evaluated;    /**< Counterfactual scenarios evaluated */
    uint64_t degraded_mode_entries;        /**< Times entered degraded mode */
    float avg_update_time_us;              /**< Average update duration (microseconds) */
    float total_free_energy_contribution;  /**< Cumulative free energy contribution */
    float peak_free_energy;                /**< Peak free energy observed */
    float avg_free_energy;                 /**< Average free energy */
    float avg_simulation_divergence;       /**< Average simulation divergence */
    float avg_counterfactual_cost;         /**< Average counterfactual cost */
} imagination_fep_stats_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct imagination_fep_bridge imagination_fep_bridge_t;

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
typedef void (*imagination_fep_high_fe_callback_t)(
    imagination_fep_bridge_t* bridge,
    float free_energy,
    void* user_data
);

/**
 * @brief Divergence event callback
 *
 * Called when simulation divergence from reality is significant
 *
 * @param bridge Bridge handle
 * @param divergence Divergence magnitude
 * @param user_data User context
 */
typedef void (*imagination_fep_divergence_callback_t)(
    imagination_fep_bridge_t* bridge,
    float divergence,
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
imagination_fep_config_t imagination_fep_config_default(void);

/**
 * @brief Create FEP bridge for imagination engine
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY:  Enable FEP orchestrator integration for imagination
 * HOW:  Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
imagination_fep_bridge_t* imagination_fep_bridge_create(
    const imagination_fep_config_t* config
);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void imagination_fep_bridge_destroy(imagination_fep_bridge_t* bridge);

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
int imagination_fep_bridge_reset(imagination_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add imagination engine to FEP coordination
 * WHY:  Enable system-wide free energy minimization
 * HOW:  Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param bridge Pre-created bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param engine Imagination engine
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int imagination_fep_bridge_register(
    imagination_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    imagination_engine_t* engine,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove imagination from FEP coordination
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int imagination_fep_bridge_unregister(imagination_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool imagination_fep_bridge_is_registered(imagination_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t imagination_fep_bridge_get_id(imagination_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for imagination engine
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY:  Compute free energy from imagination metrics, update predictions
 * HOW:  Query engine stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Divergence contribution: simulation_divergence * divergence_weight
 * - Counterfactual cost: num_counterfactuals * counterfactual_cost
 * - Coherence penalty: (1 - coherence) * coherence_weight
 * - Prediction bonus: -prediction_accuracy * prediction_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (imagination_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int imagination_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY:  Allow bridge-specific cleanup if needed
 * HOW:  Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (imagination_fep_bridge_t*)
 */
void imagination_fep_destroy_callback(void* handle);

/*=============================================================================
 * OPERATIONS
 *===========================================================================*/

/**
 * @brief Manual FEP update (outside orchestrator cycle)
 *
 * WHAT: Trigger FEP update manually
 * WHY:  Testing, debugging, or forced synchronization
 * HOW:  Call update logic directly
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int imagination_fep_bridge_update(imagination_fep_bridge_t* bridge);

/**
 * @brief Force FEP update regardless of timing
 *
 * WHAT: Force an immediate FEP update
 * WHY:  Emergency synchronization or testing
 * HOW:  Bypass timing checks, update now
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int imagination_fep_bridge_force_update(imagination_fep_bridge_t* bridge);

/*=============================================================================
 * STATISTICS AND METRICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int imagination_fep_bridge_get_stats(
    const imagination_fep_bridge_t* bridge,
    imagination_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int imagination_fep_bridge_reset_stats(imagination_fep_bridge_t* bridge);

/*=============================================================================
 * ACCESSORS
 *===========================================================================*/

/**
 * @brief Get current free energy contribution
 *
 * WHAT: Get imagination's contribution to system free energy
 * WHY:  Monitoring and debugging
 * HOW:  Return cached value from last update
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float imagination_fep_bridge_get_free_energy(imagination_fep_bridge_t* bridge);

/**
 * @brief Get current simulation divergence
 *
 * WHAT: Get divergence between simulation and reality
 * WHY:  Measure imagination accuracy
 * HOW:  Return cached value from last update
 *
 * @param bridge Bridge handle
 * @return Current divergence [0-1], -1.0f on error
 */
float imagination_fep_bridge_get_simulation_divergence(
    const imagination_fep_bridge_t* bridge
);

/**
 * @brief Get current prediction error
 *
 * @param bridge Bridge handle
 * @return Current prediction error, -1.0f on error
 */
float imagination_fep_bridge_get_prediction_error(
    const imagination_fep_bridge_t* bridge
);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
imagination_fep_state_t imagination_fep_bridge_get_state(
    const imagination_fep_bridge_t* bridge
);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool imagination_fep_bridge_is_degraded(imagination_fep_bridge_t* bridge);

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
int imagination_fep_bridge_set_high_fe_callback(
    imagination_fep_bridge_t* bridge,
    imagination_fep_high_fe_callback_t callback,
    void* user_data
);

/**
 * @brief Register divergence event callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success, -1 on failure
 */
int imagination_fep_bridge_set_divergence_callback(
    imagination_fep_bridge_t* bridge,
    imagination_fep_divergence_callback_t callback,
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
int imagination_fep_bridge_set_config(
    imagination_fep_bridge_t* bridge,
    const imagination_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int imagination_fep_bridge_get_config(
    const imagination_fep_bridge_t* bridge,
    imagination_fep_config_t* config_out
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
const char* imagination_fep_state_name(imagination_fep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMAGINATION_FEP_BRIDGE_H */
