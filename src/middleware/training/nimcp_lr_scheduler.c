/**
 * @file nimcp_lr_scheduler.c
 * @brief Learning Rate Scheduler Implementation
 *
 * Implements all learning rate scheduling strategies:
 * - StepLR, ExponentialLR, CosineAnnealingLR
 * - LinearWarmup, MultiStepLR, ReduceOnPlateau
 * - CyclicLR, OneCycleLR, PolynomialLR
 *
 * @note Part of Phase TM-4: Learning Rate Scheduling
 * @version 1.0.0
 */

#include "middleware/training/nimcp_lr_scheduler.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LRS_MODULE_NAME "LRScheduler"
#define LOG_MODULE "lr_scheduler"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/geometry/nimcp_differential_geometry.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lr_scheduler)


/* ============================================================================
 * Internal Context Structure
 * ============================================================================ */

struct nimcp_lr_scheduler_ctx {
    nimcp_lr_scheduler_config_t config;  /**< Configuration */

    /* Current state */
    float current_lr;                    /**< Current learning rate */
    uint64_t current_step;               /**< Current step (batch) */
    uint64_t current_epoch;              /**< Current epoch */

    /* Scheduler-specific state */
    union {
        struct {
            /* Plateau-specific */
            float best_metric;
            uint32_t bad_epochs;
            uint32_t cooldown_counter;
            bool in_cooldown;
        } plateau;

        struct {
            /* Cyclic-specific */
            uint32_t cycle;
            float cycle_lr;
        } cyclic;

        struct {
            /* Cosine with restarts */
            uint32_t num_restarts;
            uint32_t current_T_max;
            uint64_t last_restart_epoch;
        } cosine;

        struct {
            /* Warmup + main scheduler */
            nimcp_lr_scheduler_ctx_t* main_scheduler;
            bool warmup_complete;
        } warmup;
    } state;

    /* Statistics */
    nimcp_lr_scheduler_stats_t stats;

    /* Flags */
    bool initialized;

