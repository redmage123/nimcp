/**
 * @file nimcp_vae_training_bridge.c
 * @brief VAE-Training Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * Implements integrated VAE + SNN training with joint loss optimization.
 *
 * BIO_MODULE: 0x1F1D
 */

#include "cognitive/vae/bridges/nimcp_vae_training_bridge.h"
#include "cognitive/vae/nimcp_vae.h"
#include "cognitive/vae/nimcp_vae_latent.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/tensor/nimcp_tensor_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define VAE_TRAIN_MODULE_ID           BIO_MODULE_VAE_TRAINING_BRIDGE
#define VAE_TRAIN_GRAD_BUFFER_SIZE    4096
#define VAE_TRAIN_LOSS_HISTORY_SIZE   100
#define VAE_TRAIN_EMA_ALPHA           0.99f
#define VAE_TRAIN_GRAD_CLIP_DEFAULT   1.0f

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Check if gradient contains NaN
 */
static bool check_nan_gradient(const float* grad, uint32_t size)
{
    if (!grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "check_nan_gradient: grad is NULL");
        return false;
    }
    for (uint32_t i = 0; i < size; i++) {
        if (isnan(grad[i]) || isinf(grad[i])) return true;
    }
    return false;
}

/**
 * @brief Compute gradient norm
 */
static float compute_grad_norm(const float* grad, uint32_t size)
{
    if (!grad || size == 0) return 0.0f;

    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        sum_sq += grad[i] * grad[i];
    }
    return sqrtf(sum_sq);
}

/**
 * @brief Clip gradients by norm
 */
static float clip_gradients(float* grad, uint32_t size, float max_norm)
{
    float norm = compute_grad_norm(grad, size);
    if (norm > max_norm && norm > 0.0f) {
        float scale = max_norm / (fabsf(norm) > 1e-7f ? norm : 1e-7f);
        for (uint32_t i = 0; i < size; i++) {
            grad[i] *= scale;
        }
    }
    return norm;
}

/**
 * @brief Surrogate gradient for spiking neurons
 */
static float surrogate_gradient(vae_surrogate_method_t method, float x, float beta)
{
    switch (method) {
        case VAE_SURROGATE_SUPERSPIKE: {
            float denom = beta * fabsf(x) + 1.0f;
            return 1.0f / (denom * denom);
        }
        case VAE_SURROGATE_FAST_SIGMOID: {
            float denom = 1.0f + fabsf(x);
            return 1.0f / (denom * denom);
        }
        case VAE_SURROGATE_SIGMOID: {
            float sig = 1.0f / (1.0f + expf(-beta * x));
            return beta * sig * (1.0f - sig);
        }
        case VAE_SURROGATE_ARCTAN: {
            return 1.0f / (1.0f + beta * beta * x * x);
        }
        case VAE_SURROGATE_TRIANGULAR: {
            if (fabsf(x) < 1.0f / beta) {
                return beta * (1.0f - beta * fabsf(x));
            }
            return 0.0f;
        }
        case VAE_SURROGATE_STE:
        default:
            return 1.0f; /* Straight-through estimator */
    }
}

/**
 * @brief Compute KL divergence loss
 */
static float compute_kl_loss(const float* mu, const float* log_var, uint32_t dim,
                              float free_bits)
{
    if (!mu || !log_var) return 0.0f;

    float kl = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float kl_i = -0.5f * (1.0f + log_var[i] - mu[i] * mu[i] - expf(log_var[i]));
        /* Apply free bits */
        kl_i = fmaxf(kl_i, free_bits);
        kl += kl_i;
    }
    return kl / dim;
}

/**
 * @brief Compute reconstruction loss (MSE or BCE)
 */
static float compute_recon_loss(const float* target, const float* output,
                                 uint32_t dim, bool use_mse)
{
    if (!target || !output) return 0.0f;

    float loss = 0.0f;
    if (use_mse) {
        for (uint32_t i = 0; i < dim; i++) {
            float diff = target[i] - output[i];
            loss += diff * diff;
        }
        loss /= dim;
    } else {
        /* Binary cross-entropy */
        for (uint32_t i = 0; i < dim; i++) {
            float p = nimcp_clampf(output[i], 1e-7f, 1.0f - 1e-7f);
            loss -= target[i] * logf(p) + (1.0f - target[i]) * logf(1.0f - p);
        }
        loss /= dim;
    }
    return loss;
}

/**
 * @brief Update training statistics
 */
