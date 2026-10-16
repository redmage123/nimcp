/**
 * @file nimcp_bias_snn_bridge.h
 * @brief Cognitive Bias - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between cognitive bias detection and spiking neural networks
 * WHY:  Enable biologically-plausible bias detection through spike-based processing
 * HOW:  Encode bias signals as spike patterns, decode bias detection from population activity
 *
 * THEORETICAL FOUNDATIONS:
 * - Kahneman (2011): System 1/System 2 - biases emerge from fast pattern matching
 * - Tversky & Kahneman (1974): Heuristics and biases in judgment
 * - Friston (2009): Biases as systematic prediction errors in FEP framework
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Executive control over bias susceptibility
 * - Anterior cingulate cortex: Conflict detection between biased and unbiased responses
 * - Ventromedial PFC: Value-based biases (optimism, framing effects)
 * - Spike timing encodes bias strength and confidence
 */

#ifndef NIMCP_BIAS_SNN_BRIDGE_H
#define NIMCP_BIAS_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIAS_SNN_MAX_BIAS_TYPES         16
#define BIAS_SNN_NEURONS_PER_TYPE       32
#define BIAS_SNN_INPUT_DIM              64
#define BIAS_SNN_HIDDEN_DIM             128
#define BIO_MODULE_BIAS_SNN             0x0B20

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    BIAS_SNN_TYPE_CONFIRMATION = 0,
    BIAS_SNN_TYPE_AVAILABILITY,
    BIAS_SNN_TYPE_ANCHORING,
    BIAS_SNN_TYPE_RECENCY,
    BIAS_SNN_TYPE_OPTIMISM,
    BIAS_SNN_TYPE_PESSIMISM,
    BIAS_SNN_TYPE_FRAMING,
    BIAS_SNN_TYPE_SUNK_COST,
    BIAS_SNN_TYPE_COUNT
} bias_snn_type_t;

typedef enum {
    BIAS_SNN_ENCODE_RATE = 0,
    BIAS_SNN_ENCODE_TEMPORAL,
    BIAS_SNN_ENCODE_POPULATION,
    BIAS_SNN_ENCODE_SYNCHRONY
} bias_snn_encoding_t;

typedef enum {
    BIAS_SNN_STATE_IDLE = 0,
    BIAS_SNN_STATE_ENCODING,
    BIAS_SNN_STATE_DETECTING,
    BIAS_SNN_STATE_SIMULATING,
    BIAS_SNN_STATE_ERROR
} bias_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t max_bias_types;
    uint32_t neurons_per_type;
    uint32_t input_dim;
    uint32_t hidden_dim;

    float dt_ms;
    float bias_detection_threshold;
    float conflict_threshold;
    float baseline_activation;

    bias_snn_encoding_t encoding_type;
    bool enable_conflict_detection;
    bool enable_metacognitive_monitoring;
    bool enable_bio_async;
} bias_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

typedef struct {
    bias_snn_type_t type;
    float activation;
    float confidence;
    float conflict_level;
    uint32_t spike_count;
    float spike_rate;
} bias_type_state_t;

typedef struct {
    bias_snn_state_t state;
    float total_activity;
    float mean_bias_level;
    float max_bias_level;
    bias_snn_type_t dominant_bias;
    float metacognitive_awareness;
    uint32_t active_biases;
} bias_snn_bridge_state_t;

typedef struct {
    uint64_t total_detections;
    uint64_t confirmation_detections;
    uint64_t availability_detections;
    uint64_t anchoring_detections;
    uint64_t total_spikes;
    float mean_bias_level;
    float mean_conflict;
} bias_snn_stats_t;

//=============================================================================
// Output Structures
//=============================================================================

typedef struct {
    float bias_magnitudes[BIAS_SNN_TYPE_COUNT];
    float overall_bias_level;
    float conflict_level;
    float metacognitive_awareness;
    bias_snn_type_t dominant_bias;
    bool bias_detected;
    float detection_confidence;
} bias_snn_output_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct bias_snn_bridge bias_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

typedef void (*bias_snn_detection_callback_t)(
    bias_snn_bridge_t* bridge,
    bias_snn_type_t bias_type,
    float magnitude,
    float confidence,
    void* user_data
);

typedef void (*bias_snn_conflict_callback_t)(
    bias_snn_bridge_t* bridge,
    float conflict_level,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

bias_snn_config_t bias_snn_config_default(void);

bias_snn_bridge_t* bias_snn_create(const bias_snn_config_t* config);

void bias_snn_destroy(bias_snn_bridge_t* bridge);

int bias_snn_reset(bias_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

int bias_snn_encode_evidence(
    bias_snn_bridge_t* bridge,
    const float* evidence,
    uint32_t evidence_count,
    float prior_belief
);

int bias_snn_encode_decision_context(
    bias_snn_bridge_t* bridge,
    float anchor_value,
    float recent_evidence_weight,
    float emotional_valence
);

int bias_snn_encode_prediction_error(
    bias_snn_bridge_t* bridge,
    float prediction_error,
    float prediction_confidence
);

//=============================================================================
// Simulation Functions
//=============================================================================

int bias_snn_simulate(bias_snn_bridge_t* bridge, float duration_ms);

int bias_snn_step(bias_snn_bridge_t* bridge);

int bias_snn_forward(
    bias_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Detection Functions
//=============================================================================

int bias_snn_detect_biases(
    bias_snn_bridge_t* bridge,
    bias_snn_output_t* output
);

float bias_snn_get_bias_level(bias_snn_bridge_t* bridge, bias_snn_type_t type);

float bias_snn_get_overall_bias(bias_snn_bridge_t* bridge);

float bias_snn_get_conflict_level(bias_snn_bridge_t* bridge);

bias_snn_type_t bias_snn_get_dominant_bias(bias_snn_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

int bias_snn_get_type_state(
    bias_snn_bridge_t* bridge,
    bias_snn_type_t type,
    bias_type_state_t* state
);

int bias_snn_get_state(
    bias_snn_bridge_t* bridge,
    bias_snn_bridge_state_t* state
);

int bias_snn_get_stats(bias_snn_bridge_t* bridge, bias_snn_stats_t* stats);

int bias_snn_reset_stats(bias_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

int bias_snn_register_detection_callback(
    bias_snn_bridge_t* bridge,
    bias_snn_detection_callback_t callback,
    void* user_data
);

int bias_snn_register_conflict_callback(
    bias_snn_bridge_t* bridge,
    bias_snn_conflict_callback_t callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

int bias_snn_bio_async_connect(bias_snn_bridge_t* bridge);

int bias_snn_bio_async_disconnect(bias_snn_bridge_t* bridge);

bool bias_snn_is_bio_async_connected(bias_snn_bridge_t* bridge);

//=============================================================================
// Utility Functions
//=============================================================================

const char* bias_snn_type_name(bias_snn_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIAS_SNN_BRIDGE_H */
