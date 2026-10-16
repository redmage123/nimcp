/**
 * @file nimcp_vae_encoder.c
 * @brief VAE Encoder Network Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Implementation of the VAE encoder that maps inputs to latent
 *       distribution parameters (mu and log_var).
 *
 * WHY:  The encoder performs variational inference, computing the
 *       approximate posterior q(z|x) as a diagonal Gaussian.
 *
 * HOW:  Forward pass through hidden layers with activations, then
 *       split into two heads for mu and log_var outputs.
 */

#include "cognitive/vae/nimcp_vae_encoder.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/math/nimcp_math_helpers.h"

/* Custom set_health_agent API (instance-level, not global) - don't use BRIDGE_BOILERPLATE */
static nimcp_health_agent_t* g_vae_encoder_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(vae_encoder, MESH_ADAPTER_CATEGORY_COGNITIVE)

static inline void vae_encoder_heartbeat(const char* op, float progress) {
    if (g_vae_encoder_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_vae_encoder_health_agent, op, progress);
    }
}

#define LOG_MODULE "VAE_ENCODER"

static inline bool is_nan_f(float value) {
    return value != value;
}

static inline bool is_inf_f(float value) {
    return value == INFINITY || value == -INFINITY;
}

/**
 * @brief Xavier/Glorot initialization
 */
static float xavier_init(uint32_t fan_in, uint32_t fan_out) {
    float limit = sqrtf(6.0f / (float)(fan_in + fan_out));
    return ((float)nimcp_tl_rand() / (float)RAND_MAX) * 2.0f * limit - limit;
}

/**
 * @brief Random number from standard normal distribution (Box-Muller)
 * Thread-safe: uses _Thread_local storage for spare value caching.
 */
static float randn(void) {
    static _Thread_local bool has_spare = false;
    static _Thread_local float spare;

    if (has_spare) {
        has_spare = false;
        return spare;
    }

    float u, v, s;
    do {
        u = ((float)nimcp_tl_rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        v = ((float)nimcp_tl_rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        s = u * u + v * v;
    } while (s >= 1.0f || s == 0.0f);

    s = sqrtf(-2.0f * logf(s) / s);
    spare = v * s;
    has_spare = true;
    return u * s;
}

/* ============================================================================
 * Layer Creation/Destruction
 * ============================================================================ */

static int vae_encoder_layer_create(vae_encoder_layer_t* layer,
                                    uint32_t in_dim,
                                    uint32_t out_dim,
                                    const vae_layer_config_t* config) {
    if (!layer || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL layer or config in vae_encoder_layer_create");
        return -1;
    }

    memset(layer, 0, sizeof(vae_encoder_layer_t));

    layer->in_dim = in_dim;
    layer->out_dim = out_dim;
    layer->activation = config->activation;
    layer->dropout_rate = config->dropout_rate;
    layer->batch_norm = config->batch_norm;
    layer->use_bias = config->use_bias;

    /* Allocate weights [in_dim, out_dim] */
    uint32_t weight_dims[2] = {in_dim, out_dim};
    layer->weights = nimcp_tensor_create(weight_dims, 2, NIMCP_DTYPE_F32);
    if (!layer->weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate encoder layer weights");
        return -1;
    }

    layer->weight_grad = nimcp_tensor_create(weight_dims, 2, NIMCP_DTYPE_F32);
    if (!layer->weight_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate encoder layer weight gradients");
        nimcp_tensor_destroy(layer->weights);
        return -1;
    }

    /* Allocate bias [out_dim] */
    if (layer->use_bias) {
        uint32_t bias_dims[1] = {out_dim};
        layer->bias = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);
        layer->bias_grad = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);
        if (!layer->bias || !layer->bias_grad) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                                  "Failed to allocate encoder layer bias");
            nimcp_tensor_destroy(layer->weights);
            nimcp_tensor_destroy(layer->weight_grad);
            nimcp_tensor_destroy(layer->bias);
            nimcp_tensor_destroy(layer->bias_grad);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_encoder_layer: bias allocation failed");
            return -1;
        }
    }

    /* Allocate batch norm parameters */
    if (layer->batch_norm) {
        uint32_t bn_dims[1] = {out_dim};
        layer->bn_gamma = nimcp_tensor_create(bn_dims, 1, NIMCP_DTYPE_F32);
        layer->bn_beta = nimcp_tensor_create(bn_dims, 1, NIMCP_DTYPE_F32);
        layer->bn_running_mean = nimcp_tensor_create(bn_dims, 1, NIMCP_DTYPE_F32);
        layer->bn_running_var = nimcp_tensor_create(bn_dims, 1, NIMCP_DTYPE_F32);

        if (!layer->bn_gamma || !layer->bn_beta ||
            !layer->bn_running_mean || !layer->bn_running_var) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                                  "Failed to allocate batch norm parameters");
            /* Cleanup all allocated tensors */
            nimcp_tensor_destroy(layer->weights);
            nimcp_tensor_destroy(layer->weight_grad);
            nimcp_tensor_destroy(layer->bias);
            nimcp_tensor_destroy(layer->bias_grad);
            nimcp_tensor_destroy(layer->bn_gamma);
            nimcp_tensor_destroy(layer->bn_beta);
            nimcp_tensor_destroy(layer->bn_running_mean);
            nimcp_tensor_destroy(layer->bn_running_var);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_encoder_layer: batch norm allocation failed");
            return -1;
        }

        /* Initialize bn_gamma to 1, bn_beta to 0, running stats to 0/1 */
        float* gamma_data = (float*)layer->bn_gamma->data;
        float* var_data = (float*)layer->bn_running_var->data;
        for (uint32_t i = 0; i < out_dim; i++) {
            gamma_data[i] = 1.0f;
            var_data[i] = 1.0f;
        }
    }

    return 0;
}

