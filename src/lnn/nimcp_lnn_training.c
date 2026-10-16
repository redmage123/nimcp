/**
 * @file nimcp_lnn_training.c
 * @brief Training integration implementation for Liquid Neural Networks
 *
 * WHAT: Implements LNN training pipeline with full NIMCP integration
 * WHY:  Enable end-to-end training of continuous-time neural networks
 * HOW:  Wraps optimizer, gradient manager, loss, and training bridges
 */

#include "lnn/nimcp_lnn_training.h"
#include "training/nimcp_unified_training.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "lnn/nimcp_lnn.h"
#include "lnn/nimcp_lnn_gradient.h"
#include "lnn/nimcp_lnn_network.h"
#include "nimcp.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"
#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "middleware/immune/nimcp_training_immune.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/*=============================================================================
 * Health Agent Forward Declarations (Phase 8: Heartbeat for Long Operations)
 *===========================================================================*/
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_training)

/*=============================================================================
 * Substrate adapter globals (Phase 2 biological-substrate wiring).
 *
 * Three runtime-tunable knobs controlling how the LNN consumes substrate
 * state in the forward / optimizer paths. Defaults are "on, moderate":
 *   - enabled        : master switch (1.0 = read substrate; 0.0 = bypass).
 *   - update_period  : recompute substrate effects every N forward steps.
 *                      10 is a good balance given 100 ms substrate timescales.
 *   - tau_compose_on : multiplicative composition of substrate
 *                      membrane_time_constant_mod with the learned tau
 *                      tensor (post-compute_tau). When 0 the learned tau
 *                      is used as-is in the ODE decay term.
 *
 * Rationale for making these globals (matches the SNN adapter pattern from
 * commit 6bf4e6446): the effects are process-wide tuning rather than per-
 * instance config, identical values are read by every live LNN in the
 * address space, and the call sites are hot paths where a single atomic
 * read is cheaper than chasing a config pointer.
 *===========================================================================*/
static float g_lnn_substrate_enabled         = 1.0f;
static float g_lnn_substrate_update_period   = 10.0f;
static float g_lnn_substrate_tau_compose_on  = 1.0f;

/* Substrate adapter setters — booleans accept any nonzero as on; period
 * is clamped to [1, 10000] forward steps. Out-of-range period is ignored. */
void lnn_tune_set_substrate_enabled(float v) {
    g_lnn_substrate_enabled = (v != 0.0f) ? 1.0f : 0.0f;
}
void lnn_tune_set_substrate_update_period(float v) {
    if (v >= 1.0f && v <= 10000.0f) g_lnn_substrate_update_period = v;
}
void lnn_tune_set_substrate_tau_compose_on(float v) {
    g_lnn_substrate_tau_compose_on = (v != 0.0f) ? 1.0f : 0.0f;
}

/* Substrate adapter getters — mirror the setters. */
float lnn_tune_get_substrate_enabled(void)        { return g_lnn_substrate_enabled; }
float lnn_tune_get_substrate_update_period(void)  { return g_lnn_substrate_update_period; }
float lnn_tune_get_substrate_tau_compose_on(void) { return g_lnn_substrate_tau_compose_on; }

static float compute_step_schedule_lr(
    float base_lr,
    uint64_t step,
    uint32_t step_size,
    float gamma
) {
    uint64_t num_decays = step / step_size;
    return base_lr * powf(gamma, (float)num_decays);
}

/**
 * @brief Compute new LR for exponential schedule
 */
static float compute_exponential_schedule_lr(
    float base_lr,
    uint64_t step,
    float gamma
) {
    return base_lr * powf(gamma, (float)step);
}

/**
 * @brief Compute new LR for cosine schedule
 */
static float compute_cosine_schedule_lr(
    float base_lr,
    uint64_t step,
    uint64_t T_max
) {
    if (T_max == 0) return base_lr;
    float progress = (float)step / (float)T_max;
    if (progress > 1.0f) progress = 1.0f;
    return base_lr * 0.5f * (1.0f + cosf(NIMCP_PI_F * progress));
}

/**
 * @brief Compute new LR for warmup+cosine schedule
 */
static float compute_warmup_cosine_lr(
    float base_lr,
    uint64_t step,
    uint64_t warmup_steps,
    uint64_t T_max
) {
    if (step < warmup_steps) {
#include <stddef.h>  /* for NULL */

/* Default training hyperparameters */
#ifndef LNN_TRAINING_DEFAULT_LR
#define LNN_TRAINING_DEFAULT_LR NIMCP_LEARNING_RATE_FINE
#endif
#ifndef LNN_TRAINING_DEFAULT_WEIGHT_DECAY
#define LNN_TRAINING_DEFAULT_WEIGHT_DECAY NIMCP_WEIGHT_DECAY_DEFAULT
#endif
#ifndef LNN_TRAINING_DEFAULT_BETA1
#define LNN_TRAINING_DEFAULT_BETA1 NIMCP_ADAM_BETA1_DEFAULT
#endif
#ifndef LNN_TRAINING_DEFAULT_BETA2
#define LNN_TRAINING_DEFAULT_BETA2 NIMCP_ADAM_BETA2_DEFAULT
#endif
#ifndef LNN_TRAINING_DEFAULT_EPSILON
#define LNN_TRAINING_DEFAULT_EPSILON NIMCP_EPSILON_ADAM
#endif
#ifndef LNN_TRAINING_MAX_SCHEDULE_PARAMS
#define LNN_TRAINING_MAX_SCHEDULE_PARAMS 32
#endif

        // Linear warmup
        return base_lr * ((float)step / (float)warmup_steps);
    } else {
        // Cosine decay after warmup
        uint64_t cosine_step = step - warmup_steps;
        uint64_t cosine_T_max = (T_max > warmup_steps) ? (T_max - warmup_steps) : 1;
        return compute_cosine_schedule_lr(base_lr, cosine_step, cosine_T_max);
    }
}

/*=============================================================================
 * Thalamic adapter tunables (Phase 2 of multi-network thalamic rollout)
 *
 * These globals control how a network attached via
 * lnn_network_attach_thalamic_router() interacts with the thalamic
 * router during forward steps. They are deliberately process-wide (not
 * per-network) so a single safety toggle applies uniformly across
 * every LNN in the process — same discipline as the SNN tunables.
 *
 * Default: all three enabled (1.0) so that attaching a router has a
 * visible effect out of the box.
 *===========================================================================*/
static float g_lnn_thalamic_enabled              = 1.0f;  /* 1 = channel-aware forward path */
static float g_lnn_thalamic_input_gain_on        = 1.0f;  /* 1 = attention scales input vector */
static float g_lnn_thalamic_burst_tau_clamp_on   = 1.0f;  /* 1 = burst mode clamps tau low */

/* Setters — accept any nonzero value as on, zero as off. Matches SNN
 * tunable convention (snn_tune_set_substrate_enabled et al.). */
