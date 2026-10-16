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
// nimcp_ephaptic_fep_bridge.h - Ephaptic to Free Energy Principle Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_fep_bridge.h
 * @brief Bridge between Ephaptic coupling and Free Energy Principle framework
 *
 * WHAT: Connects ephaptic field dynamics with the Free Energy Principle (FEP),
 *       enabling field-based prediction error computation and precision weighting.
 *
 * WHY:  Bridges the gap between:
 *       - Ephaptic synchronization (coherent neural activity)
 *       - Predictive processing (prediction errors, precision)
 *       - Variational inference (free energy minimization)
 *       Ephaptic coherence provides a natural measure of precision,
 *       and field synchronization can gate prediction error propagation.
 *
 * HOW:  Integration mechanisms:
 *       1. Phase coherence maps to precision (inverse variance)
 *       2. Coherence gates prediction error propagation
 *       3. Field synchronization events trigger model updates
 *       4. LFP phase modulates sensory vs. prediction weighting
 *
 * BIOLOGICAL BASIS:
 * ```
 * EPHAPTIC COUPLING                      FREE ENERGY PRINCIPLE
 * ─────────────────────────────────────────────────────────────────────
 * Phase coherence (order parameter)   -> Precision (confidence) weighting
 * Field synchronization               -> Prediction error gating
 * LFP amplitude                       -> Sensory evidence strength
 * Kuramoto coupling K                 -> Prior strength/precision
 * Coherence transitions               -> Model update triggers
 * Field spatial pattern               -> Hierarchical message routing
 * ```
 *
 * KEY MECHANISMS:
 * - Coherence-as-precision: High coherence = high precision = strong weighting
 * - Sync-gated errors: Only propagate prediction errors during coherent states
 * - Phase-dependent inference: LFP phase biases prior vs. likelihood
 * - Free energy from field: Field disorder relates to variational free energy
 *
 * REFERENCES:
 * - Friston (2010) "The free-energy principle: a unified brain theory?"
 * - Feldman & Friston (2010) "Attention, uncertainty, and free-energy"
 * - Bastos et al. (2012) "Canonical microcircuits for predictive coding"
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_FEP_BRIDGE_H
#define NIMCP_EPHAPTIC_FEP_BRIDGE_H

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
#define EPHAPTIC_FEP_MODULE_NAME            "ephaptic_fep_bridge"

/** Maximum prediction units tracked */
#define EPHAPTIC_FEP_MAX_UNITS              2048

/** Maximum hierarchical levels */
#define EPHAPTIC_FEP_MAX_LEVELS             8

/** Default coherence-to-precision scaling */
#define EPHAPTIC_FEP_DEFAULT_PRECISION_SCALE    10.0f

/** Default prediction error threshold for update */
#define EPHAPTIC_FEP_DEFAULT_PE_THRESHOLD       0.1f

/** Default coherence threshold for error gating */
#define EPHAPTIC_FEP_DEFAULT_COHERENCE_GATE     0.5f

/** Default free energy decay time constant (ms) */
#define EPHAPTIC_FEP_DEFAULT_FE_TAU             100.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Precision estimation mode
 */
typedef enum {
    EPHAPTIC_FEP_PREC_COHERENCE = 0,      /**< Precision from coherence directly */
    EPHAPTIC_FEP_PREC_FIELD_STABILITY,    /**< Precision from field stability */
    EPHAPTIC_FEP_PREC_SYNC_DURATION,      /**< Precision from sync bout duration */
    EPHAPTIC_FEP_PREC_COMBINED            /**< Combined estimation */
} ephaptic_fep_precision_mode_t;

/**
 * @brief Prediction error gating mode
 */
typedef enum {
    EPHAPTIC_FEP_GATE_NONE = 0,           /**< No gating (always propagate) */
    EPHAPTIC_FEP_GATE_COHERENCE,          /**< Gate by coherence threshold */
    EPHAPTIC_FEP_GATE_PHASE,              /**< Gate by LFP phase window */
    EPHAPTIC_FEP_GATE_SYNC_EVENT          /**< Gate by sync event occurrence */
} ephaptic_fep_gate_mode_t;

/**
 * @brief Model update trigger mode
 */