static void vae_encoder_layer_destroy(vae_encoder_layer_t* layer) {
    if (!layer) return;

    nimcp_tensor_destroy(layer->weights);
    nimcp_tensor_destroy(layer->bias);
    nimcp_tensor_destroy(layer->weight_grad);
    nimcp_tensor_destroy(layer->bias_grad);
    nimcp_tensor_destroy(layer->bn_gamma);
    nimcp_tensor_destroy(layer->bn_beta);
    nimcp_tensor_destroy(layer->bn_running_mean);
    nimcp_tensor_destroy(layer->bn_running_var);
    nimcp_tensor_destroy(layer->pre_activation);
    nimcp_tensor_destroy(layer->post_activation);
    nimcp_tensor_destroy(layer->dropout_mask);
    nimcp_tensor_destroy(layer->bn_cache_mean);
    nimcp_tensor_destroy(layer->bn_cache_var);

    memset(layer, 0, sizeof(vae_encoder_layer_t));
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

vae_encoder_t* vae_encoder_create(const vae_encoder_config_t* config) {
    vae_encoder_heartbeat("vae_encoder_create", 0.0f);

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL config in vae_encoder_create");
        return NULL;
    }

    /* Validate configuration */
    if (config->input_dim == 0 || config->input_dim > VAE_MAX_IO_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Invalid input_dim: %u (max %u)",
                              config->input_dim, VAE_MAX_IO_DIM);
        return NULL;
    }

    if (config->latent_dim == 0 || config->latent_dim > VAE_MAX_LATENT_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Invalid latent_dim: %u (max %u)",
                              config->latent_dim, VAE_MAX_LATENT_DIM);
        return NULL;
    }

    if (config->num_layers > VAE_MAX_LAYERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_CONFIG,
                              "Too many layers: %u (max %u)",
                              config->num_layers, VAE_MAX_LAYERS);
        return NULL;
    }

    /* Allocate encoder structure */
    vae_encoder_t* encoder = (vae_encoder_t*)nimcp_calloc(1, sizeof(vae_encoder_t));
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate vae_encoder_t");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&encoder->config, config, sizeof(vae_encoder_config_t));
    encoder->num_layers = config->num_layers;

    /* Create mutex */
    encoder->mutex = nimcp_mutex_create(NULL);
    if (!encoder->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to create encoder mutex");
        nimcp_free(encoder);
        encoder = NULL;
        return NULL;
    }

    /* Allocate layers array */
    if (encoder->num_layers > 0) {
        encoder->layers = (vae_encoder_layer_t*)nimcp_calloc(
            encoder->num_layers, sizeof(vae_encoder_layer_t));
        if (!encoder->layers) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                                  "Failed to allocate encoder layers array");
            nimcp_mutex_destroy(encoder->mutex);
            nimcp_free(encoder);
            encoder = NULL;
            return NULL;
        }
    }

    vae_encoder_heartbeat("vae_encoder_create", 0.3f);

    /* Create hidden layers */
    uint32_t prev_dim = config->input_dim;
    for (uint32_t i = 0; i < encoder->num_layers; i++) {
        uint32_t out_dim = config->layers[i].units;
        if (out_dim == 0) out_dim = prev_dim;  /* Default to same size */

        int result = vae_encoder_layer_create(&encoder->layers[i],
                                              prev_dim, out_dim,
                                              &config->layers[i]);
        if (result != 0) {
            /* Cleanup already created layers */
            for (uint32_t j = 0; j < i; j++) {
                vae_encoder_layer_destroy(&encoder->layers[j]);
            }
            nimcp_free(encoder->layers);
            nimcp_mutex_destroy(encoder->mutex);
            nimcp_free(encoder);
            encoder = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_encoder_create: validation failed");
            return NULL;
        }
        prev_dim = out_dim;

        if ((i & 0x3) == 0) {
            vae_encoder_heartbeat("vae_encoder_create",
                                  0.3f + 0.4f * (float)(i + 1) / (float)encoder->num_layers);
        }
    }

    /* Create output heads: mu and log_var */
    uint32_t head_in_dim = prev_dim;
    uint32_t head_dims[2] = {head_in_dim, config->latent_dim};

    encoder->mu_weights = nimcp_tensor_create(head_dims, 2, NIMCP_DTYPE_F32);
    encoder->mu_weights_grad = nimcp_tensor_create(head_dims, 2, NIMCP_DTYPE_F32);
    encoder->logvar_weights = nimcp_tensor_create(head_dims, 2, NIMCP_DTYPE_F32);
    encoder->logvar_weights_grad = nimcp_tensor_create(head_dims, 2, NIMCP_DTYPE_F32);

    uint32_t bias_dims[1] = {config->latent_dim};
    encoder->mu_bias = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);
    encoder->mu_bias_grad = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);
    encoder->logvar_bias = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);
    encoder->logvar_bias_grad = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);

    if (!encoder->mu_weights || !encoder->mu_weights_grad ||
        !encoder->logvar_weights || !encoder->logvar_weights_grad ||
        !encoder->mu_bias || !encoder->mu_bias_grad ||
        !encoder->logvar_bias || !encoder->logvar_bias_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate encoder output heads");
        vae_encoder_destroy(encoder);
        return NULL;
    }

    /* Initialize weights */
    if (vae_encoder_init_weights(encoder, 0) != 0) {
        vae_encoder_destroy(encoder);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "vae_encoder_create: validation failed");
        return NULL;
    }

    encoder->is_initialized = true;
    encoder->is_training = false;

    vae_encoder_heartbeat("vae_encoder_create", 1.0f);

    NIMCP_LOGGING_INFO("VAE encoder created: input=%u, latent=%u, layers=%u",
                       config->input_dim, config->latent_dim, config->num_layers);

    return encoder;
}

