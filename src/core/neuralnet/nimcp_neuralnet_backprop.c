/**
 * @file nimcp_neuralnet_backprop.c
 * @brief Backpropagation implementation for weight gradient computation
 *
 * WHAT: Compute gradients for all synaptic weights using backpropagation
 * WHY:  Required for gradient-based learning (SGD, Adam, etc.)
 * HOW:  Chain rule through network layers
 *
 * ALGORITHM:
 * Forward pass:
 *   For each layer l = 1..L:
 *     z_l = W_l * a_{l-1} + b_l   (pre-activation)
 *     a_l = f(z_l)                (post-activation)
 *
 * Backward pass:
 *   For output layer:
 *     delta_L = dL/da_L * f'(z_L)
 *   For hidden layers l = L-1..1:
 *     delta_l = (W_{l+1}^T * delta_{l+1}) * f'(z_l)
 *   Weight gradients:
 *     dL/dW_l = delta_l * a_{l-1}^T
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include "core/neuralnet/nimcp_neuralnet_backprop.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#define LOG_MODULE "BACKPROP"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuralnet_backprop)

/* Single authoritative definition of neural_network_struct */
#include "core/neuralnet/nimcp_neuralnet_internal.h"

//=============================================================================
// Activation Function Derivatives
//=============================================================================

/**
 * @brief Compute activation function derivative
 *
 * @param z Pre-activation value
 * @param type Activation type
 * @return f'(z)
 */
static float activation_derivative(float z, activation_type_t type) {
    switch (type) {
        case ACTIVATION_SIGMOID: {
            float s = 1.0f / (1.0f + expf(-z));
            return s * (1.0f - s);
        }
        case ACTIVATION_TANH: {
            float t = tanhf(z);
            return 1.0f - t * t;
        }
        case ACTIVATION_RELU:
            return (z > 0.0f) ? 1.0f : 0.0f;
        case ACTIVATION_LEAKY_RELU:
            return (z > 0.0f) ? 1.0f : 0.01f;
        case ACTIVATION_ADAPTIVE:
        default:
            /* Linear/adaptive activation - derivative is 1 */
            return 1.0f;
    }
}

/**
 * @brief Compute activation function value
 *
 * @param z Pre-activation value
 * @param type Activation type
 * @return f(z)
 */
static float activation_function(float z, activation_type_t type) {
    switch (type) {
        case ACTIVATION_SIGMOID:
            return 1.0f / (1.0f + expf(-z));
        case ACTIVATION_TANH:
            return tanhf(z);
        case ACTIVATION_RELU:
            return (z > 0.0f) ? z : 0.0f;
        case ACTIVATION_LEAKY_RELU:
            return (z > 0.0f) ? z : 0.01f * z;
        case ACTIVATION_ADAPTIVE:
        default:
            /* Linear/adaptive - pass through */
            return z;
    }
}

//=============================================================================
// Lifecycle
//=============================================================================

