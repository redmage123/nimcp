/**
 * @file nimcp_mixed_precision.c
 * @brief Mixed Precision Training (AMP) Implementation
 *
 * WHAT: Automatic Mixed Precision with FP16/BF16 compute and FP32 storage
 * WHY:  2-3x speedup on modern GPUs with minimal accuracy loss
 * HOW:  Dynamic loss scaling, operator autocasting, master weight maintenance
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_mixed_precision.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mixed_precision)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief AMP context internal structure
 */
struct amp_ctx_s {
    amp_config_t config;              /**< Configuration */

    /* Loss scaling state */
    float current_scale;              /**< Current loss scale */
    uint32_t steps_since_last_overflow; /**< Steps since overflow */
    bool last_step_overflowed;        /**< Last step had overflow */

    /* Autocast state (thread-local simulation) */
    bool autocast_enabled;            /**< Autocast currently active */
    uint32_t autocast_depth;          /**< Nested autocast depth */

    /* Gradient manager integration */
    nimcp_gradient_manager_ctx_t* grad_manager;
    bool owns_grad_manager;           /**< Whether we own the grad manager */

    /* Statistics */
    amp_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Static Helper Functions
//=============================================================================

/**
 * @brief Check if array contains NaN or Inf
 */
static bool contains_nan_inf(const float* data, size_t count) {
    if (!data) {
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        if (isnan(data[i]) || isinf(data[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Count NaN and Inf values
 */
static uint64_t count_nan_inf(const float* data, size_t count) {
    if (!data) return 0;

    uint64_t bad_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (isnan(data[i]) || isinf(data[i])) {
            bad_count++;
        }
    }
    return bad_count;
}

/**
 * @brief Convert FP32 to FP16 (IEEE half precision)
 *
 * WHAT: Convert 32-bit float to 16-bit half float
 * WHY:  Reduce memory bandwidth and compute
 * HOW:  Truncate mantissa, clamp exponent
 */
static uint16_t fp32_to_fp16(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));

    uint32_t sign = (bits >> 16) & 0x8000;
    int32_t exponent = ((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mantissa = bits & 0x007FFFFF;

    if (exponent <= 0) {
        /* Underflow to zero */
        return (uint16_t)sign;
    } else if (exponent >= 31) {
        /* Overflow to infinity */
        return (uint16_t)(sign | 0x7C00);
    }

    return (uint16_t)(sign | (exponent << 10) | (mantissa >> 13));
}

/**
 * @brief Convert FP16 to FP32
 */
static float fp16_to_fp32(uint16_t value) {
    uint32_t sign = (value & 0x8000) << 16;
    uint32_t exponent = (value >> 10) & 0x1F;
    uint32_t mantissa = value & 0x03FF;

    uint32_t bits;
    if (exponent == 0) {
        /* Denormal or zero */
        bits = sign;
    } else if (exponent == 31) {
        /* Inf or NaN */
        bits = sign | 0x7F800000 | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }

    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

//=============================================================================
// Configuration API
//=============================================================================

int amp_default_config(amp_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    memset(config, 0, sizeof(*config));

    /* Loss scaling defaults */
    config->scaling.mode = AMP_SCALING_DYNAMIC;
    config->scaling.init_scale = AMP_DEFAULT_INIT_SCALE;
    config->scaling.growth_factor = AMP_DEFAULT_GROWTH_FACTOR;
    config->scaling.backoff_factor = AMP_DEFAULT_BACKOFF_FACTOR;
    config->scaling.growth_interval = AMP_DEFAULT_GROWTH_INTERVAL;
    config->scaling.min_scale = AMP_MIN_SCALE;
    config->scaling.max_scale = AMP_MAX_SCALE;
    config->scaling.overflow_strategy = AMP_OVERFLOW_SKIP;

    /* Autocast defaults */
    config->autocast.compute_dtype = AMP_DTYPE_FP16;
    config->autocast.storage_dtype = AMP_DTYPE_FP32;
    config->autocast.enabled = true;
    config->autocast.cache_enabled = true;

    /* Set default dtypes per category */
    config->autocast.category_dtypes[AMP_OP_CATEGORY_COMPUTE] = AMP_DTYPE_FP16;
    config->autocast.category_dtypes[AMP_OP_CATEGORY_REDUCE] = AMP_DTYPE_FP32;
    config->autocast.category_dtypes[AMP_OP_CATEGORY_NORMALIZE] = AMP_DTYPE_FP32;
    config->autocast.category_dtypes[AMP_OP_CATEGORY_LOSS] = AMP_DTYPE_FP32;
    config->autocast.category_dtypes[AMP_OP_CATEGORY_PROMOTE] = AMP_DTYPE_FP32;
    config->autocast.category_dtypes[AMP_OP_CATEGORY_PRESERVE] = AMP_DTYPE_FP32;

    /* Master weights defaults */
    config->master_weights.use_master_weights = true;
    config->master_weights.lazy_cast = false;
    config->master_weights.fuse_update_cast = true;

    /* Integration */
    config->integrate_gradient_manager = true;
    config->verbose = false;
    config->track_statistics = true;

    return 0;
}

int amp_bf16_config(amp_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Start with default config */
    int ret = amp_default_config(config);
    if (ret < 0) return ret;

    /* BF16-specific overrides */
    config->autocast.compute_dtype = AMP_DTYPE_BF16;

    /* BF16 typically doesn't need loss scaling */
    config->scaling.mode = AMP_SCALING_NONE;

    /* Update category dtypes for BF16 */
    config->autocast.category_dtypes[AMP_OP_CATEGORY_COMPUTE] = AMP_DTYPE_BF16;

    return 0;
}

int amp_validate_config(const amp_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Validate scaling config */
    if (config->scaling.mode != AMP_SCALING_NONE) {
        if (config->scaling.init_scale <= 0) {
            return -1;
        }
        if (config->scaling.min_scale <= 0) {
            return -1;
        }
        if (config->scaling.max_scale <= config->scaling.min_scale) {
            return -1;
        }
        if (config->scaling.growth_factor <= 1.0f) {
            return -1;
        }
        if (config->scaling.backoff_factor <= 0 ||
            config->scaling.backoff_factor >= 1.0f) return -1;
    }

    /* Validate dtype */
    if (config->autocast.compute_dtype >= AMP_DTYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "amp_validate_config: capacity exceeded");
        return -1;
    }
    if (config->autocast.storage_dtype >= AMP_DTYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "amp_validate_config: capacity exceeded");
        return -1;
    }

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

amp_ctx_t* amp_create(const amp_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "amp_create: config is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    if (amp_validate_config(config) < 0) {
        NIMCP_THROW(NIMCP_ERROR_CONFIG_INVALID, "amp_create: config validation failed");
        return NULL;
    }

    amp_ctx_t* ctx = nimcp_malloc(sizeof(amp_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(amp_ctx_t),
                          "amp_create: failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
    memcpy(&ctx->config, config, sizeof(amp_config_t));

    /* Initialize scaling state */
    ctx->current_scale = config->scaling.init_scale;
    ctx->steps_since_last_overflow = 0;
    ctx->last_step_overflowed = false;

    /* Initialize autocast state */
    ctx->autocast_enabled = false;
    ctx->autocast_depth = 0;

    /* Create mutex for thread safety */
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;
    ctx->mutex = nimcp_mutex_create(&attr);
    if (!ctx->mutex) {
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0,
                             "amp_create: failed to create mutex");
        nimcp_free(ctx);
        return NULL;
    }

    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.current_scale = ctx->current_scale;
    ctx->stats.min_scale_reached = ctx->current_scale;
    ctx->stats.max_scale_reached = ctx->current_scale;

    return ctx;
}

void amp_destroy(amp_ctx_t* ctx) {
    if (!ctx) return;

    /* Cleanup gradient manager if owned */
    if (ctx->owns_grad_manager && ctx->grad_manager) {
        nimcp_gradient_manager_destroy(ctx->grad_manager);
    }

    /* Cleanup mutex */
    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

int amp_connect_gradient_manager(amp_ctx_t* ctx,
                                  nimcp_gradient_manager_ctx_t* grad_manager) {
    if (!ctx || !grad_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amp_destroy: required parameter is NULL (ctx, grad_manager)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->grad_manager = grad_manager;
    ctx->owns_grad_manager = false;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

//=============================================================================
// Autocast API
//=============================================================================

int amp_autocast_enter(amp_ctx_t* ctx) {
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;

    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->autocast_enabled = true;
    ctx->autocast_depth++;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int amp_autocast_exit(amp_ctx_t* ctx) {
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;

    }

    nimcp_mutex_lock(ctx->mutex);
    if (ctx->autocast_depth > 0) {
        ctx->autocast_depth--;
        if (ctx->autocast_depth == 0) {
            ctx->autocast_enabled = false;
        }
    }
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

bool amp_is_autocasting(const amp_ctx_t* ctx) {
    if (!ctx) {
        return false;
    }
    nimcp_mutex_lock(((amp_ctx_t*)ctx)->mutex);
    bool result = ctx->autocast_enabled && ctx->config.autocast.enabled;
    nimcp_mutex_unlock(((amp_ctx_t*)ctx)->mutex);
    return result;
}

amp_dtype_t amp_get_op_dtype(const amp_ctx_t* ctx, amp_op_category_t category) {
    if (!ctx) return AMP_DTYPE_FP32;
    if (category >= AMP_OP_CATEGORY_COUNT) return AMP_DTYPE_FP32;

    if (!amp_is_autocasting(ctx)) {
        return AMP_DTYPE_FP32;
    }

    return ctx->config.autocast.category_dtypes[category];
}

nimcp_tensor_t* amp_autocast_tensor(amp_ctx_t* ctx,
                                     nimcp_tensor_t* tensor,
                                     amp_op_category_t category) {
    if (!ctx || !tensor) return tensor;

    if (!amp_is_autocasting(ctx)) {
        return tensor;
    }

    amp_dtype_t target_dtype = amp_get_op_dtype(ctx, category);

    /* If already correct dtype, return as-is */
    /* Note: Would need tensor dtype tracking for full implementation */

    return tensor;  /* Placeholder - full implementation would cast */
}

//=============================================================================
// Loss Scaling API
//=============================================================================

float amp_scale_loss(amp_ctx_t* ctx, float loss) {
    if (!ctx) return loss;

    if (ctx->config.scaling.mode == AMP_SCALING_NONE) {
        return loss;
    }

    return loss * ctx->current_scale;
}

bool amp_unscale_gradients(amp_ctx_t* ctx, float* gradients, size_t count) {
    if (!ctx || !gradients || count == 0) return true;

    if (ctx->config.scaling.mode == AMP_SCALING_NONE) {
        return true;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Check for overflow before unscaling */
    bool has_overflow = contains_nan_inf(gradients, count);

    if (has_overflow) {
        ctx->stats.overflow_count++;
        ctx->last_step_overflowed = true;
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "amp_unscale_gradients: validation failed");
        return false;
    }

    /* Unscale gradients */
    float inv_scale = 1.0f / ctx->current_scale;
    for (size_t i = 0; i < count; i++) {
        gradients[i] *= inv_scale;
    }

    /* Check again after unscaling */
    has_overflow = contains_nan_inf(gradients, count);

    if (has_overflow) {
        ctx->stats.underflow_count++;
        ctx->last_step_overflowed = true;
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "amp_unscale_gradients: validation failed");
        return false;
    }

    ctx->last_step_overflowed = false;
    nimcp_mutex_unlock(ctx->mutex);
    return true;
}

bool amp_unscale_tensor(amp_ctx_t* ctx, nimcp_tensor_t* grad_tensor) {
    if (!ctx || !grad_tensor) return true;

    /* Get tensor data and count */
    float* data = (float*)nimcp_tensor_data(grad_tensor);
    size_t count = nimcp_tensor_numel(grad_tensor);

    return amp_unscale_gradients(ctx, data, count);
}

void amp_update_scale(amp_ctx_t* ctx, bool gradients_valid) {
    if (!ctx) return;

    if (ctx->config.scaling.mode != AMP_SCALING_DYNAMIC) {
        return;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (!gradients_valid) {
        /* Overflow occurred - reduce scale */
        ctx->current_scale *= ctx->config.scaling.backoff_factor;
        if (ctx->current_scale < ctx->config.scaling.min_scale) {
            ctx->current_scale = ctx->config.scaling.min_scale;
        }
        ctx->steps_since_last_overflow = 0;
        ctx->stats.scale_decreases++;

        if (ctx->current_scale < ctx->stats.min_scale_reached) {
            ctx->stats.min_scale_reached = ctx->current_scale;
        }

        if (ctx->config.verbose) {
            /* Log scale decrease */
        }
    } else {
        /* No overflow - potentially increase scale */
        ctx->steps_since_last_overflow++;

        if (ctx->steps_since_last_overflow >= ctx->config.scaling.growth_interval) {
            ctx->current_scale *= ctx->config.scaling.growth_factor;
            if (ctx->current_scale > ctx->config.scaling.max_scale) {
                ctx->current_scale = ctx->config.scaling.max_scale;
            }
            ctx->steps_since_last_overflow = 0;
            ctx->stats.scale_increases++;

            if (ctx->current_scale > ctx->stats.max_scale_reached) {
                ctx->stats.max_scale_reached = ctx->current_scale;
            }
        }
    }

    ctx->stats.current_scale = ctx->current_scale;
    nimcp_mutex_unlock(ctx->mutex);
}

float amp_get_scale(const amp_ctx_t* ctx) {
    if (!ctx) return 1.0f;
    nimcp_mutex_lock(((amp_ctx_t*)ctx)->mutex);
    float scale = ctx->current_scale;
    nimcp_mutex_unlock(((amp_ctx_t*)ctx)->mutex);
    return scale;
}

int amp_set_scale(amp_ctx_t* ctx, float scale) {
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return -1;

    }

    if (scale < ctx->config.scaling.min_scale ||
        scale > ctx->config.scaling.max_scale) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "amp_set_scale: operation failed");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->current_scale = scale;
    ctx->stats.current_scale = scale;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

//=============================================================================
// Master Weights API
//=============================================================================

float* amp_create_master_weights(amp_ctx_t* ctx,
                                  const void* weights,
                                  size_t count,
                                  amp_dtype_t weight_dtype) {
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "amp_create_master_weights: ctx is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }
    if (!weights) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "amp_create_master_weights: weights is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "weights is NULL");

        return NULL;
    }
    if (count == 0) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "amp_create_master_weights: count is 0");
        return NULL;
    }

    float* master = nimcp_malloc(count * sizeof(float));
    if (!master) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, count * sizeof(float),
                          "amp_create_master_weights: failed to allocate master weights");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "master is NULL");

        return NULL;
    }

    /* Convert to FP32 master weights */
    switch (weight_dtype) {
        case AMP_DTYPE_FP32:
            memcpy(master, weights, count * sizeof(float));
            break;

        case AMP_DTYPE_FP16: {
            const uint16_t* fp16_weights = (const uint16_t*)weights;
            for (size_t i = 0; i < count; i++) {
                master[i] = fp16_to_fp32(fp16_weights[i]);
            }
            break;
        }

        case AMP_DTYPE_BF16: {
            /* BF16: upper 16 bits of IEEE 754 float — expand by shifting left 16 bits */
            const uint16_t* bf16_weights = (const uint16_t*)weights;
            for (size_t i = 0; i < count; i++) {
                uint32_t bits = (uint32_t)bf16_weights[i] << 16;
                memcpy(&master[i], &bits, sizeof(float));
            }
            break;
        }

        default:
            nimcp_free(master);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amp_set_scale: operation failed");
            return NULL;
    }

    return master;
}

int amp_update_master_weights(amp_ctx_t* ctx,
                               float* master_weights,
                               void* compute_weights,
                               const float* gradients,
                               size_t count,
                               float learning_rate) {
    if (!ctx || !master_weights || !compute_weights || !gradients) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amp_set_scale: required parameter is NULL (ctx, master_weights, compute_weights, gradients)");
        return -1;
    }

    /* Update master weights (FP32) */
    for (size_t i = 0; i < count; i++) {
        master_weights[i] -= learning_rate * gradients[i];
    }

    /* Cast back to compute dtype */
    amp_dtype_t compute_dtype = ctx->config.autocast.compute_dtype;

    switch (compute_dtype) {
        case AMP_DTYPE_FP32:
            memcpy(compute_weights, master_weights, count * sizeof(float));
            break;

        case AMP_DTYPE_FP16: {
            uint16_t* fp16_weights = (uint16_t*)compute_weights;
            for (size_t i = 0; i < count; i++) {
                fp16_weights[i] = fp32_to_fp16(master_weights[i]);
            }
            break;
        }

        case AMP_DTYPE_BF16: {
            /* FP32 → BF16: extract upper 16 bits of each float */
            uint16_t* bf16_weights = (uint16_t*)compute_weights;
            for (size_t i = 0; i < count; i++) {
                uint32_t bits;
                memcpy(&bits, &master_weights[i], sizeof(uint32_t));
                bf16_weights[i] = (uint16_t)(bits >> 16);
            }
            break;
        }

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "amp_set_scale: operation failed");
            return -1;
    }

