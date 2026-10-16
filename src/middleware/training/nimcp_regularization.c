/**
 * @file nimcp_regularization.c
 * @brief Regularization Module Implementation
 *
 * Implements all regularization techniques:
 * - L1/L2/Elastic Net weight regularization
 * - Gradient clipping (by value, by norm)
 * - Dropout (standard, alpha, spatial)
 * - Label smoothing
 * - Early stopping
 *
 * @note Part of Phase TM-5: Regularization
 * @version 1.0.0
 */

#include "middleware/training/nimcp_regularization.h"
#include "constants/nimcp_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "utils/geometry/nimcp_differential_geometry.h"

#define REG_MODULE_NAME "Regularization"
#define LOG_MODULE "regularization"
#define LOG_MODULE_ID 0x0580
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(regularization)

/* ============================================================================
 * Internal Context Structures
 * ============================================================================ */

struct nimcp_regularization_ctx {
    nimcp_regularization_config_t config;
    nimcp_regularization_stats_t stats;
    bool initialized;

    /* Security registration */
    nimcp_sec_integration_t* security_ctx;
    bool security_registered;
    uint32_t security_module_id;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

struct nimcp_dropout_ctx {
    nimcp_dropout_config_t config;
    uint64_t rng_state;  /* Simple xorshift64 state */
    uint64_t total_drops;
    uint64_t total_elements;
    bool initialized;
};

struct nimcp_early_stop_ctx {
    nimcp_early_stop_config_t config;
    float best_metric;
    uint64_t best_epoch;
    uint64_t current_epoch;
    uint32_t wait_count;
    bool improved;
    bool should_stop;
    bool initialized;
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/* Simple xorshift64 RNG */
static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    if (x == 0) x = 0x5DEECE66DULL;  /* Zero is a fixed point; use fallback seed */
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/* Generate float in [0, 1) */
static float rand_float(uint64_t* state) {
    return (float)(xorshift64(state) >> 11) / (float)(1ULL << 53);
}

static float sign(float x) {
    if (x > 0.0F) return 1.0F;
    if (x < 0.0F) return -1.0F;
    return 0.0F;
}

/* ============================================================================
 * Default Configuration Functions
 * ============================================================================ */

nimcp_l1_config_t nimcp_l1_default_config(float lambda) {
    nimcp_l1_config_t config = {
        .lambda = lambda > 0.0F ? lambda : 0.01F
    };
    return config;
}

nimcp_l2_config_t nimcp_l2_default_config(float lambda) {
    nimcp_l2_config_t config = {
        .lambda = lambda > 0.0F ? lambda : 0.01F
    };
    return config;
}

nimcp_elastic_net_config_t nimcp_elastic_net_default_config(float lambda, float alpha) {
    nimcp_elastic_net_config_t config = {
        .lambda = lambda > 0.0F ? lambda : 0.01F,
        .alpha = (alpha >= 0.0F && alpha <= 1.0F) ? alpha : 0.5F
    };
    return config;
}

nimcp_dropout_config_t nimcp_dropout_default_config(float rate) {
    nimcp_dropout_config_t config = {
        .rate = (rate >= NIMCP_DROPOUT_MIN && rate < NIMCP_DROPOUT_MAX) ? rate : 0.5F,
        .mode = NIMCP_DROPOUT_STANDARD,
        .training = true,
        .seed = 0
    };
    return config;
}

nimcp_clip_config_t nimcp_clip_default_config(nimcp_clip_mode_t mode, float threshold) {
    nimcp_clip_config_t config = {
        .mode = mode,
        .threshold = threshold > 0.0F ? threshold : NIMCP_GRADIENT_CLIP_DEFAULT
    };
    return config;
}

nimcp_label_smooth_config_t nimcp_label_smooth_default_config(float smoothing, uint32_t num_classes) {
    nimcp_label_smooth_config_t config = {
        .smoothing = (smoothing >= 0.0F && smoothing <= 1.0F) ? smoothing : 0.1F,
        .num_classes = num_classes > 0 ? num_classes : 10
    };
    return config;
}

nimcp_early_stop_config_t nimcp_early_stop_default_config(uint32_t patience) {
    nimcp_early_stop_config_t config = {
        .patience = (patience > 0 && patience <= NIMCP_REG_MAX_PATIENCE) ? patience : 10,
        .min_delta = 1e-4F,
        .mode = NIMCP_EARLY_STOP_MIN,
        .restore_best = true
    };
    return config;
}

nimcp_regularization_config_t nimcp_regularization_default_config(void) {
    nimcp_regularization_config_t config;
    memset(&config, 0, sizeof(config));

    config.weight_reg_type = NIMCP_REG_NONE;
    config.weight_reg.l2.lambda = 0.01F;

    config.gradient_clip.mode = NIMCP_CLIP_NONE;
    config.gradient_clip.threshold = NIMCP_GRADIENT_CLIP_DEFAULT;

    config.dropout.rate = 0.0F;
    config.dropout.mode = NIMCP_DROPOUT_STANDARD;
    config.dropout.training = true;

    config.label_smooth.smoothing = 0.0F;
    config.label_smooth.num_classes = 10;
    config.use_label_smoothing = false;

    config.early_stop.patience = 10;
    config.early_stop.min_delta = 1e-4F;
    config.early_stop.mode = NIMCP_EARLY_STOP_MIN;
    config.early_stop.restore_best = true;
    config.use_early_stopping = false;

    config.verbose = false;

    return config;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

nimcp_regularization_ctx_t* nimcp_regularization_create(
    const nimcp_regularization_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_regularization_create: config is NULL");
        LOG_MODULE_ERROR(LOG_MODULE, "Null configuration provided");
        return NULL;
    }

    if (nimcp_regularization_validate_config(config) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_regularization_create: invalid configuration");
        LOG_MODULE_ERROR(LOG_MODULE, "Invalid configuration");
        return NULL;
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Creating regularization context");

    nimcp_regularization_ctx_t* ctx = (nimcp_regularization_ctx_t*)nimcp_calloc(
        1, sizeof(nimcp_regularization_ctx_t)
    );
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_regularization_create: failed to allocate context");
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate context");
        return NULL;
    }

    memcpy(&ctx->config, config, sizeof(nimcp_regularization_config_t));
    memset(&ctx->stats, 0, sizeof(nimcp_regularization_stats_t));
    ctx->initialized = true;

    /* Register with security module if available */
    ctx->security_ctx = config->security_ctx;
    ctx->security_registered = false;
    if (ctx->security_ctx) {
        nimcp_result_t err = nimcp_sec_register_module(
            ctx->security_ctx,
            REG_MODULE_NAME,
            NIMCP_SEC_CAT_MIDDLEWARE,
            &ctx->security_module_id
        );
        if (err == NIMCP_SUCCESS) {
            ctx->security_registered = true;
            LOG_MODULE_INFO(LOG_MODULE, "Registered with security module (ID: %u)", ctx->security_module_id);
        } else {
            LOG_MODULE_WARN(LOG_MODULE, "Failed to register with security module");
        }
    }

    /* Bio-async registration */
    ctx->bio_ctx = NULL;
    ctx->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_TRAINING_REGULARIZATION,
            .module_name = "regularization",
            .inbox_capacity = 32,
            .user_data = ctx
        };
        ctx->bio_ctx = bio_router_register_module(&bio_info);
        if (ctx->bio_ctx) {
            ctx->bio_async_enabled = true;
            LOG_MODULE_INFO(LOG_MODULE, "Bio-async integration enabled");
        } else {
            LOG_MODULE_WARN(LOG_MODULE, "Bio-async registration failed");
        }
    }

