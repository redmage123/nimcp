/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_neurogenesis_fep_bridge.h - Neurogenesis to Free Energy Principle Bridge
//=============================================================================
/**
 * @file nimcp_neurogenesis_fep_bridge.h
 * @brief Bridge between neurogenesis and Free Energy Principle systems
 *
 * WHAT: Connects neurogenesis with active inference and free energy minimization,
 *       framing new neuron creation as uncertainty reduction in world models.
 *
 * WHY:  Bridges the gap between:
 *       - Neurogenesis (network expansion for pattern representation)
 *       - Free Energy Principle (prediction error minimization)
 *       - Active inference (action selection under uncertainty)
 *
 * HOW:  Bidirectional integration:
 *       1. Neurogenesis -> FEP: New neurons reduce prediction complexity
 *       2. FEP -> Neurogenesis: High prediction error triggers proliferation
 *       3. Precision weighting of new neuron contributions
 *       4. Variational free energy guides integration
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROGENESIS                          FREE ENERGY PRINCIPLE
 * -----------------------------------------------------------------
 * Stem cell proliferation            <- High prediction errors
 * New neuron differentiation         -> Expanded representational capacity
 * Hippocampal neurogenesis           -> Pattern separation (reduce overlap)
 * Activity-dependent survival        <- Prediction accuracy contribution
 * Environmental enrichment           <- Novelty/complexity signals
 * BDNF/growth factors                <- Expected free energy gradients
 * ```
 *
 * FEP-NEUROGENESIS COUPLING:
 * - Prediction error accumulation triggers proliferation
 * - New neurons increase model complexity (Occam penalty)
 * - Survival requires prediction error reduction contribution
 * - Integration optimizes variational free energy
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NEUROGENESIS_FEP_BRIDGE_H
#define NIMCP_NEUROGENESIS_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define NEUROGENESIS_FEP_MODULE_NAME        "neurogenesis_fep_bridge"

/** Maximum tracked prediction errors */
#define NEUROGENESIS_FEP_MAX_ERRORS         512

/** Maximum new neurons tracked */
#define NEUROGENESIS_FEP_MAX_NEURONS        256

/** Default prediction error threshold for proliferation */
#define NEUROGENESIS_FEP_ERROR_THRESHOLD    0.5f

/** Default complexity penalty for new neurons */
#define NEUROGENESIS_FEP_COMPLEXITY_PENALTY 0.1f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Neurogenesis trigger mode from FEP signals
 */
typedef enum {
    NG_FEP_TRIGGER_ERROR = 0,        /**< High prediction error triggers */
    NG_FEP_TRIGGER_COMPLEXITY,       /**< Model complexity needs increase */
    NG_FEP_TRIGGER_NOVELTY,          /**< Novelty detection triggers */
    NG_FEP_TRIGGER_EXPECTED_FE       /**< Expected free energy gradient */
} ng_fep_trigger_mode_t;

/**
 * @brief Survival criterion based on FEP contribution
 */
typedef enum {
    NG_FEP_SURVIVE_ERROR_REDUCE = 0, /**< Must reduce prediction error */
    NG_FEP_SURVIVE_PRECISION,        /**< Must increase precision */
    NG_FEP_SURVIVE_FREE_ENERGY,      /**< Must reduce variational free energy */
    NG_FEP_SURVIVE_INFO_GAIN         /**< Must contribute information gain */
} ng_fep_survival_mode_t;

/**
 * @brief Precision weighting strategy for new neurons
 */
typedef enum {
    NG_FEP_PRECISION_LOW = 0,        /**< Start with low precision */
    NG_FEP_PRECISION_ADAPTIVE,       /**< Adapt precision based on accuracy */
    NG_FEP_PRECISION_CONTEXTUAL,     /**< Context-dependent precision */
    NG_FEP_PRECISION_INHERITED       /**< Inherit from parent niche */
} ng_fep_precision_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for neurogenesis-FEP bridge
 */