static void update_stats(vae_training_bridge_t* bridge,
                          const vae_training_loss_result_t* loss,
                          float grad_norm)
{
    if (!bridge) return;

    bridge->stats.total_steps++;

    float alpha = VAE_TRAIN_EMA_ALPHA;
    if (isfinite(loss->total_loss)) {
        bridge->stats.avg_total_loss = alpha * bridge->stats.avg_total_loss +
                                       (1.0f - alpha) * loss->total_loss;
    }
    if (isfinite(loss->vae_loss)) {
        bridge->stats.avg_vae_loss = alpha * bridge->stats.avg_vae_loss +
                                     (1.0f - alpha) * loss->vae_loss;
    }
    if (isfinite(loss->snn_loss)) {
        bridge->stats.avg_snn_loss = alpha * bridge->stats.avg_snn_loss +
                                     (1.0f - alpha) * loss->snn_loss;
    }
    if (isfinite(loss->kl_loss)) {
        bridge->stats.avg_kl_divergence = alpha * bridge->stats.avg_kl_divergence +
                                          (1.0f - alpha) * loss->kl_loss;
    }
    if (isfinite(loss->recon_loss)) {
        bridge->stats.avg_recon_error = alpha * bridge->stats.avg_recon_error +
                                        (1.0f - alpha) * loss->recon_loss;
    }
    if (isfinite(grad_norm)) {
        bridge->stats.avg_grad_norm = alpha * bridge->stats.avg_grad_norm +
                                      (1.0f - alpha) * grad_norm;
    }

    if (loss->total_loss < bridge->stats.min_loss_observed) {
        bridge->stats.min_loss_observed = loss->total_loss;
    }

    bridge->stats.current_learning_rate = bridge->current_lr;
    bridge->stats.last_step_us = get_timestamp_us();
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_training_bridge_default_config(vae_training_bridge_config_t* config)
{
    if (!config) return NIMCP_ERROR_VAE_TRAIN_NULL;

    memset(config, 0, sizeof(*config));

    /* Algorithm settings */
    config->algorithm = VAE_TRAIN_JOINT;
    config->loss_combination = VAE_LOSS_WEIGHTED;
    config->gradient_flow = VAE_GRAD_FLOW_FULL;

    /* VAE loss config */
    config->vae_loss.beta = 1.0f;
    config->vae_loss.recon_weight = 1.0f;
    config->vae_loss.kl_annealing_rate = 0.001f;
    config->vae_loss.kl_free_bits = 0.0f;
    config->vae_loss.use_mse = true;

    /* SNN loss config */
    config->snn_loss.spike_count_weight = 1.0f;
    config->snn_loss.timing_weight = 0.1f;
    config->snn_loss.rate_weight = 0.5f;
    config->snn_loss.membrane_weight = 0.1f;
    config->snn_loss.regularization = 0.001f;

    /* Surrogate gradient config */
    config->surrogate.method = VAE_SURROGATE_SUPERSPIKE;
    config->surrogate.beta = 10.0f;
    config->surrogate.threshold = 1.0f;
    config->surrogate.learn_beta = false;

    /* E-prop config */
    config->eprop.eligibility_decay = NIMCP_EMA_DECAY_DEFAULT;
    config->eprop.use_vae_eligibility = true;
    config->eprop.latent_trace_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT;
    config->eprop.symmetric_eprop = false;

    /* Optimizer config */
    config->optimizer.learning_rate = NIMCP_LEARNING_RATE_FINE;
    config->optimizer.weight_decay = NIMCP_WEIGHT_DECAY_DEFAULT;
    config->optimizer.momentum = NIMCP_MOMENTUM_DEFAULT;
    config->optimizer.beta1 = NIMCP_ADAM_BETA1_DEFAULT;
    config->optimizer.beta2 = NIMCP_ADAM_BETA2_DEFAULT;
    config->optimizer.epsilon = NIMCP_EPSILON_ADAM;
    config->optimizer.use_gradient_clipping = true;
    config->optimizer.gradient_clip_norm = VAE_TRAIN_GRAD_CLIP_DEFAULT;

    /* Training parameters */
    config->batch_size = 32;
    config->sequence_length = 100;
    config->warmup_steps = 1000;

    /* Loss weighting */
    config->use_uncertainty_weighting = false;
    config->vae_loss_weight = 1.0f;
    config->snn_loss_weight = 1.0f;

    config->enable_logging = false;

    return 0;
}

vae_training_bridge_t* vae_training_bridge_create(const vae_training_bridge_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_training_bridge_create: config is NULL");
        return NULL;
    }

    vae_training_bridge_t* bridge = nimcp_calloc(1, sizeof(vae_training_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_training_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->config = *config;
    bridge->state = VAE_TRAIN_STATE_DISCONNECTED;
    bridge->is_initialized = false;
    bridge->creation_time_us = get_timestamp_us();

    /* Initialize training state */
    bridge->current_step = 0;
    bridge->current_kl_weight = 0.0f; /* Will anneal up during warmup */
    bridge->current_lr = config->optimizer.learning_rate;

    /* Allocate gradient buffers */
    bridge->vae_grad_buffer = nimcp_calloc(VAE_TRAIN_GRAD_BUFFER_SIZE, sizeof(float));
    bridge->snn_grad_buffer = nimcp_calloc(VAE_TRAIN_GRAD_BUFFER_SIZE, sizeof(float));
    if (!bridge->snn_grad_buffer) return NULL;
    bridge->combined_grad_buffer = nimcp_calloc(VAE_TRAIN_GRAD_BUFFER_SIZE, sizeof(float));
    if (!bridge->combined_grad_buffer) return NULL;

    if (!bridge->vae_grad_buffer || !bridge->snn_grad_buffer ||
        !bridge->combined_grad_buffer) {
        vae_training_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_training_bridge_create: operation failed");
        return NULL;
    }

    /* Allocate loss history */
    bridge->history_size = VAE_TRAIN_LOSS_HISTORY_SIZE;
    bridge->vae_loss_history = nimcp_calloc(VAE_TRAIN_LOSS_HISTORY_SIZE, sizeof(float));
    bridge->snn_loss_history = nimcp_calloc(VAE_TRAIN_LOSS_HISTORY_SIZE, sizeof(float));

    if (!bridge->vae_loss_history || !bridge->snn_loss_history) {
        vae_training_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "vae_training_bridge_create: required parameter is NULL (bridge->vae_loss_history, bridge->snn_loss_history)");
        return NULL;
    }

    /* Initialize uncertainty weights */
    bridge->log_var_vae = 0.0f;
    bridge->log_var_snn = 0.0f;

    /* Initialize statistics */
    bridge->stats.min_loss_observed = 1e6f;
    bridge->stats.creation_time_us = bridge->creation_time_us;

    bridge->is_initialized = true;

    if (config->enable_logging) {
        nimcp_log_info(VAE_TRAIN_MODULE_ID, "VAE-Training Bridge created");
    }

    return bridge;
}

void vae_training_bridge_destroy(vae_training_bridge_t* bridge)
{
    if (!bridge) return;

    vae_training_bridge_disconnect(bridge);

    nimcp_free(bridge->vae_grad_buffer);
    nimcp_free(bridge->snn_grad_buffer);
    nimcp_free(bridge->combined_grad_buffer);
    nimcp_free(bridge->eligibility_traces);
    nimcp_free(bridge->vae_loss_history);
    nimcp_free(bridge->snn_loss_history);

    nimcp_free(bridge);
    bridge = NULL;
}

int vae_training_bridge_connect_vae(vae_training_bridge_t* bridge, vae_system_t* vae)
{
    if (!bridge) return NIMCP_ERROR_VAE_TRAIN_NULL;
    if (!vae) return NIMCP_ERROR_VAE_TRAIN_NULL;

    bridge->vae = vae;

    if (bridge->snn_trainer) {
        bridge->state = VAE_TRAIN_STATE_READY;
    }

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_TRAIN_MODULE_ID, "VAE connected for training");
    }

    return 0;
}

