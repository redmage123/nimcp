/**
 * @file nimcp_mirror_vicarious_reward.h
 * @brief Mirror Neuron Vicarious Reward Bridge
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Enables experiencing reward vicariously through observing others
 * WHY:  Social learning and empathy depend on shared reward representations
 * HOW:  Mirror neurons in premotor/parietal project to reward circuits, allowing
 *       observed rewards to trigger dopamine responses in the observer
 *
 * THEORETICAL FOUNDATIONS:
 * - Rizzolatti & Craighero (2004): Mirror neuron system overview
 * - Mobbs et al. (2009): Vicarious reward in ACC and ventral striatum
 * - Apps & Ramnani (2014): Social action observation and reward
 * - Burke et al. (2010): Observing others' rewards activates own reward system
 * - Lockwood (2016): Vicarious effort and reward
 *
 * BIOLOGICAL BASIS:
 * - Mirror neurons in F5/PPC connect to ventral striatum via frontal projections
 * - Anterior cingulate cortex (ACC) codes vicarious reward prediction errors
 * - Orbitofrontal cortex integrates social vs personal reward
 * - Dopamine neurons respond to observed rewards (attenuated relative to direct)
 * - Stronger vicarious reward for ingroup members and familiar individuals
 *
 * VICARIOUS REWARD PROCESSING:
 * 1. Mirror neurons detect action leading to reward (goal-directed action)
 * 2. Outcome prediction based on observed action
 * 3. Observed reward triggers attenuated dopamine release
 * 4. Prediction error = observed_outcome - predicted_outcome
 * 5. Learning updates value representations for observed actions
 *
 * MODULATING FACTORS:
 * - Social distance: Closer = stronger vicarious reward
 * - Ingroup/outgroup: Ingroup amplifies vicarious reward
 * - Familiarity: Familiar agents produce stronger responses
 * - Competition: Competitive context can invert vicarious reward
 *
 * BIO-ASYNC MESSAGES:
 * - BIO_MSG_MIRROR_VICARIOUS_REWARD: Observed reward event
 * - BIO_MSG_MIRROR_VICARIOUS_RPE: Reward prediction error
 * - BIO_MSG_MIRROR_SOCIAL_REWARD_MODULATE: Social modulation signal
 *
 * @see nimcp_basal_ganglia.h
 * @see nimcp_mirror_emotion_bridge.h
 * @see nimcp_mirror_tom_bridge.h
 */

#ifndef NIMCP_MIRROR_VICARIOUS_REWARD_H
#define NIMCP_MIRROR_VICARIOUS_REWARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum tracked agents for reward learning */
#define VICARIOUS_MAX_AGENTS            32

/** @brief Maximum action-outcome associations */
#define VICARIOUS_MAX_ASSOCIATIONS      128

/** @brief Reward value history length */
#define VICARIOUS_HISTORY_SIZE          32

/** @brief Maximum reward dimensions (value, magnitude, probability, delay) */
#define VICARIOUS_REWARD_DIMS           4

/** @brief SIMD batch threshold */
#define VICARIOUS_SIMD_THRESHOLD        8

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Vicarious reward type
 *
 * WHAT: Classification of observed reward situation
 */
typedef enum {
    VICARIOUS_REWARD_NONE = 0,
    VICARIOUS_REWARD_POSITIVE,          /**< Observed gain/pleasure */
    VICARIOUS_REWARD_NEGATIVE,          /**< Observed loss/pain */
    VICARIOUS_REWARD_RELIEF,            /**< Observed avoidance of negative */
    VICARIOUS_REWARD_ANTICIPATORY       /**< Observed anticipation of reward */
} vicarious_reward_type_t;

/**
 * @brief Social relationship affecting vicarious reward
 */
typedef enum {
    SOCIAL_RELATION_UNKNOWN = 0,
    SOCIAL_RELATION_SELF,               /**< Self (direct reward) */
    SOCIAL_RELATION_INGROUP,            /**< Ingroup member */
    SOCIAL_RELATION_OUTGROUP,           /**< Outgroup member */
    SOCIAL_RELATION_COMPETITOR,         /**< Direct competitor */
    SOCIAL_RELATION_COOPERATOR,         /**< Cooperative partner */
    SOCIAL_RELATION_KIN                 /**< Family/kin */
} social_relation_t;

