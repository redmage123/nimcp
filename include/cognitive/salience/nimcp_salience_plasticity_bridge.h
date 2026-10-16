/**
 * @file nimcp_salience_plasticity_bridge.h
 * @brief Salience - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between salience system and plasticity mechanisms
 * WHY:  Enable learning-based attention improvement through STDP
 * HOW:  Track attention events for spike-timing dependent synaptic changes
 *
 * THEORETICAL FOUNDATIONS:
 * - Gottlieb (2012): Attention, learning, and the value of information
 * - Anderson (2011): Value-driven attentional capture
 * - Awh et al. (2012): Top-down versus bottom-up attention
 *
 * BIOLOGICAL BASIS:
 * - Pulvinar-cortical loops: Attention gating plasticity
 * - Superior colliculus: Salience map refinement through experience
 * - Norepinephrine: Modulates attention plasticity via LC
 * - Reward prediction: Shapes attention priorities through dopamine
 */

#ifndef NIMCP_SALIENCE_PLASTICITY_BRIDGE_H
#define NIMCP_SALIENCE_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define SALIENCE_PLASTICITY_MAX_SYNAPSES    512
#define SALIENCE_PLASTICITY_MAX_FEATURES    64
#define SALIENCE_PLASTICITY_STDP_WINDOW     50.0f
#define BIO_MODULE_SALIENCE_PLASTICITY      0x0A10

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    SALIENCE_SYNAPSE_NOVELTY = 0,
    SALIENCE_SYNAPSE_SURPRISE,
    SALIENCE_SYNAPSE_URGENCY,
    SALIENCE_SYNAPSE_VALUE,
    SALIENCE_SYNAPSE_HABITUATION
} salience_synapse_type_t;

typedef enum {
    SALIENCE_LEARN_ATTENTION_CORRECT = 0,
    SALIENCE_LEARN_ATTENTION_INCORRECT,
    SALIENCE_LEARN_NOVELTY_REWARDED,
    SALIENCE_LEARN_HABITUATION,
    SALIENCE_LEARN_DISHABITUATION,
    SALIENCE_LEARN_VALUE_UPDATE,
    SALIENCE_LEARN_REWARD
} salience_learn_event_t;

typedef enum {
    SALIENCE_PLASTICITY_STATE_IDLE = 0,
    SALIENCE_PLASTICITY_STATE_ATTENDING,
    SALIENCE_PLASTICITY_STATE_UPDATING,
    SALIENCE_PLASTICITY_STATE_CONSOLIDATING,
    SALIENCE_PLASTICITY_STATE_DISABLED
} salience_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    float stdp_ltp_window_ms;
    float stdp_ltd_window_ms;
    float stdp_a_plus;
    float stdp_a_minus;
    float stdp_tau_plus;
    float stdp_tau_minus;

    bool enable_habituation;
    float habituation_rate;
    float dishabituation_boost;

    bool enable_value_learning;
    float value_ltp_gain;
    float value_decay_rate;

    bool enable_novelty_seeking;
    float novelty_bonus;
    float exploration_drive;

    bool enable_bcm;
    float bcm_threshold_tau;
    float bcm_activity_tau;

    bool enable_homeostatic;
    float target_attention_level;
    float homeostatic_tau_ms;

    bool enable_eligibility;
    float eligibility_decay;
    float reward_modulation_gain;

    float weight_min;
    float weight_max;
    float initial_weight;

    bool enable_bio_async;
} salience_plasticity_config_t;

//=============================================================================
// Synapse Structure
//=============================================================================

typedef struct {
    uint32_t synapse_id;
    salience_synapse_type_t type;
    uint32_t feature_index;

    float weight;
    float initial_weight;

    uint64_t last_pre_spike_us;
    uint64_t last_post_spike_us;

    float eligibility_trace;

    float bcm_threshold;
    float avg_activity;

    float habituation_level;
    float value_estimate;

    uint32_t attention_count;
    uint32_t reward_count;
} salience_plasticity_synapse_t;

//=============================================================================
// Per-Feature Learning State
//=============================================================================

