/**
 * @file nimcp_dragonfly_snn_bridge.c
 * @brief Implementation of Dragonfly-to-SNN Backpropagation Bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_snn_bridge.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_constants.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_snn_bridge)

#define LOG_MODULE "DRAGONFLY_SNN_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

#define TSDN_NUM_NEURONS 16

struct dragonfly_snn_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    dragonfly_snn_config_t config;
    dragonfly_system_t* dragonfly;
    void* snn_trainer;

    /* TSDN neuron state */
    float membrane_potentials[TSDN_NUM_NEURONS];
    float spikes[TSDN_NUM_NEURONS];
    float spike_times[TSDN_NUM_NEURONS];

    /* Weights */
    float* weights;
    float* biases;
    uint32_t num_weights;
    uint32_t num_biases;

    /* Gradients */
    float* weight_gradients;
    float* bias_gradients;
    float gradient_norm;
    bool gradients_clipped;

    /* Eligibility traces */
    float* eligibility_traces;
    float reward_trace;

    /* Training state */
    bool is_training;
    float current_loss;
    uint64_t step_count;

    /* Statistics */
    dragonfly_snn_stats_t stats;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute surrogate gradient
 */
static float compute_surrogate(float v, dragonfly_surrogate_t method, float beta) {
    float abs_v = fabsf(v);

    switch (method) {
        case DRAGONFLY_SURROGATE_SUPERSPIKE: {
            float denom = beta * abs_v + 1.0f;
            return 1.0f / (denom * denom);
        }
        case DRAGONFLY_SURROGATE_FAST_SIGMOID: {
            float denom = 1.0f + abs_v;
            return v / (denom * denom);
        }
        case DRAGONFLY_SURROGATE_TRIANGULAR: {
            float width = 1.0f / beta;
            return abs_v < width ? (1.0f - abs_v / width) : 0.0f;
        }
        case DRAGONFLY_SURROGATE_EXPONENTIAL:
            return beta * expf(-beta * abs_v);
        default:
            return 0.0f;
    }
}

/**
 * @brief LIF neuron update
 */
static float lif_update(float v, float i_syn, float tau, float dt) {
    float decay = expf(-dt / tau);
    return v * decay + i_syn * (1.0f - decay);
}

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_snn_bridge_default_config(dragonfly_snn_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->algorithm = DRAGONFLY_TRAIN_BPTT;
    config->loss_type = DRAGONFLY_LOSS_DIRECTION_ERROR;

    /* Surrogate configuration */
    config->surrogate.method = DRAGONFLY_SURROGATE_SUPERSPIKE;
    config->surrogate.beta = DRAGONFLY_SNN_DEFAULT_BETA;
    config->surrogate.width = 1.0f;
    config->surrogate.adaptive_beta = false;

    /* Eligibility configuration */
    config->eligibility.tau_eligibility = DRAGONFLY_SNN_DEFAULT_TAU_ELIG;
    config->eligibility.tau_reward = 100.0f;
    config->eligibility.use_local_traces = true;
    config->eligibility.normalize_traces = false;

    /* Neuron parameters */
    config->neuron_params.model = DRAGONFLY_NEURON_LIF;
    config->neuron_params.tau_membrane = DRAGONFLY_SNN_DEFAULT_TAU;
    config->neuron_params.v_threshold = 1.0f;
    config->neuron_params.v_reset = 0.0f;
    config->neuron_params.v_rest = 0.0f;
    config->neuron_params.refractory_ms = NIMCP_REFRACTORY_PERIOD_MS;
    config->neuron_params.leak_conductance = 0.05f;

    /* BPTT parameters */
    config->unroll_steps = 100;
    config->truncate_gradients = true;

    /* Learning rate */
    config->learning_rate = 0.01f;
    config->lr_decay = 0.99f;
    config->min_learning_rate = 0.0001f;

    /* Regularization */
    config->weight_decay = 0.0001f;
    config->activity_regularization = 0.001f;
    config->gradient_clip = 10.0f;

    /* Reward shaping */
    config->reward_baseline = 0.0f;
    config->reward_discount = 0.99f;
    config->use_advantage = false;

    /* Loss weights */
    config->loss_weight_spike = 0.1f;
    config->loss_weight_direction = 1.0f;
    config->loss_weight_reward = 0.5f;

    return 0;
}

