/**
 * @file nimcp_fep_parietal_bridge.h
 * @brief Free Energy Principle integration for Parietal Lobe
 *
 * WHAT: Integrates FEP (predictive processing, active inference) with parietal lobe
 * WHY:  Mathematical/scientific reasoning as prediction error minimization
 * HOW:  FEP generative models for math patterns, physics, and spatial reasoning
 *
 * THEORETICAL BASIS:
 * =================
 * The parietal cortex implements predictive processing for:
 * - Number magnitude estimation (Weber-Fechner as precision-weighted prediction)
 * - Spatial transformations (generative models of 3D space)
 * - Pattern recognition (hierarchical priors over sequences)
 * - Scientific reasoning (hypothesis testing as belief updating)
 * - Physics intuition (generative models of physical dynamics)
 *
 * FEP provides a unifying framework where mathematical intuition emerges from:
 * - Minimizing prediction errors about mathematical relationships
 * - Active inference for exploring solution spaces
 * - Precision-weighted attention to relevant features
 * - Hierarchical beliefs from concrete to abstract
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP-PARIETAL BRIDGE                                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                 MATHEMATICAL GENERATIVE MODEL                        │ ║
 * ║   │                                                                      │ ║
 * ║   │   Level 3: Abstract Principles (symmetry, conservation laws)        │ ║
 * ║   │      ↓ predictions    ↑ prediction errors                           │ ║
 * ║   │   Level 2: Mathematical Structures (patterns, relations)            │ ║
 * ║   │      ↓ predictions    ↑ prediction errors                           │ ║
 * ║   │   Level 1: Numerical/Spatial Features                               │ ║
 * ║   │      ↓ predictions    ↑ prediction errors                           │ ║
 * ║   │   Level 0: Sensory Input (quantities, positions)                    │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                 ACTIVE MATHEMATICAL INFERENCE                        │ ║
 * ║   │                                                                      │ ║
 * ║   │   Problem State → Policy Evaluation → Action Selection               │ ║
 * ║   │      (explore)        (EFE)             (solve step)                │ ║
 * ║   │                                                                      │ ║
 * ║   │   Policies: {algebraic, geometric, numerical, analogical}           │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                 PRECISION MODULATION                                 │ ║
 * ║   │                                                                      │ ║
 * ║   │   Attention → Precision ↔ Confidence → Intuition                    │ ║
 * ║   │      ↑                                      ↓                        │ ║
 * ║   │   Surprise → Precision Update → Learning Rate                       │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 1.0.0
 */

#ifndef NIMCP_FEP_PARIETAL_BRIDGE_H
#define NIMCP_FEP_PARIETAL_BRIDGE_H

#include "utils/validation/nimcp_common.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Bio-async module ID */
#define BIO_MODULE_FEP_PARIETAL             0x03B0

/** Maximum mathematical hierarchy levels */
#define FEP_PARIETAL_MAX_LEVELS             6

/** Maximum problem-solving policies */
#define FEP_PARIETAL_MAX_POLICIES           16

/** Maximum belief dimension */
#define FEP_PARIETAL_MAX_BELIEF_DIM         128

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for FEP-Parietal bridge */
typedef struct fep_parietal_bridge fep_parietal_bridge_t;

/**
 * @brief Mathematical domain types for specialized generative models
 */
typedef enum {
    FEP_MATH_DOMAIN_NUMERICAL,          /**< Number magnitude, arithmetic */
    FEP_MATH_DOMAIN_SPATIAL,            /**< Geometry, transformations */
    FEP_MATH_DOMAIN_ALGEBRAIC,          /**< Symbolic, equations */
    FEP_MATH_DOMAIN_STATISTICAL,        /**< Probability, distributions */
    FEP_MATH_DOMAIN_PHYSICAL,           /**< Physics, dynamics */
    FEP_MATH_DOMAIN_LOGICAL,            /**< Logic, proofs */
    FEP_MATH_DOMAIN_ENGINEERING         /**< Applied engineering */
} fep_math_domain_t;

/**
 * @brief Problem-solving strategy (policy type)
 */
typedef enum {
    FEP_STRATEGY_ALGEBRAIC,             /**< Algebraic manipulation */
    FEP_STRATEGY_GEOMETRIC,             /**< Geometric/visual reasoning */
    FEP_STRATEGY_NUMERICAL,             /**< Numerical approximation */
    FEP_STRATEGY_ANALOGICAL,            /**< Analogy-based reasoning */
    FEP_STRATEGY_EXHAUSTIVE,            /**< Systematic search */
    FEP_STRATEGY_HEURISTIC,             /**< Rule-of-thumb */
    FEP_STRATEGY_INTUITIVE              /**< Pattern-based intuition */
} fep_problem_strategy_t;