    LOG_MODULE_INFO(LOG_MODULE, "Regularization context created (type=%s)",
                   nimcp_reg_type_name(config->weight_reg_type));

    return ctx;
}

void nimcp_regularization_destroy(nimcp_regularization_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Destroying regularization context");

    /* Unregister from bio-async */
    if (ctx->bio_async_enabled && ctx->bio_ctx) {
        bio_router_unregister_module(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
        ctx->bio_async_enabled = false;
        LOG_MODULE_DEBUG(LOG_MODULE, "Bio-async unregistered");
    }

    /* Unregister from security module */
    if (ctx->security_registered && ctx->security_ctx) {
        nimcp_sec_unregister_module(ctx->security_ctx, ctx->security_module_id);
        ctx->security_registered = false;
        LOG_MODULE_DEBUG(LOG_MODULE, "Security module unregistered");
    }

    nimcp_free(ctx);
    LOG_MODULE_INFO(LOG_MODULE, "Regularization context destroyed");
}

/* ============================================================================
 * Weight Regularization Operations
 * ============================================================================ */

float nimcp_regularization_loss(
    nimcp_regularization_ctx_t* ctx,
    const float* weights,
    size_t num_weights
) {
    if (!ctx || !weights || num_weights == 0) {
        return 0.0F;
    }

    float loss = 0.0F;

    switch (ctx->config.weight_reg_type) {
        case NIMCP_REG_NONE:
            break;

        case NIMCP_REG_L1:
            loss = nimcp_l1_loss(weights, num_weights, ctx->config.weight_reg.l1.lambda);
            break;

        case NIMCP_REG_L2:
            loss = nimcp_l2_loss(weights, num_weights, ctx->config.weight_reg.l2.lambda);
            break;

        case NIMCP_REG_ELASTIC_NET:
            loss = nimcp_elastic_net_loss(
                weights, num_weights,
                ctx->config.weight_reg.elastic_net.lambda,
                ctx->config.weight_reg.elastic_net.alpha
            );
            break;

        default:
            break;
    }

    /* Update statistics */
    ctx->stats.weight_reg_count++;
    ctx->stats.total_reg_loss += loss;
    ctx->stats.last_reg_loss = loss;

    return loss;
}

float nimcp_l1_loss(const float* weights, size_t num_weights, float lambda) {
    if (!weights || num_weights == 0 || lambda <= 0.0F) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (size_t i = 0; i < num_weights; i++) {
        sum += fabsf(weights[i]);
    }

    return lambda * sum;
}

float nimcp_l2_loss(const float* weights, size_t num_weights, float lambda) {
    if (!weights || num_weights == 0 || lambda <= 0.0F) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (size_t i = 0; i < num_weights; i++) {
        sum += weights[i] * weights[i];
    }

    return 0.5F * lambda * sum;
}

float nimcp_elastic_net_loss(
    const float* weights,
    size_t num_weights,
    float lambda,
    float alpha
) {
    if (!weights || num_weights == 0 || lambda <= 0.0F) {
        return 0.0F;
    }

    float l1_loss = nimcp_l1_loss(weights, num_weights, 1.0F);
    float l2_loss = nimcp_l2_loss(weights, num_weights, 1.0F);

    return lambda * (alpha * l1_loss + (1.0F - alpha) * l2_loss);
}

nimcp_result_t nimcp_regularization_apply_gradient(
    nimcp_regularization_ctx_t* ctx,
    const float* weights,
    float* gradients,
    size_t num_weights
) {
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(weights, NIMCP_ERROR_INVALID_PARAM, "weights is NULL");
    NIMCP_CHECK_THROW(gradients, NIMCP_ERROR_INVALID_PARAM, "gradients is NULL");
    NIMCP_CHECK_THROW(num_weights > 0, NIMCP_ERROR_INVALID_PARAM, "num_weights is 0");

    // Process pending bio-async messages
    if (ctx->bio_async_enabled && ctx->bio_ctx) {
        bio_router_process_inbox(ctx->bio_ctx, 5);
    }

    switch (ctx->config.weight_reg_type) {
        case NIMCP_REG_NONE:
            break;

        case NIMCP_REG_L1:
            nimcp_l1_gradient(weights, gradients, num_weights,
                             ctx->config.weight_reg.l1.lambda);
            break;

        case NIMCP_REG_L2:
            nimcp_l2_gradient(weights, gradients, num_weights,
                             ctx->config.weight_reg.l2.lambda);
            break;

        case NIMCP_REG_ELASTIC_NET: {
            float lambda = ctx->config.weight_reg.elastic_net.lambda;
            float alpha = ctx->config.weight_reg.elastic_net.alpha;

            /* L1 contribution */
            for (size_t i = 0; i < num_weights; i++) {
                gradients[i] += lambda * alpha * sign(weights[i]);
            }

            /* L2 contribution */
            for (size_t i = 0; i < num_weights; i++) {
                gradients[i] += lambda * (1.0F - alpha) * weights[i];
            }
            break;
        }

        default:
            break;
    }

    return NIMCP_SUCCESS;
}

void nimcp_l1_gradient(
    const float* weights,
    float* gradients,
    size_t num_weights,
    float lambda
) {
    if (!weights || !gradients || num_weights == 0 || lambda <= 0.0F) {
        return;
    }

    for (size_t i = 0; i < num_weights; i++) {
        gradients[i] += lambda * sign(weights[i]);
    }
}

void nimcp_l2_gradient(
    const float* weights,
    float* gradients,
    size_t num_weights,
    float lambda
) {
    if (!weights || !gradients || num_weights == 0 || lambda <= 0.0F) {
        return;
    }

    for (size_t i = 0; i < num_weights; i++) {
        gradients[i] += lambda * weights[i];
    }
}

/* ============================================================================
 * Gradient Clipping Operations
 * ============================================================================ */

float nimcp_gradient_clip(
    nimcp_regularization_ctx_t* ctx,
    float* gradients,
    size_t num_gradients
) {
    if (!ctx || !gradients || num_gradients == 0) {
        return 1.0F;
    }

    float clip_ratio = 1.0F;

    switch (ctx->config.gradient_clip.mode) {
        case NIMCP_CLIP_NONE:
            break;

        case NIMCP_CLIP_BY_VALUE: {
            uint64_t clipped = nimcp_gradient_clip_by_value(
                gradients, num_gradients, ctx->config.gradient_clip.threshold
            );
            if (clipped > 0) {
                ctx->stats.clip_count++;
                clip_ratio = 1.0F - (float)clipped / (float)num_gradients;
            }
            break;
        }

        case NIMCP_CLIP_BY_NORM:
        case NIMCP_CLIP_BY_GLOBAL_NORM: {
            clip_ratio = nimcp_gradient_clip_by_norm(
                gradients, num_gradients, ctx->config.gradient_clip.threshold
            );
            if (clip_ratio < 1.0F) {
                ctx->stats.clip_count++;
            }
            break;
        }

        default:
            break;
    }

    /* Update statistics */
    float norm = nimcp_gradient_norm(gradients, num_gradients);
    if (norm > ctx->stats.max_grad_norm) {
        ctx->stats.max_grad_norm = norm;
    }
    ctx->stats.total_clip_ratio += clip_ratio;

    return clip_ratio;
}

uint64_t nimcp_gradient_clip_by_value(
    float* gradients,
    size_t num_gradients,
    float threshold
) {
    if (!gradients || num_gradients == 0 || threshold <= 0.0F) {
        return 0;
    }

    uint64_t clipped = 0;
    for (size_t i = 0; i < num_gradients; i++) {
        if (gradients[i] > threshold) {
            gradients[i] = threshold;
            clipped++;
        } else if (gradients[i] < -threshold) {
            gradients[i] = -threshold;
            clipped++;
        }
    }

    return clipped;
}

float nimcp_gradient_clip_by_norm(
    float* gradients,
    size_t num_gradients,
    float max_norm
) {
    if (!gradients || num_gradients == 0 || max_norm <= 0.0F) {
        return 1.0F;
    }

    /* For small parameter spaces, use Riemannian norm with empirical Fisher metric.
     * This clips based on the geometry of the loss landscape rather than flat
     * Euclidean distance, preventing over-clipping in flat directions and
     * under-clipping in high-curvature directions. */
    if (num_gradients <= DIFFGEO_MAX_DIM) {
        riemannian_metric_t* fisher = riemannian_metric_create((uint32_t)num_gradients);
        if (fisher) {
            /* Diagonal Fisher approximation: G_ii = g_i^2 + epsilon */
            for (uint32_t i = 0; i < (uint32_t)num_gradients; i++) {
                fisher->g[i * num_gradients + i] = gradients[i] * gradients[i] + DIFFGEO_EPSILON;
            }
            float riem_norm = riemannian_norm(fisher, gradients);
            riemannian_metric_destroy(fisher);

            if (isfinite(riem_norm) && riem_norm > max_norm) {
                float scale = max_norm / riem_norm;
                for (size_t i = 0; i < num_gradients; i++) {
                    gradients[i] *= scale;
                }
                return scale;
            } else if (isfinite(riem_norm)) {
                return 1.0F;
            }
            /* Fall through to Euclidean clipping if Riemannian norm failed */
        }
    }

    float norm = nimcp_gradient_norm(gradients, num_gradients);
    if (norm <= max_norm) {
        return 1.0F;
    }

    float scale = max_norm / norm;
    for (size_t i = 0; i < num_gradients; i++) {
        gradients[i] *= scale;
    }

    return scale;
}

float nimcp_gradient_norm(const float* gradients, size_t num_gradients) {
    if (!gradients || num_gradients == 0) {
        return 0.0F;
    }

    float sum_sq = 0.0F;
    for (size_t i = 0; i < num_gradients; i++) {
        sum_sq += gradients[i] * gradients[i];
    }

    return sqrtf(sum_sq);
}

/* ============================================================================
 * Dropout Operations
 * ============================================================================ */

nimcp_dropout_ctx_t* nimcp_dropout_create(const nimcp_dropout_config_t* config) {
    if (!config) {
        LOG_MODULE_ERROR(LOG_MODULE, "Null dropout config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_dropout_create: config is NULL");
        return NULL;
    }

    if (config->rate < NIMCP_DROPOUT_MIN || config->rate >= NIMCP_DROPOUT_MAX) {
        LOG_MODULE_ERROR(LOG_MODULE, "Invalid dropout rate: %f", config->rate);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_dropout_create: capacity exceeded");
        return NULL;
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Creating dropout context (rate=%f)", config->rate);

    nimcp_dropout_ctx_t* ctx = (nimcp_dropout_ctx_t*)nimcp_calloc(1, sizeof(nimcp_dropout_ctx_t));
    if (!ctx) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate dropout context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_dropout_create: ctx is NULL");
        return NULL;
    }

    memcpy(&ctx->config, config, sizeof(nimcp_dropout_config_t));

    /* Initialize RNG state */
    if (config->seed != 0) {
        ctx->rng_state = config->seed;
    } else {
        ctx->rng_state = (uint64_t)time(NULL) ^ 0x5DEECE66DULL;
    }

    ctx->total_drops = 0;
    ctx->total_elements = 0;
    ctx->initialized = true;

    LOG_MODULE_INFO(LOG_MODULE, "Dropout context created (rate=%f, mode=%d)", config->rate, config->mode);

    return ctx;
}

void nimcp_dropout_destroy(nimcp_dropout_ctx_t* ctx) {
    if (ctx) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Destroying dropout context");
        nimcp_free(ctx);
    }
}