int dragonfly_snn_bridge_validate_config(const dragonfly_snn_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_bridge_validate_config: config is NULL");
        return -1;
    }
    if (config->algorithm >= DRAGONFLY_TRAIN_HYBRID + 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_snn_bridge_validate_config: invalid algorithm");
        return -1;
    }
    if (config->surrogate.method >= DRAGONFLY_SURROGATE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_snn_bridge_validate_config: invalid surrogate method");
        return -1;
    }
    if (config->learning_rate <= 0.0f) {
        return -1;
    }
    if (config->unroll_steps == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_snn_bridge_validate_config: config->unroll_steps is zero");
        return -1;
    }
    if (config->neuron_params.tau_membrane <= 0.0f) {
        return -1;
    }
    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_snn_bridge_t* dragonfly_snn_bridge_create(
    dragonfly_system_t* dragonfly,
    void* snn_trainer,
    const dragonfly_snn_config_t* config
) {
    dragonfly_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_snn_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (dragonfly_snn_bridge_validate_config(config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "dragonfly_snn_bridge_create: invalid configuration");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        dragonfly_snn_bridge_default_config(&bridge->config);
    }

    bridge->dragonfly = dragonfly;
    bridge->snn_trainer = snn_trainer;

    /* Initialize neuron state */
    for (int i = 0; i < TSDN_NUM_NEURONS; i++) {
        bridge->membrane_potentials[i] = bridge->config.neuron_params.v_rest;
        bridge->spikes[i] = 0.0f;
        bridge->spike_times[i] = -1.0f;
    }

    /* Allocate weights (fully connected: 16x16 + 16 input) */
    bridge->num_weights = TSDN_NUM_NEURONS * (TSDN_NUM_NEURONS + TSDN_NUM_NEURONS);
    bridge->num_biases = TSDN_NUM_NEURONS;

    bridge->weights = nimcp_calloc(bridge->num_weights, sizeof(float));
    bridge->biases = nimcp_calloc(bridge->num_biases, sizeof(float));
    bridge->weight_gradients = nimcp_calloc(bridge->num_weights, sizeof(float));
    bridge->bias_gradients = nimcp_calloc(bridge->num_biases, sizeof(float));
    bridge->eligibility_traces = nimcp_calloc(bridge->num_weights, sizeof(float));

    if (!bridge->weights || !bridge->biases || !bridge->weight_gradients ||
        !bridge->bias_gradients || !bridge->eligibility_traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_snn_bridge_create: failed to allocate weight arrays");
        dragonfly_snn_bridge_destroy(bridge);
        return NULL;
    }

    /* Initialize weights with small random values */
    for (uint32_t i = 0; i < bridge->num_weights; i++) {
        bridge->weights[i] = (nimcp_rand_uniform() - 0.5f) * 0.1f;
    }

    bridge->is_training = false;
    bridge->current_loss = 0.0f;
    bridge->step_count = 0;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Initialize bridge base infrastructure (mutex + metrics) */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_snn") != 0) {
        nimcp_free(bridge->weights);
        nimcp_free(bridge->biases);
        nimcp_free(bridge->weight_gradients);
        nimcp_free(bridge->bias_gradients);
        nimcp_free(bridge->eligibility_traces);
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_snn_bridge_destroy(dragonfly_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_snn");

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge->weights);
    nimcp_free(bridge->biases);
    nimcp_free(bridge->weight_gradients);
    nimcp_free(bridge->bias_gradients);
    nimcp_free(bridge->eligibility_traces);
    nimcp_free(bridge);
}

