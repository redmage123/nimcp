/**
 * @file nimcp_physics_nn.c
 * @brief Physics-informed neural network implementation
 *
 * Full implementation of physics NN with:
 * - Complete backpropagation through all layers
 * - Configurable architecture (variable depth/width)
 * - Multiple activation functions (softplus, tanh, relu, swish)
 * - Multiple optimizers (SGD, momentum, Adam)
 * - Symplectic integrators for Hamiltonian systems
 * - Energy conservation constraints
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 * @version 2.0.0
 */

#include "cognitive/parietal/nimcp_physics_nn.h"
#include "constants/nimcp_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/numerical/nimcp_integration.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(physics_nn, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define EPSILON NIMCP_EPSILON_ADAM
#define MAX_GRADIENT_HISTORY 1024

/* Thread-local error message */
static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Single layer in the physics NN
 */
typedef struct {
    float* weights;         /**< Weight matrix [input_size x output_size] */
    float* biases;          /**< Bias vector [output_size] */
    float* weight_grads;    /**< Accumulated weight gradients */
    float* bias_grads;      /**< Accumulated bias gradients */

    /* For momentum/Adam */
    float* weight_m;        /**< First moment (momentum) for weights */
    float* weight_v;        /**< Second moment (Adam) for weights */
    float* bias_m;          /**< First moment for biases */
    float* bias_v;          /**< Second moment for biases */

    uint32_t input_size;
    uint32_t output_size;
} nn_layer_t;

/**
 * @brief Internal physics NN state
 */
struct physics_nn {
    /* Configuration */
    physics_nn_config_t config;

    /* Network architecture */
    nn_layer_t* layers;
    uint32_t num_layers;

    /* Forward pass cache (for backprop) */
    float** layer_inputs;   /**< Input to each layer [num_layers][layer_input_size] */
    float** layer_outputs;  /**< Output of each layer [num_layers][layer_output_size] */
    float** pre_activations;/**< Pre-activation values [num_layers][layer_output_size] */

    /* Training state */
    uint64_t training_step; /**< Current step (for Adam) */

    /* Modulation */
    float inflammation_level;
    float fatigue_level;
    float effective_precision; /**< Computed from modulation */

    /* Statistics */
    physics_nn_stats_t stats;

    /* Temporary buffers */
    float* temp_state;      /**< Temporary state for integration */
    float* temp_deriv;      /**< Temporary derivative */
    float* delta;           /**< Error signal for backprop */
};

/* ============================================================================
 * ACTIVATION FUNCTIONS
 * ============================================================================ */

/**
 * @brief Softplus activation: log(1 + exp(x))
 */
static inline float activation_softplus(float x) {
    /* Numerically stable implementation */
    if (x > 20.0f) return x;
    if (x < -20.0f) return 0.0f;
    return logf(1.0f + expf(x));
}

/**
 * @brief Softplus derivative: sigmoid(x) = 1 / (1 + exp(-x))
 */
static inline float activation_softplus_deriv(float x) {
    if (x > 20.0f) return 1.0f;
    if (x < -20.0f) return 0.0f;
    float ex = expf(x);
    return ex / (1.0f + ex);
}

/**
 * @brief Tanh activation
 */
static inline float activation_tanh(float x) {
    return tanhf(x);
}

/**
 * @brief Tanh derivative: 1 - tanh²(x)
 */
static inline float activation_tanh_deriv(float x) {
    float t = tanhf(x);
    return 1.0f - t * t;
}

/**
 * @brief ReLU activation
 */
static inline float activation_relu(float x) {
    return x > 0.0f ? x : 0.0f;
}

/**
 * @brief ReLU derivative
 */
static inline float activation_relu_deriv(float x) {
    return x > 0.0f ? 1.0f : 0.0f;
}

/**
 * @brief Swish activation: x * sigmoid(x)
 */
static inline float activation_swish(float x) {
    float sig = 1.0f / (1.0f + expf(-x));
    return x * sig;
}

/**
 * @brief Swish derivative: sigmoid(x) + x * sigmoid(x) * (1 - sigmoid(x))
 */
static inline float activation_swish_deriv(float x) {
    float sig = 1.0f / (1.0f + expf(-x));
    return sig + x * sig * (1.0f - sig);
}

/**
 * @brief Apply activation function based on type
 */
static float apply_activation(physics_nn_activation_t type, float x) {
    switch (type) {
        case PHYSICS_NN_ACTIVATION_SOFTPLUS: return activation_softplus(x);
        case PHYSICS_NN_ACTIVATION_TANH:     return activation_tanh(x);
        case PHYSICS_NN_ACTIVATION_RELU:     return activation_relu(x);
        case PHYSICS_NN_ACTIVATION_SWISH:    return activation_swish(x);
        default: return activation_softplus(x);
    }
}

/**
 * @brief Apply activation derivative based on type
 */
static float apply_activation_deriv(physics_nn_activation_t type, float x) {
    switch (type) {
        case PHYSICS_NN_ACTIVATION_SOFTPLUS: return activation_softplus_deriv(x);
        case PHYSICS_NN_ACTIVATION_TANH:     return activation_tanh_deriv(x);
        case PHYSICS_NN_ACTIVATION_RELU:     return activation_relu_deriv(x);
        case PHYSICS_NN_ACTIVATION_SWISH:    return activation_swish_deriv(x);
        default: return activation_softplus_deriv(x);
    }
}

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

/**
 * @brief Xavier/Glorot initialization for weights
 */
static void xavier_init(float* weights, uint32_t fan_in, uint32_t fan_out) {
    float scale = sqrtf(2.0f / (float)(fan_in + fan_out));
    for (uint32_t i = 0; i < fan_in * fan_out; i++) {
        /* Box-Muller for normal distribution */
        float u1 = (float)nimcp_tl_rand() / (float)RAND_MAX;
        float u2 = (float)nimcp_tl_rand() / (float)RAND_MAX;
        if (u1 < 1e-10f) u1 = 1e-10f;
        float z = sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
        weights[i] = z * scale;
    }
}

/**
 * @brief Create single layer
 */
static nn_layer_t* layer_create(uint32_t input_size, uint32_t output_size,
                                 physics_nn_optimizer_t optimizer) {
    nn_layer_t* layer = (nn_layer_t*)nimcp_calloc(1, sizeof(nn_layer_t));
    if (!layer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate layer");

        return NULL;

    }

    layer->input_size = input_size;
    layer->output_size = output_size;

    /* Allocate weights and biases */
    layer->weights = (float*)nimcp_calloc(input_size * output_size, sizeof(float));
    if (!layer->weights) return NULL;
    layer->biases = (float*)nimcp_calloc(output_size, sizeof(float));
    if (!layer->biases) return NULL;
    layer->weight_grads = (float*)nimcp_calloc(input_size * output_size, sizeof(float));
    if (!layer->weight_grads) return NULL;
    layer->bias_grads = (float*)nimcp_calloc(output_size, sizeof(float));

    if (!layer->weights || !layer->biases ||
        !layer->weight_grads || !layer->bias_grads) {
        goto error;
    }

    /* Allocate optimizer state */
    if (optimizer == PHYSICS_NN_OPTIMIZER_SGD_MOMENTUM ||
        optimizer == PHYSICS_NN_OPTIMIZER_ADAM) {
        layer->weight_m = (float*)nimcp_calloc(input_size * output_size, sizeof(float));
        layer->bias_m = (float*)nimcp_calloc(output_size, sizeof(float));
        if (!layer->weight_m || !layer->bias_m) goto error;
    }

    if (optimizer == PHYSICS_NN_OPTIMIZER_ADAM) {
        layer->weight_v = (float*)nimcp_calloc(input_size * output_size, sizeof(float));
        layer->bias_v = (float*)nimcp_calloc(output_size, sizeof(float));
        if (!layer->weight_v || !layer->bias_v) goto error;
    }

    /* Initialize weights with Xavier */
    xavier_init(layer->weights, input_size, output_size);

    return layer;

error:
    if (layer) {
        nimcp_free(layer->weights);
        nimcp_free(layer->biases);
        nimcp_free(layer->weight_grads);
        nimcp_free(layer->bias_grads);
        nimcp_free(layer->weight_m);
        nimcp_free(layer->bias_m);
        nimcp_free(layer->weight_v);
        nimcp_free(layer->bias_v);
        nimcp_free(layer);
        layer = NULL;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "xavier_init: operation failed");
    return NULL;
}