    /* Security registration */
    nimcp_sec_integration_t* security_ctx;
    bool security_registered;
    uint32_t security_module_id;
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static float clamp_lr(float lr) {
    /* Allow 0.0 explicitly for warmup schedulers that start at 0 */
    if (lr == 0.0F) return 0.0F;
    if (lr < NIMCP_LR_MIN_VALUE) return NIMCP_LR_MIN_VALUE;
    if (lr > NIMCP_LR_MAX_VALUE) return NIMCP_LR_MAX_VALUE;
    return lr;
}

static void update_stats(nimcp_lr_scheduler_ctx_t* ctx, float new_lr) {
    ctx->stats.current_lr = new_lr;
    if (new_lr < ctx->stats.min_lr_seen) {
        ctx->stats.min_lr_seen = new_lr;
    }
    if (new_lr > ctx->stats.max_lr_seen) {
        ctx->stats.max_lr_seen = new_lr;
    }
}

/* ============================================================================
 * Default Configuration Functions
 * ============================================================================ */

nimcp_step_lr_config_t nimcp_step_lr_default_config(float initial_lr, uint32_t step_size) {
    nimcp_step_lr_config_t config = {
        .initial_lr = initial_lr,
        .step_size = step_size > 0 ? step_size : 30,
        .gamma = 0.1F,
        .min_lr = NIMCP_LR_MIN_VALUE
    };
    return config;
}

nimcp_exponential_lr_config_t nimcp_exponential_lr_default_config(float initial_lr, float gamma) {
    nimcp_exponential_lr_config_t config = {
        .initial_lr = initial_lr,
        .gamma = gamma > 0.0F ? gamma : 0.95F,
        .min_lr = NIMCP_LR_MIN_VALUE
    };
    return config;
}

nimcp_cosine_lr_config_t nimcp_cosine_lr_default_config(float initial_lr, uint32_t T_max) {
    nimcp_cosine_lr_config_t config = {
        .initial_lr = initial_lr,
        .T_max = T_max > 0 ? T_max : 100,
        .eta_min = 0.0F,
        .restart = false,
        .T_mult = 1
    };
    return config;
}

nimcp_warmup_lr_config_t nimcp_warmup_lr_default_config(float target_lr, uint32_t warmup_steps) {
    nimcp_warmup_lr_config_t config = {
        .start_lr = 0.0F,
        .target_lr = target_lr,
        .warmup_steps = warmup_steps > 0 ? warmup_steps : 1000,
        .hold_after_warmup = true
    };
    return config;
}

nimcp_plateau_lr_config_t nimcp_plateau_lr_default_config(float initial_lr) {
    nimcp_plateau_lr_config_t config = {
        .initial_lr = initial_lr,
        .mode = NIMCP_PLATEAU_MIN,
        .factor = 0.1F,
        .patience = 10,
        .threshold = 1e-4F,
        .cooldown = 0,
        .min_lr = NIMCP_LR_MIN_VALUE
    };
    return config;
}

nimcp_cyclic_lr_config_t nimcp_cyclic_lr_default_config(float base_lr, float max_lr, uint32_t step_size_up) {
    nimcp_cyclic_lr_config_t config = {
        .base_lr = base_lr,
        .max_lr = max_lr,
        .step_size_up = step_size_up > 0 ? step_size_up : 2000,
        .step_size_down = 0,  /* Same as up */
        .mode = NIMCP_CYCLIC_TRIANGULAR,
        .gamma = 1.0F
    };
    return config;
}

nimcp_one_cycle_lr_config_t nimcp_one_cycle_lr_default_config(float max_lr, uint32_t total_steps) {
    nimcp_one_cycle_lr_config_t config = {
        .max_lr = max_lr,
        .total_steps = total_steps > 0 ? total_steps : 10000,
        .pct_start = 0.3F,
        .div_factor = 25.0F,
        .final_div_factor = 10000.0F,
        .anneal_strategy_cos = true
    };
    return config;
}

/* ============================================================================
 * Scheduler Computation Functions
 * ============================================================================ */

static float compute_step_lr(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t epoch) {
    const nimcp_step_lr_config_t* cfg = &ctx->config.params.step;
    uint32_t num_decays = (uint32_t)(epoch / cfg->step_size);
    float lr = cfg->initial_lr * powf(cfg->gamma, (float)num_decays);
    return fmaxf(lr, cfg->min_lr);
}

static float compute_exponential_lr(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t epoch) {
    const nimcp_exponential_lr_config_t* cfg = &ctx->config.params.exponential;
    float lr = cfg->initial_lr * powf(cfg->gamma, (float)epoch);
    return fmaxf(lr, cfg->min_lr);
}

static float compute_cosine_lr(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t epoch) {
    const nimcp_cosine_lr_config_t* cfg = &ctx->config.params.cosine;

    uint64_t T_cur = epoch;
    uint32_t T_max = cfg->T_max;

    /* Handle warm restarts */
    if (cfg->restart && epoch >= cfg->T_max) {
        /* Find current cycle */
        uint64_t elapsed = epoch;
        uint32_t current_T = cfg->T_max;
        while (elapsed >= current_T) {
            elapsed -= current_T;
            current_T *= cfg->T_mult;
        }
        T_cur = elapsed;
        T_max = current_T;
    }

    /* Cosine annealing formula */
    float cos_val = cosf((float)M_PI * (float)T_cur / (float)T_max);
    float lr = cfg->eta_min + (cfg->initial_lr - cfg->eta_min) * (1.0F + cos_val) / 2.0F;

    return lr;
}

static float compute_warmup_lr(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t step) {
    const nimcp_warmup_lr_config_t* cfg = &ctx->config.params.warmup;

    if (step >= cfg->warmup_steps) {
        return cfg->target_lr;
    }

    /* Linear interpolation */
    float progress = (float)step / (float)cfg->warmup_steps;
    return cfg->start_lr + (cfg->target_lr - cfg->start_lr) * progress;
}

static float compute_multi_step_lr(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t epoch) {
    const nimcp_multi_step_lr_config_t* cfg = &ctx->config.params.multi_step;

    uint32_t num_decays = 0;
    for (uint32_t i = 0; i < cfg->num_milestones; i++) {
        if (epoch >= cfg->milestones[i]) {
            num_decays++;
        }
    }

    float lr = cfg->initial_lr * powf(cfg->gamma, (float)num_decays);
    return fmaxf(lr, cfg->min_lr);
}

static float compute_cyclic_lr(nimcp_lr_scheduler_ctx_t* ctx, uint64_t step) {
    const nimcp_cyclic_lr_config_t* cfg = &ctx->config.params.cyclic;

    uint32_t step_up = cfg->step_size_up;
    uint32_t step_down = cfg->step_size_down > 0 ? cfg->step_size_down : step_up;
    uint32_t cycle_length = step_up + step_down;

    uint64_t cycle_pos = step % cycle_length;
    uint32_t cycle = (uint32_t)(step / cycle_length);

    float base_lr = cfg->base_lr;
    float max_lr = cfg->max_lr;

    /* Apply mode-specific scaling */
    switch (cfg->mode) {
        case NIMCP_CYCLIC_TRIANGULAR2:
            max_lr = base_lr + (cfg->max_lr - base_lr) / powf(2.0F, (float)cycle);
            break;
        case NIMCP_CYCLIC_EXP_RANGE:
            max_lr = base_lr + (cfg->max_lr - base_lr) * powf(cfg->gamma, (float)step);
            break;
        default:
            break;
    }

    /* Geodesic interpolation for cyclic LR transitions.
     * Instead of linear interpolation between base_lr and max_lr, use
     * geodesic interpolation on the positive real line with the Fisher
     * information metric ds² = (1/lr²) dlr², where geodesics are
     * exponential curves: lr(t) = base^(1-t) * max^t.
     * This naturally respects the scale-invariant geometry of learning
     * rates (a 10x change matters equally at lr=0.001 and lr=0.01). */
    float lr;
    if (cycle_pos < step_up) {
        /* Increasing phase: geodesic from base_lr to max_lr */
        float x = (float)cycle_pos / (float)step_up;
        /* Geodesic on positive reals: lr = base^(1-x) * max^x = exp((1-x)*ln(base) + x*ln(max)) */
        if (base_lr > DIFFGEO_EPSILON && max_lr > DIFFGEO_EPSILON) {
            lr = expf((1.0f - x) * logf(base_lr) + x * logf(max_lr));
        } else {
            lr = base_lr + (max_lr - base_lr) * x;  /* Fallback to linear */
        }
    } else {
        /* Decreasing phase: geodesic from max_lr to base_lr */
        float x = (float)(cycle_pos - step_up) / (float)step_down;
        if (base_lr > DIFFGEO_EPSILON && max_lr > DIFFGEO_EPSILON) {
            lr = expf((1.0f - x) * logf(max_lr) + x * logf(base_lr));
        } else {
            lr = max_lr - (max_lr - base_lr) * x;
        }
    }

    ctx->state.cyclic.cycle = cycle;
    ctx->state.cyclic.cycle_lr = lr;

    return lr;
}

static float compute_one_cycle_lr(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t step) {
    const nimcp_one_cycle_lr_config_t* cfg = &ctx->config.params.one_cycle;

    float initial_lr = cfg->max_lr / cfg->div_factor;
    float final_lr = cfg->max_lr / cfg->final_div_factor;

    uint32_t warmup_steps = (uint32_t)(cfg->total_steps * cfg->pct_start);

    float lr;
    if (step < warmup_steps) {
        /* Warmup phase: increase to max_lr */
        float pct = (float)step / (float)warmup_steps;
        if (cfg->anneal_strategy_cos) {
            pct = (1.0F - cosf((float)M_PI * pct)) / 2.0F;
        }
        lr = initial_lr + (cfg->max_lr - initial_lr) * pct;
    } else {
        /* Annealing phase: decrease to final_lr */
        float pct = (float)(step - warmup_steps) / (float)(cfg->total_steps - warmup_steps);
        if (pct > 1.0F) pct = 1.0F;

        if (cfg->anneal_strategy_cos) {
            pct = (1.0F + cosf((float)M_PI * pct)) / 2.0F;
        } else {
            pct = 1.0F - pct;
        }
        lr = final_lr + (cfg->max_lr - final_lr) * pct;
    }

    return lr;
}

static float compute_polynomial_lr(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t step) {
    const nimcp_polynomial_lr_config_t* cfg = &ctx->config.params.polynomial;

    if (step >= cfg->total_steps) {
        return cfg->end_lr;
    }

    float decay = 1.0F - (float)step / (float)cfg->total_steps;
    float lr = (cfg->initial_lr - cfg->end_lr) * powf(decay, cfg->power) + cfg->end_lr;

    return lr;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

nimcp_lr_scheduler_ctx_t* nimcp_lr_scheduler_create(const nimcp_lr_scheduler_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    /* Validate configuration */
    if (nimcp_lr_scheduler_validate_config(config) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_lr_scheduler_create: config validation failed");
        return NULL;
    }

    nimcp_lr_scheduler_ctx_t* ctx = (nimcp_lr_scheduler_ctx_t*)nimcp_calloc(1, sizeof(nimcp_lr_scheduler_ctx_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_lr_scheduler_create: failed to allocate scheduler");

        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(nimcp_lr_scheduler_config_t));

    /* Initialize state */
    ctx->current_step = 0;
    ctx->current_epoch = 0;

    /* Set initial learning rate based on type */
    float initial_lr = 0.001F;  /* Default */
    switch (config->type) {
        case NIMCP_LR_CONSTANT:
        case NIMCP_LR_STEP:
            initial_lr = config->params.step.initial_lr;
            break;
        case NIMCP_LR_EXPONENTIAL:
            initial_lr = config->params.exponential.initial_lr;
            break;
        case NIMCP_LR_COSINE_ANNEALING:
        case NIMCP_LR_COSINE_WARMUP:
            initial_lr = config->params.cosine.initial_lr;
            break;
        case NIMCP_LR_LINEAR_WARMUP:
            initial_lr = config->params.warmup.start_lr;
            break;
        case NIMCP_LR_MULTI_STEP:
            initial_lr = config->params.multi_step.initial_lr;
            break;
        case NIMCP_LR_REDUCE_ON_PLATEAU:
            initial_lr = config->params.plateau.initial_lr;
            ctx->state.plateau.best_metric = (config->params.plateau.mode == NIMCP_PLATEAU_MIN)
                                             ? FLT_MAX : -FLT_MAX;
            ctx->state.plateau.bad_epochs = 0;
            ctx->state.plateau.cooldown_counter = 0;
            ctx->state.plateau.in_cooldown = false;
            break;
        case NIMCP_LR_CYCLIC:
            initial_lr = config->params.cyclic.base_lr;
            ctx->state.cyclic.cycle = 0;
            break;
        case NIMCP_LR_ONE_CYCLE:
            initial_lr = config->params.one_cycle.max_lr / config->params.one_cycle.div_factor;
            break;
        case NIMCP_LR_POLYNOMIAL:
            initial_lr = config->params.polynomial.initial_lr;
            break;
        case NIMCP_LR_CUSTOM:
            initial_lr = config->params.custom.initial_lr;
            break;
        default:
            break;
    }

    ctx->current_lr = clamp_lr(initial_lr);

    /* Initialize statistics */
    ctx->stats.initial_lr = ctx->current_lr;
    ctx->stats.current_lr = ctx->current_lr;
    ctx->stats.min_lr_seen = ctx->current_lr;
    ctx->stats.max_lr_seen = ctx->current_lr;
    ctx->stats.total_steps = 0;
    ctx->stats.total_epochs = 0;
    ctx->stats.num_reductions = 0;
    ctx->stats.num_cycles = 0;
    ctx->stats.best_metric = 0.0F;

    ctx->initialized = true;

    /* Register with security module if available */
    ctx->security_ctx = config->security_ctx;
    ctx->security_registered = false;
    if (ctx->security_ctx) {
        nimcp_result_t err = nimcp_sec_register_module(
            ctx->security_ctx,
            LRS_MODULE_NAME,
            NIMCP_SEC_CAT_MIDDLEWARE,
            &ctx->security_module_id
        );
        if (err == NIMCP_SUCCESS) {
            ctx->security_registered = true;
        }
    }

    return ctx;
}

void nimcp_lr_scheduler_destroy(nimcp_lr_scheduler_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Unregister from security module */
    if (ctx->security_registered && ctx->security_ctx) {
        nimcp_sec_unregister_module(ctx->security_ctx, ctx->security_module_id);
        ctx->security_registered = false;
    }

    /* Destroy chained scheduler if present */
    if (ctx->config.type == NIMCP_LR_COSINE_WARMUP && ctx->state.warmup.main_scheduler) {
        nimcp_lr_scheduler_destroy(ctx->state.warmup.main_scheduler);
    }

    nimcp_free(ctx);
}

/* ============================================================================
 * Core Operations
 * ============================================================================ */

float nimcp_lr_scheduler_step(nimcp_lr_scheduler_ctx_t* ctx) {
    if (!ctx || !ctx->initialized) {
        return 0.0F;
    }

    ctx->current_step++;
    ctx->stats.total_steps++;

    float new_lr = ctx->current_lr;

    switch (ctx->config.type) {
        case NIMCP_LR_CONSTANT:
            /* No change */
            break;

        case NIMCP_LR_LINEAR_WARMUP:
            new_lr = compute_warmup_lr(ctx, ctx->current_step);
            break;

        case NIMCP_LR_COSINE_WARMUP:
            /* Composite scheduler: warmup + main */
            if (!ctx->state.warmup.warmup_complete) {
                /* Still in warmup phase */
                new_lr = compute_warmup_lr(ctx, ctx->current_step);
                if (ctx->current_step >= ctx->config.params.warmup.warmup_steps) {
                    ctx->state.warmup.warmup_complete = true;
                }
            } else if (ctx->state.warmup.main_scheduler) {
                /* Delegate to main scheduler */
                new_lr = nimcp_lr_scheduler_step(ctx->state.warmup.main_scheduler);
            }
            break;

        case NIMCP_LR_CYCLIC:
            new_lr = compute_cyclic_lr(ctx, ctx->current_step);
            break;

        case NIMCP_LR_ONE_CYCLE:
            new_lr = compute_one_cycle_lr(ctx, ctx->current_step);
            break;

        case NIMCP_LR_POLYNOMIAL:
            new_lr = compute_polynomial_lr(ctx, ctx->current_step);
            break;

        case NIMCP_LR_CUSTOM:
            if (ctx->config.params.custom.step_fn) {
                new_lr = ctx->config.params.custom.step_fn(
                    ctx->current_step,
                    ctx->current_epoch,
                    NULL,
                    ctx->config.params.custom.user_data
                );
            }
            break;

        default:
            /* Epoch-based schedulers don't change on step */
            break;
    }

    new_lr = clamp_lr(new_lr);
    ctx->current_lr = new_lr;
    update_stats(ctx, new_lr);

    return new_lr;
}

float nimcp_lr_scheduler_step_epoch(nimcp_lr_scheduler_ctx_t* ctx) {
    if (!ctx || !ctx->initialized) {
        return 0.0F;
    }

    ctx->current_epoch++;
    ctx->stats.total_epochs++;

    float new_lr = ctx->current_lr;

    switch (ctx->config.type) {
        case NIMCP_LR_CONSTANT:
            /* No change */
            break;

        case NIMCP_LR_STEP:
            new_lr = compute_step_lr(ctx, ctx->current_epoch);
            break;

        case NIMCP_LR_EXPONENTIAL:
            new_lr = compute_exponential_lr(ctx, ctx->current_epoch);
            break;

        case NIMCP_LR_COSINE_ANNEALING:
        case NIMCP_LR_COSINE_WARMUP:
            new_lr = compute_cosine_lr(ctx, ctx->current_epoch);
            break;

        case NIMCP_LR_MULTI_STEP:
            new_lr = compute_multi_step_lr(ctx, ctx->current_epoch);
            break;

        default:
            /* Step-based schedulers handled in step() */
            break;
    }

    new_lr = clamp_lr(new_lr);
    ctx->current_lr = new_lr;
    update_stats(ctx, new_lr);

    if (ctx->config.verbose) {
        fprintf(stderr, "[LR Scheduler] Epoch %lu: lr = %.6f\n",
                (unsigned long)ctx->current_epoch, new_lr);
    }

    return new_lr;
}

float nimcp_lr_scheduler_step_metric(nimcp_lr_scheduler_ctx_t* ctx, float metric) {
    if (!ctx || !ctx->initialized) {
        return 0.0F;
    }

    if (ctx->config.type != NIMCP_LR_REDUCE_ON_PLATEAU) {
        /* Only valid for plateau scheduler */
        return ctx->current_lr;
    }

    const nimcp_plateau_lr_config_t* cfg = &ctx->config.params.plateau;

    /* Handle cooldown */
    if (ctx->state.plateau.in_cooldown) {
        ctx->state.plateau.cooldown_counter--;
        if (ctx->state.plateau.cooldown_counter == 0) {
            ctx->state.plateau.in_cooldown = false;
        }
        return ctx->current_lr;
    }

    /* Check if metric improved */
    bool improved = false;
    if (cfg->mode == NIMCP_PLATEAU_MIN) {
        if (metric < ctx->state.plateau.best_metric - cfg->threshold) {
            improved = true;
            ctx->state.plateau.best_metric = metric;
        }
    } else {
        if (metric > ctx->state.plateau.best_metric + cfg->threshold) {
            improved = true;
            ctx->state.plateau.best_metric = metric;
        }
    }

    if (improved) {
        ctx->state.plateau.bad_epochs = 0;
    } else {
        ctx->state.plateau.bad_epochs++;

        if (ctx->state.plateau.bad_epochs >= cfg->patience) {
            /* Reduce learning rate */
            float new_lr = ctx->current_lr * cfg->factor;
            new_lr = fmaxf(new_lr, cfg->min_lr);

            if (new_lr < ctx->current_lr) {
                ctx->current_lr = new_lr;
                ctx->stats.num_reductions++;

                if (ctx->config.verbose) {
                    fprintf(stderr, "[LR Scheduler] Reducing LR to %.6f (metric: %.4f, patience: %u)\n",
                            new_lr, metric, cfg->patience);
                }
            }

            ctx->state.plateau.bad_epochs = 0;

            /* Enter cooldown */
            if (cfg->cooldown > 0) {
                ctx->state.plateau.in_cooldown = true;
                ctx->state.plateau.cooldown_counter = cfg->cooldown;
            }
        }
    }

    ctx->stats.best_metric = ctx->state.plateau.best_metric;
    update_stats(ctx, ctx->current_lr);

    return ctx->current_lr;
}

float nimcp_lr_scheduler_get_lr(const nimcp_lr_scheduler_ctx_t* ctx) {
    if (!ctx) {
        return 0.0F;
    }
    return ctx->current_lr;
}

nimcp_result_t nimcp_lr_scheduler_set_lr(nimcp_lr_scheduler_ctx_t* ctx, float lr) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    ctx->current_lr = clamp_lr(lr);
    update_stats(ctx, ctx->current_lr);

    return NIMCP_SUCCESS;
}

float nimcp_lr_scheduler_get_lr_at_step(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t step) {
    if (!ctx) {
        return 0.0F;
    }

    switch (ctx->config.type) {
        case NIMCP_LR_LINEAR_WARMUP:
            return compute_warmup_lr(ctx, step);
        case NIMCP_LR_ONE_CYCLE:
            return compute_one_cycle_lr(ctx, step);
        case NIMCP_LR_POLYNOMIAL:
            return compute_polynomial_lr(ctx, step);
        default:
            return ctx->current_lr;
    }
}

float nimcp_lr_scheduler_get_lr_at_epoch(const nimcp_lr_scheduler_ctx_t* ctx, uint64_t epoch) {
    if (!ctx) {
        return 0.0F;
    }

    switch (ctx->config.type) {
        case NIMCP_LR_STEP:
            return compute_step_lr(ctx, epoch);
        case NIMCP_LR_EXPONENTIAL:
            return compute_exponential_lr(ctx, epoch);
        case NIMCP_LR_COSINE_ANNEALING:
        case NIMCP_LR_COSINE_WARMUP:
            return compute_cosine_lr(ctx, epoch);
        case NIMCP_LR_MULTI_STEP:
            return compute_multi_step_lr(ctx, epoch);
        default:
            return ctx->current_lr;
    }
}

/* ============================================================================
 * State Management
 * ============================================================================ */

nimcp_result_t nimcp_lr_scheduler_reset(nimcp_lr_scheduler_ctx_t* ctx) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");

    ctx->current_step = 0;
    ctx->current_epoch = 0;
    ctx->current_lr = ctx->stats.initial_lr;

    /* Reset scheduler-specific state */
    switch (ctx->config.type) {
        case NIMCP_LR_REDUCE_ON_PLATEAU:
            ctx->state.plateau.best_metric = (ctx->config.params.plateau.mode == NIMCP_PLATEAU_MIN)
                                             ? FLT_MAX : -FLT_MAX;
            ctx->state.plateau.bad_epochs = 0;
            ctx->state.plateau.cooldown_counter = 0;
            ctx->state.plateau.in_cooldown = false;
            break;
        case NIMCP_LR_CYCLIC:
            ctx->state.cyclic.cycle = 0;
            ctx->state.cyclic.cycle_lr = ctx->config.params.cyclic.base_lr;
            break;
        case NIMCP_LR_COSINE_ANNEALING:
            ctx->state.cosine.num_restarts = 0;
            ctx->state.cosine.current_T_max = ctx->config.params.cosine.T_max;
            ctx->state.cosine.last_restart_epoch = 0;
            break;
        default:
            break;
    }

    return NIMCP_SUCCESS;
}

uint64_t nimcp_lr_scheduler_get_step(const nimcp_lr_scheduler_ctx_t* ctx) {
    return ctx ? ctx->current_step : 0;
}

uint64_t nimcp_lr_scheduler_get_epoch(const nimcp_lr_scheduler_ctx_t* ctx) {
    return ctx ? ctx->current_epoch : 0;
}

nimcp_result_t nimcp_lr_scheduler_set_step(nimcp_lr_scheduler_ctx_t* ctx, uint64_t step) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    ctx->current_step = step;
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_lr_scheduler_set_epoch(nimcp_lr_scheduler_ctx_t* ctx, uint64_t epoch) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    ctx->current_epoch = epoch;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

nimcp_result_t nimcp_lr_scheduler_get_stats(
    const nimcp_lr_scheduler_ctx_t* ctx,
    nimcp_lr_scheduler_stats_t* stats
) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(stats != NULL, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    memcpy(stats, &ctx->stats, sizeof(nimcp_lr_scheduler_stats_t));
    return NIMCP_SUCCESS;
}

void nimcp_lr_scheduler_reset_stats(nimcp_lr_scheduler_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    ctx->stats.total_steps = 0;
    ctx->stats.total_epochs = 0;
    ctx->stats.min_lr_seen = ctx->current_lr;
    ctx->stats.max_lr_seen = ctx->current_lr;
    ctx->stats.num_reductions = 0;
    ctx->stats.num_cycles = 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* nimcp_lr_scheduler_type_name(nimcp_lr_scheduler_type_t type) {
    static const char* names[] = {
        "Constant",
        "StepLR",
        "ExponentialLR",
        "CosineAnnealingLR",
        "LinearWarmup",
        "MultiStepLR",
        "ReduceOnPlateau",
        "CyclicLR",
        "OneCycleLR",
        "CosineAnnealingWarmRestarts",
        "PolynomialLR",
        "Custom"
    };

    if (type >= NIMCP_LR_SCHEDULER_TYPE_COUNT) {
        return "Unknown";
    }
    return names[type];
}

nimcp_lr_scheduler_type_t nimcp_lr_scheduler_get_type(const nimcp_lr_scheduler_ctx_t* ctx) {
    return ctx ? ctx->config.type : NIMCP_LR_CONSTANT;
}

nimcp_result_t nimcp_lr_scheduler_validate_config(const nimcp_lr_scheduler_config_t* config) {
    NIMCP_CHECK_THROW(config != NULL, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(config->type < NIMCP_LR_SCHEDULER_TYPE_COUNT, NIMCP_ERROR_INVALID_PARAM,
        "invalid scheduler type %d", config->type);

    /* Type-specific validation */
    switch (config->type) {
        case NIMCP_LR_STEP:
            if (config->params.step.initial_lr <= 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.step.step_size == 0) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.step.gamma <= 0.0F || config->params.step.gamma > 1.0F)
                return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_EXPONENTIAL:
            if (config->params.exponential.initial_lr <= 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.exponential.gamma <= 0.0F || config->params.exponential.gamma > 1.0F)
                return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_COSINE_ANNEALING:
            if (config->params.cosine.initial_lr <= 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.cosine.T_max == 0) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.cosine.eta_min < 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_COSINE_WARMUP:
            /* Composite scheduler - validate warmup parameters */
            if (config->params.warmup.target_lr <= 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.warmup.warmup_steps == 0) return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_LINEAR_WARMUP:
            if (config->params.warmup.target_lr <= 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.warmup.warmup_steps == 0) return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_MULTI_STEP:
            if (config->params.multi_step.initial_lr <= 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.multi_step.num_milestones == 0) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.multi_step.num_milestones > NIMCP_LR_MAX_MILESTONES)
                return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_REDUCE_ON_PLATEAU:
            if (config->params.plateau.initial_lr <= 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.plateau.factor <= 0.0F || config->params.plateau.factor >= 1.0F)
                return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_CYCLIC:
            if (config->params.cyclic.base_lr < 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.cyclic.max_lr <= config->params.cyclic.base_lr)
                return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.cyclic.step_size_up == 0) return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_ONE_CYCLE:
            if (config->params.one_cycle.max_lr <= 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.one_cycle.total_steps == 0) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.one_cycle.pct_start < 0.0F || config->params.one_cycle.pct_start > 1.0F)
                return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_POLYNOMIAL:
            if (config->params.polynomial.initial_lr <= 0.0F) return NIMCP_ERROR_INVALID_PARAM;
            if (config->params.polynomial.total_steps == 0) return NIMCP_ERROR_INVALID_PARAM;
            break;

