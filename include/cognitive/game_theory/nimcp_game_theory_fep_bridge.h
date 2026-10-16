/**
 * @file nimcp_game_theory_fep_bridge.h
 * @brief Game Theory - FEP Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between game theory module and FEP orchestrator
 * WHY:  Enable free energy minimization for strategic reasoning, tracking
 *       prediction errors from strategy uncertainty, opponent modeling, and
 *       Nash equilibrium convergence
 * HOW:  Register with FEP orchestrator, compute free energy from game theory metrics,
 *       update prediction errors based on strategy outcomes
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle for decision-making
 * - Yoshida et al. (2008): Game theoretic framework for active inference
 * - Schwartenbeck et al. (2015): Optimal inference with suboptimal models
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Strategic planning and opponent modeling
 * - Anterior cingulate: Conflict monitoring and strategy selection
 * - Striatum: Reward prediction error in strategic contexts
 * - Theory of mind regions: Predicting opponent behavior
 *
 * FEP INTEGRATION MODEL:
 * - Strategy uncertainty = prediction error about optimal action
 * - Nash equilibrium = minimum free energy state (stable mutual predictions)
 * - Opponent modeling uncertainty contributes to free energy
 * - Mixed strategies have higher entropy = higher free energy than pure strategies
 * - Payoff uncertainty increases free energy through expected value variance
 *
 * @see nimcp_game_theory.h
 * @see nimcp_gt_equilibrium.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_GAME_THEORY_FEP_BRIDGE_H
#define NIMCP_GAME_THEORY_FEP_BRIDGE_H

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

/** @brief Game theory system handle */
typedef struct nimcp_gt_system_struct* nimcp_gt_system_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum strategy uncertainty for FEP normalization */
#define GT_FEP_MAX_STRATEGY_UNCERTAINTY     1.0f

/** @brief Free energy baseline for idle state */
#define GT_FEP_BASELINE_FREE_ENERGY         0.1f

/** @brief Maximum free energy ceiling */
#define GT_FEP_MAX_FREE_ENERGY              2.0f

/** @brief Prediction error decay rate per update cycle */
#define GT_FEP_ERROR_DECAY_RATE             0.95f

/** @brief Strategy uncertainty weight to free energy */
#define GT_FEP_STRATEGY_WEIGHT              0.35f

/** @brief Opponent modeling weight to free energy */
#define GT_FEP_OPPONENT_WEIGHT              0.35f

/** @brief Nash convergence weight to free energy */
#define GT_FEP_NASH_WEIGHT                  0.30f

/** @brief Bio-async module ID for game theory FEP bridge */
#define BIO_MODULE_GT_FEP                   0x1510

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    GT_FEP_STATE_UNINITIALIZED = 0,    /**< Not yet initialized */
    GT_FEP_STATE_IDLE,                 /**< Ready, no active games */
    GT_FEP_STATE_ACTIVE,               /**< Processing games, updating FEP */
    GT_FEP_STATE_DEGRADED,             /**< High free energy, reduced capacity */
    GT_FEP_STATE_ERROR                 /**< Error state */
} gt_fep_state_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge configuration
 *
 * WHAT: Configuration parameters for game theory FEP integration
 * WHY:  Allow tuning of free energy computation weights and thresholds
 * HOW:  Set weights for different uncertainty sources, define thresholds
 */