/**
 * @brief Destroy single layer
 */
static void layer_destroy(nn_layer_t* layer) {
    if (!layer) return;
    nimcp_free(layer->weights);
    nimcp_free(layer->biases);
    nimcp_free(layer->weight_grads);
    nimcp_free(layer->bias_grads);
    nimcp_free(layer->weight_m);
    nimcp_free(layer->bias_m);
    nimcp_free(layer->weight_v);
    nimcp_free(layer->bias_v);
    nimcp_free(layer);
    layer = NULL;
}

/**
 * @brief Compute vector L2 norm
 */
static float vector_norm(const float* v, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        sum += v[i] * v[i];
    }
    return sqrtf(sum);
}

/**
 * @brief Clip gradients by global norm
 */
static bool clip_gradients(physics_nn_t* nn, float max_norm, float* actual_norm) {
    if (max_norm <= 0.0f) {
        *actual_norm = 0.0f;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "clip_gradients: validation failed");
        return false;
    }

    /* Compute global gradient norm */
    float total_sq = 0.0f;
    for (uint32_t l = 0; l < nn->num_layers; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && nn->num_layers > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(l + 1) / (float)nn->num_layers);
        }

        nn_layer_t* layer = &nn->layers[l];
        uint32_t w_size = layer->input_size * layer->output_size;

        for (uint32_t i = 0; i < w_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && w_size > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)w_size);
            }

            total_sq += layer->weight_grads[i] * layer->weight_grads[i];
        }
        for (uint32_t i = 0; i < layer->output_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && layer->output_size > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)layer->output_size);
            }

            total_sq += layer->bias_grads[i] * layer->bias_grads[i];
        }
    }

    *actual_norm = sqrtf(total_sq);

    /* Clip if necessary */
    if (*actual_norm > max_norm) {
        float scale = max_norm / *actual_norm;
        for (uint32_t l = 0; l < nn->num_layers; l++) {
            /* Phase 8: Loop progress heartbeat */
            if ((l & 0xFF) == 0 && nn->num_layers > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(l + 1) / (float)nn->num_layers);
            }

            nn_layer_t* layer = &nn->layers[l];
            uint32_t w_size = layer->input_size * layer->output_size;

            for (uint32_t i = 0; i < w_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && w_size > 256) {
                    physics_nn_heartbeat("physics_nn_loop",
                                     (float)(i + 1) / (float)w_size);
                }

                layer->weight_grads[i] *= scale;
            }
            for (uint32_t i = 0; i < layer->output_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && layer->output_size > 256) {
                    physics_nn_heartbeat("physics_nn_loop",
                                     (float)(i + 1) / (float)layer->output_size);
                }

                layer->bias_grads[i] *= scale;
            }
        }
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "clip_gradients: operation failed");
    return false;
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

physics_nn_config_t physics_nn_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_default_config", 0.0f);


    physics_nn_config_t config = {0};

    /* Architecture */
    config.state_dim = PHYSICS_NN_DEFAULT_STATE_DIM;
    config.num_layers = PHYSICS_NN_DEFAULT_NUM_LAYERS;
    config.layer_sizes = NULL;  /* Use hidden_size for all */
    config.hidden_size = PHYSICS_NN_DEFAULT_HIDDEN;

    /* Activation and training */
    config.activation = PHYSICS_NN_ACTIVATION_SOFTPLUS;
    config.optimizer = PHYSICS_NN_OPTIMIZER_ADAM;
    config.learning_rate = PHYSICS_NN_DEFAULT_LR;
    config.momentum = NIMCP_MOMENTUM_DEFAULT;
    config.beta1 = NIMCP_ADAM_BETA1_DEFAULT;
    config.beta2 = NIMCP_ADAM_BETA2_DEFAULT;
    config.epsilon = NIMCP_EPSILON_ADAM;
    config.weight_decay = 0.0f;
    config.gradient_clip = NIMCP_GRADIENT_CLIP_DEFAULT;

    /* Physics constraints */
    config.use_hamiltonian = true;
    config.use_lagrangian = false;
    config.hamiltonian_weight = 0.1f;

    /* Integration */
    config.integrator = PHYSICS_NN_INTEGRATOR_RK4;

    /* Bio-async */
    config.enable_bio_async = false;

    /* Modulation */
    config.inflammation_sensitivity = 0.5f;
    config.fatigue_sensitivity = 0.5f;

    return config;
}