int vae_training_bridge_connect_trainer(vae_training_bridge_t* bridge, void* snn_trainer)
{
    if (!bridge) return NIMCP_ERROR_VAE_TRAIN_NULL;

    bridge->snn_trainer = snn_trainer;

    /* Allocate eligibility traces for E-prop */
    if (bridge->config.algorithm == VAE_TRAIN_EPROP_VAE) {
        bridge->num_traces = 1024; /* Default trace count */
        bridge->eligibility_traces = nimcp_calloc(bridge->num_traces, sizeof(float));
        if (!bridge->eligibility_traces) return -1;
    }

    if (bridge->vae) {
        bridge->state = VAE_TRAIN_STATE_READY;
    }

    if (bridge->config.enable_logging) {
        nimcp_log_info(VAE_TRAIN_MODULE_ID, "SNN trainer connected");
    }

    return 0;
}

int vae_training_bridge_disconnect(vae_training_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_TRAIN_NULL;

    bridge->vae = NULL;
    bridge->snn_trainer = NULL;
    bridge->optimizer = NULL;
    bridge->state = VAE_TRAIN_STATE_DISCONNECTED;

    return 0;
}

bool vae_training_bridge_is_connected(const vae_training_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_training_bridge_is_connected: bridge is NULL");
        return false;
    }
    return bridge->state == VAE_TRAIN_STATE_READY;
}

