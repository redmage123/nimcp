/**
 * @file nimcp_training_diagnosis.h
 * @brief Abductive Training Diagnosis -- root cause analysis for training failures
 *
 * WHAT: Uses abductive reasoning to diagnose WHY training went wrong
 * WHY:  Current system detects symptoms (NaN, grad explosion) but doesn't reason about causes
 * HOW:  Collect observations from continuous metrics, generate hypotheses, rank by plausibility
 *
 * BIOLOGICAL BASIS:
 * Models the diagnostic reasoning of the anterior prefrontal cortex when
 * monitoring ongoing processes. Instead of simple threshold-based reactions,
 * the system generates causal hypotheses about training failures.
 *
 * INTEGRATION:
 * Sits between the training logic bridge (symptom detection) and the
 * intervention system (corrective actions). Transforms detected symptoms
 * into explanatory diagnoses with actionable recommendations.
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#ifndef NIMCP_TRAINING_DIAGNOSIS_H
#define NIMCP_TRAINING_DIAGNOSIS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum observations the diagnoser can track per step */
#define TRAINING_DIAG_MAX_OBSERVATIONS 12

/*=============================================================================
 * OBSERVATION TYPES
 *===========================================================================*/

/**
 * @brief Pre-built observation categories for training anomalies
 *
 * WHAT: Enumeration of recognizable training symptoms
 * WHY:  Allows automatic detection from raw metrics
 */
typedef enum {
    TRAIN_OBS_GRADIENT_INCREASING,       /**< Gradient norm growing rapidly */
    TRAIN_OBS_GRADIENT_VANISHING,        /**< Gradient norm near zero */
    TRAIN_OBS_LOSS_INCREASING,           /**< Loss getting worse */
    TRAIN_OBS_LOSS_OSCILLATING,          /**< Loss bouncing up and down */
    TRAIN_OBS_LOSS_PLATEAU,              /**< Loss stuck, not improving */
    TRAIN_OBS_LOSS_NAN,                  /**< Loss is NaN or Inf */
    TRAIN_OBS_HIGH_VARIANCE,             /**< High gradient variance */
    TRAIN_OBS_LOW_THROUGHPUT,            /**< Training throughput dropped */
    TRAIN_OBS_HIGH_MEMORY,              /**< Memory pressure high */
    TRAIN_OBS_AROUSAL_EXTREME,           /**< Arousal level at extremes */
    TRAIN_OBS_INFLAMMATION_HIGH,         /**< Brain immune system inflamed */
    TRAIN_OBS_RESOURCE_PRESSURE_HIGH,    /**< Resource pressure from Portia */
    TRAIN_OBS_COUNT                      /**< Sentinel: number of observation types */
} training_observation_type_t;

/*=============================================================================
 * RESULT STRUCTURES
 *===========================================================================*/

/**
 * @brief Diagnosis result with root cause analysis and recommended actions
 *
 * WHAT: The output of abductive reasoning over training observations
 * WHY:  Provides explanations (not just symptoms) and derived corrective actions
 */
typedef struct {
    char primary_cause[256];             /**< Best explanation */
    float primary_plausibility;          /**< Plausibility [0,1] */
    char secondary_cause[256];           /**< Second best explanation */
    float secondary_plausibility;        /**< Second plausibility [0,1] */

    /* Recommended actions derived from diagnosis */
    bool recommend_reduce_lr;            /**< Should reduce learning rate */
    bool recommend_increase_batch;       /**< Should increase batch size */
    bool recommend_tighter_clip;         /**< Should tighten gradient clipping */
    bool recommend_pause;                /**< Should pause training */
    bool recommend_rollback;             /**< Should rollback to checkpoint */
    float recommended_lr_factor;         /**< Suggested LR multiplier [0.01,1.0] */

    uint32_t num_observations;           /**< How many observations contributed */
    uint32_t num_hypotheses;             /**< How many hypotheses generated */
} training_diagnosis_t;

/*=============================================================================
 * OPAQUE HANDLE
 *===========================================================================*/

/**
 * @brief Opaque training diagnoser handle
 */
typedef struct training_diagnoser training_diagnoser_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create a training diagnoser
 *
 * WHAT: Allocate and initialize a diagnoser backed by abductive reasoning
 * WHY:  Required before any diagnosis operations
 *
 * @return Diagnoser instance or NULL on allocation failure
 *
 * COMPLEXITY: O(1)
 */
training_diagnoser_t* training_diagnoser_create(void);

/**
 * @brief Destroy a training diagnoser
 *
 * WHAT: Free all resources including the underlying abduction engine
 * WHY:  Prevent memory leaks
 *
 * @param diag Diagnoser to destroy (NULL safe)
 */
void training_diagnoser_destroy(training_diagnoser_t* diag);

/*=============================================================================
 * OBSERVATION COLLECTION
 *===========================================================================*/

/**
 * @brief Collect observations from current training metrics
 *
 * WHAT: Analyze raw metrics and automatically detect training anomalies
 * WHY:  Transforms numerical metrics into symbolic observations for abduction
 * HOW:  Applies threshold-based detection for each observation category,
 *       then feeds detected observations into the abductive reasoning engine
 *
 * @param diag           Diagnoser instance
 * @param loss_current   Current loss value
 * @param loss_previous  Previous step's loss value
 * @param grad_norm      Current gradient L2 norm
 * @param grad_norm_previous Previous gradient norm
 * @param loss_volatility Loss volatility measure (std dev / mean)
 * @param gradient_variance Gradient variance across parameters
 * @param learning_rate  Current learning rate
 * @param batch_size     Current batch size
 * @param arousal_level  Brain arousal level [0,1]
 * @param inflammation_level Brain immune inflammation level [0,1]
 * @param resource_pressure Portia resource pressure [0,1]
 * @return 0 on success, -1 on error
 */
int training_diagnoser_observe_from_metrics(
    training_diagnoser_t* diag,
    float loss_current, float loss_previous,
    float grad_norm, float grad_norm_previous,
    float loss_volatility, float gradient_variance,
    float learning_rate, float batch_size,
    float arousal_level, float inflammation_level,
    float resource_pressure);

/*=============================================================================
 * DIAGNOSIS
 *===========================================================================*/

/**
 * @brief Run diagnosis -- generate hypotheses and select best explanation
 *
 * WHAT: Execute abductive reasoning over accumulated observations
 * WHY:  Core function that transforms symptoms into root cause explanations
 * HOW:  Calls the abduction engine to generate hypotheses, extracts top 2,
 *       then derives recommended corrective actions from the diagnosis
 *
 * @param diag   Diagnoser instance
 * @param result Output diagnosis result (caller provides)
 * @return 0 on success, -1 on error
 */
int training_diagnoser_diagnose(training_diagnoser_t* diag,
                                 training_diagnosis_t* result);

/*=============================================================================
 * STATE MANAGEMENT
 *===========================================================================*/

/**
 * @brief Reset diagnoser for next training step
 *
 * WHAT: Clear accumulated observations and prepare for fresh analysis
 * WHY:  Allow reuse across training steps without recreating
 *
 * @param diag Diagnoser instance
 * @return 0 on success, -1 on error
 */
int training_diagnoser_reset(training_diagnoser_t* diag);

/*=============================================================================
 * UTILITIES
 *===========================================================================*/

/**
 * @brief Get human-readable name for an observation type
 *
 * WHAT: Map observation enum to descriptive string
 * WHY:  Logging and debugging support
 *
 * @param type Observation type
 * @return Static string name, or "unknown" for invalid types
 */
const char* training_diagnosis_observation_name(training_observation_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_DIAGNOSIS_H */
