/**
 * @file nimcp_training_causal_model.h
 * @brief Causal Model of Training Dynamics — DAG-based intervention planning
 *
 * WHAT: Causal DAG modeling relationships between training hyperparameters and outcomes
 * WHY:  LR, batch_size, gradient_clipping, regularization are causally entangled.
 *       Changing one without understanding causal effects creates cascading misdiagnosis.
 * HOW:  Build a causal DAG, observe current metrics, query interventions via do-calculus
 *
 * BIOLOGICAL BASIS:
 * Models the prefrontal cortex's ability to mentally simulate the effects of
 * parameter changes before committing to them — "what if" planning that prevents
 * the training pipeline from chasing correlated symptoms instead of root causes.
 *
 * INTEGRATION:
 * - Uses causal_dag_t from nimcp_reasoning_causal.h
 * - Feeds into training_logic_bridge for decision gating
 * - Observes metrics from brain_training_integration
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_TRAINING_CAUSAL_MODEL_H
#define NIMCP_TRAINING_CAUSAL_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define TRAINING_CAUSAL_MODEL_NAME    "training_causal_model"
#define TRAINING_CAUSAL_MODEL_VERSION "1.0.0"

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Pre-defined node IDs for the training causal DAG
 *
 * WHAT: Enumeration of all variables in the training causal model
 * WHY:  Provides stable, named references to DAG nodes for intervention queries
 */
typedef enum {
    TRAIN_CAUSAL_LEARNING_RATE = 0,     /**< Learning rate hyperparameter */
    TRAIN_CAUSAL_BATCH_SIZE,            /**< Batch size hyperparameter */
    TRAIN_CAUSAL_GRADIENT_CLIPPING,     /**< Gradient clipping threshold */
    TRAIN_CAUSAL_REGULARIZATION,        /**< Regularization strength */
    TRAIN_CAUSAL_GRADIENT_MAGNITUDE,    /**< Observed gradient norm */
    TRAIN_CAUSAL_GRADIENT_NOISE,        /**< Gradient noise level */
    TRAIN_CAUSAL_GRADIENT_VARIANCE,     /**< Gradient variance */
    TRAIN_CAUSAL_LOSS_TRAJECTORY,       /**< Loss trajectory (primary outcome) */
    TRAIN_CAUSAL_LOSS_VOLATILITY,       /**< Loss volatility (instability) */
    TRAIN_CAUSAL_CONVERGENCE_SPEED,     /**< Convergence speed (meta outcome) */
    TRAIN_CAUSAL_EFFECTIVE_CAPACITY,    /**< Effective model capacity */
    TRAIN_CAUSAL_AROUSAL_LEVEL,         /**< Biological arousal modulation */
    TRAIN_CAUSAL_INFLAMMATION_LEVEL,    /**< Immune system inflammation */
    TRAIN_CAUSAL_RESOURCE_PRESSURE,     /**< Portia resource pressure */
    TRAIN_CAUSAL_NODE_COUNT             /**< Sentinel: total node count */
} training_causal_node_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Result of an intervention query
 *
 * WHAT: Contains predicted effects, confidence, and explanation
 * WHY:  The training pipeline needs actionable information before changing params
 */
typedef struct {
    float predicted_effect;              /**< Predicted outcome probability [0,1] */
    float confidence;                    /**< Confidence in prediction [0,1] */
    bool is_beneficial;                  /**< True if intervention improves loss_trajectory */
    float causal_strength;               /**< Strength of causal path */
    char explanation[256];               /**< Human-readable explanation */
} training_intervention_result_t;

/**
 * @brief Current training observation snapshot
 *
 * WHAT: All observable training metrics at a point in time
 * WHY:  Feed observations into the causal DAG for conditioning on current state
 */
typedef struct {
    float learning_rate;                 /**< Current learning rate */
    float batch_size;                    /**< Current batch size (as float for normalization) */
    float gradient_clip;                 /**< Current gradient clipping threshold */
    float regularization;                /**< Current regularization strength */
    float gradient_norm;                 /**< Observed gradient norm */
    float loss_current;                  /**< Current loss value */
    float loss_volatility;               /**< Loss volatility metric */
    float gradient_variance;             /**< Gradient variance metric */
    float arousal_level;                 /**< Biological arousal level [0,1] */
    float inflammation_level;            /**< Immune inflammation level [0,1] */
    float resource_pressure;             /**< Portia resource pressure [0,1] */
} training_causal_observation_t;

