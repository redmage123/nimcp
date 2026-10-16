/**
 * @file nimcp_imagination_reasoning_fep_bridge.h
 * @brief Imagination-Reasoning Bridge - FEP Orchestrator Integration
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bidirectional bridge between imagination-reasoning hub bridge and FEP orchestrator
 * WHY:  Enable free energy minimization for imagination-reasoning coordination, tracking
 *       prediction errors from scenario generation, reasoning coherence, and counterfactual
 *       validity
 * HOW:  Register with FEP orchestrator, compute free energy from imagination-reasoning metrics,
 *       update prediction errors based on scenario outcomes
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle for predictive processing
 * - Hassabis & Maguire (2007): Constructive episodic simulation in imagination
 * - Byrne (2002): Mental models and counterfactual thinking
 * - Buckner & Carroll (2007): Default network and self-projection
 *
 * BIOLOGICAL BASIS:
 * - Default Mode Network (DMN): Mental simulation and imagination
 * - Prefrontal cortex: Reasoning about imagined scenarios
 * - Hippocampus: Constructive episodic memory for scenario generation
 * - Anterior cingulate: Conflict monitoring in counterfactual evaluation
 *
 * FEP INTEGRATION MODEL:
 * - Scenario generation quality = prediction about plausible alternatives
 * - Reasoning coherence = internal consistency of inferences
 * - Counterfactual validity = accuracy of "what-if" predictions
 * - Creative inference novelty = information gain from imagination
 * - High free energy = poor scenario-reasoning integration
 * - Low free energy = coherent imagination-reasoning alignment
 *
 * @see nimcp_imagination_reasoning_bridge.h
 * @see nimcp_fep_orchestrator.h
 */

#ifndef NIMCP_IMAGINATION_REASONING_FEP_BRIDGE_H
#define NIMCP_IMAGINATION_REASONING_FEP_BRIDGE_H

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

/** @brief Imagination-Reasoning bridge handle */
typedef struct imagination_reasoning_bridge imagination_reasoning_bridge_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum scenario quality for FEP normalization */
#define IMAG_REASON_FEP_MAX_SCENARIO_QUALITY     1.0f

/** @brief Free energy baseline for idle state */
#define IMAG_REASON_FEP_BASELINE_FREE_ENERGY     0.1f

/** @brief Maximum free energy ceiling */
#define IMAG_REASON_FEP_MAX_FREE_ENERGY          2.0f

/** @brief Prediction error decay rate per update cycle */
#define IMAG_REASON_FEP_ERROR_DECAY_RATE         0.95f

/** @brief Scenario quality weight to free energy */
#define IMAG_REASON_FEP_SCENARIO_WEIGHT          0.30f

/** @brief Reasoning coherence weight to free energy */
#define IMAG_REASON_FEP_COHERENCE_WEIGHT         0.35f

/** @brief Counterfactual validity weight to free energy */
#define IMAG_REASON_FEP_COUNTERFACTUAL_WEIGHT    0.35f

/** @brief Bio-async module ID for imagination-reasoning FEP bridge */
#define BIO_MODULE_IMAG_REASON_FEP               0x1610

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Bridge operational state
 */
typedef enum {
    IMAG_REASON_FEP_STATE_UNINITIALIZED = 0,   /**< Not yet initialized */
    IMAG_REASON_FEP_STATE_IDLE,                /**< Ready, no active scenarios */
    IMAG_REASON_FEP_STATE_ACTIVE,              /**< Processing scenarios, updating FEP */
    IMAG_REASON_FEP_STATE_DEGRADED,            /**< High free energy, reduced capacity */
    IMAG_REASON_FEP_STATE_ERROR                /**< Error state */
} imag_reason_fep_state_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP bridge configuration
 *
 * WHAT: Configuration parameters for imagination-reasoning FEP integration
 * WHY:  Allow tuning of free energy computation weights and thresholds
 * HOW:  Set weights for different uncertainty sources, define thresholds
 */
