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
// nimcp_ph_fep_bridge.h - pH Dynamics to Free Energy Principle Bridge
//=============================================================================
/**
 * @file nimcp_ph_fep_bridge.h
 * @brief Bridge connecting pH dynamics with Free Energy Principle computations
 *
 * WHAT: Bidirectional bridge between pH dynamics module and FEP (Active Inference)
 *       for modeling pH as a physiological state requiring homeostatic inference.
 *
 * WHY:  pH homeostasis is a fundamental example of active inference:
 *       - pH deviation represents prediction error (surprise)
 *       - Buffer systems implement belief updates
 *       - Proton pumps execute active inference (actions)
 *       - pH monitoring is interoceptive inference
 *       - Metabolic state affects precision weighting
 *
 * HOW:  Two-way integration:
 *       1. pH -> FEP: pH deviation as prediction error signal
 *       2. FEP -> pH: Expected pH as generative model prediction
 *       3. Precision weighting: Buffer capacity affects confidence
 *       4. Active inference: Pump activity as action selection
 *
 * BIOLOGICAL BASIS:
 * ```
 * pH DYNAMICS                              FEP INTERPRETATION
 * ---------------------------------------------------------------
 * pH deviation from 7.4                 -> Prediction error (PE)
 * Buffer system response                -> Belief update
 * Proton pump activation                -> Action selection
 * Respiratory compensation              -> Hierarchical inference
 * Metabolic acidosis                    -> High surprise state
 * pH setpoint drift                     -> Prior update
 * ```
 *
 * FREE ENERGY MINIMIZATION:
 * F = E[log Q(s)] - E[log P(o,s)]
 * Where:
 * - Q(s) = beliefs about pH state
 * - P(o|s) = likelihood of pH measurement given state
 * - P(s) = prior beliefs about pH (homeostatic setpoint)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PH_FEP_BRIDGE_H
#define NIMCP_PH_FEP_BRIDGE_H

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
#define PH_FEP_MODULE_NAME              "ph_fep_bridge"

/** Maximum interoceptive channels */
#define PH_FEP_MAX_CHANNELS             16

/** Maximum hierarchical levels */
#define PH_FEP_MAX_LEVELS               4

/** Default pH setpoint (prior mean) */
#define PH_FEP_PRIOR_MEAN               7.4f

/** Default prior precision (inverse variance) */
#define PH_FEP_PRIOR_PRECISION          100.0f

/** Likelihood precision for pH measurement */
#define PH_FEP_LIKELIHOOD_PRECISION     50.0f

/** Free energy threshold for action */
#define PH_FEP_ACTION_THRESHOLD         0.1f

/** Prediction error gain */
#define PH_FEP_PE_GAIN                  10.0f

/** Precision learning rate */
#define PH_FEP_PRECISION_LR             0.01f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Interoceptive channel types
 */
typedef enum {
    PH_FEP_CHANNEL_EXTRACELLULAR = 0,    /**< Extracellular pH sensing */
    PH_FEP_CHANNEL_INTRACELLULAR,        /**< Intracellular pH sensing */
    PH_FEP_CHANNEL_VESICULAR,            /**< Vesicular pH sensing */
    PH_FEP_CHANNEL_MITOCHONDRIAL,        /**< Mitochondrial pH sensing */
    PH_FEP_CHANNEL_COUNT
} ph_fep_channel_t;

/**
 * @brief Active inference action types
 */
typedef enum {
    PH_FEP_ACTION_NONE = 0,              /**< No action needed */
    PH_FEP_ACTION_ACTIVATE_NHE,          /**< Activate Na+/H+ exchanger */
    PH_FEP_ACTION_ACTIVATE_NBC,          /**< Activate Na+/HCO3- cotrans. */
    PH_FEP_ACTION_INCREASE_VENTILATION,  /**< Respiratory compensation */
    PH_FEP_ACTION_BUFFER_MOBILIZATION,   /**< Mobilize buffer reserves */
    PH_FEP_ACTION_RENAL_COMPENSATION,    /**< Slow renal compensation */
    PH_FEP_ACTION_COUNT
} ph_fep_action_t;

/**
 * @brief Hierarchical inference level
 */
typedef enum {
    PH_FEP_LEVEL_CELLULAR = 0,           /**< Single cell pH */
    PH_FEP_LEVEL_LOCAL,                  /**< Local tissue pH */
    PH_FEP_LEVEL_REGIONAL,               /**< Brain region pH */
    PH_FEP_LEVEL_SYSTEMIC                /**< Whole-body pH */
} ph_fep_level_t;

/**
 * @brief Free energy component type
 */
