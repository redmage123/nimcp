/**
 * @file nimcp_bias_plasticity_bridge.h
 * @brief Cognitive Bias - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between cognitive bias system and plasticity mechanisms
 * WHY:  Enable learning-based bias recognition and metacognitive improvement through STDP
 * HOW:  Track bias detection events for spike-timing dependent synaptic changes
 *
 * THEORETICAL FOUNDATIONS:
 * - Lilienfeld et al. (2009): Debiasing through training
 * - Morewedge et al. (2015): Debiasing training using serious games
 * - Stanovich (2011): Rational thinking and cognitive reflection
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal-striatal circuits learn bias detection patterns
 * - Anterior cingulate strengthens conflict monitoring
 * - Synaptic plasticity improves metacognitive awareness over time
 * - Repeated exposure to biased decisions triggers corrective learning
 */

#ifndef NIMCP_BIAS_PLASTICITY_BRIDGE_H
#define NIMCP_BIAS_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIAS_PLASTICITY_MAX_SYNAPSES    512
#define BIAS_PLASTICITY_MAX_TYPES       16
#define BIAS_PLASTICITY_STDP_WINDOW     50.0f
#define BIO_MODULE_BIAS_PLASTICITY      0x0B10

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    BIAS_SYNAPSE_DETECTION = 0,
    BIAS_SYNAPSE_CONFLICT_MONITOR,
    BIAS_SYNAPSE_METACOGNITIVE,
    BIAS_SYNAPSE_CORRECTION,
    BIAS_SYNAPSE_AWARENESS
} bias_synapse_type_t;

typedef enum {
    BIAS_LEARN_DETECTION = 0,
    BIAS_LEARN_CORRECTION,
    BIAS_LEARN_FALSE_POSITIVE,
    BIAS_LEARN_FALSE_NEGATIVE,
    BIAS_LEARN_CONFLICT_RESOLVED,
    BIAS_LEARN_METACOGNITIVE_INSIGHT,
    BIAS_LEARN_REWARD
} bias_learn_event_t;

typedef enum {
    BIAS_PLASTICITY_STATE_IDLE = 0,
    BIAS_PLASTICITY_STATE_DETECTING,
    BIAS_PLASTICITY_STATE_UPDATING,
    BIAS_PLASTICITY_STATE_CONSOLIDATING,
    BIAS_PLASTICITY_STATE_DISABLED
} bias_plasticity_state_t;

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

    bool enable_detection_learning;
    float detection_correct_ltp;
    float detection_incorrect_ltd;

    bool enable_conflict_learning;
    float conflict_resolution_ltp;
    float conflict_persistence_ltd;

    bool enable_metacognitive_learning;
    float metacognitive_insight_gain;
    float awareness_growth_rate;

    bool enable_bcm;
    float bcm_threshold_tau;
    float bcm_activity_tau;

    bool enable_homeostatic;
    float target_detection_accuracy;
    float homeostatic_tau_ms;

    bool enable_eligibility;
    float eligibility_decay;
    float reward_modulation_gain;

    float weight_min;
    float weight_max;
    float initial_weight;

    bool enable_bio_async;
} bias_plasticity_config_t;

//=============================================================================
// Synapse Structure
//=============================================================================

typedef struct {
    uint32_t synapse_id;
    bias_synapse_type_t type;
    uint32_t bias_type;

    float weight;
    float initial_weight;

    uint64_t last_pre_spike_us;
    uint64_t last_post_spike_us;

    float eligibility_trace;

    float bcm_threshold;
    float avg_activity;

    float consolidation_level;
    uint32_t correct_detections;
    uint32_t false_positives;
    uint32_t false_negatives;
} bias_plasticity_synapse_t;

//=============================================================================
// Per-Bias Type Learning State
//=============================================================================

typedef struct {
    uint32_t bias_type;
    float detection_sensitivity;
    float correction_efficiency;
    float metacognitive_awareness;
    uint32_t total_encounters;
    uint32_t successful_corrections;
    uint64_t last_encounter_time_us;
} bias_type_learning_t;