typedef struct {
    /* Feature enables */
    bool enable_logging;                      /**< Enable debug logging */
    uint32_t update_interval_ms;              /**< Update interval (default 50ms) */

    /* Weighting parameters */
    float free_energy_weight;                 /**< Overall weight for FE contribution */
    float scenario_quality_weight;            /**< Weight for scenario generation quality */
    float reasoning_coherence_weight;         /**< Weight for reasoning coherence error */
    float counterfactual_validity_weight;     /**< Weight for counterfactual validity */

    /* Thresholds */
    float high_free_energy_threshold;         /**< Threshold for degraded mode */
    float prediction_error_threshold;         /**< Threshold for surprise trigger */
    float coherence_epsilon;                  /**< Epsilon for coherence check */

    /* Normalization */
    float baseline_free_energy;               /**< Baseline free energy (idle) */
    float max_free_energy;                    /**< Maximum free energy ceiling */
    float error_decay_rate;                   /**< Prediction error decay per cycle */
} imag_reason_fep_config_t;

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
    uint64_t total_updates;                   /**< Total FEP update cycles */
    uint64_t scenario_evaluations;            /**< Scenario quality evaluations */
    uint64_t coherence_checks;                /**< Reasoning coherence checks performed */
    uint64_t counterfactual_analyses;         /**< Counterfactual analyses performed */

    /* Timing */
    float avg_update_time_us;                 /**< Average update duration (microseconds) */
    uint64_t total_update_time_us;            /**< Cumulative update time */

    /* FEP metrics */
    float total_free_energy_contribution;     /**< Cumulative FE contributed */
    float avg_free_energy;                    /**< Average free energy level */
    float peak_free_energy;                   /**< Peak free energy observed */

    /* Event counts */
    uint64_t degraded_mode_entries;           /**< Times entered degraded mode */
    uint64_t surprise_events;                 /**< High-surprise events */
    uint64_t creative_insights;               /**< Creative inference events */
} imag_reason_fep_stats_t;

/*=============================================================================
 * METRICS STRUCTURES
 *===========================================================================*/

/**
 * @brief FEP metrics for imagination-reasoning
 *
 * WHAT: Current free energy state from imagination-reasoning integration
 * WHY:  FEP orchestrator uses these to coordinate updates
 * HOW:  Updated during FEP update cycle from bridge statistics
 */
typedef struct {
    /* Core FEP metrics */
    float free_energy;                        /**< Current free energy estimate [0, MAX] */
    float prediction_error;                   /**< Accumulated prediction error [0, 1] */
    float surprise;                           /**< Bayesian surprise from unexpected outcomes */
    float entropy;                            /**< Scenario entropy (diversity of imagined states) */

    /* Imagination-reasoning specific metrics */
    float scenario_quality;                   /**< Quality of generated scenarios [0, 1] */
    float reasoning_coherence;                /**< Coherence of reasoning over scenarios [0, 1] */
    float counterfactual_validity;            /**< Validity of counterfactual inferences [0, 1] */
    float creative_novelty;                   /**< Novelty of creative inferences [0, 1] */

    /* Component contributions */
    float scenario_contribution;              /**< Free energy from scenario quality */
    float coherence_contribution;             /**< Free energy from reasoning coherence */
    float counterfactual_contribution;        /**< Free energy from counterfactual validity */

    /* Bridge state */
    uint32_t active_scenarios;                /**< Number of active scenarios */
    uint32_t completed_analyses;              /**< Completed counterfactual analyses */
    bool is_coherent;                         /**< Currently has coherent reasoning */

    /* Timing */
    uint64_t last_update_time_ms;             /**< Last FEP update timestamp */
    uint32_t update_count;                    /**< Total FEP updates performed */
} imag_reason_fep_metrics_t;

/*=============================================================================
 * OPAQUE BRIDGE HANDLE
 *===========================================================================*/

