/**
 * @file nimcp_optimizers.c
 * @brief Implementation of Optimizers Module for NIMCP Training Pipeline
 *
 * WHAT: Neural network optimizers (SGD, Adam, RMSprop, etc.)
 * WHY:  Enable gradient-based parameter updates for learning
 * HOW:  Momentum, adaptive learning rates, tensor-accelerated operations
 *
 * Phase TM-2: Training Pipeline Infrastructure
 * Phase BIO-1: Bio-async integration
 * Phase LOG-1: Logging integration
 * Phase SEC-1: Security validation
 * Phase TENSOR-2: Tensor library integration
 *
 * @version 1.1.0 - Added tensor library integration for accelerated operations
 */

#include "middleware/training/nimcp_optimizers.h"
#include "constants/nimcp_constants.h"
#include "utils/tensor/nimcp_tensor.h"  /* Tensor library for vectorized operations */
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"  /* P0 fix: Thread-safe state buffer */
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/geometry/nimcp_differential_geometry.h"

/* Module name for security registration and logging */
#define OPTIMIZER_MODULE_NAME "nimcp_optimizer"
#define LOG_MODULE "optimizers"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(optimizers)

/* ============================================================================
 * KG-Driven Wiring Infrastructure
 * ============================================================================ */

/* Forward declarations for handlers */
static nimcp_error_t handle_optimizer_step_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);
static nimcp_error_t handle_gradient_computed(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/**
 * Handler map for optimizer module.
 * Handles training step requests and gradient computed events.
 */
DEFINE_HANDLER_MAP_BEGIN(optimizer)
    HANDLER_MAP_ENTRY(BIO_MSG_TRAINING_STEP_REQUEST, handle_optimizer_step_request)
    HANDLER_MAP_ENTRY(BIO_MSG_GRADIENT_COMPUTED, handle_gradient_computed)
DEFINE_HANDLER_MAP_END()

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(optimizer, nimcp_optimizer_context_t, ctx)

/* Numerical stability constants */
#define OPTIMIZER_BIAS_CORRECTION_MIN 1e-8F   /* Minimum bias correction to prevent division by zero */
#define OPTIMIZER_BIAS_CORRECTION_MAX_STEPS 10000  /* After this many steps, bias correction is ~1.0 */
#define OPTIMIZER_GRADIENT_MAX_VALUE 1e6F     /* Maximum gradient value before sanitization */
#define OPTIMIZER_PARAM_MAX_VALUE 1e9F        /* Maximum parameter value to prevent overflow */
#define OPTIMIZER_LR_MIN 1e-10F               /* Minimum learning rate */
#define OPTIMIZER_LR_MAX 10.0F                /* Maximum learning rate */

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal optimizer state for momentum-based optimizers
 */
typedef struct optimizer_momentum_state {
    float* velocity;              /**< Velocity buffer */
    size_t count;                 /**< Number of parameters */
} optimizer_momentum_state_t;

/**
 * @brief Internal optimizer state for Adam-style optimizers
 */
typedef struct optimizer_adam_state {
    float* m;                     /**< First moment estimate */
    float* v;                     /**< Second moment estimate */
    float* v_max;                 /**< Max second moment (AMSGrad) */
    size_t count;                 /**< Number of parameters */
    uint64_t t;                   /**< Timestep */
} optimizer_adam_state_t;

/**
 * @brief Internal optimizer state for RMSprop
 */
typedef struct optimizer_rmsprop_state {
    float* square_avg;            /**< Running average of squared gradients */
    float* momentum_buffer;       /**< Momentum buffer */
    float* grad_avg;              /**< Running average of gradients (centered) */
    size_t count;                 /**< Number of parameters */
} optimizer_rmsprop_state_t;

/**
 * @brief Internal optimizer state for AdaGrad
 */
typedef struct optimizer_adagrad_state {
    float* sum;                   /**< Sum of squared gradients */
    size_t count;                 /**< Number of parameters */
    uint64_t step;                /**< Step count for LR decay */
} optimizer_adagrad_state_t;

/**
 * @brief Internal optimizer context
 */
struct nimcp_optimizer_context {
    nimcp_optimizer_config_t config;
    nimcp_sec_integration_t* security_ctx;
    unified_mem_manager_t memory_mgr;
    nimcp_optimizer_stats_t stats;

    bool initialized;
    bool security_registered;
    uint32_t module_id;

    /* Current learning rate (may differ from config due to scheduling) */
    float current_lr;
    uint64_t step_count;

    /* Optimizer-specific state */
    union {
        optimizer_momentum_state_t momentum;
        optimizer_adam_state_t adam;
        optimizer_rmsprop_state_t rmsprop;
        optimizer_adagrad_state_t adagrad;
        void* custom;
    } state;

    size_t state_size;

    /* Thread safety for state buffer reallocation (P0 fix) */
    nimcp_platform_mutex_t state_mutex;  /**< Protects state buffer reallocation */
    bool state_mutex_initialized;        /**< Whether mutex was initialized */

    /* Bio-async integration (Phase BIO-1) */
    bio_module_context_t bio_ctx;      /**< Bio-async module context */
    bool bio_async_enabled;            /**< Bio-async availability flag */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static float* alloc_buffer(nimcp_optimizer_context_t* ctx, size_t count) {
    (void)ctx;  /* Reserved for future unified memory integration */
    size_t bytes = count * sizeof(float);
    return (float*)nimcp_malloc(bytes);
}

static void free_buffer(nimcp_optimizer_context_t* ctx, float* buffer) {
    (void)ctx;
    if (buffer) {
        nimcp_free(buffer);
    }
}

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void update_gradient_stats(nimcp_optimizer_context_t* ctx, float norm) {
    if (!ctx) return;

    // Process pending bio-async messages (outside of lock to avoid potential deadlock)
    if (ctx->bio_async_enabled && ctx->bio_ctx) {
        bio_router_process_inbox(ctx->bio_ctx, 5);
    }

    /* P0 fix: Mutex protection for gradient stats updates to prevent data race */
    if (ctx->state_mutex_initialized) {
        nimcp_platform_mutex_lock(&ctx->state_mutex);
    }

    ctx->stats.total_gradient_norm += norm;
    if (norm < ctx->stats.min_gradient_norm) {
        ctx->stats.min_gradient_norm = norm;
    }
    if (norm > ctx->stats.max_gradient_norm) {
        ctx->stats.max_gradient_norm = norm;
    }
    /* Prevent divide-by-zero if step_count is 0 */
    if (ctx->stats.step_count > 0) {
        ctx->stats.avg_gradient_norm = ctx->stats.total_gradient_norm / ctx->stats.step_count;
    }

    if (ctx->state_mutex_initialized) {
        nimcp_platform_mutex_unlock(&ctx->state_mutex);
    }
}

/* ============================================================================
 * Bio-Async Message Handlers (Phase BIO-1)
 * ============================================================================ */

/**
 * @brief Handle optimizer step request from bio-async
 */
static nimcp_error_t handle_optimizer_step_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    nimcp_optimizer_context_t* ctx = (nimcp_optimizer_context_t*)user_data;
    if (!ctx || !msg || msg_size < sizeof(bio_msg_training_step_t)) {
        nimcp_log(LOG_LEVEL_ERROR, "[%s] Invalid optimizer step request", LOG_MODULE);
        return NIMCP_BIO_ERROR_INVALID_CHANNEL;
    }

    const bio_msg_training_step_t* req = (const bio_msg_training_step_t*)msg;
    nimcp_log(LOG_LEVEL_DEBUG, "[%s] Optimizer step request: batch=%u, lr=%f",
              LOG_MODULE, req->batch_id, ctx->current_lr);

    /* Prepare response with current optimizer state */
    bio_msg_optimizer_step_t response = {0};
    bio_msg_init_header(&response.header, BIO_MSG_OPTIMIZER_STEP,
                       BIO_MODULE_TRAINING_OPTIMIZER, req->header.source_module,
                       sizeof(bio_msg_optimizer_step_t) - sizeof(bio_message_header_t));
    response.step_number = ctx->step_count;
    response.learning_rate = ctx->current_lr;
    response.gradient_norm = (float)ctx->stats.avg_gradient_norm;
    response.weight_updates = ctx->stats.step_count;

    /* Send response via bio-async */
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle gradient computed message
 */
static nimcp_error_t handle_gradient_computed(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    nimcp_optimizer_context_t* ctx = (nimcp_optimizer_context_t*)user_data;
    if (!ctx || !msg || msg_size < sizeof(bio_msg_gradient_computed_t)) {
        nimcp_log(LOG_LEVEL_ERROR, "[%s] Invalid gradient computed message", LOG_MODULE);
        return NIMCP_BIO_ERROR_INVALID_CHANNEL;
    }

    const bio_msg_gradient_computed_t* gradient_msg = (const bio_msg_gradient_computed_t*)msg;
    nimcp_log(LOG_LEVEL_DEBUG, "[%s] Gradient computed: layer=%u, norm=%f",
              LOG_MODULE, gradient_msg->layer_id, gradient_msg->gradient_norm);

    /* Update statistics with new gradient information */
    update_gradient_stats(ctx, gradient_msg->gradient_norm);

    /* Broadcast gradient norm on DOPAMINE channel if improvement */
    if (ctx->bio_async_enabled && gradient_msg->gradient_norm < ctx->stats.avg_gradient_norm) {
        bio_msg_training_metric_t metric_msg = {0};
        bio_msg_init_header(&metric_msg.header, BIO_MSG_TRAINING_METRIC,
                           BIO_MODULE_TRAINING_OPTIMIZER, BIO_MODULE_ALL,
                           sizeof(bio_msg_training_metric_t) - sizeof(bio_message_header_t));
        metric_msg.metric_type = 2; // Gradient improvement
        metric_msg.metric_value = ctx->stats.avg_gradient_norm;
        bio_router_broadcast(ctx->bio_ctx, &metric_msg, sizeof(metric_msg));
    }

    (void)response_promise; // No response needed
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Default Configuration Constructors
 * ============================================================================ */

nimcp_sgd_config_t nimcp_optimizer_sgd_default(float learning_rate) {
    nimcp_sgd_config_t config = {
        .learning_rate = learning_rate,
        .momentum = 0.0F,
        .nesterov = false,
        .dampening = 0.0F,
        .weight_decay = 0.0F
    };
    return config;
}

nimcp_adam_config_t nimcp_optimizer_adam_default(float learning_rate) {
    nimcp_adam_config_t config = {
        .learning_rate = learning_rate,
        .beta1 = NIMCP_ADAM_BETA1_DEFAULT,
        .beta2 = NIMCP_ADAM_BETA2_DEFAULT,
        .epsilon = NIMCP_EPSILON_ADAM,
        .weight_decay = 0.0F,
        .amsgrad = false
    };
    return config;
}

nimcp_rmsprop_config_t nimcp_optimizer_rmsprop_default(float learning_rate) {
    nimcp_rmsprop_config_t config = {
        .learning_rate = learning_rate,
        .alpha = NIMCP_EMA_DECAY_DEFAULT,
        .epsilon = NIMCP_EPSILON_ADAM,
        .weight_decay = 0.0F,
        .momentum = 0.0F,
        .centered = false
    };
    return config;
}

nimcp_adagrad_config_t nimcp_optimizer_adagrad_default(float learning_rate) {
    nimcp_adagrad_config_t config = {
        .learning_rate = learning_rate,
        .lr_decay = 0.0F,
        .weight_decay = 0.0F,
        .initial_accumulator = 0.0F,
        .epsilon = 1e-10F
    };
    return config;
}

nimcp_optimizer_config_t nimcp_optimizer_default_config(nimcp_optimizer_type_t type) {
    nimcp_optimizer_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = type;
    config.clip_gradients = false;
    config.gradient_clip_value = NIMCP_GRADIENT_CLIP_DEFAULT;
    config.gradient_clip_norm = 0.0F;
    config.use_memory_pool = true;
    config.cow_strategy = UNIFIED_STRATEGY_POOL_DIRECT;

    switch (type) {
        case NIMCP_OPTIMIZER_SGD:
            config.params.sgd = nimcp_optimizer_sgd_default(NIMCP_LEARNING_RATE_DEFAULT);
            break;
        case NIMCP_OPTIMIZER_SGD_MOMENTUM:
            config.params.sgd = nimcp_optimizer_sgd_default(NIMCP_LEARNING_RATE_DEFAULT);
            config.params.sgd.momentum = NIMCP_MOMENTUM_DEFAULT;
            break;
        case NIMCP_OPTIMIZER_NESTEROV:
            config.params.sgd = nimcp_optimizer_sgd_default(NIMCP_LEARNING_RATE_DEFAULT);
            config.params.sgd.momentum = NIMCP_MOMENTUM_DEFAULT;
            config.params.sgd.nesterov = true;
            break;
        case NIMCP_OPTIMIZER_ADAM:
            config.params.adam = nimcp_optimizer_adam_default(NIMCP_LEARNING_RATE_FINE);
            break;
        case NIMCP_OPTIMIZER_ADAMW:
            config.params.adamw.learning_rate = NIMCP_LEARNING_RATE_FINE;
            config.params.adamw.beta1 = NIMCP_ADAM_BETA1_DEFAULT;
            config.params.adamw.beta2 = NIMCP_ADAM_BETA2_DEFAULT;
            config.params.adamw.epsilon = NIMCP_EPSILON_ADAM;
            config.params.adamw.weight_decay = NIMCP_LEARNING_RATE_DEFAULT;
            config.params.adamw.amsgrad = false;
            break;
        case NIMCP_OPTIMIZER_NADAM:
            config.params.adam = nimcp_optimizer_adam_default(NIMCP_LEARNING_RATE_FINE);
            break;
        case NIMCP_OPTIMIZER_RMSPROP:
            config.params.rmsprop = nimcp_optimizer_rmsprop_default(NIMCP_LEARNING_RATE_DEFAULT);
            break;
        case NIMCP_OPTIMIZER_ADAGRAD:
            config.params.adagrad = nimcp_optimizer_adagrad_default(NIMCP_LEARNING_RATE_DEFAULT);
            break;
        default:
            break;
    }

    return config;
}

/* ============================================================================
 * Optimizer Step Implementations
 * ============================================================================ */

static void step_sgd(
    nimcp_optimizer_context_t* ctx,
    float* params,
    const float* gradients,
    size_t count)
{
    float lr = ctx->current_lr;
    float momentum = ctx->config.params.sgd.momentum;
    float dampening = ctx->config.params.sgd.dampening;
    float weight_decay = ctx->config.params.sgd.weight_decay;
    bool nesterov = ctx->config.params.sgd.nesterov;

    /* P0 fix: Thread-safe velocity buffer reallocation */
    if (momentum != 0.0F && (ctx->state.momentum.velocity == NULL || count > ctx->state.momentum.count)) {
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_lock(&ctx->state_mutex);
        }
        /* Safe realloc pattern: allocate new buffer first, only free old on success */
        float* new_velocity = alloc_buffer(ctx, count);
        if (new_velocity) {
            memset(new_velocity, 0, count * sizeof(float));
            if (ctx->state.momentum.velocity != NULL) {
                free_buffer(ctx, ctx->state.momentum.velocity);
            }
            ctx->state.momentum.velocity = new_velocity;
            ctx->state.momentum.count = count;
        }
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_unlock(&ctx->state_mutex);
        }
    }

    float* velocity = ctx->state.momentum.velocity;

    for (size_t i = 0; i < count; i++) {
        float grad = gradients[i];

        /* Sanitize gradient: check for NaN/Inf */
        if (!isfinite(grad)) {
            grad = 0.0F;
        } else if (grad > OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = OPTIMIZER_GRADIENT_MAX_VALUE;
        } else if (grad < -OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = -OPTIMIZER_GRADIENT_MAX_VALUE;
        }

        /* L2 regularization */
        if (weight_decay != 0.0F) {
            grad += weight_decay * params[i];
        }

        if (momentum != 0.0F && velocity != NULL) {
            if (ctx->step_count == 0) {
                velocity[i] = grad;
            } else {
                velocity[i] = momentum * velocity[i] + (1.0F - dampening) * grad;
            }

            if (nesterov) {
                grad = grad + momentum * velocity[i];
            } else {
                grad = velocity[i];
            }
        }

        params[i] -= lr * grad;
    }
}

static void step_adam(
    nimcp_optimizer_context_t* ctx,
    float* params,
    const float* gradients,
    size_t count)
{
    float lr = ctx->current_lr;
    float beta1 = ctx->config.params.adam.beta1;
    float beta2 = ctx->config.params.adam.beta2;
    float epsilon = ctx->config.params.adam.epsilon;
    float weight_decay = ctx->config.params.adam.weight_decay;
    bool amsgrad = ctx->config.params.adam.amsgrad;

    optimizer_adam_state_t* state = &ctx->state.adam;

    /* P0 fix: Thread-safe Adam state buffer reallocation */
    if (state->m == NULL || count > state->count) {
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_lock(&ctx->state_mutex);
        }
        /* Safe realloc pattern: allocate all new buffers first, only free old on success */
        float* new_m = alloc_buffer(ctx, count);
        float* new_v = alloc_buffer(ctx, count);
        float* new_v_max = amsgrad ? alloc_buffer(ctx, count) : NULL;

        if (new_m && new_v && (!amsgrad || new_v_max)) {
            memset(new_m, 0, count * sizeof(float));
            memset(new_v, 0, count * sizeof(float));
            if (new_v_max) memset(new_v_max, 0, count * sizeof(float));
            /* Free old buffers now that new ones are ready */
            if (state->m) free_buffer(ctx, state->m);
            if (state->v) free_buffer(ctx, state->v);
            if (state->v_max) free_buffer(ctx, state->v_max);
            state->m = new_m;
            state->v = new_v;
            state->v_max = new_v_max;
            state->count = count;
        } else {
            /* Partial allocation failed - free any successful allocs, keep old state */
            if (new_m) free_buffer(ctx, new_m);
            if (new_v) free_buffer(ctx, new_v);
            if (new_v_max) free_buffer(ctx, new_v_max);
        }
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_unlock(&ctx->state_mutex);
        }
    }

    /* Guard: if allocation failed, cannot proceed */
    if (!state->m || !state->v) {
        return;
    }

    state->t++;

    /* Compute bias correction with clamping to prevent underflow/overflow
     * For large t, powf(beta, t) underflows to 0, making bias_correction = 1.0
     * For small t with beta close to 1, bias_correction can be very small
     * Clamp to prevent division by near-zero values */
    float beta1_t = (state->t <= OPTIMIZER_BIAS_CORRECTION_MAX_STEPS)
                    ? powf(beta1, (float)state->t)
                    : 0.0F;  /* For very large t, treat as converged */
    float beta2_t = (state->t <= OPTIMIZER_BIAS_CORRECTION_MAX_STEPS)
                    ? powf(beta2, (float)state->t)
                    : 0.0F;
    float bias_correction1 = 1.0F - beta1_t;
    float bias_correction2 = 1.0F - beta2_t;

    /* Clamp bias corrections to prevent division by very small numbers */
    if (bias_correction1 < OPTIMIZER_BIAS_CORRECTION_MIN) {
        bias_correction1 = OPTIMIZER_BIAS_CORRECTION_MIN;
    }
    if (bias_correction2 < OPTIMIZER_BIAS_CORRECTION_MIN) {
        bias_correction2 = OPTIMIZER_BIAS_CORRECTION_MIN;
    }

    for (size_t i = 0; i < count; i++) {
        float grad = gradients[i];

        /* Sanitize gradient: check for NaN/Inf */
        if (!isfinite(grad)) {
            grad = 0.0F;  /* Replace invalid gradients with zero */
        } else if (grad > OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = OPTIMIZER_GRADIENT_MAX_VALUE;
        } else if (grad < -OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = -OPTIMIZER_GRADIENT_MAX_VALUE;
        }

        /* L2 regularization */
        if (weight_decay != 0.0F) {
            grad += weight_decay * params[i];
        }

        /* Update biased first moment estimate */
        state->m[i] = beta1 * state->m[i] + (1.0F - beta1) * grad;

        /* Update biased second raw moment estimate */
        state->v[i] = beta2 * state->v[i] + (1.0F - beta2) * grad * grad;

        float m_hat = state->m[i] / bias_correction1;
        float v_hat;

        if (amsgrad && state->v_max != NULL) {
            state->v_max[i] = fmaxf(state->v_max[i], state->v[i]);
            v_hat = state->v_max[i] / bias_correction2;
        } else {
            v_hat = state->v[i] / bias_correction2;
        }

        params[i] -= lr * m_hat / (sqrtf(v_hat) + epsilon);
    }
}

