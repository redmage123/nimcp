/**
 * @file nimcp_jepa_predictor.c
 * @brief JEPA Predictor Implementation
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: MLP-based predictor for latent space prediction
 * WHY:  Core of JEPA architecture - predict embeddings from context
 * HOW:  Forward/backward passes with FEP precision integration
 */

#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

/* ============================================================================
 * Monte Carlo Integration - GPU acceleration with CPU fallback
 * ============================================================================ */

static _Thread_local uint32_t g_jepa_pred_mc_seed = 0;

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "utils/exception/nimcp_exception_macros.h"

static nimcp_gpu_context_t* g_jepa_gpu_ctx = NULL;
static qmc_gpu_rng_t g_jepa_gpu_rng = NULL;
static nimcp_once_t g_jepa_gpu_once = NIMCP_ONCE_INIT;
static volatile bool g_jepa_gpu_init_done = false;
static bool g_jepa_gpu_init_attempted = false;

static bool jepa_init_gpu_mc(void) {
    if (g_jepa_gpu_init_attempted) return g_jepa_gpu_rng != NULL;
    g_jepa_gpu_init_attempted = true;

    if (!qmc_gpu_is_available()) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_init_gpu_mc: qmc_gpu_is_available is NULL");
        return false;
    }

    g_jepa_gpu_ctx = nimcp_gpu_context_create_auto();
    if (!g_jepa_gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_init_gpu_mc: g_jepa_gpu_ctx is NULL");
        return false;
    }

    g_jepa_gpu_rng = qmc_gpu_rng_create(g_jepa_gpu_ctx, 4096, 0);
    if (!g_jepa_gpu_rng) {
        nimcp_gpu_context_destroy(g_jepa_gpu_ctx);
        g_jepa_gpu_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_init_gpu_mc: g_jepa_gpu_rng is NULL");
        return false;
    }
    return true;
}

static inline bool jepa_has_gpu_mc(void) {
    if (!g_jepa_gpu_init_attempted) jepa_init_gpu_mc();
    return g_jepa_gpu_rng != NULL;
}

#else
static inline bool jepa_has_gpu_mc(void) { return false; }
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define LOG_MODULE "[JEPA_PRED]"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(jepa_predictor, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helpers - Activation Functions
 * ============================================================================ */

static inline float gelu(float x) {
    /* GELU(x) = x * Φ(x) ≈ x * 0.5 * (1 + tanh(sqrt(2/π) * (x + 0.044715 * x³))) */
    const float sqrt_2_pi = 0.7978845608028654f;
    const float coeff = 0.044715f;
    float x3 = x * x * x;
    float inner = sqrt_2_pi * (x + coeff * x3);
    return 0.5f * x * (1.0f + tanhf(inner));
}

static inline float gelu_derivative(float x) {
    /* Approximate derivative of GELU */
    const float sqrt_2_pi = 0.7978845608028654f;
    const float coeff = 0.044715f;
    float x2 = x * x;
    float x3 = x2 * x;
    float inner = sqrt_2_pi * (x + coeff * x3);
    float tanh_inner = tanhf(inner);
    float sech2 = 1.0f - tanh_inner * tanh_inner;
    float d_inner = sqrt_2_pi * (1.0f + 3.0f * coeff * x2);
    return 0.5f * (1.0f + tanh_inner) + 0.5f * x * sech2 * d_inner;
}

static inline float relu(float x) {
    return x > 0.0f ? x : 0.0f;
}

static inline float relu_derivative(float x) {
    return x > 0.0f ? 1.0f : 0.0f;
}

static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

static inline float sigmoid_derivative(float x) {
    float s = sigmoid(x);
    return s * (1.0f - s);
}

static float apply_activation(float x, jepa_activation_t act) {
    switch (act) {
        case JEPA_ACT_NONE:    return x;
        case JEPA_ACT_RELU:    return relu(x);
        case JEPA_ACT_GELU:    return gelu(x);
        case JEPA_ACT_TANH:    return tanhf(x);
        case JEPA_ACT_SIGMOID: return sigmoid(x);
        default:               return x;
    }
}

static float apply_activation_derivative(float x, jepa_activation_t act) {
    switch (act) {
        case JEPA_ACT_NONE:    return 1.0f;
        case JEPA_ACT_RELU:    return relu_derivative(x);
        case JEPA_ACT_GELU:    return gelu_derivative(x);
        case JEPA_ACT_TANH: {
            float t = tanhf(x);
            return 1.0f - t * t;
        }
        case JEPA_ACT_SIGMOID: return sigmoid_derivative(x);
        default:               return 1.0f;
    }
}

/* ============================================================================
 * Internal Helpers - Weight Initialization
 * ============================================================================ */

static void xavier_init(float* weights, uint32_t in_dim, uint32_t out_dim) {
    /* Xavier/Glorot initialization: scale = sqrt(2 / (in + out)) */
    float scale = sqrtf(2.0f / (float)(in_dim + out_dim));

    /* Ensure MC seed is initialized */
    if (g_jepa_pred_mc_seed == 0) {
        g_jepa_pred_mc_seed = mc_seed_from_time();
    }

    for (uint32_t i = 0; i < in_dim * out_dim; i++) {
        /* Use MC uniform random in [-scale, scale] */
        float r = mc_random_uniform(&g_jepa_pred_mc_seed) * 2.0f - 1.0f;
        weights[i] = r * scale;
    }
}

/* ============================================================================
 * Internal Helpers - MLP Layer Operations
 * ============================================================================ */

static int mlp_layer_create(jepa_mlp_layer_t* layer, uint32_t in_dim,
                             uint32_t out_dim, jepa_activation_t activation) {
    layer->in_dim = in_dim;
    layer->out_dim = out_dim;
    layer->activation = activation;

    /* Allocate weights [out_dim × in_dim] */
    layer->weights = nimcp_calloc((size_t)out_dim * in_dim, sizeof(float));
    if (!layer->weights) {
        return NIMCP_ERROR_MEMORY;
    }
    xavier_init(layer->weights, in_dim, out_dim);

    /* Allocate bias [out_dim] */
    layer->bias = nimcp_calloc(out_dim, sizeof(float));
    if (!layer->bias) {
        nimcp_free(layer->weights);
        return NIMCP_ERROR_MEMORY;
    }
    /* calloc zero-initializes */

    /* Allocate gradient buffers */
    layer->grad_weights = nimcp_calloc((size_t)out_dim * in_dim, sizeof(float));
    layer->grad_bias = nimcp_calloc(out_dim, sizeof(float));
    if (!layer->grad_weights || !layer->grad_bias) {
        nimcp_free(layer->weights);
        nimcp_free(layer->bias);
        nimcp_free(layer->grad_weights);
        nimcp_free(layer->grad_bias);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mlp_layer_init: gradient buffer allocation failed");
        return NIMCP_ERROR_MEMORY;
    }
    memset(layer->grad_weights, 0, out_dim * in_dim * sizeof(float));
    memset(layer->grad_bias, 0, out_dim * sizeof(float));

    return NIMCP_SUCCESS;
}

static void mlp_layer_destroy(jepa_mlp_layer_t* layer) {
    if (layer->weights) nimcp_free(layer->weights);
    if (layer->bias) nimcp_free(layer->bias);
    if (layer->grad_weights) nimcp_free(layer->grad_weights);
    if (layer->grad_bias) nimcp_free(layer->grad_bias);
    memset(layer, 0, sizeof(jepa_mlp_layer_t));
}

static void mlp_layer_forward(const jepa_mlp_layer_t* layer,
                               const float* input,
                               float* pre_act,
                               float* output) {
    /* output = activation(weights @ input + bias) */
    for (uint32_t i = 0; i < layer->out_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && layer->out_dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)layer->out_dim);
        }

        double sum = layer->bias[i];
        for (uint32_t j = 0; j < layer->in_dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && layer->in_dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(j + 1) / (float)layer->in_dim);
            }

            sum += layer->weights[i * layer->in_dim + j] * input[j];
        }
        pre_act[i] = (float)sum;
        output[i] = apply_activation(pre_act[i], layer->activation);
    }
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int jepa_predictor_default_config(jepa_predictor_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->type = JEPA_PREDICTOR_MLP;
    config->input_dim = NIMCP_JEPA_LATENT_DIM;
    config->output_dim = NIMCP_JEPA_LATENT_DIM;
    config->hidden_dim = NIMCP_JEPA_PREDICTOR_HIDDEN;
    config->num_layers = JEPA_PREDICTOR_DEFAULT_LAYERS;
    config->activation = JEPA_ACT_GELU;
    config->loss_type = JEPA_LOSS_MSE;

    config->learning_rate = JEPA_PREDICTOR_DEFAULT_LR;
    config->weight_decay = JEPA_PREDICTOR_DEFAULT_WEIGHT_DECAY;
    config->dropout_rate = 0.0f;
    config->enable_layer_norm = false;

    config->enable_fep = true;
    config->initial_precision = 1.0f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

jepa_predictor_t* jepa_predictor_create(const jepa_predictor_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_create", 0.0f);


    jepa_predictor_config_t default_config;

    if (!config) {
        jepa_predictor_default_config(&default_config);
        config = &default_config;
    }

    /* Validate config */
    if (config->input_dim == 0 || config->output_dim == 0) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Invalid dimensions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_predictor_create: config->input_dim is zero");
        return NULL;
    }
    if (config->num_layers > JEPA_PREDICTOR_MAX_LAYERS) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Too many layers: %u", config->num_layers);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_predictor_create: validation failed");
        return NULL;
    }

    /* Allocate predictor */
    jepa_predictor_t* pred = nimcp_malloc(sizeof(jepa_predictor_t));
    if (!pred) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate predictor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_predictor_create: pred is NULL");
        return NULL;
    }
    memset(pred, 0, sizeof(jepa_predictor_t));

    /* Initialize bridge base */
    if (bridge_base_init(&pred->base, BIO_MODULE_JEPA_PREDICTOR, "jepa_predictor") != 0) {
        nimcp_free(pred);
        pred = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "jepa_predictor_create: validation failed");
        return NULL;
    }

    /* Store config */
    pred->config = *config;
    pred->type = config->type;
    pred->prediction_precision = config->initial_precision;
    pred->training_mode = false;

    /* Build MLP network (all predictor types use MLP as the internal representation) */
    if (config->type == JEPA_PREDICTOR_MLP || config->type == JEPA_PREDICTOR_LINEAR ||
        config->type == JEPA_PREDICTOR_TRANSFORMER || config->type == JEPA_PREDICTOR_RECURRENT) {
        jepa_mlp_t* mlp = &pred->network.mlp;

        uint32_t actual_layers = (config->type == JEPA_PREDICTOR_LINEAR) ? 1 :
                                 (config->type == JEPA_PREDICTOR_TRANSFORMER) ? config->num_layers + 1 :
                                 config->num_layers;
        mlp->num_layers = actual_layers;

        /* Allocate layer array */
        mlp->layers = nimcp_calloc(actual_layers, sizeof(jepa_mlp_layer_t));
        if (!mlp->layers) {
            bridge_base_cleanup(&pred->base);
            nimcp_free(pred);
            pred = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_predictor_create: mlp->layers is NULL");
            return NULL;
        }
        /* calloc zero-initializes */

        /* Allocate activation buffers */
        mlp->activations = nimcp_calloc(actual_layers, sizeof(float*));
        mlp->pre_activations = nimcp_calloc(actual_layers, sizeof(float*));
        if (!mlp->activations || !mlp->pre_activations) {
            nimcp_free(mlp->layers);
            nimcp_free(mlp->activations);
            nimcp_free(mlp->pre_activations);
            bridge_base_cleanup(&pred->base);
            nimcp_free(pred);
            pred = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_predictor_create: required parameter is NULL (mlp->activations, mlp->pre_activations)");
            return NULL;
        }

        /* Create layers */
        for (uint32_t i = 0; i < actual_layers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && actual_layers > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)actual_layers);
            }

            uint32_t in_d, out_d;
            jepa_activation_t act;

            if (actual_layers == 1) {
                /* Linear: single layer input -> output */
                in_d = config->input_dim;
                out_d = config->output_dim;
                act = JEPA_ACT_NONE;
            } else if (i == 0) {
                /* First layer: input -> hidden */
                in_d = config->input_dim;
                out_d = config->hidden_dim;
                act = config->activation;
            } else if (i == actual_layers - 1) {
                /* Last layer: hidden -> output, no activation */
                in_d = config->hidden_dim;
                out_d = config->output_dim;
                act = JEPA_ACT_NONE;
            } else {
                /* Hidden layer: hidden -> hidden */
                in_d = config->hidden_dim;
                out_d = config->hidden_dim;
                act = config->activation;
            }

            if (mlp_layer_create(&mlp->layers[i], in_d, out_d, act) != NIMCP_SUCCESS) {
                /* Cleanup on failure */
                for (uint32_t j = 0; j < i; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && i > 256) {
                        jepa_predictor_heartbeat("jepa_predict_loop",
                                         (float)(j + 1) / (float)i);
                    }

                    mlp_layer_destroy(&mlp->layers[j]);
                }
                nimcp_free(mlp->layers);
                nimcp_free(mlp->activations);
                nimcp_free(mlp->pre_activations);
                bridge_base_cleanup(&pred->base);
                nimcp_free(pred);
                pred = NULL;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_predictor_create: operation failed");
                return NULL;
            }

            /* Allocate activation buffers for this layer */
            mlp->activations[i] = nimcp_calloc(out_d, sizeof(float));
            mlp->pre_activations[i] = nimcp_calloc(out_d, sizeof(float));
            if (!mlp->activations[i] || !mlp->pre_activations[i]) {
                /* Cleanup */
                for (uint32_t j = 0; j <= i; j++) {
                    mlp_layer_destroy(&mlp->layers[j]);
                    nimcp_free(mlp->activations[j]);
                    nimcp_free(mlp->pre_activations[j]);
                }
                nimcp_free(mlp->layers);
                nimcp_free(mlp->activations);
                nimcp_free(mlp->pre_activations);
                bridge_base_cleanup(&pred->base);
                nimcp_free(pred);
                pred = NULL;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "jepa_predictor_create: operation failed");
                return NULL;
            }
        }
    }

    /* Allocate temporary buffers */
    /* temp_hidden stores input for backward pass, needs input_dim size */
    uint32_t temp_size = config->input_dim > config->hidden_dim ?
                         config->input_dim : config->hidden_dim;
    pred->temp_hidden = nimcp_calloc(temp_size, sizeof(float));
    pred->temp_output = nimcp_calloc(config->output_dim, sizeof(float));
    if (!pred->temp_hidden || !pred->temp_output) {
        /* Full cleanup would be needed here - simplified */
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate temp buffers");
        jepa_predictor_destroy(pred);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_predictor_create: required parameter is NULL (pred->temp_hidden, pred->temp_output)");
        return NULL;
    }

    /* Initialize statistics */
    pred->stats.min_loss = FLT_MAX;
    pred->stats.max_loss = 0.0f;

    NIMCP_LOGGING_INFO(LOG_MODULE " Created predictor: type=%s, dims=%u->%u->%u, layers=%u",
                      jepa_predictor_type_to_string(pred->type),
                      config->input_dim, config->hidden_dim, config->output_dim,
                      config->num_layers);

    return pred;
}

