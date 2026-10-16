/**
 * @file nimcp_impulse_control.h
 * @brief Impulse control and behavioral inhibition for Raphe Nuclei
 * @date 2026-01-11
 *
 * Models the serotonergic contribution to impulse control:
 * - Behavioral inhibition (stop signals)
 * - Patience and waiting behavior
 * - Risk aversion
 * - Go/No-Go decision modulation
 *
 * Low 5-HT is associated with:
 * - Impulsivity
 * - Aggression
 * - Risk-taking
 * - Premature responses
 */

#ifndef NIMCP_IMPULSE_CONTROL_H
#define NIMCP_IMPULSE_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define IMPULSE_DEFAULT_INHIBITION    0.6f    /* Baseline inhibition */
#define IMPULSE_DEFAULT_PATIENCE      0.5f    /* Baseline patience */
#define IMPULSE_DEFAULT_RISK_AVERSION 0.5f    /* Baseline risk aversion */
#define IMPULSE_5HT_GAIN              0.8f    /* 5-HT -> inhibition gain */

/*=============================================================================
 * Enumerations
 *===========================================================================*/

typedef enum {
    IMPULSE_DECISION_GO = 0,        /**< Execute action */
    IMPULSE_DECISION_NOGO,          /**< Inhibit action */
    IMPULSE_DECISION_WAIT           /**< Delay decision */
} nimcp_impulse_decision_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Impulse control configuration
 */
typedef struct {
    float baseline_inhibition;
    float baseline_patience;
    float baseline_risk_aversion;
    float ht_inhibition_gain;       /**< 5-HT -> inhibition mapping */
    float ht_patience_gain;         /**< 5-HT -> patience mapping */
    float urgency_decay;            /**< How urgency builds over time */
} nimcp_impulse_config_t;

/**
 * @brief Action evaluation result
 */
typedef struct {
    nimcp_impulse_decision_t decision;
    float inhibition_strength;      /**< Strength of inhibition */
    float confidence;               /**< Confidence in decision */
    float waiting_cost;             /**< Cost of waiting */
    float action_cost;              /**< Cost of acting now */
} nimcp_impulse_result_t;

/**
 * @brief Impulse control system
 */
typedef struct {
    bool initialized;

    /* State */
    float inhibition_strength;      /**< Current inhibition [0-1] */
    float patience;                 /**< Current patience [0-1] */
    float risk_aversion;            /**< Current risk aversion [0-1] */
    float impulsivity;              /**< Inverse of inhibition [0-1] */

    /* Urgency accumulator */
    float accumulated_urgency;      /**< Builds during waiting */
    float urgency_threshold;        /**< Threshold for impulsive action */

    /* 5-HT state */
    float current_5ht;
    float baseline_5ht;

    /* Statistics */
    uint32_t go_decisions;
    uint32_t nogo_decisions;
    uint32_t wait_decisions;
    uint32_t impulsive_actions;     /**< Actions despite inhibition */

    /* Configuration */
    nimcp_impulse_config_t config;

} nimcp_impulse_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

int nimcp_impulse_init(nimcp_impulse_system_t* system, const nimcp_impulse_config_t* config);
int nimcp_impulse_shutdown(nimcp_impulse_system_t* system);
int nimcp_impulse_reset(nimcp_impulse_system_t* system);
nimcp_impulse_config_t nimcp_impulse_default_config(void);

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_impulse_update(nimcp_impulse_system_t* system, float ht_level, float dt);

/*=============================================================================
 * Decision API
 *===========================================================================*/

/**
 * @brief Evaluate action for impulse control
 * @param system Impulse control system
 * @param action_urgency How urgent the action is [0-1]
 * @param action_reward Expected reward
 * @param action_risk Associated risk [0-1]
 * @param result Output decision result
 */
int nimcp_impulse_evaluate(
    nimcp_impulse_system_t* system,
    float action_urgency,
    float action_reward,
    float action_risk,
    nimcp_impulse_result_t* result
);

/**
 * @brief Compute inhibition signal for behavior
 */
int nimcp_impulse_compute_inhibition(
    nimcp_impulse_system_t* system,
    float impulse_strength,
    float* inhibition_output
);

/**
 * @brief Check if waiting is sustainable
 */
int nimcp_impulse_can_wait(
    nimcp_impulse_system_t* system,
    float wait_duration,
    bool* can_wait
);

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_impulse_get_inhibition(nimcp_impulse_system_t* system, float* inhibition);
int nimcp_impulse_get_patience(nimcp_impulse_system_t* system, float* patience);
int nimcp_impulse_get_impulsivity(nimcp_impulse_system_t* system, float* impulsivity);
int nimcp_impulse_get_risk_aversion(nimcp_impulse_system_t* system, float* risk_aversion);

/*=============================================================================
 * Urgency API
 *===========================================================================*/

int nimcp_impulse_reset_urgency(nimcp_impulse_system_t* system);
int nimcp_impulse_get_urgency(nimcp_impulse_system_t* system, float* urgency);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMPULSE_CONTROL_H */