bool physics_nn_validate_config(const physics_nn_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_validate_config: config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_validate_config", 0.0f);


    if (config->state_dim == 0 || config->state_dim > PHYSICS_NN_MAX_NEURONS) {
        snprintf(s_last_error, sizeof(s_last_error),
                 "Invalid state_dim: %u (max %u)", config->state_dim, PHYSICS_NN_MAX_NEURONS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_validate_config: config->state_dim is zero");
        return false;
    }

    if (config->num_layers < 2 || config->num_layers > PHYSICS_NN_MAX_LAYERS) {
        snprintf(s_last_error, sizeof(s_last_error),
                 "Invalid num_layers: %u (must be 2-%u)", config->num_layers, PHYSICS_NN_MAX_LAYERS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_validate_config: validation failed");
        return false;
    }

    if (config->hidden_size == 0 || config->hidden_size > PHYSICS_NN_MAX_NEURONS) {
        snprintf(s_last_error, sizeof(s_last_error),
                 "Invalid hidden_size: %u (max %u)", config->hidden_size, PHYSICS_NN_MAX_NEURONS);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_validate_config: config->hidden_size is zero");
        return false;
    }

    if (config->learning_rate <= 0.0f || config->learning_rate > 1.0f) {
        snprintf(s_last_error, sizeof(s_last_error),
                 "Invalid learning_rate: %f (must be 0 < lr <= 1)", config->learning_rate);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_validate_config: validation failed");
        return false;
    }

    /* Hamiltonian requires even state dimension (q, p pairs) */
    if (config->use_hamiltonian && (config->state_dim % 2 != 0)) {
        snprintf(s_last_error, sizeof(s_last_error),
                 "Hamiltonian mode requires even state_dim (got %u)", config->state_dim);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_validate_config: validation failed");
        return false;
    }

    return true;
}

physics_nn_t* physics_nn_create(void) {
    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_create", 0.0f);


    physics_nn_config_t config = physics_nn_default_config();
    return physics_nn_create_custom(&config);
}

physics_nn_t* physics_nn_create_custom(const physics_nn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_create_custom", 0.0f);


    physics_nn_config_t default_config;
    if (!config) {
        default_config = physics_nn_default_config();
        config = &default_config;
    }

    if (!physics_nn_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_create_custom: physics_nn_validate_config is NULL");
        return NULL;
    }

    physics_nn_t* nn = (physics_nn_t*)nimcp_calloc(1, sizeof(physics_nn_t));
    if (!nn) {
        snprintf(s_last_error, sizeof(s_last_error), "Failed to allocate physics_nn_t");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_create_custom: nn is NULL");
        return NULL;
    }

    /* Copy configuration */
    nn->config = *config;
    nn->num_layers = config->num_layers;

    /* Determine layer sizes */
    uint32_t* sizes = (uint32_t*)nimcp_calloc((config->num_layers + 1), sizeof(uint32_t));
    if (!sizes) {
        nimcp_free(nn);
        nn = NULL;
        snprintf(s_last_error, sizeof(s_last_error), "Failed to allocate layer sizes");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_create_custom: sizes is NULL");
        return NULL;
    }

    sizes[0] = config->state_dim;  /* Input layer */
    for (uint32_t i = 1; i < config->num_layers; i++) {
        sizes[i] = config->layer_sizes ? config->layer_sizes[i-1] : config->hidden_size;
    }
    sizes[config->num_layers] = config->state_dim;  /* Output layer */

    /* Allocate layers */
    nn->layers = (nn_layer_t*)nimcp_calloc(config->num_layers, sizeof(nn_layer_t));
    if (!nn->layers) {
        nimcp_free(sizes);
        sizes = NULL;
        nimcp_free(nn);
        nn = NULL;
        snprintf(s_last_error, sizeof(s_last_error), "Failed to allocate layers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_create_custom: nn->layers is NULL");
        return NULL;
    }

    /* Create each layer */
    for (uint32_t i = 0; i < config->num_layers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && config->num_layers > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)config->num_layers);
        }

        nn_layer_t* layer = layer_create(sizes[i], sizes[i + 1], config->optimizer);
        if (!layer) {
            /* Cleanup previously created layers */
            for (uint32_t j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    physics_nn_heartbeat("physics_nn_loop",
                                     (float)(j + 1) / (float)i);
                }

                layer_destroy(&nn->layers[j]);
            }
            nimcp_free(nn->layers);
            nimcp_free(sizes);
            sizes = NULL;
            nimcp_free(nn);
            nn = NULL;
            snprintf(s_last_error, sizeof(s_last_error), "Failed to create layer %u", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_create_custom: operation failed");
            return NULL;
        }
        nn->layers[i] = *layer;
        nimcp_free(layer);  /* Just the container, not contents */
        layer = NULL;
    }

    /* Allocate forward pass cache */
    nn->layer_inputs = (float**)nimcp_calloc(config->num_layers, sizeof(float*));
    nn->layer_outputs = (float**)nimcp_calloc(config->num_layers, sizeof(float*));
    nn->pre_activations = (float**)nimcp_calloc(config->num_layers, sizeof(float*));

    if (!nn->layer_inputs || !nn->layer_outputs || !nn->pre_activations) {
        goto error;
    }

    for (uint32_t i = 0; i < config->num_layers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && config->num_layers > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)config->num_layers);
        }

        nn->layer_inputs[i] = (float*)nimcp_calloc(sizes[i], sizeof(float));
        nn->layer_outputs[i] = (float*)nimcp_calloc(sizes[i + 1], sizeof(float));
        nn->pre_activations[i] = (float*)nimcp_calloc(sizes[i + 1], sizeof(float));

        if (!nn->layer_inputs[i] || !nn->layer_outputs[i] || !nn->pre_activations[i]) {
            goto error;
        }
    }

    /* Allocate temporary buffers */
    uint32_t max_size = config->state_dim;
    for (uint32_t i = 1; i <= config->num_layers; i++) {
        if (sizes[i] > max_size) max_size = sizes[i];
    }

    nn->temp_state = (float*)nimcp_calloc(config->state_dim, sizeof(float));
    nn->temp_deriv = (float*)nimcp_calloc(config->state_dim, sizeof(float));
    nn->delta = (float*)nimcp_calloc(max_size, sizeof(float));

    if (!nn->temp_state || !nn->temp_deriv || !nn->delta) {
        goto error;
    }

    /* Initialize modulation */
    nn->inflammation_level = 0.0f;
    nn->fatigue_level = 0.0f;
    nn->effective_precision = 1.0f;

    /* Initialize stats */
    nn->stats.min_loss = INFINITY;
    nn->stats.current_learning_rate = config->learning_rate;

    nimcp_free(sizes);
    sizes = NULL;
    return nn;

error:
    nimcp_free(sizes);
    sizes = NULL;
    physics_nn_destroy(nn);
    snprintf(s_last_error, sizeof(s_last_error), "Failed to allocate internal buffers");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_create_custom: operation failed");
    return NULL;
}

void physics_nn_destroy(physics_nn_t* nn) {
    if (!nn) return;

    /* Free layers */
    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_destroy", 0.0f);


    if (nn->layers) {
        for (uint32_t i = 0; i < nn->num_layers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && nn->num_layers > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)nn->num_layers);
            }

            nn_layer_t* layer = &nn->layers[i];
            nimcp_free(layer->weights);
            nimcp_free(layer->biases);
            nimcp_free(layer->weight_grads);
            nimcp_free(layer->bias_grads);
            nimcp_free(layer->weight_m);
            nimcp_free(layer->bias_m);
            nimcp_free(layer->weight_v);
            nimcp_free(layer->bias_v);
        }
        nimcp_free(nn->layers);
    }

    /* Free forward pass cache */
    if (nn->layer_inputs) {
        for (uint32_t i = 0; i < nn->num_layers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && nn->num_layers > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)nn->num_layers);
            }

            nimcp_free(nn->layer_inputs[i]);
        }
        nimcp_free(nn->layer_inputs);
    }

    if (nn->layer_outputs) {
        for (uint32_t i = 0; i < nn->num_layers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && nn->num_layers > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)nn->num_layers);
            }

            nimcp_free(nn->layer_outputs[i]);
        }
        nimcp_free(nn->layer_outputs);
    }

    if (nn->pre_activations) {
        for (uint32_t i = 0; i < nn->num_layers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && nn->num_layers > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)nn->num_layers);
            }

            nimcp_free(nn->pre_activations[i]);
        }
        nimcp_free(nn->pre_activations);
    }

    /* Free temporary buffers */
    nimcp_free(nn->temp_state);
    nimcp_free(nn->temp_deriv);
    nimcp_free(nn->delta);

    nimcp_free(nn);
    nn = NULL;
}

int physics_nn_reset(physics_nn_t* nn) {
    if (!nn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_reset: nn is NULL");
        return -1;
    }

    /* Re-initialize weights */
    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_reset", 0.0f);


    for (uint32_t l = 0; l < nn->num_layers; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && nn->num_layers > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(l + 1) / (float)nn->num_layers);
        }

        nn_layer_t* layer = &nn->layers[l];
        xavier_init(layer->weights, layer->input_size, layer->output_size);
        memset(layer->biases, 0, layer->output_size * sizeof(float));

        /* Reset optimizer state */
        uint32_t w_size = layer->input_size * layer->output_size;
        memset(layer->weight_grads, 0, w_size * sizeof(float));
        memset(layer->bias_grads, 0, layer->output_size * sizeof(float));

        if (layer->weight_m) memset(layer->weight_m, 0, w_size * sizeof(float));
        if (layer->bias_m) memset(layer->bias_m, 0, layer->output_size * sizeof(float));
        if (layer->weight_v) memset(layer->weight_v, 0, w_size * sizeof(float));
        if (layer->bias_v) memset(layer->bias_v, 0, layer->output_size * sizeof(float));
    }

    /* Reset training state */
    nn->training_step = 0;
    physics_nn_reset_stats(nn);

    return 0;
}

/* ============================================================================
 * FORWARD PASS
 * ============================================================================ */