typedef struct {
    /** Proliferation trigger parameters */
    ng_fep_trigger_mode_t trigger_mode;  /**< What triggers neurogenesis */
    float error_threshold;               /**< Prediction error threshold */
    float novelty_threshold;             /**< Novelty signal threshold */
    float complexity_ceiling;            /**< Max model complexity allowed */
    float trigger_accumulation_time;     /**< Time to accumulate trigger signal */

    /** Survival parameters */
    ng_fep_survival_mode_t survival_mode; /**< Survival criterion */
    float survival_threshold;             /**< Minimum FEP contribution */
    float evaluation_window;              /**< Window for survival evaluation */
    bool enable_competitive_survival;     /**< Competition among new neurons */

    /** Precision weighting */
    ng_fep_precision_mode_t precision_mode; /**< Precision strategy */
    float initial_precision;              /**< Starting precision */
    float precision_learning_rate;        /**< How fast precision adapts */
    float min_precision;                  /**< Minimum precision floor */
    float max_precision;                  /**< Maximum precision ceiling */

    /** Free energy coupling */
    bool enable_complexity_penalty;       /**< Penalize model expansion */
    float complexity_penalty_weight;      /**< Weight of complexity penalty */
    float integration_free_energy_weight; /**< FE weight in integration */
    bool enable_expected_fe_guidance;     /**< Use expected FE for guidance */

    /** Update parameters */
    float update_interval_ms;             /**< Bridge update interval */
    bool enable_logging;
    bool enable_metrics;
} ng_fep_config_t;

/**
 * @brief Prediction error signal for neurogenesis
 */
typedef struct {
    uint32_t source_region;              /**< Region generating error */
    float sensory_error;                 /**< Sensory prediction error */
    float state_error;                   /**< State estimation error */
    float precision;                     /**< Error precision weighting */
    float accumulated_error;             /**< Accumulated over time */
    float novelty_component;             /**< Novelty contribution */
    uint64_t timestamp;                  /**< When error measured */
} ng_fep_error_signal_t;

/**
 * @brief New neuron FEP contribution state
 */
typedef struct {
    uint32_t neuron_id;                  /**< Neuron identifier */
    float precision;                     /**< Current precision weighting */
    float prediction_contribution;       /**< Error reduction contributed */
    float complexity_cost;               /**< Model complexity added */
    float net_free_energy_delta;         /**< Net FE change from neuron */
    float information_gain;              /**< Information gained */
    float survival_score;                /**< Current survival metric */
    bool survival_threshold_met;         /**< Has met survival threshold */
} ng_fep_neuron_state_t;

/**
 * @brief Free energy components for neurogenesis
 */
typedef struct {
    float variational_fe;                /**< Current variational FE */
    float accuracy;                      /**< -log p(o|s) */
    float complexity;                    /**< KL(q(s)||p(s)) */
    float expected_fe;                   /**< Expected FE under policy */
    float epistemic_value;               /**< Information gain expected */
    float pragmatic_value;               /**< Goal achievement expected */
} ng_fep_free_energy_t;

/**
 * @brief Proliferation recommendation from FEP
 */
typedef struct {
    bool should_proliferate;             /**< Recommend neurogenesis */
    float urgency;                       /**< Urgency level (0-1) */
    uint32_t target_niche;               /**< Recommended niche */
    float expected_benefit;              /**< Expected FE reduction */
    float complexity_cost;               /**< Expected complexity increase */
    ng_fep_trigger_mode_t trigger_type;  /**< What triggered recommendation */
} ng_fep_proliferation_signal_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t proliferation_signals;      /**< Total proliferation signals */
    uint64_t neurons_evaluated;          /**< Neurons evaluated for survival */
    uint64_t neurons_survived;           /**< Neurons meeting FEP criteria */
    uint64_t neurons_pruned_fep;         /**< Neurons pruned by FEP criteria */
    float avg_prediction_error;          /**< Average prediction error */
    float avg_precision;                 /**< Average new neuron precision */
    float total_complexity_added;        /**< Total model complexity added */
    float total_fe_reduction;            /**< Total FE reduction achieved */
    float current_variational_fe;        /**< Current variational FE */
    uint64_t update_count;               /**< Total updates */
    float last_update_ms;                /**< Last update timestamp */
} ng_fep_stats_t;