//=============================================================================
// State Structures
//=============================================================================

typedef struct {
    bias_plasticity_state_t state;
    uint32_t registered_synapses;
    uint32_t tracked_bias_types;
    float global_learning_rate;
    float overall_metacognitive_awareness;
    bool bio_async_connected;
} bias_plasticity_bridge_state_t;

typedef struct {
    uint64_t total_detections;
    uint64_t correct_detections;
    uint64_t false_positives;
    uint64_t false_negatives;
    uint64_t ltp_events;
    uint64_t ltd_events;
    float avg_weight_change;
    float mean_detection_accuracy;
    float total_reward;
} bias_plasticity_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

typedef void (*bias_weight_change_cb)(
    uint32_t synapse_id,
    uint32_t bias_type,
    float old_weight,
    float new_weight,
    bias_learn_event_t event_type,
    void* user_data
);

typedef void (*bias_metacognitive_cb)(
    uint32_t bias_type,
    float old_awareness,
    float new_awareness,
    void* user_data
);

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct bias_plasticity_bridge bias_plasticity_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

bias_plasticity_config_t bias_plasticity_config_default(void);

bias_plasticity_bridge_t* bias_plasticity_create(
    const bias_plasticity_config_t* config
);

void bias_plasticity_destroy(bias_plasticity_bridge_t* bridge);

int bias_plasticity_reset(bias_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

int bias_plasticity_register_synapse(
    bias_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bias_synapse_type_t type,
    uint32_t bias_type,
    float initial_weight
);

int bias_plasticity_unregister_synapse(
    bias_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

int bias_plasticity_get_synapse(
    bias_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bias_plasticity_synapse_t* synapse
);

//=============================================================================
// Event Recording
//=============================================================================

int bias_plasticity_bias_detected(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    float confidence,
    uint64_t timestamp_us
);

int bias_plasticity_detection_feedback(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    bool was_correct,
    uint64_t timestamp_us
);

int bias_plasticity_conflict_resolved(
    bias_plasticity_bridge_t* bridge,
    float conflict_level,
    bool resolution_correct,
    uint64_t timestamp_us
);

int bias_plasticity_metacognitive_insight(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    float insight_magnitude,
    uint64_t timestamp_us
);

int bias_plasticity_reward(
    bias_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
);

//=============================================================================
// Update Functions
//=============================================================================

int bias_plasticity_update(
    bias_plasticity_bridge_t* bridge,
    float dt_ms
);

int bias_plasticity_consolidate(bias_plasticity_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

float bias_plasticity_get_detection_sensitivity(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type
);

float bias_plasticity_get_correction_efficiency(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type
);

float bias_plasticity_get_metacognitive_awareness(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type
);

int bias_plasticity_get_type_learning(
    bias_plasticity_bridge_t* bridge,
    uint32_t bias_type,
    bias_type_learning_t* learning
);

//=============================================================================
// State and Statistics
//=============================================================================

int bias_plasticity_get_state(
    const bias_plasticity_bridge_t* bridge,
    bias_plasticity_bridge_state_t* state
);

int bias_plasticity_get_stats(
    const bias_plasticity_bridge_t* bridge,
    bias_plasticity_stats_t* stats
);

void bias_plasticity_reset_stats(bias_plasticity_bridge_t* bridge);

//=============================================================================
// Callbacks
//=============================================================================

int bias_plasticity_set_weight_callback(
    bias_plasticity_bridge_t* bridge,
    bias_weight_change_cb callback,
    void* user_data
);

int bias_plasticity_set_metacognitive_callback(
    bias_plasticity_bridge_t* bridge,
    bias_metacognitive_cb callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

int bias_plasticity_connect_bio_async(bias_plasticity_bridge_t* bridge);

int bias_plasticity_disconnect_bio_async(bias_plasticity_bridge_t* bridge);

bool bias_plasticity_is_bio_async_connected(const bias_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIAS_PLASTICITY_BRIDGE_H */
