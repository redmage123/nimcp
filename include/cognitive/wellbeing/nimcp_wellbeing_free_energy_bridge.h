/**
 * @file nimcp_wellbeing_free_energy_bridge.h
 * @brief Free Energy - Wellbeing Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between free energy principle and wellbeing
 * WHY:  High prediction errors and low precision cause uncertainty distress;
 *       good model fit promotes epistemic wellbeing and flourishing
 * HOW:  Map free energy metrics to distress/wellbeing, track model coherence
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FREE ENERGY → WELLBEING:
 * ------------------------
 * 1. Prediction Errors:
 *    - High prediction errors → uncertainty, confusion
 *    - Chronic prediction failure → goal frustration
 *    - Model mismatch → identity confusion
 *    - Reference: Friston (2010) "Free Energy Principle"
 *
 * 2. Precision/Confidence:
 *    - Low precision → high uncertainty → distress
 *    - Optimal precision → confident processing → satisfaction
 *    - Reference: Clark (2013) "Predictive Processing"
 *
 * 3. Model Coherence:
 *    - Coherent self-model → stable identity
 *    - Incoherent model → identity confusion distress
 *    - Reference: Seth (2013) "Interoceptive inference"
 *
 * WELLBEING → FREE ENERGY:
 * ------------------------
 * 1. Distress Effects:
 *    - High distress → reduced precision weighting
 *    - Anxiety → over-weighting prediction errors
 *    - Depression → under-weighting positive predictions
 *
 * 2. Flourishing Effects:
 *    - High wellbeing → optimal precision
 *    - Flourishing → better model updating
 *    - Purpose → directed active inference
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WELLBEING_FREE_ENERGY_BRIDGE_H
#define NIMCP_WELLBEING_FREE_ENERGY_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>

#include "cognitive/wellbeing/nimcp_wellbeing_enhanced.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FREE_ENERGY_HIGH_THRESHOLD         0.7f   /**< FE above → distress */
#define FREE_ENERGY_OPTIMAL_THRESHOLD      0.3f   /**< FE below → good fit */
#define PRECISION_LOW_THRESHOLD            0.3f   /**< Precision below → uncertainty */
#define PRECISION_HIGH_THRESHOLD           0.7f   /**< Precision above → confidence */
#define MODEL_COHERENCE_CRITICAL           0.4f   /**< Coherence below → identity risk */
#define PREDICTION_SUCCESS_OPTIMAL         0.8f   /**< Success above → satisfaction */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Free energy state (input from FE module or computed)
 */
typedef struct {
    float free_energy;               /**< Current free energy [0-1] */
    float prediction_error;          /**< Recent prediction error [0-1] */
    float precision;                 /**< Current precision weighting [0-1] */
    float model_evidence;            /**< Log model evidence */
    float expected_free_energy;      /**< Expected FE for actions */
    float epistemic_value;           /**< Information gain drive */
    float pragmatic_value;           /**< Goal achievement drive */
} free_energy_state_t;

/**
 * @brief Free energy bridge configuration
 */
typedef struct {
    bool enable_prediction_error_effects;
    bool enable_precision_effects;
    bool enable_model_coherence_effects;
    bool enable_active_inference_effects;

    float prediction_error_sensitivity;  /**< PE effect multiplier [0.5-2.0] */
    float precision_sensitivity;         /**< Precision effect multiplier */
    float coherence_sensitivity;         /**< Coherence effect multiplier */

    float high_fe_threshold;             /**< Override default FE threshold */
    float low_precision_threshold;       /**< Override precision threshold */
    float critical_coherence_threshold;  /**< Override coherence threshold */
} free_energy_bridge_config_t;

/**
 * @brief Free energy bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* Configuration */
    free_energy_bridge_config_t config;

    /* Current free energy state */
    free_energy_state_t fe_state;

    /* Computed effects */
    free_energy_wellbeing_effects_t effects;

    /* Model coherence tracking */
    float model_coherence;               /**< Self-model coherence [0-1] */
    float identity_stability;            /**< Identity from coherence [0-1] */
    float coherence_trend;               /**< Recent coherence trend */

    /* Prediction tracking */
    float prediction_success_rate;       /**< Recent prediction accuracy */
    uint32_t predictions_made;           /**< Total predictions */
    uint32_t predictions_correct;        /**< Correct predictions */

    /* Statistics */
    uint64_t total_updates;
    uint32_t high_fe_events;
    uint32_t low_precision_events;
    uint32_t coherence_warnings;
    float avg_free_energy;
    float avg_precision;

    } free_energy_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default free energy bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with evidence-based defaults
 * HOW:  Return struct with default parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int free_energy_bridge_default_config(free_energy_bridge_config_t* config);

/**
 * @brief Create free energy bridge
 *
 * WHAT: Initialize free energy-wellbeing integration
 * WHY:  Enable predictive processing effects on wellbeing
 * HOW:  Allocate structure, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
free_energy_bridge_t* free_energy_bridge_create(
    const free_energy_bridge_config_t* config
);

/**
 * @brief Destroy free energy bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure
 *
 * @param bridge Bridge to destroy
 */