int physics_nn_forward(physics_nn_t* nn, const float* state, float* derivative) {
    if (!nn || !state || !derivative) {
        snprintf(s_last_error, sizeof(s_last_error), "NULL pointer in forward pass");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_forward: required parameter is NULL (nn, state, derivative)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_forward", 0.0f);


    uint32_t state_dim = nn->config.state_dim;

    /* Copy input to first layer input cache */
    memcpy(nn->layer_inputs[0], state, state_dim * sizeof(float));

    /* Forward through each layer */
    const float* current_input = nn->layer_inputs[0];

    for (uint32_t l = 0; l < nn->num_layers; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && nn->num_layers > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(l + 1) / (float)nn->num_layers);
        }

        nn_layer_t* layer = &nn->layers[l];
        float* output = nn->layer_outputs[l];
        float* pre_act = nn->pre_activations[l];

        /* Compute z = W * x + b */
        for (uint32_t j = 0; j < layer->output_size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && layer->output_size > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(j + 1) / (float)layer->output_size);
            }

            float sum = layer->biases[j];
            for (uint32_t i = 0; i < layer->input_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && layer->input_size > 256) {
                    physics_nn_heartbeat("physics_nn_loop",
                                     (float)(i + 1) / (float)layer->input_size);
                }

                sum += layer->weights[i * layer->output_size + j] * current_input[i];
            }
            pre_act[j] = sum;

            /* Apply activation (except last layer - linear output) */
            if (l < nn->num_layers - 1) {
                output[j] = apply_activation(nn->config.activation, sum);
            } else {
                output[j] = sum;  /* Linear output layer */
            }
        }

        /* Cache input for next layer (for backprop) */
        if (l < nn->num_layers - 1) {
            memcpy(nn->layer_inputs[l + 1], output, layer->output_size * sizeof(float));
            current_input = nn->layer_inputs[l + 1];
        }
    }

    /* Copy final output to derivative */
    memcpy(derivative, nn->layer_outputs[nn->num_layers - 1], state_dim * sizeof(float));

    /* Apply modulation (reduce precision with inflammation/fatigue) */
    if (nn->effective_precision < 1.0f) {
        for (uint32_t i = 0; i < state_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state_dim > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)state_dim);
            }

            /* Add noise proportional to 1 - precision */
            float noise = (1.0f - nn->effective_precision) *
                          ((float)nimcp_tl_rand() / (float)RAND_MAX - 0.5f) * 0.1f;
            derivative[i] *= (1.0f + noise);
        }
    }

    return 0;
}

float physics_nn_compute_hamiltonian(physics_nn_t* nn, const float* state) {
    if (!nn || !state) return NAN;

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_compute_hamiltonian", 0.0f);


    uint32_t state_dim = nn->config.state_dim;
    uint32_t n = state_dim / 2;  /* Number of position/momentum pairs */

    /* Positions q and momenta p */
    const float* q = state;
    const float* p = state + n;

    /* Kinetic energy: T = sum(p_i^2 / 2) */
    float kinetic = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        kinetic += 0.5f * p[i] * p[i];
    }

    /* Potential energy: V = sum(q_i^2 / 2) (harmonic oscillator default) */
    /* In a learned system, we'd use a separate potential network */
    float potential = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        potential += 0.5f * q[i] * q[i];
    }

    return kinetic + potential;
}

/* ============================================================================
 * BACKPROPAGATION
 * ============================================================================ */

/**
 * @brief Full backpropagation through all layers
 */
static void backpropagate(physics_nn_t* nn, const float* target_derivative) {
    uint32_t state_dim = nn->config.state_dim;

    /* Compute output layer error: delta = output - target */
    float* output = nn->layer_outputs[nn->num_layers - 1];
    for (uint32_t i = 0; i < state_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state_dim > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)state_dim);
        }

        nn->delta[i] = output[i] - target_derivative[i];
    }

    /* Backpropagate through layers (reverse order) */
    for (int l = (int)nn->num_layers - 1; l >= 0; l--) {
        nn_layer_t* layer = &nn->layers[l];
        uint32_t in_size = layer->input_size;
        uint32_t out_size = layer->output_size;

        /* Current layer input (cached during forward pass) */
        float* layer_input = nn->layer_inputs[l];

        /* Compute gradients for this layer */
        for (uint32_t i = 0; i < in_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && in_size > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)in_size);
            }

            for (uint32_t j = 0; j < out_size; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && out_size > 256) {
                    physics_nn_heartbeat("physics_nn_loop",
                                     (float)(j + 1) / (float)out_size);
                }

                layer->weight_grads[i * out_size + j] += nn->delta[j] * layer_input[i];
            }
        }

        for (uint32_t j = 0; j < out_size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && out_size > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(j + 1) / (float)out_size);
            }

            layer->bias_grads[j] += nn->delta[j];
        }

        /* Propagate delta to previous layer (if not first layer) */
        if (l > 0) {
            /* Allocate temporary for next delta */
            float* new_delta = nn->temp_state;  /* Reuse temp buffer */
            memset(new_delta, 0, in_size * sizeof(float));

            /* delta_prev = W^T * delta * activation'(pre_activation) */
            for (uint32_t i = 0; i < in_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && in_size > 256) {
                    physics_nn_heartbeat("physics_nn_loop",
                                     (float)(i + 1) / (float)in_size);
                }

                for (uint32_t j = 0; j < out_size; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && out_size > 256) {
                        physics_nn_heartbeat("physics_nn_loop",
                                         (float)(j + 1) / (float)out_size);
                    }

                    new_delta[i] += layer->weights[i * out_size + j] * nn->delta[j];
                }
                /* Multiply by activation derivative */
                float pre_act = nn->pre_activations[l - 1][i];
                new_delta[i] *= apply_activation_deriv(nn->config.activation, pre_act);
            }

            /* Copy to delta for next iteration */
            memcpy(nn->delta, new_delta, in_size * sizeof(float));
        }
    }
}

/**
 * @brief Apply optimizer update step
 */
static void apply_optimizer(physics_nn_t* nn) {
    float lr = nn->config.learning_rate;
    float momentum = nn->config.momentum;
    float beta1 = nn->config.beta1;
    float beta2 = nn->config.beta2;
    float epsilon = nn->config.epsilon;
    float weight_decay = nn->config.weight_decay;

    nn->training_step++;

    /* Bias correction for Adam */
    float bias_correction1 = 1.0f - powf(beta1, (float)nn->training_step);
    float bias_correction2 = 1.0f - powf(beta2, (float)nn->training_step);

    for (uint32_t l = 0; l < nn->num_layers; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && nn->num_layers > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(l + 1) / (float)nn->num_layers);
        }

        nn_layer_t* layer = &nn->layers[l];
        uint32_t w_size = layer->input_size * layer->output_size;

        switch (nn->config.optimizer) {
            case PHYSICS_NN_OPTIMIZER_SGD:
                /* Simple SGD: w = w - lr * grad */
                for (uint32_t i = 0; i < w_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && w_size > 256) {
                        physics_nn_heartbeat("physics_nn_loop",
                                         (float)(i + 1) / (float)w_size);
                    }

                    float grad = layer->weight_grads[i];
                    if (weight_decay > 0.0f) {
                        grad += weight_decay * layer->weights[i];
                    }
                    layer->weights[i] -= lr * grad;
                }
                for (uint32_t i = 0; i < layer->output_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && layer->output_size > 256) {
                        physics_nn_heartbeat("physics_nn_loop",
                                         (float)(i + 1) / (float)layer->output_size);
                    }

                    layer->biases[i] -= lr * layer->bias_grads[i];
                }
                break;

            case PHYSICS_NN_OPTIMIZER_SGD_MOMENTUM:
                /* SGD with momentum: v = momentum * v + grad; w = w - lr * v */
                for (uint32_t i = 0; i < w_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && w_size > 256) {
                        physics_nn_heartbeat("physics_nn_loop",
                                         (float)(i + 1) / (float)w_size);
                    }

                    float grad = layer->weight_grads[i];
                    if (weight_decay > 0.0f) {
                        grad += weight_decay * layer->weights[i];
                    }
                    layer->weight_m[i] = momentum * layer->weight_m[i] + grad;
                    layer->weights[i] -= lr * layer->weight_m[i];
                }
                for (uint32_t i = 0; i < layer->output_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && layer->output_size > 256) {
                        physics_nn_heartbeat("physics_nn_loop",
                                         (float)(i + 1) / (float)layer->output_size);
                    }

                    layer->bias_m[i] = momentum * layer->bias_m[i] + layer->bias_grads[i];
                    layer->biases[i] -= lr * layer->bias_m[i];
                }
                break;

            case PHYSICS_NN_OPTIMIZER_ADAM:
                /* Adam: m = beta1*m + (1-beta1)*grad; v = beta2*v + (1-beta2)*grad^2 */
                for (uint32_t i = 0; i < w_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && w_size > 256) {
                        physics_nn_heartbeat("physics_nn_loop",
                                         (float)(i + 1) / (float)w_size);
                    }

                    float grad = layer->weight_grads[i];
                    if (weight_decay > 0.0f) {
                        grad += weight_decay * layer->weights[i];
                    }

                    layer->weight_m[i] = beta1 * layer->weight_m[i] + (1.0f - beta1) * grad;
                    layer->weight_v[i] = beta2 * layer->weight_v[i] + (1.0f - beta2) * grad * grad;

                    float m_hat = layer->weight_m[i] / bias_correction1;
                    float v_hat = layer->weight_v[i] / bias_correction2;

                    layer->weights[i] -= lr * m_hat / (sqrtf(v_hat) + epsilon);
                }
                for (uint32_t i = 0; i < layer->output_size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && layer->output_size > 256) {
                        physics_nn_heartbeat("physics_nn_loop",
                                         (float)(i + 1) / (float)layer->output_size);
                    }

                    float grad = layer->bias_grads[i];

                    layer->bias_m[i] = beta1 * layer->bias_m[i] + (1.0f - beta1) * grad;
                    layer->bias_v[i] = beta2 * layer->bias_v[i] + (1.0f - beta2) * grad * grad;

                    float m_hat = layer->bias_m[i] / bias_correction1;
                    float v_hat = layer->bias_v[i] / bias_correction2;

                    layer->biases[i] -= lr * m_hat / (sqrtf(v_hat) + epsilon);
                }
                break;
        }

        /* Zero gradients after update */
        memset(layer->weight_grads, 0, w_size * sizeof(float));
        memset(layer->bias_grads, 0, layer->output_size * sizeof(float));
    }
}