typedef enum {
    EPHAPTIC_FEP_UPDATE_CONTINUOUS = 0,   /**< Continuous updates */
    EPHAPTIC_FEP_UPDATE_SYNC_EVENT,       /**< Update on sync events */
    EPHAPTIC_FEP_UPDATE_COHERENCE_PEAK,   /**< Update at coherence peaks */
    EPHAPTIC_FEP_UPDATE_FREE_ENERGY       /**< Update when FE exceeds threshold */
} ephaptic_fep_update_mode_t;

/**
 * @brief Hierarchical level for message passing
 */
typedef enum {
    EPHAPTIC_FEP_LEVEL_SENSORY = 0,       /**< Sensory (bottom) level */
    EPHAPTIC_FEP_LEVEL_PERCEPTUAL,        /**< Perceptual level */
    EPHAPTIC_FEP_LEVEL_CONCEPTUAL,        /**< Conceptual level */
    EPHAPTIC_FEP_LEVEL_META               /**< Meta-cognitive (top) level */
} ephaptic_fep_hierarchy_level_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for ephaptic-FEP bridge
 */
typedef struct {
    /** Precision estimation */
    ephaptic_fep_precision_mode_t precision_mode;  /**< Precision computation mode */
    float precision_scale;                          /**< Coherence->precision scale */
    float baseline_precision;                       /**< Minimum precision value */
    float max_precision;                            /**< Maximum precision cap */

    /** Prediction error gating */
    ephaptic_fep_gate_mode_t gate_mode;            /**< Error gating mode */
    float coherence_gate_threshold;                 /**< Coherence for gating */
    float phase_gate_center;                        /**< Phase center for gating (rad) */
    float phase_gate_width;                         /**< Phase window width (rad) */

    /** Model update parameters */
    ephaptic_fep_update_mode_t update_mode;        /**< Update trigger mode */
    float update_threshold;                         /**< Threshold for updates */
    float learning_rate;                            /**< Model update learning rate */

    /** Free energy computation */
    float free_energy_tau;                          /**< FE decay time constant */
    bool compute_surprise;                          /**< Track surprise separately */
    bool compute_complexity;                        /**< Track complexity term */

    /** Hierarchical parameters */
    uint32_t num_levels;                            /**< Number of hierarchy levels */
    bool enable_precision_cascade;                  /**< Cascade precision across levels */
    float inter_level_coupling;                     /**< Coupling between levels */

    /** Update parameters */
    float update_interval_ms;                       /**< Bridge update interval */
} ephaptic_fep_config_t;

/**
 * @brief Precision estimate from ephaptic state
 */
typedef struct {
    float precision;                                /**< Estimated precision */
    float confidence;                               /**< Confidence in estimate */
    float coherence_contribution;                   /**< From phase coherence */
    float stability_contribution;                   /**< From field stability */
    float duration_contribution;                    /**< From sync duration */
    bool high_precision;                            /**< Above threshold flag */
} ephaptic_fep_precision_t;

/**
 * @brief Prediction error with ephaptic modulation
 */
typedef struct {
    uint32_t unit_id;                               /**< Prediction unit ID */
    float raw_error;                                /**< Unweighted prediction error */
    float precision_weighted_error;                 /**< Precision-weighted error */
    float precision_at_error;                       /**< Precision when error occurred */
    bool propagated;                                /**< Was error propagated */
    float gate_factor;                              /**< Gating factor applied */
    ephaptic_fep_hierarchy_level_t level;          /**< Hierarchical level */
} ephaptic_fep_prediction_error_t;

/**
 * @brief Free energy state
 */
typedef struct {
    float total_free_energy;                        /**< Total variational FE */
    float accuracy_term;                            /**< -log likelihood (accuracy) */
    float complexity_term;                          /**< KL divergence (complexity) */
    float surprise;                                 /**< Instantaneous surprise */
    float entropy;                                  /**< Entropy estimate */
    float free_energy_rate;                         /**< Rate of FE change */
    bool minimizing;                                /**< Is FE decreasing */
} ephaptic_fep_free_energy_t;

/**
 * @brief Model update event
 */
typedef struct {
    float event_time_ms;                            /**< Event timestamp */
    float coherence_at_event;                       /**< Coherence level */
    float precision_at_event;                       /**< Precision level */
    float free_energy_before;                       /**< FE before update */
    float free_energy_after;                        /**< FE after update */
    float delta_free_energy;                        /**< Change in FE */
    uint32_t units_updated;                         /**< Number of units updated */
    ephaptic_fep_hierarchy_level_t trigger_level;  /**< Level that triggered */
} ephaptic_fep_update_event_t;

/**
 * @brief Hierarchical level state
 */