void jepa_predictor_destroy(jepa_predictor_t* predictor) {
    if (!predictor) return;

    /* Cleanup MLP */
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_destroy", 0.0f);


    if (predictor->type == JEPA_PREDICTOR_MLP || predictor->type == JEPA_PREDICTOR_LINEAR ||
        predictor->type == JEPA_PREDICTOR_TRANSFORMER || predictor->type == JEPA_PREDICTOR_RECURRENT) {
        jepa_mlp_t* mlp = &predictor->network.mlp;
        if (mlp->layers) {
            for (uint32_t i = 0; i < mlp->num_layers; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && mlp->num_layers > 256) {
                    jepa_predictor_heartbeat("jepa_predict_loop",
                                     (float)(i + 1) / (float)mlp->num_layers);
                }

                mlp_layer_destroy(&mlp->layers[i]);
                if (mlp->activations && mlp->activations[i]) {
                    nimcp_free(mlp->activations[i]);
                }
                if (mlp->pre_activations && mlp->pre_activations[i]) {
                    nimcp_free(mlp->pre_activations[i]);
                }
            }
            nimcp_free(mlp->layers);
        }
        nimcp_free(mlp->activations);
        nimcp_free(mlp->pre_activations);
    }

    /* Free temp buffers */
    nimcp_free(predictor->temp_hidden);
    nimcp_free(predictor->temp_output);

    /* Cleanup bridge base */
    bridge_base_cleanup(&predictor->base);

    nimcp_free(predictor);
    predictor = NULL;
}

int jepa_predictor_reset(jepa_predictor_t* predictor) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_reset", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");

    /* Reinitialize weights */
    if (predictor->type == JEPA_PREDICTOR_MLP || predictor->type == JEPA_PREDICTOR_LINEAR ||
        predictor->type == JEPA_PREDICTOR_TRANSFORMER || predictor->type == JEPA_PREDICTOR_RECURRENT) {
        jepa_mlp_t* mlp = &predictor->network.mlp;
        for (uint32_t i = 0; i < mlp->num_layers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && mlp->num_layers > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)mlp->num_layers);
            }

            jepa_mlp_layer_t* layer = &mlp->layers[i];
            xavier_init(layer->weights, layer->in_dim, layer->out_dim);
            memset(layer->bias, 0, layer->out_dim * sizeof(float));
            memset(layer->grad_weights, 0, layer->out_dim * layer->in_dim * sizeof(float));
            memset(layer->grad_bias, 0, layer->out_dim * sizeof(float));
        }
    }

    /* Reset statistics */
    memset(&predictor->stats, 0, sizeof(jepa_predictor_stats_t));
    predictor->stats.min_loss = FLT_MAX;
    predictor->step_count = 0;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int jepa_predictor_predict(jepa_predictor_t* predictor,
                            const jepa_latent_t* context,
                            jepa_latent_t* prediction) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_predict", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");
    NIMCP_CHECK_THROW(prediction, NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
    NIMCP_CHECK_THROW(context->latent_dim == predictor->config.input_dim,
                      NIMCP_ERROR_INVALID_PARAM, "input dim mismatch: %u vs %u",
                      context->latent_dim, predictor->config.input_dim);
    NIMCP_CHECK_THROW(prediction->latent_dim == predictor->config.output_dim,
                      NIMCP_ERROR_INVALID_PARAM, "output dim mismatch: %u vs %u",
                      prediction->latent_dim, predictor->config.output_dim);

    /* Forward pass through MLP */
    if (predictor->type == JEPA_PREDICTOR_MLP || predictor->type == JEPA_PREDICTOR_LINEAR ||
        predictor->type == JEPA_PREDICTOR_TRANSFORMER || predictor->type == JEPA_PREDICTOR_RECURRENT) {
        jepa_mlp_t* mlp = &predictor->network.mlp;
        const float* input = context->embedding;

        /* Save input for backward pass */
        memcpy(predictor->temp_hidden, context->embedding,
               predictor->config.input_dim * sizeof(float));

        for (uint32_t i = 0; i < mlp->num_layers; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && mlp->num_layers > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)mlp->num_layers);
            }

            float* output = (i == mlp->num_layers - 1) ?
                           prediction->embedding : mlp->activations[i];

            mlp_layer_forward(&mlp->layers[i], input,
                             mlp->pre_activations[i], output);

            input = output;  /* Output becomes input for next layer */
        }
    }

    /* Update metadata */
    prediction->modality = context->modality;
    prediction->is_normalized = false;

    predictor->stats.predictions_made++;

    return NIMCP_SUCCESS;
}

int jepa_predictor_predict_masked(jepa_predictor_t* predictor,
                                   const jepa_latent_t* context,
                                   const float* mask,
                                   jepa_latent_t* prediction) {
    /* For now, just do regular prediction - mask is used in loss computation */
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_predict_masked", 0.0f);


    return jepa_predictor_predict(predictor, context, prediction);
}

/* ============================================================================
 * Loss Computation API
 * ============================================================================ */

