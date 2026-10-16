/**
 * @file nimcp_gradient_manager.c
 * @brief Gradient Management Module Implementation
 *
 * WHAT: Gradient management with tensor-accelerated operations
 * WHY:  Efficient gradient accumulation, scaling, and health checking
 * HOW:  Uses nimcp_tensor library for vectorized operations
 *
 * Implements:
 * - Gradient accumulation for large batch training
 * - Gradient scaling for mixed precision
 * - Health checking (NaN/Inf detection)
 * - Gradient statistics and monitoring
 * - Tensor-accelerated norm computations
 *
 * @note Part of Phase TM-6: Gradient Management
 * @version 1.1.0 - Added tensor library integration
 */

#include "middleware/training/nimcp_gradient_manager.h"
#include "constants/nimcp_constants.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/platform/nimcp_platform_mutex.h"  /* P0 fix: Thread-safe accum buffer */
#include "api/nimcp_api_exception.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdatomic.h>

#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#define GRADMGR_MODULE_NAME "GradientManager"
#define LOG_MODULE "gradient_manager"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gradient_manager)

/* ============================================================================
 * Internal Context Structure
 * ============================================================================ */

struct nimcp_gradient_manager_ctx {
    nimcp_gradient_manager_config_t config;

    /* Accumulation state */
    float* accum_buffer;          /**< Accumulated gradients */
    size_t accum_buffer_size;     /**< Buffer size */
    uint32_t accum_step;          /**< Current accumulation step */
    bool accum_initialized;       /**< Buffer initialized */

    /* P0 fix: Thread safety for accum_buffer reallocation */
    nimcp_platform_mutex_t accum_mutex;  /**< Protects accum_buffer reallocation */
    bool accum_mutex_initialized;        /**< Whether mutex was initialized */

    /* Scaling state */
    float current_scale;          /**< Current scale factor */
    uint32_t steps_since_growth;  /**< Steps since last scale growth */
    bool found_inf;               /**< Infinity found in current step */

    /* Statistics */
    nimcp_grad_stats_t stats;

    bool initialized;

    /* Shutdown flag for safe destroy (H12 race fix) */
    _Atomic bool shutting_down;

    /* Security registration */
    nimcp_sec_integration_t* security_ctx;
    bool security_registered;
    uint32_t security_module_id;
};

/* ============================================================================
 * Default Configurations
 * ============================================================================ */

nimcp_grad_accum_config_t nimcp_grad_accum_default_config(uint32_t accumulation_steps) {
    nimcp_grad_accum_config_t config = {
        .accumulation_steps = (accumulation_steps > 0 && accumulation_steps <= NIMCP_GRAD_MAX_ACCUM_STEPS)
                               ? accumulation_steps : 1,
        .mode = NIMCP_GRAD_ACCUM_SUM,
        .sync_on_step = false
    };
    return config;
}

nimcp_grad_scale_config_t nimcp_grad_scale_default_config(float initial_scale) {
    nimcp_grad_scale_config_t config = {
        .strategy = NIMCP_GRAD_SCALE_DYNAMIC,
        .initial_scale = initial_scale > 0.0F ? initial_scale : 65536.0F,
        .min_scale = NIMCP_GRAD_MIN_SCALE,
        .max_scale = NIMCP_GRAD_MAX_SCALE,
        .backoff_factor = NIMCP_GRAD_BACKOFF_FACTOR,
        .growth_factor = NIMCP_GRAD_GROWTH_FACTOR,
        .growth_interval = NIMCP_GRAD_GROWTH_INTERVAL
    };
    return config;
}