void lnn_tune_set_thalamic_enabled(float v) {
    g_lnn_thalamic_enabled = (v != 0.0f) ? 1.0f : 0.0f;
}
void lnn_tune_set_thalamic_input_gain_on(float v) {
    g_lnn_thalamic_input_gain_on = (v != 0.0f) ? 1.0f : 0.0f;
}
void lnn_tune_set_thalamic_burst_tau_clamp_on(float v) {
    g_lnn_thalamic_burst_tau_clamp_on = (v != 0.0f) ? 1.0f : 0.0f;
}

/* Getters — mirror the setters for diagnostic queries. */
float lnn_tune_get_thalamic_enabled(void) {
    return g_lnn_thalamic_enabled;
}
float lnn_tune_get_thalamic_input_gain_on(void) {
    return g_lnn_thalamic_input_gain_on;
}
float lnn_tune_get_thalamic_burst_tau_clamp_on(void) {
    return g_lnn_thalamic_burst_tau_clamp_on;
}

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

int lnn_training_config_default(lnn_training_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        return LNN_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(lnn_training_config_t));

    /* Optimizer defaults (Adam) */
    config->optimizer_type = NIMCP_OPTIMIZER_ADAM;
    config->learning_rate = LNN_TRAINING_DEFAULT_LR;
    config->weight_decay = LNN_TRAINING_DEFAULT_WEIGHT_DECAY;
    config->beta1 = LNN_TRAINING_DEFAULT_BETA1;
    config->beta2 = LNN_TRAINING_DEFAULT_BETA2;
    config->epsilon = LNN_TRAINING_DEFAULT_EPSILON;

    /* Gradient manager defaults */
    config->gradient_clip_norm = NIMCP_GRADIENT_CLIP_DEFAULT;  /* Enable gradient clipping by default */
    config->use_gradient_scaling = false;
    config->accum_mode = NIMCP_GRAD_ACCUM_MEAN;
    config->accumulation_steps = 1;

    /* Loss defaults (MSE) */
    config->loss_type = NIMCP_LOSS_MSE;
    config->reduction = NIMCP_LOSS_REDUCE_MEAN;

    /* LNN-specific defaults (adjoint method) */
    config->lnn_train_mode = LNN_TRAIN_ADJOINT;
    config->bptt_truncation = 100;
    config->use_adjoint_checkpointing = true;

    /* LR schedule defaults (constant) */
    config->lr_schedule = LNN_LR_SCHEDULE_CONSTANT;
    config->n_schedule_params = 0;

    /* Integration defaults (all disabled) */
    config->enable_cognitive_integration = false;
    config->enable_logic_integration = false;
    config->enable_immune_integration = false;
    config->enable_plasticity_integration = false;
    config->enable_bio_async = false;

    /* Training behavior defaults */
    config->use_mixed_precision = false;
    config->validate_gradients = true;
    config->track_statistics = true;

    /* Anti-collapse defaults */
    config->diversity_loss_weight = 0.1f;
    config->diversity_buffer_size = 16;
    config->use_gradient_normalization = true;
    config->gradient_target_norm = 1.0f;
    config->max_adjoint_steps = 200;
    config->lr_scale_tau = 0.1f;
    config->lr_scale_bias = 2.0f;

    return LNN_SUCCESS;
}

