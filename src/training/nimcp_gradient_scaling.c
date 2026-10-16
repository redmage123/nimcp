/**
 * @file nimcp_gradient_scaling.c
 * @brief Activation Function Gradient Scaling Implementation
 *
 * WHAT: Per-layer and per-activation gradient scaling for training stability
 * WHY:  Address vanishing/exploding gradients in deep networks
 * HOW:  Learnable scales, normalized gradients, surrogate gradients
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: Manual hooks, clip_grad_norm_
 * - JAX: stop_gradient, custom_vjp
 * - TensorFlow: GradientTape, clip_by_norm
 *
 * NIMCP ADVANTAGES:
 * - Unified with gradient_manager
 * - SNN surrogate gradient support
 * - Bio-inspired scaling mechanisms
 *
 * BIOLOGICAL GROUNDING:
 * - Neuromodulation: DA/ACh modulate plasticity
 * - Homeostatic scaling: Maintain healthy activity
 * - Gain modulation: Context-dependent scaling
 * - Metaplasticity: History-dependent thresholds
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_gradient_scaling.h"
#include "constants/nimcp_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gradient_scaling)

//=============================================================================
// Internal Constants
//=============================================================================

#define GS_EPSILON            NIMCP_EPSILON_ADAM
#define GS_VANISHING_THRESH   1e-7f
#define GS_EXPLODING_THRESH   1e7f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Per-layer state
 */
typedef struct {
    gs_layer_config_t config;        /**< Layer configuration */
    float current_scale;             /**< Current scale factor */
    float lr_multiplier;             /**< Learning rate multiplier */

    /* Running statistics for adaptive scaling */
    float running_norm_mean;         /**< Running mean of gradient norm */
    float running_norm_var;          /**< Running variance of gradient norm */
    uint64_t sample_count;           /**< Number of samples */

    /* Statistics */
    gs_layer_stats_t stats;

    bool registered;
} layer_state_t;

/**
 * @brief Gradient scaling context implementation
 */
struct gs_ctx_s {
    gs_config_t config;              /**< Configuration */
    bool initialized;                /**< Context initialized */

    /* Layer management */
    layer_state_t* layers;           /**< Layer states */
    uint32_t num_layers;             /**< Number of registered layers */
    uint32_t max_layers;             /**< Maximum layers */

    /* Integration points */
    nimcp_gradient_manager_ctx_t* grad_manager;
    void* snn_backprop;
    void* brain_factory;