typedef struct {
    uint32_t feature_index;
    float learned_salience;
    float habituation_level;
    float value_estimate;
    uint32_t exposure_count;
    uint32_t reward_count;
    uint64_t last_exposure_time_us;
} salience_feature_learning_t;

//=============================================================================
// State Structures
//=============================================================================

typedef struct {
    salience_plasticity_state_t state;
    uint32_t registered_synapses;
    uint32_t tracked_features;
    float global_learning_rate;
    float current_attention_level;
    bool bio_async_connected;
} salience_plasticity_bridge_state_t;

typedef struct {
    uint64_t total_attention_events;
    uint64_t correct_attention;
    uint64_t incorrect_attention;
    uint64_t habituation_events;
    uint64_t dishabituation_events;
    uint64_t ltp_events;
    uint64_t ltd_events;
    float avg_weight_change;
    float mean_habituation;
    float total_reward;
} salience_plasticity_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

typedef void (*salience_weight_change_cb)(
    uint32_t synapse_id,
    uint32_t feature_index,
    float old_weight,
    float new_weight,
    salience_learn_event_t event_type,
    void* user_data
);

typedef void (*salience_habituation_cb)(
    uint32_t feature_index,
    float old_habituation,
    float new_habituation,
    void* user_data
);

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct salience_plasticity_bridge salience_plasticity_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

salience_plasticity_config_t salience_plasticity_config_default(void);

salience_plasticity_bridge_t* salience_plasticity_create(
    const salience_plasticity_config_t* config
);

void salience_plasticity_destroy(salience_plasticity_bridge_t* bridge);

int salience_plasticity_reset(salience_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

int salience_plasticity_register_synapse(
    salience_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    salience_synapse_type_t type,
    uint32_t feature_index,
    float initial_weight
);

int salience_plasticity_unregister_synapse(
    salience_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

int salience_plasticity_get_synapse(
    salience_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    salience_plasticity_synapse_t* synapse
);

//=============================================================================
// Event Recording
//=============================================================================

int salience_plasticity_attention_event(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    float attention_strength,
    uint64_t timestamp_us
);

int salience_plasticity_attention_feedback(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    bool was_correct,
    uint64_t timestamp_us
);

int salience_plasticity_feature_exposure(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    float intensity,
    uint64_t timestamp_us
);

int salience_plasticity_novelty_response(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    float novelty_level,
    bool rewarded,
    uint64_t timestamp_us
);

int salience_plasticity_reward(
    salience_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
);

//=============================================================================
// Update Functions
//=============================================================================

int salience_plasticity_update(
    salience_plasticity_bridge_t* bridge,
    float dt_ms
);

int salience_plasticity_consolidate(salience_plasticity_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

float salience_plasticity_get_learned_salience(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index
);

float salience_plasticity_get_habituation(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index
);

float salience_plasticity_get_value_estimate(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index
);

int salience_plasticity_get_feature_learning(
    salience_plasticity_bridge_t* bridge,
    uint32_t feature_index,
    salience_feature_learning_t* learning
);

//=============================================================================
// State and Statistics
//=============================================================================

int salience_plasticity_get_state(
    const salience_plasticity_bridge_t* bridge,
    salience_plasticity_bridge_state_t* state
);

int salience_plasticity_get_stats(
    const salience_plasticity_bridge_t* bridge,
    salience_plasticity_stats_t* stats
);

void salience_plasticity_reset_stats(salience_plasticity_bridge_t* bridge);

//=============================================================================
// Callbacks
//=============================================================================

int salience_plasticity_set_weight_callback(
    salience_plasticity_bridge_t* bridge,
    salience_weight_change_cb callback,
    void* user_data
);

int salience_plasticity_set_habituation_callback(
    salience_plasticity_bridge_t* bridge,
    salience_habituation_cb callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

int salience_plasticity_connect_bio_async(salience_plasticity_bridge_t* bridge);

int salience_plasticity_disconnect_bio_async(salience_plasticity_bridge_t* bridge);

bool salience_plasticity_is_bio_async_connected(const salience_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SALIENCE_PLASTICITY_BRIDGE_H */