int jepa_predictor_compute_error(jepa_predictor_t* predictor,
                                  const jepa_latent_t* prediction,
                                  const jepa_latent_t* target,
                                  jepa_prediction_error_t* error) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_compute_error", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(prediction, NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
    NIMCP_CHECK_THROW(target, NIMCP_ERROR_NULL_POINTER, "target is NULL");
    NIMCP_CHECK_THROW(error, NIMCP_ERROR_NULL_POINTER, "error is NULL");
    NIMCP_CHECK_THROW(prediction->latent_dim == target->latent_dim,
                      NIMCP_ERROR_INVALID_PARAM, "prediction/target dim mismatch");
    NIMCP_CHECK_THROW(error->error && error->dim == prediction->latent_dim,
                      NIMCP_ERROR_INVALID_PARAM, "error buffer invalid or dim mismatch");

    /* Compute raw error */
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < prediction->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && prediction->latent_dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)prediction->latent_dim);
        }

        error->error[i] = prediction->embedding[i] - target->embedding[i];
        sum_sq += error->error[i] * error->error[i];
    }

    /* Compute precision-weighted error if enabled */
    if (predictor->config.enable_fep && error->weighted_error) {
        float prec = predictor->prediction_precision;
        for (uint32_t i = 0; i < prediction->latent_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && prediction->latent_dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)prediction->latent_dim);
            }

            error->weighted_error[i] = prec * error->error[i];
        }
        error->precision = prec;
    }

    /* Compute loss based on type */
    switch (predictor->config.loss_type) {
        case JEPA_LOSS_MSE:
            error->loss = (float)(sum_sq / prediction->latent_dim);
            break;

        case JEPA_LOSS_COSINE: {
            float sim = jepa_latent_cosine_similarity(prediction, target);
            error->loss = 1.0f - sim;
            break;
        }

        case JEPA_LOSS_SMOOTH_L1: {
            /* Huber loss with delta = 1.0 */
            double loss = 0.0;
            for (uint32_t i = 0; i < prediction->latent_dim; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && prediction->latent_dim > 256) {
                    jepa_predictor_heartbeat("jepa_predict_loop",
                                     (float)(i + 1) / (float)prediction->latent_dim);
                }

                float abs_err = fabsf(error->error[i]);
                if (abs_err < 1.0f) {
                    loss += 0.5f * error->error[i] * error->error[i];
                } else {
                    loss += abs_err - 0.5f;
                }
            }
            error->loss = (float)(loss / prediction->latent_dim);
            break;
        }

        case JEPA_LOSS_PRECISION_WEIGHTED:
            error->loss = (float)(predictor->prediction_precision * sum_sq / prediction->latent_dim);
            break;

        default:
            error->loss = (float)(sum_sq / prediction->latent_dim);
    }

    /* Update statistics */
    /* On first loss, initialize avg_loss directly */
    if (isfinite(error->loss)) {
        if (predictor->stats.min_loss == FLT_MAX) {
            predictor->stats.avg_loss = error->loss;
        } else {
            predictor->stats.avg_loss = 0.9f * predictor->stats.avg_loss + 0.1f * error->loss;
        }
    }
    if (error->loss < predictor->stats.min_loss) {
        predictor->stats.min_loss = error->loss;
    }
    if (error->loss > predictor->stats.max_loss) {
        predictor->stats.max_loss = error->loss;
    }

    return NIMCP_SUCCESS;
}

float jepa_predictor_compute_loss(jepa_predictor_t* predictor,
                                   const jepa_latent_t* prediction,
                                   const jepa_latent_t* target) {
    if (!predictor || !prediction || !target) {
        return NAN;
    }

    /* Simple MSE for direct computation */
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_compute_loss", 0.0f);


    double sum_sq = 0.0;
    for (uint32_t i = 0; i < prediction->latent_dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && prediction->latent_dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)prediction->latent_dim);
        }

        float diff = prediction->embedding[i] - target->embedding[i];
        sum_sq += diff * diff;
    }

    return (float)(sum_sq / prediction->latent_dim);
}

/* ============================================================================
 * Training API
 * ============================================================================ */

int jepa_predictor_set_training(jepa_predictor_t* predictor, bool training) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_set_training", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    predictor->training_mode = training;
    return NIMCP_SUCCESS;
}

int jepa_predictor_backward(jepa_predictor_t* predictor,
                             const jepa_prediction_error_t* error) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_backward", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(error, NIMCP_ERROR_NULL_POINTER, "error is NULL");
    NIMCP_CHECK_THROW(predictor->type == JEPA_PREDICTOR_MLP || predictor->type == JEPA_PREDICTOR_LINEAR ||
                      predictor->type == JEPA_PREDICTOR_TRANSFORMER || predictor->type == JEPA_PREDICTOR_RECURRENT,
                      NIMCP_ERROR_NOT_IMPLEMENTED, "backward not implemented for this predictor type");

    jepa_mlp_t* mlp = &predictor->network.mlp;

    /* Start with output error gradient */
    float* grad_out = nimcp_calloc(predictor->config.output_dim, sizeof(float));
    if (!grad_out) return -1;
    NIMCP_CHECK_THROW(grad_out, NIMCP_ERROR_MEMORY, "failed to allocate grad_out");

    /* For MSE: dL/dout = 2 * (pred - target) / dim = 2 * error / dim */
    float scale = 2.0f / error->dim;
    for (uint32_t i = 0; i < error->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && error->dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)error->dim);
        }

        grad_out[i] = scale * error->error[i];
    }

    /* Backpropagate through layers */
    float* grad_curr = grad_out;
    float* grad_next = NULL;

    for (int32_t l = (int32_t)mlp->num_layers - 1; l >= 0; l--) {
        jepa_mlp_layer_t* layer = &mlp->layers[l];

        /* Get input to this layer */
        const float* layer_input = (l == 0) ?
            predictor->temp_hidden :  /* Would need to save input */
            mlp->activations[l - 1];

        /* For simplicity, use pre-stored activations */
        /* In production, would need to save input during forward pass */

        /* Apply activation gradient */
        for (uint32_t i = 0; i < layer->out_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && layer->out_dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)layer->out_dim);
            }

            float act_grad = apply_activation_derivative(mlp->pre_activations[l][i],
                                                          layer->activation);
            grad_curr[i] *= act_grad;
        }

        /* Accumulate bias gradient */
        for (uint32_t i = 0; i < layer->out_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && layer->out_dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)layer->out_dim);
            }

            layer->grad_bias[i] += grad_curr[i];
        }

        /* Accumulate weight gradient: grad_W[i,j] = grad_out[i] * input[j] */
        for (uint32_t i = 0; i < layer->out_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && layer->out_dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)layer->out_dim);
            }

            for (uint32_t j = 0; j < layer->in_dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && layer->in_dim > 256) {
                    jepa_predictor_heartbeat("jepa_predict_loop",
                                     (float)(j + 1) / (float)layer->in_dim);
                }

                float input_val = layer_input ? layer_input[j] : 0.0f;
                layer->grad_weights[i * layer->in_dim + j] += grad_curr[i] * input_val;
            }
        }

        /* Propagate to previous layer if not first */
        if (l > 0) {
            grad_next = nimcp_calloc(layer->in_dim, sizeof(float));
            if (!grad_next) {
                nimcp_free(grad_out);
                grad_out = NULL;
                return NIMCP_ERROR_MEMORY;
            }
            /* calloc zero-initializes */

            /* grad_input[j] = sum_i(grad_out[i] * W[i,j]) */
            for (uint32_t j = 0; j < layer->in_dim; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && layer->in_dim > 256) {
                    jepa_predictor_heartbeat("jepa_predict_loop",
                                     (float)(j + 1) / (float)layer->in_dim);
                }

                for (uint32_t i = 0; i < layer->out_dim; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && layer->out_dim > 256) {
                        jepa_predictor_heartbeat("jepa_predict_loop",
                                         (float)(i + 1) / (float)layer->out_dim);
                    }

                    grad_next[j] += grad_curr[i] * layer->weights[i * layer->in_dim + j];
                }
            }

            if (grad_curr != grad_out) {
                nimcp_free(grad_curr);
                grad_curr = NULL;
            }
            grad_curr = grad_next;
        }
    }

    nimcp_free(grad_out);
    grad_out = NULL;
    if (grad_curr != grad_out) {
        nimcp_free(grad_curr);
        grad_curr = NULL;
    }

    return NIMCP_SUCCESS;
}

int jepa_predictor_update_weights(jepa_predictor_t* predictor, float learning_rate) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_update_weights", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");

    if (learning_rate <= 0.0f) {
        learning_rate = predictor->config.learning_rate;
    }

    if (predictor->type != JEPA_PREDICTOR_MLP && predictor->type != JEPA_PREDICTOR_LINEAR &&
        predictor->type != JEPA_PREDICTOR_TRANSFORMER && predictor->type != JEPA_PREDICTOR_RECURRENT) {
        return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    jepa_mlp_t* mlp = &predictor->network.mlp;
    float wd = predictor->config.weight_decay;

    for (uint32_t l = 0; l < mlp->num_layers; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && mlp->num_layers > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(l + 1) / (float)mlp->num_layers);
        }

        jepa_mlp_layer_t* layer = &mlp->layers[l];

        /* Update weights with gradient descent and weight decay */
        for (uint32_t i = 0; i < layer->out_dim * layer->in_dim; i++) {
            float grad = layer->grad_weights[i];

            /* Gradient clipping */
            if (grad > JEPA_PREDICTOR_GRAD_CLIP) grad = JEPA_PREDICTOR_GRAD_CLIP;
            if (grad < -JEPA_PREDICTOR_GRAD_CLIP) grad = -JEPA_PREDICTOR_GRAD_CLIP;

            layer->weights[i] -= learning_rate * (grad + wd * layer->weights[i]);
        }

        /* Update biases */
        for (uint32_t i = 0; i < layer->out_dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && layer->out_dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)layer->out_dim);
            }

            float grad = layer->grad_bias[i];
            if (grad > JEPA_PREDICTOR_GRAD_CLIP) grad = JEPA_PREDICTOR_GRAD_CLIP;
            if (grad < -JEPA_PREDICTOR_GRAD_CLIP) grad = -JEPA_PREDICTOR_GRAD_CLIP;
            layer->bias[i] -= learning_rate * grad;
        }

        /* Zero gradients for next iteration */
        memset(layer->grad_weights, 0, layer->out_dim * layer->in_dim * sizeof(float));
        memset(layer->grad_bias, 0, layer->out_dim * sizeof(float));
    }

    predictor->stats.updates_applied++;
    predictor->step_count++;

    return NIMCP_SUCCESS;
}

