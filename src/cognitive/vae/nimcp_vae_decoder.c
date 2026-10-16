/**
 * @file nimcp_vae_decoder.c
 * @brief VAE Decoder Network Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Implementation of the VAE decoder that maps latent samples
 *       to reconstructions in the original data space.
 *
 * WHY:  The decoder is the generative model p(x|z) that produces
 *       predictions/reconstructions from latent representations.
 *
 * HOW:  Forward pass through hidden layers followed by output layer.
 *       Final activation depends on data type (sigmoid for binary,
 *       linear for continuous).
 */

#include "cognitive/vae/nimcp_vae_decoder.h"
#include "cognitive/vae/nimcp_vae_encoder.h"  /* For activation functions */
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/math/nimcp_math_helpers.h"

/* Custom set_health_agent API (instance-level, not global) - don't use BRIDGE_BOILERPLATE */
static nimcp_health_agent_t* g_vae_decoder_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(vae_decoder, MESH_ADAPTER_CATEGORY_COGNITIVE)

static inline void vae_decoder_heartbeat(const char* op, float progress) {
    if (g_vae_decoder_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_vae_decoder_health_agent, op, progress);
    }
}

#define LOG_MODULE "VAE_DECODER"

static inline bool is_nan_f(float value) {
    return value != value;
}

static inline bool is_inf_f(float value) {
    return value == INFINITY || value == -INFINITY;
}

static float xavier_init(uint32_t fan_in, uint32_t fan_out) {
    float limit = sqrtf(6.0f / (float)(fan_in + fan_out));
    return ((float)nimcp_tl_rand() / (float)RAND_MAX) * 2.0f * limit - limit;
}

/* ============================================================================
 * Layer Creation/Destruction
 * ============================================================================ */

static int vae_decoder_layer_create(vae_decoder_layer_t* layer,
                                    uint32_t in_dim,
                                    uint32_t out_dim,
                                    const vae_layer_config_t* config) {
    if (!layer || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL layer or config in vae_decoder_layer_create");
        return -1;
    }

    memset(layer, 0, sizeof(vae_decoder_layer_t));

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
                              "Failed to allocate decoder layer weights");
        return -1;
    }

    layer->weight_grad = nimcp_tensor_create(weight_dims, 2, NIMCP_DTYPE_F32);
    if (!layer->weight_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate decoder layer weight gradients");
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
                                  "Failed to allocate decoder layer bias");
            nimcp_tensor_destroy(layer->weights);
            nimcp_tensor_destroy(layer->weight_grad);
            nimcp_tensor_destroy(layer->bias);
            nimcp_tensor_destroy(layer->bias_grad);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "xavier_init: required parameter is NULL (layer->bias, layer->bias_grad)");
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
            nimcp_tensor_destroy(layer->weights);
            nimcp_tensor_destroy(layer->weight_grad);
            nimcp_tensor_destroy(layer->bias);
            nimcp_tensor_destroy(layer->bias_grad);
            nimcp_tensor_destroy(layer->bn_gamma);
            nimcp_tensor_destroy(layer->bn_beta);
            nimcp_tensor_destroy(layer->bn_running_mean);
            nimcp_tensor_destroy(layer->bn_running_var);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "xavier_init: operation failed");
            return -1;
        }

        /* Initialize bn_gamma to 1, bn_beta to 0 */
        float* gamma_data = (float*)layer->bn_gamma->data;
        float* var_data = (float*)layer->bn_running_var->data;
        for (uint32_t i = 0; i < out_dim; i++) {
            gamma_data[i] = 1.0f;
            var_data[i] = 1.0f;
        }
    }

    return 0;
}