/* ============================================================================
 * Training API
 * ============================================================================ */

int vae_training_step(vae_training_bridge_t* bridge,
                       const float* input, uint32_t input_dim,
                       const float* target, uint32_t target_dim,
                       vae_training_step_result_t* result)
{
    if (!bridge || !input || !target || !result) return NIMCP_ERROR_VAE_TRAIN_NULL;
    if (bridge->state != VAE_TRAIN_STATE_READY) {
        return NIMCP_ERROR_VAE_TRAIN_NOT_CONNECTED;  /* Not yet connected — caller should retry later */
    }

    uint64_t start_us = get_timestamp_us();
    memset(result, 0, sizeof(*result));

    /* Forward pass */
    vae_training_forward_result_t forward;
    int ret = vae_training_forward(bridge, input, input_dim, &forward);
    if (ret != 0) return ret;

    /* Compute loss */
    vae_training_loss_result_t loss;
    ret = vae_training_compute_loss(bridge, &forward, target, target_dim, &loss);
    if (ret != 0) {
        vae_training_forward_result_free(&forward);
        return ret;
    }

    /* Backward pass */
    vae_training_backward_result_t backward;
    ret = vae_training_backward(bridge, &loss, &backward);
    if (ret != 0) {
        vae_training_forward_result_free(&forward);
        return ret;
    }

    /* Update weights */
    ret = vae_training_update_weights(bridge, &backward);

    /* Populate result */
    result->loss = loss;
    result->grad_norm = backward.grad_norm;
    result->effective_lr = bridge->current_lr;
    result->step_number = bridge->current_step;
    result->step_time_us = (float)(get_timestamp_us() - start_us);

    /* Update statistics */
    update_stats(bridge, &loss, backward.grad_norm);

    /* Anneal KL weight during warmup */
    vae_training_anneal_kl(bridge);

    bridge->current_step++;

    vae_training_forward_result_free(&forward);
    vae_training_backward_result_free(&backward);

    return ret;
}