backprop_ctx_t* backprop_create(neural_network_t network) {
    if (!network) {
        LOG_ERROR("Cannot create backprop context: NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;
    }

    backprop_ctx_t* ctx = nimcp_malloc(sizeof(backprop_ctx_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate backprop context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }
    memset(ctx, 0, sizeof(backprop_ctx_t));

    ctx->network = network;

    /* Get network configuration */
    uint32_t num_layers = network->config.num_layers;
    if (num_layers < 2 || !network->config.layer_sizes) {
        /* Network doesn't have layer structure - treat as single hidden layer */
        num_layers = 2;  /* input + output */
    }

    ctx->num_layers = num_layers;

    /* Allocate activation records for each layer */
    if (num_layers > SIZE_MAX / sizeof(layer_activation_t)) {
        nimcp_free(ctx);
        LOG_ERROR("Layer count overflow: %u layers", num_layers);
        return NULL;
    }
    ctx->activations = nimcp_malloc(num_layers * sizeof(layer_activation_t));
    if (!ctx->activations) {
        nimcp_free(ctx);
        LOG_ERROR("Failed to allocate activations array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "backprop_create: ctx->activations is NULL");
        return NULL;
    }
    memset(ctx->activations, 0, num_layers * sizeof(layer_activation_t));

    /* Allocate buffers for each layer */
    for (uint32_t l = 0; l < num_layers; l++) {
        uint32_t layer_size;
        if (network->config.layer_sizes) {
            layer_size = network->config.layer_sizes[l];
        } else {
            /* Fallback: assume input layer is half, output is the rest */
            layer_size = (l == 0) ? network->num_neurons / 2 :
                                    network->num_neurons - network->num_neurons / 2;
        }

        ctx->activations[l].size = layer_size;

        if (layer_size > SIZE_MAX / sizeof(float)) {
            backprop_destroy(ctx);
            LOG_ERROR("Layer %u size overflow: %u", l, layer_size);
            return NULL;
        }
        ctx->activations[l].pre_activation = nimcp_malloc(layer_size * sizeof(float));
        ctx->activations[l].post_activation = nimcp_malloc(layer_size * sizeof(float));

        if (!ctx->activations[l].pre_activation || !ctx->activations[l].post_activation) {
            backprop_destroy(ctx);
            LOG_ERROR("Failed to allocate layer %u activation buffers", l);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "backprop_create: required parameter is NULL (ctx->activations, ctx->activations)");
            return NULL;
        }
        memset(ctx->activations[l].pre_activation, 0, layer_size * sizeof(float));
        memset(ctx->activations[l].post_activation, 0, layer_size * sizeof(float));
    }

    /* Count total weights and neurons */
    ctx->total_weights = 0;
    ctx->total_neurons = 0;

    for (uint32_t n = 0; n < network->num_neurons; n++) {
        neuron_t* neuron = &network->neurons[n];
        ctx->total_weights += NEURON_OUT_COUNT(neuron);
    }

    /* Neurons excluding input layer */
    if (network->config.layer_sizes && num_layers > 1) {
        for (uint32_t l = 1; l < num_layers; l++) {
            ctx->total_neurons += network->config.layer_sizes[l];
        }
    } else {
        ctx->total_neurons = network->num_neurons;
    }

    /* Allocate gradient buffers */
    if (ctx->total_weights > 0) {
        if (ctx->total_weights > SIZE_MAX / sizeof(float)) {
            backprop_destroy(ctx);
            LOG_ERROR("Weight count overflow: %zu", ctx->total_weights);
            return NULL;
        }
        ctx->weight_gradients = nimcp_malloc(ctx->total_weights * sizeof(float));
        if (!ctx->weight_gradients) {
            backprop_destroy(ctx);
            LOG_ERROR("Failed to allocate weight gradients");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "backprop_create: ctx->weight_gradients is NULL");
            return NULL;
        }
        memset(ctx->weight_gradients, 0, ctx->total_weights * sizeof(float));
    }

    if (ctx->total_neurons > 0) {
        if (ctx->total_neurons > SIZE_MAX / sizeof(float)) {
            backprop_destroy(ctx);
            LOG_ERROR("Neuron count overflow: %zu", ctx->total_neurons);
            return NULL;
        }
        ctx->bias_gradients = nimcp_malloc(ctx->total_neurons * sizeof(float));
        if (!ctx->bias_gradients) {
            backprop_destroy(ctx);
            LOG_ERROR("Failed to allocate bias gradients");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "backprop_create: ctx->bias_gradients is NULL");
            return NULL;
        }
        memset(ctx->bias_gradients, 0, ctx->total_neurons * sizeof(float));
    }

    ctx->gradients_valid = false;

    LOG_INFO("Created backprop context: %u layers, %zu weights, %zu neurons",
             num_layers, ctx->total_weights, ctx->total_neurons);

    return ctx;
}

void backprop_destroy(backprop_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->activations) {
        for (uint32_t l = 0; l < ctx->num_layers; l++) {
            if (ctx->activations[l].pre_activation) {
                nimcp_free(ctx->activations[l].pre_activation);
            }
            if (ctx->activations[l].post_activation) {
                nimcp_free(ctx->activations[l].post_activation);
            }
        }
        nimcp_free(ctx->activations);
    }

    if (ctx->layer_deltas) {
        for (uint32_t l = 0; l < ctx->layer_deltas_count; l++) {
            nimcp_free(ctx->layer_deltas[l]);
        }
        nimcp_free(ctx->layer_deltas);
    }

    if (ctx->weight_gradients) {
        nimcp_free(ctx->weight_gradients);
    }
    if (ctx->bias_gradients) {
        nimcp_free(ctx->bias_gradients);
    }

    nimcp_free(ctx);
}

//=============================================================================
// Forward Pass
//=============================================================================