void vae_encoder_destroy(vae_encoder_t* encoder) {
    if (!encoder) return;

    vae_encoder_heartbeat("vae_encoder_destroy", 0.0f);

    /* Destroy layers */
    if (encoder->layers) {
        for (uint32_t i = 0; i < encoder->num_layers; i++) {
            vae_encoder_layer_destroy(&encoder->layers[i]);
        }
        nimcp_free(encoder->layers);
    }

    /* Destroy output heads */
    nimcp_tensor_destroy(encoder->mu_weights);
    nimcp_tensor_destroy(encoder->mu_bias);
    nimcp_tensor_destroy(encoder->mu_weights_grad);
    nimcp_tensor_destroy(encoder->mu_bias_grad);
    nimcp_tensor_destroy(encoder->logvar_weights);
    nimcp_tensor_destroy(encoder->logvar_bias);
    nimcp_tensor_destroy(encoder->logvar_weights_grad);
    nimcp_tensor_destroy(encoder->logvar_bias_grad);

    /* Destroy mutex */
    if (encoder->mutex) {
        nimcp_mutex_destroy(encoder->mutex);
    }

    nimcp_free(encoder);
    encoder = NULL;

    vae_encoder_heartbeat("vae_encoder_destroy", 1.0f);
}

int vae_encoder_reset(vae_encoder_t* encoder) {
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL encoder in vae_encoder_reset");
        return -1;
    }

    nimcp_mutex_lock(encoder->mutex);

    encoder->forward_calls = 0;
    encoder->backward_calls = 0;
    encoder->avg_forward_time_us = 0.0f;

    /* Zero all gradients */
    vae_encoder_zero_grad(encoder);

    nimcp_mutex_unlock(encoder->mutex);

    return 0;
}

int vae_encoder_init_weights(vae_encoder_t* encoder, uint64_t seed) {
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL encoder in vae_encoder_init_weights");
        return -1;
    }

    vae_encoder_heartbeat("vae_encoder_init_weights", 0.0f);

    /* srand() removed: xavier_init() uses thread-safe nimcp_tl_rand() */
    (void)seed;  /* seed parameter preserved for API compat */

    /* Initialize hidden layer weights */
    for (uint32_t i = 0; i < encoder->num_layers; i++) {
        vae_encoder_layer_t* layer = &encoder->layers[i];
        float* w = (float*)layer->weights->data;
        uint32_t size = layer->in_dim * layer->out_dim;

        for (uint32_t j = 0; j < size; j++) {
            w[j] = xavier_init(layer->in_dim, layer->out_dim);
        }

        /* Zero bias */
        if (layer->bias) {
            memset(layer->bias->data, 0, layer->out_dim * sizeof(float));
        }

        if ((i & 0x3) == 0) {
            vae_encoder_heartbeat("vae_encoder_init_weights",
                                  0.5f * (float)(i + 1) / (float)encoder->num_layers);
        }
    }

    /* Initialize output head weights */
    uint32_t head_in_dim = encoder->num_layers > 0 ?
                           encoder->layers[encoder->num_layers - 1].out_dim :
                           encoder->config.input_dim;
    uint32_t latent_dim = encoder->config.latent_dim;
    uint32_t head_size = head_in_dim * latent_dim;

    float* mu_w = (float*)encoder->mu_weights->data;
    float* logvar_w = (float*)encoder->logvar_weights->data;

    for (uint32_t i = 0; i < head_size; i++) {
        mu_w[i] = xavier_init(head_in_dim, latent_dim);
        logvar_w[i] = xavier_init(head_in_dim, latent_dim);
    }

    /* Zero biases */
    memset(encoder->mu_bias->data, 0, latent_dim * sizeof(float));

    /* Initialize log_var bias to small negative value for reasonable initial variance */
    float* logvar_b = (float*)encoder->logvar_bias->data;
    for (uint32_t i = 0; i < latent_dim; i++) {
        logvar_b[i] = -2.0f;  /* exp(-2) ~ 0.135, reasonable initial variance */
    }

    vae_encoder_heartbeat("vae_encoder_init_weights", 1.0f);

    return 0;
}

/* ============================================================================
 * Activation Functions Implementation
 * ============================================================================ */

