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
// nimcp_hh_fep_bridge.h - Hodgkin-Huxley to Free Energy Principle Bridge
//=============================================================================
/**
 * @file nimcp_hh_fep_bridge.h
 * @brief Bridge between HH biophysics and Free Energy Principle
 *
 * WHAT: Bidirectional integration between Hodgkin-Huxley neuron dynamics and
 *       the Free Energy Principle, enabling biophysically-grounded predictive
 *       processing and active inference.
 *
 * WHY:  The Free Energy Principle provides a unifying framework for neural
 *       computation, but requires biophysical grounding. HH neurons provide:
 *       - Precision-weighted prediction through ion channel states
 *       - Action potential generation as active inference
 *       - Temperature-dependent dynamics for metabolic constraints
 *       - Conductance variability as uncertainty encoding
 *
 * HOW:  - Maps HH membrane dynamics to FEP prediction errors
 *       - Ion channel states encode precision (confidence)
 *       - Temperature/ATP modulate expected free energy
 *       - Spike generation implements active inference actions
 *       - Population dynamics map to hierarchical predictions
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * HH TO FEP MAPPING:
 * ------------------
 * 1. Membrane Voltage as Prediction:
 *    - V_rest: Prior expectation (baseline prediction)
 *    - V deviation: Prediction error magnitude
 *    - Threshold crossing: Prediction error exceeds bound
 *
 * 2. Ion Channels as Precision:
 *    - g_Na availability: Precision of excitatory signals
 *    - g_K state: Precision of inhibitory signals
 *    - Channel noise: Uncertainty in sensory precision
 *    - Modulation factors: Top-down precision control
 *
 * 3. Temperature as Metabolic Constraint:
 *    - Q10 effects: Temperature modulates prediction speed
 *    - Higher temp: Faster, more precise predictions
 *    - Lower temp: Slower, less certain predictions
 *    - ATP availability: Energy for active inference
 *
 * 4. Spike Generation as Active Inference:
 *    - Action potentials: Actions to minimize free energy
 *    - Spike timing: Temporal prediction implementation
 *    - Firing rate: Accumulated prediction error
 *    - Burst patterns: Salience-driven predictions
 *
 * FEP TO HH EFFECTS:
 * ------------------
 * 1. Prediction Error Modulates Excitability:
 *    - High PE: Reduce threshold (increase responsiveness)
 *    - Low PE: Raise threshold (suppress unnecessary spiking)
 *
 * 2. Precision Weights Ion Channels:
 *    - High precision: Enhance Na+ (amplify signals)
 *    - Low precision: Reduce Na+ (attenuate uncertain signals)
 *
 * 3. Expected Free Energy Guides Behavior:
 *    - EFE drives neuromodulation of HH parameters
 *    - Curiosity (epistemic value) enhances exploration
 *    - Risk (pragmatic value) modulates caution
 *
 * FREE ENERGY DECOMPOSITION:
 * --------------------------
 * F = D_KL(q||p) - ln p(y|m)
 * F = Complexity - Accuracy
 *
 * HH Mapping:
 * - Accuracy: Match between predicted and actual voltage
 * - Complexity: Energy cost of maintaining channel states
 * - Surprise: Unexpected threshold crossings
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_HH_FEP_BRIDGE_H
#define NIMCP_HH_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define HH_FEP_MODULE_NAME              "hh_fep_bridge"

/** Maximum tracked HH neurons */
#define HH_FEP_MAX_NEURONS              1024

/** Default prediction error scaling */
#define HH_FEP_PE_SCALE                 1.0f

/** Precision base value */
#define HH_FEP_PRECISION_BASE           1.0f

/** Maximum precision value */
#define HH_FEP_PRECISION_MAX            10.0f

/** Minimum precision value */
#define HH_FEP_PRECISION_MIN            0.1f

/** Free energy reference (baseline) */
#define HH_FEP_FE_BASELINE              0.0f

/** Q10 for precision scaling */
#define HH_FEP_Q10_PRECISION            2.0f

/** Reference temperature for FEP (Celsius) */
#define HH_FEP_TEMP_REFERENCE           37.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Free energy computation mode
 */
typedef enum {
    HH_FEP_MODE_VARIATIONAL = 0,  /**< Variational free energy (recognition) */
    HH_FEP_MODE_EXPECTED,         /**< Expected free energy (planning) */
    HH_FEP_MODE_THERMODYNAMIC     /**< Thermodynamic free energy (physical) */
} hh_fep_mode_t;

