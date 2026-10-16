/**
 * @file nimcp_game_theory_executive_fep_bridge.h
 * @brief Game Theory-Executive Bridge - FEP Orchestrator Integration
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tier 3 FEP bridge for Game Theory-Executive Hub Bridge
 * WHY:  Enable free energy minimization tracking for strategic decision-making,
 *       monitoring prediction errors from strategy selection, executive alignment,
 *       and action selection coherence
 * HOW:  Register with FEP orchestrator, compute free energy from decision quality,
 *       executive alignment, risk assessment accuracy, and recommendation follow-through
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle for action selection
 * - Schwartenbeck et al. (2015): Optimal decisions under uncertainty
 * - Yoshida et al. (2008): Game-theoretic active inference
 *
 * BIOLOGICAL BASIS:
 * - Dorsolateral prefrontal cortex (dlPFC): Strategic planning integration
 * - Orbitofrontal cortex (OFC): Value-based decision evaluation
 * - Anterior cingulate cortex (ACC): Decision conflict monitoring
 * - Striatum: Action selection and reward prediction
 *
 * FEP INTEGRATION MODEL:
 * - Decision quality uncertainty = prediction error about optimal action
 * - Executive alignment = coherence between strategic advice and final decision
 * - Action coherence = consistency of action selection with strategic model
 * - Recommendation accuracy = prediction error from strategy outcomes
 * - Lower free energy when executive consistently follows optimal strategies
 *
 * ARCHITECTURE:
 * ```
 * +------------------------+       +------------------------+
 * |   Game Theory System   |       |   Executive Function   |
 * |   (Strategy Analysis)  |       |   (Decision Making)    |
 * +----------+-------------+       +----------+-------------+
 *            |                                |
 *            v                                v
 * +----------+--------------------------------+-------------+
 * |           Game Theory-Executive Bridge (Tier 2)         |
 * |   - Strategic recommendations                           |
 * |   - Risk assessment                                     |
 * |   - Opponent modeling                                   |
 * +----------+--------------------------------+-------------+
 *            |
 *            v
 * +----------+--------------------------------+-------------+
 * |     Game Theory-Executive FEP Bridge (Tier 3)           |
 * |   - Decision quality free energy                        |
 * |   - Executive alignment tracking                        |
 * |   - Action coherence monitoring                         |
 * +----------+--------------------------------+-------------+
 *            |
 *            v
 * +----------+-------------+
 * |   FEP Orchestrator    |
 * |   (System-wide FEP)   |
 * +------------------------+
 * ```
 *
 * @see nimcp_game_theory_executive_bridge.h
 * @see nimcp_fep_orchestrator.h
 * @see nimcp_game_theory_fep_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GAME_THEORY_EXECUTIVE_FEP_BRIDGE_H
#define NIMCP_GAME_THEORY_EXECUTIVE_FEP_BRIDGE_H

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

/** @brief Game theory-executive bridge handle */
typedef struct game_theory_executive_bridge game_theory_executive_bridge_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum decision quality uncertainty for FEP normalization */
#define GT_EXEC_FEP_MAX_DECISION_UNCERTAINTY   1.0f

/** @brief Free energy baseline for idle state */
#define GT_EXEC_FEP_BASELINE_FREE_ENERGY       0.1f

/** @brief Maximum free energy ceiling */
#define GT_EXEC_FEP_MAX_FREE_ENERGY            2.0f

/** @brief Prediction error decay rate per update cycle */
#define GT_EXEC_FEP_ERROR_DECAY_RATE           0.95f

/** @brief Decision quality weight to free energy */
#define GT_EXEC_FEP_DECISION_QUALITY_WEIGHT    0.35f

/** @brief Executive alignment weight to free energy */
#define GT_EXEC_FEP_EXEC_ALIGNMENT_WEIGHT      0.35f

/** @brief Action coherence weight to free energy */
#define GT_EXEC_FEP_ACTION_COHERENCE_WEIGHT    0.30f

/** @brief Bio-async module ID for game theory-executive FEP bridge */
#define BIO_MODULE_GT_EXEC_FEP                 0x1611

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    GT_EXEC_FEP_STATE_UNINITIALIZED = 0, /**< Not yet initialized */
    GT_EXEC_FEP_STATE_IDLE,              /**< Ready, no active decisions */
    GT_EXEC_FEP_STATE_ACTIVE,            /**< Processing decisions, updating FEP */
    GT_EXEC_FEP_STATE_DEGRADED,          /**< High free energy, reduced capacity */
    GT_EXEC_FEP_STATE_ERROR              /**< Error state */
} gt_exec_fep_state_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge configuration
 *
 * WHAT: Configuration parameters for game theory-executive FEP integration
 * WHY:  Allow tuning of free energy computation weights and thresholds
 * HOW:  Set weights for different uncertainty sources, define thresholds
 */