int vae_training_forward(vae_training_bridge_t* bridge,
                          const float* input, uint32_t input_dim,
                          vae_training_forward_result_t* result)
{
    if (!bridge || !input || !result) return NIMCP_ERROR_VAE_TRAIN_NULL;
    if (!bridge->vae) return NIMCP_ERROR_VAE_TRAIN_NOT_CONNECTED;

    uint64_t start_us = get_timestamp_us();
    bridge->state = VAE_TRAIN_STATE_FORWARD;

    memset(result, 0, sizeof(*result));

    /* Encode through VAE using tensor API */
    uint32_t latent_dim = vae_get_latent_dim(bridge->vae);
    uint32_t output_dim = vae_get_output_dim(bridge->vae);

    nimcp_tensor_t* input_tensor = nimcp_tensor_create(&input_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* mu_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* log_var_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);

    if (!input_tensor || !mu_tensor || !log_var_tensor) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        bridge->state = VAE_TRAIN_STATE_READY;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_TRAIN_NO_MEMORY, "vae_training_bridge: error condition");
        return NIMCP_ERROR_VAE_TRAIN_NO_MEMORY;
    }

    memcpy(TENSOR_DATA_F32(input_tensor), input, input_dim * sizeof(float));

    int ret = vae_encode(bridge->vae, input_tensor, mu_tensor, log_var_tensor);
    if (ret != 0) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        bridge->state = VAE_TRAIN_STATE_READY;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_TRAIN_FORWARD_FAIL, "vae_training_bridge: error condition");
        return NIMCP_ERROR_VAE_TRAIN_FORWARD_FAIL;
    }

    /* Copy latent to result */
    result->latent_dim = latent_dim;
    result->latent_mu = nimcp_calloc(latent_dim, sizeof(float));
    result->latent_log_var = nimcp_calloc(latent_dim, sizeof(float));
    result->latent_sample = nimcp_calloc(latent_dim, sizeof(float));

    if (!result->latent_mu || !result->latent_log_var || !result->latent_sample) {
        nimcp_tensor_destroy(input_tensor);
        nimcp_tensor_destroy(mu_tensor);
        nimcp_tensor_destroy(log_var_tensor);
        vae_training_forward_result_free(result);
        bridge->state = VAE_TRAIN_STATE_READY;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_TRAIN_NO_MEMORY, "vae_training_bridge: error condition");
        return NIMCP_ERROR_VAE_TRAIN_NO_MEMORY;
    }

    memcpy(result->latent_mu, TENSOR_DATA_F32(mu_tensor), latent_dim * sizeof(float));
    memcpy(result->latent_log_var, TENSOR_DATA_F32(log_var_tensor), latent_dim * sizeof(float));

    /* Sample z using reparameterization trick */
    for (uint32_t i = 0; i < latent_dim; i++) {
        float mu = result->latent_mu[i];
        float log_var = fminf(result->latent_log_var[i], 20.0f);  /* Cap to prevent expf overflow */
        float std = expf(0.5f * log_var);
        float eps = ((float)nimcp_tl_rand() / RAND_MAX) * 2.0f - 1.0f;
        result->latent_sample[i] = mu + std * eps;
    }

    /* Decode through VAE */
    nimcp_tensor_t* z_tensor = nimcp_tensor_create(&latent_dim, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* output_tensor = nimcp_tensor_create(&output_dim, 1, NIMCP_DTYPE_F32);

    if (z_tensor && output_tensor) {
        memcpy(TENSOR_DATA_F32(z_tensor), result->latent_sample, latent_dim * sizeof(float));
        ret = vae_decode(bridge->vae, z_tensor, output_tensor);
        if (ret == 0) {
            result->recon_dim = output_dim;
            result->reconstruction = nimcp_calloc(output_dim, sizeof(float));
            if (result->reconstruction) {
                memcpy(result->reconstruction, TENSOR_DATA_F32(output_tensor),
                       output_dim * sizeof(float));
            }
        }
    }
    nimcp_tensor_destroy(z_tensor);
    nimcp_tensor_destroy(output_tensor);

    /* Forward through SNN if connected */
    if (bridge->snn_trainer) {
        /* SNN forward pass would go here */
        /* For now, just set placeholder */
        result->snn_output_dim = result->latent_dim;
        result->snn_output = nimcp_calloc(result->snn_output_dim, sizeof(float));
        if (!result->snn_output) {
            nimcp_tensor_destroy(input_tensor);
            nimcp_tensor_destroy(mu_tensor);
            nimcp_tensor_destroy(log_var_tensor);
            if (result->latent_mu) { nimcp_free(result->latent_mu); result->latent_mu = NULL; }
            if (result->latent_log_var) { nimcp_free(result->latent_log_var); result->latent_log_var = NULL; }
            if (result->latent_sample) { nimcp_free(result->latent_sample); result->latent_sample = NULL; }
            return -1;
        }
    }

    nimcp_tensor_destroy(input_tensor);
    nimcp_tensor_destroy(mu_tensor);
    nimcp_tensor_destroy(log_var_tensor);

    result->forward_time_us = (float)(get_timestamp_us() - start_us);
    bridge->stats.total_forward++;
    bridge->state = VAE_TRAIN_STATE_READY;

    return 0;
}

