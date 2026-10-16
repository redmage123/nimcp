/**
 * @file nimcp_cochlea_audio_cortex_bridge.c
 * @brief Cochlea-Audio Cortex (A1) integration implementation
 *
 * WHAT: Connect cochlear/thalamic output to primary auditory cortex
 * WHY:  Enable cortical auditory perception and feature extraction
 * HOW:  Simulate tonotopically-organized cortical columns with predictive coding
 *
 * BIOLOGICAL NOTES:
 * - Layer 4 receives thalamic input (MGN)
 * - Layers 2/3 provide lateral integration
 * - Layer 5 projects back to subcortical structures
 * - Layer 6 provides modulatory feedback to thalamus
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "perception/bridges/nimcp_cochlea_audio_cortex_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cochlea_audio_cortex_bridge)

#define LOG_MODULE "COCHLEA_AUDIO_CORTEX_BRIDGE"

//=============================================================================
// Internal Structure
//=============================================================================

/** Bridge internal structure */
struct cochlea_audio_cortex_bridge {
    bridge_base_t base;                         /**< MUST be first */
    cochlea_t* cochlea;                         /**< Cochlea instance */
    cochlea_thalamic_bridge_t* thalamic_bridge;  /**< Thalamic bridge */
    audio_cortex_t* audio_cortex;               /**< Audio cortex instance */
    cochlea_audio_cortex_config_t config;       /**< Configuration */

    /* Cortical columns */
    cortical_column_output_t* columns;          /**< Per-column output [num_columns] */
    uint32_t num_columns;

    /* Tonotopic map */
    float* tonotopic_map;                       /**< Frequency activation map */

    /* Feature vectors */
    float* spectral_features;                   /**< Spectral features */
    float* temporal_features;                   /**< Temporal features */

    /* Predictive coding */
    float* prediction;                          /**< Predicted next input */
    float* prediction_error;                    /**< Prediction error signal */
    float* top_down_prediction;                 /**< External top-down prediction */
    float prediction_confidence;                /**< Prediction confidence */

    /* Feedback */
    cortical_feedback_t feedback;               /**< Feedback signal */

    /* Stream activations */
    float ventral_activation;                   /**< "What" stream */
    float dorsal_activation;                    /**< "Where" stream */
    cortex_stream_t active_stream;              /**< Active processing stream */

    /* Aggregate */
    float total_activation;                     /**< Total cortical activation */

    /* Bidirectional timestamps */
    uint64_t last_outbound_ts;
    uint64_t last_inbound_ts;
};

//=============================================================================
// Helper: Current time in ms
//=============================================================================

static uint64_t cochlea_audio_cortex_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

//=============================================================================
// Configuration
//=============================================================================

cochlea_audio_cortex_config_t cochlea_audio_cortex_config_default(void) {
    cochlea_audio_cortex_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_columns = COCHLEA_CORTEX_DEFAULT_COLUMNS;
    config.active_stream = CORTEX_STREAM_BOTH;
    config.prediction_mode = PRED_MODE_SIMPLE;
    config.prediction_weight = COCHLEA_CORTEX_PREDICTION_WEIGHT;
    config.error_gain = COCHLEA_CORTEX_ERROR_GAIN;
    config.enable_feedback = true;
    config.feedback_delay_ms = COCHLEA_CORTEX_FEEDBACK_DELAY_MS;
    config.feedback_strength = 0.5f;
    config.spectral_feature_dim = 32;
    config.temporal_feature_dim = 16;
    config.enable_plasticity = false;
    config.learning_rate = NIMCP_LEARNING_RATE_FINE;
    return config;
}

//=============================================================================
// Helper: Allocate/free arrays
//=============================================================================