typedef struct {
    /* Feature enables */
    bool enable_logging;                  /**< Enable debug logging */
    uint32_t update_interval_ms;          /**< Update interval (default 50ms) */

    /* Weighting parameters */
    float free_energy_weight;             /**< Overall weight for FE contribution */
    float decision_quality_weight;        /**< Weight for decision quality uncertainty */
    float executive_alignment_weight;     /**< Weight for exec-strategy alignment */
    float action_coherence_weight;        /**< Weight for action selection coherence */

    /* Thresholds */
    float high_free_energy_threshold;     /**< Threshold for degraded mode */
    float prediction_error_threshold;     /**< Threshold for surprise trigger */
    float alignment_epsilon;              /**< Epsilon for alignment check */

    /* Normalization */
    float baseline_free_energy;           /**< Baseline free energy (idle) */
    float max_free_energy;                /**< Maximum free energy ceiling */
    float error_decay_rate;               /**< Prediction error decay per cycle */
} gt_exec_fep_config_t;

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
    uint64_t total_updates;               /**< Total FEP update cycles */
    uint64_t decision_computations;       /**< Decision quality computations */
    uint64_t alignment_checks;            /**< Executive alignment checks */

    /* Timing */
    float avg_update_time_us;             /**< Average update duration (microseconds) */
    uint64_t total_update_time_us;        /**< Cumulative update time */

    /* FEP metrics */
    float total_free_energy_contribution; /**< Cumulative FE contributed */
    float avg_free_energy;                /**< Average free energy level */
    float peak_free_energy;               /**< Peak free energy observed */

    /* Event counts */
    uint64_t degraded_mode_entries;       /**< Times entered degraded mode */
    uint64_t surprise_events;             /**< High-surprise events */
    uint64_t recommendations_followed;    /**< Executive followed recommendation */
    uint64_t recommendations_overridden;  /**< Executive overrode recommendation */
} gt_exec_fep_stats_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP metrics for game theory-executive bridge
 *
 * WHAT: Current free energy state from strategic decision-making
 * WHY:  FEP orchestrator uses these to coordinate updates
 * HOW:  Updated during FEP update cycle from bridge statistics
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;                    /**< Current free energy estimate [0, MAX] */
    float prediction_error;               /**< Accumulated prediction error [0, 1] */
    float surprise;                       /**< Bayesian surprise from unexpected outcomes */
    float entropy;                        /**< Decision entropy (spread of utilities) */

    /* Game theory-executive specific metrics */
    float decision_quality;               /**< Quality of strategic decisions [0, 1] */
    float executive_alignment;            /**< Alignment between exec and strategy [0, 1] */
    float action_coherence;               /**< Coherence of action selection [0, 1] */
    float recommendation_accuracy;        /**< Accuracy of recommendations [0, 1] */

    /* Component contributions */
    float decision_contribution;          /**< Free energy from decision uncertainty */
    float alignment_contribution;         /**< Free energy from alignment errors */
    float coherence_contribution;         /**< Free energy from action incoherence */

    /* Decision state */
    uint32_t pending_decisions;           /**< Number of pending decisions */
    uint32_t decisions_this_cycle;        /**< Decisions processed this cycle */
    bool exec_aligned;                    /**< Currently executive-aligned */

    /* Timing */
    uint64_t last_update_time_ms;         /**< Last FEP update timestamp */
    uint32_t update_count;                /**< Total FEP updates performed */
} gt_exec_fep_metrics_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct gt_exec_fep_bridge gt_exec_fep_bridge_t;

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
typedef void (*gt_exec_fep_high_fe_callback_t)(
    gt_exec_fep_bridge_t* bridge,
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
 * @param source Source of surprise (decision, alignment, coherence)
 * @param user_data User context
 */
typedef void (*gt_exec_fep_surprise_callback_t)(
    gt_exec_fep_bridge_t* bridge,
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
typedef void (*gt_exec_fep_metrics_callback_t)(
    gt_exec_fep_bridge_t* bridge,
    const gt_exec_fep_metrics_t* metrics,
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
gt_exec_fep_config_t gt_exec_fep_config_default(void);

/**
 * @brief Create FEP bridge for game theory-executive
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY:  Enable FEP orchestrator integration for game theory-executive bridge
 * HOW:  Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
gt_exec_fep_bridge_t* gt_exec_fep_bridge_create(const gt_exec_fep_config_t* config);

/**
 * @brief Destroy FEP bridge
 *
 * WHAT: Clean up FEP bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister from orchestrator if registered, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void gt_exec_fep_bridge_destroy(gt_exec_fep_bridge_t* bridge);

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
int gt_exec_fep_bridge_reset(gt_exec_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add game theory-executive FEP to orchestrator coordination
 * WHY:  Enable system-wide free energy minimization
 * HOW:  Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param bridge Bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param gt_exec_bridge Game theory-executive bridge (can be NULL for standalone)
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int gt_exec_fep_bridge_register(
    gt_exec_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    game_theory_executive_bridge_t* gt_exec_bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove game theory-executive FEP from orchestrator coordination
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int gt_exec_fep_bridge_unregister(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool gt_exec_fep_bridge_is_registered(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t gt_exec_fep_bridge_get_id(gt_exec_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for game theory-executive
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY:  Compute free energy from decision quality, alignment, coherence
 * HOW:  Query bridge stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Decision contribution: (1 - decision_quality) * decision_weight
 * - Alignment contribution: (1 - executive_alignment) * alignment_weight
 * - Coherence contribution: (1 - action_coherence) * coherence_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (gt_exec_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int gt_exec_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY:  Allow bridge-specific cleanup if needed
 * HOW:  Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (gt_exec_fep_bridge_t*)
 */
void gt_exec_fep_destroy_callback(void* handle);

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
int gt_exec_fep_bridge_force_update(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Manually update decision quality
 *
 * WHAT: Set current decision quality value
 * WHY:  Allow external components to inject quality measurements
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param quality Decision quality [0, 1] (1 = optimal)
 * @return 0 on success, -1 on failure
 */
int gt_exec_fep_bridge_update_decision_quality(
    gt_exec_fep_bridge_t* bridge,
    float quality
);

/**
 * @brief Manually update executive alignment
 *
 * WHAT: Set current executive-strategy alignment
 * WHY:  Track how well executive follows strategic advice
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param alignment Executive alignment [0, 1] (1 = fully aligned)
 * @return 0 on success, -1 on failure
 */
int gt_exec_fep_bridge_update_executive_alignment(
    gt_exec_fep_bridge_t* bridge,
    float alignment
);

/**
 * @brief Manually update action coherence
 *
 * WHAT: Set action selection coherence
 * WHY:  Track consistency of action selection with strategic model
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param coherence Action coherence [0, 1] (1 = fully coherent)
 * @return 0 on success, -1 on failure
 */
int gt_exec_fep_bridge_update_action_coherence(
    gt_exec_fep_bridge_t* bridge,
    float coherence
);

/**
 * @brief Notify bridge of recommendation follow-through
 *
 * WHAT: Record whether executive followed or overrode recommendation
 * WHY:  Track prediction accuracy and alignment statistics
 * HOW:  Update counters and alignment metrics
 *
 * @param bridge Bridge handle
 * @param followed true if recommendation was followed
 * @param outcome_utility Realized utility of the decision [0, 1]
 * @return 0 on success, -1 on failure
 */
int gt_exec_fep_bridge_notify_recommendation_result(
    gt_exec_fep_bridge_t* bridge,
    bool followed,
    float outcome_utility
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
int gt_exec_fep_bridge_get_metrics(
    const gt_exec_fep_bridge_t* bridge,
    gt_exec_fep_metrics_t* metrics_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int gt_exec_fep_bridge_get_stats(
    const gt_exec_fep_bridge_t* bridge,
    gt_exec_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int gt_exec_fep_bridge_reset_stats(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Get current free energy contribution
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float gt_exec_fep_bridge_get_free_energy(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Get current decision quality
 *
 * @param bridge Bridge handle
 * @return Decision quality [0, 1], -1.0f on error
 */
float gt_exec_fep_bridge_get_decision_quality(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Get current prediction error
 *
 * @param bridge Bridge handle
 * @return Current prediction error, -1.0f on error
 */
float gt_exec_fep_bridge_get_prediction_error(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Get current executive alignment
 *
 * @param bridge Bridge handle
 * @return Executive alignment [0, 1], -1.0f on error
 */
float gt_exec_fep_bridge_get_executive_alignment(gt_exec_fep_bridge_t* bridge);

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
gt_exec_fep_state_t gt_exec_fep_bridge_get_state(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool gt_exec_fep_bridge_is_degraded(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Check if executive is aligned with strategy
 *
 * @param bridge Bridge handle
 * @return true if currently aligned
 */
bool gt_exec_fep_bridge_is_exec_aligned(gt_exec_fep_bridge_t* bridge);

/**
 * @brief Get state name as string
 *
 * @param state Bridge state
 * @return Human-readable state name
 */
const char* gt_exec_fep_state_name(gt_exec_fep_state_t state);

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
int gt_exec_fep_bridge_set_high_fe_callback(
    gt_exec_fep_bridge_t* bridge,
    gt_exec_fep_high_fe_callback_t callback,
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
int gt_exec_fep_bridge_set_surprise_callback(
    gt_exec_fep_bridge_t* bridge,
    gt_exec_fep_surprise_callback_t callback,
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
int gt_exec_fep_bridge_set_metrics_callback(
    gt_exec_fep_bridge_t* bridge,
    gt_exec_fep_metrics_callback_t callback,
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
int gt_exec_fep_bridge_set_config(
    gt_exec_fep_bridge_t* bridge,
    const gt_exec_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int gt_exec_fep_bridge_get_config(
    const gt_exec_fep_bridge_t* bridge,
    gt_exec_fep_config_t* config_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GAME_THEORY_EXECUTIVE_FEP_BRIDGE_H */