/**
 * @brief Precision source for FEP
 */
typedef enum {
    HH_FEP_PRECISION_NA = 0,      /**< Precision from Na+ channel state */
    HH_FEP_PRECISION_K,           /**< Precision from K+ channel state */
    HH_FEP_PRECISION_CA,          /**< Precision from Ca2+ dynamics */
    HH_FEP_PRECISION_COMBINED,    /**< Combined channel precision */
    HH_FEP_PRECISION_EXTERNAL     /**< Externally specified precision */
} hh_fep_precision_source_t;

/**
 * @brief Active inference action type
 */
typedef enum {
    HH_FEP_ACTION_SPIKE = 0,      /**< Generate spike (discrete action) */
    HH_FEP_ACTION_THRESHOLD,      /**< Modulate threshold (continuous) */
    HH_FEP_ACTION_CONDUCTANCE,    /**< Modulate conductance (continuous) */
    HH_FEP_ACTION_INHIBIT         /**< Suppress activity (hyperpolarize) */
} hh_fep_action_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief HH state for FEP computation
 */
typedef struct {
    uint32_t neuron_id;           /**< HH neuron ID */
    float membrane_voltage;       /**< Current voltage (mV) */
    float resting_voltage;        /**< Resting potential (prediction prior) */
    float voltage_deviation;      /**< |V - V_rest| (prediction error) */
    float g_na_fraction;          /**< Na+ available fraction [0,1] */
    float g_k_fraction;           /**< K+ activation fraction [0,1] */
    float ca_concentration;       /**< Ca2+ level (uM) */
    float temperature;            /**< Temperature (C) */
    float phi_factor;             /**< Q10 temperature factor */
    bool spiked;                  /**< Recent spike flag */
    float firing_rate;            /**< Instantaneous firing rate (Hz) */
} hh_fep_state_t;

/**
 * @brief Prediction error from HH dynamics
 */
typedef struct {
    float sensory_pe;             /**< Sensory prediction error (voltage) */
    float precision_weighted_pe;  /**< Precision-weighted PE */
    float temporal_pe;            /**< Temporal prediction error (timing) */
    float rate_pe;                /**< Firing rate prediction error */
    float total_pe;               /**< Combined prediction error */
    float pe_variance;            /**< Variance in PE estimate */
} hh_fep_prediction_error_t;

/**
 * @brief Precision estimate from HH channels
 */
typedef struct {
    float na_precision;           /**< Precision from Na+ state */
    float k_precision;            /**< Precision from K+ state */
    float ca_precision;           /**< Precision from Ca2+ dynamics */
    float combined_precision;     /**< Combined channel precision */
    float temp_scaling;           /**< Temperature scaling factor */
    float effective_precision;    /**< Final effective precision */
} hh_fep_precision_t;

/**
 * @brief Free energy decomposition
 */
typedef struct {
    float variational_fe;         /**< Variational free energy */
    float expected_fe;            /**< Expected free energy */
    float accuracy;               /**< -ln p(y|x) accuracy term */
    float complexity;             /**< D_KL(q||p) complexity term */
    float surprise;               /**< -ln p(y) surprise term */
    float epistemic_value;        /**< Information gain (curiosity) */
    float pragmatic_value;        /**< Expected utility */
} hh_fep_free_energy_t;

/**
 * @brief FEP effects on HH parameters
 */
typedef struct {
    float threshold_modulation;   /**< Spike threshold shift (mV) */
    float g_na_modulation;        /**< Na+ conductance scaling */
    float g_k_modulation;         /**< K+ conductance scaling */
    float current_injection;      /**< Predicted current (uA/cm^2) */
    hh_fep_action_t recommended_action; /**< Suggested action */
    float action_probability;     /**< Softmax action probability */
} hh_fep_effects_t;

/**
 * @brief Active inference policy
 */