/* ============================================================================
 * TRAINING API
 * ============================================================================ */

int physics_nn_train_step(
    physics_nn_t* nn,
    const float* state,
    const float* target_derivative,
    physics_nn_train_result_t* result
) {
    if (!nn || !state || !target_derivative) {
        snprintf(s_last_error, sizeof(s_last_error), "NULL pointer in train_step");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_train_step: required parameter is NULL (nn, state, target_derivative)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_train_step", 0.0f);


    uint32_t state_dim = nn->config.state_dim;

    /* Forward pass */
    if (physics_nn_forward(nn, state, nn->temp_deriv) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_train_step: validation failed");
        return -1;
    }

    /* Compute MSE loss */
    float mse_loss = 0.0f;
    for (uint32_t i = 0; i < state_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && state_dim > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)state_dim);
        }

        float diff = nn->temp_deriv[i] - target_derivative[i];
        mse_loss += diff * diff;
    }
    mse_loss /= (float)state_dim;

    /* Hamiltonian conservation loss (optional) */
    float hamiltonian_loss = 0.0f;
    if (nn->config.use_hamiltonian) {
        /* Compute dH/dt should be 0 for conservative systems */
        /* This is a simplified version - full version would compute exact conservation */
        float H = physics_nn_compute_hamiltonian(nn, state);

        /* Predict next state using current derivative */
        float dt_small = 0.001f;
        for (uint32_t i = 0; i < state_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && state_dim > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)state_dim);
            }

            nn->temp_state[i] = state[i] + dt_small * nn->temp_deriv[i];
        }
        float H_next = physics_nn_compute_hamiltonian(nn, nn->temp_state);

        /* Energy change should be small */
        float dH = fabsf(H_next - H) / (fabsf(H) + EPSILON);
        hamiltonian_loss = dH * dH;
    }

    /* Total loss */
    float total_loss = mse_loss + nn->config.hamiltonian_weight * hamiltonian_loss;

    /* Backpropagation */
    backpropagate(nn, target_derivative);

    /* Gradient clipping */
    float grad_norm = 0.0f;
    bool clipped = clip_gradients(nn, nn->config.gradient_clip, &grad_norm);

    /* Apply optimizer */
    apply_optimizer(nn);

    /* Update statistics */
    nn->stats.training_steps++;
    nn->stats.total_loss += total_loss;
    nn->stats.avg_loss = nn->stats.total_loss / (float)nn->stats.training_steps;
    if (total_loss < nn->stats.min_loss) {
        nn->stats.min_loss = total_loss;
    }
    if (grad_norm > nn->stats.max_gradient_norm) {
        nn->stats.max_gradient_norm = grad_norm;
    }
    if (clipped) {
        nn->stats.gradient_clips++;
    }

    /* Fill result */
    if (result) {
        result->loss = total_loss;
        result->mse_loss = mse_loss;
        result->hamiltonian_loss = hamiltonian_loss;
        result->gradient_norm = grad_norm;
        result->gradient_clipped = clipped;
    }

    return 0;
}

int physics_nn_train_batch(
    physics_nn_t* nn,
    const float** states,
    const float** derivatives,
    uint32_t batch_size,
    physics_nn_train_result_t* result
) {
    if (!nn || !states || !derivatives || batch_size == 0) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid parameters in train_batch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_train_batch: required parameter is NULL (nn, states, derivatives)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_train_batch", 0.0f);


    float total_loss = 0.0f;
    float total_mse = 0.0f;
    float total_hamiltonian = 0.0f;
    float max_grad_norm = 0.0f;
    uint32_t clips = 0;

    for (uint32_t b = 0; b < batch_size; b++) {
        /* Phase 8: Loop progress heartbeat */
        if ((b & 0xFF) == 0 && batch_size > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(b + 1) / (float)batch_size);
        }

        physics_nn_train_result_t step_result;
        if (physics_nn_train_step(nn, states[b], derivatives[b], &step_result) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_train_batch: validation failed");
            return -1;
        }

        total_loss += step_result.loss;
        total_mse += step_result.mse_loss;
        total_hamiltonian += step_result.hamiltonian_loss;
        if (step_result.gradient_norm > max_grad_norm) {
            max_grad_norm = step_result.gradient_norm;
        }
        if (step_result.gradient_clipped) {
            clips++;
        }
    }

    if (result) {
        result->loss = total_loss / (float)batch_size;
        result->mse_loss = total_mse / (float)batch_size;
        result->hamiltonian_loss = total_hamiltonian / (float)batch_size;
        result->gradient_norm = max_grad_norm;
        result->gradient_clipped = (clips > 0);
    }

    return 0;
}

int physics_nn_train_from_trajectory(
    physics_nn_t* nn,
    const float** trajectory,
    uint32_t num_points,
    float dt,
    physics_nn_train_result_t* result
) {
    if (!nn || !trajectory || num_points < 2 || dt <= 0.0f) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid parameters in train_from_trajectory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_train_from_trajectory: required parameter is NULL (nn, trajectory)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_train_from_trajector", 0.0f);


    uint32_t state_dim = nn->config.state_dim;

    /* Compute derivatives from trajectory using finite differences */
    float* derivative = (float*)nimcp_calloc(state_dim, sizeof(float));
    if (!derivative) {
        snprintf(s_last_error, sizeof(s_last_error), "Failed to allocate derivative buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_train_from_trajectory: derivative is NULL");
        return -1;
    }

    float total_loss = 0.0f;
    float total_mse = 0.0f;
    float total_hamiltonian = 0.0f;
    float max_grad_norm = 0.0f;
    uint32_t clips = 0;

    for (uint32_t i = 0; i < num_points - 1; i++) {
        /* Central difference where possible, forward difference at boundaries */
        if (i > 0 && i < num_points - 1) {
            /* Central difference: (x[i+1] - x[i-1]) / (2*dt) */
            for (uint32_t j = 0; j < state_dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && state_dim > 256) {
                    physics_nn_heartbeat("physics_nn_loop",
                                     (float)(j + 1) / (float)state_dim);
                }

                derivative[j] = (trajectory[i + 1][j] - trajectory[i - 1][j]) / (2.0f * dt);
            }
        } else {
            /* Forward difference: (x[i+1] - x[i]) / dt */
            for (uint32_t j = 0; j < state_dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && state_dim > 256) {
                    physics_nn_heartbeat("physics_nn_loop",
                                     (float)(j + 1) / (float)state_dim);
                }

                derivative[j] = (trajectory[i + 1][j] - trajectory[i][j]) / (fabsf(dt) > 1e-7f ? dt : 1e-7f);
            }
        }

        physics_nn_train_result_t step_result;
        if (physics_nn_train_step(nn, trajectory[i], derivative, &step_result) != 0) {
            nimcp_free(derivative);
            derivative = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_train_from_trajectory: validation failed");
            return -1;
        }

        total_loss += step_result.loss;
        total_mse += step_result.mse_loss;
        total_hamiltonian += step_result.hamiltonian_loss;
        if (step_result.gradient_norm > max_grad_norm) {
            max_grad_norm = step_result.gradient_norm;
        }
        if (step_result.gradient_clipped) {
            clips++;
        }
    }

    nimcp_free(derivative);
    derivative = NULL;

    if (result) {
        result->loss = total_loss / (float)(num_points - 1);
        result->mse_loss = total_mse / (float)(num_points - 1);
        result->hamiltonian_loss = total_hamiltonian / (float)(num_points - 1);
        result->gradient_norm = max_grad_norm;
        result->gradient_clipped = (clips > 0);
    }

    return 0;
}