lnn_training_ctx_t* lnn_training_create(
    lnn_network_t* network,
    const lnn_training_config_t* config
) {
    if (!network) {
        NIMCP_LOGGING_ERROR("NULL network pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_training_create: network is NULL");
        return NULL;
    }

    /* Use defaults if no config provided */
    lnn_training_config_t default_config;
    if (!config) {
        lnn_training_config_default(&default_config);
        config = &default_config;
    }

    /* Validate config */
    if (lnn_training_validate_config(config) != LNN_SUCCESS) {
        NIMCP_LOGGING_ERROR("Invalid training configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_training_create: validation failed");
        return NULL;
    }

    /* Allocate context */
    lnn_training_ctx_t* ctx = (lnn_training_ctx_t*)nimcp_malloc(sizeof(lnn_training_ctx_t));
    if (!ctx) {
        NIMCP_LOGGING_ERROR("Failed to allocate training context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_training_create: failed to allocate training context");
        return NULL;
    }
    memset(ctx, 0, sizeof(lnn_training_ctx_t));

    /* Store network reference */
    ctx->network = network;

    /* Copy config */
    memcpy(&ctx->config, config, sizeof(lnn_training_config_t));

    /* Initialize anti-collapse state */
    {
        nimcp_anti_collapse_state_t* ac = (nimcp_anti_collapse_state_t*)
            nimcp_calloc(1, sizeof(nimcp_anti_collapse_state_t));
        if (ac) {
            nimcp_anti_collapse_config_t ac_cfg = {
                .diversity_loss_weight = config->diversity_loss_weight,
                .diversity_buffer_size = config->diversity_buffer_size,
                .use_gradient_normalization = config->use_gradient_normalization,
                .gradient_target_norm = config->gradient_target_norm,
                .gradient_clip_value = config->gradient_clip_norm,
                .adaptive_gradient_target = true,
            };
            nimcp_anti_collapse_init(ac, &ac_cfg);
        }
        ctx->anti_collapse_state = ac;
    }

    /* Initialize state */
    ctx->step_count = 0;
    ctx->epoch_count = 0;
    ctx->current_lr = config->learning_rate;
    ctx->current_loss = 0.0f;

    /* Create optimizer */
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(config->optimizer_type);
    if (config->optimizer_type == NIMCP_OPTIMIZER_ADAM) {
        opt_config.params.adam.learning_rate = config->learning_rate;
        opt_config.params.adam.beta1 = config->beta1;
        opt_config.params.adam.beta2 = config->beta2;
        opt_config.params.adam.epsilon = config->epsilon;
        opt_config.params.adam.weight_decay = config->weight_decay;
    } else if (config->optimizer_type == NIMCP_OPTIMIZER_SGD) {
        opt_config.params.sgd.learning_rate = config->learning_rate;
        opt_config.params.sgd.weight_decay = config->weight_decay;
    }
    opt_config.clip_gradients = (config->gradient_clip_norm > 0);
    opt_config.gradient_clip_norm = config->gradient_clip_norm;

    ctx->optimizer = nimcp_optimizer_create(&opt_config, NULL, NULL);
    if (!ctx->optimizer) {
        NIMCP_LOGGING_ERROR("Failed to create optimizer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_training_create: failed to create optimizer");
        lnn_training_destroy(ctx);
        return NULL;
    }

    /* Create gradient manager */
    nimcp_gradient_manager_config_t grad_config = nimcp_gradient_manager_default_config();
    grad_config.use_accumulation = (config->accumulation_steps > 1);
    grad_config.accumulation.accumulation_steps = config->accumulation_steps;
    grad_config.accumulation.mode = config->accum_mode;
    grad_config.use_scaling = config->use_gradient_scaling;
    grad_config.check_nan_inf = config->validate_gradients;
    grad_config.track_statistics = config->track_statistics;

    ctx->gradient_manager = nimcp_gradient_manager_create(&grad_config);
    if (!ctx->gradient_manager) {
        NIMCP_LOGGING_ERROR("Failed to create gradient manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_training_create: failed to create gradient manager");
        lnn_training_destroy(ctx);
        return NULL;
    }

    /* Create loss context */
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(config->loss_type);
    if (config->loss_type == NIMCP_LOSS_MSE) {
        loss_config.params.mse.reduction = config->reduction;
        loss_config.params.mse.compute_gradient = true;
    }
    loss_config.clip_gradients = (config->gradient_clip_norm > 0);
    loss_config.gradient_clip_value = config->gradient_clip_norm;

    ctx->loss_context = nimcp_loss_create(&loss_config, NULL, NULL);
    if (!ctx->loss_context) {
        NIMCP_LOGGING_ERROR("Failed to create loss context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_training_create: failed to create loss context");
        lnn_training_destroy(ctx);
        return NULL;
    }

    /* Create gradient context for LNN adjoint computation */
    uint32_t max_steps = config->bptt_truncation > 0 ? config->bptt_truncation : 100;
    ctx->gradient_ctx = lnn_gradient_ctx_create(
        network,
        max_steps,
        config->use_adjoint_checkpointing,
        10  /* checkpoint_interval */
    );
    if (ctx->gradient_ctx) {
        ctx->gradient_ctx->max_adjoint_steps = config->max_adjoint_steps;
    } else {
        NIMCP_LOGGING_WARN("Failed to create gradient context, training may fail");
        /* Non-fatal: some use cases may not need gradients */
    }

    /* Create output and gradient buffers */
    uint32_t output_size = network->n_outputs;
    if (output_size > 0) {
        uint32_t dims[1] = { output_size };
        ctx->output_buffer = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!ctx->output_buffer) {
            NIMCP_LOGGING_ERROR("Failed to allocate output buffer");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_training_create: failed to allocate output buffer");
            lnn_training_destroy(ctx);
            return NULL;
        }
        ctx->loss_gradient = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!ctx->loss_gradient) {
            NIMCP_LOGGING_ERROR("Failed to allocate loss gradient buffer");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_training_create: failed to allocate loss gradient buffer");
            lnn_training_destroy(ctx);
            return NULL;
        }
    } else {
        ctx->output_buffer = NULL;
        ctx->loss_gradient = NULL;
    }

    /* Initialize LR schedule */
    ctx->lr_schedule = config->lr_schedule;
    if (config->n_schedule_params > 0) {
        ctx->n_schedule_params = config->n_schedule_params;
        ctx->lr_schedule_params = (float*)nimcp_malloc(config->n_schedule_params * sizeof(float));
        if (ctx->lr_schedule_params) {
            memcpy(ctx->lr_schedule_params, config->lr_schedule_params,
                   config->n_schedule_params * sizeof(float));
        }
    }
    ctx->last_lr_update_step = 0;
    ctx->plateau_best_metric = FLT_MAX;
    ctx->plateau_patience_count = 0;

    /* Initialize gradient accumulation */
    ctx->accumulate_gradients = (config->accumulation_steps > 1);
    ctx->accumulation_steps = config->accumulation_steps;
    ctx->current_accumulation = 0;

    /* Initialize bridges to NULL (connected later) */
    ctx->cognitive_training_bridge = NULL;
    ctx->training_logic_bridge = NULL;
    ctx->training_immune_system = NULL;
    ctx->training_plasticity_bridge = NULL;

    /* Initialize bio-async */
    ctx->bio_async_enabled = false;
    ctx->bio_ctx = NULL;

    /* Initialize callbacks */
    ctx->on_step_complete = NULL;
    ctx->on_epoch_complete = NULL;
    ctx->on_lr_change = NULL;
    ctx->callback_user_data = NULL;

    /* Initialize statistics */
    memset(&ctx->stats, 0, sizeof(lnn_training_stats_t));
    ctx->stats.min_loss = FLT_MAX;
    ctx->stats.max_loss = -FLT_MAX;
    ctx->stats.min_gradient_norm = FLT_MAX;
    ctx->stats.max_gradient_norm = -FLT_MAX;
    ctx->stats.current_lr = config->learning_rate;
    ctx->stats.min_lr = config->learning_rate;
    ctx->stats.max_lr = config->learning_rate;
    ctx->stats.avg_tau_network = 0.0f;
    ctx->stats.min_tau_network = FLT_MAX;
    ctx->stats.max_tau_network = -FLT_MAX;

    /* Set network to training mode */
    lnn_set_training(network, true);

    NIMCP_LOGGING_INFO("Created LNN training context with %s optimizer, %s loss",
                       nimcp_optimizer_type_name(config->optimizer_type),
                       nimcp_loss_type_name(config->loss_type));

    return ctx;
}

void lnn_training_destroy(lnn_training_ctx_t* ctx) {
    if (!ctx) return;

    /* Destroy optimizer */
    if (ctx->optimizer) {
        nimcp_optimizer_destroy(ctx->optimizer);
    }

    /* Destroy gradient manager */
    if (ctx->gradient_manager) {
        nimcp_gradient_manager_destroy(ctx->gradient_manager);
    }

    /* Destroy loss context */
    if (ctx->loss_context) {
        nimcp_loss_destroy(ctx->loss_context);
    }

    /* Destroy gradient context */
    if (ctx->gradient_ctx) {
        lnn_gradient_ctx_destroy(ctx->gradient_ctx);
    }

    /* Destroy buffers */
    if (ctx->output_buffer) {
        nimcp_tensor_destroy(ctx->output_buffer);
    }
    if (ctx->loss_gradient) {
        nimcp_tensor_destroy(ctx->loss_gradient);
    }

    /* Free LR schedule params */
    if (ctx->lr_schedule_params) {
        nimcp_free(ctx->lr_schedule_params);
    }

    /* Destroy anti-collapse state */
    if (ctx->anti_collapse_state) {
        nimcp_anti_collapse_destroy((nimcp_anti_collapse_state_t*)ctx->anti_collapse_state);
        nimcp_free(ctx->anti_collapse_state);
        ctx->anti_collapse_state = NULL;
    }

    /* Note: We don't destroy bridges - they're owned externally */

    /* Free context */
    nimcp_free(ctx);

    NIMCP_LOGGING_INFO("Destroyed LNN training context");
}

/*=============================================================================
 * Training Step Functions
 *===========================================================================*/