bool backprop_forward(backprop_ctx_t* ctx,
                      const float* inputs, uint32_t input_size,
                      float* outputs, uint32_t output_size) {
    if (!ctx || !inputs || !outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "backprop_forward: required parameter is NULL (ctx, inputs, outputs)");
        return false;
    }

    neural_network_t network = ctx->network;

    /* Store input activations (layer 0).
     * Zero-fill first so any positions beyond input_size are deterministic. */
    uint32_t in_size = ctx->activations[0].size;
    memset(ctx->activations[0].pre_activation, 0, in_size * sizeof(float));
    memset(ctx->activations[0].post_activation, 0, in_size * sizeof(float));
    for (uint32_t i = 0; i < in_size && i < input_size; i++) {
        ctx->activations[0].pre_activation[i] = inputs[i];
        ctx->activations[0].post_activation[i] = inputs[i];  /* No activation on input */
        /* Sync neuron state so layers reading .state get current values */
        if (i < network->num_neurons) {
            network->neurons[i].state = inputs[i];
        }
    }

    /* Forward propagation through layers */
    uint32_t neuron_offset = 0;
    if (network->config.layer_sizes) {
        neuron_offset = network->config.layer_sizes[0];  /* Skip input layer */
    }

    for (uint32_t l = 1; l < ctx->num_layers; l++) {
        uint32_t layer_size = ctx->activations[l].size;
        uint32_t prev_layer_size = ctx->activations[l - 1].size;

        for (uint32_t i = 0; i < layer_size; i++) {
            uint32_t neuron_id = neuron_offset + i;
            if (neuron_id >= network->num_neurons) break;

            neuron_t* neuron = &network->neurons[neuron_id];

            /* Compute weighted sum: z = sum(w * a_prev) + bias */
            float z = neuron->bias;

            /* Sum inputs from incoming synapses.
             * NOTE: For incoming synapses, target_neuron_id stores the SOURCE
             * neuron ID (set in rebuild_incoming). This is an artifact of the
             * sparse synapse API reusing the same handle struct for both directions. */
            for (uint32_t s = 0; s < NEURON_IN_COUNT(neuron); s++) {
                synapse_handle_t* syn = NEURON_IN_HANDLE(neuron, s);
                if (!syn) continue;
                uint32_t src_id = syn->target_neuron_id;  /* source ID for incoming */

                /* Find which layer and index the source is in */
                float src_activation = 0.0f;
                if (src_id < network->num_neurons) {
                    src_activation = network->neurons[src_id].state;
                }

                z += syn->weight * src_activation;
            }

            /* If no incoming synapses, use outgoing synapses from previous layer neurons */
            if (NEURON_IN_COUNT(neuron) == 0) {
                /* Previous layer neurons have outgoing synapses to this layer */
                uint32_t prev_offset = 0;
                for (uint32_t k = 0; k < l - 1; k++) {
                    prev_offset += ctx->activations[k].size;
                }

                for (uint32_t j = 0; j < prev_layer_size; j++) {
                    uint32_t prev_neuron_id = prev_offset + j;
                    if (prev_neuron_id >= network->num_neurons) continue;

                    neuron_t* prev_neuron = &network->neurons[prev_neuron_id];
                    for (uint32_t s = 0; s < NEURON_OUT_COUNT(prev_neuron); s++) {
                        synapse_handle_t* sh = NEURON_OUT_HANDLE(prev_neuron, s);
                        if (!sh) continue;
                        if (sh->target_neuron_id == neuron_id) {
                            z += sh->weight *
                                 ctx->activations[l - 1].post_activation[j];
                        }
                    }
                }
            }

            /* Store pre-activation */
            ctx->activations[l].pre_activation[i] = z;

            /* Apply activation function */
            float a = activation_function(z, neuron->activation_type);
            ctx->activations[l].post_activation[i] = a;

            /* Also update neuron state for compatibility */
            neuron->state = a;
        }

        neuron_offset += layer_size;
    }

    /* Copy output layer activations to output buffer */
    uint32_t out_layer = ctx->num_layers - 1;
    uint32_t out_size = ctx->activations[out_layer].size;
    for (uint32_t i = 0; i < out_size && i < output_size; i++) {
        outputs[i] = ctx->activations[out_layer].post_activation[i];
    }

    ctx->gradients_valid = false;  /* Gradients need recomputation */

    return true;
}

