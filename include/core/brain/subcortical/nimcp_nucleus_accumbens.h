//=============================================================================
// nimcp_nucleus_accumbens.h - Nucleus Accumbens Specialization
//=============================================================================
/**
 * @file nimcp_nucleus_accumbens.h
 * @brief Specialized nucleus accumbens (ventral striatum) implementation
 *
 * BIOLOGICAL BASIS:
 * The nucleus accumbens (NAc) is a key structure for:
 * - Reward processing and motivation
 * - Pleasure and reinforcement
 * - Pavlovian-instrumental transfer (PIT)
 * - Drug addiction and craving
 *
 * SUBREGIONS:
 * - Core: Motor/goal-directed behavior, instrumental conditioning
 * - Shell: Hedonic processing, Pavlovian conditioning, feeding
 *
 * INPUTS:
 * - VTA dopamine (mesolimbic pathway)
 * - Hippocampus (contextual information)
 * - Amygdala (emotional valence)
 * - PFC (goal information)
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_NUCLEUS_ACCUMBENS_H
#define NIMCP_NUCLEUS_ACCUMBENS_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define NAC_DEFAULT_NEURONS         256
#define NAC_MAX_REWARDS             32
#define NAC_MAX_CUES                64

/** Dopamine response parameters */
#define NAC_DA_BASELINE             0.5f
#define NAC_DA_BURST_MULTIPLIER     3.0f
#define NAC_DA_PAUSE_MULTIPLIER     0.2f

/** PIT parameters */
#define NAC_PIT_PAVLOVIAN_WEIGHT    0.4f
#define NAC_PIT_INSTRUMENTAL_WEIGHT 0.6f

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief NAc subregion
 */
typedef enum {
    NAC_SUBREGION_CORE,             /**< Goal-directed, motor */
    NAC_SUBREGION_SHELL,            /**< Hedonic, Pavlovian */
    NAC_SUBREGION_COUNT
} nac_subregion_t;

/**
 * @brief Motivational state
 */
typedef enum {
    NAC_STATE_NEUTRAL,              /**< Baseline motivation */
    NAC_STATE_WANTING,              /**< Incentive salience, seeking */
    NAC_STATE_LIKING,               /**< Hedonic pleasure */
    NAC_STATE_CRAVING,              /**< Intense wanting */
    NAC_STATE_SATIATED,             /**< Post-consummatory satisfaction */
    NAC_STATE_AVERSIVE,             /**< Avoidance motivation */
    NAC_STATE_COUNT
} nac_motivation_state_t;

/**
 * @brief Cue-reward association type
 */
typedef enum {
    NAC_ASSOC_PAVLOVIAN,            /**< Cue predicts reward */
    NAC_ASSOC_INSTRUMENTAL,         /**< Action produces reward */
    NAC_ASSOC_PIT_SPECIFIC,         /**< Cue biases specific action */
    NAC_ASSOC_PIT_GENERAL,          /**< Cue increases all responding */
    NAC_ASSOC_COUNT
} nac_association_type_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Reward representation
 */
typedef struct {
    uint32_t id;
    char name[64];
    float value;                    /**< Reward magnitude */
    float uncertainty;              /**< Reward uncertainty */
    float satiation_rate;           /**< How fast satiation develops */
    float current_satiation;        /**< Current satiation level */
    bool is_primary;                /**< Primary (food) vs secondary (money) */
} nac_reward_t;

/**
 * @brief Cue representation
 */
typedef struct {
    uint32_t id;
    float* features;                /**< Cue feature vector */
    uint32_t feature_dim;
    float salience;                 /**< Attention-grabbing strength */

    /* Associations */
    uint32_t associated_reward;
    float association_strength;
    nac_association_type_t assoc_type;

    /* Pavlovian response */
    float conditioned_response;
    uint32_t pairings;
} nac_cue_t;

/**
 * @brief Pavlovian-Instrumental Transfer state
 */
typedef struct {
    float pavlovian_bias;           /**< Current Pavlovian influence */
    float instrumental_control;     /**< Current instrumental control */
    float* action_biases;           /**< Per-action bias from cues */
    uint32_t num_actions;
    float general_activation;       /**< General PIT effect */
    float specific_activation;      /**< Specific PIT effect */
} nac_pit_state_t;

/**
 * @brief NAc configuration
 */
typedef struct {
    uint32_t core_neurons;
    uint32_t shell_neurons;

    float da_sensitivity;
    float learning_rate;
    float satiation_decay;

    float pavlovian_weight;
    float instrumental_weight;
    float pit_threshold;

    bool enable_craving;
    float craving_threshold;
    float craving_decay;

    bool enable_hedonic_hotspots;
} nac_config_t;

/**
 * @brief NAc statistics
 */
typedef struct {
    nac_motivation_state_t state;
    float wanting_level;
    float liking_level;
    float dopamine_level;
    float pit_effect;
    uint32_t rewards_processed;
    uint32_t cue_associations;
} nac_stats_t;