nimcp_gradient_manager_config_t nimcp_gradient_manager_default_config(void) {
    nimcp_gradient_manager_config_t config;
    memset(&config, 0, sizeof(config));

    config.accumulation = nimcp_grad_accum_default_config(1);
    config.use_accumulation = false;

    config.scaling = nimcp_grad_scale_default_config(65536.0F);
    config.use_scaling = false;

    config.check_nan_inf = true;
    config.skip_nan_gradients = true;
    config.replace_inf = true;
    config.max_grad_value = 1e6F;

    config.preallocate_buffers = false;
    config.max_buffer_size = 0;

    config.verbose = false;
    config.track_statistics = true;

    return config;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

nimcp_gradient_manager_ctx_t* nimcp_gradient_manager_create(
    const nimcp_gradient_manager_config_t* config
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gradient_manager_create: NULL config");
        return NULL;
    }

    if (nimcp_gradient_manager_validate_config(config) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gradient_manager_create: invalid config");
        return NULL;
    }

    nimcp_gradient_manager_ctx_t* ctx = (nimcp_gradient_manager_ctx_t*)nimcp_calloc(
        1, sizeof(nimcp_gradient_manager_ctx_t)
    );
    NIMCP_API_CHECK_ALLOC_SIZE(ctx, sizeof(nimcp_gradient_manager_ctx_t),
        "nimcp_gradient_manager_create: failed to allocate context");

    memcpy(&ctx->config, config, sizeof(nimcp_gradient_manager_config_t));

    /* Initialize scaling */
    ctx->current_scale = config->use_scaling ? config->scaling.initial_scale : 1.0F;
    ctx->steps_since_growth = 0;
    ctx->found_inf = false;

    /* Initialize accumulation buffer */
    ctx->accum_buffer = NULL;
    ctx->accum_buffer_size = 0;
    ctx->accum_step = 0;
    ctx->accum_initialized = false;

    if (config->preallocate_buffers && config->max_buffer_size > 0) {
        ctx->accum_buffer = (float*)nimcp_calloc(config->max_buffer_size, sizeof(float));
        if (ctx->accum_buffer) {
            ctx->accum_buffer_size = config->max_buffer_size;
            ctx->accum_initialized = true;
        }
    }

    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(nimcp_grad_stats_t));
    ctx->stats.min_grad_norm = FLT_MAX;
    ctx->stats.max_grad_norm = 0.0F;
    ctx->stats.current_scale = ctx->current_scale;

    ctx->initialized = true;
    atomic_store(&ctx->shutting_down, false);

    /* P0 fix: Initialize accum buffer mutex for thread safety */
    if (nimcp_platform_mutex_init(&ctx->accum_mutex, false) == 0) {
        ctx->accum_mutex_initialized = true;
    } else {
        ctx->accum_mutex_initialized = false;
        nimcp_log(LOG_LEVEL_WARN, "[%s] Failed to initialize accum mutex, continuing without thread safety",
                  LOG_MODULE);
    }

    /* Register with security module if available */
    ctx->security_ctx = config->security_ctx;
    ctx->security_registered = false;
    if (ctx->security_ctx) {
        nimcp_result_t err = nimcp_sec_register_module(
            ctx->security_ctx,
            GRADMGR_MODULE_NAME,
            NIMCP_SEC_CAT_MIDDLEWARE,
            &ctx->security_module_id
        );
        if (err == NIMCP_SUCCESS) {
            ctx->security_registered = true;
        }
    }

    return ctx;
}

void nimcp_gradient_manager_destroy(nimcp_gradient_manager_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* H12 race fix: Signal shutdown to all concurrent users FIRST.
     * Any thread checking shutting_down before acquiring the mutex
     * will bail out, preventing use-after-free on the mutex itself. */
    atomic_store(&ctx->shutting_down, true);

    /* Lock/unlock to drain any thread currently holding the mutex */
    if (ctx->accum_mutex_initialized) {
        nimcp_platform_mutex_lock(&ctx->accum_mutex);
        nimcp_platform_mutex_unlock(&ctx->accum_mutex);
    }

    /* Unregister from security module */
    if (ctx->security_registered && ctx->security_ctx) {
        nimcp_sec_unregister_module(ctx->security_ctx, ctx->security_module_id);
        ctx->security_registered = false;
    }

    if (ctx->accum_buffer) {
        nimcp_free(ctx->accum_buffer);
        ctx->accum_buffer = NULL;
    }

    /* Destroy mutex after all resources are freed */
    if (ctx->accum_mutex_initialized) {
        nimcp_platform_mutex_destroy(&ctx->accum_mutex);
        ctx->accum_mutex_initialized = false;
    }

    nimcp_free(ctx);
}

/* ============================================================================
 * Gradient Accumulation
 * ============================================================================ */