//=============================================================================
// Backward Pass
//=============================================================================

bool backprop_backward(backprop_ctx_t* ctx,
                       const float* output_gradients, uint32_t output_size) {
    if (!ctx || !output_gradients) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "backprop_backward: required parameter is NULL (ctx, output_gradients)");
        return false;
    }

    neural_network_t network = ctx->network;
    uint32_t num_layers = ctx->num_layers;

    /* Reuse or allocate per-layer delta buffers (persist across calls) */
    if (!ctx->layer_deltas || ctx->layer_deltas_count != num_layers) {
        /* Free old if resizing */
        if (ctx->layer_deltas) {
            for (uint32_t l = 0; l < ctx->layer_deltas_count; l++) {
                nimcp_free(ctx->layer_deltas[l]);
            }
            nimcp_free(ctx->layer_deltas);
        }
        ctx->layer_deltas = nimcp_malloc(num_layers * sizeof(float*));
        if (!ctx->layer_deltas) {
            LOG_ERROR("Failed to allocate layer delta array");
            return false;
        }
        ctx->layer_deltas_count = num_layers;
        for (uint32_t l = 0; l < num_layers; l++) {
            uint32_t layer_size = ctx->activations[l].size;
            ctx->layer_deltas[l] = nimcp_malloc(layer_size * sizeof(float));
            if (!ctx->layer_deltas[l]) {
                LOG_ERROR("Failed to allocate delta buffer for layer %u", l);
                return false;
            }
        }
    }
    float** layer_deltas = ctx->layer_deltas;
    /* Clear deltas for this pass */
    for (uint32_t l = 0; l < num_layers; l++) {
        memset(layer_deltas[l], 0, ctx->activations[l].size * sizeof(float));
    }

    /* Clear gradient buffers (guard NULL: buffers are NULL when total_* is 0) */
    if (ctx->weight_gradients) {
        memset(ctx->weight_gradients, 0, ctx->total_weights * sizeof(float));
    }
    if (ctx->bias_gradients) {
        memset(ctx->bias_gradients, 0, ctx->total_neurons * sizeof(float));
    }

    /* === STEP 1: Compute output layer delta === */
    uint32_t out_layer = num_layers - 1;
    uint32_t out_size = ctx->activations[out_layer].size;

    /* Compute output layer neuron offset */
    uint32_t out_offset = 0;
    for (uint32_t k = 0; k < out_layer; k++) {
        out_offset += ctx->activations[k].size;
    }

    for (uint32_t i = 0; i < out_size && i < output_size; i++) {
        float z = ctx->activations[out_layer].pre_activation[i];

        /* Use the neuron's actual activation type instead of hardcoded sigmoid */
        activation_type_t act_type = ACTIVATION_SIGMOID;
        if (out_offset + i < network->num_neurons) {
            act_type = network->neurons[out_offset + i].activation_type;
        }
        float grad_activation = activation_derivative(z, act_type);
        layer_deltas[out_layer][i] = output_gradients[i] * grad_activation;
    }

    /* === STEP 2: Backpropagate through hidden layers === */
    /* For each layer from L-1 down to 1 */
    for (int32_t l = (int32_t)num_layers - 2; l >= 1; l--) {
        uint32_t curr_layer_size = ctx->activations[l].size;
        uint32_t next_layer_size = ctx->activations[l + 1].size;

        /* Calculate neuron offsets */
        uint32_t curr_offset = 0;
        for (uint32_t k = 0; k < (uint32_t)l; k++) {
            curr_offset += ctx->activations[k].size;
        }
        uint32_t next_offset = curr_offset + curr_layer_size;

        /* For each neuron in current layer */
        for (uint32_t i = 0; i < curr_layer_size; i++) {
            float delta_sum = 0.0f;
            uint32_t curr_neuron_id = curr_offset + i;

            /* Sum: delta_l = sum_j(W_lj * delta_{l+1}_j) * f'(z_l) */
            /* Iterate through next layer neurons and find weights from curr to next */
            neuron_t* curr_neuron = NULL;
            if (curr_neuron_id < network->num_neurons) {
                curr_neuron = &network->neurons[curr_neuron_id];
            }

            if (curr_neuron) {
                /* Check outgoing synapses from current neuron */
                for (uint32_t s = 0; s < NEURON_OUT_COUNT(curr_neuron); s++) {
                    synapse_handle_t* sh = NEURON_OUT_HANDLE(curr_neuron, s);
                    if (!sh) continue;
                    uint32_t target_id = sh->target_neuron_id;

                    /* If target is in next layer */
                    if (target_id >= next_offset && target_id < next_offset + next_layer_size) {
                        uint32_t next_idx = target_id - next_offset;
                        float w = sh->weight;
                        delta_sum += w * layer_deltas[l + 1][next_idx];
                    }
                }
            }

            /* Multiply by activation derivative (use neuron's actual type) */
            float z = ctx->activations[l].pre_activation[i];
            activation_type_t act_type = curr_neuron ? curr_neuron->activation_type : ACTIVATION_SIGMOID;
            float deriv = activation_derivative(z, act_type);
            layer_deltas[l][i] = delta_sum * deriv;
        }
    }

    /* === STEP 3: Compute weight gradients === */
    /* dL/dW = a_source * delta_target */
    size_t weight_idx = 0;
    size_t bias_idx = 0;

    for (uint32_t l = 0; l < num_layers - 1; l++) {
        uint32_t curr_layer_size = ctx->activations[l].size;

        /* Calculate neuron offset for current layer */
        uint32_t curr_offset = 0;
        for (uint32_t k = 0; k < l; k++) {
            curr_offset += ctx->activations[k].size;
        }

        /* For each neuron in current layer */
        for (uint32_t i = 0; i < curr_layer_size; i++) {
            uint32_t neuron_id = curr_offset + i;
            if (neuron_id >= network->num_neurons) continue;

            neuron_t* neuron = &network->neurons[neuron_id];
            float a_source = ctx->activations[l].post_activation[i];

            /* For each outgoing synapse */
            for (uint32_t s = 0; s < NEURON_OUT_COUNT(neuron); s++) {
                synapse_handle_t* sh = NEURON_OUT_HANDLE(neuron, s);
                if (!sh) continue;
                uint32_t target_id = sh->target_neuron_id;

                /* Find which layer the target is in */
                uint32_t target_layer = 0;
                uint32_t target_offset = 0;
                uint32_t check_offset = 0;
                for (uint32_t tl = 0; tl < num_layers; tl++) {
                    if (target_id >= check_offset &&
                        target_id < check_offset + ctx->activations[tl].size) {
                        target_layer = tl;
                        target_offset = check_offset;
                        break;
                    }
                    check_offset += ctx->activations[tl].size;
                }

                uint32_t target_idx = target_id - target_offset;
                float delta_target = 0.0f;
                if (target_idx < ctx->activations[target_layer].size) {
                    delta_target = layer_deltas[target_layer][target_idx];
                }

                /* Weight gradient: dL/dW = a_source * delta_target
                 * Chain rule: gradient flows through delta (which already includes
                 * activation derivative), NOT multiplied by synapse strength.
                 * Previously multiplied by sh->strength which corrupted gradients. */
                if (weight_idx < ctx->total_weights) {
                    ctx->weight_gradients[weight_idx] = a_source * delta_target;
                }
                weight_idx++;
            }
        }
    }

    /* === STEP 4: Compute bias gradients === */
    /* dL/db = delta for each non-input neuron */
    for (uint32_t l = 1; l < num_layers; l++) {
        uint32_t layer_size = ctx->activations[l].size;
        for (uint32_t i = 0; i < layer_size; i++) {
            if (bias_idx < ctx->total_neurons) {
                ctx->bias_gradients[bias_idx] = layer_deltas[l][i];
            }
            bias_idx++;
        }
    }

    /* layer_deltas persist in ctx for reuse across calls */

    ctx->gradients_valid = true;

    LOG_DEBUG("Backprop completed: %zu weight gradients computed", ctx->total_weights);

    return true;
}