int lnn_training_step(
    lnn_training_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* targets,
    float* loss_out
) {
    if (!ctx || !inputs || !targets) {
        NIMCP_LOGGING_ERROR("NULL pointer in lnn_training_step");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Phase 8: Send heartbeat at start of training step */
    lnn_training_heartbeat("lnn_train_step", 0.0f);

    uint64_t step_start_ms = nimcp_time_get_ms();
    float loss = 0.0f;
    float grad_norm = 0.0f;

    /* 1. Forward pass through network */
    float dt = ctx->network->config ? ctx->network->config->default_dt : 1.0f;
    int result = lnn_network_forward_step(ctx->network, inputs, ctx->output_buffer, dt);
    if (result != 0) {
        NIMCP_LOGGING_ERROR("Forward pass failed: %d", result);
        return result;
    }

    /* 2. Compute loss using NIMCP loss functions */
    if (ctx->loss_context && ctx->output_buffer) {
        const float* predictions = (const float*)nimcp_tensor_data(ctx->output_buffer);
        const float* target_data = (const float*)nimcp_tensor_data_const(targets);
        size_t n_outputs = nimcp_tensor_numel(ctx->output_buffer);

        nimcp_loss_result_t loss_result_data = {0};
        nimcp_result_t loss_ret = nimcp_loss_forward(
            ctx->loss_context,
            predictions,
            target_data,
            1,  /* batch_size */
            n_outputs,
            &loss_result_data
        );

        if (loss_ret == NIMCP_OK) {
            loss = loss_result_data.loss_value;
        } else {
            NIMCP_LOGGING_WARN("Loss computation failed: %d", loss_ret);
        }

        /* Compute loss gradient (dL/dx) for backward pass */
        if (ctx->loss_gradient) {
            float* loss_grad_data = (float*)nimcp_tensor_data(ctx->loss_gradient);
            nimcp_loss_backward(ctx->loss_context, predictions, target_data,
                               1, n_outputs, loss_grad_data);
        }
    }

    /* 2b. Compute diversity loss to prevent mode collapse (unified anti-collapse) */
    /* Diversity loss — skip when managed by UTM (UTM applies centrally) */
    if (ctx->anti_collapse_state && ctx->output_buffer && !ctx->managed_by_utm) {
        const float* predictions = (const float*)nimcp_tensor_data_const(ctx->output_buffer);
        size_t out_dim = nimcp_tensor_numel(ctx->output_buffer);
        float* loss_grad = ctx->loss_gradient ?
            (float*)nimcp_tensor_data(ctx->loss_gradient) : NULL;

        float div_loss = nimcp_anti_collapse_diversity_loss(
            (nimcp_anti_collapse_state_t*)ctx->anti_collapse_state,
            predictions, loss_grad, (uint32_t)out_dim);
        loss += div_loss;
    }

    /* 3. Backward pass using adjoint method for LNN */
    if (ctx->gradient_ctx && ctx->loss_gradient) {
        result = lnn_gradient_compute_adjoint(ctx->gradient_ctx, ctx->network, ctx->loss_gradient);
        if (result != 0) {
            NIMCP_LOGGING_WARN("Gradient computation failed: %d", result);
            /* Reset ALL gradient state to prevent NaN propagation into optimizer.
             * A failed adjoint leaves NaN in both the context accumulator AND
             * the per-layer grad_W_in/grad_W_rec/etc. tensors (written by
             * accumulate_parameter_gradients during partial backward pass). */
            lnn_gradient_reset(ctx->gradient_ctx);
            lnn_network_zero_gradients(ctx->network);
            grad_norm = 0.0f;
        } else if (!ctx->managed_by_utm) {
            /* Normalize or clip gradients — skip when UTM manages globally.
             * When UTM-managed, the LNN adapter applies gradient normalization
             * after lnn_backward() instead. */
            if (ctx->config.use_gradient_normalization) {
                float current_norm = lnn_gradient_norm(ctx->gradient_ctx);
                if (current_norm > 1e-8f) {
                    float target = ctx->config.gradient_target_norm;
                    if (target <= 0.0f) target = 1.0f;
                    float scale = target / current_norm;
                    lnn_gradient_scale(ctx->gradient_ctx, scale);
                }
                grad_norm = ctx->config.gradient_target_norm;
            } else {
                float clip_norm = ctx->config.gradient_clip_norm;
                if (clip_norm <= 0.0f) clip_norm = 1.0f;
                lnn_gradient_clip(ctx->gradient_ctx, clip_norm);
                grad_norm = lnn_gradient_norm(ctx->gradient_ctx);
            }
        }
    }

    /* 4. Accumulate gradients if needed */
    ctx->current_accumulation++;

    bool should_update = !ctx->accumulate_gradients ||
                         (ctx->current_accumulation >= ctx->accumulation_steps);

    if (should_update && !ctx->managed_by_utm) {
        /* 5. Apply gradients via optimizer — skip when UTM steps all params */
        if (ctx->optimizer && ctx->network) {
            size_t n_params = lnn_network_param_count(ctx->network);
            if (n_params > 0) {
                float* params = (float*)nimcp_malloc(n_params * sizeof(float));
                float* grads = (float*)nimcp_malloc(n_params * sizeof(float));

                if (params && grads) {
                    size_t actual_params = 0;
                    size_t actual_grads = 0;

                    if (lnn_network_get_params(ctx->network, params, &actual_params) == 0 &&
                        lnn_network_get_gradients(ctx->network, grads, &actual_grads) == 0 &&
                        actual_params == actual_grads) {

                        /* Layer-wise learning rates: apply different LR scales
                         * for weight, tau, and bias parameter groups */
                        float lr_scale_tau = ctx->config.lr_scale_tau;
                        float lr_scale_bias = ctx->config.lr_scale_bias;

                        if (lr_scale_tau != 1.0f || lr_scale_bias != 1.0f) {
                            /* Scale gradients by parameter type before optimizer step.
                             * Parameter layout per layer: W_in, W_rec, W_tau, b_in, b_tau, tau_base
                             * We scale tau_base grads by lr_scale_tau and bias grads by lr_scale_bias */
                            size_t offset = 0;
                            for (uint32_t li = 0; li < ctx->network->n_layers; li++) {
                                lnn_layer_t* layer = ctx->network->layers[li];
                                if (!layer) continue;

                                /* W_in: n * n_inputs (keep scale 1.0) */
                                size_t w_in_size = layer->W_in ? nimcp_tensor_numel(layer->W_in) : 0;
                                offset += w_in_size;

                                /* W_rec: n * n (keep scale 1.0) */
                                size_t w_rec_size = layer->W_rec ? nimcp_tensor_numel(layer->W_rec) : 0;
                                offset += w_rec_size;

                                /* W_tau: variable (scale by lr_scale_tau) */
                                size_t w_tau_size = layer->W_tau ? nimcp_tensor_numel(layer->W_tau) : 0;
                                for (size_t i = offset; i < offset + w_tau_size; i++) {
                                    grads[i] *= lr_scale_tau;
                                }
                                offset += w_tau_size;

                                /* b_in: n (scale by lr_scale_bias) */
                                size_t b_in_size = layer->b_in ? nimcp_tensor_numel(layer->b_in) : 0;
                                for (size_t i = offset; i < offset + b_in_size; i++) {
                                    grads[i] *= lr_scale_bias;
                                }
                                offset += b_in_size;

                                /* b_tau: n (scale by lr_scale_tau) */
                                size_t b_tau_size = layer->b_tau ? nimcp_tensor_numel(layer->b_tau) : 0;
                                for (size_t i = offset; i < offset + b_tau_size; i++) {
                                    grads[i] *= lr_scale_tau;
                                }
                                offset += b_tau_size;

                                /* tau_base: n (scale by lr_scale_tau) */
                                size_t tau_size = layer->tau_base ? nimcp_tensor_numel(layer->tau_base) : 0;
                                for (size_t i = offset; i < offset + tau_size; i++) {
                                    grads[i] *= lr_scale_tau;
                                }
                                offset += tau_size;
                            }
                        }

                        /* Substrate-modulated plasticity (Phase 2).
                         * When the network has a substrate attached AND the
                         * master enabled knob is on, scale ALL gradients by
                         * the cached dendrite plasticity_mod (linear in ATP
                         * at the helper). This is the LR-path hook: at
                         * baseline (atp=1) mod=1 so no-op; depleted
                         * substrate shrinks the effective LR uniformly.
                         * Biological fidelity: tired circuits learn slower.
                         * We scale gradients (not the optimizer's LR) so
                         * Adam's per-parameter moments are not perturbed
                         * by a transient LR change — the effective step
                         * size still tracks the substrate signal. */
                        if (ctx->network->substrate &&
                            lnn_tune_get_substrate_enabled() != 0.0f) {
                            const float plast_mod =
                                ctx->network->cached_dend_effects.plasticity_mod;
                            /* plast_mod is clamped [0,1] in the helper so
                             * values outside that range mean the cache was
                             * never populated — skip rather than apply a
                             * meaningless zero. */
                            if (plast_mod >= 0.0f && plast_mod <= 1.0f &&
                                plast_mod != 1.0f) {
                                for (size_t i = 0; i < actual_grads; i++) {
                                    grads[i] *= plast_mod;
                                }
                            }
                        }

                        /* Apply optimizer step */
                        nimcp_optimizer_step(ctx->optimizer, params, grads, actual_params);
                        lnn_network_set_params(ctx->network, params, actual_params);
                    }

                    lnn_network_zero_gradients(ctx->network);
                }

                if (params) nimcp_free(params);
                if (grads) nimcp_free(grads);
            }

            if (ctx->gradient_ctx) {
                lnn_gradient_reset(ctx->gradient_ctx);
            }
        }

        /* Reset accumulation counter */
        ctx->current_accumulation = 0;

        /* 6. Update LR schedule */
        lnn_training_update_lr(ctx);
    }

    /* Update statistics */
    ctx->step_count++;
    ctx->current_loss = loss;
    ctx->stats.step_count = ctx->step_count;
    ctx->stats.current_loss = loss;
    ctx->stats.total_loss += (double)loss;
    ctx->stats.total_gradient_norm += (double)grad_norm;

    if (loss < ctx->stats.min_loss) ctx->stats.min_loss = loss;
    if (loss > ctx->stats.max_loss) ctx->stats.max_loss = loss;
    if (grad_norm < ctx->stats.min_gradient_norm) ctx->stats.min_gradient_norm = grad_norm;
    if (grad_norm > ctx->stats.max_gradient_norm) ctx->stats.max_gradient_norm = grad_norm;

    /* Update average loss and gradient norm */
    ctx->stats.avg_loss = (float)(ctx->stats.total_loss / (double)ctx->stats.step_count);
    ctx->stats.avg_gradient_norm = (float)(ctx->stats.total_gradient_norm / (double)ctx->stats.step_count);

    /* Track timing */
    uint64_t step_end_ms = nimcp_time_get_ms();
    (void)step_start_ms; /* Avoid unused warning if timing not tracked */
    (void)step_end_ms;

    /* 7. Call step callback */
    if (ctx->on_step_complete) {
        ctx->on_step_complete(ctx->callback_user_data, ctx->step_count, loss);
    }

    /* 8. Update training bridges */
    if (ctx->cognitive_training_bridge) {
        cognitive_training_update_metrics(ctx->cognitive_training_bridge,
                                         loss, grad_norm, ctx->current_lr, ctx->step_count);
    }
    if (ctx->training_logic_bridge) {
        training_logic_update_metrics(ctx->training_logic_bridge,
                                     loss, grad_norm, ctx->current_lr, ctx->step_count);
    }
    if (ctx->training_immune_system) {
        training_immune_update_metrics(ctx->training_immune_system,
                                      loss, grad_norm, ctx->current_lr);
    }

    /* Output loss if requested */
    if (loss_out) {
        *loss_out = loss;
    }

    return LNN_SUCCESS;
}

int lnn_training_step_batch(
    lnn_training_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    const nimcp_tensor_t* targets,
    uint32_t batch_size,
    float* loss_out
) {
    if (!ctx || !inputs || !targets) {
        NIMCP_LOGGING_ERROR("NULL pointer in lnn_training_step_batch");
        return LNN_ERROR_NULL_POINTER;
    }

    if (batch_size == 0) {
        NIMCP_LOGGING_ERROR("Invalid batch size: 0");
        return LNN_ERROR_INVALID_DIMENSION;
    }

    /* Process batch */
    float total_loss = 0.0f;

    /* Get input and target dimensions */
    const nimcp_tensor_shape_t* input_shape = nimcp_tensor_shape(inputs);
    const nimcp_tensor_shape_t* target_shape = nimcp_tensor_shape(targets);

    if (!input_shape || !target_shape) {
        NIMCP_LOGGING_ERROR("Invalid tensor shapes in batch processing");
        return LNN_ERROR_INVALID_DIMENSION;
    }

    /* Validate batch dimension */
    if (input_shape->rank < 2 || target_shape->rank < 2) {
        NIMCP_LOGGING_ERROR("Batch tensors must have rank >= 2 (batch dimension required)");
        return LNN_ERROR_INVALID_DIMENSION;
    }

    uint32_t feature_dim = input_shape->dims[1];
    uint32_t output_dim = target_shape->dims[1];

    /* Sequential batch processing */
    for (uint32_t i = 0; i < batch_size; i++) {
        float sample_loss = 0.0f;

        /* Extract sample i from batch */
        nimcp_tensor_t* sample_input = nimcp_tensor_create((uint32_t[]){feature_dim}, 1, NIMCP_DTYPE_F32);
        nimcp_tensor_t* sample_target = nimcp_tensor_create((uint32_t[]){output_dim}, 1, NIMCP_DTYPE_F32);

        if (!sample_input || !sample_target) {
            NIMCP_LOGGING_ERROR("Failed to allocate batch sample tensors");
            if (sample_input) nimcp_tensor_destroy(sample_input);
            if (sample_target) nimcp_tensor_destroy(sample_target);
            continue;
        }

        /* Copy sample data (row i from batch) */
        const float* input_data = (const float*)nimcp_tensor_data_const(inputs);
        const float* target_data = (const float*)nimcp_tensor_data_const(targets);
        float* sample_input_data = (float*)nimcp_tensor_data(sample_input);
        float* sample_target_data = (float*)nimcp_tensor_data(sample_target);

        memcpy(sample_input_data, input_data + i * feature_dim, feature_dim * sizeof(float));
        memcpy(sample_target_data, target_data + i * output_dim, output_dim * sizeof(float));

        /* Process sample through training step */
        int ret = lnn_training_step(ctx, sample_input, sample_target, &sample_loss);

        nimcp_tensor_destroy(sample_input);
        nimcp_tensor_destroy(sample_target);

        if (ret != LNN_SUCCESS) {
            NIMCP_LOGGING_WARN("Training step failed for batch sample %u", i);
            continue;
        }

        total_loss += sample_loss;
    }

    float avg_loss = total_loss / (float)batch_size;

    if (loss_out) {
        *loss_out = avg_loss;
    }

    return LNN_SUCCESS;
}

int lnn_training_epoch(
    lnn_training_ctx_t* ctx,
    const nimcp_tensor_t* dataset_inputs,
    const nimcp_tensor_t* dataset_targets,
    uint32_t n_samples,
    uint32_t batch_size,
    float* avg_loss_out
) {
    if (!ctx || !dataset_inputs || !dataset_targets) {
        NIMCP_LOGGING_ERROR("NULL pointer in lnn_training_epoch");
        return LNN_ERROR_NULL_POINTER;
    }

    if (n_samples == 0 || batch_size == 0) {
        NIMCP_LOGGING_ERROR("Invalid n_samples or batch_size");
        return LNN_ERROR_INVALID_DIMENSION;
    }

    float epoch_loss = 0.0f;
    uint32_t num_batches = (n_samples + batch_size - 1) / batch_size;

    /* Get input and target dimensions */
    const nimcp_tensor_shape_t* input_shape = nimcp_tensor_shape(dataset_inputs);
    const nimcp_tensor_shape_t* target_shape = nimcp_tensor_shape(dataset_targets);

    if (!input_shape || !target_shape) {
        NIMCP_LOGGING_ERROR("Invalid dataset tensor shapes");
        return LNN_ERROR_INVALID_DIMENSION;
    }

    /* Validate dataset has batch dimension */
    if (input_shape->rank < 2 || target_shape->rank < 2) {
        NIMCP_LOGGING_ERROR("Dataset tensors must have rank >= 2");
        return LNN_ERROR_INVALID_DIMENSION;
    }

    uint32_t feature_dim = input_shape->dims[1];
    uint32_t output_dim = target_shape->dims[1];

    const float* all_inputs = (const float*)nimcp_tensor_data_const(dataset_inputs);
    const float* all_targets = (const float*)nimcp_tensor_data_const(dataset_targets);

    /* Process each batch */
    for (uint32_t batch_idx = 0; batch_idx < num_batches; batch_idx++) {
        /* Phase 8: Send heartbeat with batch progress */
        lnn_training_heartbeat("lnn_train_epoch", (float)batch_idx / (float)num_batches);

        uint32_t batch_start = batch_idx * batch_size;
        uint32_t batch_end = batch_start + batch_size;
        if (batch_end > n_samples) batch_end = n_samples;
        uint32_t actual_batch_size = batch_end - batch_start;

        /* Extract batch from dataset */
        nimcp_tensor_t* batch_inputs = nimcp_tensor_create(
            (uint32_t[]){actual_batch_size, feature_dim}, 2, NIMCP_DTYPE_F32);
        nimcp_tensor_t* batch_targets = nimcp_tensor_create(
            (uint32_t[]){actual_batch_size, output_dim}, 2, NIMCP_DTYPE_F32);

        if (!batch_inputs || !batch_targets) {
            NIMCP_LOGGING_ERROR("Failed to allocate batch tensors");
            if (batch_inputs) nimcp_tensor_destroy(batch_inputs);
            if (batch_targets) nimcp_tensor_destroy(batch_targets);
            return LNN_ERROR_OUT_OF_MEMORY;
        }

        /* Copy batch data */
        float* batch_input_data = (float*)nimcp_tensor_data(batch_inputs);
        float* batch_target_data = (float*)nimcp_tensor_data(batch_targets);

        memcpy(batch_input_data, all_inputs + batch_start * feature_dim,
               actual_batch_size * feature_dim * sizeof(float));
        memcpy(batch_target_data, all_targets + batch_start * output_dim,
               actual_batch_size * output_dim * sizeof(float));

        /* Process batch */
        float batch_loss = 0.0f;
        int ret = lnn_training_step_batch(ctx, batch_inputs, batch_targets,
                                          actual_batch_size, &batch_loss);

        nimcp_tensor_destroy(batch_inputs);
        nimcp_tensor_destroy(batch_targets);

        if (ret != LNN_SUCCESS) {
            NIMCP_LOGGING_WARN("Batch %u/%u failed with error %d", batch_idx + 1, num_batches, ret);
            continue;
        }

        epoch_loss += batch_loss;
    }

    float avg_loss = epoch_loss / (float)num_batches;

    /* Update epoch statistics */
    ctx->epoch_count++;
    ctx->stats.epoch_count = ctx->epoch_count;

    /* Call epoch callback */
    if (ctx->on_epoch_complete) {
        ctx->on_epoch_complete(ctx->callback_user_data, ctx->epoch_count, avg_loss);
    }

    if (avg_loss_out) {
        *avg_loss_out = avg_loss;
    }

    return LNN_SUCCESS;
}

/*=============================================================================
 * Integration Connection Functions
 *===========================================================================*/

int lnn_training_connect_cognitive(
    lnn_training_ctx_t* ctx,
    cognitive_training_bridge_t* cognitive_bridge
) {
    if (!ctx) {
        NIMCP_LOGGING_ERROR("NULL context in lnn_training_connect_cognitive");
        return LNN_ERROR_NULL_POINTER;
    }

    ctx->cognitive_training_bridge = cognitive_bridge;
    ctx->stats.cognitive_bridge_connected = (cognitive_bridge != NULL);

    if (cognitive_bridge) {
        NIMCP_LOGGING_INFO("Connected LNN training to cognitive bridge");
    } else {
        NIMCP_LOGGING_INFO("Disconnected LNN training from cognitive bridge");
    }

    return LNN_SUCCESS;
}

int lnn_training_connect_logic(
    lnn_training_ctx_t* ctx,
    training_logic_bridge_t* logic_bridge
) {
    if (!ctx) {
        NIMCP_LOGGING_ERROR("NULL context in lnn_training_connect_logic");
        return LNN_ERROR_NULL_POINTER;
    }

    ctx->training_logic_bridge = logic_bridge;
    ctx->stats.logic_bridge_connected = (logic_bridge != NULL);

    if (logic_bridge) {
        NIMCP_LOGGING_INFO("Connected LNN training to logic bridge");
    } else {
        NIMCP_LOGGING_INFO("Disconnected LNN training from logic bridge");
    }

    return LNN_SUCCESS;
}

int lnn_training_connect_immune(
    lnn_training_ctx_t* ctx,
    training_immune_system_t* immune_system
) {
    if (!ctx) {
        NIMCP_LOGGING_ERROR("NULL context in lnn_training_connect_immune");
        return LNN_ERROR_NULL_POINTER;
    }

    ctx->training_immune_system = immune_system;
    ctx->stats.immune_system_connected = (immune_system != NULL);

    if (immune_system) {
        NIMCP_LOGGING_INFO("Connected LNN training to immune system");
    } else {
        NIMCP_LOGGING_INFO("Disconnected LNN training from immune system");
    }

    return LNN_SUCCESS;
}

int lnn_training_connect_plasticity(
    lnn_training_ctx_t* ctx,
    training_plasticity_bridge_t* plasticity_bridge
) {
    if (!ctx) {
        NIMCP_LOGGING_ERROR("NULL context in lnn_training_connect_plasticity");
        return LNN_ERROR_NULL_POINTER;
    }

    ctx->training_plasticity_bridge = plasticity_bridge;
    ctx->stats.plasticity_bridge_connected = (plasticity_bridge != NULL);

    if (plasticity_bridge) {
        NIMCP_LOGGING_INFO("Connected LNN training to plasticity bridge");
    } else {
        NIMCP_LOGGING_INFO("Disconnected LNN training from plasticity bridge");
    }

    return LNN_SUCCESS;
}

/*=============================================================================
 * Learning Rate Scheduling Functions
 *===========================================================================*/

int lnn_training_set_lr_schedule(
    lnn_training_ctx_t* ctx,
    lnn_lr_schedule_t schedule,
    float* params,
    uint32_t n_params
) {
    if (!ctx) {
        NIMCP_LOGGING_ERROR("NULL context in lnn_training_set_lr_schedule");
        return LNN_ERROR_NULL_POINTER;
    }

    if (n_params > LNN_TRAINING_MAX_SCHEDULE_PARAMS) {
        NIMCP_LOGGING_ERROR("Too many schedule params: %u > %u",
                           n_params, LNN_TRAINING_MAX_SCHEDULE_PARAMS);
        return LNN_ERROR_INVALID_CONFIG;
    }

    /* Free old params */
    if (ctx->lr_schedule_params) {
        nimcp_free(ctx->lr_schedule_params);
        ctx->lr_schedule_params = NULL;
    }

    /* Set new schedule */
    ctx->lr_schedule = schedule;
    ctx->n_schedule_params = n_params;

    /* Copy params if provided */
    if (n_params > 0 && params) {
        ctx->lr_schedule_params = (float*)nimcp_malloc(n_params * sizeof(float));
        if (!ctx->lr_schedule_params) {
            NIMCP_LOGGING_ERROR("Failed to allocate schedule params");
            return LNN_ERROR_OUT_OF_MEMORY;
        }
        memcpy(ctx->lr_schedule_params, params, n_params * sizeof(float));
    }

    /* Reset schedule state */
    ctx->last_lr_update_step = ctx->step_count;
    ctx->plateau_best_metric = FLT_MAX;
    ctx->plateau_patience_count = 0;

    NIMCP_LOGGING_INFO("Set LR schedule to %s", lnn_lr_schedule_name(schedule));

    return LNN_SUCCESS;
}

int lnn_training_update_lr(lnn_training_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_LOGGING_ERROR("NULL context in lnn_training_update_lr");
        return LNN_ERROR_NULL_POINTER;
    }

    float old_lr = ctx->current_lr;
    float new_lr = old_lr;

    /* Compute new LR based on schedule */
    switch (ctx->lr_schedule) {
    case LNN_LR_SCHEDULE_CONSTANT:
        /* No change */
        break;

    case LNN_LR_SCHEDULE_STEP:
        if (ctx->n_schedule_params >= 2) {
            uint32_t step_size = (uint32_t)ctx->lr_schedule_params[0];
            float gamma = ctx->lr_schedule_params[1];
            new_lr = compute_step_schedule_lr(ctx->config.learning_rate,
                                               ctx->step_count, step_size, gamma);
        }
        break;

    case LNN_LR_SCHEDULE_EXPONENTIAL:
        if (ctx->n_schedule_params >= 1) {
            float gamma = ctx->lr_schedule_params[0];
            new_lr = compute_exponential_schedule_lr(ctx->config.learning_rate,
                                                      ctx->step_count, gamma);
        }
        break;

    case LNN_LR_SCHEDULE_COSINE:
        if (ctx->n_schedule_params >= 1) {
            uint64_t T_max = (uint64_t)ctx->lr_schedule_params[0];
            new_lr = compute_cosine_schedule_lr(ctx->config.learning_rate,
                                                 ctx->step_count, T_max);
        }
        break;

    case LNN_LR_SCHEDULE_WARMUP_COSINE:
        if (ctx->n_schedule_params >= 2) {
            uint64_t warmup_steps = (uint64_t)ctx->lr_schedule_params[0];
            uint64_t T_max = (uint64_t)ctx->lr_schedule_params[1];
            new_lr = compute_warmup_cosine_lr(ctx->config.learning_rate,
                                               ctx->step_count, warmup_steps, T_max);
        }
        break;

    case LNN_LR_SCHEDULE_REDUCE_ON_PLATEAU:
        if (ctx->n_schedule_params >= 3) {
            float factor = ctx->lr_schedule_params[0];
            uint32_t patience = (uint32_t)ctx->lr_schedule_params[1];
            float threshold = ctx->lr_schedule_params[2];

            /* Guard against NaN/Inf in current loss */
            if (isnan(ctx->current_loss) || isinf(ctx->current_loss)) {
                NIMCP_LOGGING_WARN("Invalid loss value (NaN/Inf) in plateau detection, skipping");
                break;
            }

            /* Check if current loss is better than best by threshold */
            if (ctx->current_loss < ctx->plateau_best_metric - threshold) {
                ctx->plateau_best_metric = ctx->current_loss;
                ctx->plateau_patience_count = 0;
            } else {
                ctx->plateau_patience_count++;

                /* Reduce LR if plateau detected */
                if (ctx->plateau_patience_count >= patience) {
                    new_lr = ctx->current_lr * factor;
                    ctx->plateau_patience_count = 0;
                    NIMCP_LOGGING_INFO("Plateau detected, reducing LR by factor %.2f", factor);
                }
            }
        }
        break;

    default:
        NIMCP_LOGGING_WARN("Unknown LR schedule type: %d", ctx->lr_schedule);
        break;
    }

    /* Update LR if changed */
    if (fabsf(new_lr - old_lr) > 1e-9f) {
        ctx->current_lr = new_lr;
        ctx->stats.current_lr = new_lr;
        ctx->stats.lr_updates++;

        /* Update min/max */
        if (new_lr < ctx->stats.min_lr) ctx->stats.min_lr = new_lr;
        if (new_lr > ctx->stats.max_lr) ctx->stats.max_lr = new_lr;

        /* Update optimizer LR */
        nimcp_optimizer_set_lr(ctx->optimizer, new_lr);

        /* Call LR change callback */
        if (ctx->on_lr_change) {
            ctx->on_lr_change(ctx->callback_user_data, old_lr, new_lr);
        }

        ctx->last_lr_update_step = ctx->step_count;
    }

    return LNN_SUCCESS;
}

