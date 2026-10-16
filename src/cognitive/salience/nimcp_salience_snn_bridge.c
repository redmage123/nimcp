/**
 * @file nimcp_salience_snn_bridge.c
 * @brief Salience - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Implementation of SNN bridge for salience-based attention
 * WHY:  Enable biologically-plausible attention through spike-based salience processing
 * HOW:  Encode novelty, surprise, and urgency as spike patterns
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/salience/nimcp_salience_snn_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_neural_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(salience_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "SALIENCE_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

typedef struct {
    float membrane_potential;
    float threshold;
    float spike_rate;
    uint32_t spike_count;
    float refractory_remaining;
    float input_current;
} salience_neuron_t;

typedef struct {
    float* features;
    uint32_t feature_count;
    uint64_t timestamp_us;
} history_entry_t;

struct salience_snn_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    salience_snn_config_t config;
    salience_snn_state_t state;

    // Neuron populations per channel
    salience_neuron_t** channel_neurons;
    uint32_t num_channels;

    // Output neurons
    salience_neuron_t* output_neurons;
    uint32_t num_output_neurons;

    // History for novelty detection
    history_entry_t* history;
    uint32_t history_count;
    uint32_t history_head;

    // Prediction buffer
    float* prediction;
    uint32_t prediction_count;

    // Per-channel activation levels
    float channel_activations[SALIENCE_SNN_CHANNEL_COUNT];

    // Output state
    salience_snn_output_t last_output;

    // Statistics
    salience_snn_stats_t stats;

    // Callbacks
    salience_snn_spike_callback_t spike_callback;
    void* spike_callback_data;
    salience_snn_threshold_callback_t threshold_callback;
    void* threshold_callback_data;

    // Bio-async
    bool bio_async_connected;

    // Simulation time
    uint64_t sim_time_us;
};

static void reset_neuron(salience_neuron_t* neuron) {
    neuron->membrane_potential = 0.0f;
    neuron->threshold = 1.0f;
    neuron->spike_rate = 0.0f;
    neuron->spike_count = 0;
    neuron->refractory_remaining = 0.0f;
    neuron->input_current = 0.0f;
}

static bool neuron_step(salience_neuron_t* neuron, float dt_ms, float input) {
    const float tau_membrane = NIMCP_MEMBRANE_TAU_MS;
    const float refractory_period = NIMCP_REFRACTORY_PERIOD_MS;

    if (neuron->refractory_remaining > 0.0f) {
        neuron->refractory_remaining -= dt_ms;
        /* Refractory period is normal biological behavior, not an error */
        return false;
    }

    float decay = expf(-dt_ms / tau_membrane);
    neuron->membrane_potential = neuron->membrane_potential * decay + input * (1.0f - decay);

    if (neuron->membrane_potential >= neuron->threshold) {
        neuron->membrane_potential = 0.0f;
        neuron->refractory_remaining = refractory_period;
        neuron->spike_count++;
        return true;
    }

    /* Non-spiking is the common case, not an error */
    return false;
}

static float compute_distance(const float* a, const float* b, uint32_t count) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(i + 1) / (float)count);
        }

        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return (count > 0) ? sqrtf(sum / count) : 0.0f;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

salience_snn_config_t salience_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_config_", 0.0f);


    salience_snn_config_t config = {
        .max_features = SALIENCE_SNN_MAX_FEATURES,
        .neurons_per_dim = SALIENCE_SNN_NEURONS_PER_DIM,
        .history_depth = 32,
        .dt_ms = 1.0f,
        .novelty_threshold = 0.6f,
        .surprise_threshold = 0.5f,
        .urgency_threshold = 0.7f,
        .novelty_weight = 0.3f,
        .surprise_weight = 0.4f,
        .urgency_weight = 0.3f,
        .encoding_type = SALIENCE_SNN_ENCODE_RATE,
        .enable_multimodal = true,
        .enable_history = true,
        .enable_prediction = true,
        .enable_bio_async = false
    };
    return config;
}