//=============================================================================
// Gradient Access
//=============================================================================

size_t backprop_get_weight_gradients(const backprop_ctx_t* ctx,
                                     float* gradients, size_t count) {
    if (!ctx || !gradients || !ctx->gradients_valid || !ctx->weight_gradients) {
        return 0;
    }

    size_t copy_count = (count < ctx->total_weights) ? count : ctx->total_weights;
    memcpy(gradients, ctx->weight_gradients, copy_count * sizeof(float));
    return copy_count;
}

size_t backprop_get_weight_count(const backprop_ctx_t* ctx) {
    if (!ctx) return 0;
    return ctx->total_weights;
}

size_t backprop_get_bias_gradients(const backprop_ctx_t* ctx,
                                   float* gradients, size_t count) {
    if (!ctx || !gradients || !ctx->gradients_valid || !ctx->bias_gradients) {
        return 0;
    }

    size_t copy_count = (count < ctx->total_neurons) ? count : ctx->total_neurons;
    memcpy(gradients, ctx->bias_gradients, copy_count * sizeof(float));
    return copy_count;
}

//=============================================================================
// Utility Functions
//=============================================================================

void backprop_clear(backprop_ctx_t* ctx) {
    if (!ctx) return;

    for (uint32_t l = 0; l < ctx->num_layers; l++) {
        if (ctx->activations[l].pre_activation) {
            memset(ctx->activations[l].pre_activation, 0,
                   ctx->activations[l].size * sizeof(float));
        }
        if (ctx->activations[l].post_activation) {
            memset(ctx->activations[l].post_activation, 0,
                   ctx->activations[l].size * sizeof(float));
        }
    }

    if (ctx->weight_gradients) {
        memset(ctx->weight_gradients, 0, ctx->total_weights * sizeof(float));
    }
    if (ctx->bias_gradients) {
        memset(ctx->bias_gradients, 0, ctx->total_neurons * sizeof(float));
    }

    ctx->gradients_valid = false;
}

