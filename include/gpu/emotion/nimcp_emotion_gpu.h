/**
 * @file nimcp_emotion_gpu.h
 * @brief GPU-accelerated Emotion Processing Kernels
 *
 * WHAT: CUDA kernels for limbic system and emotional processing
 * WHY:  GPU acceleration for biologically-inspired emotion computation
 * HOW:  Custom kernels for amygdala, OFC, reward, fear conditioning
 *
 * ARCHITECTURE:
 * - Amygdala: Fear/threat detection, emotional memory, conditioning
 * - Orbitofrontal Cortex (OFC): Value computation, decision-making
 * - Nucleus Accumbens: Reward processing, motivation
 * - Anterior Cingulate: Conflict monitoring, error detection
 * - Insula: Interoception, emotional awareness
 *
 * All functions support both CUDA GPU and CPU fallback implementations.
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_EMOTION_GPU_H
#define NIMCP_EMOTION_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Emotion Types and Enumerations
//=============================================================================

/**
 * @brief Basic emotion types (Ekman's six + extensions)
 */
typedef enum {
    NIMCP_EMOTION_NEUTRAL = 0,
    NIMCP_EMOTION_FEAR = 1,
    NIMCP_EMOTION_ANGER = 2,
    NIMCP_EMOTION_DISGUST = 3,
    NIMCP_EMOTION_SADNESS = 4,
    NIMCP_EMOTION_HAPPINESS = 5,
    NIMCP_EMOTION_SURPRISE = 6,
    NIMCP_EMOTION_ANTICIPATION = 7,
    NIMCP_EMOTION_TRUST = 8,
    NIMCP_EMOTION_COUNT = 9
} nimcp_emotion_type_t;

/**
 * @brief Valence-arousal space representation
 */
typedef struct {
    float valence;     /**< Positive/negative (-1 to +1) */
    float arousal;     /**< Low/high activation (0 to 1) */
    float dominance;   /**< Submissive/dominant (-1 to +1) */
} nimcp_emotion_pad_t;

//=============================================================================
// Amygdala Parameters and Structures
//=============================================================================

/**
 * @brief Amygdala processing parameters
 */
typedef struct {
    float threat_threshold;       /**< Threshold for threat detection */
    float fear_learning_rate;     /**< Fear conditioning learning rate */
    float extinction_rate;        /**< Fear extinction rate */
    float generalization_sigma;   /**< Stimulus generalization width */
    float habituation_tau;        /**< Habituation time constant */
    float sensitization_tau;      /**< Sensitization time constant */
    float context_weight;         /**< Context modulation weight */
    float prefrontal_inhibition;  /**< PFC inhibitory control */
    float lateral_inhibition;     /**< Lateral nucleus inhibition */
    float basal_threshold;        /**< Basal nucleus activation threshold */
} nimcp_gpu_amygdala_params_t;

/**
 * @brief Amygdala state for GPU processing
 */
typedef struct {
    nimcp_gpu_tensor_t* threat_signal;      /**< Current threat level */
    nimcp_gpu_tensor_t* fear_memory;        /**< Consolidated fear memories */
    nimcp_gpu_tensor_t* cs_us_associations; /**< CS-US associations */
    nimcp_gpu_tensor_t* extinction_trace;   /**< Extinction learning trace */
    nimcp_gpu_tensor_t* context_gate;       /**< Context-dependent gating */
    nimcp_gpu_tensor_t* lateral_activity;   /**< Lateral nucleus activity */
    nimcp_gpu_tensor_t* basal_activity;     /**< Basal nucleus activity */
    nimcp_gpu_tensor_t* central_output;     /**< Central nucleus output */
    size_t n_stimuli;                       /**< Number of stimulus features */
    size_t n_contexts;                      /**< Number of contexts */
} nimcp_gpu_amygdala_state_t;

//=============================================================================
// Orbitofrontal Cortex Parameters and Structures
//=============================================================================

/**
 * @brief OFC value computation parameters
 */