static void step_adamw(
    nimcp_optimizer_context_t* ctx,
    float* params,
    const float* gradients,
    size_t count)
{
    float lr = ctx->current_lr;
    float beta1 = ctx->config.params.adamw.beta1;
    float beta2 = ctx->config.params.adamw.beta2;
    float epsilon = ctx->config.params.adamw.epsilon;
    float weight_decay = ctx->config.params.adamw.weight_decay;
    bool amsgrad = ctx->config.params.adamw.amsgrad;

    optimizer_adam_state_t* state = &ctx->state.adam;

    /* P0 fix: Thread-safe AdamW state buffer reallocation */
    if (state->m == NULL || count > state->count) {
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_lock(&ctx->state_mutex);
        }
        /* Safe realloc pattern: allocate all new buffers first, only free old on success */
        float* new_m = alloc_buffer(ctx, count);
        float* new_v = alloc_buffer(ctx, count);
        float* new_v_max = amsgrad ? alloc_buffer(ctx, count) : NULL;

        if (new_m && new_v && (!amsgrad || new_v_max)) {
            memset(new_m, 0, count * sizeof(float));
            memset(new_v, 0, count * sizeof(float));
            if (new_v_max) memset(new_v_max, 0, count * sizeof(float));
            /* Free old buffers now that new ones are ready */
            if (state->m) free_buffer(ctx, state->m);
            if (state->v) free_buffer(ctx, state->v);
            if (state->v_max) free_buffer(ctx, state->v_max);
            state->m = new_m;
            state->v = new_v;
            state->v_max = new_v_max;
            state->count = count;
        } else {
            /* Partial allocation failed - free any successful allocs, keep old state */
            if (new_m) free_buffer(ctx, new_m);
            if (new_v) free_buffer(ctx, new_v);
            if (new_v_max) free_buffer(ctx, new_v_max);
        }
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_unlock(&ctx->state_mutex);
        }
    }

    /* Guard: if allocation failed, cannot proceed */
    if (!state->m || !state->v) {
        return;
    }

    state->t++;

    /* Compute bias correction with clamping to prevent underflow/overflow */
    float beta1_t = (state->t <= OPTIMIZER_BIAS_CORRECTION_MAX_STEPS)
                    ? powf(beta1, (float)state->t)
                    : 0.0F;
    float beta2_t = (state->t <= OPTIMIZER_BIAS_CORRECTION_MAX_STEPS)
                    ? powf(beta2, (float)state->t)
                    : 0.0F;
    float bias_correction1 = 1.0F - beta1_t;
    float bias_correction2 = 1.0F - beta2_t;

    /* Clamp bias corrections to prevent division by very small numbers */
    if (bias_correction1 < OPTIMIZER_BIAS_CORRECTION_MIN) {
        bias_correction1 = OPTIMIZER_BIAS_CORRECTION_MIN;
    }
    if (bias_correction2 < OPTIMIZER_BIAS_CORRECTION_MIN) {
        bias_correction2 = OPTIMIZER_BIAS_CORRECTION_MIN;
    }

    for (size_t i = 0; i < count; i++) {
        /* Decoupled weight decay */
        if (weight_decay != 0.0F) {
            params[i] -= lr * weight_decay * params[i];
        }

        float grad = gradients[i];

        /* Sanitize gradient: check for NaN/Inf */
        if (!isfinite(grad)) {
            grad = 0.0F;
        } else if (grad > OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = OPTIMIZER_GRADIENT_MAX_VALUE;
        } else if (grad < -OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = -OPTIMIZER_GRADIENT_MAX_VALUE;
        }

        /* Update biased first moment estimate */
        state->m[i] = beta1 * state->m[i] + (1.0F - beta1) * grad;

        /* Update biased second raw moment estimate */
        state->v[i] = beta2 * state->v[i] + (1.0F - beta2) * grad * grad;

        float m_hat = state->m[i] / bias_correction1;
        float v_hat;

        if (amsgrad && state->v_max != NULL) {
            state->v_max[i] = fmaxf(state->v_max[i], state->v[i]);
            v_hat = state->v_max[i] / bias_correction2;
        } else {
            v_hat = state->v[i] / bias_correction2;
        }

        params[i] -= lr * m_hat / (sqrtf(v_hat) + epsilon);
    }
}

