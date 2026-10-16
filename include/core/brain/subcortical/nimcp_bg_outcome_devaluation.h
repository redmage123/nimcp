//=============================================================================
// nimcp_bg_outcome_devaluation.h - Outcome Devaluation for Goal/Habit Testing
//=============================================================================
/**
 * @file nimcp_bg_outcome_devaluation.h
 * @brief Outcome devaluation system for goal-directed vs habitual behavior
 *
 * BIOLOGICAL BASIS:
 * Outcome devaluation is the gold standard test for goal-directed behavior:
 * - Goal-directed: Sensitive to outcome value changes (reduces responding)
 * - Habitual: Insensitive to outcome value (maintains responding)
 *
 * MECHANISMS:
 * - Sensory-specific satiety: Reduces value of consumed outcome
 * - Taste aversion: Associates outcome with illness
 * - Outcome revaluation: Changes outcome-action associations
 *
 * NEURAL SUBSTRATES:
 * - Dorsomedial striatum: Goal-directed, sensitive to devaluation
 * - Dorsolateral striatum: Habitual, insensitive to devaluation
 * - OFC: Represents outcome value
 * - BLA: Updates value representations
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BG_OUTCOME_DEVALUATION_H
#define NIMCP_BG_OUTCOME_DEVALUATION_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define BGOD_MAX_OUTCOMES           32
#define BGOD_MAX_ACTIONS            32
#define BGOD_SATIETY_DECAY_RATE     0.01f   /**< Satiety decay per step */
#define BGOD_AVERSION_STRENGTH      0.8f    /**< Max aversion strength */

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Devaluation method
 */
typedef enum {
    BGOD_METHOD_SATIETY,            /**< Sensory-specific satiety */
    BGOD_METHOD_AVERSION,           /**< Conditioned taste aversion */
    BGOD_METHOD_EXTINCTION,         /**< Outcome-action extinction */
    BGOD_METHOD_DEGRADATION,        /**< Contingency degradation */
    BGOD_METHOD_COUNT
} bgod_method_t;

/**
 * @brief Behavior type detected
 */
typedef enum {
    BGOD_BEHAVIOR_GOAL_DIRECTED,    /**< Sensitive to devaluation */
    BGOD_BEHAVIOR_HABITUAL,         /**< Insensitive to devaluation */
    BGOD_BEHAVIOR_MIXED,            /**< Partial sensitivity */
    BGOD_BEHAVIOR_UNKNOWN,          /**< Not enough data */
    BGOD_BEHAVIOR_COUNT
} bgod_behavior_type_t;

/**
 * @brief Outcome state
 */
typedef enum {
    BGOD_OUTCOME_VALUED,            /**< Normal value */
    BGOD_OUTCOME_DEVALUED,          /**< Currently devalued */
    BGOD_OUTCOME_REVALUED,          /**< Re-increased value */
    BGOD_OUTCOME_COUNT
} bgod_outcome_state_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Outcome representation
 */
typedef struct {
    uint32_t id;
    char name[64];
    float base_value;               /**< Inherent value */
    float current_value;            /**< Current subjective value */
    float satiety_level;            /**< Current satiety for this outcome */
    float aversion_level;           /**< Conditioned aversion */
    bgod_outcome_state_t state;
    uint32_t consumption_count;
} bgod_outcome_t;

/**
 * @brief Action-outcome association
 */
typedef struct {
    uint32_t action_id;
    uint32_t outcome_id;
    float association_strength;     /**< Learned A-O contingency */
    float baseline_rate;            /**< Baseline response rate */
    float current_rate;             /**< Current response rate */
    uint32_t training_trials;
} bgod_action_outcome_t;

/**
 * @brief Devaluation test result
 */
typedef struct {
    uint32_t outcome_id;
    bgod_method_t method;
    float pre_rate;                 /**< Response rate before devaluation */
    float post_rate;                /**< Response rate after devaluation */
    float ratio;                    /**< post/pre ratio */
    bgod_behavior_type_t behavior;  /**< Detected behavior type */
    float sensitivity;              /**< Devaluation sensitivity [0,1] */
} bgod_test_result_t;

/**
 * @brief Configuration
 */