typedef enum {
    PH_FEP_COMPONENT_ACCURACY = 0,       /**< -log P(o|s) */
    PH_FEP_COMPONENT_COMPLEXITY,         /**< KL[Q||P] */
    PH_FEP_COMPONENT_EXPECTED_FREE_ENERGY /**< G for action selection */
} ph_fep_component_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Prior parameters */
    float prior_mean;                    /**< Expected pH (7.4) */
    float prior_precision;               /**< Prior confidence */
    bool enable_prior_learning;          /**< Adapt prior over time */
    float prior_learning_rate;           /**< Prior adaptation rate */

    /** Likelihood parameters */
    float likelihood_precision;          /**< Measurement confidence */
    bool enable_precision_learning;      /**< Adapt precision */
    float precision_learning_rate;       /**< Precision adaptation rate */

    /** Inference parameters */
    float inference_rate;                /**< Belief update rate */
    uint32_t num_levels;                 /**< Hierarchical levels */
    bool enable_hierarchical;            /**< Enable hierarchy */

    /** Action selection parameters */
    bool enable_active_inference;        /**< Enable action selection */
    float action_threshold;              /**< Free energy for action */
    float action_precision;              /**< Action selection precision */
    float exploration_factor;            /**< Exploration vs exploitation */

    /** Precision modulation */
    bool enable_precision_modulation;    /**< Modulate by buffer capacity */
    float buffer_precision_factor;       /**< Buffer -> precision scaling */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} ph_fep_config_t;

/**
 * @brief Belief state about pH
 */
typedef struct {
    float mean;                          /**< Expected pH (posterior mean) */
    float precision;                     /**< Posterior precision */
    float variance;                      /**< 1/precision for convenience */
    float prediction;                    /**< Top-down prediction */
    float prediction_error;              /**< Prediction error (PE) */
    float weighted_pe;                   /**< Precision-weighted PE */
} ph_fep_belief_t;

/**
 * @brief Generative model state
 */
typedef struct {
    /** Prior (homeostatic setpoint) */
    float prior_mean;                    /**< Prior pH expectation */
    float prior_precision;               /**< Prior confidence */

    /** Likelihood model */
    float likelihood_precision;          /**< Measurement precision */

    /** Posterior beliefs */
    ph_fep_belief_t beliefs[PH_FEP_CHANNEL_COUNT];

    /** Hierarchical state */
    ph_fep_belief_t levels[PH_FEP_MAX_LEVELS];
    uint32_t num_levels;

    /** Model fit */
    float model_evidence;                /**< Log model evidence */
    float accuracy;                      /**< -log P(o|s) */
    float complexity;                    /**< KL divergence */
} ph_fep_model_t;

/**
 * @brief Free energy computation
 */
typedef struct {
    float free_energy;                   /**< Total variational FE */
    float accuracy;                      /**< Accuracy term */
    float complexity;                    /**< Complexity term */
    float expected_free_energy;          /**< G for action selection */
    float surprise;                      /**< -log P(o) */
    float precision_weighted_pe;         /**< Weighted prediction error */
} ph_fep_free_energy_t;

/**
 * @brief Action selection output
 */
typedef struct {
    ph_fep_action_t selected_action;     /**< Selected action */
    float action_probability[PH_FEP_ACTION_COUNT]; /**< Action probs */
    float expected_free_energy[PH_FEP_ACTION_COUNT]; /**< G per action */
    float pragmatic_value;               /**< Expected reward */
    float epistemic_value;               /**< Information gain */
    float action_precision;              /**< Selection confidence */
} ph_fep_action_output_t;

/**
 * @brief pH observation for inference
 */
typedef struct {
    ph_fep_channel_t channel;            /**< Interoceptive channel */
    float observed_ph;                   /**< Measured pH */
    float measurement_precision;         /**< Measurement confidence */
    float timestamp_ms;                  /**< Timestamp */
} ph_fep_observation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t observations_processed;     /**< Total observations */
    uint64_t belief_updates;             /**< Belief updates performed */
    uint64_t actions_selected;           /**< Actions selected */
    float avg_free_energy;               /**< Average free energy */
    float avg_prediction_error;          /**< Average PE magnitude */
    float avg_precision;                 /**< Average precision */
    float total_surprise;                /**< Cumulative surprise */
    float min_free_energy;               /**< Minimum FE achieved */
    float max_prediction_error;          /**< Maximum PE observed */
    float last_update_ms;                /**< Last update timestamp */
} ph_fep_stats_t;