/**
 * @brief Vicarious response type
 */
typedef enum {
    VICARIOUS_RESPONSE_NONE = 0,
    VICARIOUS_RESPONSE_EMPATHIC,        /**< Share the reward feeling */
    VICARIOUS_RESPONSE_SCHADENFREUDE,   /**< Pleasure at other's misfortune */
    VICARIOUS_RESPONSE_ENVY,            /**< Distress at other's gain */
    VICARIOUS_RESPONSE_SYMPATHETIC      /**< Concern for other's loss */
} vicarious_response_t;

/**
 * @brief Module processing state
 */
typedef enum {
    VICARIOUS_STATE_IDLE = 0,
    VICARIOUS_STATE_OBSERVING,          /**< Observing action */
    VICARIOUS_STATE_PREDICTING,         /**< Predicting outcome */
    VICARIOUS_STATE_PROCESSING_OUTCOME, /**< Processing observed outcome */
    VICARIOUS_STATE_UPDATING            /**< Updating value estimates */
} vicarious_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Observed action-reward event
 *
 * WHAT: Input describing observed agent receiving reward
 */
typedef struct {
    uint32_t agent_id;                  /**< Observed agent */
    uint32_t action_id;                 /**< Action that led to reward */

    /** Reward parameters */
    vicarious_reward_type_t reward_type;
    float reward_magnitude;             /**< Magnitude [0-1] */
    float reward_probability;           /**< How certain was reward? [0-1] */
    float reward_delay_ms;              /**< Delay from action to reward */

    /** Context */
    social_relation_t social_relation;
    float social_distance;              /**< Perceived social distance [0-1] */
    float familiarity;                  /**< Agent familiarity [0-1] */
    bool is_competitive_context;

    /** Timing */
    uint64_t action_time_us;
    uint64_t reward_time_us;

    /** Mirror observation quality */
    float mirror_activation;            /**< Mirror neuron response strength */
    float observation_confidence;
} vicarious_observation_t;

/**
 * @brief Vicarious reward response
 *
 * WHAT: Output - observer's vicarious reward experience
 */
typedef struct {
    /** Reward signal */
    float vicarious_reward;             /**< Experienced vicarious reward [-1, +1] */
    float reward_prediction_error;      /**< RPE: actual - predicted */
    float expected_reward;              /**< What we predicted */

    /** Response characterization */
    vicarious_response_t response_type;
    float response_intensity;           /**< Response strength [0-1] */

    /** Dopamine proxy */
    float dopamine_delta;               /**< Change in dopamine-like signal */
    float baseline_dopamine;            /**< Baseline before observation */

    /** Modulation factors (for transparency) */
    float social_modulation;            /**< Social distance effect */
    float familiarity_modulation;       /**< Familiarity effect */
    float competition_modulation;       /**< Competition context effect */

    /** Learning signals */
    float value_update;                 /**< Amount to update action value */
    bool should_imitate;                /**< Should observer imitate action? */
    float imitation_likelihood;         /**< Likelihood of imitation [0-1] */

    /** Timing */
    uint64_t timestamp_us;
    float processing_latency_ms;
} vicarious_response_result_t;

/**
 * @brief Per-agent vicarious learning state
 */
typedef struct {
    uint32_t agent_id;
    bool active;

    /** Social parameters */
    social_relation_t relation;
    float social_distance;
    float familiarity;
    float trust;

    /** Vicarious history */
    float reward_history[VICARIOUS_HISTORY_SIZE];
    float prediction_history[VICARIOUS_HISTORY_SIZE];
    uint32_t history_count;
    uint32_t history_index;

    /** Aggregates */
    float avg_reward;
    float reward_variance;
    float avg_prediction_error;

    /** Learning */
    uint32_t observations_count;
    uint64_t first_observation_us;
    uint64_t last_observation_us;
} vicarious_agent_state_t;