salience_snn_bridge_t* salience_snn_create(const salience_snn_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_create", 0.0f);


    salience_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(salience_snn_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->config = *config;
    bridge->state = SALIENCE_SNN_STATE_IDLE;
    bridge->num_channels = SALIENCE_SNN_CHANNEL_COUNT;

    // Allocate neurons per channel
    bridge->channel_neurons = nimcp_calloc(bridge->num_channels, sizeof(salience_neuron_t*));
    if (!bridge->channel_neurons) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_snn_create: bridge->channel_neurons is NULL");
        return NULL;
    }

    for (uint32_t c = 0; c < bridge->num_channels; c++) {
        /* Phase 8: Loop progress heartbeat */
        if ((c & 0xFF) == 0 && bridge->num_channels > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(c + 1) / (float)bridge->num_channels);
        }

        bridge->channel_neurons[c] = nimcp_calloc(config->neurons_per_dim, sizeof(salience_neuron_t));
        if (!bridge->channel_neurons[c]) {
            salience_snn_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_snn_create: bridge->channel_neurons is NULL");
            return NULL;
        }
        for (uint32_t n = 0; n < config->neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && config->neurons_per_dim > 256) {
                salience_snn_bridge_heartbeat("salience_snn_loop",
                                 (float)(n + 1) / (float)config->neurons_per_dim);
            }

            reset_neuron(&bridge->channel_neurons[c][n]);
        }
    }

    // Allocate output neurons
    bridge->num_output_neurons = config->neurons_per_dim * 2;
    bridge->output_neurons = nimcp_calloc(bridge->num_output_neurons, sizeof(salience_neuron_t));
    if (!bridge->output_neurons) {
        salience_snn_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_snn_create: bridge->output_neurons is NULL");
        return NULL;
    }
    for (uint32_t n = 0; n < bridge->num_output_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(n + 1) / (float)bridge->num_output_neurons);
        }

        reset_neuron(&bridge->output_neurons[n]);
    }

    // Allocate history
    if (config->enable_history) {
        bridge->history = nimcp_calloc(config->history_depth, sizeof(history_entry_t));
        if (!bridge->history) {
            salience_snn_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_snn_create: bridge->history is NULL");
            return NULL;
        }
        for (uint32_t i = 0; i < config->history_depth; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && config->history_depth > 256) {
                salience_snn_bridge_heartbeat("salience_snn_loop",
                                 (float)(i + 1) / (float)config->history_depth);
            }

            bridge->history[i].features = nimcp_calloc(config->max_features, sizeof(float));
            if (!bridge->history[i].features) {
                salience_snn_destroy(bridge);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_snn_create: bridge->history is NULL");
                return NULL;
            }
        }
    }

    // Allocate prediction buffer
    if (config->enable_prediction) {
        bridge->prediction = nimcp_calloc(config->max_features, sizeof(float));
        if (!bridge->prediction) {
            salience_snn_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "salience_snn_create: bridge->prediction is NULL");
            return NULL;
        }
    }

    NIMCP_LOGGING_INFO("Created %s bridge", "salience_snn");
    return bridge;
}

void salience_snn_destroy(salience_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "salience_snn");

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_destroy", 0.0f);


    if (bridge->channel_neurons) {
        for (uint32_t c = 0; c < bridge->num_channels; c++) {
            /* Phase 8: Loop progress heartbeat */
            if ((c & 0xFF) == 0 && bridge->num_channels > 256) {
                salience_snn_bridge_heartbeat("salience_snn_loop",
                                 (float)(c + 1) / (float)bridge->num_channels);
            }

            nimcp_free(bridge->channel_neurons[c]);
        }
        nimcp_free(bridge->channel_neurons);
    }
    nimcp_free(bridge->output_neurons);

    if (bridge->history) {
        for (uint32_t i = 0; i < bridge->config.history_depth; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->config.history_depth > 256) {
                salience_snn_bridge_heartbeat("salience_snn_loop",
                                 (float)(i + 1) / (float)bridge->config.history_depth);
            }

            nimcp_free(bridge->history[i].features);
        }
        nimcp_free(bridge->history);
    }

    nimcp_free(bridge->prediction);
    nimcp_free(bridge);
    bridge = NULL;
}