int dragonfly_snn_bridge_reset(dragonfly_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Reset neuron state */
    for (int i = 0; i < TSDN_NUM_NEURONS; i++) {
        bridge->membrane_potentials[i] = bridge->config.neuron_params.v_rest;
        bridge->spikes[i] = 0.0f;
        bridge->spike_times[i] = -1.0f;
    }

    /* Reset gradients and traces */
    memset(bridge->weight_gradients, 0, bridge->num_weights * sizeof(float));
    memset(bridge->bias_gradients, 0, bridge->num_biases * sizeof(float));
    memset(bridge->eligibility_traces, 0, bridge->num_weights * sizeof(float));
    bridge->reward_trace = 0.0f;

    bridge->current_loss = 0.0f;
    bridge->step_count = 0;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

//=============================================================================
// Forward Pass
//=============================================================================

int dragonfly_snn_forward(
    dragonfly_snn_bridge_t* bridge,
    const float* input,
    uint32_t input_size,
    uint32_t timesteps
) {
    if (!bridge || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_forward: required parameter is NULL (bridge, input)");
        return -1;
    }

    float dt = NIMCP_SIMULATION_DT_MS;  /* 1ms timestep */
    float tau = bridge->config.neuron_params.tau_membrane;
    float v_thresh = bridge->config.neuron_params.v_threshold;
    float v_reset = bridge->config.neuron_params.v_reset;

    uint64_t total_spikes = 0;

    for (uint32_t t = 0; t < timesteps; t++) {
        /* Clear spike outputs */
        memset(bridge->spikes, 0, TSDN_NUM_NEURONS * sizeof(float));

        for (int n = 0; n < TSDN_NUM_NEURONS; n++) {
            /* Compute synaptic input */
            float i_syn = 0.0f;

            /* Input current */
            if ((uint32_t)n < input_size) {
                i_syn += input[n] * bridge->weights[n];
            }

            /* Recurrent connections */
            for (int m = 0; m < TSDN_NUM_NEURONS; m++) {
                i_syn += bridge->spikes[m] *
                         bridge->weights[TSDN_NUM_NEURONS + n * TSDN_NUM_NEURONS + m];
            }

            i_syn += bridge->biases[n];

            /* LIF dynamics */
            bridge->membrane_potentials[n] = lif_update(
                bridge->membrane_potentials[n], i_syn, tau, dt);

            /* Spike generation */
            if (bridge->membrane_potentials[n] >= v_thresh) {
                bridge->spikes[n] = 1.0f;
                bridge->spike_times[n] = (float)t;
                bridge->membrane_potentials[n] = v_reset;
                total_spikes++;
            }
        }
    }

    bridge->stats.spikes_total += total_spikes;
    bridge->stats.avg_spike_rate =
        (float)total_spikes / (TSDN_NUM_NEURONS * timesteps) * 1000.0f;

    return 0;
}

int dragonfly_snn_get_spikes(
    const dragonfly_snn_bridge_t* bridge,
    float* spikes,
    uint32_t max_spikes
) {
    if (!bridge || !spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_get_spikes: required parameter is NULL (bridge, spikes)");
        return -1;
    }

    uint32_t count = max_spikes < TSDN_NUM_NEURONS ? max_spikes : TSDN_NUM_NEURONS;
    memcpy(spikes, bridge->spikes, count * sizeof(float));

    return (int)count;
}

int dragonfly_snn_get_potentials(
    const dragonfly_snn_bridge_t* bridge,
    float* potentials,
    uint32_t num_neurons
) {
    if (!bridge || !potentials) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_get_potentials: required parameter is NULL (bridge, potentials)");
        return -1;
    }

    uint32_t count = num_neurons < TSDN_NUM_NEURONS ? num_neurons : TSDN_NUM_NEURONS;
    memcpy(potentials, bridge->membrane_potentials, count * sizeof(float));

    return 0;
}

//=============================================================================
// Backward Pass
//=============================================================================

float dragonfly_snn_compute_loss(
    dragonfly_snn_bridge_t* bridge,
    const float* target,
    uint32_t target_size
) {
    if (!bridge || !target) return -1.0f;

    float loss = 0.0f;
    uint32_t n = target_size < TSDN_NUM_NEURONS ? target_size : TSDN_NUM_NEURONS;

    switch (bridge->config.loss_type) {
        case DRAGONFLY_LOSS_SPIKE_COUNT:
            for (uint32_t i = 0; i < n; i++) {
                float diff = bridge->spikes[i] - target[i];
                loss += diff * diff;
            }
            break;

        case DRAGONFLY_LOSS_RATE_MSE:
        case DRAGONFLY_LOSS_DIRECTION_ERROR:
            for (uint32_t i = 0; i < n; i++) {
                float diff = bridge->membrane_potentials[i] - target[i];
                loss += diff * diff;
            }
            break;

        default:
            for (uint32_t i = 0; i < n; i++) {
                float diff = bridge->spikes[i] - target[i];
                loss += diff * diff;
            }
    }

    loss = (n > 0) ? (loss / n) : 0.0f;
    bridge->current_loss = loss;

    return loss;
}