/**
 * @brief Action-outcome association
 *
 * WHAT: Learned mapping from observed actions to expected outcomes
 */
typedef struct {
    uint32_t action_id;

    /** Learned values */
    float expected_reward;              /**< Average observed reward */
    float expected_probability;         /**< Probability of reward */
    float expected_delay_ms;            /**< Average delay to reward */

    /** Confidence */
    uint32_t observation_count;
    float value_confidence;

    /** Temporal */
    uint64_t last_update_us;
} action_outcome_assoc_t;

/**
 * @brief Configuration
 */
typedef struct {
    /** Attenuation parameters */
    float vicarious_gain;               /**< Scale of vicarious vs direct [0-1] */
    float social_distance_decay;        /**< How much distance attenuates */
    float familiarity_boost;            /**< Familiarity amplification */

    /** Competition effects */
    float competition_inversion;        /**< How much competition inverts reward */
    bool enable_schadenfreude;          /**< Allow pleasure at other's misfortune */
    bool enable_envy;                   /**< Allow distress at other's gain */

    /** Learning */
    float learning_rate;                /**< Value learning rate */
    float prediction_decay;             /**< Decay of predictions over time */
    float eligibility_trace_decay;      /**< For temporal credit assignment */

    /** Imitation threshold */
    float imitation_threshold;          /**< Min vicarious reward to consider imitation */
    float imitation_familiar_bonus;     /**< Bonus for imitating familiar agents */

    /** Social weighting */
    float ingroup_weight;               /**< Weight for ingroup members */
    float outgroup_weight;              /**< Weight for outgroup */
    float kin_weight;                   /**< Weight for kin */

    /** SIMD */
    bool enable_simd;

    /** Bio-async */
    bool bio_async_enabled;
} vicarious_reward_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t total_observations;
    uint64_t positive_rewards;
    uint64_t negative_rewards;

    float avg_vicarious_reward;
    float avg_prediction_error;
    float avg_social_modulation;

    uint64_t empathic_responses;
    uint64_t schadenfreude_responses;
    uint64_t envy_responses;
    uint64_t sympathetic_responses;

    uint64_t imitation_recommendations;
    float avg_imitation_likelihood;

    uint32_t active_agents;
    uint32_t learned_associations;

    uint64_t simd_operations;
} vicarious_reward_stats_t;

/** Forward declaration */
typedef struct vicarious_reward_system vicarious_reward_system_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 */
vicarious_reward_config_t vicarious_reward_config_default(void);

/**
 * @brief Create vicarious reward system
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on error
 */
vicarious_reward_system_t* vicarious_reward_create(
    const vicarious_reward_config_t* config
);

/**
 * @brief Destroy system
 */
void vicarious_reward_destroy(vicarious_reward_system_t* system);

//=============================================================================
// Core Processing API
//=============================================================================

/**
 * @brief Process observed reward event
 *
 * WHAT: Main entry - observe another agent receiving reward
 * WHY:  Generate vicarious reward response and update learning
 *
 * @param system System handle
 * @param observation Observed reward event
 * @param result Output: Vicarious response
 * @return true on success
 */
bool vicarious_reward_process(
    vicarious_reward_system_t* system,
    const vicarious_observation_t* observation,
    vicarious_response_result_t* result
);

/**
 * @brief Batch process observations (SIMD)
 */
uint32_t vicarious_reward_process_batch(
    vicarious_reward_system_t* system,
    const vicarious_observation_t* observations,
    vicarious_response_result_t* results,
    uint32_t count
);

/**
 * @brief Predict reward for observed action
 *
 * WHAT: Predict expected reward before outcome is observed
 * WHY:  Required for computing prediction error
 *
 * @param system System handle
 * @param agent_id Agent performing action
 * @param action_id Action being performed
 * @return Expected reward value
 */
float vicarious_reward_predict(
    vicarious_reward_system_t* system,
    uint32_t agent_id,
    uint32_t action_id
);

