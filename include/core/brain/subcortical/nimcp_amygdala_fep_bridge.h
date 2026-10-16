/**
 * @file nimcp_amygdala_fep_bridge.h
 * @brief Amygdala-to-Free Energy Principle Integration Bridge
 *
 * WHAT: Bridges amygdala threat processing to Free Energy Principle framework
 * WHY:  Fear/anxiety as prediction error about safety; threat detection as precision-weighted inference
 * HOW:  Generative model of safety/threat, active inference for defensive responses
 *
 * BIOLOGICAL BASIS:
 * - Fear = prediction error when safety expectations are violated
 * - Threat detection = precision-weighted sensory evidence
 * - Fear conditioning = updating generative model (CS-US associations)
 * - Extinction = learning new predictive model that inhibits fear
 * - Anxiety = sustained prior expectation of threat (high precision on threat priors)
 * - Context = precision modulation based on familiarity
 *
 * FEP MAPPING:
 * - Generative model: Predicts whether stimulus/context is safe or threatening
 * - Prediction error: Difference between expected safety and actual threat
 * - Precision: Confidence in threat predictions (modulated by NE, stress)
 * - Active inference: Action selection to minimize expected harm
 *
 * References:
 * - Seth & Friston (2016). Active interoceptive inference and the emotional brain
 * - Barrett & Simmons (2015). Interoceptive predictions in the brain
 * - Paulus & Stein (2010). Interoception in anxiety and depression
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

#ifndef NIMCP_AMYGDALA_FEP_BRIDGE_H
#define NIMCP_AMYGDALA_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct amygdala;
typedef struct amygdala amygdala_t;

//=============================================================================
// Constants
//=============================================================================

#define AMYG_FEP_MAX_STATE_DIM 16       /**< Max state dimension */
#define AMYG_FEP_MAX_OBS_DIM 32         /**< Max observation dimension */
#define AMYG_FEP_DEFAULT_PRECISION 1.0f /**< Default precision */
#define AMYG_FEP_NUM_THREAT_MODELS 5    /**< Number of threat models */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Generative model types for threat prediction
 */
typedef enum {
    AMYG_FEP_MODEL_SAFE = 0,         /**< Safe environment model */
    AMYG_FEP_MODEL_VIGILANT,         /**< Elevated vigilance model */
    AMYG_FEP_MODEL_THREAT,           /**< Active threat model */
    AMYG_FEP_MODEL_DANGER,           /**< Imminent danger model */
    AMYG_FEP_MODEL_PANIC             /**< Panic/overwhelm model */
} amyg_fep_model_t;

/**
 * @brief Active inference action types for defensive behavior
 */
typedef enum {
    AMYG_FEP_ACTION_OBSERVE = 0,     /**< Continue observation */
    AMYG_FEP_ACTION_ORIENT,          /**< Orient toward stimulus */
    AMYG_FEP_ACTION_FREEZE,          /**< Freeze response */
    AMYG_FEP_ACTION_AVOID,           /**< Avoidance/escape */
    AMYG_FEP_ACTION_APPROACH         /**< Cautious approach (curiosity) */
} amyg_fep_action_t;

/**
 * @brief Precision weighting mode
 */
typedef enum {
    AMYG_FEP_PRECISION_FIXED = 0,    /**< Fixed precision weights */
    AMYG_FEP_PRECISION_ADAPTIVE,     /**< Adaptive based on arousal/stress */
    AMYG_FEP_PRECISION_INTEROCEPTIVE /**< Include interoceptive precision */
} amyg_fep_precision_mode_t;

/**
 * @brief Belief state about safety
 */
typedef enum {
    AMYG_FEP_BELIEF_SAFE = 0,        /**< Believe environment is safe */
    AMYG_FEP_BELIEF_UNCERTAIN,       /**< Uncertain about safety */
    AMYG_FEP_BELIEF_UNSAFE           /**< Believe environment is unsafe */
} amyg_fep_safety_belief_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Prediction error components
 */
typedef struct {
    float safety_error;              /**< Safety prediction error (threat when expected safe) */
    float threat_error;              /**< Threat prediction error (safe when expected threat) */
    float interoceptive_error;       /**< Body state prediction error */
    float contextual_error;          /**< Context mismatch error */
    float total_free_energy;         /**< Total variational free energy */
    float precision_weighted_error;  /**< Precision-weighted prediction error */
} amyg_fep_errors_t;

/**
 * @brief Active inference state for threat processing
 */