nimcp_result_t nimcp_gradient_accumulate(
    nimcp_gradient_manager_ctx_t* ctx,
    const float* gradients,
    size_t num_gradients
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(gradients != NULL, NIMCP_ERROR_INVALID_PARAM, "gradients is NULL");
    NIMCP_CHECK_THROW(num_gradients > 0, NIMCP_ERROR_INVALID_PARAM, "num_gradients must be > 0");
    NIMCP_CHECK_THROW(ctx->config.use_accumulation, NIMCP_ERROR_INVALID_PARAM,
        "accumulation not enabled in config");

    /* H12 race fix: Bail out if destroy is in progress */
    if (atomic_load(&ctx->shutting_down)) {
        return NIMCP_ERROR_INVALID;
    }

    /* P0 fix: Thread-safe accum buffer reallocation */
    if (!ctx->accum_initialized || ctx->accum_buffer_size < num_gradients) {
        if (ctx->accum_mutex_initialized) {
            nimcp_platform_mutex_lock(&ctx->accum_mutex);
        }
        /* Re-check condition after acquiring lock (TOCTOU fix) */
        if (!ctx->accum_initialized || ctx->accum_buffer_size < num_gradients) {
            if (ctx->accum_buffer) {
                nimcp_free(ctx->accum_buffer);
            }
            ctx->accum_buffer = (float*)nimcp_calloc(num_gradients, sizeof(float));
            if (!ctx->accum_buffer) {
                if (ctx->accum_mutex_initialized) {
                    nimcp_platform_mutex_unlock(&ctx->accum_mutex);
                }
                return NIMCP_ERROR_MEMORY;
            }
            ctx->accum_buffer_size = num_gradients;
            ctx->accum_initialized = true;
            ctx->accum_step = 0;
        }
        if (ctx->accum_mutex_initialized) {
            nimcp_platform_mutex_unlock(&ctx->accum_mutex);
        }
    }

    /* Reset buffer at start of accumulation */
    if (ctx->accum_step == 0) {
        memset(ctx->accum_buffer, 0, num_gradients * sizeof(float));
    }

    /* Check gradient health */
    if (ctx->config.check_nan_inf) {
        nimcp_grad_health_t health = nimcp_gradient_check_health(gradients, num_gradients);
        if (health == NIMCP_GRAD_HAS_NAN || health == NIMCP_GRAD_HAS_INF) {
            if (ctx->config.skip_nan_gradients) {
                ctx->stats.skipped_steps++;
                if (health == NIMCP_GRAD_HAS_NAN) ctx->stats.nan_count++;
                if (health == NIMCP_GRAD_HAS_INF) ctx->stats.inf_count++;
                return NIMCP_SUCCESS;  /* Skip this accumulation step */
            }
        }
    }

    /* Accumulate */
    for (size_t i = 0; i < num_gradients; i++) {
        ctx->accum_buffer[i] += gradients[i];
    }

    ctx->accum_step++;
    ctx->stats.total_accum_steps++;

    return NIMCP_SUCCESS;
}

bool nimcp_gradient_accum_ready(const nimcp_gradient_manager_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gradient_accum_ready: ctx is NULL");
        return false;  /* Invalid context = not ready */
    }
    if (!ctx->config.use_accumulation) {
        return true;  /* No accumulation = always ready */
    }
    return ctx->accum_step >= ctx->config.accumulation.accumulation_steps;
}

nimcp_result_t nimcp_gradient_get_accumulated(
    nimcp_gradient_manager_ctx_t* ctx,
    float* output,
    size_t num_gradients
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(output != NULL, NIMCP_ERROR_INVALID_PARAM, "output is NULL");
    NIMCP_CHECK_THROW(num_gradients > 0, NIMCP_ERROR_INVALID_PARAM, "num_gradients must be > 0");
    NIMCP_CHECK_THROW(ctx->config.use_accumulation, NIMCP_ERROR_INVALID_PARAM,
        "accumulation not enabled in config");
    NIMCP_CHECK_THROW(ctx->accum_initialized, NIMCP_ERROR_INVALID_PARAM,
        "accumulation buffer not initialized");
    NIMCP_CHECK_THROW(ctx->accum_buffer_size >= num_gradients, NIMCP_ERROR_INVALID_PARAM,
        "accum buffer size %zu < requested %zu", ctx->accum_buffer_size, num_gradients);

    /* Copy accumulated gradients */
    memcpy(output, ctx->accum_buffer, num_gradients * sizeof(float));

    /* Apply averaging if mode is MEAN */
    if (ctx->config.accumulation.mode == NIMCP_GRAD_ACCUM_MEAN && ctx->accum_step > 0) {
        float divisor = (float)ctx->accum_step;
        for (size_t i = 0; i < num_gradients; i++) {
            output[i] /= divisor;
        }
    }

    ctx->stats.total_steps++;

    return NIMCP_SUCCESS;
}