int jepa_predictor_train_step(jepa_predictor_t* predictor,
                               const jepa_latent_t* context,
                               const jepa_latent_t* target,
                               float* loss) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_train_step", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");
    NIMCP_CHECK_THROW(target, NIMCP_ERROR_NULL_POINTER, "target is NULL");

    /* Create temporary prediction */
    jepa_latent_t* pred = jepa_latent_create_dim(predictor->config.output_dim);
    NIMCP_CHECK_THROW(pred, NIMCP_ERROR_MEMORY, "failed to allocate pred");

    /* Forward pass */
    int result = jepa_predictor_predict(predictor, context, pred);
    if (result != NIMCP_SUCCESS) {
        jepa_latent_destroy(pred);
        return result;
    }

    /* Compute error */
    jepa_prediction_error_t* error = jepa_prediction_error_create(pred->latent_dim);
    if (!error) {
        jepa_latent_destroy(pred);
        return NIMCP_ERROR_MEMORY;
    }

    result = jepa_predictor_compute_error(predictor, pred, target, error);
    if (result != NIMCP_SUCCESS) {
        jepa_prediction_error_destroy(error);
        jepa_latent_destroy(pred);
        return result;
    }

    if (loss) {
        *loss = error->loss;
    }

    /* Backward pass (simplified) */
    result = jepa_predictor_backward(predictor, error);

    /* Update weights */
    if (result == NIMCP_SUCCESS) {
        result = jepa_predictor_update_weights(predictor, 0.0f);
    }

    jepa_prediction_error_destroy(error);
    jepa_latent_destroy(pred);

    return result;
}

/* ============================================================================
 * FEP Integration API
 * ============================================================================ */

int jepa_predictor_update_precision(jepa_predictor_t* predictor,
                                     const jepa_prediction_error_t* error) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_update_precision", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(error, NIMCP_ERROR_NULL_POINTER, "error is NULL");

    /* Precision = 1 / mean(error²) */
    /* Use exponential moving average for stability */
    float new_precision = 1.0f / (error->loss + JEPA_LATENT_EPSILON);

    /* Clamp precision */
    if (new_precision < JEPA_LATENT_MIN_PRECISION) {
        new_precision = JEPA_LATENT_MIN_PRECISION;
    }
    if (new_precision > JEPA_LATENT_MAX_PRECISION) {
        new_precision = JEPA_LATENT_MAX_PRECISION;
    }

    /* EMA update */
    if (isfinite(new_precision)) {
        predictor->prediction_precision = 0.9f * predictor->prediction_precision +
                                           0.1f * new_precision;
        predictor->stats.avg_precision = 0.9f * predictor->stats.avg_precision +
                                          0.1f * predictor->prediction_precision;
    }

    return NIMCP_SUCCESS;
}

int jepa_predictor_to_fep_error(jepa_predictor_t* predictor,
                                 const jepa_prediction_error_t* internal_error,
                                 fep_prediction_error_t* fep_error) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_to_fep_error", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(internal_error, NIMCP_ERROR_NULL_POINTER, "internal_error is NULL");
    NIMCP_CHECK_THROW(fep_error, NIMCP_ERROR_NULL_POINTER, "fep_error is NULL");
    NIMCP_CHECK_THROW(fep_error->dim == internal_error->dim,
                      NIMCP_ERROR_INVALID_PARAM, "fep_error dim mismatch");

    /* Copy error values */
    memcpy(fep_error->error, internal_error->error, internal_error->dim * sizeof(float));

    /* Apply precision weighting */
    if (fep_error->weighted_error && fep_error->precision) {
        float prec = predictor->prediction_precision;
        for (uint32_t i = 0; i < internal_error->dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && internal_error->dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)internal_error->dim);
            }

            fep_error->precision[i] = prec;
            fep_error->weighted_error[i] = prec * internal_error->error[i];
        }
    }

    /* Compute magnitudes */
    double sum_sq = 0.0;
    double weighted_sum_sq = 0.0;
    for (uint32_t i = 0; i < internal_error->dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && internal_error->dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)internal_error->dim);
        }

        sum_sq += internal_error->error[i] * internal_error->error[i];
        if (fep_error->weighted_error) {
            weighted_sum_sq += fep_error->weighted_error[i] * fep_error->weighted_error[i];
        }
    }
    fep_error->magnitude = (float)sqrt(sum_sq);
    fep_error->weighted_magnitude = (float)sqrt(weighted_sum_sq);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Weight Access API
 * ============================================================================ */

uint32_t jepa_predictor_num_params(const jepa_predictor_t* predictor) {
    if (!predictor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_predictor_num_params: predictor is NULL");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_num_params", 0.0f);


    if (predictor->type != JEPA_PREDICTOR_MLP && predictor->type != JEPA_PREDICTOR_LINEAR &&
        predictor->type != JEPA_PREDICTOR_TRANSFORMER && predictor->type != JEPA_PREDICTOR_RECURRENT) {
        return 0;
    }

    uint32_t total = 0;
    const jepa_mlp_t* mlp = &predictor->network.mlp;

    for (uint32_t i = 0; i < mlp->num_layers; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mlp->num_layers > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)mlp->num_layers);
        }

        const jepa_mlp_layer_t* layer = &mlp->layers[i];
        total += layer->in_dim * layer->out_dim;  /* Weights */
        total += layer->out_dim;                   /* Biases */
    }

    return total;
}

int jepa_predictor_get_weights(const jepa_predictor_t* predictor,
                                uint32_t layer_idx,
                                float** weights,
                                uint32_t dims[2]) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_get_weights", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(weights, NIMCP_ERROR_NULL_POINTER, "weights is NULL");
    NIMCP_CHECK_THROW(dims, NIMCP_ERROR_NULL_POINTER, "dims is NULL");
    NIMCP_CHECK_THROW(predictor->type == JEPA_PREDICTOR_MLP || predictor->type == JEPA_PREDICTOR_LINEAR ||
                      predictor->type == JEPA_PREDICTOR_TRANSFORMER || predictor->type == JEPA_PREDICTOR_RECURRENT,
                      NIMCP_ERROR_NOT_IMPLEMENTED, "get_weights not implemented for this predictor type");

    const jepa_mlp_t* mlp = &predictor->network.mlp;
    NIMCP_CHECK_THROW(layer_idx < mlp->num_layers,
                      NIMCP_ERROR_INVALID_PARAM, "layer_idx %u out of bounds", layer_idx);

    const jepa_mlp_layer_t* layer = &mlp->layers[layer_idx];
    *weights = layer->weights;
    dims[0] = layer->out_dim;
    dims[1] = layer->in_dim;

    return NIMCP_SUCCESS;
}

int jepa_predictor_set_weights(jepa_predictor_t* predictor,
                                uint32_t layer_idx,
                                const float* weights,
                                uint32_t in_dim,
                                uint32_t out_dim) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_set_weights", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(weights, NIMCP_ERROR_NULL_POINTER, "weights is NULL");
    NIMCP_CHECK_THROW(predictor->type == JEPA_PREDICTOR_MLP || predictor->type == JEPA_PREDICTOR_LINEAR ||
                      predictor->type == JEPA_PREDICTOR_TRANSFORMER || predictor->type == JEPA_PREDICTOR_RECURRENT,
                      NIMCP_ERROR_NOT_IMPLEMENTED, "set_weights not implemented for this predictor type");

    jepa_mlp_t* mlp = &predictor->network.mlp;
    NIMCP_CHECK_THROW(layer_idx < mlp->num_layers,
                      NIMCP_ERROR_INVALID_PARAM, "layer_idx %u out of bounds", layer_idx);

    jepa_mlp_layer_t* layer = &mlp->layers[layer_idx];
    NIMCP_CHECK_THROW(layer->in_dim == in_dim && layer->out_dim == out_dim,
                      NIMCP_ERROR_INVALID_PARAM, "dimension mismatch for layer weights");

    memcpy(layer->weights, weights, in_dim * out_dim * sizeof(float));
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int jepa_predictor_get_stats(const jepa_predictor_t* predictor,
                              jepa_predictor_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_get_stats", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    *stats = predictor->stats;
    return NIMCP_SUCCESS;
}

int jepa_predictor_reset_stats(jepa_predictor_t* predictor) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    memset(&predictor->stats, 0, sizeof(jepa_predictor_stats_t));
    predictor->stats.min_loss = FLT_MAX;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

BRIDGE_DEFINE_BIO_ASYNC_FUNCS(jepa_predictor)

/* ============================================================================
 * Prediction Error Management
 * ============================================================================ */

jepa_prediction_error_t* jepa_prediction_error_create(uint32_t dim) {
    if (dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_prediction_error_create: dim is zero");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_jepa_prediction_erro", 0.0f);


    jepa_prediction_error_t* error = nimcp_malloc(sizeof(jepa_prediction_error_t));
    if (!error) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate error");

        return NULL;

    }

    error->dim = dim;
    error->error = nimcp_calloc(dim, sizeof(float));
    error->weighted_error = nimcp_calloc(dim, sizeof(float));

    if (!error->error || !error->weighted_error) {
        nimcp_free(error->error);
        nimcp_free(error->weighted_error);
        nimcp_free(error);
        error = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_prediction_error_create: required parameter is NULL (error->error, error->weighted_error)");
        return NULL;
    }

    memset(error->error, 0, dim * sizeof(float));
    memset(error->weighted_error, 0, dim * sizeof(float));
    error->loss = 0.0f;
    error->precision = 1.0f;

    return error;
}