typedef struct {
    hh_fep_action_t action;       /**< Selected action */
    float expected_fe_reduction;  /**< Expected FE reduction */
    float action_cost;            /**< Metabolic cost of action */
    float net_utility;            /**< Net utility of action */
    float confidence;             /**< Action selection confidence */
} hh_fep_policy_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* FEP computation mode */
    hh_fep_mode_t mode;           /**< Free energy computation mode */

    /* Prediction error parameters */
    float pe_scale;               /**< Prediction error scaling */
    float pe_threshold;           /**< PE threshold for significance */
    float voltage_prior_mean;     /**< Prior mean voltage (mV) */
    float voltage_prior_variance; /**< Prior voltage variance */

    /* Precision parameters */
    hh_fep_precision_source_t precision_source; /**< Precision source */
    float precision_base;         /**< Base precision value */
    float precision_min;          /**< Minimum precision */
    float precision_max;          /**< Maximum precision */
    bool enable_temp_scaling;     /**< Temperature scales precision */

    /* Active inference parameters */
    bool enable_active_inference; /**< Enable action selection */
    float action_temperature;     /**< Softmax temperature for actions */
    float exploration_bonus;      /**< Epistemic value weighting */

    /* Feedback to HH */
    bool enable_hh_feedback;      /**< Enable FEP modulation of HH */
    float feedback_strength;      /**< Feedback scaling [0, 1] */
    float max_threshold_shift;    /**< Maximum threshold change (mV) */
    float max_conductance_mod;    /**< Maximum conductance scaling */

    /* Update parameters */
    float update_interval_ms;     /**< Bridge update interval */
    float belief_decay_tau_ms;    /**< Belief decay time constant */
} hh_fep_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Free energy tracking */
    uint64_t fe_computations;          /**< Total FE computations */
    float avg_variational_fe;          /**< Average variational FE */
    float avg_expected_fe;             /**< Average expected FE */
    float min_fe_achieved;             /**< Minimum FE achieved */
    float max_fe_observed;             /**< Maximum FE observed */

    /* Prediction error statistics */
    float avg_prediction_error;        /**< Average PE magnitude */
    float avg_precision_weighted_pe;   /**< Average precision-weighted PE */
    float pe_variance;                 /**< PE variance */

    /* Precision statistics */
    float avg_effective_precision;     /**< Average effective precision */
    float precision_range;             /**< Range of precision values */

    /* Active inference */
    uint64_t actions_selected;         /**< Actions selected */
    uint64_t spikes_generated;         /**< Spikes as active inference */
    uint64_t threshold_modulations;    /**< Threshold adjustments */
    uint64_t conductance_modulations;  /**< Conductance adjustments */
    float avg_action_utility;          /**< Average action utility */

    /* Performance */
    float last_update_ms;              /**< Last update timestamp */
    float processing_latency_us;       /**< Processing latency */
} hh_fep_stats_t;

