/**
 * @file nimcp_epistemic_plasticity_bridge.h
 * @brief Epistemic - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between epistemic filtering and plasticity mechanisms
 * WHY:  Enable learning-based improvement of belief evaluation through STDP
 * HOW:  Track belief updates for spike-timing dependent synaptic changes
 *
 * THEORETICAL FOUNDATIONS:
 * - Tenenbaum et al. (2006): Theory-based Bayesian models of induction
 * - Griffiths et al. (2008): Bayesian models of cognition
 * - McClelland (2013): Integrating probabilistic and neural models
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal-hippocampal interactions support belief updating
 * - Dopamine modulates learning from prediction errors
 * - Synaptic plasticity adjusts evidence integration weights
 * - Source reliability learned through experience
 */

#ifndef NIMCP_EPISTEMIC_PLASTICITY_BRIDGE_H
#define NIMCP_EPISTEMIC_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define EPISTEMIC_PLASTICITY_MAX_SYNAPSES    512
#define EPISTEMIC_PLASTICITY_MAX_SOURCES     32
#define EPISTEMIC_PLASTICITY_STDP_WINDOW     50.0f
#define BIO_MODULE_EPISTEMIC_PLASTICITY      0x0E10

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    EPISTEMIC_SYNAPSE_EVIDENCE_INTEGRATION = 0,
    EPISTEMIC_SYNAPSE_SOURCE_RELIABILITY,
    EPISTEMIC_SYNAPSE_BIAS_DETECTION,
    EPISTEMIC_SYNAPSE_PRIOR_UPDATE,
    EPISTEMIC_SYNAPSE_CONSPIRACY_DETECTION
} epistemic_synapse_type_t;

typedef enum {
    EPISTEMIC_LEARN_EVIDENCE_UPDATE = 0,
    EPISTEMIC_LEARN_SOURCE_CORRECT,
    EPISTEMIC_LEARN_SOURCE_INCORRECT,
    EPISTEMIC_LEARN_BIAS_DETECTED,
    EPISTEMIC_LEARN_CONSPIRACY_DETECTED,
    EPISTEMIC_LEARN_BELIEF_REVISION,
    EPISTEMIC_LEARN_REWARD
} epistemic_learn_event_t;

typedef enum {
    EPISTEMIC_PLASTICITY_STATE_IDLE = 0,
    EPISTEMIC_PLASTICITY_STATE_EVALUATING,
    EPISTEMIC_PLASTICITY_STATE_UPDATING,
    EPISTEMIC_PLASTICITY_STATE_CONSOLIDATING,
    EPISTEMIC_PLASTICITY_STATE_DISABLED
} epistemic_plasticity_state_t;

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

    bool enable_source_learning;
    float source_correct_ltp;
    float source_incorrect_ltd;

    bool enable_bias_learning;
    float bias_detection_ltp;
    float bias_correction_reward;

    bool enable_evidence_weighting;
    float evidence_quality_gain;
    float evidence_recency_decay;

    bool enable_bcm;
    float bcm_threshold_tau;
    float bcm_activity_tau;

    bool enable_homeostatic;
    float target_epistemic_quality;
    float homeostatic_tau_ms;

    bool enable_eligibility;
    float eligibility_decay;
    float reward_modulation_gain;

    float weight_min;
    float weight_max;
    float initial_weight;

    bool enable_bio_async;
} epistemic_plasticity_config_t;

//=============================================================================
// Synapse Structure
//=============================================================================

typedef struct {
    uint32_t synapse_id;
    epistemic_synapse_type_t type;
    uint32_t source_id;

    float weight;
    float initial_weight;

    uint64_t last_pre_spike_us;
    uint64_t last_post_spike_us;

    float eligibility_trace;

    float bcm_threshold;
    float avg_activity;

    float consolidation_level;
    uint32_t correct_count;
    uint32_t incorrect_count;
} epistemic_plasticity_synapse_t;

//=============================================================================
// Per-Source State
//=============================================================================

typedef struct {
    uint32_t source_id;
    float learned_reliability;
    float confidence;
    uint32_t total_evaluations;
    uint32_t correct_evaluations;
    uint64_t last_evaluation_time_us;
} epistemic_source_learning_t;

