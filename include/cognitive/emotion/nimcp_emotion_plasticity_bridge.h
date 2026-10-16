/**
 * @file nimcp_emotion_plasticity_bridge.h
 * @brief Emotion System - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between emotion system and plasticity mechanisms
 * WHY:  Enable emotional learning through STDP, BCM, and reward-modulated plasticity
 * HOW:  Track emotional observations/responses for spike-timing dependent learning
 *
 * THEORETICAL FOUNDATIONS:
 * - LeDoux (2000): Emotional learning and the amygdala
 * - Phelps & LeDoux (2005): Fear conditioning and extinction
 * - Pessoa (2008): Emotion-cognition interactions
 *
 * BIOLOGICAL BASIS:
 * - Amygdala-based fear conditioning relies on STDP
 * - Emotional salience modulates learning rate
 * - Reward/punishment signals gate plasticity
 * - Emotional state influences memory consolidation
 *
 * INTEGRATION FLOWS:
 *
 * Emotion System --> Plasticity:
 *   1. Emotional observations trigger pre-synaptic spike timing
 *   2. Emotion intensity modulates learning rate
 *   3. Valence determines LTP vs LTD bias
 *   4. Arousal gates eligibility trace decay
 *
 * Plasticity --> Emotion System:
 *   1. Weight changes affect emotion sensitivity
 *   2. Learned associations modify emotional responses
 *   3. Habituation reduces emotional reactivity
 *   4. Extinction learning modifies fear responses
 *
 * @see nimcp_plasticity.h
 * @see nimcp_emotion_recognition.h
 * @see nimcp_emotion_snn_bridge.h
 */

#ifndef NIMCP_EMOTION_PLASTICITY_BRIDGE_H
#define NIMCP_EMOTION_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/nimcp_emotion_recognition.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum tracked emotion synapses */
#define EMOTION_PLASTICITY_MAX_SYNAPSES     256

/** @brief Default STDP time window (ms) */
#define EMOTION_PLASTICITY_STDP_WINDOW      50.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_EMOTION_PLASTICITY       0x0B10

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Emotion synapse types
 */
typedef enum {
    EMOTION_SYNAPSE_STIMULUS_TO_EMOTION = 0,  /**< Stimulus -> emotion association */
    EMOTION_SYNAPSE_EMOTION_TO_RESPONSE,      /**< Emotion -> response pathway */
    EMOTION_SYNAPSE_CONTEXT_TO_EMOTION,       /**< Context -> emotion modulation */
    EMOTION_SYNAPSE_INTERHEMISPHERIC          /**< Cross-hemisphere emotion links */
} emotion_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    EMOTION_LEARN_CONDITIONING = 0,  /**< Classical conditioning */
    EMOTION_LEARN_EXTINCTION,        /**< Extinction learning */
    EMOTION_LEARN_RECONSOLIDATION,   /**< Memory reconsolidation */
    EMOTION_LEARN_HABITUATION,       /**< Habituation/sensitization */
    EMOTION_LEARN_REWARD,            /**< Reward-based learning */
    EMOTION_LEARN_PUNISHMENT         /**< Punishment-based learning */
} emotion_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    EMOTION_PLASTICITY_STATE_IDLE = 0,
    EMOTION_PLASTICITY_STATE_OBSERVING,
    EMOTION_PLASTICITY_STATE_RESPONDING,
    EMOTION_PLASTICITY_STATE_UPDATING,
    EMOTION_PLASTICITY_STATE_CONSOLIDATING,
    EMOTION_PLASTICITY_STATE_DISABLED
} emotion_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Emotion-plasticity bridge configuration
 */
typedef struct {
    /* STDP parameters */
    float stdp_ltp_window_ms;        /**< LTP time window */
    float stdp_ltd_window_ms;        /**< LTD time window */
    float stdp_a_plus;               /**< LTP amplitude */
    float stdp_a_minus;              /**< LTD amplitude */
    float stdp_tau_plus;             /**< LTP time constant */
    float stdp_tau_minus;            /**< LTD time constant */

    /* Valence modulation */
    bool enable_valence_modulation;  /**< Valence affects LTP/LTD ratio */
    float positive_valence_ltp_boost;/**< LTP boost for positive emotions */
    float negative_valence_ltd_boost;/**< LTD boost for negative emotions */

    /* Arousal modulation */
    bool enable_arousal_modulation;  /**< Arousal affects learning rate */
    float arousal_learning_gain;     /**< How much arousal boosts learning */
    float low_arousal_decay_boost;   /**< Faster decay at low arousal */

    /* BCM metaplasticity */
    bool enable_bcm;                 /**< Enable BCM threshold sliding */
    float bcm_threshold_tau;         /**< Threshold adaptation rate */
    float bcm_activity_tau;          /**< Activity averaging rate */

    /* Homeostatic plasticity */
    bool enable_homeostatic;         /**< Enable synaptic scaling */
    float target_response_rate;      /**< Target emotional response rate */
    float homeostatic_tau_ms;        /**< Scaling time constant */

    /* Eligibility traces */
    bool enable_eligibility;         /**< Enable eligibility traces */
    float eligibility_decay;         /**< Trace decay rate */
    float reward_modulation_gain;    /**< Reward scaling */

    /* Weight bounds */
    float weight_min;                /**< Minimum weight */
    float weight_max;                /**< Maximum weight */
    float initial_weight;            /**< Initial weight */

    /* Extinction */
    bool enable_extinction;          /**< Enable extinction learning */
    float extinction_rate;           /**< Extinction learning rate */
    float spontaneous_recovery_tau;  /**< Spontaneous recovery time constant */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_immune_modulation;   /**< Enable immune system effects */
    bool enable_sleep_consolidation; /**< Enable sleep-dependent consolidation */
} emotion_plasticity_config_t;