bool backprop_has_valid_gradients(const backprop_ctx_t* ctx) {
    return ctx ? ctx->gradients_valid : false;
}

void backprop_store_activations_from_network(backprop_ctx_t* ctx,
                                             const float* inputs,
                                             uint32_t input_size) {
    if (!ctx || !inputs) return;

    neural_network_t network = ctx->network;
    if (!network) return;

    /* Store input activations (layer 0) */
    uint32_t in_size = ctx->activations[0].size;
    for (uint32_t i = 0; i < in_size && i < input_size; i++) {
        ctx->activations[0].pre_activation[i] = inputs[i];
        ctx->activations[0].post_activation[i] = inputs[i];
    }

    /* For other layers, extract activations from neuron states */
    /* The network's forward pass already computed neuron->state */
    /* BUGFIX: Input layer neurons are at indices 0 to layer_sizes[0]-1 */
    /* Hidden/output layers start AFTER input layer */
    uint32_t neuron_idx = ctx->activations[0].size;  /* Skip input layer neurons */

    for (uint32_t l = 1; l < ctx->num_layers; l++) {
        uint32_t layer_size = ctx->activations[l].size;

        for (uint32_t i = 0; i < layer_size; i++) {
            /* Map to actual neuron in network */
            /* After input layer, neurons are contiguous in network */
            if (neuron_idx < network->num_neurons) {
                neuron_t* neuron = &network->neurons[neuron_idx];

                /* Use neuron state as post-activation value */
                float a = neuron->state;
                ctx->activations[l].post_activation[i] = a;

                /* Reconstruct pre-activation from post-activation using
                 * inverse activation function.
                 *
                 * BUGFIX: Previously stored post-activation 'a' as pre-activation,
                 * which caused incorrect gradient computation since
                 * activation_derivative(a, type) != activation_derivative(z, type).
                 */
                float z = a;  /* default for linear/adaptive */
                if (neuron->activation_type == ACTIVATION_SIGMOID) {
                    if (a > 0.001f && a < 0.999f) {
                        z = logf(a / (1.0f - a));  /* logit (inverse sigmoid) */
                    } else {
                        z = (a <= 0.001f) ? -6.9f : 6.9f;  /* clamped extremes */
                    }
                } else if (neuron->activation_type == ACTIVATION_TANH) {
                    if (a > -0.999f && a < 0.999f) {
                        z = 0.5f * logf((1.0f + a) / (1.0f - a));  /* arctanh */
                    } else {
                        z = (a <= -0.999f) ? -3.8f : 3.8f;
                    }
                } else if (neuron->activation_type == ACTIVATION_RELU) {
                    z = (a > 0.0f) ? a : 0.0f;  /* ReLU is identity for positive */
                } else if (neuron->activation_type == ACTIVATION_LEAKY_RELU) {
                    z = (a > 0.0f) ? a : fminf(fmaxf(a / 0.01f, -20.0f), 20.0f);  /* inverse leaky ReLU, clamped */
                }
                ctx->activations[l].pre_activation[i] = z;

                neuron_idx++;
            }
        }
    }
}