    return 0;
}

int amp_sync_compute_weights(amp_ctx_t* ctx,
                              const float* master_weights,
                              void* compute_weights,
                              size_t count,
                              amp_dtype_t compute_dtype) {
    if (!ctx || !master_weights || !compute_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amp_set_scale: required parameter is NULL (ctx, master_weights, compute_weights)");
        return -1;
    }

    switch (compute_dtype) {
        case AMP_DTYPE_FP32:
            memcpy(compute_weights, master_weights, count * sizeof(float));
            break;

        case AMP_DTYPE_FP16: {
            uint16_t* fp16_weights = (uint16_t*)compute_weights;
            for (size_t i = 0; i < count; i++) {
                fp16_weights[i] = fp32_to_fp16(master_weights[i]);
            }
            break;
        }

        case AMP_DTYPE_BF16: {
            /* FP32 → BF16: extract upper 16 bits of each float */
            uint16_t* bf16_weights = (uint16_t*)compute_weights;
            for (size_t i = 0; i < count; i++) {
                uint32_t bits;
                memcpy(&bits, &master_weights[i], sizeof(uint32_t));
                bf16_weights[i] = (uint16_t)(bits >> 16);
            }
            break;
        }

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "amp_set_scale: operation failed");
            return -1;
    }

    return 0;
}