//=============================================================================
// State Structures
//=============================================================================

typedef struct {
    epistemic_plasticity_state_t state;
    uint32_t registered_synapses;
    uint32_t tracked_sources;
    float global_learning_rate;
    float current_epistemic_quality;
    bool bio_async_connected;
} epistemic_plasticity_bridge_state_t;

typedef struct {
    uint64_t total_evaluations;
    uint64_t source_updates;
    uint64_t bias_detections;
    uint64_t belief_revisions;
    uint64_t ltp_events;
    uint64_t ltd_events;
    float avg_weight_change;
    float avg_source_reliability;
    float total_reward;
} epistemic_plasticity_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

typedef void (*epistemic_weight_change_cb)(
    uint32_t synapse_id,
    uint32_t source_id,
    float old_weight,
    float new_weight,
    epistemic_learn_event_t event_type,
    void* user_data
);

typedef void (*epistemic_source_update_cb)(
    uint32_t source_id,
    float old_reliability,
    float new_reliability,
    void* user_data
);

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct epistemic_plasticity_bridge epistemic_plasticity_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

epistemic_plasticity_config_t epistemic_plasticity_config_default(void);

epistemic_plasticity_bridge_t* epistemic_plasticity_create(
    const epistemic_plasticity_config_t* config
);

void epistemic_plasticity_destroy(epistemic_plasticity_bridge_t* bridge);

int epistemic_plasticity_reset(epistemic_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

int epistemic_plasticity_register_synapse(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    epistemic_synapse_type_t type,
    uint32_t source_id,
    float initial_weight
);

int epistemic_plasticity_unregister_synapse(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

int epistemic_plasticity_get_synapse(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    epistemic_plasticity_synapse_t* synapse
);

//=============================================================================
// Event Recording
//=============================================================================

int epistemic_plasticity_evidence_update(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id,
    float evidence_quality,
    uint64_t timestamp_us
);

int epistemic_plasticity_source_feedback(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id,
    bool was_correct,
    uint64_t timestamp_us
);

int epistemic_plasticity_bias_detected(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    float confidence,
    uint64_t timestamp_us
);

int epistemic_plasticity_belief_revision(
    epistemic_plasticity_bridge_t* bridge,
    float prior,
    float posterior,
    uint64_t timestamp_us
);

int epistemic_plasticity_reward(
    epistemic_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
);

//=============================================================================
// Update Functions
//=============================================================================

int epistemic_plasticity_update(
    epistemic_plasticity_bridge_t* bridge,
    float dt_ms
);

int epistemic_plasticity_consolidate(epistemic_plasticity_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

float epistemic_plasticity_get_source_reliability(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id
);

float epistemic_plasticity_get_evidence_weight(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id
);

float epistemic_plasticity_get_bias_sensitivity(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t bias_type
);

int epistemic_plasticity_get_source_learning(
    epistemic_plasticity_bridge_t* bridge,
    uint32_t source_id,
    epistemic_source_learning_t* learning
);

//=============================================================================
// State and Statistics
//=============================================================================

int epistemic_plasticity_get_state(
    const epistemic_plasticity_bridge_t* bridge,
    epistemic_plasticity_bridge_state_t* state
);

int epistemic_plasticity_get_stats(
    const epistemic_plasticity_bridge_t* bridge,
    epistemic_plasticity_stats_t* stats
);

void epistemic_plasticity_reset_stats(epistemic_plasticity_bridge_t* bridge);

//=============================================================================
// Callbacks
//=============================================================================

int epistemic_plasticity_set_weight_callback(
    epistemic_plasticity_bridge_t* bridge,
    epistemic_weight_change_cb callback,
    void* user_data
);

int epistemic_plasticity_set_source_callback(
    epistemic_plasticity_bridge_t* bridge,
    epistemic_source_update_cb callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

int epistemic_plasticity_connect_bio_async(epistemic_plasticity_bridge_t* bridge);

int epistemic_plasticity_disconnect_bio_async(epistemic_plasticity_bridge_t* bridge);

bool epistemic_plasticity_is_bio_async_connected(const epistemic_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPISTEMIC_PLASTICITY_BRIDGE_H */