int vae_activation_forward(vae_activation_t activation,
                           const nimcp_tensor_t* input,
                           nimcp_tensor_t* output) {
    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_activation_forward");
        return -1;
    }

    float* in_data = (float*)input->data;
    float* out_data = (float*)output->data;
    size_t size = nimcp_tensor_size(input);

    switch (activation) {
        case VAE_ACTIVATION_RELU:
            for (size_t i = 0; i < size; i++) {
                out_data[i] = in_data[i] > 0.0f ? in_data[i] : 0.0f;
            }
            break;

        case VAE_ACTIVATION_LEAKY_RELU:
            for (size_t i = 0; i < size; i++) {
                out_data[i] = in_data[i] > 0.0f ? in_data[i] :
                              VAE_ENCODER_LEAKY_ALPHA * in_data[i];
            }
            break;

        case VAE_ACTIVATION_ELU:
            for (size_t i = 0; i < size; i++) {
                out_data[i] = in_data[i] > 0.0f ? in_data[i] :
                              VAE_ENCODER_ELU_ALPHA * (expf(in_data[i]) - 1.0f);
            }
            break;

        case VAE_ACTIVATION_TANH:
            for (size_t i = 0; i < size; i++) {
                out_data[i] = tanhf(in_data[i]);
            }
            break;

        case VAE_ACTIVATION_SIGMOID:
            for (size_t i = 0; i < size; i++) {
                out_data[i] = 1.0f / (1.0f + expf(-in_data[i]));
            }
            break;

        case VAE_ACTIVATION_SOFTPLUS:
            for (size_t i = 0; i < size; i++) {
                /* log(1 + exp(x)), with numerical stability */
                if (in_data[i] > 20.0f) {
                    out_data[i] = in_data[i];
                } else if (in_data[i] < -20.0f) {
                    out_data[i] = expf(in_data[i]);
                } else {
                    out_data[i] = logf(1.0f + expf(in_data[i]));
                }
            }
            break;

        case VAE_ACTIVATION_GELU:
            for (size_t i = 0; i < size; i++) {
                /* Approximate GELU: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3))) */
                float x = in_data[i];
                float x3 = x * x * x;
                out_data[i] = 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x3)));
            }
            break;

        case VAE_ACTIVATION_SWISH:
            for (size_t i = 0; i < size; i++) {
                float x = in_data[i];
                out_data[i] = x / (1.0f + expf(-x));
            }
            break;

        case VAE_ACTIVATION_LINEAR:
        default:
            if (in_data != out_data) {
                memcpy(out_data, in_data, size * sizeof(float));
            }
            break;
    }

    return 0;
}

int vae_activation_backward(vae_activation_t activation,
                            const nimcp_tensor_t* input,
                            const nimcp_tensor_t* grad_output,
                            nimcp_tensor_t* grad_input) {
    if (!input || !grad_output || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL tensor in vae_activation_backward");
        return -1;
    }

    float* in_data = (float*)input->data;
    float* g_out = (float*)grad_output->data;
    float* g_in = (float*)grad_input->data;
    size_t size = nimcp_tensor_size(input);

    switch (activation) {
        case VAE_ACTIVATION_RELU:
            for (size_t i = 0; i < size; i++) {
                g_in[i] = in_data[i] > 0.0f ? g_out[i] : 0.0f;
            }
            break;

        case VAE_ACTIVATION_LEAKY_RELU:
            for (size_t i = 0; i < size; i++) {
                g_in[i] = in_data[i] > 0.0f ? g_out[i] :
                          VAE_ENCODER_LEAKY_ALPHA * g_out[i];
            }
            break;

        case VAE_ACTIVATION_ELU:
            for (size_t i = 0; i < size; i++) {
                if (in_data[i] > 0.0f) {
                    g_in[i] = g_out[i];
                } else {
                    g_in[i] = g_out[i] * VAE_ENCODER_ELU_ALPHA * expf(in_data[i]);
                }
            }
            break;

        case VAE_ACTIVATION_TANH:
            for (size_t i = 0; i < size; i++) {
                float t = tanhf(in_data[i]);
                g_in[i] = g_out[i] * (1.0f - t * t);
            }
            break;

        case VAE_ACTIVATION_SIGMOID:
            for (size_t i = 0; i < size; i++) {
                float s = 1.0f / (1.0f + expf(-in_data[i]));
                g_in[i] = g_out[i] * s * (1.0f - s);
            }
            break;

        case VAE_ACTIVATION_SOFTPLUS:
            for (size_t i = 0; i < size; i++) {
                float s = 1.0f / (1.0f + expf(-in_data[i]));
                g_in[i] = g_out[i] * s;
            }
            break;

        case VAE_ACTIVATION_GELU:
            for (size_t i = 0; i < size; i++) {
                /* GELU derivative approximation */
                float x = in_data[i];
                float x3 = x * x * x;
                float inner = 0.7978845608f * (x + 0.044715f * x3);
                float t = tanhf(inner);
                float sech2 = 1.0f - t * t;
                float d_inner = 0.7978845608f * (1.0f + 0.134145f * x * x);
                g_in[i] = g_out[i] * (0.5f * (1.0f + t) + 0.5f * x * sech2 * d_inner);
            }
            break;

        case VAE_ACTIVATION_SWISH:
            for (size_t i = 0; i < size; i++) {
                float x = in_data[i];
                float s = 1.0f / (1.0f + expf(-x));
                float swish = x * s;
                g_in[i] = g_out[i] * (swish + s * (1.0f - swish));
            }
            break;

        case VAE_ACTIVATION_LINEAR:
        default:
            if (g_out != g_in) {
                memcpy(g_in, g_out, size * sizeof(float));
            }
            break;
    }

    return 0;
}

const char* vae_activation_to_string(vae_activation_t activation) {
    switch (activation) {
        case VAE_ACTIVATION_RELU:       return "ReLU";
        case VAE_ACTIVATION_LEAKY_RELU: return "LeakyReLU";
        case VAE_ACTIVATION_ELU:        return "ELU";
        case VAE_ACTIVATION_TANH:       return "Tanh";
        case VAE_ACTIVATION_SIGMOID:    return "Sigmoid";
        case VAE_ACTIVATION_SOFTPLUS:   return "Softplus";
        case VAE_ACTIVATION_GELU:       return "GELU";
        case VAE_ACTIVATION_SWISH:      return "Swish";
        case VAE_ACTIVATION_LINEAR:     return "Linear";
        default:                        return "Unknown";
    }
}