/** Opaque bridge handle */
typedef struct hh_fep_bridge_struct hh_fep_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Initialize configuration with balanced defaults
 * WHY:  Easy creation with biologically-motivated parameters
 * HOW:  Set variational mode, combined precision, moderate feedback
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_default_config(hh_fep_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create HH-FEP bridge
 *
 * WHAT: Initialize bridge for HH-FEP integration
 * WHY:  Enable biophysically-grounded predictive processing
 * HOW:  Allocate state tracking, initialize priors
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT hh_fep_bridge_t* hh_fep_bridge_create(
    const hh_fep_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void hh_fep_bridge_destroy(hh_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_bridge_reset(hh_fep_bridge_t* bridge);

//=============================================================================
// HH to FEP API (Forward Direction)
//=============================================================================

/**
 * @brief Compute prediction error from HH state
 *
 * WHAT: Map HH voltage dynamics to FEP prediction error
 * WHY:  Voltage deviation from rest represents prediction failure
 * HOW:  Compare V to prior, weight by channel-derived precision
 *
 * @param bridge Bridge handle
 * @param state Current HH state
 * @param pe_out Output prediction error
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_compute_prediction_error(
    hh_fep_bridge_t* bridge,
    const hh_fep_state_t* state,
    hh_fep_prediction_error_t* pe_out
);

/**
 * @brief Compute precision from HH channel states
 *
 * WHAT: Derive FEP precision from ion channel availability
 * WHY:  Channel states encode confidence in neural signals
 * HOW:  Map g_Na, g_K, Ca to precision with temp scaling
 *
 * @param bridge Bridge handle
 * @param state Current HH state
 * @param precision_out Output precision estimate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_compute_precision(
    hh_fep_bridge_t* bridge,
    const hh_fep_state_t* state,
    hh_fep_precision_t* precision_out
);

/**
 * @brief Compute free energy from HH dynamics
 *
 * WHAT: Calculate variational/expected free energy
 * WHY:  Free energy quantifies model-world discrepancy
 * HOW:  Combine accuracy, complexity terms from HH state
 *
 * @param bridge Bridge handle
 * @param state Current HH state
 * @param fe_out Output free energy decomposition
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_compute_free_energy(
    hh_fep_bridge_t* bridge,
    const hh_fep_state_t* state,
    hh_fep_free_energy_t* fe_out
);

/**
 * @brief Process population for free energy
 *
 * WHAT: Compute population-level free energy
 * WHY:  Hierarchical predictions across neurons
 * HOW:  Aggregate individual FE, compute population statistics
 *
 * @param bridge Bridge handle
 * @param states Array of HH states
 * @param num_neurons Number of neurons
 * @param population_fe_out Output population free energy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_process_population(
    hh_fep_bridge_t* bridge,
    const hh_fep_state_t* states,
    uint32_t num_neurons,
    hh_fep_free_energy_t* population_fe_out
);

//=============================================================================
// FEP to HH API (Feedback Direction)
//=============================================================================

/**
 * @brief Compute FEP effects on HH parameters
 *
 * WHAT: Generate HH modulation from FEP state
 * WHY:  Active inference requires modifying biophysics
 * HOW:  Map free energy to threshold/conductance changes
 *
 * @param bridge Bridge handle
 * @param fe Current free energy state
 * @param effects_out Output HH modulation effects
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_compute_effects(
    hh_fep_bridge_t* bridge,
    const hh_fep_free_energy_t* fe,
    hh_fep_effects_t* effects_out
);

/**
 * @brief Select active inference action
 *
 * WHAT: Choose action to minimize expected free energy
 * WHY:  Implement active inference through HH
 * HOW:  Evaluate actions via softmax policy
 *
 * @param bridge Bridge handle
 * @param state Current HH state
 * @param fe Current free energy
 * @param policy_out Output selected policy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_select_action(
    hh_fep_bridge_t* bridge,
    const hh_fep_state_t* state,
    const hh_fep_free_energy_t* fe,
    hh_fep_policy_t* policy_out
);

/**
 * @brief Apply FEP-derived modulation to HH
 *
 * WHAT: Modify HH parameters based on FEP computation
 * WHY:  Close the active inference loop
 * HOW:  Apply threshold/conductance changes
 *
 * @param bridge Bridge handle
 * @param neuron_id Target neuron
 * @param effects Effects to apply
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_apply_effects(
    hh_fep_bridge_t* bridge,
    uint32_t neuron_id,
    const hh_fep_effects_t* effects
);

//=============================================================================
// Belief Update API
//=============================================================================

/**
 * @brief Update FEP beliefs based on HH observations
 *
 * WHAT: Bayesian belief update from new HH data
 * WHY:  Continuous learning of generative model
 * HOW:  Variational inference update step
 *
 * @param bridge Bridge handle
 * @param state Observed HH state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_update_beliefs(
    hh_fep_bridge_t* bridge,
    const hh_fep_state_t* state
);

/**
 * @brief Get current belief about voltage
 *
 * WHAT: Query expected voltage from generative model
 * WHY:  Compare predictions to observations
 * HOW:  Return prior mean and variance
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to query
 * @param mean_out Output predicted voltage mean
 * @param variance_out Output predicted variance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_get_voltage_belief(
    const hh_fep_bridge_t* bridge,
    uint32_t neuron_id,
    float* mean_out,
    float* variance_out
);

/**
 * @brief Set voltage prior
 *
 * WHAT: Update voltage prior for FEP computation
 * WHY:  Adapt predictions based on context
 * HOW:  Store new prior mean and variance
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to update
 * @param prior_mean New prior mean (mV)
 * @param prior_variance New prior variance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_set_voltage_prior(
    hh_fep_bridge_t* bridge,
    uint32_t neuron_id,
    float prior_mean,
    float prior_variance
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic bridge housekeeping
 * WHY:  Decay beliefs, update statistics
 * HOW:  Time-based state transitions
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_bridge_update(
    hh_fep_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_get_stats(
    const hh_fep_bridge_t* bridge,
    hh_fep_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_fep_reset_stats(hh_fep_bridge_t* bridge);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle (NULL safe)
 */
NIMCP_EXPORT void hh_fep_print_summary(const hh_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_FEP_BRIDGE_H */