int salience_snn_reset(salience_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_reset", 0.0f);


    bridge->state = SALIENCE_SNN_STATE_IDLE;
    bridge->sim_time_us = 0;

    // Reset neurons
    for (uint32_t c = 0; c < bridge->num_channels; c++) {
        /* Phase 8: Loop progress heartbeat */
        if ((c & 0xFF) == 0 && bridge->num_channels > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(c + 1) / (float)bridge->num_channels);
        }

        for (uint32_t n = 0; n < bridge->config.neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_dim > 256) {
                salience_snn_bridge_heartbeat("salience_snn_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_dim);
            }

            reset_neuron(&bridge->channel_neurons[c][n]);
        }
    }
    for (uint32_t n = 0; n < bridge->num_output_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(n + 1) / (float)bridge->num_output_neurons);
        }

        reset_neuron(&bridge->output_neurons[n]);
    }

    // Reset activations
    memset(bridge->channel_activations, 0, sizeof(bridge->channel_activations));

    // Reset history
    bridge->history_count = 0;
    bridge->history_head = 0;

    // Reset output
    memset(&bridge->last_output, 0, sizeof(salience_snn_output_t));

    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int salience_snn_encode_features(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count
) {
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_encode_features: required parameter is NULL (bridge, features)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_encode_", 0.0f);


    bridge->state = SALIENCE_SNN_STATE_ENCODING;

    // Compute novelty from history (first stimulus = maximum novelty)
    float novelty = 1.0f;  // Default to max novelty for first-time stimuli
    if (bridge->config.enable_history && bridge->history_count > 0) {
        novelty = salience_snn_compute_novelty(bridge, features, feature_count);
    }

    // Compute intensity from feature magnitude
    float intensity = 0.0f;
    for (uint32_t i = 0; i < feature_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && feature_count > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(i + 1) / (float)feature_count);
        }

        intensity += fabsf(features[i]);
    }
    if (feature_count > 0) { intensity /= feature_count; }

    // Encode into channel neurons
    for (uint32_t n = 0; n < bridge->config.neurons_per_dim; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->config.neurons_per_dim > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(n + 1) / (float)bridge->config.neurons_per_dim);
        }

        bridge->channel_neurons[SALIENCE_SNN_CHANNEL_NOVELTY][n].input_current = novelty * 2.0f;
        bridge->channel_neurons[SALIENCE_SNN_CHANNEL_INTENSITY][n].input_current = intensity * 2.0f;
    }

    // Add to history
    if (bridge->config.enable_history) {
        salience_snn_add_to_history(bridge, features, feature_count);
    }

    bridge->stats.total_evaluations++;
    return 0;
}

int salience_snn_encode_with_prediction(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count,
    const float* prediction,
    uint32_t prediction_count
) {
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_encode_with_prediction: required parameter is NULL (bridge, features)");
        return -1;
    }

    // First encode features
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_encode_", 0.0f);


    salience_snn_encode_features(bridge, features, feature_count);

    // Compute surprise from prediction error
    if (prediction && prediction_count > 0) {
        uint32_t compare_count = feature_count < prediction_count ? feature_count : prediction_count;
        float surprise = compute_distance(features, prediction, compare_count);
        surprise = nimcp_clampf(surprise, 0.0f, 1.0f);

        for (uint32_t n = 0; n < bridge->config.neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_dim > 256) {
                salience_snn_bridge_heartbeat("salience_snn_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_dim);
            }

            bridge->channel_neurons[SALIENCE_SNN_CHANNEL_SURPRISE][n].input_current = surprise * 2.0f;
        }

        // Store prediction for later
        if (bridge->prediction) {
            memcpy(bridge->prediction, prediction, prediction_count * sizeof(float));
            bridge->prediction_count = prediction_count;
        }
    }

    return 0;
}

int salience_snn_encode_temporal(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count,
    uint64_t timestamp_us
) {
    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_encode_temporal: required parameter is NULL (bridge, features)");
        return -1;
    }

    // Encode features
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_encode_", 0.0f);


    salience_snn_encode_features(bridge, features, feature_count);

    // Compute urgency from temporal dynamics
    float urgency = 0.0f;
    if (bridge->history_count > 0 && bridge->config.enable_history) {
        // Check for rapid changes
        uint32_t recent_idx = (bridge->history_head + bridge->config.history_depth - 1) %
                              bridge->config.history_depth;
        uint64_t time_delta = timestamp_us - bridge->history[recent_idx].timestamp_us;

        if (time_delta > 0 && time_delta < 100000) {  // Within 100ms
            float change_rate = compute_distance(features, bridge->history[recent_idx].features,
                                                feature_count);
            urgency = change_rate * (100000.0f / time_delta);  // Scale by temporal proximity
            urgency = nimcp_clampf(urgency, 0.0f, 1.0f);
        }
    }

    for (uint32_t n = 0; n < bridge->config.neurons_per_dim; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->config.neurons_per_dim > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(n + 1) / (float)bridge->config.neurons_per_dim);
        }

        bridge->channel_neurons[SALIENCE_SNN_CHANNEL_URGENCY][n].input_current = urgency * 2.0f;
    }

    return 0;
}