/**
 * @brief Mathematical belief state
 */
typedef struct {
    float* mean;                        /**< Expected value */
    float* precision;                   /**< Confidence (inverse variance) */
    uint32_t dim;                       /**< Dimensionality */
    fep_math_domain_t domain;           /**< Mathematical domain */
    float confidence;                   /**< Overall confidence [0,1] */
    float surprise;                     /**< Current surprise level */
} fep_math_belief_t;

/**
 * @brief Mathematical prediction
 */
typedef struct {
    float* predicted;                   /**< Predicted values */
    float* actual;                      /**< Actual observed values */
    float* error;                       /**< Prediction error */
    float* weighted_error;              /**< Precision-weighted error */
    uint32_t dim;
    float error_magnitude;              /**< L2 norm of error */
    float free_energy;                  /**< Free energy contribution */
} fep_math_prediction_t;

/**
 * @brief Problem-solving policy
 */
typedef struct {
    fep_problem_strategy_t strategy;    /**< Strategy type */
    float expected_free_energy;         /**< G(π) for this policy */
    float epistemic_value;              /**< Information gain */
    float pragmatic_value;              /**< Goal achievement */
    float complexity_cost;              /**< Computational cost */
    float probability;                  /**< Policy probability */
    char description[128];              /**< Strategy description */
} fep_math_policy_t;

/**
 * @brief Problem state for active inference
 */
typedef struct {
    float* state_vector;                /**< Current problem state */
    uint32_t state_dim;
    fep_math_domain_t domain;
    float* goal_state;                  /**< Target solution state */
    uint32_t goal_dim;
    float distance_to_goal;             /**< Estimated distance */
    bool solved;                        /**< Solution found */
    float solution_confidence;          /**< Confidence in solution */
} fep_problem_state_t;

/**
 * @brief Active inference result
 */
typedef struct {
    fep_problem_strategy_t selected_strategy;
    float* action;                      /**< Next problem-solving step */
    uint32_t action_dim;
    float expected_improvement;         /**< Expected reduction in uncertainty */
    float exploration_bonus;            /**< Epistemic value */
    fep_math_policy_t* evaluated_policies;
    uint32_t num_policies;
} fep_active_inference_result_t;

/**
 * @brief Hierarchical generative model for mathematics
 */
typedef struct {
    /* Level 0: Sensory/numerical input */
    fep_math_belief_t sensory_beliefs;

    /* Level 1: Feature/pattern level */
    fep_math_belief_t feature_beliefs;

    /* Level 2: Structural/relational level */
    fep_math_belief_t structural_beliefs;

    /* Level 3: Abstract principles */
    fep_math_belief_t abstract_beliefs;

    /* Predictions between levels */
    fep_math_prediction_t level_predictions[FEP_PARIETAL_MAX_LEVELS - 1];

    /* Total free energy */
    float total_free_energy;
    float complexity;
    float inaccuracy;
} fep_math_generative_model_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enabled;

    /* Hierarchy settings */
    uint32_t num_levels;
    uint32_t level_dims[FEP_PARIETAL_MAX_LEVELS];

    /* Learning rates */
    float belief_learning_rate;
    float precision_learning_rate;
    float policy_learning_rate;

    /* Active inference settings */
    bool enable_active_inference;
    uint32_t planning_horizon;
    float exploration_weight;           /**< Epistemic vs pragmatic balance */
    float action_temperature;           /**< Softmax temperature */

    /* Precision settings */
    float initial_precision;
    float min_precision;
    float max_precision;
    bool adaptive_precision;

    /* Domain-specific settings */
    bool enable_numerical_model;
    bool enable_spatial_model;
    bool enable_algebraic_model;
    bool enable_physical_model;

    /* Modulation sensitivity */
    float inflammation_sensitivity;
    float fatigue_sensitivity;
} fep_parietal_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t belief_updates;
    uint64_t predictions_made;
    uint64_t active_inferences;
    uint64_t policies_evaluated;
    float avg_prediction_error;
    float avg_free_energy;
    float avg_precision;
    float total_surprise;
    uint32_t strategy_selections[7];    /**< Count per strategy type */
} fep_parietal_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
fep_parietal_config_t fep_parietal_default_config(void);

/**
 * @brief Create FEP-Parietal bridge
 */
fep_parietal_bridge_t* fep_parietal_bridge_create(
    const fep_parietal_config_t* config
);

/**
 * @brief Destroy bridge
 */
void fep_parietal_bridge_destroy(fep_parietal_bridge_t* bridge);

/**
 * @brief Enable/disable bridge
 */
int fep_parietal_set_enabled(fep_parietal_bridge_t* bridge, bool enabled);

/**
 * @brief Check if bridge is available
 */