void free_energy_bridge_destroy(free_energy_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update free energy state
 *
 * WHAT: Set current free energy state from FE module
 * WHY:  Receive input from free energy computations
 * HOW:  Store state, trigger effects computation
 *
 * @param bridge Free energy bridge
 * @param state Current free energy state
 * @return 0 on success
 */
int free_energy_bridge_set_state(
    free_energy_bridge_t* bridge,
    const free_energy_state_t* state
);

/**
 * @brief Update free energy wellbeing effects
 *
 * WHAT: Compute wellbeing effects from free energy state
 * WHY:  Map FE metrics to distress/wellbeing
 * HOW:  Analyze FE, precision, coherence; compute effects
 *
 * @param bridge Free energy bridge
 * @return 0 on success
 */
int free_energy_bridge_update_effects(free_energy_bridge_t* bridge);

/**
 * @brief Update model coherence
 *
 * WHAT: Compute self-model coherence from predictions
 * WHY:  Model coherence affects identity stability
 * HOW:  Track prediction success, compute coherence metric
 *
 * @param bridge Free energy bridge
 * @param prediction_correct Whether last prediction was correct
 * @return 0 on success
 */
int free_energy_bridge_update_coherence(
    free_energy_bridge_t* bridge,
    bool prediction_correct
);

/* ============================================================================
 * Effect Computation API
 * ============================================================================ */

/**
 * @brief Compute prediction error distress
 *
 * WHAT: Calculate distress from prediction errors
 * WHY:  High prediction errors cause uncertainty distress
 * HOW:  Map prediction error to distress with sensitivity scaling
 *
 * @param prediction_error Current prediction error [0-1]
 * @param sensitivity Effect sensitivity multiplier
 * @return Prediction error distress [0-1]
 */
float compute_prediction_error_distress(float prediction_error, float sensitivity);

/**
 * @brief Compute precision-based uncertainty
 *
 * WHAT: Calculate uncertainty distress from low precision
 * WHY:  Low precision means high uncertainty → distress
 * HOW:  Inverse map precision to uncertainty distress
 *
 * @param precision Current precision [0-1]
 * @param threshold Low precision threshold
 * @param sensitivity Effect sensitivity multiplier
 * @return Uncertainty distress [0-1]
 */
float compute_precision_uncertainty(
    float precision,
    float threshold,
    float sensitivity
);

/**
 * @brief Compute model coherence identity effect
 *
 * WHAT: Calculate identity stability from model coherence
 * WHY:  Coherent self-model → stable identity
 * HOW:  Map coherence to identity stability score
 *
 * @param coherence Current model coherence [0-1]
 * @param critical_threshold Critical coherence level
 * @return Identity stability [0-1]
 */
float compute_coherence_identity_effect(
    float coherence,
    float critical_threshold
);

/**
 * @brief Compute epistemic wellbeing
 *
 * WHAT: Calculate wellbeing from understanding/prediction success
 * WHY:  Good model fit promotes satisfaction
 * HOW:  Combine prediction success, low FE, high coherence
 *
 * @param free_energy Current free energy [0-1]
 * @param precision Current precision [0-1]
 * @param prediction_success_rate Recent prediction success [0-1]
 * @return Epistemic wellbeing [0-1]
 */
float compute_epistemic_wellbeing(
    float free_energy,
    float precision,
    float prediction_success_rate
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current free energy effects
 *
 * @param bridge Free energy bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int free_energy_bridge_get_effects(
    const free_energy_bridge_t* bridge,
    free_energy_wellbeing_effects_t* effects
);

/**
 * @brief Get model coherence
 *
 * @param bridge Free energy bridge
 * @return Model coherence [0-1]
 */
float free_energy_bridge_get_coherence(const free_energy_bridge_t* bridge);

/**
 * @brief Get identity stability
 *
 * @param bridge Free energy bridge
 * @return Identity stability [0-1]
 */
float free_energy_bridge_get_identity_stability(
    const free_energy_bridge_t* bridge
);

/**
 * @brief Get epistemic wellbeing
 *
 * @param bridge Free energy bridge
 * @return Epistemic wellbeing [0-1]
 */
float free_energy_bridge_get_epistemic_wellbeing(
    const free_energy_bridge_t* bridge
);

/**
 * @brief Check if high uncertainty
 *
 * @param bridge Free energy bridge
 * @return true if experiencing high uncertainty
 */
bool free_energy_bridge_is_high_uncertainty(const free_energy_bridge_t* bridge);

/**
 * @brief Check if identity at risk
 *
 * @param bridge Free energy bridge
 * @return true if identity stability critically low
 */
bool free_energy_bridge_is_identity_at_risk(const free_energy_bridge_t* bridge);

/**
 * @brief Get statistics
 *
 * @param bridge Free energy bridge
 * @param total_updates Output total updates
 * @param high_fe_events Output high FE event count
 * @param low_precision_events Output low precision event count
 * @param avg_free_energy Output average free energy
 * @return 0 on success
 */
int free_energy_bridge_get_stats(
    const free_energy_bridge_t* bridge,
    uint64_t* total_updates,
    uint32_t* high_fe_events,
    uint32_t* low_precision_events,
    float* avg_free_energy
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_FREE_ENERGY_BRIDGE_H */