static void step_nadam(
    nimcp_optimizer_context_t* ctx,
    float* params,
    const float* gradients,
    size_t count)
{
    float lr = ctx->current_lr;
    float beta1 = ctx->config.params.adam.beta1;
    float beta2 = ctx->config.params.adam.beta2;
    float epsilon = ctx->config.params.adam.epsilon;
    float weight_decay = ctx->config.params.adam.weight_decay;

    optimizer_adam_state_t* state = &ctx->state.adam;

    /* P0 fix: Thread-safe NAdam state buffer reallocation */
    if (state->m == NULL || count > state->count) {
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_lock(&ctx->state_mutex);
        }
        /* Safe realloc pattern: allocate all new buffers first, only free old on success */
        float* new_m = alloc_buffer(ctx, count);
        float* new_v = alloc_buffer(ctx, count);

        if (new_m && new_v) {
            memset(new_m, 0, count * sizeof(float));
            memset(new_v, 0, count * sizeof(float));
            /* Free old buffers now that new ones are ready */
            if (state->m) free_buffer(ctx, state->m);
            if (state->v) free_buffer(ctx, state->v);
            state->m = new_m;
            state->v = new_v;
            state->count = count;
        } else {
            /* Partial allocation failed - free any successful allocs, keep old state */
            if (new_m) free_buffer(ctx, new_m);
            if (new_v) free_buffer(ctx, new_v);
        }
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_unlock(&ctx->state_mutex);
        }
    }

    /* Guard: if allocation failed, cannot proceed */
    if (!state->m || !state->v) {
        return;
    }

    state->t++;

    /* Compute bias correction with clamping to prevent underflow/overflow */
    float beta1_t = (state->t <= OPTIMIZER_BIAS_CORRECTION_MAX_STEPS)
                    ? powf(beta1, (float)state->t)
                    : 0.0F;
    float beta2_t = (state->t <= OPTIMIZER_BIAS_CORRECTION_MAX_STEPS)
                    ? powf(beta2, (float)state->t)
                    : 0.0F;
    float beta1_t_next = (state->t + 1 <= OPTIMIZER_BIAS_CORRECTION_MAX_STEPS)
                         ? powf(beta1, (float)(state->t + 1))
                         : 0.0F;
    float bias_correction1 = 1.0F - beta1_t;
    float bias_correction2 = 1.0F - beta2_t;
    float bias_correction1_next = 1.0F - beta1_t_next;

    /* Clamp bias corrections to prevent division by very small numbers */
    if (bias_correction1 < OPTIMIZER_BIAS_CORRECTION_MIN) {
        bias_correction1 = OPTIMIZER_BIAS_CORRECTION_MIN;
    }
    if (bias_correction2 < OPTIMIZER_BIAS_CORRECTION_MIN) {
        bias_correction2 = OPTIMIZER_BIAS_CORRECTION_MIN;
    }
    if (bias_correction1_next < OPTIMIZER_BIAS_CORRECTION_MIN) {
        bias_correction1_next = OPTIMIZER_BIAS_CORRECTION_MIN;
    }

    for (size_t i = 0; i < count; i++) {
        float grad = gradients[i];

        /* Sanitize gradient: check for NaN/Inf */
        if (!isfinite(grad)) {
            grad = 0.0F;
        } else if (grad > OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = OPTIMIZER_GRADIENT_MAX_VALUE;
        } else if (grad < -OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = -OPTIMIZER_GRADIENT_MAX_VALUE;
        }

        if (weight_decay != 0.0F) {
            grad += weight_decay * params[i];
        }

        state->m[i] = beta1 * state->m[i] + (1.0F - beta1) * grad;
        state->v[i] = beta2 * state->v[i] + (1.0F - beta2) * grad * grad;

        float m_hat = state->m[i] / bias_correction1;
        float v_hat = state->v[i] / bias_correction2;

        /* Nesterov momentum */
        float m_bar = (beta1 * m_hat) + ((1.0F - beta1) * grad / bias_correction1_next);

        params[i] -= lr * m_bar / (sqrtf(v_hat) + epsilon);
    }
}