//=============================================================================
// Training Step API
//=============================================================================

int amp_step(amp_ctx_t* ctx,
             float loss,
             float* gradients,
             float* params,
             size_t count,
             float learning_rate,
             bool* step_performed) {
    if (!ctx || !gradients || !params || !step_performed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amp_set_scale: required parameter is NULL (ctx, gradients, params, step_performed)");
        return -1;
    }

    *step_performed = false;

    nimcp_mutex_lock(ctx->mutex);
    ctx->stats.total_steps++;
    nimcp_mutex_unlock(ctx->mutex);

    /* Scale loss (already computed outside, this is for tracking) */

    /* Unscale gradients */
    bool gradients_valid = amp_unscale_gradients(ctx, gradients, count);

    if (!gradients_valid) {
        /* Skip this step due to overflow */
        nimcp_mutex_lock(ctx->mutex);
        ctx->stats.skipped_steps++;
        nimcp_mutex_unlock(ctx->mutex);

        amp_update_scale(ctx, false);
        return 0;
    }

    /* Apply gradient update (simple SGD for demonstration) */
    for (size_t i = 0; i < count; i++) {
        params[i] -= learning_rate * gradients[i];
    }

    *step_performed = true;

    /* Update scale */
    amp_update_scale(ctx, true);

    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

int amp_get_stats(const amp_ctx_t* ctx, amp_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amp_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    memcpy(stats, &ctx->stats, sizeof(amp_stats_t));
    return 0;
}

void amp_reset_stats(amp_ctx_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(amp_stats_t));
    ctx->stats.current_scale = ctx->current_scale;
    ctx->stats.min_scale_reached = ctx->current_scale;
    ctx->stats.max_scale_reached = ctx->current_scale;
    nimcp_mutex_unlock(ctx->mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* amp_dtype_name(amp_dtype_t dtype) {
    switch (dtype) {
        case AMP_DTYPE_FP32:     return "FP32";
        case AMP_DTYPE_FP16:     return "FP16";
        case AMP_DTYPE_BF16:     return "BF16";
        case AMP_DTYPE_TF32:     return "TF32";
        case AMP_DTYPE_FP8_E4M3: return "FP8_E4M3";
        case AMP_DTYPE_FP8_E5M2: return "FP8_E5M2";
        default:                 return "Unknown";
    }
}

size_t amp_dtype_size(amp_dtype_t dtype) {
    switch (dtype) {
        case AMP_DTYPE_FP32:     return 4;
        case AMP_DTYPE_FP16:     return 2;
        case AMP_DTYPE_BF16:     return 2;
        case AMP_DTYPE_TF32:     return 4;  /* Stored as FP32 */
        case AMP_DTYPE_FP8_E4M3: return 1;
        case AMP_DTYPE_FP8_E5M2: return 1;
        default:                 return 4;
    }
}

bool amp_dtype_supported(amp_dtype_t dtype) {
    /* For now, support FP32, FP16, and BF16 */
    switch (dtype) {
        case AMP_DTYPE_FP32:
        case AMP_DTYPE_FP16:
        case AMP_DTYPE_BF16:
            return true;
        default:
            return false;
    }
}

bool amp_bf16_available(void) {
    /* Check for BF16 support */
    /* This would check for AVX-512 BF16 or Ampere+ GPU */
#if defined(__AVX512BF16__)
    return true;
#else
    /* Could also check GPU capabilities at runtime */
    return false;
#endif
}

int amp_cast(const void* src,
             amp_dtype_t src_dtype,
             void* dst,
             amp_dtype_t dst_dtype,
             size_t count) {
    if (!src || !dst || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "amp_bf16_available: required parameter is NULL (src, dst)");
        return -1;
    }

    /* Same dtype, just copy */
    if (src_dtype == dst_dtype) {
        size_t size = amp_dtype_size(src_dtype);
        memcpy(dst, src, count * size);
        return 0;
    }

    /* FP32 -> FP16 */
    if (src_dtype == AMP_DTYPE_FP32 && dst_dtype == AMP_DTYPE_FP16) {
        const float* src_f32 = (const float*)src;
        uint16_t* dst_f16 = (uint16_t*)dst;
        for (size_t i = 0; i < count; i++) {
            dst_f16[i] = fp32_to_fp16(src_f32[i]);
        }
        return 0;
    }

    /* FP16 -> FP32 */
    if (src_dtype == AMP_DTYPE_FP16 && dst_dtype == AMP_DTYPE_FP32) {
        const uint16_t* src_f16 = (const uint16_t*)src;
        float* dst_f32 = (float*)dst;
        for (size_t i = 0; i < count; i++) {
            dst_f32[i] = fp16_to_fp32(src_f16[i]);
        }
        return 0;
    }

    /* Other conversions not implemented */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "amp_bf16_available: operation failed");
    return -1;
}