    /* Global statistics */
    gs_stats_t stats;
    uint64_t step_count;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static float compute_norm(const float* data, size_t count);
static float compute_mean(const float* data, size_t count);
static float compute_std(const float* data, size_t count, float mean);
static void update_running_stats(layer_state_t* layer, float norm, float smoothing);

//=============================================================================
// Lifecycle API
//=============================================================================

int gs_default_config(gs_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "gs_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->method = GS_METHOD_NORMALIZED;

    /* Adaptive scaling defaults */
    config->adaptive.target_norm = 1.0f;
    config->adaptive.adaptation_rate = 0.1f;
    config->adaptive.smoothing_factor = NIMCP_EMA_DECAY_DEFAULT;
    config->adaptive.use_running_stats = true;
    config->adaptive.warmup_steps = 100;

    /* Surrogate gradient defaults (for SNN) */
    config->surrogate.default_surrogate = GS_SURROGATE_SIGMOID;
    config->surrogate.default_beta = 5.0f;
    config->surrogate.scale_by_threshold = true;
    config->surrogate.temporal_scaling = false;
    config->surrogate.temporal_decay = 0.1f;

    /* Global settings */
    config->global_clip = GS_CLIP_GLOBAL_NORM;
    config->global_clip_value = NIMCP_GRADIENT_CLIP_DEFAULT;
    config->normalize_per_layer = true;

    /* Integration enabled by default */
    config->integrate_gradient_manager = true;
    config->integrate_snn_backprop = true;

    config->verbose = false;
    config->track_statistics = true;

    return 0;
}

int gs_snn_config(gs_config_t* config) {
    if (gs_default_config(config) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gs_snn_config: validation failed");
        return -1;
    }

    /* SNN-specific settings */
    config->surrogate.default_surrogate = GS_SURROGATE_SUPERSPIKE;
    config->surrogate.default_beta = 10.0f;
    config->surrogate.scale_by_threshold = true;
    config->surrogate.temporal_scaling = true;
    config->surrogate.temporal_decay = 0.5f;

    /* Tighter clipping for SNN */
    config->global_clip_value = 0.5f;

    return 0;
}

gs_ctx_t* gs_create(const gs_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "gs_create: config is NULL");
        return NULL;
    }

    if (gs_validate_config(config) != 0) {
        NIMCP_THROW(NIMCP_ERROR_CONFIG_INVALID, "gs_create: config validation failed");
        return NULL;
    }

    gs_ctx_t* ctx = nimcp_calloc(1, sizeof(gs_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(gs_ctx_t),
                          "gs_create: failed to allocate context");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(gs_config_t));

    /* Allocate layer storage */
    ctx->max_layers = GS_MAX_LAYERS;
    ctx->layers = nimcp_calloc(ctx->max_layers, sizeof(layer_state_t));
    if (!ctx->layers) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, ctx->max_layers * sizeof(layer_state_t),
                          "gs_create: failed to allocate layers array");
        nimcp_free(ctx);
        return NULL;
    }

    /* Copy pre-configured layers */
    if (config->layer_configs && config->num_layers > 0) {
        for (uint32_t i = 0; i < config->num_layers && i < ctx->max_layers; i++) {
            memcpy(&ctx->layers[i].config, &config->layer_configs[i],
                   sizeof(gs_layer_config_t));
            ctx->layers[i].current_scale = config->layer_configs[i].scale;
            ctx->layers[i].lr_multiplier = 1.0f;
            ctx->layers[i].registered = true;
        }
        ctx->num_layers = config->num_layers;
    }

    /* Allocate layer stats array */
    ctx->stats.layer_stats = nimcp_calloc(ctx->max_layers, sizeof(gs_layer_stats_t));
    if (!ctx->stats.layer_stats) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, ctx->max_layers * sizeof(gs_layer_stats_t),
                          "gs_create: failed to allocate stats array");
        nimcp_free(ctx->layers);
        nimcp_free(ctx);
        return NULL;
    }

    /* Create mutex */
    ctx->mutex = nimcp_mutex_create(NULL);
    if (!ctx->mutex) {
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0,
                             "gs_create: failed to create mutex");
        nimcp_free(ctx->stats.layer_stats);
        nimcp_free(ctx->layers);
        nimcp_free(ctx);
        return NULL;
    }

    ctx->initialized = true;
    return ctx;
}

void gs_destroy(gs_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    nimcp_free(ctx->layers);
    nimcp_free(ctx->stats.layer_stats);

    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

int gs_register_layer(gs_ctx_t* ctx, const gs_layer_config_t* layer_config) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "gs_register_layer: ctx is NULL");
        return -1;
    }
    if (!layer_config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "gs_register_layer: layer_config is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->num_layers >= ctx->max_layers) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "gs_register_layer: max layers (%u) reached",
                   ctx->max_layers);
        nimcp_mutex_unlock(ctx->mutex);
        return -1;
    }

    uint32_t idx = ctx->num_layers;
    layer_state_t* layer = &ctx->layers[idx];

    memcpy(&layer->config, layer_config, sizeof(gs_layer_config_t));
    layer->current_scale = layer_config->scale > 0 ? layer_config->scale : GS_DEFAULT_SCALE;
    layer->lr_multiplier = 1.0f;
    layer->running_norm_mean = 0.0f;
    layer->running_norm_var = 0.0f;
    layer->sample_count = 0;
    layer->registered = true;

    ctx->num_layers++;

    nimcp_mutex_unlock(ctx->mutex);
    return (int)idx;
}

