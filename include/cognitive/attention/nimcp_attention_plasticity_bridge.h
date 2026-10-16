/**
 * @file nimcp_attention_plasticity_bridge.h
 * @brief Attention System - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between attention system and plasticity mechanisms
 * WHY:  Enable attention-modulated learning through STDP, BCM, and reward-modulated plasticity
 * HOW:  Track attention allocation for spike-timing dependent learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Desimone & Duncan (1995): Biased competition model of attention
 * - Roelfsema & van Ooyen (2005): Attention-gated reinforcement learning
 * - Noudoost et al. (2010): Attention modulates synaptic plasticity
 *
 * BIOLOGICAL BASIS:
 * - Attended stimuli induce stronger LTP than unattended
 * - Attention gates what enters working memory for consolidation
 * - Acetylcholine release during attention enhances plasticity
 * - Top-down attention modulates sensory cortex learning rates
 *
 * INTEGRATION FLOWS:
 *
 * Attention System --> Plasticity:
 *   1. Attention weight modulates learning rate
 *   2. Salience gates eligibility trace formation
 *   3. Focus strength influences STDP window
 *   4. Top-k selection determines which synapses learn
 *
 * Plasticity --> Attention System:
 *   1. Weight changes affect attention biases
 *   2. Learned associations guide attention allocation
 *   3. Habituation reduces attention to familiar stimuli
 *   4. Novelty detection increases attention gain
 *
 * @see nimcp_plasticity.h
 * @see nimcp_attention.h
 * @see nimcp_attention_snn_bridge.h
 */

#ifndef NIMCP_ATTENTION_PLASTICITY_BRIDGE_H
#define NIMCP_ATTENTION_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "plasticity/attention/nimcp_attention.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum tracked attention synapses */
#define ATTENTION_PLASTICITY_MAX_SYNAPSES     512

/** @brief Default STDP time window (ms) */
#define ATTENTION_PLASTICITY_STDP_WINDOW      40.0f

/* Note: Bio-async module ID is defined in nimcp_bio_messages.h as BIO_MODULE_ATTENTION_PLASTICITY */

/** @brief Maximum attention heads for plasticity tracking */
#define ATTENTION_PLASTICITY_MAX_HEADS        16

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Attention synapse types
 */
typedef enum {
    ATTENTION_SYNAPSE_QUERY_KEY = 0,      /**< Query-key projection synapse */
    ATTENTION_SYNAPSE_KEY_VALUE,          /**< Key-value pathway */
    ATTENTION_SYNAPSE_HEAD_OUTPUT,        /**< Head output projection */
    ATTENTION_SYNAPSE_GATE_CONTROL,       /**< Thalamic gate control synapse */
    ATTENTION_SYNAPSE_SALIENCE,           /**< Salience computation synapse */
    ATTENTION_SYNAPSE_COMPETITION         /**< Competition/WTA synapse */
} attention_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    ATTENTION_LEARN_FOCUS = 0,       /**< Learning during focus state */
    ATTENTION_LEARN_SHIFT,           /**< Learning during attention shift */
    ATTENTION_LEARN_COMPETITION,     /**< Competition-based learning */
    ATTENTION_LEARN_HABITUATION,     /**< Habituation (reduced attention) */
    ATTENTION_LEARN_NOVELTY,         /**< Novelty-driven learning */
    ATTENTION_LEARN_REWARD           /**< Reward-gated attention learning */
} attention_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    ATTENTION_PLASTICITY_STATE_IDLE = 0,
    ATTENTION_PLASTICITY_STATE_OBSERVING,
    ATTENTION_PLASTICITY_STATE_UPDATING,
    ATTENTION_PLASTICITY_STATE_COMPETING,
    ATTENTION_PLASTICITY_STATE_CONSOLIDATING,
    ATTENTION_PLASTICITY_STATE_DISABLED
} attention_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Attention-plasticity bridge configuration
 */