static void vae_decoder_layer_destroy(vae_decoder_layer_t* layer) {
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

    memset(layer, 0, sizeof(vae_decoder_layer_t));
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

vae_decoder_t* vae_decoder_create(const vae_decoder_config_t* config) {
    vae_decoder_heartbeat("vae_decoder_create", 0.0f);

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL config in vae_decoder_create");
        return NULL;
    }

    /* Validate configuration */
    if (config->latent_dim == 0 || config->latent_dim > VAE_MAX_LATENT_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Invalid latent_dim: %u (max %u)",
                              config->latent_dim, VAE_MAX_LATENT_DIM);
        return NULL;
    }

    if (config->output_dim == 0 || config->output_dim > VAE_MAX_IO_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Invalid output_dim: %u (max %u)",
                              config->output_dim, VAE_MAX_IO_DIM);
        return NULL;
    }

    if (config->num_layers > VAE_MAX_LAYERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_CONFIG,
                              "Too many layers: %u (max %u)",
                              config->num_layers, VAE_MAX_LAYERS);
        return NULL;
    }

    /* Allocate decoder structure */
    vae_decoder_t* decoder = (vae_decoder_t*)nimcp_calloc(1, sizeof(vae_decoder_t));
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate vae_decoder_t");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&decoder->config, config, sizeof(vae_decoder_config_t));
    decoder->num_layers = config->num_layers;

    /* Create mutex */
    decoder->mutex = nimcp_mutex_create(NULL);
    if (!decoder->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to create decoder mutex");
        nimcp_free(decoder);
        decoder = NULL;
        return NULL;
    }

    /* Allocate layers array */
    if (decoder->num_layers > 0) {
        decoder->layers = (vae_decoder_layer_t*)nimcp_calloc(
            decoder->num_layers, sizeof(vae_decoder_layer_t));
        if (!decoder->layers) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                                  "Failed to allocate decoder layers array");
            nimcp_mutex_destroy(decoder->mutex);
            nimcp_free(decoder);
            decoder = NULL;
            return NULL;
        }
    }

    vae_decoder_heartbeat("vae_decoder_create", 0.3f);

    /* Create hidden layers */
    uint32_t prev_dim = config->latent_dim;
    for (uint32_t i = 0; i < decoder->num_layers; i++) {
        uint32_t out_dim = config->layers[i].units;
        if (out_dim == 0) out_dim = prev_dim;  /* Default to same size */

        int result = vae_decoder_layer_create(&decoder->layers[i],
                                              prev_dim, out_dim,
                                              &config->layers[i]);
        if (result != 0) {
            /* Cleanup already created layers */
            for (uint32_t j = 0; j < i; j++) {
                vae_decoder_layer_destroy(&decoder->layers[j]);
            }
            nimcp_free(decoder->layers);
            nimcp_mutex_destroy(decoder->mutex);
            nimcp_free(decoder);
            decoder = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_decoder_create: validation failed");
            return NULL;
        }
        prev_dim = out_dim;

        if ((i & 0x3) == 0) {
            vae_decoder_heartbeat("vae_decoder_create",
                                  0.3f + 0.4f * (float)(i + 1) / (float)decoder->num_layers);
        }
    }

    /* Create output layer */
    uint32_t output_in_dim = prev_dim;
    uint32_t output_dims[2] = {output_in_dim, config->output_dim};

    decoder->output_weights = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_F32);
    decoder->output_weights_grad = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_F32);

    uint32_t bias_dims[1] = {config->output_dim};
    decoder->output_bias = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);
    decoder->output_bias_grad = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);

    if (!decoder->output_weights || !decoder->output_weights_grad ||
        !decoder->output_bias || !decoder->output_bias_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate decoder output layer");
        vae_decoder_destroy(decoder);
        return NULL;
    }

    /* Create variance output layer if heteroscedastic */
    if (config->output_variance) {
        decoder->var_weights = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_F32);
        decoder->var_weights_grad = nimcp_tensor_create(output_dims, 2, NIMCP_DTYPE_F32);
        decoder->var_bias = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);
        decoder->var_bias_grad = nimcp_tensor_create(bias_dims, 1, NIMCP_DTYPE_F32);

        if (!decoder->var_weights || !decoder->var_weights_grad ||
            !decoder->var_bias || !decoder->var_bias_grad) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                                  "Failed to allocate decoder variance output");
            vae_decoder_destroy(decoder);
            return NULL;
        }
    }

    /* Initialize weights */
    if (vae_decoder_init_weights(decoder, 0) != 0) {
        vae_decoder_destroy(decoder);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "vae_decoder_create: validation failed");
        return NULL;
    }

    decoder->is_initialized = true;
    decoder->is_training = false;

    vae_decoder_heartbeat("vae_decoder_create", 1.0f);

    NIMCP_LOGGING_INFO("VAE decoder created: latent=%u, output=%u, layers=%u",
                       config->latent_dim, config->output_dim, config->num_layers);

    return decoder;
}