int dragonfly_snn_backward(dragonfly_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_backward: bridge is NULL");
        return -1;
    }

    float beta = bridge->config.surrogate.beta;
    dragonfly_surrogate_t method = bridge->config.surrogate.method;

    /* Clear gradients */
    memset(bridge->weight_gradients, 0, bridge->num_weights * sizeof(float));
    memset(bridge->bias_gradients, 0, bridge->num_biases * sizeof(float));

    float grad_norm = 0.0f;

    /* Compute surrogate gradients */
    for (int n = 0; n < TSDN_NUM_NEURONS; n++) {
        float v = bridge->membrane_potentials[n] -
                  bridge->config.neuron_params.v_threshold;
        float surrogate = compute_surrogate(v, method, beta);

        /* Gradient for bias */
        bridge->bias_gradients[n] = surrogate * bridge->current_loss;

        /* Gradient for weights */
        for (int m = 0; m < TSDN_NUM_NEURONS; m++) {
            uint32_t idx = TSDN_NUM_NEURONS + n * TSDN_NUM_NEURONS + m;
            bridge->weight_gradients[idx] =
                surrogate * bridge->spikes[m] * bridge->current_loss;
            grad_norm += bridge->weight_gradients[idx] * bridge->weight_gradients[idx];
        }
    }

    grad_norm = sqrtf(grad_norm);
    bridge->gradient_norm = grad_norm;
    {
        float new_avg = (bridge->stats.gradient_norm_avg * bridge->step_count + grad_norm) /
            (bridge->step_count + 1);
        if (isfinite(new_avg)) bridge->stats.gradient_norm_avg = new_avg;
    }

    /* Gradient clipping */
    if (grad_norm > bridge->config.gradient_clip) {
        float scale = bridge->config.gradient_clip / grad_norm;
        for (uint32_t i = 0; i < bridge->num_weights; i++) {
            bridge->weight_gradients[i] *= scale;
        }
        for (uint32_t i = 0; i < bridge->num_biases; i++) {
            bridge->bias_gradients[i] *= scale;
        }
        bridge->gradients_clipped = true;
        bridge->stats.gradient_clips++;
    } else {
        bridge->gradients_clipped = false;
    }

    return 0;
}

int dragonfly_snn_get_gradients(
    const dragonfly_snn_bridge_t* bridge,
    dragonfly_snn_gradients_t* gradients
) {
    if (!bridge || !gradients) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_get_gradients: required parameter is NULL (bridge, gradients)");
        return -1;
    }

    gradients->weight_gradients = bridge->weight_gradients;
    gradients->bias_gradients = bridge->bias_gradients;
    gradients->num_weights = bridge->num_weights;
    gradients->num_biases = bridge->num_biases;
    gradients->gradient_norm = bridge->gradient_norm;
    gradients->clipped = bridge->gradients_clipped;

    return 0;
}

int dragonfly_snn_apply_gradients(dragonfly_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_apply_gradients: bridge is NULL");
        return -1;
    }

    float lr = bridge->config.learning_rate;
    float wd = bridge->config.weight_decay;

    /* Update weights with gradient descent + weight decay */
    for (uint32_t i = 0; i < bridge->num_weights; i++) {
        bridge->weights[i] -= lr * (bridge->weight_gradients[i] +
                                    wd * bridge->weights[i]);
    }

    /* Update biases */
    for (uint32_t i = 0; i < bridge->num_biases; i++) {
        bridge->biases[i] -= lr * bridge->bias_gradients[i];
    }

    return 0;
}

//=============================================================================
// Training
//=============================================================================

