/**
 * @file nimcp_loss_functions.c
 * @brief Loss Functions Module Implementation
 *
 * WHAT: Neural network loss functions with tensor-accelerated operations
 * WHY:  Efficient training with vectorized loss computation
 * HOW:  Uses nimcp_tensor library for batch operations
 *
 * Full implementation of loss functions for neural network training.
 * Includes security registration and memory pool integration.
 *
 * @version 1.1.0 - Added tensor library integration
 */

#include "middleware/training/nimcp_loss_functions.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/tensor/nimcp_tensor.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>

/* Default epsilon for numerical stability */
#define LOSS_DEFAULT_EPSILON 1e-7f
#define LOG_MODULE "loss_functions"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(loss_functions)

#define LOSS_MODULE_NAME "loss_functions"

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal loss context structure
 */
struct nimcp_loss_context {
    nimcp_loss_config_t config;
    nimcp_sec_integration_t* security_ctx;
    unified_mem_manager_t memory_mgr;
    nimcp_loss_stats_t stats;
    bool initialized;
    bool security_registered;
    uint32_t module_id;

    /* Working buffers (allocated from memory pool if available) */
    float* work_buffer;
    size_t work_buffer_size;

    /* Gradient buffer for forward_backward */
    float* grad_buffer;
    size_t grad_buffer_size;

    /* Bio-async integration (Phase BIO-1) */
    bio_module_context_t bio_ctx;      /**< Bio-async module context */
    bool bio_async_enabled;            /**< Bio-async availability flag */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in nanoseconds
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Update running statistics with new loss value
 */
static void update_stats(nimcp_loss_stats_t* stats, float loss_value, uint64_t compute_time) {
    stats->forward_count++;
    stats->total_loss += loss_value;

    if (stats->forward_count == 1) {
        stats->min_loss = loss_value;
        stats->max_loss = loss_value;
        stats->avg_loss = loss_value;
        stats->loss_variance = 0.0;
    } else {
        if (loss_value < stats->min_loss) stats->min_loss = loss_value;
        if (loss_value > stats->max_loss) stats->max_loss = loss_value;

        /* Welford's online algorithm for variance */
        double delta = loss_value - stats->avg_loss;
        stats->avg_loss += delta / stats->forward_count;
        double delta2 = loss_value - stats->avg_loss;
        stats->loss_variance += delta * delta2;
    }

    stats->total_compute_time_ns += compute_time;
}

/**
 * @brief Apply reduction to per-sample losses
 */
static float apply_reduction(const float* losses, size_t count, nimcp_loss_reduction_t reduction) {
    if (count == 0) return 0.0F;

    float sum = 0.0F;
    for (size_t i = 0; i < count; i++) {
        sum += losses[i];
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)count;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum; /* Will use per-sample array */
    }
}

/**
 * @brief Allocate buffer from unified memory
 */
static float* alloc_buffer(nimcp_loss_context_t* ctx, size_t count) {
    (void)ctx;  /* Reserved for future unified memory integration */
    size_t bytes = count * sizeof(float);
    return (float*)nimcp_malloc(bytes);
}

/**
 * @brief Free buffer to unified memory
 */
static void free_buffer(nimcp_loss_context_t* ctx, float* buffer) {
    (void)ctx;  /* Reserved for future unified memory integration */
    if (buffer) {
        nimcp_free(buffer);
    }
}

/* ============================================================================
 * Default Configuration Constructors
 * ============================================================================ */

nimcp_mse_config_t nimcp_loss_mse_default_config(void) {
    nimcp_mse_config_t cfg = {
        .reduction = NIMCP_LOSS_REDUCE_MEAN,
        .compute_gradient = true
    };
    return cfg;
}

nimcp_cross_entropy_config_t nimcp_loss_cross_entropy_default_config(size_t num_classes) {
    nimcp_cross_entropy_config_t cfg = {
        .reduction = NIMCP_LOSS_REDUCE_MEAN,
        .compute_gradient = true,
        .class_weights = NULL,
        .num_classes = num_classes,
        .label_smoothing = 0.0F,
        .ignore_index = -1
    };
    return cfg;
}

nimcp_kl_config_t nimcp_loss_kl_default_config(void) {
    nimcp_kl_config_t cfg = {
        .reduction = NIMCP_LOSS_REDUCE_MEAN,
        .compute_gradient = true,
        .log_target = false
    };
    return cfg;
}

nimcp_huber_config_t nimcp_loss_huber_default_config(float delta) {
    nimcp_huber_config_t cfg = {
        .reduction = NIMCP_LOSS_REDUCE_MEAN,
        .compute_gradient = true,
        .delta = (delta > 0.0F) ? delta : 1.0F
    };
    return cfg;
}

nimcp_focal_config_t nimcp_loss_focal_default_config(void) {
    nimcp_focal_config_t cfg = {
        .reduction = NIMCP_LOSS_REDUCE_MEAN,
        .compute_gradient = true,
        .gamma = 2.0F,
        .alpha = 0.25F
    };
    return cfg;
}

