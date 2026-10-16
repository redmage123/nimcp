/**
 * @file nimcp_salience_snn_bridge.h
 * @brief Salience - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between salience system and spiking neural networks
 * WHY:  Enable biologically-plausible attention through spike-based salience processing
 * HOW:  Encode novelty, surprise, and urgency as spike patterns, decode from population activity
 *
 * THEORETICAL FOUNDATIONS:
 * - Itti & Koch (2001): Computational model of visual attention
 * - Friston (2010): Free energy and salience
 * - Knudsen (2007): Fundamental components of attention
 *
 * BIOLOGICAL BASIS:
 * - Superior colliculus: Multimodal salience integration
 * - Parietal cortex: Attention priority maps
 * - Pulvinar nucleus: Salience gating
 * - Locus coeruleus: Norepinephrine modulation of attention
 */

#ifndef NIMCP_SALIENCE_SNN_BRIDGE_H
#define NIMCP_SALIENCE_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define SALIENCE_SNN_MAX_FEATURES       128
#define SALIENCE_SNN_NEURONS_PER_DIM    32
#define SALIENCE_SNN_MODALITY_COUNT     4
#define BIO_MODULE_SALIENCE_SNN         0x0A20

//=============================================================================
// Enumerations
//=============================================================================

typedef enum {
    SALIENCE_SNN_ENCODE_RATE = 0,
    SALIENCE_SNN_ENCODE_TEMPORAL,
    SALIENCE_SNN_ENCODE_POPULATION,
    SALIENCE_SNN_ENCODE_PHASE
} salience_snn_encoding_t;

typedef enum {
    SALIENCE_SNN_CHANNEL_NOVELTY = 0,
    SALIENCE_SNN_CHANNEL_SURPRISE,
    SALIENCE_SNN_CHANNEL_URGENCY,
    SALIENCE_SNN_CHANNEL_INTENSITY,
    SALIENCE_SNN_CHANNEL_COUNT
} salience_snn_channel_t;

typedef enum {
    SALIENCE_SNN_STATE_IDLE = 0,
    SALIENCE_SNN_STATE_ENCODING,
    SALIENCE_SNN_STATE_EVALUATING,
    SALIENCE_SNN_STATE_SIMULATING,
    SALIENCE_SNN_STATE_ERROR
} salience_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

typedef struct {
    uint32_t max_features;
    uint32_t neurons_per_dim;
    uint32_t history_depth;

    float dt_ms;
    float novelty_threshold;
    float surprise_threshold;
    float urgency_threshold;

    float novelty_weight;
    float surprise_weight;
    float urgency_weight;

    salience_snn_encoding_t encoding_type;
    bool enable_multimodal;
    bool enable_history;
    bool enable_prediction;
    bool enable_bio_async;
} salience_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

typedef struct {
    salience_snn_channel_t channel;
    float activation;
    float spike_rate;
    uint32_t spike_count;
    float confidence;
} salience_channel_state_t;

typedef struct {
    salience_snn_state_t state;
    float total_activity;
    float combined_salience;
    float novelty;
    float surprise;
    float urgency;
    uint32_t high_salience_count;
} salience_snn_bridge_state_t;

typedef struct {
    uint64_t total_evaluations;
    uint64_t high_novelty_events;
    uint64_t high_surprise_events;
    uint64_t high_urgency_events;
    uint64_t total_spikes;
    float mean_salience;
    float mean_novelty;
    float mean_surprise;
} salience_snn_stats_t;

//=============================================================================
// Output Structures
//=============================================================================

typedef struct {
    float combined_salience;
    float novelty;
    float surprise;
    float urgency;
    float intensity;
    float confidence;
    bool high_salience;
    salience_snn_channel_t dominant_channel;
} salience_snn_output_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct salience_snn_bridge salience_snn_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

typedef void (*salience_snn_spike_callback_t)(
    salience_snn_bridge_t* bridge,
    salience_snn_channel_t channel,
    float rate,
    void* user_data
);

typedef void (*salience_snn_threshold_callback_t)(
    salience_snn_bridge_t* bridge,
    salience_snn_channel_t channel,
    float value,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

salience_snn_config_t salience_snn_config_default(void);

salience_snn_bridge_t* salience_snn_create(const salience_snn_config_t* config);

void salience_snn_destroy(salience_snn_bridge_t* bridge);

int salience_snn_reset(salience_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

int salience_snn_encode_features(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count
);

int salience_snn_encode_with_prediction(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count,
    const float* prediction,
    uint32_t prediction_count
);

int salience_snn_encode_temporal(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count,
    uint64_t timestamp_us
);

//=============================================================================
// Simulation Functions
//=============================================================================

int salience_snn_simulate(salience_snn_bridge_t* bridge, float duration_ms);

int salience_snn_step(salience_snn_bridge_t* bridge);

int salience_snn_forward(
    salience_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
);

//=============================================================================
// Decoding Functions
//=============================================================================

int salience_snn_decode_salience(
    salience_snn_bridge_t* bridge,
    salience_snn_output_t* output
);

float salience_snn_get_combined_salience(salience_snn_bridge_t* bridge);

float salience_snn_get_novelty(salience_snn_bridge_t* bridge);

float salience_snn_get_surprise(salience_snn_bridge_t* bridge);

float salience_snn_get_urgency(salience_snn_bridge_t* bridge);

salience_snn_channel_t salience_snn_get_dominant_channel(salience_snn_bridge_t* bridge);

//=============================================================================
// History Functions
//=============================================================================

int salience_snn_add_to_history(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count
);

int salience_snn_clear_history(salience_snn_bridge_t* bridge);

float salience_snn_compute_novelty(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count
);

//=============================================================================
// State Query Functions
//=============================================================================

int salience_snn_get_channel_state(
    salience_snn_bridge_t* bridge,
    salience_snn_channel_t channel,
    salience_channel_state_t* state
);

int salience_snn_get_state(
    salience_snn_bridge_t* bridge,
    salience_snn_bridge_state_t* state
);

int salience_snn_get_stats(salience_snn_bridge_t* bridge, salience_snn_stats_t* stats);

int salience_snn_reset_stats(salience_snn_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

int salience_snn_register_spike_callback(
    salience_snn_bridge_t* bridge,
    salience_snn_spike_callback_t callback,
    void* user_data
);

int salience_snn_register_threshold_callback(
    salience_snn_bridge_t* bridge,
    salience_snn_threshold_callback_t callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

int salience_snn_bio_async_connect(salience_snn_bridge_t* bridge);

int salience_snn_bio_async_disconnect(salience_snn_bridge_t* bridge);

bool salience_snn_is_bio_async_connected(salience_snn_bridge_t* bridge);

//=============================================================================
// Weight Configuration
//=============================================================================

int salience_snn_set_weights(
    salience_snn_bridge_t* bridge,
    float novelty_weight,
    float surprise_weight,
    float urgency_weight
);

int salience_snn_set_thresholds(
    salience_snn_bridge_t* bridge,
    float novelty_threshold,
    float surprise_threshold,
    float urgency_threshold
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SALIENCE_SNN_BRIDGE_H */