float dragonfly_snn_train_step(
    dragonfly_snn_bridge_t* bridge,
    const float* input,
    uint32_t input_size,
    const float* target,
    uint32_t target_size,
    uint32_t timesteps
) {
    if (!bridge || !bridge->is_training) return -1.0f;

    /* Forward pass */
    dragonfly_snn_forward(bridge, input, input_size, timesteps);

    /* Compute loss */
    float loss = dragonfly_snn_compute_loss(bridge, target, target_size);

    /* Backward pass */
    dragonfly_snn_backward(bridge);

    /* Apply gradients */
    dragonfly_snn_apply_gradients(bridge);

    /* Update statistics */
    bridge->step_count++;
    bridge->stats.training_steps++;
    bridge->stats.current_loss = loss;
    bridge->stats.avg_loss =
        (bridge->stats.avg_loss * (bridge->stats.training_steps - 1) + loss) /
        bridge->stats.training_steps;

    if (loss < bridge->stats.min_loss || bridge->stats.min_loss == 0.0f) {
        bridge->stats.min_loss = loss;
    }

    bridge->stats.learning_rate_current = bridge->config.learning_rate;

    return loss;
}

int dragonfly_snn_apply_reward(
    dragonfly_snn_bridge_t* bridge,
    float reward
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_apply_reward: bridge is NULL");
        return -1;
    }

    float advantage = reward - bridge->config.reward_baseline;
    if (bridge->config.use_advantage) {
        /* Update baseline with exponential moving average */
        bridge->config.reward_baseline =
            NIMCP_EMA_DECAY_DEFAULT * bridge->config.reward_baseline + NIMCP_LEARNING_RATE_DEFAULT * reward;
    } else {
        advantage = reward;
    }

    /* Modulate eligibility traces by reward */
    for (uint32_t i = 0; i < bridge->num_weights; i++) {
        bridge->weights[i] += bridge->config.learning_rate *
                              advantage * bridge->eligibility_traces[i];
    }

    bridge->stats.avg_reward =
        (bridge->stats.avg_reward * bridge->stats.training_steps + reward) /
        (bridge->stats.training_steps + 1);

    return 0;
}

int dragonfly_snn_update_eligibility(
    dragonfly_snn_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_update_eligibility: bridge is NULL");
        return -1;
    }

    float tau = bridge->config.eligibility.tau_eligibility;
    float decay = expf(-dt_ms / tau);

    /* Decay eligibility traces */
    for (uint32_t i = 0; i < bridge->num_weights; i++) {
        bridge->eligibility_traces[i] *= decay;
    }

    /* Add new traces from current spikes */
    float beta = bridge->config.surrogate.beta;
    dragonfly_surrogate_t method = bridge->config.surrogate.method;

    for (int n = 0; n < TSDN_NUM_NEURONS; n++) {
        float v = bridge->membrane_potentials[n] -
                  bridge->config.neuron_params.v_threshold;
        float surrogate = compute_surrogate(v, method, beta);

        for (int m = 0; m < TSDN_NUM_NEURONS; m++) {
            uint32_t idx = TSDN_NUM_NEURONS + n * TSDN_NUM_NEURONS + m;
            bridge->eligibility_traces[idx] += surrogate * bridge->spikes[m];
        }
    }

    /* Decay reward trace */
    float tau_r = bridge->config.eligibility.tau_reward;
    bridge->reward_trace *= expf(-dt_ms / tau_r);

    return 0;
}

int dragonfly_snn_set_learning_rate(dragonfly_snn_bridge_t* bridge, float lr) {
    if (!bridge || lr <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_snn_set_learning_rate: bridge is NULL or lr <= 0");
        return -1;
    }
    bridge->config.learning_rate = lr;
    bridge->stats.learning_rate_current = lr;
    return 0;
}

float dragonfly_snn_decay_learning_rate(dragonfly_snn_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    float lr = bridge->config.learning_rate * bridge->config.lr_decay;
    if (lr < bridge->config.min_learning_rate) {
        lr = bridge->config.min_learning_rate;
    }

    bridge->config.learning_rate = lr;
    bridge->stats.learning_rate_current = lr;

    return lr;
}

//=============================================================================
// TSDN Integration
//=============================================================================

int dragonfly_snn_sync_from_tsdn(dragonfly_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_sync_from_tsdn: bridge is NULL");
        return -1;
    }
    /* Would sync from dragonfly TSDN module if connected */
    return 0;
}