typedef struct {
    float beliefs[AMYG_FEP_MAX_STATE_DIM];    /**< Current beliefs about threat state */
    float precision[AMYG_FEP_MAX_STATE_DIM];  /**< Precision of beliefs */
    uint32_t state_dim;                        /**< State dimension */
    float expected_free_energy;                /**< Expected FE for current action */
    amyg_fep_action_t current_action;          /**< Current defensive action */
    amyg_fep_safety_belief_t safety_belief;    /**< Current safety belief */
} amyg_fep_inference_t;

/**
 * @brief Interoceptive state (body signals)
 */
typedef struct {
    float heart_rate_deviation;      /**< HR deviation from baseline [-1, 1] */
    float respiratory_rate;          /**< Respiratory rate deviation */
    float skin_conductance;          /**< Electrodermal activity */
    float muscle_tension;            /**< Overall muscle tension */
    float gut_feeling;               /**< Visceral interoception */
} amyg_fep_interoception_t;

/**
 * @brief Configuration for amygdala FEP bridge
 */
typedef struct {
    /* Model settings */
    amyg_fep_model_t default_model;         /**< Default threat model */
    bool auto_model_selection;               /**< Auto-select best model */
    float model_evidence_threshold;          /**< Threshold for model switching */

    /* Precision settings */
    amyg_fep_precision_mode_t precision_mode; /**< Precision weighting mode */
    float sensory_precision;                 /**< External sensory precision */
    float interoceptive_precision;           /**< Body signal precision */
    float contextual_precision;              /**< Context precision */
    float prior_precision;                   /**< Prior belief precision */

    /* Arousal/stress modulation */
    float arousal_precision_gain;            /**< How arousal affects precision */
    float stress_precision_gain;             /**< How stress affects precision */
    float anxiety_prior_boost;               /**< Anxiety increases threat prior */

    /* Inference settings */
    float learning_rate;                     /**< Belief update rate */
    uint32_t inference_steps;                /**< Steps per update cycle */
    float action_precision;                  /**< Precision of action selection */

    /* Integration */
    bool use_interoception;                  /**< Include body signals */
    bool use_context;                        /**< Use contextual modulation */
    float prediction_horizon_ms;             /**< How far ahead to predict */
} amyg_fep_config_t;

/**
 * @brief Statistics for amygdala FEP bridge
 */
typedef struct {
    uint64_t inference_steps_total;
    uint64_t predictions_made;
    uint64_t model_switches;
    uint64_t threat_detections;
    uint64_t false_alarms;
    float avg_prediction_error;
    float avg_free_energy;
    float avg_precision;
    float model_evidence[AMYG_FEP_NUM_THREAT_MODELS]; /**< Evidence per model */
} amyg_fep_stats_t;

/**
 * @brief FEP bridge handle
 */
typedef struct amyg_fep_bridge_s amyg_fep_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Initialize default configuration
 */
int amyg_fep_bridge_default_config(amyg_fep_config_t* config);

/**
 * @brief Validate configuration
 */