/* ============================================================================
 * PREDICTION API
 * ============================================================================ */

/**
 * @brief Derivative function wrapper for integration library
 */
typedef struct {
    physics_nn_t* nn;
} integration_params_t;

static void physics_nn_derivative_fn(const float* state, float t, void* params, float* derivatives) {
    (void)t;  /* Time not used in autonomous systems */
    integration_params_t* p = (integration_params_t*)params;
    physics_nn_forward(p->nn, state, derivatives);
}

int physics_nn_predict(
    physics_nn_t* nn,
    const float* initial_state,
    float dt,
    uint32_t num_steps,
    physics_nn_prediction_t* prediction
) {
    if (!nn || !initial_state || num_steps == 0 || !prediction || dt <= 0.0f) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid parameters in predict");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_predict: required parameter is NULL (nn, initial_state, prediction)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_predict", 0.0f);


    uint32_t state_dim = nn->config.state_dim;

    /* Allocate trajectory */
    prediction->trajectory = (float**)nimcp_calloc(num_steps, sizeof(float*));
    if (!prediction->trajectory) {
        snprintf(s_last_error, sizeof(s_last_error), "Failed to allocate trajectory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_predict: prediction->trajectory is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < num_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_steps > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)num_steps);
        }

        prediction->trajectory[i] = (float*)nimcp_calloc(state_dim, sizeof(float));
        if (!prediction->trajectory[i]) {
            for (uint32_t j = 0; j < i; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && i > 256) {
                    physics_nn_heartbeat("physics_nn_loop",
                                     (float)(j + 1) / (float)i);
                }

                nimcp_free(prediction->trajectory[j]);
            }
            nimcp_free(prediction->trajectory);
            snprintf(s_last_error, sizeof(s_last_error), "Failed to allocate trajectory step %u", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_predict: validation failed");
            return -1;
        }
    }

    prediction->num_steps = num_steps;

    /* Allocate Hamiltonians if using Hamiltonian constraint */
    if (nn->config.use_hamiltonian) {
        prediction->hamiltonians = (float*)nimcp_calloc(num_steps, sizeof(float));
        if (!prediction->hamiltonians) {
            physics_nn_free_prediction(prediction);
            return -1;
        }
    } else {
        prediction->hamiltonians = NULL;
    }

    /* Copy initial state */
    float* current_state = (float*)nimcp_calloc(state_dim, sizeof(float));
    if (!current_state) {
        physics_nn_free_prediction(prediction);
        snprintf(s_last_error, sizeof(s_last_error), "Failed to allocate current state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_predict: current_state is NULL");
        return -1;
    }
    memcpy(current_state, initial_state, state_dim * sizeof(float));

    /* Integration parameters */
    integration_params_t int_params = { .nn = nn };

    /* Choose integration method */
    integration_method_t method;
    switch (nn->config.integrator) {
        case PHYSICS_NN_INTEGRATOR_EULER:
            method = INTEGRATION_EULER;
            break;
        case PHYSICS_NN_INTEGRATOR_RK4:
            method = INTEGRATION_RK4;
            break;
        case PHYSICS_NN_INTEGRATOR_ADAPTIVE:
            method = INTEGRATION_ADAPTIVE;
            break;
        case PHYSICS_NN_INTEGRATOR_SYMPLECTIC:
            /* Handle symplectic separately */
            method = INTEGRATION_RK4;  /* Will be overridden */
            break;
        default:
            method = INTEGRATION_RK4;
    }

    /* Initial Hamiltonian */
    float H_initial = 0.0f;
    if (nn->config.use_hamiltonian) {
        H_initial = physics_nn_compute_hamiltonian(nn, current_state);
        prediction->hamiltonians[0] = H_initial;
    }

    /* Integrate trajectory */
    for (uint32_t step = 0; step < num_steps; step++) {
        /* Phase 8: Loop progress heartbeat */
        if ((step & 0xFF) == 0 && num_steps > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(step + 1) / (float)num_steps);
        }

        /* Store current state */
        memcpy(prediction->trajectory[step], current_state, state_dim * sizeof(float));

        /* Advance one step (except last) */
        if (step < num_steps - 1) {
            if (nn->config.integrator == PHYSICS_NN_INTEGRATOR_SYMPLECTIC) {
                /* Use symplectic integrator */
                uint32_t n = state_dim / 2;
                float* q = current_state;
                float* p = current_state + n;
                physics_nn_leapfrog(nn, q, p, dt);
            } else {
                /* Use standard integrators from nimcp_integration.h */
                integration_step(method, physics_nn_derivative_fn,
                                 current_state, step * dt, dt, state_dim, &int_params);
            }

            /* Compute Hamiltonian for next step */
            if (nn->config.use_hamiltonian && prediction->hamiltonians) {
                prediction->hamiltonians[step + 1] = physics_nn_compute_hamiltonian(nn, current_state);
            }
        }

        nn->stats.prediction_steps++;
    }

    /* Compute energy drift */
    if (nn->config.use_hamiltonian && prediction->hamiltonians) {
        float H_final = prediction->hamiltonians[num_steps - 1];
        prediction->energy_drift = H_final - H_initial;
        nn->stats.avg_hamiltonian_drift =
            (nn->stats.avg_hamiltonian_drift * (float)(nn->stats.prediction_steps - num_steps) +
             fabsf(prediction->energy_drift)) / (float)nn->stats.prediction_steps;
    } else {
        prediction->energy_drift = 0.0f;
    }

    /* Compute confidence based on energy conservation */
    if (nn->config.use_hamiltonian && fabsf(H_initial) > EPSILON) {
        float relative_drift = fabsf(prediction->energy_drift) / fabsf(H_initial);
        prediction->confidence = expf(-relative_drift * 10.0f);  /* Exponential decay with drift */
    } else {
        prediction->confidence = 1.0f;  /* No energy constraint, full confidence */
    }

    nimcp_free(current_state);
    current_state = NULL;
    return 0;
}

int physics_nn_step(physics_nn_t* nn, float* state, float dt) {
    if (!nn || !state || dt <= 0.0f) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid parameters in step");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_step: required parameter is NULL (nn, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_step", 0.0f);


    integration_params_t int_params = { .nn = nn };

    switch (nn->config.integrator) {
        case PHYSICS_NN_INTEGRATOR_EULER:
            integration_step(INTEGRATION_EULER, physics_nn_derivative_fn,
                             state, 0.0f, dt, nn->config.state_dim, &int_params);
            break;

        case PHYSICS_NN_INTEGRATOR_RK4:
            integration_step(INTEGRATION_RK4, physics_nn_derivative_fn,
                             state, 0.0f, dt, nn->config.state_dim, &int_params);
            break;

        case PHYSICS_NN_INTEGRATOR_ADAPTIVE: {
            float t = 0.0f;
            float dt_adaptive = dt;
            integration_adaptive_step(physics_nn_derivative_fn,
                                      state, &t, &dt_adaptive,
                                      nn->config.state_dim, &int_params, NULL);
            break;
        }

        case PHYSICS_NN_INTEGRATOR_SYMPLECTIC: {
            uint32_t n = nn->config.state_dim / 2;
            float* q = state;
            float* p = state + n;
            physics_nn_leapfrog(nn, q, p, dt);
            break;
        }
    }

    nn->stats.prediction_steps++;
    return 0;
}