bool fep_parietal_is_available(const fep_parietal_bridge_t* bridge);

/* ============================================================================
 * PREDICTIVE PROCESSING API
 * ============================================================================ */

/**
 * @brief Update beliefs from observations
 *
 * Performs hierarchical belief update using prediction error minimization.
 *
 * @param bridge Bridge handle
 * @param observations Input observations
 * @param num_observations Number of observations
 * @param domain Mathematical domain
 * @param beliefs Output updated beliefs
 * @return 0 on success
 */
int fep_parietal_update_beliefs(
    fep_parietal_bridge_t* bridge,
    const float* observations,
    uint32_t num_observations,
    fep_math_domain_t domain,
    fep_math_belief_t* beliefs
);

/**
 * @brief Generate prediction from beliefs
 *
 * Uses generative model to predict expected observations.
 *
 * @param bridge Bridge handle
 * @param beliefs Current beliefs
 * @param prediction Output prediction
 * @return 0 on success
 */
int fep_parietal_predict(
    fep_parietal_bridge_t* bridge,
    const fep_math_belief_t* beliefs,
    fep_math_prediction_t* prediction
);

/**
 * @brief Compute prediction error
 *
 * Computes precision-weighted prediction error.
 *
 * @param bridge Bridge handle
 * @param predicted Predicted values
 * @param actual Actual values
 * @param dim Dimensionality
 * @param error Output prediction error
 * @return 0 on success
 */
int fep_parietal_prediction_error(
    fep_parietal_bridge_t* bridge,
    const float* predicted,
    const float* actual,
    uint32_t dim,
    fep_math_prediction_t* error
);

/**
 * @brief Compute free energy
 *
 * Computes variational free energy for current state.
 *
 * @param bridge Bridge handle
 * @param beliefs Current beliefs
 * @param observations Current observations
 * @param num_observations Number of observations
 * @return Free energy value
 */
float fep_parietal_compute_free_energy(
    fep_parietal_bridge_t* bridge,
    const fep_math_belief_t* beliefs,
    const float* observations,
    uint32_t num_observations
);

/* ============================================================================
 * ACTIVE INFERENCE API
 * ============================================================================ */

/**
 * @brief Evaluate problem-solving policies
 *
 * Computes expected free energy for each strategy.
 *
 * @param bridge Bridge handle
 * @param problem Current problem state
 * @param policies Output policy evaluations
 * @param num_policies Number of policies to evaluate
 * @return 0 on success
 */
int fep_parietal_evaluate_policies(
    fep_parietal_bridge_t* bridge,
    const fep_problem_state_t* problem,
    fep_math_policy_t* policies,
    uint32_t* num_policies
);

/**
 * @brief Select problem-solving action via active inference
 *
 * Uses expected free energy minimization to select next step.
 *
 * @param bridge Bridge handle
 * @param problem Current problem state
 * @param result Output action and policy selection
 * @return 0 on success
 */
int fep_parietal_active_inference(
    fep_parietal_bridge_t* bridge,
    const fep_problem_state_t* problem,
    fep_active_inference_result_t* result
);

/**
 * @brief Update beliefs after taking action
 *
 * Incorporates action outcome into belief state.
 *
 * @param bridge Bridge handle
 * @param action Action taken
 * @param action_dim Action dimensionality
 * @param outcome Observed outcome
 * @param outcome_dim Outcome dimensionality
 * @return 0 on success
 */
int fep_parietal_update_from_action(
    fep_parietal_bridge_t* bridge,
    const float* action,
    uint32_t action_dim,
    const float* outcome,
    uint32_t outcome_dim
);

/* ============================================================================
 * PRECISION MODULATION API
 * ============================================================================ */

/**
 * @brief Set attention-driven precision
 *
 * Modulates precision based on attention/salience.
 *
 * @param bridge Bridge handle
 * @param attention_weights Attention weights per dimension
 * @param dim Dimensionality
 * @return 0 on success
 */
int fep_parietal_set_attention_precision(
    fep_parietal_bridge_t* bridge,
    const float* attention_weights,
    uint32_t dim
);

/**
 * @brief Adapt precision from prediction errors
 *
 * Learns precision from recent prediction error history.
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int fep_parietal_adapt_precision(fep_parietal_bridge_t* bridge);

/**
 * @brief Get current precision estimates
 *
 * @param bridge Bridge handle
 * @param level Hierarchy level
 * @param precision Output precision values
 * @param dim Output dimensionality
 * @return 0 on success
 */
int fep_parietal_get_precision(
    const fep_parietal_bridge_t* bridge,
    uint32_t level,
    float** precision,
    uint32_t* dim
);

/* ============================================================================
 * HIERARCHICAL MODEL API
 * ============================================================================ */