static void step_rmsprop(
    nimcp_optimizer_context_t* ctx,
    float* params,
    const float* gradients,
    size_t count)
{
    float lr = ctx->current_lr;
    float alpha = ctx->config.params.rmsprop.alpha;
    float epsilon = ctx->config.params.rmsprop.epsilon;
    float weight_decay = ctx->config.params.rmsprop.weight_decay;
    float momentum = ctx->config.params.rmsprop.momentum;
    bool centered = ctx->config.params.rmsprop.centered;

    optimizer_rmsprop_state_t* state = &ctx->state.rmsprop;

    /* P0 fix: Thread-safe RMSprop state buffer reallocation */
    if (state->square_avg == NULL || count > state->count) {
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_lock(&ctx->state_mutex);
        }
        /* Safe realloc pattern: allocate all new buffers first, only free old on success */
        float* new_square_avg = alloc_buffer(ctx, count);
        float* new_momentum_buffer = (momentum != 0.0F) ? alloc_buffer(ctx, count) : NULL;
        float* new_grad_avg = centered ? alloc_buffer(ctx, count) : NULL;

        if (new_square_avg &&
            (momentum == 0.0F || new_momentum_buffer) &&
            (!centered || new_grad_avg)) {
            memset(new_square_avg, 0, count * sizeof(float));
            if (new_momentum_buffer) memset(new_momentum_buffer, 0, count * sizeof(float));
            if (new_grad_avg) memset(new_grad_avg, 0, count * sizeof(float));
            /* Free old buffers now that new ones are ready */
            if (state->square_avg) free_buffer(ctx, state->square_avg);
            if (state->momentum_buffer) free_buffer(ctx, state->momentum_buffer);
            if (state->grad_avg) free_buffer(ctx, state->grad_avg);
            state->square_avg = new_square_avg;
            state->momentum_buffer = new_momentum_buffer;
            state->grad_avg = new_grad_avg;
            state->count = count;
        } else {
            /* Partial allocation failed - free any successful allocs, keep old state */
            if (new_square_avg) free_buffer(ctx, new_square_avg);
            if (new_momentum_buffer) free_buffer(ctx, new_momentum_buffer);
            if (new_grad_avg) free_buffer(ctx, new_grad_avg);
        }
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_unlock(&ctx->state_mutex);
        }
    }

    /* Guard: if allocation failed, cannot proceed */
    if (!state->square_avg) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        float grad = gradients[i];

        /* Sanitize gradient: check for NaN/Inf */
        if (!isfinite(grad)) {
            grad = 0.0F;
        } else if (grad > OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = OPTIMIZER_GRADIENT_MAX_VALUE;
        } else if (grad < -OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = -OPTIMIZER_GRADIENT_MAX_VALUE;
        }

        if (weight_decay != 0.0F) {
            grad += weight_decay * params[i];
        }

        state->square_avg[i] = alpha * state->square_avg[i] + (1.0F - alpha) * grad * grad;

        float avg;
        if (centered && state->grad_avg != NULL) {
            state->grad_avg[i] = alpha * state->grad_avg[i] + (1.0F - alpha) * grad;
            avg = state->square_avg[i] - state->grad_avg[i] * state->grad_avg[i];
            /* Ensure avg is non-negative to prevent sqrt of negative */
            if (avg < 0.0F) {
                avg = 0.0F;
            }
        } else {
            avg = state->square_avg[i];
        }

        float update;
        if (momentum != 0.0F && state->momentum_buffer != NULL) {
            state->momentum_buffer[i] = momentum * state->momentum_buffer[i] +
                                        grad / (sqrtf(avg) + epsilon);
            update = state->momentum_buffer[i];
        } else {
            update = grad / (sqrtf(avg) + epsilon);
        }

        params[i] -= lr * update;
    }
}

static void step_adagrad(
    nimcp_optimizer_context_t* ctx,
    float* params,
    const float* gradients,
    size_t count)
{
    float base_lr = ctx->config.params.adagrad.learning_rate;
    float lr_decay = ctx->config.params.adagrad.lr_decay;
    float weight_decay = ctx->config.params.adagrad.weight_decay;
    float initial_acc = ctx->config.params.adagrad.initial_accumulator;
    float epsilon = ctx->config.params.adagrad.epsilon;

    optimizer_adagrad_state_t* state = &ctx->state.adagrad;

    /* P0 fix: Thread-safe AdaGrad state buffer reallocation */
    if (state->sum == NULL || count > state->count) {
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_lock(&ctx->state_mutex);
        }
        /* Safe realloc pattern: allocate new buffer first, only free old on success */
        float* new_sum = alloc_buffer(ctx, count);
        if (new_sum) {
            for (size_t i = 0; i < count; i++) {
                new_sum[i] = initial_acc;
            }
            if (state->sum != NULL) {
                free_buffer(ctx, state->sum);
            }
            state->sum = new_sum;
            state->count = count;
        }
        if (ctx->state_mutex_initialized) {
            nimcp_platform_mutex_unlock(&ctx->state_mutex);
        }
    }

    /* Guard: if allocation failed, cannot proceed */
    if (!state->sum) {
        return;
    }

    state->step++;
    float clr = base_lr / (1.0F + (float)(state->step - 1) * lr_decay);

    /* Clamp learning rate to valid bounds */
    if (clr < OPTIMIZER_LR_MIN) {
        clr = OPTIMIZER_LR_MIN;
    } else if (clr > OPTIMIZER_LR_MAX) {
        clr = OPTIMIZER_LR_MAX;
    }

    for (size_t i = 0; i < count; i++) {
        float grad = gradients[i];

        /* Sanitize gradient: check for NaN/Inf */
        if (!isfinite(grad)) {
            grad = 0.0F;
        } else if (grad > OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = OPTIMIZER_GRADIENT_MAX_VALUE;
        } else if (grad < -OPTIMIZER_GRADIENT_MAX_VALUE) {
            grad = -OPTIMIZER_GRADIENT_MAX_VALUE;
        }

        if (weight_decay != 0.0F) {
            grad += weight_decay * params[i];
        }

        state->sum[i] += grad * grad;
        params[i] -= clr * grad / (sqrtf(state->sum[i]) + epsilon);
    }

    ctx->current_lr = clr;
}

/* ============================================================================
 * Core Optimizer Operations
 * ============================================================================ */

nimcp_optimizer_context_t* nimcp_optimizer_create(
    const nimcp_optimizer_config_t* config,
    nimcp_sec_integration_t* security_ctx,
    unified_mem_manager_t memory_mgr)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_optimizer_create: config is NULL");
        return NULL;
    }

    nimcp_optimizer_context_t* ctx = (nimcp_optimizer_context_t*)nimcp_calloc(1, sizeof(*ctx));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_optimizer_create: failed to allocate context");
        return NULL;
    }

    memcpy(&ctx->config, config, sizeof(nimcp_optimizer_config_t));
    ctx->security_ctx = security_ctx;
    ctx->memory_mgr = memory_mgr;

    /* Set initial learning rate based on optimizer type */
    switch (config->type) {
        case NIMCP_OPTIMIZER_SGD:
        case NIMCP_OPTIMIZER_SGD_MOMENTUM:
        case NIMCP_OPTIMIZER_NESTEROV:
            ctx->current_lr = config->params.sgd.learning_rate;
            break;
        case NIMCP_OPTIMIZER_ADAM:
        case NIMCP_OPTIMIZER_NADAM:
            ctx->current_lr = config->params.adam.learning_rate;
            break;
        case NIMCP_OPTIMIZER_ADAMW:
            ctx->current_lr = config->params.adamw.learning_rate;
            break;
        case NIMCP_OPTIMIZER_RMSPROP:
            ctx->current_lr = config->params.rmsprop.learning_rate;
            break;
        case NIMCP_OPTIMIZER_ADAGRAD:
            ctx->current_lr = config->params.adagrad.learning_rate;
            break;
        default:
            ctx->current_lr = 0.01F;
            break;
    }

    /* Initialize stats */
    ctx->stats.min_gradient_norm = (double)HUGE_VALF;
    ctx->stats.current_lr = ctx->current_lr;

    /* Security registration */
    if (security_ctx) {
        nimcp_result_t err = nimcp_sec_register_module(
            security_ctx,
            OPTIMIZER_MODULE_NAME,
            NIMCP_SEC_CAT_PLASTICITY,
            &ctx->module_id
        );
        ctx->security_registered = (err == NIMCP_SUCCESS);
        nimcp_log(LOG_LEVEL_DEBUG, "[%s] Security registration: %s (id=%u)",
                  LOG_MODULE, ctx->security_registered ? "success" : "failed", ctx->module_id);
    }

    /* Bio-async integration (Phase BIO-1) */
    ctx->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_TRAINING_OPTIMIZER,
            .module_name = LOG_MODULE,
            .inbox_capacity = 64,
            .user_data = ctx
        };
        ctx->bio_ctx = bio_router_register_module(&bio_info);
        if (ctx->bio_ctx) {
            ctx->bio_async_enabled = true;

            /* Register handlers via KG-driven wiring callback */
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_TRAINING_OPTIMIZER,
                (void*)optimizer_handler_callback,
                ctx
            );

            if (wiring_result != NIMCP_SUCCESS) {
                /* Legacy fallback: direct handler registration */
                nimcp_log(LOG_LEVEL_DEBUG, "[%s] KG wiring unavailable, using legacy registration", LOG_MODULE);
                LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(ctx->bio_ctx,
                                           BIO_MSG_TRAINING_STEP_REQUEST,
                                           handle_optimizer_step_request));
                LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(ctx->bio_ctx,
                                           BIO_MSG_GRADIENT_COMPUTED,
                                           handle_gradient_computed));
            }

            nimcp_log(LOG_LEVEL_INFO, "[%s] Bio-async integration enabled for optimizer type=%s",
                     LOG_MODULE, nimcp_optimizer_type_name(config->type));
        } else {
            nimcp_log(LOG_LEVEL_WARN, "[%s] Bio-async integration failed, continuing without it",
                     LOG_MODULE);
        }
    }

    /* P0 fix: Initialize state buffer mutex for thread safety */
    if (nimcp_platform_mutex_init(&ctx->state_mutex, false) == 0) {
        ctx->state_mutex_initialized = true;
    } else {
        ctx->state_mutex_initialized = false;
        nimcp_log(LOG_LEVEL_WARN, "[%s] Failed to initialize state mutex, continuing without thread safety",
                  LOG_MODULE);
    }

    ctx->initialized = true;
    nimcp_log(LOG_LEVEL_INFO, "[%s] Optimizer created: type=%s, lr=%f",
              LOG_MODULE, nimcp_optimizer_type_name(config->type), ctx->current_lr);
    return ctx;
}