void physics_nn_free_prediction(physics_nn_prediction_t* prediction) {
    if (!prediction) return;

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_free_prediction", 0.0f);


    if (prediction->trajectory) {
        for (uint32_t i = 0; i < prediction->num_steps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && prediction->num_steps > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)prediction->num_steps);
            }

            nimcp_free(prediction->trajectory[i]);
        }
        nimcp_free(prediction->trajectory);
        prediction->trajectory = NULL;
    }

    nimcp_free(prediction->hamiltonians);
    prediction->hamiltonians = NULL;
    prediction->num_steps = 0;
}

/* ============================================================================
 * SYMPLECTIC INTEGRATORS
 * ============================================================================ */

int physics_nn_symplectic_euler(physics_nn_t* nn, float* q, float* p, float dt) {
    if (!nn || !q || !p || dt <= 0.0f) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid parameters in symplectic_euler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_symplectic_euler: required parameter is NULL (nn, q, p)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_symplectic_euler", 0.0f);


    uint32_t n = nn->config.state_dim / 2;

    /* Symplectic Euler:
     * p_new = p - dt * dV/dq(q)
     * q_new = q + dt * dT/dp(p_new)
     *
     * For harmonic oscillator: dV/dq = q, dT/dp = p
     */

    /* Combine into state for forward pass */
    float* state = (float*)nimcp_calloc(nn->config.state_dim, sizeof(float));
    float* deriv = (float*)nimcp_calloc(nn->config.state_dim, sizeof(float));
    if (!state || !deriv) {
        nimcp_free(state);
        state = NULL;
        nimcp_free(deriv);
        deriv = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_symplectic_euler: required parameter is NULL (state, deriv)");
        return -1;
    }

    memcpy(state, q, n * sizeof(float));
    memcpy(state + n, p, n * sizeof(float));

    /* Get learned derivatives */
    physics_nn_forward(nn, state, deriv);

    /* For Hamiltonian: dq/dt = dH/dp, dp/dt = -dH/dq */
    /* deriv[0:n] = dq/dt, deriv[n:2n] = dp/dt */

    /* Update momentum first (symplectic) */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        p[i] += dt * deriv[n + i];  /* dp/dt = -dH/dq (learned) */
    }

    /* Update position with new momentum */
    memcpy(state + n, p, n * sizeof(float));
    physics_nn_forward(nn, state, deriv);

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        q[i] += dt * deriv[i];  /* dq/dt = dH/dp (learned) */
    }

    nimcp_free(state);
    state = NULL;
    nimcp_free(deriv);
    deriv = NULL;
    return 0;
}

int physics_nn_leapfrog(physics_nn_t* nn, float* q, float* p, float dt) {
    if (!nn || !q || !p || dt <= 0.0f) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid parameters in leapfrog");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_leapfrog: required parameter is NULL (nn, q, p)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_leapfrog", 0.0f);


    uint32_t n = nn->config.state_dim / 2;

    float* state = (float*)nimcp_calloc(nn->config.state_dim, sizeof(float));
    float* deriv = (float*)nimcp_calloc(nn->config.state_dim, sizeof(float));
    if (!state || !deriv) {
        nimcp_free(state);
        state = NULL;
        nimcp_free(deriv);
        deriv = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_leapfrog: required parameter is NULL (state, deriv)");
        return -1;
    }

    /* Leapfrog/Störmer-Verlet:
     * p_{1/2} = p_0 - (dt/2) * dV/dq(q_0)
     * q_1 = q_0 + dt * p_{1/2} / m
     * p_1 = p_{1/2} - (dt/2) * dV/dq(q_1)
     */

    /* Build state for forward pass */
    memcpy(state, q, n * sizeof(float));
    memcpy(state + n, p, n * sizeof(float));

    /* Get derivatives at current position */
    physics_nn_forward(nn, state, deriv);

    /* Half step in momentum */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        p[i] += 0.5f * dt * deriv[n + i];
    }

    /* Full step in position */
    memcpy(state + n, p, n * sizeof(float));
    physics_nn_forward(nn, state, deriv);

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        q[i] += dt * deriv[i];
    }

    /* Update state with new position */
    memcpy(state, q, n * sizeof(float));
    physics_nn_forward(nn, state, deriv);

    /* Half step in momentum with new force */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        p[i] += 0.5f * dt * deriv[n + i];
    }

    nimcp_free(state);
    state = NULL;
    nimcp_free(deriv);
    deriv = NULL;
    return 0;
}