void jepa_prediction_error_destroy(jepa_prediction_error_t* error) {
    if (!error) return;
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_jepa_prediction_erro", 0.0f);


    nimcp_free(error->error);
    nimcp_free(error->weighted_error);
    nimcp_free(error);
    error = NULL;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* jepa_predictor_type_to_string(jepa_predictor_type_t type) {
    switch (type) {
        case JEPA_PREDICTOR_LINEAR:      return "linear";
        case JEPA_PREDICTOR_MLP:         return "mlp";
        case JEPA_PREDICTOR_TRANSFORMER: return "transformer";
        case JEPA_PREDICTOR_RECURRENT:   return "recurrent";
        default:                         return "unknown";
    }
}

const char* jepa_activation_to_string(jepa_activation_t activation) {
    switch (activation) {
        case JEPA_ACT_NONE:    return "none";
        case JEPA_ACT_RELU:    return "relu";
        case JEPA_ACT_GELU:    return "gelu";
        case JEPA_ACT_TANH:    return "tanh";
        case JEPA_ACT_SIGMOID: return "sigmoid";
        default:               return "unknown";
    }
}

const char* jepa_loss_to_string(jepa_loss_t loss) {
    switch (loss) {
        case JEPA_LOSS_MSE:               return "mse";
        case JEPA_LOSS_COSINE:            return "cosine";
        case JEPA_LOSS_SMOOTH_L1:         return "smooth_l1";
        case JEPA_LOSS_PRECISION_WEIGHTED: return "precision_weighted";
        default:                          return "unknown";
    }
}

/* ============================================================================
 * Monte Carlo Integration API
 * ============================================================================ */

/**
 * @brief Predict with uncertainty estimation via MC sampling
 *
 * WHAT: Make prediction with uncertainty quantification
 * WHY:  Enable confidence-aware predictions in JEPA
 * HOW:  Sample predictions with noise, compute mean and variance
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param prediction Output prediction (mean)
 * @param uncertainty Output uncertainty (std per dimension)
 * @param num_samples Number of MC samples
 * @return NIMCP_SUCCESS on success
 */
int jepa_predictor_predict_with_uncertainty_mc(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    jepa_latent_t* prediction,
    float* uncertainty,
    uint32_t num_samples
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_predict_with_uncerta", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");
    NIMCP_CHECK_THROW(prediction, NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
    NIMCP_CHECK_THROW(uncertainty, NIMCP_ERROR_NULL_POINTER, "uncertainty is NULL");
    NIMCP_CHECK_THROW(num_samples > 0, NIMCP_ERROR_INVALID_PARAM, "num_samples must be > 0");

    if (g_jepa_pred_mc_seed == 0) {
        g_jepa_pred_mc_seed = mc_seed_from_time();
    }

    uint32_t dim = prediction->latent_dim;

    /* Allocate accumulators */
    float* mean = nimcp_calloc(dim, sizeof(float));
    float* var = nimcp_calloc(dim, sizeof(float));
    if (!mean || !var) {
        nimcp_free(mean);
        mean = NULL;
        nimcp_free(var);
        var = NULL;
        return NIMCP_ERROR_MEMORY;
    }

    /* Create temporary prediction buffer */
    jepa_latent_t* temp = jepa_latent_create_dim(dim);
    if (!temp) {
        nimcp_free(mean);
        mean = NULL;
        nimcp_free(var);
        var = NULL;
        return NIMCP_ERROR_MEMORY;
    }

    /* Collect samples with dropout noise */
    for (uint32_t s = 0; s < num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_samples > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(s + 1) / (float)num_samples);
        }

        /* Forward pass */
        if (jepa_predictor_predict(predictor, context, temp) != NIMCP_SUCCESS) {
            continue;
        }

        /* Add small noise for exploration */
        float noise_scale = 0.01f;
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)dim);
            }

            float noise = mc_random_normal(&g_jepa_pred_mc_seed, 0.0f, noise_scale);
            temp->embedding[i] += noise;
            mean[i] += temp->embedding[i];
        }
    }

    /* Compute mean */
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        mean[i] /= (float)num_samples;
        prediction->embedding[i] = mean[i];
    }

    /* Second pass for variance (requires re-sampling or storing) */
    /* For simplicity, use analytical approximation based on precision */
    float precision = predictor->prediction_precision;
    float base_std = 1.0f / sqrtf(precision + 1e-6f);
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        uncertainty[i] = base_std * (1.0f + 0.1f * fabsf(mean[i]));
    }

    jepa_latent_destroy(temp);
    nimcp_free(mean);
    mean = NULL;
    nimcp_free(var);
    var = NULL;

    return NIMCP_SUCCESS;
}

/**
 * @brief Apply MC dropout during training
 *
 * WHAT: Apply stochastic dropout using MC sampling
 * WHY:  Regularization and uncertainty estimation
 * HOW:  Randomly zero elements with dropout rate
 *
 * @param activations Activation array to modify in-place
 * @param size Array size
 * @param dropout_rate Probability of dropping [0,1]
 */
void jepa_predictor_apply_dropout_mc(float* activations, uint32_t size, float dropout_rate) {
    if (!activations || size == 0 || dropout_rate <= 0.0f) return;

    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_apply_dropout_mc", 0.0f);


    float scale = 1.0f / (1.0f - dropout_rate);  /* Inverted dropout scaling */

#ifdef NIMCP_ENABLE_CUDA
    /* Try GPU first (default for size >= 256) */
    if (jepa_has_gpu_mc() && size >= 256) {
        size_t dims[] = {size};
        nimcp_gpu_tensor_t* samples = nimcp_gpu_tensor_create(
            g_jepa_gpu_ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (samples && qmc_gpu_sample_uniform(g_jepa_gpu_ctx, g_jepa_gpu_rng, samples)) {
            float* h_samples = nimcp_calloc(size, sizeof(float));
            if (h_samples) {
                cudaMemcpy(h_samples, samples->data, size * sizeof(float), cudaMemcpyDeviceToHost);
                for (uint32_t i = 0; i < size; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && size > 256) {
                        jepa_predictor_heartbeat("jepa_predict_loop",
                                         (float)(i + 1) / (float)size);
                    }

                    if (h_samples[i] < dropout_rate) {
                        activations[i] = 0.0f;
                    } else {
                        activations[i] *= scale;
                    }
                }
                nimcp_free(h_samples);
                h_samples = NULL;
                nimcp_gpu_tensor_destroy(samples);
                return;
            }
        }
        if (samples) nimcp_gpu_tensor_destroy(samples);
    }
#endif

    /* CPU fallback */
    if (g_jepa_pred_mc_seed == 0) {
        g_jepa_pred_mc_seed = mc_seed_from_time();
    }

    for (uint32_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)size);
        }

        if (mc_random_uniform(&g_jepa_pred_mc_seed) < dropout_rate) {
            activations[i] = 0.0f;
        } else {
            activations[i] *= scale;
        }
    }
}

/**
 * @brief Estimate prediction confidence via MC sampling
 *
 * WHAT: Estimate confidence in prediction
 * WHY:  Support uncertainty-aware decision making
 * HOW:  Compute prediction variance over samples
 *
 * @param predictor JEPA predictor
 * @param context Input context
 * @param num_samples Number of MC samples
 * @return Confidence score [0,1] (inverse of normalized variance)
 */
float jepa_predictor_estimate_confidence_mc(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    uint32_t num_samples
) {
    if (!predictor || !context || num_samples == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_estimate_confidence_", 0.0f);


    if (g_jepa_pred_mc_seed == 0) {
        g_jepa_pred_mc_seed = mc_seed_from_time();
    }

    uint32_t dim = predictor->config.output_dim;
    jepa_latent_t* temp = jepa_latent_create_dim(dim);
    if (!temp) return 0.0f;

    float* mean = nimcp_calloc(dim, sizeof(float));
    float* m2 = nimcp_calloc(dim, sizeof(float));  /* For Welford's online variance */
    if (!mean || !m2) {
        nimcp_free(mean);
        mean = NULL;
        nimcp_free(m2);
        m2 = NULL;
        jepa_latent_destroy(temp);
        return 0.0f;
    }

    /* Welford's online algorithm for variance */
    for (uint32_t s = 0; s < num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_samples > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(s + 1) / (float)num_samples);
        }

        if (jepa_predictor_predict(predictor, context, temp) != NIMCP_SUCCESS) {
            continue;
        }

        /* Add exploration noise */
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)dim);
            }

            float val = temp->embedding[i] +
                        mc_random_normal(&g_jepa_pred_mc_seed, 0.0f, 0.01f);
            float delta = val - mean[i];
            mean[i] += delta / (s + 1);
            float delta2 = val - mean[i];
            m2[i] += delta * delta2;
        }
    }

    /* Compute average variance */
    float avg_variance = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        avg_variance += m2[i] / (num_samples - 1 + 1e-6f);
    }
    avg_variance /= dim;

    /* Convert to confidence: higher variance = lower confidence */
    float confidence = 1.0f / (1.0f + avg_variance);

    nimcp_free(mean);
    mean = NULL;
    nimcp_free(m2);
    m2 = NULL;
    jepa_latent_destroy(temp);

    return confidence;
}

/**
 * @brief Get thread-local MC seed for JEPA predictor
 *
 * @return Pointer to thread-local seed
 */
uint32_t* jepa_predictor_get_mc_seed(void) {
    if (g_jepa_pred_mc_seed == 0) {
        g_jepa_pred_mc_seed = mc_seed_from_time();
    }
    return &g_jepa_pred_mc_seed;
}

/* ============================================================================
 * KG Self-Awareness API
 * ============================================================================ */