nimcp_loss_config_t nimcp_loss_default_config(nimcp_loss_type_t type) {
    nimcp_loss_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.type = type;
    cfg.use_memory_pool = true;
    cfg.cow_strategy = UNIFIED_STRATEGY_POOL_DIRECT;
    cfg.epsilon = LOSS_DEFAULT_EPSILON;
    cfg.clip_gradients = false;
    cfg.gradient_clip_value = 1.0F;

    switch (type) {
        case NIMCP_LOSS_MSE:
        case NIMCP_LOSS_MAE:
            cfg.params.mse = nimcp_loss_mse_default_config();
            break;
        case NIMCP_LOSS_CROSS_ENTROPY:
        case NIMCP_LOSS_BINARY_CROSS_ENTROPY:
            cfg.params.cross_entropy = nimcp_loss_cross_entropy_default_config(2);
            break;
        case NIMCP_LOSS_KL_DIVERGENCE:
            cfg.params.kl = nimcp_loss_kl_default_config();
            break;
        case NIMCP_LOSS_HUBER:
            cfg.params.huber = nimcp_loss_huber_default_config(1.0F);
            break;
        case NIMCP_LOSS_FOCAL:
            cfg.params.focal = nimcp_loss_focal_default_config();
            break;
        case NIMCP_LOSS_CONTRASTIVE:
            cfg.params.contrastive.reduction = NIMCP_LOSS_REDUCE_MEAN;
            cfg.params.contrastive.compute_gradient = true;
            cfg.params.contrastive.margin = 1.0F;
            break;
        case NIMCP_LOSS_TRIPLET:
            cfg.params.triplet.reduction = NIMCP_LOSS_REDUCE_MEAN;
            cfg.params.triplet.compute_gradient = true;
            cfg.params.triplet.margin = 1.0F;
            cfg.params.triplet.swap = false;
            break;
        default:
            break;
    }

    return cfg;
}

/* ============================================================================
 * Loss Context Lifecycle
 * ============================================================================ */

nimcp_loss_context_t* nimcp_loss_create(
    const nimcp_loss_config_t* config,
    nimcp_sec_integration_t* security_ctx,
    unified_mem_manager_t memory_mgr)
{
    if (!config) {
        LOG_ERROR(LOSS_MODULE_NAME, "NULL config provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_loss_create: config is NULL");
        return NULL;
    }

    nimcp_loss_context_t* ctx = (nimcp_loss_context_t*)nimcp_calloc(1, sizeof(nimcp_loss_context_t));
    if (!ctx) {
        LOG_ERROR(LOSS_MODULE_NAME, "Failed to allocate loss context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_loss_create: ctx is NULL");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(nimcp_loss_config_t));
    ctx->security_ctx = security_ctx;
    ctx->memory_mgr = memory_mgr;
    ctx->initialized = false;
    ctx->security_registered = false;

    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(nimcp_loss_stats_t));
    ctx->stats.min_loss = FLT_MAX;
    ctx->stats.max_loss = -FLT_MAX;

    LOG_DEBUG(LOSS_MODULE_NAME, "Created loss context type=%s",
              nimcp_loss_type_name(config->type));

    return ctx;
}

nimcp_result_t nimcp_loss_init(nimcp_loss_context_t* ctx) {
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    if (ctx->initialized) {
        return NIMCP_SUCCESS; /* Already initialized */
    }

    /* Register with security module if available */
    if (ctx->security_ctx) {
        nimcp_result_t err = nimcp_sec_register_module(
            ctx->security_ctx,
            LOSS_MODULE_NAME,
            NIMCP_SEC_CAT_PLASTICITY,
            &ctx->module_id
        );

        if (err == NIMCP_SUCCESS) {
            ctx->security_registered = true;
            LOG_DEBUG(LOSS_MODULE_NAME, "Registered with security module (id=%u)", ctx->module_id);
        } else {
            LOG_WARNING(LOSS_MODULE_NAME, "Security registration failed: %d", err);
        }
    }

    /* Register with bio-async router */
    ctx->bio_ctx = NULL;
    ctx->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_TRAINING_LOSS,
            .module_name = "loss_functions",
            .inbox_capacity = 16,
            .user_data = ctx
        };
        ctx->bio_ctx = bio_router_register_module(&bio_info);
        if (ctx->bio_ctx) {
            ctx->bio_async_enabled = true;
            LOG_DEBUG(LOSS_MODULE_NAME, "Registered with bio-async router");
        }
    }

    ctx->initialized = true;
    LOG_INFO(LOSS_MODULE_NAME, "Initialized loss function type=%s",
             nimcp_loss_type_name(ctx->config.type));

    return NIMCP_SUCCESS;
}