typedef struct {
    ephaptic_fep_hierarchy_level_t level;          /**< Level identifier */
    float precision;                                /**< Level precision */
    float prediction_error;                         /**< Aggregate PE at level */
    float free_energy;                              /**< Level free energy */
    float coherence_coupling;                       /**< Coupling to ephaptic */
    bool active;                                    /**< Is level active */
} ephaptic_fep_level_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t precision_estimates;                   /**< Precision computations */
    uint64_t errors_gated;                          /**< Prediction errors gated */
    uint64_t errors_propagated;                     /**< Prediction errors propagated */
    uint64_t model_updates;                         /**< Model update events */
    float avg_precision;                            /**< Average precision */
    float avg_free_energy;                          /**< Average free energy */
    float avg_prediction_error;                     /**< Average PE magnitude */
    float min_free_energy;                          /**< Minimum FE achieved */
    float max_coherence;                            /**< Maximum coherence seen */
    float last_update_ms;                           /**< Last update timestamp */
} ephaptic_fep_stats_t;

/** Opaque bridge handle */
typedef struct ephaptic_fep_bridge_struct ephaptic_fep_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration values
 * WHY:  Provide biologically plausible starting parameters
 * HOW:  Based on predictive coding literature
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_default_config(ephaptic_fep_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ephaptic-FEP bridge
 *
 * WHAT: Allocates and initializes bridge instance
 * WHY:  Enable ephaptic-informed predictive processing
 * HOW:  Sets up precision estimation, error gating, FE tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ephaptic_fep_bridge_t* ephaptic_fep_bridge_create(
    const ephaptic_fep_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void ephaptic_fep_bridge_destroy(ephaptic_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_bridge_reset(ephaptic_fep_bridge_t* bridge);

//=============================================================================
// Precision Estimation API
//=============================================================================

/**
 * @brief Estimate precision from ephaptic state
 *
 * WHAT: Computes precision (inverse variance) from ephaptic coherence
 * WHY:  Coherence indicates reliable neural representation
 * HOW:  Maps order parameter to precision via configured scale
 *
 * BIOLOGICAL: Synchronized neural populations represent information
 * with less noise (higher precision), enabling stronger weighting
 * in Bayesian inference.
 *
 * @param bridge      Bridge handle
 * @param coherence   Phase coherence [0,1]
 * @param field_stability Field stability measure [0,1]
 * @param sync_duration   Duration of current sync bout (ms)
 * @param precision   Output precision estimate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_estimate_precision(
    ephaptic_fep_bridge_t* bridge,
    float coherence,
    float field_stability,
    float sync_duration,
    ephaptic_fep_precision_t* precision
);

/**
 * @brief Set precision for hierarchical level
 *
 * WHAT: Assigns precision to a hierarchy level
 * WHY:  Different levels have different precisions
 * HOW:  Stores precision for level-specific weighting
 *
 * @param bridge    Bridge handle
 * @param level     Hierarchical level
 * @param precision Precision value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_set_level_precision(
    ephaptic_fep_bridge_t* bridge,
    ephaptic_fep_hierarchy_level_t level,
    float precision
);

//=============================================================================
// Prediction Error API
//=============================================================================

/**
 * @brief Process prediction error with ephaptic gating
 *
 * WHAT: Gates and weights prediction error by ephaptic state
 * WHY:  Only propagate reliable errors (during coherent states)
 * HOW:  Applies gating and precision weighting
 *
 * @param bridge    Bridge handle
 * @param unit_id   Prediction unit identifier
 * @param raw_error Raw prediction error magnitude
 * @param level     Hierarchical level of error
 * @param coherence Current coherence
 * @param lfp_phase Current LFP phase
 * @param result    Output processed error
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_process_error(
    ephaptic_fep_bridge_t* bridge,
    uint32_t unit_id,
    float raw_error,
    ephaptic_fep_hierarchy_level_t level,
    float coherence,
    float lfp_phase,
    ephaptic_fep_prediction_error_t* result
);

/**
 * @brief Check if prediction error should propagate
 *
 * WHAT: Determines if current state allows error propagation
 * WHY:  Gating prevents noise from corrupting inference
 * HOW:  Checks gate mode criteria
 *
 * @param bridge    Bridge handle
 * @param coherence Current coherence
 * @param lfp_phase Current LFP phase
 * @return true if errors should propagate
 */
NIMCP_EXPORT bool ephaptic_fep_should_propagate(
    const ephaptic_fep_bridge_t* bridge,
    float coherence,
    float lfp_phase
);

/**
 * @brief Get precision-weighted error for level
 *
 * @param bridge Bridge handle
 * @param level  Hierarchical level
 * @param weighted_error Output: precision-weighted error
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_get_weighted_error(
    const ephaptic_fep_bridge_t* bridge,
    ephaptic_fep_hierarchy_level_t level,
    float* weighted_error
);

//=============================================================================
// Free Energy API
//=============================================================================

/**
 * @brief Compute free energy from ephaptic state
 *
 * WHAT: Estimates variational free energy
 * WHY:  FE provides a unified objective for inference
 * HOW:  Combines accuracy and complexity terms
 *
 * @param bridge       Bridge handle
 * @param coherence    Current coherence (relates to accuracy)
 * @param field_entropy Field entropy (relates to complexity)
 * @param free_energy  Output free energy state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_compute_free_energy(
    ephaptic_fep_bridge_t* bridge,
    float coherence,
    float field_entropy,
    ephaptic_fep_free_energy_t* free_energy
);

/**
 * @brief Update free energy with new observation
 *
 * WHAT: Integrates new information into FE estimate
 * WHY:  Track FE dynamics over time
 * HOW:  Exponential moving average with configured tau
 *
 * @param bridge           Bridge handle
 * @param prediction_error New prediction error
 * @param precision        Precision of observation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_update_free_energy(
    ephaptic_fep_bridge_t* bridge,
    float prediction_error,
    float precision
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @param fe     Output: current free energy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_get_free_energy(
    const ephaptic_fep_bridge_t* bridge,
    float* fe
);

/**
 * @brief Check if free energy is minimizing
 *
 * @param bridge Bridge handle
 * @return true if FE is decreasing
 */
NIMCP_EXPORT bool ephaptic_fep_is_minimizing(
    const ephaptic_fep_bridge_t* bridge
);

//=============================================================================
// Model Update API
//=============================================================================

/**
 * @brief Check if model update should occur
 *
 * WHAT: Determines if current state warrants model update
 * WHY:  Updates should occur at appropriate times (sync events)
 * HOW:  Checks update mode criteria
 *
 * @param bridge    Bridge handle
 * @param coherence Current coherence
 * @return true if update should occur
 */
NIMCP_EXPORT bool ephaptic_fep_should_update(
    ephaptic_fep_bridge_t* bridge,
    float coherence
);

/**
 * @brief Trigger model update
 *
 * WHAT: Initiates model parameter update
 * WHY:  Update generative model based on accumulated errors
 * HOW:  Applies learning rate-scaled gradient
 *
 * @param bridge  Bridge handle
 * @param event   Output update event
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_trigger_update(
    ephaptic_fep_bridge_t* bridge,
    ephaptic_fep_update_event_t* event
);

//=============================================================================
// Hierarchy API
//=============================================================================

/**
 * @brief Get level state
 *
 * @param bridge Bridge handle
 * @param level  Hierarchical level
 * @param state  Output level state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_get_level_state(
    const ephaptic_fep_bridge_t* bridge,
    ephaptic_fep_hierarchy_level_t level,
    ephaptic_fep_level_state_t* state
);

/**
 * @brief Propagate precision up hierarchy
 *
 * WHAT: Cascades precision estimates up levels
 * WHY:  Higher levels inherit lower-level precision
 * HOW:  Applies decay factor per level
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_cascade_precision(ephaptic_fep_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Decay FE, update precision, check triggers
 * HOW:  Called during simulation step
 *
 * @param bridge      Bridge handle
 * @param dt_ms       Time step in milliseconds
 * @param coherence   Current ephaptic coherence
 * @param lfp_phase   Current LFP phase
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_update(
    ephaptic_fep_bridge_t* bridge,
    float dt_ms,
    float coherence,
    float lfp_phase
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats  Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_get_stats(
    const ephaptic_fep_bridge_t* bridge,
    ephaptic_fep_stats_t* stats
);

/**
 * @brief Get current precision
 *
 * @param bridge    Bridge handle
 * @param precision Output: current precision estimate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_fep_get_precision(
    const ephaptic_fep_bridge_t* bridge,
    float* precision
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge handle
 * @return true if bridge is active
 */
NIMCP_EXPORT bool ephaptic_fep_is_active(const ephaptic_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_FEP_BRIDGE_H */