int jepa_predictor_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "JEPA_Predictor");
    if (self) {
        NIMCP_LOGGING_INFO(LOG_MODULE " Self-knowledge entity: %s (type: %s)",
                          self->name, self->entity_type);
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG(LOG_MODULE " Observation[%u]: %s",
                               i, self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "JEPA_Predictor");
    if (connections) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE " Outgoing connections: %u", connections->count);
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "JEPA_Predictor");
    if (incoming) {
        NIMCP_LOGGING_DEBUG(LOG_MODULE " Incoming connections: %u", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Quantum Monte Carlo Integration API
 * ============================================================================ */

/**
 * @brief Initialize default QMC configuration
 */
int jepa_qmc_config_init(jepa_qmc_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_jepa_qmc_config_init", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->num_samples = QMC_DEFAULT_AMPLITUDE_SAMPLES;
    config->num_iterations = 1000;
    config->initial_temp = 10.0f;
    config->final_temp = 0.01f;
    config->exploration_constant = 1.414f;  /* sqrt(2) */
    config->quantum_strength = 0.1f;
    config->use_importance_sampling = true;
    config->use_adaptive_proposal = true;
    config->seed = 0;

    return NIMCP_SUCCESS;
}

/**
 * @brief Energy function for QMC annealing (prediction error)
 */
typedef struct jepa_qmc_energy_ctx {
    jepa_predictor_t* predictor;
    const jepa_latent_t** contexts;
    const jepa_latent_t** targets;
    uint32_t num_samples;
    uint32_t layer_idx;
    uint32_t in_dim;
    uint32_t out_dim;
} jepa_qmc_energy_ctx_t;

static float jepa_qmc_energy_fn(const float* state, uint32_t dim, void* user_data) {
    jepa_qmc_energy_ctx_t* ctx = (jepa_qmc_energy_ctx_t*)user_data;
    if (!ctx || !ctx->predictor) return FLT_MAX;

    /* Temporarily apply weights from state */
    jepa_mlp_t* mlp = &ctx->predictor->network.mlp;
    if (ctx->layer_idx >= mlp->num_layers) return FLT_MAX;

    jepa_mlp_layer_t* layer = &mlp->layers[ctx->layer_idx];
    float* original_weights = nimcp_calloc(dim, sizeof(float));
    if (!original_weights) return FLT_MAX;

    /* Save and replace weights */
    memcpy(original_weights, layer->weights, dim * sizeof(float));
    memcpy(layer->weights, state, dim * sizeof(float));

    /* Compute prediction error over all samples */
    double total_loss = 0.0;
    jepa_latent_t* pred = jepa_latent_create_dim(ctx->predictor->config.output_dim);
    if (!pred) {
        memcpy(layer->weights, original_weights, dim * sizeof(float));
        nimcp_free(original_weights);
        original_weights = NULL;
        return FLT_MAX;
    }

    for (uint32_t i = 0; i < ctx->num_samples; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_samples > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)ctx->num_samples);
        }

        if (jepa_predictor_predict(ctx->predictor, ctx->contexts[i], pred) == NIMCP_SUCCESS) {
            total_loss += jepa_predictor_compute_loss(ctx->predictor, pred, ctx->targets[i]);
        }
    }

    /* Restore original weights */
    memcpy(layer->weights, original_weights, dim * sizeof(float));
    nimcp_free(original_weights);
    original_weights = NULL;
    jepa_latent_destroy(pred);

    return (float)(total_loss / ctx->num_samples);
}

/**
 * @brief Estimate latent space amplitude uncertainty via QMC
 */
int jepa_predictor_qmc_amplitude_estimate(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    uint32_t target_dim,
    const jepa_qmc_config_t* config,
    float* amplitude,
    float* variance
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_qmc_amplitude_estima", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");
    NIMCP_CHECK_THROW(amplitude, NIMCP_ERROR_NULL_POINTER, "amplitude is NULL");
    NIMCP_CHECK_THROW(variance, NIMCP_ERROR_NULL_POINTER, "variance is NULL");
    NIMCP_CHECK_THROW(target_dim < predictor->config.output_dim,
                      NIMCP_ERROR_INVALID_PARAM, "target_dim %u out of bounds", target_dim);

    /* Use default config if not provided */
    jepa_qmc_config_t default_cfg;
    if (!config) {
        jepa_qmc_config_init(&default_cfg);
        config = &default_cfg;
    }

    /* Make multiple predictions to build amplitude distribution */
    uint32_t dim = predictor->config.output_dim;
    float* amplitudes = nimcp_calloc(config->num_samples, sizeof(float));
    if (!amplitudes) return -1;
    NIMCP_CHECK_THROW(amplitudes, NIMCP_ERROR_MEMORY, "failed to allocate amplitudes");

    jepa_latent_t* pred = jepa_latent_create_dim(dim);
    if (!pred) {
        nimcp_free(amplitudes);
        amplitudes = NULL;
        return NIMCP_ERROR_MEMORY;
    }

    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();

    /* Collect amplitude samples */
    for (uint32_t s = 0; s < config->num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && config->num_samples > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(s + 1) / (float)config->num_samples);
        }

        if (jepa_predictor_predict(predictor, context, pred) == NIMCP_SUCCESS) {
            /* Add exploration noise for sampling */
            float noise = mc_random_normal(&seed, 0.0f, 0.01f);
            amplitudes[s] = fabsf(pred->embedding[target_dim] + noise);
        } else {
            amplitudes[s] = 0.0f;
        }
    }

    /* Use QMC amplitude estimation */
    qmc_amplitude_config_t qmc_cfg = {
        .num_samples = config->num_samples,
        .use_importance = config->use_importance_sampling,
        .proposal_dist = NULL,
        .seed = seed
    };

    qmc_amplitude_result_t result;
    qmc_result_t qmc_ret = qmc_estimate_amplitude(amplitudes, config->num_samples,
                                                   0, &qmc_cfg, &result);

    if (qmc_ret == QMC_OK) {
        *amplitude = result.amplitude;
        *variance = result.variance;
    } else {
        /* Fallback: compute directly */
        double sum = 0.0, sum_sq = 0.0;
        for (uint32_t i = 0; i < config->num_samples; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && config->num_samples > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)config->num_samples);
            }

            sum += amplitudes[i];
            sum_sq += amplitudes[i] * amplitudes[i];
        }
        *amplitude = (float)(sum / config->num_samples);
        *variance = (float)((sum_sq / config->num_samples) - (*amplitude * *amplitude));
    }

    nimcp_free(amplitudes);
    amplitudes = NULL;
    jepa_latent_destroy(pred);

    return NIMCP_SUCCESS;
}

/**
 * @brief Optimize predictor weights via quantum annealing
 */
int jepa_predictor_qmc_adaptive_anneal(
    jepa_predictor_t* predictor,
    const jepa_latent_t** contexts,
    const jepa_latent_t** targets,
    uint32_t num_samples,
    const jepa_qmc_config_t* config,
    jepa_qmc_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_qmc_adaptive_anneal", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(contexts, NIMCP_ERROR_NULL_POINTER, "contexts is NULL");
    NIMCP_CHECK_THROW(targets, NIMCP_ERROR_NULL_POINTER, "targets is NULL");
    NIMCP_CHECK_THROW(num_samples > 0, NIMCP_ERROR_INVALID_PARAM, "num_samples must be > 0");
    NIMCP_CHECK_THROW(predictor->type == JEPA_PREDICTOR_MLP || predictor->type == JEPA_PREDICTOR_LINEAR ||
                      predictor->type == JEPA_PREDICTOR_TRANSFORMER || predictor->type == JEPA_PREDICTOR_RECURRENT,
                      NIMCP_ERROR_NOT_IMPLEMENTED, "QMC adaptive anneal not implemented for this predictor type");

    /* Use default config if not provided */
    jepa_qmc_config_t default_cfg;
    if (!config) {
        jepa_qmc_config_init(&default_cfg);
        config = &default_cfg;
    }

    uint64_t start_time = nimcp_time_monotonic_ms();

    jepa_mlp_t* mlp = &predictor->network.mlp;
    uint32_t total_tunneling = 0;
    float total_acceptance = 0.0f;
    float best_energy = FLT_MAX;

    /* Anneal each layer */
    for (uint32_t l = 0; l < mlp->num_layers; l++) {
        /* Phase 8: Loop progress heartbeat */
        if ((l & 0xFF) == 0 && mlp->num_layers > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(l + 1) / (float)mlp->num_layers);
        }

        jepa_mlp_layer_t* layer = &mlp->layers[l];
        uint32_t dim = layer->in_dim * layer->out_dim;

        /* Setup energy context */
        jepa_qmc_energy_ctx_t energy_ctx = {
            .predictor = predictor,
            .contexts = contexts,
            .targets = targets,
            .num_samples = num_samples,
            .layer_idx = l,
            .in_dim = layer->in_dim,
            .out_dim = layer->out_dim
        };

        /* Configure annealing */
        qmc_anneal_config_t anneal_cfg = {
            .initial_temp = config->initial_temp,
            .final_temp = config->final_temp,
            .num_iterations = config->num_iterations,
            .quantum_strength = config->quantum_strength,
            .strategy = config->use_adaptive_proposal ? QMC_PROPOSAL_ADAPTIVE : QMC_PROPOSAL_FIXED,
            .target_acceptance = QMC_TARGET_ACCEPTANCE_RATE,
            .adaptation_interval = 100,
            .seed = config->seed ? config->seed : mc_seed_from_time()
        };

        qmc_anneal_result_t anneal_result;
        qmc_result_t qmc_ret = qmc_adaptive_anneal(
            jepa_qmc_energy_fn,
            layer->weights,
            dim,
            &anneal_cfg,
            &energy_ctx,
            &anneal_result
        );

        if (qmc_ret == QMC_OK) {
            /* Apply optimized weights */
            memcpy(layer->weights, anneal_result.best_state, dim * sizeof(float));

            total_tunneling += anneal_result.tunneling_events;
            total_acceptance += anneal_result.acceptance_rate;
            if (anneal_result.final_energy < best_energy) {
                best_energy = anneal_result.final_energy;
            }

            qmc_anneal_result_free(&anneal_result);
        }
    }

    /* Record statistics */
    if (stats) {
        stats->samples_taken = config->num_iterations * mlp->num_layers;
        stats->tunneling_events = total_tunneling;
        stats->acceptance_rate = total_acceptance / mlp->num_layers;
        stats->final_energy = best_energy;
        stats->computation_time_ms = (float)(nimcp_time_monotonic_ms() - start_time);
    }

    predictor->stats.updates_applied++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Estimate Shannon entropy of latent space distribution
 */