//=============================================================================
// Scaling API
//=============================================================================

float gs_scale_layer(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    float* gradients,
    size_t count
) {
    if (!ctx || !gradients || count == 0) {
        return 1.0f;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (layer_id >= ctx->num_layers || !ctx->layers[layer_id].registered) {
        nimcp_mutex_unlock(ctx->mutex);
        return 1.0f;
    }

    layer_state_t* layer = &ctx->layers[layer_id];
    float scale = 1.0f;

    /* Compute gradient statistics */
    float norm = compute_norm(gradients, count);
    float mean = compute_mean(gradients, count);
    float std = compute_std(gradients, count, mean);

    /* Update running stats */
    if (ctx->config.adaptive.use_running_stats) {
        update_running_stats(layer, norm, ctx->config.adaptive.smoothing_factor);
    }

    switch (ctx->config.method) {
        case GS_METHOD_NONE:
            scale = 1.0f;
            break;

        case GS_METHOD_FIXED:
            scale = layer->current_scale;
            break;

        case GS_METHOD_NORMALIZED: {
            /* Normalize to unit variance */
            if (std > GS_EPSILON) {
                scale = 1.0f / std;
            }
            break;
        }

        case GS_METHOD_ADAPTIVE: {
            /* Adaptive based on running statistics */
            scale = gs_compute_adaptive_scale(ctx, layer_id, gradients, count);
            break;
        }

        case GS_METHOD_LAYER_WISE_LR: {
            /* Apply layer-wise learning rate multiplier */
            scale = layer->lr_multiplier;
            break;
        }

        case GS_METHOD_LSUV: {
            /* Layer-Sequential Unit-Variance: scale to unit variance */
            float target_std = 1.0f;
            if (std > GS_EPSILON) {
                scale = target_std / std;
            }
            break;
        }

        case GS_METHOD_CENTRALIZED: {
            /* Centralized gradients: subtract mean */
            for (size_t i = 0; i < count; i++) {
                gradients[i] -= mean;
            }
            scale = 1.0f;
            break;
        }

        default:
            scale = 1.0f;
            break;
    }

    /* Clamp scale to valid range */
    if (scale < layer->config.min_scale && layer->config.min_scale > 0) {
        scale = layer->config.min_scale;
    }
    if (scale > layer->config.max_scale && layer->config.max_scale > 0) {
        scale = layer->config.max_scale;
    }
    if (scale < GS_MIN_SCALE) scale = GS_MIN_SCALE;
    if (scale > GS_MAX_SCALE) scale = GS_MAX_SCALE;

    /* Apply scaling */
    for (size_t i = 0; i < count; i++) {
        gradients[i] *= scale;
    }

    /* Apply layer-specific clipping */
    if (layer->config.clip_strategy != GS_CLIP_NONE) {
        switch (layer->config.clip_strategy) {
            case GS_CLIP_VALUE:
                gs_clip_by_value(gradients, count, layer->config.clip_value);
                break;
            case GS_CLIP_NORM:
                gs_clip_by_norm(gradients, count, layer->config.clip_value);
                break;
            default:
                break;
        }
    }

    /* Update statistics */
    layer->stats.grad_norm = norm;
    layer->stats.grad_mean = mean;
    layer->stats.grad_std = std;
    layer->stats.current_scale = scale;
    layer->stats.running_norm_avg = layer->running_norm_mean;
    layer->stats.running_norm_var = layer->running_norm_var;

    layer->current_scale = scale;

    nimcp_mutex_unlock(ctx->mutex);
    return scale;
}

float gs_scale_layer_tensor(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    nimcp_tensor_t* grad_tensor
) {
    if (!ctx || !grad_tensor) {
        return 1.0f;
    }

    size_t count = nimcp_tensor_numel(grad_tensor);
    return gs_scale_layer(ctx, layer_id, (float*)nimcp_tensor_data(grad_tensor), count);
}

int gs_scale_all_layers(
    gs_ctx_t* ctx,
    float** gradients,
    size_t* counts
) {
    if (!ctx || !gradients || !counts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gs_scale_all_layers: required parameter is NULL (ctx, gradients, counts)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* First, compute global norm if needed */
    float global_norm = 0.0f;
    if (ctx->config.global_clip == GS_CLIP_GLOBAL_NORM) {
        for (uint32_t l = 0; l < ctx->num_layers; l++) {
            if (!ctx->layers[l].registered) continue;
            float layer_norm = compute_norm(gradients[l], counts[l]);
            global_norm += layer_norm * layer_norm;
        }
        global_norm = sqrtf(global_norm);
    }

    nimcp_mutex_unlock(ctx->mutex);

    /* Scale each layer */
    for (uint32_t l = 0; l < ctx->num_layers; l++) {
        gs_scale_layer(ctx, l, gradients[l], counts[l]);
    }

    /* Apply global clipping if needed */
    if (ctx->config.global_clip == GS_CLIP_GLOBAL_NORM &&
        global_norm > ctx->config.global_clip_value) {
        gs_clip_global_norm(gradients, counts, ctx->num_layers,
                           ctx->config.global_clip_value);
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Update global stats */
    ctx->stats.global_grad_norm = global_norm;
    ctx->stats.total_steps++;

    /* Compute layer norm statistics */
    float sum_norms = 0.0f;
    float sum_sq_norms = 0.0f;
    uint32_t vanishing_count = 0;
    uint32_t exploding_count = 0;

    for (uint32_t l = 0; l < ctx->num_layers; l++) {
        if (!ctx->layers[l].registered) continue;
        float norm = ctx->layers[l].stats.grad_norm;
        sum_norms += norm;
        sum_sq_norms += norm * norm;

        if (norm < GS_VANISHING_THRESH) vanishing_count++;
        if (norm > GS_EXPLODING_THRESH) exploding_count++;
    }

    ctx->stats.avg_layer_norm = ctx->num_layers > 0 ?
        sum_norms / (float)ctx->num_layers : 0.0f;
    ctx->stats.norm_variance = ctx->num_layers > 0 ?
        sum_sq_norms / (float)ctx->num_layers -
        ctx->stats.avg_layer_norm * ctx->stats.avg_layer_norm : 0.0f;

    ctx->stats.vanishing_ratio = ctx->num_layers > 0 ?
        (float)vanishing_count / (float)ctx->num_layers : 0.0f;
    ctx->stats.exploding_ratio = ctx->num_layers > 0 ?
        (float)exploding_count / (float)ctx->num_layers : 0.0f;

    ctx->stats.gradient_healthy =
        (ctx->stats.vanishing_ratio < 0.5f && ctx->stats.exploding_ratio < 0.1f);

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

float gs_compute_adaptive_scale(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    const float* gradients,
    size_t count
) {
    if (!ctx || !gradients || count == 0 || layer_id >= ctx->num_layers) {
        return 1.0f;
    }

    layer_state_t* layer = &ctx->layers[layer_id];

    float current_norm = compute_norm(gradients, count);
    float target_norm = ctx->config.adaptive.target_norm;

    /* During warmup, use fixed scaling */
    if (layer->sample_count < ctx->config.adaptive.warmup_steps) {
        return 1.0f;
    }

    /* Compute adaptive scale based on running statistics */
    float running_mean = layer->running_norm_mean;

    if (running_mean < GS_EPSILON) {
        return 1.0f;
    }

    /* Scale to match target norm relative to running mean */
    float ratio = current_norm / running_mean;
    float scale = target_norm / (running_mean * ratio + GS_EPSILON);

    /* Smooth adaptation */
    float adapt_rate = ctx->config.adaptive.adaptation_rate;
    scale = (1.0f - adapt_rate) * 1.0f + adapt_rate * scale;

    return scale;
}

int gs_update_running_stats(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    float grad_norm
) {
    if (!ctx || layer_id >= ctx->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gs_update_running_stats: ctx is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    layer_state_t* layer = &ctx->layers[layer_id];
    update_running_stats(layer, grad_norm, ctx->config.adaptive.smoothing_factor);

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Surrogate Gradient API (for SNN)
//=============================================================================

int gs_surrogate_gradient(
    gs_ctx_t* ctx,
    const float* membrane_potential,
    float threshold,
    const float* grad_output,
    float* grad_input,
    size_t count,
    gs_surrogate_t surrogate,
    float beta
) {
    if (!membrane_potential || !grad_output || !grad_input || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gs_surrogate_gradient: required parameter is NULL (membrane_potential, grad_output, grad_input)");
        return -1;
    }

    /* Default beta if not specified */
    if (beta <= 0) {
        beta = ctx ? ctx->config.surrogate.default_beta : 5.0f;
    }

    for (size_t i = 0; i < count; i++) {
        float x = membrane_potential[i] - threshold;
        float surrogate_val = gs_surrogate_value(x, surrogate, beta);
        grad_input[i] = grad_output[i] * surrogate_val;
    }

    /* Scale by threshold if configured */
    if (ctx && ctx->config.surrogate.scale_by_threshold && threshold > 0) {
        float scale = 1.0f / threshold;
        for (size_t i = 0; i < count; i++) {
            grad_input[i] *= scale;
        }
    }

    return 0;
}

float gs_surrogate_value(float x, gs_surrogate_t surrogate, float beta) {
    switch (surrogate) {
        case GS_SURROGATE_NONE:
            return 1.0f;

        case GS_SURROGATE_SIGMOID: {
            /* sigma(beta * x) * (1 - sigma(beta * x)) * beta */
            float sig = 1.0f / (1.0f + expf(-beta * x));
            return beta * sig * (1.0f - sig);
        }

        case GS_SURROGATE_FAST_SIGMOID: {
            /* d/dx [x / (1 + |x|)] = 1 / (1 + |x|)^2 */
            float abs_x = fabsf(beta * x);
            float denom = 1.0f + abs_x;
            return beta / (denom * denom);
        }

        case GS_SURROGATE_ARCTAN: {
            /* d/dx [arctan(beta * x)] = beta / (1 + (beta*x)^2) */
            float bx = beta * x;
            return beta / (1.0f + bx * bx);
        }

        case GS_SURROGATE_TRIANGLE: {
            /* Triangular: 1 - |x|/width if |x| < width, else 0 */
            float width = 1.0f / beta;
            float abs_x = fabsf(x);
            if (abs_x < width) {
                return (1.0f - abs_x / width) / width;
            }
            return 0.0f;
        }

        case GS_SURROGATE_SUPERSPIKE: {
            /* SuperSpike: 1 / (1 + beta * |x|)^2 */
            float abs_x = fabsf(x);
            float denom = 1.0f + beta * abs_x;
            return 1.0f / (denom * denom);
        }

        case GS_SURROGATE_MULTI_GAUSSIAN: {
            /* Multi-Gaussian (SLAYER): sum of Gaussians */
            float sum = 0.0f;
            float sigma = 1.0f / beta;

            /* Central Gaussian */
            sum += expf(-x * x / (2.0f * sigma * sigma));

            /* Side Gaussians for wider support */
            float offset = 0.5f;
            sum += 0.5f * expf(-(x - offset) * (x - offset) / (2.0f * sigma * sigma));
            sum += 0.5f * expf(-(x + offset) * (x + offset) / (2.0f * sigma * sigma));

            return sum / (sigma * sqrtf(NIMCP_TWO_PI_F));
        }

        default:
            return 1.0f;
    }
}

//=============================================================================
// Clipping API
//=============================================================================

uint64_t gs_clip_by_value(
    float* gradients,
    size_t count,
    float max_value
) {
    if (!gradients || count == 0 || max_value <= 0) {
        return 0;
    }

    uint64_t clipped = 0;

    for (size_t i = 0; i < count; i++) {
        if (gradients[i] > max_value) {
            gradients[i] = max_value;
            clipped++;
        } else if (gradients[i] < -max_value) {
            gradients[i] = -max_value;
            clipped++;
        }
    }

    return clipped;
}

float gs_clip_by_norm(
    float* gradients,
    size_t count,
    float max_norm
) {
    if (!gradients || count == 0 || max_norm <= 0) {
        return 0.0f;
    }

    float norm = compute_norm(gradients, count);

    if (norm > max_norm) {
        float scale = max_norm / norm;
        for (size_t i = 0; i < count; i++) {
            gradients[i] *= scale;
        }
    }

    return norm;
}

float gs_clip_global_norm(
    float** gradients,
    size_t* counts,
    uint32_t num_layers,
    float max_norm
) {
    if (!gradients || !counts || num_layers == 0 || max_norm <= 0) {
        return 0.0f;
    }

    /* Compute global norm */
    float global_norm_sq = 0.0f;

    for (uint32_t l = 0; l < num_layers; l++) {
        if (!gradients[l]) continue;
        float layer_norm = compute_norm(gradients[l], counts[l]);
        global_norm_sq += layer_norm * layer_norm;
    }

    float global_norm = sqrtf(global_norm_sq);

    /* Scale if exceeds max */
    if (global_norm > max_norm) {
        float scale = max_norm / global_norm;

        for (uint32_t l = 0; l < num_layers; l++) {
            if (!gradients[l]) continue;
            for (size_t i = 0; i < counts[l]; i++) {
                gradients[l][i] *= scale;
            }
        }
    }

    return global_norm;
}

int gs_clip_adaptive(
    gs_ctx_t* ctx,
    float* gradients,
    const float* params,
    size_t count,
    float clip_factor
) {
    if (!ctx || !gradients || !params || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gs_clip_adaptive: required parameter is NULL (ctx, gradients, params)");
        return -1;
    }

    /* Adaptive Gradient Clipping (AGC) from NFNet paper */
    /* Clip when ||g||_2 / ||w||_2 > clip_factor */

    float grad_norm = compute_norm(gradients, count);
    float param_norm = compute_norm(params, count);

    if (param_norm < GS_EPSILON) {
        param_norm = GS_EPSILON;
    }

    float ratio = grad_norm / param_norm;

    if (ratio > clip_factor) {
        float scale = clip_factor * param_norm / grad_norm;
        for (size_t i = 0; i < count; i++) {
            gradients[i] *= scale;
        }
    }

    return 0;
}

//=============================================================================
// Layer-wise Learning Rate API
//=============================================================================

float gs_get_layer_lr(
    const gs_ctx_t* ctx,
    uint32_t layer_id,
    float base_lr
) {
    if (!ctx || layer_id >= ctx->num_layers) {
        return base_lr;
    }

    return base_lr * ctx->layers[layer_id].lr_multiplier;
}

int gs_set_layer_lr_multiplier(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    float multiplier
) {
    if (!ctx || layer_id >= ctx->num_layers || multiplier <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gs_set_layer_lr_multiplier: ctx is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->layers[layer_id].lr_multiplier = multiplier;
    ctx->layers[layer_id].stats.effective_lr = multiplier;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int gs_compute_depth_decay_lr(
    gs_ctx_t* ctx,
    float base_lr,
    float decay_rate
) {
    if (!ctx || base_lr <= 0 || decay_rate <= 0 || decay_rate >= 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gs_compute_depth_decay_lr: ctx is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Layer-wise LR: lr_l = base_lr * decay^(L - l) */
    /* Deeper layers get smaller LR */
    uint32_t total_layers = ctx->num_layers;

    for (uint32_t l = 0; l < ctx->num_layers; l++) {
        float depth_factor = powf(decay_rate, (float)(total_layers - 1 - l));
        ctx->layers[l].lr_multiplier = depth_factor;
        ctx->layers[l].stats.effective_lr = base_lr * depth_factor;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Integration API
//=============================================================================

int gs_connect_gradient_manager(
    gs_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->grad_manager = grad_manager;
    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int gs_connect_snn_backprop(gs_ctx_t* ctx, void* snn_backprop) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->snn_backprop = snn_backprop;
    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int gs_connect_brain_factory(gs_ctx_t* ctx, void* brain_factory) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->brain_factory = brain_factory;
    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

int gs_get_stats(const gs_ctx_t* ctx, gs_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gs_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);

    /* Copy global stats */
    stats->total_steps = ctx->stats.total_steps;
    stats->global_grad_norm = ctx->stats.global_grad_norm;
    stats->avg_layer_norm = ctx->stats.avg_layer_norm;
    stats->norm_variance = ctx->stats.norm_variance;
    stats->vanishing_ratio = ctx->stats.vanishing_ratio;
    stats->exploding_ratio = ctx->stats.exploding_ratio;
    stats->gradient_healthy = ctx->stats.gradient_healthy;

    /* Allocate and copy layer stats */
    stats->num_layers = ctx->num_layers;
    stats->layer_stats = nimcp_calloc(ctx->num_layers, sizeof(gs_layer_stats_t));
    if (stats->layer_stats) {
        for (uint32_t l = 0; l < ctx->num_layers; l++) {
            memcpy(&stats->layer_stats[l], &ctx->layers[l].stats,
                   sizeof(gs_layer_stats_t));
            stats->layer_stats[l].layer_id = l;
        }
    }

    /* Compute average scale and variance */
    float sum_scale = 0.0f;
    float sum_sq_scale = 0.0f;
    for (uint32_t l = 0; l < ctx->num_layers; l++) {
        float scale = ctx->layers[l].current_scale;
        sum_scale += scale;
        sum_sq_scale += scale * scale;
    }

    if (ctx->num_layers > 0) {
        stats->avg_scale = sum_scale / (float)ctx->num_layers;
        stats->scale_variance = sum_sq_scale / (float)ctx->num_layers -
                               stats->avg_scale * stats->avg_scale;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);
    return 0;
}

int gs_get_layer_stats(
    const gs_ctx_t* ctx,
    uint32_t layer_id,
    gs_layer_stats_t* stats
) {
    if (!ctx || !stats || layer_id >= ctx->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gs_get_layer_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);
    memcpy(stats, &ctx->layers[layer_id].stats, sizeof(gs_layer_stats_t));
    stats->layer_id = layer_id;
    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);

    return 0;
}

void gs_reset_stats(gs_ctx_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(ctx->mutex);

    memset(&ctx->stats, 0, sizeof(gs_stats_t));
    ctx->stats.layer_stats = nimcp_calloc(ctx->max_layers, sizeof(gs_layer_stats_t));

    for (uint32_t l = 0; l < ctx->num_layers; l++) {
        memset(&ctx->layers[l].stats, 0, sizeof(gs_layer_stats_t));
    }

    ctx->step_count = 0;

    nimcp_mutex_unlock(ctx->mutex);
}

bool gs_check_health(
    gs_ctx_t* ctx,
    float** gradients,
    size_t* counts,
    uint32_t num_layers
) {
    if (!ctx || !gradients || !counts || num_layers == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gs_check_health: required parameter is NULL (ctx, gradients, counts)");
        return false;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t vanishing = 0;
    uint32_t exploding = 0;
    uint32_t nan_count = 0;

    for (uint32_t l = 0; l < num_layers; l++) {
        if (!gradients[l]) continue;

        float norm = compute_norm(gradients[l], counts[l]);

        if (norm < GS_VANISHING_THRESH) vanishing++;
        if (norm > GS_EXPLODING_THRESH) exploding++;

        /* Check for NaN/Inf */
        for (size_t i = 0; i < counts[l]; i++) {
            if (!isfinite(gradients[l][i])) {
                nan_count++;
                break;
            }
        }
    }

    bool healthy = (vanishing < num_layers / 2) &&
                   (exploding == 0) &&
                   (nan_count == 0);

    ctx->stats.gradient_healthy = healthy;

    nimcp_mutex_unlock(ctx->mutex);
    return healthy;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* gs_method_name(gs_method_t method) {
    static const char* names[] = {
        "None",
        "Fixed",
        "Normalized",
        "Adaptive",
        "Layer-wise LR",
        "LSUV",
        "Spectral",
        "Centralized"
    };
    return method < GS_METHOD_COUNT ? names[method] : "Unknown";
}

const char* gs_surrogate_name(gs_surrogate_t surrogate) {
    static const char* names[] = {
        "None",
        "Sigmoid",
        "Fast Sigmoid",
        "Arctan",
        "Triangle",
        "SuperSpike",
        "Multi-Gaussian"
    };
    return surrogate < GS_SURROGATE_COUNT ? names[surrogate] : "Unknown";
}

const char* gs_activation_name(gs_activation_t activation) {
    static const char* names[] = {
        "Linear",
        "ReLU",
        "Leaky ReLU",
        "GELU",
        "Swish",
        "Tanh",
        "Sigmoid",
        "Softmax",
        "Spike",
        "LTC"
    };
    return activation < GS_ACTIVATION_COUNT ? names[activation] : "Unknown";
}

const char* gs_clip_strategy_name(gs_clip_strategy_t strategy) {
    static const char* names[] = {
        "None",
        "By Value",
        "By Norm",
        "Global Norm",
        "Adaptive"
    };
    return strategy < GS_CLIP_COUNT ? names[strategy] : "Unknown";
}

int gs_validate_config(const gs_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    if (config->method >= GS_METHOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gs_validate_config: capacity exceeded");
        return -1;
    }

    if (config->global_clip >= GS_CLIP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gs_validate_config: capacity exceeded");
        return -1;
    }

    if (config->adaptive.smoothing_factor <= 0 ||
        config->adaptive.smoothing_factor >= 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gs_validate_config: capacity exceeded");
        return -1;
    }

    if (config->surrogate.default_surrogate >= GS_SURROGATE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gs_validate_config: capacity exceeded");
        return -1;
    }

    return 0;
}

void gs_free_stats(gs_stats_t* stats) {
    if (!stats) return;

    nimcp_free(stats->layer_stats);
    stats->layer_stats = NULL;
    stats->num_layers = 0;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static float compute_norm(const float* data, size_t count) {
    if (!data || count == 0) {
        return 0.0f;
    }

    float sum_sq = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum_sq += data[i] * data[i];
    }

    return sqrtf(sum_sq);
}

static float compute_mean(const float* data, size_t count) {
    if (!data || count == 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += data[i];
    }

    return sum / (float)count;
}

static float compute_std(const float* data, size_t count, float mean) {
    if (!data || count == 0) {
        return 0.0f;
    }

    float sum_sq = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float diff = data[i] - mean;
        sum_sq += diff * diff;
    }

    return sqrtf(sum_sq / (float)count);
}

static void update_running_stats(layer_state_t* layer, float norm, float smoothing) {
    if (!layer) return;

    if (layer->sample_count == 0) {
        /* First sample */
        layer->running_norm_mean = norm;
        layer->running_norm_var = 0.0f;
    } else {
        /* Exponential moving average */
        float delta = norm - layer->running_norm_mean;
        layer->running_norm_mean = smoothing * layer->running_norm_mean +
                                   (1.0f - smoothing) * norm;

        /* Welford's online variance */
        float new_delta = norm - layer->running_norm_mean;
        layer->running_norm_var = smoothing * layer->running_norm_var +
                                  (1.0f - smoothing) * delta * new_delta;
    }

    layer->sample_count++;
}