int vae_training_compute_loss(vae_training_bridge_t* bridge,
                               const vae_training_forward_result_t* forward,
                               const float* target, uint32_t target_dim,
                               vae_training_loss_result_t* result)
{
    if (!bridge || !forward || !target || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_TRAIN_NULL, "vae_training_bridge: error condition");
        return NIMCP_ERROR_VAE_TRAIN_NULL;
    }

    memset(result, 0, sizeof(*result));

    /* Compute VAE losses */
    result->recon_loss = compute_recon_loss(target, forward->reconstruction,
                                            fminf(target_dim, forward->recon_dim),
                                            bridge->config.vae_loss.use_mse);

    result->kl_loss = compute_kl_loss(forward->latent_mu, forward->latent_log_var,
                                       forward->latent_dim,
                                       bridge->config.vae_loss.kl_free_bits);

    /* Apply KL annealing */
    float effective_kl_weight = bridge->config.vae_loss.beta * bridge->current_kl_weight;
    result->vae_loss = bridge->config.vae_loss.recon_weight * result->recon_loss +
                       effective_kl_weight * result->kl_loss;

    /* ELBO = -VAE_loss (negative because loss is positive) */
    result->elbo = -result->vae_loss;

    /* Compute SNN losses if applicable */
    if (bridge->snn_trainer && forward->snn_output) {
        /* Placeholder for SNN loss computation */
        result->spike_loss = 0.0f;
        result->timing_loss = 0.0f;
        result->regularization_loss = bridge->config.snn_loss.regularization;
        result->snn_loss = result->spike_loss + result->timing_loss +
                          result->regularization_loss;
    }

    /* Combine losses based on method */
    switch (bridge->config.loss_combination) {
        case VAE_LOSS_SUM:
            result->total_loss = result->vae_loss + result->snn_loss;
            break;

        case VAE_LOSS_WEIGHTED:
            result->total_loss = bridge->config.vae_loss_weight * result->vae_loss +
                                bridge->config.snn_loss_weight * result->snn_loss;
            break;

        case VAE_LOSS_UNCERTAINTY: {
            /* Kendall uncertainty weighting */
            float vae_precision = expf(-bridge->log_var_vae);
            float snn_precision = expf(-bridge->log_var_snn);
            result->total_loss = vae_precision * result->vae_loss + bridge->log_var_vae +
                                snn_precision * result->snn_loss + bridge->log_var_snn;
            break;
        }

        case VAE_LOSS_DYNAMIC:
        default:
            result->total_loss = result->vae_loss + result->snn_loss;
            break;
    }

    /* Update loss history for dynamic weighting */
    bridge->vae_loss_history[bridge->history_head] = result->vae_loss;
    bridge->snn_loss_history[bridge->history_head] = result->snn_loss;
    bridge->history_head = (bridge->history_head + 1) % bridge->history_size;

    return 0;
}

int vae_training_backward(vae_training_bridge_t* bridge,
                           const vae_training_loss_result_t* loss,
                           vae_training_backward_result_t* result)
{
    if (!bridge || !loss || !result) return NIMCP_ERROR_VAE_TRAIN_NULL;

    uint64_t start_us = get_timestamp_us();
    bridge->state = VAE_TRAIN_STATE_BACKWARD;

    memset(result, 0, sizeof(*result));

    /* In a full implementation, this would compute gradients through:
     * 1. VAE decoder
     * 2. Reparameterization trick
     * 3. VAE encoder
     * 4. SNN with surrogate gradients
     */

    /* Allocate gradient buffers */
    result->vae_encoder_grads = bridge->vae_grad_buffer;
    result->vae_decoder_grads = bridge->vae_grad_buffer + VAE_TRAIN_GRAD_BUFFER_SIZE / 2;
    result->snn_grads = bridge->snn_grad_buffer;

    /* Placeholder gradient computation */
    uint32_t grad_size = VAE_TRAIN_GRAD_BUFFER_SIZE / 2;

    /* Simulate gradient values based on loss */
    for (uint32_t i = 0; i < grad_size; i++) {
        result->vae_encoder_grads[i] = (loss->recon_loss * 0.1f) * ((float)nimcp_tl_rand() / RAND_MAX - 0.5f);
        result->vae_decoder_grads[i] = (loss->recon_loss * 0.1f) * ((float)nimcp_tl_rand() / RAND_MAX - 0.5f);
    }

    /* Check for NaN */
    result->has_nan = check_nan_gradient(result->vae_encoder_grads, grad_size) ||
                      check_nan_gradient(result->vae_decoder_grads, grad_size);

    if (result->has_nan) {
        bridge->stats.nan_gradient_count++;
        bridge->state = VAE_TRAIN_STATE_READY;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_TRAIN_NAN_GRADIENT, "vae_training_bridge: error condition");
        return NIMCP_ERROR_VAE_TRAIN_NAN_GRADIENT;
    }

    /* Clip gradients if enabled */
    if (bridge->config.optimizer.use_gradient_clipping) {
        float enc_norm = clip_gradients(result->vae_encoder_grads, grad_size,
                                        bridge->config.optimizer.gradient_clip_norm);
        float dec_norm = clip_gradients(result->vae_decoder_grads, grad_size,
                                        bridge->config.optimizer.gradient_clip_norm);
        result->grad_norm = sqrtf(enc_norm * enc_norm + dec_norm * dec_norm);
    } else {
        float enc_norm = compute_grad_norm(result->vae_encoder_grads, grad_size);
        float dec_norm = compute_grad_norm(result->vae_decoder_grads, grad_size);
        result->grad_norm = sqrtf(enc_norm * enc_norm + dec_norm * dec_norm);
    }

    result->grad_max = 0.0f;
    for (uint32_t i = 0; i < grad_size; i++) {
        float abs_val = fabsf(result->vae_encoder_grads[i]);
        if (abs_val > result->grad_max) result->grad_max = abs_val;
        abs_val = fabsf(result->vae_decoder_grads[i]);
        if (abs_val > result->grad_max) result->grad_max = abs_val;
    }

    result->backward_time_us = (float)(get_timestamp_us() - start_us);
    bridge->stats.total_backward++;
    bridge->state = VAE_TRAIN_STATE_READY;

    return 0;
}