//=============================================================================
// Simulation Functions
//=============================================================================

int salience_snn_simulate(salience_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_simulate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_simulat", 0.0f);


    bridge->state = SALIENCE_SNN_STATE_SIMULATING;

    float dt = bridge->config.dt_ms;
    if (dt <= 0.0f) dt = 1.0f;
    int steps = (int)(duration_ms / dt);

    for (int step = 0; step < steps; step++) {
        /* Phase 8: Loop progress heartbeat */
        if ((step & 0xFF) == 0 && steps > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(step + 1) / (float)steps);
        }

        salience_snn_step(bridge);
    }

    bridge->state = SALIENCE_SNN_STATE_IDLE;
    return 0;
}

int salience_snn_step(salience_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_step: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_step", 0.0f);


    float dt = bridge->config.dt_ms;

    // Step all channel neurons
    for (uint32_t c = 0; c < bridge->num_channels; c++) {
        /* Phase 8: Loop progress heartbeat */
        if ((c & 0xFF) == 0 && bridge->num_channels > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(c + 1) / (float)bridge->num_channels);
        }

        float channel_activity = 0.0f;
        float membrane_activity = 0.0f;
        for (uint32_t n = 0; n < bridge->config.neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_dim > 256) {
                salience_snn_bridge_heartbeat("salience_snn_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_dim);
            }

            float input = bridge->channel_neurons[c][n].input_current;
            if (neuron_step(&bridge->channel_neurons[c][n], dt, input)) {
                channel_activity += 1.0f;
                bridge->stats.total_spikes++;
            }
            // Track membrane potential for continuous activation
            membrane_activity += bridge->channel_neurons[c][n].membrane_potential;
            membrane_activity += bridge->channel_neurons[c][n].input_current * 0.4f;
        }
        // Combine spike and membrane activity
        float spike_rate = (bridge->config.neurons_per_dim > 0) ? (channel_activity / bridge->config.neurons_per_dim) : 0.0f;
        if (bridge->config.neurons_per_dim > 0) membrane_activity /= bridge->config.neurons_per_dim;
        bridge->channel_activations[c] = nimcp_clampf(spike_rate * 2.0f + membrane_activity * 0.8f, 0.0f, 1.0f);
    }

    // Output neurons integrate weighted channel activity
    float weighted_input =
        bridge->channel_activations[SALIENCE_SNN_CHANNEL_NOVELTY] * bridge->config.novelty_weight +
        bridge->channel_activations[SALIENCE_SNN_CHANNEL_SURPRISE] * bridge->config.surprise_weight +
        bridge->channel_activations[SALIENCE_SNN_CHANNEL_URGENCY] * bridge->config.urgency_weight +
        bridge->channel_activations[SALIENCE_SNN_CHANNEL_INTENSITY] * 0.2f;

    for (uint32_t n = 0; n < bridge->num_output_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(n + 1) / (float)bridge->num_output_neurons);
        }

        if (neuron_step(&bridge->output_neurons[n], dt, weighted_input)) {
            bridge->stats.total_spikes++;
        }
    }

    bridge->sim_time_us += (uint64_t)(dt * 1000.0f);
    return 0;
}

int salience_snn_forward(
    salience_snn_bridge_t* bridge,
    const float* inputs,
    uint32_t input_count
) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    // Distribute inputs across channels
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_forward", 0.0f);


    if (bridge->num_channels == 0) return -1;
    uint32_t per_channel = input_count / bridge->num_channels;
    for (uint32_t c = 0; c < bridge->num_channels && c * per_channel < input_count; c++) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < per_channel && (c * per_channel + i) < input_count; i++) {
            sum += inputs[c * per_channel + i];
        }
        if (per_channel > 0) { sum /= per_channel; }

        for (uint32_t n = 0; n < bridge->config.neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && bridge->config.neurons_per_dim > 256) {
                salience_snn_bridge_heartbeat("salience_snn_loop",
                                 (float)(n + 1) / (float)bridge->config.neurons_per_dim);
            }

            bridge->channel_neurons[c][n].input_current = sum;
        }
    }

    return salience_snn_step(bridge);
}