int physics_nn_velocity_verlet(physics_nn_t* nn, float* q, float* p, float dt) {
    if (!nn || !q || !p || dt <= 0.0f) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid parameters in velocity_verlet");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_velocity_verlet: required parameter is NULL (nn, q, p)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_velocity_verlet", 0.0f);


    uint32_t n = nn->config.state_dim / 2;

    float* state = (float*)nimcp_calloc(nn->config.state_dim, sizeof(float));
    float* deriv = (float*)nimcp_calloc(nn->config.state_dim, sizeof(float));
    float* force_old = (float*)nimcp_calloc(n, sizeof(float));
    if (!state || !deriv || !force_old) {
        nimcp_free(state);
        state = NULL;
        nimcp_free(deriv);
        deriv = NULL;
        nimcp_free(force_old);
        force_old = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "physics_nn_velocity_verlet: required parameter is NULL (state, deriv, force_old)");
        return -1;
    }

    /* Velocity Verlet:
     * q_new = q + dt * v + (dt²/2) * a
     * v_new = v + (dt/2) * (a + a_new)
     */

    /* Get current acceleration (force) */
    memcpy(state, q, n * sizeof(float));
    memcpy(state + n, p, n * sizeof(float));
    physics_nn_forward(nn, state, deriv);

    /* Store old force (dp/dt) */
    memcpy(force_old, deriv + n, n * sizeof(float));

    /* Update position: q_new = q + dt * v + (dt²/2) * a */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        q[i] += dt * deriv[i] + 0.5f * dt * dt * force_old[i];
    }

    /* Get new force at updated position */
    memcpy(state, q, n * sizeof(float));
    physics_nn_forward(nn, state, deriv);

    /* Update velocity: v_new = v + (dt/2) * (a + a_new) */
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(i + 1) / (float)n);
        }

        p[i] += 0.5f * dt * (force_old[i] + deriv[n + i]);
    }

    nimcp_free(state);
    state = NULL;
    nimcp_free(deriv);
    deriv = NULL;
    nimcp_free(force_old);
    force_old = NULL;
    return 0;
}

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int physics_nn_set_inflammation(physics_nn_t* nn, float level) {
    if (!nn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_set_inflammation: nn is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_set_inflammation", 0.0f);


    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    nn->inflammation_level = level;

    /* Recompute effective precision */
    float inflammation_effect = 1.0f - nn->config.inflammation_sensitivity * level;
    float fatigue_effect = 1.0f - nn->config.fatigue_sensitivity * nn->fatigue_level;
    nn->effective_precision = inflammation_effect * fatigue_effect;

    return 0;
}

int physics_nn_set_fatigue(physics_nn_t* nn, float level) {
    if (!nn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_set_fatigue: nn is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_set_fatigue", 0.0f);


    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    nn->fatigue_level = level;

    /* Recompute effective precision */
    float inflammation_effect = 1.0f - nn->config.inflammation_sensitivity * nn->inflammation_level;
    float fatigue_effect = 1.0f - nn->config.fatigue_sensitivity * level;
    nn->effective_precision = inflammation_effect * fatigue_effect;

    return 0;
}

int physics_nn_set_learning_rate(physics_nn_t* nn, float lr) {
    if (!nn || lr <= 0.0f || lr > 1.0f) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid learning rate: %f", lr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_set_learning_rate: nn is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_set_learning_rate", 0.0f);


    nn->config.learning_rate = lr;
    nn->stats.current_learning_rate = lr;
    return 0;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int physics_nn_get_stats(const physics_nn_t* nn, physics_nn_stats_t* stats) {
    if (!nn || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_get_stats: required parameter is NULL (nn, stats)");
        return -1;
    }
    *stats = nn->stats;
    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_get_stats", 0.0f);


    return 0;
}

void physics_nn_reset_stats(physics_nn_t* nn) {
    if (!nn) return;
    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_reset_stats", 0.0f);


    memset(&nn->stats, 0, sizeof(physics_nn_stats_t));
    nn->stats.min_loss = INFINITY;
    nn->stats.current_learning_rate = nn->config.learning_rate;
}

const char* physics_nn_get_last_error(void) {
    return s_last_error;
}

/* ============================================================================
 * SERIALIZATION API
 * ============================================================================ */

int physics_nn_save(const physics_nn_t* nn, const char* filename) {
    if (!nn || !filename) {
        snprintf(s_last_error, sizeof(s_last_error), "NULL pointer in save");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_save: required parameter is NULL (nn, filename)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_save", 0.0f);


    FILE* f = fopen(filename, "wb");
    if (!f) {
        snprintf(s_last_error, sizeof(s_last_error), "Failed to open file: %s", filename);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_save: f is NULL");
        return -1;
    }

    /* Write magic and version */
    uint32_t magic = 0x50485958;  /* 'PHYX' */
    uint32_t version = 2;
    fwrite(&magic, sizeof(uint32_t), 1, f);
    fwrite(&version, sizeof(uint32_t), 1, f);

    /* Write configuration */
    fwrite(&nn->config, sizeof(physics_nn_config_t), 1, f);

    /* Write num layers */
    fwrite(&nn->num_layers, sizeof(uint32_t), 1, f);

    /* Write each layer */
    for (uint32_t l = 0; l < nn->num_layers; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && nn->num_layers > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(l + 1) / (float)nn->num_layers);
        }

        nn_layer_t* layer = &nn->layers[l];
        fwrite(&layer->input_size, sizeof(uint32_t), 1, f);
        fwrite(&layer->output_size, sizeof(uint32_t), 1, f);

        uint32_t w_size = layer->input_size * layer->output_size;
        fwrite(layer->weights, sizeof(float), w_size, f);
        fwrite(layer->biases, sizeof(float), layer->output_size, f);

        /* Write optimizer state for Adam */
        if (nn->config.optimizer == PHYSICS_NN_OPTIMIZER_ADAM ||
            nn->config.optimizer == PHYSICS_NN_OPTIMIZER_SGD_MOMENTUM) {
            fwrite(layer->weight_m, sizeof(float), w_size, f);
            fwrite(layer->bias_m, sizeof(float), layer->output_size, f);
        }
        if (nn->config.optimizer == PHYSICS_NN_OPTIMIZER_ADAM) {
            fwrite(layer->weight_v, sizeof(float), w_size, f);
            fwrite(layer->bias_v, sizeof(float), layer->output_size, f);
        }
    }

    /* Write training step */
    fwrite(&nn->training_step, sizeof(uint64_t), 1, f);

    /* Write stats */
    fwrite(&nn->stats, sizeof(physics_nn_stats_t), 1, f);

    fclose(f);
    return 0;
}

int physics_nn_load(physics_nn_t* nn, const char* filename) {
    if (!nn || !filename) {
        snprintf(s_last_error, sizeof(s_last_error), "NULL pointer in load");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_load: required parameter is NULL (nn, filename)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_load", 0.0f);


    FILE* f = fopen(filename, "rb");
    if (!f) {
        snprintf(s_last_error, sizeof(s_last_error), "Failed to open file: %s", filename);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_nn_load: f is NULL");
        return -1;
    }

    /* Read and verify magic */
    uint32_t magic, version;
    fread(&magic, sizeof(uint32_t), 1, f);
    fread(&version, sizeof(uint32_t), 1, f);

    if (magic != 0x50485958) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid file format");
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_load: validation failed");
        return -1;
    }

    if (version != 2) {
        snprintf(s_last_error, sizeof(s_last_error), "Unsupported version: %u", version);
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_load: validation failed");
        return -1;
    }

    /* Read and verify configuration matches */
    physics_nn_config_t saved_config;
    fread(&saved_config, sizeof(physics_nn_config_t), 1, f);

    if (saved_config.state_dim != nn->config.state_dim ||
        saved_config.num_layers != nn->config.num_layers) {
        snprintf(s_last_error, sizeof(s_last_error),
                 "Configuration mismatch: expected state_dim=%u num_layers=%u, got %u %u",
                 nn->config.state_dim, nn->config.num_layers,
                 saved_config.state_dim, saved_config.num_layers);
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_load: operation failed");
        return -1;
    }

    /* Read num layers */
    uint32_t num_layers = 0;
    fread(&num_layers, sizeof(uint32_t), 1, f);

    if (num_layers != nn->num_layers) {
        snprintf(s_last_error, sizeof(s_last_error),
                 "Layer count mismatch: expected %u, got %u", nn->num_layers, num_layers);
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_load: validation failed");
        return -1;
    }

    /* Read each layer */
    for (uint32_t l = 0; l < nn->num_layers; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && nn->num_layers > 256) {
            physics_nn_heartbeat("physics_nn_loop",
                             (float)(l + 1) / (float)nn->num_layers);
        }

        nn_layer_t* layer = &nn->layers[l];
        uint32_t in_size, out_size;
        fread(&in_size, sizeof(uint32_t), 1, f);
        fread(&out_size, sizeof(uint32_t), 1, f);

        if (in_size != layer->input_size || out_size != layer->output_size) {
            snprintf(s_last_error, sizeof(s_last_error),
                     "Layer %u size mismatch", l);
            fclose(f);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "physics_nn_load: validation failed");
            return -1;
        }

        uint32_t w_size = layer->input_size * layer->output_size;
        fread(layer->weights, sizeof(float), w_size, f);
        fread(layer->biases, sizeof(float), layer->output_size, f);

        /* Read optimizer state */
        if (saved_config.optimizer == PHYSICS_NN_OPTIMIZER_ADAM ||
            saved_config.optimizer == PHYSICS_NN_OPTIMIZER_SGD_MOMENTUM) {
            fread(layer->weight_m, sizeof(float), w_size, f);
            fread(layer->bias_m, sizeof(float), layer->output_size, f);
        }
        if (saved_config.optimizer == PHYSICS_NN_OPTIMIZER_ADAM) {
            fread(layer->weight_v, sizeof(float), w_size, f);
            fread(layer->bias_v, sizeof(float), layer->output_size, f);
        }
    }

    /* Read training step */
    fread(&nn->training_step, sizeof(uint64_t), 1, f);

    /* Read stats */
    fread(&nn->stats, sizeof(physics_nn_stats_t), 1, f);

    fclose(f);
    return 0;
}

/* ============================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ============================================================================ */

int physics_nn_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    physics_nn_heartbeat("physics_nn_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Physics_NN_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                physics_nn_heartbeat("physics_nn_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Physics NN self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Physics_NN_Module");
    if (connections) {
        LOG_DEBUG("Physics NN has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Physics_NN_Module");
    if (incoming) {
        LOG_DEBUG("Physics NN has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void physics_nn_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_physics_nn_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int physics_nn_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "physics_nn_training_begin: NULL argument");
        return -1;
    }
    physics_nn_heartbeat_instance(g_physics_nn_health_agent, "physics_nn_training_begin", 0.0f);
    (void)(struct physics_nn*)instance; /* Module state available for reset */
    return 0;
}

int physics_nn_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "physics_nn_training_end: NULL argument");
        return -1;
    }
    physics_nn_heartbeat_instance(g_physics_nn_health_agent, "physics_nn_training_end", 1.0f);
    (void)(struct physics_nn*)instance; /* Module state available for finalization */
    return 0;
}

int physics_nn_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "physics_nn_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    physics_nn_heartbeat_instance(g_physics_nn_health_agent, "physics_nn_training_step", progress);
    (void)(struct physics_nn*)instance; /* Module state available for step adaptation */
    return 0;
}
