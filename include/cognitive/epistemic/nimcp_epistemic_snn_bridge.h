/**
 * @file nimcp_epistemic_snn_bridge.h
 * @brief Epistemic - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between epistemic filtering and spiking neural networks
 * WHY:  Enable biologically-plausible belief evaluation through spike-based processing
 * HOW:  Encode evidence quality and bias signals as spike patterns, decode from population activity
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy principle and epistemic foraging
 * - Tenenbaum et al. (2011): Bayesian inference in neural circuits
 * - Pouget et al. (2013): Probabilistic population codes
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex integrates evidence for belief updating
 * - Anterior cingulate cortex monitors uncertainty and conflict
 * - Orbitofrontal cortex tracks source reliability
 * - Spike timing encodes evidence strength and certainty
 */

#ifndef NIMCP_EPISTEMIC_SNN_BRIDGE_H
#define NIMCP_EPISTEMIC_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define EPISTEMIC_SNN_MAX_SOURCES           32
#define EPISTEMIC_SNN_NEURONS_PER_DIM       16
#define EPISTEMIC_SNN_INPUT_DIM             64
#define EPISTEMIC_SNN_HIDDEN_DIM            128
#define BIO_MODULE_EPISTEMIC_SNN            0x0E20

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    EPISTEMIC_SNN_ENCODE_RATE = 0,
    EPISTEMIC_SNN_ENCODE_TEMPORAL,
    EPISTEMIC_SNN_ENCODE_POPULATION,
    EPISTEMIC_SNN_ENCODE_SYNCHRONY
} epistemic_snn_encoding_t;

typedef enum {
    EPISTEMIC_SNN_STATE_IDLE = 0,
    EPISTEMIC_SNN_STATE_ENCODING,
    EPISTEMIC_SNN_STATE_EVALUATING,
    EPISTEMIC_SNN_STATE_SIMULATING,
    EPISTEMIC_SNN_STATE_ERROR
} epistemic_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t max_sources;
    uint32_t neurons_per_dim;
    uint32_t input_dim;
    uint32_t hidden_dim;

    float dt_ms;
    float evidence_gain;
    float uncertainty_gain;
    float bias_detection_threshold;

    epistemic_snn_encoding_t encoding_type;
    bool enable_source_tracking;
    bool enable_bias_detection;
    bool enable_conspiracy_detection;
    bool enable_bio_async;
} epistemic_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

typedef struct {
    float evidence_quality;
    float source_reliability;
    float bias_level;
    float uncertainty;
    float spike_rate;
    uint32_t spike_count;
} epistemic_dimension_state_t;

typedef struct {
    epistemic_snn_state_t state;
    float total_activity;
    float mean_evidence_quality;
    float mean_uncertainty;
    float conspiracy_score;
    uint32_t active_sources;
} epistemic_snn_bridge_state_t;

typedef struct {
    uint64_t total_evaluations;
    uint64_t bias_detections;
    uint64_t conspiracy_detections;
    uint64_t total_spikes;
    float mean_evidence_quality;
    float mean_source_reliability;
} epistemic_snn_stats_t;

//=============================================================================
// Output Structures
//=============================================================================

typedef struct {
    float epistemic_quality;
    float evidence_strength;
    float source_reliability;
    float bias_magnitude;
    float conspiracy_likelihood;
    float uncertainty;
    bool bias_detected;
    bool conspiracy_detected;
} epistemic_snn_output_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct epistemic_snn_bridge epistemic_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

typedef void (*epistemic_snn_spike_callback_t)(
    epistemic_snn_bridge_t* bridge,
    uint32_t source_id,
    float rate,
    void* user_data
);

typedef void (*epistemic_snn_bias_callback_t)(
    epistemic_snn_bridge_t* bridge,
    uint32_t bias_type,
    float confidence,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

epistemic_snn_config_t epistemic_snn_config_default(void);

epistemic_snn_bridge_t* epistemic_snn_create(const epistemic_snn_config_t* config);

void epistemic_snn_destroy(epistemic_snn_bridge_t* bridge);

int epistemic_snn_reset(epistemic_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

int epistemic_snn_encode_evidence(
    epistemic_snn_bridge_t* bridge,
    float evidence_quality,
    float plausibility,
    float source_reliability
);

int epistemic_snn_encode_claim(
    epistemic_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count,
    float prior_probability
);

int epistemic_snn_encode_bias_signals(
    epistemic_snn_bridge_t* bridge,
    const float* bias_magnitudes,
    uint32_t num_biases
);

//=============================================================================
// Simulation Functions
//=============================================================================

int epistemic_snn_simulate(epistemic_snn_bridge_t* bridge, float duration_ms);

int epistemic_snn_step(epistemic_snn_bridge_t* bridge);

int epistemic_snn_forward(
    epistemic_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

int epistemic_snn_decode_assessment(
    epistemic_snn_bridge_t* bridge,
    epistemic_snn_output_t* output
);

float epistemic_snn_get_epistemic_quality(epistemic_snn_bridge_t* bridge);

float epistemic_snn_get_uncertainty(epistemic_snn_bridge_t* bridge);

float epistemic_snn_get_bias_level(epistemic_snn_bridge_t* bridge);

float epistemic_snn_get_conspiracy_score(epistemic_snn_bridge_t* bridge);

//=============================================================================
// Source Tracking Functions
//=============================================================================

int epistemic_snn_register_source(
    epistemic_snn_bridge_t* bridge,
    uint32_t source_id,
    float initial_reliability
);

int epistemic_snn_update_source_reliability(
    epistemic_snn_bridge_t* bridge,
    uint32_t source_id,
    bool was_correct
);

float epistemic_snn_get_source_reliability(
    epistemic_snn_bridge_t* bridge,
    uint32_t source_id
);

//=============================================================================
// State Query Functions
//=============================================================================

int epistemic_snn_get_dimension_state(
    epistemic_snn_bridge_t* bridge,
    uint32_t dim,
    epistemic_dimension_state_t* state
);

int epistemic_snn_get_state(
    epistemic_snn_bridge_t* bridge,
    epistemic_snn_bridge_state_t* state
);

int epistemic_snn_get_stats(epistemic_snn_bridge_t* bridge, epistemic_snn_stats_t* stats);

int epistemic_snn_reset_stats(epistemic_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

int epistemic_snn_register_spike_callback(
    epistemic_snn_bridge_t* bridge,
    epistemic_snn_spike_callback_t callback,
    void* user_data
);

int epistemic_snn_register_bias_callback(
    epistemic_snn_bridge_t* bridge,
    epistemic_snn_bias_callback_t callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

int epistemic_snn_bio_async_connect(epistemic_snn_bridge_t* bridge);

int epistemic_snn_bio_async_disconnect(epistemic_snn_bridge_t* bridge);

bool epistemic_snn_is_bio_async_connected(epistemic_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPISTEMIC_SNN_BRIDGE_H */