//=============================================================================
// Decoding Functions
//=============================================================================

int salience_snn_decode_salience(
    salience_snn_bridge_t* bridge,
    salience_snn_output_t* output
) {
    if (!bridge || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_decode_salience: required parameter is NULL (bridge, output)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_decode_", 0.0f);


    bridge->state = SALIENCE_SNN_STATE_EVALUATING;

    output->novelty = bridge->channel_activations[SALIENCE_SNN_CHANNEL_NOVELTY];
    output->surprise = bridge->channel_activations[SALIENCE_SNN_CHANNEL_SURPRISE];
    output->urgency = bridge->channel_activations[SALIENCE_SNN_CHANNEL_URGENCY];
    output->intensity = bridge->channel_activations[SALIENCE_SNN_CHANNEL_INTENSITY];

    // Combined salience (include intensity as contributing factor)
    output->combined_salience =
        output->novelty * bridge->config.novelty_weight +
        output->surprise * bridge->config.surprise_weight +
        output->urgency * bridge->config.urgency_weight +
        output->intensity * 0.2f;  // Intensity contributes to salience

    // Find dominant channel
    float max_activation = 0.0f;
    output->dominant_channel = SALIENCE_SNN_CHANNEL_NOVELTY;
    for (uint32_t c = 0; c < SALIENCE_SNN_CHANNEL_COUNT; c++) {
        /* Phase 8: Loop progress heartbeat */
        if ((c & 0xFF) == 0 && SALIENCE_SNN_CHANNEL_COUNT > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(c + 1) / (float)SALIENCE_SNN_CHANNEL_COUNT);
        }

        if (bridge->channel_activations[c] > max_activation) {
            max_activation = bridge->channel_activations[c];
            output->dominant_channel = (salience_snn_channel_t)c;
        }
    }

    // Confidence based on output neuron activity consistency
    float output_activity = 0.0f;
    float output_variance = 0.0f;
    for (uint32_t n = 0; n < bridge->num_output_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(n + 1) / (float)bridge->num_output_neurons);
        }

        output_activity += bridge->output_neurons[n].membrane_potential;
    }
    if (bridge->num_output_neurons > 0) { output_activity /= bridge->num_output_neurons; }

    for (uint32_t n = 0; n < bridge->num_output_neurons; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->num_output_neurons > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(n + 1) / (float)bridge->num_output_neurons);
        }

        float diff = bridge->output_neurons[n].membrane_potential - output_activity;
        output_variance += diff * diff;
    }
    if (bridge->num_output_neurons > 0) { output_variance /= bridge->num_output_neurons; }
    output->confidence = nimcp_clampf(1.0f - sqrtf(output_variance), 0.0f, 1.0f);

    // High salience detection
    output->high_salience = (output->combined_salience > 0.6f);

    bridge->last_output = *output;

    // Update stats
    if (output->novelty > bridge->config.novelty_threshold) {
        bridge->stats.high_novelty_events++;
        if (bridge->threshold_callback) {
            bridge->threshold_callback(bridge, SALIENCE_SNN_CHANNEL_NOVELTY, output->novelty,
                                      bridge->threshold_callback_data);
        }
    }
    if (output->surprise > bridge->config.surprise_threshold) {
        bridge->stats.high_surprise_events++;
        if (bridge->threshold_callback) {
            bridge->threshold_callback(bridge, SALIENCE_SNN_CHANNEL_SURPRISE, output->surprise,
                                      bridge->threshold_callback_data);
        }
    }
    if (output->urgency > bridge->config.urgency_threshold) {
        bridge->stats.high_urgency_events++;
        if (bridge->threshold_callback) {
            bridge->threshold_callback(bridge, SALIENCE_SNN_CHANNEL_URGENCY, output->urgency,
                                      bridge->threshold_callback_data);
        }
    }

    bridge->stats.mean_salience = bridge->stats.mean_salience * 0.99f + output->combined_salience * 0.01f;
    bridge->stats.mean_novelty = bridge->stats.mean_novelty * 0.99f + output->novelty * 0.01f;
    bridge->stats.mean_surprise = bridge->stats.mean_surprise * 0.99f + output->surprise * 0.01f;

    bridge->state = SALIENCE_SNN_STATE_IDLE;
    return 0;
}