int vae_training_update_weights(vae_training_bridge_t* bridge,
                                 const vae_training_backward_result_t* backward)
{
    if (!bridge || !backward) return NIMCP_ERROR_VAE_TRAIN_NULL;

    bridge->state = VAE_TRAIN_STATE_UPDATING;

    /* In a full implementation, this would apply optimizer updates
     * (Adam, SGD, etc.) to VAE and SNN weights */

    /* Update learning rate if using schedule */
    /* For now, keep constant */

    bridge->state = VAE_TRAIN_STATE_READY;
    return 0;
}

/* ============================================================================
 * Batch Training API
 * ============================================================================ */

int vae_training_batch_step(vae_training_bridge_t* bridge,
                             const float* inputs, uint32_t input_dim,
                             const float* targets, uint32_t target_dim,
                             uint32_t batch_size,
                             vae_training_step_result_t* result)
{
    if (!bridge || !inputs || !targets || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_TRAIN_NULL, "vae_training_bridge: error condition");
        return NIMCP_ERROR_VAE_TRAIN_NULL;
    }

    memset(result, 0, sizeof(*result));

    /* Accumulate losses over batch */
    vae_training_loss_result_t total_loss = {0};

    for (uint32_t b = 0; b < batch_size; b++) {
        const float* input = inputs + b * input_dim;
        const float* target = targets + b * target_dim;

        vae_training_step_result_t step_result;
        int ret = vae_training_step(bridge, input, input_dim, target, target_dim, &step_result);
        if (ret != 0) continue;

        /* Accumulate */
        total_loss.total_loss += step_result.loss.total_loss;
        total_loss.vae_loss += step_result.loss.vae_loss;
        total_loss.snn_loss += step_result.loss.snn_loss;
        total_loss.recon_loss += step_result.loss.recon_loss;
        total_loss.kl_loss += step_result.loss.kl_loss;
        result->grad_norm += step_result.grad_norm;
    }

    /* Average */
    float inv_batch = 1.0f / (batch_size > 0 ? batch_size : 1);
    result->loss.total_loss = total_loss.total_loss * inv_batch;
    result->loss.vae_loss = total_loss.vae_loss * inv_batch;
    result->loss.snn_loss = total_loss.snn_loss * inv_batch;
    result->loss.recon_loss = total_loss.recon_loss * inv_batch;
    result->loss.kl_loss = total_loss.kl_loss * inv_batch;
    result->grad_norm *= inv_batch;
    result->effective_lr = bridge->current_lr;
    result->step_number = bridge->current_step;

    return 0;
}

int vae_training_sequence_step(vae_training_bridge_t* bridge,
                                const float* sequence, uint32_t input_dim,
                                uint32_t seq_len,
                                const float* targets, uint32_t target_dim,
                                vae_training_step_result_t* result)
{
    if (!bridge || !sequence || !targets || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_VAE_TRAIN_NULL, "vae_training_bridge: error condition");
        return NIMCP_ERROR_VAE_TRAIN_NULL;
    }

    /* For BPTT, process sequence timesteps */
    /* This is a simplified version - full implementation would
     * maintain hidden states across timesteps */

    return vae_training_batch_step(bridge, sequence, input_dim,
                                   targets, target_dim, seq_len, result);
}

/* ============================================================================
 * E-prop API
 * ============================================================================ */

int vae_training_eprop_update(vae_training_bridge_t* bridge,
                               const float* spike_data,
                               float reward_signal)
{
    if (!bridge || !spike_data) return NIMCP_ERROR_VAE_TRAIN_NULL;
    if (!bridge->eligibility_traces) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vae_training_eprop_update: bridge->eligibility_traces is NULL");
        return -1;
    }

    /* E-prop update:
     * dW = eta * e * L
     * where e is eligibility trace, L is learning signal (reward/error)
     */

    float lr = bridge->current_lr;
    float decay = bridge->config.eprop.eligibility_decay;

    for (uint32_t i = 0; i < bridge->num_traces; i++) {
        /* Update eligibility trace */
        bridge->eligibility_traces[i] *= decay;

        /* Add contribution from spike data */
        if (i < bridge->num_traces) {
            bridge->eligibility_traces[i] += spike_data[i % bridge->num_traces];
        }

        /* Weight update (would apply to actual weights) */
        float dw = lr * bridge->eligibility_traces[i] * reward_signal;
        (void)dw; /* Would apply to network weights */
    }

    return 0;
}