void vae_decoder_destroy(vae_decoder_t* decoder) {
    if (!decoder) return;

    vae_decoder_heartbeat("vae_decoder_destroy", 0.0f);

    /* Destroy layers */
    if (decoder->layers) {
        for (uint32_t i = 0; i < decoder->num_layers; i++) {
            vae_decoder_layer_destroy(&decoder->layers[i]);
        }
        nimcp_free(decoder->layers);
    }

    /* Destroy output layer */
    nimcp_tensor_destroy(decoder->output_weights);
    nimcp_tensor_destroy(decoder->output_bias);
    nimcp_tensor_destroy(decoder->output_weights_grad);
    nimcp_tensor_destroy(decoder->output_bias_grad);

    /* Destroy variance output */
    nimcp_tensor_destroy(decoder->var_weights);
    nimcp_tensor_destroy(decoder->var_bias);
    nimcp_tensor_destroy(decoder->var_weights_grad);
    nimcp_tensor_destroy(decoder->var_bias_grad);

    /* Destroy mutex */
    if (decoder->mutex) {
        nimcp_mutex_destroy(decoder->mutex);
    }

    nimcp_free(decoder);
    decoder = NULL;

    vae_decoder_heartbeat("vae_decoder_destroy", 1.0f);
}

int vae_decoder_reset(vae_decoder_t* decoder) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL decoder in vae_decoder_reset");
        return -1;
    }

    nimcp_mutex_lock(decoder->mutex);

    decoder->forward_calls = 0;
    decoder->backward_calls = 0;
    decoder->avg_forward_time_us = 0.0f;

    /* Zero all gradients */
    vae_decoder_zero_grad(decoder);

    nimcp_mutex_unlock(decoder->mutex);

    return 0;
}

int vae_decoder_init_weights(vae_decoder_t* decoder, uint64_t seed) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL decoder in vae_decoder_init_weights");
        return -1;
    }

    vae_decoder_heartbeat("vae_decoder_init_weights", 0.0f);

    /* srand() removed: xavier_init() uses thread-safe nimcp_tl_rand() */
    (void)seed;  /* seed parameter preserved for API compat */

    /* Initialize hidden layer weights */
    for (uint32_t i = 0; i < decoder->num_layers; i++) {
        vae_decoder_layer_t* layer = &decoder->layers[i];
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
            vae_decoder_heartbeat("vae_decoder_init_weights",
                                  0.5f * (float)(i + 1) / (float)decoder->num_layers);
        }
    }

    /* Initialize output layer weights */
    uint32_t output_in_dim = decoder->num_layers > 0 ?
                             decoder->layers[decoder->num_layers - 1].out_dim :
                             decoder->config.latent_dim;
    uint32_t output_dim = decoder->config.output_dim;
    uint32_t output_size = output_in_dim * output_dim;

    float* output_w = (float*)decoder->output_weights->data;
    for (uint32_t i = 0; i < output_size; i++) {
        output_w[i] = xavier_init(output_in_dim, output_dim);
    }
    memset(decoder->output_bias->data, 0, output_dim * sizeof(float));

    /* Initialize variance output if present */
    if (decoder->var_weights) {
        float* var_w = (float*)decoder->var_weights->data;
        for (uint32_t i = 0; i < output_size; i++) {
            var_w[i] = xavier_init(output_in_dim, output_dim);
        }
        /* Initialize variance bias to small negative value */
        float* var_b = (float*)decoder->var_bias->data;
        for (uint32_t i = 0; i < output_dim; i++) {
            var_b[i] = -2.0f;  /* exp(-2) ~ 0.135 */
        }
    }

    vae_decoder_heartbeat("vae_decoder_init_weights", 1.0f);

    return 0;
}

/* ============================================================================
 * Forward Pass Implementation
 * ============================================================================ */