static int cochlea_audio_cortex_alloc(cochlea_audio_cortex_bridge_t* bridge) {
    uint32_t n = bridge->config.num_columns;
    uint32_t sf = bridge->config.spectral_feature_dim;
    uint32_t tf = bridge->config.temporal_feature_dim;

    bridge->columns = (cortical_column_output_t*)nimcp_calloc(n, sizeof(cortical_column_output_t));
    bridge->tonotopic_map = (float*)nimcp_calloc(n, sizeof(float));
    bridge->spectral_features = (float*)nimcp_calloc(sf, sizeof(float));
    bridge->temporal_features = (float*)nimcp_calloc(tf, sizeof(float));
    bridge->prediction = (float*)nimcp_calloc(n, sizeof(float));
    bridge->prediction_error = (float*)nimcp_calloc(n, sizeof(float));
    bridge->top_down_prediction = (float*)nimcp_calloc(n, sizeof(float));

    bridge->feedback.frequency_attention = (float*)nimcp_calloc(n, sizeof(float));
    bridge->feedback.gain_modulation = (float*)nimcp_calloc(n, sizeof(float));
    bridge->feedback.num_channels = n;

    if (!bridge->columns || !bridge->tonotopic_map ||
        !bridge->spectral_features || !bridge->temporal_features ||
        !bridge->prediction || !bridge->prediction_error ||
        !bridge->top_down_prediction ||
        !bridge->feedback.frequency_attention || !bridge->feedback.gain_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_audio_cortex_alloc: operation failed");
        return -1;
    }

    /* Initialize column center frequencies */
    for (uint32_t i = 0; i < n; i++) {
        bridge->columns[i].column_id = i;
        bridge->columns[i].center_freq_hz = 20.0f + (float)i / (float)(n > 1 ? n - 1 : 1) * (20000.0f - 20.0f);
        bridge->feedback.frequency_attention[i] = 1.0f;
        bridge->feedback.gain_modulation[i] = 1.0f;
    }

    bridge->num_columns = n;
    return 0;
}

static void cochlea_audio_cortex_free(cochlea_audio_cortex_bridge_t* bridge) {
    if (bridge->columns) { nimcp_free(bridge->columns); bridge->columns = NULL; }
    if (bridge->tonotopic_map) { nimcp_free(bridge->tonotopic_map); bridge->tonotopic_map = NULL; }
    if (bridge->spectral_features) { nimcp_free(bridge->spectral_features); bridge->spectral_features = NULL; }
    if (bridge->temporal_features) { nimcp_free(bridge->temporal_features); bridge->temporal_features = NULL; }
    if (bridge->prediction) { nimcp_free(bridge->prediction); bridge->prediction = NULL; }
    if (bridge->prediction_error) { nimcp_free(bridge->prediction_error); bridge->prediction_error = NULL; }
    if (bridge->top_down_prediction) { nimcp_free(bridge->top_down_prediction); bridge->top_down_prediction = NULL; }
    if (bridge->feedback.frequency_attention) { nimcp_free(bridge->feedback.frequency_attention); bridge->feedback.frequency_attention = NULL; }
    if (bridge->feedback.gain_modulation) { nimcp_free(bridge->feedback.gain_modulation); bridge->feedback.gain_modulation = NULL; }
}

//=============================================================================
// Core API
//=============================================================================