        case NIMCP_LR_CUSTOM:
            if (!config->params.custom.step_fn) return NIMCP_ERROR_INVALID_PARAM;
            break;

        default:
            break;
    }

    return NIMCP_SUCCESS;
}

nimcp_lr_scheduler_config_t nimcp_lr_scheduler_config_from_type(
    nimcp_lr_scheduler_type_t type,
    float initial_lr
) {
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));

    config.type = type;
    config.verbose = false;
    config.lr_epsilon = 1e-8F;

    switch (type) {
        case NIMCP_LR_CONSTANT:
            config.params.step.initial_lr = initial_lr;
            break;

        case NIMCP_LR_STEP:
            config.params.step = nimcp_step_lr_default_config(initial_lr, 30);
            break;

        case NIMCP_LR_EXPONENTIAL:
            config.params.exponential = nimcp_exponential_lr_default_config(initial_lr, 0.95F);
            break;

        case NIMCP_LR_COSINE_ANNEALING:
            config.params.cosine = nimcp_cosine_lr_default_config(initial_lr, 100);
            break;

        case NIMCP_LR_COSINE_WARMUP:
            /* Composite: warmup + cosine - set warmup defaults */
            config.params.warmup = nimcp_warmup_lr_default_config(initial_lr, 1000);
            break;

        case NIMCP_LR_LINEAR_WARMUP:
            config.params.warmup = nimcp_warmup_lr_default_config(initial_lr, 1000);
            break;

        case NIMCP_LR_MULTI_STEP: {
            config.params.multi_step.initial_lr = initial_lr;
            config.params.multi_step.gamma = 0.1F;
            config.params.multi_step.milestones[0] = 30;
            config.params.multi_step.milestones[1] = 60;
            config.params.multi_step.milestones[2] = 90;
            config.params.multi_step.num_milestones = 3;
            config.params.multi_step.min_lr = NIMCP_LR_MIN_VALUE;
            break;
        }

        case NIMCP_LR_REDUCE_ON_PLATEAU:
            config.params.plateau = nimcp_plateau_lr_default_config(initial_lr);
            break;

        case NIMCP_LR_CYCLIC:
            config.params.cyclic = nimcp_cyclic_lr_default_config(initial_lr / 10.0F, initial_lr, 2000);
            break;

        case NIMCP_LR_ONE_CYCLE:
            config.params.one_cycle = nimcp_one_cycle_lr_default_config(initial_lr, 10000);
            break;

        case NIMCP_LR_POLYNOMIAL:
            config.params.polynomial.initial_lr = initial_lr;
            config.params.polynomial.end_lr = initial_lr / 100.0F;
            config.params.polynomial.total_steps = 10000;
            config.params.polynomial.power = 1.0F;
            break;

        default:
            config.params.step.initial_lr = initial_lr;
            break;
    }

    return config;
}

/* ============================================================================
 * Composite Schedulers
 * ============================================================================ */

nimcp_lr_scheduler_ctx_t* nimcp_lr_scheduler_create_with_warmup(
    const nimcp_warmup_lr_config_t* warmup_config,
    const nimcp_lr_scheduler_config_t* main_config
) {
    if (!warmup_config || !main_config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_lr_scheduler_create_with_warmup: required parameter is NULL (warmup_config, main_config)");
        return NULL;
    }

    /* Create warmup scheduler configuration */
    nimcp_lr_scheduler_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = NIMCP_LR_COSINE_WARMUP;  /* Special type for warmup + main */
    config.params.warmup = *warmup_config;
    config.verbose = main_config->verbose;

    nimcp_lr_scheduler_ctx_t* ctx = nimcp_lr_scheduler_create(&config);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    /* Create the main scheduler */
    ctx->state.warmup.main_scheduler = nimcp_lr_scheduler_create(main_config);
    if (!ctx->state.warmup.main_scheduler) {
        nimcp_lr_scheduler_destroy(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_lr_scheduler_create_with_warmup: ctx->state is NULL");
        return NULL;
    }

    ctx->state.warmup.warmup_complete = false;

    return ctx;
}