int vae_decoder_layer_forward(vae_decoder_layer_t* layer,
                              const nimcp_tensor_t* input,
                              nimcp_tensor_t* output,
                              bool training) {
    if (!layer || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL parameter in vae_decoder_layer_forward");
        return -1;
    }

    uint32_t batch_size = input->shape.dims[0];
    uint32_t in_dim = layer->in_dim;
    uint32_t out_dim = layer->out_dim;

    float* in_data = (float*)input->data;
    float* w_data = (float*)layer->weights->data;
    float* out_data = (float*)output->data;

    /* Matrix multiply: output = input @ weights */
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

    /* Batch normalization */
    if (layer->batch_norm && layer->bn_gamma && layer->bn_beta) {
        float* gamma = (float*)layer->bn_gamma->data;
        float* beta = (float*)layer->bn_beta->data;

        for (uint32_t j = 0; j < out_dim; j++) {
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

            float inv_std = 1.0f / sqrtf(var + 1e-5f);
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

int vae_decoder_forward(vae_decoder_t* decoder,
                        const nimcp_tensor_t* z,
                        nimcp_tensor_t* reconstruction) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL decoder in vae_decoder_forward");
        return -1;
    }
    if (!z) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL latent tensor in vae_decoder_forward");
        return -1;
    }
    if (!reconstruction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL reconstruction tensor in vae_decoder_forward");
        return -1;
    }

    if (!decoder->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NOT_INITIALIZED,
                              "Decoder not initialized in vae_decoder_forward");
        return -1;
    }

    vae_decoder_heartbeat("vae_decoder_forward", 0.0f);

    nimcp_mutex_lock(decoder->mutex);

    /* Validate input dimensions */
    if (z->shape.rank < 1 || z->shape.rank > 2) {
        nimcp_mutex_unlock(decoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Latent must be 1D or 2D tensor, got rank %u",
                              z->shape.rank);
        return -1;
    }

    uint32_t batch_size = (z->shape.rank == 2) ? z->shape.dims[0] : 1;
    uint32_t latent_dim = (z->shape.rank == 2) ? z->shape.dims[1] : z->shape.dims[0];

    if (latent_dim != decoder->config.latent_dim) {
        nimcp_mutex_unlock(decoder->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_INVALID_DIM,
                              "Latent dimension mismatch: expected %u, got %u",
                              decoder->config.latent_dim, latent_dim);
        return -1;
    }

    /* Create intermediate buffers */
    uint32_t max_dim = decoder->config.latent_dim;
    for (uint32_t i = 0; i < decoder->num_layers; i++) {
        if (decoder->layers[i].out_dim > max_dim) {
            max_dim = decoder->layers[i].out_dim;
        }
    }
    if (decoder->config.output_dim > max_dim) {
        max_dim = decoder->config.output_dim;
    }

    uint32_t buffer_dims[2] = {batch_size, max_dim};
    nimcp_tensor_t* current = nimcp_tensor_create(buffer_dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* next = nimcp_tensor_create(buffer_dims, 2, NIMCP_DTYPE_F32);

    if (!current || !next) {
        nimcp_mutex_unlock(decoder->mutex);
        nimcp_tensor_destroy(current);
        nimcp_tensor_destroy(next);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NO_MEMORY,
                              "Failed to allocate decoder intermediate buffers");
        return -1;
    }

    /* Copy latent to current buffer */
    memcpy(current->data, z->data, batch_size * latent_dim * sizeof(float));
    current->shape.dims[1] = latent_dim;

    /* Forward through hidden layers */
    for (uint32_t i = 0; i < decoder->num_layers; i++) {
        vae_decoder_layer_t* layer = &decoder->layers[i];
        next->shape.dims[1] = layer->out_dim;

        int result = vae_decoder_layer_forward(layer, current, next, decoder->is_training);
        if (result != 0) {
            nimcp_mutex_unlock(decoder->mutex);
            nimcp_tensor_destroy(current);
            nimcp_tensor_destroy(next);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "vae_decoder_forward: layer forward failed");
            return -1;
        }

        /* Swap buffers */
        nimcp_tensor_t* tmp = current;
        current = next;
        next = tmp;

        if ((i & 0x3) == 0 && decoder->num_layers > 4) {
            vae_decoder_heartbeat("vae_decoder_forward",
                                  0.3f + 0.4f * (float)(i + 1) / (float)decoder->num_layers);
        }
    }

    /* Output layer: reconstruction = current @ output_weights + output_bias */
    uint32_t final_dim = decoder->num_layers > 0 ?
                         decoder->layers[decoder->num_layers - 1].out_dim :
                         decoder->config.latent_dim;
    uint32_t output_dim = decoder->config.output_dim;

    float* curr_data = (float*)current->data;
    float* out_w = (float*)decoder->output_weights->data;
    float* out_b = (float*)decoder->output_bias->data;
    float* recon_data = (float*)reconstruction->data;

    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t j = 0; j < output_dim; j++) {
            float sum = out_b[j];
            for (uint32_t k = 0; k < final_dim; k++) {
                sum += curr_data[b * final_dim + k] * out_w[k * output_dim + j];
            }
            recon_data[b * output_dim + j] = sum;
        }
    }

    vae_decoder_heartbeat("vae_decoder_forward", 0.8f);

    /* Apply final activation */
    if (decoder->config.final_activation != VAE_ACTIVATION_LINEAR) {
        vae_activation_forward(decoder->config.final_activation,
                               reconstruction, reconstruction);
    }

    /* Check for NaN in output */
    for (uint32_t i = 0; i < batch_size * output_dim; i++) {
        if (is_nan_f(recon_data[i])) {
            nimcp_mutex_unlock(decoder->mutex);
            nimcp_tensor_destroy(current);
            nimcp_tensor_destroy(next);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_GRADIENT_NAN,
                                  "NaN detected in decoder output");
            return -1;
        }
    }

    /* Cleanup */
    nimcp_tensor_destroy(current);
    nimcp_tensor_destroy(next);

    decoder->forward_calls++;

    nimcp_mutex_unlock(decoder->mutex);

    vae_decoder_heartbeat("vae_decoder_forward", 1.0f);

    return 0;
}