nimcp_result_t nimcp_dropout_forward(
    nimcp_dropout_ctx_t* ctx,
    float* activations,
    size_t num_activations,
    uint8_t* mask
) {
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(activations, NIMCP_ERROR_INVALID_PARAM, "activations is NULL");
    NIMCP_CHECK_THROW(num_activations > 0, NIMCP_ERROR_INVALID_PARAM, "num_activations is 0");

    /* In inference mode, no dropout */
    if (!ctx->config.training) {
        if (mask) {
            memset(mask, 1, num_activations);
        }
        return NIMCP_SUCCESS;
    }

    float rate = ctx->config.rate;
    float scale = 1.0F / (1.0F - rate);  /* Inverted dropout scaling */

    uint64_t drops = 0;

    for (size_t i = 0; i < num_activations; i++) {
        float r = rand_float(&ctx->rng_state);
        if (r < rate) {
            activations[i] = 0.0F;
            if (mask) mask[i] = 0;
            drops++;
        } else {
            activations[i] *= scale;
            if (mask) mask[i] = 1;
        }
    }

    ctx->total_drops += drops;
    ctx->total_elements += num_activations;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_dropout_backward(
    nimcp_dropout_ctx_t* ctx,
    float* gradients,
    size_t num_gradients,
    const uint8_t* mask
) {
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(gradients, NIMCP_ERROR_INVALID_PARAM, "gradients is NULL");
    NIMCP_CHECK_THROW(num_gradients > 0, NIMCP_ERROR_INVALID_PARAM, "num_gradients is 0");

    /* In inference mode, pass through */
    if (!ctx->config.training) {
        return NIMCP_SUCCESS;
    }

    float scale = 1.0F / (1.0F - ctx->config.rate);

    if (mask) {
        for (size_t i = 0; i < num_gradients; i++) {
            gradients[i] = mask[i] ? gradients[i] * scale : 0.0F;
        }
    }

    return NIMCP_SUCCESS;
}

void nimcp_dropout_set_training(nimcp_dropout_ctx_t* ctx, bool training) {
    if (ctx) {
        ctx->config.training = training;
    }
}

float nimcp_dropout_get_rate(const nimcp_dropout_ctx_t* ctx) {
    return ctx ? ctx->config.rate : 0.0F;
}

nimcp_result_t nimcp_dropout_set_rate(nimcp_dropout_ctx_t* ctx, float rate) {
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(rate >= NIMCP_DROPOUT_MIN && rate < NIMCP_DROPOUT_MAX,
        NIMCP_ERROR_INVALID_PARAM, "rate out of valid range");

    ctx->config.rate = rate;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Label Smoothing Operations
 * ============================================================================ */

void nimcp_label_smooth(
    float* labels,
    size_t num_samples,
    uint32_t num_classes,
    float smoothing
) {
    if (!labels || num_samples == 0 || num_classes == 0) {
        return;
    }

    if (smoothing <= 0.0F || smoothing >= 1.0F) {
        return;  /* No smoothing */
    }

    float smooth_val = smoothing / (float)num_classes;
    float confident_val = 1.0F - smoothing + smooth_val;

    for (size_t s = 0; s < num_samples; s++) {
        for (uint32_t c = 0; c < num_classes; c++) {
            size_t idx = s * num_classes + c;
            if (labels[idx] > 0.5F) {
                labels[idx] = confident_val;
            } else {
                labels[idx] = smooth_val;
            }
        }
    }
}

void nimcp_label_smooth_single(
    uint32_t true_class,
    uint32_t num_classes,
    float smoothing,
    float* output
) {
    if (!output || num_classes == 0 || true_class >= num_classes) {
        return;
    }

    float smooth_val = smoothing / (float)num_classes;
    float confident_val = 1.0F - smoothing + smooth_val;

    for (uint32_t c = 0; c < num_classes; c++) {
        output[c] = (c == true_class) ? confident_val : smooth_val;
    }
}

/* ============================================================================
 * Early Stopping Operations
 * ============================================================================ */

nimcp_early_stop_ctx_t* nimcp_early_stop_create(
    const nimcp_early_stop_config_t* config
) {
    if (!config) {
        LOG_MODULE_ERROR(LOG_MODULE, "Null early stop config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_early_stop_create: config is NULL");
        return NULL;
    }

    if (config->patience == 0 || config->patience > NIMCP_REG_MAX_PATIENCE) {
        LOG_MODULE_ERROR(LOG_MODULE, "Invalid patience: %u", config->patience);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_early_stop_create: config->patience is zero");
        return NULL;
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Creating early stopping context (patience=%u)", config->patience);

    nimcp_early_stop_ctx_t* ctx = (nimcp_early_stop_ctx_t*)nimcp_calloc(
        1, sizeof(nimcp_early_stop_ctx_t)
    );
    if (!ctx) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to allocate early stop context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_early_stop_create: ctx is NULL");
        return NULL;
    }

    memcpy(&ctx->config, config, sizeof(nimcp_early_stop_config_t));

    ctx->best_metric = (config->mode == NIMCP_EARLY_STOP_MIN) ? FLT_MAX : -FLT_MAX;
    ctx->best_epoch = 0;
    ctx->current_epoch = 0;
    ctx->wait_count = 0;
    ctx->improved = false;
    ctx->should_stop = false;
    ctx->initialized = true;

    LOG_MODULE_INFO(LOG_MODULE, "Early stopping created (patience=%u, mode=%d)",
                   config->patience, config->mode);

    return ctx;
}

void nimcp_early_stop_destroy(nimcp_early_stop_ctx_t* ctx) {
    if (ctx) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Destroying early stop context");
        nimcp_free(ctx);
    }
}

bool nimcp_early_stop_check(nimcp_early_stop_ctx_t* ctx, float metric) {
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_early_stop_check: required parameter is NULL (ctx, ctx->initialized)");
        return false;
    }

    ctx->current_epoch++;
    ctx->improved = false;

    bool is_improvement = false;

    if (ctx->config.mode == NIMCP_EARLY_STOP_MIN) {
        if (metric < ctx->best_metric - ctx->config.min_delta) {
            is_improvement = true;
        }
    } else {
        if (metric > ctx->best_metric + ctx->config.min_delta) {
            is_improvement = true;
        }
    }

    if (is_improvement) {
        ctx->best_metric = metric;
        ctx->best_epoch = ctx->current_epoch;
        ctx->wait_count = 0;
        ctx->improved = true;
    } else {
        ctx->wait_count++;
        if (ctx->wait_count >= ctx->config.patience) {
            ctx->should_stop = true;
        }
    }

    return ctx->should_stop;
}