float lnn_training_get_lr(const lnn_training_ctx_t* ctx) {
    if (!ctx) return 0.0f;
    return ctx->current_lr;
}

void lnn_training_set_lr(lnn_training_ctx_t* ctx, float lr) {
    if (!ctx) return;

    float old_lr = ctx->current_lr;
    ctx->current_lr = lr;
    ctx->stats.current_lr = lr;

    /* Update optimizer */
    nimcp_optimizer_set_lr(ctx->optimizer, lr);

    /* Call callback */
    if (ctx->on_lr_change) {
        ctx->on_lr_change(ctx->callback_user_data, old_lr, lr);
    }
}

/*=============================================================================
 * Callback Functions
 *===========================================================================*/

void lnn_training_set_step_callback(
    lnn_training_ctx_t* ctx,
    void (*callback)(void* user_data, uint64_t step, float loss),
    void* user_data
) {
    if (!ctx) return;
    ctx->on_step_complete = callback;
    ctx->callback_user_data = user_data;
}

void lnn_training_set_epoch_callback(
    lnn_training_ctx_t* ctx,
    void (*callback)(void* user_data, uint64_t epoch, float avg_loss),
    void* user_data
) {
    if (!ctx) return;
    ctx->on_epoch_complete = callback;
    ctx->callback_user_data = user_data;
}