void nimcp_gradient_reset_accum(nimcp_gradient_manager_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    ctx->accum_step = 0;
    if (ctx->accum_buffer && ctx->accum_buffer_size > 0) {
        memset(ctx->accum_buffer, 0, ctx->accum_buffer_size * sizeof(float));
    }
}

uint32_t nimcp_gradient_get_accum_step(const nimcp_gradient_manager_ctx_t* ctx) {
    return ctx ? ctx->accum_step : 0;
}

/* ============================================================================
 * Gradient Scaling
 * ============================================================================ */

float nimcp_gradient_scale(
    nimcp_gradient_manager_ctx_t* ctx,
    float* gradients,
    size_t num_gradients
) {
    if (!ctx || !gradients || num_gradients == 0) {
        return 1.0F;
    }

    if (!ctx->config.use_scaling || ctx->config.scaling.strategy == NIMCP_GRAD_SCALE_NONE) {
        return 1.0F;
    }

    float scale = ctx->current_scale;

    for (size_t i = 0; i < num_gradients; i++) {
        gradients[i] *= scale;
    }

    return scale;
}

float nimcp_gradient_unscale(
    nimcp_gradient_manager_ctx_t* ctx,
    float* gradients,
    size_t num_gradients
) {
    if (!ctx || !gradients || num_gradients == 0) {
        return 1.0F;
    }

    if (!ctx->config.use_scaling || ctx->config.scaling.strategy == NIMCP_GRAD_SCALE_NONE) {
        return 1.0F;
    }

    float inv_scale = 1.0F / ctx->current_scale;

    for (size_t i = 0; i < num_gradients; i++) {
        gradients[i] *= inv_scale;
    }

    /* Check for overflow after unscaling */
    nimcp_grad_health_t health = nimcp_gradient_check_health(gradients, num_gradients);
    if (health == NIMCP_GRAD_HAS_INF || health == NIMCP_GRAD_HAS_NAN) {
        ctx->found_inf = true;
    }

    return inv_scale;
}

void nimcp_gradient_update_scale(
    nimcp_gradient_manager_ctx_t* ctx,
    nimcp_grad_health_t health
) {
    if (!ctx || !ctx->config.use_scaling) {
        return;
    }

    if (ctx->config.scaling.strategy != NIMCP_GRAD_SCALE_DYNAMIC) {
        return;
    }

    const nimcp_grad_scale_config_t* cfg = &ctx->config.scaling;

    if (health == NIMCP_GRAD_HAS_INF || health == NIMCP_GRAD_HAS_NAN || ctx->found_inf) {
        /* Reduce scale on overflow */
        ctx->current_scale *= cfg->backoff_factor;
        if (ctx->current_scale < cfg->min_scale) {
            ctx->current_scale = cfg->min_scale;
        }
        ctx->steps_since_growth = 0;
        ctx->found_inf = false;
        ctx->stats.scale_decreases++;
        ctx->stats.overflow_count++;
    } else {
        /* Try to grow scale */
        ctx->steps_since_growth++;
        if (ctx->steps_since_growth >= cfg->growth_interval) {
            ctx->current_scale *= cfg->growth_factor;
            if (ctx->current_scale > cfg->max_scale) {
                ctx->current_scale = cfg->max_scale;
            }
            ctx->steps_since_growth = 0;
            ctx->stats.scale_increases++;
        }
    }

    ctx->stats.current_scale = ctx->current_scale;
}

float nimcp_gradient_get_scale(const nimcp_gradient_manager_ctx_t* ctx) {
    return ctx ? ctx->current_scale : 1.0F;
}