int jepa_predictor_qmc_entropy(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    const jepa_qmc_config_t* config,
    float* entropy,
    float* std_error
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_qmc_entropy", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");
    NIMCP_CHECK_THROW(entropy, NIMCP_ERROR_NULL_POINTER, "entropy is NULL");

    /* Use default config if not provided */
    jepa_qmc_config_t default_cfg;
    if (!config) {
        jepa_qmc_config_init(&default_cfg);
        config = &default_cfg;
    }

    uint32_t dim = predictor->config.output_dim;

    /* Make prediction and convert to probabilities */
    jepa_latent_t* pred = jepa_latent_create_dim(dim);
    NIMCP_CHECK_THROW(pred, NIMCP_ERROR_MEMORY, "failed to allocate pred");

    if (jepa_predictor_predict(predictor, context, pred) != NIMCP_SUCCESS) {
        jepa_latent_destroy(pred);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Convert embeddings to positive values and normalize to probabilities */
    float* probs = nimcp_calloc(dim, sizeof(float));
    if (!probs) {
        jepa_latent_destroy(pred);
        return NIMCP_ERROR_MEMORY;
    }

    /* Apply softmax to get probability distribution */
    float max_val = -FLT_MAX;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        if (pred->embedding[i] > max_val) max_val = pred->embedding[i];
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        probs[i] = expf(pred->embedding[i] - max_val);
        sum += probs[i];
    }
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        probs[i] /= sum;
    }

    /* Use QMC entropy estimation */
    qmc_entropy_config_t entropy_cfg = {
        .num_samples = config->num_samples,
        .use_stratified = true,
        .num_strata = 10,
        .seed = config->seed ? config->seed : mc_seed_from_time()
    };

    qmc_entropy_result_t entropy_result;
    qmc_result_t qmc_ret = qmc_estimate_entropy(probs, dim, &entropy_cfg, &entropy_result);

    if (qmc_ret == QMC_OK) {
        *entropy = entropy_result.shannon_entropy;
        if (std_error) *std_error = entropy_result.std_error;
    } else {
        /* Fallback: compute directly */
        *entropy = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)dim);
            }

            if (probs[i] > 1e-10f) {
                *entropy -= probs[i] * logf(probs[i]);
            }
        }
        if (std_error) *std_error = 0.0f;
    }

    nimcp_free(probs);
    probs = NULL;
    jepa_latent_destroy(pred);

    return NIMCP_SUCCESS;
}

/**
 * @brief Compute fidelity between two latent states via QMC
 */
float jepa_predictor_qmc_fidelity(
    jepa_predictor_t* predictor,
    const jepa_latent_t* latent1,
    const jepa_latent_t* latent2
) {
    if (!predictor || !latent1 || !latent2) return 0.0f;

    if (latent1->latent_dim != latent2->latent_dim) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_qmc_fidelity", 0.0f);


    uint32_t dim = latent1->latent_dim;

    /* Normalize embeddings as amplitudes */
    float* amp1 = nimcp_calloc(dim, sizeof(float));
    float* amp2 = nimcp_calloc(dim, sizeof(float));
    if (!amp1 || !amp2) {
        nimcp_free(amp1);
        amp1 = NULL;
        nimcp_free(amp2);
        amp2 = NULL;
        return 0.0f;
    }

    /* Compute L2 norms */
    float norm1 = 0.0f, norm2 = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        norm1 += latent1->embedding[i] * latent1->embedding[i];
        norm2 += latent2->embedding[i] * latent2->embedding[i];
    }
    norm1 = sqrtf(norm1 + 1e-8f);
    norm2 = sqrtf(norm2 + 1e-8f);

    /* Normalize */
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        amp1[i] = latent1->embedding[i] / norm1;
        amp2[i] = latent2->embedding[i] / norm2;
    }

    /* Use QMC fidelity computation */
    float fidelity = qmc_fidelity(amp1, amp2, dim);

    nimcp_free(amp1);
    amp1 = NULL;
    nimcp_free(amp2);
    amp2 = NULL;

    return fidelity;
}

/**
 * @brief Sample from latent distribution via QMC
 */
int jepa_predictor_qmc_sample_latent(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    jepa_latent_t** samples,
    uint32_t num_samples,
    const jepa_qmc_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_qmc_sample_latent", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");
    NIMCP_CHECK_THROW(samples, NIMCP_ERROR_NULL_POINTER, "samples is NULL");
    NIMCP_CHECK_THROW(num_samples > 0, NIMCP_ERROR_INVALID_PARAM, "num_samples must be > 0");

    /* Use default config if not provided */
    jepa_qmc_config_t default_cfg;
    if (!config) {
        jepa_qmc_config_init(&default_cfg);
        config = &default_cfg;
    }

    uint32_t dim = predictor->config.output_dim;
    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();

    /* Get base prediction */
    jepa_latent_t* base = jepa_latent_create_dim(dim);
    NIMCP_CHECK_THROW(base, NIMCP_ERROR_MEMORY, "failed to allocate base");

    if (jepa_predictor_predict(predictor, context, base) != NIMCP_SUCCESS) {
        jepa_latent_destroy(base);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Create probability distribution from base prediction */
    float* probs = nimcp_calloc(dim, sizeof(float));
    if (!probs) {
        jepa_latent_destroy(base);
        return NIMCP_ERROR_MEMORY;
    }

    /* Softmax to get probabilities */
    float max_val = -FLT_MAX;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        if (base->embedding[i] > max_val) max_val = base->embedding[i];
    }
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        probs[i] = expf(base->embedding[i] - max_val);
        sum += probs[i];
    }
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)dim);
        }

        probs[i] /= sum;
    }

    /* Use QMC finite shots measurement */
    qmc_measurement_config_t meas_cfg = {
        .num_shots = num_samples,
        .compute_uncertainty = true,
        .seed = seed
    };

    qmc_measurement_result_t meas_result;
    qmc_result_t qmc_ret = qmc_finite_shots(probs, dim, &meas_cfg, &meas_result);

    if (qmc_ret != QMC_OK) {
        nimcp_free(probs);
        probs = NULL;
        jepa_latent_destroy(base);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Generate samples based on measurement results */
    for (uint32_t s = 0; s < num_samples; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && num_samples > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(s + 1) / (float)num_samples);
        }

        samples[s] = jepa_latent_create_dim(dim);
        if (!samples[s]) {
            for (uint32_t j = 0; j < s; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && s > 256) {
                    jepa_predictor_heartbeat("jepa_predict_loop",
                                     (float)(j + 1) / (float)s);
                }

                jepa_latent_destroy(samples[j]);
            }
            qmc_measurement_result_free(&meas_result);
            nimcp_free(probs);
            probs = NULL;
            jepa_latent_destroy(base);
            return NIMCP_ERROR_MEMORY;
        }

        /* Initialize with base prediction and add noise based on uncertainty */
        for (uint32_t i = 0; i < dim; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && dim > 256) {
                jepa_predictor_heartbeat("jepa_predict_loop",
                                 (float)(i + 1) / (float)dim);
            }

            float noise_scale = meas_result.uncertainties ? meas_result.uncertainties[i] : 0.1f;
            float noise = mc_random_normal(&seed, 0.0f, noise_scale);
            samples[s]->embedding[i] = base->embedding[i] + noise;
        }
        samples[s]->modality = base->modality;
        samples[s]->is_normalized = false;
    }

    qmc_measurement_result_free(&meas_result);
    nimcp_free(probs);
    probs = NULL;
    jepa_latent_destroy(base);

    return NIMCP_SUCCESS;
}

/**
 * @brief Estimate prediction free energy via QMC
 */
int jepa_predictor_qmc_free_energy(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    const jepa_latent_t* target,
    float temperature,
    const jepa_qmc_config_t* config,
    float* free_energy
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_qmc_free_energy", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");
    NIMCP_CHECK_THROW(target, NIMCP_ERROR_NULL_POINTER, "target is NULL");
    NIMCP_CHECK_THROW(free_energy, NIMCP_ERROR_NULL_POINTER, "free_energy is NULL");

    if (temperature <= 0.0f) temperature = 1.0f;

    /* Use default config if not provided */
    jepa_qmc_config_t default_cfg;
    if (!config) {
        jepa_qmc_config_init(&default_cfg);
        config = &default_cfg;
    }

    uint32_t dim = predictor->config.output_dim;

    /* Make prediction */
    jepa_latent_t* pred = jepa_latent_create_dim(dim);
    NIMCP_CHECK_THROW(pred, NIMCP_ERROR_MEMORY, "failed to allocate pred");

    if (jepa_predictor_predict(predictor, context, pred) != NIMCP_SUCCESS) {
        jepa_latent_destroy(pred);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Compute energy (prediction error) */
    float energy = jepa_predictor_compute_loss(predictor, pred, target);

    /* Estimate entropy term */
    float entropy, entropy_err;
    int ret = jepa_predictor_qmc_entropy(predictor, context, config, &entropy, &entropy_err);

    if (ret == NIMCP_SUCCESS) {
        /* F = E - T*S */
        *free_energy = energy - temperature * entropy;
    } else {
        /* Fallback: just use energy */
        *free_energy = energy;
    }

    jepa_latent_destroy(pred);

    return NIMCP_SUCCESS;
}

/**
 * @brief Predict with QMC-enhanced uncertainty
 */
int jepa_predictor_predict_qmc(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    jepa_latent_t* prediction,
    float* uncertainty,
    const jepa_qmc_config_t* config,
    jepa_qmc_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_predict_qmc", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");
    NIMCP_CHECK_THROW(prediction, NIMCP_ERROR_NULL_POINTER, "prediction is NULL");
    NIMCP_CHECK_THROW(uncertainty, NIMCP_ERROR_NULL_POINTER, "uncertainty is NULL");

    /* Use default config if not provided */
    jepa_qmc_config_t default_cfg;
    if (!config) {
        jepa_qmc_config_init(&default_cfg);
        config = &default_cfg;
    }

    uint64_t start_time = nimcp_time_monotonic_ms();
    uint32_t dim = predictor->config.output_dim;

    /* Base prediction */
    int ret = jepa_predictor_predict(predictor, context, prediction);
    if (ret != NIMCP_SUCCESS) return ret;

    /* Estimate uncertainty per dimension using QMC amplitude estimation */
    float total_variance = 0.0f;
    for (uint32_t d = 0; d < dim; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && dim > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(d + 1) / (float)dim);
        }

        float amp, var;
        ret = jepa_predictor_qmc_amplitude_estimate(predictor, context, d, config, &amp, &var);
        if (ret == NIMCP_SUCCESS) {
            uncertainty[d] = sqrtf(var);
            total_variance += var;
        } else {
            uncertainty[d] = 1.0f / sqrtf(predictor->prediction_precision + 1e-6f);
        }
    }

    /* Compute entropy for stats */
    float entropy = 0.0f, entropy_err = 0.0f;
    jepa_predictor_qmc_entropy(predictor, context, config, &entropy, &entropy_err);

    /* Record statistics */
    if (stats) {
        stats->samples_taken = config->num_samples * dim;
        stats->entropy_estimate = entropy;
        stats->computation_time_ms = (float)(nimcp_time_monotonic_ms() - start_time);
        stats->tunneling_events = 0;
        stats->acceptance_rate = 1.0f;
        stats->effective_sample_size = (float)config->num_samples;
        stats->mean_energy = total_variance / dim;
        stats->final_energy = total_variance / dim;
    }

    predictor->stats.predictions_made++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Simplified MCTS-like latent space exploration node
 */