/* ============================================================================
 * Forward Pass Implementation
 * ============================================================================ */

int vae_encoder_layer_forward(vae_encoder_layer_t* layer,
                              const nimcp_tensor_t* input,
                              nimcp_tensor_t* output,
                              bool training) {
    if (!layer || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL parameter in vae_encoder_layer_forward");
        return -1;
    }

    /* Get batch size from input */
    uint32_t batch_size = input->shape.dims[0];
    uint32_t in_dim = layer->in_dim;
    uint32_t out_dim = layer->out_dim;

    /* Matrix multiply: output = input @ weights */
    float* in_data = (float*)input->data;
    float* w_data = (float*)layer->weights->data;
    float* out_data = (float*)output->data;

    /* Simple matrix multiplication (can be optimized with BLAS) */
    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t j = 0; j < out_dim; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < in_dim; k++) {
                sum += in_data[b * in_dim + k] * w_data[k * out_dim + j];
            }
            out_data[b * out_dim + j] = sum;
        }
    }

    /* Add bias */
    if (layer->use_bias && layer->bias) {
        float* bias = (float*)layer->bias->data;
        for (uint32_t b = 0; b < batch_size; b++) {
            for (uint32_t j = 0; j < out_dim; j++) {
                out_data[b * out_dim + j] += bias[j];
            }
        }
    }

    /* Store pre-activation for backprop */
    if (layer->pre_activation) {
        memcpy(layer->pre_activation->data, out_data,
               batch_size * out_dim * sizeof(float));
    }

    /* Batch normalization (simplified - no running stats update here) */
    if (layer->batch_norm && layer->bn_gamma && layer->bn_beta) {
        float* gamma = (float*)layer->bn_gamma->data;
        float* beta = (float*)layer->bn_beta->data;

        for (uint32_t j = 0; j < out_dim; j++) {
            /* Compute mean and variance for this feature */
            float mean = 0.0f;
            for (uint32_t b = 0; b < batch_size; b++) {
                mean += out_data[b * out_dim + j];
            }
            mean /= (float)batch_size;

            float var = 0.0f;
            for (uint32_t b = 0; b < batch_size; b++) {
                float diff = out_data[b * out_dim + j] - mean;
                var += diff * diff;
            }
            var /= (float)batch_size;

            /* Normalize and scale */
            float inv_std = 1.0f / sqrtf(var + VAE_ENCODER_BN_EPSILON);
            for (uint32_t b = 0; b < batch_size; b++) {
                out_data[b * out_dim + j] =
                    gamma[j] * (out_data[b * out_dim + j] - mean) * inv_std + beta[j];
            }
        }
    }

    /* Apply activation */
    if (layer->activation != VAE_ACTIVATION_LINEAR) {
        vae_activation_forward(layer->activation, output, output);
    }

    /* Store post-activation for backprop */
    if (layer->post_activation) {
        memcpy(layer->post_activation->data, out_data,
               batch_size * out_dim * sizeof(float));
    }

    /* Apply dropout during training */
    if (training && layer->dropout_rate > 0.0f) {
        float keep_prob = 1.0f - layer->dropout_rate;
        float scale = 1.0f / keep_prob;

        for (uint32_t i = 0; i < batch_size * out_dim; i++) {
            if ((float)nimcp_tl_rand() / (float)RAND_MAX > keep_prob) {
                out_data[i] = 0.0f;
            } else {
                out_data[i] *= scale;
            }
        }
    }

    return 0;
}