nimcp_result_t nimcp_gradient_set_scale(
    nimcp_gradient_manager_ctx_t* ctx,
    float scale
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(scale >= NIMCP_GRAD_MIN_SCALE, NIMCP_ERROR_INVALID_PARAM,
        "scale %f < min %f", scale, NIMCP_GRAD_MIN_SCALE);
    NIMCP_CHECK_THROW(scale <= NIMCP_GRAD_MAX_SCALE, NIMCP_ERROR_INVALID_PARAM,
        "scale %f > max %f", scale, NIMCP_GRAD_MAX_SCALE);

    ctx->current_scale = scale;
    ctx->stats.current_scale = scale;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Health Checking
 * ============================================================================ */

nimcp_grad_health_t nimcp_gradient_check_health(
    const float* gradients,
    size_t num_gradients
) {
    if (!gradients || num_gradients == 0) {
        return NIMCP_GRAD_HEALTHY;
    }

    bool all_zero = true;

    for (size_t i = 0; i < num_gradients; i++) {
        if (isnan(gradients[i])) {
            return NIMCP_GRAD_HAS_NAN;
        }
        if (isinf(gradients[i])) {
            return NIMCP_GRAD_HAS_INF;
        }
        if (gradients[i] != 0.0F) {
            all_zero = false;
        }
    }

    if (all_zero) {
        return NIMCP_GRAD_HAS_ZERO;
    }

    return NIMCP_GRAD_HEALTHY;
}

nimcp_grad_health_t nimcp_gradient_check_health_ctx(
    nimcp_gradient_manager_ctx_t* ctx,
    const float* gradients,
    size_t num_gradients
) {
    nimcp_grad_health_t health = nimcp_gradient_check_health(gradients, num_gradients);

    if (ctx && ctx->config.track_statistics) {
        if (health == NIMCP_GRAD_HAS_NAN) ctx->stats.nan_count++;
        if (health == NIMCP_GRAD_HAS_INF) ctx->stats.inf_count++;
    }

    return health;
}

uint64_t nimcp_gradient_sanitize(
    float* gradients,
    size_t num_gradients,
    float replace_value
) {
    if (!gradients || num_gradients == 0) {
        return 0;
    }

    uint64_t replaced = 0;

    for (size_t i = 0; i < num_gradients; i++) {
        if (isnan(gradients[i]) || isinf(gradients[i])) {
            gradients[i] = replace_value;
            replaced++;
        }
    }

    return replaced;
}

/* ============================================================================
 * Gradient Statistics - Tensor-Accelerated
 * ============================================================================ */

/**
 * @brief Compute L2 norm using tensor operations
 *
 * WHAT: Euclidean norm of gradient vector
 * WHY:  Standard measure for gradient magnitude
 * HOW:  Uses nimcp_tensor_norm_p for vectorized computation
 */
float nimcp_gradient_l2_norm(const float* gradients, size_t num_gradients) {
    if (!gradients || num_gradients == 0) {
        return 0.0F;
    }

    /* Create tensor view of gradient data (no copy) */
    uint32_t dims[] = {(uint32_t)num_gradients};
    nimcp_tensor_t* t = nimcp_tensor_from_data(gradients, dims, 1, NIMCP_DTYPE_F32, false);
    if (!t) {
        /* Fallback to scalar computation */
        float sum_sq = 0.0F;
        for (size_t i = 0; i < num_gradients; i++) {
            sum_sq += gradients[i] * gradients[i];
        }
        return sqrtf(sum_sq);
    }

    float norm = (float)nimcp_tensor_norm_p(t, 2.0);
    nimcp_tensor_destroy(t);
    return norm;
}

/**
 * @brief Compute L1 norm using tensor operations
 *
 * WHAT: Sum of absolute values
 * WHY:  Sparsity-aware gradient measure
 * HOW:  Uses nimcp_tensor_norm_p(t, 1.0)
 */
float nimcp_gradient_l1_norm(const float* gradients, size_t num_gradients) {
    if (!gradients || num_gradients == 0) {
        return 0.0F;
    }

    /* Create tensor view of gradient data (no copy) */
    uint32_t dims[] = {(uint32_t)num_gradients};
    nimcp_tensor_t* t = nimcp_tensor_from_data(gradients, dims, 1, NIMCP_DTYPE_F32, false);
    if (!t) {
        /* Fallback to scalar computation */
        float sum = 0.0F;
        for (size_t i = 0; i < num_gradients; i++) {
            sum += fabsf(gradients[i]);
        }
        return sum;
    }

    float norm = (float)nimcp_tensor_norm_p(t, 1.0);
    nimcp_tensor_destroy(t);
    return norm;
}

/**
 * @brief Compute max norm (infinity norm) using tensor operations
 *
 * WHAT: Maximum absolute value
 * WHY:  Detect gradient spikes
 * HOW:  Uses nimcp_tensor_norm_p(t, INFINITY)
 */
float nimcp_gradient_max_norm(const float* gradients, size_t num_gradients) {
    if (!gradients || num_gradients == 0) {
        return 0.0F;
    }

    /* Create tensor view of gradient data (no copy) */
    uint32_t dims[] = {(uint32_t)num_gradients};
    nimcp_tensor_t* t = nimcp_tensor_from_data(gradients, dims, 1, NIMCP_DTYPE_F32, false);
    if (!t) {
        /* Fallback to scalar computation */
        float max_abs = 0.0F;
        for (size_t i = 0; i < num_gradients; i++) {
            float abs_val = fabsf(gradients[i]);
            if (abs_val > max_abs) {
                max_abs = abs_val;
            }
        }
        return max_abs;
    }

    /* Infinity norm = max absolute value */
    float norm = (float)nimcp_tensor_norm_p(t, INFINITY);
    nimcp_tensor_destroy(t);
    return norm;
}

nimcp_result_t nimcp_gradient_manager_get_stats(
    const nimcp_gradient_manager_ctx_t* ctx,
    nimcp_grad_stats_t* stats
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(stats != NULL, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    memcpy(stats, &ctx->stats, sizeof(nimcp_grad_stats_t));

    /* Compute average */
    if (ctx->stats.total_steps > 0) {
        stats->avg_grad_norm = (float)(ctx->stats.sum_grad_norm / (double)ctx->stats.total_steps);
    }

    return NIMCP_SUCCESS;
}

void nimcp_gradient_manager_reset_stats(nimcp_gradient_manager_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    memset(&ctx->stats, 0, sizeof(nimcp_grad_stats_t));
    ctx->stats.min_grad_norm = FLT_MAX;
    ctx->stats.current_scale = ctx->current_scale;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* nimcp_grad_accum_mode_name(nimcp_grad_accum_mode_t mode) {
    static const char* names[] = {
        "Sum",
        "Mean"
    };

    if (mode >= NIMCP_GRAD_ACCUM_MODE_COUNT) {
        return "Unknown";
    }
    return names[mode];
}

const char* nimcp_grad_scale_strategy_name(nimcp_grad_scale_strategy_t strategy) {
    static const char* names[] = {
        "None",
        "Fixed",
        "Dynamic"
    };

    if (strategy >= NIMCP_GRAD_SCALE_STRATEGY_COUNT) {
        return "Unknown";
    }
    return names[strategy];
}

const char* nimcp_grad_health_name(nimcp_grad_health_t health) {
    static const char* names[] = {
        "Healthy",
        "Has NaN",
        "Has Inf",
        "All Zero",
        "Overflow",
        "Underflow"
    };

    if (health > NIMCP_GRAD_UNDERFLOW) {
        return "Unknown";
    }
    return names[health];
}

nimcp_result_t nimcp_gradient_manager_validate_config(
    const nimcp_gradient_manager_config_t* config
) {
    NIMCP_CHECK_THROW(config != NULL, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    /* Validate accumulation config */
    if (config->use_accumulation) {
        if (config->accumulation.accumulation_steps == 0 ||
            config->accumulation.accumulation_steps > NIMCP_GRAD_MAX_ACCUM_STEPS) {
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->accumulation.mode >= NIMCP_GRAD_ACCUM_MODE_COUNT) {
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate scaling config */
    if (config->use_scaling) {
        if (config->scaling.strategy >= NIMCP_GRAD_SCALE_STRATEGY_COUNT) {
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->scaling.strategy != NIMCP_GRAD_SCALE_NONE) {
            if (config->scaling.initial_scale <= 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (config->scaling.min_scale <= 0.0F ||
                config->scaling.max_scale <= config->scaling.min_scale) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (config->scaling.backoff_factor <= 0.0F ||
                config->scaling.backoff_factor >= 1.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (config->scaling.growth_factor <= 1.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Distributed Training Support
 * ============================================================================ */

void nimcp_gradient_prepare_allreduce(
    float* gradients,
    size_t num_gradients,
    uint32_t num_workers
) {
    /* For all-reduce averaging, no pre-processing needed */
    /* This is a placeholder for potential future optimizations */
    (void)gradients;
    (void)num_gradients;
    (void)num_workers;
}

/**
 * @brief Finalize all-reduce using tensor operations
 *
 * WHAT: Average gradients after all-reduce sum
 * WHY:  Distributed training synchronization
 * HOW:  Uses tensor scalar multiplication for efficiency
 */
void nimcp_gradient_finalize_allreduce(
    float* gradients,
    size_t num_gradients,
    uint32_t num_workers
) {
    if (!gradients || num_gradients == 0 || num_workers == 0) {
        return;
    }

    /* Use tensor operations for averaging */
    uint32_t dims[] = {(uint32_t)num_gradients};
    nimcp_tensor_t* t = nimcp_tensor_from_data(gradients, dims, 1, NIMCP_DTYPE_F32, false);
    if (t) {
        nimcp_tensor_mul_scalar_(t, 1.0 / (double)num_workers);
        nimcp_tensor_destroy(t);
    } else {
        /* Fallback to scalar computation */
        float divisor = (float)num_workers;
        for (size_t i = 0; i < num_gradients; i++) {
            gradients[i] /= divisor;
        }
    }
}

/* ============================================================================
 * Tensor-Based Gradient Operations
 * ============================================================================ */

/**
 * @brief Accumulate gradients using tensor operations
 *
 * WHAT: Add gradients to accumulation buffer using tensors
 * WHY:  More efficient for large gradient arrays
 * HOW:  Uses nimcp_tensor_add_ for in-place accumulation
 */
nimcp_result_t nimcp_gradient_accumulate_tensor(
    nimcp_gradient_manager_ctx_t* ctx,
    const nimcp_tensor_t* grad_tensor
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(grad_tensor != NULL, NIMCP_ERROR_INVALID_PARAM, "grad_tensor is NULL");
    NIMCP_CHECK_THROW(ctx->config.use_accumulation, NIMCP_ERROR_INVALID_PARAM,
        "accumulation not enabled in config");

    /* H12 race fix: Bail out if destroy is in progress */
    if (atomic_load(&ctx->shutting_down)) {
        return NIMCP_ERROR_INVALID;
    }

    size_t num_gradients = nimcp_tensor_numel(grad_tensor);
    NIMCP_CHECK_THROW(num_gradients > 0, NIMCP_ERROR_INVALID_PARAM, "tensor has 0 elements");

    /* P0 fix: Thread-safe accum buffer reallocation */
    if (!ctx->accum_initialized || ctx->accum_buffer_size < num_gradients) {
        if (ctx->accum_mutex_initialized) {
            nimcp_platform_mutex_lock(&ctx->accum_mutex);
        }
        /* Re-check condition after acquiring lock (TOCTOU fix) */
        if (!ctx->accum_initialized || ctx->accum_buffer_size < num_gradients) {
            if (ctx->accum_buffer) {
                nimcp_free(ctx->accum_buffer);
            }
            ctx->accum_buffer = (float*)nimcp_calloc(num_gradients, sizeof(float));
            if (!ctx->accum_buffer) {
                if (ctx->accum_mutex_initialized) {
                    nimcp_platform_mutex_unlock(&ctx->accum_mutex);
                }
                return NIMCP_ERROR_MEMORY;
            }
            ctx->accum_buffer_size = num_gradients;
            ctx->accum_initialized = true;
            ctx->accum_step = 0;
        }
        if (ctx->accum_mutex_initialized) {
            nimcp_platform_mutex_unlock(&ctx->accum_mutex);
        }
    }

    /* Reset buffer at start of accumulation */
    if (ctx->accum_step == 0) {
        memset(ctx->accum_buffer, 0, num_gradients * sizeof(float));
    }

    /* Create tensor view of accumulation buffer */
    uint32_t dims[] = {(uint32_t)num_gradients};
    nimcp_tensor_t* accum_tensor = nimcp_tensor_from_data(
        ctx->accum_buffer, dims, 1, NIMCP_DTYPE_F32, false
    );
    if (!accum_tensor) {
        return NIMCP_ERROR_MEMORY;
    }

    /* Accumulate using tensor in-place add */
    int result = nimcp_tensor_add_(accum_tensor, grad_tensor);
    nimcp_tensor_destroy(accum_tensor);

    if (result != NIMCP_TENSOR_OK) {
        return NIMCP_ERROR_INVALID;
    }

    ctx->accum_step++;
    ctx->stats.total_accum_steps++;

    return NIMCP_SUCCESS;
}

/**
 * @brief Scale gradients using tensor operations
 *
 * WHAT: Multiply gradient tensor by scale factor
 * WHY:  Mixed precision training, loss scaling
 * HOW:  Uses nimcp_tensor_mul_scalar_ for in-place scaling
 */
nimcp_result_t nimcp_gradient_scale_tensor(
    nimcp_gradient_manager_ctx_t* ctx,
    nimcp_tensor_t* grad_tensor
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(grad_tensor != NULL, NIMCP_ERROR_INVALID_PARAM, "grad_tensor is NULL");

    if (!ctx->config.use_scaling || ctx->config.scaling.strategy == NIMCP_GRAD_SCALE_NONE) {
        return NIMCP_SUCCESS;
    }

    int result = nimcp_tensor_mul_scalar_(grad_tensor, (double)ctx->current_scale);
    return (result == NIMCP_TENSOR_OK) ? NIMCP_SUCCESS : NIMCP_ERROR_INVALID;
}

/**
 * @brief Unscale gradients using tensor operations
 *
 * WHAT: Divide gradient tensor by scale factor
 * WHY:  Restore original gradient magnitude after loss scaling
 * HOW:  Uses nimcp_tensor_mul_scalar_ with inverse scale
 */
nimcp_result_t nimcp_gradient_unscale_tensor(
    nimcp_gradient_manager_ctx_t* ctx,
    nimcp_tensor_t* grad_tensor
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(grad_tensor != NULL, NIMCP_ERROR_INVALID_PARAM, "grad_tensor is NULL");

    if (!ctx->config.use_scaling || ctx->config.scaling.strategy == NIMCP_GRAD_SCALE_NONE) {
        return NIMCP_SUCCESS;
    }

    double inv_scale = 1.0 / (double)ctx->current_scale;
    int result = nimcp_tensor_mul_scalar_(grad_tensor, inv_scale);
    return (result == NIMCP_TENSOR_OK) ? NIMCP_SUCCESS : NIMCP_ERROR_INVALID;
}

/**
 * @brief Compute gradient norm using tensor operations
 *
 * WHAT: Compute Lp norm of gradient tensor
 * WHY:  Gradient clipping, health monitoring
 * HOW:  Uses nimcp_tensor_norm_p
 *
 * @param grad_tensor Input gradient tensor
 * @param p Norm order (1=L1, 2=L2, INFINITY=max)
 * @return Computed norm value
 */
float nimcp_gradient_tensor_norm(
    const nimcp_tensor_t* grad_tensor,
    double p
) {
    if (!grad_tensor) {
        return 0.0F;
    }
    return (float)nimcp_tensor_norm_p(grad_tensor, p);
}

/**
 * @brief Clip gradients by norm using tensor operations
 *
 * WHAT: Scale down gradients if norm exceeds threshold
 * WHY:  Prevent exploding gradients
 * HOW:  Compute norm, scale if exceeds max_norm
 *
 * @param grad_tensor Gradient tensor (modified in place)
 * @param max_norm Maximum allowed norm
 * @param norm_type Norm type (1, 2, or INFINITY)
 * @return Actual norm before clipping
 */
float nimcp_gradient_clip_norm_tensor(
    nimcp_tensor_t* grad_tensor,
    float max_norm,
    double norm_type
) {
    if (!grad_tensor || max_norm <= 0.0F) {
        return 0.0F;
    }

    /* Compute current norm */
    float current_norm = (float)nimcp_tensor_norm_p(grad_tensor, norm_type);
    if (current_norm <= max_norm) {
        return current_norm;  /* No clipping needed */
    }

    /* Scale down gradients */
    double scale = (double)max_norm / (double)current_norm;
    nimcp_tensor_mul_scalar_(grad_tensor, scale);

    return current_norm;
}