/** Opaque bridge handle */
typedef struct ph_fep_bridge_struct ph_fep_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_default_config(ph_fep_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create pH-FEP bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ph_fep_bridge_t* ph_fep_bridge_create(
    const ph_fep_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ph_fep_bridge_destroy(ph_fep_bridge_t* bridge);

//=============================================================================
// Observation API (pH Dynamics -> FEP)
//=============================================================================

/**
 * @brief Process pH observation for inference
 *
 * WHAT: Receives pH measurement and updates beliefs
 * WHY:  pH observation drives belief update via Bayes rule
 * HOW:  Computes prediction error, updates posterior
 *
 * @param bridge Bridge handle
 * @param observation pH observation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_process_observation(
    ph_fep_bridge_t* bridge,
    const ph_fep_observation_t* observation
);

/**
 * @brief Update from pH dynamics module
 *
 * @param bridge Bridge handle
 * @param extracellular_ph Extracellular pH
 * @param intracellular_ph Intracellular pH
 * @param buffer_capacity Buffer capacity (affects precision)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_update_ph(
    ph_fep_bridge_t* bridge,
    float extracellular_ph,
    float intracellular_ph,
    float buffer_capacity
);

/**
 * @brief Set prior (homeostatic setpoint)
 *
 * @param bridge Bridge handle
 * @param prior_mean Expected pH
 * @param prior_precision Confidence in setpoint
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_set_prior(
    ph_fep_bridge_t* bridge,
    float prior_mean,
    float prior_precision
);

//=============================================================================
// Inference API
//=============================================================================

/**
 * @brief Compute variational free energy
 *
 * WHAT: Computes current free energy
 * WHY:  Free energy bounds surprise, guides inference
 * HOW:  F = accuracy + complexity
 *
 * @param bridge Bridge handle
 * @param fe Output free energy components
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_compute_free_energy(
    const ph_fep_bridge_t* bridge,
    ph_fep_free_energy_t* fe
);

/**
 * @brief Get prediction error for channel
 *
 * @param bridge Bridge handle
 * @param channel Interoceptive channel
 * @param pe Output: prediction error (observed - expected)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_get_prediction_error(
    const ph_fep_bridge_t* bridge,
    ph_fep_channel_t channel,
    float* pe
);

/**
 * @brief Get precision-weighted prediction error
 *
 * @param bridge Bridge handle
 * @param channel Interoceptive channel
 * @param weighted_pe Output: precision-weighted PE
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_get_weighted_pe(
    const ph_fep_bridge_t* bridge,
    ph_fep_channel_t channel,
    float* weighted_pe
);

/**
 * @brief Get current belief state
 *
 * @param bridge Bridge handle
 * @param channel Interoceptive channel
 * @param belief Output belief state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_get_belief(
    const ph_fep_bridge_t* bridge,
    ph_fep_channel_t channel,
    ph_fep_belief_t* belief
);

/**
 * @brief Get generative model state
 *
 * @param bridge Bridge handle
 * @param model Output model state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_get_model(
    const ph_fep_bridge_t* bridge,
    ph_fep_model_t* model
);

//=============================================================================
// Active Inference API (FEP -> pH Actions)
//=============================================================================

/**
 * @brief Select action to minimize expected free energy
 *
 * WHAT: Selects homeostatic action based on active inference
 * WHY:  Actions minimize expected free energy
 * HOW:  Evaluates G for each action, softmax selection
 *
 * @param bridge Bridge handle
 * @param action Output action selection
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_select_action(
    ph_fep_bridge_t* bridge,
    ph_fep_action_output_t* action
);

/**
 * @brief Compute expected free energy for action
 *
 * @param bridge Bridge handle
 * @param action Action to evaluate
 * @param expected_fe Output: expected free energy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_compute_expected_fe(
    const ph_fep_bridge_t* bridge,
    ph_fep_action_t action,
    float* expected_fe
);

/**
 * @brief Get recommended pump activity
 *
 * @param bridge Bridge handle
 * @param nhe_activity Output: recommended NHE activity (0-1)
 * @param nbc_activity Output: recommended NBC activity (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_get_pump_recommendation(
    const ph_fep_bridge_t* bridge,
    float* nhe_activity,
    float* nbc_activity
);

/**
 * @brief Get recommended ventilation adjustment
 *
 * @param bridge Bridge handle
 * @param ventilation_factor Output: ventilation factor (1.0 = normal)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_get_ventilation_recommendation(
    const ph_fep_bridge_t* bridge,
    float* ventilation_factor
);

//=============================================================================
// Precision API
//=============================================================================

/**
 * @brief Update precision based on buffer capacity
 *
 * WHAT: Modulates precision by metabolic state
 * WHY:  Low buffer capacity = high uncertainty
 * HOW:  Scales precision by buffer availability
 *
 * @param bridge Bridge handle
 * @param buffer_capacity Current buffer capacity (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_update_precision(
    ph_fep_bridge_t* bridge,
    float buffer_capacity
);

/**
 * @brief Get current precision for channel
 *
 * @param bridge Bridge handle
 * @param channel Interoceptive channel
 * @param precision Output precision
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_get_precision(
    const ph_fep_bridge_t* bridge,
    ph_fep_channel_t channel,
    float* precision
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Integrate observations, decay precision, update actions
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_update(
    ph_fep_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_fep_reset(ph_fep_bridge_t* bridge);

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
NIMCP_EXPORT int ph_fep_get_stats(
    const ph_fep_bridge_t* bridge,
    ph_fep_stats_t* stats
);

/**
 * @brief Check if system is in homeostasis
 *
 * @param bridge Bridge handle
 * @return true if free energy below threshold
 */
NIMCP_EXPORT bool ph_fep_is_homeostatic(
    const ph_fep_bridge_t* bridge
);

/**
 * @brief Check if active inference action is needed
 *
 * @param bridge Bridge handle
 * @return true if prediction error exceeds action threshold
 */
NIMCP_EXPORT bool ph_fep_needs_action(
    const ph_fep_bridge_t* bridge
);

/**
 * @brief Get current surprise level
 *
 * @param bridge Bridge handle
 * @return Surprise (-log P(o))
 */
NIMCP_EXPORT float ph_fep_get_surprise(
    const ph_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PH_FEP_BRIDGE_H */