float salience_snn_get_combined_salience(salience_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    // Compute from channel activations for immediate access
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_get_com", 0.0f);


    return bridge->channel_activations[SALIENCE_SNN_CHANNEL_NOVELTY] * bridge->config.novelty_weight +
           bridge->channel_activations[SALIENCE_SNN_CHANNEL_SURPRISE] * bridge->config.surprise_weight +
           bridge->channel_activations[SALIENCE_SNN_CHANNEL_URGENCY] * bridge->config.urgency_weight +
           bridge->channel_activations[SALIENCE_SNN_CHANNEL_INTENSITY] * 0.2f;
}

float salience_snn_get_novelty(salience_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_get_nov", 0.0f);


    return bridge->channel_activations[SALIENCE_SNN_CHANNEL_NOVELTY];
}

float salience_snn_get_surprise(salience_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_get_sur", 0.0f);


    return bridge->channel_activations[SALIENCE_SNN_CHANNEL_SURPRISE];
}

float salience_snn_get_urgency(salience_snn_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_get_urg", 0.0f);


    return bridge->channel_activations[SALIENCE_SNN_CHANNEL_URGENCY];
}

salience_snn_channel_t salience_snn_get_dominant_channel(salience_snn_bridge_t* bridge) {
    if (!bridge) return SALIENCE_SNN_CHANNEL_NOVELTY;
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_get_dom", 0.0f);


    return bridge->last_output.dominant_channel;
}

//=============================================================================
// History Functions
//=============================================================================

int salience_snn_add_to_history(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count
) {
    if (!bridge || !features || !bridge->history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_add_to_history: required parameter is NULL (bridge, features, bridge->history)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_add_to_", 0.0f);


    uint32_t copy_count = feature_count < bridge->config.max_features ?
                          feature_count : bridge->config.max_features;

    memcpy(bridge->history[bridge->history_head].features, features, copy_count * sizeof(float));
    bridge->history[bridge->history_head].feature_count = copy_count;
    bridge->history[bridge->history_head].timestamp_us = bridge->sim_time_us;

    bridge->history_head = (bridge->history_head + 1) % bridge->config.history_depth;
    if (bridge->history_count < bridge->config.history_depth) {
        bridge->history_count++;
    }

    return 0;
}

int salience_snn_clear_history(salience_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_clear_history: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_clear_h", 0.0f);


    bridge->history_count = 0;
    bridge->history_head = 0;

    return 0;
}

float salience_snn_compute_novelty(
    salience_snn_bridge_t* bridge,
    const float* features,
    uint32_t feature_count
) {
    if (!bridge || !features || !bridge->history || bridge->history_count == 0) {
        return 1.0f;  // No history = maximum novelty
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_compute", 0.0f);


    float min_distance = 1000.0f;

    for (uint32_t i = 0; i < bridge->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->history_count > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(i + 1) / (float)bridge->history_count);
        }

        uint32_t compare_count = feature_count < bridge->history[i].feature_count ?
                                 feature_count : bridge->history[i].feature_count;
        float distance = compute_distance(features, bridge->history[i].features, compare_count);
        if (distance < min_distance) {
            min_distance = distance;
        }
    }

    // Convert distance to novelty (closer = less novel)
    float novelty = 1.0f - expf(-min_distance * 2.0f);
    return nimcp_clampf(novelty, 0.0f, 1.0f);
}

//=============================================================================
// State Query Functions
//=============================================================================

int salience_snn_get_channel_state(
    salience_snn_bridge_t* bridge,
    salience_snn_channel_t channel,
    salience_channel_state_t* state
) {
    if (!bridge || !state || channel >= SALIENCE_SNN_CHANNEL_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_get_channel_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_get_cha", 0.0f);


    state->channel = channel;
    state->activation = bridge->channel_activations[channel];

    uint32_t total_spikes = 0;
    for (uint32_t n = 0; n < bridge->config.neurons_per_dim; n++) {
        /* Phase 8: Loop progress heartbeat */
        if ((n & 0xFF) == 0 && bridge->config.neurons_per_dim > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(n + 1) / (float)bridge->config.neurons_per_dim);
        }

        total_spikes += bridge->channel_neurons[channel][n].spike_count;
    }
    state->spike_count = total_spikes;
    state->spike_rate = (bridge->config.neurons_per_dim > 0) ? ((float)total_spikes / bridge->config.neurons_per_dim) : 0.0f;
    state->confidence = bridge->last_output.confidence;

    return 0;
}