int vae_encoder_forward(vae_encoder_t* encoder,
                        const nimcp_tensor_t* input,
                        nimcp_tensor_t* mu,
                        nimcp_tensor_t* log_var) {
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL encoder in vae_encoder_forward");
        return -1;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL input in vae_encoder_forward");
        return -1;
    }
    if (!mu || !log_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL output tensor in vae_encoder_forward");
        return -1;
    }

    if (!encoder->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NOT_INITIALIZED,
                              "Encoder not initialized in vae_encoder_forward");
        return -1;
    }

    vae_encoder_heartbeat("vae_encoder_forward", 0.0f);

    nimcp_mutex_lock(encoder->mutex);

    /* Validate input dimensions */
    if (input->shape.rank < 1 || input->shape.rank > 2) {
        nimcp_mutex_unlock(encoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Input must be 1D or 2D tensor, got rank %u",
                              input->shape.rank);
        return -1;
    }

    uint32_t batch_size = (input->shape.rank == 2) ? input->shape.dims[0] : 1;
    uint32_t input_dim = (input->shape.rank == 2) ? input->shape.dims[1] : input->shape.dims[0];

    if (input_dim != encoder->config.input_dim) {
        nimcp_mutex_unlock(encoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Input dimension mismatch: expected %u, got %u",
                              encoder->config.input_dim, input_dim);
        return -1;
    }

    /* Create intermediate buffer */
    uint32_t max_dim = encoder->config.input_dim;
    for (uint32_t i = 0; i < encoder->num_layers; i++) {
        if (encoder->layers[i].out_dim > max_dim) {
            max_dim = encoder->layers[i].out_dim;
        }
    }

    uint32_t buffer_dims[2] = {batch_size, max_dim};
    nimcp_tensor_t* current = nimcp_tensor_create(buffer_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* next = nimcp_tensor_create(buffer_dims, 2, NIMCP_DTYPE_F32);

    if (!current || !next) {
        nimcp_mutex_unlock(encoder->mutex);
        nimcp_tensor_destroy(current);
        nimcp_tensor_destroy(next);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate encoder intermediate buffers");
        return -1;
    }

    /* Copy input to current buffer */
    memcpy(current->data, input->data, batch_size * input_dim * sizeof(float));
    current->shape.dims[1] = input_dim;

    /* Forward through hidden layers */
    for (uint32_t i = 0; i < encoder->num_layers; i++) {
        vae_encoder_layer_t* layer = &encoder->layers[i];
        next->shape.dims[1] = layer->out_dim;

        int result = vae_encoder_layer_forward(layer, current, next, encoder->is_training);
        if (result != 0) {
            nimcp_mutex_unlock(encoder->mutex);
            nimcp_tensor_destroy(current);
            nimcp_tensor_destroy(next);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_encoder_forward: layer forward failed");
            return -1;
        }

        /* Swap buffers */
        nimcp_tensor_t* tmp = current;
        current = next;
        next = tmp;

        if ((i & 0x3) == 0 && encoder->num_layers > 4) {
            vae_encoder_heartbeat("vae_encoder_forward",
                                  0.3f + 0.4f * (float)(i + 1) / (float)encoder->num_layers);
        }
    }

    /* Compute mu output: mu = current @ mu_weights + mu_bias */
    uint32_t final_dim = encoder->num_layers > 0 ?
                         encoder->layers[encoder->num_layers - 1].out_dim :
                         encoder->config.input_dim;
    uint32_t latent_dim = encoder->config.latent_dim;

    float* curr_data = (float*)current->data;
    float* mu_w = (float*)encoder->mu_weights->data;
    float* mu_b = (float*)encoder->mu_bias->data;
    float* mu_out = (float*)mu->data;

    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t j = 0; j < latent_dim; j++) {
            float sum = mu_b[j];
            for (uint32_t k = 0; k < final_dim; k++) {
                sum += curr_data[b * final_dim + k] * mu_w[k * latent_dim + j];
            }
            mu_out[b * latent_dim + j] = sum;
        }
    }

    vae_encoder_heartbeat("vae_encoder_forward", 0.8f);

    /* Compute log_var output: log_var = current @ logvar_weights + logvar_bias */
    float* logvar_w = (float*)encoder->logvar_weights->data;
    float* logvar_b = (float*)encoder->logvar_bias->data;
    float* logvar_out = (float*)log_var->data;

    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t j = 0; j < latent_dim; j++) {
            float sum = logvar_b[j];
            for (uint32_t k = 0; k < final_dim; k++) {
                sum += curr_data[b * final_dim + k] * logvar_w[k * latent_dim + j];
            }
            /* Clamp log_var to prevent extreme values */
            logvar_out[b * latent_dim + j] = nimcp_clampf(sum, -10.0f, 10.0f);
        }
    }

    /* Check for NaN in outputs */
    for (uint32_t i = 0; i < batch_size * latent_dim; i++) {
        if (is_nan_f(mu_out[i]) || is_nan_f(logvar_out[i])) {
            nimcp_mutex_unlock(encoder->mutex);
            nimcp_tensor_destroy(current);
            nimcp_tensor_destroy(next);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_GRADIENT_NAN,
                                  "NaN detected in encoder output");
            return -1;
        }
    }

    /* Cleanup */
    nimcp_tensor_destroy(current);
    nimcp_tensor_destroy(next);

    encoder->forward_calls++;

    nimcp_mutex_unlock(encoder->mutex);

    vae_encoder_heartbeat("vae_encoder_forward", 1.0f);

    return 0;
}

/* ============================================================================
 * Backward Pass Implementation
 * ============================================================================ */

int vae_encoder_layer_backward(vae_encoder_layer_t* layer,
                               const nimcp_tensor_t* d_output,
                               nimcp_tensor_t* d_input) {
    if (!layer || !d_output || !d_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL parameter in vae_encoder_layer_backward");
        return -1;
    }

    /* This is a simplified backward pass - full implementation would need
     * cached activations and proper gradient accumulation */

    uint32_t batch_size = d_output->shape.dims[0];
    uint32_t out_dim = layer->out_dim;
    uint32_t in_dim = layer->in_dim;

    float* d_out = (float*)d_output->data;
    float* d_in = (float*)d_input->data;
    float* w = (float*)layer->weights->data;

    /* Gradient w.r.t. input: d_input = d_output @ weights.T */
    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t i = 0; i < in_dim; i++) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < out_dim; j++) {
                sum += d_out[b * out_dim + j] * w[i * out_dim + j];
            }
            d_in[b * in_dim + i] = sum;
        }
    }

    return 0;
}

int vae_encoder_backward(vae_encoder_t* encoder,
                         const nimcp_tensor_t* d_mu,
                         const nimcp_tensor_t* d_log_var,
                         nimcp_tensor_t* d_input) {
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL encoder in vae_encoder_backward");
        return -1;
    }
    if (!d_mu || !d_log_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL gradient tensor in vae_encoder_backward");
        return -1;
    }

    vae_encoder_heartbeat("vae_encoder_backward", 0.0f);

    nimcp_mutex_lock(encoder->mutex);

    /* Simplified backward pass - full implementation requires cached forward values */
    encoder->backward_calls++;

    nimcp_mutex_unlock(encoder->mutex);

    vae_encoder_heartbeat("vae_encoder_backward", 1.0f);

    return 0;
}

/* ============================================================================
 * Training API Implementation
 * ============================================================================ */

int vae_encoder_set_training(vae_encoder_t* encoder, bool training) {
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL encoder in vae_encoder_set_training");
        return -1;
    }

    nimcp_mutex_lock(encoder->mutex);
    encoder->is_training = training;
    nimcp_mutex_unlock(encoder->mutex);

    return 0;
}