typedef struct {
    /* Feature enables */
    bool enable_logging;                 /**< Enable debug logging */
    uint32_t update_interval_ms;         /**< Update interval (default 50ms) */

    /* Weighting parameters */
    float free_energy_weight;            /**< Overall weight for FE contribution */
    float strategy_uncertainty_weight;   /**< Weight for strategy uncertainty */
    float opponent_modeling_weight;      /**< Weight for opponent prediction error */
    float nash_convergence_weight;       /**< Weight for Nash equilibrium distance */

    /* Thresholds */
    float high_free_energy_threshold;    /**< Threshold for degraded mode */
    float prediction_error_threshold;    /**< Threshold for surprise trigger */
    float nash_epsilon;                  /**< Epsilon for Nash equilibrium check */

    /* Normalization */
    float baseline_free_energy;          /**< Baseline free energy (idle) */
    float max_free_energy;               /**< Maximum free energy ceiling */
    float error_decay_rate;              /**< Prediction error decay per cycle */
} gt_fep_config_t;

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
    uint64_t strategy_computations;      /**< Strategy uncertainty computations */
    uint64_t nash_equilibrium_checks;    /**< Nash equilibrium checks performed */

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
} gt_fep_stats_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP metrics for game theory
 *
 * WHAT: Current free energy state from game-theoretic reasoning
 * WHY:  FEP orchestrator uses these to coordinate updates
 * HOW:  Updated during FEP update cycle from game theory statistics
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;                   /**< Current free energy estimate [0, MAX] */
    float prediction_error;              /**< Accumulated prediction error [0, 1] */
    float surprise;                      /**< Bayesian surprise from unexpected outcomes */
    float entropy;                       /**< Strategy entropy (mixed strategy spread) */

    /* Game theory specific metrics */
    float strategy_uncertainty;          /**< Uncertainty in optimal strategy [0, 1] */
    float opponent_prediction_error;     /**< Error in opponent modeling [0, 1] */
    float nash_distance;                 /**< Distance from Nash equilibrium [0, 1] */
    float payoff_variance;               /**< Variance in expected payoffs */

    /* Component contributions */
    float strategy_contribution;         /**< Free energy from strategy uncertainty */
    float opponent_contribution;         /**< Free energy from opponent modeling */
    float nash_contribution;             /**< Free energy from Nash distance */

    /* Game state */
    uint32_t active_games;               /**< Number of active games */
    uint32_t equilibria_found;           /**< Equilibria found this cycle */
    bool at_nash_equilibrium;            /**< Currently at Nash equilibrium */

    /* Timing */
    uint64_t last_update_time_ms;        /**< Last FEP update timestamp */
    uint32_t update_count;               /**< Total FEP updates performed */
} gt_fep_metrics_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct gt_fep_bridge gt_fep_bridge_t;

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
typedef void (*gt_fep_high_fe_callback_t)(
    gt_fep_bridge_t* bridge,
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
 * @param source Source of surprise (strategy, opponent, nash)
 * @param user_data User context
 */
typedef void (*gt_fep_surprise_callback_t)(
    gt_fep_bridge_t* bridge,
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
typedef void (*gt_fep_metrics_callback_t)(
    gt_fep_bridge_t* bridge,
    const gt_fep_metrics_t* metrics,
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
gt_fep_config_t gt_fep_config_default(void);

/**
 * @brief Create FEP bridge for game theory
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY:  Enable FEP orchestrator integration for game theory module
 * HOW:  Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
gt_fep_bridge_t* gt_fep_bridge_create(const gt_fep_config_t* config);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void gt_fep_bridge_destroy(gt_fep_bridge_t* bridge);

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
int gt_fep_bridge_reset(gt_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add game theory to FEP coordination
 * WHY:  Enable system-wide free energy minimization
 * HOW:  Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param bridge Bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param gt_system Game theory system (can be NULL for standalone testing)
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int gt_fep_bridge_register(
    gt_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    nimcp_gt_system_t gt_system,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove game theory from FEP coordination
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int gt_fep_bridge_unregister(gt_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool gt_fep_bridge_is_registered(gt_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t gt_fep_bridge_get_id(gt_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for game theory
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY:  Compute free energy from game theory metrics, update predictions
 * HOW:  Query game stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Strategy contribution: strategy_uncertainty * strategy_weight
 * - Opponent contribution: opponent_prediction_error * opponent_weight
 * - Nash contribution: (1 - nash_convergence) * nash_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (gt_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int gt_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY:  Allow bridge-specific cleanup if needed
 * HOW:  Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (gt_fep_bridge_t*)
 */
void gt_fep_destroy_callback(void* handle);

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
int gt_fep_bridge_force_update(gt_fep_bridge_t* bridge);

/**
 * @brief Manually update strategy uncertainty
 *
 * WHAT: Set current strategy uncertainty value
 * WHY:  Allow external components to inject uncertainty measurements
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param uncertainty Strategy uncertainty [0, 1]
 * @return 0 on success, -1 on failure
 */
int gt_fep_bridge_update_strategy_uncertainty(
    gt_fep_bridge_t* bridge,
    float uncertainty
);

/**
 * @brief Manually update opponent prediction error
 *
 * WHAT: Set current opponent modeling error
 * WHY:  Allow external ToM components to inject prediction errors
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param error Opponent prediction error [0, 1]
 * @return 0 on success, -1 on failure
 */
int gt_fep_bridge_update_opponent_error(
    gt_fep_bridge_t* bridge,
    float error
);

/**
 * @brief Manually update Nash equilibrium distance
 *
 * WHAT: Set distance from Nash equilibrium
 * WHY:  Allow equilibrium solvers to report convergence
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param distance Nash distance [0, 1] (0 = at equilibrium)
 * @return 0 on success, -1 on failure
 */
int gt_fep_bridge_update_nash_distance(
    gt_fep_bridge_t* bridge,
    float distance
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
int gt_fep_bridge_get_metrics(
    const gt_fep_bridge_t* bridge,
    gt_fep_metrics_t* metrics_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int gt_fep_bridge_get_stats(
    const gt_fep_bridge_t* bridge,
    gt_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int gt_fep_bridge_reset_stats(gt_fep_bridge_t* bridge);

/**
 * @brief Get current free energy contribution
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float gt_fep_bridge_get_free_energy(gt_fep_bridge_t* bridge);

/**
 * @brief Get current strategy uncertainty
 *
 * @param bridge Bridge handle
 * @return Strategy uncertainty [0, 1], -1.0f on error
 */
float gt_fep_bridge_get_strategy_uncertainty(gt_fep_bridge_t* bridge);

/**
 * @brief Get current prediction error
 *
 * @param bridge Bridge handle
 * @return Current prediction error, -1.0f on error
 */
float gt_fep_bridge_get_prediction_error(gt_fep_bridge_t* bridge);

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
gt_fep_state_t gt_fep_bridge_get_state(gt_fep_bridge_t* bridge);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool gt_fep_bridge_is_degraded(gt_fep_bridge_t* bridge);

/**
 * @brief Check if at Nash equilibrium
 *
 * @param bridge Bridge handle
 * @return true if currently at Nash equilibrium
 */
bool gt_fep_bridge_is_at_nash(gt_fep_bridge_t* bridge);

/**
 * @brief Get state name as string
 *
 * @param state Bridge state
 * @return Human-readable state name
 */
const char* gt_fep_state_name(gt_fep_state_t state);

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
int gt_fep_bridge_set_high_fe_callback(
    gt_fep_bridge_t* bridge,
    gt_fep_high_fe_callback_t callback,
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
int gt_fep_bridge_set_surprise_callback(
    gt_fep_bridge_t* bridge,
    gt_fep_surprise_callback_t callback,
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
int gt_fep_bridge_set_metrics_callback(
    gt_fep_bridge_t* bridge,
    gt_fep_metrics_callback_t callback,
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
int gt_fep_bridge_set_config(
    gt_fep_bridge_t* bridge,
    const gt_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int gt_fep_bridge_get_config(
    const gt_fep_bridge_t* bridge,
    gt_fep_config_t* config_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GAME_THEORY_FEP_BRIDGE_H */