/** Opaque bridge handle */
typedef struct ng_fep_bridge_struct ng_fep_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_default_config(ng_fep_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create neurogenesis-FEP bridge
 *
 * WHAT: Creates bridge for FEP-guided neurogenesis
 * WHY:  Enable prediction error-driven network expansion
 * HOW:  Tracks errors, evaluates contributions, guides proliferation
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ng_fep_bridge_t* ng_fep_bridge_create(
    const ng_fep_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ng_fep_bridge_destroy(ng_fep_bridge_t* bridge);

//=============================================================================
// Error Signal API (FEP -> Neurogenesis)
//=============================================================================

/**
 * @brief Report prediction error signal
 *
 * WHAT: Records prediction error for proliferation trigger
 * WHY:  High errors indicate need for expanded representations
 * HOW:  Accumulates weighted error over time
 *
 * @param bridge Bridge handle
 * @param error Error signal data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_report_error(
    ng_fep_bridge_t* bridge,
    const ng_fep_error_signal_t* error
);

/**
 * @brief Report novelty signal
 *
 * WHAT: Records novelty detection signal
 * WHY:  Novelty suggests need for new representations
 * HOW:  Weighted contribution to proliferation trigger
 *
 * @param bridge Bridge handle
 * @param novelty Novelty magnitude (0-1)
 * @param region Source region
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_report_novelty(
    ng_fep_bridge_t* bridge,
    float novelty,
    uint32_t region
);

/**
 * @brief Get proliferation recommendation
 *
 * WHAT: Evaluates current FEP signals for proliferation
 * WHY:  Determines if neurogenesis should occur
 * HOW:  Aggregates errors, novelty, complexity headroom
 *
 * @param bridge Bridge handle
 * @param signal Output proliferation signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_get_proliferation_signal(
    ng_fep_bridge_t* bridge,
    ng_fep_proliferation_signal_t* signal
);

/**
 * @brief Update free energy state
 *
 * WHAT: Updates current free energy components
 * WHY:  Track global FE for neurogenesis decisions
 * HOW:  External FEP system provides components
 *
 * @param bridge Bridge handle
 * @param fe Free energy components
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_update_free_energy(
    ng_fep_bridge_t* bridge,
    const ng_fep_free_energy_t* fe
);

//=============================================================================
// Neuron Contribution API (Neurogenesis -> FEP)
//=============================================================================

/**
 * @brief Register new neuron for FEP evaluation
 *
 * WHAT: Starts FEP contribution tracking for new neuron
 * WHY:  Evaluate neuron's contribution to FE reduction
 * HOW:  Initializes precision, starts contribution tracking
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_register_neuron(
    ng_fep_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Report neuron prediction contribution
 *
 * WHAT: Records neuron's prediction accuracy
 * WHY:  Track contribution to FE reduction
 * HOW:  Updates running contribution metric
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param prediction_error Error in neuron's predictions
 * @param contribution_weight How much neuron contributed
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_report_contribution(
    ng_fep_bridge_t* bridge,
    uint32_t neuron_id,
    float prediction_error,
    float contribution_weight
);

/**
 * @brief Update neuron precision
 *
 * WHAT: Updates precision weighting for neuron
 * WHY:  Precision adapts based on reliability
 * HOW:  Increases for accurate, decreases for inaccurate
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param accuracy Recent prediction accuracy (0-1)
 * @return New precision value
 */
NIMCP_EXPORT float ng_fep_update_precision(
    ng_fep_bridge_t* bridge,
    uint32_t neuron_id,
    float accuracy
);

/**
 * @brief Get neuron FEP state
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_get_neuron_state(
    const ng_fep_bridge_t* bridge,
    uint32_t neuron_id,
    ng_fep_neuron_state_t* state
);

//=============================================================================
// Survival Evaluation API
//=============================================================================

/**
 * @brief Evaluate neuron for FEP-based survival
 *
 * WHAT: Determines if neuron meets FEP survival criteria
 * WHY:  Neurons must justify their complexity cost
 * HOW:  Compares contribution against complexity penalty
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Survival score (>0 survives)
 */
NIMCP_EXPORT float ng_fep_evaluate_survival(
    ng_fep_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Check if neuron should be pruned (FEP criteria)
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return true if should prune
 */
NIMCP_EXPORT bool ng_fep_should_prune(
    const ng_fep_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Get expected FE change from pruning neuron
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Expected FE change (negative = FE would increase if pruned)
 */
NIMCP_EXPORT float ng_fep_prune_cost(
    const ng_fep_bridge_t* bridge,
    uint32_t neuron_id
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Update contributions, evaluate survival, manage triggers
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_update(
    ng_fep_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_reset(ng_fep_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_get_stats(
    const ng_fep_bridge_t* bridge,
    ng_fep_stats_t* stats
);

/**
 * @brief Get current free energy components
 *
 * @param bridge Bridge handle
 * @param fe Output free energy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_fep_get_free_energy(
    const ng_fep_bridge_t* bridge,
    ng_fep_free_energy_t* fe
);

/**
 * @brief Get accumulated prediction error
 *
 * @param bridge Bridge handle
 * @param region Region to query (or UINT32_MAX for total)
 * @return Accumulated error
 */
NIMCP_EXPORT float ng_fep_get_accumulated_error(
    const ng_fep_bridge_t* bridge,
    uint32_t region
);

/**
 * @brief Get top contributing neurons
 *
 * @param bridge Bridge handle
 * @param ids Output array
 * @param scores Output contribution scores
 * @param max_count Maximum to return
 * @return Number returned
 */
NIMCP_EXPORT int ng_fep_get_top_contributors(
    const ng_fep_bridge_t* bridge,
    uint32_t* ids,
    float* scores,
    uint32_t max_count
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROGENESIS_FEP_BRIDGE_H */