/**
 * @brief Main NAc handle
 */
typedef struct nucleus_accumbens nucleus_accumbens_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

void nac_default_config(nac_config_t* config);
nucleus_accumbens_t* nac_create(const nac_config_t* config);
void nac_destroy(nucleus_accumbens_t* nac);
int nac_reset(nucleus_accumbens_t* nac);

/* ============================================================================
 * REWARD PROCESSING API
 * ============================================================================ */

/**
 * @brief Register a reward type
 */
int nac_register_reward(nucleus_accumbens_t* nac,
                         const nac_reward_t* reward,
                         uint32_t* out_id);

/**
 * @brief Process reward receipt
 */
int nac_process_reward(nucleus_accumbens_t* nac,
                        uint32_t reward_id,
                        float magnitude);

/**
 * @brief Process reward prediction
 */
int nac_process_reward_prediction(nucleus_accumbens_t* nac,
                                   uint32_t reward_id,
                                   float predicted_value);

/**
 * @brief Get wanting (incentive salience) for reward
 */
float nac_get_wanting(const nucleus_accumbens_t* nac, uint32_t reward_id);

/**
 * @brief Get liking (hedonic value) for reward
 */
float nac_get_liking(const nucleus_accumbens_t* nac, uint32_t reward_id);

/* ============================================================================
 * CUE PROCESSING API
 * ============================================================================ */

/**
 * @brief Register a cue
 */
int nac_register_cue(nucleus_accumbens_t* nac,
                      const nac_cue_t* cue,
                      uint32_t* out_id);

/**
 * @brief Process cue presentation
 */
int nac_process_cue(nucleus_accumbens_t* nac,
                     uint32_t cue_id,
                     float intensity);

/**
 * @brief Create cue-reward association (Pavlovian learning)
 */
int nac_associate_cue_reward(nucleus_accumbens_t* nac,
                              uint32_t cue_id,
                              uint32_t reward_id,
                              float reward_magnitude);

/**
 * @brief Get conditioned response to cue
 */
float nac_get_conditioned_response(const nucleus_accumbens_t* nac,
                                    uint32_t cue_id);

/* ============================================================================
 * PAVLOVIAN-INSTRUMENTAL TRANSFER API
 * ============================================================================ */

/**
 * @brief Compute PIT effect on actions
 */
int nac_compute_pit(nucleus_accumbens_t* nac,
                     const float* cue_activations,
                     uint32_t num_cues,
                     nac_pit_state_t* out_pit);

/**
 * @brief Get action bias from current cues
 */
int nac_get_action_bias(const nucleus_accumbens_t* nac,
                         float* action_biases,
                         uint32_t num_actions);

/**
 * @brief Set Pavlovian vs instrumental balance
 */
int nac_set_pit_balance(nucleus_accumbens_t* nac,
                         float pavlovian_weight,
                         float instrumental_weight);

/* ============================================================================
 * DOPAMINE MODULATION API
 * ============================================================================ */

/**
 * @brief Set dopamine level
 */
int nac_set_dopamine(nucleus_accumbens_t* nac, float level);

/**
 * @brief Trigger dopamine burst
 */
int nac_trigger_dopamine_burst(nucleus_accumbens_t* nac, float magnitude);

/**
 * @brief Trigger dopamine pause
 */
int nac_trigger_dopamine_pause(nucleus_accumbens_t* nac, float magnitude);

/**
 * @brief Get current dopamine level
 */
float nac_get_dopamine(const nucleus_accumbens_t* nac);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Step NAc dynamics
 */
int nac_step(nucleus_accumbens_t* nac, float dt_ms);

/**
 * @brief Get output to ventral pallidum
 */
int nac_get_output(const nucleus_accumbens_t* nac,
                    float* core_output,
                    float* shell_output);

/**
 * @brief Get current motivation state
 */
nac_motivation_state_t nac_get_motivation_state(const nucleus_accumbens_t* nac);

/**
 * @brief Get system statistics
 */
int nac_get_stats(const nucleus_accumbens_t* nac, nac_stats_t* stats);

/* ============================================================================
 * INTEGRATION API
 * ============================================================================ */

/**
 * @brief Receive VTA dopamine input
 */
int nac_receive_vta_input(nucleus_accumbens_t* nac,
                           float dopamine,
                           float rpe);

/**
 * @brief Receive amygdala input (emotional valence)
 */
int nac_receive_amygdala_input(nucleus_accumbens_t* nac,
                                float valence,
                                float arousal);

/**
 * @brief Receive hippocampal input (context)
 */
int nac_receive_hippocampal_input(nucleus_accumbens_t* nac,
                                   const float* context,
                                   uint32_t context_dim);

/**
 * @brief Receive PFC input (goals)
 */
int nac_receive_pfc_input(nucleus_accumbens_t* nac,
                           const float* goal_vector,
                           uint32_t goal_dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NUCLEUS_ACCUMBENS_H */