int vae_decoder_forward_hetero(vae_decoder_t* decoder,
                               const nimcp_tensor_t* z,
                               nimcp_tensor_t* reconstruction,
                               nimcp_tensor_t* log_var) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL decoder in vae_decoder_forward_hetero");
        return -1;
    }

    /* First do standard forward pass */
    int result = vae_decoder_forward(decoder, z, reconstruction);
    if (result != 0) {
        return result;
    }

    /* If variance output requested and available */
    if (log_var && decoder->var_weights) {
        /* This would compute the variance output - simplified for now */
        uint32_t batch_size = (z->shape.rank == 2) ? z->shape.dims[0] : 1;
        uint32_t output_dim = decoder->config.output_dim;

        float* var_data = (float*)log_var->data;
        float* var_b = (float*)decoder->var_bias->data;

        /* For now, just use the bias as constant variance */
        for (uint32_t b = 0; b < batch_size; b++) {
            for (uint32_t j = 0; j < output_dim; j++) {
                var_data[b * output_dim + j] = var_b[j];
            }
        }
    }

    return 0;
}

/* ============================================================================
 * Backward Pass Implementation
 * ============================================================================ */

int vae_decoder_layer_backward(vae_decoder_layer_t* layer,
                               const nimcp_tensor_t* d_output,
                               nimcp_tensor_t* d_input) {
    if (!layer || !d_output || !d_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL parameter in vae_decoder_layer_backward");
        return -1;
    }

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

int vae_decoder_backward(vae_decoder_t* decoder,
                         const nimcp_tensor_t* d_reconstruction,
                         nimcp_tensor_t* d_z) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL decoder in vae_decoder_backward");
        return -1;
    }
    if (!d_reconstruction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL gradient tensor in vae_decoder_backward");
        return -1;
    }

    vae_decoder_heartbeat("vae_decoder_backward", 0.0f);

    nimcp_mutex_lock(decoder->mutex);

    /* Simplified backward pass */
    decoder->backward_calls++;

    nimcp_mutex_unlock(decoder->mutex);

    vae_decoder_heartbeat("vae_decoder_backward", 1.0f);

    return 0;
}

/* ============================================================================
 * Training API Implementation
 * ============================================================================ */