/**
 * @brief Opaque handle for the training causal model
 */
typedef struct training_causal_model training_causal_model_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create a training causal model
 *
 * WHAT: Allocate and initialize the causal DAG with training-specific nodes and edges
 * WHY:  Required before any intervention queries
 * HOW:  Creates a causal_dag_t, adds TRAIN_CAUSAL_NODE_COUNT nodes, wires causal edges
 *
 * @return Model instance or NULL on allocation failure
 *
 * COMPLEXITY: O(N + E) where N = node count, E = edge count
 */
training_causal_model_t* training_causal_model_create(void);

/**
 * @brief Destroy a training causal model
 *
 * WHAT: Free all model resources including the underlying DAG
 * WHY:  Prevent memory leaks
 *
 * @param model Model to destroy (NULL safe)
 *
 * COMPLEXITY: O(1)
 */
void training_causal_model_destroy(training_causal_model_t* model);

/*=============================================================================
 * OBSERVATION
 *===========================================================================*/

/**
 * @brief Update the model with current training observations
 *
 * WHAT: Feed current metric values into the causal DAG
 * WHY:  Observations condition the DAG for accurate intervention predictions
 * HOW:  Call causal_dag_observe() for each non-NAN metric
 *
 * @param model Training causal model
 * @param obs Observation snapshot (NAN values are skipped)
 * @return 0 on success, -1 on error
 */
int training_causal_model_observe(training_causal_model_t* model,
                                   const training_causal_observation_t* obs);

/*=============================================================================
 * INTERVENTION QUERIES
 *===========================================================================*/

/**
 * @brief Query: "If I set do(LR = proposed_lr), what happens to loss?"
 *
 * @param model Training causal model
 * @param proposed_lr Proposed learning rate value
 * @param result Output intervention result
 * @return 0 on success, -1 on error
 */
int training_causal_model_query_lr_intervention(training_causal_model_t* model,
                                                  float proposed_lr,
                                                  training_intervention_result_t* result);

/**
 * @brief Query: "If I scale batch by proposed_batch_factor, what happens to loss?"
 *
 * @param model Training causal model
 * @param proposed_batch_factor Batch size scaling factor (e.g., 2.0 = double)
 * @param result Output intervention result
 * @return 0 on success, -1 on error
 */
int training_causal_model_query_batch_intervention(training_causal_model_t* model,
                                                     float proposed_batch_factor,
                                                     training_intervention_result_t* result);

/**
 * @brief Query: "If I set do(clip = proposed_clip), what happens to loss?"
 *
 * @param model Training causal model
 * @param proposed_clip Proposed gradient clipping threshold
 * @param result Output intervention result
 * @return 0 on success, -1 on error
 */
int training_causal_model_query_clip_intervention(training_causal_model_t* model,
                                                    float proposed_clip,
                                                    training_intervention_result_t* result);

/**
 * @brief Generic intervention query on any node
 *
 * WHAT: Query the effect of do(node = value) on target
 * WHY:  Flexible interface for custom intervention analysis
 *
 * @param model Training causal model
 * @param node Node to intervene on
 * @param value Intervention value
 * @param target Node to measure effect on
 * @param result Output intervention result
 * @return 0 on success, -1 on error
 */
int training_causal_model_query_intervention(training_causal_model_t* model,
                                               training_causal_node_t node,
                                               float value,
                                               training_causal_node_t target,
                                               training_intervention_result_t* result);

/*=============================================================================
 * EXPLANATION
 *===========================================================================*/

/**
 * @brief Generate a human-readable explanation of the current causal state
 *
 * WHAT: Summarize observed values and key causal relationships
 * WHY:  Debugging and logging support for training decisions
 *
 * @param model Training causal model (const)
 * @param buf Output buffer
 * @param buf_len Buffer length
 * @return 0 on success, -1 on error
 */
int training_causal_model_explain_state(const training_causal_model_t* model,
                                          char* buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_CAUSAL_MODEL_H */