typedef struct {
    uint32_t max_outcomes;
    uint32_t max_actions;

    float satiety_rate;             /**< How fast satiety develops */
    float satiety_decay;            /**< How fast satiety dissipates */
    float aversion_learning_rate;   /**< Aversion acquisition rate */
    float aversion_extinction_rate; /**< Aversion extinction rate */

    float goal_threshold;           /**< Threshold for goal-directed */
    float habit_threshold;          /**< Threshold for habitual */

    bool enable_revaluation;        /**< Allow value recovery */
    bool track_sensitivity;         /**< Track devaluation sensitivity */
} bgod_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint32_t total_outcomes;
    uint32_t devalued_outcomes;
    float avg_goal_sensitivity;
    float avg_habit_insensitivity;
    uint32_t tests_performed;
    bgod_behavior_type_t dominant_behavior;
} bgod_stats_t;

/**
 * @brief Main handle
 */
typedef struct bg_outcome_deval bg_outcome_deval_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

void bgod_default_config(bgod_config_t* config);
bg_outcome_deval_t* bgod_create(const bgod_config_t* config);
void bgod_destroy(bg_outcome_deval_t* deval);
int bgod_reset(bg_outcome_deval_t* deval);

/* ============================================================================
 * OUTCOME MANAGEMENT API
 * ============================================================================ */

/**
 * @brief Register an outcome
 */
int bgod_register_outcome(bg_outcome_deval_t* deval,
                           const bgod_outcome_t* outcome,
                           uint32_t* out_id);

/**
 * @brief Update outcome value
 */
int bgod_set_outcome_value(bg_outcome_deval_t* deval,
                            uint32_t outcome_id,
                            float value);

/**
 * @brief Get current outcome value
 */
float bgod_get_outcome_value(const bg_outcome_deval_t* deval,
                              uint32_t outcome_id);

/**
 * @brief Register action-outcome association
 */
int bgod_register_association(bg_outcome_deval_t* deval,
                               uint32_t action_id,
                               uint32_t outcome_id,
                               float strength);

/* ============================================================================
 * DEVALUATION API
 * ============================================================================ */

/**
 * @brief Devalue outcome via satiety (pre-feeding)
 */
int bgod_devalue_by_satiety(bg_outcome_deval_t* deval,
                             uint32_t outcome_id,
                             float consumption_amount);

/**
 * @brief Devalue outcome via taste aversion
 */
int bgod_devalue_by_aversion(bg_outcome_deval_t* deval,
                              uint32_t outcome_id,
                              float aversion_strength);

/**
 * @brief Revalue outcome (restore value)
 */
int bgod_revalue_outcome(bg_outcome_deval_t* deval,
                          uint32_t outcome_id);

/**
 * @brief Get devaluation effect on action
 */
float bgod_get_action_value(const bg_outcome_deval_t* deval,
                             uint32_t action_id);

/* ============================================================================
 * TESTING API
 * ============================================================================ */

/**
 * @brief Run devaluation test
 */
int bgod_run_test(bg_outcome_deval_t* deval,
                   uint32_t action_id,
                   uint32_t outcome_id,
                   bgod_method_t method,
                   bgod_test_result_t* result);

/**
 * @brief Classify behavior type based on test
 */
bgod_behavior_type_t bgod_classify_behavior(const bgod_test_result_t* result);

/**
 * @brief Get overall behavior classification
 */
bgod_behavior_type_t bgod_get_overall_behavior(const bg_outcome_deval_t* deval);

/**
 * @brief Get devaluation sensitivity for action
 */
float bgod_get_sensitivity(const bg_outcome_deval_t* deval,
                            uint32_t action_id);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Step dynamics (satiety decay, etc.)
 */
int bgod_step(bg_outcome_deval_t* deval, float dt_ms);

/**
 * @brief Record outcome consumption
 */
int bgod_record_consumption(bg_outcome_deval_t* deval,
                             uint32_t outcome_id,
                             float amount);

/**
 * @brief Record action response
 */
int bgod_record_response(bg_outcome_deval_t* deval,
                          uint32_t action_id);

/**
 * @brief Get statistics
 */
int bgod_get_stats(const bg_outcome_deval_t* deval, bgod_stats_t* stats);

/* ============================================================================
 * INTEGRATION API
 * ============================================================================ */

/**
 * @brief Modulate BG action values based on outcome values
 */
int bgod_modulate_action_values(const bg_outcome_deval_t* deval,
                                 float* action_values,
                                 uint32_t num_actions);

/**
 * @brief Get goal-directed weight (vs habitual)
 */
float bgod_get_goal_weight(const bg_outcome_deval_t* deval);

/**
 * @brief Get habit weight
 */
float bgod_get_habit_weight(const bg_outcome_deval_t* deval);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_OUTCOME_DEVALUATION_H */