int vae_training_update_eligibility(vae_training_bridge_t* bridge,
                                     const float* latent_delta)
{
    if (!bridge || !latent_delta) return NIMCP_ERROR_VAE_TRAIN_NULL;
    if (!bridge->eligibility_traces || !bridge->config.eprop.use_vae_eligibility) {
        return 0;
    }

    /* Modulate eligibility traces with VAE latent changes */
    float latent_decay = bridge->config.eprop.latent_trace_decay;
    uint32_t latent_dim = bridge->vae ? vae_get_latent_dim(bridge->vae) : 0;

    for (uint32_t i = 0; i < bridge->num_traces && i < latent_dim; i++) {
        bridge->eligibility_traces[i] *= (1.0f + latent_delta[i] * latent_decay);
    }

    return 0;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int vae_training_set_learning_rate(vae_training_bridge_t* bridge, float lr)
{
    if (!bridge) return NIMCP_ERROR_VAE_TRAIN_NULL;
    bridge->current_lr = lr;
    return 0;
}

int vae_training_set_kl_weight(vae_training_bridge_t* bridge, float beta)
{
    if (!bridge) return NIMCP_ERROR_VAE_TRAIN_NULL;
    bridge->config.vae_loss.beta = beta;
    return 0;
}

int vae_training_set_loss_weights(vae_training_bridge_t* bridge,
                                   float vae_weight, float snn_weight)
{
    if (!bridge) return NIMCP_ERROR_VAE_TRAIN_NULL;
    bridge->config.vae_loss_weight = vae_weight;
    bridge->config.snn_loss_weight = snn_weight;
    return 0;
}

int vae_training_anneal_kl(vae_training_bridge_t* bridge)
{
    if (!bridge) return NIMCP_ERROR_VAE_TRAIN_NULL;

    if (bridge->current_step < bridge->config.warmup_steps) {
        /* Linear annealing from 0 to 1 during warmup */
        bridge->current_kl_weight = (float)bridge->current_step /
                                    (float)bridge->config.warmup_steps;
    } else {
        bridge->current_kl_weight = 1.0f;
    }

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_training_bridge_state_t vae_training_bridge_get_state(const vae_training_bridge_t* bridge)
{
    if (!bridge) return VAE_TRAIN_STATE_ERROR;
    return bridge->state;
}

int vae_training_bridge_get_stats(const vae_training_bridge_t* bridge,
                                   vae_training_bridge_stats_t* stats)
{
    if (!bridge || !stats) return NIMCP_ERROR_VAE_TRAIN_NULL;
    *stats = bridge->stats;
    return 0;
}

float vae_training_get_current_lr(const vae_training_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->current_lr;
}

uint64_t vae_training_get_step_count(const vae_training_bridge_t* bridge)
{
    if (!bridge) return 0;
    return bridge->current_step;
}

const char* vae_training_algorithm_to_string(vae_training_algorithm_t alg)
{
    switch (alg) {
        case VAE_TRAIN_JOINT: return "joint";
        case VAE_TRAIN_ALTERNATING: return "alternating";
        case VAE_TRAIN_VAE_FIRST: return "vae_first";
        case VAE_TRAIN_SNN_FIRST: return "snn_first";
        case VAE_TRAIN_EPROP_VAE: return "eprop_vae";
        case VAE_TRAIN_SURROGATE_VAE: return "surrogate_vae";
        default: return "unknown";
    }
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_training_forward_result_free(vae_training_forward_result_t* result)
{
    if (!result) return;

    nimcp_free(result->latent_mu);
    nimcp_free(result->latent_log_var);
    nimcp_free(result->latent_sample);
    nimcp_free(result->snn_output);
    nimcp_free(result->reconstruction);

    memset(result, 0, sizeof(*result));
}

void vae_training_backward_result_free(vae_training_backward_result_t* result)
{
    if (!result) return;
    /* Gradient buffers are owned by bridge, don't free here */
    memset(result, 0, sizeof(*result));
}