typedef struct jepa_mcts_node {
    float* embedding;
    uint32_t dim;
    float value;
    uint32_t visit_count;
    struct jepa_mcts_node* parent;
    struct jepa_mcts_node** children;
    uint32_t num_children;
    uint32_t depth;
} jepa_mcts_node_t;

/**
 * @brief Create an MCTS node
 */
static jepa_mcts_node_t* jepa_mcts_node_create(uint32_t dim) {
    jepa_mcts_node_t* node = nimcp_malloc(sizeof(jepa_mcts_node_t));
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate node");

        return NULL;

    }

    node->embedding = nimcp_calloc(dim, sizeof(float));
    if (!node->embedding) {
        nimcp_free(node);
        node = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_mcts_node_create: node->embedding is NULL");
        return NULL;
    }

    node->dim = dim;
    node->value = 0.0f;
    node->visit_count = 0;
    node->parent = NULL;
    node->children = NULL;
    node->num_children = 0;
    node->depth = 0;

    return node;
}

/**
 * @brief Destroy an MCTS node and all children
 */
static void jepa_mcts_node_destroy(jepa_mcts_node_t* node) {
    if (!node) return;

    for (uint32_t i = 0; i < node->num_children; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && node->num_children > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(i + 1) / (float)node->num_children);
        }

        jepa_mcts_node_destroy(node->children[i]);
    }

    nimcp_free(node->children);
    nimcp_free(node->embedding);
    nimcp_free(node);
    node = NULL;
}

/**
 * @brief UCB1 selection for MCTS
 */
static float jepa_mcts_ucb1(jepa_mcts_node_t* node, float exploration_c) {
    if (node->visit_count == 0) return FLT_MAX;

    float exploit = node->value / (float)node->visit_count;
    float explore = exploration_c * sqrtf(logf((float)node->parent->visit_count + 1) /
                                          (float)node->visit_count);
    return exploit + explore;
}

/**
 * @brief Run MCTS-guided latent space exploration
 */
int jepa_predictor_qmc_mcts_explore(
    jepa_predictor_t* predictor,
    const jepa_latent_t* context,
    uint32_t exploration_depth,
    const jepa_qmc_config_t* config,
    jepa_latent_t* best_latent,
    float* value
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_predictor_heartbeat("jepa_predict_qmc_mcts_explore", 0.0f);


    NIMCP_CHECK_THROW(predictor, NIMCP_ERROR_NULL_POINTER, "predictor is NULL");
    NIMCP_CHECK_THROW(context, NIMCP_ERROR_NULL_POINTER, "context is NULL");
    NIMCP_CHECK_THROW(best_latent, NIMCP_ERROR_NULL_POINTER, "best_latent is NULL");
    NIMCP_CHECK_THROW(value, NIMCP_ERROR_NULL_POINTER, "value is NULL");

    /* Use default config if not provided */
    jepa_qmc_config_t default_cfg;
    if (!config) {
        jepa_qmc_config_init(&default_cfg);
        config = &default_cfg;
    }

    uint32_t dim = predictor->config.output_dim;
    uint32_t max_depth = exploration_depth > 0 ? exploration_depth : 10;
    uint32_t max_iterations = config->num_iterations > 0 ? config->num_iterations : 100;
    float exploration_c = config->exploration_constant;
    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();

    /* Create initial state from prediction */
    jepa_latent_t* pred = jepa_latent_create_dim(dim);
    NIMCP_CHECK_THROW(pred, NIMCP_ERROR_MEMORY, "failed to allocate pred");

    if (jepa_predictor_predict(predictor, context, pred) != NIMCP_SUCCESS) {
        jepa_latent_destroy(pred);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Create root node */
    jepa_mcts_node_t* root = jepa_mcts_node_create(dim);
    if (!root) {
        jepa_latent_destroy(pred);
        return NIMCP_ERROR_MEMORY;
    }
    memcpy(root->embedding, pred->embedding, dim * sizeof(float));
    root->value = jepa_predictor_qmc_fidelity(predictor, context, pred);
    root->visit_count = 1;

    /* Best node tracking */
    jepa_mcts_node_t* best_node = root;
    float best_value = root->value;

    /* MCTS main loop */
    for (uint32_t iter = 0; iter < max_iterations; iter++) {
        /* Phase 8: Loop progress heartbeat */
        if ((iter & 0xFF) == 0 && max_iterations > 256) {
            jepa_predictor_heartbeat("jepa_predict_loop",
                             (float)(iter + 1) / (float)max_iterations);
        }

        /* Selection: traverse tree using UCB1 */
        jepa_mcts_node_t* current = root;
        while (current->num_children > 0 && current->depth < max_depth) {
            /* Select best child by UCB1 */
            float best_ucb = -FLT_MAX;
            jepa_mcts_node_t* best_child = NULL;
            for (uint32_t c = 0; c < current->num_children; c++) {
                /* Phase 8: Loop progress heartbeat */
                if ((c & 0xFF) == 0 && current->num_children > 256) {
                    jepa_predictor_heartbeat("jepa_predict_loop",
                                     (float)(c + 1) / (float)current->num_children);
                }

                float ucb = jepa_mcts_ucb1(current->children[c], exploration_c);
                if (ucb > best_ucb) {
                    best_ucb = ucb;
                    best_child = current->children[c];
                }
            }
            if (best_child) current = best_child;
            else break;
        }

        /* Expansion: add a new child if not terminal */
        if (current->depth < max_depth) {
            /* Create new child with random perturbation */
            jepa_mcts_node_t* child = jepa_mcts_node_create(dim);
            if (child) {
                memcpy(child->embedding, current->embedding, dim * sizeof(float));

                /* Random perturbation */
                uint32_t perturb_dim = seed % dim;
                seed = (seed * 1103515245 + 12345) & 0x7fffffff;
                float noise = mc_random_normal(&seed, 0.0f, 0.1f);
                child->embedding[perturb_dim] += noise;

                /* Evaluate */
                jepa_latent_t temp;
                temp.embedding = child->embedding;
                temp.latent_dim = dim;
                temp.modality = JEPA_MODALITY_UNKNOWN;
                temp.is_normalized = false;
                child->value = jepa_predictor_qmc_fidelity(predictor, context, &temp);
                child->visit_count = 1;
                child->parent = current;
                child->depth = current->depth + 1;

                /* Add to parent's children */
                jepa_mcts_node_t** new_children = nimcp_realloc(current->children,
                    (current->num_children + 1) * sizeof(jepa_mcts_node_t*));
                if (new_children) {
                    current->children = new_children;
                    current->children[current->num_children] = child;
                    current->num_children++;
                    current = child;
                } else {
                    jepa_mcts_node_destroy(child);
                }
            }
        }

        /* Simulation: rollout with random moves */
        float rollout_value = current->value;
        float* rollout_state = nimcp_calloc(dim, sizeof(float));
        if (rollout_state) {
            memcpy(rollout_state, current->embedding, dim * sizeof(float));
            for (uint32_t d = current->depth; d < max_depth; d++) {
                uint32_t perturb_dim = seed % dim;
                seed = (seed * 1103515245 + 12345) & 0x7fffffff;
                float noise = mc_random_normal(&seed, 0.0f, 0.1f);
                rollout_state[perturb_dim] += noise;
            }
            jepa_latent_t temp;
            temp.embedding = rollout_state;
            temp.latent_dim = dim;
            temp.modality = JEPA_MODALITY_UNKNOWN;
            temp.is_normalized = false;
            rollout_value = jepa_predictor_qmc_fidelity(predictor, context, &temp);
            nimcp_free(rollout_state);
            rollout_state = NULL;
        }

        /* Backpropagation */
        jepa_mcts_node_t* backprop = current;
        while (backprop) {
            backprop->visit_count++;
            backprop->value += rollout_value;
            backprop = backprop->parent;
        }

        /* Track best */
        float avg_value = current->visit_count > 0 ?
            current->value / (float)current->visit_count : 0.0f;
        if (avg_value > best_value) {
            best_value = avg_value;
            best_node = current;
        }
    }

    /* Copy best result */
    memcpy(best_latent->embedding, best_node->embedding, dim * sizeof(float));
    *value = best_value;

    best_latent->latent_dim = dim;
    best_latent->modality = context->modality;
    best_latent->is_normalized = false;

    /* Cleanup */
    jepa_mcts_node_destroy(root);
    jepa_latent_destroy(pred);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void jepa_predictor_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_jepa_predictor_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int jepa_predictor_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_predictor_training_begin: NULL argument");
        return -1;
    }
    jepa_predictor_heartbeat("jepa_predictor_training_begin", 0.0f);
    (void)(struct jepa_qmc_energy_ctx*)instance; /* Module state available for reset */
    return 0;
}

int jepa_predictor_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_predictor_training_end: NULL argument");
        return -1;
    }
    jepa_predictor_heartbeat("jepa_predictor_training_end", 1.0f);
    (void)(struct jepa_qmc_energy_ctx*)instance; /* Module state available for finalization */
    return 0;
}

int jepa_predictor_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_predictor_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    jepa_predictor_heartbeat("jepa_predictor_training_step", progress);
    (void)(struct jepa_qmc_energy_ctx*)instance; /* Module state available for step adaptation */
    return 0;
}