//=============================================================================
// Synapse Structure
//=============================================================================

/**
 * @brief Emotion-associated synapse state
 */
typedef struct {
    uint32_t synapse_id;             /**< Unique synapse identifier */
    emotion_synapse_type_t type;     /**< Synapse type */
    emotion_category_t associated_emotion; /**< Associated emotion category */

    /* Weight state */
    float weight;                    /**< Current synaptic weight */
    float initial_weight;            /**< Initial weight (for extinction) */

    /* Timing state */
    uint64_t last_pre_spike_us;      /**< Last pre-synaptic spike time */
    uint64_t last_post_spike_us;     /**< Last post-synaptic spike time */

    /* Eligibility */
    float eligibility_trace;         /**< Current eligibility trace */

    /* BCM state */
    float bcm_threshold;             /**< Sliding threshold */
    float avg_activity;              /**< Running activity average */

    /* Extinction state */
    float extinction_level;          /**< Amount of extinction [0, 1] */
    float original_strength;         /**< Pre-extinction strength */
} emotion_plasticity_synapse_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Bridge runtime state
 */
typedef struct {
    emotion_plasticity_state_t state;
    uint32_t registered_synapses;
    float global_learning_rate;
    float current_valence_mod;
    float current_arousal_mod;
    bool bio_async_connected;
} emotion_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_observations;
    uint64_t total_responses;
    uint64_t total_pre_spikes;
    uint64_t total_post_spikes;
    uint64_t ltp_events;
    uint64_t ltd_events;
    uint64_t extinction_events;
    float avg_weight_change;
    float total_reward;
    float total_punishment;
} emotion_plasticity_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Weight change callback
 */
typedef void (*emotion_weight_change_cb)(
    uint32_t synapse_id,
    emotion_category_t emotion,
    float old_weight,
    float new_weight,
    emotion_learn_event_t event_type,
    void* user_data
);

//=============================================================================
// Main Bridge Structure
//=============================================================================

/** @brief Forward declaration */
typedef struct emotion_plasticity_bridge emotion_plasticity_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
emotion_plasticity_config_t emotion_plasticity_config_default(void);

/**
 * @brief Create emotion-plasticity bridge
 */
emotion_plasticity_bridge_t* emotion_plasticity_create(
    const emotion_plasticity_config_t* config
);

/**
 * @brief Destroy emotion-plasticity bridge
 */
void emotion_plasticity_destroy(emotion_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int emotion_plasticity_reset(emotion_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register synapse for emotional learning
 */
int emotion_plasticity_register_synapse(
    emotion_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    emotion_synapse_type_t type,
    emotion_category_t associated_emotion,
    float initial_weight
);

/**
 * @brief Unregister synapse
 */
int emotion_plasticity_unregister_synapse(
    emotion_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 */
int emotion_plasticity_get_synapse(
    emotion_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    emotion_plasticity_synapse_t* synapse
);

//=============================================================================
// Event Recording (Emotion --> Plasticity)
//=============================================================================

/**
 * @brief Record emotional stimulus observation
 */
int emotion_plasticity_stimulus(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion,
    float intensity,
    uint64_t timestamp_us
);

/**
 * @brief Record emotional response
 */
int emotion_plasticity_response(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion,
    float response_strength,
    uint64_t timestamp_us
);

/**
 * @brief Record reward/punishment signal
 */
int emotion_plasticity_reward(
    emotion_plasticity_bridge_t* bridge,
    float reward,  /* Positive = reward, negative = punishment */
    uint64_t timestamp_us
);

/**
 * @brief Record extinction trial (CS without US)
 */
int emotion_plasticity_extinction_trial(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion,
    uint64_t timestamp_us
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update all plasticity mechanisms
 */
int emotion_plasticity_update(
    emotion_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Trigger memory consolidation
 */
int emotion_plasticity_consolidate(
    emotion_plasticity_bridge_t* bridge
);

//=============================================================================
// Query Functions (Plasticity --> Emotion)
//=============================================================================

/**
 * @brief Get emotional response modulation
 */
int emotion_plasticity_get_response_modulation(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion,
    float* modulation
);

/**
 * @brief Get extinction level for emotion
 */
float emotion_plasticity_get_extinction_level(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion
);

/**
 * @brief Get learned emotional sensitivity
 */
float emotion_plasticity_get_sensitivity(
    emotion_plasticity_bridge_t* bridge,
    emotion_category_t emotion
);

//=============================================================================
// State and Statistics
//=============================================================================

/**
 * @brief Get bridge state
 */
int emotion_plasticity_get_state(
    const emotion_plasticity_bridge_t* bridge,
    emotion_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int emotion_plasticity_get_stats(
    const emotion_plasticity_bridge_t* bridge,
    emotion_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void emotion_plasticity_reset_stats(emotion_plasticity_bridge_t* bridge);

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Register weight change callback
 */
int emotion_plasticity_set_weight_callback(
    emotion_plasticity_bridge_t* bridge,
    emotion_weight_change_cb callback,
    void* user_data
);

//=============================================================================
// Modulation Functions
//=============================================================================

/**
 * @brief Set valence modulation
 */
int emotion_plasticity_set_valence_modulation(
    emotion_plasticity_bridge_t* bridge,
    float valence
);

/**
 * @brief Set arousal modulation
 */
int emotion_plasticity_set_arousal_modulation(
    emotion_plasticity_bridge_t* bridge,
    float arousal
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int emotion_plasticity_connect_bio_async(emotion_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int emotion_plasticity_disconnect_bio_async(emotion_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool emotion_plasticity_is_bio_async_connected(const emotion_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_PLASTICITY_BRIDGE_H */