int vae_decoder_set_training(vae_decoder_t* decoder, bool training) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL decoder in vae_decoder_set_training");
        return -1;
    }

    nimcp_mutex_lock(decoder->mutex);
    decoder->is_training = training;
    nimcp_mutex_unlock(decoder->mutex);

    return 0;
}

int vae_decoder_apply_gradients(vae_decoder_t* decoder, float learning_rate) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL decoder in vae_decoder_apply_gradients");
        return -1;
    }

    vae_decoder_heartbeat("vae_decoder_apply_gradients", 0.0f);

    nimcp_mutex_lock(decoder->mutex);

    /* Update hidden layer weights */
    for (uint32_t i = 0; i < decoder->num_layers; i++) {
        vae_decoder_layer_t* layer = &decoder->layers[i];
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

    /* Update output layer weights */
    uint32_t output_size = decoder->output_weights->shape.dims[0] *
                           decoder->output_weights->shape.dims[1];
    uint32_t output_dim = decoder->config.output_dim;

    float* out_w = (float*)decoder->output_weights->data;
    float* out_w_grad = (float*)decoder->output_weights_grad->data;
    for (uint32_t i = 0; i < output_size; i++) {
        out_w[i] -= learning_rate * out_w_grad[i];
    }

    float* out_b = (float*)decoder->output_bias->data;
    float* out_b_grad = (float*)decoder->output_bias_grad->data;
    for (uint32_t i = 0; i < output_dim; i++) {
        out_b[i] -= learning_rate * out_b_grad[i];
    }

    /* Update variance output if present */
    if (decoder->var_weights && decoder->var_weights_grad) {
        float* var_w = (float*)decoder->var_weights->data;
        float* var_w_grad = (float*)decoder->var_weights_grad->data;
        for (uint32_t i = 0; i < output_size; i++) {
            var_w[i] -= learning_rate * var_w_grad[i];
        }

        float* var_b = (float*)decoder->var_bias->data;
        float* var_b_grad = (float*)decoder->var_bias_grad->data;
        for (uint32_t i = 0; i < output_dim; i++) {
            var_b[i] -= learning_rate * var_b_grad[i];
        }
    }

    nimcp_mutex_unlock(decoder->mutex);

    vae_decoder_heartbeat("vae_decoder_apply_gradients", 1.0f);

    return 0;
}

int vae_decoder_zero_grad(vae_decoder_t* decoder) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL decoder in vae_decoder_zero_grad");
        return -1;
    }

    /* Zero hidden layer gradients */
    for (uint32_t i = 0; i < decoder->num_layers; i++) {
        vae_decoder_layer_t* layer = &decoder->layers[i];

        if (layer->weight_grad) {
            memset(layer->weight_grad->data, 0,
                   layer->in_dim * layer->out_dim * sizeof(float));
        }
        if (layer->bias_grad) {
            memset(layer->bias_grad->data, 0, layer->out_dim * sizeof(float));
        }
    }

    /* Zero output layer gradients */
    if (decoder->output_weights_grad) {
        size_t size = decoder->output_weights_grad->shape.dims[0] *
                      decoder->output_weights_grad->shape.dims[1] * sizeof(float);
        memset(decoder->output_weights_grad->data, 0, size);
    }
    if (decoder->output_bias_grad) {
        memset(decoder->output_bias_grad->data, 0,
               decoder->config.output_dim * sizeof(float));
    }

    /* Zero variance gradients if present */
    if (decoder->var_weights_grad) {
        size_t size = decoder->var_weights_grad->shape.dims[0] *
                      decoder->var_weights_grad->shape.dims[1] * sizeof(float);
        memset(decoder->var_weights_grad->data, 0, size);
    }
    if (decoder->var_bias_grad) {
        memset(decoder->var_bias_grad->data, 0,
               decoder->config.output_dim * sizeof(float));
    }

    return 0;
}