void lnn_training_set_lr_callback(
    lnn_training_ctx_t* ctx,
    void (*callback)(void* user_data, float old_lr, float new_lr),
    void* user_data
) {
    if (!ctx) return;
    ctx->on_lr_change = callback;
    ctx->callback_user_data = user_data;
}

/*=============================================================================
 * Statistics Functions
 *===========================================================================*/

uint64_t lnn_training_get_step_count(const lnn_training_ctx_t* ctx) {
    if (!ctx) return 0;
    return ctx->step_count;
}

uint64_t lnn_training_get_epoch_count(const lnn_training_ctx_t* ctx) {
    if (!ctx) return 0;
    return ctx->epoch_count;
}

float lnn_training_get_current_loss(const lnn_training_ctx_t* ctx) {
    if (!ctx) return 0.0f;
    return ctx->current_loss;
}

int lnn_training_get_stats(
    const lnn_training_ctx_t* ctx,
    lnn_training_stats_t* stats
) {
    if (!ctx || !stats) {
        NIMCP_LOGGING_ERROR("NULL pointer in lnn_training_get_stats");
        return LNN_ERROR_NULL_POINTER;
    }

    memcpy(stats, &ctx->stats, sizeof(lnn_training_stats_t));
    return LNN_SUCCESS;
}