typedef struct {
    /* STDP parameters */
    float stdp_ltp_window_ms;        /**< LTP time window */
    float stdp_ltd_window_ms;        /**< LTD time window */
    float stdp_a_plus;               /**< LTP amplitude */
    float stdp_a_minus;              /**< LTD amplitude */
    float stdp_tau_plus;             /**< LTP time constant */
    float stdp_tau_minus;            /**< LTD time constant */

    /* Attention modulation */
    bool enable_attention_modulation; /**< Attention affects learning rate */
    float focus_learning_boost;      /**< LTP boost for focused items */
    float unfocused_ltd_boost;       /**< LTD boost for unfocused items */
    float attention_learning_gain;   /**< How much attention boosts learning */

    /* Salience modulation */
    bool enable_salience_modulation; /**< Salience affects learning rate */
    float salience_learning_gain;    /**< Salience scaling */
    float salience_threshold;        /**< Threshold for salience gating */

    /* BCM metaplasticity */
    bool enable_bcm;                 /**< Enable BCM threshold sliding */
    float bcm_threshold_tau;         /**< Threshold adaptation rate */
    float bcm_activity_tau;          /**< Activity averaging rate */

    /* Homeostatic plasticity */
    bool enable_homeostatic;         /**< Enable synaptic scaling */
    float target_attention_rate;     /**< Target attention allocation rate */
    float homeostatic_tau_ms;        /**< Scaling time constant */

    /* Eligibility traces */
    bool enable_eligibility;         /**< Enable eligibility traces */
    float eligibility_decay;         /**< Trace decay rate */
    float reward_modulation_gain;    /**< Reward scaling */

    /* Weight bounds */
    float weight_min;                /**< Minimum weight */
    float weight_max;                /**< Maximum weight */
    float initial_weight;            /**< Initial weight */

    /* Habituation */
    bool enable_habituation;         /**< Enable habituation learning */
    float habituation_rate;          /**< Habituation decay rate */
    float spontaneous_recovery_tau;  /**< Recovery time constant */

    /* Novelty detection */
    bool enable_novelty_detection;   /**< Enable novelty modulation */
    float novelty_boost;             /**< Novelty learning boost */
    float familiarity_threshold;     /**< Threshold for "familiar" */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_immune_modulation;   /**< Enable immune system effects */
    bool enable_sleep_consolidation; /**< Enable sleep-dependent consolidation */
} attention_plasticity_config_t;

//=============================================================================
// Synapse Structure
//=============================================================================

/**
 * @brief Attention-associated synapse state
 */
typedef struct {
    uint32_t synapse_id;             /**< Unique synapse identifier */
    attention_synapse_type_t type;   /**< Synapse type */
    uint32_t head_idx;               /**< Associated attention head */

    /* Weight state */
    float weight;                    /**< Current synaptic weight */
    float initial_weight;            /**< Initial weight */

    /* Timing state */
    uint64_t last_pre_spike_us;      /**< Last pre-synaptic spike time */
    uint64_t last_post_spike_us;     /**< Last post-synaptic spike time */

    /* Eligibility */
    float eligibility_trace;         /**< Current eligibility trace */

    /* BCM state */
    float bcm_threshold;             /**< Sliding threshold */
    float avg_activity;              /**< Running activity average */

    /* Habituation state */
    float habituation_level;         /**< Current habituation [0, 1] */
    float original_strength;         /**< Pre-habituation strength */

    /* Novelty state */
    float familiarity;               /**< Familiarity level [0, 1] */
    uint32_t exposure_count;         /**< Number of exposures */
} attention_plasticity_synapse_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Per-head plasticity state
 */
typedef struct {
    float learning_rate;             /**< Current head learning rate */
    float attention_bias;            /**< Learned attention bias */
    float habituation_level;         /**< Head-level habituation */
    float novelty_score;             /**< Current novelty score */
    uint64_t last_focus_time_us;     /**< Last time this head was focused */
} attention_head_plasticity_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    attention_plasticity_state_t state;
    uint32_t registered_synapses;
    float global_learning_rate;
    float current_attention_mod;
    float current_salience_mod;
    float current_novelty_mod;
    bool bio_async_connected;
} attention_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_focus_events;
    uint64_t total_shift_events;
    uint64_t total_pre_spikes;
    uint64_t total_post_spikes;
    uint64_t ltp_events;
    uint64_t ltd_events;
    uint64_t habituation_events;
    uint64_t novelty_events;
    float avg_weight_change;
    float total_reward;
    float avg_attention_modulation;
} attention_plasticity_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Weight change callback
 */
typedef void (*attention_weight_change_cb)(
    uint32_t synapse_id,
    uint32_t head_idx,
    float old_weight,
    float new_weight,
    attention_learn_event_t event_type,
    void* user_data
);

/**
 * @brief Attention shift callback
 */
typedef void (*attention_shift_cb)(
    uint32_t old_head,
    uint32_t new_head,
    float shift_strength,
    void* user_data
);

//=============================================================================
// Main Bridge Structure
//=============================================================================

/** @brief Forward declaration */
typedef struct attention_plasticity_bridge attention_plasticity_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
attention_plasticity_config_t attention_plasticity_config_default(void);