int vae_decoder_clip_gradients(vae_decoder_t* decoder, float max_norm) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_NULL_POINTER,
                              "NULL decoder in vae_decoder_clip_gradients");
        return -1;
    }

    float total_norm = vae_decoder_grad_norm(decoder);

    if (total_norm > max_norm) {
        float scale = max_norm / (total_norm + 1e-6f);

        /* Scale all gradients */
        for (uint32_t i = 0; i < decoder->num_layers; i++) {
            vae_decoder_layer_t* layer = &decoder->layers[i];

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

        /* Scale output gradients */
        uint32_t output_size = decoder->output_weights_grad->shape.dims[0] *
                               decoder->output_weights_grad->shape.dims[1];

        float* out_w_g = (float*)decoder->output_weights_grad->data;
        for (uint32_t i = 0; i < output_size; i++) {
            out_w_g[i] *= scale;
        }

        float* out_b_g = (float*)decoder->output_bias_grad->data;
        for (uint32_t i = 0; i < decoder->config.output_dim; i++) {
            out_b_g[i] *= scale;
        }
    }

    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

uint64_t vae_decoder_param_count(const vae_decoder_t* decoder) {
    if (!decoder) return 0;

    uint64_t count = 0;

    /* Hidden layers */
    for (uint32_t i = 0; i < decoder->num_layers; i++) {
        const vae_decoder_layer_t* layer = &decoder->layers[i];
        count += layer->in_dim * layer->out_dim;
        if (layer->use_bias) {
            count += layer->out_dim;
        }
        if (layer->batch_norm) {
            count += 2 * layer->out_dim;
        }
    }

    /* Output layer */
    uint32_t output_in_dim = decoder->num_layers > 0 ?
                             decoder->layers[decoder->num_layers - 1].out_dim :
                             decoder->config.latent_dim;
    count += output_in_dim * decoder->config.output_dim + decoder->config.output_dim;

    /* Variance output */
    if (decoder->config.output_variance) {
        count += output_in_dim * decoder->config.output_dim + decoder->config.output_dim;
    }

    return count;
}

float vae_decoder_grad_norm(const vae_decoder_t* decoder) {
    if (!decoder) return 0.0f;

    float sum_sq = 0.0f;

    /* Hidden layers */
    for (uint32_t i = 0; i < decoder->num_layers; i++) {
        const vae_decoder_layer_t* layer = &decoder->layers[i];

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

    /* Output layer */
    if (decoder->output_weights_grad) {
        float* g = (float*)decoder->output_weights_grad->data;
        uint32_t size = decoder->output_weights_grad->shape.dims[0] *
                        decoder->output_weights_grad->shape.dims[1];
        for (uint32_t i = 0; i < size; i++) {
            sum_sq += g[i] * g[i];
        }
    }
    if (decoder->output_bias_grad) {
        float* g = (float*)decoder->output_bias_grad->data;
        for (uint32_t i = 0; i < decoder->config.output_dim; i++) {
            sum_sq += g[i] * g[i];
        }
    }

    return sqrtf(sum_sq);
}

bool vae_decoder_has_nan(const vae_decoder_t* decoder) {
    if (!decoder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_decoder_has_nan: decoder is NULL");
        return false;
    }

    /* Check hidden layers */
    for (uint32_t i = 0; i < decoder->num_layers; i++) {
        const vae_decoder_layer_t* layer = &decoder->layers[i];

        if (layer->weights) {
            float* w = (float*)layer->weights->data;
            uint32_t size = layer->in_dim * layer->out_dim;
            for (uint32_t j = 0; j < size; j++) {
                if (is_nan_f(w[j]) || is_inf_f(w[j])) return true;
            }
        }
    }

    /* Check output layer */
    if (decoder->output_weights) {
        float* w = (float*)decoder->output_weights->data;
        uint32_t size = decoder->output_weights->shape.dims[0] *
                        decoder->output_weights->shape.dims[1];
        for (uint32_t i = 0; i < size; i++) {
            if (is_nan_f(w[i]) || is_inf_f(w[i])) return true;
        }
    }

    return false;  /* No NaN/Inf found */
}

uint32_t vae_decoder_get_layer_dim(const vae_decoder_t* decoder, uint32_t layer_idx) {
    if (!decoder) return 0;

    if (layer_idx >= decoder->num_layers) {
        return decoder->config.output_dim;
    }

    return decoder->layers[layer_idx].out_dim;
}

void vae_decoder_set_health_agent(vae_decoder_t* decoder,
                                  nimcp_health_agent_t* agent) {
    if (decoder) {
        g_vae_decoder_health_agent = agent;
    }
}