int amyg_fep_bridge_validate_config(const amyg_fep_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create amygdala FEP bridge
 *
 * @param amygdala Connected amygdala instance (can be NULL)
 * @param fep_system FEP orchestrator (can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
amyg_fep_bridge_t* amyg_fep_bridge_create(
    amygdala_t* amygdala,
    void* fep_system,
    const amyg_fep_config_t* config
);

/**
 * @brief Destroy bridge
 */
void amyg_fep_bridge_destroy(amyg_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int amyg_fep_bridge_reset(amyg_fep_bridge_t* bridge);

//=============================================================================
// Prediction Error
//=============================================================================

/**
 * @brief Compute prediction errors from sensory observations
 *
 * @param bridge Bridge instance
 * @param observations Sensory observations (threat-related features)
 * @param obs_dim Number of observations
 * @param errors Output prediction errors
 * @return 0 on success, -1 on error
 */
int amyg_fep_compute_errors(
    amyg_fep_bridge_t* bridge,
    const float* observations,
    uint32_t obs_dim,
    amyg_fep_errors_t* errors
);

/**
 * @brief Get current free energy
 */
float amyg_fep_get_free_energy(const amyg_fep_bridge_t* bridge);

/**
 * @brief Get current surprise (negative log probability)
 */
float amyg_fep_get_surprise(const amyg_fep_bridge_t* bridge);

/**
 * @brief Update precision based on arousal and stress
 *
 * @param bridge Bridge instance
 * @param arousal Current arousal level [0-1]
 * @param stress Current stress level [0-1]
 * @return 0 on success, -1 on error
 */
int amyg_fep_update_precision(
    amyg_fep_bridge_t* bridge,
    float arousal,
    float stress
);

/**
 * @brief Set interoceptive state for body-based inference
 */
int amyg_fep_set_interoception(
    amyg_fep_bridge_t* bridge,
    const amyg_fep_interoception_t* intero
);

//=============================================================================
// Active Inference
//=============================================================================

/**
 * @brief Infer threat state from observations
 *
 * @param bridge Bridge instance
 * @param observations Sensory observations
 * @param obs_dim Number of observations
 * @param inference Output inference state
 * @return 0 on success, -1 on error
 */
int amyg_fep_infer_state(
    amyg_fep_bridge_t* bridge,
    const float* observations,
    uint32_t obs_dim,
    amyg_fep_inference_t* inference
);

/**
 * @brief Select best defensive action via active inference
 *
 * @param bridge Bridge instance
 * @param action Output selected action
 * @param action_value Output action value (negative EFE)
 * @return 0 on success, -1 on error
 */
int amyg_fep_select_action(
    amyg_fep_bridge_t* bridge,
    amyg_fep_action_t* action,
    float* action_value
);

/**
 * @brief Apply selected action (updates internal state)
 */
int amyg_fep_apply_action(
    amyg_fep_bridge_t* bridge,
    amyg_fep_action_t action
);

/**
 * @brief Compute expected free energy for an action
 */
float amyg_fep_expected_free_energy(
    const amyg_fep_bridge_t* bridge,
    amyg_fep_action_t action
);

//=============================================================================
// Generative Model
//=============================================================================

/**
 * @brief Set current threat model
 */
int amyg_fep_set_model(
    amyg_fep_bridge_t* bridge,
    amyg_fep_model_t model
);

/**
 * @brief Get best fitting threat model
 */
amyg_fep_model_t amyg_fep_get_best_model(const amyg_fep_bridge_t* bridge);

/**
 * @brief Get model evidence (log likelihood)
 */
float amyg_fep_get_model_evidence(
    const amyg_fep_bridge_t* bridge,
    amyg_fep_model_t model
);

/**
 * @brief Predict future threat state
 *
 * @param bridge Bridge instance
 * @param horizon_ms Prediction horizon in ms
 * @param predicted_state Output predicted state
 * @param state_dim State dimension
 * @return Number of states predicted, -1 on error
 */
int amyg_fep_predict(
    amyg_fep_bridge_t* bridge,
    float horizon_ms,
    float* predicted_state,
    uint32_t state_dim
);

/**
 * @brief Update model after fear conditioning event
 *
 * @param bridge Bridge instance
 * @param cs_features Conditioned stimulus features
 * @param cs_dim Number of CS features
 * @param threat_intensity Threat intensity [0-1]
 * @return 0 on success, -1 on error
 */
int amyg_fep_condition(
    amyg_fep_bridge_t* bridge,
    const float* cs_features,
    uint32_t cs_dim,
    float threat_intensity
);

/**
 * @brief Update model after extinction event
 */
int amyg_fep_extinction(
    amyg_fep_bridge_t* bridge,
    const float* cs_features,
    uint32_t cs_dim
);

//=============================================================================
// Integration
//=============================================================================

/**
 * @brief Connect to amygdala instance
 */
int amyg_fep_connect_amygdala(
    amyg_fep_bridge_t* bridge,
    amygdala_t* amygdala
);

/**
 * @brief Connect to FEP orchestrator
 */
int amyg_fep_connect_system(
    amyg_fep_bridge_t* bridge,
    void* fep_system
);

/**
 * @brief Update bridge state (one timestep)
 */
int amyg_fep_update(amyg_fep_bridge_t* bridge, float dt_ms);

/**
 * @brief Synchronize with amygdala state
 */
int amyg_fep_sync_with_amygdala(amyg_fep_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get statistics
 */
int amyg_fep_bridge_get_stats(
    const amyg_fep_bridge_t* bridge,
    amyg_fep_stats_t* stats
);

/**
 * @brief Reset statistics
 */
int amyg_fep_bridge_reset_stats(amyg_fep_bridge_t* bridge);

//=============================================================================
// Utility
//=============================================================================

const char* amyg_fep_model_name(amyg_fep_model_t model);
const char* amyg_fep_action_name(amyg_fep_action_t action);
const char* amyg_fep_precision_mode_name(amyg_fep_precision_mode_t mode);
const char* amyg_fep_safety_belief_name(amyg_fep_safety_belief_t belief);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AMYGDALA_FEP_BRIDGE_H */