/**
 * @brief Get generative model state
 *
 * @param bridge Bridge handle
 * @param model Output model state
 * @return 0 on success
 */
int fep_parietal_get_generative_model(
    const fep_parietal_bridge_t* bridge,
    fep_math_generative_model_t* model
);

/**
 * @brief Update generative model from experience
 *
 * Learns model parameters from observed data.
 *
 * @param bridge Bridge handle
 * @param observations Training observations
 * @param targets Target outputs
 * @param num_samples Number of training samples
 * @return Training loss
 */
float fep_parietal_train_model(
    fep_parietal_bridge_t* bridge,
    const float** observations,
    const float** targets,
    uint32_t num_samples
);

/* ============================================================================
 * DOMAIN-SPECIFIC API
 * ============================================================================ */

/**
 * @brief Process numerical reasoning through FEP
 *
 * Uses predictive processing for number sense.
 *
 * @param bridge Bridge handle
 * @param quantities Input quantities
 * @param num_quantities Number of quantities
 * @param estimated Output estimate with uncertainty
 * @return 0 on success
 */
int fep_parietal_numerical_inference(
    fep_parietal_bridge_t* bridge,
    const float* quantities,
    uint32_t num_quantities,
    fep_math_belief_t* estimated
);

/**
 * @brief Process spatial reasoning through FEP
 *
 * Uses generative models for spatial transformations.
 *
 * @param bridge Bridge handle
 * @param positions Spatial positions
 * @param num_positions Number of positions
 * @param transformed Output transformed beliefs
 * @return 0 on success
 */
int fep_parietal_spatial_inference(
    fep_parietal_bridge_t* bridge,
    const float* positions,
    uint32_t num_positions,
    fep_math_belief_t* transformed
);

/**
 * @brief Process physics reasoning through FEP
 *
 * Uses physics-informed generative models.
 *
 * @param bridge Bridge handle
 * @param state Physical state
 * @param state_dim State dimensionality
 * @param dt Time step
 * @param predicted Output predicted state with uncertainty
 * @return 0 on success
 */
int fep_parietal_physics_inference(
    fep_parietal_bridge_t* bridge,
    const float* state,
    uint32_t state_dim,
    float dt,
    fep_math_belief_t* predicted
);

/**
 * @brief Process engineering analysis through FEP
 *
 * Uses domain-specific generative models for engineering.
 *
 * @param bridge Bridge handle
 * @param input Engineering input parameters
 * @param input_dim Input dimensionality
 * @param domain Engineering domain type
 * @param result Output result with uncertainty
 * @return 0 on success
 */
int fep_parietal_engineering_inference(
    fep_parietal_bridge_t* bridge,
    const float* input,
    uint32_t input_dim,
    fep_math_domain_t domain,
    fep_math_belief_t* result
);

/* ============================================================================
 * SURPRISE & CURIOSITY API
 * ============================================================================ */

/**
 * @brief Compute surprise from observation
 *
 * @param bridge Bridge handle
 * @param observation Observation
 * @param dim Dimensionality
 * @return Surprise value (negative log probability)
 */
float fep_parietal_compute_surprise(
    fep_parietal_bridge_t* bridge,
    const float* observation,
    uint32_t dim
);

/**
 * @brief Compute epistemic value (curiosity)
 *
 * Information gain from potential observation.
 *
 * @param bridge Bridge handle
 * @param query Potential query/action
 * @param query_dim Query dimensionality
 * @return Expected information gain
 */
float fep_parietal_epistemic_value(
    fep_parietal_bridge_t* bridge,
    const float* query,
    uint32_t query_dim
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int fep_parietal_set_inflammation(fep_parietal_bridge_t* bridge, float level);
int fep_parietal_set_fatigue(fep_parietal_bridge_t* bridge, float level);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int fep_parietal_get_stats(
    const fep_parietal_bridge_t* bridge,
    fep_parietal_stats_t* stats
);

void fep_parietal_reset_stats(fep_parietal_bridge_t* bridge);

const char* fep_parietal_get_last_error(void);

/* ============================================================================
 * INTEGRATION API
 * ============================================================================ */

/**
 * @brief Attach to core FEP system
 *
 * Enables coordination with main FEP module.
 *
 * @param bridge Bridge handle
 * @param fep Core FEP system
 * @return 0 on success
 */
int fep_parietal_attach_fep_system(
    fep_parietal_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Free math belief
 */
void fep_parietal_free_belief(fep_math_belief_t* belief);

/**
 * @brief Free prediction
 */
void fep_parietal_free_prediction(fep_math_prediction_t* prediction);

/**
 * @brief Free active inference result
 */
void fep_parietal_free_inference_result(fep_active_inference_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_PARIETAL_BRIDGE_H */