cochlea_audio_cortex_bridge_t* cochlea_audio_cortex_bridge_create(
    cochlea_t* cochlea,
    cochlea_thalamic_bridge_t* thalamic_bridge,
    audio_cortex_t* audio_cortex,
    const cochlea_audio_cortex_config_t* config)
{
    cochlea_audio_cortex_bridge_t* bridge = (cochlea_audio_cortex_bridge_t*)nimcp_calloc(1, sizeof(cochlea_audio_cortex_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_audio_cortex_bridge_create: alloc failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = cochlea_audio_cortex_config_default();
    }

    if (bridge_base_init(&bridge->base, 0, "cochlea_audio_cortex_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "cochlea_audio_cortex_bridge_create: validation failed");
        return NULL;
    }

    bridge->cochlea = cochlea;
    bridge->thalamic_bridge = thalamic_bridge;
    bridge->audio_cortex = audio_cortex;
    bridge->active_stream = bridge->config.active_stream;

    if (cochlea) {
        bridge_base_connect_a_unlocked(&bridge->base, cochlea);
    }
    if (audio_cortex) {
        bridge_base_connect_b_unlocked(&bridge->base, audio_cortex);
    }

    if (cochlea_audio_cortex_alloc(bridge) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cochlea_audio_cortex_bridge_create: array alloc failed");
        bridge_base_cleanup(&bridge->base);
        cochlea_audio_cortex_free(bridge);
        nimcp_free(bridge);
        return NULL;
    }

    bridge->feedback.global_gain = 1.0f;
    bridge->feedback.prediction_signal = 0.0f;
    bridge->prediction_confidence = 0.0f;

    cochlea_audio_cortex_bridge_heartbeat("create", 1.0f);
    return bridge;
}

void cochlea_audio_cortex_bridge_destroy(cochlea_audio_cortex_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cochlea_audio_cortex");
    cochlea_audio_cortex_bridge_heartbeat("destroy", 0.0f);
    cochlea_audio_cortex_free(bridge);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

nimcp_error_t cochlea_audio_cortex_bridge_update(
    cochlea_audio_cortex_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    const mgn_output_t* mgn_output,
    float dt_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_bridge_update: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_audio_cortex_bridge_heartbeat("update", 0.1f);

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = bridge->num_columns;
    float total = 0.0f;
    float ventral = 0.0f;
    float dorsal = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        cortical_column_output_t* col = &bridge->columns[i];

        /* Determine input activation: prefer MGN if available, fallback to raw cochlea */
        float input_activation = 0.0f;
        if (mgn_output && i < mgn_output->num_channels) {
            input_activation = mgn_output->relay_activation[i];
        }

        /* Layer 4: Thalamic input */
        col->layer_activation[CORTEX_LAYER_4] = input_activation;

        /* Layer 2/3: Local integration (smoothed from layer 4) */
        float prev_l23 = col->layer_activation[CORTEX_LAYER_2_3];
        col->layer_activation[CORTEX_LAYER_2_3] = prev_l23 * 0.7f + input_activation * 0.3f;

        /* Layer 1: Feedback (top-down prediction influence) */
        if (bridge->config.prediction_mode != PRED_MODE_NONE) {
            col->layer_activation[CORTEX_LAYER_1] = bridge->top_down_prediction[i] * bridge->config.prediction_weight;
        }

        /* Layer 5: Deep output (combination of layers) */
        col->layer_activation[CORTEX_LAYER_5] =
            col->layer_activation[CORTEX_LAYER_4] * 0.6f +
            col->layer_activation[CORTEX_LAYER_2_3] * 0.3f +
            col->layer_activation[CORTEX_LAYER_1] * 0.1f;

        /* Layer 6: Modulatory feedback to MGN */
        col->layer_activation[CORTEX_LAYER_6] = col->layer_activation[CORTEX_LAYER_5] * 0.5f;

        /* Derived features */
        float prev_rate = col->rate_response;
        col->onset_response = fmaxf(0.0f, input_activation - prev_rate);
        col->offset_response = fmaxf(0.0f, prev_rate - input_activation);
        col->rate_response = input_activation;
        col->temporal_response = col->layer_activation[CORTEX_LAYER_2_3];

        /* Tonotopic map */
        bridge->tonotopic_map[i] = col->layer_activation[CORTEX_LAYER_5];

        /* Accumulate totals */
        total += col->layer_activation[CORTEX_LAYER_5];
        ventral += col->rate_response * 0.5f;
        dorsal += col->onset_response * 0.5f;

        /* Predictive coding: compute error if prediction mode is active */
        if (bridge->config.prediction_mode != PRED_MODE_NONE) {
            bridge->prediction_error[i] = (input_activation - bridge->prediction[i]) * bridge->config.error_gain;
        }

        /* Update prediction (simple exponential smoothing) */
        if (bridge->config.prediction_mode == PRED_MODE_SIMPLE) {
            float new_pred = bridge->prediction[i] * 0.8f + input_activation * 0.2f;
            if (isfinite(new_pred)) {
                bridge->prediction[i] = new_pred;
            }
        }
    }

    bridge->total_activation = total;
    bridge->ventral_activation = ventral;
    bridge->dorsal_activation = dorsal;

    /* Update prediction confidence */
    float error_sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        error_sum += fabsf(bridge->prediction_error[i]);
    }
    float new_confidence = fmaxf(0.0f, 1.0f - error_sum / (float)n);
    if (isfinite(new_confidence)) {
        bridge->prediction_confidence = new_confidence;
    }

    /* Update feedback */
    if (bridge->config.enable_feedback) {
        bridge->feedback.global_gain = 1.0f;
        bridge->feedback.prediction_signal = bridge->prediction_confidence;
        for (uint32_t i = 0; i < n; i++) {
            bridge->feedback.gain_modulation[i] = bridge->columns[i].layer_activation[CORTEX_LAYER_6];
        }
        bridge->last_inbound_ts = cochlea_audio_cortex_time_ms();
    }

    bridge->last_outbound_ts = cochlea_audio_cortex_time_ms();
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    (void)dt_ms; /* Used for timing in future extensions */
    cochlea_audio_cortex_bridge_heartbeat("update", 1.0f);
    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_audio_cortex_bridge_reset(cochlea_audio_cortex_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_bridge_reset: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t n = bridge->num_columns;
    for (uint32_t i = 0; i < n; i++) {
        memset(bridge->columns[i].layer_activation, 0, sizeof(bridge->columns[i].layer_activation));
        bridge->columns[i].onset_response = 0.0f;
        bridge->columns[i].offset_response = 0.0f;
        bridge->columns[i].rate_response = 0.0f;
        bridge->columns[i].temporal_response = 0.0f;
    }

    memset(bridge->tonotopic_map, 0, n * sizeof(float));
    memset(bridge->spectral_features, 0, bridge->config.spectral_feature_dim * sizeof(float));
    memset(bridge->temporal_features, 0, bridge->config.temporal_feature_dim * sizeof(float));
    memset(bridge->prediction, 0, n * sizeof(float));
    memset(bridge->prediction_error, 0, n * sizeof(float));
    memset(bridge->top_down_prediction, 0, n * sizeof(float));

    bridge->prediction_confidence = 0.0f;
    bridge->total_activation = 0.0f;
    bridge->ventral_activation = 0.0f;
    bridge->dorsal_activation = 0.0f;

    bridge->feedback.global_gain = 1.0f;
    bridge->feedback.prediction_signal = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        bridge->feedback.frequency_attention[i] = 1.0f;
        bridge->feedback.gain_modulation[i] = 1.0f;
    }

    bridge->last_outbound_ts = 0;
    bridge->last_inbound_ts = 0;

    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_audio_cortex_bridge_heartbeat("reset", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Cortical Output Access
//=============================================================================

nimcp_error_t cochlea_audio_cortex_get_output(
    const cochlea_audio_cortex_bridge_t* bridge,
    a1_output_t* output)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_output: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_output: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    output->columns = bridge->columns;
    output->num_columns = bridge->num_columns;
    output->total_activation = bridge->total_activation;
    output->tonotopic_map = bridge->tonotopic_map;
    output->map_size = bridge->num_columns;
    output->spectral_features = bridge->spectral_features;
    output->temporal_features = bridge->temporal_features;
    output->feature_dim = bridge->config.spectral_feature_dim;
    output->prediction = bridge->prediction;
    output->prediction_error = bridge->prediction_error;
    output->prediction_confidence = bridge->prediction_confidence;
    output->ventral_activation = bridge->ventral_activation;
    output->dorsal_activation = bridge->dorsal_activation;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_audio_cortex_get_column(
    const cochlea_audio_cortex_bridge_t* bridge,
    uint32_t column_id,
    cortical_column_output_t* output)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_column: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_column: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (column_id >= bridge->num_columns) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    *output = bridge->columns[column_id];

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_audio_cortex_get_layer(
    const cochlea_audio_cortex_bridge_t* bridge,
    cortex_layer_t layer,
    float** activation,
    uint32_t* num_columns)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_layer: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!activation || !num_columns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_layer: output params are NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if ((int)layer < 0 || (int)layer >= COCHLEA_CORTEX_LAYERS) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    *num_columns = bridge->num_columns;
    /* Return pointer to first column's layer activation at the given layer index.
       Note: layer activations are embedded in column structs, not contiguous.
       We return the tonotopic map as a proxy for layer output. */
    *activation = bridge->tonotopic_map;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Predictive Coding
//=============================================================================

nimcp_error_t cochlea_audio_cortex_get_prediction(
    const cochlea_audio_cortex_bridge_t* bridge,
    float** prediction,
    uint32_t* num_channels)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_prediction: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!prediction || !num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_prediction: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *prediction = bridge->prediction;
    *num_channels = bridge->num_columns;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_audio_cortex_get_prediction_error(
    const cochlea_audio_cortex_bridge_t* bridge,
    float** error,
    uint32_t* num_channels)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_prediction_error: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!error || !num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_prediction_error: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *error = bridge->prediction_error;
    *num_channels = bridge->num_columns;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_audio_cortex_set_top_down_prediction(
    cochlea_audio_cortex_bridge_t* bridge,
    const float* prediction,
    uint32_t num_channels)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_set_top_down_prediction: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_set_top_down_prediction: prediction is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t copy_n = (num_channels < bridge->num_columns) ? num_channels : bridge->num_columns;
    memcpy(bridge->top_down_prediction, prediction, copy_n * sizeof(float));
    bridge->last_inbound_ts = cochlea_audio_cortex_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    cochlea_audio_cortex_bridge_heartbeat("set_top_down_prediction", 1.0f);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Feedback Control
//=============================================================================

nimcp_error_t cochlea_audio_cortex_get_feedback(
    const cochlea_audio_cortex_bridge_t* bridge,
    cortical_feedback_t* feedback)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_feedback: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_feedback: feedback is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    feedback->frequency_attention = bridge->feedback.frequency_attention;
    feedback->gain_modulation = bridge->feedback.gain_modulation;
    feedback->num_channels = bridge->feedback.num_channels;
    feedback->global_gain = bridge->feedback.global_gain;
    feedback->prediction_signal = bridge->feedback.prediction_signal;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_audio_cortex_apply_feedback(
    cochlea_audio_cortex_bridge_t* bridge,
    cochlea_thalamic_bridge_t* thalamic_bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_apply_feedback: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!thalamic_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_apply_feedback: thalamic_bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    cochlea_audio_cortex_bridge_heartbeat("apply_feedback", 0.5f);

    /* Apply feedback by setting frequency attention on thalamic bridge using
       the dominant column's center frequency */
    nimcp_mutex_lock(bridge->base.mutex);

    float peak_freq = 1000.0f;
    float peak_val = 0.0f;
    for (uint32_t i = 0; i < bridge->num_columns; i++) {
        float val = bridge->columns[i].layer_activation[CORTEX_LAYER_6];
        if (val > peak_val) {
            peak_val = val;
            peak_freq = bridge->columns[i].center_freq_hz;
        }
    }

    float gain = bridge->feedback.global_gain * bridge->config.feedback_strength * 6.0f;
    bridge->last_inbound_ts = cochlea_audio_cortex_time_ms();

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Apply to thalamic bridge (this acquires its own lock) */
    nimcp_error_t result = cochlea_thalamic_set_frequency_attention(
        thalamic_bridge, peak_freq, 1.0f, gain);

    cochlea_audio_cortex_bridge_heartbeat("apply_feedback", 1.0f);
    return result;
}

nimcp_error_t cochlea_audio_cortex_set_feedback_strength(
    cochlea_audio_cortex_bridge_t* bridge,
    float strength)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_set_feedback_strength: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.feedback_strength = strength;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Stream Control
//=============================================================================

nimcp_error_t cochlea_audio_cortex_set_stream(
    cochlea_audio_cortex_bridge_t* bridge,
    cortex_stream_t stream)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_set_stream: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->active_stream = stream;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_audio_cortex_get_stream_activation(
    const cochlea_audio_cortex_bridge_t* bridge,
    float* ventral,
    float* dorsal)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_stream_activation: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!ventral || !dorsal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_stream_activation: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *ventral = bridge->ventral_activation;
    *dorsal = bridge->dorsal_activation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Feature Extraction
//=============================================================================

nimcp_error_t cochlea_audio_cortex_get_spectral_features(
    const cochlea_audio_cortex_bridge_t* bridge,
    float** features,
    uint32_t* dim)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_spectral_features: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!features || !dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_spectral_features: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *features = bridge->spectral_features;
    *dim = bridge->config.spectral_feature_dim;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t cochlea_audio_cortex_get_temporal_features(
    const cochlea_audio_cortex_bridge_t* bridge,
    float** features,
    uint32_t* dim)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_temporal_features: bridge is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!features || !dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_get_temporal_features: output is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *features = bridge->temporal_features;
    *dim = bridge->config.temporal_feature_dim;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_audio_cortex_verify_bidirectional(
    const cochlea_audio_cortex_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cochlea_audio_cortex_verify_bidirectional: bridge is NULL");
        return false;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bool result = (bridge->last_outbound_ts > 0 && bridge->last_inbound_ts > 0);
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

uint64_t cochlea_audio_cortex_get_last_outbound(
    const cochlea_audio_cortex_bridge_t* bridge)
{
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_outbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}

uint64_t cochlea_audio_cortex_get_last_inbound(
    const cochlea_audio_cortex_bridge_t* bridge)
{
    if (!bridge) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    uint64_t ts = bridge->last_inbound_ts;
    nimcp_mutex_unlock(bridge->base.mutex);
    return ts;
}