void nimcp_optimizer_destroy(nimcp_optimizer_context_t* ctx) {
    if (!ctx) return;

    /* P0 fix: Acquire lock before destroying state buffers to prevent races */
    if (ctx->state_mutex_initialized) {
        nimcp_platform_mutex_lock(&ctx->state_mutex);
    }

    /* Bio-async cleanup (Phase BIO-1) */
    if (ctx->bio_async_enabled && ctx->bio_ctx) {
        bio_router_unregister_module(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
        ctx->bio_async_enabled = false;
        nimcp_log(LOG_LEVEL_DEBUG, "[%s] Bio-async integration cleaned up", LOG_MODULE);
    }

    /* Free optimizer state */
    switch (ctx->config.type) {
        case NIMCP_OPTIMIZER_SGD:
        case NIMCP_OPTIMIZER_SGD_MOMENTUM:
        case NIMCP_OPTIMIZER_NESTEROV:
            free_buffer(ctx, ctx->state.momentum.velocity);
            break;
        case NIMCP_OPTIMIZER_ADAM:
        case NIMCP_OPTIMIZER_ADAMW:
        case NIMCP_OPTIMIZER_NADAM:
            free_buffer(ctx, ctx->state.adam.m);
            free_buffer(ctx, ctx->state.adam.v);
            free_buffer(ctx, ctx->state.adam.v_max);
            break;
        case NIMCP_OPTIMIZER_RMSPROP:
            free_buffer(ctx, ctx->state.rmsprop.square_avg);
            free_buffer(ctx, ctx->state.rmsprop.momentum_buffer);
            free_buffer(ctx, ctx->state.rmsprop.grad_avg);
            break;
        case NIMCP_OPTIMIZER_ADAGRAD:
            free_buffer(ctx, ctx->state.adagrad.sum);
            break;
        default:
            break;
    }

    /* P0 fix: Cleanup state mutex */
    if (ctx->state_mutex_initialized) {
        nimcp_platform_mutex_unlock(&ctx->state_mutex);
        nimcp_platform_mutex_destroy(&ctx->state_mutex);
        ctx->state_mutex_initialized = false;
    }

    nimcp_log(LOG_LEVEL_INFO, "[%s] Optimizer destroyed: type=%s",
              LOG_MODULE, nimcp_optimizer_type_name(ctx->config.type));
    nimcp_free(ctx);
}

nimcp_result_t nimcp_optimizer_init_params(
    nimcp_optimizer_context_t* ctx,
    size_t num_params)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(num_params > 0, NIMCP_ERROR_INVALID_PARAM, "num_params must be > 0");

    /* State will be lazily initialized on first step */
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_optimizer_step(
    nimcp_optimizer_context_t* ctx,
    float* params,
    const float* gradients,
    size_t count)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL for optimizer step");
    NIMCP_CHECK_THROW(params != NULL, NIMCP_ERROR_INVALID_PARAM, "params is NULL for optimizer step");
    NIMCP_CHECK_THROW(gradients != NULL, NIMCP_ERROR_INVALID_PARAM, "gradients is NULL for optimizer step");
    NIMCP_CHECK_THROW(count > 0, NIMCP_ERROR_INVALID_PARAM, "count must be > 0 for optimizer step");

    uint64_t start_time = get_time_ns();

    /* Compute gradient norm */
    float grad_norm = nimcp_optimizer_gradient_norm(gradients, count);

    /* Mandatory gradient sanitization: replace NaN/Inf with zero, then clip.
     * Previous approach (skip entire update) meant the brain NEVER learned
     * because every early update had inf gradients from random-weight forward
     * passes on large networks (1024-dim input → 512-dim hidden). */
    float* clipped_grads = alloc_buffer(ctx, count);
    const float* grads_to_use = gradients;

    if (clipped_grads) {
        memcpy(clipped_grads, gradients, count * sizeof(float));

        /* Step 1: Sanitize NaN/Inf values to zero */
        if (!isfinite(grad_norm) || grad_norm > 1e6F) {
            ctx->stats.gradient_explosions++;
            for (size_t i = 0; i < count; i++) {
                if (!isfinite(clipped_grads[i])) {
                    clipped_grads[i] = 0.0f;
                }
            }
        }

        /* Step 2: Normalize or clip gradient norm.
         * Normalization preserves direction perfectly (better for large networks).
         * Clipping only reduces when above threshold. */
        float clip_norm = (ctx->config.clip_gradients && ctx->config.gradient_clip_norm > 0.0F)
                          ? ctx->config.gradient_clip_norm : 1.0F;
        float current_norm = nimcp_optimizer_gradient_norm(clipped_grads, count);
        if (current_norm > 1e-8F) {
            /* Always normalize to target norm — prevents both vanishing and exploding */
            float scale = clip_norm / current_norm;
            for (size_t i = 0; i < count; i++) {
                clipped_grads[i] *= scale;
            }
        }

        /* Step 3: Optional per-value clipping if configured */
        if (ctx->config.clip_gradients && ctx->config.gradient_clip_value > 0.0F) {
            size_t clips = nimcp_optimizer_clip_by_value(
                clipped_grads, count, ctx->config.gradient_clip_value);
            ctx->stats.gradient_clips += clips;
        }
        grads_to_use = clipped_grads;
    } else if (!isfinite(grad_norm)) {
        /* Fallback: can't allocate clip buffer and gradients are bad — skip */
        ctx->stats.gradient_explosions++;
        return NIMCP_SUCCESS;
    }

    /* Natural gradient pre-conditioning via Fisher information metric.
     * For small parameter spaces (dim ≤ 64), compute the Riemannian metric
     * G = diag(v_hat) from Adam's second moment as an approximation of the
     * Fisher information matrix, then use g^{-1} * grad for the natural gradient.
     * This is mathematically equivalent to what Adam already does per-parameter,
     * but for non-Adam optimizers (SGD, Nesterov) it provides curvature awareness.
     * Only applied to SGD-family optimizers where it adds value. */
    if (count <= DIFFGEO_MAX_DIM &&
        (ctx->config.type == NIMCP_OPTIMIZER_SGD ||
         ctx->config.type == NIMCP_OPTIMIZER_SGD_MOMENTUM ||
         ctx->config.type == NIMCP_OPTIMIZER_NESTEROV) &&
        clipped_grads != NULL && ctx->step_count > 0) {
        riemannian_metric_t* fisher = riemannian_metric_create((uint32_t)count);
        if (fisher) {
            /* Build diagonal Fisher metric from squared gradients (empirical Fisher).
             * G_ii = grad_i^2 approximates the Fisher information for each parameter. */
            for (uint32_t i = 0; i < (uint32_t)count; i++) {
                float g_ii = clipped_grads[i] * clipped_grads[i] + DIFFGEO_EPSILON;
                fisher->g[i * count + i] = g_ii;
            }
            if (riemannian_metric_invert(fisher) == DIFFGEO_OK) {
                /* Apply natural gradient: g_nat = G^{-1} * g_euclidean */
                float nat_grad[DIFFGEO_MAX_DIM];
                if (riemannian_raise_index(fisher, clipped_grads, nat_grad) == DIFFGEO_OK) {
                    /* Re-normalize to preserve original gradient scale */
                    float nat_norm = 0.0f;
                    for (uint32_t i = 0; i < (uint32_t)count; i++) {
                        nat_norm += nat_grad[i] * nat_grad[i];
                    }
                    nat_norm = sqrtf(nat_norm + DIFFGEO_EPSILON);
                    float scale = grad_norm / (nat_norm + DIFFGEO_EPSILON);
                    for (uint32_t i = 0; i < (uint32_t)count; i++) {
                        clipped_grads[i] = nat_grad[i] * scale;
                    }
                }
            }
            riemannian_metric_destroy(fisher);
        }
    }

    nimcp_log(LOG_LEVEL_TRACE, "[%s] Optimizer step: lr=%f, grad_norm=%f, step=%lu",
              LOG_MODULE, ctx->current_lr, grad_norm, ctx->step_count);

    /* Perform optimizer step */
    switch (ctx->config.type) {
        case NIMCP_OPTIMIZER_SGD:
        case NIMCP_OPTIMIZER_SGD_MOMENTUM:
        case NIMCP_OPTIMIZER_NESTEROV:
            step_sgd(ctx, params, grads_to_use, count);
            break;
        case NIMCP_OPTIMIZER_ADAM:
            step_adam(ctx, params, grads_to_use, count);
            break;
        case NIMCP_OPTIMIZER_ADAMW:
            step_adamw(ctx, params, grads_to_use, count);
            break;
        case NIMCP_OPTIMIZER_NADAM:
            step_nadam(ctx, params, grads_to_use, count);
            break;
        case NIMCP_OPTIMIZER_RMSPROP:
            step_rmsprop(ctx, params, grads_to_use, count);
            break;
        case NIMCP_OPTIMIZER_ADAGRAD:
            step_adagrad(ctx, params, grads_to_use, count);
            break;
        case NIMCP_OPTIMIZER_CUSTOM:
            if (ctx->config.params.custom.step_fn) {
                ctx->config.params.custom.step_fn(
                    params, grads_to_use, ctx->state.custom, count,
                    ctx->config.params.custom.user_data);
            }
            break;
        default:
            free_buffer(ctx, clipped_grads);
            return NIMCP_ERROR_NOT_IMPLEMENTED;
    }

    free_buffer(ctx, clipped_grads);

    /* Update statistics */
    ctx->step_count++;
    ctx->stats.step_count = ctx->step_count;
    update_gradient_stats(ctx, grad_norm);
    ctx->stats.current_lr = ctx->current_lr;

    uint64_t end_time = get_time_ns();
    ctx->stats.total_compute_time_ns += (end_time - start_time);

    /* Phase BIO-1: Broadcast optimizer step completion */
    if (ctx->bio_async_enabled) {
        bio_msg_optimizer_step_t step_msg = {0};
        bio_msg_init_header(&step_msg.header, BIO_MSG_OPTIMIZER_STEP,
                           BIO_MODULE_TRAINING_OPTIMIZER, BIO_MODULE_ALL,
                           sizeof(bio_msg_optimizer_step_t) - sizeof(bio_message_header_t));
        step_msg.step_number = ctx->step_count;
        step_msg.learning_rate = ctx->current_lr;
        step_msg.gradient_norm = grad_norm;
        step_msg.weight_updates = count;

        bio_router_broadcast(ctx->bio_ctx, &step_msg, sizeof(step_msg));
    }

    /* Phase LOG-1: Log significant events */
    if (ctx->step_count % 100 == 0) {
        nimcp_log(LOG_LEVEL_INFO, "[%s] Optimizer step %lu: lr=%f, grad_norm=%f, avg_grad=%f",
                  LOG_MODULE, ctx->step_count, ctx->current_lr, grad_norm,
                  ctx->stats.avg_gradient_norm);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_optimizer_step_group(
    nimcp_optimizer_context_t* ctx,
    nimcp_param_group_t* group)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(group != NULL, NIMCP_ERROR_INVALID_PARAM, "group is NULL");
    NIMCP_CHECK_THROW(group->params != NULL, NIMCP_ERROR_INVALID_PARAM, "group->params is NULL");
    NIMCP_CHECK_THROW(group->gradients != NULL, NIMCP_ERROR_INVALID_PARAM, "group->gradients is NULL");

    /* Temporarily adjust learning rate for this group */
    float saved_lr = ctx->current_lr;
    if (group->learning_rate > 0.0F) {
        ctx->current_lr = group->learning_rate;
    }

    nimcp_result_t result = nimcp_optimizer_step(
        ctx, group->params, group->gradients, group->count);

    ctx->current_lr = saved_lr;
    return result;
}

void nimcp_optimizer_zero_grad(nimcp_param_group_t* group) {
    if (!group || !group->gradients) return;
    memset(group->gradients, 0, group->count * sizeof(float));
}

float nimcp_optimizer_get_lr(const nimcp_optimizer_context_t* ctx) {
    if (!ctx) return 0.0F;
    return ctx->current_lr;
}

void nimcp_optimizer_set_lr(nimcp_optimizer_context_t* ctx, float lr) {
    if (!ctx || lr <= 0.0F) return;

    /* Clamp learning rate to valid bounds */
    if (lr < OPTIMIZER_LR_MIN) {
        lr = OPTIMIZER_LR_MIN;
        nimcp_log(LOG_LEVEL_WARN, "[%s] Learning rate clamped to minimum: %e",
                  LOG_MODULE, OPTIMIZER_LR_MIN);
    } else if (lr > OPTIMIZER_LR_MAX) {
        lr = OPTIMIZER_LR_MAX;
        nimcp_log(LOG_LEVEL_WARN, "[%s] Learning rate clamped to maximum: %f",
                  LOG_MODULE, OPTIMIZER_LR_MAX);
    }

    float old_lr = ctx->current_lr;
    ctx->current_lr = lr;
    ctx->stats.current_lr = lr;

    /* Phase LOG-1: Log learning rate changes */
    nimcp_log(LOG_LEVEL_INFO, "[%s] Learning rate changed: %f -> %f (step=%lu)",
              LOG_MODULE, old_lr, lr, ctx->step_count);

    /* Phase BIO-1: Broadcast LR change on DOPAMINE if improvement (increase) */
    if (ctx->bio_async_enabled && lr > old_lr) {
        bio_msg_training_metric_t metric_msg = {0};
        bio_msg_init_header(&metric_msg.header, BIO_MSG_TRAINING_METRIC,
                           BIO_MODULE_TRAINING_OPTIMIZER, BIO_MODULE_ALL,
                           sizeof(bio_msg_training_metric_t) - sizeof(bio_message_header_t));
        metric_msg.metric_type = 3; // Learning rate change
        metric_msg.metric_value = lr;
        bio_router_broadcast(ctx->bio_ctx, &metric_msg, sizeof(metric_msg));
    }
}

uint64_t nimcp_optimizer_get_step(const nimcp_optimizer_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->step_count;
}

void nimcp_optimizer_reset_state(nimcp_optimizer_context_t* ctx) {
    if (!ctx) return;

    switch (ctx->config.type) {
        case NIMCP_OPTIMIZER_SGD:
        case NIMCP_OPTIMIZER_SGD_MOMENTUM:
        case NIMCP_OPTIMIZER_NESTEROV:
            if (ctx->state.momentum.velocity) {
                memset(ctx->state.momentum.velocity, 0,
                       ctx->state.momentum.count * sizeof(float));
            }
            break;
        case NIMCP_OPTIMIZER_ADAM:
        case NIMCP_OPTIMIZER_ADAMW:
        case NIMCP_OPTIMIZER_NADAM:
            if (ctx->state.adam.m) {
                memset(ctx->state.adam.m, 0, ctx->state.adam.count * sizeof(float));
            }
            if (ctx->state.adam.v) {
                memset(ctx->state.adam.v, 0, ctx->state.adam.count * sizeof(float));
            }
            if (ctx->state.adam.v_max) {
                memset(ctx->state.adam.v_max, 0, ctx->state.adam.count * sizeof(float));
            }
            ctx->state.adam.t = 0;
            break;
        case NIMCP_OPTIMIZER_RMSPROP:
            if (ctx->state.rmsprop.square_avg) {
                memset(ctx->state.rmsprop.square_avg, 0,
                       ctx->state.rmsprop.count * sizeof(float));
            }
            if (ctx->state.rmsprop.momentum_buffer) {
                memset(ctx->state.rmsprop.momentum_buffer, 0,
                       ctx->state.rmsprop.count * sizeof(float));
            }
            if (ctx->state.rmsprop.grad_avg) {
                memset(ctx->state.rmsprop.grad_avg, 0,
                       ctx->state.rmsprop.count * sizeof(float));
            }
            break;
        case NIMCP_OPTIMIZER_ADAGRAD:
            if (ctx->state.adagrad.sum) {
                float initial = ctx->config.params.adagrad.initial_accumulator;
                for (size_t i = 0; i < ctx->state.adagrad.count; i++) {
                    ctx->state.adagrad.sum[i] = initial;
                }
            }
            ctx->state.adagrad.step = 0;
            break;
        default:
            break;
    }

    ctx->step_count = 0;
}

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

nimcp_result_t nimcp_optimizer_get_stats(
    const nimcp_optimizer_context_t* ctx,
    nimcp_optimizer_stats_t* stats)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(stats != NULL, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    memcpy(stats, &ctx->stats, sizeof(nimcp_optimizer_stats_t));
    return NIMCP_SUCCESS;
}

void nimcp_optimizer_reset_stats(nimcp_optimizer_context_t* ctx) {
    if (!ctx) return;
    memset(&ctx->stats, 0, sizeof(nimcp_optimizer_stats_t));
    ctx->stats.min_gradient_norm = (double)HUGE_VALF;
    ctx->stats.current_lr = ctx->current_lr;
}

const char* nimcp_optimizer_type_name(nimcp_optimizer_type_t type) {
    switch (type) {
        case NIMCP_OPTIMIZER_SGD:          return "SGD";
        case NIMCP_OPTIMIZER_SGD_MOMENTUM: return "SGD+Momentum";
        case NIMCP_OPTIMIZER_NESTEROV:     return "Nesterov";
        case NIMCP_OPTIMIZER_ADAGRAD:      return "AdaGrad";
        case NIMCP_OPTIMIZER_RMSPROP:      return "RMSprop";
        case NIMCP_OPTIMIZER_ADAM:         return "Adam";
        case NIMCP_OPTIMIZER_ADAMW:        return "AdamW";
        case NIMCP_OPTIMIZER_NADAM:        return "NAdam";
        case NIMCP_OPTIMIZER_CUSTOM:       return "Custom";
        default:                           return "Unknown";
    }
}

nimcp_result_t nimcp_optimizer_validate_config(const nimcp_optimizer_config_t* config) {
    NIMCP_CHECK_THROW(config != NULL, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(config->type < NIMCP_OPTIMIZER_TYPE_COUNT, NIMCP_ERROR_INVALID_PARAM,
        "invalid optimizer type %d", config->type);

    /* Validate type-specific parameters */
    switch (config->type) {
        case NIMCP_OPTIMIZER_SGD:
        case NIMCP_OPTIMIZER_SGD_MOMENTUM:
        case NIMCP_OPTIMIZER_NESTEROV:
            if (config->params.sgd.learning_rate <= 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (config->params.sgd.momentum < 0.0F || config->params.sgd.momentum > 1.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;
        case NIMCP_OPTIMIZER_ADAM:
        case NIMCP_OPTIMIZER_NADAM:
            if (config->params.adam.learning_rate <= 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (config->params.adam.beta1 < 0.0F || config->params.adam.beta1 >= 1.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (config->params.adam.beta2 < 0.0F || config->params.adam.beta2 >= 1.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;
        case NIMCP_OPTIMIZER_RMSPROP:
            if (config->params.rmsprop.learning_rate <= 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;
        case NIMCP_OPTIMIZER_ADAGRAD:
            if (config->params.adagrad.learning_rate <= 0.0F) {
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;
        default:
            break;
    }

    return NIMCP_SUCCESS;
}

bool nimcp_optimizer_is_registered(const nimcp_optimizer_context_t* ctx) {
    if (!ctx) {
        return false;
    }
    return ctx->security_registered;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

float nimcp_optimizer_gradient_norm(const float* gradients, size_t count) {
    if (!gradients || count == 0) return 0.0F;

    /* Use tensor library for vectorized norm computation */
    uint32_t dims[] = {(uint32_t)count};
    nimcp_tensor_t* t = nimcp_tensor_from_data(gradients, dims, 1, NIMCP_DTYPE_F32, false);
    if (t) {
        float norm = (float)nimcp_tensor_norm_p(t, 2.0);
        nimcp_tensor_destroy(t);
        return norm;
    }

    /* Fallback to scalar computation */
    float sum_sq = 0.0F;
    for (size_t i = 0; i < count; i++) {
        sum_sq += gradients[i] * gradients[i];
    }
    return sqrtf(sum_sq);
}

size_t nimcp_optimizer_clip_by_value(
    float* gradients,
    size_t count,
    float max_value)
{
    if (!gradients || count == 0 || max_value <= 0.0F) return 0;

    /* Scalar computation for value clipping (preserves clip count) */
    size_t clips = 0;
    for (size_t i = 0; i < count; i++) {
        if (gradients[i] > max_value) {
            gradients[i] = max_value;
            clips++;
        } else if (gradients[i] < -max_value) {
            gradients[i] = -max_value;
            clips++;
        }
    }
    return clips;
}

float nimcp_optimizer_clip_by_norm(
    float* gradients,
    size_t count,
    float max_norm)
{
    if (!gradients || count == 0 || max_norm <= 0.0F) return 0.0F;

    /* Use tensor library for vectorized operations */
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
    float norm = nimcp_optimizer_gradient_norm(gradients, count);
    if (norm > max_norm) {
        float scale = max_norm / norm;
        for (size_t i = 0; i < count; i++) {
            gradients[i] *= scale;
        }
    }
    return norm;
}

/* ============================================================================
 * Tensor-Based Operations (Phase TENSOR-2)
 * ============================================================================ */

nimcp_result_t nimcp_optimizer_step_tensor(
    nimcp_optimizer_context_t* ctx,
    nimcp_tensor_t* params,
    const nimcp_tensor_t* gradients)
{
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID, "ctx is NULL for tensor step");
    NIMCP_CHECK_THROW(params != NULL, NIMCP_ERROR_INVALID, "params tensor is NULL");
    NIMCP_CHECK_THROW(gradients != NULL, NIMCP_ERROR_INVALID, "gradients tensor is NULL");

    /* Get flat data pointers */
    size_t count = nimcp_tensor_numel(params);
    size_t grad_count = nimcp_tensor_numel(gradients);
    NIMCP_CHECK_THROW(count == grad_count, NIMCP_ERROR_INVALID,
        "params/gradients size mismatch: %zu vs %zu", count, grad_count);

    float* param_data = (float*)nimcp_tensor_data(params);
    const float* grad_data = (const float*)nimcp_tensor_data((nimcp_tensor_t*)gradients);
    NIMCP_CHECK_THROW(param_data != NULL, NIMCP_ERROR_INVALID, "failed to get params data");
    NIMCP_CHECK_THROW(grad_data != NULL, NIMCP_ERROR_INVALID, "failed to get gradients data");

    return nimcp_optimizer_step(ctx, param_data, grad_data, count);
}

float nimcp_optimizer_gradient_norm_tensor(const nimcp_tensor_t* gradients) {
    if (!gradients) return 0.0F;
    return (float)nimcp_tensor_norm_p(gradients, 2.0);
}

float nimcp_optimizer_clip_by_norm_tensor(
    nimcp_tensor_t* gradients,
    float max_norm)
{
    if (!gradients || max_norm <= 0.0F) return 0.0F;

    float norm = (float)nimcp_tensor_norm_p(gradients, 2.0);
    if (norm > max_norm) {
        nimcp_tensor_mul_scalar_(gradients, (double)max_norm / (double)norm);
    }
    return norm;
}

/* ============================================================================
 * Optimizer State Persistence
 * ============================================================================ */

#define OPT_STATE_MAGIC 0x4F505453  /* "OPTS" */

int nimcp_optimizer_save(const nimcp_optimizer_context_t* ctx, FILE* file) {
    if (!ctx || !file) return -1;

    uint32_t magic = OPT_STATE_MAGIC;
    uint32_t type = (uint32_t)ctx->config.type;
    uint64_t step = ctx->step_count;
    float lr = ctx->current_lr;

    if (fwrite(&magic, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&type, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&step, sizeof(uint64_t), 1, file) != 1) return -1;
    if (fwrite(&lr, sizeof(float), 1, file) != 1) return -1;

    switch (ctx->config.type) {
        case NIMCP_OPTIMIZER_SGD:
        case NIMCP_OPTIMIZER_SGD_MOMENTUM:
        case NIMCP_OPTIMIZER_NESTEROV: {
            uint64_t count = (uint64_t)ctx->state.momentum.count;
            if (fwrite(&count, sizeof(uint64_t), 1, file) != 1) return -1;
            if (count > 0 && ctx->state.momentum.velocity) {
                if (fwrite(ctx->state.momentum.velocity, sizeof(float), count, file) != count)
                    return -1;
            }
            break;
        }

        case NIMCP_OPTIMIZER_ADAM:
        case NIMCP_OPTIMIZER_ADAMW:
        case NIMCP_OPTIMIZER_NADAM: {
            uint64_t count = (uint64_t)ctx->state.adam.count;
            uint64_t t = ctx->state.adam.t;
            if (fwrite(&count, sizeof(uint64_t), 1, file) != 1) return -1;
            if (fwrite(&t, sizeof(uint64_t), 1, file) != 1) return -1;
            if (count > 0) {
                if (ctx->state.adam.m &&
                    fwrite(ctx->state.adam.m, sizeof(float), count, file) != count) return -1;
                if (ctx->state.adam.v &&
                    fwrite(ctx->state.adam.v, sizeof(float), count, file) != count) return -1;
                /* v_max for AMSGrad (may be NULL) */
                bool has_vmax = (ctx->state.adam.v_max != NULL);
                if (fwrite(&has_vmax, sizeof(bool), 1, file) != 1) return -1;
                if (has_vmax &&
                    fwrite(ctx->state.adam.v_max, sizeof(float), count, file) != count) return -1;
            }
            break;
        }

        case NIMCP_OPTIMIZER_RMSPROP: {
            uint64_t count = (uint64_t)ctx->state.rmsprop.count;
            if (fwrite(&count, sizeof(uint64_t), 1, file) != 1) return -1;
            if (count > 0) {
                if (ctx->state.rmsprop.square_avg &&
                    fwrite(ctx->state.rmsprop.square_avg, sizeof(float), count, file) != count) return -1;
                bool has_momentum_buf = (ctx->state.rmsprop.momentum_buffer != NULL);
                if (fwrite(&has_momentum_buf, sizeof(bool), 1, file) != 1) return -1;
                if (has_momentum_buf &&
                    fwrite(ctx->state.rmsprop.momentum_buffer, sizeof(float), count, file) != count) return -1;
                bool has_grad_avg = (ctx->state.rmsprop.grad_avg != NULL);
                if (fwrite(&has_grad_avg, sizeof(bool), 1, file) != 1) return -1;
                if (has_grad_avg &&
                    fwrite(ctx->state.rmsprop.grad_avg, sizeof(float), count, file) != count) return -1;
            }
            break;
        }

        case NIMCP_OPTIMIZER_ADAGRAD: {
            uint64_t count = (uint64_t)ctx->state.adagrad.count;
            uint64_t adagrad_step = ctx->state.adagrad.step;
            if (fwrite(&count, sizeof(uint64_t), 1, file) != 1) return -1;
            if (fwrite(&adagrad_step, sizeof(uint64_t), 1, file) != 1) return -1;
            if (count > 0 && ctx->state.adagrad.sum &&
                fwrite(ctx->state.adagrad.sum, sizeof(float), count, file) != count) return -1;
            break;
        }

        default:
            /* Unknown/custom — save nothing beyond header */
            break;
    }

    return 0;
}

int nimcp_optimizer_load(nimcp_optimizer_context_t* ctx, FILE* file) {
    if (!ctx || !file) return -1;

    uint32_t magic = 0, type = 0;
    uint64_t step = 0;
    float lr = 0.0f;

    if (fread(&magic, sizeof(uint32_t), 1, file) != 1 || magic != OPT_STATE_MAGIC) return -1;
    if (fread(&type, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fread(&step, sizeof(uint64_t), 1, file) != 1) return -1;
    if (fread(&lr, sizeof(float), 1, file) != 1) return -1;

    /* Type mismatch — skip restore */
    if (type != (uint32_t)ctx->config.type) {
        NIMCP_LOGGING_WARN("nimcp_optimizer_load: type mismatch (file=%u, ctx=%u)",
                           type, (uint32_t)ctx->config.type);
        return -1;
    }

    ctx->step_count = step;
    if (lr > 0.0f) ctx->current_lr = lr;

    /* Audit fix #13: dimension-mismatch policy.
     * If existing ctx is allocated for a different parameter count than
     * what's in the file, skipping the restore is safer than reallocating
     * to the file's size (which would mismatch the actual model). The
     * optimizer keeps its fresh zero-initialized state at the current dim.
     * Only allocate from file if ctx has no buffers yet (count==0). */
    #define DIM_MISMATCH(ctx_count, file_count) \
        ((ctx_count) > 0 && (size_t)(file_count) != (size_t)(ctx_count))

    switch (ctx->config.type) {
        case NIMCP_OPTIMIZER_SGD:
        case NIMCP_OPTIMIZER_SGD_MOMENTUM:
        case NIMCP_OPTIMIZER_NESTEROV: {
            uint64_t count = 0;
            if (fread(&count, sizeof(uint64_t), 1, file) != 1) return -1;
            if (count > 0) {
                if (DIM_MISMATCH(ctx->state.momentum.count, count)) {
                    NIMCP_LOGGING_WARN("optimizer_load: SGD dim mismatch "
                        "(file=%llu ctx=%zu) — skipping state restore, "
                        "keeping fresh momentum buffer",
                        (unsigned long long)count, ctx->state.momentum.count);
                    /* Skip the velocity floats in the file */
                    if (fseek(file, (long)(count * sizeof(float)), SEEK_CUR) != 0) return -1;
                    break;
                }
                if (!ctx->state.momentum.velocity) {
                    ctx->state.momentum.velocity = alloc_buffer(ctx, (size_t)count);
                    ctx->state.momentum.count = (size_t)count;
                }
                if (ctx->state.momentum.velocity &&
                    fread(ctx->state.momentum.velocity, sizeof(float), count, file) != count)
                    return -1;
            }
            break;
        }

        case NIMCP_OPTIMIZER_ADAM:
        case NIMCP_OPTIMIZER_ADAMW:
        case NIMCP_OPTIMIZER_NADAM: {
            uint64_t count = 0, t = 0;
            if (fread(&count, sizeof(uint64_t), 1, file) != 1) return -1;
            if (fread(&t, sizeof(uint64_t), 1, file) != 1) return -1;
            if (count > 0) {
                if (DIM_MISMATCH(ctx->state.adam.count, count)) {
                    NIMCP_LOGGING_WARN("optimizer_load: Adam dim mismatch "
                        "(file=%llu ctx=%zu) — skipping state restore, "
                        "keeping fresh m/v buffers",
                        (unsigned long long)count, ctx->state.adam.count);
                    /* Skip m, v, has_vmax flag, and optionally v_max */
                    if (fseek(file, (long)(count * sizeof(float) * 2), SEEK_CUR) != 0)
                        return -1;
                    bool has_vmax = false;
                    if (fread(&has_vmax, sizeof(bool), 1, file) != 1) return -1;
                    if (has_vmax) {
                        if (fseek(file, (long)(count * sizeof(float)), SEEK_CUR) != 0)
                            return -1;
                    }
                    break;  /* DON'T apply ctx->state.adam.t either — keep fresh */
                }
                ctx->state.adam.t = t;  /* only apply t when dims match */
                /* Allocate buffers if needed (fresh context after load) */
                if (!ctx->state.adam.m) {
                    ctx->state.adam.m = alloc_buffer(ctx, (size_t)count);
                    ctx->state.adam.v = alloc_buffer(ctx, (size_t)count);
                    ctx->state.adam.v_max = NULL;
                    ctx->state.adam.count = (size_t)count;
                }
                if (ctx->state.adam.m &&
                    fread(ctx->state.adam.m, sizeof(float), count, file) != count) return -1;
                if (ctx->state.adam.v &&
                    fread(ctx->state.adam.v, sizeof(float), count, file) != count) return -1;
                bool has_vmax = false;
                if (fread(&has_vmax, sizeof(bool), 1, file) != 1) return -1;
                if (has_vmax) {
                    if (!ctx->state.adam.v_max) {
                        ctx->state.adam.v_max = alloc_buffer(ctx, (size_t)count);
                    }
                    if (ctx->state.adam.v_max &&
                        fread(ctx->state.adam.v_max, sizeof(float), count, file) != count) return -1;
                }
            } else {
                ctx->state.adam.t = t;  /* zero-count case: still apply step */
            }
            break;
        }

        case NIMCP_OPTIMIZER_RMSPROP: {
            uint64_t count = 0;
            if (fread(&count, sizeof(uint64_t), 1, file) != 1) return -1;
            if (count > 0) {
                if (DIM_MISMATCH(ctx->state.rmsprop.count, count)) {
                    NIMCP_LOGGING_WARN("optimizer_load: RMSprop dim mismatch "
                        "(file=%llu ctx=%zu) — skipping state restore",
                        (unsigned long long)count, ctx->state.rmsprop.count);
                    /* Skip square_avg + has_momentum flag + optional momentum
                     * + has_grad_avg flag + optional grad_avg */
                    if (fseek(file, (long)(count * sizeof(float)), SEEK_CUR) != 0) return -1;
                    bool _hm = false, _hg = false;
                    if (fread(&_hm, sizeof(bool), 1, file) != 1) return -1;
                    if (_hm && fseek(file, (long)(count * sizeof(float)), SEEK_CUR) != 0) return -1;
                    if (fread(&_hg, sizeof(bool), 1, file) != 1) return -1;
                    if (_hg && fseek(file, (long)(count * sizeof(float)), SEEK_CUR) != 0) return -1;
                    break;
                }
                if (!ctx->state.rmsprop.square_avg) {
                    ctx->state.rmsprop.square_avg = alloc_buffer(ctx, (size_t)count);
                    ctx->state.rmsprop.momentum_buffer = NULL;
                    ctx->state.rmsprop.grad_avg = NULL;
                    ctx->state.rmsprop.count = (size_t)count;
                }
                if (ctx->state.rmsprop.square_avg &&
                    fread(ctx->state.rmsprop.square_avg, sizeof(float), count, file) != count) return -1;
                bool has_momentum_buf = false;
                if (fread(&has_momentum_buf, sizeof(bool), 1, file) != 1) return -1;
                if (has_momentum_buf) {
                    if (!ctx->state.rmsprop.momentum_buffer) {
                        ctx->state.rmsprop.momentum_buffer = alloc_buffer(ctx, (size_t)count);
                    }
                    if (ctx->state.rmsprop.momentum_buffer &&
                        fread(ctx->state.rmsprop.momentum_buffer, sizeof(float), count, file) != count) return -1;
                }
                bool has_grad_avg = false;
                if (fread(&has_grad_avg, sizeof(bool), 1, file) != 1) return -1;
                if (has_grad_avg) {
                    if (!ctx->state.rmsprop.grad_avg) {
                        ctx->state.rmsprop.grad_avg = alloc_buffer(ctx, (size_t)count);
                    }
                    if (ctx->state.rmsprop.grad_avg &&
                        fread(ctx->state.rmsprop.grad_avg, sizeof(float), count, file) != count) return -1;
                }
            }
            break;
        }

        case NIMCP_OPTIMIZER_ADAGRAD: {
            uint64_t count = 0, adagrad_step = 0;
            if (fread(&count, sizeof(uint64_t), 1, file) != 1) return -1;
            if (fread(&adagrad_step, sizeof(uint64_t), 1, file) != 1) return -1;
            if (count > 0) {
                if (DIM_MISMATCH(ctx->state.adagrad.count, count)) {
                    NIMCP_LOGGING_WARN("optimizer_load: Adagrad dim mismatch "
                        "(file=%llu ctx=%zu) — skipping state restore",
                        (unsigned long long)count, ctx->state.adagrad.count);
                    if (fseek(file, (long)(count * sizeof(float)), SEEK_CUR) != 0) return -1;
                    break;
                }
                ctx->state.adagrad.step = adagrad_step;
                if (!ctx->state.adagrad.sum) {
                    ctx->state.adagrad.sum = alloc_buffer(ctx, (size_t)count);
                    ctx->state.adagrad.count = (size_t)count;
                }
                if (ctx->state.adagrad.sum &&
                    fread(ctx->state.adagrad.sum, sizeof(float), count, file) != count) return -1;
            } else {
                ctx->state.adagrad.step = adagrad_step;
            }
            break;
        }

        default:
            break;
    }
    #undef DIM_MISMATCH

    NIMCP_LOGGING_INFO("Restored optimizer state: type=%s, step=%lu, lr=%.6f",
                       nimcp_optimizer_type_name(ctx->config.type),
                       (unsigned long)ctx->step_count, ctx->current_lr);
    return 0;
}