typedef struct {
    float value_learning_rate;    /**< Value update learning rate */
    float discount_factor;        /**< Temporal discount gamma */
    float reversal_rate;          /**< Reversal learning rate */
    float comparison_gain;        /**< Comparison signal gain */
    float risk_sensitivity;       /**< Risk sensitivity parameter */
    float satiety_decay;          /**< Satiety decay rate */
    float integration_tau;        /**< Value integration time constant */
    float outcome_sensitivity;    /**< Outcome sensitivity */
} nimcp_gpu_ofc_params_t;

/**
 * @brief OFC state for GPU processing
 */
typedef struct {
    nimcp_gpu_tensor_t* option_values;      /**< Value of each option */
    nimcp_gpu_tensor_t* expected_outcomes;  /**< Expected outcome per option */
    nimcp_gpu_tensor_t* outcome_history;    /**< Recent outcome history */
    nimcp_gpu_tensor_t* reversal_signal;    /**< Reversal learning signal */
    nimcp_gpu_tensor_t* choice_probabilities; /**< Softmax choice probs */
    nimcp_gpu_tensor_t* satiety_state;      /**< Current satiety levels */
    nimcp_gpu_tensor_t* risk_assessment;    /**< Risk per option */
    size_t n_options;                       /**< Number of choice options */
    size_t n_outcomes;                      /**< Number of outcome types */
} nimcp_gpu_ofc_state_t;

//=============================================================================
// Nucleus Accumbens Parameters and Structures
//=============================================================================

/**
 * @brief Nucleus Accumbens reward processing parameters
 */
typedef struct {
    float reward_sensitivity;     /**< Reward signal sensitivity */
    float effort_cost;            /**< Effort cost weight */
    float delay_discount;         /**< Delay discounting rate */
    float hedonic_baseline;       /**< Hedonic setpoint */
    float motivation_decay;       /**< Motivation decay rate */
    float dopamine_gain;          /**< DA modulation gain */
    float gaba_inhibition;        /**< GABAergic inhibition */
    float glutamate_excitation;   /**< Glutamatergic excitation */
} nimcp_gpu_nacc_params_t;

/**
 * @brief Nucleus Accumbens state
 */
typedef struct {
    nimcp_gpu_tensor_t* reward_prediction;  /**< Predicted reward */
    nimcp_gpu_tensor_t* motivation_signal;  /**< Motivation/wanting */
    nimcp_gpu_tensor_t* hedonic_signal;     /**< Hedonic/liking */
    nimcp_gpu_tensor_t* effort_signal;      /**< Effort evaluation */
    nimcp_gpu_tensor_t* dopamine_input;     /**< DA input from VTA */
    nimcp_gpu_tensor_t* msn_d1_activity;    /**< D1-MSN activity (Go) */
    nimcp_gpu_tensor_t* msn_d2_activity;    /**< D2-MSN activity (NoGo) */
    size_t n_states;                        /**< Number of states */
} nimcp_gpu_nacc_state_t;

//=============================================================================
// Anterior Cingulate Cortex Parameters
//=============================================================================

/**
 * @brief ACC conflict/error monitoring parameters
 */
typedef struct {
    float conflict_threshold;     /**< Conflict detection threshold */
    float error_learning_rate;    /**< Error-driven learning rate */
    float effort_sensitivity;     /**< Effort allocation sensitivity */
    float prediction_weight;      /**< Prediction error weight */
    float volatility_estimate;    /**< Environmental volatility */
    float control_gain;           /**< Cognitive control gain */
} nimcp_gpu_acc_params_t;

/**
 * @brief ACC state
 */
typedef struct {
    nimcp_gpu_tensor_t* conflict_signal;    /**< Response conflict */
    nimcp_gpu_tensor_t* error_signal;       /**< Prediction error */
    nimcp_gpu_tensor_t* effort_allocation;  /**< Effort allocation */
    nimcp_gpu_tensor_t* volatility;         /**< Estimated volatility */
    nimcp_gpu_tensor_t* control_signal;     /**< Control demand signal */
    size_t n_responses;                     /**< Number of response options */
} nimcp_gpu_acc_state_t;