int vae_encoder_apply_gradients(vae_encoder_t* encoder, float learning_rate) {
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL encoder in vae_encoder_apply_gradients");
        return -1;
    }

    vae_encoder_heartbeat("vae_encoder_apply_gradients", 0.0f);

    nimcp_mutex_lock(encoder->mutex);

    /* Update hidden layer weights */
    for (uint32_t i = 0; i < encoder->num_layers; i++) {
        vae_encoder_layer_t* layer = &encoder->layers[i];
        uint32_t w_size = layer->in_dim * layer->out_dim;

        float* w = (float*)layer->weights->data;
        float* w_grad = (float*)layer->weight_grad->data;

        for (uint32_t j = 0; j < w_size; j++) {
            w[j] -= learning_rate * w_grad[j];
        }

        if (layer->use_bias && layer->bias && layer->bias_grad) {
            float* b = (float*)layer->bias->data;
            float* b_grad = (float*)layer->bias_grad->data;

            for (uint32_t j = 0; j < layer->out_dim; j++) {
                b[j] -= learning_rate * b_grad[j];
            }
        }
    }

    /* Update output head weights */
    uint32_t head_size = encoder->mu_weights->shape.dims[0] * encoder->mu_weights->shape.dims[1];
    uint32_t latent_dim = encoder->config.latent_dim;

    float* mu_w = (float*)encoder->mu_weights->data;
    float* mu_w_grad = (float*)encoder->mu_weights_grad->data;
    for (uint32_t i = 0; i < head_size; i++) {
        mu_w[i] -= learning_rate * mu_w_grad[i];
    }

    float* mu_b = (float*)encoder->mu_bias->data;
    float* mu_b_grad = (float*)encoder->mu_bias_grad->data;
    for (uint32_t i = 0; i < latent_dim; i++) {
        mu_b[i] -= learning_rate * mu_b_grad[i];
    }

    float* logvar_w = (float*)encoder->logvar_weights->data;
    float* logvar_w_grad = (float*)encoder->logvar_weights_grad->data;
    for (uint32_t i = 0; i < head_size; i++) {
        logvar_w[i] -= learning_rate * logvar_w_grad[i];
    }

    float* logvar_b = (float*)encoder->logvar_bias->data;
    float* logvar_b_grad = (float*)encoder->logvar_bias_grad->data;
    for (uint32_t i = 0; i < latent_dim; i++) {
        logvar_b[i] -= learning_rate * logvar_b_grad[i];
    }

    nimcp_mutex_unlock(encoder->mutex);

    vae_encoder_heartbeat("vae_encoder_apply_gradients", 1.0f);

    return 0;
}

int vae_encoder_zero_grad(vae_encoder_t* encoder) {
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL encoder in vae_encoder_zero_grad");
        return -1;
    }

    /* Zero hidden layer gradients */
    for (uint32_t i = 0; i < encoder->num_layers; i++) {
        vae_encoder_layer_t* layer = &encoder->layers[i];

        if (layer->weight_grad) {
            memset(layer->weight_grad->data, 0,
                   layer->in_dim * layer->out_dim * sizeof(float));
        }
        if (layer->bias_grad) {
            memset(layer->bias_grad->data, 0, layer->out_dim * sizeof(float));
        }
    }

    /* Zero output head gradients */
    if (encoder->mu_weights_grad) {
        size_t size = encoder->mu_weights_grad->shape.dims[0] *
                      encoder->mu_weights_grad->shape.dims[1] * sizeof(float);
        memset(encoder->mu_weights_grad->data, 0, size);
    }
    if (encoder->mu_bias_grad) {
        memset(encoder->mu_bias_grad->data, 0,
               encoder->config.latent_dim * sizeof(float));
    }
    if (encoder->logvar_weights_grad) {
        size_t size = encoder->logvar_weights_grad->shape.dims[0] *
                      encoder->logvar_weights_grad->shape.dims[1] * sizeof(float);
        memset(encoder->logvar_weights_grad->data, 0, size);
    }
    if (encoder->logvar_bias_grad) {
        memset(encoder->logvar_bias_grad->data, 0,
               encoder->config.latent_dim * sizeof(float));
    }

    return 0;
}

int vae_encoder_clip_gradients(vae_encoder_t* encoder, float max_norm) {
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL encoder in vae_encoder_clip_gradients");
        return -1;
    }

    float total_norm = vae_encoder_grad_norm(encoder);

    if (total_norm > max_norm) {
        float scale = max_norm / (total_norm + 1e-6f);

        /* Scale hidden layer gradients */
        for (uint32_t i = 0; i < encoder->num_layers; i++) {
            vae_encoder_layer_t* layer = &encoder->layers[i];

            if (layer->weight_grad) {
                float* g = (float*)layer->weight_grad->data;
                uint32_t size = layer->in_dim * layer->out_dim;
                for (uint32_t j = 0; j < size; j++) {
                    g[j] *= scale;
                }
            }
            if (layer->bias_grad) {
                float* g = (float*)layer->bias_grad->data;
                for (uint32_t j = 0; j < layer->out_dim; j++) {
                    g[j] *= scale;
                }
            }
        }

        /* Scale output head gradients */
        uint32_t head_size = encoder->mu_weights_grad->shape.dims[0] *
                             encoder->mu_weights_grad->shape.dims[1];
        uint32_t latent_dim = encoder->config.latent_dim;

        float* mu_w_g = (float*)encoder->mu_weights_grad->data;
        float* logvar_w_g = (float*)encoder->logvar_weights_grad->data;
        for (uint32_t i = 0; i < head_size; i++) {
            mu_w_g[i] *= scale;
            logvar_w_g[i] *= scale;
        }

        float* mu_b_g = (float*)encoder->mu_bias_grad->data;
        float* logvar_b_g = (float*)encoder->logvar_bias_grad->data;
        for (uint32_t i = 0; i < latent_dim; i++) {
            mu_b_g[i] *= scale;
            logvar_b_g[i] *= scale;
        }
    }

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