int lnn_training_reset_stats(lnn_training_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_LOGGING_ERROR("NULL context in lnn_training_reset_stats");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Preserve connection status */
    bool cognitive_connected = ctx->stats.cognitive_bridge_connected;
    bool logic_connected = ctx->stats.logic_bridge_connected;
    bool immune_connected = ctx->stats.immune_system_connected;
    bool plasticity_connected = ctx->stats.plasticity_bridge_connected;

    /* Reset all stats */
    memset(&ctx->stats, 0, sizeof(lnn_training_stats_t));

    /* Restore connection status */
    ctx->stats.cognitive_bridge_connected = cognitive_connected;
    ctx->stats.logic_bridge_connected = logic_connected;
    ctx->stats.immune_system_connected = immune_connected;
    ctx->stats.plasticity_bridge_connected = plasticity_connected;

    /* Reset min/max values */
    ctx->stats.min_loss = FLT_MAX;
    ctx->stats.max_loss = -FLT_MAX;
    ctx->stats.min_gradient_norm = FLT_MAX;
    ctx->stats.max_gradient_norm = -FLT_MAX;
    ctx->stats.min_tau_network = FLT_MAX;
    ctx->stats.max_tau_network = -FLT_MAX;
    ctx->stats.current_lr = ctx->current_lr;
    ctx->stats.min_lr = ctx->current_lr;
    ctx->stats.max_lr = ctx->current_lr;

    NIMCP_LOGGING_INFO("Reset LNN training statistics");

    return LNN_SUCCESS;
}

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