void nimcp_early_stop_reset(nimcp_early_stop_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    ctx->best_metric = (ctx->config.mode == NIMCP_EARLY_STOP_MIN) ? FLT_MAX : -FLT_MAX;
    ctx->best_epoch = 0;
    ctx->current_epoch = 0;
    ctx->wait_count = 0;
    ctx->improved = false;
    ctx->should_stop = false;
}

float nimcp_early_stop_get_best(const nimcp_early_stop_ctx_t* ctx) {
    return ctx ? ctx->best_metric : 0.0F;
}

uint64_t nimcp_early_stop_get_best_epoch(const nimcp_early_stop_ctx_t* ctx) {
    return ctx ? ctx->best_epoch : 0;
}

bool nimcp_early_stop_improved(const nimcp_early_stop_ctx_t* ctx) {
    return ctx ? ctx->improved : false;
}

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

nimcp_result_t nimcp_regularization_get_stats(
    const nimcp_regularization_ctx_t* ctx,
    nimcp_regularization_stats_t* stats
) {
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    memcpy(stats, &ctx->stats, sizeof(nimcp_regularization_stats_t));
    return NIMCP_SUCCESS;
}

void nimcp_regularization_reset_stats(nimcp_regularization_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    memset(&ctx->stats, 0, sizeof(nimcp_regularization_stats_t));
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* nimcp_reg_type_name(nimcp_reg_type_t type) {
    static const char* names[] = {
        "None",
        "L1 (Lasso)",
        "L2 (Ridge)",
        "Elastic Net"
    };

    if (type >= NIMCP_REG_TYPE_COUNT) {
        return "Unknown";
    }
    return names[type];
}

const char* nimcp_clip_mode_name(nimcp_clip_mode_t mode) {
    static const char* names[] = {
        "None",
        "Clip by Value",
        "Clip by Norm",
        "Clip by Global Norm"
    };

    if (mode >= NIMCP_CLIP_MODE_COUNT) {
        return "Unknown";
    }
    return names[mode];
}

nimcp_result_t nimcp_regularization_validate_config(
    const nimcp_regularization_config_t* config
) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    /* Validate weight regularization */
    if (config->weight_reg_type >= NIMCP_REG_TYPE_COUNT) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    switch (config->weight_reg_type) {
        case NIMCP_REG_L1:
            if (config->weight_reg.l1.lambda < 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NIMCP_REG_L2:
            if (config->weight_reg.l2.lambda < 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NIMCP_REG_ELASTIC_NET:
            if (config->weight_reg.elastic_net.lambda < 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (config->weight_reg.elastic_net.alpha < 0.0F ||
                config->weight_reg.elastic_net.alpha > 1.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        default:
            break;
    }

    /* Validate gradient clipping */
    if (config->gradient_clip.mode >= NIMCP_CLIP_MODE_COUNT) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->gradient_clip.mode != NIMCP_CLIP_NONE &&
        config->gradient_clip.threshold <= 0.0F) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate dropout */
    if (config->dropout.rate < NIMCP_DROPOUT_MIN ||
        config->dropout.rate >= NIMCP_DROPOUT_MAX) {
        if (config->dropout.rate != 0.0F) {  /* Allow 0.0 explicitly */
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    if (config->dropout.mode >= NIMCP_DROPOUT_MODE_COUNT) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate label smoothing */
    if (config->use_label_smoothing) {
        if (config->label_smooth.smoothing < 0.0F ||
            config->label_smooth.smoothing > 1.0F) {
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->label_smooth.num_classes == 0) {
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate early stopping */
    if (config->use_early_stopping) {
        if (config->early_stop.patience == 0 ||
            config->early_stop.patience > NIMCP_REG_MAX_PATIENCE) {
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    return NIMCP_SUCCESS;
}