//=============================================================================
// Agent State API
//=============================================================================

/**
 * @brief Get or create agent state
 */
vicarious_agent_state_t* vicarious_reward_get_agent(
    vicarious_reward_system_t* system,
    uint32_t agent_id
);

/**
 * @brief Set social relation for agent
 */
void vicarious_reward_set_relation(
    vicarious_reward_system_t* system,
    uint32_t agent_id,
    social_relation_t relation
);

/**
 * @brief Update agent familiarity
 */
void vicarious_reward_update_familiarity(
    vicarious_reward_system_t* system,
    uint32_t agent_id,
    float delta
);

/**
 * @brief Set competitive context for agent
 */
void vicarious_reward_set_competitive(
    vicarious_reward_system_t* system,
    uint32_t agent_id,
    bool is_competitor
);

//=============================================================================
// Value Learning API
//=============================================================================

/**
 * @brief Get learned action value
 *
 * @param system System handle
 * @param action_id Action to query
 * @return Learned expected reward
 */
float vicarious_reward_get_action_value(
    const vicarious_reward_system_t* system,
    uint32_t action_id
);

/**
 * @brief Get action-outcome association
 */
const action_outcome_assoc_t* vicarious_reward_get_association(
    const vicarious_reward_system_t* system,
    uint32_t action_id
);

/**
 * @brief Decay all predictions (temporal decay)
 */
void vicarious_reward_decay_predictions(
    vicarious_reward_system_t* system,
    float dt_sec
);

//=============================================================================
// Dopamine Signal API
//=============================================================================

/**
 * @brief Get current dopamine-like signal
 *
 * WHAT: Query dopamine proxy for reward signaling
 * WHY:  Interface with basal ganglia and other dopamine-sensitive systems
 *
 * @param system System handle
 * @return Current dopamine level
 */
float vicarious_reward_get_dopamine(
    const vicarious_reward_system_t* system
);

/**
 * @brief Inject external reward signal
 *
 * WHAT: Add direct (non-vicarious) reward signal
 * WHY:  Combine with vicarious for total reward
 *
 * @param system System handle
 * @param direct_reward Direct reward signal
 */
void vicarious_reward_inject_direct(
    vicarious_reward_system_t* system,
    float direct_reward
);

//=============================================================================
// Social Modulation API
//=============================================================================

/**
 * @brief Compute social modulation factor
 *
 * @param system System handle
 * @param agent_id Agent identifier
 * @return Modulation factor [0-2] (1.0 = neutral)
 */
float vicarious_reward_social_modulation(
    const vicarious_reward_system_t* system,
    uint32_t agent_id
);

/**
 * @brief Check if should feel schadenfreude
 */
bool vicarious_reward_is_schadenfreude(
    const vicarious_reward_system_t* system,
    uint32_t agent_id,
    float observed_negative_reward
);

/**
 * @brief Check if should feel envy
 */
bool vicarious_reward_is_envy(
    const vicarious_reward_system_t* system,
    uint32_t agent_id,
    float observed_positive_reward
);

//=============================================================================
// SIMD Optimization API
//=============================================================================

/**
 * @brief SIMD batch social modulation computation
 */
void vicarious_reward_simd_social_mod(
    const float* distances,
    const float* familiarities,
    const float* relation_weights,
    float* modulations,
    uint32_t count,
    const vicarious_reward_config_t* config
);

/**
 * @brief SIMD batch reward prediction error
 */
void vicarious_reward_simd_rpe(
    const float* observed_rewards,
    const float* predicted_rewards,
    const float* modulations,
    float* rpes,
    uint32_t count
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async
 */
bool vicarious_reward_register_bio_async(vicarious_reward_system_t* system);

/**
 * @brief Unregister from bio-async
 */
void vicarious_reward_unregister_bio_async(vicarious_reward_system_t* system);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get statistics
 */
bool vicarious_reward_get_stats(
    const vicarious_reward_system_t* system,
    vicarious_reward_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void vicarious_reward_reset_stats(vicarious_reward_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_VICARIOUS_REWARD_H */