int salience_snn_get_state(
    salience_snn_bridge_t* bridge,
    salience_snn_bridge_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_get_sta", 0.0f);


    state->state = bridge->state;

    float total = 0.0f;
    for (uint32_t c = 0; c < bridge->num_channels; c++) {
        /* Phase 8: Loop progress heartbeat */
        if ((c & 0xFF) == 0 && bridge->num_channels > 256) {
            salience_snn_bridge_heartbeat("salience_snn_loop",
                             (float)(c + 1) / (float)bridge->num_channels);
        }

        total += bridge->channel_activations[c];
    }
    state->total_activity = total;

    state->combined_salience = bridge->last_output.combined_salience;
    state->novelty = bridge->last_output.novelty;
    state->surprise = bridge->last_output.surprise;
    state->urgency = bridge->last_output.urgency;

    state->high_salience_count = bridge->stats.high_novelty_events +
                                  bridge->stats.high_surprise_events +
                                  bridge->stats.high_urgency_events;

    return 0;
}

int salience_snn_get_stats(salience_snn_bridge_t* bridge, salience_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_get_sta", 0.0f);


    return 0;
}

int salience_snn_reset_stats(salience_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_reset_stats: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_reset_s", 0.0f);


    memset(&bridge->stats, 0, sizeof(salience_snn_stats_t));
    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int salience_snn_register_spike_callback(
    salience_snn_bridge_t* bridge,
    salience_snn_spike_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_register_spike_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_registe", 0.0f);


    bridge->spike_callback = callback;
    bridge->spike_callback_data = user_data;
    return 0;
}

int salience_snn_register_threshold_callback(
    salience_snn_bridge_t* bridge,
    salience_snn_threshold_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_register_threshold_callback: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_registe", 0.0f);


    bridge->threshold_callback = callback;
    bridge->threshold_callback_data = user_data;
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int salience_snn_bio_async_connect(salience_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_bio_async_connect: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_bio_async_connect: bridge->config is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_bio_asy", 0.0f);


    bridge->bio_async_connected = true;
    return 0;
}

int salience_snn_bio_async_disconnect(salience_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_bio_asy", 0.0f);


    bridge->bio_async_connected = false;
    return 0;
}

bool salience_snn_is_bio_async_connected(salience_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_is_bio_", 0.0f);


    return bridge->bio_async_connected;
}

//=============================================================================
// Weight Configuration
//=============================================================================

int salience_snn_set_weights(
    salience_snn_bridge_t* bridge,
    float novelty_weight,
    float surprise_weight,
    float urgency_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_set_weights: bridge is NULL");
        return -1;
    }

    // Normalize weights
    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_set_wei", 0.0f);


    float total = novelty_weight + surprise_weight + urgency_weight;
    if (total > 0.0f) {
        bridge->config.novelty_weight = novelty_weight / total;
        bridge->config.surprise_weight = surprise_weight / total;
        bridge->config.urgency_weight = urgency_weight / total;
    }

    return 0;
}

int salience_snn_set_thresholds(
    salience_snn_bridge_t* bridge,
    float novelty_threshold,
    float surprise_threshold,
    float urgency_threshold
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "salience_snn_set_thresholds: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_snn_bridge_heartbeat("salience_snn_salience_snn_set_thr", 0.0f);


    bridge->config.novelty_threshold = nimcp_clampf(novelty_threshold, 0.0f, 1.0f);
    bridge->config.surprise_threshold = nimcp_clampf(surprise_threshold, 0.0f, 1.0f);
    bridge->config.urgency_threshold = nimcp_clampf(urgency_threshold, 0.0f, 1.0f);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void salience_snn_bridge_set_instance_health_agent(salience_snn_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "salience_snn_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int salience_snn_bridge_training_begin(salience_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    salience_snn_bridge_heartbeat_instance(bridge->health_agent, "salience_snn_bridge_training_begin", 0.0f);
    return 0;
}

int salience_snn_bridge_training_end(salience_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_snn_bridge_training_end: NULL argument");
        return -1;
    }
    salience_snn_bridge_heartbeat_instance(bridge->health_agent, "salience_snn_bridge_training_end", 1.0f);
    return 0;
}

int salience_snn_bridge_training_step(salience_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_snn_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    salience_snn_bridge_heartbeat_instance(bridge->health_agent, "salience_snn_bridge_training_step", progress);
    return 0;
}