typedef struct imag_reason_fep_bridge imag_reason_fep_bridge_t;

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
typedef void (*imag_reason_fep_high_fe_callback_t)(
    imag_reason_fep_bridge_t* bridge,
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
 * @param source Source of surprise (scenario, coherence, counterfactual)
 * @param user_data User context
 */
typedef void (*imag_reason_fep_surprise_callback_t)(
    imag_reason_fep_bridge_t* bridge,
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
typedef void (*imag_reason_fep_metrics_callback_t)(
    imag_reason_fep_bridge_t* bridge,
    const imag_reason_fep_metrics_t* metrics,
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
imag_reason_fep_config_t imag_reason_fep_config_default(void);

/**
 * @brief Create FEP bridge for imagination-reasoning
 *
 * WHAT: Initialize FEP bridge infrastructure
 * WHY:  Enable FEP orchestrator integration for imagination-reasoning module
 * HOW:  Allocate state, configure parameters, prepare for registration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
imag_reason_fep_bridge_t* imag_reason_fep_bridge_create(
    const imag_reason_fep_config_t* config
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
void imag_reason_fep_bridge_destroy(imag_reason_fep_bridge_t* bridge);

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
int imag_reason_fep_bridge_reset(imag_reason_fep_bridge_t* bridge);

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Register bridge with FEP orchestrator
 *
 * WHAT: Add imagination-reasoning to FEP coordination
 * WHY:  Enable system-wide free energy minimization
 * HOW:  Register update callback with orchestrator at cognitive timescale (50ms)
 *
 * @param bridge Bridge handle
 * @param orchestrator FEP orchestrator instance
 * @param imag_reason_bridge Imagination-reasoning bridge (can be NULL for standalone testing)
 * @param bridge_id_out Output: assigned FEP bridge ID
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int imag_reason_fep_bridge_register(
    imag_reason_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    imagination_reasoning_bridge_t* imag_reason_bridge,
    uint32_t* bridge_id_out
);

/**
 * @brief Unregister bridge from FEP orchestrator
 *
 * WHAT: Remove imagination-reasoning from FEP coordination
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unregister from orchestrator, clear references
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * THREAD-SAFE: Yes
 */
int imag_reason_fep_bridge_unregister(imag_reason_fep_bridge_t* bridge);

/**
 * @brief Check if bridge is registered
 *
 * @param bridge Bridge handle
 * @return true if registered with orchestrator
 */
bool imag_reason_fep_bridge_is_registered(imag_reason_fep_bridge_t* bridge);

/**
 * @brief Get the FEP bridge ID assigned by orchestrator
 *
 * @param bridge Bridge handle
 * @return Bridge ID, or 0 if not registered
 */
uint32_t imag_reason_fep_bridge_get_id(imag_reason_fep_bridge_t* bridge);

/*=============================================================================
 * FEP UPDATE CALLBACK (Internal - used by FEP orchestrator)
 *===========================================================================*/

/**
 * @brief FEP update callback for imagination-reasoning
 *
 * WHAT: Called by FEP orchestrator during cognitive update cycle (50ms)
 * WHY:  Compute free energy from imagination-reasoning metrics, update predictions
 * HOW:  Query bridge stats, compute FE components, update metrics
 *
 * FREE ENERGY COMPUTATION:
 * - Scenario contribution: (1 - scenario_quality) * scenario_weight
 * - Coherence contribution: (1 - reasoning_coherence) * coherence_weight
 * - Counterfactual contribution: (1 - counterfactual_validity) * counterfactual_weight
 * - Total FE = baseline + sum of contributions
 *
 * @param handle Opaque handle (imag_reason_fep_bridge_t*)
 * @return 0 on success, -1 on error
 */
int imag_reason_fep_update_callback(void* handle);

/**
 * @brief FEP destroy callback for cleanup
 *
 * WHAT: Called by FEP orchestrator when unregistering
 * WHY:  Allow bridge-specific cleanup if needed
 * HOW:  Currently no-op (bridge destroyed separately)
 *
 * @param handle Opaque handle (imag_reason_fep_bridge_t*)
 */
void imag_reason_fep_destroy_callback(void* handle);

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
int imag_reason_fep_bridge_force_update(imag_reason_fep_bridge_t* bridge);

/**
 * @brief Manually update scenario quality
 *
 * WHAT: Set current scenario generation quality value
 * WHY:  Allow external components to inject quality measurements
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param quality Scenario quality [0, 1] (1 = high quality)
 * @return 0 on success, -1 on failure
 */
int imag_reason_fep_bridge_update_scenario_quality(
    imag_reason_fep_bridge_t* bridge,
    float quality
);

/**
 * @brief Manually update reasoning coherence
 *
 * WHAT: Set current reasoning coherence value
 * WHY:  Allow reasoning module to inject coherence measurements
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param coherence Reasoning coherence [0, 1] (1 = fully coherent)
 * @return 0 on success, -1 on failure
 */
int imag_reason_fep_bridge_update_reasoning_coherence(
    imag_reason_fep_bridge_t* bridge,
    float coherence
);

/**
 * @brief Manually update counterfactual validity
 *
 * WHAT: Set counterfactual analysis validity
 * WHY:  Allow counterfactual evaluators to report accuracy
 * HOW:  Update internal metric, triggers FE recalculation
 *
 * @param bridge Bridge handle
 * @param validity Counterfactual validity [0, 1] (1 = valid)
 * @return 0 on success, -1 on failure
 */
int imag_reason_fep_bridge_update_counterfactual_validity(
    imag_reason_fep_bridge_t* bridge,
    float validity
);

/**
 * @brief Manually update creative novelty
 *
 * WHAT: Set creative inference novelty value
 * WHY:  Track information gain from creative inference
 * HOW:  Update internal metric for tracking
 *
 * @param bridge Bridge handle
 * @param novelty Creative novelty [0, 1] (1 = highly novel)
 * @return 0 on success, -1 on failure
 */
int imag_reason_fep_bridge_update_creative_novelty(
    imag_reason_fep_bridge_t* bridge,
    float novelty
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
int imag_reason_fep_bridge_get_metrics(
    const imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_metrics_t* metrics_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, -1 on error
 */
int imag_reason_fep_bridge_get_stats(
    const imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_stats_t* stats_out
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int imag_reason_fep_bridge_reset_stats(imag_reason_fep_bridge_t* bridge);

/**
 * @brief Get current free energy contribution
 *
 * @param bridge Bridge handle
 * @return Current free energy, -1.0f on error
 */
float imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge_t* bridge);

/**
 * @brief Get current scenario quality
 *
 * @param bridge Bridge handle
 * @return Scenario quality [0, 1], -1.0f on error
 */
float imag_reason_fep_bridge_get_scenario_quality(
    const imag_reason_fep_bridge_t* bridge
);

/**
 * @brief Get current reasoning coherence
 *
 * @param bridge Bridge handle
 * @return Reasoning coherence [0, 1], -1.0f on error
 */
float imag_reason_fep_bridge_get_reasoning_coherence(
    const imag_reason_fep_bridge_t* bridge
);

/**
 * @brief Get current prediction error
 *
 * @param bridge Bridge handle
 * @return Current prediction error, -1.0f on error
 */
float imag_reason_fep_bridge_get_prediction_error(
    const imag_reason_fep_bridge_t* bridge
);

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
imag_reason_fep_state_t imag_reason_fep_bridge_get_state(
    const imag_reason_fep_bridge_t* bridge
);

/**
 * @brief Check if in degraded mode
 *
 * @param bridge Bridge handle
 * @return true if free energy is above threshold
 */
bool imag_reason_fep_bridge_is_degraded(imag_reason_fep_bridge_t* bridge);

/**
 * @brief Check if reasoning is coherent
 *
 * @param bridge Bridge handle
 * @return true if reasoning coherence is above epsilon
 */
bool imag_reason_fep_bridge_is_coherent(imag_reason_fep_bridge_t* bridge);

/**
 * @brief Get state name as string
 *
 * @param state Bridge state
 * @return Human-readable state name
 */
const char* imag_reason_fep_state_name(imag_reason_fep_state_t state);

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
int imag_reason_fep_bridge_set_high_fe_callback(
    imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_high_fe_callback_t callback,
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
int imag_reason_fep_bridge_set_surprise_callback(
    imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_surprise_callback_t callback,
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
int imag_reason_fep_bridge_set_metrics_callback(
    imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_metrics_callback_t callback,
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
int imag_reason_fep_bridge_set_config(
    imag_reason_fep_bridge_t* bridge,
    const imag_reason_fep_config_t* config
);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config_out Output: current configuration
 * @return 0 on success, -1 on failure
 */
int imag_reason_fep_bridge_get_config(
    const imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_config_t* config_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMAGINATION_REASONING_FEP_BRIDGE_H */