//=============================================================================
// Integrated Emotion System
//=============================================================================

/**
 * @brief Unified emotion system state
 */
typedef struct {
    nimcp_gpu_amygdala_state_t* amygdala;
    nimcp_gpu_ofc_state_t* ofc;
    nimcp_gpu_nacc_state_t* nacc;
    nimcp_gpu_acc_state_t* acc;
    nimcp_gpu_tensor_t* emotion_vector;     /**< Current emotion state */
    nimcp_gpu_tensor_t* arousal_level;      /**< Overall arousal */
    nimcp_gpu_tensor_t* valence_signal;     /**< Overall valence */
    float dt;                               /**< Simulation timestep */
} nimcp_gpu_emotion_system_t;

//=============================================================================
// Default Parameter Functions
//=============================================================================

NIMCP_EXPORT nimcp_gpu_amygdala_params_t nimcp_gpu_amygdala_params_default(void);
NIMCP_EXPORT nimcp_gpu_ofc_params_t nimcp_gpu_ofc_params_default(void);
NIMCP_EXPORT nimcp_gpu_nacc_params_t nimcp_gpu_nacc_params_default(void);
NIMCP_EXPORT nimcp_gpu_acc_params_t nimcp_gpu_acc_params_default(void);

//=============================================================================
// Amygdala Kernel Functions
//=============================================================================

/**
 * @brief Process threat detection in amygdala
 *
 * @param ctx GPU context
 * @param state Amygdala state
 * @param sensory_input Incoming sensory features
 * @param context Current context representation
 * @param threat_out Output threat signal
 * @param params Amygdala parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_amygdala_threat_detection(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* sensory_input,
    const nimcp_gpu_tensor_t* context,
    nimcp_gpu_tensor_t* threat_out,
    const nimcp_gpu_amygdala_params_t* params);

/**
 * @brief Update fear conditioning (CS-US learning)
 *
 * @param ctx GPU context
 * @param state Amygdala state
 * @param cs Conditioned stimulus
 * @param us Unconditioned stimulus (threat)
 * @param dt Time step
 * @param params Amygdala parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_amygdala_fear_conditioning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* cs,
    const nimcp_gpu_tensor_t* us,
    float dt,
    const nimcp_gpu_amygdala_params_t* params);

/**
 * @brief Update fear extinction learning
 */
NIMCP_EXPORT bool nimcp_gpu_amygdala_extinction(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* cs,
    const nimcp_gpu_tensor_t* no_us,
    float dt,
    const nimcp_gpu_amygdala_params_t* params);

/**
 * @brief Apply prefrontal inhibition to amygdala
 */
NIMCP_EXPORT bool nimcp_gpu_amygdala_prefrontal_inhibition(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* pfc_signal,
    const nimcp_gpu_amygdala_params_t* params);

//=============================================================================
// OFC Kernel Functions
//=============================================================================

/**
 * @brief Compute option values in OFC
 */
NIMCP_EXPORT bool nimcp_gpu_ofc_compute_values(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_ofc_state_t* state,
    const nimcp_gpu_tensor_t* stimulus_features,
    const nimcp_gpu_tensor_t* context,
    const nimcp_gpu_ofc_params_t* params);

/**
 * @brief Update values based on outcome
 */
NIMCP_EXPORT bool nimcp_gpu_ofc_value_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_ofc_state_t* state,
    const nimcp_gpu_tensor_t* chosen_option,
    const nimcp_gpu_tensor_t* outcome,
    float dt,
    const nimcp_gpu_ofc_params_t* params);

/**
 * @brief Compute choice probabilities via softmax
 */
NIMCP_EXPORT bool nimcp_gpu_ofc_choice_probabilities(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_ofc_state_t* state,
    float temperature,
    const nimcp_gpu_ofc_params_t* params);

/**
 * @brief Detect reversal and update accordingly
 */