int dragonfly_snn_push_to_tsdn(dragonfly_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_push_to_tsdn: bridge is NULL");
        return -1;
    }
    /* Would push trained weights to dragonfly TSDN */
    return 0;
}

float dragonfly_snn_get_direction_error(
    const dragonfly_snn_bridge_t* bridge,
    float target_azimuth,
    float target_elevation
) {
    if (!bridge) return -1.0f;

    /* Decode direction from TSDN population */
    float decoded_az = 0.0f, decoded_el = 0.0f;
    float total_weight = 0.0f;

    for (int i = 0; i < TSDN_NUM_NEURONS; i++) {
        float az = ((float)i / TSDN_NUM_NEURONS) * 360.0f - 180.0f;
        float el = ((float)(i % 4) / 4.0f) * 180.0f - 90.0f;

        decoded_az += az * bridge->spikes[i];
        decoded_el += el * bridge->spikes[i];
        total_weight += bridge->spikes[i];
    }

    if (total_weight > 0.0f) {
        decoded_az /= total_weight;
        decoded_el /= total_weight;
    }

    /* Compute angular error */
    float az_err = fabsf(decoded_az - target_azimuth);
    float el_err = fabsf(decoded_el - target_elevation);

    if (az_err > 180.0f) az_err = 360.0f - az_err;

    float error = sqrtf(az_err * az_err + el_err * el_err);

    return error;
}

//=============================================================================
// Integration
//=============================================================================

int dragonfly_snn_connect_dragonfly(
    dragonfly_snn_bridge_t* bridge,
    dragonfly_system_t* dragonfly
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_connect_dragonfly: bridge is NULL");
        return -1;
    }
    bridge->dragonfly = dragonfly;
    return 0;
}

int dragonfly_snn_connect_trainer(
    dragonfly_snn_bridge_t* bridge,
    void* trainer
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_connect_trainer: bridge is NULL");
        return -1;
    }
    bridge->snn_trainer = trainer;
    return 0;
}

bool dragonfly_snn_is_training(const dragonfly_snn_bridge_t* bridge) {
    return bridge ? bridge->is_training : false;
}

int dragonfly_snn_set_training(dragonfly_snn_bridge_t* bridge, bool training) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_set_training: bridge is NULL");
        return -1;
    }
    bridge->is_training = training;
    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_snn_bridge_get_stats(
    const dragonfly_snn_bridge_t* bridge,
    dragonfly_snn_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int dragonfly_snn_bridge_reset_stats(dragonfly_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_snn_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_snn_algorithm_name(dragonfly_snn_algorithm_t algorithm) {
    switch (algorithm) {
        case DRAGONFLY_TRAIN_BPTT:           return "bptt";
        case DRAGONFLY_TRAIN_TRUNCATED_BPTT: return "truncated_bptt";
        case DRAGONFLY_TRAIN_EPROP:          return "eprop";
        case DRAGONFLY_TRAIN_REWARD_STDP:    return "reward_stdp";
        case DRAGONFLY_TRAIN_HYBRID:         return "hybrid";
        default:                              return "unknown";
    }
}

const char* dragonfly_snn_surrogate_name(dragonfly_surrogate_t method) {
    switch (method) {
        case DRAGONFLY_SURROGATE_SUPERSPIKE:    return "superspike";
        case DRAGONFLY_SURROGATE_FAST_SIGMOID:  return "fast_sigmoid";
        case DRAGONFLY_SURROGATE_TRIANGULAR:    return "triangular";
        case DRAGONFLY_SURROGATE_EXPONENTIAL:   return "exponential";
        default:                                 return "unknown";
    }
}

const char* dragonfly_snn_loss_name(dragonfly_snn_loss_t loss) {
    switch (loss) {
        case DRAGONFLY_LOSS_SPIKE_COUNT:      return "spike_count";
        case DRAGONFLY_LOSS_SPIKE_TIMING:     return "spike_timing";
        case DRAGONFLY_LOSS_RATE_MSE:         return "rate_mse";
        case DRAGONFLY_LOSS_DIRECTION_ERROR:  return "direction_error";
        case DRAGONFLY_LOSS_INTERCEPTION:     return "interception";
        case DRAGONFLY_LOSS_COMBINED:         return "combined";
        default:                               return "unknown";
    }
}