/**
 * @brief Create attention-plasticity bridge
 */
attention_plasticity_bridge_t* attention_plasticity_create(
    const attention_plasticity_config_t* config
);

/**
 * @brief Destroy attention-plasticity bridge
 */
void attention_plasticity_destroy(attention_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int attention_plasticity_reset(attention_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register synapse for attention-based learning
 */
int attention_plasticity_register_synapse(
    attention_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    attention_synapse_type_t type,
    uint32_t head_idx,
    float initial_weight
);

/**
 * @brief Unregister synapse
 */
int attention_plasticity_unregister_synapse(
    attention_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 */
int attention_plasticity_get_synapse(
    attention_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    attention_plasticity_synapse_t* synapse
);

//=============================================================================
// Event Recording (Attention --> Plasticity)
//=============================================================================

/**
 * @brief Record attention focus event
 */
int attention_plasticity_focus(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx,
    float attention_weight,
    uint64_t timestamp_us
);

/**
 * @brief Record attention shift event
 */
int attention_plasticity_shift(
    attention_plasticity_bridge_t* bridge,
    uint32_t from_head,
    uint32_t to_head,
    float shift_strength,
    uint64_t timestamp_us
);

/**
 * @brief Record salience observation
 */
int attention_plasticity_salience(
    attention_plasticity_bridge_t* bridge,
    const float* salience_map,
    uint32_t sequence_length,
    uint64_t timestamp_us
);

/**
 * @brief Record reward/punishment signal
 */
int attention_plasticity_reward(
    attention_plasticity_bridge_t* bridge,
    float reward,  /* Positive = reward, negative = punishment */
    uint64_t timestamp_us
);

/**
 * @brief Record habituation trial
 */
int attention_plasticity_habituation_trial(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx,
    uint64_t timestamp_us
);

/**
 * @brief Record novelty detection
 */
int attention_plasticity_novelty(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx,
    float novelty_score,
    uint64_t timestamp_us
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update all plasticity mechanisms
 */
int attention_plasticity_update(
    attention_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Trigger memory consolidation
 */
int attention_plasticity_consolidate(
    attention_plasticity_bridge_t* bridge
);

//=============================================================================
// Query Functions (Plasticity --> Attention)
//=============================================================================

/**
 * @brief Get learned attention bias for head
 */
int attention_plasticity_get_bias(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx,
    float* bias
);

/**
 * @brief Get attention modulation for all heads
 */
int attention_plasticity_get_modulation(
    attention_plasticity_bridge_t* bridge,
    float* modulation,
    uint32_t num_heads
);

/**
 * @brief Get habituation level for head
 */
float attention_plasticity_get_habituation(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx
);

/**
 * @brief Get novelty score for head
 */
float attention_plasticity_get_novelty_score(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx
);

/**
 * @brief Get learned attention sensitivity
 */
float attention_plasticity_get_sensitivity(
    attention_plasticity_bridge_t* bridge,
    uint32_t head_idx
);

//=============================================================================
// State and Statistics
//=============================================================================

/**
 * @brief Get bridge state
 */
int attention_plasticity_get_state(
    const attention_plasticity_bridge_t* bridge,
    attention_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int attention_plasticity_get_stats(
    const attention_plasticity_bridge_t* bridge,
    attention_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void attention_plasticity_reset_stats(attention_plasticity_bridge_t* bridge);

//=============================================================================
// Callbacks
//=============================================================================

/**
 * @brief Register weight change callback
 */
int attention_plasticity_set_weight_callback(
    attention_plasticity_bridge_t* bridge,
    attention_weight_change_cb callback,
    void* user_data
);

/**
 * @brief Register attention shift callback
 */
int attention_plasticity_set_shift_callback(
    attention_plasticity_bridge_t* bridge,
    attention_shift_cb callback,
    void* user_data
);

//=============================================================================
// Modulation Functions
//=============================================================================

/**
 * @brief Set attention modulation
 */
int attention_plasticity_set_attention_modulation(
    attention_plasticity_bridge_t* bridge,
    float attention_level
);

/**
 * @brief Set salience modulation
 */
int attention_plasticity_set_salience_modulation(
    attention_plasticity_bridge_t* bridge,
    float salience_level
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int attention_plasticity_connect_bio_async(attention_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int attention_plasticity_disconnect_bio_async(attention_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool attention_plasticity_is_bio_async_connected(const attention_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ATTENTION_PLASTICITY_BRIDGE_H */