NIMCP_EXPORT bool nimcp_gpu_ofc_reversal_learning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_ofc_state_t* state,
    const nimcp_gpu_tensor_t* prediction_error,
    float dt,
    const nimcp_gpu_ofc_params_t* params);

//=============================================================================
// Nucleus Accumbens Kernel Functions
//=============================================================================

/**
 * @brief Update reward prediction in NAcc
 */
NIMCP_EXPORT bool nimcp_gpu_nacc_reward_prediction(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_nacc_state_t* state,
    const nimcp_gpu_tensor_t* state_features,
    const nimcp_gpu_tensor_t* action,
    const nimcp_gpu_nacc_params_t* params);

/**
 * @brief Compute motivation signal (wanting)
 */
NIMCP_EXPORT bool nimcp_gpu_nacc_compute_motivation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_nacc_state_t* state,
    const nimcp_gpu_tensor_t* dopamine,
    const nimcp_gpu_tensor_t* effort_required,
    const nimcp_gpu_nacc_params_t* params);

/**
 * @brief Update D1/D2 MSN activity
 */
NIMCP_EXPORT bool nimcp_gpu_nacc_msn_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_nacc_state_t* state,
    const nimcp_gpu_tensor_t* cortical_input,
    const nimcp_gpu_tensor_t* dopamine,
    float dt,
    const nimcp_gpu_nacc_params_t* params);

/**
 * @brief Compute Go/NoGo decision signal
 */
NIMCP_EXPORT bool nimcp_gpu_nacc_go_nogo(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_nacc_state_t* state,
    nimcp_gpu_tensor_t* go_signal,
    nimcp_gpu_tensor_t* nogo_signal,
    const nimcp_gpu_nacc_params_t* params);

//=============================================================================
// ACC Kernel Functions
//=============================================================================

/**
 * @brief Detect response conflict in ACC
 */
NIMCP_EXPORT bool nimcp_gpu_acc_conflict_detection(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acc_state_t* state,
    const nimcp_gpu_tensor_t* response_activations,
    const nimcp_gpu_acc_params_t* params);

/**
 * @brief Compute prediction error signal
 */
NIMCP_EXPORT bool nimcp_gpu_acc_error_signal(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acc_state_t* state,
    const nimcp_gpu_tensor_t* expected,
    const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_acc_params_t* params);

/**
 * @brief Allocate cognitive effort
 */
NIMCP_EXPORT bool nimcp_gpu_acc_effort_allocation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acc_state_t* state,
    const nimcp_gpu_tensor_t* task_demand,
    const nimcp_gpu_tensor_t* reward_expectation,
    const nimcp_gpu_acc_params_t* params);

//=============================================================================
// Integrated Emotion Functions
//=============================================================================

/**
 * @brief Update complete emotion system
 */
NIMCP_EXPORT bool nimcp_gpu_emotion_system_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_emotion_system_t* system,
    const nimcp_gpu_tensor_t* sensory_input,
    const nimcp_gpu_tensor_t* reward_signal,
    const nimcp_gpu_tensor_t* context,
    float dt);

/**
 * @brief Compute current emotion state (PAD model)
 */
NIMCP_EXPORT bool nimcp_gpu_emotion_compute_state(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_emotion_system_t* system,
    nimcp_gpu_tensor_t* valence_out,
    nimcp_gpu_tensor_t* arousal_out,
    nimcp_gpu_tensor_t* dominance_out);

/**
 * @brief Map continuous emotion to discrete categories
 */
NIMCP_EXPORT bool nimcp_gpu_emotion_categorize(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* valence,
    const nimcp_gpu_tensor_t* arousal,
    nimcp_gpu_tensor_t* emotion_probs);

/**
 * @brief Compute emotional influence on cognition
 */
NIMCP_EXPORT bool nimcp_gpu_emotion_cognitive_modulation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_emotion_system_t* system,
    nimcp_gpu_tensor_t* attention_bias,
    nimcp_gpu_tensor_t* memory_enhancement,
    nimcp_gpu_tensor_t* decision_bias);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EMOTION_GPU_H