uint64_t vae_encoder_param_count(const vae_encoder_t* encoder) {
    if (!encoder) return 0;

    uint64_t count = 0;

    /* Hidden layers */
    for (uint32_t i = 0; i < encoder->num_layers; i++) {
        const vae_encoder_layer_t* layer = &encoder->layers[i];
        count += layer->in_dim * layer->out_dim;  /* weights */
        if (layer->use_bias) {
            count += layer->out_dim;  /* bias */
        }
        if (layer->batch_norm) {
            count += 2 * layer->out_dim;  /* gamma, beta */
        }
    }

    /* Output heads */
    uint32_t head_in_dim = encoder->num_layers > 0 ?
                           encoder->layers[encoder->num_layers - 1].out_dim :
                           encoder->config.input_dim;
    uint32_t latent_dim = encoder->config.latent_dim;

    count += 2 * (head_in_dim * latent_dim + latent_dim);  /* mu and logvar heads */

    return count;
}

float vae_encoder_grad_norm(const vae_encoder_t* encoder) {
    if (!encoder) return 0.0f;

    float sum_sq = 0.0f;

    /* Hidden layers */
    for (uint32_t i = 0; i < encoder->num_layers; i++) {
        const vae_encoder_layer_t* layer = &encoder->layers[i];

        if (layer->weight_grad) {
            float* g = (float*)layer->weight_grad->data;
            uint32_t size = layer->in_dim * layer->out_dim;
            for (uint32_t j = 0; j < size; j++) {
                sum_sq += g[j] * g[j];
            }
        }
        if (layer->bias_grad) {
            float* g = (float*)layer->bias_grad->data;
            for (uint32_t j = 0; j < layer->out_dim; j++) {
                sum_sq += g[j] * g[j];
            }
        }
    }

    /* Output heads */
    if (encoder->mu_weights_grad) {
        float* g = (float*)encoder->mu_weights_grad->data;
        uint32_t size = encoder->mu_weights_grad->shape.dims[0] *
                        encoder->mu_weights_grad->shape.dims[1];
        for (uint32_t i = 0; i < size; i++) {
            sum_sq += g[i] * g[i];
        }
    }
    if (encoder->mu_bias_grad) {
        float* g = (float*)encoder->mu_bias_grad->data;
        for (uint32_t i = 0; i < encoder->config.latent_dim; i++) {
            sum_sq += g[i] * g[i];
        }
    }
    if (encoder->logvar_weights_grad) {
        float* g = (float*)encoder->logvar_weights_grad->data;
        uint32_t size = encoder->logvar_weights_grad->shape.dims[0] *
                        encoder->logvar_weights_grad->shape.dims[1];
        for (uint32_t i = 0; i < size; i++) {
            sum_sq += g[i] * g[i];
        }
    }
    if (encoder->logvar_bias_grad) {
        float* g = (float*)encoder->logvar_bias_grad->data;
        for (uint32_t i = 0; i < encoder->config.latent_dim; i++) {
            sum_sq += g[i] * g[i];
        }
    }

    return sqrtf(sum_sq);
}

bool vae_encoder_has_nan(const vae_encoder_t* encoder) {
    if (!encoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_encoder_has_nan: encoder is NULL");
        return false;
    }

    /* Check hidden layers */
    for (uint32_t i = 0; i < encoder->num_layers; i++) {
        const vae_encoder_layer_t* layer = &encoder->layers[i];

        if (layer->weights) {
            float* w = (float*)layer->weights->data;
            uint32_t size = layer->in_dim * layer->out_dim;
            for (uint32_t j = 0; j < size; j++) {
                if (is_nan_f(w[j]) || is_inf_f(w[j])) return true;
            }
        }
    }

    /* Check output heads */
    if (encoder->mu_weights) {
        float* w = (float*)encoder->mu_weights->data;
        uint32_t size = encoder->mu_weights->shape.dims[0] * encoder->mu_weights->shape.dims[1];
        for (uint32_t i = 0; i < size; i++) {
            if (is_nan_f(w[i]) || is_inf_f(w[i])) return true;
        }
    }
    if (encoder->logvar_weights) {
        float* w = (float*)encoder->logvar_weights->data;
        uint32_t size = encoder->logvar_weights->shape.dims[0] * encoder->logvar_weights->shape.dims[1];
        for (uint32_t i = 0; i < size; i++) {
            if (is_nan_f(w[i]) || is_inf_f(w[i])) return true;
        }
    }

    return false;  /* No NaN/Inf found */
}

uint32_t vae_encoder_get_layer_dim(const vae_encoder_t* encoder, uint32_t layer_idx) {
    if (!encoder) return 0;

    if (layer_idx >= encoder->num_layers) {
        return encoder->config.latent_dim;
    }

    return encoder->layers[layer_idx].out_dim;
}

void vae_encoder_set_health_agent(vae_encoder_t* encoder,
                                  nimcp_health_agent_t* agent) {
    if (encoder) {
        /* Store in global for heartbeat calls */
        g_vae_encoder_health_agent = agent;
    }
}