const char* lnn_lr_schedule_name(lnn_lr_schedule_t schedule) {
    switch (schedule) {
    case LNN_LR_SCHEDULE_CONSTANT:        return "CONSTANT";
    case LNN_LR_SCHEDULE_STEP:            return "STEP";
    case LNN_LR_SCHEDULE_EXPONENTIAL:     return "EXPONENTIAL";
    case LNN_LR_SCHEDULE_COSINE:          return "COSINE";
    case LNN_LR_SCHEDULE_WARMUP_COSINE:   return "WARMUP_COSINE";
    case LNN_LR_SCHEDULE_REDUCE_ON_PLATEAU: return "REDUCE_ON_PLATEAU";
    default:                              return "UNKNOWN";
    }
}

int lnn_training_validate_config(const lnn_training_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        return LNN_ERROR_NULL_POINTER;
    }

    /* Validate learning rate */
    if (config->learning_rate <= 0.0f || config->learning_rate > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid learning rate: %f", config->learning_rate);
        return LNN_ERROR_INVALID_CONFIG;
    }

    /* Validate weight decay */
    if (config->weight_decay < 0.0f || config->weight_decay > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid weight decay: %f", config->weight_decay);
        return LNN_ERROR_INVALID_CONFIG;
    }

    /* Validate Adam parameters */
    if (config->optimizer_type == NIMCP_OPTIMIZER_ADAM) {
        if (config->beta1 < 0.0f || config->beta1 >= 1.0f) {
            NIMCP_LOGGING_ERROR("Invalid beta1: %f", config->beta1);
            return LNN_ERROR_INVALID_CONFIG;
        }
        if (config->beta2 < 0.0f || config->beta2 >= 1.0f) {
            NIMCP_LOGGING_ERROR("Invalid beta2: %f", config->beta2);
            return LNN_ERROR_INVALID_CONFIG;
        }
        if (config->epsilon <= 0.0f) {
            NIMCP_LOGGING_ERROR("Invalid epsilon: %f", config->epsilon);
            return LNN_ERROR_INVALID_CONFIG;
        }
    }

    /* Validate gradient clipping */
    if (config->gradient_clip_norm < 0.0f) {
        NIMCP_LOGGING_ERROR("Invalid gradient clip norm: %f", config->gradient_clip_norm);
        return LNN_ERROR_INVALID_CONFIG;
    }

    /* Validate accumulation steps */
    if (config->accumulation_steps == 0) {
        NIMCP_LOGGING_ERROR("Invalid accumulation steps: 0");
        return LNN_ERROR_INVALID_CONFIG;
    }

    /* Validate BPTT truncation */
    if (config->bptt_truncation == 0) {
        NIMCP_LOGGING_ERROR("Invalid BPTT truncation: 0");
        return LNN_ERROR_INVALID_CONFIG;
    }

    /* Validate schedule params count */
    if (config->n_schedule_params > LNN_TRAINING_MAX_SCHEDULE_PARAMS) {
        NIMCP_LOGGING_ERROR("Too many schedule params: %u", config->n_schedule_params);
        return LNN_ERROR_INVALID_CONFIG;
    }

    return LNN_SUCCESS;
}

/**
 * @brief Connect network to external optimizer
 *
 * WHAT: Links network gradients to optimizer for parameter updates
 * WHY:  Enable gradient-based learning with various optimizers
 * HOW:  Store optimizer reference and register for gradient updates
 */
int lnn_training_connect_optimizer(lnn_network_t* network, void* optimizer) {
    if (!network) {
        NIMCP_LOGGING_ERROR("Null network in lnn_training_connect_optimizer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_training_connect_optimizer: network is NULL");
        return -1;
    }

    /* Store optimizer reference for gradient-based parameter updates */
    network->optimizer = optimizer;
    NIMCP_LOGGING_INFO("LNN network connected to optimizer");
    return 0;
}