void nimcp_loss_destroy(nimcp_loss_context_t* ctx) {
    if (!ctx) return;

    /* Unregister from bio-async router */
    if (ctx->bio_async_enabled && ctx->bio_ctx) {
        bio_router_unregister_module(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
        ctx->bio_async_enabled = false;
    }

    /* Free working buffers */
    if (ctx->work_buffer) {
        free_buffer(ctx, ctx->work_buffer);
        ctx->work_buffer = NULL;
    }

    if (ctx->grad_buffer) {
        free_buffer(ctx, ctx->grad_buffer);
        ctx->grad_buffer = NULL;
    }

    /* Free class weights if allocated */
    if (ctx->config.type == NIMCP_LOSS_CROSS_ENTROPY &&
        ctx->config.params.cross_entropy.class_weights) {
        free_buffer(ctx, ctx->config.params.cross_entropy.class_weights);
    }

    LOG_DEBUG(LOSS_MODULE_NAME, "Destroyed loss context");
    nimcp_free(ctx);
}

nimcp_result_t nimcp_loss_reset(nimcp_loss_context_t* ctx) {
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    memset(&ctx->stats, 0, sizeof(nimcp_loss_stats_t));
    ctx->stats.min_loss = FLT_MAX;
    ctx->stats.max_loss = -FLT_MAX;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Direct Loss Function Implementations
 * ============================================================================ */

float nimcp_loss_mse(
    const float* predictions,
    const float* targets,
    size_t count,
    nimcp_loss_reduction_t reduction)
{
    if (!predictions || !targets || count == 0) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (size_t i = 0; i < count; i++) {
        float diff = predictions[i] - targets[i];
        sum += diff * diff;
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)count;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

void nimcp_loss_mse_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count)
{
    if (!predictions || !targets || !gradients || count == 0) return;

    float scale = 2.0F / (float)count;
    for (size_t i = 0; i < count; i++) {
        gradients[i] = scale * (predictions[i] - targets[i]);
    }
}

float nimcp_loss_mae(
    const float* predictions,
    const float* targets,
    size_t count,
    nimcp_loss_reduction_t reduction)
{
    if (!predictions || !targets || count == 0) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (size_t i = 0; i < count; i++) {
        sum += fabsf(predictions[i] - targets[i]);
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)count;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

void nimcp_loss_mae_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count)
{
    if (!predictions || !targets || !gradients || count == 0) return;

    float scale = 1.0F / (float)count;
    for (size_t i = 0; i < count; i++) {
        float diff = predictions[i] - targets[i];
        if (diff > 0.0F) {
            gradients[i] = scale;
        } else if (diff < 0.0F) {
            gradients[i] = -scale;
        } else {
            gradients[i] = 0.0F;
        }
    }
}

float nimcp_loss_binary_cross_entropy(
    const float* predictions,
    const float* targets,
    size_t count,
    nimcp_loss_reduction_t reduction,
    float epsilon)
{
    if (!predictions || !targets || count == 0) {
        return 0.0F;
    }

    if (epsilon <= 0.0F) epsilon = LOSS_DEFAULT_EPSILON;

    float sum = 0.0F;
    for (size_t i = 0; i < count; i++) {
        /* Clamp predictions to prevent log(0) */
        float p = fmaxf(fminf(predictions[i], 1.0F - epsilon), epsilon);
        float t = targets[i];

        /* BCE = -[t*log(p) + (1-t)*log(1-p)] */
        sum += -(t * logf(p) + (1.0F - t) * logf(1.0F - p));
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)count;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

void nimcp_loss_binary_cross_entropy_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count,
    float epsilon)
{
    if (!predictions || !targets || !gradients || count == 0) return;

    if (epsilon <= 0.0F) epsilon = LOSS_DEFAULT_EPSILON;

    float scale = 1.0F / (float)count;
    for (size_t i = 0; i < count; i++) {
        float p = fmaxf(fminf(predictions[i], 1.0F - epsilon), epsilon);
        float t = targets[i];

        /* d(BCE)/dp = -t/p + (1-t)/(1-p) = (p-t) / (p*(1-p)) */
        gradients[i] = scale * (p - t) / (p * (1.0F - p));
    }
}

float nimcp_loss_cross_entropy(
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t num_classes,
    nimcp_loss_reduction_t reduction,
    float epsilon)
{
    if (!predictions || !targets || batch_size == 0 || num_classes == 0) {
        return 0.0F;
    }

    if (epsilon <= 0.0F) epsilon = LOSS_DEFAULT_EPSILON;

    float sum = 0.0F;
    for (size_t b = 0; b < batch_size; b++) {
        const float* pred_row = predictions + b * num_classes;
        const float* target_row = targets + b * num_classes;

        float sample_loss = 0.0F;
        for (size_t c = 0; c < num_classes; c++) {
            float p = fmaxf(pred_row[c], epsilon);
            sample_loss -= target_row[c] * logf(p);
        }
        sum += sample_loss;
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)batch_size;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

void nimcp_loss_cross_entropy_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t batch_size,
    size_t num_classes)
{
    if (!predictions || !targets || !gradients || batch_size == 0 || num_classes == 0) return;

    /* For softmax + cross entropy, gradient simplifies to: p - t */
    float scale = 1.0F / (float)batch_size;
    size_t total = batch_size * num_classes;

    for (size_t i = 0; i < total; i++) {
        gradients[i] = scale * (predictions[i] - targets[i]);
    }
}

float nimcp_loss_kl_divergence(
    const float* p,
    const float* q,
    size_t count,
    nimcp_loss_reduction_t reduction,
    float epsilon)
{
    if (!p || !q || count == 0) {
        return 0.0F;
    }

    if (epsilon <= 0.0F) epsilon = LOSS_DEFAULT_EPSILON;

    float sum = 0.0F;
    for (size_t i = 0; i < count; i++) {
        float p_i = fmaxf(p[i], epsilon);
        float q_i = fmaxf(q[i], epsilon);

        /* KL(p||q) = sum(p * log(p/q)) */
        sum += p_i * logf(p_i / q_i);
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)count;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

void nimcp_loss_kl_divergence_grad(
    const float* p,
    const float* q,
    float* gradients,
    size_t count,
    float epsilon)
{
    if (!p || !q || !gradients || count == 0) return;

    if (epsilon <= 0.0F) epsilon = LOSS_DEFAULT_EPSILON;

    /* d(KL)/dp = log(p/q) + 1 */
    float scale = 1.0F / (float)count;
    for (size_t i = 0; i < count; i++) {
        float p_i = fmaxf(p[i], epsilon);
        float q_i = fmaxf(q[i], epsilon);
        gradients[i] = scale * (logf(p_i / q_i) + 1.0F);
    }
}

float nimcp_loss_huber(
    const float* predictions,
    const float* targets,
    size_t count,
    float delta,
    nimcp_loss_reduction_t reduction)
{
    if (!predictions || !targets || count == 0) {
        return 0.0F;
    }

    if (delta <= 0.0F) delta = 1.0F;

    float sum = 0.0F;
    for (size_t i = 0; i < count; i++) {
        float abs_diff = fabsf(predictions[i] - targets[i]);

        if (abs_diff <= delta) {
            /* Quadratic region (like MSE) */
            sum += 0.5F * abs_diff * abs_diff;
        } else {
            /* Linear region (like MAE) */
            sum += delta * (abs_diff - 0.5F * delta);
        }
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)count;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

void nimcp_loss_huber_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count,
    float delta)
{
    if (!predictions || !targets || !gradients || count == 0) return;

    if (delta <= 0.0F) delta = 1.0F;

    float scale = 1.0F / (float)count;
    for (size_t i = 0; i < count; i++) {
        float diff = predictions[i] - targets[i];
        float abs_diff = fabsf(diff);

        if (abs_diff <= delta) {
            gradients[i] = scale * diff;
        } else {
            gradients[i] = scale * delta * (diff > 0.0F ? 1.0F : -1.0F);
        }
    }
}

float nimcp_loss_hinge(
    const float* predictions,
    const float* targets,
    size_t count,
    nimcp_loss_reduction_t reduction)
{
    if (!predictions || !targets || count == 0) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (size_t i = 0; i < count; i++) {
        /* Hinge: max(0, 1 - y*f(x)) where y in {-1, +1} */
        float margin = 1.0F - targets[i] * predictions[i];
        sum += fmaxf(0.0F, margin);
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)count;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

void nimcp_loss_hinge_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count)
{
    if (!predictions || !targets || !gradients || count == 0) return;

    float scale = 1.0F / (float)count;
    for (size_t i = 0; i < count; i++) {
        float margin = 1.0F - targets[i] * predictions[i];
        if (margin > 0.0F) {
            gradients[i] = -scale * targets[i];
        } else {
            gradients[i] = 0.0F;
        }
    }
}

float nimcp_loss_focal(
    const float* predictions,
    const float* targets,
    size_t count,
    float gamma,
    float alpha,
    nimcp_loss_reduction_t reduction,
    float epsilon)
{
    if (!predictions || !targets || count == 0) {
        return 0.0F;
    }

    if (epsilon <= 0.0F) epsilon = LOSS_DEFAULT_EPSILON;

    float sum = 0.0F;
    for (size_t i = 0; i < count; i++) {
        float p = fmaxf(fminf(predictions[i], 1.0F - epsilon), epsilon);
        float t = targets[i];

        /* Focal loss: -alpha * (1-p)^gamma * log(p) for positive class */
        float pt = t * p + (1.0F - t) * (1.0F - p);
        float at = t * alpha + (1.0F - t) * (1.0F - alpha);

        sum += -at * powf(1.0F - pt, gamma) * logf(pt);
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)count;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

void nimcp_loss_focal_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count,
    float gamma,
    float alpha,
    float epsilon)
{
    if (!predictions || !targets || !gradients || count == 0) return;

    if (epsilon <= 0.0F) epsilon = LOSS_DEFAULT_EPSILON;

    float scale = 1.0F / (float)count;
    for (size_t i = 0; i < count; i++) {
        float p = fmaxf(fminf(predictions[i], 1.0F - epsilon), epsilon);
        float t = targets[i];

        float pt = t * p + (1.0F - t) * (1.0F - p);
        float at = t * alpha + (1.0F - t) * (1.0F - alpha);

        /* Focal loss gradient is complex - approximation */
        float focal_weight = powf(1.0F - pt, gamma);
        float grad_factor = gamma * focal_weight * logf(pt) / (1.0F - pt) + focal_weight / pt;

        gradients[i] = scale * at * grad_factor * (2.0F * t - 1.0F);
    }
}

/**
 * @brief Compute Euclidean distance between two vectors
 */
static float euclidean_distance(const float* a, const float* b, size_t dim) {
    float sum = 0.0F;
    for (size_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

float nimcp_loss_contrastive(
    const float* embeddings1,
    const float* embeddings2,
    const float* labels,
    size_t batch_size,
    size_t embed_dim,
    float margin,
    nimcp_loss_reduction_t reduction)
{
    if (!embeddings1 || !embeddings2 || !labels || batch_size == 0 || embed_dim == 0) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (size_t b = 0; b < batch_size; b++) {
        const float* e1 = embeddings1 + b * embed_dim;
        const float* e2 = embeddings2 + b * embed_dim;
        float y = labels[b]; /* 1 for similar, 0 for dissimilar */

        float dist = euclidean_distance(e1, e2, embed_dim);

        /* Contrastive: y*d^2 + (1-y)*max(0, margin-d)^2 */
        float similar_term = y * dist * dist;
        float margin_term = fmaxf(0.0F, margin - dist);
        float dissimilar_term = (1.0F - y) * margin_term * margin_term;

        sum += 0.5F * (similar_term + dissimilar_term);
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)batch_size;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

float nimcp_loss_triplet(
    const float* anchors,
    const float* positives,
    const float* negatives,
    size_t batch_size,
    size_t embed_dim,
    float margin,
    nimcp_loss_reduction_t reduction)
{
    if (!anchors || !positives || !negatives || batch_size == 0 || embed_dim == 0) {
        return 0.0F;
    }

    float sum = 0.0F;
    for (size_t b = 0; b < batch_size; b++) {
        const float* a = anchors + b * embed_dim;
        const float* p = positives + b * embed_dim;
        const float* n = negatives + b * embed_dim;

        float d_ap = euclidean_distance(a, p, embed_dim);
        float d_an = euclidean_distance(a, n, embed_dim);

        /* Triplet: max(0, d(a,p) - d(a,n) + margin) */
        sum += fmaxf(0.0F, d_ap - d_an + margin);
    }

    switch (reduction) {
        case NIMCP_LOSS_REDUCE_MEAN:
            return sum / (float)batch_size;
        case NIMCP_LOSS_REDUCE_SUM:
            return sum;
        case NIMCP_LOSS_REDUCE_NONE:
        default:
            return sum;
    }
}

/* ============================================================================
 * Loss Context Forward/Backward
 * ============================================================================ */

nimcp_result_t nimcp_loss_forward(
    nimcp_loss_context_t* ctx,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    nimcp_loss_result_t* result)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(predictions, NIMCP_ERROR_INVALID_PARAM, "predictions is NULL");
    NIMCP_CHECK_THROW(targets, NIMCP_ERROR_INVALID_PARAM, "targets is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");

    if (!ctx->initialized) {
        nimcp_result_t err = nimcp_loss_init(ctx);
        if (err != NIMCP_SUCCESS) return err;
    }

    uint64_t start_time = get_time_ns();
    size_t count = batch_size * output_size;

    memset(result, 0, sizeof(nimcp_loss_result_t));
    result->sample_count = batch_size;

    float loss_value = 0.0F;

    switch (ctx->config.type) {
        case NIMCP_LOSS_MSE:
            loss_value = nimcp_loss_mse(predictions, targets, count,
                                        ctx->config.params.mse.reduction);
            break;

        case NIMCP_LOSS_MAE:
            loss_value = nimcp_loss_mae(predictions, targets, count,
                                        ctx->config.params.mse.reduction);
            break;

        case NIMCP_LOSS_BINARY_CROSS_ENTROPY:
            loss_value = nimcp_loss_binary_cross_entropy(predictions, targets, count,
                                                          ctx->config.params.cross_entropy.reduction,
                                                          ctx->config.epsilon);
            break;

        case NIMCP_LOSS_CROSS_ENTROPY:
            loss_value = nimcp_loss_cross_entropy(predictions, targets,
                                                   batch_size, output_size,
                                                   ctx->config.params.cross_entropy.reduction,
                                                   ctx->config.epsilon);
            break;

        case NIMCP_LOSS_KL_DIVERGENCE:
            loss_value = nimcp_loss_kl_divergence(predictions, targets, count,
                                                   ctx->config.params.kl.reduction,
                                                   ctx->config.epsilon);
            break;

        case NIMCP_LOSS_HUBER:
            loss_value = nimcp_loss_huber(predictions, targets, count,
                                           ctx->config.params.huber.delta,
                                           ctx->config.params.huber.reduction);
            break;

        case NIMCP_LOSS_HINGE:
            loss_value = nimcp_loss_hinge(predictions, targets, count,
                                           ctx->config.params.mse.reduction);
            break;

        case NIMCP_LOSS_FOCAL:
            loss_value = nimcp_loss_focal(predictions, targets, count,
                                           ctx->config.params.focal.gamma,
                                           ctx->config.params.focal.alpha,
                                           ctx->config.params.focal.reduction,
                                           ctx->config.epsilon);
            break;

        case NIMCP_LOSS_CUSTOM:
            if (ctx->config.params.custom.forward_fn) {
                loss_value = ctx->config.params.custom.forward_fn(
                    predictions, targets, count,
                    ctx->config.params.custom.user_data);
            }
            break;

        default:
            LOG_ERROR(LOSS_MODULE_NAME, "Unsupported loss type: %d", ctx->config.type);
            return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    result->loss_value = loss_value;
    result->compute_time_ns = get_time_ns() - start_time;

    /* Update statistics */
    update_stats(&ctx->stats, loss_value, result->compute_time_ns);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_loss_backward(
    nimcp_loss_context_t* ctx,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    float* gradients)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(predictions, NIMCP_ERROR_INVALID_PARAM, "predictions is NULL");
    NIMCP_CHECK_THROW(targets, NIMCP_ERROR_INVALID_PARAM, "targets is NULL");
    NIMCP_CHECK_THROW(gradients, NIMCP_ERROR_INVALID_PARAM, "gradients is NULL");

    if (!ctx->initialized) {
        nimcp_result_t err = nimcp_loss_init(ctx);
        if (err != NIMCP_SUCCESS) return err;
    }

    size_t count = batch_size * output_size;

    switch (ctx->config.type) {
        case NIMCP_LOSS_MSE:
            nimcp_loss_mse_grad(predictions, targets, gradients, count);
            break;

        case NIMCP_LOSS_MAE:
            nimcp_loss_mae_grad(predictions, targets, gradients, count);
            break;

        case NIMCP_LOSS_BINARY_CROSS_ENTROPY:
            nimcp_loss_binary_cross_entropy_grad(predictions, targets, gradients,
                                                  count, ctx->config.epsilon);
            break;

        case NIMCP_LOSS_CROSS_ENTROPY:
            nimcp_loss_cross_entropy_grad(predictions, targets, gradients,
                                           batch_size, output_size);
            break;

        case NIMCP_LOSS_KL_DIVERGENCE:
            nimcp_loss_kl_divergence_grad(predictions, targets, gradients,
                                           count, ctx->config.epsilon);
            break;

        case NIMCP_LOSS_HUBER:
            nimcp_loss_huber_grad(predictions, targets, gradients, count,
                                   ctx->config.params.huber.delta);
            break;

        case NIMCP_LOSS_HINGE:
            nimcp_loss_hinge_grad(predictions, targets, gradients, count);
            break;

        case NIMCP_LOSS_FOCAL:
            nimcp_loss_focal_grad(predictions, targets, gradients, count,
                                   ctx->config.params.focal.gamma,
                                   ctx->config.params.focal.alpha,
                                   ctx->config.epsilon);
            break;

        case NIMCP_LOSS_CUSTOM:
            if (ctx->config.params.custom.backward_fn) {
                ctx->config.params.custom.backward_fn(
                    predictions, targets, gradients, count,
                    ctx->config.params.custom.user_data);
            }
            break;

        default:
            LOG_ERROR(LOSS_MODULE_NAME, "Unsupported loss type: %d", ctx->config.type);
            return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    /* Apply gradient clipping if enabled */
    if (ctx->config.clip_gradients) {
        size_t clipped = nimcp_loss_clip_gradients(gradients, count,
                                                    ctx->config.gradient_clip_value);
        ctx->stats.gradient_clips += clipped;
    }

    ctx->stats.backward_count++;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_loss_forward_backward(
    nimcp_loss_context_t* ctx,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    nimcp_loss_result_t* result)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(predictions, NIMCP_ERROR_INVALID_PARAM, "predictions is NULL");
    NIMCP_CHECK_THROW(targets, NIMCP_ERROR_INVALID_PARAM, "targets is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result is NULL");

    size_t count = batch_size * output_size;

    /* Compute forward */
    nimcp_result_t err = nimcp_loss_forward(ctx, predictions, targets,
                                            batch_size, output_size, result);
    if (err != NIMCP_SUCCESS) return err;

    /* Allocate gradient buffer */
    result->gradients = alloc_buffer(ctx, count);
    if (!result->gradients) {
        return NIMCP_ERROR_MEMORY;
    }
    result->gradient_count = count;

    /* Compute backward */
    err = nimcp_loss_backward(ctx, predictions, targets,
                               batch_size, output_size, result->gradients);
    if (err != NIMCP_SUCCESS) {
        free_buffer(ctx, result->gradients);
        result->gradients = NULL;
        result->gradient_count = 0;
        return err;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

nimcp_result_t nimcp_loss_get_stats(
    const nimcp_loss_context_t* ctx,
    nimcp_loss_stats_t* stats)
{
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    memcpy(stats, &ctx->stats, sizeof(nimcp_loss_stats_t));

    /* Finalize variance calculation */
    if (stats->forward_count > 1) {
        stats->loss_variance /= (stats->forward_count - 1);
    }

    return NIMCP_SUCCESS;
}

void nimcp_loss_reset_stats(nimcp_loss_context_t* ctx) {
    if (!ctx) return;
    memset(&ctx->stats, 0, sizeof(nimcp_loss_stats_t));
    ctx->stats.min_loss = (double)HUGE_VALF;
}

const char* nimcp_loss_type_name(nimcp_loss_type_t type) {
    switch (type) {
        case NIMCP_LOSS_MSE:              return "MSE";
        case NIMCP_LOSS_MAE:              return "MAE";
        case NIMCP_LOSS_CROSS_ENTROPY:    return "CrossEntropy";
        case NIMCP_LOSS_BINARY_CROSS_ENTROPY: return "BinaryCrossEntropy";
        case NIMCP_LOSS_KL_DIVERGENCE:    return "KLDivergence";
        case NIMCP_LOSS_HUBER:            return "Huber";
        case NIMCP_LOSS_HINGE:            return "Hinge";
        case NIMCP_LOSS_LOG_COSH:         return "LogCosh";
        case NIMCP_LOSS_FOCAL:            return "Focal";
        case NIMCP_LOSS_CONTRASTIVE:      return "Contrastive";
        case NIMCP_LOSS_TRIPLET:          return "Triplet";
        case NIMCP_LOSS_CUSTOM:           return "Custom";
        default:                          return "Unknown";
    }
}

nimcp_result_t nimcp_loss_validate_config(const nimcp_loss_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    if (config->type < 0 || config->type >= NIMCP_LOSS_TYPE_COUNT) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->epsilon < 0.0F) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (config->clip_gradients && config->gradient_clip_value <= 0.0F) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Type-specific validation */
    switch (config->type) {
        case NIMCP_LOSS_HUBER:
            if (config->params.huber.delta <= 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NIMCP_LOSS_FOCAL:
            if (config->params.focal.gamma < 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NIMCP_LOSS_CUSTOM:
            if (!config->params.custom.forward_fn) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        default:
            break;
    }

    return NIMCP_SUCCESS;
}

void nimcp_loss_softmax(
    const float* logits,
    float* output,
    size_t batch_size,
    size_t num_classes)
{
    if (!logits || !output || batch_size == 0 || num_classes == 0) return;

    for (size_t b = 0; b < batch_size; b++) {
        const float* in_row = logits + b * num_classes;
        float* out_row = output + b * num_classes;

        /* Find max for numerical stability */
        float max_val = in_row[0];
        for (size_t c = 1; c < num_classes; c++) {
            if (in_row[c] > max_val) max_val = in_row[c];
        }

        /* Compute exp(x - max) and sum */
        float sum = 0.0F;
        for (size_t c = 0; c < num_classes; c++) {
            out_row[c] = expf(in_row[c] - max_val);
            sum += out_row[c];
        }

        /* Normalize */
        for (size_t c = 0; c < num_classes; c++) {
            out_row[c] /= sum;
        }
    }
}

void nimcp_loss_sigmoid(
    const float* input,
    float* output,
    size_t count)
{
    if (!input || !output || count == 0) return;

    for (size_t i = 0; i < count; i++) {
        output[i] = 1.0F / (1.0F + expf(-input[i]));
    }
}

size_t nimcp_loss_clip_gradients(
    float* gradients,
    size_t count,
    float max_value)
{
    if (!gradients || count == 0 || max_value <= 0.0F) return 0;

    size_t clipped = 0;
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

/**
 * @brief Clip gradients by L2 norm using tensor operations
 *
 * WHAT: Scale gradients if L2 norm exceeds max_norm
 * WHY:  Prevent gradient explosion during training
 * HOW:  Uses nimcp_tensor_norm_p for vectorized norm computation
 */
float nimcp_loss_clip_gradients_norm(
    float* gradients,
    size_t count,
    float max_norm)
{
    if (!gradients || count == 0 || max_norm <= 0.0F) return 0.0F;

    /* Try tensor-accelerated gradient clipping */
    uint32_t dims[] = {(uint32_t)count};
    nimcp_tensor_t* t = nimcp_tensor_from_data(gradients, dims, 1, NIMCP_DTYPE_F32, false);
    if (t) {
        float norm = (float)nimcp_tensor_norm_p(t, 2.0);
        if (norm > max_norm) {
            nimcp_tensor_mul_scalar_(t, (double)max_norm / (double)norm);
        }
        nimcp_tensor_destroy(t);
        return norm;
    }

    /* Fallback to scalar computation */
    float norm_sq = 0.0F;
    for (size_t i = 0; i < count; i++) {
        norm_sq += gradients[i] * gradients[i];
    }
    float norm = sqrtf(norm_sq);

    if (norm > max_norm) {
        float scale = max_norm / norm;
        for (size_t i = 0; i < count; i++) {
            gradients[i] *= scale;
        }
    }

    return norm;
}

void nimcp_loss_result_free(nimcp_loss_result_t* result) {
    if (!result) return;

    /* Use unified memory for proper allocation tracking */
    if (result->gradients) {
        nimcp_free(result->gradients);
        result->gradients = NULL;
    }

    if (result->per_sample_loss) {
        nimcp_free(result->per_sample_loss);
        result->per_sample_loss = NULL;
    }

    result->gradient_count = 0;
    result->sample_count = 0;
}

bool nimcp_loss_is_registered(const nimcp_loss_context_t* ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->security_registered;
}

/* ============================================================================
 * Tensor-Based Loss Functions
 * ============================================================================ */

/**
 * @brief Compute MSE loss from tensors
 *
 * WHAT: Mean squared error using tensor operations
 * WHY:  Efficient batch loss computation
 * HOW:  Uses tensor subtraction and element-wise operations
 */
float nimcp_loss_mse_tensor(
    const nimcp_tensor_t* predictions,
    const nimcp_tensor_t* targets,
    nimcp_loss_reduction_t reduction
) {
    if (!predictions || !targets) {
        return 0.0F;
    }

    size_t n = nimcp_tensor_numel(predictions);
    if (n != nimcp_tensor_numel(targets) || n == 0) {
        return 0.0F;
    }

    /* Compute (predictions - targets)^2 */
    nimcp_tensor_t* diff = nimcp_tensor_sub(predictions, targets);
    if (!diff) {
        return 0.0F;
    }

    nimcp_tensor_t* sq = nimcp_tensor_mul(diff, diff);
    nimcp_tensor_destroy(diff);
    if (!sq) {
        return 0.0F;
    }

    /* Reduce */
    float result;
    if (reduction == NIMCP_LOSS_REDUCE_SUM) {
        nimcp_tensor_t* sum_t = nimcp_tensor_sum(sq);
        result = (float)nimcp_tensor_get_flat(sum_t, 0);
        nimcp_tensor_destroy(sum_t);
    } else {
        /* Default to mean */
        nimcp_tensor_t* mean_t = nimcp_tensor_mean(sq);
        result = (float)nimcp_tensor_get_flat(mean_t, 0);
        nimcp_tensor_destroy(mean_t);
    }

    nimcp_tensor_destroy(sq);
    return result;
}

/**
 * @brief Compute MSE gradient from tensors
 *
 * WHAT: Gradient of MSE loss
 * WHY:  Backpropagation for neural network training
 * HOW:  gradient = 2 * (predictions - targets) / n
 */
nimcp_tensor_t* nimcp_loss_mse_grad_tensor(
    const nimcp_tensor_t* predictions,
    const nimcp_tensor_t* targets
) {
    if (!predictions || !targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_loss_mse_grad_tensor: required parameter is NULL (predictions, targets)");
        return NULL;
    }

    size_t n = nimcp_tensor_numel(predictions);
    if (n != nimcp_tensor_numel(targets) || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_loss_mse_grad_tensor: n is zero");
        return NULL;
    }

    /* gradient = 2 * (pred - target) / n */
    nimcp_tensor_t* grad = nimcp_tensor_sub(predictions, targets);
    if (!grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "grad is NULL");

        return NULL;
    }

    double scale = 2.0 / (double)n;
    nimcp_tensor_mul_scalar_(grad, scale);

    return grad;
}

/**
 * @brief Compute softmax from tensor
 *
 * WHAT: Softmax activation function
 * WHY:  Convert logits to probabilities
 * HOW:  Uses nimcp_tensor_softmax
 */
nimcp_tensor_t* nimcp_loss_softmax_tensor(
    const nimcp_tensor_t* logits,
    int axis
) {
    if (!logits) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logits is NULL");

        return NULL;
    }
    return nimcp_tensor_softmax(logits, axis);
}

/**
 * @brief Compute cross-entropy loss from tensors
 *
 * WHAT: Cross-entropy loss for classification
 * WHY:  Standard loss for multi-class classification
 * HOW:  -sum(target * log(prediction + eps)) / n
 */
float nimcp_loss_cross_entropy_tensor(
    const nimcp_tensor_t* predictions,
    const nimcp_tensor_t* targets,
    float epsilon,
    nimcp_loss_reduction_t reduction
) {
    if (!predictions || !targets) {
        return 0.0F;
    }

    size_t n = nimcp_tensor_numel(predictions);
    if (n != nimcp_tensor_numel(targets) || n == 0) {
        return 0.0F;
    }

    if (epsilon <= 0.0F) epsilon = LOSS_DEFAULT_EPSILON;

    /* Clamp predictions for numerical stability */
    nimcp_tensor_t* pred_clamped = nimcp_tensor_clone(predictions);
    if (!pred_clamped) {
        return 0.0F;
    }

    float* data = (float*)nimcp_tensor_data(pred_clamped);
    for (size_t i = 0; i < n; i++) {
        if (data[i] < epsilon) data[i] = epsilon;
        if (data[i] > 1.0F - epsilon) data[i] = 1.0F - epsilon;
    }

    /* Compute log(pred) */
    nimcp_tensor_t* log_pred = nimcp_tensor_log(pred_clamped);
    nimcp_tensor_destroy(pred_clamped);
    if (!log_pred) {
        return 0.0F;
    }

    /* Compute -target * log(pred) */
    nimcp_tensor_t* loss = nimcp_tensor_mul(targets, log_pred);
    nimcp_tensor_destroy(log_pred);
    if (!loss) {
        return 0.0F;
    }

    nimcp_tensor_mul_scalar_(loss, -1.0);

    /* Reduce */
    float result;
    nimcp_tensor_t* reduced = nimcp_tensor_sum(loss);
    nimcp_tensor_destroy(loss);
    if (!reduced) {
        return 0.0F;
    }

    result = (float)nimcp_tensor_get_flat(reduced, 0);
    nimcp_tensor_destroy(reduced);

    if (reduction == NIMCP_LOSS_REDUCE_MEAN) {
        /* Get batch size from first dimension */
        const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(predictions);
        size_t batch_size = (shape->rank > 1) ? shape->dims[0] : 1;
        result /= (float)batch_size;
    }

    return result;
}

/**
 * @brief Clip tensor gradients by norm
 *
 * WHAT: Scale gradient tensor if norm exceeds threshold
 * WHY:  Prevent gradient explosion
 * HOW:  Uses tensor norm and scalar multiplication
 *
 * @param gradients Gradient tensor (modified in place)
 * @param max_norm Maximum allowed norm
 * @return Original norm before clipping
 */
float nimcp_loss_clip_gradients_norm_tensor(
    nimcp_tensor_t* gradients,
    float max_norm
) {
    if (!gradients || max_norm <= 0.0F) {
        return 0.0F;
    }

    float norm = (float)nimcp_tensor_norm_p(gradients, 2.0);
    if (norm > max_norm) {
        nimcp_tensor_mul_scalar_(gradients, (double)max_norm / (double)norm);
    }

    return norm;
}
